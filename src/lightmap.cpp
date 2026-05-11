#include "lightmap.h" // IWYU pragma: associated
#include "shadowcasting.h" // IWYU pragma: associated

#include <algorithm>
#include <cmath>
#include <ranges>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_unreachable.h"
#include "cata_utility.h"
#include "character.h"
#include "cuboid_rectangle.h"
#include "field.h"
#include "fragment_cloud.h" // IWYU pragma: keep
#include "game.h"
#include "game_constants.h"
#include "int_id.h"
#include "item.h"
#include "item_stack.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "math_defines.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "player.h"
#include "point.h"
#include "profile.h"
#include "string_formatter.h"
#include "submap.h"
#include "cached_options.h"
#include "thread_pool.h"
#include "tileray.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"

static const efftype_id effect_haslight( "haslight" );
static const efftype_id effect_onfire( "onfire" );

// Fraction of default_daylight_level() seen by outdoor tiles in shadow (not in direct solar LOS).
// Must be < LIGHT_SOURCE_BRIGHT / default_daylight_level() = 10/100 = 0.10 so shadow tiles
// render as LOW (dim, visible) rather than BRIGHT (same as direct sunlight).
static constexpr float SOLAR_SHADOW_SCATTER = 0.09f;

void map::add_light_from_items( const tripoint &p, const item_stack::iterator &begin,
                                const item_stack::iterator &end )
{
    for( auto itm_it = begin; itm_it != end; ++itm_it ) {
        float ilum = 0.0f; // brightness
        units::angle iwidth = 0_degrees; // 0-360 degrees. 0 is a circular light_source
        units::angle idir = 0_degrees;   // otherwise, it's a light_arc pointed in this direction
        if( ( *itm_it )->getlight( ilum, iwidth, idir ) ) {
            if( iwidth > 0_degrees ) {
                apply_light_arc( p, idir, ilum, iwidth );
            } else {
                add_light_source( p, ilum );
            }
        }
    }
}

// Refresh the weather-transparency lookup table to match the current sight
// penalty.  Must be called once serially before any parallel invocation of
// build_transparency_cache() so that the shared table is never written by
// more than one thread (RISK-1 fix).
void map::update_weather_transparency_lookup()
{
    assert( !is_pool_worker_thread() &&
            "update_weather_transparency_lookup() must be called serially "
            "before any parallel build_transparency_cache() invocation" );
    const float sight_penalty = get_weather().weather_id->sight_penalty;
    if( sight_penalty != 1.0f &&
        LIGHT_TRANSPARENCY_OPEN_AIR * sight_penalty != weather_lookup_.transparency ) {
        weather_lookup_.reset( LIGHT_TRANSPARENCY_OPEN_AIR * sight_penalty );
    }
}

bool map::build_transparency_cache( const int zlev )
{
    ZoneScopedN( "build_transparency_cache" );
    auto &map_cache = get_cache( zlev );
    auto &transparency_cache = map_cache.transparency_cache;

    if( map_cache.transparency_cache_dirty.none() ) {
        return false;
    }

    // if true, all submaps are invalid (can use batch init)
    const bool rebuild_all = map_cache.transparency_cache_dirty.all();

    if( rebuild_all ) {
        // Default to just barely not transparent.
        std::fill( transparency_cache.begin(), transparency_cache.end(),
                   static_cast<float>( LIGHT_TRANSPARENCY_OPEN_AIR ) );
    }

    // Traverse the submaps; delegate to per-submap rebuild, then copy the
    // 12×12 result into the flat render cache.
    //
    // Each smx column writes to a unique flat-cache region and reads only its
    // own submap's terrain data, so the smx loop is embarrassingly parallel.
    const auto process_smx = [&]( int smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            auto *cur_submap = get_submap_at_grid( tripoint_bub_sm{smx, smy, zlev} );
            const point sm_offset = sm_to_ms_copy( point( smx, smy ) );

            if( cur_submap == nullptr ) {
                // Null slots occur at bounded-dimension edges.
                // Treat as open air so they don't block light propagation.
                if( !rebuild_all ) {
                    for( int sx = 0; sx < SEEX; ++sx ) {
                        std::fill_n( transparency_cache.data() + map_cache.idx( sm_offset.x + sx, sm_offset.y ),
                                     SEEY, LIGHT_TRANSPARENCY_OPEN_AIR );
                    }
                }
                continue;
            }

            if( !rebuild_all && !map_cache.transparency_cache_dirty.test(
                    static_cast<size_t>( map_cache.bidx( smx, smy ) ) ) ) {
                continue;
            }

            cur_submap->transparency_dirty = true;
            cur_submap->rebuild_transparency_cache( *this, tripoint_bub_sm( smx, smy, zlev ) );

            if( cur_submap->is_uniform ) {
                const float value = cur_submap->transparency_cache[0][0];
                // if rebuild_all==true all values were already set to LIGHT_TRANSPARENCY_OPEN_AIR
                if( !rebuild_all || value != LIGHT_TRANSPARENCY_OPEN_AIR ) {
                    for( int sx = 0; sx < SEEX; ++sx ) {
                        std::fill_n( transparency_cache.data() + map_cache.idx( sm_offset.x + sx, sm_offset.y ),
                                     SEEY, value );
                    }
                }
            } else {
                for( int sx = 0; sx < SEEX; ++sx ) {
                    const int x = sx + sm_offset.x;
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        const int y = sy + sm_offset.y;
                        auto value = cur_submap->transparency_cache[sx][sy];
                        // Nudge towards fast paths
                        if( std::fabs( value - LIGHT_TRANSPARENCY_OPEN_AIR ) <= 0.0001f ) {
                            value = LIGHT_TRANSPARENCY_OPEN_AIR;
                        } else if( std::fabs( value - weather_lookup_.transparency ) <= 0.0001f ) {
                            value = weather_lookup_.transparency;
                        }
                        transparency_cache[map_cache.idx( x, y )] = value;
                    }
                }
            }
        }
    };

    if( parallel_enabled && parallel_map_cache && !is_pool_worker_thread() ) {
        parallel_for( 0, my_MAPSIZE, process_smx );
    } else {
        for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
            process_smx( smx );
        }
    }

    map_cache.transparency_cache_dirty.reset();

    return true;
}

bool map::build_vision_transparency_cache( const Character &player )
{
    const tripoint &p = player.pos();

    bool dirty = false;

    if( player.movement_mode_is( CMM_CROUCH ) ) {

        const auto check_vehicle_coverage = []( const vehicle * veh, point  p ) -> bool {
            return veh->obstacle_at_position( p ) == -1 && ( veh->part_with_feature( p,  "AISLE", true ) != -1 || veh->part_with_feature( p,  "PROTRUSION", true ) != -1 );
        };

        const optional_vpart_position player_vp = veh_at( p );

        point player_mount;
        if( player_vp ) {
            player_mount = player_vp->vehicle().bubble_to_mount( tripoint_bub_ms( p ) );
        }

        int i = 0;
        for( point adjacent : eight_adjacent_offsets ) {
            vision_transparency_cache[i] = VISION_ADJUST_NONE;

            // If we're crouching behind an obstacle, we can't see past it.
            if( coverage( adjacent + p ) >= 30 ) {
                dirty = true;
                vision_transparency_cache[i] = VISION_ADJUST_SOLID;
            } else {
                if( std::ranges::find( four_diagonal_offsets,
                                       adjacent ) != four_diagonal_offsets.end() ) {
                    const optional_vpart_position adjacent_vp = veh_at( p + adjacent );

                    point adjacent_mount;
                    if( adjacent_vp ) {
                        adjacent_mount = adjacent_vp->vehicle().bubble_to_mount( tripoint_bub_ms( p ) );
                    }

                    if( ( player_vp &&
                          !player_vp->vehicle().check_rotated_intervening( player_mount,
                                  player_vp->vehicle().bubble_to_mount( tripoint_bub_ms( p + adjacent ) ),
                                  check_vehicle_coverage ) )
                        || ( adjacent_vp && ( !player_vp ||  &( player_vp->vehicle() ) != &( adjacent_vp->vehicle() ) ) &&
                             !adjacent_vp->vehicle().check_rotated_intervening(
                                 adjacent_vp->vehicle().bubble_to_mount( tripoint_bub_ms( p ) ),
                                 adjacent_vp->vehicle().bubble_to_mount( tripoint_bub_ms( p + adjacent ) ),
                                 check_vehicle_coverage ) ) ) {
                        dirty = true;
                        vision_transparency_cache[ i ] = VISION_ADJUST_HIDDEN;
                    }
                }
            }

            i++;
        }
    } else {
        std::fill_n( &vision_transparency_cache[0], 8, VISION_ADJUST_NONE );
    }
    return dirty;
}

void map::apply_character_light( Character &p )
{
    if( p.has_effect( effect_onfire ) ) {
        apply_light_source( p.pos(), 8 );
    } else if( p.has_effect( effect_haslight ) ) {
        apply_light_source( p.pos(), 4 );
    }

    const float held_luminance = p.active_light();
    if( held_luminance > LIGHT_AMBIENT_LOW ) {
        apply_light_source( p.pos(), held_luminance );
    }

    if( held_luminance >= 4 && held_luminance > ambient_light_at( p.pos() ) - 0.5f ) {
        p.add_effect( effect_haslight, 1_turns );
    }
}

// This function raytraces starting at the upper limit of the simulated area descending
void map::update_solar_params()
{
    const time_point now        = calendar::turn;
    const time_point sr         = sunrise( now );
    const time_point ss         = sunset( now );
    const time_duration day_dur = ss - sr;

    if( day_dur <= 0_turns || now <= sr || now >= ss ) {
        m_solar.direct_active = false;
        m_solar.dx_per_z      = 0.f;
        m_solar.dy_per_z      = 0.f;
        return;
    }

    // Map daylight progress [0,1] to theta [0°,180°]: sunrise = 0°, noon = 90°, sunset = 180°.
    const auto progress  = to_turns<double>( now - sr ) / to_turns<double>( day_dur );
    const auto theta_deg = static_cast<float>( progress * 180.0 );
    const auto theta_rad = theta_deg * static_cast<float>( M_PI ) / 180.f;

    // Active throughout all daylight hours; night is handled by the early return above.
    m_solar.direct_active = true;

    // Clamp theta before computing cot to keep sin well away from zero near the horizon.
    // This replaces the old threshold cutoff: instead of disabling shadows at shallow angles,
    // we hold them at maximum length. Shadows exist from sunrise to sunset with no abrupt
    // appear/disappear transition; only the day/night boundary itself triggers the change.
    static constexpr auto SUN_CLAMP_MIN_RAD = 15.f * static_cast<float>( M_PI ) / 180.f;
    static constexpr auto SUN_CLAMP_MAX_RAD = 165.f * static_cast<float>( M_PI ) / 180.f;
    const auto theta_clamped = std::clamp( theta_rad, SUN_CLAMP_MIN_RAD, SUN_CLAMP_MAX_RAD );
    const auto sin_c = std::sin( theta_clamped );
    const auto cos_c = std::cos( theta_clamped );

    // dx_per_z = SOLAR_SHADOW_SCALE * Z_LEVEL_SCALE * cot(theta_clamped).
    // SOLAR_SHADOW_SCALE (1.5) is a tuning multiplier; SOLAR_SHADOW_MAX caps extreme values.
    // Flip SUN_EAST_SIGN to -1 if +x does not map to east in the tileset.
    static constexpr auto SUN_EAST_SIGN      = 1.f;
    static constexpr auto SOLAR_SHADOW_SCALE = 1.5f;
    static constexpr auto SOLAR_SHADOW_MAX   = 6.f;
    const auto raw = SUN_EAST_SIGN * SOLAR_SHADOW_SCALE * Z_LEVEL_SCALE * cos_c / sin_c;
    m_solar.dx_per_z = std::clamp( raw, -SOLAR_SHADOW_MAX, SOLAR_SHADOW_MAX );
    m_solar.dy_per_z = 0.f;  // No latitude tilt modelled.
}

