#include "sounds.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>
#include <queue>

#include "active_tile_data.h"
#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "construction.h"
#include "coordinates.h"
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
#include "mapdata.h"
#include "messages.h"
#include "monfaction.h"
#include "monster.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "overmapbuffer_registry.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "rng.h"
#include "safemode_ui.h"
#include "string_formatter.h"
#include "string_id.h"
#include "thread_pool.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "units_angle.h"
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

// For use with the floodfill logic.
static constexpr auto tile_structure_sound_absorption_tier = std::array<short, 4>
{
    {SOUND_ABSORPTION_OPEN_FIELD, SOUND_ABSORPTION_BARRIER, SOUND_ABSORPTION_THICK_BARRIER, SOUND_ABSORPTION_WALL}
};
static constexpr auto tile_base_sound_absorption_tier = std::array<short, 4>
{
    {SOUND_ABSORPTION_OPEN_FIELD, SOUND_ABSORPTION_LIGHT_VEGITATION, SOUND_ABSORPTION_FOREST_FALL, SOUND_ABSORPTION_FOREST}
};

static constexpr auto tile_absorp_from_index( const uint8_t &index )
{
    if( index > 3 ) {
        return tile_structure_sound_absorption_tier[0];
    } else {
        return tile_structure_sound_absorption_tier[index];
    }
}

static constexpr uint8_t get_tile_absorp_index( const short &absorp )
{
    if( absorp >= SOUND_ABSORPTION_WALL ) {
        return 3;
    } else if( absorp >= SOUND_ABSORPTION_THICK_BARRIER ) {
        return 2;
    } else if( absorp >= SOUND_ABSORPTION_BARRIER ) {
        return 1;
    } else {
        return 0;
    }
}
static constexpr auto base_absorp_from_index( const uint8_t &index )
{
    if( index > 3 ) {
        return tile_base_sound_absorption_tier[0];
    } else {
        return tile_base_sound_absorption_tier[index];
    }
}

static constexpr auto absorption_from_checkvar_bitset( const std::bitset<8> &checkvars ) -> short
{
    const uint8_t &base_absp_cnd = ( checkvars[0] && checkvars[1] ) ? 3 : ( !checkvars[0] &&
                                   checkvars[1] ) ? 2 : checkvars[0] + checkvars[1];
    const uint8_t &tile_absp_cnd = ( checkvars[2] && checkvars[3] ) ? 3 : ( !checkvars[2] &&
                                   checkvars[3] ) ? 2 : checkvars[2] + checkvars[3];
    return base_absorp_from_index( base_absp_cnd ) + tile_absorp_from_index( tile_absp_cnd );
}

static constexpr uint8_t get_base_absorp_index( const short &absorp )
{
    if( absorp == SOUND_ABSORPTION_OPEN_FIELD ) {
        return 0;
    }
    // The absorption from walls or other structures comes in multiples of 500.
    // Deal with the potential for snow bonus as well while we are at it.
    const short mod_result = ( ( absorp % SOUND_ABSORPTION_BARRIER ) >= SOUND_ABSORPTION_SNOW_BONUS )
                             ? ( absorp % SOUND_ABSORPTION_BARRIER ) - SOUND_ABSORPTION_SNOW_BONUS : absorp %
                             SOUND_ABSORPTION_BARRIER ;
    if( mod_result >= SOUND_ABSORPTION_FOREST ) {
        return 3;
    } else if( mod_result >= SOUND_ABSORPTION_FOREST_FALL ) {
        return 2;
    } else if( mod_result >= SOUND_ABSORPTION_LIGHT_VEGITATION ) {
        return 1;
    } else {
        return 0;
    }
}

// We have these to feed to skip_due_to_wall to speed up the floodfill logic by just a bit.
static constexpr std::pair<bool, bool> s_wall_false_false = { false, false };
static constexpr std::pair<bool, bool> s_wall_true_false = { true, false };
static constexpr std::pair<bool, bool> s_wall_false_true = { false, true };
static constexpr std::pair<bool, bool> s_wall_true_true = { true, true };
static constexpr auto s_wall_bool_pairs = std::array<std::pair<bool, bool>, 4>
{
    {
        s_wall_false_false, s_wall_true_false, s_wall_false_true, s_wall_true_true
    }
};
static constexpr auto get_s_wall_bool_pair( const bool &wall1, const bool &wall2 )
{
    if( !wall1 && !wall2 ) {
        // Using the sound direction indext constexpr since they are constexpr.
        return s_wall_bool_pairs[SDI_NW];
    } else if( wall1 && !wall2 ) {
        return s_wall_bool_pairs[SDI_N];
    } else if( !wall1 && wall2 ) {
        return s_wall_bool_pairs[SDI_NE];
    } else {
        return s_wall_bool_pairs[SDI_E];
    }
}

static constexpr bool skip_due_to_wall( const std::pair<bool, bool> &wall,
                                        const uint8_t &source_dir, const uint8_t &dir_index )
{
    const auto &walls_to_check = wall_check_by_sdirection[source_dir];
    const auto &wall1_invalid = wall_sdir_invalidation[walls_to_check.first];
    const auto &wall2_invalid = wall_sdir_invalidation[walls_to_check.second];
    if( wall.first && wall.second ) {
        return ( dir_index == wall2_invalid.first || dir_index == wall2_invalid.second ||
                 dir_index == wall1_invalid.first || dir_index == wall1_invalid.second );
    } else if( wall.first ) {
        return ( dir_index == wall1_invalid.first || dir_index == wall1_invalid.second );
    } else if( wall.second ) {
        return ( dir_index == wall2_invalid.first || dir_index == wall2_invalid.second );
    }
    return false;
}

// Returns the direction index to use for checking sound volume at a distance.
// Returns a uint8_t value between 0 and 9. If we are given two points at the same x/y, returns 8 for down or 9 for up/in same tile.
// [NW] [ N] [NE]   [ 0 ] [ 1 ] [ 2 ]
// [W ] [ @] [ E] = [ 7 ] [8/9] [ 3 ]
// [SW] [ S] [SE]   [ 6 ] [ 5 ] [ 4 ]
auto sounds::direction_index_to_sound_source( const tripoint_bub_ms &source,
        const tripoint_bub_ms &listener ) -> uint8_t
{
    if( source.x() == listener.x() && source.y() == listener.y() ) {
        if( source.z() > listener.z() ) {
            return SDI_DOWN;
        } else {
            return SDI_UP;
        }
    }

    switch( direction_from( source.xy(), listener.xy() ) ) {
        case direction::NORTHWEST:
            return SDI_NW;
        case direction::NORTH:
            return SDI_N;
        case direction::NORTHEAST:
            return SDI_NE;
        case direction::EAST:
            return SDI_E;
        case direction::SOUTHEAST:
            return SDI_SE;
        case direction::SOUTH:
            return SDI_S;
        case direction::SOUTHWEST:
            return SDI_SW;
        case direction::WEST:
            return SDI_W;
        default:
            return SDI_UP;
    }
}

// Returns the volume adjustment due to altitude in mdB spl. While better than assessing a direct penalty per zlevel, this still has has many assumptions worked into it.
static short vol_z_adjust( const tripoint_bub_ms &source, const tripoint_bub_ms &listener,
                           const bool &lineofsight = false, const bool &for_horde_signal = false )
{
    // Take our easy out if we have it for whatever reason.
    const auto &map = get_map();
    if( source.z() == listener.z() ) {
        return 0;
    } else if( lineofsight ) {
        // If we have direct line of sight, the horizontal volume loss over distance most likely wont be that far off from the amount that would have been lost straight line.
        // Just assign a 1dB penalty per zlevel difference.
        return ( for_horde_signal ) ? std::abs( source.z() - listener.z() ) : ( dBspl_to_mdBspl( std::abs(
                    source.z() - listener.z() ) ) );
    } else if( map.inbounds( listener ) ) {
        int vol_adjust = 0;
        // We want to work our way down from the top.
        const int c_index = map.get_cache_ref( 0 ).idx( listener.x(), listener.y() );
        const int maxz = std::max( source.z(), listener.z() );
        const int minz = std::min( source.z(), listener.z() );
        for( int i = maxz; i > minz; i-- ) {
            // If we only have a floor, assess the pentalty for a barrier. If we have a soundwall, asses the full wall value.
            const auto &lev_cache = map.get_cache_ref( i );
            if( lev_cache.sound_wall_cache[c_index] ) {
                vol_adjust += SOUND_ABSORPTION_PER_ZLEV;
            } else if( lev_cache.floor_cache[c_index] > 0 ) {
                vol_adjust += SOUND_ABSORPTION_BARRIER;
            }
        }
        vol_adjust = std::min( vol_adjust, static_cast<int>( MAXIMUM_VOLUME_ATMOSPHERE ) );
        return ( for_horde_signal ) ? mdBspl_to_dBspl( vol_adjust ) : vol_adjust;
    } else {
        //Well, the listener is out of bounds. Just assign a 1dB penalty per zlevel difference.
        return ( for_horde_signal ) ? std::abs( source.z() - listener.z() ) : ( dBspl_to_mdBspl( std::abs(
                    source.z() - listener.z() ) ) );
    }
};

// Returns a the mdB volume of a given sound cache at some tripoint.
// If you feed this an invalid tripoint, there is a very good chance it explodes or gives you 0.
static short svol_at( const sound_instance_cache &sound_inst, const tripoint_bub_ms &tri,
                      const short &t_absorp = SOUND_ABSORPTION_OPEN_FIELD, const bool &listener_indoors = false,
                      const bool &lineofsight = false )
{
    const auto &map = get_map();
    // Oddly enough everything should still work if we get asked for a noise outside of the bubble, with a simple check.

    // Good idea to track this.
    const bool samez = sound_inst.origin.z() == tri.z();
    // Lets go for our easy solution first.
    if( sound_inst.in_envelope( tri ) && map.inbounds( tri ) ) {
        if( samez ) {
            return sound_inst.vol_at_tri( tri );
        } else {
            // Return the loudest of either the tile vol or the vertical escape vol - vol_z_adjust, minimum 0.
            const auto &vertical_escape_vol = ( sound_inst.origin.z() > tri.z() ) ?
                                              sound_inst.base_distance_vol_by_dir[SDI_UP] : sound_inst.base_distance_vol_by_dir[SDI_DOWN];
            const short zadj = vol_z_adjust( sound_inst.origin, tri, lineofsight );
            if( zadj >= std::max( sound_inst.vol_at_tri( tri ), vertical_escape_vol ) ) {
                return 0;
            }
            return ( std::max( sound_inst.vol_at_tri( tri ), vertical_escape_vol ) - zadj );
        }
    }
    // Use manhattan distance as our flood envelopes are actually squares, not circles.
    const int distance = manhattan_dist( sound_inst.origin.xy(), tri.xy() );
    const auto dir_index = sounds::direction_index_to_sound_source( sound_inst.origin, tri );
    const auto &san_dir = get_sound_direction_index( dir_index );
    // We know at this point that we are out of the envelope so our distance is greater than our flood radius.
    // We use a different escape volume depending upon a couple of factors.
    // Apologies for the messy ternary.

    // For vertical escape use up by default.
    const auto &vertical_escape_vol = ( sound_inst.origin.z() <= tri.z() ) ?
                                      sound_inst.base_distance_vol_by_dir[SDI_UP] : sound_inst.base_distance_vol_by_dir[SDI_DOWN];
    // Determine if we can use the up escape.
    // Broken out for readability. If we are not on the same Z level we can use the vertical escape, or if we are outside, the sound is indoors and escaped.
    const bool use_vert_escape = ( !samez ) ? true : ( ( lineofsight &&
                                 samez ) ? false : ( !listener_indoors && sound_inst.source_indoors &&
                                         sound_inst.escaped_indoors ) );

    const int vol_escape = ( use_vert_escape ) ? std::max( vertical_escape_vol,
                           sound_inst.base_distance_vol_by_dir[san_dir] ) : sound_inst.base_distance_vol_by_dir[san_dir];

    if( vol_escape > MAXIMUM_VOLUME_ATMOSPHERE ||
        vol_escape > dBspl_to_mdBspl( sound_inst.sound.volume ) ) {
        const uint8_t actualdir = ( use_vert_escape &&
                                    vertical_escape_vol > sound_inst.base_distance_vol_by_dir[san_dir] ) ? ( (
                                                sound_inst.origin.z() <= tri.z() ) ? SDI_UP : SDI_DOWN ) : san_dir;
        add_msg( m_debug,
                 _( "Error in sounds::svol:at(). Sound [ %1s ] from %i:%i:%i with origin volume of %i dB, has impossible escape vol in %i direction of %i mdB." ),
                 sound_inst.sound.description, sound_inst.sound.origin.x(), sound_inst.sound.origin.y(),
                 sound_inst.sound.origin.z(), sound_inst.sound.volume, actualdir, vol_escape );
    }
    const int zadj = vol_z_adjust( sound_inst.origin, tri, lineofsight );
    const int cumulative_dist_loss = get_cumulative_vol_dist_loss( sound_inst.flood_radius, distance,
                                     t_absorp );
    if( ( zadj + cumulative_dist_loss ) > MAXIMUM_VOLUME_ATMOSPHERE ) {
        return 0;
    }
    if( zadj < 0 || zadj > MAXIMUM_VOLUME_ATMOSPHERE || cumulative_dist_loss < 0 ||
        cumulative_dist_loss > MAXIMUM_VOLUME_ATMOSPHERE ) {
        add_msg( m_debug,
                 _( "Error in sounds::svol:at(). Calced Escape Vol of %i mdB, Calced vol_z_adjust of %i mdB, Calced cumulative distance loss of %i mdB across %i tiles from heard sound [ %1s ] at %i:%i:%i to heard location %i:%i:%i" ),
                 vol_escape, zadj, cumulative_dist_loss, distance, sound_inst.sound.description,
                 sound_inst.sound.origin.x(), sound_inst.sound.origin.y(),
                 sound_inst.sound.origin.z(), tri.x(), tri.y(), tri.z() );
    }
    const int heard_volume = std::max( 0, ( vol_escape  - ( zadj + cumulative_dist_loss ) ) );
    if( heard_volume > MAXIMUM_VOLUME_ATMOSPHERE ||
        heard_volume > dBspl_to_mdBspl( sound_inst.sound.volume ) ) {
        const uint8_t actualdir = ( use_vert_escape &&
                                    vertical_escape_vol > sound_inst.base_distance_vol_by_dir[san_dir] ) ? ( (
                                                sound_inst.origin.z() <= tri.z() ) ? SDI_UP : SDI_DOWN ) : san_dir;
        add_msg( m_debug,
                 _( "Error in sounds::svol:at(). Sound [ %1s ] from %i:%i:%i with origin volume of %i dB, has impossible heard vol in %i direction of %i mdB at a distance of %i manhattan.xt" ),
                 sound_inst.sound.description, sound_inst.sound.origin.x(), sound_inst.sound.origin.y(),
                 sound_inst.sound.origin.z(), sound_inst.sound.volume, actualdir, heard_volume, distance );
    }
    return heard_volume;
}

// For use when flood filling sounds to allow for Dijkstra-like max-heap processing instead of breadth first, not preserved.
struct propagation_tile {
    point_bub_ms  pos;
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
    // This is a bit heinous, but we have to step through several structs to actually get to the int code number for the land use codes.
    // 40 land use cases in total. We either use the integer identifier, or the string id. Int id is significantly easier to expand upon later or mod.
    // We only care about the terrain around the player.
    const int landusecodenum = get_overmapbuffer( get_avatar().get_dimension() ).ter(
                                   omtpos ).obj().get_land_use_code()->land_use_code;
    // Forests have less attenuation in the fall, and during winter sound attenuation is higher accross the board
    // because of expected ambient snow, which is a extremely strong sound attenuator and can absorb somewhere between 50% and 90% of high frequency sound.
    // const season_type season = season_of_year( calendar::turn );
    // Attenuation bonus from expected ambient snow.
    // These are approximates from US Army ERDC research on the effects of snow cover on sound propagation of .45 ACP and other high frequency sounds.
    const short snowbonus = ( season != WINTER ) ? SOUND_ABSORPTION_OPEN_FIELD :
                            ( horde_signal ) ? 42 : SOUND_ABSORPTION_SNOW_BONUS;

    // We want 4 total cases, open field, light vegitation/agriculture, urban, and forest/heavy vegitation.
    // Return urban if none of the specified use codes, i.e., 0
    // Technically how much a sound is attenuated also heavily depends on its frequency,
    // But we are mostly concerned with the "high frequency" portion of sounds (1kHz+)
    // High frequency sounds are what most creatures can easily pinpoint the direction of.
    // Gunshots are really a meddly of sounds across a very wide frequency band, but we care about the high frequency portion.

    // This is the really heinous bit. We either use the integer id, or the string id. Integer id it is.
    if( landusecodenum == 3 || landusecodenum == 37 || landusecodenum == 35 )  {
        // Heavy vegitation or forest. Heaviest attenuation, except in the fall.
        return snowbonus + ( ( season == AUTUMN ) ? ( ( horde_signal ) ? 20 : SOUND_ABSORPTION_FOREST_FALL )
                             : ( (
                                     horde_signal ) ? 26 : SOUND_ABSORPTION_FOREST ) );

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
        return snowbonus + ( ( horde_signal ) ? 12 : SOUND_ABSORPTION_LIGHT_VEGITATION );

    } else {
        // Default is an urban enviornment. There are alot of codes that go into here.
        // Not great at short range attenuation, better at long range attenuation.
        // More attenuation in the winter.
        return snowbonus + ( ( horde_signal ) ? 12 : SOUND_ABSORPTION_OPEN_FIELD );
    }
}

