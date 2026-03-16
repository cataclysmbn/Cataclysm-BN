#include "sounds.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <system_error>
#include <unordered_map>
#include <vector>
#include <queue>

#include "active_tile_data.h"
#include "avatar.h"
#include "coordinate_conversions.h"
#include "character.h"
#include "creature.h"
#include "debug.h"
#include "enums.h"
#include "faction.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "itype.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "messages.h"
#include "monfaction.h"
#include "monster.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "rng.h"
#include "safemode_ui.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "weather.h"
#include "profile.h"
#include "omdata.h"
#include "submap.h"
#include "mtype.h"

#if defined(SDL_SOUND)
#   if defined(_MSC_VER) && defined(USE_VCPKG)
#      include <SDL2/SDL_mixer.h>
#   else
#      include <SDL_mixer.h>
#   endif
#   include <thread>
#   if defined(_WIN32) && !defined(_MSC_VER)
#       include "mingw.thread.h"
#   endif

#   define dbg(x) DebugLogFL((x),DC::SDL)
#endif

weather_type_id previous_weather;
int prev_hostiles = 0;
int previous_speed = 0;
int previous_gear = 0;
bool audio_muted = false;
float g_sfx_volume_multiplier = 1;
auto start_sfx_timestamp = std::chrono::high_resolution_clock::now();
auto end_sfx_timestamp = std::chrono::high_resolution_clock::now();
auto sfx_time = end_sfx_timestamp - start_sfx_timestamp;
activity_id act;
std::pair<std::string, std::string> engine_external_id_and_variant;