void map::build_angled_sunlight_cache( const int zlev )
{
    level_cache &ch      = get_cache( zlev );
    auto &solar_cache    = ch.angled_sunlight_cache;

    if( !m_solar.direct_active ) {
        std::ranges::fill( solar_cache, false );
        return;
    }

    const auto step_x  = m_solar.dx_per_z;
    const auto step_y  = m_solar.dy_per_z;
    const auto max_step = OVERMAP_HEIGHT - zlev;

    std::ranges::for_each(
        std::views::iota( 0, ch.cache_x ),
    [&]( int x ) {
        std::ranges::for_each(
            std::views::iota( 0, ch.cache_y ),
        [&]( int y ) {
            // A floor directly overhead always blocks, regardless of sun angle.
            // Without this, angled rays jump laterally past the roof tile at step 1.
            if( zlev + 1 <= OVERMAP_HEIGHT ) {
                const level_cache &above = get_cache_ref( zlev + 1 );
                const auto aidx = above.idx( x, y );
                if( above.floor_cache[aidx] ) {
                    solar_cache[ch.idx( x, y )] = false;
                    return;
                }
            }
            // Trace the sun ray upward one z-level at a time.
            // Solar-lit if the ray exits the map bounds without hitting a floor.
            const auto blocked = std::ranges::any_of(
                                     std::views::iota( 1, max_step + 1 )
            | std::views::transform( [&]( int step ) {
                return std::pair{ step, point{
                        x + static_cast<int>( std::roundf( step_x * static_cast<float>( step ) ) ),
                        y + static_cast<int>( std::roundf( step_y * static_cast<float>( step ) ) )
                    } };
            } )
            | std::views::take_while( [&]( const auto & sp ) {
                return sp.second.x >= 0 && sp.second.x < ch.cache_x
                       && sp.second.y >= 0 && sp.second.y < ch.cache_y;
            } ),
            [&]( const auto & sp ) {
                const level_cache &uch = get_cache_ref( zlev + sp.first );
                const auto uidx = uch.idx( sp.second.x, sp.second.y );
                return static_cast<bool>( uch.floor_cache[uidx] );
            }
                                 );
            solar_cache[ch.idx( x, y )] = !blocked;
        }
        );
    }
    );
}

// toward the lower limit. Since it's sunlight, the rays are parallel.
// Each layer consults the next layer up to determine the intensity of the light that reaches it.
// Once this is complete, additional operations add more dynamic lighting.
void map::build_sunlight_cache( int pzlev )
{
    const int zlev_min = zlevels ? -OVERMAP_DEPTH : pzlev;
    // Start at the topmost populated zlevel to avoid unnecessary raycasting
    // Plus one zlevel to prevent clipping inside structures
    const int zlev_max = zlevels
                         ? clamp( calc_max_populated_zlev() + 1,
                                  std::min( OVERMAP_HEIGHT, pzlev + 1 ),
                                  OVERMAP_HEIGHT )
                         : pzlev;

    // true if all previous z-levels are fully transparent to light (no floors, transparency >= air)
    bool fully_outside = true;

    // true if no light reaches this level, i.e. there were no lit tiles on the above level (light level <= inside_light_level)
    bool fully_inside = false;

    // fully_outside and fully_inside define following states:
    // initially: fully_outside=true, fully_inside=false  (fast fill)
    //    ↓
    // when first obstacles occur: fully_outside=false, fully_inside=false  (slow quadrant logic)
    //    ↓
    // when fully below ground: fully_outside=false, fully_inside=true  (fast fill)

    // Rebuild the directional sunlight cache once per in-game hour (expensive path only).
    if( fov_3d_occlusion ) {
        const auto current_hour = to_hours<int>( time_past_midnight( calendar::turn ) );
        if( current_hour != m_solar.last_built_hour ) {
            update_solar_params();
            std::ranges::for_each(
                std::views::iota( zlev_min, zlev_max + 1 ),
            [this]( int z ) { build_angled_sunlight_cache( z ); }
            );
            m_solar.last_built_hour = current_hour;
        }
    }

    // Iterate top to bottom because sunlight cache needs to construct in that order.
    for( int zlev = zlev_max; zlev >= zlev_min; zlev-- ) {

        level_cache &map_cache = get_cache( zlev );
        auto &lm = map_cache.lm;
        // Grab illumination at ground level.
        const float outside_light_level = g->natural_light_level( 0 );
        // TODO: if zlev < 0 is open to sunlight, this won't calculate correct light, but neither does g->natural_light_level()
        const float inside_light_level = ( zlev >= 0 && outside_light_level > LIGHT_SOURCE_BRIGHT ) ?
                                         LIGHT_AMBIENT_DIM * 0.8 : LIGHT_AMBIENT_LOW;
        // Handling when z-levels are disabled is based on whether a tile is considered "outside".
        if( !zlevels ) {
            const auto &outside_cache = map_cache.outside_cache;
            for( int x = 0; x < map_cache.cache_x; x++ ) {
                for( int y = 0; y < map_cache.cache_y; y++ ) {
                    if( outside_cache[map_cache.idx( x, y )] ) {
                        lm[map_cache.idx( x, y )].fill( outside_light_level );
                    } else {
                        lm[map_cache.idx( x, y )].fill( inside_light_level );
                    }
                }
            }
            continue;
        }

        // all light was blocked before
        if( fully_inside ) {
            std::fill( lm.begin(), lm.end(), four_quadrants( inside_light_level ) );
            continue;
        }

        // If there were no obstacles before this level, just apply weather illumination since there's no opportunity
        // for light to be blocked.
        if( fully_outside ) {
            // No floors above: every tile has unobstructed sky, so all get the natural outdoor level.
            // Shadow dimming (SOLAR_SHADOW_SCATTER) is applied in ambient_light_at / apparent_light_at,
            // not here, so lm always carries full outdoor brightness for cascade and bleed purposes.
            const auto sky_level = outside_light_level;
            std::fill( lm.begin(), lm.end(), four_quadrants( sky_level ) );

            const auto &this_floor_cache = map_cache.floor_cache;
            const auto &this_transparency_cache = map_cache.transparency_cache;
            fully_inside = true; // recalculate

            for( int x = 0; x < map_cache.cache_x; ++x ) {
                for( int y = 0; y < map_cache.cache_y; ++y ) {
                    // && semantics below is important, we want to skip the evaluation if possible, do not replace with &=

                    // fully_outside stays true if tile is transparent and there is no floor
                    fully_outside = fully_outside &&
                                    this_transparency_cache[map_cache.idx( x, y )] >= LIGHT_TRANSPARENCY_OPEN_AIR
                                    && !this_floor_cache[map_cache.idx( x, y )];
                    // fully_inside stays true if tile is opaque OR there is floor
                    fully_inside = fully_inside &&
                                   ( this_transparency_cache[map_cache.idx( x, y )] <= LIGHT_TRANSPARENCY_SOLID ||
                                     this_floor_cache[map_cache.idx( x, y )] );
                }
            }
            continue;
        }

        // Directional sunlight is handled via angled_sunlight_cache (see build_angled_sunlight_cache).
        // The cascade here provides indoor bleed and scatter propagation; it samples directly above.
        const point offset = point_zero;
        const level_cache &prev_map_cache = get_cache_ref( zlev + 1 );
        const auto &prev_lm = prev_map_cache.lm;
        const auto &prev_transparency_cache = prev_map_cache.transparency_cache;
        const auto &prev_floor_cache = prev_map_cache.floor_cache;
        const auto &prev_vehicle_floor_cache = prev_map_cache.vehicle_floor_cache;
        const auto &outside_cache = map_cache.outside_cache;
        const float sight_penalty = get_weather().weather_id->sight_penalty;
        // TODO: Replace these with a lookup inside the four_quadrants class.
        constexpr std::array<point, 5> cardinals = {
            {point_zero, point_north, point_west, point_east, point_south}
        };
        constexpr std::array<std::array<quadrant, 2>, 5> dir_quadrants = {{
                {{quadrant::NE, quadrant::NW}},
                {{quadrant::NE, quadrant::NW}},
                {{quadrant::SW, quadrant::NW}},
                {{quadrant::SE, quadrant::NE}},
                {{quadrant::SE, quadrant::SW}},
            }
        };

        fully_inside = true; // recalculate

        // Fall back to minimal light level if we don't find anything.
        std::fill( lm.begin(), lm.end(), four_quadrants( inside_light_level ) );

        for( int x = 0; x < map_cache.cache_x; ++x ) {
            for( int y = 0; y < map_cache.cache_y; ++y ) {
                // Check center, then four adjacent cardinals.
                for( int i = 0; i < 5; ++i ) {
                    int prev_x = x + offset.x + cardinals[i].x;
                    int prev_y = y + offset.y + cardinals[i].y;
                    bool inbounds = prev_x >= 0 && prev_x < prev_map_cache.cache_x &&
                                    prev_y >= 0 && prev_y < prev_map_cache.cache_y;

                    if( !inbounds ) {
                        continue;
                    }

                    float prev_light_max;
                    float prev_transparency = prev_transparency_cache[prev_map_cache.idx( prev_x, prev_y )];
                    // This is pretty gross, this cancels out the per-tile transparency effect
                    // derived from weather.
                    if( outside_cache[map_cache.idx( x, y )] ) {
                        prev_transparency /= sight_penalty;
                    }

                    if( prev_transparency > LIGHT_TRANSPARENCY_SOLID &&
                        !prev_floor_cache[prev_map_cache.idx( prev_x, prev_y )] &&
                        !prev_vehicle_floor_cache[prev_map_cache.idx( prev_x, prev_y )] &&
                        ( prev_light_max = prev_lm[prev_map_cache.idx( prev_x, prev_y )].max() ) > 0.0 ) {
                        const float light_level = clamp( prev_light_max * LIGHT_TRANSPARENCY_OPEN_AIR / prev_transparency,
                                                         inside_light_level, prev_light_max );

                        if( i == 0 ) {
                            lm[map_cache.idx( x, y )].fill( light_level );
                            fully_inside &= light_level <= inside_light_level;
                            break;
                        } else {
                            fully_inside &= light_level <= inside_light_level;
                            lm[map_cache.idx( x, y )][dir_quadrants[i][0]] = light_level;
                            lm[map_cache.idx( x, y )][dir_quadrants[i][1]] = light_level;
                        }
                    }
                }
            }
        }

        // Override solar-lit outdoor tiles to full outside_light_level.
        // Shadow tiles are left at cascade-computed values; scatter dimming is applied
        // in apparent_light_at (rendering only) so distance decay works correctly.
        if( fov_3d_occlusion && m_solar.direct_active ) {
            const auto &solar_cache = map_cache.angled_sunlight_cache;
            std::ranges::for_each(
                std::views::iota( 0, map_cache.cache_x * map_cache.cache_y ),
            [&]( int i ) {
                const auto idx = static_cast<size_t>( i );
                if( solar_cache[idx] ) {
                    lm[idx].fill( outside_light_level );
                    fully_inside = false;
                }
            }
            );
        }
    }
}