void map::cull_heard_sounds()
{
    //If we dont clear our filtered sounds list map, there *will* be errors.
    const int soundnumbefore = m_sound_cache.sound_instances.size();
    const int soundlistnum = m_sound_cache.sound_list_filtered.size();
    m_sound_cache.sound_list_filtered.clear();
    //Now we can safely cull sounds.
    std::erase_if( m_sound_cache.sound_instances, []( const auto & sound ) {
        return sound.heard_by_monsters && sound.heard_by_player && sound.heard_by_npcs;
    } );
    const int soundnumafter = m_sound_cache.sound_instances.size();
    m_sound_cache.sounds_culled_this_turn += ( std::max( 0, soundnumbefore - soundnumafter ) );
    m_sound_cache.filtered_sound_lists_cleared += soundlistnum;

}
// TODO: method for getting the escaped sounds per fascing side
// Creates a sound_instance_cache by "flood filling" a given sound event through the absorption map of the given z-level.
// This sound_instance_cache is then added to the sound_instance_caches vector in map.
// This version is usually only used by the player and a few other unique sound sources. Most sounds are batch flooded.
// Fear nothing but the consequences of your own poor decisions.
void map::flood_fill_sound( const sound_event soundevent, const int zlev )
{
    const auto &map_cache = get_cache( zlev );
    const auto &absorption_cache = map_cache.absorption_cache;
    const auto &outside_cache = map_cache.outside_cache;
    const auto &wall_present = map_cache.sound_wall_cache;
    // Use if checking to make sure sounds dont get LOUDER over distance.
    const auto &origin_volume = dBspl_to_mdBspl( soundevent.volume );
    // If we are at max zlev, we dont want to try to check whats above us to avoid out of bounds checks.
    const bool check_up_valid = zlev < OVERMAP_HEIGHT;
    // Make sure that there is a valid reference to A mapcache, even if we dont call it if we are too high up.
    const auto &up_map_cache = ( check_up_valid ) ? get_cache( zlev + 1 ) : map_cache;
    const bool is_winter =  season_of_year( calendar::turn ) == WINTER;
    const auto &snowbonus = ( is_winter ) ? SOUND_ABSORPTION_SNOW_BONUS : SOUND_ABSORPTION_OPEN_FIELD;

    //// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
    //// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
    //// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
    // 8 is the center, and should not normally be called. Kept incase of a sound looping back to its origin point.
    std::array<point_bub_ms, 8> adjacent_tiles;

    auto get_san_dir = [&]( const uint8_t &dir ) -> uint8_t{
        return get_sound_direction_index( dir );
    };

    auto cmp = []( const propagation_tile & a, const propagation_tile & b ) {
        return a.vol < b.vol;
    };
    // max-heap: highest volume processed first. We clear this after each sound processed. pqt = priority tile que
    std::priority_queue<propagation_tile, std::vector<propagation_tile>, decltype( cmp )> ptq( cmp );

    if( dBspl_to_mdBspl( soundevent.volume ) >= ( 700 ) ) {

        const auto vol_enum = get_flood_dist_enum( soundevent.volume );
        const auto f_radius = get_flood_radius_by_enum( vol_enum );
        // We need a non-const version to hand to our sound_instance_cache.
        sound_event sound_event_copy = soundevent;
        sound_instance_cache temp_sound_cache( sound_event_copy, vol_enum, f_radius );
        auto &svol = temp_sound_cache.volume;

        temp_sound_cache.source_indoors = !outside_cache[map_cache.idx( temp_sound_cache.origin.x(),
                                                         temp_sound_cache.origin.y() )];

        // Set this for use with the slightly cheaper direct line propagation.
        temp_sound_cache.terrain_sound_absorbtion_at_source = absorption_cache[map_cache.idx(
                    temp_sound_cache.origin.x(), temp_sound_cache.origin.y() )];
        // And lets make a pair of vectors to store our up and down escapes. Stored as volume, distance. We seed the first value at 0,0.
        std::vector<std::pair<const short, const uint8_t>> up_escape_vector = {{0, 0}};
        std::vector<std::pair<const short, const uint8_t>> down_escape_vector = {{0, 0}};


        //Initialize our checkvars array
        // We need to check the tiles 1 outside our actual envelope as well.
        // Little bit of shenanagins.
        const auto vol_enum_index = get_san_dir( static_cast<uint8_t>( vol_enum ) );
        const auto &actual_check_radius = total_check_radius_by_index[vol_enum_index];
        // const auto &actual_env_length = total_check_envelop_by_index[vol_enum_index];
        // As our checkvar envelope is of a set size, we need to mark our absolute index position and our "relative" index position.
        const auto checkvar_index_p = temp_sound_cache.origin + point{-total_check_radius_DEAFENING, -total_check_radius_DEAFENING};
        // const auto checkvar_rel_index_p = temp_sound_cache.origin + point{-total_check_radius, -total_check_radius};
        // const auto checkvar_rel_index_opp_p = temp_sound_cache.origin + point{total_check_radius, total_check_radius};
        // Our checkvar index point is located at 0,0 of our checkvar envelope.
        // We only actually check from our rel_index_p to our rell_index_opp_p
        const auto cv_env_rel_ms_adj = point{-checkvar_index_p.x(), -checkvar_index_p.y()};
        // Located at 0,0 of our flood envelope
        // const auto &f_env_index_p = temp_sound_cache.envelope_index_point;
        // const auto f_env_rel_ms_adj = point{-f_env_index_p.x(), -f_env_index_p.y()};

        // The start x/y adjustment to where we actually want to check.
        const uint8_t startxy = total_check_radius_DEAFENING - actual_check_radius;
        const uint8_t endxy = total_check_envelope_DEAFENING - startxy;

        /** Windows MSVS fails compile with variable length arrays while everything else works. Max size it is.
        *    @param checkvars[][][0] = terrain base sound attenuation cases 1 & 3
        *    @param checkvars[][][1] = terrain base sound attenuation cases 2 & 3
        *    @param checkvars[][][2] = tile sound attenuation cases 1 & 3
        *    @param checkvars[][][3] = tile sound attenuation cases 2 & 3
        *    @param checkvars[][][4] = True if there is a roof above us. If there is no roof and we are not indoors, we can escape up.
        *    @param checkvars[][][5] = True if no floor in this tile. If there is no floor in our tile, we can escape down.
        *    @param checkvars[][][6] = Are we outside?
        *    @param checkvars[][][7] = Does the tile count as a sound wall?
        *   It is possible for all bits to be false, but not for all bits to be true.
        */
        std::bitset<8> checkvars[total_check_envelope_DEAFENING][total_check_envelope_DEAFENING] = {{0}};

        // Our x/y zone is max +27
        // We now poll all of the relevant tiles in or adjacent to our envelope and encode the conditions in our bitset.
        // We are likely not filling out our entire checkvar array, just the entries that correlate to the relevant tiles.
        for( uint8_t x = startxy; x < endxy; x++ ) {
            for( uint8_t y = startxy; y < endxy; y++ ) {
                const auto env_tile = checkvar_index_p + point{x, y};
                auto &env_tile_vars = checkvars[x][y];
                if( !inbounds( env_tile ) ) {
                    // If we are out of bounds, set all bits to true to indicate this.
                    env_tile_vars.set();
                    continue;
                } else {
                    // envelope tile map idx
                    const auto et_midx = map_cache.idx( env_tile.x(), env_tile.y() );
                    const auto tile_absorp = absorption_cache[et_midx];
                    // First two bits set our base terrain absorption from landusecode
                    const uint8_t base_absorp = get_base_absorp_index( tile_absorp );
                    const uint8_t struc_absorp = get_tile_absorp_index( tile_absorp );
                    // Redo the encoding here to make more compact. 6 total cases for the various absorption, which can fit on 3 bits
                    // Which gives us space for floor chache on this level check
                    env_tile_vars[0] = ( base_absorp == 1 ||
                                         base_absorp == 3 ); // terrain base sound attenuation cases 1 & 3
                    env_tile_vars[1] = ( base_absorp == 2 ||
                                         base_absorp == 3 ); // terrain base sound attenuation cases 2 & 3
                    env_tile_vars[2] = ( struc_absorp == 1 || struc_absorp == 3 ); // tile sound attenuation cases 1 & 3
                    env_tile_vars[3] = ( struc_absorp == 2 || struc_absorp == 3 ); // tile sound attenuation cases 2 & 3
                    env_tile_vars[4] = ( check_up_valid ) ? up_map_cache.floor_cache[et_midx] > 0 :
                                       false; // Is there a roof above us?
                    env_tile_vars[5] = map_cache.floor_cache[et_midx] ==
                                       0; // If there is no floor in our tile, we can escape down.
                    env_tile_vars[6] = outside_cache[et_midx]; // are we outside?
                    env_tile_vars[7] = wall_present[et_midx]; // Does the tile count as a sound wall?
                    // It is impossible for all of these bools to be true at once.
                    // So if they are all true, we know the tile is out of bounds.
                    // If we have no roof, we are outdoors, no soundwall and it is winter, we can assume that there is snow cover.
                }
            }
        }
        // Set our initial conditions. We want 100ths of a decibel for the volume
        // We dont apply directional sound propagation penalties at the very start.
        // the volume vector in the sound instance IS NOT aligned with normal game tripoints, and uses a limited local "area"
        // The origin will always be located at (radius, radius), equivalent to and index position of (radius * ( (2 * radius) + 1 ) + radius)
        svol[temp_sound_cache.p_to_env_index( temp_sound_cache.origin )] =  origin_volume;
        auto &checkvars_origin = checkvars[total_check_radius_DEAFENING][total_check_radius_DEAFENING];

        // If we are not indoors and there is no floor on the tile above us, escape up.
        // We know by definition that our origin tile is located at f_radius, f_radius or check_radius,check_radius respectively.
        if( !temp_sound_cache.source_indoors && check_up_valid ) {
            if( !checkvars_origin[4] ) {
                up_escape_vector.push_back( {origin_volume, 1} );
            }
        }
        // If there is no floor where we are, escape down.
        if( checkvars_origin[5] ) {
            down_escape_vector.push_back( {origin_volume, 1} );
        }
        adjacent_tiles = get_adjacent_tiles( temp_sound_cache.origin.xy() );

        // Returns the flood envelope volume vector index position given some normal tripoint.
        // We only want to flood fill within the boundries of our floodfill envelope. We get our index point from our origin point and floodfill radius.
        // From that we get our x and y offsets.
        auto v_index_from_p = [&]( const point_bub_ms & p ) {
            return temp_sound_cache.p_to_env_index( p );
        };

        auto check_escape = [&]( const int &cv_env_x, const int &cv_env_y, const short & tile_vol,
        const uint8_t &dist ) {
            if( checkvars[cv_env_x][cv_env_y][6] && check_up_valid ) {
                if( !checkvars[cv_env_x][cv_env_y][4] ) {
                    up_escape_vector.push_back( {tile_vol, dist} );
                }
            }
            if( checkvars[cv_env_x][cv_env_y][5] ) {
                down_escape_vector.push_back( {tile_vol, dist} );
            }
        };

        // This propagates the sounds from the source tile to the 8 adjacent tiles, setting initial directions, distances and volumes.
        // Adj tiles are 0-7. The adjacent tiles at the very start will always be in the envelope, as the envelope is always atleast 3x3.
        for( uint8_t i : sanitized_sound_direction_indexes ) {
            const auto &tile = adjacent_tiles[i];
            const auto cv_trip = tile + cv_env_rel_ms_adj;
            const auto &tile_checkvars = checkvars[cv_trip.x()][cv_trip.y()];
            // Lets make sure that we only propagate inbounds
            if( !tile_checkvars.all() ) {
                // If our sound started inside, lets check if it got outside.

                const auto &tile_outdoors = tile_checkvars[6];
                if( temp_sound_cache.source_indoors && tile_outdoors ) {
                    temp_sound_cache.escaped_indoors = true;
                }
                const int vol_index = v_index_from_p( tile );
                auto &tile_svol = svol[vol_index];
                // Set our initial distance to 2. At the source there is no sound direction distance modifier.
                // And set our tile volume based on the distance. We know that the sound origin is atleast 1600mdB.
                // Set our direction based upon the adjacent tile index.
                tile_svol = std::max( 0,
                                      ( origin_volume - dist_vol_loss[2] - ( absorption_from_checkvar_bitset( tile_checkvars ) ) -
                                        snowbonus ) ) ;
                if( tile_svol > ( SOUND_ABSORPTION_OPEN_FIELD ) ) {

                    // Check this if sound propagation is acting up.
                    if( tile_svol > origin_volume ) {
                        debugmsg( "Sound with description [ %1s ] attempted to propagate from %i:%i at %i mdB to %i:%i at %i mdB, a louder volume than the origin volume of %i mdB!"
                                  , soundevent.description, soundevent.origin.x(), soundevent.origin.y(),
                                  svol[temp_sound_cache.p_to_env_index( soundevent.origin )], tile.x(), tile.y(), tile_svol,
                                  origin_volume );
                        // Dont break the laws of physics
                        continue;
                    }
                    // If there is no floor above or below with a few other conditions, escape.
                    check_escape( cv_trip.x(), cv_trip.y(), tile_svol, 2 );

                    if( temp_sound_cache.in_envelope( tile ) && tile_svol >= SOUND_MINIMUM_VOLUME_FOR_PROPAGATION ) {
                        ptq.emplace( propagation_tile( tile, tile_svol, i, 2 ) );
                    }
                }
            }
        }

        std::pair<bool, bool> wall_bools;

        auto check_walls = [&]( const uint8_t &dir ) -> void{
            const auto &wall_dirs = wall_check_by_sdirection[get_san_dir( dir )];
            const auto &wall1_cv_p = adjacent_tiles[wall_dirs.first] + cv_env_rel_ms_adj;
            const auto &wall1_checkvars = checkvars[wall1_cv_p.x()][wall1_cv_p.y()];
            const auto &wall2_cv_p = adjacent_tiles[wall_dirs.second] + cv_env_rel_ms_adj;
            const auto &wall2_checkvars = checkvars[wall2_cv_p.x()][wall2_cv_p.y()];
            const auto &wall1 = wall1_checkvars[7];
            const auto &wall2 = wall2_checkvars[7];
            wall_bools = get_s_wall_bool_pair( wall1, wall2 );
        };

        auto spropagate_from_tile = [&]( const propagation_tile & top_of_que ) {
            // Its possible if something goes wrong to slot a different top_of_que in part way through this proccess, so prof against that.
            const auto ptile = top_of_que;
            // Now that we have our const reference, pop the old top listing.
            ptq.pop();

            // We know that we are not propagating from a tile along the map border, so it is safe to check for walls.
            // Grab our adjacent tiles, and the values for our center tile.
            adjacent_tiles = get_adjacent_tiles( ptile.pos );

            const auto &san_pdir = get_san_dir( ptile.dir );
            // We only have two walls to check for, use teh lamba.
            check_walls( san_pdir );
            // Iterate through adjacent tiles.
            const auto &dirs_to_check = spropagation_tiles_by_sdirection[san_pdir];
            for( uint8_t i : dirs_to_check ) {

                auto &adj_tile_dir = i;
                // Only check if we know a wall is present.
                if( wall_bools.first || wall_bools.second ) {
                    if( skip_due_to_wall( get_s_wall_bool_pair( wall_bools.first, wall_bools.second ), san_pdir,
                                          adj_tile_dir ) ) {
                        continue;
                    }
                }
                auto &adj_tile = adjacent_tiles[adj_tile_dir];
                const auto adj_tile_cv_env = adj_tile + cv_env_rel_ms_adj;
                const auto &adj_tile_checkvars = checkvars[adj_tile_cv_env.x()][adj_tile_cv_env.y()];

                // Dont check tiles that are not valid for propagation, i.e. behind the direction of sound, around a corner, out of bound
                if( !adj_tile_checkvars.all() && temp_sound_cache.in_envelope( adj_tile ) ) {

                    auto &adj_tile_vol = svol[v_index_from_p( adj_tile )];
                    // Cap our tile distance between 1 and 121 to prevent overflow. We dont have or need distance loss values past dist_vol_loss[121]
                    // as the change in distance loss values past this point are negligible for gameplay scale.
                    const uint8_t dist_for_vol_loss = get_distance_for_volume_loss( ptile.dist,
                                                      ( adj_tile_dir == dirs_to_check.front() ||
                                                        adj_tile_dir == dirs_to_check.back() ) );
                    const short vol_to_check = std::max( 0,
                                                         ( ptile.vol - ( absorption_from_checkvar_bitset( adj_tile_checkvars ) ) -
                                                           ( dist_vol_loss[dist_for_vol_loss] ) - snowbonus ) );
                    // General priority goes loudest volume, then largest distance. Smaller distances loose volume more quickly.
                    // If volumes are equal and directions are one off from eachother, the cardinal direction wins.
                    // We dont want to track inaudible single dB values across the entire map for each sound.
                    if( ( vol_to_check ) > adj_tile_vol ) {
                        // Check this if sound propagation is acting up. Comment out if things are playing nice.
                        if( vol_to_check > origin_volume ) {
                            debugmsg( "Sound with description [ %1s ] attempted to propagate from %i:%i at %i mdB to %i:%i at %i mdB, a louder volume than the origin volume of %i mdB!"
                                      , soundevent.description, ptile.pos.x(), ptile.pos.y(), ptile.vol, adj_tile.x(), adj_tile.y(),
                                      vol_to_check,
                                      origin_volume );
                            // Dont break the laws of physics
                            continue;
                        }
                        check_escape( adj_tile_cv_env.x(), adj_tile_cv_env.y(), vol_to_check, dist_for_vol_loss );

                        adj_tile_vol = vol_to_check;
                        if( adj_tile_vol > SOUND_ABSORPTION_OPEN_FIELD ) {
                            // If the tiles new volume is greater than the old one and is inside our propagation envelope, propagate.
                            // Will not update if the adjacent tile is along the map boundry.
                            ptq.emplace( propagation_tile( adj_tile, vol_to_check, adj_tile_dir, dist_for_vol_loss ) );
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
            // Our loudest sound should already have been removed by spropagate, and a new set of tiles potentially added.

        }
        // Now we want to build out our directional escape vectors. The length of any one side of our envelop is 1 + 2 * radius, or just 2 * radius when we start from 0
        // 0 1 2    We take the rms value of the set of volumes greater than 0 along each cartesian boundary.
        // 7   3    For the diagonals, we take the rms volume of the two adjacent cartesian directions.
        // 6 5 4
        auto &escape_direction_vol = temp_sound_cache.base_distance_vol_by_dir;
        const int envelope_width = get_flood_envelope_by_enum( temp_sound_cache.dist_enum );
        const int env_2r = f_radius * 2;
        double vol_tally = 0;
        int non_zero = 0;

        auto comp_cart_escape = [&]( const uint8_t &cart_dir ) -> void{
            escape_direction_vol[cart_dir] = ( non_zero == 0 ) ? 0 : static_cast<short>( std::round( sqrt( vol_tally  / non_zero ) ) );
            vol_tally = 0;
            non_zero = 0;
        };

        for( int i = 0; i < envelope_width; i++ ) {
            // Starting with north escapes, so all of our desired volumes will be at envelope_y = radius * 2.
            if( svol[( i * envelope_width ) + env_2r] > 0 ) {
                non_zero++;
                vol_tally += pow( svol[( i * envelope_width ) + env_2r], 2 );
            }
        }
        comp_cart_escape( SDI_N );

        for( int i = 0; i < envelope_width; i++ ) {
            // For east escapes all all of our desired volumes will be at envelope_x = radius * 2.
            if( svol[( ( env_2r ) * envelope_width ) + i] > 0 ) {
                non_zero++;
                vol_tally += pow( svol[( env_2r * envelope_width ) + i], 2 );
            }
        }
        comp_cart_escape( SDI_E );

        for( int i = 0; i < envelope_width; i++ ) {
            // For south escapes all all of our desired volumes will be at envelope_y = 0.
            if( svol[( i * envelope_width )] > 0 ) {
                non_zero++;
                vol_tally += pow( svol[ i * envelope_width ], 2 );
            }
        }
        comp_cart_escape( SDI_S );

        for( int i = 0; i < envelope_width; i++ ) {
            // For west escapes all all of our desired volumes will be at envelope_x = 0.
            if( svol[ i ] > 0 ) {
                non_zero++;
                vol_tally += pow( svol[ i ], 2 );
            }
        }
        comp_cart_escape( SDI_W );

        // Now run through our diagonals.
        for( uint8_t i : sanitized_sound_direction_indexes_diagonal ) {
            const auto &cclockwise = wall_check_by_sdirection[i].first;
            const auto &clockwise = wall_check_by_sdirection[i].second;
            escape_direction_vol[i] = ( escape_direction_vol[clockwise] != 0 ||
                                        escape_direction_vol[cclockwise] != 0 ) ? sqrt( ( pow( escape_direction_vol[cclockwise],
                                                2 ) + pow( escape_direction_vol[clockwise], 2 ) ) / 2 ) : 0;
        }
        // Now lets sum up our up and down escapes. If our distance value is less than our flood radius, reduce volume by the appropriate amount.

        for( std::pair<short, uint8_t> listing : down_escape_vector ) {
            if( listing.first > 0 ) {
                short vol = listing.first;
                if( listing.second < temp_sound_cache.flood_radius ) {
                    for( uint8_t i = listing.second; i < temp_sound_cache.flood_radius; i++ ) {
                        vol -= dist_vol_loss[i + 1];
                    }
                }
                if( vol > 0 ) {
                    vol_tally += pow( vol, 2 );
                    non_zero++;
                }
            }
        }
        // 8 is down, 9 is up.
        comp_cart_escape( SDI_DOWN );

        for( std::pair<short, uint8_t> listing : up_escape_vector ) {
            if( listing.first > 0 ) {
                short vol = listing.first;
                if( listing.second < temp_sound_cache.flood_radius ) {
                    for( uint8_t i = listing.second; i < temp_sound_cache.flood_radius; i++ ) {
                        vol -= dist_vol_loss[i + 1];
                    }
                }
                if( vol > 0 ) {
                    vol_tally += pow( vol, 2 );
                    non_zero++;
                }
            }
        }
        // 8 is down, 9 is up.
        comp_cart_escape( SDI_UP );

        // And from the maximum escape volume, approximate our minvol radius for easy distance filtering.
        vol_tally = 0;
        for( uint8_t dir : sanitized_sound_direction_indexes_full ) {
            auto &esc_vol = escape_direction_vol[dir];
            if( esc_vol > origin_volume ) {
                const auto &sop = temp_sound_cache.origin;
                debugmsg( "Sound with description [ %1s ] from %i:%i:%i at %i mdB has impossible escape volume in direction %i of %i mdB ",
                          temp_sound_cache.sound.description, sop.x(), sop.y(), sop.z(), origin_volume, dir, esc_vol );
                esc_vol = origin_volume;
            }
            vol_tally = std::max( vol_tally, static_cast<double>( esc_vol ) );
        }
        vol_tally = std::round( vol_tally );
        // We use this for an easy distance check threshold when feeding monsters sound.
        temp_sound_cache.approximate_minvol_distance = average_minvol_distance(
                    temp_sound_cache.flood_radius, static_cast<short>( vol_tally ),
                    temp_sound_cache.terrain_sound_absorbtion_at_source );

        // The sound cache should be built out by now.
        // Add our new sound cache to the games sound_caches vector.
        // add_msg(m_debug, _("Attempting to add sound_cache with origin %i x: %i y: %i z and source volume %i to sound_caches vector ."), temp_sound_cache.sound.origin.x, temp_sound_cache.sound.origin.y, temp_sound_cache.sound.origin.z, temp_sound_cache.sound.volume );
        m_sound_cache.sound_instances.push_back( temp_sound_cache );
    }

}

// Batch flood fills a given vector of sound events, stepping through all z levels.
// New sound_cache are then added to the sound_caches vector in map.
void map::batch_flood_fill_sounds()
{
    ZoneScoped;
    // Our que of sound events to flood fill
    auto &batch_que = sound_batch_floodfill_que;
    const bool is_winter =  season_of_year( calendar::turn ) == WINTER;
    const auto &snowbonus = ( is_winter ) ? SOUND_ABSORPTION_SNOW_BONUS : SOUND_ABSORPTION_OPEN_FIELD;

    // How many sounds did we actually process?
    auto &num_processed_mon_sounds = m_sound_cache.batch_flooded_monster_sounds;
    auto &num_processed_NPC_sounds = m_sound_cache.batch_flooded_NPC_sounds;
    auto &num_invalidated_sounds = m_sound_cache.invalidated_batch_sounds;

    //// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
    //// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
    //// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
    // 8 is the center, and should not normally be called. Kept incase of a sound looping back to its origin point.
    std::array<point_bub_ms, 8> adjacent_tiles;

    std::pair<bool, bool> wall_bools;

    // Grab a constexpr version of our sound direction
    auto get_san_dir = [&]( const uint8_t &dir ) -> uint8_t{
        return get_sound_direction_index( dir );
    };

    auto cmp = []( const propagation_tile & a, const propagation_tile & b ) {
        return a.vol < b.vol;
    };

    // max-heap: highest volume processed first. We clear this after each sound processed. pqt = priority tile que
    std::priority_queue<propagation_tile, std::vector<propagation_tile>, decltype( cmp )> ptq( cmp );

    // We will just use one checkvars envelope for all the sounds, and set it to zero when we begin to floodfill a sound.

    /** Windows MSVS wont compile with variable sized multidimension arrays but everything else will. So we just use the max size.
    *    @param checkvars[][][0] = terrain base sound attenuation cases 1 & 3
    *    @param checkvars[][][1] = terrain base sound attenuation cases 2 & 3
    *    @param checkvars[][][2] = tile sound attenuation cases 1 & 3
    *    @param checkvars[][][3] = tile sound attenuation cases 2 & 3
    *    @param checkvars[][][4] = True if there is a roof above us. If there is no roof and we are not indoors, we can escape up.
    *    @param checkvars[][][5] = True if no floor in this tile. If there is no floor in our tile, we can escape down.
    *    @param checkvars[][][6] = Are we outside?
    *    @param checkvars[][][7] = Does the tile count as a sound wall?
    */
    std::bitset<8> checkvars[total_check_envelope_DEAFENING][total_check_envelope_DEAFENING] = {{0}};
    memset( checkvars, 0, sizeof( checkvars ) );

    // Now we step through our zlevels
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {

        const auto &map_cache = get_cache( z );
        const auto &absorption_cache = map_cache.absorption_cache;
        const auto &outside_cache = map_cache.outside_cache;

        // If we are at zlev 10, we dont want to try to check whats above us to avoid out of bounds checks.
        const bool check_up_valid = z < 10;
        // Make sure that there is a valid reference to *a* mapcache, even if we dont call it if we are too high up.
        const auto &up_map_cache = ( check_up_valid ) ? get_cache( z + 1 ) : map_cache;

        // We cycle through all the sounds in the batch que
        for( sound_event flooded_sound : batch_que ) {
            // Skip all sounds that do not originate from our zlevel, are along the map border, or that are too far below our ambient volume.
            if( flooded_sound.origin.z() != z ) {
                // We still floodfill these sounds out later, just when we get to the right z-level.
                continue;
            } else if( flooded_sound.volume < 7 ) {
                num_invalidated_sounds++;
                continue;
            } else {

                // Set our checkvars to zero before we get too much farther and break stuff.
                memset( checkvars, 0, sizeof( checkvars ) );
                sound_instance_cache temp_sound_cache( flooded_sound,
                                                       get_flood_dist_enum( flooded_sound.volume ),
                                                       get_flood_radius_by_enum( get_flood_dist_enum(
                                                               flooded_sound.volume ) ) );
                auto &svol = temp_sound_cache.volume;
                auto &f_radius = temp_sound_cache.flood_radius;
                temp_sound_cache.source_indoors = !outside_cache[map_cache.idx( temp_sound_cache.origin.x(),
                                                                 temp_sound_cache.origin.y() )];
                auto &escape_vol = temp_sound_cache.base_distance_vol_by_dir;
                // Set this for use with the slightly cheaper direct line propagation.
                temp_sound_cache.terrain_sound_absorbtion_at_source = absorption_cache[map_cache.idx(
                            temp_sound_cache.origin.x(), temp_sound_cache.origin.y() )];
                const auto vol_enum_index = get_san_dir( static_cast<uint8_t>( get_flood_dist_enum(
                                                flooded_sound.volume ) ) );
                const auto &actual_check_radius = total_check_radius_by_index[vol_enum_index];
                // const auto &actual_env_length = total_check_envelop_by_index[vol_enum_index];
                const auto checkvar_index_p = temp_sound_cache.origin + point{-total_check_radius_DEAFENING, -total_check_radius_DEAFENING};
                // Our checkvar index point is located at 0,0 of our checkvar envelope.
                const auto cv_env_rel_ms_adj = point{-checkvar_index_p.x(), -checkvar_index_p.y()};
                // Located at 0,0 of our flood envelope
                // const auto &f_env_index_p = temp_sound_cache.envelope_index_point;
                // const auto f_env_rel_ms_adj = point{-f_env_index_p.x(), -f_env_index_p.y()};
                const uint8_t startxy = total_check_radius_DEAFENING - actual_check_radius;
                const uint8_t endxy = total_check_envelope_DEAFENING - startxy;


                // We run through this often enough that we might as well make a lambda of it.
                auto check_walls = [&]( const uint8_t &dir ) -> void{
                    const auto &wall_dirs = wall_check_by_sdirection[get_san_dir( dir )];
                    const auto &wall1_cv_p = adjacent_tiles[wall_dirs.first] + cv_env_rel_ms_adj;
                    const auto &wall1_checkvars = checkvars[wall1_cv_p.x()][wall1_cv_p.y()];
                    const auto &wall2_cv_p = adjacent_tiles[wall_dirs.second] + cv_env_rel_ms_adj;
                    const auto &wall2_checkvars = checkvars[wall2_cv_p.x()][wall2_cv_p.y()];
                    const auto &wall1 = wall1_checkvars[7];
                    const auto &wall2 = wall2_checkvars[7];
                    wall_bools = get_s_wall_bool_pair( wall1, wall2 );
                };

                // Given some point, return the corresponding volume vector index.
                // Only use after confirming that the tile is within the envelope using in_envelope().
                auto v_index_from_p = [&]( const point_bub_ms & p ) {
                    return temp_sound_cache.p_to_env_index( p );
                };

                for( uint8_t x = startxy; x < endxy; x++ ) {
                    for( uint8_t y = startxy; y < endxy; y++ ) {
                        const auto env_tile = checkvar_index_p + point{x, y};
                        auto &env_tile_vars = checkvars[x][y];
                        if( !inbounds( env_tile ) ) {
                            // If we are out of bounds, set all bits to true to indicate this.
                            env_tile_vars.set();
                            continue;
                        } else {
                            // envelope tile map idx
                            const auto et_midx = map_cache.idx( env_tile.x(), env_tile.y() );
                            const auto tile_absorp = absorption_cache[et_midx];
                            // First two bits set our base terrain absorption from landusecode
                            const uint8_t base_absorp = get_base_absorp_index( tile_absorp );
                            const uint8_t struc_absorp = get_tile_absorp_index( tile_absorp );
                            // Redo the encoding here to make more compact. 6 total cases for the various absorption, which can fit on 3 bits
                            // Which gives us space for floor chache on this level check
                            env_tile_vars[0] = ( base_absorp == 1 ||
                                                 base_absorp == 3 ); // terrain base sound attenuation cases 1 & 3
                            env_tile_vars[1] = ( base_absorp == 2 ||
                                                 base_absorp == 3 ); // terrain base sound attenuation cases 2 & 3
                            env_tile_vars[2] = ( struc_absorp == 1 || struc_absorp == 3 ); // tile sound attenuation cases 1 & 3
                            env_tile_vars[3] = ( struc_absorp == 2 || struc_absorp == 3 ); // tile sound attenuation cases 2 & 3
                            env_tile_vars[4] = ( check_up_valid ) ? up_map_cache.floor_cache[et_midx] > 0 :
                                               false; // Is there a roof above us?
                            env_tile_vars[5] = map_cache.floor_cache[et_midx] ==
                                               0; // If there is no floor in our tile, we can escape down.
                            env_tile_vars[6] = outside_cache[et_midx]; // are we outside?
                            env_tile_vars[7] = map_cache.sound_wall_cache[et_midx]; // Does the tile count as a sound wall?
                            // It is impossible for all of these bools to be true at once.
                            // So if they are all true, we know the tile is out of bounds.
                            // If we have no roof, we are outdoors, no soundwall and it is winter, we can assume that there is snow cover.
                        }
                    }
                }

                auto check_escape = [&]( const int &cv_env_x, const int &cv_env_y, const short & tile_vol,
                const uint8_t &dist ) {
                    const auto &tilevars = checkvars[cv_env_x][cv_env_y];
                    if( tilevars[6] && check_up_valid ) {
                        if( !tilevars[4] && tile_vol > escape_vol[SDI_UP] ) {
                            short vol = tile_vol;
                            if( dist < f_radius ) {
                                vol -= get_cumulative_vol_dist_loss( dist, f_radius,
                                                                     temp_sound_cache.terrain_sound_absorbtion_at_source );
                            }
                            escape_vol[SDI_UP] = std::max( vol, escape_vol[SDI_UP] );
                        }
                    }
                    if( tilevars[5] && tile_vol > escape_vol[SDI_DOWN] ) {
                        short vol = tile_vol;
                        if( dist < f_radius ) {
                            vol -= get_cumulative_vol_dist_loss( dist, f_radius,
                                                                 temp_sound_cache.terrain_sound_absorbtion_at_source );
                        }
                        escape_vol[SDI_DOWN] = std::max( vol, escape_vol[SDI_DOWN] );
                    }
                };
                // Set our initial conditions. We want 100ths of a decibel for the volume
                // We dont apply directional sound propagation penalties at the very start.
                // The center of our flood envelope is always (flood radius, flood radius). Index location math is (radius * ( ( 2 * radius ) + 1 ) + radius) = 2 * ( radius * radius ) + ( 2 * radius )
                auto &origin_volume = svol[temp_sound_cache.p_to_env_index( temp_sound_cache.origin )];
                origin_volume =  dBspl_to_mdBspl( temp_sound_cache.sound.volume );
                adjacent_tiles = get_adjacent_tiles( temp_sound_cache.sound.origin.xy() );

                const auto &orig_to_cv = temp_sound_cache.origin + cv_env_rel_ms_adj;
                check_escape( orig_to_cv.x(), orig_to_cv.y(),
                              origin_volume - dist_vol_loss[2], 2 );

                // This propagates the sounds from the source tile to the 8 adjacent tiles, setting initial directions, distances and volumes.
                // Adj tiles are 0-7
                for( uint8_t i : sanitized_sound_direction_indexes ) {
                    const auto &tile = adjacent_tiles[i];
                    const auto cv_env_tile = tile + cv_env_rel_ms_adj;
                    const auto &t_checkvars = checkvars[cv_env_tile.x()][cv_env_tile.y()];
                    // Lets make sure that we only propagate inbounds, and not along the map border. After this we can just check !tile_along_map_border
                    // We know that our initial adjacent tiles will always be inside the envelope.
                    if( !t_checkvars.all() ) {
                        const auto vol_index = v_index_from_p( tile );
                        // Set our initial distance to 2. At the source there is no sound direction distance modifier.
                        // And set our tile volume based on the distance. We know that the sound origin is atleast 1600mdB.
                        // Set our direction based upon the adjacent tile index.
                        svol[vol_index] = std::max( 0,
                                                    ( origin_volume -  dist_vol_loss[2] - absorption_from_checkvar_bitset(
                                                          t_checkvars ) - snowbonus ) );


                        if( temp_sound_cache.in_envelope( tile ) && svol[vol_index] > 0 ) {

                            check_escape( cv_env_tile.x(), cv_env_tile.y(), svol[vol_index], 2 );

                            ptq.emplace( propagation_tile( tile, svol[vol_index], i, 2 ) );
                        }
                    }
                }

                auto spropagate_from_tile = [&]( const propagation_tile & top_of_que ) {
                    // Make a more consistant reference so we dont potentially somehow sort away our reference.
                    const auto ptile = top_of_que;
                    // Remove the old top listing.
                    ptq.pop();

                    // We know that we are not propagating from a tile along the map border, so it is safe to check for walls.
                    // Grab our adjacent tiles, and the values for our center tile.
                    adjacent_tiles = get_adjacent_tiles( ptile.pos );
                    // Set our wall1 and wall2 bools
                    const auto &san_pdir = get_san_dir( ptile.dir );
                    check_walls( ptile.dir );

                    // Iterate through adjacent tiles.
                    const auto &dirs_to_check = spropagation_tiles_by_sdirection[san_pdir];
                    for( uint8_t adj_tile_dir : dirs_to_check ) {
                        // Check should be being fed only constexpr inputs by this point.
                        if( skip_due_to_wall( get_s_wall_bool_pair( wall_bools.first, wall_bools.second ), san_pdir,
                                              adj_tile_dir ) ) {
                            continue;
                        }

                        const auto &adj_tile = adjacent_tiles[adj_tile_dir];
                        const auto adj_tile_cve = adj_tile + cv_env_rel_ms_adj;
                        const auto &adjt_checkvars = checkvars[adj_tile_cve.x()][adj_tile_cve.y()];
                        // Dont check tiles that are not valid for propagation, i.e. behind the direction of sound, around a corner, or out of bounds.
                        if( temp_sound_cache.in_envelope( adj_tile ) && !adjt_checkvars.all() ) {
                            auto &adj_tile_vol = svol[v_index_from_p( adj_tile )];
                            // Cap our tile distance between 1 and 121 to prevent overflow. We dont have or need distance loss values past dist_vol_loss[121]
                            // as the change in distance loss values past this point are negligible for gameplay scale.
                            const uint8_t dist_for_vol_loss = get_distance_for_volume_loss( ptile.dist,
                                                              ( adj_tile_dir == dirs_to_check.front() ||
                                                                adj_tile_dir == dirs_to_check.back() ) );
                            const short vol_to_check = std::max( 0,
                                                                 ( ptile.vol - absorption_from_checkvar_bitset( adjt_checkvars ) -
                                                                   ( dist_vol_loss[dist_for_vol_loss] ) - snowbonus ) );
                            // General priority goes loudest volume, then largest distance. Smaller distances loose volume more quickly.
                            // If volumes are equal and directions are one off from eachother, the cardinal direction wins.
                            // We dont want to track inaudible single dB values across the entire map for each sound.
                            if( vol_to_check > adj_tile_vol ) {
                                // Check this if sound propagation is acting up. Comment out if things are playing nice.
                                if( vol_to_check > dBspl_to_mdBspl( temp_sound_cache.sound.volume ) ) {
                                    debugmsg( "Sound with description [ %1s ] attempted to propagate from %i:%i at %i mdB to %i:%i at %i mdB, a louder volume than the origin volume of %i mdB!"
                                              , temp_sound_cache.sound.description, ptile.pos.x(), ptile.pos.y(), ptile.vol, adj_tile.x(),
                                              adj_tile.y(),
                                              vol_to_check, origin_volume );
                                    // Dont break the laws of physics
                                    continue;
                                }
                                adj_tile_vol = vol_to_check;

                                check_escape( adj_tile_cve.x(), adj_tile_cve.y(), vol_to_check, dist_for_vol_loss );

                                if( adj_tile_vol > SOUND_ABSORPTION_OPEN_FIELD ) {
                                    // If the tiles new volume is greater than our old one and is inside the envelope, mark it for update.
                                    // Will not update if the adjacent tile is along the map boundry.
                                    ptq.emplace( propagation_tile( adj_tile, vol_to_check, adj_tile_dir, dist_for_vol_loss ) );
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
                    // After calculating our loudest sound should already have been removed.
                }

                // Probably a cleaner way to do this but oh well.
                // Less total work than checking if we are at the edge of the envelope, figuring out which side of the envelope, and then incrimenting our bean count every time we propagate a tile.
                // RMS is sqrt( (x1^2 + x2^2 + ... xn^2)/n )
                const int envelope_width = get_flood_envelope_by_enum( flood_dist_enum_by_index[vol_enum_index] );
                const int env_2r = f_radius * 2;
                double vol_tally = 0;
                int non_zero = 0;

                auto comp_cart_escape = [&]( const uint8_t &cart_dir ) -> void{
                    escape_vol[cart_dir] = ( non_zero == 0 ) ? 0 : static_cast<short>( std::round( sqrt( ( vol_tally ) / non_zero ) ) );
                    vol_tally = 0;
                    non_zero = 0;
                };
                for( int i = 0; i < envelope_width; i++ ) {
                    // Starting with north escapes, so all of our desired volumes will be at envelope_y = radius * 2.
                    if( svol[( i * envelope_width ) + env_2r] > 0 ) {
                        non_zero++;
                        vol_tally += pow( svol[( i * envelope_width ) + env_2r], 2 );
                    }
                }
                comp_cart_escape( SDI_N );

                for( int i = 0; i < envelope_width; i++ ) {
                    // For east escapes all all of our desired volumes will be at envelope_x = radius * 2.
                    if( svol[( env_2r * envelope_width ) + i] > 0 ) {
                        non_zero++;
                        vol_tally += pow( svol[( env_2r * envelope_width ) + i], 2 );
                    }
                }
                comp_cart_escape( SDI_E );

                for( int i = 0; i < envelope_width; i++ ) {
                    // For south escapes all all of our desired volumes will be at envelope_y = 0.
                    if( svol[( i * envelope_width )] > 0 ) {
                        non_zero++;
                        vol_tally += pow( svol[ i * envelope_width ], 2 );
                    }
                }
                comp_cart_escape( SDI_S );

                for( int i = 0; i < envelope_width; i++ ) {
                    // For west escapes all all of our desired volumes will be at envelope_x = 0.

                    if( svol[ i ] > 0 ) {
                        non_zero++;
                        vol_tally += pow( svol[ i ], 2 );
                    }
                }
                comp_cart_escape( SDI_W );

                // Now run through our diagonals.
                // Now run through our diagonals.
                for( uint8_t i : sanitized_sound_direction_indexes_diagonal ) {
                    const auto &cclockwise = wall_check_by_sdirection[i].first;
                    const auto &clockwise = wall_check_by_sdirection[i].second;
                    escape_vol[i] = ( escape_vol[clockwise] != 0 ||
                                      escape_vol[cclockwise] != 0 ) ? std::round( sqrt( ( pow( escape_vol[cclockwise],
                                              2 ) + pow( escape_vol[clockwise], 2 ) ) / 2 ) ) : 0;
                }

                if( temp_sound_cache.source_indoors && escape_vol[SDI_UP] > 0 ) {
                    temp_sound_cache.escaped_indoors = true;
                }

                // And from the maximum escape volume, approximate our minvol radius for easy distance filtering.
                vol_tally = 0;
                for( uint8_t dir : sanitized_sound_direction_indexes_full ) {
                    auto &esc_vol = escape_vol[dir];
                    if( esc_vol > origin_volume ) {
                        const auto &sop = temp_sound_cache.origin;
                        debugmsg( "Sound with description [ %1s ] from %i:%i:%i at %i mdB has impossible escape volume in direction %i of %i mdB ",
                                  temp_sound_cache.sound.description, sop.x(), sop.y(), sop.z(), origin_volume, dir, esc_vol );
                        esc_vol = origin_volume;
                    }
                    vol_tally = std::max( vol_tally, static_cast<double>( esc_vol ) );
                }
                // We use this for an easy distance check threshold when feeding monsters sound.
                temp_sound_cache.approximate_minvol_distance = average_minvol_distance(
                            temp_sound_cache.flood_radius, static_cast<short>( vol_tally ),
                            temp_sound_cache.terrain_sound_absorbtion_at_source );


                // The sound cache should be built out by now.
                // Add our new sound cache to the games sound_caches vector.
                // add_msg(m_debug, _("Attempting to add sound_cache with origin %i x: %i y: %i z and source volume %i to sound_caches vector ."), temp_sound_cache.sound.origin.x, temp_sound_cache.sound.origin.y, temp_sound_cache.sound.origin.z, temp_sound_cache.sound.volume );
                m_sound_cache.sound_instances.push_back( temp_sound_cache );
                // add_msg(m_debug, _("Sound cache added to vector"));
                if( temp_sound_cache.from_monster ) {
                    num_processed_mon_sounds++;
                } else if( temp_sound_cache.from_npc ) {
                    num_processed_NPC_sounds++;
                }
                continue;
            }
            // If our sound was not loud enough, invalidate it.
            num_invalidated_sounds++;
            continue;
        }
    }
    batch_que.clear();

}

// Rebuild the sound absorption cache of a given submap
// This will also build out the sound wall cache of the given submap.
auto submap::rebuild_absorption_cache( const map &m, const tripoint_bub_sm &grid_pos ) -> void
{
    if( !absorption_dirty ) {
        return;
    }
    // outside_cache must be current before building the absorption cache.
    // We still want the level cache above us to grab our floor checks with.
    const level_cache *above = ( grid_pos.z() < OVERMAP_HEIGHT )
                               ? &m.get_cache_ref( grid_pos.z() + 1 )
                               : nullptr;

    if( outside_dirty ) {
        // Fortunatly there is a nullptr catch in rebuild_outside_cache.
        rebuild_outside_cache( above, grid_pos );
    }
    const auto &lev_cache = m.get_cache_ref( grid_pos.z() );

    const season_type &season = season_of_year( calendar::turn );
    // If we are at max zlev, always assume the tile above us has no floor.
    // Very important so that we dont try to access a nullptr
    const bool at_max_zlev = !( grid_pos.z() < OVERMAP_HEIGHT );
    const auto &ms_conversion = coords::project_to<coords::ms>( grid_pos );
    const auto &abs_conversion = bub_to_abs( ms_conversion );
    const auto &abs_omt = coords::project_to<coords::omt>( abs_conversion );
    // The default sound absorption for this submap based upon the land use code.
    const short default_terrain_absorption = terrain_sound_attenuation( abs_omt, season );

    // Fill out our absorption cache to the default terrain value.
    std::ranges::fill( std::span( &absorption_cache[0][0], SEEX * SEEY ), default_terrain_absorption );
    // We are by neccesity rebuilding the sound wall cache as well, so lets zero that out.
    std::ranges::fill( std::span( &sound_wall_cache[0][0], SEEX * SEEY ), false );

    // const point_bub_ms abs_p = project_to<coords::ms>( grid_pos ).xy();
    // For use when checking the vehicle cache.
    const tripoint_bub_ms abs_trip = project_to<coords::ms>( grid_pos );
    // If somebody increases the size of submaps beyond what a uint8_t can handle I will beat them to death with a dull rusty spoon.
    constexpr uint8_t checkvars_boundary_max_x = SEEX + 1;
    constexpr uint8_t checkvars_boundary_max_y = SEEY + 1;
    constexpr uint8_t checkvars_envelope_max_x = SEEX + 2;
    constexpr uint8_t checkvars_envelope_max_y = SEEY + 2;
    // So we can use constexpr array values where needed
    const auto &san_sdir = sanitized_sound_direction_indexes;
    /** Instead of repeatedly checking for properties on all sm tiles upwards of 9 time each,
    *   just do it once for the whole submap and store the results as a std::bitset<8> we decode later.
    *   Because we need to check adjacent tiles, our total checkvars envelope is +2 the size of a submap.
    *    @param checkvars[][][0] = Valid blocker cases 1 & 3. If cv[0] and cv[1] are both false, the tile is not valid for adjacency.
    *    @param checkvars[][][1] = Valid blocker cases 2 & 3
    *    @param checkvars[][][2] = Is the tile outside? Different from the roof check, if a tile is indoors, with no valid furniture or wall etc it gets no base absorption.
    *    @param checkvars[][][3] = True if there is a roof above us.
    *    @param checkvars[][][4] = Does the tile have the permeable or reduced scent flag?
    *    @param checkvars[][][5] = Does the tile have the no scent flag?
    *    @param checkvars[][][6] = Does the tile have the blocks-wind flag?
    *    @param checkvars[][][7] = Does the tile have furniture with the no scent or blocks-wind flags?
    *   It is possible for all bits to be false, but not for all bits to be true.
    *   If a tile is out of bounds, we set all bits to true to indicate this.
    */
    std::bitset<8> checkvars[checkvars_envelope_max_x][checkvars_envelope_max_y] = { { 0 } };
    // We need to make sure that we are not attempting to check outside the submap with submap points.
    // If this returns true, we will check will bubble tripoints instead when filling out our checkvars.
    // As our total envelope is SEEX/Y + 2, our out of bounds is at X/Y == 0, X/Y == SEEX/Y + 1.
    auto outside_submap = [&]( const uint8_t &x, const uint8_t &y ) -> bool {
        return x == 0 || x == checkvars_boundary_max_x || y == 0 || y == checkvars_boundary_max_y;
    };
    // Apply this to checkvars x/y to get an equivalent submap point.
    auto cv_to_sm_ms_rel_adj = point{-1, -1};
    // Apply this to a submap point to get an equivalent checkvars point.
    auto sm_ms_to_cv_rel_adj = point{1, 1};
    // The bubble tripoint location of 0,0 in our cv envelope, which is -1,-1 from our submap 0,0.
    const tripoint_bub_ms cv_abs_trip = abs_trip + cv_to_sm_ms_rel_adj;

    for( uint8_t x = 0; x < checkvars_envelope_max_x; x++ ) {
        for( uint8_t y = 0; y < checkvars_envelope_max_y; y++ ) {
            auto &cv = checkvars[x][y];
            if( outside_submap( x, y ) ) {
                // We are not in our submap, so we have to check tripoints and queary map.
                // This is also where we can potentially grap out of bounds points.
                const auto tile = cv_abs_trip + point{x, y};
                if( !m.inbounds( tile ) ) {
                    // If we are not inbounds, set all bits to true and move on. Dont test anything else.
                    cv.set();
                    continue;
                } else {
                    const auto &tidx = lev_cache.idx( tile.x(), tile.y() );
                    const auto &ter = m.ter( tile );
                    const auto &furn = m.furn( tile );
                    // Rather common for tents, standalone curtains, and other such oddities to have one of these. These will count for adjacency purposes, so long as they are not permeable.
                    cv[7] = furn->has_flag( TFLAG_NO_SCENT ) || furn->has_flag( TFLAG_BLOCK_WIND );
                    cv[2] = m.is_outside( tile );
                    cv[3] = ( !at_max_zlev ) ? above->floor_cache[tidx] > 0 : false;
                    // There are something like 5 pieces of furniture with the reduce scent flag, and most of them are cardboard walls that are also permeable. Fine to not check that.
                    cv[4] = ter->has_flag( TFLAG_PERMEABLE ) || ter->has_flag( TFLAG_REDUCE_SCENT ) ||
                            furn->has_flag( TFLAG_PERMEABLE );
                    cv[5] = ter->has_flag( TFLAG_NO_SCENT );
                    // For whatever reason some terrain has both blocks wind and permeable, or reduce scent and block wind.
                    cv[6] = ter->has_flag( TFLAG_BLOCK_WIND );
                    // If the tile has furniture that blocks wind/scent, it cant also have valid terrain that does the same.
                    if( !cv[7] ) {
                        // Better for us to poll these once and store it then repeatedly poll map for them
                        const bool wall = ter->has_flag( TFLAG_WALL );
                        const bool connect = ter->has_flag( TFLAG_CONNECT_TO_WALL );
                        // We can now check if there is a wall, a connect to wall, a block wind, or no scent in the tile.
                        if( cv[5] || cv[6] || wall || connect ) {
                            const bool ter_road = ter->has_flag(
                                                      TFLAG_ROAD ); // This is the only real way we can distinguish open doors from closed ones as in many cases both will have blocks wind.
                            const uint8_t move_cost = ter->movecost;
                            // We only have a full wall if its not permeable, not reduce_scent, not a road, and does not have a move cost >0.
                            // The terrain in question needs to be a proper wall.
                            if( !ter_road && move_cost == 0 && !cv[4] && wall ) {
                                // Full wall absorption is case 3, cv0 && cv1
                                cv[0] = true;
                                cv[1] = true;
                            } else if( ter_road || move_cost != 0 ) {
                                // If ter road is set we most likely have an open door. If the move cost is not zero, we can walk through it
                                // If we can walk through it, it will not meaningfully limit sound propagation.
                                // Tweak this later if passable sound forcefields or something are desired.
                                // This tile counts for adjacency, but will not limit sound to simulate an open door or something.
                                cv[0] = true;
                            } else {
                                if( wall || connect ) {
                                    // Needs to be a wall or connect to wall to count as a thick barrier
                                    cv[1] = true;
                                }

                            }

                        }

                    }

                }
            } else {
                // We are inside our submap We know we are inbounds, and can poll freely.
                const point_sm_ms sm_tile = point_sm_ms{x - 1, y - 1};
                const auto tile = cv_abs_trip + point{x, y};
                const auto tidx = lev_cache.idx( tile.x(), tile.y() );
                const auto &ter = get_ter( sm_tile ).obj();
                const auto &furn = get_furn( sm_tile ).obj();
                // Rather common for tents, standalone curtains, and other such oddities to have one of these. These will count for adjacency purposes, so long as they are not permeable.
                cv[7] = furn.has_flag( TFLAG_NO_SCENT ) || furn.has_flag( TFLAG_BLOCK_WIND );
                cv[2] = m.is_outside( tile );
                cv[3] = ( !at_max_zlev ) ? ( above->floor_cache[tidx] != 0 ) : false;
                // There are something like 5 pieces of furniture with the reduce scent flag, and most of them are cardboard walls that are also permeable. Fine to not check that.
                cv[4] = ter.has_flag( TFLAG_PERMEABLE ) || ter.has_flag( TFLAG_REDUCE_SCENT ) ||
                        furn.has_flag( TFLAG_PERMEABLE );
                cv[5] = ter.has_flag( TFLAG_NO_SCENT );
                // For whatever reason some terrain has both blocks wind and permeable, or reduce scent and block wind.
                cv[6] = ter.has_flag( TFLAG_BLOCK_WIND );
                // If the tile has furniture that blocks wind/scent, it cant also have valid terrain that does the same.
                if( !cv[7] ) {
                    // Better for us to poll these once and store it then repeatedly poll map for them
                    const bool wall = ter.has_flag( TFLAG_WALL );
                    const bool connect = ter.has_flag( TFLAG_CONNECT_TO_WALL );
                    // We can now check if there is a wall, a connect to wall, a block wind, or no scent in the tile.
                    if( cv[5] || cv[6] || wall || connect ) {
                        const bool ter_road = ter.has_flag(
                                                  TFLAG_ROAD ); // This is the only real way we can distinguish open doors from closed ones as in many cases both will have blocks wind.
                        const uint8_t move_cost = ter.movecost;
                        // We only have a full wall if its not permeable, not reduce_scent, not a road, and does not have a move cost >0.
                        // The terrain in question needs to be a proper wall.
                        if( !ter_road && move_cost == 0 && !cv[4] && wall ) {
                            // Full wall absorption is case 3, cv0 && cv1
                            cv[0] = true;
                            cv[1] = true;
                        } else if( ter_road || move_cost != 0 ) {
                            // If ter road is set we most likely have an open door. If the move cost is not zero, we can walk through it
                            // If we can walk through it, it will not meaningfully limit sound propagation.
                            // Tweak this later if passable sound forcefields or something are desired.
                            // This tile counts for adjacency, but will not limit sound to simulate an open door or something.
                            cv[0] = true;
                        } else {
                            if( wall || connect ) {
                                // Needs to be a wall, connect to wall, or block wind/scent to potentially be a thick barrier.
                                cv[1] = true;
                                // We later check against permeability to see if it actually only counts as a barrier.
                            }

                        }

                    }

                }

            }
        }

    }
    std::array<point_sm_ms, 8> points_to_check;
    std::array<bool, 8> point_valid = { {false, false, false, false, false, false, false, false} };
    std::array<bool, 8> roof_cover = { {false, false, false, false, false, false, false, false} };
    std::array<bool, 8> out_of_bounds = { {false, false, false, false, false, false, false, false} };
    std::array<bool, 8> indoors = { {false, false, false, false, false, false, false, false} };

    // Dont call this at max Z, no tiles would be valid anyways.
    auto pol_adjacent = [&]( const point_sm_ms & sp ) -> void {
        // Zero and update our arrays based upon the adjacent tiles to the provided submap tile.
        point_valid = { {false, false, false, false, false, false, false, false} };
        out_of_bounds = { {false, false, false, false, false, false, false, false} };
        roof_cover = { {false, false, false, false, false, false, false, false} };
        indoors = { {false, false, false, false, false, false, false, false} };
        points_to_check = get_adjacent_submap_points( sp );
        // Our checkvars are already filled out, so the only things we might have to grab are from inbounds but out of submap tiles.

        // We are fully inside our submap, so we can take things easy and just use the checkvars.
        for( uint8_t i : san_sdir )
        {
            const auto cv_env_tile = points_to_check[i] + sm_ms_to_cv_rel_adj;
            const auto &cv = checkvars[cv_env_tile.x()][cv_env_tile.y()];
            if( cv.all() ) {
                out_of_bounds[i] = true;
                continue;
            } else {
                roof_cover[i] = cv[3];
                indoors[i] = !cv[2];
                point_valid[i] = cv[0] || cv[1] || cv[7];
            }
        }

    };

    // We have our checkvars, so lets assign the right absorption and sound wall values to each tile.
    for( const auto &sp : submap_tiles() ) {
        const tripoint_bub_ms &btri = abs_trip + sp.raw();
        // See if there is a vehicle in our given tripoint.
        // If there is, if there is a full board or a closed door, return thick barrier sound absorption.
        // We could technically run through checking adjacent tiles as we do below, but vehicles are dynamic and rechecking all of the vehicles tiles every turn would not provide enough benifit.
        if( const auto &vp = m.veh_at( btri ) ) {
            if( vp.part_with_feature( "FULL_BOARD", true ) || ( vp.obstacle_at_part() &&
                    vp.part_with_feature( "OPENABLE", true ) ) ) {
                absorption_cache[sp.x()][sp.y()] = SOUND_ABSORPTION_THICK_BARRIER;
                sound_wall_cache[sp.x()][sp.y()] = true;
                continue;
            }
        }
        const auto cv_env_tile = sp + sm_ms_to_cv_rel_adj;
        // center_check_vars
        const auto &ccv = checkvars[cv_env_tile.x()][cv_env_tile.y()];

        const bool case3 = ccv[0] && ccv[1];
        const auto &blockswind = ccv[6];
        const auto &noscent = ccv[5];
        // If we are not case 3, we need to deal with the annoying possibility
        // that windows/doors/damaged walls may be block_wind/no_scent but paradoxically have reduced scent or permeable
        const bool case2_full = ( case3 ) ? false : ( ccv[1] && !ccv[0] ) && ( ( blockswind || noscent ) &&
                                !ccv[4] );
        const bool case1 = ( case3 || case2_full ) ? false : ( ccv[0] && !ccv[1] );
        // If we have furniture that is valid, apply a barrier absorption and move on.
        // For whatever it is worth, it gets to count as a sound wall.
        if( ccv[7] ) {
            absorption_cache[sp.x()][sp.y()] += SOUND_ABSORPTION_BARRIER;
            sound_wall_cache[sp.x()][sp.y()] = !ccv[4];
            continue;
        } else if( case1 ) {
            // If we have case 1 for our terrain absorption its an open door or window equivalent. It gets no absorption.
            absorption_cache[sp.x()][sp.y()] += SOUND_ABSORPTION_OPEN_FIELD;
            continue;
        } else if( !ccv[0] && !ccv[1] ) {
            // If we are not cases 1 2 or 3, then we are an "empty" tile. Outdoors gets the default.
            // The tile may still have checkvars[5] or [6], which count for (some) sound reduction
            absorption_cache[sp.x()][sp.y()] = ( ccv[5] ||
                                                 ccv[6] ) ? ( ( ccv[2] ) ? SOUND_ABSORPTION_BARRIER + default_terrain_absorption :
                                                         SOUND_ABSORPTION_OPEN_FIELD ) : ( ( ccv[2] ) ? default_terrain_absorption :
                                                                 SOUND_ABSORPTION_OPEN_FIELD ) ;
            continue;
        }
        // Alrighty, here we go. Queary the adjacent terrain to see if it blocks sound or connects to a wall.
        // Lets build out the bool indexes.
        pol_adjacent( sp );
        // points to check and tris to check both follow this index direction scheme.
        // [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
        // [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ @ ] [ 3 ]
        // [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]

        // We have a few valid conditions. For the terrain to provide its full sound absorption, it must have at least two directly (x/y, no diagonals) adjacent wind blocking or connect_to_wall buddies,
        // And all of the adjacent valid terrain features must have an adjacent rooved/indoor tile that is also adjacent to the center tile.
        // In effect, we are looking for solid lines, or L shapes. There will be some oddities with this, if it becomes a significant issue we can look into making it more granular.
        //
        // 0 0 0                W R R    R W 0     W R W                     W R 0                         R 0 R    R 0 R
        // W W W works, as does W W W or W W 0 but 0 W 0 will not, nor would 0 W W      As a special rule, W W W or W W W and any rotation/inversion therein will not work.
        // R R R                0 0 R    0 0 0     W R W                     0 R W                         0 R 0    0 0 0
        //
        // The terrain would have to properly prevent creature movement, and if there is a straight line of walls they must have a contiguous roof.
        // Out of bounds tiles are treated as wildcards for roof cover and point validity.
        // For roof validity we want all the checked tiles to have a floor in the z level above them, or if they are indoors.
        // If all 4 adjacent cartesian tiles are valid or out of bounds, we are probably inside a solid layer and count for full absorption.

        // Does our terrain have enough buddies?
        uint8_t buddynumber = 0;
        // We check each of our adjacent terrain to see if it is properly rooved. ( 1, 3, 5, 7)
        // Could probably find a more elegant way to do this, but this is relatively quick.
        // Lets step through our direction index and add buddies or invalidate based on adjacent roofing.
        for( const uint8_t &i : sanitized_sound_direction_indexes_cartesian ) {
            if( point_valid[i] ) {
                // This gives us the direction indexes to check for a roof or out of bounds.
                // If either of the spots we want to check for a roof have valid walls present we count it.
                const auto &r_dirs = wall_sdir_invalidation[i];
                if( roof_cover[r_dirs.first] || roof_cover[r_dirs.second] || out_of_bounds[r_dirs.first] ||
                    out_of_bounds[r_dirs.second] || point_valid[r_dirs.first] || point_valid[r_dirs.second] ) {
                    buddynumber++;
                }
            }
            // Now we account for out of bounds tiles if they have an opposite wall.
            // Generally if a tile is out of bounds the tiles immediatly adjacent to it will also tend to be out of bounds
            // We only want to check walls on the cartesian directions.
            // There should never be a case where both sides of a submap tile are out of bounds.
            if( out_of_bounds[i] ) {
                // This gives us the direction indexes to check for a roof or out of bounds.
                const auto &opp_dir = opposite_tile_by_sdir[i];
                if( point_valid[opp_dir] ) {
                    buddynumber++;
                }
            }
        }
        // This is for the contiguous/corner wall check. If all roof_check_by_sdir indexes come back true from this, we count it for being a contiguous/corner wall.
        auto tile_cw_check = [&]( const uint8_t &dir ) -> bool {
            const auto &sdi = sanitized_sound_direction_indexes[dir];
            return ( indoors[sdi] || roof_cover[sdi] || out_of_bounds[sdi] || point_valid[sdi] );
        };
        auto opp_invert_corner = [&]( const uint8_t &dir ) -> bool {
            // This is explicitly to catch inverted corners inside buildings.
            const auto &opp = opposite_tile_by_sdir[sanitized_sound_direction_indexes[dir]];
            const auto &rcheck = spropagation_tiles_by_sdirection[opp];
            return ( tile_cw_check( rcheck[0] ) && tile_cw_check( rcheck[1] ) && tile_cw_check( rcheck[2] ) && tile_cw_check( rcheck[3] ) && tile_cw_check( rcheck[4] ) );
        };
        // If we get a valid result, set this to true and exit so we dont reset it accidentally.
        bool terminate_checks = false;
        // We know by this point that the center tile is either case 2 or case 3 for terrain sound absorption.
        // At one or zero buddies sound dampening is reduced.
        if( buddynumber < 2 ) {
            if( case3 ) {
                // All alone, but if they are outdoors give them some extra absorption from any little buddies like bushes or shrubs.
                absorption_cache[sp.x()][sp.y()] = ( ccv[2] ) ? default_terrain_absorption +
                                                   SOUND_ABSORPTION_THICK_BARRIER : SOUND_ABSORPTION_THICK_BARRIER;
                sound_wall_cache[sp.x()][sp.y()] = buddynumber != 0;
            } else if( case2_full ) {
                absorption_cache[sp.x()][sp.y()] = ( ccv[2] ) ? default_terrain_absorption +
                                                   SOUND_ABSORPTION_BARRIER : SOUND_ABSORPTION_BARRIER;
            }
            // Nothing for a partial case2 here.
            continue;

        } else if( buddynumber >= 3 ) {
            // We have 3 or more buddies. Case3 and case2_full valid for max value.
            // Case2_full gets wall absorption here, as it is likely surrounded by other walls.
            absorption_cache[sp.x()][sp.y()] = ( case3 ||
                                                 case2_full ) ? SOUND_ABSORPTION_WALL : SOUND_ABSORPTION_THICK_BARRIER;
            // If we have a windblocking tile, declare it a sound wall.
            sound_wall_cache[sp.x()][sp.y()] = true;
            continue;
        } else {
            // At this point we have explicitly two buddies. We need to check for straight walls and corners.
            for( const uint8_t &i : sanitized_sound_direction_indexes_cartesian ) {
                if( point_valid[i] ) {
                    // Lets run through our straight wall condition, and check for an opposite valid or OOB tile.
                    const auto &opp_dir = opposite_tile_by_sdir[i];
                    // corner wall, this will provide the indexes to check for walls when doing our corner condition.
                    const auto &c_wall = wall_check_by_sdirection[i];
                    if( point_valid[opp_dir] || out_of_bounds[opp_dir] ) {
                        // We have a valid or OOB tile on the other side.
                        // Now we need to see if we have a contiguous roof or indoors condition on either side.
                        // We also count an OOB condition or valid condition for this.
                        const auto &rc = roof_to_check_by_sdir[i];
                        const auto &opp_rc = roof_to_check_by_sdir[i];
                        const auto &roofcheck = tile_cw_check( rc[0] ) && tile_cw_check( rc[1] ) && tile_cw_check( rc[2] );
                        const auto &opp_roofcheck = tile_cw_check( opp_rc[0] ) && tile_cw_check( opp_rc[1] ) &&
                                                    tile_cw_check( opp_rc[2] );
                        if( roofcheck || opp_roofcheck ) {
                            absorption_cache[sp.x()][sp.y()] = ( case3 ) ? SOUND_ABSORPTION_WALL : ( (
                                                                   case2_full ) ? SOUND_ABSORPTION_BARRIER : SOUND_ABSORPTION_OPEN_FIELD );
                            sound_wall_cache[sp.x()][sp.y()] = case3 || case2_full;
                            terminate_checks = true;
                            break;
                        }
                    } else if( point_valid[c_wall.first] || out_of_bounds[c_wall.first] || point_valid[c_wall.second] ||
                               out_of_bounds[c_wall.second] ) {
                        // Now lets handle our corner condition.
                        const auto &diag_tile = wall_sdir_invalidation[i];
                        // now we just have to check if the tile between these two is indoors/ has a roof/ has a wall/ is OOB.
                        if( ( point_valid[c_wall.first] || out_of_bounds[c_wall.first] ) &&
                            ( tile_cw_check( diag_tile.first ) || opp_invert_corner( diag_tile.first ) ) ) {
                            absorption_cache[sp.x()][sp.y()] = ( case3 ) ? SOUND_ABSORPTION_WALL : ( (
                                                                   case2_full ) ? SOUND_ABSORPTION_BARRIER : SOUND_ABSORPTION_OPEN_FIELD );
                            sound_wall_cache[sp.x()][sp.y()] = case3 || case2_full;
                            terminate_checks = true;
                            break;
                        } else if( ( point_valid[c_wall.second] || out_of_bounds[c_wall.second] ) &&
                                   ( tile_cw_check( diag_tile.second ) || opp_invert_corner( diag_tile.second ) ) ) {
                            absorption_cache[sp.x()][sp.y()] = ( case3 ) ? SOUND_ABSORPTION_WALL : ( (
                                                                   case2_full ) ? SOUND_ABSORPTION_BARRIER : SOUND_ABSORPTION_OPEN_FIELD );
                            sound_wall_cache[sp.x()][sp.y()] = case3 || case2_full;
                            terminate_checks = true;
                            break;
                        }
                    }
                }
            }
            if( !terminate_checks ) {
                // At this point we have run through all our possible valid outcomes. This means that there are holes or other gaps.
                if( case3 ) {
                    absorption_cache[sp.x()][sp.y()] = SOUND_ABSORPTION_THICK_BARRIER;
                    sound_wall_cache[sp.x()][sp.y()] = true;
                    continue;
                } else if( case2_full ) {
                    absorption_cache[sp.x()][sp.y()] = SOUND_ABSORPTION_BARRIER;
                    sound_wall_cache[sp.x()][sp.y()] = true;
                    continue;
                }
                // Only remaining case is a case2 partial, which gets no absorption in this case.
                absorption_cache[sp.x()][sp.y()] = SOUND_ABSORPTION_OPEN_FIELD;
                continue;
            }
        }
    }
    // Mark our cache clean.
    absorption_dirty = false;
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
    auto &sound_wall_cache = map_cache.sound_wall_cache;

    if( map_cache.absorption_cache_dirty.none() ) {
        return false;
    }

    // if true, all submaps are invalid (can use batch init)
    bool rebuild_all = map_cache.absorption_cache_dirty.all();

    if( rebuild_all ) {
        // We have two general cases, sound absorption due to a barrier
        // And sound absorption due to surface effect.
        // We default to no absorption, i.e., some arbitrarily hard surface (asphault/concrete ground surfaces are effectively 0 for our purposes)
        std::fill( absorption_cache.begin(), absorption_cache.end(),
                   static_cast<short>( SOUND_ABSORPTION_OPEN_FIELD ) );
    }
    // Traverse the submaps in order
    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {

            const auto sm_pos = tripoint_bub_sm( smx, smy, zlev );
            auto *cur_submap = get_submap_at_grid( sm_pos );
            const auto sm_offset = project_to<coords::ms>( sm_pos );

            if( cur_submap == nullptr ) {
                // Null slots occur at bounded-dimension edges.
                // Treat as open air so they don't block sound propagation
                if( !rebuild_all ) {
                    for( int sx = 0; sx < SEEX; ++sx ) {
                        std::fill_n( absorption_cache.data() + map_cache.idx( sm_offset.x() + sx, sm_offset.y() ),
                                     SEEY, SOUND_ABSORPTION_OPEN_FIELD );
                    }
                }
                continue;
            }

            if( !rebuild_all && !map_cache.absorption_cache_dirty.test(
                    static_cast<size_t>( map_cache.bidx( smx, smy ) ) ) ) {
                continue;
            }
            cur_submap->absorption_dirty = true;
            cur_submap->rebuild_absorption_cache( *this, tripoint_bub_sm( smx, smy, zlev ) );



            if( cur_submap->is_uniform ) {
                const short value = cur_submap->absorption_cache[0][0];
                // if rebuild_all==true all values were already set to 0
                if( !rebuild_all || value != SOUND_ABSORPTION_OPEN_FIELD ) {
                    for( int sx = 0; sx < SEEX; ++sx ) {
                        std::fill_n( absorption_cache.data() + map_cache.idx( sm_offset.x() + sx, sm_offset.y() ),
                                     SEEY, value );
                    }
                }
            } else {
                for( int sx = 0; sx < SEEX; ++sx ) {
                    const int x = sx + sm_offset.x();
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        const int y = sy + sm_offset.y();
                        absorption_cache[map_cache.idx( x, y )] = cur_submap->absorption_cache[sx][sy];
                        //if( absorption_cache[map_cache.idx( x, y )] >= SOUND_ABSORPTION_THICK_BARRIER ) {
                        //    map_cache.sound_wall_cache[map_cache.idx( x, y )] = true;
                        //}
                    }
                }
            }
            for( const auto sm_ms : submap_tiles() ) {
                const auto ms_pos = project_combine( sm_pos, sm_ms );
                sound_wall_cache[static_cast<size_t>( map_cache.idx( ms_pos.x(), ms_pos.y() ) )] =
                    cur_submap->sound_wall_cache[sm_ms.x()][sm_ms.y()];
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
//static std::vector<std::pair<tripoint_bub_ms, sound_event>> recent_sounds;

// The sound events since the last interactive player turn. (doesn't count sleep etc)
// Depreciated, kept as comment for reference
//static std::vector<std::pair<tripoint_bub_ms, sound_event>> sounds_since_last_turn;

// The sound events currently displayed to the player.
static std::unordered_map<tripoint_bub_ms, sound_event> sound_markers;

void sounds::sound( const sound_event &soundevent )
{
    // Worker-safe mapgen (e.g. a building collapse bash during background OMT
    // generation) can reach this, but the sound system mutates the global player
    // map's sound cache, which is not thread-safe and would be meaningless for an
    // area still being generated. Drop sounds raised off the main thread.
    if( is_pool_worker_thread() ) {
        return;
    }
    // Make a copy so that when our referenced sound event inevitably goes out of scope things dont explode.
    auto temp_se = soundevent;
    // Make sure our sound came from a valid inbounds location.
    auto &map = get_map();
    // Make sure that we dont absolutely blast somebodies RAM/Processor during batch floodfill.
    // 19100 attempted sound events in one turn is plenty.
    if( map.m_sound_cache.sounds_this_turn < 1000 ) {
        map.m_sound_cache.sounds_this_turn++;
    } else {
        add_msg( m_debug,
                 _( "Maximum number of attempted floodfills in a turn reached!" ) );
        map.m_sound_cache.sounds_this_turn = 0;
        add_msg( m_debug,
                 _( "Sound instances vector contained %i sounds before emergency flush." ),
                 map.m_sound_cache.sound_instances.size() );
        map.m_sound_cache.sound_instances.clear();

    }
    if( !map.inbounds( temp_se.origin ) ) {
        debugmsg( "Sound with description [ %1s ] attempted to propagate from out of bounds location %i:%i:%i!",
                  temp_se.description, temp_se.origin.x(),
                  temp_se.origin.y(), temp_se.origin.z() );
        return;
    }


    // Error out if volume is negative, or bail out if volume is less than 16dB.
    // There are not anechoic chambers in game, so actually hearing such sounds after even 1 tile of distance (16dB -> 10dB 1 tile away) is very unlikely for the vast majority of creatures.
    // A good threshold for sounds that should only really be faintly audible to the player in a quiet room is 20dB.
    // Most sounds intended to be quiet but still audible to the player, and maybe to creatures very close, is 35-45dB.
    // Ambient volume minimum is usually between 35 and 55dB in game. A player with normal hearing can notice sounds 20dB below ambient.

    if( temp_se.volume < 16 ) {

        add_msg( m_debug,
                 _( "Sound with description [ %1s ] at %i:%i with a volume %i too quiet for propagation." ),
                 temp_se.description, temp_se.origin.x(), temp_se.origin.y(),
                 temp_se.volume );

        return;
    }
    // Reduction in size of the flood filled area and filtered list for monster checking allows us to have more permissive flood fill restrictions.
    //else if( temp_sound_event.volume < ( ( temp_sound_event.origin.z() < 0 ) ? mdBspl_to_dBspl(
    //    AMBIENT_VOLUME_UNDERGROUND ) : mdBspl_to_dBspl( AMBIENT_VOLUME_ABOVEGROUND ) +
    //                                   get_weather().weather_id->sound_attn ) - 19 ) {
    // Dont propagate sounds that are too quiet to be heard.
    //return;
    //}
    else if( temp_se.volume > mdBspl_to_dBspl( MAXIMUM_VOLUME_ATMOSPHERE ) ) {
        add_msg( m_debug,
                 _( "Sound with description [ %1s ] at %i:%i attempted to propagate with a volume of %i dB which is louder than possible in atmosphere!" ),
                 temp_se.description, temp_se.origin.x(), temp_se.origin.y(),
                 temp_se.volume );
    }
    // Description is not an optional parameter
    if( temp_se.description.empty() ) {
        debugmsg( "Sound at %i:%i has no description!", temp_se.origin.x(),
                  temp_se.origin.y() );
        return;
    }
    // Check to see if more than one source has been set somehow.
    // More than one source entity will break alot of logic downstream.
    if( ( temp_se.from_monster + temp_se.from_npc + temp_se.from_player ) >
        1 ) {
        debugmsg( "Sound at %i:%i has too many source entity types!", temp_se.origin.x(),
                  temp_se.origin.y() );
        return;
    }
    // Maximum possible sound pressure level in atmosphere is 191 dB, cap our volume for sanity.
    // Sound volumes above 191dB are not sound pressure waves, they are supersonic blast/shock waves and should be modeled as damaging explosions.
    // Check above should catch any volumes that are too low or negative.
    temp_se.volume = std::min( temp_se.volume,
                               mdBspl_to_dBspl( MAXIMUM_VOLUME_ATMOSPHERE ) );
    if( temp_se.volume >= 120 ) {
        map.m_sound_cache.attempted_potential_deafening_sounds++;
    }
    if( temp_se.movement_noise ) {
        map.m_sound_cache.attempted_movement_sounds++;
    }

    // We flood fill sounds from monsters and NPCs for performance and efficiency reasons.
    // Along with any environmental sound with sufficient volume to be a horde signal.
    if( temp_se.from_monster || temp_se.from_npc || ( !temp_se.from_player &&
            temp_se.volume >= 120 ) ) {
        if( temp_se.from_monster ) {
            map.m_sound_cache.attempted_monster_sounds++;
        } else if( temp_se.from_npc ) {
            map.m_sound_cache.attempted_NPC_sounds++;
        }

        sound_batch_floodfill_que.push_back( temp_se );

    } else {
        map.flood_fill_sound( temp_se, temp_se.origin.z() );
        map.m_sound_cache.attempted_non_batch_floodfills++;
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
    auto &sound_cache = map.m_sound_cache;

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

    auto &sound_instance_caches = sound_cache.sound_instances;
    auto &filtered_sounds = sound_cache.sound_list_filtered;

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
    auto ambient = [&]( const int &zlev, const bool &indoors = false ) {
        if( zlev < 0 ) {
            return AMBIENT_VOLUME_UNDERGROUND;
        } else if( indoors ) {
            return INDOOR_AMBIENT;
        } else {
            return OUTDOOR_AMBIENT;
        }
    };

    // Store our ground level ambient in dB for the horde signal check incase there are alot of sounds.
    const short ground_ambient_vol = mdBspl_to_dBspl( OUTDOOR_AMBIENT );
    for( auto &sound : sound_instance_caches ) {

        // Mark all our sound_instance_caches as heard by monsters, easier to do here than reiterate later.
        sound.heard_by_monsters = true;
        // Sounds louder than 110dB are potentially valid for horde signal.
        const auto &s_origin = sound.sound.origin;
        // We assume that we dont have a direct line of sight so we actually check for floors and walls.
        const auto surface_origin = tripoint_bub_ms( s_origin.x(), s_origin.y(), 0 );
        const auto alt_adjust = vol_z_adjust( s_origin, surface_origin, false, true );
        if( ( ( sound.sound.volume ) - alt_adjust ) >= 110 ) {
            const auto abs_omt = project_to<coords::omt>( bub_to_abs( s_origin ) );
            const short default_terrain_absorption = terrain_sound_attenuation( abs_omt, season, true );

            const int sig_power = get_signal_for_hordes( sound.sound, ground_ambient_vol,
                                  default_terrain_absorption,
                                  alt_adjust );
            if( sig_power > 0 ) {
                get_overmapbuffer( get_avatar().get_dimension() ).signal_hordes( project_to<coords::sm>( bub_to_abs(
                            s_origin ) ), sig_power );

            }
        }
    }
    // Make sure this is empty before we get going. If its not, it is pretty much garunteed that all the entries will have jumbled iterators.
    if( !sound_cache.sound_list_filtered.empty() ) {
        sound_cache.filtered_sound_lists_cleared += sound_cache.sound_list_filtered.size();
        sound_cache.sound_list_filtered.clear();
    }

    auto make_new_filtered_list = [&]( const sound_filter_key & key ) {
        std::vector<short> filtered_list;
        // If we have more sounds in the vector than a short can point to we have bigger problems than a bad memory access.
        const short length = static_cast<short>( sound_instance_caches.size() );
        for( short i = 0; i < length; i++ ) {
            const auto &sound = sound_instance_caches[i];

            if( sound.sound.category <= key.category || ( key.ignore_movement && sound.movement_noise ) ) {
                continue;
            }

            // We generally want to ignore the movement noises from our own faction, to prevent zombies from all hearing their own footsteps.
            // And to prevent monsters that are afraid of noise from being locked in an eternal fear loop by their allies footsteps.
            if( key.monfaction == sound.sound.monfaction && sound.movement_noise ) {
                continue;
            }
            // Handle our most likely case, horde monsters, first.
            // Horde monsters dont get to be afraid of sounds, and *should* be drawn to any kind of noise.
            // We make an exception here to reduce the amount of sounds we will need to process, especially for standard zombies.
            // If a sound is below a category threshold or origin volume threshold and is from our faction/a faction we are neutral to, we ignore it.
            // So hulk roars will draw nearby zombies, but random zombie jibbering or flailing at a fence post wont.
            if( key.horde_monster ) {
                if( key.monfaction == sound.sound.monfaction ||
                    key.monfaction->attitude( sound.sound.monfaction ) == MFA_FRIENDLY ||
                    key.monfaction->attitude( sound.sound.monfaction ) == MFA_NEUTRAL ) {
                    if( sound.sound.category < sound_t::alarm && sound.sound.volume < 90 ) {
                        continue;
                    }
                }
            } else if( ( key.noise_angers || key.noise_fear ) && ( key.monfaction == sound.sound.monfaction ||
                       key.monfaction->attitude( sound.sound.monfaction ) == MFA_FRIENDLY ) &&
                       sound.sound.category < sound_t::destructive_activity ) {
                // If we are afraid of noises or angered by noises, make sure we dont trigger on our faction or friendly faction noises below a category threshold.
                continue;
            }

            // If we pass all the filter checks, return our list.
            filtered_list.push_back( i );
        }
        const sound_filter_key key_2_electric_boogalo = key;
        const std::vector<short> filt_list_2_electric_boogalo = filtered_list;
        filtered_sounds.insert( {key_2_electric_boogalo, filt_list_2_electric_boogalo} );
        sound_cache.filtered_sound_lists_made++;
    };

    // Lets run through all the monsters and feed them sound info.
    // Monsters just go to the loudest thing they hear, so we run through that here.
    // Monsters ignore movement sounds from their own faction, a bit omiscient but it simplifies things.
    for( monster &critter : g->all_monsters() ) {

        // Monster is deaf, skip. We also skip hallucinations and monsters not loaded in.
        if( !critter.can_hear() || critter.is_hallucination() || !map.inbounds( critter.bub_pos() ) ) {
            continue;
        }
        sound_filter_key filter_key;
        filter_key.monfaction = critter.faction.id();
        const auto &critterfact = filter_key.monfaction;

        // Is our monster afraid of sounds and nearby enemies? If so, we will use slightly expanded logic.
        // The vast majority of "dumb" monsters do not have fear triggers for sound or nearby enemies.
        filter_key.noise_fear = critter.type->has_fear_trigger( mon_trigger::SOUND );
        filter_key.noise_angers = critter.type->has_anger_trigger( mon_trigger::SOUND );
        const auto &fears_sounds = filter_key.noise_fear;
        const bool fears_enemy_sounds = fears_sounds &&
                                        ( critter.type->has_fear_trigger( mon_trigger::HOSTILE_CLOSE ) );

        // If our monster is a player pet, they use expanded logic.
        const bool player_ally = critter.friendly != 0;
        filter_key.horde_monster = !player_ally && !fears_enemy_sounds && filter_key.noise_angers;
        const auto &horde_monster = filter_key.horde_monster;

        const auto &critterloc = critter.bub_pos();

        // Check to see if there is a matching filtered list, or if our list list empty.
        if( !filtered_sounds.contains( filter_key ) || filtered_sounds.empty() ) {
            make_new_filtered_list( filter_key );
        }
        // We should have a fancy new filtered list by now. If we still dont, skip this monster because something funky happened.
        if( !filtered_sounds.contains( filter_key ) ) {
            add_msg( m_debug,
                     _( "Monster %1s at %i:%i:%i attempted to make a filtered sound list but could not find it after generation!" ),
                     critter.disp_name(), critterloc.x(), critterloc.y(), critterloc.z() );
            continue;
        }
        // Lets get our shiny list.
        const auto &s_list = sound_cache.sound_list_filtered.at( filter_key );

        // Make sure our sound list is not empty, skip if it is.
        // Entirely possible for some creatures to not have any valid sounds in the list.
        if( s_list.empty() ) {
            continue;
        }

        // We only need to store the sound_instance_caches iterator value of our loudest sound.
        // We set this initially to -1 so we know if we did not hear anything.
        short loudest_sound_iter = -1;
        short loudest_vol = 0;
        const bool goodhearing = critter.has_flag( MF_GOODHEARING );


        // Grab the ambient volume at the critter in dB spl
        const short critter_ambient_vol = ( ambient( critterloc.z(), !map.is_outside( critterloc ) ) );

        // Working in mdB when dealing with tile volumes.
        // Make sure our volume threshold is not nonsensically low. Underground monsters are especially prone to getting too or below the volume threshold.
        // This results in them technically hearing all sounds on the map, which is not ideal. Volume threshold is set to be atleast 10dB for sanity and performance reasons.
        const short critter_vol_threshold = std::max( ( critter_ambient_vol -
                                            ( goodhearing ? 2000 : 1000 ) ), 1000 );

        const auto &level_cache = map.get_cache_ref( critterloc.z() );
        const short critter_t_absorb = level_cache.absorption_cache[level_cache.idx( critterloc.x(),
                                                        critterloc.y() )];
        const bool critter_indoors = !level_cache.outside_cache[level_cache.idx( critterloc.x(),
                                                      critterloc.y() )];

        for( auto &iter : s_list ) {
            const auto &sound = sound_instance_caches[iter];
            const auto &s_category = sound.sound.category;
            // Do a quick check to see if we even have a hope of hearing the sound.
            if( critter_vol_threshold >= SOUND_MINIMUM_VOLUME_FOR_PROPAGATION && !goodhearing &&
                rl_dist( critterloc, sound.origin ) > sound.approximate_minvol_distance ) {
                continue;
            }
            // Alot of our simpler sorting should already be done by this point.
            // What we have left to do is checking distance, volume, and some specific stuff on monsters that are the players pets.

            const bool lineofsight = critter.sees( sound.origin );
            // We grab either the average of our terrain absorption, or zero if both are zero.
            const short avg_t_absorp = ( sound.terrain_sound_absorbtion_at_source == 0 &&
                                         critter_t_absorb == 0 ) ? 0 : std::round( ( sound.terrain_sound_absorbtion_at_source +
                                                 critter_t_absorb ) / 2 );
            // Which we already have a handler for.
            const short heard_vol = svol_at( sound, critterloc, avg_t_absorp, critter_indoors, lineofsight );


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
                            if( !fears_enemy_sounds && ( s_category == sounds::sound_t::combat ||
                                                         s_category == sounds::sound_t::alarm ) ) {

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
                    loudest_sound_iter = iter;
                    continue;
                }

            }
        }
        // If we heard no sound, dont try to queary the vector.
        if( loudest_sound_iter < 0 ) {
            continue;
        }
        critter.hear_sound( sound_instance_caches[loudest_sound_iter].sound, loudest_vol,
                            critter_ambient_vol, false, fears_sounds );
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
    auto &map = get_map();
    auto &sound_vector = map.m_sound_cache.sound_instances;
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

    // Now we work through all of our active NPCs.
    for( npc &who : g->all_npcs() ) {
        // We only want NPCs who are simulated and inbounds to hear noises.
        if( who.is_simulated() && map.inbounds( who.bub_pos() ) ) {
            bool is_deaf = who.is_deaf();
            const auto &loc = who.bub_pos();
            const auto &level_cache = map.get_cache_ref( loc.z() );
            const float volume_multiplier = who.hearing_ability();
            // Deafening is based on the loudest volume at that tile.
            // A deaf npc might not "hear" the deafening sound but still suffer additional hearing loss.
            // The average pain threshold is generally taken as 120dB.
            // The maximum threshold for pain and garunteed instant hearing loss is generally taken as 140dB spl
            // The NIOSH daily safe exposure for 115dB sounds is ~28 seconds, 120dB sounds have a daily safe exposure of less than 2 seconds.
            // Deafening in game is temporary, and effectively simulates the ear being unable to distinguish any sound due to spontaneous damage.
            // Threshold for instant hearing loss is 12000mdB
            // Volume for garunteed deafening is 14000mdB
            const short deafening_threshold = std::max( 0.0f,
                                              std::floor( 12000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;
            const short deafening_garuntee = std::max( 0.0f,
                                             std::floor( 14000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;
            // How far below ambient can this character hear? Default of 20dB, caps out at 30dB below ambient for sanity.
            // The player character gets a better calc, but these are NPCs and we dont love them enough.
            const short below_ambient = std::min( 3000.0f,
                                                  ( std::floor( 1500 + 500 * volume_multiplier ) ) );

            const auto charx = loc.x();
            const auto chary = loc.y();
            const auto npc_indoors = !level_cache.outside_cache[level_cache.idx( charx, chary )];
            const auto ambient_vol = ambient( loc.z(), npc_indoors );
            // Passive sound dampening reduces all heard volume by a set amount, but protects against hearing loss by 2x this amount.
            const short passive_sound_dampening = dBspl_to_mdBspl( who.get_char_hearing_protection() );
            // Active dampening does not reduce heard volume and directly protects against hearing loss.
            const short active_sound_dampening = dBspl_to_mdBspl( who.get_char_hearing_protection( true ) );
            // Sounds quieter than this are inaudible and are skipped.
            // Passive sound dampening reduces the "heard" volume of all sounds, including ambient volume.
            // In a perfect simulation most hearing protection absorbs high frequency sounds much more than low frequency sounds.
            // We cap our minimum at 10dB to prevent underground NPCs from hearing everything everywhere on the entire map.
            const short min_vol = std::max( 1000, ( ambient_vol - below_ambient + passive_sound_dampening ) );
            const short npc_t_absorb = level_cache.absorption_cache[level_cache.idx( charx, chary )];

            // dBspl is a root-mean-square value so while all the volumes in the tile should be cumulative,
            // proper tile volume would follow the formula sqrt((1/n)*(v1^2 + v2^2+ ... + vn^2)) where n is the number of volumes.
            // In general practice unless there are only 20+ copies of the same sound in a tile the volume is dominated by the loudest sound volume.
            // 100dB + 20dB + 80dB +70dB = ~101dB  So we just take the loudest.
            for( auto &element : sound_vector ) {
                //Skip sounds that NPCs have already heard, if for whatever reason they manage to hear such a sound.
                if( element.heard_by_npcs ) {
                    continue;
                }
                const auto &average_t_absorp = ( npc_t_absorb == 0 &&
                                                 element.terrain_sound_absorbtion_at_source == 0 ) ? 0 : std::round( ( npc_t_absorb +
                                                         element.terrain_sound_absorbtion_at_source ) / 2 );
                // Do an early filter for sounds that would always be indaudible.
                // Check to see if the NPC is deaf here as well, as we may deafen them part way through the process.
                const auto tile_vol = svol_at( element, who.bub_pos(), average_t_absorp,
                                               npc_indoors, who.sees( element.origin ) );

                if( tile_vol <= min_vol ) {
                    continue;
                }

                if( tile_vol  > min_vol && !who.is_deaf() ) {

                    // We only want to feed NPC AI sounds they should react to.
                    // This is more than a bit hackey and gives the NPCs a bit of omniscience,
                    // but we dont want NPCs going out to investigate every single sound under the sun.
                    if( ( element.from_player || element.from_monster ||
                          element.from_npc ) ) {

                        who.handle_sound( ( tile_vol - passive_sound_dampening ), element.sound );
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
    // Set our current sound caches to heard_by_npcs instead of doing it repeatedly with each NPC.
    for( auto &element : sound_vector ) {
        element.heard_by_npcs = true;
    }
}
// Process sounds for the player character. Once upon a time this was also used for NPCs.
void sounds::process_sound_markers( Character *who )
{

    bool is_deaf = who->is_deaf();
    const float volume_multiplier = who->hearing_ability();
    const auto &loc = who->bub_pos();
    auto &map = get_map();
    const auto &level_cache = map.get_cache_ref( loc.z() );
    auto &sound_vector = map.m_sound_cache.sound_instances;
    // We want constant ints for our x/y, makes the compiler happier when getting cache[x][y].
    // How far below ambient can this character hear? Default of 20dB, uncapped unlike NPCs.
    const short below_ambient = std::min( 19100,
                                          static_cast<int>( std::floor( 1500 + 500 * volume_multiplier ) ) );
    // is the npc underground?
    const bool pcunderground = loc.z() < 0;
    const bool pcoutdoors = map.is_outside( loc.xy() );
    const weather_manager &weather = get_weather();
    const short player_t_absorp = level_cache.absorption_cache[level_cache.idx( loc.x(), loc.y() )];
    const bool  player_indoors = !level_cache.outside_cache[level_cache.idx( loc.x(), loc.y() )];

    // Ambient underground is 20dB, ambient in a above ground building is 40. The assumption is that there are zombies making noise, and its not perfectly dead quiet.
    // Weather sound attenuation ranges from 0 - 8. We add this to existing ambient if applicable to approximate the sound of rain, snow, etc.
    const short weather_vol = dBspl_to_mdBspl( ( pcunderground &&
                              !pcoutdoors ) ? 0 : ( !pcunderground &&
                                      !pcoutdoors ) ? 2 * ( weather.weather_id->sound_attn ) : weather.weather_id->sound_attn );

    // Wind volume should be somewhere VAUGELY around 40dB+mph in reality, however Cata frequently simulates absolutely batshit insane steady windspeeds.
    const short wind_volume = dBspl_to_mdBspl( ( pcunderground ||
                              !pcoutdoors ) ? 0 : weather.windspeed );

    const short ambient_vol = wind_volume + weather_vol + ( (
                                  pcunderground ) ? AMBIENT_VOLUME_UNDERGROUND : AMBIENT_VOLUME_ABOVEGROUND );

    const short passive_sound_dampening = dBspl_to_mdBspl( who->get_char_hearing_protection() );

    const short active_sound_dampening = dBspl_to_mdBspl( who->get_char_hearing_protection( true ) );

    who->volume = std::max( static_cast<int>( mdBspl_to_dBspl( ambient_vol ) ), who->volume );

    // Deafening is based on the loudest volume at that tile.
    // A deaf player might not "hear" the deafening sound but still suffer additional hearing loss.
    // The average pain threshold is generally taken as 120dB.
    // The maximum threshold for pain and garunteed instant hearing loss is generally taken as 140dB spl
    // The NIOSH daily safe exposure for 115dB sounds is ~28 seconds, 120dB sounds have a daily safe exposure of less than 2 seconds.
    // Deafening in game is temporary, and effectively simulates the ear being unable to distinguish any sound due to spontaneous damage.
    // Threshold for instant hearing loss is 12000mdB
    // Volume for garunteed deafening is 14000mdB
    const short deafening_threshold = std::max( 0.0f,
                                      std::floor( 12000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;
    const short deafening_garuntee = std::max( 0.0f,
                                     std::floor( 14000 - ( 200 * ( volume_multiplier - 1 ) ) ) ) ;

    // Lets figure out our loudest volume in tile, and a few other bits of diagnostic information.
    short loudest_vol = 0;
    short num_sounds_checked = 0;
    short num_sounds_in_minvol_dist = 0;
    short num_sound_in_envelope = 0;
    // We can make a copy of the much smaller sound event rather than the whole sound instance, and just record bits of relevant info.
    // We want these so we can figure out if anything is wrong with hearing sounds / terrain interaction.
    sound_event loudest_sound_dummy;
    int loudest_sound_minvol_radius = 0;
    uint8_t loudest_sound_flood_radius = 0;
    short loudest_sound_escape_vol = 0;
    uint8_t loudest_sound_escape_dir = SDI_UP;

    // Cant hear noises that are significantly quieter than the loudest noise you are currently hearing,
    // Softer sounds just get drowned out in loud areas.
    const short vol_threshold = std::max( 0,
                                          ( ( passive_sound_dampening + ambient_vol ) - below_ambient ) );

    for( auto &element : sound_vector ) {
        // Skip sounds the player has already heard.
        if( element.heard_by_player ) {
            continue;
        }
        num_sounds_checked++;
        const int distance_to_sound = rl_dist( loc, element.origin );
        if( element.approximate_minvol_distance >= distance_to_sound ) {
            num_sounds_in_minvol_dist++;
        }
        if( element.in_envelope( loc.xy() ) ) {
            num_sound_in_envelope++;
        }
        // And set the sound as having been heard by the player, before we potentially skip it for volume reasons.
        element.heard_by_player = true;
        if( element.sound.volume >= mdBspl_to_dBspl( MAXIMUM_VOLUME_ATMOSPHERE ) ) {
            // Dont count impossibly loud sounds.
            add_msg( m_debug,
                     _( "Player given sound louder than possible in Atmosphere! Sound with description [ %1s ] from %i:%i:%i with an origin volume of %i dB is louder than possible." ),
                     element.sound.description, element.sound.origin.x(), element.sound.origin.y(),
                     element.sound.origin.z(),
                     element.sound.volume );
            continue;
        }
        const short average_t_absorp = ( player_t_absorp == 0 &&
                                         element.terrain_sound_absorbtion_at_source == 0 ) ? 0 : static_cast<short>( std::round( ( (
                                                 player_t_absorp +
                                                 element.terrain_sound_absorbtion_at_source ) * 0.5 ) ) );
        const short tile_vol = svol_at( element, loc, average_t_absorp, player_indoors,
                                        who->sees( element.origin ) );

        // Set our dummy sound event. This is just the sound event (description, origin, etc), not the full flooded sound instance.
        // Also set a few relevant bits of info instead of recording the whole
        if( tile_vol > loudest_vol ) {
            loudest_sound_dummy = element.sound;
            loudest_sound_minvol_radius = element.approximate_minvol_distance;
            loudest_sound_flood_radius = element.flood_radius;
            loudest_sound_escape_dir = sounds::direction_index_to_sound_source( loc, element.sound.origin );
            loudest_sound_escape_vol = element.base_distance_vol_by_dir[get_sound_direction_index(
                                           loudest_sound_escape_dir )];
        }
        loudest_vol = std::max( loudest_vol, tile_vol );

        if( tile_vol >= MAXIMUM_VOLUME_ATMOSPHERE || tile_vol > dBspl_to_mdBspl( element.sound.volume ) ) {
            // Dont count impossibly loud sounds.
            debugmsg( "Player given impossibly loud sound! Sound with description [ %1s ] from %i:%i:%i with an origin volume of %i dB, tile volume of %i mdB, distance %i at %i:%i:%i is louder than possible.",
                      element.sound.description, element.sound.origin.x(), element.sound.origin.y(),
                      element.sound.origin.z(), element.sound.volume, tile_vol, distance_to_sound, loc.x(), loc.y(),
                      loc.z() );
            continue;
        }
        // If the sound is loud enough, inform the player of it.
        if( tile_vol > vol_threshold ) {

            // Deafening is based on the felt volume, as a player may be too deaf to
            // hear the deafening sound but still suffer additional hearing loss.
            // Is the loudest tile volume louder than the deafening threshold?
            // Passive sound dampening counts 2x for protecting against hearing loss compared to is normal volume adjustment to approximate hearing protection working more effectively against harmful high frequency sounds.
            const short deafening_vol = std::max( 0,
                                                  tile_vol - ( active_sound_dampening + passive_sound_dampening + passive_sound_dampening ) );
            const bool is_sound_deafening =  deafening_vol >= rng( deafening_threshold, deafening_garuntee );
            if( is_sound_deafening ) {

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
            if( is_deaf ) {
                continue;
            }
            // Secure the flag before wake_up() clears the effect
            bool slept_through = who->has_effect( effect_slept_through_alarm );
            // Grab the decibel value of our adjusted vol for use with comparisons etc.
            const int db_vol = mdBspl_to_dBspl( tile_vol - passive_sound_dampening );
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
                ( element.sound.origin != who->bub_pos() ) &&
                !get_map().pl_sees( element.sound.origin, distance_to_sound ) ) {
                if( !who->activity->is_distraction_ignored( distraction_type::noise ) &&
                    !get_safemode().is_sound_safe( element.sound.description, distance_to_sound ) ) {
                    const std::string final_description = ensure_punctuation( description, '!' );
                    const std::string query = string_format( _( "Heard %s!" ), final_description );
                    g->cancel_activity_or_ignore_query( distraction_type::noise, query );
                }
            }

            // skip some sounds to avoid message spam
            if( describe_sound( element.sound.category, element.sound.origin == who->bub_pos() ) ) {
                game_message_type severity = m_info;
                if( element.sound.category == sound_t::combat || element.sound.category == sound_t::alarm ) {
                    severity = m_warning;
                }

                std::string final_description = ensure_punctuation( description, '.' );

                // if we can see it, don't print a direction
                if( element.sound.origin == who->bub_pos() ) {
                    add_msg( severity, _( "From your position you hear %1$s" ), final_description );
                } else if( who->sees( element.sound.origin ) ) {
                    add_msg( severity, _( "You hear %1$s" ), final_description );
                } else {
                    std::string direction = direction_name( direction_from( who->bub_pos(), element.sound.origin ) );
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
            // TODO: Fix the sfx volumes in here to simply take a ratio of the sounds original volume to its volume at the player
            // so that we avoid double calcing and properly account for walls.
            const std::string &sfx_id = element.sound.id;
            const std::string &sfx_variant = element.sound.variant;
            if( !sfx_id.empty() ) {
                sfx::play_variant_sound( sfx_id, sfx_variant, sfx::get_heard_volume( element.sound.origin,
                                         element.sound.volume ) );
            }

            // Place footstep markers.
            if( element.sound.origin == who->bub_pos() || who->sees( element.sound.origin ) ) {
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
            const bool diff_z = element.sound.origin.z() != who->bub_pos().z();

            // Enumerate the valid points the player *cannot* see.
            // Unless the source is on a different z-level, then any point is fine
            std::vector<tripoint_bub_ms> unseen_points;
            for( const tripoint_bub_ms &newp : get_map().points_in_radius( element.sound.origin,
                    err_offset ) ) {
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
    // Volume gets reset to zero later in the turn order after the player has acted.
    // Keep the volume at the maximum to prevent the volume in the UI from being repeatedly reset to the ambient volume whenever the UI is refreshed.
    who->volume = std::max( static_cast<int>( mdBspl_to_dBspl( loudest_vol ) ), who->volume );

    // Do our diagnostic blurb.
    // We can make a copy of the much smaller sound event rather than the whole sound instance, and just record bits of relevant info.
    // We want these so we can figure out if anything is wrong with hearing sounds / terrain interaction.
    add_msg( m_debug,
             _( "Avatar sound processing diagnostic: Checked:%i, Within minvol distance:%i, Within flood envelope:%i, Ambient Vol:%i mdB, Vol Threshold:%i mdB" ),
             num_sounds_checked, num_sounds_in_minvol_dist, num_sound_in_envelope, ambient_vol, vol_threshold );
    if( loudest_vol > 0 ) {
        add_msg( m_debug,
                 _( "Loudest Sound: Description:[%1s], Origin vol:%i dB at [%i:%i:%i], Minvol distance:%i, Floodfill radius:%i" ),
                 loudest_sound_dummy.description, loudest_sound_dummy.volume, loudest_sound_dummy.origin.x(),
                 loudest_sound_dummy.origin.y(), loudest_sound_dummy.origin.z(), loudest_sound_minvol_radius,
                 loudest_sound_flood_radius );
        add_msg( m_debug,
                 _( "Heard vol:%i mdB at [%i:%i:%i], Distance:%i, SDI from sound to Avatar:%i, with envelope escape vol in that SDI:%i mdB" ),
                 loudest_vol, loc.x(), loc.y(), loc.z(), rl_dist( loc, loudest_sound_dummy.origin ),
                 loudest_sound_escape_dir, loudest_sound_escape_vol );
    }

}
// Use map::cull_sound_instance_caches for managing the sound_instance_caches vector during play.
void sounds::reset_sounds()
{
    auto &map = get_map();
    map.m_sound_cache.sound_instances.clear();
    sound_markers.clear();
    map.m_sound_cache.sound_list_filtered.clear();
    sound_batch_floodfill_que.clear();
}

auto sounds::shift_sound_positions( const point_rel_ms &offset ) -> void
{
    if( offset == point_rel_ms::zero() ) {
        return;
    }

    const auto sound_bounds = half_open_rectangle<point_bub_ms>(
                                  point_bub_ms::zero(), point_bub_ms( g_mapsize_x, g_mapsize_y ) );
    auto &sound_cache = get_map().m_sound_cache;

    for( auto &sound : sound_cache.sound_instances ) {
        sound.origin = sound.origin + offset;
        sound.sound.origin = sound.sound.origin + offset;
        sound.envelope_index_point = sound.envelope_index_point + offset;
        sound.offset_x += offset.x();
        sound.offset_y += offset.y();
    }
    std::erase_if( sound_cache.sound_instances, [&]( const auto & sound ) {
        return !sound_bounds.contains( sound.origin.xy() );
    } );
    sound_cache.sound_list_filtered.clear();

    for( auto &sound : sound_batch_floodfill_que ) {
        sound.origin = sound.origin + offset;
    }
    std::erase_if( sound_batch_floodfill_que, [&]( const auto & sound ) {
        return !sound_bounds.contains( sound.origin.xy() );
    } );

    auto shifted_markers = std::unordered_map<tripoint_bub_ms, sound_event>();
    for( auto &marker : sound_markers ) {
        const auto shifted_marker_pos = marker.first + offset;
        if( sound_bounds.contains( shifted_marker_pos.xy() ) ) {
            marker.second.origin = marker.second.origin + offset;
            shifted_markers.emplace( shifted_marker_pos, std::move( marker.second ) );
        }
    }
    sound_markers = std::move( shifted_markers );
}

void sounds::clear_floodfill_que( const bool &soundperf )
{
    auto &map = get_map();

    if( !soundperf ) {
        add_msg( m_debug,
                 _( "Attempted to floodfill %i total sounds. Flooding attempted on %i deafening, %i movement, %i from NPCs, %i from monsters, %i non-batch floods." ),
                 map.m_sound_cache.sounds_this_turn, map.m_sound_cache.attempted_potential_deafening_sounds,
                 map.m_sound_cache.attempted_movement_sounds, map.m_sound_cache.attempted_NPC_sounds,
                 map.m_sound_cache.attempted_monster_sounds, map.m_sound_cache.attempted_non_batch_floodfills );
        add_msg( m_debug,
                 _( "Batch flooded %i sounds from monsters and %i sounds from NPCs. %i sounds invalidated during batch flooding." ),
                 map.m_sound_cache.batch_flooded_monster_sounds, map.m_sound_cache.batch_flooded_NPC_sounds,
                 map.m_sound_cache.invalidated_batch_sounds );
        add_msg( m_debug,
                 _( "Caught %i sounds still in floodfill que. Cleared %i sound filter lists out of %i sound filter lists made." ),
                 sound_batch_floodfill_que.size(),
                 map.m_sound_cache.filtered_sound_lists_cleared, map.m_sound_cache.filtered_sound_lists_made );
        add_msg( m_debug,
                 _( "Culled %i sounds this turn. Sounds vector size %i end of turn. Prior turn sound vector size %i" ),
                 map.m_sound_cache.sounds_culled_this_turn, sound_batch_floodfill_que.size(),
                 map.m_sound_cache.filtered_sound_lists_cleared, map.m_sound_cache.filtered_sound_lists_made );
    }
    map.m_sound_cache.prior_turn_sound_vector_size = ( soundperf ) ? 0 :
            map.m_sound_cache.sound_instances.size();
    map.m_sound_cache.sounds_this_turn = 0;
    map.m_sound_cache.attempted_monster_sounds = 0;
    map.m_sound_cache.attempted_NPC_sounds = 0;
    map.m_sound_cache.attempted_movement_sounds = 0;
    map.m_sound_cache.attempted_potential_deafening_sounds = 0;
    map.m_sound_cache.attempted_non_batch_floodfills = 0;
    map.m_sound_cache.batch_flooded_monster_sounds = 0;
    map.m_sound_cache.batch_flooded_NPC_sounds = 0;
    map.m_sound_cache.invalidated_batch_sounds = 0;
    map.m_sound_cache.filtered_sound_lists_made = 0;
    map.m_sound_cache.filtered_sound_lists_cleared = 0;
    // Also kill our filtered sound lists, just in case.
    map.m_sound_cache.sound_list_filtered.clear();
    sound_batch_floodfill_que.clear();
    if( soundperf ) {
        // If we are on sound performance mode, wipe any sounds that slipped through.
        map.m_sound_cache.sound_instances.clear();
    }
}

void sounds::reset_markers()
{
    sound_markers.clear();
}

std::vector<tripoint_bub_ms> sounds::get_footstep_markers()
{
    // Optimization, make this static and clear it in reset_markers?
    std::vector<tripoint_bub_ms> footsteps;
    footsteps.reserve( sound_markers.size() );
    for( const auto &mark : sound_markers ) {
        footsteps.push_back( mark.first );
    }
    return footsteps;
}

std::pair< std::vector<tripoint_bub_ms>, std::vector<tripoint_bub_ms>> sounds::get_monster_sounds()
{
    std::vector<tripoint_bub_ms> allsounds;
    std::vector<tripoint_bub_ms> monster_sounds;
    map &map = get_map();
    for( auto &soundcache : map.m_sound_cache.sound_instances ) {
        allsounds.emplace_back( soundcache.sound.origin );
        if( soundcache.from_monster ) {
            monster_sounds.emplace_back( soundcache.sound.origin );
        }
    }
    return { allsounds, monster_sounds };
}

std::string sounds::sound_at( const tripoint_bub_ms &location )
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
    optional_vpart_position vpart_opt = get_map().veh_at( player_character.bub_pos() );
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
        // We take the generic dB volume of a running engine to be 80dB
        play_ambient_variant_sound( id_and_variant.first, id_and_variant.second,
                                    sfx::get_heard_volume( player_character.bub_pos(), 80 ), ch, 1000 );
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
        // We take the generic dB volume of a running vehicle engine to be 80dB
        play_variant_sound( "vehicle", "gear_shift", get_heard_volume( player_character.bub_pos(), 80 ),
                            0_degrees, 0.8, 0.8 );
        add_msg( m_debug, "GEAR UP" );
    } else if( current_gear < previous_gear ) {
        play_variant_sound( "vehicle", "gear_shift", get_heard_volume( player_character.bub_pos(), 80 ),
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
        fade_audio_channel( ch, 0 );
        add_msg( m_debug, "STOP speed %d =/= %d", current_speed, previous_speed );
        play_ambient_variant_sound( id_and_variant.first, id_and_variant.second,
                                    sfx::get_heard_volume( player_character.bub_pos(), 80 ), ch, 1000, pitch );
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
    const avatar &player_character = get_avatar();
    const auto &ploc = player_character.bub_pos();
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
    const auto &map = get_map();
    VehicleList vehs = get_map().get_vehicles();
    unsigned char noise_factor = 0;
    unsigned char vol = 0;
    vehicle *veh = nullptr;


    const short t_absorp_player = map.get_cache_ref( ploc.z() ).absorption_cache[map.get_cache_ref(
                                      ploc.z() ).idx( ploc.x(), ploc.y() )];

    for( wrapped_vehicle vehicle : vehs ) {
        if( vehicle.v->vehicle_noise > 0 ) {
            const auto &veh_loc = vehicle.v->bub_ms_location();
            // This is a jank fix to get vehicles to not be deafening from accross the map.
            const int dist = rl_dist( ploc, veh_loc );
            const short unadjusted_vol = std::min( MAXIMUM_VOLUME_ATMOSPHERE,
                                                   dBspl_to_mdBspl( static_cast<short>( vehicle.v->vehicle_noise ) ) );
            const auto &veh_idx = map.get_cache_ref( veh_loc.z() ).idx( veh_loc.x(), veh_loc.y() );
            const auto &t_absorp_avg = static_cast<short>( std::round( ( t_absorp_player + ( map.get_cache_ref(
                                           veh_loc.z() ).absorption_cache[veh_idx] ) ) / 2 ) );
            const short adjusted_vol = mdBspl_to_dBspl( std::max( 0,
                                       unadjusted_vol - get_cumulative_vol_dist_loss( 1, dist, t_absorp_avg ) - vol_z_adjust( veh_loc,
                                               ploc, player_character.sees( veh_loc ) ) ) );
            if( adjusted_vol > noise_factor ) {

                noise_factor = adjusted_vol;
                veh = vehicle.v;
            }
        }

    }
    if( !noise_factor || !veh ) {
        fade_audio_channel( ch, 300 );
        add_msg( m_debug, "STOP exterior_engine_sound, NO NOISE" );
        return;
    }
    // we only want volume going from 0 - 100. Our dB level goes from 0 - 191, but we want to hit the top of our range near 120.
    vol = static_cast<unsigned char>( ( noise_factor >= 120 ) ? 100 : ( noise_factor > 90 ) ?
                                      std::round( noise_factor * 0.8 ) : noise_factor );
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
            set_channel_3d_position( ch, get_heard_angle( veh->bub_ms_location() ) );
            set_channel_volume( ch, vol );
            add_msg( m_debug, "PLAYING exterior_engine_sound, vol: %d", vol );
        } else {
            engine_external_id_and_variant = id_and_variant;
            fade_audio_channel( ch, 0 );
            add_msg( m_debug, "STOP exterior_engine_sound, change id/var" );
            play_ambient_variant_sound( id_and_variant.first, id_and_variant.second, 128, ch, 0 );
            set_channel_3d_position( ch, get_heard_angle( veh->bub_ms_location() ) );
            set_channel_volume( ch, vol );
            add_msg( m_debug, "START exterior_engine_sound %s %s vol: %d", id_and_variant.first,
                     id_and_variant.second, get_channel_volume( ch ) );
        }
    } else {
        play_ambient_variant_sound( id_and_variant.first, id_and_variant.second, 128, ch, 0 );
        set_channel_3d_position( ch, get_heard_angle( veh->bub_ms_location() ) );
        set_channel_volume( ch, vol );
        add_msg( m_debug, "START exterior_engine_sound NEW %s %s vol: ex:%d true:%d",
                 id_and_variant.first, id_and_variant.second, vol, get_channel_volume( ch ) );
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
    // If the source is the player avatar, our ambient volume is returned as 100 * g_sfx_volume_multiplier anyways.
    // const int heard_volume = get_heard_volume( player_character.bub_pos(), 100 );
    const int heard_volume = std::ceil( 100 * g_sfx_volume_multiplier );
    const bool is_underground = player_character.bub_pos().z() < 0;
    const bool is_sheltered = g->is_sheltered( player_character.bub_pos() );
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
void sfx::generate_gun_sound( const tripoint_bub_ms &source, const item &firing,
                              const short &origin_vol )
{
    if( test_mode ) {
        return;
    }

    end_sfx_timestamp = std::chrono::high_resolution_clock::now();
    sfx_time = end_sfx_timestamp - start_sfx_timestamp;
    if( std::chrono::duration_cast<std::chrono::milliseconds> ( sfx_time ).count() < 80 ) {
        return;
    }
    int heard_volume = get_heard_volume( source, origin_vol );
    heard_volume = std::max( heard_volume, 10 );

    itype_id weapon_id = firing.typeId();
    units::angle angle = 0_degrees;
    std::string selected_sound;
    const avatar &player_character = get_avatar();
    // this does not mean p == avatar (it could be a vehicle turret)
    if( player_character.bub_pos() == source ) {
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
        if( heard_volume >= 60 ) {
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
    sound_thread( const tripoint_bub_ms &source, const tripoint_bub_ms &target, bool hit, bool targ_mon,
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

void sfx::generate_melee_sound( const tripoint_bub_ms &source, const tripoint_bub_ms &target,
                                bool hit,
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

sfx::sound_thread::sound_thread( const tripoint_bub_ms &source, const tripoint_bub_ms &target,
                                 const bool hit,
                                 const bool targ_mon, const std::string &material )
    : hit( hit )
    , targ_mon( targ_mon )
    , material( material )
{
    // This is function is run in the main thread.
    // Take melee strikes at 80dB
    const player *p = g->critter_at<npc>( source );
    const int heard_volume = get_heard_volume( source, 80 );

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
    // Take projectile impacts at a baseline of 80dB
    const int heard_volume = sfx::get_heard_volume( target.bub_pos(), 80 );
    const units::angle angle = get_heard_angle( target.bub_pos() );
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
    // Take the origin volume at 80dB
    int heard_volume = get_heard_volume( target.bub_pos(), 80 );
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
    const int hostiles = player_character.get_mon_visible().combat_hostile_count;
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
        // Take footsteps at 60dB.
        int heard_volume = sfx::get_heard_volume( player_character.bub_pos(), 60 );
        const auto terrain = get_map().ter( player_character.bub_pos() ).id();
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

        auto veh_displayed_part = g->m.veh_at( g->u.bub_pos() ).part_displayed();

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
    // Take volume at 60dB
    int heard_volume = sfx::get_heard_volume( get_avatar().bub_pos(), 60 );

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
void sfx::generate_gun_sound( const tripoint_bub_ms &, const item &, const short &origin_vol ) { }
void sfx::generate_melee_sound( const tripoint_bub_ms &, const tripoint_bub_ms &, bool, bool,
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
  * without sound support.
  * @param source: bubble tripoint of the source of the sound.
  * @param origin_volume: Volume at the origin in dB spl. The originating sound even should have a origin volume in dB that can be converted from.
  * @param in_mdB: Was the origin volume supplied in mdB spl instead of dB spl?
  */
/*@{*/
int sfx::get_heard_volume( const tripoint_bub_ms &source, const short &origin_volume,
                           const bool &in_mdB )
{
    // Dont play any sfx if a sound is out of bounds or if it has no volume.
    if( !get_map().inbounds( source ) || origin_volume == 0 ) {
        return 0;
    }
    if( source == get_avatar().bub_pos() ) {
        return ( 100 * g_sfx_volume_multiplier );
    }
    const int distance = rl_dist( get_avatar().bub_pos(), source );
    const auto &ploc = get_avatar().bub_pos();
    const auto &lev_cache = get_map().get_cache_ref( ploc.z() );
    const auto &absorp_cache = lev_cache.absorption_cache;
    const auto &avg_t_absorp = static_cast<short>( std::round( 0.5 * ( absorp_cache[lev_cache.idx(
                                   ploc.x(), ploc.y() )] + absorp_cache[lev_cache.idx( source.x(), source.y() )] ) ) );
    const auto &o_vol = ( in_mdB ) ? origin_volume : dBspl_to_mdBspl( origin_volume );
    // We are currently working in mdB spl. We will need to convert this to our sfx volume, which is 0 - 100.
    int heard_volume = std::max( 0, o_vol - get_cumulative_vol_dist_loss( 1, distance,
                                 avg_t_absorp ) - vol_z_adjust( source, ploc, get_avatar().sees( source ) ) );
    // Return 0 if we were too far to hear it to avoid a divide by zero error.
    if( heard_volume == 0 ) {
        return 0;
    }
    // We need to scale our sfx volume output based upon the ratio of the heard sound to its origin volume
    // If a sound is supposed to be played at 100 sfx vol at 60dB, if we hear it at 54dB we want to hear it at close to 90 sfx vol
    // But if a sound is supposed to play at 100sfx vol at 180dB, if we hear it at 60dB we should be hearing it at a sfx vol of ~33
    const auto vol_ratio = static_cast<float>( heard_volume ) / static_cast<float>( o_vol );
    // Convert our volume and bound it to 1-100. It was loud enough to be heard, but our conversions might round it to 0 and we want it to be played.
    heard_volume = std::min( 100, std::max( 1, static_cast<int>( std::round( 100 * vol_ratio ) ) ) );
    heard_volume *= g_sfx_volume_multiplier;
    return ( heard_volume );
}

units::angle sfx::get_heard_angle( const tripoint_bub_ms &source )
{
    units::angle angle = coord_to_angle( get_player_character().bub_pos(), source ) + 90_degrees;
    //add_msg(m_warning, "angle: %i", angle);
    return angle;
}
/*@}*/