static const efftype_id effect_alarm_clock( "alarm_clock" );
static const efftype_id effect_deaf( "deaf" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_slept_through_alarm( "slept_through_alarm" );

static const trait_id trait_HEAVYSLEEPER2( "HEAVYSLEEPER2" );
static const trait_id trait_HEAVYSLEEPER( "HEAVYSLEEPER" );

static const itype_id fuel_type_muscle( "muscle" );
static const itype_id fuel_type_wind( "wind" );
static const itype_id fuel_type_battery( "battery" );

static const itype_id itype_weapon_fire_suppressed( "weapon_fire_suppressed" );

// Well made residential walls with sound proofing materials can have transmission loss values of upwards of 63 dB.
// STC ratings (in dB of sound reduction) range from 25 to 55+
// We dont have a good way of differentiating walls, so we take an average of 40dB
// Applies to more than just walls, applies to any terrain with the block_wind flag.
// Only applies when sound is being cast if it has at least two adjacent terrain of equivalent sound absorption, and all have a roof.
// In mdB spl, 100ths of a dB spl
static constexpr short SOUND_ABSORPTION_WALL = 4000;
// This is equivalent to a well designed highway sound barrier.
// If a wind blocking wall does not have a roof, it gets this.
// This is what sealed connect_to_wall terrain gets.
static constexpr short SOUND_ABSORPTION_THICK_BARRIER = 2000;
// This is what connect_to_wall terrain offers with no overhead cover. 5dB spl, 500mdB spl
static constexpr short SOUND_ABSORPTION_BARRIER = 500;
// If a block_wind terrain is completely alone, it does nothing to block sound.
// This is the default for most terrain.
// Maybe silly to cache this, but we call this frequently.
static constexpr short SOUND_ABSORPTION_OPEN_FIELD = 0;
// Maximum mdB spl value a sound can have in atmosphere.
static constexpr short MAXIMUM_VOLUME_ATMOSPHERE = 19100;
// Volume loss in mdB spl per underground zlevel difference.
// Cache this because we call it every time we check a sound to see if a monster hears it, which adds up quickly.
static constexpr short SOUND_ABSORPTION_PER_ZLEV = 4200;
// The base ambient volume above ground in mdB spl. Called frequently enough to warrant caching, and to avoid magic number usage.
static constexpr short AMBIENT_VOLUME_ABOVEGROUND = 4500;
// The base ambient volume underground in mdB spl. Called frequently enough to warrant caching, and to avoid magic number usage.
static constexpr short AMBIENT_VOLUME_UNDERGROUND = 3500;
// The base "unit" is the Bel, 10 decibels to the bel, 100 centibels to the bell, 1000 millibels to the belm and finially 100 millibels to the decibel.
static constexpr short dBspl_to_mdBspl_coeff = 100;
static constexpr double mdBspl_to_dBspl_coeff = 0.01;
// Sounds cease propagating when they go below this volume.
static constexpr short SOUND_MINIMUM_VOLUME_FOR_PROPAGATION = 2000;

// Converts decibels sound pressure level to milli-decibels sound pressure level.
// We do this often enough its worth it to have a constexpr even though its just *100
static constexpr short dBspl_to_mdBspl( const short dB )
{
    return dBspl_to_mdBspl_coeff * dB;
}
// Converts milli-decibels sound pressure level to decibels sound pressure level.
static constexpr short mdBspl_to_dBspl( const short mdB )
{
    return mdBspl_to_dBspl_coeff * mdB;
}
static constexpr uint8_t MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION = 1;
static constexpr uint8_t MAXIMUM_DISTANCE_FOR_SOUND_PROPAGATION = 120;

static constexpr uint8_t get_distance_for_volume_loss( const uint8_t tile_distance,
        const bool propagating_perpendicular )
{
    if( tile_distance <= MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION ) {
        return MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION;
    } else if( tile_distance >= MAXIMUM_DISTANCE_FOR_SOUND_PROPAGATION ) {
        return MAXIMUM_DISTANCE_FOR_SOUND_PROPAGATION;
    } else {
        return tile_distance + ( propagating_perpendicular ? -1 : 1 );
    }
}

static constexpr std::array<point, 8> get_adjacent_tiles( const point &p )
{
    const std::array<point, 8> adj_tiles = { { point( p.x - 1, p.y + 1 ), point( p.x, p.y + 1 ), point( p.x + 1, p.y + 1 ), point( p.x + 1, p.y ), point( p.x + 1, p.y - 1 ), point( p.x, p.y - 1 ), point( p.x - 1, p.y + 1 ), point( p.x - 1, p.y - 1 ) } };
    return adj_tiles;
}

// Used when flood filling sounds. We dont flood fill to tiles along the map border to prevent attempts to reference out of bounds tiles.
// Do this here insted of with an inbouds
static constexpr bool tile_along_map_border( const point &p )
{
    return ( p.x == 0 || p.x == MAPSIZE_X || p.y == 0 || p.y == MAPSIZE_Y );
}

static constexpr bool skip_due_to_wall( const bool &wall1, const bool &wall2,
                                        const uint8_t &source_dir, const uint8_t &dir_index )
{
    const auto &walls_to_check = wall_check_by_sdirection[source_dir];
    const auto &wall1_invalid = wall_sdir_invalidation[walls_to_check.first];
    const auto &wall2_invalid = wall_sdir_invalidation[walls_to_check.second];
    if( wall1 && wall2 ) {
        return ( dir_index == wall2_invalid.first || dir_index == wall2_invalid.second ||
                 dir_index == wall1_invalid.first || dir_index == wall1_invalid.second );
    } else if( wall1 ) {
        return ( dir_index == wall1_invalid.first || dir_index == wall1_invalid.second );
    } else if( wall2 ) {
        return ( dir_index == wall2_invalid.first || dir_index == wall2_invalid.second );
    }
    return false;
}

// For use when flood filling sounds to allow for Dijkstra-like max-heap processing instead of breadth first, not preserved.
struct propagation_tile {
    point  pos;
    short  vol;  // millibels
    uint8_t dir; // Sound Direction index
    uint8_t dist;// Tile distance in meters the sound has traveled. 1 tile = 1 meter.
};

// Vector of sound events qued for batch floodfilling for efficiency. By default all monster and NPC sounds are batch floodfilled.
static std::vector<sound_event> sound_batch_floodfill_que;

// Returns the reduction in dB due to terrain in mdB (100ths of a decibel) for a given terrain
// If horde signal is true, returns reducion due to terrain at a distance of ~312m
// Grab this once and store the results.
static short terrain_sound_attenuation( tripoint_abs_omt omtpos, season_type season,
                                        bool horde_signal = false )
{
    //Grab the player
    // const Character &player = get_player_character();
    // This is a bit heinous, but we have to step through several structs to actually get to the int code number for the land use codes.
    // 40 land use cases in total. We either use the integer identifier, or the string id.
    const int landusecodenum = overmap_buffer.ter( omtpos ).obj().get_land_use_code()->land_use_code;
    //player.global_omt_location()
    // Forests have less attenuation in the fall, and during winter sound attenuation is higher accross the board
    // because of expected ambient snow, which is a extremely strong sound attenuator and can absorb somewhere between 50% and 90% of high frequency sound.
    // const season_type season = season_of_year( calendar::turn );
    // Attenuation bonus from expected ambient snow.
    // These are approximates from US Army ERDC research on the effects of snow cover on sound propagation of .45 ACP and other high frequency sounds.
    const short snowbonus = ( season != WINTER ) ? SOUND_ABSORPTION_OPEN_FIELD :
                            ( horde_signal ) ? 42 : 128;

    // We want 4 total cases, open field, light vegitation/agriculture, urban, and forest/heavy vegitation.
    // Return urban if none of the specified use codes, i.e., 0
    // Technically how much a sound is attenuated also heavily depends on its frequency,
    // But we are mostly concerned with the "high frequency" portion of sounds (1kHz+)
    // High frequency sounds are what most creatures can easily pinpoint the direction of.
    // Gunshots are really a meddly of sounds across a very wide frequency band, but we care about the high frequency portion.

    // This is the really heinous bit. We either use the integer id, or the string id. Integer id it is.
    if( landusecodenum == 3 || landusecodenum == 37 || landusecodenum == 35 )  {
        // Heavy vegitation or forest. Heaviest attenuation, except in the fall.
        return snowbonus + ( ( season == AUTUMN ) ? ( ( horde_signal ) ? 20 : 9 ) : ( (
                                 horde_signal ) ? 26 : 20 ) );

    } else if( landusecodenum == 6 || landusecodenum == 9 || landusecodenum == 20 ||
               landusecodenum == 25 || landusecodenum == 26 )  {
        // Open field. No reduction to sound signature, unless its winter!
        return snowbonus;

    } else if( landusecodenum == 1 || landusecodenum == 2 || landusecodenum == 4 ||
               landusecodenum == 5 || landusecodenum == 14 || landusecodenum == 17 || landusecodenum == 23 ||
               landusecodenum == 34 || landusecodenum == 40 )  {
        // Light vegitation or agriculture. Light attenuation.
        // Farms are no longer tended, so probably overgrown.
        // Farmland is actually spectacular at attenuating low frequency sound, but we dont care about that too much here.
        return snowbonus + ( ( horde_signal ) ? 12 : 6 );

    } else {
        // Default is an urban enviornment. There are alot of codes that go into here.
        // Not great at short range attenuation, better at long range attenuation.
        // More attenuation in the winter.
        return snowbonus + ( ( horde_signal ) ? 12 : SOUND_ABSORPTION_OPEN_FIELD );
    }
}

void map::cull_heard_sounds()
{
    std::erase_if( sound_caches, []( const auto & sound ) {
        return sound.heard_by_monsters && sound.heard_by_player && sound.heard_by_npcs;
    } );
}

// Creates a sound_cache by "flood filling" a given sound event through the absorption map of the given z-level.
// This sound_cache is then added to the sound_caches vector in map.
// Fear nothing but the consequences of your own poor decisions.
void map::flood_fill_sound( const sound_event soundevent, const int zlev )
{

    const weather_manager &weather = get_weather();
    const short weather_vol = dBspl_to_mdBspl( weather.weather_id->sound_attn );
    const short wind_volume = dBspl_to_mdBspl( std::min( 150, weather.windspeed ) );
    const auto &map_cache = get_cache( zlev );
    const auto &absorption_cache = map_cache.absorption_cache;
    const auto &outside_cache = map_cache.outside_cache;
    const auto &wall_present = map_cache.sound_wall_cache;
    const short ambient_indoors = ( zlev < 0 ) ? AMBIENT_VOLUME_UNDERGROUND : AMBIENT_VOLUME_ABOVEGROUND
                                  + ( weather_vol * 2 );
    const short ambient_outside = ( zlev < 0 ) ? AMBIENT_VOLUME_UNDERGROUND : AMBIENT_VOLUME_ABOVEGROUND
                                  + ( weather_vol + wind_volume );
    // 30dB, 3000mdB is the most we can expect anything to hear below ambient.
    const short minvol_indoors = std::max( SOUND_MINIMUM_VOLUME_FOR_PROPAGATION,
                                           static_cast<short>( ambient_indoors - 3000 ) );
    const short minvol_outside = std::max( SOUND_MINIMUM_VOLUME_FOR_PROPAGATION,
                                           static_cast<short>( ambient_outside - 3000 ) );

    //// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
    //// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
    //// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
    // 8 is the center, and should not normally be called. Kept incase of a sound looping back to its origin point.
    std::array<point, 8> adjacent_tiles;

    auto cmp = []( const propagation_tile & a, const propagation_tile & b ) {
        return a.vol < b.vol;
    };
    // max-heap: highest volume processed first. We clear this after each sound processed. pqt = priority tile que
    std::priority_queue<propagation_tile, std::vector<propagation_tile>, decltype( cmp )> ptq( cmp );

    if( ( dBspl_to_mdBspl( soundevent.volume ) > ( (
                outside_cache[soundevent.origin.x][soundevent.origin.y] ) ? minvol_outside : minvol_indoors ) ) ) {

        sound_cache temp_sound_cache;
        temp_sound_cache.sound = soundevent;

        // Grab our filtering bools
        temp_sound_cache.movement_noise = temp_sound_cache.sound.movement_noise;
        temp_sound_cache.from_player = temp_sound_cache.sound.from_player;
        temp_sound_cache.from_monster = temp_sound_cache.sound.from_monster;
        temp_sound_cache.from_npc = temp_sound_cache.sound.from_npc;
        auto &svol = temp_sound_cache.volume;

        // Set our initial conditions. We want 100ths of a decibel for the volume
        // We dont apply directional sound propagation penalties at the very start.
        svol[temp_sound_cache.sound.origin.x][temp_sound_cache.sound.origin.y] =  dBspl_to_mdBspl(
                    temp_sound_cache.sound.volume ) ;
        adjacent_tiles = get_adjacent_tiles( temp_sound_cache.sound.origin.xy() );

        // This propagates the sounds from the source tile to the 8 adjacent tiles, setting initial directions, distances and volumes.
        // Adj tiles are 0-7
        for( short i = 0; i < 8; i++ ) {
            const auto tile = adjacent_tiles[i];
            // Lets make sure that we only propagate inbounds, and not along the map border. After this we can just check !tile_along_map_border
            if( !tile_along_map_border( tile ) && inbounds( tile ) ) {
                // Set our initial distance to 2. At the source there is no sound direction distance modifier.
                // And set our tile volume based on the distance. We know that the sound origin is atleast 1600mdB.
                // Set our direction based upon the adjacent tile index.
                svol[tile.x][tile.y] = std::max( 0,
                                                 ( svol[temp_sound_cache.sound.origin.x][temp_sound_cache.sound.origin.y] -
                                                   ( dist_vol_loss[2] + absorption_cache[tile.x][tile.y] ) ) );
                if( svol[tile.x][tile.y] > ( outside_cache[tile.x][tile.y] ? minvol_outside : minvol_indoors ) ) {
                    ptq.push( propagation_tile( tile, svol[tile.x][tile.y], i, 2 ) );
                }
            }
        }

        auto spropagate_from_tile = [&]( const propagation_tile & ptile ) {
            // We know that we are not propagating from a tile along the map border, so it is safe to check for walls.
            // Grab our adjacent tiles, and the values for our center tile.
            adjacent_tiles = get_adjacent_tiles( ptile.pos );

            const auto &wall_dirs = wall_check_by_sdirection[ptile.dir];
            // We only have two walls to check for, and we know what order they are in.
            const bool wall1 = wall_present[ adjacent_tiles[wall_dirs.first].x * MAPSIZE_X +
                                                                               adjacent_tiles[wall_dirs.first].y ];
            const bool wall2 = wall_present[ adjacent_tiles[wall_dirs.second].x * MAPSIZE_X +
                                                                                adjacent_tiles[wall_dirs.second].y ];
            // Iterate through adjacent tiles.
            const auto &dirs_to_check = spropagation_tiles_by_sdirection[ptile.dir];
            for( short index = 0; index < 5; index++ ) {

                auto &adj_tile_dir = dirs_to_check[index];
                // Only check if we know a wall is present.
                if( wall1 || wall2 ) {
                    if( skip_due_to_wall( wall1, wall2, ptile.dir, adj_tile_dir ) ) {
                        continue;
                    }
                }
                auto &adj_tile = adjacent_tiles[adj_tile_dir];
                // Dont check tiles that are not valid for propagation, i.e. behind the direction of sound, around a corner, or out of bounds.
                if( !tile_along_map_border( adj_tile ) ) {
                    auto &adj_tile_vol = svol[adj_tile.x][adj_tile.y];
                    // Cap our tile distance between 1 and 121 to prevent overflow. We dont have or need distance loss values past dist_vol_loss[121]
                    // as the change in distance loss values past this point are negligible for gameplay scale.
                    const uint8_t dist_for_vol_loss = get_distance_for_volume_loss( ptile.dist, ( index == 0 ||
                                                      index == 4 ) );
                    const short vol_to_check = ptile.vol - ( absorption_cache[adj_tile.x][adj_tile.y] +
                                               dist_vol_loss[dist_for_vol_loss] );
                    // General priority goes loudest volume, then largest distance. Smaller distances loose volume more quickly.
                    // If volumes are equal and directions are one off from eachother, the cardinal direction wins.
                    // We dont want to track inaudible single dB values across the entire map for each sound.
                    if( ( vol_to_check ) > adj_tile_vol ) {
                        adj_tile_vol = vol_to_check;
                        if( vol_to_check > SOUND_MINIMUM_VOLUME_FOR_PROPAGATION ) {
                            // If the tiles new volume is greater than 20dB, mark it for update.
                            // Will not update if the adjacent tile is along the map boundry.
                            ptq.push( propagation_tile( adj_tile, vol_to_check, adj_tile_dir, dist_for_vol_loss ) );
                        }
                    }
                }
            }

        };

        // Run through the priority que using the spropagate_from_tile lambda.
        // And then we repeat until no new tiles need to be updated.
        while( !ptq.empty() ) {
            // Propagate our loudest tile.
            spropagate_from_tile( ptq.top() );
            // After calculating, remove our loudest entry.
            ptq.pop();
        }
        // The sound cache should be built out by now.
        // Add our new sound cache to the games sound_caches vector.
        // add_msg(m_debug, _("Attempting to add sound_cache with origin %i x: %i y: %i z and source volume %i to sound_caches vector ."), temp_sound_cache.sound.origin.x, temp_sound_cache.sound.origin.y, temp_sound_cache.sound.origin.z, temp_sound_cache.sound.volume );
        sound_caches.push_back( temp_sound_cache );
    }

}

// Batch flood fills a given vector of sound events, stepping through all z levels.
// New sound_cache are then added to the sound_caches vector in map.
void map::batch_flood_fill_sounds()
{
    ZoneScoped;
    // Our que of sound events to flood fill
    auto &batch_que = sound_batch_floodfill_que;

    add_msg( m_debug, _( "Attempting to batch flood fill %i sounds." ), batch_que.size() );

    const weather_manager &weather = get_weather();
    const short weather_vol = dBspl_to_mdBspl( weather.weather_id->sound_attn );
    const short wind_volume = dBspl_to_mdBspl( std::min( 150, weather.windspeed ) );

    // How many sounds did we actually process?
    short num_processed_sounds = 0;
    short num_invalidated_sounds = 0;

    //// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
    //// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
    //// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
    // 8 is the center, and should not normally be called. Kept incase of a sound looping back to its origin point.
    std::array<point, 8> adjacent_tiles;

    auto cmp = []( const propagation_tile & a, const propagation_tile & b ) {
        return a.vol < b.vol;
    };
    // max-heap: highest volume processed first. We clear this after each sound processed. pqt = priority tile que
    std::priority_queue<propagation_tile, std::vector<propagation_tile>, decltype( cmp )> ptq( cmp );

    // Now we step through our zlevels
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {

        const auto &map_cache = get_cache( z );
        const auto &absorption_cache = map_cache.absorption_cache;
        const auto &outside_cache = map_cache.outside_cache;
        const auto &wall_present = map_cache.sound_wall_cache;
        const short ambient_indoors = ( z < 0 ) ? AMBIENT_VOLUME_UNDERGROUND : AMBIENT_VOLUME_ABOVEGROUND
                                      + ( weather_vol * 2 );
        const short ambient_outside = ( z < 0 ) ? AMBIENT_VOLUME_UNDERGROUND : AMBIENT_VOLUME_ABOVEGROUND
                                      + ( weather_vol + wind_volume );
        // 30dB, 3000mdB is the most we can expect anything to hear below ambient.
        const short minvol_indoors = std::max( SOUND_MINIMUM_VOLUME_FOR_PROPAGATION,
                                               static_cast<short>( ambient_indoors - 3000 ) );
        const short minvol_outside = std::max( SOUND_MINIMUM_VOLUME_FOR_PROPAGATION,
                                               static_cast<short>( ambient_outside - 3000 ) );

        // We cycle through all the sounds in the batch que
        for( sound_event flooded_sound : batch_que ) {
            // Skip all sounds that do not originate from our zlevel, are along the map border, or that are too far below our ambient volume.
            if( flooded_sound.origin.z != z ) {
                if( ( z == flooded_sound.origin.z ) &&
                    ( dBspl_to_mdBspl( flooded_sound.volume ) < ( (
                                outside_cache[flooded_sound.origin.x][flooded_sound.origin.y] ) ? minvol_outside :
                            minvol_indoors ) ) ) {
                    num_invalidated_sounds++;
                    continue;
                }
                continue;
            } else if( ( dBspl_to_mdBspl( flooded_sound.volume ) > ( (
                             outside_cache[flooded_sound.origin.x][flooded_sound.origin.y] ) ? minvol_outside :
                         minvol_indoors ) ) ) {

                sound_cache temp_sound_cache;
                temp_sound_cache.sound = flooded_sound;

                // Grab our filtering bools
                temp_sound_cache.movement_noise = temp_sound_cache.sound.movement_noise;
                temp_sound_cache.from_player = temp_sound_cache.sound.from_player;
                temp_sound_cache.from_monster = temp_sound_cache.sound.from_monster;
                temp_sound_cache.from_npc = temp_sound_cache.sound.from_npc;
                auto &svol = temp_sound_cache.volume;

                // Set our initial conditions. We want 100ths of a decibel for the volume
                // We dont apply directional sound propagation penalties at the very start.
                svol[temp_sound_cache.sound.origin.x][temp_sound_cache.sound.origin.y] =  dBspl_to_mdBspl(
                            temp_sound_cache.sound.volume ) ;
                adjacent_tiles = get_adjacent_tiles( temp_sound_cache.sound.origin.xy() );

                // This propagates the sounds from the source tile to the 8 adjacent tiles, setting initial directions, distances and volumes.
                // Adj tiles are 0-7
                for( short i = 0; i < 8; i++ ) {
                    const auto tile = adjacent_tiles[i];
                    // Lets make sure that we only propagate inbounds, and not along the map border. After this we can just check !tile_along_map_border
                    if( !tile_along_map_border( tile ) && inbounds( tile ) ) {
                        // Set our initial distance to 2. At the source there is no sound direction distance modifier.
                        // And set our tile volume based on the distance. We know that the sound origin is atleast 1600mdB.
                        // Set our direction based upon the adjacent tile index.
                        svol[tile.x][tile.y] = std::max( 0,
                                                         ( svol[temp_sound_cache.sound.origin.x][temp_sound_cache.sound.origin.y] -
                                                           ( dist_vol_loss[2] + absorption_cache[tile.x][tile.y] ) ) );
                        if( svol[tile.x][tile.y] > ( outside_cache[tile.x][tile.y] ? minvol_outside : minvol_indoors ) ) {
                            ptq.push( propagation_tile( tile, svol[tile.x][tile.y], i, 2 ) );
                        }
                    }
                }

                auto spropagate_from_tile = [&]( const propagation_tile & ptile ) {
                    // We know that we are not propagating from a tile along the map border, so it is safe to check for walls.
                    // Grab our adjacent tiles, and the values for our center tile.
                    adjacent_tiles = get_adjacent_tiles( ptile.pos );

                    const auto &wall_dirs = wall_check_by_sdirection[ptile.dir];
                    // We only have two walls to check for, and we know what order they are in.
                    const bool wall1 = wall_present[ adjacent_tiles[wall_dirs.first].x * MAPSIZE_X +
                                                                                       adjacent_tiles[wall_dirs.first].y ];
                    const bool wall2 = wall_present[ adjacent_tiles[wall_dirs.second].x * MAPSIZE_X +
                                                                                        adjacent_tiles[wall_dirs.second].y ];
                    // Iterate through adjacent tiles.
                    const auto &dirs_to_check = spropagation_tiles_by_sdirection[ptile.dir];
                    for( short index = 0; index < 5; index++ ) {

                        auto &adj_tile_dir = dirs_to_check[index];
                        // Only check if we know a wall is present.
                        if( wall1 || wall2 ) {
                            if( skip_due_to_wall( wall1, wall2, ptile.dir, adj_tile_dir ) ) {
                                continue;
                            }
                        }
                        auto &adj_tile = adjacent_tiles[adj_tile_dir];
                        // Dont check tiles that are not valid for propagation, i.e. behind the direction of sound, around a corner, or out of bounds.
                        if( !tile_along_map_border( adj_tile ) ) {
                            auto &adj_tile_vol = svol[adj_tile.x][adj_tile.y];
                            // Cap our tile distance between 1 and 121 to prevent overflow. We dont have or need distance loss values past dist_vol_loss[121]
                            // as the change in distance loss values past this point are negligible for gameplay scale.
                            const uint8_t dist_for_vol_loss = get_distance_for_volume_loss( ptile.dist, ( index == 0 ||
                                                              index == 4 ) );
                            const short vol_to_check = ptile.vol - ( absorption_cache[adj_tile.x][adj_tile.y] +
                                                       dist_vol_loss[dist_for_vol_loss] );
                            // General priority goes loudest volume, then largest distance. Smaller distances loose volume more quickly.
                            // If volumes are equal and directions are one off from eachother, the cardinal direction wins.
                            // We dont want to track inaudible single dB values across the entire map for each sound.
                            if( ( vol_to_check ) > adj_tile_vol ) {
                                adj_tile_vol = vol_to_check;
                                if( vol_to_check > SOUND_MINIMUM_VOLUME_FOR_PROPAGATION ) {
                                    // If the tiles new volume is greater than 20dB, mark it for update.
                                    // Will not update if the adjacent tile is along the map boundry.
                                    ptq.push( propagation_tile( adj_tile, vol_to_check, adj_tile_dir, dist_for_vol_loss ) );
                                }
                            }
                        }
                    }

                };

                // Run through the priority que using the spropagate_from_tile lambda.
                // And then we repeat until no new tiles need to be updated.
                while( !ptq.empty() ) {
                    // Propagate our loudest tile.
                    spropagate_from_tile( ptq.top() );
                    // After calculating, remove our loudest entry.
                    ptq.pop();
                }
                // The sound cache should be built out by now.
                // Add our new sound cache to the games sound_caches vector.
                // add_msg(m_debug, _("Attempting to add sound_cache with origin %i x: %i y: %i z and source volume %i to sound_caches vector ."), temp_sound_cache.sound.origin.x, temp_sound_cache.sound.origin.y, temp_sound_cache.sound.origin.z, temp_sound_cache.sound.volume );
                sound_caches.push_back( temp_sound_cache );
                // add_msg(m_debug, _("Sound cache added to vector"));
                num_processed_sounds++;
                continue;
            }
            // If our sound was not loud enough, invalidate it.
            num_invalidated_sounds++;
        }
    }
    batch_que.clear();
    add_msg( m_debug,
             _( "Batch flood filled %i sounds, %i sounds invalidated during batch processing." ),
             num_processed_sounds, num_invalidated_sounds );
}


// Nominally ground effect varies by terrain, sound frequency, and distance from source.
// The ranges we are dealing with are short (at most ~120m)
// For consistancy we are assuming that the majority of sounds are high frequency (1+kHz, generally 2kHz)
// We are not taking into account changes in sound attenuation effects due to changes in temperature or humidity.
// With real physics sound attenuation due to foliage or ground clutter drops off sharply after a few meters,
// as the sound travels up and over the obstacle in question, and then radiates back down to the listener.
// Modelling that for each sound would be hell on performance, so we approximate.
// Terrain absorption is in addition to the logarithmic loss of pressure over distance.
// Building the absorption cache also builds the sound_wall_cache bitset.
// Measured in 100ths of a decibel
bool map::build_absorption_cache( const int zlev )
{
    ZoneScoped;

    auto &map_cache = get_cache( zlev );
    auto &absorption_cache = map_cache.absorption_cache;
    // We use this to determine if wind blocking terrain gives its full 20 dB reduction,
    // or only counts as a barrier with a 5dB reduction
    // Indoors is false, outdoors is true.
    auto &outside_cache = map_cache.outside_cache;

    if( map_cache.absorption_cache_dirty.none() ) {
        return false;
    }

    // if true, all submaps are invalid (can use batch init)
    bool rebuild_all = map_cache.absorption_cache_dirty.all();

    if( rebuild_all ) {
        // We have two general cases, sound absorption due to a barrier
        // And sound absorption due to surface effect.
        // We default to no absorption, i.e., some arbitrarily hard surface (asphault/concrete ground surfaces are effectively 0 for our purposes)
        std::uninitialized_fill_n( &absorption_cache[0][0], MAPSIZE_X * MAPSIZE_Y,
                                   SOUND_ABSORPTION_OPEN_FIELD );
    }

    const season_type season = season_of_year( calendar::turn );

    // Traverse the submaps in order
    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const auto cur_submap = get_submap_at_grid( { smx, smy, zlev } );

            const point sm_offset = sm_to_ms_copy( point( smx, smy ) );

            if( !rebuild_all && !map_cache.absorption_cache_dirty[smx * MAPSIZE + smy] ) {
                continue;
            }
            //submap tripoint, do not use for normal coords!
            const tripoint sm( smx, smy, zlev );
            const auto abs_sm = map::abs_sub + sm;
            const tripoint_abs_omt abs_omt( sm_to_omt_copy( abs_sm ) );
            const auto default_terrain_absorption = terrain_sound_attenuation( abs_omt, season );

            // calculates absorption of a single tile
            // x,y - coords in map local coords
            // Used below
            auto calc_absorption = [&]( point  p ) {
                const point sp = p - sm_offset;
                // If we are indoors, we dont get terrain sound attenuation.
                short value = ( outside_cache[p.x][p.y] ) ? default_terrain_absorption :
                              SOUND_ABSORPTION_OPEN_FIELD;

                // See if there is a vehicle in our given tripoint.
                // If there is, if there is a full board or a closed door, return thick barrier sound absorption.
                // We could technically run through checking adjacent tiles as we do below, but vehicles are dynamic and rechecking all of the vehicles tiles every turn would not provide enough benifit.
                if( const auto vp = veh_at( tripoint( p.x, p.y, zlev ) ) ) {
                    if( vp.part_with_feature( "FULL_BOARD", true ) || ( vp.obstacle_at_part() &&
                            vp.part_with_feature( "OPENABLE", true ) ) ) {
                        return SOUND_ABSORPTION_THICK_BARRIER;
                    }
                }
                const auto &tile_furn = cur_submap->get_furn( sp ).obj();
                const auto &tile_ter = cur_submap->get_ter( sp ).obj();
                // Count as a barrier if its furniture with block wind. These tend to be lighter things
                // like tent walls or sandbags, so they count as a barrier
                if( tile_furn.has_flag( "BLOCK_WIND" ) ) {
                    value += SOUND_ABSORPTION_BARRIER;
                    return value;
                }

                // Do this last as it involves the most calcs.
                if( ( tile_ter.has_flag( "BLOCK_WIND" ) ||
                      tile_ter.has_flag( "CONNECT_TO_WALL" ) ) &&
                    !outside_cache[p.x][p.y] ) {
                    // Store which type of sound block we are using. If true we have a windblocker, if false we have a barrier
                    const bool blockswind = tile_ter.has_flag( "BLOCK_WIND" );


                    // Make an array for points, and two for bools (valid terrain, and if there is a roof).
                    // [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
                    // [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 3 ] [ 4 ] [ 5 ]
                    // [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 7 ] [ 8 ]
                    // A bit ugly, apologies. We use the normal point instead of subgrid point because checking adjacent at subgrid boundry will result in checking negative subgrid coords
                    const std::array<point, 9> points_to_check = { point( p.x - 1, p.y + 1 ), point( p.x, p.y + 1 ), point( p.x + 1, p.y + 1 ), point( p.x - 1, p.y ), p, point( p.x + 1, p.y + 1 ), point( p.x - 1, p.y - 1 ), point( p.x, p.y - 1 ), point( p.x + 1, p.y - 1 )};

                    // Alrighty, here we go. Queary the adjacent terrain to see if it blocks sound or connects to a wall.
                    // Lets build out the bool indexes. We need these bool indexes for actually calcing the terrain absorption.
                    std::array<bool, 9> point_valid = { {false, false, false, false, false, false, false, false, false} };
                    std::array<bool, 9> roof_cover = { {false, false, false, false, false, false, false, false, false} };
                    for( short i = 0; i < 9; i++ ) {

                        // Because checking adjacent tiles can result in us attempting to check a negative subgrid point,
                        // We have to convert back over to normal tripoints.
                        const tripoint temp_point = tripoint( points_to_check[i].x, points_to_check[i].y, zlev );

                        // Does the point in question have a roof?
                        // Remember, outside cache returns true if something counts as outdoors and has no roof.
                        roof_cover[i] = !outside_cache[temp_point.x][temp_point.y];
                        //( outside_cache[temp_point.x][temp_point.y] ) ? roof_cover[i] = false : roof_cover[i] = true;

                        // Does the point in question have terrain that blocks wind or connects to wall, and does it have a roof?
                        // Adjacent furniture that blocks wind can count for the purposes of if this tile is valid.
                        // TODO: Eval this check, expecially the furniture check, to see if it needs to be improved or removed.
                        const auto &adj_terrain = ter( temp_point ).obj();
                        const auto &adj_furn = furn( temp_point ).obj();
                        ( roof_cover[i] && ( adj_furn.has_flag( "BLOCK_WIND" ) || adj_terrain.has_flag( "BLOCK_WIND" ) ||
                                             adj_terrain.has_flag( "CONNECT_TO_WALL" ) ) ) ? point_valid[i] = true :
                                                     point_valid[i] = false;
                    }

                    // We have a few valid conditions. For the terrain to provide its full sound absorption, it must have at least two directly (x/y, no diagonals) adjacent wind blocking or connect_to_wall buddies which must be roofed,
                    // And all of the adjacent valid terrain features must have an adjacent rooved tile that is also adjacent to the center tile.
                    // In effect, we are looking for solid lines, or L shapes. There will be some oddities with this, if it becomes a significant issue we can look into making it more granular.
                    //
                    // 0 0 0                W R R    R W 0     W R W                     W R 0                         R 0 R    R 0 R
                    // W W W works, as does W W W or W W 0 but 0 W 0 will not, nor would 0 W W      As a special rule, W W W or W W W and any rotation/inversion therein will not work.
                    // R R R                0 0 R    0 0 0     W R W                     0 R W                         0 R 0    0 0 0
                    //
                    // In effect, the terrain would have to properly prevent creature movement, and if there is a straight line of walls they must have a contiguous roof.
                    // We dont care if there are more valid points than nessesary.

                    // Does our terrain have enough buddies?
                    short buddynumber = 0;
                    // In effect, we check each of our adjacent terrain to see if it is properly rooved. ( 1, 3, 5, 7)
                    // Could probably find a more elegant way to do this, but this is relatively quick.
                    // If a valid point does not have a directly adjacent roof, set it to not valid for a future check.
                    if( point_valid[1] && ( roof_cover[0] || roof_cover[2] ) ) {
                        buddynumber++;
                    }
                    if( point_valid[3] && ( roof_cover[0] || roof_cover[6] ) ) {
                        buddynumber++;
                    }
                    if( point_valid[5] && ( roof_cover[2] || roof_cover[8] ) ) {
                        buddynumber++;
                    }
                    if( point_valid[7] && ( roof_cover[6] || roof_cover[8] ) ) {
                        buddynumber++;
                    }
                    // At one or zero buddies sound dampening is reduced.
                    if( buddynumber < 2 ) {
                        return ( buddynumber == 0 ) ? value : ( blockswind ) ?
                               SOUND_ABSORPTION_BARRIER : value;
                    } else if( buddynumber >= 3 ) {
                        return ( blockswind ) ? SOUND_ABSORPTION_WALL : SOUND_ABSORPTION_BARRIER;
                    }
                    // Our special rule, this one is a bit of a doozy.
                    // This case can only happen with 2 valid directly adjacent terrain,
                    // and we have invalidated any terrain pieces without an adjacent roof.
                    // so we can check to see if we only have a straight line.
                    else if( point_valid[3] && point_valid[5] ) {
                        // Only grant full value if we have contiguous roof.
                        return ( ( roof_cover[0] && roof_cover[1] && roof_cover[2] ) || ( roof_cover[6] && roof_cover[7] &&
                                 roof_cover[8] ) ) ? ( ( blockswind ) ? SOUND_ABSORPTION_WALL : SOUND_ABSORPTION_THICK_BARRIER ) :
                               ( blockswind ) ?
                               SOUND_ABSORPTION_THICK_BARRIER : SOUND_ABSORPTION_BARRIER;

                    } else if( point_valid[1] && point_valid[7] ) {
                        return ( ( roof_cover[0] && roof_cover[3] && roof_cover[6] ) || ( roof_cover[2] && roof_cover[5] &&
                                 roof_cover[8] ) ) ? ( ( blockswind ) ? SOUND_ABSORPTION_WALL : SOUND_ABSORPTION_THICK_BARRIER ) :
                               ( blockswind ) ?
                               SOUND_ABSORPTION_THICK_BARRIER : SOUND_ABSORPTION_BARRIER;
                    }
                }
                // If none of the above
                return value;
            };

            if( cur_submap->is_uniform ) {
                const short value = calc_absorption( sm_offset );
                // if rebuild_all==true all values were already set to 0
                if( !rebuild_all || value != SOUND_ABSORPTION_OPEN_FIELD ) {
                    for( int sx = 0; sx < SEEX; ++sx ) {
                        // init all sy indices in one go
                        std::uninitialized_fill_n( &absorption_cache[sm_offset.x + sx][sm_offset.y], SEEY, value );
                        if( value >= SOUND_ABSORPTION_THICK_BARRIER ) {
                            const int x = sx + sm_offset.x;
                            for( int sy = 0; sy < SEEY; ++sy ) {
                                const int y = sy + sm_offset.y;
                                set_sound_wall_cache( tripoint( x, y, zlev ) );
                            }
                        }
                    }
                }
            } else {
                for( int sx = 0; sx < SEEX; ++sx ) {
                    const int x = sx + sm_offset.x;
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        const int y = sy + sm_offset.y;
                        absorption_cache[x][y] = calc_absorption( { x, y } );
                        if( absorption_cache[x][y] >= SOUND_ABSORPTION_THICK_BARRIER ) {
                            set_sound_wall_cache( tripoint( x, y, zlev ) );
                        }
                    }
                }
            }
        }
    }
    map_cache.absorption_cache_dirty.reset();
    return true;
}