void map::generate_lightmap( const int zlev )
{
    auto &map_cache = get_cache( zlev );
    auto &lm = map_cache.lm;
    auto &sm = map_cache.sm;
    auto &light_source_buffer = map_cache.light_source_buffer;

    std::fill( lm.begin(), lm.end(), four_quadrants( 0.0f ) );
    std::fill( sm.begin(), sm.end(), 0.0f );
    std::fill( light_source_buffer.begin(), light_source_buffer.end(), 0.0f );

    build_sunlight_cache( zlev );

    apply_character_light( get_player_character() );
    for( npc &guy : g->all_npcs() ) {
        apply_character_light( guy );
    }
    for( monster &critter : g->all_monsters() ) {
        if( critter.is_hallucination() ) {
            continue;
        }
        const tripoint &mp = critter.pos();
        if( inbounds( mp ) ) {
            if( critter.has_effect( effect_onfire ) ) {
                apply_light_source( mp, 8 );
            }
            // TODO: [lightmap] Attach natural light brightness to creatures
            // TODO: [lightmap] Allow creatures to have light attacks (i.e.: eyebot)
            // TODO: [lightmap] Allow creatures to have facing and arc lights
            if( critter.type->luminance > 0 ) {
                apply_light_source( mp, critter.type->luminance );
            }
        }
    }

    generate_lightmap_worker( zlev );
}

void map::generate_lightmap_worker( const int zlev )
{
    ZoneScoped;
    auto &map_cache = get_cache( zlev );
    auto &lm = map_cache.lm;
    auto &prev_floor_cache = get_cache( clamp( zlev + 1, -OVERMAP_DEPTH, OVERMAP_DEPTH ) ).floor_cache;
    auto &prev_vehicle_floor_cache = get_cache( clamp( zlev + 1, -OVERMAP_DEPTH,
                                     OVERMAP_DEPTH ) ).vehicle_floor_cache;
    bool top_floor = zlev == OVERMAP_DEPTH;

    /* Bulk light sources wastefully cast rays into neighbors; a burning hospital can produce
         significant slowdown, so for stuff like fire and lava:
     * Step 1: Store the position and luminance in buffer via add_light_source, for efficient
         checking of neighbors.
     * Step 2: After everything else, iterate buffer and apply_light_source only in non-redundant
         directions
     * Step 3: ????
     * Step 4: Profit!
     */
    auto &light_source_buffer = map_cache.light_source_buffer;

    constexpr std::array<int, 4> dir_x = { {  0, -1, 1, 0 } };    //    [0]
    constexpr std::array<int, 4> dir_y = { { -1,  0, 0, 1 } };    // [1][X][2]
    constexpr std::array<int, 4> dir_d = { { 90, 0, 180, 270 } }; //    [3]
    constexpr std::array<std::array<quadrant, 2>, 4> dir_quadrants = { {
            {{ quadrant::NE, quadrant::NW }},
            {{ quadrant::SW, quadrant::NW }},
            {{ quadrant::SE, quadrant::NE }},
            {{ quadrant::SE, quadrant::SW }},
        }
    };

    const float natural_light = g->natural_light_level( zlev );

    std::vector<std::pair<tripoint, float>> lm_override;
    {
        ZoneScopedN( "generate_lightmap_collect" );

        // Per-smx deferred accumulators for light operations that write across the map.
        // apply_directional_light and apply_light_arc are unsafe to run concurrently;
        // they are collected here and applied serially after the parallel pass.
        struct dir_light_def {
            tripoint p;
            int direction;
            float luminance;
        };
        struct arc_light_def {
            tripoint p;
            units::angle dir;
            float luminance;
            units::angle width;
        };
        struct smx_acc {
            std::vector<std::pair<tripoint, float>> lm_override;
            std::vector<dir_light_def>              dir_lights;
            std::vector<arc_light_def>              arc_lights;
        };
        std::vector<smx_acc> smx_accs( my_MAPSIZE );

        auto process_smx = [&]( int smx ) {
            auto &local = smx_accs[smx];
            for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
                const auto cur_submap = get_submap_at_grid( tripoint_bub_sm{ smx, smy, zlev } );
                if( cur_submap == nullptr ) {
                    continue;
                }
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        const int x = sx + smx * SEEX;
                        const int y = sy + smy * SEEY;
                        const tripoint p( x, y, zlev );
                        // Project light into any openings into buildings.
                        // Check both terrain floor_cache and vehicle_floor_cache since vehicle
                        // roofs are no longer written into floor_cache.
                        auto has_floor_above = [&]( int idx ) {
                            return prev_floor_cache[idx] || prev_vehicle_floor_cache[idx];
                        };
                        if( top_floor || has_floor_above( map_cache.idx( p.x, p.y ) ) ) {
                            // Apply light sources for external/internal divide.
                            // A neighbour is an outdoor light source if it has no floor above it
                            // and is part of a genuine outdoor area (not an isolated skylight hole).
                            // An isolated skylight has no adjacent open-sky neighbours, so we
                            // require at least one cardinal neighbour of `nb` (other than p) that
                            // also has no floor above.
                            for( int i = 0; i < 4; ++i ) {
                                const point neighbour = p.xy() + point( dir_x[i], dir_y[i] );
                                if( neighbour.x < 0 || neighbour.y < 0 ||
                                    neighbour.x >= map_cache.cache_x || neighbour.y >= map_cache.cache_y ) {
                                    continue;
                                }
                                if( !( top_floor || !has_floor_above( map_cache.idx( neighbour.x, neighbour.y ) ) ) ) {
                                    continue;
                                }
                                const bool nb_has_open_sky_neighbour = std::ranges::any_of(
                                std::views::iota( 0, 4 ), [&]( int j ) {
                                    const point cn = neighbour + point( dir_x[j], dir_y[j] );
                                    return cn != p.xy() &&
                                           cn.x >= 0 && cn.y >= 0 &&
                                           cn.x < map_cache.cache_x && cn.y < map_cache.cache_y &&
                                           !has_floor_above( map_cache.idx( cn.x, cn.y ) );
                                } );
                                if( !nb_has_open_sky_neighbour ) {
                                    continue;
                                }
                                const float source_light =
                                    std::min( natural_light, lm[map_cache.idx( neighbour.x, neighbour.y )].max() );
                                if( light_transparency( p ) > LIGHT_TRANSPARENCY_SOLID ) {
                                    update_light_quadrants( lm[map_cache.idx( p.x, p.y )], source_light, quadrant::default_ );
                                    // apply_directional_light writes to arbitrary lm positions — defer.
                                    local.dir_lights.push_back( { p, dir_d[i], source_light } );
                                } else {
                                    update_light_quadrants( lm[map_cache.idx( p.x, p.y )], source_light, dir_quadrants[i][0] );
                                    update_light_quadrants( lm[map_cache.idx( p.x, p.y )], source_light, dir_quadrants[i][1] );
                                }
                            }
                        }

                        if( cur_submap->get_lum( { sx, sy } ) && has_items( p ) ) {
                            // Inline add_light_from_items to split arc (deferred) from point (safe).
                            auto items = i_at( p );
                            for( auto itm_it = items.begin(); itm_it != items.end(); ++itm_it ) {
                                float ilum = 0.0f;
                                units::angle iwidth = 0_degrees;
                                units::angle idir = 0_degrees;
                                if( ( *itm_it )->getlight( ilum, iwidth, idir ) ) {
                                    if( iwidth > 0_degrees ) {
                                        // apply_light_arc writes to arbitrary lm positions — defer.
                                        local.arc_lights.push_back( { p, idir, ilum, iwidth } );
                                    } else {
                                        add_light_source( p, ilum );
                                    }
                                }
                            }
                        }

                        const ter_id terrain = cur_submap->get_ter( { sx, sy } );
                        if( terrain->light_emitted > 0 ) {
                            add_light_source( p, terrain->light_emitted );
                        }
                        const furn_id furniture = cur_submap->get_furn( {sx, sy } );
                        if( furniture->light_emitted > 0 ) {
                            add_light_source( p, furniture->light_emitted );
                        }

                        std::ranges::for_each( cur_submap->get_field( { sx, sy } ), [&]( auto & fld ) {
                            if( !fld.first.is_valid() ) {
                                debugmsg( "generate_lightmap: invalid field type id %d at "
                                          "grid(%d,%d,%d) tile(%d,%d) field_count=%d is_uniform=%d",
                                          fld.first.to_i(), smx, smy, zlev, sx, sy,
                                          cur_submap->field_count,
                                          static_cast<int>( cur_submap->is_uniform ) );
                                return;
                            }
                            const auto *cur = &fld.second;
                            const int light_emitted = cur->light_emitted();
                            if( light_emitted > 0 ) {
                                add_light_source( p, light_emitted );
                            }
                            const float light_override = cur->local_light_override();
                            if( light_override >= 0.0 ) {
                                local.lm_override.emplace_back( p, light_override );
                            }
                        } );
                    }
                }
            }
        };

        if( parallel_enabled && parallel_map_cache && !is_pool_worker_thread() ) {
            parallel_for( 0, my_MAPSIZE, process_smx );
        } else {
            for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
                process_smx( smx );
            }
        }

        // Merge per-smx accumulators.  Apply deferred shadowcasts serially to avoid lm races.
        std::ranges::for_each( smx_accs, [&]( auto & local ) {
            lm_override.insert( lm_override.end(), local.lm_override.begin(), local.lm_override.end() );
            std::ranges::for_each( local.dir_lights, [&]( auto & dl ) {
                apply_directional_light( dl.p, dl.direction, dl.luminance );
            } );
            std::ranges::for_each( local.arc_lights, [&]( auto & al ) {
                apply_light_arc( al.p, al.dir, al.luminance, al.width );
            } );
        } );


        // Apply any vehicle light sources
        VehicleList vehs = get_vehicles();
        for( auto &vv : vehs ) {
            vehicle *v = vv.v;

            auto lights = v->lights( true );

            float veh_luminance = 0.0;
            float iteration = 1.0;

            for( const auto pt : lights ) {
                const auto &vp = pt->info();
                if( vp.has_flag( VPFLAG_CONE_LIGHT ) ||
                    vp.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ) {
                    veh_luminance += vp.bonus / iteration;
                    iteration = iteration * 1.1;
                }
            }

            for( const auto pt : lights ) {
                const auto &vp = pt->info();
                tripoint src = v->global_part_pos3( *pt );

                if( !inbounds( src ) ) {
                    continue;
                }
                if( src.z != zlev ) {
                    continue;
                }

                if( vp.has_flag( VPFLAG_CONE_LIGHT ) ) {
                    if( veh_luminance > lit_level::LIT ) {
                        add_light_source( src, M_SQRT2 ); // Add a little surrounding light
                        apply_light_arc( src, v->face.dir() + pt->direction, veh_luminance,
                                         45_degrees );
                    }

                } else if( vp.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ) {
                    if( veh_luminance > lit_level::LIT ) {
                        add_light_source( src, M_SQRT2 ); // Add a little surrounding light
                        apply_light_arc( src, v->face.dir() + pt->direction, veh_luminance,
                                         90_degrees );
                    }

                } else if( vp.has_flag( VPFLAG_HALF_CIRCLE_LIGHT ) ) {
                    add_light_source( src, M_SQRT2 ); // Add a little surrounding light
                    apply_light_arc( src, v->face.dir() + pt->direction, vp.bonus, 180_degrees );

                } else if( vp.has_flag( VPFLAG_CIRCLE_LIGHT ) ) {
                    const bool odd_turn = calendar::once_every( 2_turns );
                    if( ( odd_turn && vp.has_flag( VPFLAG_ODDTURN ) ) ||
                        ( !odd_turn && vp.has_flag( VPFLAG_EVENTURN ) ) ||
                        ( !( vp.has_flag( VPFLAG_EVENTURN ) || vp.has_flag( VPFLAG_ODDTURN ) ) ) ) {

                        add_light_source( src, vp.bonus );
                    }

                } else {
                    add_light_source( src, vp.bonus );
                }
            }

            for( const vpart_reference &vp : v->get_all_parts() ) {
                const size_t p = vp.part_index();
                const tripoint pp = vp.pos();
                if( !inbounds( pp ) ) {
                    continue;
                }
                if( pp.z != zlev ) {
                    continue;
                }
                if( vp.has_feature( VPFLAG_CARGO ) && !vp.has_feature( "COVERED" ) ) {
                    add_light_from_items( pp, v->get_items( static_cast<int>( p ) ).begin(),
                                          v->get_items( static_cast<int>( p ) ).end() );
                }
            }
        }

    } // ZoneScopedN generate_lightmap_collect

    /* Now that we have position and intensity of all bulk light sources, apply_ them
      This may seem like extra work, but take a 12x12 raging inferno:
        unbuffered: (12^2)*(160*4) = apply_light_ray x 92160
        buffered:   (12*4)*(160)   = apply_light_ray x 7680
    */
    {
        ZoneScopedN( "generate_lightmap_flush" );
        const tripoint cache_start( 0, 0, zlev );
        const tripoint cache_end( map_cache.cache_x, map_cache.cache_y, zlev );
        for( const tripoint &p : points_in_rectangle( cache_start, cache_end ) ) {
            if( light_source_buffer[map_cache.idx( p.x, p.y )] > 0.0 ) {
                apply_light_source( p, light_source_buffer[map_cache.idx( p.x, p.y )] );
            }
        }
        for( const std::pair<tripoint, float> &elem : lm_override ) {
            lm[map_cache.idx( elem.first.x, elem.first.y )].fill( elem.second );
        }
    } // ZoneScopedN generate_lightmap_flush
}

void map::add_light_source( const tripoint &p, float luminance )
{
    auto &cache = get_cache( p.z );
    auto &light_source_buffer = cache.light_source_buffer;
    light_source_buffer[cache.idx( p.x, p.y )] = std::max( luminance,
            light_source_buffer[cache.idx( p.x, p.y )] );
}

// Tile light/transparency: 3D

lit_level map::light_at( const tripoint &p ) const
{
    if( !inbounds( p ) ) {
        return lit_level::DARK;    // Out of bounds
    }

    const auto &map_cache = get_cache_ref( p.z );
    const auto &lm = map_cache.lm;
    const auto &sm = map_cache.sm;
    if( sm[map_cache.idx( p.x, p.y )] >= LIGHT_SOURCE_BRIGHT ) {
        return lit_level::BRIGHT;
    }

    const float max_light = lm[map_cache.idx( p.x, p.y )].max();
    if( max_light >= LIGHT_AMBIENT_LIT ) {
        return lit_level::LIT;
    }

    if( max_light >= LIGHT_AMBIENT_LOW ) {
        return lit_level::LOW;
    }

    return lit_level::DARK;
}

float map::ambient_light_at( const tripoint &p ) const
{
    if( !inbounds( p ) ) {
        return 0.0f;
    }

    const auto &map_cache = get_cache_ref( p.z );
    float light = map_cache.lm[map_cache.idx( p.x, p.y )].max();

    if( fov_3d_occlusion && m_solar.direct_active ) {
        const auto idx = static_cast<size_t>( map_cache.idx( p.x, p.y ) );
        if( map_cache.outside_cache[idx] && !map_cache.angled_sunlight_cache[idx] ) {
            light = std::min( light,
                              static_cast<float>( default_daylight_level() ) * SOLAR_SHADOW_SCATTER );
        }
    }

    return light;
}

bool map::is_transparent( const tripoint &p ) const
{
    return light_transparency( p ) > LIGHT_TRANSPARENCY_SOLID;
}

float map::light_transparency( const tripoint &p ) const
{
    const auto &map_cache = get_cache_ref( p.z );
    return map_cache.transparency_cache[map_cache.idx( p.x, p.y )];
}

// End of tile light/transparency

map::apparent_light_info map::apparent_light_helper( const level_cache &map_cache,
        const tripoint &p )
{
    const float vis = std::max( map_cache.seen_cache[map_cache.idx( p.x, p.y )],
                                map_cache.camera_cache[map_cache.idx( p.x, p.y )] );
    // Use g_visible_threshold which scales with g_max_view_distance.
    const bool obstructed = vis <= LIGHT_TRANSPARENCY_SOLID + g_visible_threshold;

    // Scale vis so the LIT/LOW transition happens at g_max_view_distance instead of 60.
    // vis^(60/g_max) stretches the 1/exp(t*d) decay curve to match the current bubble size.
    const float scale_factor = 60.0f / static_cast<float>( g_max_view_distance );
    const float scaled_vis = ( vis > 0.0f ) ? std::pow( vis, scale_factor ) : 0.0f;

    auto is_opaque = [&map_cache]( point  p ) {
        return map_cache.transparency_cache[map_cache.idx( p.x, p.y )] <= LIGHT_TRANSPARENCY_SOLID &&
               get_player_character().pos().xy() != p;
    };

    const bool p_opaque = is_opaque( p.xy() );
    float apparent_light;

    if( p_opaque && scaled_vis > 0 ) {
        // This is the complicated case.  We want to check which quadrants the
        // player can see the tile from, and only count light values from those
        // quadrants.
        struct offset_and_quadrants {
            point offset;
            std::array<quadrant, 2> quadrants;
        };
        static constexpr std::array<offset_and_quadrants, 8> adjacent_offsets = {{
                { point_south,      {{ quadrant::SE, quadrant::SW }} },
                { point_north,      {{ quadrant::NE, quadrant::NW }} },
                { point_east,       {{ quadrant::SE, quadrant::NE }} },
                { point_south_east, {{ quadrant::SE, quadrant::SE }} },
                { point_north_east, {{ quadrant::NE, quadrant::NE }} },
                { point_west,       {{ quadrant::SW, quadrant::NW }} },
                { point_south_west, {{ quadrant::SW, quadrant::SW }} },
                { point_north_west, {{ quadrant::NW, quadrant::NW }} },
            }
        };

        four_quadrants seen_from( 0 );
        for( const offset_and_quadrants &oq : adjacent_offsets ) {
            const point neighbour = p.xy() + oq.offset;

            if( neighbour.x < 0 || neighbour.y < 0 ||
                neighbour.x >= map_cache.cache_x || neighbour.y >= map_cache.cache_y ) {
                continue;
            }
            if( is_opaque( neighbour ) ) {
                continue;
            }
            if( map_cache.seen_cache[map_cache.idx( neighbour.x, neighbour.y )] == 0 &&
                map_cache.camera_cache[map_cache.idx( neighbour.x, neighbour.y )] == 0 ) {
                continue;
            }
            // This is a non-opaque visible neighbour, so count visibility from the relevant
            // quadrants. Use scaled_vis to stretch the falloff over g_max_view_distance.
            seen_from[oq.quadrants[0]] = scaled_vis;
            seen_from[oq.quadrants[1]] = scaled_vis;
        }
        apparent_light = ( seen_from * map_cache.lm[map_cache.idx( p.x, p.y )] ).max();
    } else {
        // This is the simple case, for a non-opaque tile light from all
        // directions is equivalent. Use scaled_vis for the brightness calculation.
        apparent_light = scaled_vis * map_cache.lm[map_cache.idx( p.x, p.y )].max();
    }
    return { obstructed, apparent_light };
}