namespace io
{
// *INDENT-OFF*
template<>
std::string enum_to_string<sounds::sound_t>( sounds::sound_t data )
{
    switch ( data ) {
    case sounds::sound_t::background: return "background";
    case sounds::sound_t::weather: return "weather";
    case sounds::sound_t::music: return "music";
    case sounds::sound_t::movement: return "movement";
    case sounds::sound_t::speech: return "speech";
    case sounds::sound_t::electronic_speech: return "electronic_speech";
    case sounds::sound_t::activity: return "activity";
    case sounds::sound_t::destructive_activity: return "destructive_activity";
    case sounds::sound_t::alarm: return "alarm";
    case sounds::sound_t::combat: return "combat";
    case sounds::sound_t::alert: return "alert";
    case sounds::sound_t::order: return "order";
    case sounds::sound_t::_LAST: break;
    }
    debugmsg( "Invalid sound_t" );
    abort();
}

template<>
std::string enum_to_string<sfx::channel>( sfx::channel chan )
{
    switch ( chan ) {
    case sfx::channel::any: return "any";
    case sfx::channel::daytime_outdoors_env: return "daytime_outdoors_env";
    case sfx::channel::nighttime_outdoors_env: return "nighttime_outdoors_env";
    case sfx::channel::underground_env: return "underground_env";
    case sfx::channel::indoors_env: return "indoors_env";
    case sfx::channel::indoors_rain_env: return "indoors_rain_env";
    case sfx::channel::outdoors_snow_env: return "outdoors_snow_env";
    case sfx::channel::outdoors_flurry_env: return "outdoors_flurry_env";
    case sfx::channel::outdoors_thunderstorm_env: return "outdoors_thunderstorm_env";
    case sfx::channel::outdoors_rain_env: return "outdoors_rain_env";
    case sfx::channel::outdoors_drizzle_env: return "outdoors_drizzle_env";
    case sfx::channel::outdoor_blizzard: return "outdoor_blizzard";
    case sfx::channel::deafness_tone: return "deafness_tone";
    case sfx::channel::danger_extreme_theme: return "danger_extreme_theme";
    case sfx::channel::danger_high_theme: return "danger_high_theme";
    case sfx::channel::danger_medium_theme: return "danger_medium_theme";
    case sfx::channel::danger_low_theme: return "danger_low_theme";
    case sfx::channel::stamina_75: return "stamina_75";
    case sfx::channel::stamina_50: return "stamina_50";
    case sfx::channel::stamina_35: return "stamina_35";
    case sfx::channel::idle_chainsaw: return "idle_chainsaw";
    case sfx::channel::chainsaw_theme: return "chainsaw_theme";
    case sfx::channel::player_activities: return "player_activities";
    case sfx::channel::exterior_engine_sound: return "exterior_engine_sound";
    case sfx::channel::interior_engine_sound: return "interior_engine_sound";
    case sfx::channel::radio: return "radio";
    case sfx::channel::MAX_CHANNEL: break;
    }
    debugmsg( "Invalid sound channel" );
    abort();
}
// *INDENT-ON*
} // namespace io

// Static globals tracking sounds events of various kinds.
// The sound events since the last monster turn.
// Depreciated, kept as comment for reference
//static std::vector<std::pair<tripoint, sound_event>> recent_sounds;

// The sound events since the last interactive player turn. (doesn't count sleep etc)
// Depreciated, kept as comment for reference
//static std::vector<std::pair<tripoint, sound_event>> sounds_since_last_turn;

// The sound events currently displayed to the player.
static std::unordered_map<tripoint, sound_event> sound_markers;

// This is an attempt to handle attenuation of sound for underground areas.
// The main issue it adresses is that you can hear activity
// relatively deep underground while on the surface.
// My research indicates that attenuation through soil-like materials is as
// high as 100x the attenuation through air, plus vertical distances are
// roughly five times as large as horizontal ones.
// TODO: Update this to the dB overhaul
static int sound_distance( const tripoint &source, const tripoint &sink )
{
    const int lower_z = std::min( source.z, sink.z );
    const int upper_z = std::max( source.z, sink.z );
    const int vertical_displacement = upper_z - lower_z;
    int vertical_attenuation = vertical_displacement;
    if( lower_z < 0 && vertical_displacement > 0 ) {
        // Apply a moderate bonus attenuation (5x) for the first level of vertical displacement.
        vertical_attenuation += 4;
        // At displacements greater than one, apply a large additional attenuation (100x) per level.
        const int underground_displacement = std::min( -lower_z, vertical_displacement );
        vertical_attenuation += ( underground_displacement - 1 ) * 20;
    }
    // Regardless of underground effects, scale the vertical distance by 5x.
    vertical_attenuation *= 5;
    return rl_dist( source.xy(), sink.xy() ) + vertical_attenuation;
}

void sounds::sound( const sound_event &soundevent )
{
    // Error out if volume is negative, or bail out if volume is less than 16dB.
    // There are not anechoic chambers in game, so actually hearing such sounds after even 1 tile of distance (15dB -> 9dB 1 tile away) is very unlikely for the vast majority of creatures.
    // A good threshold for sounds that should only really be faintly audible to the player in a quiet room is 20dB.
    // Most sounds intended to be quiet but still audible to the player, and maybe to creatures very close, is 35-45dB.
    // Ambient volume minimum is usually between 35 and 50dB in game. A player with normal hearing can notice sounds 20dB below ambient.
    sound_event temp_sound_event = soundevent;
    if( temp_sound_event.volume < mdBspl_to_dBspl( SOUND_MINIMUM_VOLUME_FOR_PROPAGATION ) ) {

        add_msg( m_debug,
                 _( "Sound with description [ %1s ] at %i:%i with a volume %i too quiet for propagation." ),
                 temp_sound_event.description, temp_sound_event.origin.x, temp_sound_event.origin.y,
                 temp_sound_event.volume );

        return;
    } else if( temp_sound_event.volume < ( ( temp_sound_event.origin.z < 0 ) ? mdBspl_to_dBspl(
            AMBIENT_VOLUME_UNDERGROUND ) : mdBspl_to_dBspl( AMBIENT_VOLUME_ABOVEGROUND ) +
                                           get_weather().weather_id->sound_attn ) - 19 ) {
        // Dont propagate sounds that are too quiet to be heard.
        return;
    }
    // Description is not an optional parameter
    if( temp_sound_event.description.empty() ) {
        debugmsg( "Sound at %i:%i has no description!", temp_sound_event.origin.x,
                  temp_sound_event.origin.y );
        return;
    }
    // Check to see if more than one source has been set somehow.
    // More than one source entity will break alot of logic downstream.
    if( ( temp_sound_event.from_monster + temp_sound_event.from_npc + temp_sound_event.from_player ) >
        1 ) {
        debugmsg( "Sound at %i:%i has too many source entity types!", temp_sound_event.origin.x,
                  temp_sound_event.origin.y );
        return;
    }
    map &map = get_map();

    // Maximum possible sound pressure level in atmosphere is 191 dB, cap our volume for sanity.
    // Sound volumes above 191dB are not sound pressure waves, they are supersonic blast/shock waves and should be modeled as damaging explosions.
    // Check above should catch any volumes that are too low or negative.
    temp_sound_event.volume = std::min( temp_sound_event.volume,
                                        mdBspl_to_dBspl( MAXIMUM_VOLUME_ATMOSPHERE ) );

    // We flood fill sounds from monsters and NPCs for performance and efficiency reasons.
    if( soundevent.from_monster || soundevent.from_npc ) {
        sound_batch_floodfill_que.push_back( temp_sound_event );
    } else {
        map.flood_fill_sound( temp_sound_event, temp_sound_event.origin.z );
    }

}

template <typename C>
static void vector_quick_remove( std::vector<C> &source, int index )
{
    if( source.size() != 1 ) {
        // Swap the target and the last element of the vector.
        // This scrambles the vector, but makes removal O(1).
        std::iter_swap( source.begin() + index, source.end() - 1 );
    }
    source.pop_back();
}