lit_level map::apparent_light_at( const tripoint &p, const visibility_variables &cache ) const
{
    const int dist = rl_dist( g->u.pos(), p );

    // Clairvoyance overrides everything.
    if( dist <= cache.u_clairvoyance ) {
        return lit_level::BRIGHT;
    }
    const auto &map_cache = get_cache_ref( p.z );
    const apparent_light_info a = apparent_light_helper( map_cache, p );

    float apparent_light = a.apparent_light;

    // Shadow cap: outdoor tiles without direct solar LOS are capped at scatter level.
    // Uses min rather than multiply so distance decay acts on full daylight first —
    // shadow tiles stay visible at full render distance and only fade at the edge.
    if( fov_3d_occlusion && m_solar.direct_active && !a.obstructed ) {
        const auto idx = static_cast<size_t>( map_cache.idx( p.x, p.y ) );
        if( map_cache.outside_cache[idx] && !map_cache.angled_sunlight_cache[idx] ) {
            apparent_light = std::min( apparent_light,
                                       static_cast<float>( default_daylight_level() ) * SOLAR_SHADOW_SCATTER );
        }
    }

    // Unimpaired range is an override to strictly limit vision range based on various conditions,
    // but the player can still see light sources.
    if( dist > g->u.unimpaired_range() ) {
        if( !a.obstructed && map_cache.sm[map_cache.idx( p.x, p.y )] > 0.0 ) {
            return lit_level::BRIGHT_ONLY;
        } else {
            return lit_level::DARK;
        }
    }
    if( a.obstructed ) {
        if( apparent_light > LIGHT_AMBIENT_LIT ) {
            if( apparent_light > cache.g_light_level ) {
                // This represents too hazy to see detail,
                // but enough light getting through to illuminate.
                return lit_level::BRIGHT_ONLY;
            } else {
                // If it's not brighter than the surroundings, it just ends up shadowy.
                return lit_level::LOW;
            }
        } else if( apparent_light >= cache.vision_threshold ) {
            // Tile is hazy but still within the player's actual vision capability
            // (e.g. extended night-vision range pushes the perceptible horizon past 60 tiles).
            return lit_level::LOW;
        } else {
            return lit_level::BLANK;
        }
    }
    // Then we just search for the light level in descending order.
    if( apparent_light > LIGHT_SOURCE_BRIGHT || map_cache.sm[map_cache.idx( p.x, p.y )] > 0.0 ) {
        return lit_level::BRIGHT;
    }
    if( apparent_light > LIGHT_AMBIENT_LIT ) {
        return lit_level::LIT;
    }
    if( apparent_light >= cache.vision_threshold ) {
        return lit_level::LOW;
    } else {
        return lit_level::BLANK;
    }
}

bool map::pl_sees( const tripoint &t, const int max_range ) const
{
    if( !inbounds( t ) ) {
        return false;
    }

    if( max_range >= 0 && square_dist( t, g->u.pos() ) > max_range ) {
        return false;    // Out of range!
    }

    const auto &map_cache = get_cache_ref( t.z );
    const apparent_light_info a = apparent_light_helper( map_cache, t );
    const float light_at_player = map_cache.lm[map_cache.idx( g->u.posx(), g->u.posy() )].max();
    return !a.obstructed &&
           ( a.apparent_light >= g->u.get_vision_threshold( light_at_player ) ||
             map_cache.sm[map_cache.idx( t.x, t.y )] > 0.0 );
}

bool map::pl_line_of_sight( const tripoint &t, const int max_range ) const
{
    if( !inbounds( t ) ) {
        return false;
    }

    if( max_range >= 0 && square_dist( t, g->u.pos() ) > max_range ) {
        // Out of range!
        return false;
    }

    const auto &map_cache = get_cache_ref( t.z );
    // Any epsilon > 0 is fine - it means lightmap processing visited the point
    return map_cache.seen_cache[map_cache.idx( t.x, t.y )] > 0.0f ||
           map_cache.camera_cache[map_cache.idx( t.x, t.y )] > 0.0f;
}

//Alters the vision caches to the player specific version, the restore caches will be filled so it can be undone with restore_vision_transparency_cache
void map::apply_vision_transparency_cache( const tripoint &center, int target_z,
        float ( &vision_restore_cache )[9], bool ( &blocked_restore_cache )[8] )
{
    level_cache &map_cache = get_cache( target_z );
    auto &transparency_cache = map_cache.transparency_cache;
    auto *blocked_data = map_cache.vehicle_obscured_cache.data();
    const int sy = map_cache.cache_y;

    int i = 0;
    for( point adjacent : eight_adjacent_offsets ) {
        const tripoint p = center + adjacent;
        if( !inbounds( p ) ) {
            continue;
        }
        vision_restore_cache[i] = transparency_cache[map_cache.idx( p.x, p.y )];
        if( vision_transparency_cache[i] == VISION_ADJUST_SOLID ) {
            transparency_cache[map_cache.idx( p.x, p.y )] = LIGHT_TRANSPARENCY_SOLID;
        } else if( vision_transparency_cache[i] == VISION_ADJUST_HIDDEN ) {

            if( std::ranges::find( four_diagonal_offsets,
                                   adjacent ) == four_diagonal_offsets.end() ) {
                debugmsg( "Hidden tile not on a diagonal" );
                continue;
            }

            bool &relevant_blocked =
                adjacent == point_north_east ? blocked_data[center.x * sy + center.y].ne :
                adjacent == point_south_east ? blocked_data[p.x * sy + p.y].nw :
                adjacent == point_south_west ? blocked_data[p.x * sy + p.y].ne :
                /* point_north_west */         blocked_data[center.x * sy + center.y].nw;

            //We only set the restore cache if we actually flip the bit
            blocked_restore_cache[i] = !relevant_blocked;

            relevant_blocked = true;
        }
        i++;
    }
    vision_restore_cache[8] = transparency_cache[map_cache.idx( center.x, center.y )];
}

void map::restore_vision_transparency_cache( const tripoint &center, int target_z,
        float ( &vision_restore_cache )[9], bool ( &blocked_restore_cache )[8] )
{
    auto &map_cache = get_cache( target_z );
    auto &transparency_cache = map_cache.transparency_cache;
    auto *blocked_data = map_cache.vehicle_obscured_cache.data();
    const int sy = map_cache.cache_y;

    int i = 0;
    for( point adjacent : eight_adjacent_offsets ) {
        const tripoint p = center + adjacent;
        if( !inbounds( p ) ) {
            continue;
        }
        transparency_cache[map_cache.idx( p.x, p.y )] = vision_restore_cache[i];

        if( blocked_restore_cache[i] ) {
            bool &relevant_blocked =
                adjacent == point_north_east ? blocked_data[center.x * sy + center.y].ne :
                adjacent == point_south_east ? blocked_data[p.x * sy + p.y].nw :
                adjacent == point_south_west ? blocked_data[p.x * sy + p.y].ne :
                /* point_north_west */         blocked_data[center.x * sy + center.y].nw;
            relevant_blocked = false;
        }

        i++;
    }
    transparency_cache[map_cache.idx( center.x, center.y )] = vision_restore_cache[8];
}


// Sight model for build_seen_cache: Beer-Lambert attenuation with fast-path
// lookup table for open-air and weather-modified transparency.
static const light_model k_sight_model = {
    sight_calc, sight_check, update_light, nullptr, sight_from_lookup, accumulate_transparency
};

/**
 * Calculates the Field Of View for the provided map from the given x, y
 * coordinates. Returns a lightmap for a result where the values represent a
 * percentage of fully lit.
 *
 * A value equal to or below 0 means that cell is not in the
 * field of view, whereas a value equal to or above 1 means that cell is
 * in the field of view.
 *
 * @param origin the starting location
 * @param target_z Z-level to draw light map on
 */
void map::build_seen_cache( const tripoint &origin, const int target_z )
{
    ZoneScopedN( "build_seen_cache" );
    if( !inbounds( origin ) ) {
        return;
    }
    auto &target_cache = get_cache( target_z );

    constexpr float light_transparency_solid = LIGHT_TRANSPARENCY_SOLID;

    std::fill( target_cache.camera_cache.begin(), target_cache.camera_cache.end(),
               light_transparency_solid );

    float vision_restore_cache [9] = {0};
    bool blocked_restore_cache[8] = {false};

    if( origin.z == target_z ) {
        apply_vision_transparency_cache( get_player_character().pos(), target_z, vision_restore_cache,
                                         blocked_restore_cache );
    }

    if( !fov_3d ) {
        ZoneScopedN( "build_seen_cache_2d" );
        std::vector<int> levels_to_build;
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
            auto &cur_cache = get_cache( z );
            if( z == target_z || cur_cache.seen_cache_dirty ) {
                std::fill( cur_cache.seen_cache.begin(), cur_cache.seen_cache.end(),
                           light_transparency_solid );
                std::fill( cur_cache.camera_cache.begin(), cur_cache.camera_cache.end(),
                           light_transparency_solid );
                cur_cache.seen_cache_dirty = false;
                levels_to_build.push_back( z );
            }
        }

        for( const int level : levels_to_build ) {
            auto &lc = get_cache( level );
            lc.seen_cache[lc.idx( origin.x, origin.y )] = VISIBILITY_FULL;
            castLightAll( lc.seen_cache.data(), lc.transparency_cache.data(),
                          lc.vehicle_obscured_cache.data(), lc.cache_x, lc.cache_y,
                          origin.xy(), 0, VISIBILITY_FULL, k_sight_model, &weather_lookup_ );
        }

        // Floor-blocking pass for non-origin z-levels: accumulate floor_cache going
        // down then up from origin.z (same vert_blocked sweep as the 3D fast path).
        // Without this, the independent 2D casts above let players see through floors
        // to caves below buildings, rooftops above, etc.
        {
            const auto &origin_lc = get_cache( origin.z );
            const int cache_sz = origin_lc.cache_x * origin_lc.cache_y;
            std::vector<char> vert_blocked( cache_sz, 0 );

            // Going down: floor_cache[z+1] blocks crossing from z+1 to z.
            std::ranges::fill( vert_blocked, 0 );
            for( int z = origin.z - 1; z >= -OVERMAP_DEPTH; --z ) {
                const auto &fc = get_cache( z + 1 ).floor_cache;
                std::ranges::transform( vert_blocked, fc, vert_blocked.begin(),
                                        []( char a, char b ) -> char { return a | b; } );
                auto &zc = get_cache( z );
                std::ranges::transform( zc.seen_cache, vert_blocked, zc.seen_cache.begin(),
                []( float s, char blocked ) -> float {
                    return blocked ? 0.0f : s;
                } );
            }

            // Going up: floor_cache[z] blocks crossing from z-1 to z.
            std::ranges::fill( vert_blocked, 0 );
            for( int z = origin.z + 1; z <= OVERMAP_HEIGHT; ++z ) {
                const auto &fc = get_cache( z ).floor_cache;
                std::ranges::transform( vert_blocked, fc, vert_blocked.begin(),
                                        []( char a, char b ) -> char { return a | b; } );
                auto &zc = get_cache( z );
                std::ranges::transform( zc.seen_cache, vert_blocked, zc.seen_cache.begin(),
                []( float s, char blocked ) -> float {
                    return blocked ? 0.0f : s;
                } );
            }
        }
    } else {
        ZoneScopedN( "build_seen_cache_3d" );
        // Cache per-z-level data pointers.
        array_of_grids_of<const float> transparency_caches;
        array_of_grids_of<float> seen_caches;
        array_of_grids_of<const char> floor_caches;
        array_of_grids_of<const char> vehicle_floor_caches;
        array_of_grids_of<const diagonal_blocks> blocked_caches;
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
            auto &cur_cache = get_cache( z );
            const int idx = z + OVERMAP_DEPTH;
            transparency_caches[idx] = { cur_cache.transparency_cache.data(), cur_cache.cache_x, cur_cache.cache_y };
            seen_caches[idx]         = { cur_cache.seen_cache.data(),         cur_cache.cache_x, cur_cache.cache_y };
            floor_caches[idx]         = { cur_cache.floor_cache.data(),         cur_cache.cache_x, cur_cache.cache_y };
            vehicle_floor_caches[idx] = { cur_cache.vehicle_floor_cache.data(), cur_cache.cache_x, cur_cache.cache_y };
            blocked_caches[idx] = { cur_cache.vehicle_obscured_cache.data(), cur_cache.cache_x, cur_cache.cache_y };
            std::fill( cur_cache.seen_cache.begin(), cur_cache.seen_cache.end(),
                       light_transparency_solid );
            cur_cache.seen_cache_dirty = false;
        }

        auto &origin_cache = get_cache( origin.z );

        if( fov_3d_occlusion ) {
            // Accurate path: cast_zlight computes proper 3D shadows across all octants.
            // It fully populates origin.z (delta.z == 0 octants) as well as off-levels.
            // Always set the origin tile so blind-spot fill can use it as origin_vis source
            // regardless of which target_z is currently being built.
            origin_cache.seen_cache[origin_cache.idx( origin.x, origin.y )] = VISIBILITY_FULL;
            cast_zlight( seen_caches, transparency_caches, floor_caches, blocked_caches,
                         origin, 0, 1.0f, k_sight_model );
        } else {
            // Fast path: single 2D cast at origin.z, projected to other levels below.
            // No cast_zlight; off-level tiles filled from the projected result.
            origin_cache.seen_cache[origin_cache.idx( origin.x, origin.y )] = VISIBILITY_FULL;
            castLightAll( origin_cache.seen_cache.data(), origin_cache.transparency_cache.data(),
                          origin_cache.vehicle_obscured_cache.data(), origin_cache.cache_x, origin_cache.cache_y,
                          origin.xy(), 0, VISIBILITY_FULL, k_sight_model, &weather_lookup_ );
        }

        // Fill off-level tiles from origin.z's seen_cache.
        //
        // fov_3d_occlusion=true:  cast_zlight filled non-blind-spot tiles; this pass
        //   fills steep-angle blind spots (sc==0) from the projected origin.z result,
        //   and validates cast_zlight-lit tiles via a per-level 2D cast + DDA check.
        //   The per-level cast uses the target z-level's own transparency, so walls
        //   on that level correctly trigger the DDA and produce proper 3D shadows.
        // fov_3d_occlusion=false: cast_zlight skipped; all off-level tiles filled by
        //   projecting origin.z visibility through the cumulative floor filter.
        //
        // vert_blocked[tile_idx] accumulates floor_cache OR across levels between
        // origin.z and the current z.  Non-zero means the vertical path is obstructed.
        // Accumulated cumulatively so each z-level costs one OR-sweep instead of k.
        {
            ZoneScopedN( "build_seen_cache_3d_fill" );

            // 3D DDA: walk the line from origin to (tx, ty, tz), returning false if any
            // intermediate tile is solid or a floor crosses the ray.
            // Only invoked for the fov_3d_occlusion=true path.
            const auto is_3d_clear = [&]( int tx, int ty, int tz ) -> bool {
                const float dx    = static_cast<float>( tx - origin.x );
                const float dy    = static_cast<float>( ty - origin.y );
                const float dz    = static_cast<float>( tz - origin.z );
                const float total = std::max( {
                    std::abs( dx ), std::abs( dy ),
                    std::abs( dz ) * Z_LEVEL_SCALE } );
                if( total < 1.0f )
                {
                    return true;
                }

                // Explicit z-boundary crossing check.
                // The discrete DDA loop can miss floor crossings at shallow angles:
                // lround(0.5) rounds up, keeping cz at origin.z so no transition is
                // detected for e.g. fdh=2, fdz=1.  Interpolate each crossing directly.
                //   Going down: crossing k separates z=(origin.z-k) from z=(origin.z-k-1),
                //               so check floor_cache at z=(origin.z-k).
                //   Going up:   crossing k separates z=(origin.z+k) from z=(origin.z+k+1),
                //               so check floor_cache at z=(origin.z+k+1).
                // The origin tile is exempted: the player stands on top of that floor.
                {
                    const int n_cross = static_cast<int>( std::abs( dz ) );
                    for( int k = 0; k < n_cross; ++k )
                    {
                        const float t  = ( static_cast<float>( k ) + 0.5f ) / std::abs( dz );
                        const int   fx = static_cast<int>( std::lround(
                                                               static_cast<float>( origin.x ) + t * dx ) );
                        const int   fy = static_cast<int>( std::lround(
                                                               static_cast<float>( origin.y ) + t * dy ) );
                        if( k == 0 && dz < 0.0f &&
                            fx == origin.x && fy == origin.y ) {
                            continue; // player's own floor; they stand on top of it
                        }
                        const int floor_z = ( dz < 0.0f )
                                            ? static_cast<int>( origin.z ) - k
                                            : static_cast<int>( origin.z ) + k + 1;
                        if( floor_z < -OVERMAP_DEPTH || floor_z > OVERMAP_HEIGHT ) {
                            continue;
                        }
                        const auto &fc = floor_caches[floor_z + OVERMAP_DEPTH];
                        if( fx >= 0 && fy >= 0 && fx < fc.sx && fy < fc.sy &&
                            fc.at( fx, fy ) ) {
                            return false;
                        }
                    }
                }

                const int   steps = static_cast<int>( total );
                const float sx    = dx / total;
                const float sy    = dy / total;
                const float sz    = dz / total;
                int ox = origin.x;
                int oy = origin.y;
                int oz = origin.z;
                for( int s = 1; s < steps; ++s )
                {
                    const int cx = static_cast<int>( std::lround( origin.x + s * sx ) );
                    const int cy = static_cast<int>( std::lround( origin.y + s * sy ) );
                    const int cz = static_cast<int>( std::lround( origin.z + s * sz ) );
                    if( cz < -OVERMAP_DEPTH || cz > OVERMAP_HEIGHT ) {
                        continue;
                    }
                    if( cx >= 0 && cy >= 0 ) {
                        if( oz != cz && oz > -OVERMAP_DEPTH && cz < OVERMAP_HEIGHT ) {
                            if( oz < cz ) {
                                const auto &fc = floor_caches[cz + OVERMAP_DEPTH];
                                if( cx < fc.sx && cy < fc.sy && fc.at( cx, cy ) ) {
                                    return false;
                                }
                            } else {
                                const auto &fc = floor_caches[oz + OVERMAP_DEPTH];
                                if( ox >= 0 && oy >= 0 && ox < fc.sx && oy < fc.sy &&
                                    fc.at( ox, oy ) ) {
                                    return false;
                                }
                            }
                        }
                        const auto &ic = transparency_caches[cz + OVERMAP_DEPTH];
                        if( cx < ic.sx && cy < ic.sy &&
                            ic.at( cx, cy ) <= LIGHT_TRANSPARENCY_SOLID ) {
                            return false;
                        }
                    }
                    ox = cx;
                    oy = cy;
                    oz = cz;
                }
                return true;
            };

            // Cheaper variant: checks only whether a floor intervenes on the oblique
            // path from origin to (tx, ty, tz).  Skips the transparency DDA because
            // cast_zlight already verified transparency when sc > 0.
            const auto floor_crossing_blocked = [&]( int tx, int ty, int tz ) -> bool {
                const float dx      = static_cast<float>( tx - origin.x );
                const float dy      = static_cast<float>( ty - origin.y );
                const float dz      = static_cast<float>( tz - origin.z );
                const int   n_cross = static_cast<int>( std::abs( dz ) );
                for( int k = 0; k < n_cross; ++k )
                {
                    const float t  = ( static_cast<float>( k ) + 0.5f ) / std::abs( dz );
                    const int   fx = static_cast<int>( std::lround(
                                                           static_cast<float>( origin.x ) + t * dx ) );
                    const int   fy = static_cast<int>( std::lround(
                                                           static_cast<float>( origin.y ) + t * dy ) );
                    if( k == 0 && dz < 0.0f &&
                        fx == origin.x && fy == origin.y ) {
                        continue; // player's own floor
                    }
                    const int floor_z = ( dz < 0.0f )
                                        ? static_cast<int>( origin.z ) - k
                                        : static_cast<int>( origin.z ) + k + 1;
                    if( floor_z < -OVERMAP_DEPTH || floor_z > OVERMAP_HEIGHT ) {
                        continue;
                    }
                    const auto &fc  = floor_caches[floor_z + OVERMAP_DEPTH];
                    const auto &vfc = vehicle_floor_caches[floor_z + OVERMAP_DEPTH];
                    if( fx >= 0 && fy >= 0 && fx < fc.sx && fy < fc.sy &&
                        fc.at( fx, fy ) && !vfc.at( fx, fy ) ) {
                        return true;
                    }
                }
                return false;
            };

            const float *const origin_seen = origin_cache.seen_cache.data();
            const int cache_sz = origin_cache.cache_x * origin_cache.cache_y;

            // Accurate path only: 2D cast at the target level used to gate blind-spot fill.
            // Prevents the pyramid artifact by excluding tiles unreachable at their own level.
            std::vector<float> temp_seen;
            if( fov_3d_occlusion ) {
                temp_seen.resize( cache_sz );
            }

            // Per-tile vertical obstruction mask, accumulated cumulatively per direction.
            std::vector<char> vert_blocked( cache_sz );

            // Process one z-level using the current vert_blocked state.
            const auto process_z_level = [&]( int z ) {
                auto &zc = get_cache( z );

                // Accurate path: 2D cast at the target level gates both the DDA check
                // and the blind-spot fill; only tiles reachable at their own level are kept.
                if( fov_3d_occlusion ) {
                    std::fill( temp_seen.begin(), temp_seen.end(), light_transparency_solid );
                    temp_seen[zc.idx( origin.x, origin.y )] = VISIBILITY_FULL;
                    castLightAll( temp_seen.data(), zc.transparency_cache.data(),
                                  zc.vehicle_obscured_cache.data(), zc.cache_x, zc.cache_y,
                                  origin.xy(), 0, VISIBILITY_FULL, k_sight_model, &weather_lookup_ );
                }

                for( int x = 0; x < zc.cache_x; ++x ) {
                    for( int y = 0; y < zc.cache_y; ++y ) {
                        const int tile_idx = zc.idx( x, y );
                        float    &sc       = zc.seen_cache[tile_idx];
                        if( sc > 0.0f ) {
                            // cast_zlight lit this tile; validate to correct octant leaks.
                            if( !fov_3d_occlusion ) {
                                continue; // fast path: trust cast_zlight
                            }
                            if( temp_seen[tile_idx] > 0.0f ) {
                                // Horizontal path confirmed by 2D cast; transparency confirmed
                                // by cast_zlight.  Only a floor on the oblique path can block.
                                if( floor_crossing_blocked( x, y, z ) ) {
                                    sc = 0.0f;
                                }
                                continue;
                            }
                            // temp_seen == 0: possible octant leak; full DDA to verify.
                            if( !is_3d_clear( x, y, z ) ) {
                                sc = 0.0f;
                            }
                            continue;
                        }
                        // Blind spot (accurate) or all tiles (fast): fill from origin.z
                        // projection when the vertical path is clear.
                        // Accurate path: restrict to tiles geometrically unreachable by
                        // cast_zlight (dz * Z_LEVEL_SCALE > max(|dx|,|dy|)), then verify
                        // via DDA to block paths shadowed by walls at intermediate levels.
                        //
                        // The threshold `fdz * Z_LEVEL_SCALE > fdh` is the exact condition
                        // under which is_3d_clear's DDA is dominated by the vertical component
                        // (i.e. total = |dz|*Z_LEVEL_SCALE in max(|dx|,|dy|,|dz|*Z_LEVEL_SCALE)).
                        // Both share the same Z_LEVEL_SCALE constant; if that constant or the
                        // DDA distance formula changes, this threshold must be updated to match.
                        const float origin_vis = origin_seen[tile_idx];
                        if( !vert_blocked[tile_idx] && origin_vis > 0.0f ) {
                            if( fov_3d_occlusion ) {
                                const float fdz = static_cast<float>( std::abs( z - origin.z ) );
                                const float fdh = static_cast<float>(
                                                      std::max( std::abs( x - origin.x ),
                                                                std::abs( y - origin.y ) ) );
                                if( fdz * Z_LEVEL_SCALE > fdh && is_3d_clear( x, y, z ) ) {
                                    sc = origin_vis;
                                }
                            } else {
                                sc = origin_vis;
                            }
                        }
                    }
                }

                // Accurate path: close cast_zlight octant-seam notches on the south
                // and east faces of shadows.  These are the only two faces affected
                // by the octant sweep asymmetry.  A dark tile whose south (y+1) or
                // east (x+1) neighbour is lit, and whose oblique floor-crossing path
                // is clear, is a seam artefact and should be lit.
                //
                // Reading neighbours from a snapshot of seen_cache (overwriting the
                // now-unused temp_seen buffer) prevents cascade: a tile filled in
                // this pass cannot itself become a neighbour source for other tiles
                // in the same pass.
                if( fov_3d_occlusion ) {
                    std::ranges::copy( zc.seen_cache, temp_seen.begin() );
                    for( int x = 1; x < zc.cache_x - 1; ++x ) {
                        for( int y = 1; y < zc.cache_y - 1; ++y ) {
                            const int tile_idx = zc.idx( x, y );
                            if( temp_seen[tile_idx] > 0.0f || vert_blocked[tile_idx] ) {
                                continue;
                            }
                            if( origin_seen[tile_idx] <= 0.0f ) {
                                continue;
                            }
                            const float best = std::ranges::max( {
                                temp_seen[zc.idx( x,     y + 1 )],   // south
                                temp_seen[zc.idx( x + 1, y )],       // east
                                temp_seen[zc.idx( x,     y - 1 )],   // north
                                temp_seen[zc.idx( x - 1, y )] },     // west
                            std::less<float> {} );
                            if( best > 0.0f && !floor_crossing_blocked( x, y, z ) ) {
                                zc.seen_cache[tile_idx] = best;
                            }
                        }
                    }
                }

                // Fast path: one-ring neighbor propagation for tiles adjacent to a
                // directly-projected tile. Handles wall faces visible laterally through
                // a gap when the wall itself sits under a solid floor above it.
                if( !fov_3d_occlusion ) {
                    static constexpr std::array<std::pair<int, int>, 4> k_dirs = {{
                            { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
                        }
                    };
                    for( int x = 0; x < zc.cache_x; ++x ) {
                        for( int y = 0; y < zc.cache_y; ++y ) {
                            const int tile_idx = zc.idx( x, y );
                            if( zc.seen_cache[tile_idx] > 0.0f ) {
                                continue;
                            }
                            const float best = std::ranges::fold_left( k_dirs, 0.0f,
                            [&]( float acc, const std::pair<int, int> &d ) -> float {
                                const int nx = x + d.first;
                                const int ny = y + d.second;
                                if( nx < 0 || ny < 0 ||
                                    nx >= zc.cache_x || ny >= zc.cache_y )
                                {
                                    return acc;
                                }
                                const int nidx = zc.idx( nx, ny );
                                if( !vert_blocked[nidx] && origin_seen[nidx] > acc )
                                {
                                    return origin_seen[nidx];
                                }
                                return acc;
                            } );
                            if( best > 0.0f ) {
                                zc.seen_cache[tile_idx] = best;
                            }
                        }
                    }
                }
            };

            const int z_lo = std::max( -OVERMAP_DEPTH, origin.z - fov_3d_z_range );
            const int z_hi = std::min( OVERMAP_HEIGHT, origin.z + fov_3d_z_range );

            // Going down: crossing from z=k to z=k-1 is blocked by floor_cache[k].
            // Accumulate one level at a time so each step is a single OR-sweep.
            std::fill( vert_blocked.begin(), vert_blocked.end(), 0 );
            for( int z = origin.z - 1; z >= z_lo; --z ) {
                const auto &fc = get_cache( z + 1 ).floor_cache;
                std::ranges::transform( vert_blocked, fc, vert_blocked.begin(),
                                        []( char a, char b ) -> char { return a | b; } );
                process_z_level( z );
            }

            // Fixed vehicle-roof shadow stamp: when viewed from above, zero out
            // seen_cache at any tile directly beneath a vehicle roof.  This gives a
            // shadow footprint glued to the vehicle (no mirror-position artifact)
            // at the cost of perspective accuracy.
            for( int z = origin.z - 1; z >= z_lo; --z ) {
                const auto &vfc = vehicle_floor_caches[z + 1 + OVERMAP_DEPTH];
                const auto  sc  = seen_caches[z + OVERMAP_DEPTH];
                const auto vfc_span = std::span( vfc.data, static_cast<size_t>( vfc.sx * vfc.sy ) );
                const auto  sc_span = std::span( sc.data,  static_cast<size_t>( sc.sx  * sc.sy ) );
                std::ranges::transform( sc_span, vfc_span, sc_span.begin(),
                                        []( float s, bool v ) -> float { return v ? 0.0f : s; } );
            }

            // Going up: crossing from z=k-1 to z=k is blocked by floor_cache[k].
            std::fill( vert_blocked.begin(), vert_blocked.end(), 0 );
            for( int z = origin.z + 1; z <= z_hi; ++z ) {
                const auto &fc = get_cache( z ).floor_cache;
                std::ranges::transform( vert_blocked, fc, vert_blocked.begin(),
                                        []( char a, char b ) -> char { return a | b; } );
                process_z_level( z );
            }
        }
    }

    if( origin.z == target_z ) {
        restore_vision_transparency_cache( get_player_character().pos(), target_z, vision_restore_cache,
                                           blocked_restore_cache );
    }

    apply_vehicle_optics( origin, target_z );
}