static int get_signal_for_hordes( const sound_event centr, const short ambient_vol,
                                  const short terrain_absorption, const short alt_adjust )
{
    // Volume in dB. Signal for hordes in submaps
    // Reduce volume by the ambient weather volume. Sounds quieter than this are effectively drowned out/ignored.
    // However hordes themselves are noisy, taken at ~60 dB ( 60 dB for normal conversation )
    // Its not that the zombies cant technically hear noises quieter than this, its that the sound is not more interesting than any of the other noise assorted zombies are making.
    // Most of cata is not nice flat plains. Urban enviornments and especially forests attenuate sound more effectively than a flat plain.
    // volume in dB must be atleast 40 dB greater than the ambient noise (~40 dB is lost over 96 tiles (taken as 96m))
    // and we only want sounds that are louder so round up from 39.6 dB
    // A min signal of 8 corresponds roughly to 96 tiles (96m)
    // The max signal of 26 corresponds roughly to 312 tiles (312m) (~50 dB are lost over 312 meters)
    // Just take the 50 dB loss from 312 meters, 10 dB difference is perceived as twice as loud
    // Subtract by terrain absorption as well.

    const int vol = centr.volume - 50 - terrain_absorption - alt_adjust;

    // Hordes can't hear lower than this due to loss of volume from distance.
    // The ambient noise is either the volume of the hordes ambient zombie noises, or louder weather.
    // Intended result is that hordes will have significantly reduced signal with loud ambient weather like a thunderstorm.

    // Coefficient for volume reduction underground. Sound attenuation of soil/rock can be upwards of 100x
    // and each vertical tile is roughly 3 to 5x the distance for a maximum of 500x if there is solid rock. This is a reduction to the energy of a pressure wave.
    // We are dealing with decibels however, which is a relative logrithmic measure of a pressure wave and it is likely that there is not just solid rock in the way.
    // Every time a pressure waves energy is doubled or halved, the dB value changes by 6.
    // Reducing the energy by 256x per level results in a dB reduction of 42 per z level underground.
    // This is handled by alt_adjust provided to the signal function.

    // dB outgoing to the horde with reduction for ground adjustment
    if( vol < ambient_vol ) {
        return 0;
    }
    // A rough ballpart for small arms fire is 150-160 dB at the shooters ear, usually ~2 feet from the muzzle of the firearm.
    // The ambient noise for a horde would be however loud the horde itself is, or weather if louder.
    // that puts us 90-100 dB above ambient at the shooter, 30-40 dB above ambient 96 tiles away, 20-30 dB above 312 tiles away

    // Loudness 96 was a signal of 8, and a loudness of 312 was a signal of 26
    // An old loudness of 160 for 12 gauge 00 buck from a shotgun would have a signal would have a signal of 13.333
    // (160dB is about right for a 20" barrel 12 gauge, but most shotguns are 150-156 dB)
    // Old 9mm pistol loudness was exactly 96 for JHP, for a signal of 8. IRL they produce just shy of 160 dB at the shooters ear, which would be ~13 signal?
    // .50 BMG had a loudness of 402 at the lowest, and IRL out of a barret is ~170 dB 1m from the barrel (180 dB 1 ft from the barrel!)
    // Explosions sorta cap out at 194 dB because of physics. They dont really get to have a sound wave until they are done being a supersonic shockwave.

    // How humans perceive sound is wonky, dB differences below 3 are not really perceptible.
    // Noticable differences in sound start at a difference of 5 dB, a sound is perceived as roughly 2x or 0.5x as loud around 10 dB difference, and about 4x or 1/4 as loud at around 20 dB difference.
    // A 10 dB difference is important, so we do need to work in the 10 dB lost from 96m to 312m somehow.
    // a 10 dB difference is effectively a 2x perceived loudness difference for the signal.
    //
    // Signal goes from 8 - 26, a range of 18. Effectively 12 tiles per signal point for the old noise logic
    //
    // Our minimum required dB is somewhere around 110 dB : 60 dB minimum ambient + 40 dB from distance + 10 dB to be twice as loud as ambient.
    // If we take the general dB volume for max signal as 170 dB, gives us a range of 60 dB, or 3 signal per 10 dB ( 1 signal per ~3.3333 dB )
    else {
        // Grid size is 12 by default. Retained as a reference comment, sound does not decrease linearly
        // const int hordes_sig_div = SEEX;
        //Signal for hordes can't be lower that this if it pass min_vol_cap, 8 * 12 = 96
        const int min_sig_cap = 8;
        //Signal for hordes can't be higher that this, 26 * 12 = 312
        const int max_sig_cap = 26;
        //Lower the level - lower the sound
        //Calculating horde hearing signal
        int sig_power = 8 + std::ceil( ( static_cast<float>( vol ) / 3.333 ) );
        //Capping minimum horde hearing signal
        sig_power = std::max( sig_power, min_sig_cap );
        //Capping extremely high signal to hordes
        sig_power = std::min( sig_power, max_sig_cap );
        add_msg( m_debug, _( "vol %i  vol_hordes %i sig_power %i " ), centr.volume, vol, sig_power );
        return sig_power;
    }
}

// Proccess sounds for monsters.
void sounds::process_sounds()
{
    ZoneScoped;

    map &map = get_map();

    // If the player is underground there is effectively no wind or significant weather noises.
    // However we still assume a minimum above ground ambient of 40dB, and a minimum underground of 20dB
    // We just check based pm
    // bool playerunderground = ( get_player_character().pos().z < 0 );
    // Weather conditions are very important for sound attenuation over distance
    const weather_manager &weather = get_weather();
    // Weather sound attenuation * 2, which we add to ambient noise. sound_attn ranges from 0-8
    const short weather_vol = ( weather.weather_id->sound_attn );

    // Wind can also heavily attenuate sound. Windspeed *should* be in mph.
    // This is a bad estimate based on the volume of wind found by this study https://pubmed.ncbi.nlm.nih.gov/28742424/
    // Which places volume due to 10mph winds at ~85 dB, and volume at 60mph at ~120 dB, and
    // OHSA reccommends motorcyclists riding at speeds above 37mph to wear hearing protection, as they can be exposed to sounds between 75-90 dB.
    // As a bad but conservative measurment for gameplay purposes, sound due to windspeed is 40 + windspeed dB. Capped at 180, if for whatever reason the game gives out 130mph winds.
    // This is not very close to realism at low wind speeds, but we are taking this as an ambient volume below which sounds will be difficult to hear.
    // A proper atmospherics dB calc does not offer enough improvement to gameplay to be worth the processing power.
    const short wind_volume = ( std::min( 150, weather.windspeed ) );

    // For use with horde signal terrain attenuation.
    const season_type season = season_of_year( calendar::turn );

    auto &sound_caches = map.sound_caches;

    // Its assumed when indoors that you can atleast somewhat hear the outside weather, even if its quiet.
    // Walls and rooves can effectively amplify the noise of rain and other similar weather.
    // Weather sound attenuation goes from 0-8, so just take sound attenuation *2 for added volume.
    const short INDOOR_AMBIENT = AMBIENT_VOLUME_ABOVEGROUND + dBspl_to_mdBspl( 2 * weather_vol );
    // We also use this as the base ambient to measure horde signals against.
    const short OUTDOOR_AMBIENT = AMBIENT_VOLUME_ABOVEGROUND + dBspl_to_mdBspl(
                                      wind_volume + weather_vol );

    // How loud is our ambient at a specific zlevel in mdB spl?
    // Below ground our ambient minimum is 20dB, 2000mdB
    // Because we are going to use this with all monsters its faster to just precalc our three conditions and return those.
    // We are either underground, aboveground but indoors, or outdoors above ground.
    auto ambient = [&]( const int zlev, const bool indoors = false ) {
        if( zlev < 0 ) {
            return AMBIENT_VOLUME_UNDERGROUND;
        } else if( indoors ) {
            return INDOOR_AMBIENT;
        } else {
            return OUTDOOR_AMBIENT;
        }
    };


    // Sound is approximated to lose 42 dB for every interveening z level of solid terrain. This only really happens underground.
    // Maximum sound lost is 191 dB, i.e. the loudest a sound can be.
    // Returns sound in mdB spl, or dB spl if horde signal true.
    auto vol_z_adjust = [&]( const int source_zlev, const int listener_zlev, bool for_horde_signal ) {
        const int max_vol = ( for_horde_signal ) ? mdBspl_to_dBspl( MAXIMUM_VOLUME_ATMOSPHERE ) :
                            MAXIMUM_VOLUME_ATMOSPHERE;
        const int per_zlev = ( for_horde_signal ) ? mdBspl_to_dBspl( SOUND_ABSORPTION_PER_ZLEV ) :
                             SOUND_ABSORPTION_PER_ZLEV;
        const bool listener_below_source = listener_zlev < source_zlev;
        // If either zlev is underground and they are not the same zlev, we find the lowest of either the maximum volume, or the per_zlev * however many underground z levels there are between the two.
        const short vol_adjust = ( ( source_zlev < 0 || listener_zlev < 0 ) &&
                                   source_zlev != listener_zlev ) ? std::min( max_vol,
                                           ( ( listener_below_source ) ? ( per_zlev * std::abs( listener_zlev - std::min( source_zlev,
                                                   0 ) ) ) : ( per_zlev * std::abs( source_zlev - std::min( listener_zlev, 0 ) ) ) ) ) : 0;
        // const short vol_adjust = ( source_zlev < 0 && source_zlev != listener_zlev ) ? std::min( max_vol,( per_zlev * ( std::abs( std::min( listener_zlev, 0 ) - source_zlev ) ) ) ) : 0;
        return vol_adjust;
    };

    // Store our ground level ambient in dB for the horde signal check incase there are alot of sounds.
    const short ground_ambient_vol = mdBspl_to_dBspl( OUTDOOR_AMBIENT );
    for( auto &sound : sound_caches ) {

        // Mark all our sound_caches as heard by monsters, easier to do here than reiterate later.
        sound.heard_by_monsters = true;
        // Sounds louder than 110dB are potentially valid for horde signal.
        // Horde signals are broadcasted at z-level 0, which does mean that sounds below ground are significantly less likely to generate horde signals.
        // Its impossible for signals below zlevel 2 to be loud enough for horde signals, so just skip them. (42dB * 3 = 126dB volume reduction)
        const auto &s_origin = sound.sound.origin;
        if( s_origin.z < -2 ) {
            continue;
        }
        const short alt_adjust = vol_z_adjust( ( sound.sound.origin.z ), 0, true );
        if( ( ( sound.sound.volume ) - alt_adjust ) >= 110 ) {
            const tripoint_abs_omt abs_omt( sm_to_omt_copy( s_origin ) );
            const short default_terrain_absorption = terrain_sound_attenuation( abs_omt, season, true );

            const int sig_power = get_signal_for_hordes( sound.sound, ground_ambient_vol,
                                  default_terrain_absorption,
                                  alt_adjust );
            if( sig_power > 0 ) {
                overmap_buffer.signal_hordes( tripoint_abs_sm( point_abs_sm( ms_to_sm_copy( map.getabs(
                                                  s_origin.xy() ) ) ), s_origin.z ), sig_power );

            }
        }
    }

    // Lets run through all the monsters and feed them sound info.
    // Monsters just go to the loudest thing they hear, so we run through that here.
    // Monsters ignore movement sounds from their own faction, a bit omiscient but it simplifies things.
    for( monster &critter : g->all_monsters() ) {

        // Monster is deaf, skip. We also skip hallucinations.
        if( !critter.can_hear() || critter.is_hallucination() ) {
            continue;
        }

        const auto &critterfact = critter.faction.id();

        // Is our monster afraid of sounds and nearby enemies? If so, we will use slightly expanded logic.
        // The vast majority of "dumb" monsters do not have fear triggers for sound or nearby enemies.
        const bool fears_sounds = critter.type->has_fear_trigger( mon_trigger::SOUND );
        const bool fears_enemy_sounds = fears_sounds &&
                                        ( critter.type->has_fear_trigger( mon_trigger::HOSTILE_CLOSE ) );

        // If our monster is a player pet, they use expanded logic.
        const bool player_ally = critter.friendly != 0;
        const bool horde_monster = !player_ally && !fears_enemy_sounds;

        const auto &critterloc = critter.pos();

        // Making a copy of the sound event and cycling is relatively low weight and helps prevent dependancy issues.
        // Not const so we can dynamically set it to the loudest sound_event in the monster's tile.
        // If this is to performance heavy alternatives can be looked at.
        sound_event loudest_sound{};
        short loudest_vol = 0;
        const bool goodhearing = critter.has_flag( MF_GOODHEARING );

        // Grab the ambient volume at the critter in dB spl
        const short critter_ambient_vol = ( ambient( critterloc.z, !map.is_outside( critterloc ) ) );

        // Working in mdB when dealing with tile volumes.
        // Make sure our volume threshold is not nonsensically low. Underground monsters are especially prone to getting too or below the volume threshold.
        // This results in them technically hearing all sounds on the map, which is not ideal. Volume threshold is set to be atleast 10dB for sanity and performance reasons.
        const short critter_vol_threshold = std::max( ( critter_ambient_vol -
                                            ( goodhearing ? 2000 : 1000 ) ), 1000 );

        const auto &critterx = critterloc.x;
        const auto &crittery = critterloc.y;

        for( auto &sound : sound_caches ) {
            const auto &s_category = sound.sound.category;
            // Lets do a quick check to see if there is any possibility of hearing the sound at all, or if we should care at all.
            // If the sound is footsteps from a monster, skip it.
            // While this is not ideal for zombie vs. other monsters this optimization is to combat performance drop with lots of monsters in the reality bubble.
            if( sound.volume[critterx][crittery] < critter_vol_threshold ||
                s_category == sounds::sound_t::background || s_category == sounds::sound_t::weather ||
                ( horde_monster && sound.movement_noise && sound.from_monster ) ) {
                continue;
            }

            // Sound is approximated to lose 42 dB for every interveening z level of solid terrain. This only really happens underground.
            // Maximum sound lost is 191 dB, i.e. the loudest a sound can be.
            const short heard_vol = sound.volume[critterx][crittery] - vol_z_adjust( sound.sound.origin.z,
                                    critterloc.z, false );
            if( heard_vol >= critter_vol_threshold ) {

                // If we are not a horde monster, we get to potentially use better logic.
                if( !horde_monster ) {

                    const auto &source_mfac = sound.sound.monfaction;
                    const auto &source_fac = sound.sound.faction;
                    const bool source_mfac_valid = source_mfac.is_valid();
                    const bool source_fac_valid = source_fac.is_valid();

                    if( ( player_ally || fears_enemy_sounds ) && source_fac_valid && source_mfac_valid ) {

                        // If we are a player ally or if we are afraid of sounds, check to see if we skip the sound based on designated sound faction.
                        if( player_ally && ( sound.from_player || source_fac == faction_id( "your_followers" ) ||
                                             source_mfac == critterfact ) && s_category < sounds::sound_t::alarm ) {
                            continue;

                        } else if( fears_enemy_sounds && source_mfac == critterfact ) {
                            continue;

                        }
                        if( fears_enemy_sounds && source_mfac_valid ) {
                            const auto source_mon_att = sound.sound.monfaction->attitude( critterfact );

                            // If the source is not friendly or neutral to us, run away!
                            if( source_mon_att == MFA_BY_MOOD || source_mon_att == MFA_HATE || ( !player_ally &&
                                    sound.from_player ) ) {
                                critter.hear_sound( sound.sound, heard_vol, critter_ambient_vol, false, true );
                                continue;
                            }
                        }
                        if( player_ally ) {
                            const bool source_ally = sound.from_player ||
                                                     critterfact->attitude( source_mfac ) == MFA_FRIENDLY ||
                                                     source_fac == faction_id( "your_followers" );

                            // Are we a brave monster?
                            if( !fears_enemy_sounds && s_category == sounds::sound_t::combat ) {

                                // Do our friends need help?
                                if( source_ally ) {

                                    // Sally forth.
                                    critter.hear_sound( sound.sound, heard_vol, critter_ambient_vol, true, false );
                                    continue;
                                }
                            } else if( fears_sounds && source_ally ) {

                                // Sound is from our friends, no need to worry.
                                continue;
                            }

                        }
                    }

                }
                // If we are a horde monster or none of the above checks passed, we only want the loudest volume in the tile.
                // If the current loudest volume is louder than the volume of a sound in the critters tile, skip it
                if( ( heard_vol > loudest_vol ) ) {
                    // If the new sound is louder, update the values and keep going.
                    loudest_vol = ( heard_vol );
                    loudest_sound = sound.sound;
                    continue;
                }

            }
        }
        // If we are afraid of sounds, run away!
        critter.hear_sound( loudest_sound, loudest_vol, critter_ambient_vol, false, fears_sounds );
    }
}

// Ensure description ends with punctuation, using a preferred character if missing
static auto ensure_punctuation = []( const std::string &desc, char preferred )
{
    if( desc.empty() ) {
        return desc;
    }
    char last = desc.back();
    if( last == '.' || last == '!' || last == '?' || last == '"' ) {
        return desc;
    }
    return desc + preferred;
};

// skip some sounds to avoid message spam
static bool describe_sound( sounds::sound_t category, bool from_player_position )
{
    if( from_player_position ) {
        switch( category ) {
            case sounds::sound_t::_LAST:
                debugmsg( "ERROR: Incorrect sound category" );
                return false;
            case sounds::sound_t::background:
            case sounds::sound_t::weather:
            case sounds::sound_t::music:
            // detailed music descriptions are printed in iuse::play_music
            case sounds::sound_t::movement:
            case sounds::sound_t::activity:
            case sounds::sound_t::destructive_activity:
            case sounds::sound_t::combat:
            case sounds::sound_t::alert:
            case sounds::sound_t::order:
            case sounds::sound_t::speech:
                return false;
            case sounds::sound_t::electronic_speech:
            case sounds::sound_t::alarm:
                return true;
        }
    } else {
        switch( category ) {
            case sounds::sound_t::background:
            case sounds::sound_t::weather:
            case sounds::sound_t::music:
            case sounds::sound_t::movement:
            case sounds::sound_t::activity:
            case sounds::sound_t::destructive_activity:
                return one_in( 100 );
            case sounds::sound_t::speech:
            case sounds::sound_t::electronic_speech:
            case sounds::sound_t::alarm:
            case sounds::sound_t::combat:
            case sounds::sound_t::alert:
            case sounds::sound_t::order:
                return true;
            case sounds::sound_t::_LAST:
                debugmsg( "ERROR: Incorrect sound category" );
                return false;
        }
    }
    return true;
}