void map::apply_vehicle_optics( const tripoint &origin, const int target_z )
{
    ZoneScopedN( "apply_vehicle_optics" );
    const optional_vpart_position vp = veh_at( origin );
    if( !vp ) {
        return;
    }
    vehicle *const veh = &vp->vehicle();

    auto &target_cache = get_cache( target_z );

    // We're inside a vehicle. Do mirror calculations.
    std::vector<int> mirrors;
    // Do all the sight checks first to prevent fake multiple reflection
    // from happening due to mirrors becoming visible due to processing order.
    // Cameras are also handled here, so that we only need to get through all vehicle parts once.
    int cam_control = -1;
    for( const vpart_reference &vp : veh->get_avail_parts( VPFLAG_EXTENDS_VISION ) ) {
        const tripoint mirror_pos = vp.pos();
        // We can utilize the current state of the seen cache to determine
        // if the player can see the mirror from their position.
        // Use g_visible_threshold for consistency with apparent_light_helper.
        if( !vp.info().has_flag( "CAMERA" ) &&
            target_cache.seen_cache[target_cache.idx( mirror_pos.x,
                                                      mirror_pos.y )] < LIGHT_TRANSPARENCY_SOLID + g_visible_threshold ) {
            continue;
        } else if( !vp.info().has_flag( "CAMERA_CONTROL" ) ) {
            mirrors.emplace_back( static_cast<int>( vp.part_index() ) );
        } else {
            if( square_dist( origin, mirror_pos ) <= 1 && veh->camera_on ) {
                cam_control = static_cast<int>( vp.part_index() );
            }
        }
    }

    for( const int mirror : mirrors ) {
        const bool is_camera = veh->part_info( mirror ).has_flag( "CAMERA" );
        if( is_camera && cam_control < 0 ) {
            continue; // Player not at camera control, so cameras don't work.
        }

        const tripoint mirror_pos = veh->global_part_pos3( mirror );

        // Determine how far the light has already traveled so mirrors
        // don't cheat the light distance falloff.
        int offset_distance;
        if( !is_camera ) {
            offset_distance = rl_dist( origin, mirror_pos );
        } else {
            offset_distance = g_max_view_distance - veh->part_info( mirror ).bonus *
                              veh->part( mirror ).hp() / veh->part_info( mirror ).durability;
            target_cache.camera_cache[target_cache.idx( mirror_pos.x,
                                                        mirror_pos.y )] = LIGHT_TRANSPARENCY_OPEN_AIR;
        }

        // TODO: Factor in the mirror facing and only cast in the
        // directions the player's line of sight reflects to.
        //
        // The naive solution of making the mirrors act like a second player
        // at an offset appears to give reasonable results though.
        castLightAll( target_cache.camera_cache.data(), target_cache.transparency_cache.data(),
                      target_cache.vehicle_obscured_cache.data(),
                      target_cache.cache_x, target_cache.cache_y,
                      mirror_pos.xy(), offset_distance, VISIBILITY_FULL,
                      k_sight_model, &weather_lookup_ );
    }
}

//Schraudolph's algorithm with John's constants
static inline
float fastexp( float x )
{
    union {
        float f;
        int i;
    } u, v;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
    u.i = static_cast<long long>( 6051102 * x + 1056478197 );
    v.i = static_cast<long long>( 1056478197 - 6051102 * x );
#pragma GCC diagnostic pop
    return u.f / v.f;
}

static float light_calc( const float &numerator, const float &transparency,
                         const int &distance )
{
    // Light needs inverse square falloff in addition to attenuation.
    return numerator  / ( fastexp( transparency * distance ) * distance );
}