void sounds::process_sounds_npc()
{
    ZoneScoped;
    auto &u = get_avatar();
    auto &map = get_map();
    auto &sound_vector = map.sound_caches;
    const weather_manager &weather = get_weather();
    // Set all of our sounds to be heard by NPCs for culling purposes.
    // If the player is underground there is effectively no wind or significant weather noises.
    // However we still assume a minimum above ground ambient of 40dB, and a minimum underground of 20dB
    // We just check based pm
    // bool playerunderground = ( get_player_character().pos().z < 0 );
    // Weather conditions are very important for sound attenuation over distance
    // Weather sound attenuation * 2, which we add to ambient noise. sound_attn ranges from 0-8
    const short weather_vol = ( weather.weather_id->sound_attn );

    // Wind can also heavily attenuate sound. Windspeed *should* be in mph.
    // This is a bad estimate based on the volume of wind found by this study https://pubmed.ncbi.nlm.nih.gov/28742424/
    // Which places volume due to 10mph winds at ~85 dB, and volume at 60mph at ~120 dB, and
    // OHSA reccommends motorcyclists riding at speeds above 37mph to wear hearing protection, as they can be exposed to sounds between 75-90 dB.
    // As a bad but conservative measurment for gameplay purposes, sound due to windspeed is 40 + windspeed dB. Capped at 180, if for whatever reason the game gives out 130mph winds.
    // This is not very close to realism at low wind speeds, but we are taking this as an ambient volume below which sounds will be difficult to hear.
    // A proper atmospherics dB calc does not offer enough improvement to gameplay to be worth the processing power.
    const short wind_volume = ( std::min( 150, weather.windspeed ) );

    // Its assumed when indoors that you can atleast somewhat hear the outside weather, even if its quiet.
    // Walls and rooves can effectively amplify the noise of rain and other similar weather.
    // Weather sound attenuation goes from 0-8, so just take sound attenuation *2 for added volume.
    const short INDOOR_AMBIENT = AMBIENT_VOLUME_ABOVEGROUND + dBspl_to_mdBspl( 2 * weather_vol );
    // We also use this as the base ambient to measure horde signals against.
    const short OUTDOOR_AMBIENT = AMBIENT_VOLUME_ABOVEGROUND + dBspl_to_mdBspl(
                                      wind_volume + weather_vol );

    // How loud is our ambient at a specific zlevel in mdB spl?
    // Below ground our ambient minimum is 20dB, 2000mdB
    // Because we are going to use this with all monsters its faster to just precalc our three conditions and return those.
    // We are either underground, aboveground but indoors, or outdoors above ground.
    auto ambient = [&]( const int zlev, const bool indoors = false ) {
        if( zlev < 0 ) {
            return AMBIENT_VOLUME_UNDERGROUND;
        } else if( indoors ) {
            return INDOOR_AMBIENT;
        } else {
            return OUTDOOR_AMBIENT;
        }
    };

    // Sound is approximated to lose 42 dB for every interveening z level of solid terrain. This only really happens underground.
    // Maximum sound lost is 191 dB, i.e. the loudest a sound can be.
    // Returns sound in mdB spl, or dB spl if horde signal true.
    auto vol_z_adjust = [&]( const int source_zlev, const int listener_zlev, bool for_horde_signal ) {
        const int max_vol = ( for_horde_signal ) ? mdBspl_to_dBspl( MAXIMUM_VOLUME_ATMOSPHERE ) :
                            MAXIMUM_VOLUME_ATMOSPHERE;
        const int per_zlev = ( for_horde_signal ) ? mdBspl_to_dBspl( SOUND_ABSORPTION_PER_ZLEV ) :
                             SOUND_ABSORPTION_PER_ZLEV;
        const bool listener_below_source = listener_zlev < source_zlev;
        // If either zlev is underground and they are not the same zlev, we find the lowest of either the maximum volume, or the per_zlev * however many underground z levels there are between the two.
        const short vol_adjust = ( ( source_zlev < 0 || listener_zlev < 0 ) &&
                                   source_zlev != listener_zlev ) ? std::min( max_vol,
                                           ( ( listener_below_source ) ? ( per_zlev * std::abs( listener_zlev - std::min( source_zlev,
                                                   0 ) ) ) : ( per_zlev * std::abs( source_zlev - std::min( listener_zlev, 0 ) ) ) ) ) : 0;
        // const short vol_adjust = ( source_zlev < 0 && source_zlev != listener_zlev ) ? std::min( max_vol,( per_zlev * ( std::abs( std::min( listener_zlev, 0 ) - source_zlev ) ) ) ) : 0;
        return vol_adjust;
    };
    // Set our current sound caches to heard_by_npcs instead of doing it repeatedly with each NPC.
    for( auto &element : sound_vector ) {
        element.heard_by_npcs = true;
    }
    // Now we work through all of our active NPCs.
    for( npc &who : g->all_npcs() ) {
        if( rl_dist( who.pos(), u.pos() ) < MAX_VIEW_DISTANCE ) {
            bool is_deaf = who.is_deaf();
            auto &loc = who.pos();
            const float volume_multiplier = who.hearing_ability();
            const short deafening_threshold = std::max( 0.0f,
                                              std::floor( 12000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;
            const short deafening_garuntee = std::max( 0.0f,
                                             std::floor( 17000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;
            // How far below ambient can this character hear? Default of 20dB, caps out at 30dB below ambient for sanity.
            // The player character gets a better calc, but these are NPCs and we dont love them enough.
            const short below_ambient = std::min( 3000.0f,
                                                  ( std::floor( 1500 + 500 * volume_multiplier ) ) );

            const short ambient_vol = ambient( loc.z, map.is_outside( loc.xy() ) );
            // Passive sound dampening reduces all heard volume by a set amount, but protects against hearing loss by 2x this amount.
            const short passive_sound_dampening = dBspl_to_mdBspl( who.get_char_hearing_protection() );
            // Active dampening does not reduce heard volume and directly protects against hearing loss.
            const short active_sound_dampening = dBspl_to_mdBspl( who.get_char_hearing_protection( true ) );
            // We want constant ints for our x/y, makes the compiler happier when getting volume[x][y].
            const int charx = loc.x;
            const int chary = loc.y;
            // Sounds quieter than this are inaudible and are skipped.
            // Passive sound dampening reduces the "heard" volume of all sounds, including ambient volume.
            // In a perfect simulation most hearing protection absorbs high frequency sounds much more than low frequency sounds.
            // We cap our minimum at 10dB to prevent underground NPCs from hearing everything everywhere on the entire map.
            const short min_vol = std::max( 1000, ( ambient_vol - below_ambient + passive_sound_dampening ) );

            // dBspl is a root-mean-square value so while all the volumes in the tile should be cumulative,
            // proper tile volume would follow the formula sqrt((1/n)*(v1^2 + v2^2+ ... + vn^2)) where n is the number of volumes.
            // In general practice unless there are only 20+ copies of the same sound in a tile the volume is dominated by the loudest sound volume.
            // 100dB + 20dB + 80dB +70dB = ~101dB  So we just take the loudest.
            for( auto &element : sound_vector ) {
                // Do an early filter for sounds that would always be indaudible.
                // Check to see if the NPC is deaf here as well, as we may deafen them part way through the process.
                auto &tile_vol = element.volume[charx][chary];

                if( tile_vol == 0 ) {
                    continue;
                }

                const short adjusted_vol = tile_vol - vol_z_adjust( element.sound.origin.z, loc.z, false );

                if( adjusted_vol  > min_vol && !who.is_deaf() ) {

                    // We only want to feed NPC AI sounds they should react to.
                    // This is more than a bit hackey and gives the NPCs a bit of omniscience,
                    // but we dont want NPCs going out to investigate every single sound under the sun.
                    if( ( element.from_player || element.from_monster ||
                          element.from_npc ) ) {

                        who.handle_sound( ( adjusted_vol - passive_sound_dampening ), element.sound );
                    }
                }
                // Deafening is based on the felt volume, as an NPC may be too deaf to
                // hear the deafening sound but still suffer additional hearing loss.
                // Threshold for instant hearing loss is 14000mdB
                // Volume for garunteed deafening is 17000mdB
                if( tile_vol - ( ( passive_sound_dampening * 2 ) + active_sound_dampening )  >=
                    deafening_threshold ) {
                    const bool is_sound_deafening = ( tile_vol - ( ( passive_sound_dampening * 2 ) +
                                                      active_sound_dampening ) )
                                                    >= rng( deafening_threshold, deafening_garuntee );

                    // Deaf NPCs hear no sound, but still are at risk of additional hearing loss.
                    if( is_deaf ) {
                        if( is_sound_deafening && !who.is_immune_effect( effect_deaf ) ) {
                            who.add_effect( effect_deaf, std::min( 4_minutes,
                                                                   time_duration::from_turns( mdBspl_to_dBspl( tile_vol - ( ( passive_sound_dampening * 2 ) +
                                                                           active_sound_dampening ) ) - 130 ) ) );
                            if( !who.has_trait( trait_id( "NOPAIN" ) ) ) {
                                if( who.get_pain() < 10 ) {
                                    who.mod_pain( rng( 0, 2 ) );
                                }
                            }
                        }

                    }

                    if( is_sound_deafening && !who.is_immune_effect( effect_deaf ) ) {
                        const time_duration deafness_duration = time_duration::from_turns( mdBspl_to_dBspl(
                                tile_vol - ( ( passive_sound_dampening * 2 ) + active_sound_dampening ) ) - 130 );
                        who.add_effect( effect_deaf, deafness_duration );
                        if( who.is_deaf() && !is_deaf ) {
                            is_deaf = true;

                        }
                    }
                }
            }

        }
    }
}

void sounds::process_sound_markers( Character *who )
{

    bool is_deaf = who->is_deaf();
    const float volume_multiplier = who->hearing_ability();
    auto &loc = who->pos();
    auto &map = get_map();
    auto &sound_vector = map.sound_caches;
    // We want constant ints for our x/y, makes the compiler happier when getting cache[x][y].
    const int charx = loc.x;
    const int chary = loc.y;
    // How far below ambient can this character hear? Default of 20dB, uncapped unlike NPCs.
    const short below_ambient = std::floor( 1500 + 500 * volume_multiplier );
    // is the npc underground?
    const bool pcunderground = loc.z < 0;
    const bool pcoutdoors = map.is_outside( loc.xy() );
    const weather_manager &weather = get_weather();
    // Ambient underground is 20dB, ambient in a above ground building is 40. The assumption is that there are zombies making noise, and its not perfectly dead quiet.
    // Weather sound attenuation ranges from 0 - 8. We add this to existing ambient if applicable to approximate the sound of rain, snow, etc.
    const short weather_vol = dBspl_to_mdBspl( ( pcunderground &&
                              !pcoutdoors ) ? 0 : ( !pcunderground &&
                                      !pcoutdoors ) ? 2 * ( weather.weather_id->sound_attn ) : weather.weather_id->sound_attn );
    // Wind volume should be somewhere VAUGELY around 40dB+mph in reality, however Cata frequently simulates absolutely batshit insane steady windspeeds.
    const short wind_volume = dBspl_to_mdBspl( ( pcunderground ||
                              !pcoutdoors ) ? 0 : weather.windspeed );
    const short ambient_vol = wind_volume + weather_vol + dBspl_to_mdBspl( (
                                  pcunderground ) ? 20 : 40 );
    const short passive_sound_dampening = dBspl_to_mdBspl( who->get_char_hearing_protection() );
    const short active_sound_dampening = dBspl_to_mdBspl( who->get_char_hearing_protection( true ) );

    // Deafening is based on the loudest volume at that tile.
    // hear the deafening sound but still suffer additional hearing loss.
    // The threshold for pain is generally taken as 140dB spl. The NIOSH daily safe exposure for 115dB sounds is ~28 seconds, 120dB sounds have a daily safe exposure of less than 2 seconds.
    // Threshold for instant hearing loss is 1200mdB
    // Volume for garunteed deafening is 1700mdB
    const short deafening_threshold = std::max( 0.0f,
                                      std::floor( 12000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;
    const short deafening_garuntee = std::max( 0.0f,
                                     std::floor( 17000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;

    auto vol_z_adjust = [&]( const int source_zlev, const int listener_zlev ) {
        const int max_vol = MAXIMUM_VOLUME_ATMOSPHERE;
        const int per_zlev =  SOUND_ABSORPTION_PER_ZLEV;
        const bool listener_below_source = listener_zlev < source_zlev;
        // If either zlev is underground and they are not the same zlev, we find the lowest of either the maximum volume, or the per_zlev * however many underground z levels there are between the two.
        const short vol_adjust = ( ( source_zlev < 0 || listener_zlev < 0 ) &&
                                   source_zlev != listener_zlev ) ? std::min( max_vol,
                                           ( ( listener_below_source ) ? ( per_zlev * std::abs( listener_zlev - std::min( source_zlev,
                                                   0 ) ) ) : ( per_zlev * std::abs( source_zlev - std::min( listener_zlev, 0 ) ) ) ) ) : 0;
        // const short vol_adjust = ( source_zlev < 0 && source_zlev != listener_zlev ) ? std::min( max_vol,( per_zlev * ( std::abs( std::min( listener_zlev, 0 ) - source_zlev ) ) ) ) : 0;
        return vol_adjust;
    };

    // Lets figure out our loudest volume in tile.
    // We dont actually really care about the details here, we just want to know what sound to set the players sound panel reading to and if we should deafen the player.
    // Also go through and mark all the sounds as heard by the player.
    short loudest_vol = ambient_vol;
    for( auto &element : sound_vector ) {
        element.heard_by_player = true;
        auto &tile_vol = element.volume[charx][chary];
        // Do an early filter for all the 0 volume sounds
        if( tile_vol > 0 ) {
            const short adjusted_vol = tile_vol - vol_z_adjust( element.sound.origin.z,
                                       loc.z );
            loudest_vol = std::max( adjusted_vol, loudest_vol );
        }
    }
    who->volume = static_cast<int>( mdBspl_to_dBspl( loudest_vol ) );


    // Cant hear noises that are significantly quieter than the loudest noise you are currently hearing,
    // Softer sounds just get drowned out. Minimum of 10dB to prevent hearing all the sounds on the map and filling the screen with sound markers.
    const short vol_threshold = passive_sound_dampening + std::max( 1000,
                                ( loudest_vol - below_ambient ) );

    for( auto &element : sound_vector ) {

        auto &tile_vol = element.volume[charx][chary];


        // Do an early filter to only check sounds we have a chance of hearing.
        if( tile_vol > vol_threshold ) {

            // What is our adjusted volume for this tile, including from passive sound dampening and z-level adjustments?
            const short adjusted_vol = std::max( 0, tile_vol - vol_z_adjust( element.sound.origin.z,
                                                 loc.z ) );

            // If the sound is loud enough, inform the player of it.
            if( adjusted_vol > vol_threshold ) {

                // Deafening is based on the felt volume, as a player may be too deaf to
                // hear the deafening sound but still suffer additional hearing loss.
                // Is the loudest tile volume louder than the deafening threshold?
                // Passive sound dampening counts 2x for protecting against hearing loss compared to is normal volume adjustment to approximate hearing protection working more effectively against harmful high frequency sounds.
                const short deafening_vol = std::max( 0,
                                                      adjusted_vol - ( active_sound_dampening + passive_sound_dampening + passive_sound_dampening ) );
                if( ( deafening_vol >= deafening_threshold ) || is_deaf ) {
                    const bool is_sound_deafening = ( deafening_vol )
                                                    >= rng( deafening_threshold, deafening_garuntee );

                    // A deaf player hear no sound, but they are still at risk of additional hearing loss.
                    if( is_deaf ) {
                        if( is_sound_deafening && !who->is_immune_effect( effect_deaf ) ) {
                            who->add_effect( effect_deaf, std::min( 4_minutes,
                                                                    time_duration::from_turns( mdBspl_to_dBspl( deafening_vol ) - 130 ) ) );
                            if( !who->has_trait( trait_id( "NOPAIN" ) ) ) {
                                who->add_msg_if_player( m_bad, _( "Your eardrums suddenly ache!" ) );
                                if( who->get_pain() < 10 ) {

                                    who->mod_pain( rng( 0, 2 ) );
                                }
                            }
                        }
                        continue;
                    }

                    if( is_sound_deafening && !who->is_immune_effect( effect_deaf ) ) {
                        const time_duration deafness_duration = time_duration::from_turns( mdBspl_to_dBspl(
                                deafening_vol ) - 130 );
                        who->add_effect( effect_deaf, deafness_duration );
                        if( who->is_deaf() && !is_deaf ) {
                            is_deaf = true;
                            continue;
                        }
                    }
                }
                // Direct distance to the sound source. elevation effects are handled by the z level adjust.
                const int distance_to_sound = rl_dist( loc, element.sound.origin );

                // Secure the flag before wake_up() clears the effect
                bool slept_through = who->has_effect( effect_slept_through_alarm );
                // Grab the decibel value of our adjusted vol for use with comparisons etc.
                const int db_vol = mdBspl_to_dBspl( adjusted_vol - passive_sound_dampening );
                // See if we need to wake someone up
                // Remember we are working with dB spl volumes instead of tile volumes and dB spl is a logarithmic unit. 60dB is normal conversation, 80-100 is a car horn, ~160 is a gunshot, 180+ can kill a human.
                // We want somewhat less swingy results, so use d10s
                // Noise past 60dB should automatically wake up not heavy sleepers.
                // Noise past 100dB should automatically wake up heavy sleepers.
                // Noise past 120dB will cause pain and should automatically wake up heavy sleeper 2.
                if( who->has_effect( effect_sleep ) ) {
                    if( ( ( !( who->has_trait( trait_HEAVYSLEEPER ) ||
                               who->has_trait( trait_HEAVYSLEEPER2 ) ) && dice( 6, 10 ) <= db_vol ) ||
                          ( who->has_trait( trait_HEAVYSLEEPER ) && dice( 10, 10 ) <= db_vol ) ||
                          ( who->has_trait( trait_HEAVYSLEEPER2 ) && dice( 12, 10 ) <= db_vol ) ) &&
                        !who->has_effect( effect_narcosis ) ) {
                        //Not kidding about sleep-through-firefight
                        who->wake_up();
                        who->add_msg_if_player( m_warning, _( "Something is making noise." ) );
                    } else {
                        continue;
                    }
                }
                const std::string &description = element.sound.description.empty() ? _( "a noise" ) :
                                                 element.sound.description;

                // don't print our own noise or things without descriptions
                if( ( element.sound.from_monster || element.sound.from_player || element.sound.from_npc ) &&
                    ( element.sound.origin != who->pos() ) &&
                    !get_map().pl_sees( element.sound.origin, distance_to_sound ) ) {
                    if( !who->activity->is_distraction_ignored( distraction_type::noise ) &&
                        !get_safemode().is_sound_safe( element.sound.description, distance_to_sound ) ) {
                        const std::string final_description = ensure_punctuation( description, '!' );
                        const std::string query = string_format( _( "Heard %s!" ), final_description );
                        g->cancel_activity_or_ignore_query( distraction_type::noise, query );
                    }
                }

                // skip some sounds to avoid message spam
                if( describe_sound( element.sound.category, element.sound.origin == who->pos() ) ) {
                    game_message_type severity = m_info;
                    if( element.sound.category == sound_t::combat || element.sound.category == sound_t::alarm ) {
                        severity = m_warning;
                    }

                    std::string final_description = ensure_punctuation( description, '.' );

                    // if we can see it, don't print a direction
                    if( element.sound.origin == who->pos() ) {
                        add_msg( severity, _( "From your position you hear %1$s" ), final_description );
                    } else if( who->sees( element.sound.origin ) ) {
                        add_msg( severity, _( "You hear %1$s" ), final_description );
                    } else {
                        std::string direction = direction_name( direction_from( who->pos(), element.sound.origin ) );
                        add_msg( severity, _( "From the %1$s you hear %2$s" ), direction, final_description );
                    }
                }

                if( !who->has_effect( effect_sleep ) && who->has_effect( effect_alarm_clock ) &&
                    !who->has_bionic( bionic_id( "bio_infolink" ) ) ) {
                    // if we don't have effect_sleep but we're in_sleep_state, either
                    // we were trying to fall asleep for so long our alarm is now going
                    // off or something disturbed us while trying to sleep
                    const bool trying_to_sleep = who->in_sleep_state();
                    if( who->get_effect( effect_alarm_clock ).get_duration() == 1_turns ) {
                        if( slept_through ) {
                            add_msg( _( "Your alarm clock finally wakes you up." ) );
                        } else if( !trying_to_sleep ) {
                            add_msg( _( "Your alarm clock wakes you up." ) );
                        } else {
                            add_msg( _( "Your alarm clock goes off and you haven't slept a wink." ) );
                            who->activity->set_to_null();
                        }
                        add_msg( _( "You turn off your alarm-clock." ) );
                        who->get_effect( effect_alarm_clock ).set_duration( 0_turns );
                    }
                }

                const std::string &sfx_id = element.sound.id;
                const std::string &sfx_variant = element.sound.variant;
                if( !sfx_id.empty() ) {
                    sfx::play_variant_sound( sfx_id, sfx_variant, sfx::get_heard_volume( element.sound.origin ) );
                }

                // Place footstep markers.
                if( element.sound.origin == who->pos() || who->sees( element.sound.origin ) ) {
                    // If we are or can see the source, don't draw a marker.
                    continue;
                }

                int err_offset;
                if( ( db_vol + distance_to_sound ) / distance_to_sound < 2 ) {
                    err_offset = 3;
                } else if( ( db_vol + distance_to_sound ) / distance_to_sound < 3 ) {
                    err_offset = 2;
                } else {
                    err_offset = 1;
                }

                // If Z-coordinate is different, draw even when you can see the source
                const bool diff_z = element.sound.origin.z != who->posz();

                // Enumerate the valid points the player *cannot* see.
                // Unless the source is on a different z-level, then any point is fine
                std::vector<tripoint> unseen_points;
                for( const tripoint &newp : get_map().points_in_radius( element.sound.origin, err_offset ) ) {
                    if( diff_z || !who->sees( newp ) ) {
                        unseen_points.emplace_back( newp );
                    }
                }

                // Then place the sound marker in a random one.
                if( !unseen_points.empty() ) {
                    sound_markers.emplace( random_entry( unseen_points ), element.sound );
                }
            }
        }
    }
}
// Use map::cull_sound_caches for managing the sound_caches vector during play.
void sounds::reset_sounds()
{
    auto &map = get_map();
    map.sound_caches.clear();
    sound_markers.clear();
}

void sounds::reset_markers()
{
    sound_markers.clear();
}

std::vector<tripoint> sounds::get_footstep_markers()
{
    // Optimization, make this static and clear it in reset_markers?
    std::vector<tripoint> footsteps;
    footsteps.reserve( sound_markers.size() );
    for( const auto &mark : sound_markers ) {
        footsteps.push_back( mark.first );
    }
    return footsteps;
}

std::pair< std::vector<tripoint>, std::vector<tripoint>> sounds::get_monster_sounds()
{
    std::vector<tripoint> allsounds;
    std::vector<tripoint> monster_sounds;
    map &map = get_map();
    for( auto &soundcache : map.sound_caches ) {
        allsounds.emplace_back( soundcache.sound.origin );
        if( soundcache.from_monster ) {
            monster_sounds.emplace_back( soundcache.sound.origin );
        }
    }
    return { allsounds, monster_sounds };
}

std::string sounds::sound_at( const tripoint &location )
{
    auto this_sound = sound_markers.find( location );
    if( this_sound == sound_markers.end() ) {
        return std::string();
    }
    if( !this_sound->second.description.empty() ) {
        return this_sound->second.description;
    }
    return _( "a sound" );
}

#if defined(SDL_SOUND)
void sfx::fade_audio_group( group group, int duration )
{
    if( test_mode ) {
        return;
    }
    Mix_FadeOutGroup( static_cast<int>( group ), duration );
}

void sfx::fade_audio_channel( channel channel, int duration )
{
    if( test_mode ) {
        return;
    }
    Mix_FadeOutChannel( static_cast<int>( channel ), duration );
}

bool sfx::is_channel_playing( channel channel )
{
    if( test_mode ) {
        return false;
    }
    return Mix_Playing( static_cast<int>( channel ) ) != 0;
}

void sfx::stop_sound_effect_fade( channel channel, int duration )
{
    if( test_mode ) {
        return;
    }
    if( Mix_FadeOutChannel( static_cast<int>( channel ), duration ) == -1 ) {
        dbg( DL::Error ) << "Failed to stop sound effect: " << Mix_GetError();
    }
}

void sfx::stop_sound_effect_timed( channel channel, int time )
{
    if( test_mode ) {
        return;
    }
    Mix_ExpireChannel( static_cast<int>( channel ), time );
}

int sfx::set_channel_volume( channel channel, int volume )
{
    if( test_mode ) {
        return 0;
    }
    int ch = static_cast<int>( channel );
    if( !Mix_Playing( ch ) ) {
        return -1;
    }
    if( Mix_FadingChannel( ch ) != MIX_NO_FADING ) {
        return -1;
    }
    return Mix_Volume( ch, volume );
}

void sfx::do_vehicle_engine_sfx()
{
    if( test_mode ) {
        return;
    }

    static const channel ch = channel::interior_engine_sound;
    const Character &player_character = get_player_character();
    if( !player_character.in_vehicle ) {
        fade_audio_channel( ch, 300 );
        add_msg( m_debug, "STOP interior_engine_sound, OUT OF CAR" );
        return;
    }
    if( player_character.in_sleep_state() && !audio_muted ) {
        fade_audio_channel( channel::any, 300 );
        audio_muted = true;
        return;
    } else if( player_character.in_sleep_state() && audio_muted ) {
        return;
    }
    optional_vpart_position vpart_opt = get_map().veh_at( player_character.pos() );
    vehicle *veh;
    if( vpart_opt.has_value() ) {
        veh = &vpart_opt->vehicle();
    } else {
        return;
    }
    if( !veh->engine_on ) {
        fade_audio_channel( ch, 100 );
        add_msg( m_debug, "STOP interior_engine_sound" );
        return;
    }

    std::pair<std::string, std::string> id_and_variant;

    for( size_t e = 0; e < veh->engines.size(); ++e ) {
        if( veh->is_engine_on( e ) ) {
            if( sfx::has_variant_sound( "engine_working_internal",
                                        veh->part_info( veh->engines[ e ] ).get_id().str() ) ) {
                id_and_variant = std::make_pair( "engine_working_internal",
                                                 veh->part_info( veh->engines[ e ] ).get_id().str() );
            } else if( veh->is_engine_type( e, fuel_type_muscle ) ) {
                id_and_variant = std::make_pair( "engine_working_internal", "muscle" );
            } else if( veh->is_engine_type( e, fuel_type_wind ) ) {
                id_and_variant = std::make_pair( "engine_working_internal", "wind" );
            } else if( veh->is_engine_type( e, fuel_type_battery ) ) {
                id_and_variant = std::make_pair( "engine_working_internal", "electric" );
            } else {
                id_and_variant = std::make_pair( "engine_working_internal", "combustion" );
            }
        }
    }

    if( !is_channel_playing( ch ) ) {
        play_ambient_variant_sound( id_and_variant.first, id_and_variant.second,
                                    sfx::get_heard_volume( player_character.pos() ), ch, 1000 );
        add_msg( m_debug, "START %s %s", id_and_variant.first, id_and_variant.second );
    } else {
        add_msg( m_debug, "PLAYING" );
    }
    int current_speed = veh->velocity;
    bool in_reverse = false;
    if( current_speed <= -1 ) {
        current_speed = current_speed * -1;
        in_reverse = true;
    }
    double pitch = 1.0;
    int safe_speed = veh->safe_velocity();
    int current_gear;
    if( in_reverse ) {
        current_gear = -1;
    } else if( current_speed == 0 ) {
        current_gear = 0;
    } else if( current_speed > 0 && current_speed <= safe_speed / 12 ) {
        current_gear = 1;
    } else if( current_speed > safe_speed / 12 && current_speed <= safe_speed / 5 ) {
        current_gear = 2;
    } else if( current_speed > safe_speed / 5 && current_speed <= safe_speed / 4 ) {
        current_gear = 3;
    } else if( current_speed > safe_speed / 4 && current_speed <= safe_speed / 3 ) {
        current_gear = 4;
    } else if( current_speed > safe_speed / 3 && current_speed <= safe_speed / 2 ) {
        current_gear = 5;
    } else {
        current_gear = 6;
    }
    if( veh->has_engine_type( fuel_type_muscle, true ) ||
        veh->has_engine_type( fuel_type_wind, true ) ) {
        current_gear = previous_gear;
    }

    if( current_gear > previous_gear ) {
        play_variant_sound( "vehicle", "gear_shift", get_heard_volume( player_character.pos() ),
                            0_degrees, 0.8, 0.8 );
        add_msg( m_debug, "GEAR UP" );
    } else if( current_gear < previous_gear ) {
        play_variant_sound( "vehicle", "gear_shift", get_heard_volume( player_character.pos() ),
                            0_degrees, 1.2, 1.2 );
        add_msg( m_debug, "GEAR DOWN" );
    }
    if( ( safe_speed != 0 ) ) {
        if( current_gear == 0 ) {
            pitch = 1.0;
        } else if( current_gear == -1 ) {
            pitch = 1.2;
        } else {
            pitch = 1.0 - static_cast<double>( current_speed ) / static_cast<double>( safe_speed );
        }
    }
    pitch = std::max( pitch, 0.5 );

    if( current_speed != previous_speed ) {
        Mix_HaltChannel( static_cast<int>( ch ) );
        add_msg( m_debug, "STOP speed %d =/= %d", current_speed, previous_speed );
        play_ambient_variant_sound( id_and_variant.first, id_and_variant.second,
                                    sfx::get_heard_volume( player_character.pos() ), ch, 1000, pitch );
        add_msg( m_debug, "PITCH %f", pitch );
    }
    previous_speed = current_speed;
    previous_gear = current_gear;
}

void sfx::do_vehicle_exterior_engine_sfx()
{
    if( test_mode ) {
        return;
    }

    static const channel ch = channel::exterior_engine_sound;
    static const int ch_int = static_cast<int>( ch );
    const avatar &player_character = get_avatar();
    // early bail-outs for efficiency
    if( player_character.in_vehicle ) {
        fade_audio_channel( ch, 300 );
        add_msg( m_debug, "STOP exterior_engine_sound, IN CAR" );
        return;
    }
    if( player_character.in_sleep_state() && !audio_muted ) {
        fade_audio_channel( channel::any, 300 );
        audio_muted = true;
        return;
    } else if( player_character.in_sleep_state() && audio_muted ) {
        return;
    }

    VehicleList vehs = get_map().get_vehicles();
    unsigned char noise_factor = 0;
    unsigned char vol = 0;
    vehicle *veh = nullptr;

    for( wrapped_vehicle vehicle : vehs ) {
        if( vehicle.v->vehicle_noise > 0 &&
            vehicle.v->vehicle_noise -
            sound_distance( player_character.pos(), vehicle.v->global_pos3() ) > noise_factor ) {

            noise_factor = vehicle.v->vehicle_noise - sound_distance( player_character.pos(),
                           vehicle.v->global_pos3() );
            veh = vehicle.v;
        }
    }
    if( !noise_factor || !veh ) {
        fade_audio_channel( ch, 300 );
        add_msg( m_debug, "STOP exterior_engine_sound, NO NOISE" );
        return;
    }

    vol = MIX_MAX_VOLUME * noise_factor / veh->vehicle_noise;
    std::pair<std::string, std::string> id_and_variant;

    for( size_t e = 0; e < veh->engines.size(); ++e ) {
        if( veh->is_engine_on( e ) ) {
            if( sfx::has_variant_sound( "engine_working_external",
                                        veh->part_info( veh->engines[ e ] ).get_id().str() ) ) {
                id_and_variant = std::make_pair( "engine_working_external",
                                                 veh->part_info( veh->engines[ e ] ).get_id().str() );
            } else if( veh->is_engine_type( e, fuel_type_muscle ) ) {
                id_and_variant = std::make_pair( "engine_working_external", "muscle" );
            } else if( veh->is_engine_type( e, fuel_type_wind ) ) {
                id_and_variant = std::make_pair( "engine_working_external", "wind" );
            } else if( veh->is_engine_type( e, fuel_type_battery ) ) {
                id_and_variant = std::make_pair( "engine_working_external", "electric" );
            } else {
                id_and_variant = std::make_pair( "engine_working_external", "combustion" );
            }
        }
    }

    if( is_channel_playing( ch ) ) {
        if( engine_external_id_and_variant == id_and_variant ) {
            Mix_SetPosition( ch_int, to_degrees( get_heard_angle( veh->global_pos3() ) ), 0 );
            set_channel_volume( ch, vol );
            add_msg( m_debug, "PLAYING exterior_engine_sound, vol: ex:%d true:%d", vol, Mix_Volume( ch_int,
                     -1 ) );
        } else {
            engine_external_id_and_variant = id_and_variant;
            Mix_HaltChannel( ch_int );
            add_msg( m_debug, "STOP exterior_engine_sound, change id/var" );
            play_ambient_variant_sound( id_and_variant.first, id_and_variant.second, 128, ch, 0 );
            Mix_SetPosition( ch_int, to_degrees( get_heard_angle( veh->global_pos3() ) ), 0 );
            set_channel_volume( ch, vol );
            add_msg( m_debug, "START exterior_engine_sound %s %s vol: %d", id_and_variant.first,
                     id_and_variant.second,
                     Mix_Volume( ch_int, -1 ) );
        }
    } else {
        play_ambient_variant_sound( id_and_variant.first, id_and_variant.second, 128, ch, 0 );
        add_msg( m_debug, "Vol: %d %d", vol, Mix_Volume( ch_int, -1 ) );
        Mix_SetPosition( ch_int, to_degrees( get_heard_angle( veh->global_pos3() ) ), 0 );
        add_msg( m_debug, "Vol: %d %d", vol, Mix_Volume( ch_int, -1 ) );
        set_channel_volume( ch, vol );
        add_msg( m_debug, "START exterior_engine_sound NEW %s %s vol: ex:%d true:%d", id_and_variant.first,
                 id_and_variant.second, vol, Mix_Volume( ch_int, -1 ) );
    }
}

void sfx::do_ambient()
{
    if( test_mode ) {
        return;
    }

    Character &player_character = get_player_character();
    if( player_character.in_sleep_state() && !audio_muted ) {
        fade_audio_channel( channel::any, 300 );
        audio_muted = true;
        return;
    } else if( player_character.in_sleep_state() && audio_muted ) {
        return;
    }
    audio_muted = false;
    const bool is_deaf = player_character.is_deaf();
    const int heard_volume = get_heard_volume( player_character.pos() );
    const bool is_underground = player_character.pos().z < 0;
    const bool is_sheltered = g->is_sheltered( player_character.pos() );
    const bool weather_changed = get_weather().weather_id != previous_weather;
    // Step in at night time / we are not indoors
    if( is_night( calendar::turn ) && !is_sheltered &&
        !is_channel_playing( channel::nighttime_outdoors_env ) && !is_deaf ) {
        fade_audio_group( group::time_of_day, 1000 );
        play_ambient_variant_sound( "environment", "nighttime", heard_volume,
                                    channel::nighttime_outdoors_env, 1000 );
        // Step in at day time / we are not indoors
    } else if( !is_night( calendar::turn ) && !is_channel_playing( channel::daytime_outdoors_env ) &&
               !is_sheltered && !is_deaf ) {
        fade_audio_group( group::time_of_day, 1000 );
        play_ambient_variant_sound( "environment", "daytime", heard_volume, channel::daytime_outdoors_env,
                                    1000 );
    }
    // We are underground
    if( ( is_underground && !is_channel_playing( channel::underground_env ) &&
          !is_deaf ) || ( is_underground &&
                          weather_changed && !is_deaf ) ) {
        fade_audio_group( group::weather, 1000 );
        fade_audio_group( group::time_of_day, 1000 );
        play_ambient_variant_sound( "environment", "underground", heard_volume, channel::underground_env,
                                    1000 );
        // We are indoors
    } else if( ( is_sheltered && !is_underground &&
                 !is_channel_playing( channel::indoors_env ) && !is_deaf ) ||
               ( is_sheltered && !is_underground &&
                 weather_changed && !is_deaf ) ) {
        fade_audio_group( group::weather, 1000 );
        fade_audio_group( group::time_of_day, 1000 );
        play_ambient_variant_sound( "environment", "indoors", heard_volume, channel::indoors_env, 1000 );
    }

    // We are indoors and it is also raining
    if( get_weather().weather_id->rains &&
        get_weather().weather_id->precip != precip_class::very_light &&
        !is_underground && is_sheltered && !is_channel_playing( channel::indoors_rain_env ) ) {
        play_ambient_variant_sound( "environment", "indoors_rain", heard_volume, channel::indoors_rain_env,
                                    1000 );
    }
    if( ( !is_sheltered &&
          get_weather().weather_id->sound_category != weather_sound_category::silent && !is_deaf &&
          !is_channel_playing( channel::outdoors_snow_env ) &&
          !is_channel_playing( channel::outdoors_flurry_env ) &&
          !is_channel_playing( channel::outdoors_thunderstorm_env ) &&
          !is_channel_playing( channel::outdoors_rain_env ) &&
          !is_channel_playing( channel::outdoors_drizzle_env ) &&
          !is_channel_playing( channel::outdoor_blizzard ) )
        || ( !is_sheltered &&
             weather_changed  && !is_deaf ) ) {
        fade_audio_group( group::weather, 1000 );
        // We are outside and there is precipitation
        switch( get_weather().weather_id->sound_category ) {
            case weather_sound_category::drizzle:
                play_ambient_variant_sound( "environment", "WEATHER_DRIZZLE", heard_volume,
                                            channel::outdoors_drizzle_env,
                                            1000 );
                break;
            case weather_sound_category::rainy:
                play_ambient_variant_sound( "environment", "WEATHER_RAINY", heard_volume,
                                            channel::outdoors_rain_env,
                                            1000 );
                break;
            case weather_sound_category::thunder:
                play_ambient_variant_sound( "environment", "WEATHER_THUNDER", heard_volume,
                                            channel::outdoors_thunderstorm_env,
                                            1000 );
                break;
            case weather_sound_category::flurries:
                play_ambient_variant_sound( "environment", "WEATHER_FLURRIES", heard_volume,
                                            channel::outdoors_flurry_env,
                                            1000 );
                break;
            case weather_sound_category::snowstorm:
                play_ambient_variant_sound( "environment", "WEATHER_SNOWSTORM", heard_volume,
                                            channel::outdoor_blizzard,
                                            1000 );
                break;
            case weather_sound_category::snow:
                play_ambient_variant_sound( "environment", "WEATHER_SNOW", heard_volume, channel::outdoors_snow_env,
                                            1000 );
                break;
            case weather_sound_category::silent:
                break;
            case weather_sound_category::last:
                debugmsg( "Invalid weather sound category." );
                break;
        }
    }
    // Keep track of weather to compare for next iteration
    previous_weather = get_weather().weather_id;
}

// firing is the item that is fired. It may be the wielded gun, but it can also be an attached
// gunmod.
void sfx::generate_gun_sound( const tripoint &source, const item &firing )
{
    if( test_mode ) {
        return;
    }

    end_sfx_timestamp = std::chrono::high_resolution_clock::now();
    sfx_time = end_sfx_timestamp - start_sfx_timestamp;
    if( std::chrono::duration_cast<std::chrono::milliseconds> ( sfx_time ).count() < 80 ) {
        return;
    }
    int heard_volume = get_heard_volume( source );
    heard_volume = std::max( heard_volume, 30 );

    itype_id weapon_id = firing.typeId();
    units::angle angle = 0_degrees;
    int distance = 0;
    std::string selected_sound;
    const avatar &player_character = get_avatar();
    // this does not mean p == avatar (it could be a vehicle turret)
    if( player_character.pos() == source ) {
        selected_sound = "fire_gun";

        const auto mods = firing.gunmods();
        if( std::ranges::any_of( mods,
        []( const item * e ) {
        return e->type->gunmod->loudness < 0;
    } ) ) {
            weapon_id = itype_weapon_fire_suppressed;
        }

    } else {
        angle = get_heard_angle( source );
        distance = sound_distance( player_character.pos(), source );
        if( distance <= 17 ) {
            selected_sound = "fire_gun";
        } else {
            selected_sound = "fire_gun_distant";
        }
    }

    play_variant_sound( selected_sound, weapon_id.str(), heard_volume, angle, 0.8, 1.2 );
    start_sfx_timestamp = std::chrono::high_resolution_clock::now();
}

namespace sfx
{
struct sound_thread {
    sound_thread( const tripoint &source, const tripoint &target, bool hit, bool targ_mon,
                  const std::string &material );

    bool hit;
    bool targ_mon;
    std::string material;

    skill_id weapon_skill;
    int weapon_volume;
    // volume and angle for calls to play_variant_sound
    units::angle ang_src;
    int vol_src;
    int vol_targ;
    units::angle ang_targ;

    // Operator overload required for thread API.
    void operator()() const;
};
} // namespace sfx

void sfx::generate_melee_sound( const tripoint &source, const tripoint &target, bool hit,
                                bool targ_mon,
                                const std::string &material )
{
    if( test_mode ) {
        return;
    }
    // If creating a new thread for each invocation is to much, we have to consider a thread
    // pool or maybe a single thread that works continuously, but that requires a queue or similar
    // to coordinate its work.
    try {
        std::thread the_thread( sound_thread( source, target, hit, targ_mon, material ) );
        try {
            if( the_thread.joinable() ) {
                the_thread.detach();
            }
        } catch( std::system_error &err ) {
            dbg( DL::Error ) << "Failed to detach melee sound thread: std::system_error: " << err.what();
        }
    } catch( std::system_error &err ) {
        // not a big deal, just skip playing the sound.
        dbg( DL::Error ) << "Failed to create melee sound thread: std::system_error: " << err.what();
    }
}

sfx::sound_thread::sound_thread( const tripoint &source, const tripoint &target, const bool hit,
                                 const bool targ_mon, const std::string &material )
    : hit( hit )
    , targ_mon( targ_mon )
    , material( material )
{
    // This is function is run in the main thread.
    const int heard_volume = get_heard_volume( source );
    const player *p = g->critter_at<npc>( source );
    if( !p ) {
        p = &g->u;
        // sound comes from the same place as the player is, calculation of angle wouldn't work
        ang_src = 0_degrees;
        vol_src = heard_volume;
        vol_targ = heard_volume;
    } else {
        ang_src = get_heard_angle( source );
        vol_src = std::max( heard_volume - 30, 0 );
        vol_targ = std::max( heard_volume - 20, 0 );
    }
    ang_targ = get_heard_angle( target );
    weapon_skill = p->primary_weapon().melee_skill();
    weapon_volume = p->primary_weapon().volume() / units::legacy_volume_factor;
}

// Operator overload required for thread API.
void sfx::sound_thread::operator()() const
{
    // This is function is run in a separate thread. One must be careful and not access game data
    // that might change (e.g. g->u.weapon, the character could switch weapons while this thread
    // runs).
    std::this_thread::sleep_for( std::chrono::milliseconds( rng( 1, 2 ) ) );
    std::string variant_used;

    static const skill_id skill_bashing( "bashing" );
    static const skill_id skill_cutting( "cutting" );
    static const skill_id skill_stabbing( "stabbing" );

    if( weapon_skill == skill_bashing && weapon_volume <= 8 ) {
        variant_used = "small_bash";
        play_variant_sound( "melee_swing", "small_bash", vol_src, ang_src, 0.8, 1.2 );
    } else if( weapon_skill == skill_bashing && weapon_volume >= 9 ) {
        variant_used = "big_bash";
        play_variant_sound( "melee_swing", "big_bash", vol_src, ang_src, 0.8, 1.2 );
    } else if( ( weapon_skill == skill_cutting || weapon_skill == skill_stabbing ) &&
               weapon_volume <= 6 ) {
        variant_used = "small_cutting";
        play_variant_sound( "melee_swing", "small_cutting", vol_src, ang_src, 0.8, 1.2 );
    } else if( ( weapon_skill == skill_cutting || weapon_skill == skill_stabbing ) &&
               weapon_volume >= 7 ) {
        variant_used = "big_cutting";
        play_variant_sound( "melee_swing", "big_cutting", vol_src, ang_src, 0.8, 1.2 );
    } else {
        variant_used = "default";
        play_variant_sound( "melee_swing", "default", vol_src, ang_src, 0.8, 1.2 );
    }
    if( hit ) {
        if( targ_mon ) {
            if( material == "steel" ) {
                std::this_thread::sleep_for( std::chrono::milliseconds( rng( weapon_volume * 12,
                                             weapon_volume * 16 ) ) );
                play_variant_sound( "melee_hit_metal", variant_used, vol_targ, ang_targ, 0.8, 1.2 );
            } else {
                std::this_thread::sleep_for( std::chrono::milliseconds( rng( weapon_volume * 12,
                                             weapon_volume * 16 ) ) );
                play_variant_sound( "melee_hit_flesh", variant_used, vol_targ, ang_targ, 0.8, 1.2 );
            }
        } else {
            std::this_thread::sleep_for( std::chrono::milliseconds( rng( weapon_volume * 9,
                                         weapon_volume * 12 ) ) );
            play_variant_sound( "melee_hit_flesh", variant_used, vol_targ, ang_targ, 0.8, 1.2 );
        }
    }
}

void sfx::do_projectile_hit( const Creature &target )
{
    if( test_mode ) {
        return;
    }

    const int heard_volume = sfx::get_heard_volume( target.pos() );
    const units::angle angle = get_heard_angle( target.pos() );
    if( target.is_monster() ) {
        const monster &mon = dynamic_cast<const monster &>( target );
        static const std::set<material_id> fleshy = {
            material_id( "flesh" ),
            material_id( "hflesh" ),
            material_id( "iflesh" ),
            material_id( "veggy" ),
            material_id( "bone" ),
        };
        const bool is_fleshy = std::ranges::any_of( fleshy, [&mon]( const material_id & m ) {
            return mon.made_of( m );
        } );

        if( is_fleshy ) {
            play_variant_sound( "bullet_hit", "hit_flesh", heard_volume, angle, 0.8, 1.2 );
            return;
        } else if( mon.made_of( material_id( "stone" ) ) ) {
            play_variant_sound( "bullet_hit", "hit_wall", heard_volume, angle, 0.8, 1.2 );
            return;
        } else if( mon.made_of( material_id( "steel" ) ) ) {
            play_variant_sound( "bullet_hit", "hit_metal", heard_volume, angle, 0.8, 1.2 );
            return;
        } else {
            play_variant_sound( "bullet_hit", "hit_flesh", heard_volume, angle, 0.8, 1.2 );
            return;
        }
    }
    play_variant_sound( "bullet_hit", "hit_flesh", heard_volume, angle, 0.8, 1.2 );
}

void sfx::do_player_death_hurt( const player &target, bool death )
{
    if( test_mode ) {
        return;
    }

    int heard_volume = get_heard_volume( target.pos() );
    const bool male = target.male;
    if( !male && !death ) {
        play_variant_sound( "deal_damage", "hurt_f", heard_volume );
    } else if( male && !death ) {
        play_variant_sound( "deal_damage", "hurt_m", heard_volume );
    } else if( !male && death ) {
        play_variant_sound( "clean_up_at_end", "death_f", heard_volume );
    } else if( male && death ) {
        play_variant_sound( "clean_up_at_end", "death_m", heard_volume );
    }
}

void sfx::do_danger_music()
{
    if( test_mode ) {
        return;
    }

    avatar &player_character = get_avatar();
    if( player_character.in_sleep_state() && !audio_muted ) {
        fade_audio_channel( channel::any, 100 );
        audio_muted = true;
        return;
    } else if( ( player_character.in_sleep_state() && audio_muted ) ||
               is_channel_playing( channel::chainsaw_theme ) ) {
        fade_audio_group( group::context_themes, 1000 );
        return;
    }
    audio_muted = false;
    int hostiles = 0;
    for( auto &critter : player_character.get_visible_creatures( 40 ) ) {
        if( player_character.attitude_to( *critter ) == Attitude::A_HOSTILE ) {
            hostiles++;
        }
    }
    if( hostiles == prev_hostiles ) {
        return;
    }
    if( hostiles <= 4 ) {
        fade_audio_group( group::context_themes, 1000 );
        prev_hostiles = hostiles;
        return;
    } else if( hostiles >= 5 && hostiles <= 9 && !is_channel_playing( channel::danger_low_theme ) ) {
        fade_audio_group( group::context_themes, 1000 );
        play_ambient_variant_sound( "danger_low", "default", 100, channel::danger_low_theme, 1000 );
        prev_hostiles = hostiles;
        return;
    } else if( hostiles >= 10 && hostiles <= 14 &&
               !is_channel_playing( channel::danger_medium_theme ) ) {
        fade_audio_group( group::context_themes, 1000 );
        play_ambient_variant_sound( "danger_medium", "default", 100, channel::danger_medium_theme, 1000 );
        prev_hostiles = hostiles;
        return;
    } else if( hostiles >= 15 && hostiles <= 19 && !is_channel_playing( channel::danger_high_theme ) ) {
        fade_audio_group( group::context_themes, 1000 );
        play_ambient_variant_sound( "danger_high", "default", 100, channel::danger_high_theme, 1000 );
        prev_hostiles = hostiles;
        return;
    } else if( hostiles >= 20 && !is_channel_playing( channel::danger_extreme_theme ) ) {
        fade_audio_group( group::context_themes, 1000 );
        play_ambient_variant_sound( "danger_extreme", "default", 100, channel::danger_extreme_theme, 1000 );
        prev_hostiles = hostiles;
        return;
    }
    prev_hostiles = hostiles;
}

void sfx::do_fatigue()
{
    if( test_mode ) {
        return;
    }

    avatar &player_character = get_avatar();
    /*15: Stamina 75%
    16: Stamina 50%
    17: Stamina 25%*/
    if( player_character.get_stamina() >= player_character.get_stamina_max() * .75 ) {
        fade_audio_group( group::fatigue, 2000 );
        return;
    } else if( player_character.get_stamina() <= player_character.get_stamina_max() * .74 &&
               player_character.get_stamina() >= player_character.get_stamina_max() * .5 &&
               player_character.male && !is_channel_playing( channel::stamina_75 ) ) {
        fade_audio_group( group::fatigue, 1000 );
        play_ambient_variant_sound( "plmove", "fatigue_m_low", 100, channel::stamina_75, 1000 );
        return;
    } else if( player_character.get_stamina() <= player_character.get_stamina_max() * .49 &&
               player_character.get_stamina() >= player_character.get_stamina_max() * .25 &&
               player_character.male && !is_channel_playing( channel::stamina_50 ) ) {
        fade_audio_group( group::fatigue, 1000 );
        play_ambient_variant_sound( "plmove", "fatigue_m_med", 100, channel::stamina_50, 1000 );
        return;
    } else if( player_character.get_stamina() <= player_character.get_stamina_max() * .24 &&
               player_character.get_stamina() >= 0 && player_character.male &&
               !is_channel_playing( channel::stamina_35 ) ) {
        fade_audio_group( group::fatigue, 1000 );
        play_ambient_variant_sound( "plmove", "fatigue_m_high", 100, channel::stamina_35, 1000 );
        return;
    } else if( player_character.get_stamina() <= player_character.get_stamina_max() * .74 &&
               player_character.get_stamina() >= player_character.get_stamina_max() * .5 &&
               !player_character.male && !is_channel_playing( channel::stamina_75 ) ) {
        fade_audio_group( group::fatigue, 1000 );
        play_ambient_variant_sound( "plmove", "fatigue_f_low", 100, channel::stamina_75, 1000 );
        return;
    } else if( player_character.get_stamina() <= player_character.get_stamina_max() * .49 &&
               player_character.get_stamina() >= player_character.get_stamina_max() * .25 &&
               !player_character.male && !is_channel_playing( channel::stamina_50 ) ) {
        fade_audio_group( group::fatigue, 1000 );
        play_ambient_variant_sound( "plmove", "fatigue_f_med", 100, channel::stamina_50, 1000 );
        return;
    } else if( player_character.get_stamina() <= player_character.get_stamina_max() * .24 &&
               player_character.get_stamina() >= 0 && !player_character.male &&
               !is_channel_playing( channel::stamina_35 ) ) {
        fade_audio_group( group::fatigue, 1000 );
        play_ambient_variant_sound( "plmove", "fatigue_f_high", 100, channel::stamina_35, 1000 );
        return;
    }
}

void sfx::do_hearing_loss( int turns )
{
    if( test_mode ) {
        return;
    }

    g_sfx_volume_multiplier = .1;
    fade_audio_group( group::weather, 50 );
    fade_audio_group( group::time_of_day, 50 );
    // Negative duration is just insuring we stay in sync with player condition,
    // don't play any of the sound effects for going deaf.
    if( turns == -1 ) {
        return;
    }
    play_variant_sound( "environment", "deafness_shock", 100 );
    play_variant_sound( "environment", "deafness_tone_start", 100 );
    if( turns <= 35 ) {
        play_ambient_variant_sound( "environment", "deafness_tone_light", 90, channel::deafness_tone, 100 );
    } else if( turns <= 90 ) {
        play_ambient_variant_sound( "environment", "deafness_tone_medium", 90, channel::deafness_tone,
                                    100 );
    } else if( turns >= 91 ) {
        play_ambient_variant_sound( "environment", "deafness_tone_heavy", 90, channel::deafness_tone, 100 );
    }
}

void sfx::remove_hearing_loss()
{
    if( test_mode ) {
        return;
    }
    stop_sound_effect_fade( channel::deafness_tone, 300 );
    g_sfx_volume_multiplier = 1;
    do_ambient();
}

void sfx::do_footstep()
{
    if( test_mode ) {
        return;
    }

    end_sfx_timestamp = std::chrono::high_resolution_clock::now();
    sfx_time = end_sfx_timestamp - start_sfx_timestamp;
    if( std::chrono::duration_cast<std::chrono::milliseconds> ( sfx_time ).count() > 400 ) {
        const avatar &player_character = get_avatar();
        int heard_volume = sfx::get_heard_volume( player_character.pos() );
        const auto terrain = get_map().ter( player_character.pos() ).id();
        static const std::set<ter_str_id> grass = {
            ter_str_id( "t_grass" ),
            ter_str_id( "t_shrub" ),
            ter_str_id( "t_shrub_peanut" ),
            ter_str_id( "t_shrub_peanut_harvested" ),
            ter_str_id( "t_shrub_blueberry" ),
            ter_str_id( "t_shrub_blueberry_harvested" ),
            ter_str_id( "t_shrub_strawberry" ),
            ter_str_id( "t_shrub_strawberry_harvested" ),
            ter_str_id( "t_shrub_blackberry" ),
            ter_str_id( "t_shrub_blackberry_harvested" ),
            ter_str_id( "t_shrub_huckleberry" ),
            ter_str_id( "t_shrub_huckleberry_harvested" ),
            ter_str_id( "t_shrub_raspberry" ),
            ter_str_id( "t_shrub_raspberry_harvested" ),
            ter_str_id( "t_shrub_grape" ),
            ter_str_id( "t_shrub_grape_harvested" ),
            ter_str_id( "t_shrub_rose" ),
            ter_str_id( "t_shrub_rose_harvested" ),
            ter_str_id( "t_shrub_hydrangea" ),
            ter_str_id( "t_shrub_hydrangea_harvested" ),
            ter_str_id( "t_shrub_lilac" ),
            ter_str_id( "t_shrub_lilac_harvested" ),
            ter_str_id( "t_underbrush" ),
            ter_str_id( "t_underbrush_harvested_spring" ),
            ter_str_id( "t_underbrush_harvested_summer" ),
            ter_str_id( "t_underbrush_harvested_autumn" ),
            ter_str_id( "t_underbrush_harvested_winter" ),
            ter_str_id( "t_moss" ),
            ter_str_id( "t_moss_underground" ),
            ter_str_id( "t_grass_white" ),
            ter_str_id( "t_grass_long" ),
            ter_str_id( "t_grass_tall" ),
            ter_str_id( "t_grass_dead" ),
            ter_str_id( "t_grass_golf" ),
            ter_str_id( "t_golf_hole" ),
            ter_str_id( "t_trunk" ),
            ter_str_id( "t_stump" ),
        };
        static const std::set<ter_str_id> dirt = {
            ter_str_id( "t_dirt" ),
            ter_str_id( "t_dirtmound" ),
            ter_str_id( "t_dirtmoundfloor" ),
            ter_str_id( "t_sand" ),
            ter_str_id( "t_clay" ),
            ter_str_id( "t_dirtfloor" ),
            ter_str_id( "t_palisade_gate_o" ),
            ter_str_id( "t_sandbox" ),
            ter_str_id( "t_claymound" ),
            ter_str_id( "t_sandmound" ),
            ter_str_id( "t_rootcellar" ),
            ter_str_id( "t_railroad_rubble" ),
            ter_str_id( "t_railroad_track" ),
            ter_str_id( "t_railroad_track_h" ),
            ter_str_id( "t_railroad_track_v" ),
            ter_str_id( "t_railroad_track_d" ),
            ter_str_id( "t_railroad_track_d1" ),
            ter_str_id( "t_railroad_track_d2" ),
            ter_str_id( "t_railroad_tie" ),
            ter_str_id( "t_railroad_tie_d" ),
            ter_str_id( "t_railroad_tie_d" ),
            ter_str_id( "t_railroad_tie_h" ),
            ter_str_id( "t_railroad_tie_v" ),
            ter_str_id( "t_railroad_tie_d" ),
            ter_str_id( "t_railroad_track_on_tie" ),
            ter_str_id( "t_railroad_track_h_on_tie" ),
            ter_str_id( "t_railroad_track_v_on_tie" ),
            ter_str_id( "t_railroad_track_d_on_tie" ),
            ter_str_id( "t_railroad_tie" ),
            ter_str_id( "t_railroad_tie_h" ),
            ter_str_id( "t_railroad_tie_v" ),
            ter_str_id( "t_railroad_tie_d1" ),
            ter_str_id( "t_railroad_tie_d2" ),
        };
        static const std::set<ter_str_id> metal = {
            ter_str_id( "t_ov_smreb_cage" ),
            ter_str_id( "t_metal_floor" ),
            ter_str_id( "t_grate" ),
            ter_str_id( "t_bridge" ),
            ter_str_id( "t_elevator" ),
            ter_str_id( "t_guardrail_bg_dp" ),
            ter_str_id( "t_slide" ),
            ter_str_id( "t_conveyor" ),
            ter_str_id( "t_machinery_light" ),
            ter_str_id( "t_machinery_heavy" ),
            ter_str_id( "t_machinery_old" ),
            ter_str_id( "t_machinery_electronic" ),
        };
        static const std::set<ter_str_id> water = {
            ter_str_id( "t_water_moving_sh" ),
            ter_str_id( "t_water_moving_dp" ),
            ter_str_id( "t_water_sh" ),
            ter_str_id( "t_water_dp" ),
            ter_str_id( "t_swater_sh" ),
            ter_str_id( "t_swater_dp" ),
            ter_str_id( "t_water_pool" ),
            ter_str_id( "t_sewage" ),
        };
        static const std::set<ter_str_id> chain_fence = {
            ter_str_id( "t_chainfence" ),
        };

        const auto play_plmove_sound_variant = [&]( const std::string & variant ) {
            play_variant_sound( "plmove", variant, heard_volume, 0_degrees, 0.8, 1.2 );
            start_sfx_timestamp = std::chrono::high_resolution_clock::now();
        };

        auto veh_displayed_part = g->m.veh_at( g->u.pos() ).part_displayed();

        if( !veh_displayed_part && ( water.contains( terrain ) ) ) {
            play_plmove_sound_variant( "walk_water" );
            return;
        }
        if( !g->u.wearing_something_on( bodypart_id( bp_foot_l ) ) ) {
            play_plmove_sound_variant( "walk_barefoot" );
            return;
        }
        if( veh_displayed_part ) {
            const std::string &part_id = veh_displayed_part->part().info().get_id().str();
            if( has_variant_sound( "plmove", part_id ) ) {
                play_plmove_sound_variant( part_id );
            } else if( veh_displayed_part->has_feature( VPFLAG_AISLE ) ) {
                play_plmove_sound_variant( "walk_tarmac" );
            } else {
                play_plmove_sound_variant( "clear_obstacle" );
            }
            return;
        }
        if( sfx::has_variant_sound( "plmove", terrain.str() ) ) {
            play_plmove_sound_variant( terrain.str() );
            return;
        }
        if( grass.contains( terrain ) ) {
            play_plmove_sound_variant( "walk_grass" );
            return;
        }
        if( dirt.contains( terrain ) ) {
            play_plmove_sound_variant( "walk_dirt" );
            return;
        }
        if( metal.contains( terrain ) ) {
            play_plmove_sound_variant( "walk_metal" );
            return;
        }
        if( chain_fence.contains( terrain ) ) {
            play_plmove_sound_variant( "clear_obstacle" );
            return;
        }

        play_plmove_sound_variant( "walk_tarmac" );
    }
}

void sfx::do_obstacle( const std::string &obst )
{
    if( test_mode ) {
        return;
    }

    int heard_volume = sfx::get_heard_volume( get_avatar().pos() );

    static const std::set<std::string> water = {
        "t_water_sh",
        "t_water_dp",
        "t_water_moving_sh",
        "t_water_moving_dp",
        "t_swater_sh",
        "t_swater_dp",
        "t_water_pool",
        "t_sewage",
    };
    if( sfx::has_variant_sound( "plmove", obst ) ) {
        play_variant_sound( "plmove", obst, heard_volume, 0_degrees, 0.8, 1.2 );
    } else if( water.contains( obst ) ) {
        play_variant_sound( "plmove", "walk_water", heard_volume, 0_degrees, 0.8, 1.2 );
    } else {
        play_variant_sound( "plmove", "clear_obstacle", heard_volume, 0_degrees, 0.8, 1.2 );
    }
    // prevent footsteps from triggering
    start_sfx_timestamp = std::chrono::high_resolution_clock::now();
}

void sfx::play_activity_sound( const std::string &id, const std::string &variant, int volume )
{
    if( test_mode ) {
        return;
    }

    avatar &player_character = get_avatar();
    if( act != player_character.activity->id() ) {
        act = player_character.activity->id();
        play_ambient_variant_sound( id, variant, volume, channel::player_activities, 0 );
    }
}

void sfx::end_activity_sounds()
{
    if( test_mode ) {
        return;
    }
    act = activity_id::NULL_ID();
    fade_audio_channel( channel::player_activities, 2000 );
}

#else // if defined(SDL_SOUND)

/** Dummy implementations for builds without sound */
/*@{*/
void sfx::load_sound_effects( const JsonObject & ) { }
void sfx::load_sound_effect_preload( const JsonObject & ) { }
void sfx::load_playlist( const JsonObject & ) { }
void sfx::play_variant_sound( const std::string &, const std::string &, int, units::angle, double,
                              double ) { }
void sfx::play_variant_sound( const std::string &, const std::string &, int ) { }
void sfx::play_ambient_variant_sound( const std::string &, const std::string &, int, channel, int,
                                      double, int ) { }
void sfx::play_activity_sound( const std::string &, const std::string &, int ) { }
void sfx::end_activity_sounds() { }
void sfx::generate_gun_sound( const tripoint &, const item & ) { }
void sfx::generate_melee_sound( const tripoint &, const tripoint &, bool, bool,
                                const std::string & ) { }
void sfx::do_hearing_loss( int ) { }
void sfx::remove_hearing_loss() { }
void sfx::do_projectile_hit( const Creature & ) { }
void sfx::do_footstep() { }
void sfx::do_danger_music() { }
void sfx::do_vehicle_engine_sfx() { }
void sfx::do_vehicle_exterior_engine_sfx() { }
void sfx::do_ambient() { }
void sfx::fade_audio_group( group, int ) { }
void sfx::fade_audio_channel( channel, int ) { }
bool sfx::is_channel_playing( channel )
{
    return false;
}
int sfx::set_channel_volume( channel, int )
{
    return 0;
}
bool sfx::has_variant_sound( const std::string &, const std::string & )
{
    return false;
}
void sfx::stop_sound_effect_fade( channel, int ) { }
void sfx::stop_sound_effect_timed( channel, int ) {}
void sfx::do_player_death_hurt( const player &, bool ) { }
void sfx::do_fatigue() { }
void sfx::do_obstacle( const std::string & ) { }
/*@}*/

#endif // if defined(SDL_SOUND)

/** Functions from sfx that do not use the SDL_mixer API at all. They can be used in builds
  * without sound support. */
/*@{*/
int sfx::get_heard_volume( const tripoint &source )
{
    if( source == get_avatar().pos() ) {
        return ( 100 * g_sfx_volume_multiplier );
    }
    int distance = sound_distance( get_avatar().pos(), source );
    // fract = -100 / 24
    const float fract = -4.166666;
    int heard_volume = fract * ( distance - 1 ) + 100;
    // Cap our volume from 0 - 100
    heard_volume = std::min( std::max( heard_volume, 0 ), 100 );
    heard_volume *= g_sfx_volume_multiplier;
    return ( heard_volume );
}

units::angle sfx::get_heard_angle( const tripoint &source )
{
    units::angle angle = coord_to_angle( get_player_character().pos(), source ) + 90_degrees;
    //add_msg(m_warning, "angle: %i", angle);
    return angle;
}
/*@}*/