static bool light_check( const float &transparency, const float &intensity )
{
    return transparency > LIGHT_TRANSPARENCY_SOLID && intensity > LIGHT_AMBIENT_LOW;
}

static float light_from_lookup( const float &numerator, const float &transparency,
                                const int &distance )
{
    return numerator *  transparency  / distance ;
}

// Light model for apply_light_source / apply_directional_light.
// Uses fastexp + inverse-square falloff; lookup_calc provides the matching fast
// path for the common open-air / weather transparency cases.
static const light_model k_light_model = {
    light_calc, light_check, nullptr, update_light_quadrants, light_from_lookup,
    accumulate_transparency
};

void map::apply_light_source( const tripoint &p, float luminance )
{
    auto &cache = get_cache( p.z );
    auto *lm_data        = cache.lm.data();
    auto *sm_data        = cache.sm.data();
    auto *trans_data     = cache.transparency_cache.data();
    auto *lsb_data       = cache.light_source_buffer.data();
    auto *blocked_data   = cache.vehicle_obscured_cache.data();
    const int sx = cache.cache_x;
    const int sy = cache.cache_y;

    const point p2( p.xy() );

    if( inbounds( p ) ) {
        const float min_light = std::max( static_cast<float>( lit_level::LOW ), luminance );
        lm_data[p2.x * sy + p2.y] = elementwise_max( lm_data[p2.x * sy + p2.y], min_light );
        sm_data[p2.x * sy + p2.y] = std::max( sm_data[p2.x * sy + p2.y], luminance );
    }
    if( luminance <= lit_level::LOW ) {
        return;
    } else if( luminance <= lit_level::BRIGHT_ONLY ) {
        luminance = 1.49f;
    }

    /* If we're a 5 luminance fire , we skip casting rays into ey && sx if we have
         neighboring fires to the north and west that were applied via light_source_buffer
       If there's a 1 luminance candle east in buffer, we still cast rays into ex since it's smaller
       If there's a 100 luminance magnesium flare south added via apply_light_source instead od
         add_light_source, it's unbuffered so we'll still cast rays into sy.

          ey
        nnnNnnn
        w     e
        w  5 +e
     sx W 5*1+E ex
        w ++++e
        w+++++e
        sssSsss
           sy
    */
    bool north = ( p2.y != 0       && lsb_data[p2.x * sy + p2.y - 1]       < luminance );
    bool south = ( p2.y != sy - 1  && lsb_data[p2.x * sy + p2.y + 1]       < luminance );
    bool east  = ( p2.x != sx - 1  && lsb_data[( p2.x + 1 ) * sy + p2.y]   < luminance );
    bool west  = ( p2.x != 0       && lsb_data[( p2.x - 1 ) * sy + p2.y]   < luminance );

    // Build octant mask from the directions that have a weaker-or-absent neighbor
    // in the light-source buffer.  Skipping covered directions is an optimization
    // for dense fire / lava: equal-brightness neighbors already project those rays.
    auto mask = uint8_t{};
    if( north ) {
        mask |= OCTANT_NORTH;
    }
    if( east ) {
        mask |= OCTANT_EAST;
    }
    if( south ) {
        mask |= OCTANT_SOUTH;
    }
    if( west ) {
        mask |= OCTANT_WEST;
    }
    if( mask != 0 ) {
        castLightOctants_q( lm_data, trans_data, blocked_data, sx, sy, p2, 0, luminance,
                            k_light_model, mask, &weather_lookup_ );
    }
}

void map::apply_directional_light( const tripoint &p, int direction, float luminance )
{
    const point p2( p.xy() );

    auto &cache = get_cache( p.z );
    auto *lm_data      = cache.lm.data();
    auto *trans_data   = cache.transparency_cache.data();
    auto *blocked_data = cache.vehicle_obscured_cache.data();
    const int sx = cache.cache_x;
    const int sy = cache.cache_y;

    // direction convention: 90=north-facing (light goes south), 0=east-facing (west),
    // 270=south-facing (north), 180=west-facing (east).  Each maps to the two octants
    // covering the relevant half-space in k_octant_xforms.
    auto mask = uint8_t{};
    if( direction == 90 ) {
        mask = OCTANT_NORTH;
    } else if( direction == 0 ) {
        mask = OCTANT_EAST;
    } else if( direction == 270 ) {
        mask = OCTANT_SOUTH;
    } else if( direction == 180 ) {
        mask = OCTANT_WEST;
    }
    if( mask != 0 ) {
        castLightOctants_q( lm_data, trans_data, blocked_data, sx, sy, p2, 0, luminance,
                            k_light_model, mask, &weather_lookup_ );
    }
}

void map::apply_light_arc( const tripoint &p, units::angle angle, float luminance,
                           units::angle wideangle )
{
    if( luminance <= LIGHT_SOURCE_LOCAL ) {
        return;
    }

    const auto &arc_cache = get_cache( p.z );
    auto lit = std::vector<bool>( static_cast<size_t>( arc_cache.cache_x ) * arc_cache.cache_y,
                                  false );

    apply_light_source( p, LIGHT_SOURCE_LOCAL );

    // Normalize (should work with negative values too)
    const units::angle wangle = wideangle / 2.0;

    units::angle nangle = fmod( angle, 360_degrees );

    tripoint end;
    int range = LIGHT_RANGE( luminance );
    calc_ray_end( nangle, range, p, end );
    apply_light_ray( lit, p, end, luminance );

    tripoint test;
    calc_ray_end( wangle + nangle, range, p, test );

    const float wdist = hypot( end.x - test.x, end.y - test.y );
    if( wdist <= 0.5 ) {
        return;
    }

    // attempt to determine beam intensity required to cover all squares
    const units::angle wstep = ( wangle / ( wdist * M_SQRT2 ) );

    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter)
    for( units::angle ao = wstep; ao <= wangle; ao += wstep ) {
        if( trigdist ) {
            double fdist = ( ao * M_PI_2 ) / wangle;
            end.x = static_cast<int>(
                        p.x + ( static_cast<double>( range ) - fdist * 2.0 ) * cos( nangle + ao ) );
            end.y = static_cast<int>(
                        p.y + ( static_cast<double>( range ) - fdist * 2.0 ) * sin( nangle + ao ) );
            apply_light_ray( lit, p, end, luminance );

            end.x = static_cast<int>(
                        p.x + ( static_cast<double>( range ) - fdist * 2.0 ) * cos( nangle - ao ) );
            end.y = static_cast<int>(
                        p.y + ( static_cast<double>( range ) - fdist * 2.0 ) * sin( nangle - ao ) );
            apply_light_ray( lit, p, end, luminance );
        } else {
            calc_ray_end( nangle + ao, range, p, end );
            apply_light_ray( lit, p, end, luminance );
            calc_ray_end( nangle - ao, range, p, end );
            apply_light_ray( lit, p, end, luminance );
        }
    }
}

// Local helper for apply_light_ray — maps a direction sign pair to the quadrant
// that is the source of that direction.  Assumes x != 0 && y != 0.
// NOLINTNEXTLINE(cata-xy)
static constexpr quadrant quadrant_from_x_y( int x, int y )
{
    return ( x > 0 ) ?
           ( ( y > 0 ) ? quadrant::NW : quadrant::SW ) :
           ( ( y > 0 ) ? quadrant::NE : quadrant::SE );
}

void map::apply_light_ray( std::vector<bool> &lit,
                           const tripoint &s, const tripoint &e, float luminance )
{
    point a( std::abs( e.x - s.x ) * 2, std::abs( e.y - s.y ) * 2 );
    point d( ( s.x < e.x ) ? 1 : -1, ( s.y < e.y ) ? 1 : -1 );
    point p( s.xy() );

    quadrant quad = quadrant_from_x_y( d.x, d.y );

    // TODO: Invert that z comparison when it's sane
    if( s.z != e.z || ( s.x == e.x && s.y == e.y ) ) {
        return;
    }

    auto &cache_ref = get_cache( s.z );
    auto *lm_data          = cache_ref.lm.data();
    auto *trans_data       = cache_ref.transparency_cache.data();
    const int sx = cache_ref.cache_x;
    const int sy = cache_ref.cache_y;

    float distance = 1.0;
    float transparency = LIGHT_TRANSPARENCY_OPEN_AIR;
    const float scaling_factor = static_cast<float>( rl_dist( s, e ) ) /
                                 static_cast<float>( square_dist( s, e ) );
    // TODO: [lightmap] Pull out the common code here rather than duplication
    if( a.x > a.y ) {
        int t = a.y - ( a.x / 2 );
        do {
            if( t >= 0 ) {
                p.y += d.y;
                t -= a.x;
            }

            p.x += d.x;
            t += a.y;

            // TODO: clamp coordinates to map bounds before this method is called.
            if( p.x >= 0 && p.y >= 0 && p.x < sx && p.y < sy ) {
                const int idx = p.x * sy + p.y;
                float current_transparency = trans_data[idx];
                bool is_opaque = ( current_transparency == LIGHT_TRANSPARENCY_SOLID );
                if( !lit[idx] ) {
                    // Multiple rays will pass through the same squares so we need to record that
                    lit[idx] = true;
                    float lm_val = luminance / ( fastexp( transparency * distance ) * distance );
                    quadrant q = is_opaque ? quad : quadrant::default_;
                    lm_data[idx][q] = std::max( lm_data[idx][q], lm_val );
                }
                if( is_opaque ) {
                    break;
                }
                // Cumulative average of the transparency values encountered.
                transparency = ( ( distance - 1.0 ) * transparency + current_transparency ) / distance;
            } else {
                break;
            }

            distance += scaling_factor;
        } while( !( p.x == e.x && p.y == e.y ) );
    } else {
        int t = a.x - ( a.y / 2 );
        do {
            if( t >= 0 ) {
                p.x += d.x;
                t -= a.y;
            }

            p.y += d.y;
            t += a.x;

            if( p.x >= 0 && p.y >= 0 && p.x < sx && p.y < sy ) {
                const int idx = p.x * sy + p.y;
                float current_transparency = trans_data[idx];
                bool is_opaque = ( current_transparency == LIGHT_TRANSPARENCY_SOLID );
                if( !lit[idx] ) {
                    // Multiple rays will pass through the same squares so we need to record that
                    lit[idx] = true;
                    float lm_val = luminance / ( fastexp( transparency * distance ) * distance );
                    quadrant q = is_opaque ? quad : quadrant::default_;
                    lm_data[idx][q] = std::max( lm_data[idx][q], lm_val );
                }
                if( is_opaque ) {
                    break;
                }
                // Cumulative average of the transparency values encountered.
                transparency = ( ( distance - 1.0 ) * transparency + current_transparency ) / distance;
            } else {
                break;
            }

            distance += scaling_factor;
        } while( !( p.x == e.x && p.y == e.y ) );
    }
}
