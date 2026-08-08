#include "mapbuffer.h"

#include <algorithm>
#include "pathfinding.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "avatar.h"
#include "sounds.h"
#include "batch_turns.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "creature.h"
#include "debug.h"
#include "emit.h"
#include "detached_ptr.h"
#include "distribution_grid.h"
#include "filesystem.h"
#include "field_type.h"
#include "fluid_grid.h"
#include "flag.h"
#include "fstream_utils.h"
#include "fungal_effects.h"
#include "game.h"
#include "game_constants.h"
#include "harvest.h"
#include "iexamine.h"
#include "item.h"
#include "itype.h"
#include "iuse_actor.h"
#include "json.h"
#include "map.h"
#include "mapdata.h"
#include "mapgen_constructor.h"
#include "map/utils/map_functions.h"
#include "map_iterator.h"
#include "map_mutation_hooks.h"
#include "omdata.h"
#include "item_category.h"
#include "item_group.h"
#include "monster.h"
#include "npc.h"
#include "messages.h"
#include "mongroup.h"
#include "mtype.h"
#include "options.h"
#include "overmapbuffer.h"
#include "output.h"
#include "popup.h"
#include "profile.h"
#include "rng.h"
#include "rot.h"
#include "shadowcasting.h"
#include "skill.h"
#include "string_formatter.h"
#include "submap.h"
#include "submap_load_manager.h"
#include "thread_pool.h"
#include "timed_event.h"
#include "translations.h"
#include "trap.h"
#include "ui_manager.h"
#include "units_mass.h"
#include "units_utility.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_move.h"
#include "vehicle_group.h"
#include "vehicle_part.h"
#include "vpart_range.h"
#include "weather.h"
#include "world.h"

namespace
{

struct mapbuffer_tile_lookup {
    submap *sm = nullptr;
    point_sm_ms local;
};

struct actualize_tile_options {
    mapbuffer &buffer;
    submap &sm;
    point_sm_ms local;
    tripoint_abs_ms abs_pos;
    std::optional<tripoint_bub_ms> active_bubble_pos;
    time_point last_touched;
    time_duration elapsed;
    mapbuffer_lookup_options lookup;
};

struct add_submap_spawn_options {
    submap &sm;
    point_sm_ms local;
    mtype_id type;
    spawn_disposition disposition;
};

auto calculate_vehicle_submap_footprints( const vehicle &veh, const tripoint_abs_ms &base,
        const std::size_t precalc_index ) -> vehicle_submap_footprints
{
    auto footprints = vehicle_submap_footprints {};
    for( const vpart_reference &vpr : veh.get_all_parts() ) {
        if( vpr.part().removed ) {
            continue;
        }
        const auto &part = vpr.part();
        const auto part_pos = base + tripoint_rel_ms( part.precalc[precalc_index],
                              part.mount.z() + part.z_terrain[precalc_index] );
        const auto part_sm = project_to<coords::sm>( part_pos );
        if( part_sm.z() < -OVERMAP_DEPTH || part_sm.z() > OVERMAP_HEIGHT ) {
            continue;
        }
        auto &footprint = footprints[part_sm.z() + OVERMAP_DEPTH];
        if( !footprint ) {
            footprint = vehicle_submap_footprint { .min = part_sm, .max = part_sm };
            continue;
        }
        footprint->min.x() = std::min( footprint->min.x(), part_sm.x() );
        footprint->min.y() = std::min( footprint->min.y(), part_sm.y() );
        footprint->max.x() = std::max( footprint->max.x(), part_sm.x() );
        footprint->max.y() = std::max( footprint->max.y(), part_sm.y() );
    }
    return footprints;
}

auto calculate_vehicle_submap_footprints( const vehicle &veh ) -> vehicle_submap_footprints
{
    return calculate_vehicle_submap_footprints( veh, veh.abs_ms_location(), 0 );
}

auto vehicle_submap_footprint_overlaps_active_bubble( const mapbuffer &buffer,
        const vehicle_submap_footprint &footprint ) -> bool
{
    if( g == nullptr || g->m.get_bound_dimension() != buffer.get_dimension_id() ) {
        return false;
    }

    const auto bubble_min = g->m.get_abs_sub();
    const auto bubble_max = bubble_min + point_rel_sm( g->m.getmapsize() - 1,
                            g->m.getmapsize() - 1 );
    return footprint.max.x() >= bubble_min.x() && footprint.min.x() <= bubble_max.x() &&
           footprint.max.y() >= bubble_min.y() && footprint.min.y() <= bubble_max.y();
}

auto invalidate_active_vehicle_footprints( const mapbuffer &buffer,
        const vehicle_submap_footprints &footprints ) -> bool
{
    if( g == nullptr || g->m.get_bound_dimension() != buffer.get_dimension_id() ) {
        return false;
    }

    bool invalidated = false;
    for( const auto &footprint : footprints ) {
        if( !footprint || !vehicle_submap_footprint_overlaps_active_bubble( buffer, *footprint ) ) {
            continue;
        }
        invalidated = true;
        g->m.on_vehicle_moved( abs_to_map_local( g->m, footprint->min ),
                               abs_to_map_local( g->m, footprint->max ), footprint->min.z() );
    }
    return invalidated;
}

static const auto fault_bionic_nonsterile = fault_id( "fault_bionic_nonsterile" );
static const std::string str_OPENCLOSE_INSIDE( "OPENCLOSE_INSIDE" );
static const auto itype_burnt_out_bionic = itype_id( "burnt_out_bionic" );

auto lookup_tile( mapbuffer &buffer, const tripoint_abs_ms &p,
                  const mapbuffer_lookup_options options ) -> std::optional<mapbuffer_tile_lookup>
{
    if( buffer.is_outside_pocket_dimension_bounds( p ) ) {
        return std::nullopt;
    }

    const auto split = project_remain<coords::sm>( p );
    auto *const sm = buffer.get_submap( split.quotient_tripoint, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }

    return mapbuffer_tile_lookup {
        .sm = sm,
        .local = split.remainder,
    };
}

auto ensure_roof_above_cache( mapbuffer &buffer, submap &sm,
                              const mapbuffer_lookup_options options ) -> void
{
    if( !sm.roof_above_dirty ) {
        return;
    }
    auto *const above = buffer.get_submap( sm.position() + tripoint_above, options );
    sm.rebuild_roof_above_cache( above );
}

auto uniform_terrain_for_omt( const dimension_id &dimension_id,
                              const tripoint_abs_omt &omt_addr ) -> std::optional<ter_id>
{
    static const oter_id rock( "empty_rock" );
    static const oter_id air( "open_air" );

    const auto terrain_type = get_overmapbuffer( dimension_id ).ter( omt_addr );
    if( terrain_type == air ) {
        return t_open_air;
    }
    if( terrain_type == rock ) {
        return t_rock;
    }
    return std::nullopt;
}

auto add_uniform_omt( mapbuffer &dest, const tripoint_abs_sm &base,
                      const ter_id &terrain_type ) -> bool
{
    static constexpr auto offsets = std::array{
        point_rel_sm::zero(),
        point_rel_sm::east(),
        point_rel_sm::south(),
        point_rel_sm::south_east()
    };

    auto added_any = false;
    std::ranges::for_each( offsets, [&]( const auto & offset ) {
        const auto pos = base + offset;
        auto sm = std::make_unique<submap>( pos, dest.get_dimension_id() );
        sm->is_uniform = true;
        sm->set_all_ter( terrain_type );
        sm->last_touched = calendar::turn;
        added_any |= dest.add_submap( pos, sm, { .skip_luminance_refresh = true } );
    } );
    return added_any;
}

auto tile_has_flag( const mapbuffer_tile_lookup &tile, const std::string &flag ) -> bool
{
    return tile.sm->get_ter( tile.local ).obj().has_flag( flag ) ||
           tile.sm->get_furn( tile.local ).obj().has_flag( flag );
}

auto trap_at_tile( const submap &sm, const point_sm_ms &local ) -> const trap &
{
    const auto terrain_trap = sm.get_ter( local ).obj().trap;
    if( terrain_trap != tr_null ) {
        return terrain_trap.obj();
    }

    return sm.get_trap( local ).obj();
}

auto temperature_flag_at_tile( const submap &sm, const point_sm_ms &local ) -> temperature_flag
{
    const auto &furn = sm.get_furn( local ).obj();
    return rot::temp::for_tile( {
        .root_cellar = sm.get_ter( local ) == t_rootcellar,
        .fridge = furn.has_flag( TFLAG_FRIDGE ),
        .freezer = furn.has_flag( TFLAG_FREEZER ),
    } );
}

auto add_item_to_actualized_tile( const actualize_tile_options &options,
                                  detached_ptr<item> &&obj ) -> void
{
    options.buffer.add_item_or_charges( options.abs_pos, std::move( obj ), {
        .lookup = options.lookup,
    } );
}

auto add_spawn_to_submap( const add_submap_spawn_options &options ) -> void
{
    if( !options.type || MonsterGroupManager::monster_is_blacklisted( options.type ) ) {
        return;
    }

    options.sm.spawns.emplace_back( options.type, 1, options.local, -1, -1, options.disposition );
}

auto handle_decayed_corpse( const actualize_tile_options &options, const item &corpse ) -> void
{
    const auto *const dead_monster = corpse.get_corpse_mon();
    if( dead_monster == nullptr ) {
        debugmsg( "Corpse at tripoint %s has no associated monster?!",
                  options.abs_pos.to_string() );
        return;
    }

    auto decayed_weight_grams = to_gram( dead_monster->weight );
    decayed_weight_grams *= rng_float( 0.5, 0.9 );

    for( const auto &entry : dead_monster->harvest.obj() ) {
        if( entry.type == "bionic" || entry.type == "bionic_group" || entry.type == "blood" ) {
            continue;
        }

        auto harvest = item::spawn( entry.drop, corpse.birthday() );
        const auto random_decay_modifier = rng_float( 0.0f, static_cast<float>( MAX_SKILL ) );
        const auto min_num = entry.scale_num.first * random_decay_modifier + entry.base_num.first;
        const auto max_num = entry.scale_num.second * random_decay_modifier + entry.base_num.second;
        auto roll = 0;
        if( entry.mass_ratio != 0.00f ) {
            roll = static_cast<int>( std::round( entry.mass_ratio * decayed_weight_grams ) );
            roll = std::ceil( static_cast<double>( roll ) / to_gram( harvest->weight() ) );
        } else {
            roll = std::min<int>( entry.max, std::round( rng_float( min_num, max_num ) ) );
        }
        if( roll <= 0 ) {
            continue;
        }

        for( const auto ignored : std::views::iota( 0, roll ) ) {
            ( void )ignored;
            if( harvest->charges > 1 ) {
                harvest->charges = 1;
            }
            if( !harvest->rotten() ) {
                add_item_to_actualized_tile( options, item::spawn( *harvest ) );
            }
        }
    }

    for( item *const comp : corpse.get_components() ) {
        if( comp->is_bionic() ) {
            if( !one_in( 10 ) ) {
                comp->convert( itype_burnt_out_bionic );
                if( comp->has_fault( fault_bionic_nonsterile ) ) {
                    comp->faults.erase( fault_bionic_nonsterile );
                }
            }
            add_item_to_actualized_tile( options, item::spawn( *comp ) );
            continue;
        }

        if( one_in( 10 ) ) {
            if( comp->has_fault( fault_bionic_nonsterile ) ) {
                comp->faults.erase( fault_bionic_nonsterile );
            }
            add_item_to_actualized_tile( options, item::spawn( *comp ) );
        }
    }
}

auto rotten_item_spawn( const actualize_tile_options &options, const item &source ) -> void
{
    if( options.buffer.creature_at( options.abs_pos ) != nullptr ) {
        return;
    }

    const auto &comestible = source.get_comestible();
    if( !comestible || !comestible->rot_spawn ) {
        return;
    }

    const auto chance = static_cast<int>( comestible->rot_spawn_chance *
                                          get_option<float>( "CARRION_SPAWNRATE" ) );
    if( rng( 0, 100 ) >= chance ) {
        return;
    }

    const auto spawn_details = MonsterGroupManager::GetResultFromGroup( comestible->rot_spawn );
    const auto disposition = source.has_own_flag( flag_SPAWN_FRIENDLY ) ?
                             spawn_disposition::SpawnDisp_Pet :
                             spawn_disposition::SpawnDisp_Default;
    add_spawn_to_submap( {
        .sm = options.sm,
        .local = options.local,
        .type = spawn_details.name,
        .disposition = disposition,
    } );

    if( !g->u.sees( options.abs_pos ) ) {
        return;
    }

    if( source.is_seed() ) {
        add_msg( m_warning, _( "Something has crawled out of the %s plants!" ),
                 source.get_plant_name() );
        return;
    }

    add_msg( m_warning, _( "Something has crawled out of the %s!" ), source.tname() );
}

auto remove_rotten_items( const actualize_tile_options &options,
                          location_vector<item> &items ) -> void
{
    auto decayed_corpses = std::vector<detached_ptr<item>> {};
    const auto temperature = temperature_flag_at_tile( options.sm, options.local );
    items.remove_with( [&]( detached_ptr<item> &&it ) {
        if( !it ) {
            debugmsg( "remove_rotten_items: null item pointer at %s", options.abs_pos.to_string() );
            return std::move( it );
        }
        if( !it->type ) {
            debugmsg( "remove_rotten_items: item with null type at %s", options.abs_pos.to_string() );
            return std::move( it );
        }
        const auto can_spawn_rot = it->is_comestible();
        const auto can_decay_corpse = it->is_corpse();
        auto removed_snapshot = can_spawn_rot || can_decay_corpse ?
                                item::spawn( *it ) : detached_ptr<item>();
        it = item::actualize_rot( std::move( it ), {
            .position = options.abs_pos,
            .temperature = temperature,
            .weather = &get_weather(),
            .local_temperature = options.sm.get_temperature(),
        } );
        if( !it ) {
            if( can_spawn_rot && removed_snapshot ) {
                rotten_item_spawn( options, *removed_snapshot );
            } else if( can_decay_corpse && removed_snapshot ) {
                decayed_corpses.push_back( std::move( removed_snapshot ) );
            }
        }
        return std::move( it );
    } );

    for( const auto &corpse : decayed_corpses ) {
        handle_decayed_corpse( options, *corpse );
    }
}

auto fill_funnels( const actualize_tile_options &options ) -> void
{
    const auto &tr = trap_at_tile( options.sm, options.local );
    if( !tr.is_funnel() ) {
        return;
    }

    const auto is_outside = options.buffer.is_outside( options.abs_pos, options.lookup );
    if( !is_outside ) {
        return;
    }

    auto &items = options.sm.get_items( options.local );
    auto maxvolume = 0_ml;
    auto biggest_container = items.end();
    for( auto candidate = items.begin(); candidate != items.end(); ++candidate ) {
        if( ( *candidate )->is_funnel_container( maxvolume ) ) {
            biggest_container = candidate;
        }
    }
    if( biggest_container != items.end() ) {
        retroactively_fill_from_funnel( **biggest_container, tr, options.last_touched, calendar::turn,
                                        options.abs_pos );
    }
}

auto remove_fertilizer( const actualize_tile_options &options,
                        location_vector<item> &items ) -> void
{
    const auto fertilizer = std::ranges::find_if( items, []( const item * const it ) {
        return it->has_flag( flag_FERTILIZER );
    } );
    if( fertilizer != items.end() ) {
        options.buffer.erase_item( options.abs_pos, {
            .it = fertilizer,
            .lookup = options.lookup,
        } );
    }
}

auto grow_plant( const actualize_tile_options &options ) -> void
{
    const auto &furn = options.sm.get_furn( options.local ).obj();
    if( !furn.has_flag( "PLANT" ) ) {
        return;
    }

    auto &items = options.sm.get_items( options.local );
    const auto seed_it = std::ranges::find_if( items, []( const item * const it ) {
        return it->is_seed();
    } );

    if( seed_it == items.end() ) {
        const auto overmap_terrain = get_overmapbuffer( options.buffer.get_dimension_id() ).ter(
                                         project_to<coords::omt>( options.abs_pos ) );
        debugmsg( "a planted item at %s (within overmap terrain %s) has no seed data",
                  options.abs_pos.to_string(), overmap_terrain.id().str() );
        options.buffer.clear_items( options.abs_pos, options.lookup );
        options.buffer.set_furn( options.abs_pos, f_null, options.lookup );
        return;
    }

    const auto *const seed = *seed_it;
    const auto plant_epoch = seed->get_plant_epoch();
    if( seed->age() < plant_epoch * furn.plant->growth_multiplier ||
        furn.has_flag( "GROWTH_HARVEST" ) ) {
        return;
    }

    if( seed->age() < plant_epoch * 2 ) {
        if( furn.has_flag( "GROWTH_SEEDLING" ) ) {
            return;
        }
        remove_fertilizer( options, items );
        rotten_item_spawn( options, *seed );
        options.buffer.set_furn( options.abs_pos, furn_str_id( furn.plant->transform ), options.lookup );
        return;
    }

    if( seed->age() < plant_epoch * 3 * furn.plant->growth_multiplier ) {
        if( furn.has_flag( "GROWTH_MATURE" ) ) {
            return;
        }
        remove_fertilizer( options, items );
        rotten_item_spawn( options, *seed );
        if( !furn.has_flag( "GROWTH_SEEDLING" ) ) {
            rotten_item_spawn( options, *seed );
        }
        options.buffer.set_furn( options.abs_pos, furn_str_id( furn.plant->transform ), options.lookup );
        return;
    }

    if( furn.has_flag( "GROWTH_SEEDLING" ) ) {
        rotten_item_spawn( options, *seed );
        rotten_item_spawn( options, *seed );
    } else if( furn.has_flag( "GROWTH_MATURE" ) ) {
        rotten_item_spawn( options, *seed );
    } else {
        rotten_item_spawn( options, *seed );
        rotten_item_spawn( options, *seed );
        rotten_item_spawn( options, *seed );
    }
    options.buffer.set_furn( options.abs_pos, furn_str_id( furn.plant->transform ), options.lookup );
}

auto restock_fruits( const actualize_tile_options &options ) -> void
{
    const auto &ter = options.sm.get_ter( options.local ).obj();
    const auto &furn = options.sm.get_furn( options.local ).obj();
    if( !ter.has_flag( TFLAG_HARVESTED ) && !furn.has_flag( TFLAG_HARVESTED ) ) {
        return;
    }

    const auto last_touched = calendar::turn - options.elapsed;
    if( season_of_year( calendar::turn ) == season_of_year( last_touched ) &&
        options.elapsed < calendar::season_length() ) {
        return;
    }

    if( ter.has_flag( TFLAG_HARVESTED ) ) {
        options.buffer.set_ter( options.abs_pos, ter.transforms_into, options.lookup );
    }
    if( furn.has_flag( TFLAG_HARVESTED ) ) {
        options.buffer.set_furn( options.abs_pos, furn.transforms_into, options.lookup );
    }
}

auto produce_sap( const actualize_tile_options &options ) -> void
{
    if( options.elapsed <= 0_turns || options.sm.get_ter( options.local ) != t_tree_maple_tapped ) {
        return;
    }

    static const auto maple_sap_per_season = 56;
    const auto producing_length = 0.75 * calendar::season_length();
    const auto turns_to_produce = producing_length / ( maple_sap_per_season * 4 );
    auto time_producing = 0_turns;

    if( options.elapsed >= calendar::year_length() ) {
        time_producing = producing_length;
    } else {
        const auto early_spring_end = 0.5f * calendar::season_length();
        const auto late_winter_start = 3.75f * calendar::season_length();
        const auto last_actualize = calendar::turn - options.elapsed;
        const auto last_actualize_tof = time_past_new_year( last_actualize );
        const auto last_producing = last_actualize_tof >= late_winter_start ||
                                    last_actualize_tof < early_spring_end;
        const auto current_tof = time_past_new_year( calendar::turn );
        const auto current_producing = current_tof >= late_winter_start ||
                                       current_tof < early_spring_end;
        const auto non_producing_length = 3.25 * calendar::season_length();

        if( last_producing && current_producing ) {
            time_producing = options.elapsed < non_producing_length ?
                             options.elapsed : options.elapsed - non_producing_length;
        } else if( !last_producing && !current_producing ) {
            if( options.elapsed > non_producing_length ) {
                time_producing = options.elapsed - non_producing_length;
            }
        } else if( last_producing && !current_producing ) {
            time_producing = last_actualize_tof < early_spring_end ?
                             early_spring_end - last_actualize_tof :
                             calendar::year_length() - last_actualize_tof + early_spring_end;
        } else if( !last_producing && current_producing ) {
            time_producing = current_tof >= late_winter_start ?
                             current_tof - late_winter_start :
                             0.25f * calendar::season_length() + current_tof;
        }
    }

    auto new_charges = roll_remainder( time_producing / turns_to_produce );
    if( new_charges <= 0 ) {
        return;
    }

    auto &items = options.sm.get_items( options.local );
    for( auto &it : items ) {
        if( !it->is_bucket() && !it->is_watertight_container() ) {
            continue;
        }

        auto sap = item::spawn( "maple_sap", calendar::turn );
        const auto capacity = it->get_remaining_capacity_for_liquid( *sap, true );
        if( capacity > 0 ) {
            new_charges = std::min( new_charges, capacity );
            sap->poison = one_in( 10 ) ? 1 : 0;
            sap->charges = new_charges;
            it->fill_with( std::move( sap ) );
        }
        break;
    }
}

auto rad_scorch( const actualize_tile_options &options ) -> void
{
    const auto rads = options.sm.get_radiation( options.local );
    if( rads == 0 ) {
        return;
    }

    if( !x_in_y( 1.0 * rads * rads * options.elapsed, 91_days ) ) {
        return;
    }

    const auto &furn = options.sm.get_furn( options.local ).obj();
    if( furn.has_flag( "PLANT" ) ) {
        options.buffer.clear_items( options.abs_pos, options.lookup );
        options.buffer.set_furn( options.abs_pos, f_null, options.lookup );
    }

    static const auto dies_into = std::map<ter_id, ter_str_id> {{
            {t_grass, ter_str_id( "t_dirt" )},
            {t_tree_young, ter_str_id( "t_dirt" )},
            {t_tree_pine, ter_str_id( "t_tree_deadpine" )},
            {t_tree_birch, ter_str_id( "t_tree_birch_harvested" )},
            {t_tree_willow, ter_str_id( "t_tree_willow_harvested" )},
            {t_tree_hickory, ter_str_id( "t_tree_hickory_dead" )},
            {t_tree_hickory_harvested, ter_str_id( "t_tree_hickory_dead" )},
        }
    };

    const auto tid = options.sm.get_ter( options.local );
    const auto iter = dies_into.find( tid );
    if( iter != dies_into.end() ) {
        options.buffer.set_ter( options.abs_pos, iter->second, options.lookup );
        return;
    }

    const auto &terrain = tid.obj();
    if( terrain.has_flag( "SHRUB" ) ) {
        options.buffer.set_ter( options.abs_pos, t_dirt, options.lookup );
    } else if( terrain.has_flag( "TREE" ) ) {
        options.buffer.set_ter( options.abs_pos, ter_str_id( "t_tree_dead" ), options.lookup );
    }
}

auto decay_cosmetic_fields( const actualize_tile_options &options ) -> void
{
    auto &fields = options.sm.get_field( options.local );
    for( auto &pr : fields ) {
        auto &fd = pr.second;
        const auto half_life = fd.get_field_type().obj().half_life;
        if( !fd.decays_on_actualize() || half_life <= 0_turns ) {
            continue;
        }

        const auto added_age = 2 * options.elapsed / rng( 2, 4 );
        fd.mod_field_age( added_age );
        const auto intensity_drop = fd.get_field_age() / half_life;
        if( intensity_drop > 0 ) {
            fd.set_field_intensity( fd.get_field_intensity() - intensity_drop );
            fd.mod_field_age( -half_life * intensity_drop );
        }
    }
}

auto tile_allows_item_despite_noitem_flag( const item &target,
        const mapbuffer_tile_lookup &tile ) -> bool
{
    return target.made_of( LIQUID ) && tile_has_flag( tile, "LIQUIDCONT" );
}

auto tile_allows_item_despite_noitem_flag( const item &target,
        const abs_tile_handle &tile ) -> bool
{
    return target.made_of( LIQUID ) && tile.has_flag( TFLAG_LIQUIDCONT );
}

auto move_cost_from_tile_parts( const ter_id &terrain_id, const furn_id &furniture_id,
                                const optional_vpart_position &vp ) -> int
{
    const auto &terrain = terrain_id.obj();
    const auto &furniture = furniture_id.obj();
    if( terrain.movecost == 0 || ( furniture.id && furniture.movecost < 0 ) ) {
        return 0;
    }

    if( vp ) {
        if( vp.obstacle_at_part() ) {
            return 0;
        }
        if( vp.part_with_feature( VPFLAG_AISLE, true ) ) {
            return 2;
        }
        return 8;
    }

    if( furniture.id ) {
        return std::max( terrain.movecost + furniture.movecost, 0 );
    }

    return std::max( terrain.movecost, 0 );
}

auto omt_submap_offsets() -> const std::array<point_omt_sm, 4> &
{
    static const auto offsets = std::array{
        point_omt_sm::zero(),
        point_omt_sm( 1, 0 ),
        point_omt_sm( 0, 1 ),
        point_omt_sm( 1, 1 ),
    };
    return offsets;
}

auto omt_submap_index( const point_omt_sm &local ) -> std::optional<std::size_t>
{
    if( local.x() < 0 || local.x() > 1 || local.y() < 0 || local.y() > 1 ) {
        return std::nullopt;
    }
    return static_cast<std::size_t>( local.x() + local.y() * 2 );
}

auto vertical_transition_target_below( const ter_id &source ) -> std::optional<ter_id>
{
    static const auto t_stairs_down_no_roof = ter_id( "t_stairs_down_no_roof" );
    static const auto t_stairs_down_underwater = ter_id( "t_stairs_down_underwater" );
    static const auto t_stairs_up_underwater = ter_id( "t_stairs_up_underwater" );
    static const auto t_wood_stairs_down = ter_id( "t_wood_stairs_down" );
    static const auto t_wood_stairs_up = ter_id( "t_wood_stairs_up" );
    static const auto t_ladder_up_down = ter_id( "t_ladder_up_down" );
    static const auto t_slope_down_underground = ter_id( "t_slope_down_underground" );
    static const auto t_triffid_slope_down = ter_id( "t_triffid_slope_down" );
    static const auto t_triffid_slope_up = ter_id( "t_triffid_slope_up" );

    if( source == t_stairs_down || source == t_stairs_down_no_roof ) {
        return t_stairs_up;
    }
    if( source == t_stairs_down_underwater ) {
        return t_stairs_up_underwater;
    }
    if( source == t_wood_stairs_down ) {
        return t_wood_stairs_up;
    }
    if( source == t_ladder_down || source == t_ladder_up_down ) {
        return t_ladder_up;
    }
    if( source == t_slope_down || source == t_slope_down_underground ) {
        return t_slope_up;
    }
    if( source == t_triffid_slope_down ) {
        return t_triffid_slope_up;
    }
    return std::nullopt;
}

auto vertical_transition_target_above( const ter_id &source ) -> std::optional<ter_id>
{
    static const auto t_stairs_down = ter_id( "t_stairs_down" );
    static const auto t_stairs_up_underwater = ter_id( "t_stairs_up_underwater" );
    static const auto t_stairs_down_underwater = ter_id( "t_stairs_down_underwater" );
    static const auto t_wood_stairs_down = ter_id( "t_wood_stairs_down" );
    static const auto t_wood_stairs_up = ter_id( "t_wood_stairs_up" );
    static const auto t_ladder_up_down = ter_id( "t_ladder_up_down" );
    static const auto t_triffid_slope_down = ter_id( "t_triffid_slope_down" );
    static const auto t_triffid_slope_up = ter_id( "t_triffid_slope_up" );

    if( source == t_stairs_up ) {
        return t_stairs_down;
    }
    if( source == t_stairs_up_underwater ) {
        return t_stairs_down_underwater;
    }
    if( source == t_wood_stairs_up ) {
        return t_wood_stairs_down;
    }
    if( source == t_ladder_up || source == t_ladder_up_down ) {
        return t_ladder_down;
    }
    if( source == t_slope_up ) {
        return t_slope_down;
    }
    if( source == t_triffid_slope_up ) {
        return t_triffid_slope_down;
    }
    return std::nullopt;
}

auto terrain_has_vertical_transition_direction( const ter_t &terrain,
        const ter_id &desired ) -> bool
{
    const auto &desired_terrain = desired.obj();
    return ( desired_terrain.has_flag( TFLAG_GOES_UP ) && terrain.has_flag( TFLAG_GOES_UP ) ) ||
           ( desired_terrain.has_flag( TFLAG_GOES_DOWN ) && terrain.has_flag( TFLAG_GOES_DOWN ) );
}

auto can_replace_with_vertical_transition( const submap &sm, const point_sm_ms &local,
        const ter_id &desired ) -> bool
{
    const auto current = sm.get_ter( local );
    const auto &current_terrain = current.obj();
    if( terrain_has_vertical_transition_direction( current_terrain, desired ) ) {
        return false;
    }
    if( current_terrain.has_flag( TFLAG_GOES_UP ) ||
        current_terrain.has_flag( TFLAG_GOES_DOWN ) ) {
        return false;
    }
    if( sm.get_furn( local ) != f_null || sm.get_trap( local ) != tr_null ||
        !sm.get_items( local ).empty() ) {
        return false;
    }
    if( current_terrain.has_flag( TFLAG_WALL ) ||
        current_terrain.has_flag( TFLAG_LIQUID ) ) {
        return false;
    }
    return current_terrain.has_flag( TFLAG_NO_FLOOR ) ||
           current_terrain.has_flag( TFLAG_SUPPORTS_ROOF ) ||
           current_terrain.has_flag( TFLAG_FLAT ) ||
           current_terrain.has_flag( "ROOF" );
}

auto mark_post_pass_changed( mapbuffer &buffer, submap &sm ) -> void
{
    sm.transparency_dirty = true;
    sm.floor_dirty = true;
    sm.roof_above_dirty = true;
    sm.absorption_dirty = true;
    sm.pf_dirty = true;

    if( auto *below = buffer.lookup_submap_in_memory( sm.position() + tripoint_below ) ) {
        below->roof_above_dirty = true;
    }
}

} // namespace


// ==========================================================================
// abs_tile_handle — absolute tile handle with embedded vehicle data
// ==========================================================================

abs_tile_handle::abs_tile_handle( const submap &sm, tripoint_abs_sm abs_sm,
                                  point_sm_ms local ) :
    sm_( &sm ),
    abs_sm_( abs_sm ),
    local_( local ),
    veh_part_( std::optional<vpart_position>() )
{
}

abs_tile_handle::abs_tile_handle( const submap &sm, tripoint_abs_sm abs_sm,
                                  point_sm_ms local,
                                  optional_vpart_position veh_part ) :
    sm_( &sm ),
    abs_sm_( abs_sm ),
    local_( local ),
    veh_part_( std::move( veh_part ) )
{
}

abs_tile_handle::operator bool() const
{
    return sm_ != nullptr;
}

auto abs_tile_handle::abs_pos() const -> tripoint_abs_ms
{
    return project_combine( abs_sm_, local_ );
}

auto abs_tile_handle::abs_submap_pos() const -> tripoint_abs_sm
{
    return abs_sm_;
}

auto abs_tile_handle::submap_pos() const -> point_sm_ms
{
    return local_;
}

auto abs_tile_handle::ter() const -> ter_id
{
    return sm_ ? sm_->get_ter( local_ ) : t_null;
}

auto abs_tile_handle::furn() const -> furn_id
{
    if( !sm_ ) {
        return f_null;
    }
    return sm_->get_furn( local_ );
}

::trap_id abs_tile_handle::trap_id() const
{
    return sm_ ? sm_->get_trap( local_ ) : tr_null;
}

auto abs_tile_handle::ter_obj() const -> const ter_t &
{
    static const ter_t null_ter;
    return sm_ ? ter().obj() : null_ter;
}

auto abs_tile_handle::furn_obj() const -> const furn_t &
{
    static const furn_t null_furn;
    return sm_ ? furn().obj() : null_furn;
}

auto abs_tile_handle::trap_obj() const -> const trap &
{
    static const trap null_trap;
    return sm_ ? trap_id().obj() : null_trap;
}

auto abs_tile_handle::field() const -> const class field &
{
        static const class field null_field;
        return sm_ ? sm_->get_field( local_ ) : null_field;
}

auto abs_tile_handle::items() const -> const location_vector<item> &
{
    static const location_vector<item> null_items;
    return sm_ ? sm_->get_items( local_ ) : null_items;
}

auto abs_tile_handle::furn_vars() const -> const data_vars::data_set &
{
    static const data_vars::data_set null_vars;
    return sm_ ? sm_->get_furn_vars( local_ ) : null_vars;
}

auto abs_tile_handle::radiation() const -> int
{
    return sm_ ? sm_->get_radiation( local_ ) : 0;
}

auto abs_tile_handle::lum() const -> std::uint8_t
{
    return sm_ ? sm_->get_lum( local_ ) : 0;
}

auto abs_tile_handle::move_cost( const vehicle *ignored_vehicle ) const -> int
{
    return move_cost_from_tile_parts( ter(), furn(), veh_part_ && ignored_vehicle &&
                                      &veh_part_->vehicle() == ignored_vehicle ?
                                      optional_vpart_position{} : veh_part_ );
}

auto abs_tile_handle::passable() const -> bool
{
    return move_cost() != 0;
}

auto abs_tile_handle::vehicle_part() const -> const optional_vpart_position &
{
    return veh_part_;
}

// ----- Read-only tile property queries -----

auto abs_tile_handle::has_flag( const std::string &flag ) const -> bool
{
    return has_flag_ter_or_furn( flag );
}

auto abs_tile_handle::has_flag_ter( const std::string &flag ) const -> bool
{
    return ter_obj().has_flag( flag );
}

auto abs_tile_handle::has_flag_furn( const std::string &flag ) const -> bool
{
    return furn_obj().has_flag( flag );
}

auto abs_tile_handle::has_flag_ter_or_furn( const std::string &flag ) const -> bool
{
    return ter_obj().has_flag( flag ) || furn_obj().has_flag( flag );
}

auto abs_tile_handle::has_flag_furn_or_vpart( const std::string &flag ) const -> bool
{
    return furn_obj().has_flag( flag ) ||
           static_cast<bool>( veh_part_.part_with_feature( flag, true ) );
}

auto abs_tile_handle::has_flag_vpart( const std::string &flag ) const -> bool
{
    return static_cast<bool>( veh_part_.part_with_feature( flag, true ) );
}

auto abs_tile_handle::has_flag( const ter_bitflags flag ) const -> bool
{
    return has_flag_ter_or_furn( flag );
}

auto abs_tile_handle::has_flag_ter( const ter_bitflags flag ) const -> bool
{
    return ter_obj().has_flag( flag );
}

auto abs_tile_handle::has_flag_furn( const ter_bitflags flag ) const -> bool
{
    return furn_obj().has_flag( flag );
}

auto abs_tile_handle::has_flag_ter_or_furn( const ter_bitflags flag ) const -> bool
{
    return ter_obj().has_flag( flag ) || furn_obj().has_flag( flag );
}

auto abs_tile_handle::is_bashable( const bool allow_floor ) const -> bool
{
    if( veh_part_ && veh_part_->obstacle_at_part() ) {
        return true;
    }
    return is_bashable_ter_furn( allow_floor );
}

auto abs_tile_handle::is_bashable_ter( const bool allow_floor ) const -> bool
{
    const auto &ter_bash = ter_obj().bash;
    return ter_bash.str_max != -1 &&
           ( ( !ter_bash.bash_below &&
               !ter_obj().has_flag( "VEH_TREAT_AS_BASH_BELOW" ) ) || allow_floor );
}

auto abs_tile_handle::is_bashable_furn() const -> bool
{
    const auto furn_id = furn();
    return furn_id != f_null && furn_id.obj().bash.str_max != -1;
}

auto abs_tile_handle::is_bashable_ter_furn( const bool allow_floor ) const -> bool
{
    return is_bashable_furn() || is_bashable_ter( allow_floor );
}

auto abs_tile_handle::bash_strength( const bool allow_floor ) const -> int
{
    const auto furn_id = furn();
    if( furn_id != f_null && furn_id.obj().bash.str_max != -1 ) {
        return furn_id.obj().bash.str_max;
    }
    const auto &ter_bash = ter_obj().bash;
    if( ter_bash.str_max != -1 && ( !ter_bash.bash_below || allow_floor ) ) {
        return ter_bash.str_max;
    }
    return -1;
}

auto abs_tile_handle::bash_resistance( const bool allow_floor ) const -> int
{
    const auto furn_id = furn();
    if( furn_id != f_null && furn_id.obj().bash.str_min != -1 ) {
        return furn_id.obj().bash.str_min;
    }
    const auto &ter_bash = ter_obj().bash;
    if( ter_bash.str_min != -1 && ( !ter_bash.bash_below || allow_floor ) ) {
        return ter_bash.str_min;
    }
    return -1;
}

auto abs_tile_handle::bash_rating( const int str, const bool allow_floor ) const -> int
{
    if( str <= 0 ) {
        return -1;
    }

    const auto &furniture = furn_obj();
    const auto &terrain = ter_obj();

    if( veh_part_ && veh_part_->obstacle_at_part() ) {
        return 2;
    }

    bool furn_smash = false;
    bool ter_smash = false;
    if( furniture.id && furniture.bash.str_max != -1 ) {
        furn_smash = true;
    } else if( terrain.bash.str_max != -1 && ( !terrain.bash.bash_below || allow_floor ) ) {
        ter_smash = true;
    }

    int bash_min = 0;
    int bash_max = 0;
    if( furn_smash ) {
        bash_min = furniture.bash.str_min;
        bash_max = furniture.bash.str_max;
    } else if( ter_smash ) {
        bash_min = terrain.bash.str_min;
        bash_max = terrain.bash.str_max;
    } else {
        return -1;
    }

    if( str < bash_min ) {
        return 1;
    } else if( str >= bash_min + ( bash_max - bash_min ) * 0.5 + 0.5 ) {
        return 10;
    } else if( str >= bash_min + ( bash_max - bash_min ) * 0.2 ) {
        return 7;
    } else if( str >= bash_min - bash_max * 0.2 ) {
        return 4;
    }

    return 1;
}

auto abs_tile_handle::is_divable() const -> bool
{
    if( veh_part_.part_with_feature( VPFLAG_BOARDABLE, true ) ) {
        return false;
    }
    return ter_obj().has_flag( "SWIMMABLE" ) &&
           ter_obj().has_flag( TFLAG_DEEP_WATER );
}

auto abs_tile_handle::is_water_shallow_current() const -> bool
{
    return ter_obj().has_flag( "CURRENT" ) &&
           !ter_obj().has_flag( TFLAG_DEEP_WATER );
}

auto abs_tile_handle::has_items() const -> bool
{
    return !items().empty();
}

auto abs_tile_handle::has_field_at() const -> bool
{
    return sm_ ? sm_->field_count > 0 : false;
}

auto abs_tile_handle::get_field_entry( const field_type_id &type ) const -> const field_entry *
{
    return field().find_field( type );
}

auto abs_tile_handle::get_field_age( const field_type_id &type ) const -> time_duration
{
    const auto *entry = get_field_entry( type );
    return entry != nullptr ? entry->get_field_age() : -1_turns;
}

auto abs_tile_handle::get_field_intensity( const field_type_id &type ) const -> int
{
    const auto *entry = get_field_entry( type );
    return entry != nullptr ? entry->get_field_intensity() : 0;
}

auto abs_tile_handle::has_graffiti_at() const -> bool
{
    return sm_ ? sm_->has_graffiti( local_ ) : false;
}

auto abs_tile_handle::graffiti_at() const -> const std::string &
{
    static const std::string empty;
    return sm_ ? sm_->get_graffiti( local_ ) : empty;
}

auto abs_tile_handle::has_signage() const -> bool
{
    return sm_ ? sm_->has_signage( local_ ) : false;
}

auto abs_tile_handle::get_signage() const -> std::string
{
    return sm_ ? sm_->get_signage( local_ ) : std::string{};
}

auto abs_tile_handle::has_computer() const -> bool
{
    return sm_ ? sm_->has_computer( local_ ) : false;
}

auto abs_tile_handle::get_computer() const -> const computer *
{
    return sm_ ? sm_->get_computer( local_ ) : nullptr;
}

auto abs_tile_handle::can_put_items_ter_furn() const -> bool
{
    return !has_flag( "NOITEM" ) && !has_flag( "SEALED" );
}

auto abs_tile_handle::accessible_items() const -> bool
{
    return !has_flag( "SEALED" ) || ter_obj().has_flag( "LIQUIDCONT" );
}

auto abs_tile_handle::move_cost_ter_furn() const -> int
{
    const int tercost = ter_obj().movecost;
    if( tercost == 0 ) {
        return 0;
    }
    const int furncost = furn_obj().movecost;
    if( furncost < 0 ) {
        return 0;
    }
    const int cost = tercost + furncost;
    return cost > 0 ? cost : 0;
}

auto abs_tile_handle::impassable() const -> bool
{
    return !passable();
}

auto abs_tile_handle::impassable_ter_furn() const -> bool
{
    return !passable_ter_furn();
}

auto abs_tile_handle::passable_ter_furn() const -> bool
{
    return move_cost_ter_furn() != 0;
}

auto abs_tile_handle::ter_vars() const -> const data_vars::data_set &
{
    static const data_vars::data_set null_vars;
    return sm_ ? sm_->get_ter_vars( local_ ) : null_vars;
}

auto abs_tile_handle::is_harvestable() const -> bool
{
    const auto furn_id = furn();
    if( furn_id != f_null && furn_id.obj().examine != iexamine::none ) {
        const auto &furniture = furn_id.obj();
        return !furniture.has_flag( TFLAG_HARVESTED ) &&
               !furniture.get_harvest().is_null() &&
               !furniture.get_harvest()->empty();
    }
    const auto &terrain = ter_obj();
    return !terrain.get_harvest().is_null() &&
           !terrain.get_harvest()->empty();
}

auto abs_tile_handle::dangerous_field_at() const -> bool
{
    for( const auto &pr : field() ) {
        if( pr.second.is_dangerous() ) {
            return true;
        }
    }
    return false;
}

auto abs_tile_handle::furnname() const -> std::string
{
    const furn_t &f = furn_obj();
    if( f.has_flag( "PLANT" ) ) {
        const auto &item_list = items();
        for( auto it = item_list.begin(); it != item_list.end(); ++it ) {
            if( ( *it )->is_seed() ) {
                return string_format( "%s (%s)", f.name(), ( *it )->get_plant_name() );
            }
        }
        debugmsg( "Missing seed for plant at %s", abs_pos().to_string() );
        return "null";
    }
    return f.name();
}

auto abs_tile_handle::tername() const -> std::string
{
    return ter_obj().name();
}

auto abs_tile_handle::name() const -> std::string
{
    return furn() != f_null ? furnname() : tername();
}

auto abs_tile_handle::disp_name() const -> std::string
{
    return string_format( _( "the %s" ), name() );
}

auto abs_tile_handle::fetch( mapbuffer &buf, const tripoint_abs_ms p )
-> std::optional<abs_tile_handle> // *NOPAD*
{
    return fetch( buf, p, {
        .mode = mapbuffer_lookup_mode::resident_only,
    } );
}

auto abs_tile_handle::fetch( mapbuffer &buf, const tripoint_abs_ms p,
                             const mapbuffer_lookup_options options )
-> std::optional<abs_tile_handle> // *NOPAD*
{
    const auto split = project_remain<coords::sm>( p );
    auto *const sm = buf.get_submap( split.quotient_tripoint, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }
    return abs_tile_handle( *sm, split.quotient_tripoint, split.remainder,
                            buf.vehicle_part_at_loaded_tile( p ) );
}

auto abs_tile_handle::fetch_terrain_only( mapbuffer &buf, const tripoint_abs_ms p )
-> std::optional<abs_tile_handle> // *NOPAD*
{
    return fetch_terrain_only( buf, p, {
        .mode = mapbuffer_lookup_mode::resident_only,
    } );
}

auto abs_tile_handle::fetch_terrain_only( mapbuffer &buf, const tripoint_abs_ms p,
        const mapbuffer_lookup_options options )
-> std::optional<abs_tile_handle> // *NOPAD*
{
    const auto split = project_remain<coords::sm>( p );
    auto *const sm = buf.get_submap( split.quotient_tripoint, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }
    return abs_tile_handle( *sm, split.quotient_tripoint, split.remainder );
}

// ==========================================================================
// submap_tile_range — one-hash range over all 144 tiles in a submap
// ==========================================================================

submap_tile_range::submap_tile_range( const submap &sm, tripoint_abs_sm abs_sm,
                                      const bool has_vehicles, mapbuffer &buf ) :
    sm_( &sm ),
    abs_sm_( abs_sm ),
    has_vehicles_( has_vehicles ),
    buf_( &buf )
{
}

auto submap_tile_range::begin() const -> iterator
{
    return iterator( *sm_, abs_sm_, has_vehicles_, buf_, 0 );
}

auto submap_tile_range::end() const -> iterator
{
    return iterator( *sm_, abs_sm_, has_vehicles_, buf_,
                     static_cast<int>( SEEX ) * static_cast<int>( SEEY ) );
}

submap_tile_range::iterator::iterator( const submap &sm, tripoint_abs_sm abs_sm,
                                       const bool has_vehicles, mapbuffer *const buf,
                                       const int idx ) :
    sm_( &sm ),
    abs_sm_( abs_sm ),
    has_vehicles_( has_vehicles ),
    buf_( buf ),
    idx_( idx )
{
}

auto submap_tile_range::iterator::operator*() const -> abs_tile_handle
{
    const point_sm_ms local( idx_ % SEEX, idx_ / SEEX );
    if( has_vehicles_ ) {
        return abs_tile_handle( *sm_, abs_sm_, local,
                                buf_->vehicle_part_at_loaded_tile(
                                    project_combine( abs_sm_, local ) ) );
    } else {
        return abs_tile_handle( *sm_, abs_sm_, local );
    }
}

auto submap_tile_range::iterator::operator++() -> iterator &
{
    ++idx_;
    return *this;
}

bool submap_tile_range::iterator::operator==( const iterator &other ) const
{
    return idx_ == other.idx_;
}

bool submap_tile_range::iterator::operator!=( const iterator &other ) const
{
    return idx_ != other.idx_;
}

auto mapbuffer::submap_tiles( const tripoint_abs_sm &p )
-> std::optional<submap_tile_range> // *NOPAD*
{
    submap *sm = lookup_submap_in_memory( p );
    if( !sm ) {
        return std::nullopt;
    }
    return submap_tile_range( *sm, p, !sm->vehicles.empty(), *this );
}

// ==========================================================================
// simulated_island — connected component of simulated columns
// ==========================================================================

bool simulated_island::contains( point_abs_sm p ) const
{
    if( p.x() < begin.x() || p.x() >= end.x() ||
        p.y() < begin.y() || p.y() >= end.y() ) {
        return false;
    }
    const int width = end.x() - begin.x();
    const int idx = ( p.x() - begin.x() ) + ( p.y() - begin.y() ) * width;
    return idx >= 0 && idx < static_cast<int>( bits_.size() ) && bits_[idx];
}

auto simulated_island::columns_in( point_abs_sm r_begin, point_abs_sm r_end ) const
-> std::vector<point_abs_sm> // *NOPAD*
{
    std::vector<point_abs_sm> result;

    // Clamp query rect to island bounds
    const point_abs_sm scan_begin(
        std::max( r_begin.x(), begin.x() ),
        std::max( r_begin.y(), begin.y() )
    );
    const point_abs_sm scan_end(
        std::min( r_end.x(), end.x() ),
        std::min( r_end.y(), end.y() )
    );

    if( scan_begin.x() >= scan_end.x() || scan_begin.y() >= scan_end.y() ) {
        return result;  // empty intersection
    }

    const int width = end.x() - begin.x();
    for( int y = scan_begin.y(); y < scan_end.y(); ++y ) {
        for( int x = scan_begin.x(); x < scan_end.x(); ++x ) {
            const int idx = ( x - begin.x() ) + ( y - begin.y() ) * width;
            if( idx >= 0 && idx < static_cast<int>( bits_.size() ) && bits_[idx] ) {
                result.emplace_back( x, y );
            }
        }
    }
    return result;
}

auto simulated_island::size() const -> std::size_t
{
    return bits_.size();
}

// ==========================================================================
// Island builder — connected-components pass over simulated columns
// ==========================================================================

auto mapbuffer::build_islands( const std::unordered_set<point_abs_sm> &columns )
-> std::vector<simulated_island> // *NOPAD*
{
    std::vector<simulated_island> islands;
    std::unordered_set<point_abs_sm> visited;
    visited.reserve( columns.size() );

    static constexpr std::array<point_rel_sm, 4> dirs = {
        point_rel_sm::east(),
        point_rel_sm::west(),
        point_rel_sm::north(),
        point_rel_sm::south(),
    };

    for( const point_abs_sm &seed : columns ) {
        if( visited.contains( seed ) ) {
            continue;
        }

        // BFS for this connected component (4-directional adjacency)
        std::vector<point_abs_sm> component;
        std::queue<point_abs_sm> q;
        q.push( seed );
        visited.insert( seed );

        int min_x = seed.x();
        int min_y = seed.y();
        int max_x = seed.x();
        int max_y = seed.y();

        while( !q.empty() ) {
            const point_abs_sm cur = q.front();
            q.pop();
            component.push_back( cur );

            min_x = std::min( min_x, cur.x() );
            min_y = std::min( min_y, cur.y() );
            max_x = std::max( max_x, cur.x() );
            max_y = std::max( max_y, cur.y() );

            for( const point_rel_sm &d : dirs ) {
                const point_abs_sm nxt = cur + d;
                if( columns.contains( nxt ) && !visited.contains( nxt ) ) {
                    visited.insert( nxt );
                    q.push( nxt );
                }
            }
        }

        // Build island from component
        simulated_island island;
        island.begin = point_abs_sm( min_x, min_y );
        island.end = point_abs_sm( max_x + 1, max_y + 1 );  // half-open

        const int w = island.end.x() - island.begin.x();
        const int h = island.end.y() - island.begin.y();
        island.bits_.resize( static_cast<std::size_t>( w ) * static_cast<std::size_t>( h ), false );
        for( const point_abs_sm &p : component ) {
            const int idx = ( p.x() - island.begin.x() ) +
                            ( p.y() - island.begin.y() ) * w;
            island.bits_[idx] = true;
        }

        islands.push_back( std::move( island ) );
    }

    return islands;
}

auto mapbuffer::simulated_islands() const -> std::span<const simulated_island>
{
    return simulated_islands_;
}

// ==========================================================================
// submap_tile_iterator_range — island-mapped spatial iterators (Layer 3)
// ==========================================================================

submap_tile_iterator_range::iterator::iterator(
    mapbuffer &buf,
    std::vector<point_abs_sm> columns,
    const int z,
    const tripoint_abs_ms center,
    const int radius,
    const tripoint_abs_ms rect_begin,
    const tripoint_abs_ms rect_end,
    const int mode ) :
    buf_( &buf ),
    columns_( std::move( columns ) ),
    z_( z ),
    center_( center ),
    radius_( radius ),
    rect_begin_( rect_begin ),
    rect_end_( rect_end ),
    column_idx_( 0 ),
    tile_idx_( 0 ),
    mode_( mode )
{
    advance_to_valid();
}

submap_tile_iterator_range::iterator::iterator( const int column_count ) :
    column_idx_( column_count ),
    tile_idx_( 0 )
{
}

void submap_tile_iterator_range::iterator::advance_to_valid()
{
    if( !buf_ || column_idx_ < 0 ) {
        column_idx_ = -1;
        return;
    }

    const int tiles_per_submap = SEEX * SEEY;

    while( column_idx_ < static_cast<int>( columns_.size() ) ) {
        while( tile_idx_ < tiles_per_submap ) {
            const point_abs_sm &col = columns_[column_idx_];
            const point_sm_ms local( tile_idx_ % SEEX, tile_idx_ / SEEX );
            const tripoint_abs_sm sm_pos( col, z_ );

            // Quick column-state check before constructing handle
            if( !buf_->is_column_state( col, submap_column_load_state::simulated ) ) {
                tile_idx_ = tiles_per_submap;  // skip this column
                break;
            }

            if( mode_ == 0 ) {
                // mode=all: no per-tile filter needed
                return;
            }

            // Compute absolute ms position of this tile
            const tripoint_abs_ms abs_pos = project_combine( sm_pos, local );

            if( mode_ == 1 ) {
                // radius filter: Chebyshev distance
                if( square_dist( abs_pos.xy(), center_.xy() ) <= radius_ ) {
                    return;
                }
            } else if( mode_ == 2 ) {
                // rectangle filter: inclusive bounds
                if( abs_pos.x() >= rect_begin_.x() && abs_pos.x() <= rect_end_.x() &&
                    abs_pos.y() >= rect_begin_.y() && abs_pos.y() <= rect_end_.y() &&
                    abs_pos.z() >= rect_begin_.z() && abs_pos.z() <= rect_end_.z() ) {
                    return;
                }
            }

            ++tile_idx_;
        }

        // Exhausted this column; move to the next
        ++column_idx_;
        tile_idx_ = 0;
    }

    // No more valid tiles — mark as end sentinel
    column_idx_ = static_cast<int>( columns_.size() );
    tile_idx_ = 0;
}

auto submap_tile_iterator_range::iterator::operator*() const -> abs_tile_handle
{
    const point_abs_sm &col = columns_[column_idx_];
    const point_sm_ms local( tile_idx_ % SEEX, tile_idx_ / SEEX );
    const tripoint_abs_sm sm_pos( col, z_ );

    // Single hash lookup: find the submap pointer, then construct the handle.
    submap *sm = buf_->lookup_submap_in_memory( sm_pos );
    if( sm ) {
        const bool has_veh = !sm->vehicles.empty();
        if( has_veh ) {
            return abs_tile_handle( *sm, sm_pos, local,
                                    buf_->vehicle_part_at_loaded_tile(
                                        project_combine( sm_pos, local ) ) );
        } else {
            return abs_tile_handle( *sm, sm_pos, local );
        }
    }

    return abs_tile_handle();
}

auto submap_tile_iterator_range::iterator::operator++() -> iterator &
{
    if( column_idx_ >= static_cast<int>( columns_.size() ) ) {
        return *this;  // already at end
    }

    ++tile_idx_;
    advance_to_valid();
    return *this;
}

bool submap_tile_iterator_range::iterator::operator==( const iterator &other ) const
{
    return column_idx_ == other.column_idx_ && tile_idx_ == other.tile_idx_;
}

bool submap_tile_iterator_range::iterator::operator!=( const iterator &other ) const
{
    return !( *this == other );
}

auto submap_tile_iterator_range::begin() const -> iterator
{
    return iterator( *buf_, columns_, z_, center_, radius_,
                     rect_begin_, rect_end_, mode_ );
}

auto submap_tile_iterator_range::end() const -> iterator
{
    return iterator( static_cast<int>( columns_.size() ) );
}

// ----- Free functions (Layer 3 entry points) -----
// These are friend functions of submap_tile_iterator_range, so they
// access private members directly.

auto simulated_tiles_in_radius( mapbuffer &buf,
                                const tripoint_abs_ms center, const int radius )
-> submap_tile_iterator_range // *NOPAD*
{
    // Compute bounding box in abs_ms coordinates
    const tripoint_abs_ms bb_begin(
        center.x() - radius, center.y() - radius, center.z() );
    const tripoint_abs_ms bb_end(
        center.x() + radius, center.y() + radius, center.z() );

    // Project to submap coordinates.
    // sm_begin rounds down (inclusive), sm_end rounds down then +1 so the
    // half-open columns_in() interval covers all submaps that intersect
    // the ms bounding box.
    const point_abs_sm sm_begin = project_to<coords::sm>( bb_begin ).xy();
    const point_abs_sm sm_end =
        project_to<coords::sm>( bb_end ).xy() + point_rel_sm( 1, 1 );

    // Collect columns from all islands intersecting the bounding box
    std::vector<point_abs_sm> columns;
    for( const simulated_island &island : buf.simulated_islands() ) {
        auto island_cols = island.columns_in( sm_begin, sm_end );
        columns.insert( columns.end(),
                        std::make_move_iterator( island_cols.begin() ),
                        std::make_move_iterator( island_cols.end() ) );
    }

    submap_tile_iterator_range result;
    result.buf_ = &buf;
    result.columns_ = std::move( columns );
    result.z_ = center.z();
    result.center_ = center;
    result.radius_ = radius;
    result.mode_ = static_cast<int>( submap_tile_iterator_range::shape::radius );
    return result;
}

auto simulated_tiles_in_rectangle( mapbuffer &buf,
                                   const tripoint_abs_ms begin, const tripoint_abs_ms end )
-> submap_tile_iterator_range // *NOPAD*
{
    // Project the rectangle bounds to submap coordinates.
    // sm_begin rounds down (inclusive), sm_end rounds down then +1 so the
    // half-open columns_in() interval covers all submaps that intersect.
    const point_abs_sm sm_begin = project_to<coords::sm>( begin ).xy();
    const point_abs_sm sm_end =
        project_to<coords::sm>( end ).xy() + point_rel_sm( 1, 1 );

    // Collect columns from all islands intersecting the rectangle
    std::vector<point_abs_sm> columns;
    for( const simulated_island &island : buf.simulated_islands() ) {
        auto island_cols = island.columns_in( sm_begin, sm_end );
        columns.insert( columns.end(),
                        std::make_move_iterator( island_cols.begin() ),
                        std::make_move_iterator( island_cols.end() ) );
    }

    submap_tile_iterator_range result;
    result.buf_ = &buf;
    result.columns_ = std::move( columns );
    result.z_ = begin.z();
    result.rect_begin_ = begin;
    result.rect_end_ = end;
    result.mode_ = static_cast<int>( submap_tile_iterator_range::shape::rectangle );
    return result;
}

auto simulated_tiles_on_zlevel( mapbuffer &buf, const int z )
-> submap_tile_iterator_range // *NOPAD*
{
    // Collect all columns from all islands
    std::vector<point_abs_sm> columns;
    for( const simulated_island &island : buf.simulated_islands() ) {
        auto island_cols = island.columns_in( island.begin, island.end );
        columns.insert( columns.end(),
                        std::make_move_iterator( island_cols.begin() ),
                        std::make_move_iterator( island_cols.end() ) );
    }

    submap_tile_iterator_range result;
    result.buf_ = &buf;
    result.columns_ = std::move( columns );
    result.z_ = z;
    result.mode_ = static_cast<int>( submap_tile_iterator_range::shape::all );
    return result;
}

auto mapbuffer::for_each_submap_tile(
    const submap &sm, tripoint_abs_sm abs_sm,
    const std::function<void( const abs_tile_handle & )> &fn ) -> void
{
    for( const point_sm_ms &local : ::submap_tiles() ) {
        fn( abs_tile_handle( sm, abs_sm, local,
                             vehicle_part_at_loaded_tile( project_combine( abs_sm, local ) ) ) );
    }
}

auto mapbuffer::for_each_simulated_tile(
    const int zlev,
    const std::function<void( const abs_tile_handle & )> &fn ) -> void
{
    for_each_simulated_submap( [&]( const tripoint_abs_sm & abs_sm, submap & sm ) {
        if( abs_sm.z() != zlev ) {
            return;
        }
        for( const point_sm_ms &local : ::submap_tiles() ) {
            fn( abs_tile_handle( sm, abs_sm, local,
                                 vehicle_part_at_loaded_tile( project_combine( abs_sm, local ) ) ) );
        }
    } );
}

mapbuffer_bounds_view::mapbuffer_bounds_view( mapbuffer &buffer,
        const point_abs_sm &begin,
        const point_abs_sm &end,
        const mapbuffer_lookup_options options ) :
    buffer_( &buffer ),
    options_( options )
{
    update( begin, end );
}

auto mapbuffer_bounds_view::operator=( mapbuffer_bounds_view &&rhs )
noexcept -> mapbuffer_bounds_view & // *NOPAD*
{
    if( this == &rhs ) {
        return *this;
    }

    buffer_ = std::exchange( rhs.buffer_, nullptr );
    options_ = rhs.options_;
    begin_ = rhs.begin_;
    end_ = rhs.end_;
    submaps_ = std::move( rhs.submaps_ );
    submaps_by_zlev_ = std::move( rhs.submaps_by_zlev_ );
    indexed_submaps_ = std::move( rhs.indexed_submaps_ );
    return *this;
}

auto mapbuffer_bounds_view::begin() const -> point_abs_sm
{
    return begin_;
}

auto mapbuffer_bounds_view::end() const -> point_abs_sm
{
    return end_;
}

auto mapbuffer_bounds_view::bounds_size() const -> point_rel_sm
{
    return point_rel_sm( end_.x() - begin_.x(), end_.y() - begin_.y() );
}

auto mapbuffer_bounds_view::indexed_submap_index( const point_rel_sm &offset,
        const int zlev ) const -> std::optional<std::size_t>
{
    const auto size = bounds_size();
    if( offset.x() < 0 || offset.y() < 0 || offset.x() >= size.x() || offset.y() >= size.y() ) {
        return std::nullopt;
    }
    if( zlev < -OVERMAP_DEPTH || zlev > OVERMAP_HEIGHT ) {
        return std::nullopt;
    }

    const auto width = static_cast<std::size_t>( size.x() );
    const auto height = static_cast<std::size_t>( size.y() );
    const auto z_offset = static_cast<std::size_t>( zlev + OVERMAP_DEPTH );
    const auto x_offset = static_cast<std::size_t>( offset.x() );
    const auto y_offset = static_cast<std::size_t>( offset.y() );
    return z_offset * width * height + x_offset * height + y_offset;
}

auto mapbuffer_bounds_view::get_submap_view( const tripoint_abs_sm &pos )
const -> std::optional<submap_ref>
{
    if( pos.x() < begin_.x() || pos.y() < begin_.y() || pos.x() >= end_.x() ||
        pos.y() >= end_.y() ) {
        return std::nullopt;
    }
    return get_submap_view( point_rel_sm( pos.x() - begin_.x(), pos.y() - begin_.y() ), pos.z() );
}

auto mapbuffer_bounds_view::get_submap_view( const point_rel_sm &offset,
        const int zlev ) const -> std::optional<submap_ref>
{
    const auto index = indexed_submap_index( offset, zlev );
    if( !index || *index >= indexed_submaps_.size() ) {
        return std::nullopt;
    }

    const auto *sm = indexed_submaps_[*index];
    if( sm == nullptr ) {
        return std::nullopt;
    }

    return submap_ref{ .sm = sm, .pos = tripoint_abs_sm( begin_ + offset, zlev ) };
}

auto mapbuffer_bounds_view::is_complete() const -> bool
{
    return !indexed_submaps_.empty() && std::ranges::all_of(
    indexed_submaps_, []( const auto * sm ) {
        return sm != nullptr;
    } );
}

auto mapbuffer_bounds_view::update( const point_abs_sm &begin,
                                    const point_abs_sm &end,
                                    mapbuffer *buffer ) -> void
{
    if( buffer != nullptr ) {
        buffer_ = buffer;
    }
    begin_ = begin;
    end_ = end;
    submaps_.clear();
    indexed_submaps_.clear();
    for( auto &submaps : submaps_by_zlev_ ) {
        submaps.clear();
    }

    const auto size = bounds_size();
    if( buffer_ == nullptr || size.x() <= 0 || size.y() <= 0 ) {
        return;
    }

    const auto width = static_cast<std::size_t>( size.x() );
    const auto height = static_cast<std::size_t>( size.y() );
    indexed_submaps_.assign( width * height *
                             static_cast<std::size_t>( OVERMAP_LAYERS ), nullptr );

    const auto max = point_abs_sm( end_.x() - 1, end_.y() - 1 );
    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        const auto z_index = static_cast<std::size_t>( zlev + OVERMAP_DEPTH );
        for( const auto pos : point_range<point_abs_sm>( begin_, max ) ) {
            const tripoint_abs_sm abs_sm( pos, zlev );
            auto *sm = buffer_->lookup_submap_in_memory( abs_sm );
            if( !sm ) {
                continue;
            }

            const auto offset = point_rel_sm( pos.x() - begin_.x(), pos.y() - begin_.y() );
            if( const auto index = indexed_submap_index( offset, zlev ) ) {
                indexed_submaps_[*index] = sm;
            }
            submap_ref ref{ .sm = sm, .pos = abs_sm };
            submaps_.push_back( ref );
            submaps_by_zlev_[z_index].push_back( ref );
        }
    }
}

auto mapbuffer_bounds_view::update( const point_rel_sm &offset ) -> void
{
    update( begin_ + offset, end_ + offset );
}

mapbuffer_load_region::mapbuffer_load_region( mapbuffer &buffer,
        const load_request_source source,
        const point_abs_sm &begin,
        const point_abs_sm &end,
        const mapbuffer_lookup_options options ) :
    buffer_( &buffer ),
    source_( source ),
    options_( options )
{
    update( begin, end );
}

mapbuffer_load_region::mapbuffer_load_region( const options &opts ) :
    buffer_( &opts.buffer ),
    source_( opts.source ),
    options_( opts.lookup )
{
    update( opts.begin, opts.end );
}

mapbuffer_load_region::~mapbuffer_load_region()
{
    release();
}

mapbuffer_load_region::mapbuffer_load_region( mapbuffer_load_region &&rhs ) noexcept
{
    *this = std::move( rhs );
}

auto mapbuffer_load_region::operator=( mapbuffer_load_region &&rhs )
noexcept -> mapbuffer_load_region & // *NOPAD*
{
    if( this == &rhs ) {
        return *this;
    }

    release();
    buffer_ = std::exchange( rhs.buffer_, nullptr );
    source_ = rhs.source_;
    options_ = rhs.options_;
    begin_ = rhs.begin_;
    end_ = rhs.end_;
    handle_ = std::exchange( rhs.handle_, 0 );
    view_ = std::move( rhs.view_ );
    return *this;
}

auto mapbuffer_load_region::update( const point_abs_sm &begin,
                                    const point_abs_sm &end ) -> void
{
    begin_ = begin;
    end_ = end;
    if( buffer_ == nullptr ) {
        view_.update( begin_, end_ );
        return;
    }

    if( handle_ == 0 ) {
        assert( source_.has_value() );
        handle_ = submap_loader.request_load( source_.value(),
                                              buffer_->get_dimension_id(), begin_, end_ );
    } else {
        submap_loader.update_request( handle_, begin_, end_ );
    }
    refresh_view();
}

auto mapbuffer_load_region::update( const point_rel_sm &offset ) -> void
{
    update( begin_ + offset, end_ + offset );
}

auto mapbuffer_load_region::refresh_view() -> void
{
    if( buffer_ == nullptr ) {
        view_.update( begin_, end_ );
        return;
    }
    view_.update( begin_, end_, buffer_ );
}

auto mapbuffer_load_region::release() -> void
{
    if( handle_ != 0 ) {
        submap_loader.release_load( handle_ );
        handle_ = 0;
    }
}

auto mapbuffer::register_submap_vehicles(
    const tripoint_abs_sm &p, submap &sm ) -> void
{
    for( const auto &veh : sm.vehicles ) {
        if( veh == nullptr || veh->part_count() <= 0 ) {
            continue;
        }
        veh->abs_sm_pos = p;
        veh->set_dimension( dimension_id_ );
        loaded_vehicles_.insert( veh.get() );
        index_vehicle_footprint_unlocked( *veh );
        const auto footprints = calculate_vehicle_submap_footprints( *veh );
        const auto overlaps_active_bubble = std::ranges::any_of( footprints,
        [&]( const auto & footprint ) {
            return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this,
                    *footprint );
        } );
        if( overlaps_active_bubble ) {
            map &here = get_map();
            here.invalidate_max_populated_zlev( veh->abs_sm_pos.z() );
            auto &ch = here.get_cache( veh->abs_sm_pos.z() );
            ch.vehicle_list.insert( veh.get() );
            here.add_vehicle_to_cache( veh.get() );
        }
    }
}

auto mapbuffer::unregister_submap_vehicles( const tripoint_abs_sm &p ) -> void
{
    for( auto iter = loaded_vehicles_.begin(); iter != loaded_vehicles_.end(); ) {
        const auto *const veh = *iter;
        if( veh == nullptr || veh->abs_sm_pos == p ) {
            if( veh != nullptr ) {
                const auto footprints = calculate_vehicle_submap_footprints( *veh );
                const auto overlaps_active_bubble = std::ranges::any_of( footprints,
                [&]( const auto & footprint ) {
                    return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this,
                            *footprint );
                } );
                if( overlaps_active_bubble ) {
                    map &here = get_map();
                    auto &ch = here.get_cache( veh->abs_sm_pos.z() );
                    ch.vehicle_list.erase( const_cast<vehicle *>( veh ) );
                    ch.zone_vehicles.erase( const_cast<vehicle *>( veh ) );
                    for( const vpart_reference &vpr : veh->get_all_parts() ) {
                        if( !vpr.part().removed ) {
                            const auto local_pos = veh->bub_part_location( vpr.part() );
                            here.clear_vehicle_point_from_cache(
                                const_cast<vehicle *>( veh ), local_pos );
                        }
                    }
                }
            }
            unindex_vehicle_footprint_unlocked( veh );
            iter = loaded_vehicles_.erase( iter );
        } else {
            ++iter;
        }
    }
}

auto mapbuffer::unindex_vehicle_footprint_unlocked( const vehicle *veh ) -> void
{
    const auto locations_iter = vehicle_footprint_locations_.find( veh );
    if( locations_iter == vehicle_footprint_locations_.end() ) {
        return;
    }

    for( const auto &pos : locations_iter->second ) {
        const auto footprint_iter = vehicle_footprint_by_location_.find( pos );
        if( footprint_iter == vehicle_footprint_by_location_.end() ) {
            continue;
        }

        std::erase_if( footprint_iter->second, [&]( const vehicle_footprint_entry & entry ) {
            return entry.veh == veh;
        } );
        if( footprint_iter->second.empty() ) {
            vehicle_footprint_by_location_.erase( footprint_iter );
        }
    }

    vehicle_footprint_locations_.erase( locations_iter );
}

auto mapbuffer::index_vehicle_footprint_unlocked( vehicle &veh ) -> void
{
    unindex_vehicle_footprint_unlocked( &veh );
    if( veh.part_count() <= 0 ) {
        return;
    }

    auto &locations = vehicle_footprint_locations_[&veh];
    for( const auto &vpr : veh.get_all_parts() ) {
        if( vpr.part().removed ) {
            continue;
        }

        const auto pos = veh.abs_part_location( vpr.part() );
        vehicle_footprint_by_location_[pos].push_back( vehicle_footprint_entry {
            .veh = &veh,
            .part_index = vpr.part_index(),
        } );
        locations.push_back( pos );
    }
}

auto mapbuffer::indexed_vehicle_part_at_unlocked(
    const tripoint_abs_ms &p ) -> optional_vpart_position
{
    const auto footprint_iter = vehicle_footprint_by_location_.find( p );
    if( footprint_iter == vehicle_footprint_by_location_.end() ) {
        return optional_vpart_position( std::nullopt );
    }

    auto &entries = footprint_iter->second;
    std::erase_if( entries, [&]( const vehicle_footprint_entry & entry ) {
        const auto *const veh = entry.veh;
        if( veh == nullptr || !loaded_vehicles_.contains( const_cast<vehicle *>( veh ) ) ) {
            return true;
        }
        if( entry.part_index >= static_cast<std::size_t>( veh->part_count() ) ) {
            return true;
        }
        const auto part_index = static_cast<int>( entry.part_index );
        const auto &part = veh->cpart( part_index );
        return part.removed || veh->abs_part_location( part ) != p;
    } );

    if( entries.empty() ) {
        vehicle_footprint_by_location_.erase( footprint_iter );
        return optional_vpart_position( std::nullopt );
    }

    auto *selected = static_cast<vehicle_footprint_entry *>( nullptr );
    for( auto &entry : entries ) {
        const auto part_index = static_cast<int>( entry.part_index );
        if( selected == nullptr || !entry.veh->part_info( part_index ).has_flag( VPFLAG_NOCOLLIDE ) ) {
            selected = &entry;
        }
    }

    if( selected == nullptr ) {
        return optional_vpart_position( std::nullopt );
    }
    return optional_vpart_position( vpart_position( *selected->veh, selected->part_index ) );
}

mapbuffer::mapbuffer() = default;
mapbuffer::~mapbuffer() = default;

auto mapbuffer::get_boundary_terrain() const -> ter_id
{
    if( pocket_info_ && pocket_info_->bounds.boundary_terrain.is_valid() ) {
        return pocket_info_->bounds.boundary_terrain.id();
    }
    return ter_id( "t_null" );
}

void mapbuffer::clear()
{
    {
        std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
        creature_tracker_.clear();
        active_npcs_.clear();
        active_npcs_by_location_.clear();
        loaded_vehicles_.clear();
        vehicle_footprint_by_location_.clear();
        vehicle_footprint_locations_.clear();
        submaps_with_active_items_.clear();
        submaps_with_luminous_items_.clear();
        column_states_.clear();
        dirty_columns_.clear();
        column_to_island_.clear();
        island_sounds_.clear();
        simulated_islands_.clear();
        submaps.clear();
        pocket_info_.reset();

        if( g != nullptr && g->m.get_bound_dimension() == dimension_id_ ) {
            for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                g->m.clear_vehicle_list( z );
                g->m.invalidate_map_cache( z );
            }
            g->m.clear_vehicle_cache();
        }
    }
    std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
    pending_writes_.clear();
}

bool mapbuffer::add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm )
{
    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );
    if( submaps.contains( p ) ) {
        return false;
    }

    submaps[p] = std::move( sm );
    register_submap_vehicles( p, *submaps[p] );
    if( !submaps[p]->active_items.empty() ) {
        submaps_with_active_items_.insert( p );
    }
    refresh_luminous_item_submap_index( p, {
        .mode = mapbuffer_lookup_mode::resident_only,
    } );

    // New submap is at least resident in the column-state cache.
    column_states_.emplace( p.xy(), submap_column_load_state::resident );

    return true;
}

auto mapbuffer::is_column_state( const point_abs_sm col,
                                 const submap_column_load_state min_state ) const noexcept -> bool
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    const auto it = column_states_.find( col );
    return it != column_states_.end() && it->second >= min_state;
}

// Overload: add a submap without updating the luminous item index.
// Used by add_uniform_omt since uniform submaps have zero items.
bool mapbuffer::add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm,
                            const mapbuffer_add_submap_options opts )
{
    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );
    if( submaps.contains( p ) ) {
        return false;
    }

    submaps[p] = std::move( sm );
    register_submap_vehicles( p, *submaps[p] );
    if( !submaps[p]->active_items.empty() ) {
        submaps_with_active_items_.insert( p );
    }
    if( !opts.skip_luminance_refresh ) {
        refresh_luminous_item_submap_index( p, {
            .mode = mapbuffer_lookup_mode::resident_only,
        } );
    }

    column_states_.emplace( p.xy(), submap_column_load_state::resident );

    return true;
}

void mapbuffer::set_simulated_submaps(
    const std::unordered_set<point_abs_sm> &columns )
{
    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );

    // First pass: collect transitions for bookkeeping below.
    // newly_simulated: columns that just entered simulation.
    // last_demoted_columns_: columns that just left simulation
    // (consumed by the game loop for NPC/monster eviction).
    std::vector<point_abs_sm> newly_simulated;
    last_demoted_columns_.clear();

    // Walk current column states: demote departed,
    // promote new arrivals and mark them dirty.
    for( auto it = column_states_.begin(); it != column_states_.end(); ) {
        const bool should_be_simulated = columns.contains( it->first );
        if( should_be_simulated ) {
            if( it->second == submap_column_load_state::resident ) {
                // First entry into simulation — mark dirty.
                it->second = submap_column_load_state::simulated;
                dirty_columns_.insert( it->first );
                newly_simulated.push_back( it->first );
            }
            ++it;
        } else {
            if( it->second == submap_column_load_state::simulated ) {
                // Leave simulation — stay resident, stay dirty.
                it->second = submap_column_load_state::resident;
                last_demoted_columns_.push_back( it->first );
            }
            ++it;
        }
    }

    // Columns in `columns` but not in column_states_ are theoretical
    // (simulated implies resident), but handle them defensively.
    for( const point_abs_sm &col : columns ) {
        if( !column_states_.contains( col ) ) {
            column_states_.emplace( col, submap_column_load_state::simulated );
            dirty_columns_.insert( col );
            newly_simulated.push_back( col );
        }
    }

    // For newly-simulated columns: refresh vehicle registry, active item
    // index, and distribution grid for every z-level so they are immediately
    // queryable.
    for( const point_abs_sm &col : newly_simulated ) {
        for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            const tripoint_abs_sm sm_pos{ col, z };
            if( submaps.contains( sm_pos ) ) {
                refresh_vehicle_registry_for_submap( sm_pos, {
                    .mode = mapbuffer_lookup_mode::resident_only,
                } );
                refresh_active_item_submap_index( sm_pos, mapbuffer_lookup_options{
                    .mode = mapbuffer_lookup_mode::resident_only,
                } );
                if( auto *tracker = get_distribution_grid_tracker_for( dimension_id_ ) ) {
                    tracker->on_submap_loaded( sm_pos, dimension_id_ );
                }
            }
        }
    }

    // For newly-demoted columns: stamp last_touched so actualize() computes
    // the correct time-since-simulated on the next load, and notify the
    // distribution grid tracker.
    for( const point_abs_sm &col : last_demoted_columns_ ) {
        for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            const tripoint_abs_sm sm_pos{ col, z };
            const auto it = submaps.find( sm_pos );
            if( it != submaps.end() && it->second ) {
                it->second->last_touched = calendar::turn;
            }
            if( auto *tracker = get_distribution_grid_tracker_for( dimension_id_ ) ) {
                tracker->on_submap_unloaded( sm_pos, dimension_id_ );
            }
        }
    }

    // Rebuild simulated islands from the new column set.
    simulated_islands_ = build_islands( columns );

    // Rebuild column → island index map and resize per-island sound queues.
    column_to_island_.clear();
    column_to_island_.reserve( columns.size() );
    island_sounds_.resize( simulated_islands_.size() );
    for( size_t idx = 0; idx < simulated_islands_.size(); ++idx ) {
        const simulated_island &isl = simulated_islands_[idx];
        for( int y = isl.begin.y(); y < isl.end.y(); ++y ) {
            for( int x = isl.begin.x(); x < isl.end.x(); ++x ) {
                const point_abs_sm p( x, y );
                if( isl.contains( p ) ) {
                    column_to_island_[p] = idx;
                }
            }
        }
        island_sounds_[idx].clear();
    }
}

void mapbuffer::remove_submap( tripoint_abs_sm addr )
{
    auto m_target = submaps.find( addr );
    if( m_target == submaps.end() ) {
        debugmsg( "Tried to remove non-existing submap %s", addr.to_string() );
        return;
    }
    // Safety: skip freeing if submap still in bubble
    if( g != nullptr && m_target->second ) {
        const submap *doomed = m_target->second.get();
        const map &here = g->m;
        if( here.inbounds( addr ) ) {
            debugmsg( "remove_submap: skipping free of submap at %s (ptr %p) "
                      "— reality bubble still references it (dim='%s')",
                      addr.to_string(), static_cast<const void *>( doomed ),
                      dimension_id_.c_str() );
            return;  // do NOT erase — prevent use-after-free
        }
    }
    unregister_submap_vehicles( addr );
    submaps_with_active_items_.erase( addr );
    submaps_with_luminous_items_.erase( addr );
    submaps.erase( m_target );
}

void mapbuffer::transfer_all_to( mapbuffer &dest )
{
    for( auto &kv : submaps ) {
        if( dest.submaps.count( kv.first ) ) {
            // Destination already has a submap at this position.  This should
            // never happen when the callers (capture_from_primary /
            // restore_to_primary) clear the destination first.  Log an error
            // and keep the destination entry rather than silently losing either.
            debugmsg( "transfer_all_to: collision at %s; destination entry retained, source lost",
                      kv.first.to_string() );
            continue;
        }
        dest.register_submap_vehicles( kv.first, *kv.second );
        if( !kv.second->active_items.empty() ) {
            dest.submaps_with_active_items_.insert( kv.first );
        }
        if( std::ranges::any_of( ::submap_tiles(), [&]( const point_sm_ms & pos ) {
        return kv.second->get_lum( pos ) != 0;
        } ) ) {
            dest.submaps_with_luminous_items_.insert( kv.first );
        }
        dest.submaps.emplace( kv.first, std::move( kv.second ) );
    }
    // Transfer column state cache entries for each moved submap.
    for( auto &kv : column_states_ ) {
        // Only transfer entries whose submap was actually moved.
        if( submaps.find( tripoint_abs_sm( kv.first, 0 ) ) == submaps.end() ) {
            continue;
        }
        if( dest.column_states_.count( kv.first ) ) {
            // Destination already tracks this column — keep the higher state.
            auto &dst = dest.column_states_[kv.first];
            dst = std::max( dst, kv.second );
        } else {
            dest.column_states_.emplace( kv.first, kv.second );
        }
        if( dirty_columns_.contains( kv.first ) ) {
            dest.dirty_columns_.insert( kv.first );
        }
    }

    loaded_vehicles_.clear();
    vehicle_footprint_by_location_.clear();
    vehicle_footprint_locations_.clear();
    submaps_with_active_items_.clear();
    submaps_with_luminous_items_.clear();
    column_states_.clear();
    dirty_columns_.clear();
    submaps.clear();
}

submap *mapbuffer::load_submap( const tripoint_abs_sm &pos )
{
    ZoneScoped;
    // lookup_submap already handles the disk-read path transparently.
    return lookup_submap( pos );
}

void mapbuffer::unload_omt( const tripoint_abs_omt &omt_addr )
{
    // Hold the mutex for the entire save+erase so that background lazy-border
    // preload_omt() workers (which acquire the mutex per add_submap()) cannot
    // race with our submaps.find()/erase() calls.
    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );

    const auto base = project_to<coords::sm>( omt_addr );
    const std::array<tripoint_abs_sm, 4> addrs = { {
            base,
            base + point_rel_sm::east(),
            base + point_rel_sm::south(),
            base + point_rel_sm::south_east()
        }
    };

    // Check dirty state: if any of the four column positions was dirty,
    // serialise the entire OMT into pending_writes before erasing.
    const bool is_dirty = std::ranges::any_of( addrs, [&]( const tripoint_abs_sm & addr ) {
        return dirty_columns_.contains( addr.xy() );
    } );

    std::list<tripoint_abs_sm> to_delete;

    if( is_dirty ) {
        bool all_uniform = true;
        for( const tripoint_abs_sm &addr : addrs ) {
            const auto it = submaps.find( addr );
            if( it != submaps.end() && it->second && !it->second->is_uniform ) {
                all_uniform = false;
                break;
            }
        }

        if( !all_uniform && !disable_mapgen ) {
            std::ostringstream buf;
            {
                JsonOut jsout( buf );
                jsout.start_array();
                for( const tripoint_abs_sm &addr : addrs ) {
                    const auto it = submaps.find( addr );
                    if( it == submaps.end() || !it->second ) {
                        continue;
                    }
                    jsout.start_object();
                    jsout.member( "version", savegame_version );
                    jsout.member( "coordinates" );
                    jsout.start_array();
                    jsout.write( addr.x() );
                    jsout.write( addr.y() );
                    jsout.write( addr.z() );
                    jsout.end_array();
                    it->second->store( jsout );
                    jsout.end_object();
                }
                jsout.end_array();
            }
            std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
            pending_writes_[omt_addr] = std::move( buf ).str();
        }

        for( const tripoint_abs_sm &addr : addrs ) {
            if( submaps.contains( addr ) ) {
                to_delete.push_back( addr );
            }
        }
    } else {
        // Not dirty: content is identical to what is already on disk.
        // Skip serialisation; just collect the four submap addresses to discard.
        for( const auto &addr : addrs ) {
            to_delete.push_back( addr );
        }
    }

    // Safety: skip freeing submaps that map::grid[] still references.
    // This prevents use-after-free when submap_loader eviction races with
    // map::shift() / copy_grid() during large map shifts (e.g. pocket entry).
    if( g != nullptr ) {
        const map &here = g->m;
        to_delete.remove_if( [&]( const tripoint_abs_sm & p ) {
            const auto it = submaps.find( p );
            if( it == submaps.end() || !it->second ) {
                return false;
            }
            const submap *doomed = it->second.get();
            if( here.inbounds( p ) ) {
                debugmsg( "unload_omt: skipping free of submap at %s (ptr %p) "
                          "— reality bubble still references it (dim='%s')",
                          p.to_string(), static_cast<const void *>( doomed ),
                          dimension_id_.c_str() );
                return true;  // remove from to_delete → keep alive
            }
            return false;
        } );
    }

    for( const auto &p : to_delete ) {
        unregister_submap_vehicles( p );
        submaps_with_active_items_.erase( p );
        // Clear column state and dirty flag on eviction.
        column_states_.erase( p.xy() );
        dirty_columns_.erase( p.xy() );
        submaps.erase( p );
    }
}

submap *mapbuffer::lookup_submap( const tripoint_abs_sm &p )
{
    // Fast path: submap already resident in memory.
    auto *resident_sm = static_cast<submap *>( nullptr );
    {
        resident_sm = lookup_submap_in_memory( p );
    }
    if( resident_sm != nullptr ) {
        return resident_sm;
    }

    // Cache miss — perform disk I/O outside submaps_mutex_ so that concurrent
    // preload_omt() workers on other omts are not stalled behind this call.
    const tripoint_abs_omt omt_addr = project_to<coords::omt>( p );

    std::string pending_data;
    {
        std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
        const auto it = pending_writes_.find( omt_addr );
        if( it != pending_writes_.end() ) {
            pending_data = std::move( it->second );
            pending_writes_.erase( it );
        }
    }

    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> loaded;
    auto already_loaded = [this]( const tripoint_abs_sm & q ) {
        return lookup_submap_in_memory( q ) != nullptr;
    };

    try {
        bool found = false;
        if( !pending_data.empty() ) {
            std::istringstream iss( pending_data );
            JsonIn jsin( iss );
            deserialize_into_vec( jsin, loaded, already_loaded );
            found = true;
        } else {
            found = g->get_active_world()->read_map_omt( dimension_id_.str(), omt_addr,
            [this, &loaded, &already_loaded]( JsonIn & jsin ) {
                deserialize_into_vec( jsin, loaded, already_loaded );
            } );
        }
        if( !found ) {
            return nullptr;
        }
    } catch( const std::exception &err ) {
        debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        return nullptr;
    }

    {
        for( auto &[pos, sm] : loaded ) {
            if( !add_submap( pos, sm ) ) {
                DebugLog( DL::Warn, DC::Map ) << string_format(
                                                  "lookup_submap: submap %d,%d,%d already loaded; keeping in-memory version",
                                                  pos.x(), pos.y(), pos.z() );
            }
        }
    }
    auto *result = static_cast<submap *>( nullptr );
    {
        result = lookup_submap_in_memory( p );
    }
    if( !result ) {
        debugmsg( "file did not contain the expected submap %d,%d,%d", p.x(), p.y(), p.z() );
    }
    return result;
}

auto mapbuffer::get_submap( const tripoint_abs_sm &p,
                            const mapbuffer_lookup_options options ) -> submap *
{
    switch( options.mode ) {
        case mapbuffer_lookup_mode::simulated_only:
            if( !submap_loader.is_simulated( dimension_id_, p ) ) {
                return nullptr;
            }
            return lookup_submap_in_memory( p );
        case mapbuffer_lookup_mode::resident_only:
            return lookup_submap_in_memory( p );
        case mapbuffer_lookup_mode::load_from_disk:
            return lookup_submap( p );
        case mapbuffer_lookup_mode::load_or_generate:
            if( auto *sm = lookup_submap( p ) ) {
                return sm;
            }
            generate_omt( project_to<coords::omt>( p ) );
            return lookup_submap_in_memory( p );
    }

    return nullptr;
}

auto mapbuffer::for_each_simulated_submap_position(
    const std::function<void( const tripoint_abs_sm & )> &fn,
    const std::optional<int> zlev_filter ) const -> void
{
    if( zlev_filter && ( *zlev_filter < -OVERMAP_DEPTH || *zlev_filter > OVERMAP_HEIGHT ) ) {
        return;
    }

    const auto horizontal_positions = submap_loader.simulated_submaps( dimension_id_ );
    if( horizontal_positions.empty() ) {
        std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
        for( const std::pair<const tripoint_abs_sm, std::unique_ptr<submap>> &entry : submaps ) {
            const auto abs_pos = tripoint_abs_sm( entry.first );
            if( entry.second && ( !zlev_filter || abs_pos.z() == *zlev_filter ) &&
                submap_loader.is_simulated( dimension_id_, abs_pos ) ) {
                fn( abs_pos );
            }
        }
        return;
    }

    for( const point_abs_sm &pos : horizontal_positions ) {
        if( zlev_filter ) {
            fn( tripoint_abs_sm( pos, *zlev_filter ) );
            continue;
        }
        for( const auto current_zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            fn( tripoint_abs_sm( pos, current_zlev ) );
        }
    }
}

auto mapbuffer::for_each_simulated_submap(
    const std::function<void( const tripoint_abs_sm &, submap & )> &fn ) -> void
{
    for_each_simulated_submap_position( [&]( const tripoint_abs_sm & pos ) {
        auto *const sm = lookup_submap_in_memory( pos );
        if( sm != nullptr ) {
            fn( pos, *sm );
        }
    } );
}

auto mapbuffer::simulated_submap_positions() const -> std::vector<tripoint_abs_sm>
{
    auto result = std::vector<tripoint_abs_sm> {};
    const auto horizontal_positions = submap_loader.simulated_submaps( dimension_id_ );
    result.reserve( horizontal_positions.empty() ? loaded_submap_count() :
                    horizontal_positions.size() * static_cast<std::size_t>( OVERMAP_LAYERS ) );
    for_each_simulated_submap_position( [&]( const tripoint_abs_sm & pos ) {
        result.push_back( pos );
    } );
    return result;
}

auto mapbuffer::mark_submap_caches_dirty(
    const mapbuffer_mark_submap_caches_dirty_options &options ) -> void
{
    if( options.zlev < -OVERMAP_DEPTH || options.zlev > OVERMAP_HEIGHT ) {
        return;
    }
    if( options.begin.x() >= options.end.x() || options.begin.y() >= options.end.y() ) {
        return;
    }

    const auto max = point_abs_sm( options.end.x() - 1, options.end.y() - 1 );
    for( const auto pos : point_range<point_abs_sm>( options.begin, max ) ) {
        auto *const sm = lookup_submap_in_memory( tripoint_abs_sm( pos, options.zlev ) );
        if( sm == nullptr ) {
            continue;
        }
        sm->transparency_dirty = sm->transparency_dirty || options.transparency;
        sm->floor_dirty = sm->floor_dirty || options.floor;
        sm->absorption_dirty = sm->absorption_dirty || options.absorption;
        sm->pf_dirty = sm->pf_dirty || options.pathfinding;
    }
}

auto mapbuffer::queue_sound( sound_event evt ) -> void
{
    const auto col = project_to<coords::sm>( evt.origin.xy() );
    const auto it = column_to_island_.find( col );
    if( it != column_to_island_.end() ) {
        island_sounds_[it->second].push_back( std::move( evt ) );
    }
    // Sound origin not in a simulated column — discard.
}

auto mapbuffer::island_sounds_for( point_abs_sm col ) -> std::vector<sound_event> *
{
    const auto it = column_to_island_.find( col );
    if( it != column_to_island_.end() ) {
        return &island_sounds_[it->second];
    }
    return nullptr;
}

auto mapbuffer::island_for( point_abs_sm col ) const -> const simulated_island *
{
    const auto it = column_to_island_.find( col );
    if( it != column_to_island_.end() ) {
        return &simulated_islands_[it->second];
    }
    return nullptr;
}

auto mapbuffer::clear_spawns( const mapbuffer_submap_bounds_mutation_options &options ) -> void
{
    if( options.begin.x() >= options.end.x() || options.begin.y() >= options.end.y() ) {
        return;
    }

    const auto max = point_abs_sm( options.end.x() - 1, options.end.y() - 1 );
    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        for( const auto pos : point_range<point_abs_sm>( options.begin, max ) ) {
            auto *const sm = get_submap( tripoint_abs_sm( pos, zlev ), options.lookup );
            if( sm == nullptr ) {
                continue;
            }
            sm->spawns.clear();
        }
    }
}

auto mapbuffer::clear_traps( const mapbuffer_submap_bounds_mutation_options &options ) -> void
{
    if( options.begin.x() >= options.end.x() || options.begin.y() >= options.end.y() ) {
        return;
    }

    const auto max = point_abs_sm( options.end.x() - 1, options.end.y() - 1 );
    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        for( const auto pos : point_range<point_abs_sm>( options.begin, max ) ) {
            auto *const sm = get_submap( tripoint_abs_sm( pos, zlev ), options.lookup );
            if( sm == nullptr ) {
                continue;
            }
            for( const auto local : ::submap_tiles() ) {
                if( sm->get_trap( local ) == tr_null ) {
                    continue;
                }
                set_trap( project_combine( tripoint_abs_sm( pos, zlev ), local ), tr_null,
                          options.lookup );
            }
            sm->set_all_traps( tr_null );
        }
    }
}

auto mapbuffer::fill_terrain( const mapbuffer_fill_terrain_options &options ) -> void
{
    if( options.begin.x() >= options.end.x() || options.begin.y() >= options.end.y() ) {
        return;
    }

    const auto max = point_abs_sm( options.end.x() - 1, options.end.y() - 1 );
    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        for( const auto pos : point_range<point_abs_sm>( options.begin, max ) ) {
            auto *const sm = get_submap( tripoint_abs_sm( pos, zlev ), options.lookup );
            if( sm == nullptr ) {
                continue;
            }
            sm->is_uniform = true;
            sm->set_all_ter( options.terrain );
        }
    }
}

auto mapbuffer::run_submap_batch_turns(
    const mapbuffer_run_submap_batch_turns_options &options ) -> void
{
    if( options.turns <= 0 ) {
        return;
    }
    if( options.begin.x() >= options.end.x() || options.begin.y() >= options.end.y() ) {
        return;
    }

    const auto max = point_abs_sm( options.end.x() - 1, options.end.y() - 1 );
    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        for( const auto pos : point_range<point_abs_sm>( options.begin, max ) ) {
            auto *const sm = get_submap( tripoint_abs_sm( pos, zlev ), options.lookup );
            if( sm == nullptr ) {
                continue;
            }
            ::run_submap_batch_turns( *sm, options.turns );
            sm->last_touched = calendar::turn;
        }
    }
}

auto mapbuffer::creature_tracker() -> Creature_tracker &
{
    return creature_tracker_;
}

auto mapbuffer::creature_tracker() const -> const Creature_tracker &
{
    return creature_tracker_;
}

auto mapbuffer::find_active_npc( const tripoint_abs_ms &p ) const -> shared_ptr_fast<npc>
{
    const auto iter = active_npcs_by_location_.find( p );
    if( iter != active_npcs_by_location_.end() ) {
        const auto &guy = iter->second;
        if( guy && !guy->is_dead() ) {
            return guy;
        }
    }
    return nullptr;
}

auto mapbuffer::creature_at( const tripoint_abs_ms &p,
                             const bool allow_hallucination )
const -> Creature * // *NOPAD*
{
    if( const auto mon_ptr = creature_tracker_.find( p ) ) {
        if( allow_hallucination || !mon_ptr->is_hallucination() ) {
            return mon_ptr.get();
        }
        return nullptr;
    }
    if( g != nullptr && dimension_id_ == g->get_current_dimension_id() && g->u.abs_pos() == p ) {
        return &g->u;
    }
    if( const auto guy = find_active_npc( p ) ) {
        return guy.get();
    }
    return nullptr;
}

auto mapbuffer::has_creature_at(
    const tripoint_abs_ms &p,
    const bool allow_hallucination ) const -> bool // *NOPAD*
{
    return creature_at( p, allow_hallucination ) != nullptr;
}

auto mapbuffer::tile_empty( const tripoint_abs_ms &p ) -> bool
{
    return ( passable( p ) || has_flag( TFLAG_LIQUID, p ) ) && !has_creature_at( p );
}

auto mapbuffer::add_active_npc( const shared_ptr_fast<npc> &guy ) -> bool
{
    if( !guy || guy->is_dead() ) {
        return false;
    }
    if( guy->get_dimension() != dimension_id_ ) {
        debugmsg( "Tried to add NPC %s to dimension '%s' tracker, but NPC is in '%s'",
                  guy->get_name(), dimension_id_.c_str(), guy->get_dimension().c_str() );
        return false;
    }
    if( const auto existing = find_active_npc( guy->abs_pos() ) ) {
        if( existing.get() != guy.get() ) {
            debugmsg( "Tried to add NPC %s to occupied active NPC tile %s",
                      guy->get_name(), guy->abs_pos().to_string() );
            return false;
        }
        return true;
    }
    const auto iter = std::ranges::find_if( active_npcs_,
    [&]( const shared_ptr_fast<npc> &existing ) {
        return existing.get() == guy.get();
    } );
    if( iter == active_npcs_.end() ) {
        active_npcs_.push_back( guy );
    }
    active_npcs_by_location_[guy->abs_pos()] = guy;
    return true;
}

auto mapbuffer::remove_active_npc_from_location_map( const npc &guy ) -> void
{
    const auto pos_iter = active_npcs_by_location_.find( guy.abs_pos() );
    if( pos_iter != active_npcs_by_location_.end() && pos_iter->second.get() == &guy ) {
        active_npcs_by_location_.erase( pos_iter );
        return;
    }

    const auto iter = std::ranges::find_if( active_npcs_by_location_,
    [&]( const decltype( active_npcs_by_location_ )::value_type & v ) {
        return v.second.get() == &guy;
    } );
    if( iter != active_npcs_by_location_.end() ) {
        active_npcs_by_location_.erase( iter );
    }
}

auto mapbuffer::update_active_npc_pos( const npc &guy, const tripoint_abs_ms &new_pos ) -> bool
{
    if( guy.is_dead() ) {
        remove_active_npc_from_location_map( guy );
        return true;
    }

    if( const auto existing = find_active_npc( new_pos ) ) {
        if( existing.get() != &guy ) {
            debugmsg( "Tried to move NPC %s to occupied active NPC tile %s",
                      guy.get_name(), new_pos.to_string() );
            return false;
        }
    }

    const auto iter = std::ranges::find_if( active_npcs_,
    [&]( const shared_ptr_fast<npc> &existing ) {
        return existing.get() == &guy;
    } );
    if( iter == active_npcs_.end() ) {
        return false;
    }

    remove_active_npc_from_location_map( guy );
    active_npcs_by_location_[new_pos] = *iter;
    return true;
}

auto mapbuffer::remove_active_npc( const npc &guy ) -> void
{
    remove_active_npc_from_location_map( guy );
    const auto iter = std::ranges::find_if( active_npcs_,
    [&]( const shared_ptr_fast<npc> &existing ) {
        return existing.get() == &guy;
    } );
    if( iter != active_npcs_.end() ) {
        active_npcs_.erase( iter );
    }
}

auto mapbuffer::has_loaded_vehicle( const vehicle *veh ) const -> bool
{
    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );
    return loaded_vehicles_.contains( const_cast<vehicle *>( veh ) );
}

auto mapbuffer::register_vehicle( vehicle *veh ) -> void
{
    if( veh == nullptr ) {
        return;
    }

    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );
    veh->set_dimension( dimension_id_ );
    loaded_vehicles_.insert( veh );
    index_vehicle_footprint_unlocked( *veh );
}

auto mapbuffer::unregister_vehicle( vehicle *veh ) -> void
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    unindex_vehicle_footprint_unlocked( veh );
    loaded_vehicles_.erase( veh );

    // A footprint-promoted vehicle can be present in the map-side caches even
    // when its pivot is outside the bubble.  Remove every such reference
    // before the owning submap releases the vehicle object.
    if( veh != nullptr && g != nullptr && g->m.get_bound_dimension() == dimension_id_ ) {
        map &here = g->m;
        for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            auto &cache = here.get_cache( zlev );
            cache.vehicle_list.erase( veh );
            cache.zone_vehicles.erase( veh );
        }
        for( const vpart_reference &vpr : veh->get_all_parts() ) {
            if( !vpr.part().removed ) {
                const auto part_pos = veh->abs_part_location( vpr.part() );
                if( !here.inbounds_z( part_pos.z() ) ) {
                    continue;
                }
                here.clear_vehicle_point_from_cache( veh,
                                                     abs_to_map_local( here,
                                                             part_pos ) );
            }
        }
        here.last_full_vehicle_list_dirty = true;
    }
}

auto mapbuffer::refresh_vehicle_footprint( vehicle *veh ) -> void
{
    if( veh == nullptr ) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    if( !loaded_vehicles_.contains( veh ) ) {
        return;
    }
    index_vehicle_footprint_unlocked( *veh );

    // A vehicle can remain resident with its pivot outside the active bubble
    // while one or more parts overlap the bubble.  Keep the map-side cache in
    // sync with the absolute footprint index so rendering and collision
    // queries see those parts as well.
    const auto footprints = calculate_vehicle_submap_footprints( *veh );
    const auto overlaps_active_bubble = std::ranges::any_of( footprints,
    [&]( const auto & footprint ) {
        return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this,
                *footprint );
    } );
    if( !overlaps_active_bubble ) {
        return;
    }

    map &here = get_map();
    here.invalidate_max_populated_zlev( veh->abs_sm_pos.z() );
    auto &cache = here.get_cache( veh->abs_sm_pos.z() );
    cache.vehicle_list.insert( veh );
    if( !veh->loot_zones.empty() ) {
        cache.zone_vehicles.insert( veh );
    }
    here.add_vehicle_to_cache( veh );
}

auto mapbuffer::invalidate_vehicle_footprint( const vehicle &veh ) -> bool
{
    return invalidate_active_vehicle_footprints( *this, calculate_vehicle_submap_footprints( veh ) );
}

auto mapbuffer::get_vehicle_submap_footprints( const vehicle &veh ) const
-> vehicle_submap_footprints // *NOPAD*
{
    return calculate_vehicle_submap_footprints( veh );
}

auto mapbuffer::refresh_vehicle_registry_for_submap( const tripoint_abs_sm &p,
        const mapbuffer_lookup_options options ) -> void
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    unregister_submap_vehicles( p );
    auto *const sm = get_submap( p, options );
    if( sm == nullptr ) {
        return;
    }
    register_submap_vehicles( p, *sm );
}

// ----- Vehicle movement / collision functions -----

static const trait_id trait_PROF_SKATER( "PROF_SKATER" );
static const trait_id trait_DEFT( "DEFT" );
static const skill_id skill_driving( "driving" );

// -- Rail detection helpers (absolute-coordinate versions) --

static int get_num_cw_rots_of_ray_delta( point v )
{
    if( v == point_north_east ) {
        return 0;
    } else if( v == point_south_east ) {
        return 1;
    } else if( v == point_south_west ) {
        return 2;
    } else {
        return 3;
    }
}

static bool has_rail_at_abs( const mapbuffer &buf, const tripoint_abs_ms &p )
{
    auto tile = abs_tile_handle::fetch( const_cast<mapbuffer &>( buf ), p );
    if( !tile ) {
        return false;
    }
    if( tile->has_flag_ter_or_furn( "RAIL" ) ) {
        return true;
    }
    if( !tile->has_flag_ter_or_furn( "NO_FLOOR" ) ) {
        return false;
    }
    // Check vertical neighbors for rails through open floor
    const auto neighbors = std::array<tripoint_abs_ms, 2> {
        p + tripoint_rel_ms( 0, 0, 1 ),
        p + tripoint_rel_ms( 0, 0, -1 ),
    };
    return std::ranges::any_of( neighbors, [&]( const tripoint_abs_ms & candidate ) {
        auto ct = abs_tile_handle::fetch( const_cast<mapbuffer &>( buf ), candidate );
        return ct && ct->has_flag_ter_or_furn( "RAIL" );
    } );
}

static bool scan_rails_from_veh_abs( const mapbuffer &buf, const vehicle &veh,
                                     tripoint_abs_ms scan_initial_pos,
                                     point veh_plus_y_vec, point scan_vec )
{
    for( size_t rail_id = 0; rail_id < veh.rail_profile.size(); rail_id++ ) {
        int rail_y_rel_to_pivot = veh.rail_profile[rail_id] - veh.pivot_point().y();
        tripoint_abs_ms scan_pos = scan_initial_pos + rail_y_rel_to_pivot * veh_plus_y_vec;
        for( int step = 0; step < 3; step++ ) {
            tripoint_abs_ms p = scan_pos + scan_vec * step;
            if( !has_rail_at_abs( buf, p ) ) {
                return false;
            }
        }
    }
    return true;
}

static bool scan_rails_at_shift_abs( const mapbuffer &buf, const vehicle &veh,
                                     int velocity_sign, units::angle dir,
                                     int shift_sign )
{
    point ray_delta;
    {
        tileray ray( dir );
        ray.advance( 1 );
        ray_delta.x = ray.dx();
        ray_delta.y = ray.dy();
    }
    if( ray_delta.x != 0 && ray_delta.y != 0 ) {
        int num_cw_rots = get_num_cw_rots_of_ray_delta( ray_delta );
        point rd_l = point_north.rotate( num_cw_rots );
        point rd_r = point_east.rotate( num_cw_rots );
        point vyp_l = point_east.rotate( num_cw_rots );
        point vyp_r = point_south.rotate( num_cw_rots );

        tripoint_abs_ms scan_start = veh.abs_ms_location();
        if( shift_sign > 0 ) {
            scan_start += rd_r * velocity_sign;
        } else if( shift_sign < 0 ) {
            scan_start += rd_l * velocity_sign;
        }

        point scan_vec = ray_delta * velocity_sign;

        bool scan_res_l = scan_rails_from_veh_abs( buf, veh, scan_start, vyp_l, scan_vec );
        bool scan_res_r = scan_rails_from_veh_abs( buf, veh, scan_start, vyp_r, scan_vec );
        return scan_res_l || scan_res_r;
    } else {
        point veh_plus_y_vec = ray_delta.rotate( 1 );
        point scan_vec = ray_delta * velocity_sign;
        tripoint_abs_ms scan_start = veh.abs_ms_location();
        if( shift_sign != 0 ) {
            scan_start += scan_vec + veh_plus_y_vec * shift_sign;
        }
        return scan_rails_from_veh_abs( buf, veh, scan_start, veh_plus_y_vec, scan_vec );
    }
}

static bool vehicle_on_rails_abs( const mapbuffer &buf, const vehicle &veh )
{
    if( !veh.can_use_rails() ) {
        return false;
    }

    int face_dir_degrees = std::round( units::to_degrees( veh.face.dir() ) );
    int face_dir_snapped = ( face_dir_degrees / 45 ) * 45;

    if( face_dir_degrees != face_dir_snapped ) {
        return false;
    }

    units::angle dir_straight = normalize( units::from_degrees( face_dir_snapped ) );
    return scan_rails_at_shift_abs( buf, veh, 1, dir_straight, 0 ) ||
           scan_rails_at_shift_abs( buf, veh, -1, dir_straight, 0 );
}

auto mapbuffer::obstructed_by_vehicle_rotation( const tripoint_abs_ms &from,
        const tripoint_abs_ms &to ) const -> bool
{
    if( from.xy() == to.xy() && from.z() == to.z() ) {
        return false;
    }

    if( from.z() != to.z() ) {
        // Split into two checks, one for each z level
        const tripoint_abs_ms flattened( from.xy(), to.z() );
        if( obstructed_by_vehicle_rotation( flattened, to ) ) {
            return true;
        }
    }

    // Authoritative fallback: check loaded vehicle parts directly.  This also
    // serves as a diagnostic oracle for the derived in-bubble cache.
    const auto check_vehicle_block = [this]( const tripoint_abs_ms & part_tile,
    const tripoint_abs_ms & neighbor ) -> bool {
        const auto vp = const_cast<mapbuffer &>( *this ).veh_at( part_tile );
        if( !vp )
        {
            return false;
        }
        const vehicle *v = &vp->vehicle();
        const tripoint_mnt_veh neighbor_mount = v->abs_to_mount( neighbor );
        return !v->allowed_move( neighbor_mount, vp->mount() );
    };

    const auto direct_obstruction = [&]() {
        const auto delta = to - from;

        if( delta == tripoint_rel_ms::north_west() ||
            delta == tripoint_rel_ms::north_east() ) {
            return check_vehicle_block( from, to );
        }

        // For SW and SE, check the destination tile as the cache does.
        const auto neg_delta = from - to;
        if( neg_delta == tripoint_rel_ms::north_west() ||
            neg_delta == tripoint_rel_ms::north_east() ) {
            return check_vehicle_block( to, from );
        }

        return false;
    };

    // The cache is valid only when both endpoints are represented by the
    // current reality-bubble frame.  A teleport can leave one endpoint inside
    // the old frame while the other is outside it.
    const auto local_from = active_reality_bubble_local( from );
    const auto local_to = active_reality_bubble_local( to );
    if( !local_from || !local_to || local_from->z() != local_to->z() ) {
        return direct_obstruction();
    }

    map &here = get_map();
    const level_cache &lc = here.get_cache_ref( local_from->z() );
    if( !lc.veh_in_active_range || lc.vehicle_obstruction_cache_dirty ) {
        return direct_obstruction();
    }
    const auto &cache = lc.vehicle_obstructed_cache;
    const auto delta = local_to->xy() - local_from->xy();
    auto cached_obstruction = false;
    if( delta == point_rel_ms::north_west() ) {
        cached_obstruction = cache[lc.idx( local_from->x(), local_from->y() )].nw;
    } else if( delta == point_rel_ms::north_east() ) {
        cached_obstruction = cache[lc.idx( local_from->x(), local_from->y() )].ne;
    } else if( delta == point_rel_ms::south_west() ) {
        cached_obstruction = cache[lc.idx( local_to->x(), local_to->y() )].ne;
    } else if( delta == point_rel_ms::south_east() ) {
        cached_obstruction = cache[lc.idx( local_to->x(), local_to->y() )].nw;
    } else {
        return false;
    }

    return cached_obstruction;
}

auto mapbuffer::vehicle_wheel_traction( const vehicle &veh,
                                        const bool ignore_movement_modifiers ) const -> float
{
    if( veh.is_in_water( true ) ) {
        return veh.can_float() ? 1.0f : -1.0f;
    }
    if( veh.is_in_water() && veh.is_watercraft() && veh.can_float() ) {
        return 1.0f;
    }
    if( veh.is_flying_in_air() ) {
        return ( veh.has_lift() ) ? 1.0f : -1.0f;
    }

    const auto &wheel_indices = veh.wheelcache;
    const int num_wheels = wheel_indices.size();
    if( num_wheels == 0 ) {
        return 0.0f;
    }

    // Check rails — use fast path when in bubble, absolute-coordinate slow path otherwise
    bool on_rails = false;
    if( const auto local = active_reality_bubble_local( veh.abs_ms_location() ) ) {
        map &here = get_map();
        on_rails = vehicle_movement::is_on_rails( here, veh );
    } else {
        on_rails = vehicle_on_rails_abs( *this, veh );
    }

    if( on_rails ) {
        float traction_wheel_area = 0.0f;
        for( const int p : veh.rail_wheelcache ) {
            traction_wheel_area += veh.cpart( p ).wheel_area();
        }
        return traction_wheel_area;
    }

    float traction_wheel_area = 0.0f;
    for( const int p : wheel_indices ) {
        const auto &pp = veh.abs_part_location( p );
        const int wheel_area = veh.cpart( p ).wheel_area();

        auto tile = abs_tile_handle::fetch( const_cast<mapbuffer &>( *this ), pp );
        if( !tile ) {
            continue;
        }
        const auto &tr = tile->ter_obj();
        // Deep water and air
        if( tr.has_flag( TFLAG_DEEP_WATER ) || tr.has_flag( TFLAG_NO_FLOOR ) ) {
            continue;
        }

        int move_mod = tile->move_cost_ter_furn();
        if( move_mod == 0 ) {
            return 0.0f;
        }

        // Use const terrain_mod lookup from vehicle part info
        for( const auto &terrain_mod : veh.part_info( p ).wheel_terrain_mod() ) {
            if( !tr.has_flag( terrain_mod.first ) ) {
                move_mod += terrain_mod.second;
                break;
            }
        }

        if( ignore_movement_modifiers ) {
            move_mod = 2;
        }

        traction_wheel_area += 2.0 * wheel_area / move_mod;
    }

    return traction_wheel_area;
}

auto mapbuffer::vehicle_vehicle_collision( vehicle &veh, vehicle &veh2,
        const std::vector<veh_collision> &collisions ) -> float
{
    if( &veh == &veh2 ) {
        debugmsg( "Vehicle %s collided with itself", veh.name );
        return 0.0f;
    }

    const veh_collision &c = collisions[0];
    add_msg( m_bad, _( "The %1$s's %2$s collides with %3$s's %4$s." ),
             veh.name, veh.part_info( c.part ).name(),
             veh2.name, veh2.part_info( c.target_part ).name() );

    const bool vertical = veh.abs_sm_pos.z() != veh2.abs_sm_pos.z();

    tripoint_rel_veh epicenter1;
    tripoint_rel_veh epicenter2;

    float veh1_impulse = 0;
    float veh2_impulse = 0;
    float delta_vel = 0;
    const float dmg_adjust = impulse_to_damage( 1 );
    float dmg_veh1 = 0;
    float dmg_veh2 = 0;

    if( !vertical ) {
        rl_vec2d velo_veh1 = veh.velo_vec();
        rl_vec2d velo_veh2 = veh2.velo_vec();
        const float m1 = to_kilogram( veh.total_mass() );
        const float m2 = to_kilogram( veh2.total_mass() );

        tripoint_mnt_veh cof1 = veh.rotated_center_of_mass();
        tripoint_mnt_veh cof2 = veh2.rotated_center_of_mass();
        int &x_cof1 = cof1.x();
        int &y_cof1 = cof1.y();
        int &x_cof2 = cof2.x();
        int &y_cof2 = cof2.y();

        // Collision axis is based on absolute positions
        rl_vec2d collision_axis_y;
        collision_axis_y.x = ( veh.abs_ms_location().x() + x_cof1 ) -
                             ( veh2.abs_ms_location().x() + x_cof2 );
        collision_axis_y.y = ( veh.abs_ms_location().y() + y_cof1 ) -
                             ( veh2.abs_ms_location().y() + y_cof2 );
        collision_axis_y = collision_axis_y.normalized();
        rl_vec2d collision_axis_x = collision_axis_y.rotated( M_PI / 2 );

        float vel1_y = cmps_to_mps( collision_axis_y.dot_product( velo_veh1 ) );
        float vel1_x = cmps_to_mps( collision_axis_x.dot_product( velo_veh1 ) );
        float vel2_y = cmps_to_mps( collision_axis_y.dot_product( velo_veh2 ) );
        float vel2_x = cmps_to_mps( collision_axis_x.dot_product( velo_veh2 ) );
        delta_vel = std::abs( vel1_y - vel2_y );
        float e = get_collision_factor( vel1_y - vel2_y );
        add_msg( m_debug, "Requested collision factor, received %.2f", e );

        float vel1_x_a = vel1_x;
        float vel2_x_a = vel2_x;
        float vel1_y_a = ( ( m2 * vel2_y * ( 1 + e ) + vel1_y * ( m1 - m2 * e ) ) / ( m1 + m2 ) );
        float vel2_y_a = ( ( m1 * vel1_y * ( 1 + e ) + vel2_y * ( m2 - m1 * e ) ) / ( m1 + m2 ) );

        rl_vec2d final1 = ( collision_axis_y * vel1_y_a + collision_axis_x * vel1_x_a ) * 100.0;
        rl_vec2d final2 = ( collision_axis_y * vel2_y_a + collision_axis_x * vel2_x_a ) * 100.0;

        veh.move.init( point_rel_ms( final1.as_point() ) );
        if( final1.dot_product( veh.face_vec() ) < 0 ) {
            veh.velocity = -final1.magnitude();
        } else {
            veh.velocity = final1.magnitude();
        }

        veh2.move.init( point_rel_ms( final2.as_point() ) );
        if( final2.dot_product( veh2.face_vec() ) < 0 ) {
            veh2.velocity = -final2.magnitude();
        } else {
            veh2.velocity = final2.magnitude();
        }

        float avg_of_turn = ( veh2.of_turn + veh.of_turn ) / 2;
        if( avg_of_turn < .1f ) {
            avg_of_turn = .1f;
        }

        veh.of_turn = avg_of_turn * .9;
        veh2.of_turn = std::max( 1.0f, avg_of_turn * 1.1f );

        veh1_impulse = std::abs( m1 * ( vel1_y_a - vel1_y ) );
        veh2_impulse = std::abs( m2 * ( vel2_y_a - vel2_y ) );
    } else {
        const float m1 = to_kilogram( veh.total_mass() );
        dmg_veh1 = ( std::abs( cmps_to_mps( veh.vertical_velocity ) ) * ( m1 / 10 ) ) / 2;
        dmg_veh2 = dmg_veh1;
        veh.vertical_velocity = 0;
    }

    if( delta_vel >= 6.0f ) {
        dmg_veh1 = veh1_impulse * dmg_adjust;
        dmg_veh2 = veh2_impulse * dmg_adjust;
    } else {
        dmg_veh1 = 0;
        dmg_veh2 = 0;
    }

    int coll_parts_cnt = 0;
    for( const auto &veh_veh_coll : collisions ) {
        if( &veh2 == static_cast<vehicle *>( veh_veh_coll.target ) ) {
            coll_parts_cnt++;
        }
    }

    const float dmg1_part = dmg_veh1 / coll_parts_cnt;
    const float dmg2_part = dmg_veh2 / coll_parts_cnt;

    for( const auto &veh_veh_coll : collisions ) {
        if( &veh2 != static_cast<vehicle *>( veh_veh_coll.target ) ) {
            continue;
        }

        int parm1 = veh.part_with_feature( veh_veh_coll.part, VPFLAG_ARMOR, true );
        if( parm1 < 0 ) {
            parm1 = veh_veh_coll.part;
        }
        int parm2 = veh2.part_with_feature( veh_veh_coll.target_part, VPFLAG_ARMOR, true );
        if( parm2 < 0 ) {
            parm2 = veh_veh_coll.target_part;
        }

        epicenter1 += veh.part( parm1 ).mount.raw();
        veh.damage( parm1, dmg1_part, DT_BASH );

        epicenter2 += veh2.part( parm2 ).mount.raw();
        veh2.damage( parm2, dmg2_part, DT_BASH );
    }

    epicenter2.x() /= coll_parts_cnt;
    epicenter2.y() /= coll_parts_cnt;

    if( dmg2_part > 100 ) {
        veh2.damage_all( dmg2_part / 2, dmg2_part, DT_BASH, tripoint_mnt_veh( epicenter2.raw() ) );
    }

    if( dmg_veh1 > 800 ) {
        veh.skidding = true;
    }
    if( dmg_veh2 > 800 ) {
        veh2.skidding = true;
    }

    return dmg_veh1;
}

auto mapbuffer::shake_vehicle( vehicle &veh, const int velocity_before,
                               const units::angle direction ) -> units::angle
{
    const int d_vel = std::abs( cmps_to_mps( veh.velocity - velocity_before ) ) * 2.23694;

    std::vector<rider_data> riders = veh.get_riders();

    units::angle coll_turn = 0_degrees;
    for( const rider_data &r : riders ) {
        const int ps = r.prt;
        Creature *rider = r.psg;
        if( rider == nullptr ) {
            debugmsg( "throw passenger: empty passenger at part %d", ps );
            continue;
        }

        const auto part_pos = veh.abs_part_location( ps );
        if( rider->abs_pos() != part_pos ) {
            debugmsg( "throw passenger: passenger at %d,%d,%d, part at %d,%d,%d",
                      rider->abs_pos().x(), rider->abs_pos().y(), rider->abs_pos().z(),
                      part_pos.x(), part_pos.y(), part_pos.z() );
            veh.part( ps ).remove_flag( vehicle_part::passenger_flag );
            continue;
        }

        player *psg = dynamic_cast<player *>( rider );
        monster *pet = dynamic_cast<monster *>( rider );

        bool throw_from_seat = false;
        int move_resist = 1;
        if( psg ) {
            move_resist = psg->str_cur * 150 + 500;
            if( veh.part( ps ).info().has_flag( "SEAT_REQUIRES_BALANCE" ) ) {
                int resist_penalty = 500;
                if( psg->has_trait( trait_PROF_SKATER ) ) {
                    resist_penalty -= 150;
                }
                if( psg->has_trait( trait_DEFT ) ) {
                    resist_penalty -= 150;
                }
                move_resist -= resist_penalty;
            }
        } else {
            int pet_resist = 0;
            if( pet != nullptr ) {
                pet_resist = static_cast<int>( to_kilogram( pet->get_weight() ) * 200 );
            }
            move_resist = std::max( 100, pet_resist );
        }

        if( veh.part_with_feature( ps, VPFLAG_SEATBELT, true ) == -1 ) {
            throw_from_seat = d_vel * rng( 80, 120 ) > move_resist;
        }

        if( !throw_from_seat && ( 10 * d_vel ) > 6 * rng( 50, 100 ) ) {
            const int dmg = d_vel * rng( 70, 100 ) / 400;
            if( psg ) {
                psg->hurtall( dmg, nullptr );
                psg->add_msg_player_or_npc( m_bad,
                                            _( "You take %d damage by the power of the impact!" ),
                                            _( "<npcname> takes %d damage by the power of the impact!" ),
                                            dmg );
            } else {
                pet->apply_damage( nullptr, bodypart_id( "torso" ), dmg );
            }
        }

        if( psg && veh.player_in_control( *psg ) ) {
            const int lose_ctrl_roll = rng( 0, d_vel );
            if( lose_ctrl_roll > psg->dex_cur * 2 + psg->get_skill_level( skill_driving ) * 3 ) {
                psg->add_msg_player_or_npc( m_warning,
                                            _( "You lose control of the %s." ),
                                            _( "<npcname> loses control of the %s." ), veh.name );
                int turn_amount = rng( 1, 3 ) * std::sqrt( std::abs( veh.velocity ) ) / 20;
                if( turn_amount < 1 ) {
                    turn_amount = 1;
                }
                units::angle turn_angle = std::min( turn_amount * 15_degrees, 120_degrees );
                coll_turn = one_in( 2 ) ? turn_angle : -turn_angle;
            }
        }

        if( throw_from_seat ) {
            if( psg ) {
                psg->add_msg_player_or_npc( m_bad,
                                            _( "You are hurled from the %s's seat by the power of the impact!" ),
                                            _( "<npcname> is hurled from the %s's seat by the power of the impact!" ),
                                            veh.name );
            } else if( get_player_character().sees( part_pos ) ) {
                add_msg( m_bad, _( "%s is hurled from %s's by the power of the impact!" ),
                         pet->disp_name( false, true ), veh.name );
            }

            // Gate map-level operations behind the reality bubble
            if( const auto local = active_reality_bubble_local( part_pos ) ) {
                get_map().unboard_vehicle( *local );
            }
            if( g != nullptr ) {
                g->fling_creature( rider, direction + rng_float( -30_degrees, 30_degrees ),
                                   std::max( 10, d_vel - move_resist / 100 ) );
            }
        }
    }

    return coll_turn;
}

auto mapbuffer::shift_vehicle_z( vehicle &veh, int z_shift ) -> void
{
    auto src = veh.abs_sm_pos;
    auto dst = src + tripoint_rel_sm( 0, 0, z_shift );

    const auto old_footprints = calculate_vehicle_submap_footprints( veh );
    const auto old_inbubble = std::ranges::any_of( old_footprints, [&]( const auto & footprint ) {
        return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this, *footprint );
    } );

    submap *src_submap = lookup_submap_in_memory( src );
    submap *dst_submap = lookup_submap_in_memory( dst );

    if( src_submap == nullptr || dst_submap == nullptr ) {
        debugmsg( "shift_vehicle [%s] failed because source or destination submap is not resident",
                  veh.name );
        return;
    }

    int our_i = -1;
    for( size_t i = 0; i < src_submap->vehicles.size(); i++ ) {
        if( src_submap->vehicles[i].get() == &veh ) {
            our_i = i;
            break;
        }
    }

    if( our_i == -1 ) {
        debugmsg( "shift_vehicle [%s] failed could not find vehicle", veh.name );
        return;
    }

    for( auto &prt : veh.get_all_parts() ) {
        prt.part().z_terrain[0] -= z_shift;
        prt.part().z_terrain[1] -= z_shift;
    }

    auto src_submap_veh_it = src_submap->vehicles.begin() + our_i;
    dst_submap->vehicles.push_back( std::move( *src_submap_veh_it ) );
    src_submap->vehicles.erase( src_submap_veh_it );
    dst_submap->is_uniform = false;

    veh.abs_sm_pos = dst;
    refresh_vehicle_footprint( &veh );
    veh.update_overmap( src );

    const auto new_footprints = calculate_vehicle_submap_footprints( veh );
    const auto new_inbubble = std::ranges::any_of( new_footprints, [&]( const auto & footprint ) {
        return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this, *footprint );
    } );

    if( new_inbubble ) {
        map &here = get_map();
        here.invalidate_max_populated_zlev( dst.z() );
        here.update_vehicle_list( dst_submap, dst.z() );
        here.add_vehicle_to_cache( &veh );
    }

    if( old_inbubble ) {
        map &here = get_map();
        level_cache &ch = here.get_cache( src.z() );
        ch.vehicle_list.erase( &veh );
        ch.zone_vehicles.erase( &veh );
    }

    invalidate_active_vehicle_footprints( *this, old_footprints );
    invalidate_active_vehicle_footprints( *this, new_footprints );
}

auto mapbuffer::move_vehicle( vehicle &veh, const tripoint_rel_ms &dp,
                              const tileray &facing ) -> vehicle *
{
    if( dp == tripoint_rel_ms::zero() ) {
        debugmsg( "Empty displacement vector" );
        return &veh;
    } else if( std::abs( dp.x() ) > 1 || std::abs( dp.y() ) > 1 || std::abs( dp.z() ) > 1 ) {
        debugmsg( "Invalid displacement vector: %d, %d, %d", dp.x(), dp.y(), dp.z() );
        return &veh;
    }

    // Split the movement into horizontal and vertical for easier processing
    if( dp.xy() != point_rel_ms::zero() && dp.z() != 0 ) {
        vehicle *const new_pointer = move_vehicle( veh, tripoint_rel_ms( dp.xy(), 0 ), facing );
        if( !new_pointer ) {
            return nullptr;
        }

        vehicle *const result = move_vehicle( *new_pointer, tripoint_rel_ms( 0, 0, dp.z() ), facing );
        if( !result ) {
            return nullptr;
        }

        result->is_falling = false;
        return result;
    }

    const bool vertical = dp.z() != 0;
    assert( vertical == ( dp.xy() == point_rel_ms::zero() ) );

    const int target_z = dp.z() + veh.abs_sm_pos.z();
    if( target_z < -OVERMAP_DEPTH || target_z > OVERMAP_HEIGHT ) {
        return &veh;
    }

    const auto old_footprints = calculate_vehicle_submap_footprints( veh );
    const auto vehicle_overlaps_active_bubble = std::ranges::any_of( old_footprints,
    [&]( const auto & footprint ) {
        return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this, *footprint );
    } );

    veh.precalc_mounts( 1, veh.skidding ? veh.turn_dir : facing.dir(), veh.pivot_point() );

    tripoint_rel_ms dp1 = tripoint_rel_ms( dp - veh.pivot_displacement() );

    if( !vertical ) {
        veh.adjust_zlevel( 1, dp1 );
    }

    const auto projected_footprints = calculate_vehicle_submap_footprints(
                                          veh, veh.abs_ms_location() + dp1, 1 );
    const auto projected_vehicle_overlaps_active_bubble = std::ranges::any_of(
    projected_footprints, [&]( const auto & footprint ) {
        return footprint && vehicle_submap_footprint_overlaps_active_bubble( *this, *footprint );
    } );

    int impulse = 0;

    std::vector<veh_collision> collisions;
    std::vector<vehicle *> passthrough;

    const int &coll_velocity = vertical ? veh.vertical_velocity : veh.velocity;
    const int velocity_before = coll_velocity;
    if( velocity_before == 0 && !veh.is_aircraft() && !veh.is_flying_in_air() ) {
        debugmsg( "%s tried to move %s with no velocity",
                  veh.name, vertical ? "vertically" : "horizontally" );
        return &veh;
    }

    bool veh_veh_coll_flag = false;
    size_t collision_attempts = 10;
    do {
        collisions.clear();
        veh.collision( vehicle_collision_options{
            .colls = collisions,
            .dp = dp1,
        } );

        std::map<vehicle *, std::vector<veh_collision>> veh_collisions;
        for( auto &coll : collisions ) {
            if( coll.type != veh_coll_veh ) {
                continue;
            }

            veh_veh_coll_flag = true;
            veh_collisions[static_cast<vehicle *>( coll.target )].push_back( coll );
        }

        for( auto &pair : veh_collisions ) {
            impulse += vehicle_vehicle_collision( veh, *pair.first, pair.second );
        }

        for( const auto &coll : collisions ) {
            if( coll.type == veh_coll_veh ) {
                continue;
            }
            if( coll.type == veh_coll_veh_nocollide ) {
                passthrough.push_back( static_cast<vehicle *>( coll.target ) );
                continue;
            }
            if( coll.part > veh.part_count() ||
                veh.part( coll.part ).removed ) {
                continue;
            }

            tripoint_mnt_veh collision_point = veh.part( coll.part ).mount;
            const int coll_dmg = coll.imp;
            if( veh.part_info( coll.part ).rotor_diameter() > 0 ) {
                veh.damage( coll.part, coll_dmg, DT_BASH, true );
            } else {
                impulse += coll_dmg;
                veh.damage( coll.part, coll_dmg, DT_BASH );
                int shock_max = coll_dmg;
                int shock_min = coll_dmg / 2;
                float coll_part_bash_resist =
                    veh.part_info( coll.part ).damage_reduction.type_resist( DT_BASH );
                shock_min = std::max<int>( 0, shock_min - coll_part_bash_resist );
                shock_max = std::max<int>( 0, shock_max - coll_part_bash_resist );
                if( shock_min >= 20 ) {
                    veh.damage_all( shock_min, shock_max, DT_BASH, collision_point );
                }
            }
        }

        if( vertical && velocity_before < 0 && coll_velocity > 0 ) {
            veh.vertical_velocity = 0;
        }

    } while( collision_attempts-- > 0 && coll_velocity != 0 &&
             sgn( coll_velocity ) == sgn( velocity_before ) &&
             !collisions.empty() && !veh_veh_coll_flag );

    const int velocity_after = coll_velocity;
    bool can_move = velocity_after != 0 && sgn( velocity_after ) == sgn( velocity_before );
    if( dp.z() != 0 && veh.is_aircraft() ) {
        can_move = true;
    }

    units::angle coll_turn = 0_degrees;
    if( impulse > 0 ) {
        coll_turn = shake_vehicle( veh, velocity_before, facing.dir() );
        veh.stop_autodriving();
        const int volume = std::min<int>( 120, std::sqrt( impulse ) );
        sound_event se;
        se.origin = veh.abs_ms_location();
        se.volume = volume;
        se.category = sounds::sound_t::combat;
        se.description = _( "crash!" );
        se.id = "smash_success";
        se.variant = "hit_vehicle";
        sounds::sound( se );
    }

    if( veh_veh_coll_flag ) {
        return nullptr;
    }

    // If not enough wheels, mess up the ground — use mapbuffer's set_ter
    if( !vertical && !veh.valid_wheel_config() && !veh.is_in_water() && !veh.is_flying_in_air() &&
        !veh.has_sufficient_lift( true ) &&
        dp.z() == 0 ) {
        veh.velocity += veh.velocity < 0 ? 2000 : -2000;
        for( const auto &p : veh.get_points() ) {
            const auto tile = abs_tile_handle::fetch( *this, p );
            if( tile ) {
                const ter_id &pter = tile->ter();
                if( pter == t_dirt || pter == t_grass ) {
                    set_ter( p, t_dirtmound );
                }
            }
        }
    }

    const units::angle last_turn_dec = 1_degrees;
    if( veh.last_turn < 0_degrees ) {
        veh.last_turn += last_turn_dec;
        if( veh.last_turn > -last_turn_dec ) {
            veh.last_turn = 0_degrees;
        }
    } else if( veh.last_turn > 0_degrees ) {
        veh.last_turn -= last_turn_dec;
        if( veh.last_turn < last_turn_dec ) {
            veh.last_turn = 0_degrees;
        }
    }

    Character &player_character = get_player_character();

    // Determine if the vehicle is seen — only relevant in the bubble
    auto sees_veh = []( const Creature & c, vehicle & veh, bool force_recalc ) -> bool {
        const auto &veh_points = veh.get_points( force_recalc );
        return std::ranges::any_of( veh_points, [&c]( const tripoint_abs_ms & pt )
        {
            return c.sees( pt );
        } );
    };
    bool seen = false;
    if( const auto local = active_reality_bubble_local( veh.abs_ms_location() ) ) {
        seen = sees_veh( player_character, veh, false );
    }

    if( can_move || ( vertical && veh.is_falling ) ) {
        if( veh.skidding ) {
            veh.face.init( veh.turn_dir );
        } else {
            veh.face = facing;
        }

        veh.move = facing;
        if( coll_turn != 0_degrees ) {
            veh.skidding = true;
            veh.turn( coll_turn );
        }
        veh.on_move();

        // displace_vehicle requires the local map — gate on complete footprint overlap
        if( vehicle_overlaps_active_bubble || projected_vehicle_overlaps_active_bubble ) {
            get_map().displace_vehicle( veh, tripoint_rel_ms( dp1 ) );
        }
        veh.shift_zlevel();
    } else if( !vertical ) {
        veh.stop();
    }

    veh.check_falling_or_floating();

    // If the PC is in the currently moved vehicle, adjust view offset
    if( const auto local = active_reality_bubble_local( veh.abs_ms_location() ) ) {
        map &here = get_map();
        if( g->u.controlling_vehicle && veh_pointer_or_null( veh_at( g->u.abs_pos() ) ) == &veh ) {
            g->calc_driving_offset( &veh );
            if( veh.skidding && can_move ) {
                veh.possibly_recover_from_skid();
            }
        }
    }

    // Handle traps under wheels
    if( !vertical && can_move ) {
        const auto wheel_indices = veh.wheelcache;

        float vehicle_grounded_wheel_area = static_cast<int>( vehicle_wheel_traction( veh, true ) );
        const float weight_to_damage_factor = 0.05f;
        const float vehicle_mass_kg = to_kilogram( veh.total_mass() );

        for( auto &w : wheel_indices ) {
            const auto wheel_abs = veh.abs_part_location( w );
            const auto wheel_bub = veh.bub_part_location( w );

            // displace_water and sound — gate behind bubble
            if( const auto local = active_reality_bubble_local( wheel_abs ) ) {
                map &here = get_map();
                if( one_in( 2 ) && here.displace_water( wheel_bub ) ) {
                    sound_event se;
                    se.origin = wheel_abs;
                    se.volume = 50;
                    se.category = sounds::sound_t::movement;
                    se.movement_noise = true;
                    se.description = _( "splash!" );
                    se.id = "environment";
                    se.variant = "splash";
                    sounds::sound( se );
                }
            }

            veh.handle_trap( wheel_abs, w );

            // Check SEALED flag via abs_tile_handle
            auto tile = abs_tile_handle::fetch( *this, wheel_abs );
            if( tile && !tile->has_flag( "SEALED" ) ) {
                const float wheel_area = veh.part( w ).wheel_area();

                const int wheel_damage = static_cast<int>( ( ( wheel_area / vehicle_grounded_wheel_area ) *
                                         vehicle_mass_kg ) * weight_to_damage_factor );

                // smash_items requires the local map — gate behind bubble
                if( const auto smash_local = active_reality_bubble_local( wheel_abs ) ) {
                    get_map().smash_items( *smash_local, wheel_damage,
                                           string_format( _( "weight of %1$s" ), veh.disp_name() ),
                                           false );
                }
            }
        }
    }

    if( veh.is_towing() ) {
        veh.do_towing_move();
        if( veh.is_towing() && veh.tow_data.get_towed()->tow_cable_too_far() ) {
            add_msg( m_info, _( "A towing cable snaps off of %s." ),
                     veh.tow_data.get_towed()->disp_name() );
            veh.tow_data.get_towed()->invalidate_towing( true );
        }
    }

    // add_vehicle_to_cache for passthrough vehicles — gate behind bubble
    for( vehicle *colveh : passthrough ) {
        if( const auto local = active_reality_bubble_local( colveh->abs_ms_location() ) ) {
            get_map().add_vehicle_to_cache( colveh );
        }
    }

    // Redraw scene — only in the bubble
    if( const auto local = active_reality_bubble_local( veh.abs_ms_location() ) ) {
        map &here = get_map();
        if( !player_character.activity && ( seen || sees_veh( player_character, veh, true ) ) ) {
            g->invalidate_main_ui_adaptor();
            inp_mngr.pump_events();
            ui_manager::redraw_invalidated();
            refresh_display();
        }
    }
    return &veh;
}

namespace
{

static const trait_id trait_NPC_STATIC_NPC( "NPC_STATIC_NPC" );

static bool can_place_monster_at( const monster &mon, const tripoint_abs_ms &p,
                                  mapbuffer &buf )
{
    if( const auto existing = buf.creature_at( p ) ) {
        const auto *mon_ptr = dynamic_cast<const monster *>( existing );
        if( mon_ptr && !mon_ptr->is_hallucination() ) {
            return false;
        }
        if( !mon_ptr ) {
            // NPC or player
            return false;
        }
    }
    return mon.will_move_to( p );
}

} // namespace

auto mapbuffer::place_critter_at( const mtype_id &id, const tripoint_abs_ms &p ) -> monster *
{
    return place_critter_around( id, p, 0 );
}

auto mapbuffer::place_critter_at( const shared_ptr_fast<monster> &mon,
                                  const tripoint_abs_ms &p ) -> monster *
{
    return place_critter_around( mon, p, 0 );
}

auto mapbuffer::place_critter_around( const mtype_id &id, const tripoint_abs_ms &center,
                                      const int radius ) -> monster *
{
    if( id.is_null() ) {
        return nullptr;
    }
    const auto temp = make_shared_fast<monster>( id );
    return place_critter_around( temp, center, radius );
}

auto mapbuffer::place_critter_around( const shared_ptr_fast<monster> &mon,
                                      const tripoint_abs_ms &center,
                                      const int radius, const bool forced ) -> monster *
{
    std::optional<tripoint_abs_ms> where;

    if( forced || can_place_monster_at( *mon, center, *this ) ) {
        where = center;
    }

    // This loop ensures the monster is placed as close to the center as possible,
    // but all places that equally far from the center have the same probability.
    for( int r = 1; r <= radius && !where; ++r ) {
        std::vector<tripoint_abs_ms> candidates;
        for( const auto &pt : simulated_tiles_in_radius( *this, center, r ) ) {
            if( can_place_monster_at( *mon, pt.abs_pos(), *this ) ) {
                candidates.push_back( pt.abs_pos() );
            }
        }
        if( !candidates.empty() ) {
            where = random_entry( candidates );
        }
    }

    if( !where ) {
        return nullptr;
    }
    mon->set_dimension( dimension_id_ );
    mon->spawn( *where );
    return creature_tracker().add( mon ) ? mon.get() : nullptr;
}

auto mapbuffer::place_npc( const tripoint_abs_ms &p, const string_id<npc_template> &type,
                           const bool force ) -> character_id
{
    if( !force && !get_option<bool>( "STATIC_NPC" ) ) {
        return character_id();
    }
    shared_ptr_fast<npc> temp = make_shared_fast<npc>();
    const auto proj = project_remain<coords::sm>( p );
    temp->load_npc_template( type );
    temp->spawn_at_precise( proj.quotient, proj.remainder_tripoint );
    temp->set_dimension( dimension_id_ );
    temp->toggle_trait( trait_NPC_STATIC_NPC );
    get_overmapbuffer( dimension_id_ ).insert_npc( temp );
    return temp->getID();
}

auto mapbuffer::is_sheltered( const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    if( const auto local = active_reality_bubble_local( p ) ) {
        return g->m.is_sheltered( *local );
    }

    const auto sm_pos = project_to<coords::sm>( p );
    const auto sm = get_submap( sm_pos, options );
    if( !sm ) {
        return true; // outside loaded area — treat as sheltered
    }
    ensure_roof_above_cache( *this, *sm, options );
    const auto split = project_remain<coords::sm>( p );
    const auto has_roof = sm->roof_above_cache[split.remainder.x()][split.remainder.y()];
    const auto vp = veh_at( p, options );
    return has_roof || ( vp && vp->is_inside() );
}

// ----- Collapse / suspension helpers -----

auto mapbuffer::is_suspension_valid( const tripoint_abs_ms &point,
                                     const mapbuffer_lookup_options options ) -> bool
{
    auto ter_at = [&]( const tripoint_abs_ms & p ) -> std::optional<ter_id> {
        return ter( p, options );
    };

    // Check: if both east and west are not open air — supported
    {
        const auto te = ter_at( point + tripoint_east );
        const auto tw = ter_at( point + tripoint_west );
        if( te && tw && *te != t_open_air && *tw != t_open_air ) {
            return true;
        }
    }
    // Check: if both southeast and northwest are not open air — supported
    {
        const auto tse = ter_at( point + tripoint_south_east );
        const auto tnw = ter_at( point + tripoint_north_west );
        if( tse && tnw && *tse != t_open_air && *tnw != t_open_air ) {
            return true;
        }
    }
    // Check: if both south and north are not open air — supported
    {
        const auto ts = ter_at( point + tripoint_south );
        const auto tn = ter_at( point + tripoint_north );
        if( ts && tn && *ts != t_open_air && *tn != t_open_air ) {
            return true;
        }
    }
    // Check: if both northeast and southwest are not open air — supported
    {
        const auto tne = ter_at( point + tripoint_north_east );
        const auto tsw = ter_at( point + tripoint_south_west );
        if( tne && tsw && *tne != t_open_air && *tsw != t_open_air ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::collapse_invalid_suspension( const tripoint_abs_ms &point,
        const mapbuffer_lookup_options options ) -> void
{
    if( !is_suspension_valid( point, options ) ) {
        set_ter( point, t_open_air, options );
        set_furn( point, f_null, options );

        propagate_suspension_check( point, options );
    }
}

auto mapbuffer::propagate_suspension_check( const tripoint_abs_ms &point,
        const mapbuffer_lookup_options options ) -> void
{
    for( const auto &neighbor : simulated_tiles_in_radius( *this, point, 1 ) ) {
        if( neighbor.abs_pos() != point &&
            has_flag( TFLAG_SUSPENDED, neighbor.abs_pos(), options ) ) {
            collapse_invalid_suspension( neighbor.abs_pos(), options );
        }
    }
}

auto mapbuffer::collapse_check( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> int
{
    const bool collapses = has_flag( TFLAG_COLLAPSES, p, options );
    const bool supports_roof = has_flag( TFLAG_SUPPORTS_ROOF, p, options );

    int num_supports = p.z() == -OVERMAP_DEPTH ? 0 : -5;
    // If there's support below, things are less likely to collapse
    if( p.z() > -OVERMAP_DEPTH ) {
        const auto pbelow = tripoint_abs_ms( p.xy(), p.z() - 1 );
        for( const auto &tbelow : points_in_radius( pbelow, 1 ) ) {
            if( has_flag( TFLAG_SUPPORTS_ROOF, tbelow, options ) ) {
                num_supports += 1;
                if( has_flag( TFLAG_WALL, tbelow, options ) ) {
                    num_supports += 2;
                }
                if( tbelow == pbelow ) {
                    num_supports += 2;
                }
            }
        }
    }

    for( const auto &t : points_in_radius( p, 1 ) ) {
        if( p == t ) {
            continue;
        }

        if( collapses ) {
            if( has_flag( TFLAG_COLLAPSES, t, options ) ) {
                num_supports++;
            } else if( has_flag( TFLAG_SUPPORTS_ROOF, t, options ) ) {
                num_supports += 2;
            }
        } else if( supports_roof ) {
            if( has_flag( TFLAG_SUPPORTS_ROOF, t, options ) ) {
                if( has_flag( TFLAG_WALL, t, options ) ) {
                    num_supports += 4;
                } else if( !has_flag( TFLAG_COLLAPSES, t, options ) ) {
                    num_supports += 3;
                }
            }
        }
    }

    return 1.7 * num_supports;
}

static const efftype_id effect_crushed( "crushed" );

auto mapbuffer::crush( const tripoint_abs_ms &p,
                       const mapbuffer_lookup_options options ) -> void
{
    auto crushed_creature = creature_at( p );
    if( !crushed_creature ) {
        return;
    }

    if( auto crushed_player = crushed_creature->as_player() ) {
        bool player_inside = false;
        if( crushed_player->in_vehicle ) {
            const optional_vpart_position vp = veh_at( p, options );
            player_inside = vp && vp->is_inside();
        }
        if( !player_inside ) { //If there's a player at p and he's not in a covered vehicle...
            //This is the roof coming down on top of us, no chance to dodge
            crushed_player->add_msg_player_or_npc( m_bad, _( "You are crushed by the falling debris!" ),
                                                   _( "<npcname> is crushed by the falling debris!" ) );
            // TODO: Make this depend on the ceiling material
            const int dam = rng( 0, 40 );
            // Torso and head take the brunt of the blow
            crushed_player->deal_damage( nullptr, bodypart_id( "head" ), damage_instance( DT_BASH,
                                         dam * .25 ) );
            crushed_player->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_BASH,
                                         dam * .45 ) );
            // Legs take the next most through transferred force
            crushed_player->deal_damage( nullptr, bodypart_id( "leg_l" ), damage_instance( DT_BASH,
                                         dam * .10 ) );
            crushed_player->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( DT_BASH,
                                         dam * .10 ) );
            // Arms take the least
            crushed_player->deal_damage( nullptr, bodypart_id( "arm_l" ), damage_instance( DT_BASH,
                                         dam * .05 ) );
            crushed_player->deal_damage( nullptr, bodypart_id( "arm_r" ), damage_instance( DT_BASH,
                                         dam * .05 ) );

            // Pin whoever got hit
            crushed_player->add_effect( effect_crushed, 1_turns, bodypart_str_id::NULL_ID() );
            crushed_player->check_dead_state();
        }
    }

    if( auto monhit = crushed_creature->as_monster() ) {
        // 25 ~= 60 * .45 (torso)
        monhit->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_BASH, rng( 0, 25 ) ) );

        // Pin whoever got hit
        monhit->add_effect( effect_crushed, 1_turns, bodypart_str_id::NULL_ID() );
        monhit->check_dead_state();
    }

    if( const optional_vpart_position vp = veh_at( p, options ) ) {
        // Arbitrary number is better than collapsing house roof crushing APCs
        vp->vehicle().damage( vp->part_index(), rng( 100, 1000 ), DT_BASH, false );
    }
}

// there is still some odd behavior here and there and you can get floating chunks of
// unsupported floor, but this is much better than it used to be
auto mapbuffer::collapse_at( const tripoint_abs_ms &p, const bool silent,
                             const bool was_supporting, const bool destroy_pos,
                             const mapbuffer_lookup_options options ) -> void
{
    const bool supports = was_supporting || has_flag( TFLAG_SUPPORTS_ROOF, p, options );
    const bool wall = was_supporting || has_flag( TFLAG_WALL, p, options );
    // don't bash again if the caller already bashed here
    if( destroy_pos ) {
        destroy( p, silent, options );
        crush( p, options );
        make_rubble( p, f_rubble, t_dirt, false, options );
    }
    const bool still_supports = has_flag( TFLAG_SUPPORTS_ROOF, p, options );

    // If something supporting the roof collapsed, see what else collapses
    if( supports && !still_supports ) {
        for( const auto &t : points_in_radius( p, 1 ) ) {
            // If z-levels are off, tz == t, so we end up skipping a lot of stuff to avoid bugs.
            const auto tz = tripoint_abs_ms( t.xy(), t.z() + 1 );
            // if nothing above us had the chance of collapsing, move on
            if( !one_in( collapse_check( tz, options ) ) ) {
                continue;
            }
            // if a wall collapses, walls without support from below risk collapsing and
            // propagate the collapse upwards
            if( wall && p == t && has_flag( TFLAG_WALL, tz, options ) ) {
                collapse_at( tz, silent, false, true, options );
            }
            // floors without support from below risk collapsing into open air and can propagate
            // the collapse horizontally but not vertically
            if( p != t && ( has_flag( TFLAG_SUPPORTS_ROOF, t, options ) &&
                            has_flag( TFLAG_COLLAPSES, t, options ) ) ) {
                collapse_at( t, silent, false, true, options );
            }
        }
        // this tile used to support a roof, now it doesn't, which means there is only
        // open air above us
        const tripoint_abs_ms tabove( p.xy(), p.z() + 1 );
        set_ter( tabove, t_open_air, options );
        set_furn( tabove, f_null, options );
        propagate_suspension_check( tabove, options );
    }
    // it would be great to check if collapsing ceilings smashed through the floor, but
    // that's not handled for now
}

auto mapbuffer::revive_corpse( item &it ) -> bool
{
    const tripoint_abs_ms p = it.abs_pos();
    if( !it.is_corpse() ) {
        debugmsg( "Tried to revive a non-corpse." );
        return false;
    }

    shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>
                                          ( it.get_mtype()->id );
    monster &critter = *newmon_ptr;
    critter.init_from_item( it );
    if( critter.get_hp() < 1 ) {
        // Failed reanimation due to corpse being too burned
        return false;
    }
    if( it.has_flag( flag_FIELD_DRESS ) || it.has_flag( flag_FIELD_DRESS_FAILED ) ||
        it.has_flag( flag_QUARTERED ) ) {
        // Failed reanimation due to corpse being butchered
        return false;
    }

    critter.no_extra_death_drops = true;
    for( detached_ptr<item> &component : it.remove_components() ) {
        critter.add_corpse_component( std::move( component ) );
    }

    return place_critter_at( newmon_ptr, p ) != nullptr;
}

auto mapbuffer::emit_field( const tripoint_abs_ms &pos, const emit_id &src, const float mul,
                            const mapbuffer_lookup_options options ) -> void
{
    if( !src.is_valid() ) {
        return;
    }

    const float chance = src->chance() * mul;
    if( x_in_y( chance, 100 ) ) {
        const int qty = chance > 100.0f ? roll_remainder( src->qty() * chance / 100.0f ) : src->qty();
        propagate_field( pos, src->field(), qty, src->intensity(), options );
    }
}

auto mapbuffer::get_fishable_locations( const int radius, const tripoint_abs_ms &fish_pos,
                                        const mapbuffer_lookup_options options )
-> std::unordered_set<tripoint_abs_ms> // *NOPAD*
{
    std::unordered_set<tripoint_abs_ms> visited;

    const tripoint_abs_ms fishing_boundary_min(
        fish_pos + point_rel_ms( -radius, -radius ) );
    const tripoint_abs_ms fishing_boundary_max(
        fish_pos + point_rel_ms( radius, radius ) );

    const inclusive_cuboid<tripoint_abs_ms> fishing_boundaries(
        fishing_boundary_min, fishing_boundary_max );

    std::unordered_set<tripoint_abs_ms> fishable_points;
    std::queue<tripoint_abs_ms> to_check;
    to_check.push( fish_pos );

    while( !to_check.empty() ) {
        const auto current_point = to_check.front();
        to_check.pop();

        if( visited.contains( current_point ) ) {
            continue;
        }

        if( !fishing_boundaries.contains( current_point ) ) {
            continue;
        }

        visited.emplace( current_point );

        if( has_flag( "FISHABLE", current_point, options ) ) {
            fishable_points.emplace( current_point );
            to_check.push( current_point + tripoint_south );
            to_check.push( current_point + tripoint_north );
            to_check.push( current_point + tripoint_east );
            to_check.push( current_point + tripoint_west );
        }
    }

    return fishable_points;
}

auto mapbuffer::is_cornerfloor( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> bool
{
    // Check if the tile itself is passable
    const auto tile_passable = passable( p, options );
    if( !tile_passable ) {
        return false;
    }

    // Collect impassable adjacent tiles
    std::set<tripoint_abs_ms> impassable_adjacent;
    for( const auto &pt : simulated_tiles_in_radius( *this, p, 1 ) ) {
        const auto pt_passable = passable( pt.abs_pos(), options );
        if( !pt_passable ) {
            impassable_adjacent.insert( pt.abs_pos() );
        }
    }

    if( impassable_adjacent.empty() ) {
        return false;
    }

    // Check diagonal tiles
    const std::array<tripoint_abs_ms, 4> diagonals = {{
            p + tripoint_north_east, p + tripoint_north_west,
            p + tripoint_south_east, p + tripoint_south_west
        }
    };

    for( const auto &impassable_diagonal : diagonals ) {
        if( impassable_adjacent.contains( impassable_diagonal ) ) {
            int f = 0;
            for( const auto &l : simulated_tiles_in_radius( *this, impassable_diagonal, 1 ) ) {
                if( impassable_adjacent.contains( l.abs_pos() ) ) {
                    f++;
                }
                if( f > 2 ) {
                    return true;
                }
            }
        }
    }

    return false;
}

auto mapbuffer::mop_spills( const tripoint_abs_ms &p,
                            const mapbuffer_lookup_options options ) -> bool
{
    bool retval = false;

    if( !has_flag( "LIQUIDCONT", p, options ) && !has_flag( "SEALED", p, options ) ) {
        auto items = get_items( p, options );
        if( items ) {
            items->remove_with( [&retval]( detached_ptr<item> &&e ) {
                if( e->made_of( LIQUID ) ) {
                    retval = true;
                    return detached_ptr<item>();
                }
                return std::move( e );
            } );
        }
    }

    auto *fld = get_field( p, options );
    if( fld ) {
        static const std::vector<field_type_id> to_check = {
            fd_blood,
            fd_blood_veggy,
            fd_blood_insect,
            fd_blood_invertebrate,
            fd_gibs_flesh,
            fd_gibs_veggy,
            fd_gibs_insect,
            fd_gibs_invertebrate,
            fd_bile,
            fd_slime,
            fd_sludge
        };
        for( const field_type_id &fid : to_check ) {
            retval |= fld->remove_field( fid );
        }
    }

    if( const auto vp = veh_at( p, options ) ) {
        vehicle *const veh = &vp->vehicle();
        std::vector<int> parts_here = veh->parts_at_relative( vp->mount(), true );
        for( auto &elem : parts_here ) {
            if( veh->part( elem ).blood > 0 ) {
                veh->part( elem ).blood = 0;
                retval = true;
            }
            vehicle_stack here_items = veh->get_items( elem );
            here_items.remove_top_items_with( [&retval]( detached_ptr<item> &&e ) {
                if( e->made_of( LIQUID ) ) {
                    retval = true;
                    return detached_ptr<item>();
                }
                return std::move( e );
            } );
        }
    }

    return retval;
}

auto mapbuffer::set_ter( const tripoint_abs_ms &p, const ter_id terrain,
                         const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto old_id = tile->sm->get_ter( tile->local );
    if( old_id == terrain ) {
        return false;
    }

    tile->sm->set_ter( tile->local, terrain );
    invalidate_active_terrain_set_caches( p, old_id, terrain );
    mark_post_pass_changed( *this, *tile->sm );
    return true;
}

auto mapbuffer::set_furn( const tripoint_abs_ms &p, const furn_id furn,
                          const mapbuffer_lookup_options options ) -> bool
{
    return set_furn( p, {
        .furniture = furn,
        .lookup = options,
    } );
}

auto mapbuffer::set_furn( const tripoint_abs_ms &p,
                          const mapbuffer_set_furn_options &options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options.lookup );
    if( !tile ) {
        return false;
    }

    const auto old_id = tile->sm->get_furn( tile->local );
    const furn_id &new_id = options.furniture;
    if( old_id == new_id ) {
        return false;
    }

    tile->sm->set_furn( tile->local, new_id );
    sync_furniture_change_side_tables( p, *tile->sm, tile->local, old_id, new_id, options.active );
    mark_post_pass_changed( *this, *tile->sm );
    invalidate_active_furniture_set_caches( p, old_id, new_id );
    map_mutation_hooks::on_furniture_changed( {
        .dim_id = dimension_id_,
        .p = p,
        .old_furniture = old_id,
        .new_furniture = new_id,
    } );
    return true;
}

auto mapbuffer::veh_at( const tripoint_abs_ms &p,
                        const mapbuffer_lookup_options options ) -> optional_vpart_position
{
    const auto target_sm = project_to<coords::sm>( p );
    if( get_submap( target_sm, options ) == nullptr ) {
        return optional_vpart_position( std::nullopt );
    }

    return vehicle_part_at_loaded_tile( p );
}

auto mapbuffer::vehicle_part_at_loaded_tile( const tripoint_abs_ms &p ) -> optional_vpart_position
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    return indexed_vehicle_part_at_unlocked( p );
}

auto mapbuffer::valid_move( const tripoint_abs_ms &from, const tripoint_abs_ms &to,
                            const mapbuffer_valid_move_options options ) -> bool
{
    assert( to.z() > std::numeric_limits<int>::min() );
    if( std::abs( from.x() - to.x() ) > 1 || std::abs( from.y() - to.y() ) > 1 ||
        std::abs( from.z() - to.z() ) > 1 ) {
        return false;
    }

    if( from.z() == to.z() ) {
        const auto target_tile = abs_tile_handle::fetch( *this, to, options.lookup );
        if( !target_tile ) {
            return false;
        }
        return target_tile->passable() || options.bash;
    }
    if( !options.zlevels ) {
        return false;
    }

    const auto going_up = from.z() < to.z();
    const auto up_p = going_up ? to : from;
    const auto down_p = going_up ? from : to;

    const auto up_tile = abs_tile_handle::fetch_terrain_only( *this, up_p, options.lookup );
    if( !up_tile ) {
        return false;
    }
    const auto &up_ter = up_tile->ter_obj();
    if( up_ter.id.is_null() ) {
        return false;
    }
    const auto &up_furn = up_tile->furn_obj();
    const auto up_trap_id = up_tile->trap_id();
    const auto up_is_ledge = up_ter.trap == tr_ledge || up_trap_id == tr_ledge;

    if( up_ter.movecost == 0 ) {
        return false;
    }

    const auto down_tile = abs_tile_handle::fetch_terrain_only( *this, down_p, options.lookup );
    if( !down_tile ) {
        return false;
    }
    const auto &down_ter = down_tile->ter_obj();
    if( down_ter.id.is_null() ) {
        return false;
    }

    if( !up_is_ledge && down_ter.movecost == 0 ) {
        return false;
    }

    if( !up_ter.has_flag( TFLAG_NO_FLOOR ) && !up_ter.has_flag( TFLAG_GOES_DOWN ) &&
        !up_is_ledge && !options.via_ramp ) {
        if( std::abs( from.x() - to.x() ) == 1 || std::abs( from.y() - to.y() ) == 1 ) {
            const auto midpoint = tripoint_abs_ms( down_p.xy(), up_p.z() );
            return valid_move( down_p, midpoint, options ) && valid_move( midpoint, up_p, options );
        }
        return false;
    }

    if( !options.flying && !down_ter.has_flag( TFLAG_GOES_UP ) &&
        !down_ter.has_flag( TFLAG_RAMP ) && !up_is_ledge && !options.via_ramp ) {
        return false;
    }

    if( options.bash ) {
        return true;
    }

    const auto up_vehicle = veh_at( up_p, options.lookup );
    if( up_vehicle && !up_vehicle.part_with_feature( VPFLAG_NOCOLLIDEBELOW, false ) ) {
        return false;
    }

    const auto down_vehicle = veh_at( down_p, options.lookup );
    if( down_vehicle &&
        down_vehicle->vehicle().roof_at_part( static_cast<int>( down_vehicle->part_index() ) ) >= 0 ) {
        return false;
    }

    return up_furn.movecost >= 0;
}

auto mapbuffer::climb_difficulty( const tripoint_abs_ms &p,
                                  const mapbuffer_lookup_options options ) -> std::optional<int>
{
    if( p.z() > OVERMAP_HEIGHT || p.z() < -OVERMAP_DEPTH ) {
        return std::nullopt;
    }

    const auto center_tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !center_tile ) {
        return std::nullopt;
    }
    const auto has_flag = []( const abs_tile_handle & tile, const auto & flag ) {
        return tile.ter_obj().has_flag( flag ) || tile.furn_obj().has_flag( flag );
    };

    auto best_difficulty = std::numeric_limits<int>::max();
    auto blocks_movement = 0;
    if( has_flag( *center_tile, "LADDER" ) ) {
        return 1;
    }
    if( has_flag( *center_tile, TFLAG_RAMP ) ||
        has_flag( *center_tile, TFLAG_RAMP_UP ) ||
        has_flag( *center_tile, TFLAG_RAMP_DOWN ) ) {
        best_difficulty = 7;
    }

    for( const auto &tile : simulated_tiles_in_radius( *this, p, 1 ) ) {
        if( !tile.passable() ) {
            best_difficulty = std::min( best_difficulty, 10 );
            blocks_movement++;
        } else if( tile.vehicle_part() ) {
            best_difficulty = std::min( best_difficulty, 7 );
        }

        if( best_difficulty > 5 && has_flag( tile, "CLIMBABLE" ) ) {
            best_difficulty = 5;
        }
    }

    return std::max( 0, best_difficulty - blocks_movement );
}

auto mapbuffer::floor_between( const tripoint_abs_ms &first, const tripoint_abs_ms &second,
                               const mapbuffer_lookup_options options ) -> bool
{
    int diff = std::abs( first.z() - second.z() );
    if( diff == 0 ) {
        return false;
    }
    if( diff != 1 ) {
        debugmsg( "mapbuffer::floor_between should only be called on tiles that are "
                  "exactly 1 z level apart" );
        return true;
    }
    int upper = std::max( first.z(), second.z() );
    if( first.xy() == second.xy() ) {
        return has_floor( tripoint_abs_ms( first.xy(), upper ), false, options );
    }
    return has_floor( tripoint_abs_ms( first.xy(), upper ), false, options ) &&
           has_floor( tripoint_abs_ms( second.xy(), upper ), false, options );
}

auto mapbuffer::clear_path( const tripoint_abs_ms &f, const tripoint_abs_ms &t, int range,
                            int cost_min, int cost_max,
                            const mapbuffer_lookup_options options ) -> bool
{
    if( f.z() == t.z() ) {
        if( range >= 0 && range < rl_dist( f.xy(), t.xy() ) ) {
            return false; // Out of range!
        }
        bool is_clear = true;
        point_abs_ms last_point = f.xy();
        bresenham( f.xy().raw(), t.xy().raw(), 0,
        [this, &is_clear, cost_min, cost_max, &t, &last_point, options]( const point & new_point ) {
            // Exit before checking the last square, it's still reachable even if it is an obstacle.
            if( new_point == t.xy().raw() ) {
                return false;
            }

            const tripoint_abs_ms p( point_abs_ms( new_point ), t.z() );
            const tripoint_abs_ms lp( point_abs_ms( last_point ), t.z() );
            const int cost = move_cost( p, nullptr, options );
            if( cost < cost_min || cost > cost_max ||
                obstructed_by_vehicle_rotation( lp, p ) ) {
                is_clear = false;
                return false;
            }

            last_point = point_abs_ms( new_point );
            return true;
        } );
        return is_clear;
    }

    if( range >= 0 && range < rl_dist( f, t ) ) {
        return false; // Out of range!
    }
    bool is_clear = true;
    tripoint_abs_ms last_point = f;
    bresenham( f.raw(), t.raw(), 0, 0,
    [this, &is_clear, cost_min, cost_max, t, &last_point, options]( const tripoint & new_point ) {
        // Exit before checking the last square, it's still reachable even if it is an obstacle.
        if( new_point == t.raw() ) {
            return false;
        }

        const tripoint_abs_ms pt( new_point );
        // We have to check a weird case where the move is both vertical and horizontal
        if( new_point.z == last_point.z() ) {
            const int cost = move_cost( pt, nullptr, options );
            if( cost < cost_min || cost > cost_max ||
                obstructed_by_vehicle_rotation( last_point, pt ) ) {
                is_clear = false;
                return false;
            }
        } else {
            bool this_clear = false;
            const int max_z = std::max( new_point.z, last_point.z() );
            const point_abs_ms new_xy( new_point.x, new_point.y );
            const tripoint_abs_ms no_floor_check( new_xy, max_z );
            if( !has_floor_or_support( no_floor_check ) ) {
                const tripoint_abs_ms from_prev_z( new_xy, last_point.z() );
                const int cost = move_cost( from_prev_z, nullptr, options );
                if( cost > cost_min && cost < cost_max &&
                    !obstructed_by_vehicle_rotation( last_point, pt ) ) {
                    this_clear = true;
                }
            }

            if( !this_clear ) {
                const tripoint_abs_ms floor_check( last_point.xy(), max_z );
                if( has_floor_or_support( floor_check ) ) {
                    const tripoint_abs_ms from_new_z( last_point.xy(), new_point.z );
                    const int cost = move_cost( from_new_z, nullptr, options );
                    if( cost > cost_min && cost < cost_max &&
                        !obstructed_by_vehicle_rotation( last_point, pt ) ) {
                        this_clear = true;
                    }
                }
            }

            if( !this_clear ) {
                is_clear = false;
                return false;
            }
        }

        last_point = pt;
        return true;
    } );
    return is_clear;
}

// This method tries a bunch of initial offsets for the line to try and find a clear one.
// Basically it does, "Find a line from any point in the source that ends up in the target square".
auto mapbuffer::find_clear_path( const tripoint_abs_ms &source,
                                 const tripoint_abs_ms &destination ) -> std::vector<tripoint_abs_ms>
{
    // TODO: Push this junk down into the Bresenham method, it's already doing it.
    const point_rel_ms d = destination.xy() - source.xy();
    const point_rel_ms a( std::abs( d.x() ) * 2, std::abs( d.y() ) * 2 );
    const int dominant = std::max( a.x(), a.y() );
    const int minor = std::min( a.x(), a.y() );
    // This seems to be the method for finding the ideal start value for the error value.
    const int ideal_start_offset = minor - dominant / 2;
    const int start_sign = ( ideal_start_offset > 0 ) - ( ideal_start_offset < 0 );
    // Not totally sure of the derivation.
    const int max_start_offset = std::abs( ideal_start_offset ) * 2 + 1;
    for( int horizontal_offset = -1; horizontal_offset <= max_start_offset; ++horizontal_offset ) {
        int candidate_offset = horizontal_offset * start_sign;
        if( sees( source, destination, rl_dist( source, destination ), candidate_offset ) ) {
            return line_to( source, destination, candidate_offset, 0 );
        }
    }
    // If we couldn't find a clear LoS, just return the ideal one.
    return line_to( source, destination, ideal_start_offset, 0 );
}

auto mapbuffer::get_lum( const tripoint_abs_ms &p,
                         const mapbuffer_lookup_options options ) -> std::optional<std::uint8_t>
{
    const auto tile = abs_tile_handle::fetch( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->lum();
}

auto mapbuffer::get_temperature( const tripoint_abs_ms &p,
                                 const mapbuffer_lookup_options options ) -> std::optional<int>
{
    if( is_outside_pocket_dimension_bounds( p ) ) {
        return std::nullopt;
    }

    const auto split = project_to<coords::sm>( p );
    auto *const sm = get_submap( split, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }

    return sm->get_temperature();
}

auto mapbuffer::get_field( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> field *
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return nullptr;
    }

    return &tile->sm->get_field( tile->local );
}

auto mapbuffer::has_field_at( const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->field_count > 0;
}

auto mapbuffer::get_field_entry( const tripoint_abs_ms &p, const field_type_id &type,
                                 const mapbuffer_lookup_options options ) -> field_entry *
{
    if( !has_field_at( p, options ) ) {
        return nullptr;
    }

    return get_field( p, options )->find_field( type );
}

auto mapbuffer::get_field_age( const tripoint_abs_ms &p, const field_type_id &type,
                               const mapbuffer_lookup_options options ) -> std::optional<time_duration>
{
    if( !get_field( p, options ) ) {
        return std::nullopt;
    }

    const auto *const field_ptr = get_field_entry( p, type, options );
    return field_ptr == nullptr ? -1_turns : field_ptr->get_field_age();
}

auto mapbuffer::get_field_intensity( const tripoint_abs_ms &p, const field_type_id &type,
                                     const mapbuffer_lookup_options options ) -> std::optional<int>
{
    if( !get_field( p, options ) ) {
        return std::nullopt;
    }

    const auto *const field_ptr = get_field_entry( p, type, options );
    return field_ptr == nullptr ? 0 : field_ptr->get_field_intensity();
}

auto mapbuffer::passable( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch( *this, p, options );
    if( !tile ) {
        return false;
    }

    return tile->passable();
}

auto mapbuffer::ter_vars( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> data_vars::data_set *
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return nullptr;
    }

    return &tile->sm->get_ter_vars( tile->local );
}

auto mapbuffer::furn_vars( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> data_vars::data_set *
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return nullptr;
    }

    return &tile->sm->get_furn_vars( tile->local );
}

auto mapbuffer::get_trap( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::optional<trap_id>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    // Check terrain-associated trap (e.g. t_pit carries tr_pit).
    // Mirroring map::tr_at() which checks both sources.
    const auto &ter_trap = tile->sm->get_ter( tile->local ).obj().trap;
    if( ter_trap != tr_null ) {
        return ter_trap;
    }

    return tile->sm->get_trap( tile->local );
}

auto mapbuffer::get_radiation( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> std::optional<int>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_radiation( tile->local );
}

auto mapbuffer::set_trap( const tripoint_abs_ms &p, const trap_id trap,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    if( tile->sm->get_ter( tile->local ).obj().trap != tr_null && trap != tr_null ) {
        debugmsg( "set trap %s on top of terrain %s which already has a built-in trap",
                  trap.obj().name(), tile->sm->get_ter( tile->local ).obj().name() );
        return false;
    }

    const auto old_id = tile->sm->get_trap( tile->local );
    if( old_id == trap ) {
        return false;
    }

    tile->sm->set_trap( tile->local, trap );
    sync_active_trap_change_side_tables( p, tile->local, old_id, trap );
    return true;
}

auto mapbuffer::remove_trap( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto old_id = tile->sm->get_trap( tile->local );
    if( old_id == tr_null ) {
        return false;
    }

    tile->sm->set_trap( tile->local, tr_null );
    sync_active_trap_change_side_tables( p, tile->local, old_id, tr_null );
    return true;
}

auto mapbuffer::creature_on_trap( Creature &critter, const bool may_avoid ) -> void
{
    const auto pos = critter.abs_pos();
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, pos );
    if( !tile ) {
        return;
    }

    auto trap_here = tile->ter_obj().trap;
    if( trap_here == tr_null ) {
        trap_here = tile->trap_id();
    }

    const auto &tr = trap_here.obj();
    if( tr.is_null() ) {
        return;
    }
    const player *const pl = critter.as_player();
    if( pl != nullptr && pl->in_vehicle ) {
        return;
    }

    if( may_avoid && critter.avoid_trap( pos, tr ) ) {
        return;
    }
    tr.trigger( pos, &critter );
}

auto mapbuffer::set_radiation( const tripoint_abs_ms &p, const int radiation,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->set_radiation( tile->local, radiation );
    return true;
}

auto mapbuffer::adjust_radiation( const tripoint_abs_ms &p, const int delta,
                                  const mapbuffer_lookup_options options ) -> std::optional<int>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    const auto adjusted = tile->sm->get_radiation( tile->local ) + delta;
    tile->sm->set_radiation( tile->local, adjusted );
    return adjusted;
}

auto mapbuffer::set_lum( const tripoint_abs_ms &p, const std::uint8_t luminance,
                         const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto old_luminance = tile->sm->get_lum( tile->local );
    if( old_luminance == luminance ) {
        return false;
    }

    tile->sm->set_lum( tile->local, luminance );
    refresh_luminous_item_submap_index( project_to<coords::sm>( p ), {
        .mode = mapbuffer_lookup_mode::resident_only,
    } );
    if( active_reality_bubble_local( p ) ) {
        g->m.invalidate_lightmap_caches();
    }
    return true;
}

auto mapbuffer::set_temperature( const tripoint_abs_ms &p, const int temperature,
                                 const mapbuffer_lookup_options options ) -> bool
{
    if( is_outside_pocket_dimension_bounds( p ) ) {
        return false;
    }

    const auto split = project_to<coords::sm>( p );
    auto *const sm = get_submap( split, options );
    if( sm == nullptr ) {
        return false;
    }

    sm->set_temperature( temperature );
    return true;
}

auto mapbuffer::mod_field_age( const tripoint_abs_ms &p,
                               const mapbuffer_field_age_options &options ) -> std::optional<time_duration>
{
    auto set_options = options;
    set_options.isoffset = true;
    return set_field_age( p, set_options );
}

auto mapbuffer::mod_field_intensity( const tripoint_abs_ms &p,
                                     const mapbuffer_field_intensity_options &options ) -> std::optional<int>
{
    auto set_options = options;
    set_options.isoffset = true;
    return set_field_intensity( p, set_options );
}

auto mapbuffer::set_field_age( const tripoint_abs_ms &p,
                               const mapbuffer_field_age_options &options ) -> std::optional<time_duration>
{
    if( !get_field( p, options.lookup ) ) {
        return std::nullopt;
    }

    auto *const field_ptr = get_field_entry( p, options.type, options.lookup );
    if( field_ptr == nullptr ) {
        return -1_turns;
    }

    return field_ptr->set_field_age( ( options.isoffset ? field_ptr->get_field_age() : 0_turns ) +
                                     options.age );
}

auto mapbuffer::set_field_intensity( const tripoint_abs_ms &p,
                                     const mapbuffer_field_intensity_options &options ) -> std::optional<int>
{
    if( !get_field( p, options.lookup ) ) {
        return std::nullopt;
    }

    auto *const field_ptr = get_field_entry( p, options.type, options.lookup );
    if( field_ptr != nullptr ) {
        const auto adjusted = ( options.isoffset ? field_ptr->get_field_intensity() : 0 ) +
                              options.intensity;
        if( adjusted > 0 ) {
            return field_ptr->set_field_intensity( adjusted );
        }
        remove_field( p, options.type, options.lookup );
        return 0;
    }

    if( options.intensity <= 0 ) {
        return 0;
    }

    return add_field( p, {
        .type = options.type,
        .intensity = options.intensity,
        .lookup = options.lookup,
    } ) ? options.intensity : 0;
}

auto mapbuffer::add_field( const tripoint_abs_ms &p,
                           const mapbuffer_add_field_options &options ) -> bool
{
    if( !options.type ) {
        debugmsg( "Tried to add null field at %d,%d,%d",
                  p.x(), p.y(), p.z() );
        return false;
    }

    const auto tile = lookup_tile( *this, p, options.lookup );
    if( !tile ) {
        return false;
    }

    const auto &field_type = *options.type;
    const auto intensity = std::min( options.intensity, field_type.get_max_intensity() );
    if( intensity <= 0 ) {
        return false;
    }

    tile->sm->is_uniform = false;
    if( tile->sm->get_field( tile->local ).add_field( options.type, intensity, options.age ) ) {
        tile->sm->field_count++;
        tile->sm->field_cache.push_back( tile->local );
    }

    invalidate_active_field_add_caches( p, options.type );
    return true;
}

auto mapbuffer::remove_field( const tripoint_abs_ms &p, const field_type_id &type,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    if( !tile->sm->get_field( tile->local ).remove_field( type ) ) {
        return false;
    }

    --tile->sm->field_count;
    invalidate_active_field_remove_caches( p, type );
    return true;
}

auto mapbuffer::get_items( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> location_vector<item> *
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return nullptr;
    }

    return &tile->sm->get_items( tile->local );
}

auto mapbuffer::add_item_or_charges( const tripoint_abs_ms &p, detached_ptr<item> &&new_item,
                                     const mapbuffer_add_item_or_charges_options &options ) -> detached_ptr<item>
{
    if( !new_item ) {
        return std::move( new_item );
    }
    if( new_item->is_null() ) {
        debugmsg( "Tried to add a null item to the mapbuffer" );
        return std::move( new_item );
    }
    if( new_item->has_flag( flag_NO_DROP ) ) {
        return std::move( new_item );
    }

    auto valid_tile = [&]( const tripoint_abs_ms & target ) -> std::optional<abs_tile_handle> {
        auto tile = abs_tile_handle::fetch( *this, target, options.lookup );
        if( !tile )
        {
            return std::nullopt;
        }
        if( tile->has_flag( TFLAG_DESTROY_ITEM ) )
        {
            return std::nullopt;
        }
        if( new_item->made_of( LIQUID ) && tile->has_flag( TFLAG_SWIMMABLE ) )
        {
            return std::nullopt;
        }
        return tile;
    };

    auto valid_limits = [&]( const abs_tile_handle & tile ) {
        const auto max_volume = tile.furn() != f_null ?
                                tile.furn().obj().max_volume :
                                tile.ter().obj().max_volume;
        auto stored_volume = 0_ml;
        const auto &items = tile.items();
        for( const auto *const existing : items ) {
            stored_volume += existing->volume();
        }
        return new_item->volume() <= max_volume - stored_volume &&
               items.size() < MAX_ITEM_IN_SQUARE;
    };

    auto call_active_drop_hook = [&]( const tripoint_abs_ms & target ) {
        if( new_item->made_of( LIQUID ) || !new_item->has_flag( flag_DROP_ACTION_ONLY_IF_LIQUID ) ) {
            return new_item->on_drop();
        }
        return false;
    };

    auto route_allows_overflow = [&]( const tripoint_abs_ms & target ) {
        PathfindingSettings pf_settings;
        pf_settings.bash_strength_val = 0;
        RouteSettings rt_settings;
        rt_settings.max_dist = 3;
        rt_settings.max_s_coeff = 4.0f;
        for( const auto &origin : simulated_tiles_in_radius( *this, p, 1 ) ) {
            if( origin.abs_pos() == p || !origin.passable() ) {
                continue;
            }
            if( origin.abs_pos() == target ) {
                return true;
            }
            const auto abs_route = Pathfinding::route( *this, origin.abs_pos(), target,
                                   pf_settings, rt_settings );
            if( !abs_route.empty() ) {
                return true;
            }
        }
        return false;
    };

    auto place_item = [&]( const abs_tile_handle & tile ) {
        auto &items = *get_items( tile.abs_pos(), options.lookup );
        if( new_item->count_by_charges() ) {
            for( auto &existing : items ) {
                // Remove the location before merge so the merged-away item is
                // not destroyed with a dangling loc pointer, which would
                // trigger the "Attempted to destroy an item with a location"
                // warning in game_object::destroy().
                new_item->saved_loc = nullptr;
                new_item->remove_location();
                if( existing->merge_charges( std::move( new_item ) ) ) {
                    return;
                }
            }
        }

        if( const auto local = active_reality_bubble_local( tile.abs_pos() ) ) {
            g->m.support_dirty( *local );
        }
        new_item = add_item( tile.abs_pos(), std::move( new_item ), options.lookup );
    };

    auto try_place = [&]( const tripoint_abs_ms & target, const bool reject_noitem,
    const bool call_drop_hook_first ) {
        auto tile = valid_tile( target );
        if( !tile ) {
            return false;
        }
        if( reject_noitem && ( tile->has_flag( TFLAG_NOITEM ) || tile->has_flag( TFLAG_SEALED ) ) ) {
            return false;
        }

        // Associate the detached item with the destination while calling hooks
        // that may access its mapbuffer and absolute position.
        new_item->saved_loc = tile->items().get_location();

        if( call_drop_hook_first && call_active_drop_hook( target ) ) {
            new_item->saved_loc = nullptr;
            new_item->remove_location();
            return true;
        }
        if( !tile->has_flag( TFLAG_SEALED ) &&
            ( !tile->has_flag( TFLAG_NOITEM ) ||
              tile_allows_item_despite_noitem_flag( *new_item, *tile ) ) &&
            valid_limits( *tile ) ) {
            if( !call_drop_hook_first && call_active_drop_hook( target ) ) {
                new_item->saved_loc = nullptr;
                new_item->remove_location();
                return true;
            }
            place_item( *tile );
            return true;
        }

        new_item->saved_loc = nullptr;
        new_item->remove_location();
        return false;
    };

    if( try_place( p, false, false ) ) {
        return std::move( new_item );
    }

    if( options.overflow ) {
        const auto max_dist = 2;
        auto tiles = closest_points_first( p, max_dist );
        tiles.erase( tiles.begin() );
        for( const auto &candidate : tiles ) {
            if( !route_allows_overflow( candidate ) ) {
                continue;
            }
            if( try_place( candidate, true, true ) ) {
                return std::move( new_item );
            }
        }
    }

    return std::move( new_item );
}

auto mapbuffer::add_item( const tripoint_abs_ms &p, detached_ptr<item> &&new_item,
                          const mapbuffer_lookup_options options ) -> detached_ptr<item>
{
    if( !new_item ) {
        return std::move( new_item );
    }
    if( new_item->is_null() ) {
        debugmsg( "Tried to add a null item to the mapbuffer" );
        return std::move( new_item );
    }

    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::move( new_item );
    }

    // Keep the detached item associated with the destination while hooks and
    // processing run, but do not leave a live location pointer on an object
    // that a callback may destroy.
    auto &items = tile->sm->get_items( tile->local );
    new_item->saved_loc = items.get_location();

    // --- inlined from prepare_item_for_placement ---

    auto reject = [&]() {
        new_item->saved_loc = nullptr;
        new_item->remove_location();
        return std::move( new_item );
    };

    if( new_item->is_food() ) {
        new_item = item::process( std::move( new_item ), nullptr, false );
        if( !new_item ) {
            return std::move( new_item );
        }
    }

    if( new_item->made_of( LIQUID ) && tile_has_flag( *tile, "SWIMMABLE" ) ) {
        return reject();
    }

    if( tile_has_flag( *tile, "DESTROY_ITEM" ) ) {
        return reject();
    }

    if( new_item->has_flag( flag_ACT_IN_FIRE ) &&
        tile->sm->get_field( tile->local ).find_field( fd_fire ) != nullptr ) {
        if( new_item->has_flag( flag_BOMB ) && new_item->is_transformable() ) {
            new_item->convert( dynamic_cast<const iuse_transform *>(
                                   new_item->type->get_use( "transform" )->get_actor_ptr() )->target );
        }
        new_item->activate();
    }

    new_item->on_placement();

    tile->sm->is_uniform = false;
    if( active_reality_bubble_local( p ) ) {
        g->m.invalidate_max_populated_zlev( p.z() );
    }

    const auto adds_luminance = new_item->is_emissive();
    tile->sm->update_lum_add( tile->local, *new_item );
    if( adds_luminance ) {
        refresh_luminous_item_submap_index( project_to<coords::sm>( p ), {
            .mode = mapbuffer_lookup_mode::resident_only,
        } );
        invalidate_active_item_luminance_cache( p );
    }

    if( new_item->needs_processing() ) {
        tile->sm->active_items.add( *new_item );
        sync_active_item_submap_index( p, *tile->sm );
    }

    new_item->saved_loc = nullptr;
    tile->sm->get_items( tile->local ).push_back( std::move( new_item ) );
    return detached_ptr<item>();
}

auto mapbuffer::process_item_at( const tripoint_abs_ms &p, detached_ptr<item> &&new_item,
                                 const bool activate ) -> detached_ptr<item>
{
    if( !new_item ) {
        return std::move( new_item );
    }

    auto *items = get_items( p );
    if( items == nullptr ) {
        return std::move( new_item );
    }

    auto *const item_location = items->get_location();
    new_item->saved_loc = item_location;
    new_item = item::process( std::move( new_item ), nullptr, activate );
    if( new_item && new_item->saved_loc == item_location ) {
        new_item->saved_loc = nullptr;
    }
    return std::move( new_item );
}

auto mapbuffer::erase_item( const tripoint_abs_ms &p,
                            const mapbuffer_erase_item_options &options ) -> location_vector<item>::iterator
{
    const auto tile = lookup_tile( *this, p, options.lookup );
    if( !tile ) {
        return location_vector<item>::iterator();
    }

    auto &items = tile->sm->get_items( tile->local );
    item *const to_remove = *options.it;

    tile->sm->active_items.remove( to_remove );
    sync_active_item_submap_index( p, *tile->sm );

    const auto removed_luminance = to_remove->is_emissive();
    tile->sm->update_lum_rem( tile->local, *to_remove );
    if( removed_luminance ) {
        refresh_luminous_item_submap_index( project_to<coords::sm>( p ), {
            .mode = mapbuffer_lookup_mode::resident_only,
        } );
        invalidate_active_item_luminance_cache( p );
    }

    return items.erase( options.it, options.out );
}

auto mapbuffer::remove_item( const tripoint_abs_ms &p, item *const to_remove,
                             const mapbuffer_lookup_options options ) -> detached_ptr<item>
{
    if( to_remove == nullptr ) {
        return detached_ptr<item>();
    }

    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return detached_ptr<item>();
    }

    auto &items = tile->sm->get_items( tile->local );
    const auto iter = std::ranges::find( items, to_remove );
    if( iter == items.end() ) {
        return detached_ptr<item>();
    }

    detached_ptr<item> removed;
    erase_item( p, {
        .it = iter,
        .out = &removed,
        .lookup = options,
    } );
    return removed;
}

auto mapbuffer::clear_items( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> std::vector<detached_ptr<item>>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return {};
    }

    auto &items = tile->sm->get_items( tile->local );
    for( item *const it : items ) {
        tile->sm->active_items.remove( it );
    }
    sync_active_item_submap_index( p, *tile->sm );

    const auto had_luminance = tile->sm->get_lum( tile->local ) != 0;
    tile->sm->set_lum( tile->local, 0 );
    if( had_luminance ) {
        refresh_luminous_item_submap_index( project_to<coords::sm>( p ), {
            .mode = mapbuffer_lookup_mode::resident_only,
        } );
        invalidate_active_item_luminance_cache( p );
    }

    return items.clear();
}

auto mapbuffer::handle_rotten_away_item( const tripoint_abs_ms &p, const item &rotten_item,
        const mapbuffer_lookup_options options ) -> void
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return;
    }

    const auto actualize_options = actualize_tile_options {
        .buffer = *this,
        .sm = *tile->sm,
        .local = tile->local,
        .abs_pos = p,
        .active_bubble_pos = active_reality_bubble_local( p ),
        .last_touched = calendar::turn,
        .elapsed = 0_turns,
        .lookup = options,
    };

    if( rotten_item.is_comestible() ) {
        rotten_item_spawn( actualize_options, rotten_item );
    } else if( rotten_item.is_corpse() ) {
        handle_decayed_corpse( actualize_options, rotten_item );
    }
}

auto mapbuffer::make_item_active( const tripoint_abs_ms &p, item &target,
                                  const mapbuffer_lookup_options options ) -> bool
{
    if( !target.needs_processing() ) {
        return false;
    }

    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->active_items.add( target );
    sync_active_item_submap_index( p, *tile->sm );
    return true;
}

auto mapbuffer::make_item_inactive( const tripoint_abs_ms &p, item &target,
                                    const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->active_items.remove( &target );
    sync_active_item_submap_index( p, *tile->sm );
    return true;
}

auto mapbuffer::update_item_lum( const tripoint_abs_ms &p, item &target,
                                 const mapbuffer_item_lum_options &options ) -> bool
{
    if( !target.is_emissive() ) {
        return false;
    }

    const auto tile = lookup_tile( *this, p, options.lookup );
    if( !tile ) {
        return false;
    }

    if( options.add_luminance ) {
        tile->sm->update_lum_add( tile->local, target );
    } else {
        tile->sm->update_lum_rem( tile->local, target );
    }
    refresh_luminous_item_submap_index( project_to<coords::sm>( p ), {
        .mode = mapbuffer_lookup_mode::resident_only,
    } );
    invalidate_active_item_luminance_cache( p );
    return true;
}

auto mapbuffer::refresh_active_item_submap_index( const tripoint_abs_ms &p,
        const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    sync_active_item_submap_index( p, *tile->sm );
    return true;
}

auto mapbuffer::refresh_active_item_submap_index( const tripoint_abs_sm &p,
        const mapbuffer_lookup_options options ) -> bool
{
    auto *sm = get_submap( p, options );
    if( sm == nullptr ) {
        return false;
    }

    if( sm->active_items.empty() ) {
        submaps_with_active_items_.erase( p );
    } else {
        submaps_with_active_items_.insert( p );
    }
    return true;
}

auto mapbuffer::forget_active_item_submap_index( const tripoint_abs_sm &p ) -> void
{
    submaps_with_active_items_.erase( p );
}

auto mapbuffer::clear_active_item_submap_index() -> void
{
    submaps_with_active_items_.clear();
}

auto mapbuffer::get_submaps_with_active_items() const -> const std::set<tripoint_abs_sm> &
{
    return submaps_with_active_items_;
}

auto mapbuffer::refresh_luminous_item_submap_index( const tripoint_abs_ms &p,
        const mapbuffer_lookup_options options ) -> bool
{
    return refresh_luminous_item_submap_index( project_to<coords::sm>( p ), options );
}

auto mapbuffer::refresh_luminous_item_submap_index( const tripoint_abs_sm &p,
        const mapbuffer_lookup_options options ) -> bool
{
    auto *const sm = get_submap( p, options );
    if( sm == nullptr ) {
        submaps_with_luminous_items_.erase( p );
        return false;
    }

    if( std::ranges::any_of( ::submap_tiles(), [&]( const point_sm_ms & pos ) {
    return sm->get_lum( pos ) != 0;
    } ) ) {
        submaps_with_luminous_items_.insert( p );
    } else {
        submaps_with_luminous_items_.erase( p );
    }
    return true;
}

auto mapbuffer::forget_luminous_item_submap_index( const tripoint_abs_sm &p ) -> void
{
    submaps_with_luminous_items_.erase( p );
}

auto mapbuffer::get_submaps_with_luminous_items() const -> const std::set<tripoint_abs_sm> &
{
    return submaps_with_luminous_items_;
}

auto mapbuffer::get_active_items_in_radius( const tripoint_abs_ms &center, const int radius,
        const special_item_type type ) -> std::vector<item *>
{
    auto result = std::vector<item *> {};

    const auto minp = center.xy() + point_rel_ms( -radius, -radius );
    const auto maxp = center.xy() + point_rel_ms( radius, radius );

    for( const tripoint_abs_sm &abs_submap_loc : submaps_with_active_items_ ) {
        if( !submap_loader.is_simulated( dimension_id_, abs_submap_loc ) ) {
            continue;
        }

        const auto sm_origin = project_to<coords::ms>( abs_submap_loc );
        const auto sm_max = sm_origin.xy() + point_rel_ms( SEEX - 1, SEEY - 1 );
        if( sm_origin.x() > maxp.x() || sm_origin.y() > maxp.y() ||
            sm_max.x() < minp.x() || sm_max.y() < minp.y() ) {
            continue;
        }

        auto *sm = lookup_submap_in_memory( abs_submap_loc );
        if( sm == nullptr ) {
            continue;
        }

        auto items = type == special_item_type::none ? sm->active_items.get() :
                     sm->active_items.get_special( type );
        for( item *elem : items ) {
            if( elem == nullptr ) {
                continue;
            }
            if( rl_dist( elem->abs_pos(), center ) > radius ) {
                continue;
            }
            result.emplace_back( elem );
        }
    }

    return result;
}

auto mapbuffer::has_graffiti_at( const tripoint_abs_ms &p,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->has_graffiti( tile->local );
}

auto mapbuffer::graffiti_at( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> std::optional<std::string>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_graffiti( tile->local );
}

auto mapbuffer::set_graffiti( const tripoint_abs_ms &p, const std::string &contents,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->set_graffiti( tile->local, contents );
    return true;
}

auto mapbuffer::delete_graffiti( const tripoint_abs_ms &p,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->delete_graffiti( tile->local );
    return true;
}

auto mapbuffer::has_signage( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->has_signage( tile->local );
}

auto mapbuffer::get_signage( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> std::optional<std::string>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_signage( tile->local );
}

auto mapbuffer::set_signage( const tripoint_abs_ms &p, const std::string &message,
                             const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->set_signage( tile->local, message );
    return true;
}

auto mapbuffer::delete_signage( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->delete_signage( tile->local );
    return true;
}

auto mapbuffer::has_computer( const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->has_computer( tile->local );
}

auto mapbuffer::get_computer( const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> computer *
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return nullptr;
    }

    return tile->sm->get_computer( tile->local );
}

auto mapbuffer::set_computer( const tripoint_abs_ms &p, const computer &terminal,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->set_computer( tile->local, terminal );
    return true;
}

auto mapbuffer::add_computer( const tripoint_abs_ms &p,
                              const mapbuffer_add_computer_options &options ) -> computer *
{
    const auto tile = lookup_tile( *this, p, options.lookup );
    if( !tile ) {
        return nullptr;
    }

    set_ter( p, t_console, options.lookup );
    tile->sm->set_computer( tile->local, computer( options.name, options.security ) );
    return tile->sm->get_computer( tile->local );
}

auto mapbuffer::delete_computer( const tripoint_abs_ms &p,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    tile->sm->delete_computer( tile->local );
    return true;
}

void mapbuffer::add_spawn( const mtype_id &type, int count, const tripoint_abs_ms &p, bool friendly,
                           int faction_id, int mission_id, const std::string &name,
                           mapbuffer_lookup_options options ) const
{
    add_spawn( type, count, p, spawn_point::friendly_to_spawn_disposition( friendly ), faction_id,
               mission_id, name );
}

void mapbuffer::add_spawn( const mtype_id &type, int count, const tripoint_abs_ms &p,
                           spawn_disposition disposition, int faction_id, int mission_id,
                           const std::string &name, mapbuffer_lookup_options options ) const
{
    auto proj = project_remain<coords::sm>( p );
    auto &buffer = MAPBUFFER_REGISTRY.get( dimension_id_ );
    auto *const place_on_submap = buffer.get_submap( proj.quotient_tripoint, options );

    if( !place_on_submap ) {
        debugmsg( "centadodecamonant doesn't exist in grid; within add_spawn(%s, %d, %d, %d, %d)",
                  type.c_str(), count, p.x(), p.y(), p.z() );
        return;
    }
    if( MonsterGroupManager::monster_is_blacklisted( type ) ) {
        return;
    }
    spawn_point tmp( type, count, proj.remainder, faction_id, mission_id, disposition, name );
    place_on_submap->spawns.push_back( tmp );
}

vehicle *mapbuffer::add_vehicle( const std::variant<vgroup_id, vproto_id> &type_,
                                 const tripoint_abs_ms &p,
                                 const units::angle dir, const int veh_fuel,
                                 const int veh_status, const bool merge_wrecks,
                                 std::optional<bool> locked,
                                 std::optional<bool> has_keys,
                                 mapbuffer_lookup_options options )
{
    constexpr auto pos_selector = []<typename T>( const T & v, int z ) -> tripoint_bub_ms {
        if constexpr( std::is_same_v<T, point_bub_ms> )
        {
            return tripoint_bub_ms( v, z );
        } else
        {
            return v;
        }
    };

    constexpr auto type_selector = []<typename T>( const T & v ) -> vproto_id {
        if constexpr( std::is_same_v<T, vgroup_id> )
        {
            return v.obj().pick();
        } else
        {
            return v;
        }
    };

    const auto type = std::visit( type_selector, type_ );

    if( !type.is_valid() ) {
        debugmsg( "Nonexistent vehicle type: \"%s\"", type.c_str() );
        return nullptr;
    }
    auto proj = project_remain<coords::sm>( p );
    auto *const place_on_submap = get_submap( proj.quotient_tripoint, options );

    if( !place_on_submap ) {
        debugmsg( "add_vehicle triggered for nonexistent submap t=%s d=%d p=%s",
                  type, to_degrees( dir ), p.to_string() );
        return nullptr;
    }

    // debugmsg("n=%d x=%d y=%d MAPSIZE=%d ^2=%d", nonant, x, y, MAPSIZE, MAPSIZE*MAPSIZE);
    auto veh = std::make_unique<vehicle>( type, veh_fuel, veh_status, locked, has_keys );
    veh->abs_sm_pos = proj.quotient_tripoint;
    veh->sm_ms_pos = proj.remainder;
    veh->place_spawn_items();
    // for backwards compatibility, we always spawn with a pivot point of (0,0) so
    // that the mount at (0,0) is located at the spawn position.
    veh->set_facing_and_pivot( dir, tripoint_mnt_veh::zero(), false );
    //debugmsg("adding veh: %d, sm: %d,%d,%d, pos: %d, %d", veh, veh->smx, veh->smy, veh->smz, veh->posx, veh->posy);
    std::unique_ptr<vehicle> placed_vehicle_up =
        add_vehicle_to_mapbuffer( std::move( veh ), merge_wrecks, options );
    vehicle *placed_vehicle = placed_vehicle_up.get();

    if( placed_vehicle != nullptr ) {
        place_on_submap->vehicles.push_back( std::move( placed_vehicle_up ) );
        place_on_submap->is_uniform = false;
        register_vehicle( placed_vehicle );
        const auto footprints = calculate_vehicle_submap_footprints( *placed_vehicle );
        if( invalidate_active_vehicle_footprints( *this, footprints ) ) {
            auto &map = get_map();
            const auto placed_vehicle_sm = placed_vehicle->abs_sm_pos;
            map.invalidate_max_populated_zlev( placed_vehicle_sm.z() );
            map.get_cache( placed_vehicle_sm.z() ).vehicle_list.insert( placed_vehicle );
            map.add_vehicle_to_cache( placed_vehicle );
        }
        //debugmsg ("grid[%d]->vehicles.size=%d veh.parts.size=%d", nonant, grid[nonant]->vehicles.size(),veh.parts.size());
    }
    return placed_vehicle;
}

/**
 * Takes a vehicle already created with new and attempts to place it on the map,
 * checking for collisions. If the vehicle can't be placed, returns NULL,
 * otherwise returns a pointer to the placed vehicle, which may not necessarily
 * be the one passed in (if wreckage is created by fusing cars).
 * @param veh The vehicle to place on the map.
 * @param merge_wrecks Whether crashed vehicles become part of each other
 * @return The vehicle that was finally placed.
 */
std::unique_ptr<vehicle> mapbuffer::add_vehicle_to_mapbuffer(
    std::unique_ptr<vehicle> veh, const bool merge_wrecks,
    mapbuffer_lookup_options options )
{
    //We only want to check once per square, so loop over all structural parts
    std::vector<int> frame_indices = veh->all_standalone_parts();

    //Check for boat type vehicles that should be placeable in deep water
    //WARNING: CURSED CODE
    //If changed to veh->can_float mass calculations are messed up
    const bool can_float = !veh->get_avail_parts( "FLOATS" ).empty();

    //When hitting a wall, only smash the vehicle once (but walls many times)
    bool needs_smashing = false;

    veh->attach();
    veh->refresh_position();

    for( std::vector<int>::const_iterator part = frame_indices.begin();
         part != frame_indices.end(); part++ ) {
        // Use abs_part_location + explicit map-local conversion so that during mapgen
        // (where get_map() is the player map, not this mapgen constructor) the position
        // checks reference the correct submap grid.
        const auto abs_pos = veh->abs_part_location( *part );

        //Don't spawn anything in water
        if( has_flag( TFLAG_DEEP_WATER, abs_pos, options ) && !can_float ) {
            return nullptr;
        }

        // Don't spawn shopping carts on top of another vehicle or other obstacle.
        if( veh->type == vproto_id( "shopping_cart" ) ) {
            if( veh_at( abs_pos, options ) || !passable( abs_pos, options ) ) {
                return nullptr;
            }
        }

        //For other vehicles, simulate collisions with (non-shopping cart) stuff
        vehicle *const other_veh = veh_pointer_or_null( veh_at( abs_pos, options ) );
        if( other_veh != nullptr && other_veh->type != vproto_id( "shopping_cart" ) ) {
            if( !merge_wrecks ) {
                return nullptr;
            }

            // Hard wreck-merging limit: 250 tiles
            // Merging is slow for big vehicles which lags the mapgen
            if( frame_indices.size() + other_veh->all_standalone_parts().size() > 250 ) {
                return nullptr;
            }

            // We must remove the vehicle from the map before we move away its parts
            std::unique_ptr<vehicle> old_veh = detach_vehicle( other_veh, options );
            if( old_veh == nullptr ) {
                return nullptr;
            }

            for( const vpart_reference &vpr : old_veh->get_all_parts() ) {
                const auto part_pos = veh->abs_to_mount( old_veh->abs_part_location( vpr.part() ) );
                auto transferred_part = vehicle_part{ vpr.part(), & *veh };
                transferred_part.direction = normalize( old_veh->face.dir() + transferred_part.direction -
                                                        veh->face.dir() );
                veh->install_part( part_pos, std::move( transferred_part ) );
            }

            veh->name = _( "Wreckage" );


            // Try again with the wreckage
            std::unique_ptr<vehicle> new_veh = add_vehicle_to_mapbuffer( std::move( veh ), true, options );
            if( new_veh != nullptr ) {
                new_veh->smash();
                return new_veh;
            }

            // If adding the wreck failed, we want to restore the vehicle we tried to merge with
            add_vehicle_to_mapbuffer( std::move( old_veh ), false, options );
            return nullptr;

        } else if( !( veh->has_lift() && has_flag( TFLAG_NO_FLOOR, abs_pos, options ) ) &&
                   !passable( abs_pos, options ) ) {
            if( !merge_wrecks ) {
                return nullptr;
            }

            // There's a wall or other obstacle here; destroy it
            destroy( abs_pos, true, options );

            // Some weird terrain, don't place the vehicle
            if( !passable( abs_pos, options ) ) {
                return nullptr;
            }

            needs_smashing = true;
        }
    }

    if( needs_smashing ) {
        veh->smash();
    }

    return veh;
}

std::set<vehicle *> mapbuffer::get_vehicles( const tripoint_abs_sm &start,
        const tripoint_abs_sm &end,
        mapbuffer_lookup_options options )
{
    auto vehs = std::set<vehicle *> {};

    if( start.x() > end.x() || start.y() > end.y() ||
        start.z() > end.z() ) {
        return vehs;
    }
    for( const auto sm_pos : tripoint_range<tripoint_abs_sm>( start, end ) ) {
        auto *const sm = get_submap( sm_pos, options );
        if( !sm ) {
            continue;
        }
        const submap *current_submap = sm;
        for( const auto &elem : current_submap->vehicles ) {
            vehs.emplace( elem.get() );
        }
    }

    return vehs;
}

std::set<vehicle *> mapbuffer::get_vehicles()
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    return loaded_vehicles_;
}

std::unique_ptr<vehicle> mapbuffer::detach_vehicle( vehicle *veh,
        mapbuffer_lookup_options options )
{
    if( veh == nullptr ) {
        debugmsg( "mapbuffer::detach_vehicle was passed nullptr" );
        return std::unique_ptr<vehicle>();
    }

    int z = veh->abs_sm_pos.z();
    if( z < -OVERMAP_DEPTH || z > OVERMAP_HEIGHT ) {
        debugmsg( "detach_vehicle got a vehicle outside allowed z-level range!  name=%s, submap:%d,%d,%d",
                  veh->name, veh->abs_sm_pos.x(), veh->abs_sm_pos.y(), veh->abs_sm_pos.z() );
        // Try to fix by moving the vehicle here
        z = veh->abs_sm_pos.z() = z > OVERMAP_HEIGHT ? OVERMAP_HEIGHT : -OVERMAP_DEPTH;
    }

    const auto footprints = calculate_vehicle_submap_footprints( *veh );
    bool inbubble = false;

    const auto mark_detached_vehicle_footprint_dirty = [&]() {
        for( const auto &footprint : footprints ) {
            if( !footprint || !vehicle_submap_footprint_overlaps_active_bubble( *this, *footprint ) ) {
                continue;
            }
            inbubble = true;
            g->m.on_vehicle_moved( abs_to_map_local( g->m, footprint->min ),
                                   abs_to_map_local( g->m, footprint->max ), footprint->min.z() );
        }
    };

    // Unboard all passengers before detaching
    for( auto const &part : veh->get_avail_parts( VPFLAG_BOARDABLE ) ) {
        player *passenger = part.get_passenger();
        if( passenger ) {
            unboard_vehicle( part.abs_pos() );
        }
    }
    veh->invalidate_towing( true );
    auto &here = MAPBUFFER_REGISTRY.get( dimension_id_ );
    submap *current_submap = here.get_submap( veh->abs_sm_pos, options );
    if( current_submap == nullptr ) {
        debugmsg( "detach_vehicle can't find submap!  name=%s, submap:%d,%d,%d",
                  veh->name, veh->abs_sm_pos.x(), veh->abs_sm_pos.y(), veh->abs_sm_pos.z() );
        here.unregister_vehicle( veh );
        if( g->m.dirty_vehicle_list.contains( veh ) ) { g->m.dirty_vehicle_list.erase( veh ); }
        mark_detached_vehicle_footprint_dirty();
        return std::unique_ptr<vehicle>();
    }
    // Fairly ugly, could be better
    if( inbubble ) {
        level_cache &ch = g->m.get_cache( z );
        for( size_t i = 0; i < current_submap->vehicles.size(); i++ ) {
            if( current_submap->vehicles[i].get() == veh ) {
                ch.vehicle_list.erase( veh );
                ch.zone_vehicles.erase( veh );
                g->m.reset_vehicle_cache( );
                std::unique_ptr<vehicle> result = std::move( current_submap->vehicles[i] );
                current_submap->vehicles.erase( current_submap->vehicles.begin() + i );
                unregister_vehicle( veh );
                if( veh->tracking_on ) {
                    get_overmapbuffer( dimension_id_ ).remove_vehicle( veh );
                }
                g->m.dirty_vehicle_list.erase( veh );
                mark_detached_vehicle_footprint_dirty();
                veh->detach();
                veh->refresh_position();
                return result;
            }
        }
    } else {
        for( size_t i = 0; i < current_submap->vehicles.size(); i++ ) {
            if( current_submap->vehicles[i].get() == veh ) {
                std::unique_ptr<vehicle> result = std::move( current_submap->vehicles[i] );
                current_submap->vehicles.erase( current_submap->vehicles.begin() + i );
                unregister_vehicle( veh );
                if( veh->tracking_on ) {
                    get_overmapbuffer( dimension_id_ ).remove_vehicle( veh );
                }
                veh->detach();
                veh->refresh_position();
                return result;
            }
        }
    }
    debugmsg( "detach_vehicle can't find it!  name=%s, submap:%d,%d,%d", veh->name, veh->abs_sm_pos.x(),
              veh->abs_sm_pos.y(), veh->abs_sm_pos.z() );
    return std::unique_ptr<vehicle>();
}

void mapbuffer::destroy_vehicle( vehicle *veh,
                                 mapbuffer_lookup_options options )
{
    detach_vehicle( veh );
}

auto mapbuffer::partial_con_at( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> partial_con *
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return nullptr;
    }

    const auto iter = tile->sm->partial_constructions.find( tripoint_sm_ms( tile->local, p.z() ) );
    if( iter == tile->sm->partial_constructions.end() ) {
        return nullptr;
    }
    return iter->second.get();
}

auto mapbuffer::partial_con_set( const tripoint_abs_ms &p, std::unique_ptr<partial_con> con,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto inserted = tile->sm->partial_constructions.emplace( tripoint_sm_ms( tile->local, p.z() ),
                          std::move( con ) ).second;
    if( !inserted ) {
        debugmsg( "set partial con on top of terrain which already has a partial con" );
    }
    return inserted;
}

auto mapbuffer::partial_con_remove( const tripoint_abs_ms &p,
                                    const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    return tile->sm->partial_constructions.erase( tripoint_sm_ms( tile->local, p.z() ) ) > 0;
}

// ----- Tile property queries -----

auto mapbuffer::is_bashable( const tripoint_abs_ms &p, const bool allow_floor,
                             const mapbuffer_lookup_options options ) -> bool
{
    if( veh_at( p, options ).obstacle_at_part() ) {
        return true;
    }

    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    // Check furniture bash
    const auto furn = tile->sm->get_furn( tile->local );
    if( furn != f_null && furn.obj().bash.str_max != -1 ) {
        return true;
    }

    // Check terrain bash
    const auto &ter_bash = tile->sm->get_ter( tile->local ).obj().bash;
    return ter_bash.str_max != -1 && ( !ter_bash.bash_below || allow_floor );
}

auto mapbuffer::is_bashable_ter( const tripoint_abs_ms &p, const bool allow_floor,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto &ter_bash = tile->sm->get_ter( tile->local ).obj().bash;
    return ter_bash.str_max != -1 && ( ( !ter_bash.bash_below &&
                                         !tile->sm->get_ter( tile->local ).obj().has_flag( "VEH_TREAT_AS_BASH_BELOW" ) ) || allow_floor );
}

auto mapbuffer::is_bashable_furn( const tripoint_abs_ms &p,
                                  const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto furn = tile->sm->get_furn( tile->local );
    return furn != f_null && furn.obj().bash.str_max != -1;
}

auto mapbuffer::is_bashable_ter_furn( const tripoint_abs_ms &p, const bool allow_floor,
                                      const mapbuffer_lookup_options options ) -> bool
{
    return is_bashable_furn( p, options ) || is_bashable_ter( p, allow_floor, options );
}

auto mapbuffer::bash_strength( const tripoint_abs_ms &p, const bool allow_floor,
                               const mapbuffer_lookup_options options ) -> int
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return -1;
    }

    const auto furn = tile->sm->get_furn( tile->local );
    if( furn != f_null && furn.obj().bash.str_max != -1 ) {
        return furn.obj().bash.str_max;
    }

    const auto &ter_bash = tile->sm->get_ter( tile->local ).obj().bash;
    if( ter_bash.str_max != -1 && ( !ter_bash.bash_below || allow_floor ) ) {
        return ter_bash.str_max;
    }

    return -1;
}

auto mapbuffer::bash_resistance( const tripoint_abs_ms &p, const bool allow_floor,
                                 const mapbuffer_lookup_options options ) -> int
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return -1;
    }

    const auto furn = tile->sm->get_furn( tile->local );
    if( furn != f_null && furn.obj().bash.str_min != -1 ) {
        return furn.obj().bash.str_min;
    }

    const auto &ter_bash = tile->sm->get_ter( tile->local ).obj().bash;
    if( ter_bash.str_min != -1 && ( !ter_bash.bash_below || allow_floor ) ) {
        return ter_bash.str_min;
    }

    return -1;
}

auto mapbuffer::bash_rating( const int str, const tripoint_abs_ms &p, const bool allow_floor,
                             const mapbuffer_lookup_options options ) -> int
{
    if( str <= 0 ) {
        return -1;
    }

    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return -1;
    }

    const auto &furniture = tile->sm->get_furn( tile->local ).obj();
    const auto &terrain = tile->sm->get_ter( tile->local ).obj();
    const auto vp = veh_at( p, options );
    vehicle *const veh = vp ? &vp->vehicle() : nullptr;
    const int part = vp ? vp->part_index() : -1;

    // bash_rating_internal logic inlined
    bool furn_smash = false;
    bool ter_smash = false;
    if( furniture.id && furniture.bash.str_max != -1 ) {
        furn_smash = true;
    } else if( terrain.bash.str_max != -1 && ( !terrain.bash.bash_below || allow_floor ) ) {
        ter_smash = true;
    }

    if( veh != nullptr && vp && vp->obstacle_at_part() ) {
        return 2;
    }

    int bash_min = 0;
    int bash_max = 0;
    if( furn_smash ) {
        bash_min = furniture.bash.str_min;
        bash_max = furniture.bash.str_max;
    } else if( ter_smash ) {
        bash_min = terrain.bash.str_min;
        bash_max = terrain.bash.str_max;
    } else {
        return -1;
    }

    if( str < bash_min ) {
        return 1;
    } else if( str >= bash_min + ( bash_max - bash_min ) * 0.5 + 0.5 ) {
        return 10;
    } else if( str >= bash_min + ( bash_max - bash_min ) * 0.2 ) {
        return 7;
    } else if( str >= bash_min - bash_max * 0.2 ) {
        return 4;
    }

    return 1;
}

auto mapbuffer::is_divable( const tripoint_abs_ms &p,
                            const mapbuffer_lookup_options options ) -> bool
{
    const auto vp = veh_at( p, options ).part_with_feature( VPFLAG_BOARDABLE, true );
    if( vp ) {
        return false;
    }

    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    return tile->sm->get_ter( tile->local ).obj().has_flag( "SWIMMABLE" ) &&
           tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_DEEP_WATER );
}

auto mapbuffer::is_water_shallow_current( const tripoint_abs_ms &p,
        const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    return tile->sm->get_ter( tile->local ).obj().has_flag( "CURRENT" ) &&
           !tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_DEEP_WATER );
}

auto mapbuffer::has_items( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && !tile->sm->get_items( tile->local ).empty();
}

// ----- Nearby / radius queries -----

auto mapbuffer::has_nearby_fire( const tripoint_abs_ms &p, const int radius,
                                 const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &pt : points_in_radius( p, radius ) ) {
        const auto field_entry = get_field_entry( pt, fd_fire, options );
        if( field_entry != nullptr ) {
            return true;
        }
        {
            auto h = abs_tile_handle::fetch_terrain_only( *this, pt, options );
            if( h && h->has_flag_ter_or_furn( "USABLE_FIRE" ) ) {
                return true;
            }
        }
    }
    return false;
}

auto mapbuffer::has_nearby_table( const tripoint_abs_ms &p, const int radius,
                                  const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &pt : points_in_radius( p, radius ) ) {
        const auto vp = veh_at( pt, options );
        {
            auto h = abs_tile_handle::fetch_terrain_only( *this, pt, options );
            if( h && h->has_flag_ter_or_furn( "FLAT_SURF" ) ) {
                return true;
            }
        }
        if( vp && vp->vehicle().has_part( "FLAT_SURF" ) ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::has_nearby_chair( const tripoint_abs_ms &p, const int radius,
                                  const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &pt : points_in_radius( p, radius ) ) {
        const auto vp = veh_at( pt, options );
        {
            auto h = abs_tile_handle::fetch_terrain_only( *this, pt, options );
            if( h && h->has_flag_ter_or_furn( "CAN_SIT" ) ) {
                return true;
            }
        }
        if( vp && vp->vehicle().has_part( "SEAT" ) ) {
            return true;
        }
    }
    return false;
}

// ----- Flag / convenience checks -----

auto mapbuffer::can_put_items( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> bool
{
    if( can_put_items_ter_furn( p, options ) ) {
        return true;
    }
    const auto vp = veh_at( p, options );
    return static_cast<bool>( vp.part_with_feature( "CARGO", true ) );
}

auto mapbuffer::can_put_items_ter_furn( const tripoint_abs_ms &p,
                                        const mapbuffer_lookup_options options ) -> bool
{
    auto h = abs_tile_handle::fetch_terrain_only( *this, p, options );
    return h && !h->has_flag( "NOITEM" ) && !h->has_flag( "SEALED" );
}

auto mapbuffer::dangerous_field_at( const tripoint_abs_ms &p,
                                    const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto &fld = tile->sm->get_field( tile->local );
    for( const auto &pr : fld ) {
        if( pr.second.is_dangerous() ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::is_harvestable( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> bool
{
    const auto &harvest_here = get_harvest( p, options );
    return !harvest_here.is_null() && !harvest_here->empty();
}

auto mapbuffer::accessible_items( const tripoint_abs_ms &p,
                                  const mapbuffer_lookup_options options ) -> bool
{
    auto h = abs_tile_handle::fetch_terrain_only( *this, p, options );
    return h && ( !h->has_flag( "SEALED" ) || h->has_flag( "LIQUIDCONT" ) );
}

auto mapbuffer::is_wall_adjacent( const tripoint_abs_ms &p,
                                  const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &pt : points_in_radius( p, 1 ) ) {
        if( pt != p ) {
            auto h = abs_tile_handle::fetch( *this, pt, options );
            if( h && h->move_cost() == 0 ) {
                return true;
            }
        }
    }
    return false;
}

auto mapbuffer::is_flammable( const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    if( flammable_items_at( p, 0, options ) ) {
        return true;
    }
    {
        auto h = abs_tile_handle::fetch_terrain_only( *this, p, options );
        if( h && ( h->has_flag( "FLAMMABLE" ) || h->has_flag( "FLAMMABLE_ASH" ) ) ) {
            return true;
        }
    }
    if( get_field_intensity( p, fd_web, options ).value_or( 0 ) > 0 ) {
        return true;
    }
    return false;
}

auto mapbuffer::tinder_at( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> bool
{
    auto *items = get_items( p, options );
    if( !items ) {
        return false;
    }
    for( const auto &i : *items ) {
        if( ( *i ).has_flag( flag_TINDER ) ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::flammable_items_at( const tripoint_abs_ms &p, const int threshold,
                                    const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile || tile->sm->get_items( tile->local ).empty() ) {
        return false;
    }

    if( tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_SEALED ) &&
        !tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_ALLOW_FIELD_EFFECT ) ) {
        return false;
    }

    for( const auto &i : tile->sm->get_items( tile->local ) ) {
        if( ( *i ).flammable( threshold ) ) {
            return true;
        }
    }
    return false;
}

// ----- Data getters -----

auto mapbuffer::get_harvest( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> const harvest_id &
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return harvest_id::NULL_ID();
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here.obj().examine != iexamine::none ) {
        if( furn_here.obj().has_flag( TFLAG_HARVESTED ) ) {
            return harvest_id::NULL_ID();
        }
        return furn_here.obj().get_harvest();
    }
    const auto ter_here = tile->sm->get_ter( tile->local );
    if( ter_here.obj().has_flag( TFLAG_HARVESTED ) ) {
        return harvest_id::NULL_ID();
    }
    return ter_here.obj().get_harvest();
}

auto mapbuffer::get_harvest_names( const tripoint_abs_ms &p,
                                   const mapbuffer_lookup_options options ) -> const std::set<std::string> &
{
    const auto &harvest_here = get_harvest( p, options );
    static const std::set<std::string> empty_set;
    if( harvest_here.is_null() ) {
        return empty_set;
    }
    return harvest_here->names();
}

auto mapbuffer::get_ter_transforms_into( const tripoint_abs_ms &p,
        const mapbuffer_lookup_options options ) -> ter_id
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return t_null;
    }
    return tile->sm->get_ter( tile->local ).obj().transforms_into.id();
}

auto mapbuffer::get_furn_transforms_into( const tripoint_abs_ms &p,
        const mapbuffer_lookup_options options ) -> furn_id
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return f_null;
    }
    return tile->sm->get_furn( tile->local ).obj().transforms_into.id();
}

auto mapbuffer::tername( const tripoint_abs_ms &p,
                         const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return "unknown";
    }
    return tile->sm->get_ter( tile->local ).obj().name();
}

auto mapbuffer::name( const tripoint_abs_ms &p,
                      const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::string();
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null ) {
        return furn_here.obj().name();
    }

    const auto vp = veh_at( p, options );
    if( vp ) {
        const auto displayed = vp->part_displayed();
        if( displayed ) {
            return displayed->info().name();
        }
    }

    return tile->sm->get_ter( tile->local ).obj().name();
}

auto mapbuffer::disp_name( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::string();
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null ) {
        return string_format( _( "the %s" ), furn_here.obj().name() );
    }

    const auto vp = veh_at( p, options );
    if( vp ) {
        const auto displayed = vp->part_displayed();
        if( displayed ) {
            return string_format( _( "the %s" ), displayed->info().name() );
        }
    }

    return string_format( _( "the %s" ), tile->sm->get_ter( tile->local ).obj().name() );
}

auto mapbuffer::obstacle_name( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::string();
    }

    const auto vp = veh_at( p, options );
    if( vp && vp->obstacle_at_part() ) {
        const auto obst = vp->obstacle_at_part();
        return obst->info().name();
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null ) {
        return furn_here.obj().name();
    }

    return tile->sm->get_ter( tile->local ).obj().name();
}

auto mapbuffer::sees( const tripoint_abs_ms &F, const tripoint_abs_ms &T, const int range,
                      mapbuffer_lookup_options options ) -> bool
{
    int dummy = 0;
    return sees( F, T, range, dummy, options );
}

/**
 * This one is internal-only, we don't want to expose the slope tweaking ickiness outside the map class.
 **/
auto mapbuffer::sees( const tripoint_abs_ms &F, const tripoint_abs_ms &T, const int range,
                      int &bresenham_slope, mapbuffer_lookup_options options ) -> bool
{
    if( ( range >= 0 && range < rl_dist( F, T ) ) ) {
        bresenham_slope = 0;
        return false; // Out of range!
    }
    // Cannonicalize the order of the tripoints so the cache is reflexive.
    const tripoint_abs_ms &min = F < T ? F : T;
    const tripoint_abs_ms &max = !( F < T ) ? F : T;

    bool visible = true;

    // Ugly `if` for now
    if( F.z() == T.z() ) {

        auto last_point = F.xy();
        // Please someone make bresenham work with typed points, I'm running out of willpower
        bresenham( F.xy().raw(), T.xy().raw(), bresenham_slope,
        [this, &visible, &T, &last_point, options]( const point & new_point ) {
            // Exit before checking the last square, it's still visible even if opaque.
            if( new_point.x == T.x() && new_point.y == T.y() ) {
                return false;
            }
            const auto new_tripoint = tripoint_abs_ms( point_abs_ms( new_point ), T.z() );
            if( !is_transparent( new_tripoint, options ) ||
                obstructed_by_vehicle_rotation( tripoint_abs_ms( last_point, T.z() ),
                                                new_tripoint ) ) {
                visible = false;
                return false;
            }
            last_point = new_tripoint.xy();
            return true;
        } );
        return visible;
    }

    auto last_point = F;
    bresenham( F.raw(), T.raw(), bresenham_slope, 0,
    [this, &visible, &T, &last_point, options]( const tripoint & new_point ) {
        // Exit before checking the last square if it's not a vertical transition,
        // it's still visible even if opaque.
        if( new_point == T.raw() && last_point.z() == T.z() ) {
            return false;
        }

        // TODO: Allow transparent floors (and cache them!)
        if( new_point.z == last_point.z() ) {
            if( !is_transparent( tripoint_abs_ms( new_point ), options ) ||
                obstructed_by_vehicle_rotation( last_point, tripoint_abs_ms( new_point ) ) ) {
                visible = false;
                return false;
            }
        } else {
            const int max_z = std::max( new_point.z, last_point.z() );
            if( ( has_floor( tripoint_abs_ms{ new_point.x, new_point.y, max_z }, true, options ) ||
                  !is_transparent( tripoint_abs_ms{ new_point.x, new_point.y, last_point.z() }, options ) ) &&
                ( has_floor( {last_point.xy(), max_z}, true, options ) ||
                  !is_transparent( {last_point.xy(), new_point.z}, options ) ) ) {
                visible = false;
                return false;
            }
        }

        last_point = tripoint_abs_ms( new_point );
        return true;
    } );
    return visible;
}

auto mapbuffer::obstacle_coverage( const tripoint_abs_ms &loc1, const tripoint_abs_ms &loc2,
                                   const mapbuffer_lookup_options options ) -> int
{
    const auto tile1 = lookup_tile( *this, loc1, options );
    const auto tile2 = lookup_tile( *this, loc2, options );
    if( !tile1 || !tile2 ) {
        return 100;
    }
    // Can't hide if you are standing on furniture, or non-flat slowing-down terrain tile.
    if( tile1->sm->get_furn( tile1->local ).obj().id || ( move_cost( loc2, nullptr, options ) > 2 &&
            !tile2->sm->get_ter( tile2->local ).obj().has_flag( TFLAG_FLAT ) ) ) {
        return 0;
    }
    const point_bub_ms a( std::abs( loc1.x() - loc2.x() ) * 2, std::abs( loc1.y() - loc2.y() ) * 2 );
    int offset = std::min( a.x(), a.y() ) - ( std::max( a.x(), a.y() ) / 2 );
    tripoint_abs_ms obstaclepos;
    bresenham( loc2.raw(), loc1.raw(), offset, 0, [&obstaclepos]( const tripoint & new_point ) {
        // Only adjacent tile between you and enemy is checked for cover.
        obstaclepos = tripoint_abs_ms( new_point );
        return false;
    } );
    const auto obst_tile = lookup_tile( *this, obstaclepos, options );
    if( !obst_tile ) {
        return 100;
    }
    const auto obstacle_f = obst_tile->sm->get_furn( obst_tile->local );
    if( obstacle_f != f_null ) {
        return obstacle_f->coverage;
    }
    if( const auto vp = veh_at( obstaclepos, options ) ) {
        if( vp->obstacle_at_part() ) {
            return 60;
        } else if( !vp->part_with_feature( VPFLAG_AISLE, true ) ) {
            return 45;
        }
    }
    return obst_tile->sm->get_ter( obst_tile->local )->coverage;
}

auto mapbuffer::features( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::string();
    }

    std::string feats;
    const auto &ter = tile->sm->get_ter( tile->local ).obj();
    const auto furn_here = tile->sm->get_furn( tile->local );

    // Gather feature strings from terrain
    if( ter.has_flag( "BARRICADE" ) ) {
        feats += _( "barricaded" ) + std::string( " " );
    }
    if( ter.has_flag( "BASHED" ) ) {
        feats += _( "bashed" ) + std::string( " " );
    }
    if( ter.has_flag( "ROUGH" ) ) {
        feats += _( "rough" ) + std::string( " " );
    }
    if( ter.has_flag( "SHARP" ) ) {
        feats += _( "sharp" ) + std::string( " " );
    }
    if( ter.has_flag( "SHORT" ) ) {
        feats += _( "short" ) + std::string( " " );
    }
    if( ter.has_flag( "RAMP_UP" ) ) {
        feats += _( "ramp up" ) + std::string( " " );
    }
    if( ter.has_flag( "RAMP_DOWN" ) ) {
        feats += _( "ramp down" ) + std::string( " " );
    }

    // Gather from furniture
    if( furn_here != f_null ) {
        const auto &furn = furn_here.obj();
        if( furn.has_flag( "BARRICADE" ) ) {
            feats += _( "barricaded" ) + std::string( " " );
        }
        if( furn.has_flag( "BASHED" ) ) {
            feats += _( "bashed" ) + std::string( " " );
        }
        if( furn.has_flag( "ROUGH" ) ) {
            feats += _( "rough" ) + std::string( " " );
        }
        if( furn.has_flag( "SHARP" ) ) {
            feats += _( "sharp" ) + std::string( " " );
        }
        if( furn.has_flag( "SHORT" ) ) {
            feats += _( "short" ) + std::string( " " );
        }
    }

    if( !feats.empty() ) {
        feats.erase( feats.length() - 1, 1 ); // Remove trailing space
    }
    return feats;
}

auto mapbuffer::ranged_target_size( const tripoint_abs_ms &p,
                                    const mapbuffer_lookup_options options ) -> double
{
    auto h = abs_tile_handle::fetch( *this, p, options );
    if( h && h->move_cost() == 0 ) {
        return 1.0;
    }
    // No floor check in mapbuffer — return 0 for open air-like terrains
    const auto tile = lookup_tile( *this, p, options );
    if( tile && tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_NO_FLOOR ) ) {
        return 0.0;
    }
    // TODO: Size based on furniture/terrain cover
    return 0.1;
}

auto mapbuffer::max_volume( const tripoint_abs_ms &p,
                            const mapbuffer_lookup_options options ) -> units::volume
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return 0_ml;
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null ) {
        return furn_here.obj().max_volume;
    }
    return tile->sm->get_ter( tile->local ).obj().max_volume;
}

auto mapbuffer::free_volume( const tripoint_abs_ms &p,
                             const mapbuffer_lookup_options options ) -> units::volume
{
    return max_volume( p, options ) - stored_volume( p, options );
}

auto mapbuffer::stored_volume( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> units::volume
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return 0_ml;
    }
    units::volume vol = 0_ml;
    for( const auto &i : tile->sm->get_items( tile->local ) ) {
        vol += ( *i ).volume();
    }
    return vol;
}

// ----- Item search -----

auto mapbuffer::has_item_with( const tripoint_abs_ms &p,
                               const std::function<bool( const item & )> &filter,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    for( const auto &i : tile->sm->get_items( tile->local ) ) {
        if( filter( *i ) ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::has_adjacent_item_with( const tripoint_abs_ms &p,
                                        const std::function<bool( const item & )> &filter,
                                        const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &adj : points_in_radius( p, 1 ) ) {
        if( adj == p ) {
            continue;
        }
        auto *items = get_items( adj, options );
        if( !items || items->empty() ) {
            continue;
        }
        for( const auto &i : *items ) {
            if( filter( *i ) ) {
                return true;
            }
        }
    }
    return false;
}

auto mapbuffer::has_adjacent_furniture_with( const tripoint_abs_ms &p,
        const std::function<bool( const furn_t & )> &filter,
        const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &adj : points_in_radius( p, 1 ) ) {
        if( adj == p ) {
            continue;
        }
        const auto tile = lookup_tile( *this, adj, options );
        if( !tile ) {
            continue;
        }
        const auto furn_here = tile->sm->get_furn( tile->local );
        if( furn_here != f_null && filter( furn_here.obj() ) ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::has_adjacent_terrain_with( const tripoint_abs_ms &p,
        const std::function<bool( const ter_t & )> &filter,
        const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &adj : points_in_radius( p, 1 ) ) {
        if( adj == p ) {
            continue;
        }
        const auto tile = lookup_tile( *this, adj, options );
        if( !tile ) {
            continue;
        }
        if( filter( tile->sm->get_ter( tile->local ).obj() ) ) {
            return true;
        }
    }
    return false;
}

auto mapbuffer::sees_some_items( const tripoint_abs_ms &p, const tripoint_abs_ms &from,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile || tile->sm->get_items( tile->local ).empty() ) {
        return false;
    }

    // SEALED -> never visible
    if( tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_SEALED ) ) {
        return false;
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null ) {
        if( furn_here.obj().has_flag( "SEALED" ) ) {
            return false;
        }
        // CONTAINER -> only visible when adjacent or at same tile
        if( furn_here.obj().has_flag( "CONTAINER" ) ) {
            return square_dist( p.xy(), from.xy() ) <= 1;
        }
    }

    return true;
}

auto mapbuffer::could_see_items( const tripoint_abs_ms &p, const tripoint_abs_ms &from,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    // SEALED -> never visible regardless of items
    if( tile->sm->get_ter( tile->local ).obj().has_flag( TFLAG_SEALED ) ) {
        return false;
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null ) {
        if( furn_here.obj().has_flag( "SEALED" ) ) {
            return false;
        }
        // CONTAINER -> only visible when adjacent or at same tile
        if( furn_here.obj().has_flag( "CONTAINER" ) ) {
            return square_dist( p.xy(), from.xy() ) <= 1;
        }
    }

    return true;
}

// ----- Movement cost helpers -----

auto mapbuffer::move_cost_ter_furn( const tripoint_abs_ms &p,
                                    const mapbuffer_lookup_options options ) -> int
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return 2;
    }

    const auto &terrain = tile->sm->get_ter( tile->local ).obj();
    if( terrain.movecost == 0 ) {
        return 0;
    }

    const auto furn_here = tile->sm->get_furn( tile->local );
    if( furn_here != f_null && furn_here.obj().movecost < 0 ) {
        return 0;
    }

    int movecost = terrain.movecost;
    if( furn_here != f_null && furn_here.obj().movecost > 0 ) {
        movecost += furn_here.obj().movecost;
    }

    return movecost;
}

auto mapbuffer::impassable_ter_furn( const tripoint_abs_ms &p,
                                     const mapbuffer_lookup_options options ) -> bool
{
    return move_cost_ter_furn( p, options ) == 0;
}

auto mapbuffer::passable_ter_furn( const tripoint_abs_ms &p,
                                   const mapbuffer_lookup_options options ) -> bool
{
    return move_cost_ter_furn( p, options ) > 0;
}

// ----- Movement execution helpers (off-bubble support) -----

auto mapbuffer::ter( const tripoint_abs_ms &p,
                     const mapbuffer_lookup_options options ) -> std::optional<ter_id>
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }
    return tile->ter();
}

auto mapbuffer::furn( const tripoint_abs_ms &p,
                      const mapbuffer_lookup_options options ) -> std::optional<furn_id>
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }
    return tile->furn();
}

auto mapbuffer::furnname( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return std::string();
    }
    return tile->furnname();
}

auto mapbuffer::has_flag( const std::string &flag, const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    return tile->has_flag( flag );
}

auto mapbuffer::has_flag( ter_bitflags flag, const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    return tile->has_flag( flag );
}

auto mapbuffer::has_flag_ter( const std::string &flag, const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    return tile->has_flag_ter( flag );
}

auto mapbuffer::has_flag_ter_or_furn( const std::string &flag, const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    return tile->has_flag_ter_or_furn( flag );
}

auto mapbuffer::has_flag_ter_or_furn( ter_bitflags flag, const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    return tile->has_flag_ter_or_furn( flag );
}

auto mapbuffer::has_floor_or_support( const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    if( !tile->ter_obj().has_flag( TFLAG_NO_FLOOR ) ) {
        return true;
    }
    if( p.z() <= -OVERMAP_DEPTH ) {
        return false;
    }

    const auto below = p + tripoint_rel_ms::below();
    const auto below_tile = abs_tile_handle::fetch_terrain_only( *this, below, options );
    if( !below_tile ) {
        return false;
    }
    if( below_tile->ter_obj().movecost == 0 ) {
        return true;
    }
    if( below_tile->furn_obj().movecost < 0 ) {
        return true;
    }
    return veh_at( below, options ).has_value();
}

auto mapbuffer::has_floor( const tripoint_abs_ms &p, bool visible_only,
                           const mapbuffer_lookup_options options ) -> bool
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return false;
    }
    // Check the submap floor cache for a fast answer
    const auto sm_pos = project_to<coords::sm>( p );
    const submap *sm = get_submap( sm_pos, options );
    if( !sm ) {
        return false;
    }
    const auto split = project_remain<coords::sm>( p );
    const auto &cache = sm->floor_cache;
    // If floor cache is dirty we can't rebuild it here (needs map pointer),
    // fall back to has_floor_or_support which checks TFLAG_NO_FLOOR.
    if( sm->floor_dirty ) {
        return has_floor_or_support( p, options ) ||
               ( !visible_only && has_flag( TFLAG_Z_TRANSPARENT, p, options ) );
    }
    return cache[split.remainder.x()][split.remainder.y()] ||
           ( !visible_only && has_flag( TFLAG_Z_TRANSPARENT, p, options ) );
}

auto mapbuffer::is_transparent( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    // Transparency determined by terrain/furniture opacity
    return tile->ter_obj().transparent &&
           tile->furn_obj().transparent;
}

auto mapbuffer::is_outside( const tripoint_abs_ms &p,
                            const mapbuffer_lookup_options options ) -> bool
{
    if( const auto local = active_reality_bubble_local( p ) ) {
        return g->m.is_outside( *local );
    }
    const auto sm_pos = project_to<coords::sm>( p );
    const auto sm = get_submap( sm_pos, options );
    if( !sm ) {
        return false;
    }
    ensure_roof_above_cache( *this, *sm, options );
    const auto split = project_remain<coords::sm>( p );
    const auto has_roof = sm->roof_above_cache[split.remainder.x()][split.remainder.y()];
    const auto vp = veh_at( p, options );
    return !has_roof && !( vp && vp->is_inside() );
}

auto mapbuffer::combined_movecost( const tripoint_abs_ms &from, const tripoint_abs_ms &to,
                                   const vehicle *ignored_vehicle,
                                   const int modifier, const bool flying, const bool via_ramp,
                                   const mapbuffer_lookup_options options ) -> int
{
    const int cost1 = move_cost( from, ignored_vehicle, options );
    const int cost2 = move_cost( to, ignored_vehicle, options );
    const int mults[4] = { 0, 50, 71, 100 };
    size_t match = trigdist ? ( from.x() != to.x() ) + ( from.y() != to.y() ) +
                   ( from.z() != to.z() ) : 1;
    if( flying || from.z() == to.z() ) {
        return ( cost1 + cost2 ) * mults[match] / 2;
    }
    // Inter-z-level movement by foot (not flying)
    if( !valid_move( from, to, {
    .flying = flying,
    .via_ramp = via_ramp,
    .lookup = options,
} ) ) {
        return 0;
    }
    return ( cost1 + cost2 + modifier ) * mults[match] / 2;
}

auto mapbuffer::move_cost( const tripoint_abs_ms &p, const vehicle *ignored_vehicle,
                           const mapbuffer_lookup_options options ) -> int
{
    const auto tile = abs_tile_handle::fetch( *this, p, options );
    if( !tile ) {
        return 0;
    }
    return tile->move_cost( ignored_vehicle );
}

auto mapbuffer::hit_with_acid( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch( *this, p, options );
    if( !tile ) {
        return false;
    }
    if( tile->passable() ) {
        return false;    // Didn't hit the tile!
    }
    const ter_id t = tile->ter();
    if( t == t_wall_glass || t == t_wall_glass_alarm ||
        t == t_vat ) {
        set_ter( p, t_floor, options );
    } else if( t == t_door_c || t == t_door_locked || t == t_door_locked_peep ||
               t == t_door_locked_alarm ) {
        if( one_in( 3 ) ) {
            set_ter( p, t_door_b, options );
        }
    } else if( t == t_door_bar_c || t == t_door_bar_o || t == t_door_bar_locked || t == t_bars ||
               t == t_reb_cage ) {
        set_ter( p, t_floor, options );
        add_msg( m_warning, _( "The metal bars melt!" ) );
    } else if( t == t_door_b ) {
        if( one_in( 4 ) ) {
            set_ter( p, t_door_frame, options );
        } else {
            return false;
        }
    } else if( t == t_window || t == t_window_alarm || t == t_window_no_curtains ) {
        set_ter( p, t_window_empty, options );
    } else if( t == t_wax ) {
        set_ter( p, t_floor_wax, options );
    } else if( t == t_gas_pump || t == t_gas_pump_smashed ) {
        return false;
    } else if( t == t_card_science || t == t_card_military || t == t_card_industrial ) {
        set_ter( p, t_card_reader_broken, options );
    }
    return true;
}

// returns true if terrain stops fire
auto mapbuffer::hit_with_fire( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch( *this, p, options );
    if( !tile ) {
        return false;
    }
    if( tile->passable() ) {
        return false;    // Didn't hit the tile!
    }

    // non passable but flammable terrain, set it on fire
    if( tile->has_flag( "FLAMMABLE" ) || tile->has_flag( "FLAMMABLE_ASH" ) ) {
        add_field( p, { .type = fd_fire, .intensity = 3, .lookup = options } );
    }
    return true;
}

auto mapbuffer::can_open_door( const tripoint_abs_ms &p, bool inside,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }
    const auto &ter = tile->ter_obj();
    if( ter.open ) {
        return !has_flag( str_OPENCLOSE_INSIDE, p, options ) || inside;
    }
    const auto &furn = tile->furn_obj();
    if( furn.open ) {
        return !has_flag( str_OPENCLOSE_INSIDE, p, options ) || inside;
    }
    const auto vp = veh_at( p, options );
    if( vp ) {
        // Check for openable vehicle door part
        return vp->part_with_feature( "OPENABLE", true ).has_value();
    }
    return false;
}

auto mapbuffer::open_door( const tripoint_abs_ms &p, bool inside,
                           const mapbuffer_lookup_options options ) -> bool
{
    return open_door( p, {
        .inside = inside,
        .lookup = options,
    } );
}

auto mapbuffer::open_door( const tripoint_abs_ms &p,
                           const mapbuffer_open_door_options &options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options.lookup );
    if( !tile ) {
        return false;
    }

    // Try terrain door
    const auto &ter = tile->ter_obj();
    if( ter.open ) {
        if( has_flag( str_OPENCLOSE_INSIDE, p, options.lookup ) && !options.inside ) {
            return false;
        }
        if( !set_ter( p, ter.open, options.lookup ) ) {
            return false;
        }
        sound_event se;
        se.origin = p;
        se.volume = 50;
        se.category = sounds::sound_t::movement;
        se.movement_noise = true;
        se.description = _( "swish" );
        se.id = "open_door";
        se.variant = ter.id.str();
        sounds::sound( se );
        return true;
    }

    // Try furniture door
    const auto &furn = tile->furn_obj();
    if( furn.open ) {
        if( has_flag( str_OPENCLOSE_INSIDE, p, options.lookup ) && !options.inside ) {
            return false;
        }
        if( !set_furn( p, furn.open, options.lookup ) ) {
            return false;
        }
        sound_event se;
        se.origin = p;
        se.volume = 50;
        se.category = sounds::sound_t::movement;
        se.movement_noise = true;
        se.description = _( "swish" );
        se.id = "open_door";
        se.variant = furn.id.str();
        sounds::sound( se );
        return true;
    }

    // Try vehicle door
    const auto vp = veh_at( p, options.lookup );
    if( !vp ) {
        return false;
    }

    // Preserve the legacy, character-less query used by NPC and monster movement.
    // Character-aware callers apply the vehicle's ownership and door-lock rules below.
    if( options.who == nullptr && vp->vehicle().is_locked ) {
        return false;
    }

    if( options.who != nullptr && options.who->is_mounted() ) {
        const auto mounted_creature = options.who->mounted_creature.get();
        if( mounted_creature == nullptr || !mounted_creature->has_flag( MF_MOUNTABLE_DOORS ) ) {
            options.who->add_msg_if_player( m_info, _( "You can't open things while you're riding." ) );
            return false;
        }
    }

    const auto openable = vp->vehicle().next_part_to_open( vp->part_index(), !options.inside );
    if( openable < 0 ) {
        return false;
    }

    if( options.who != nullptr && options.who->is_avatar() &&
        !vp->vehicle().handle_potential_theft( *options.who->as_avatar() ) ) {
        return false;
    }

    const auto lock_part = vp.part_with_feature( "DOOR_LOCKING", true );
    const auto has_locked_door = lock_part.has_value() && lock_part->part().enabled;
    const auto is_owner = options.who != nullptr && vp->vehicle().is_owned_by( *options.who );
    if( has_locked_door && ( !is_owner || vp->vehicle().is_locked ) ) {
        return false;
    }

    vp->vehicle().open_all_at( openable );
    return true;
}

auto mapbuffer::close_door( const tripoint_abs_ms &p, bool inside, bool check_only,
                            const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return false;
    }

    if( has_flag( str_OPENCLOSE_INSIDE, p, options ) && !inside ) {
        return false;
    }

    const auto &ter = tile->ter_obj();
    const auto &furn = tile->furn_obj();

    if( ter.close && !furn.id ) {
        if( !check_only ) {
            sound_event se;
            se.origin = p;
            se.volume = 60;
            se.category = sounds::sound_t::movement;
            se.movement_noise = true;
            se.description = _( "swish" );
            se.id = "close_door";
            se.variant = ter.id.str();
            sounds::sound( se );
            return set_ter( p, ter.close, options );
        }
        return true;
    } else if( furn.close ) {
        if( !check_only ) {
            sound_event se;
            se.origin = p;
            se.volume = 60;
            se.category = sounds::sound_t::movement;
            se.movement_noise = true;
            se.description = _( "swish" );
            se.id = "close_door";
            se.variant = furn.id.str();
            sounds::sound( se );
            return set_furn( p, furn.close, options );
        }
        return true;
    }
    return false;
}

auto mapbuffer::forced_door_closing( const tripoint_abs_ms &p, const ter_id &door_type,
                                     int bash_dmg,
                                     const mapbuffer_lookup_options options ) -> bool
{
    const auto valid_location = [&]( const tripoint_abs_ms & p ) {
        return ( passable( p, options ) ||
                 has_flag( "LIQUID", p, options ) ) &&
               g->critter_at( p ) == nullptr;
    };
    const auto get_random_point = [&]() -> tripoint_abs_ms {
        tripoint_abs_ms center;
        // Use radius iteration over simulated tiles to find a valid push point
        for( const auto &neighbor : simulated_tiles_in_radius( *this, p, 2 ) )
        {
            if( neighbor.abs_pos() != p && valid_location( neighbor.abs_pos() ) ) {
                return tripoint_abs_ms( p.raw() * 2 - neighbor.abs_pos().raw() );
            }
        }
        return p;
    };

    const std::string &door_name = door_type.obj().name();
    const auto kbp = get_random_point();

    // can't pushback any creatures/items anywhere, that means the door can't close.
    const bool cannot_push = kbp == p;
    const bool can_see = g->u.sees( p );

    auto creature = creature_at( p, false );
    auto *npc_or_player = creature ? creature->as_character() : nullptr;
    if( npc_or_player != nullptr ) {
        if( bash_dmg <= 0 ) {
            return false;
        }
        if( npc_or_player->is_npc() && can_see ) {
            add_msg( _( "The %1$s hits the %2$s." ), door_name, npc_or_player->name );
        } else if( npc_or_player->is_player() ) {
            add_msg( m_bad, _( "The %s hits you." ), door_name );
        }
        if( npc_or_player->activity ) {
            npc_or_player->cancel_activity();
        }
        npc_or_player->hitall( bash_dmg, 0, nullptr );
        if( cannot_push ) {
            return false;
        }
        g->knockback( kbp, p, std::max( 1, bash_dmg / 10 ), -1, 1, nullptr );
    }
    if( monster *const mon_ptr = creature ? creature->as_monster() : nullptr ) {
        monster &critter = *mon_ptr;
        if( bash_dmg <= 0 ) {
            return false;
        }
        if( can_see ) {
            add_msg( _( "The %1$s hits the %2$s." ), door_name, critter.name() );
        }
        if( critter.type->size <= creature_size::small ) {
            critter.die_in_explosion( nullptr );
        } else {
            critter.apply_damage( nullptr, bodypart_id( "torso" ), bash_dmg );
            critter.check_dead_state();
        }
        if( !critter.is_dead() && critter.type->size >= creature_size::huge ) {
            return false;
        }
        if( !critter.is_dead() ) {
            if( cannot_push ) {
                return false;
            }
            g->knockback( kbp, p, std::max( 1, bash_dmg / 10 ), -1, 1, nullptr );
            if( g->critter_at( p ) ) {
                return false;
            }
        }
    }


    if( const optional_vpart_position vp = veh_at( p, options ) ) {
        if( bash_dmg <= 0 ) {
            return false;
        }
        vp->vehicle().damage( vp->part_index(), bash_dmg );
        if( veh_at( p, options ) ) {
            return false;
        }
    }

    // Item checks — use mapbuffer
    if( bash_dmg < 0 && !has_items( p, options ) ) {
        return false;
    }
    if( bash_dmg == 0 ) {
        auto *items = get_items( p, options );
        if( items ) {
            for( const auto &elem : *items ) {
                if( elem->made_of( LIQUID ) ) {
                    continue;
                } else if( elem->volume() < 250_ml ) {
                    continue;
                }
                return false;
            }
        }
    }

    // Set the door terrain
    set_ter( p, door_type, options );

    // Clear items if NOITEM flag
    if( has_flag( "NOITEM", p, options ) ) {
        auto *items = get_items( p, options );
        if( items ) {
            for( auto it = items->begin(); it != items->end(); ) {
                item *elem = *it;
                if( elem->made_of( LIQUID ) ) {
                    it = items->erase( it );
                    continue;
                }
                if( elem->can_shatter() && one_in( 2 ) ) {
                    if( can_see ) {
                        add_msg( m_warning, _( "A %s shatters!" ), elem->tname() );
                    } else {
                        add_msg( m_warning, _( "Something shatters!" ) );
                    }
                    it = items->erase( it );
                    continue;
                }
                if( cannot_push ) {
                    return false;
                }
                detached_ptr<item> det = items->remove( elem );
                add_item_or_charges( kbp, std::move( det ) );
                // Re-get iterator since remove invalidated it
                it = items->begin();
            }
        }
    }
    return true;
}

static const ter_str_id t_rock_floor_no_roof( "t_rock_floor_no_roof" );

ter_id mapbuffer::get_roof( const tripoint_abs_ms &p, const bool allow_air,
                            mapbuffer_lookup_options options )
{
    if( p.z() <= -OVERMAP_DEPTH ) {
        // Could be magma/"void" instead
        return t_rock_floor;
    }
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( !tile ) {
        return t_null;
    }
    const auto &ter_there = tile->ter().obj();
    const auto &roof = ter_there.roof;
    if( !roof ) {
        // No roof
        if( !allow_air ) {
            // TODO: Biomes? By setting? Forbid and treat as bug?
            if( p.z() < 0 ) {
                return t_rock_floor_no_roof;
            }

            return t_dirt;
        }

        return t_open_air;
    }

    ter_id new_ter = roof.id();
    if( new_ter == t_null ) {
        debugmsg( "map::get_new_floor: %d,%d,%d has invalid roof type %s",
                  p.x(), p.y(), p.z(), roof.c_str() );
        return t_dirt;
    }

    if( p.z() == -1 && new_ter == t_rock_floor ) {
        // HACK: A hack to work around not having a "solid earth" tile
        new_ter = t_dirt;
    }

    return new_ter;
}

auto mapbuffer::place_items( const item_group_id &loc, int chance,
                             const tripoint_abs_ms &p1, const tripoint_abs_ms &p2,
                             bool ongrass, const time_point &turn,
                             int magazine, int ammo,
                             const mapbuffer_lookup_options options ) -> std::vector<item *>
{
    std::vector<item *> res;
    itype_id it;
    bool is_type = false;
    if( chance > 100 || chance <= 0 ) {
        debugmsg( "mapbuffer::place_items() called with an invalid chance (%d)", chance );
        return res;
    }
    if( !item_group::group_is_defined( loc ) ) {
        it = itype_id( loc.str() );
        if( !it.is_valid() ) {
            const tripoint_abs_omt omt( project_to<coords::omt>( g->u.abs_pos() ) );
            const oter_id &oid = get_overmapbuffer( dimension_id_ ).ter( omt );
            debugmsg( "place_items: invalid item group / item '%s', om_terrain = '%s' (%s)",
                      loc.c_str(), oid.id().c_str(), oid->get_mapgen_id().c_str() );
            return res;
        }
        is_type = true;
    }

    const float spawn_rate = get_option<float>( "ITEM_SPAWNRATE" );
    const int spawn_count = roll_remainder( chance * spawn_rate / 100.0f );

    for( int i = 0; i < spawn_count; i++ ) {
        int tries = 0;
        auto is_valid_terrain = [this, ongrass, &options]( const tripoint_abs_ms & p ) {
            const auto ter_opt = ter( p, options );
            if( !ter_opt ) {
                return false;
            }
            const ter_t &terrain = ter_opt->obj();
            return terrain.movecost == 0 &&
                   !terrain.has_flag( "PLACE_ITEM" ) &&
                   !ongrass &&
                   !terrain.has_flag( "FLAT" );
        };

        tripoint_abs_ms p;
        do {
            p.x() = rng( p1.x(), p2.x() );
            p.y() = rng( p1.y(), p2.y() );
            p.z() = p1.z();
            tries++;
        } while( is_valid_terrain( p ) && tries < 20 );

        if( tries < 20 ) {
            if( is_type ) {
                detached_ptr<item> placed = add_item_or_charges( p, item::spawn( it ) );
                if( placed ) {
                    res.push_back( std::move( &*placed ) );
                }
                return res;
            }
            std::vector<detached_ptr<item>> initial = item_group::items_from( loc, turn );

            for( detached_ptr<item> &itm : initial ) {
                const std::string &cat = itm->get_category().id.c_str();
                float cat_rate = get_option<float>( "SPAWN_RATE_" + cat );
                if( itm->goes_bad_after_opening( true ) ) {
                    float spawn_rate_mod = get_option<float>( "SPAWN_RATE_perishables_canned" );
                    cat_rate *= spawn_rate_mod;
                } else if( itm->goes_bad() ) {
                    float spawn_rate_mod = get_option<float>( "SPAWN_RATE_perishables" );
                    cat_rate *= spawn_rate_mod;
                }
                cat_rate = cat_rate > 1.0f ? roll_remainder( cat_rate ) : cat_rate;

                if( cat_rate <= 1.0f ) {
                    if( rng_float( 0.1f, 1.0f ) <= cat_rate ) {
                        detached_ptr<item> placed = add_item_or_charges( p, std::move( itm ) );
                        if( placed ) {
                            res.push_back( std::move( &*placed ) );
                        }
                    }
                } else {
                    const item &real_item = *itm;

                    detached_ptr<item> placed = add_item_or_charges( p, std::move( itm ) );
                    if( placed ) {
                        res.push_back( std::move( &*placed ) );
                    }

                    std::vector<detached_ptr<item>> extra = item_group::items_from( loc, turn );
                    extra.erase(
                        std::remove_if(
                            extra.begin(), extra.end(),
                    [&real_item]( const detached_ptr<item> &it ) {
                        return item_category_id( it->get_category_id() )
                               != item_category_id( real_item.get_category_id() );
                    }
                        ),
                    extra.end()
                    );

                    int base_count = static_cast<int>( cat_rate );
                    for( int i = 0; i < base_count; i++ ) {
                        if( extra.empty() ) {
                            break;
                        }
                        int idx = rng( 0, static_cast<int>( extra.size() ) - 1 );
                        detached_ptr<item> spawned = add_item_or_charges( p, std::move( extra[idx] ) );
                        if( spawned ) {
                            res.push_back( std::move( &*spawned ) );
                        }
                    }

                    if( rng_float( 0.0f, 1.0f ) < ( cat_rate - static_cast<float>( base_count ) ) ) {
                        if( !extra.empty() ) {
                            int idx = rng( 0, static_cast<int>( extra.size() ) - 1 );
                            detached_ptr<item> spawned = add_item_or_charges( p, std::move( extra[idx] ) );
                            if( spawned ) {
                                res.push_back( std::move( &*spawned ) );
                            }
                        }
                    }
                }
            }
        }
    }

    for( auto e : res ) {
        if( e->is_tool() || e->is_gun() || e->is_magazine() ) {
            if( rng( 0, 99 ) < magazine && !e->magazine_current() &&
                e->magazine_default() != itype_id::NULL_ID() ) {
                e->put_in( item::spawn( e->magazine_default(), e->birthday() ) );
            }
            if( rng( 0, 99 ) < ammo && e->ammo_remaining() == 0 ) {
                e->ammo_set( e->ammo_default(), e->ammo_capacity() );
            }
        }
    }
    return res;
}

static bool furn_is_supported( mapbuffer &m, const tripoint_abs_ms &p )
{
    const signed char cx[4] = { 0, -1, 0, 1};
    const signed char cy[4] = { -1,  0, 1, 0};

    for( int i = 0; i < 4; i++ ) {
        const auto adj =  p.xy() + point_rel_ms( cx[i], cy[i] );
        auto furn = m.furn( tripoint_abs_ms( adj, p.z() ) );
        if( furn && furn->obj().has_flag( "BLOCKSDOOR" ) ) {
            return true;
        }
    }

    return false;
}

static auto get_sound_volume( const map_bash_info &bash, const bash_params &params ) -> int
{
    // Just take the minimum/base volume at 20dB.
    const auto minvol = 20;
    // Set maxvol to 140dB, which can be deafening for extreme impacts.
    const auto maxvol = 140;
    const auto impact_strength = params.destroy ? bash.str_max : params.strength;
    return units::to_decibel( bash.sound_vol.value_or(
                                  units::from_decibel( std::clamp( minvol + impact_strength, minvol, maxvol ) ) ) );
}

static void set_bash_sound_source( sound_event &se, const bash_params &params )
{
    if( !params.caused_by_player ) {
        return;
    }

    auto &player_character = get_avatar();
    se.from_player = true;
    se.faction = player_character.get_faction()->id;
    se.monfaction = player_character.get_faction()->mon_faction;
}

static auto release_avatar_grabbed_furniture_if_destroyed( const tripoint_abs_ms &p,
        const furn_t &old_furniture, const furn_id &new_furniture ) -> void
{
    avatar &you = get_avatar();
    if( you.get_grab_type() == OBJECT_FURNITURE &&
        you.abs_pos() + you.grab_point == p &&
        !new_furniture.obj().is_movable() ) {
        add_msg( _( "The %s you were grabbing is destroyed!" ), old_furniture.name() );
        you.grab( OBJECT_NONE );
    }
}

bash_results mapbuffer::bash_ter_success( const tripoint_abs_ms &p, const bash_params &params,
        mapbuffer_lookup_options options )
{
    bash_results result;
    const auto maybe_ter = ter( p, options );
    if( !maybe_ter ) {
        return result;
    }
    result.success = true;
    const ter_t &ter_before = maybe_ter->obj();
    const map_bash_info &bash = ter_before.bash;
    if( has_flag_ter( "FUNGUS", p, options ) ) {
        fungal_effects( *g, *this ).create_spores( p );
    }
    const std::string soundfxvariant = ter_before.id.str();
    const bool will_collapse = ter_before.has_flag( TFLAG_SUPPORTS_ROOF );
    const bool suspended = ter_before.has_flag( TFLAG_SUSPENDED );
    bool follow_below = false;
    if( params.bashing_from_above && bash.ter_set_bashed_from_above ) {
        // If this terrain is being bashed from above and this terrain
        // has a valid post-destroy bashed-from-above terrain, set it
        set_ter( p, bash.ter_set_bashed_from_above, options );
    } else if( bash.ter_set ) {
        // If the terrain has a valid post-destroy terrain, set it
        set_ter( p, bash.ter_set, options );
        follow_below |= bash.bash_below;
    } else if( suspended ) {
        // Its important that we change the ter value before recursing, otherwise we'll hit an infinite loop.
        // This could be prevented by assembling a visited list, but in order to avoid that cost, we're going
        // build our recursion to just be resilient.
        set_ter( p, t_open_air, options );
        propagate_suspension_check( p );
    } else {
        tripoint_abs_ms below( p.xy(), p.z() - 1 );
        const ter_t &ter_below = ter( below, options )->obj();
        // Only setting the flag here because we want drops and sounds in correct order
        follow_below |= bash.bash_below && ter_below.roof;

        set_ter( p, t_open_air, options );
    }

    spawn_items( p, item_group::items_from( bash.drop_group, calendar::turn ), options );

    if( !bash.sound.empty() && !params.silent ) {
        static const std::string soundfxid = "smash_success";
        const auto sound_volume = get_sound_volume( bash, params );
        sound_event se;
        se.origin = p;
        se.volume = sound_volume;
        se.category = sounds::sound_t::combat;
        se.description = bash.sound.translated();
        se.id = soundfxid;
        se.variant = soundfxvariant;
        set_bash_sound_source( se, params );
        sounds::sound( se );
    }

    if( !params.bashing_from_above &&
        ( follow_below || ter( p, options ) == t_open_air ) ) {
        const tripoint_abs_ms below( p.xy(), p.z() - 1 );
        // We may need multiple bashes in some weird cases
        // Example:
        //   W has roof A
        //   A bashes to B
        //   B bashes to nothing
        //   Below our point P, there is a W
        // If we bash down a B over a W, it might be from earlier A or just constructed over it!
        //
        // Current solution: bash roof until you reach same roof type twice, then bash down
        if( follow_below && params.do_recurse ) {
            bool blocked_by_roof = false;
            std::set<ter_id> encountered_types;
            encountered_types.insert( ter_before.id );
            encountered_types.insert( t_open_air );
            // Note: we're bashing the new roof, not the tile supported by it!
            int down_bash_tries = 10;
            do {
                const ter_id &ter_now = ter( p, options )->id();
                if( encountered_types.contains( ter_now ) ) {
                    // We have encountered this type before and destroyed it (didn't block us)
                    set_ter( p, t_open_air, options );
                    bash_params params_below = params;
                    params_below.bashing_from_above = true;
                    params_below.bash_floor = false;
                    params_below.do_recurse = false;
                    params_below.destroy = true;
                    int impassable_bash_tries = 10;
                    // Unconditionally destroy, but don't go deeper
                    do {
                        result |= bash_ter_success( below, params_below, options );
                    } while( ter( below, options )->obj().movecost == 0 && impassable_bash_tries-- > 0 );
                    if( impassable_bash_tries <= 0 ) {
                        debugmsg( "Loop in terrain bashing for type %s", ter_before.id.str() );
                    }
                } else if( ter_now == t_open_air ) {
                    const ter_id &roof = get_roof( below, params.bash_floor &&
                                                   ter( below, options )->obj().movecost != 0,
                                                   options );
                    if( roof != t_open_air ) {
                        set_ter( p, roof, options );
                    }
                } else {
                    // This floor/roof tile wasn't destroyed in this loop yet
                    encountered_types.insert( ter_now );
                    bash_params params_copy = params;
                    params_copy.do_recurse = false;
                    // TODO: Unwrap the calls, don't recurse
                    // TODO: Don't bash furn
                    bash_results results_sub = bash_ter_furn( p, params_copy, options );
                    result |= results_sub;
                    if( !results_sub.success ) {
                        // Blocked, as in "the roof was too strong to bash"
                        blocked_by_roof = true;
                    }
                }
            } while( down_bash_tries-- > 0 && !blocked_by_roof &&
                     ( ter( p, options ) != t_open_air || ter( p, options )->obj().movecost == 0 ||
                       ter( below, options )->obj().roof ) );
            if( down_bash_tries <= 0 ) {
                debugmsg( "Loop in terrain bashing for type %s", ter_before.id.str() );
            }
        } else {
            const ter_id &roof = get_roof( below, params.bash_floor &&
                                           ter( below, options )->obj().movecost != 0,
                                           options );

            set_ter( p, roof, options );
        }
    }

    if( will_collapse && !has_flag( TFLAG_SUPPORTS_ROOF, p, options ) ) {
        collapse_at( p, params.silent, true, bash.explosive > 0, options );
    }

    if( bash.explosive > 0 ) {
        // TODO Implement if the player triggered the explosive terrain
        auto bub_pos = active_reality_bubble_local( p );
        if( bub_pos ) {
            explosion_handler::explosion( *bub_pos, nullptr, bash.explosive, 0.8, false );
        }
    }

    return result;
}

bash_results mapbuffer::bash_furn_success( const tripoint_abs_ms &p, const bash_params &params,
        mapbuffer_lookup_options options )
{
    bash_results result;
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return result;
    }
    const auto &furnid = tile->sm->get_furn( tile->local ).obj();
    const map_bash_info &bash = furnid.bash;

    if( has_flag( TFLAG_FUNGUS, p, options ) ) {
        fungal_effects( *g, *this ).create_spores( p );
    }
    if( has_flag( "MIGO_NERVE", p, options ) ) {
        map_funcs::migo_nerve_cage_removal( *this, p, true );
    }
    std::string soundfxvariant = furnid.id.str();
    const bool tent = !bash.tent_centers.empty();

    // Special code to collapse the tent if destroyed
    if( tent ) {
        // Get ids of possible centers
        std::set<furn_id> centers;
        for( const auto &cur_id : bash.tent_centers ) {
            if( cur_id.is_valid() ) {
                centers.insert( cur_id );
            }
        }

        std::optional<std::pair<tripoint_abs_ms, furn_id>> tentp;

        // Find the center of the tent
        // First check if we're not currently bashing the center
        if( const auto furniture = furn( p, options ); furniture && centers.contains( *furniture ) ) {
            tentp.emplace( p, *furniture );
        } else {
            for( const auto &pt : simulated_tiles_in_radius( *this, p, bash.collapse_radius ) ) {
                const auto f_at = furn( pt.abs_pos(), options );
                // Check if we found the center of the current tent
                if( f_at && centers.contains( *f_at ) ) {
                    tentp.emplace( pt.abs_pos(), *f_at );
                    break;
                }
            }
        }
        // Didn't find any tent center, wreck the current tile
        if( !tentp ) {
            spawn_items( p, item_group::items_from( bash.drop_group, calendar::turn ), options );
            release_avatar_grabbed_furniture_if_destroyed( p, furnid, bash.furn_set );
            set_furn( p, bash.furn_set, options );
        } else {
            // Take the tent down
            const int rad = tentp->second.obj().bash.collapse_radius;
            for( const auto &pt : simulated_tiles_in_radius( *this, tentp->first, rad ) ) {
                const auto frn = furn( pt.abs_pos(), options );
                if( !frn || *frn == f_null ) {
                    continue;
                }

                const auto &furn_obj = frn->obj();
                const auto &recur_bash = furn_obj.bash;
                // Check if we share a center type and thus a "tent type"
                for( const auto &cur_id : recur_bash.tent_centers ) {
                    if( centers.contains( cur_id.id() ) ) {
                        // Found same center, wreck current tile
                        if( furn_obj.fluid_grid &&
                            furn_obj.fluid_grid->role == fluid_grid_role::tank ) {
                            fluid_grid::on_tank_removed( pt.abs_pos() );
                        }
                        spawn_items( p, item_group::items_from( recur_bash.drop_group, calendar::turn ), options );
                        release_avatar_grabbed_furniture_if_destroyed( pt.abs_pos(), furn_obj, recur_bash.furn_set );
                        set_furn( pt.abs_pos(), recur_bash.furn_set, options );
                        break;
                    }
                }
            }
        }
        soundfxvariant = "smash_cloth";
    } else {
        if( furnid.fluid_grid && furnid.fluid_grid->role == fluid_grid_role::tank ) {
            fluid_grid::on_tank_removed( p );
        }
        release_avatar_grabbed_furniture_if_destroyed( p, furnid, bash.furn_set );
        set_furn( p, bash.furn_set, options );
        for( auto it : *get_items( p, options ) )  {
            it->on_drop();
        }
        // HACK: Hack alert.
        // Signs have cosmetics associated with them on the submap since
        // furniture can't store dynamic data to disk. To prevent writing
        // mysteriously appearing for a sign later built here, remove the
        // writing from the submap.
        delete_signage( p, options );
    }

    if( !tent ) {
        spawn_items( p, item_group::items_from( bash.drop_group, calendar::turn ), options );
    }

    if( !bash.sound.empty() && !params.silent ) {
        static const std::string soundfxid = "smash_success";
        const auto sound_volume = get_sound_volume( bash, params );
        sound_event se;
        se.origin = p;
        se.volume = sound_volume;
        se.category = sounds::sound_t::combat;
        se.description = bash.sound.translated();
        se.id = soundfxid;
        se.variant = soundfxvariant;
        set_bash_sound_source( se, params );
        sounds::sound( se );
    }

    if( bash.explosive > 0 ) {
        // TODO implement if the player triggered the explosive furniture
        auto bub_pos = active_reality_bubble_local( p );
        if( bub_pos ) {
            explosion_handler::explosion( *bub_pos, nullptr, bash.explosive, 0.8, false );
        }
    }

    return result;
}

bash_results mapbuffer::bash_ter_furn( const tripoint_abs_ms &p, const bash_params &params,
                                       mapbuffer_lookup_options options )
{
    bash_results result;
    std::string soundfxvariant;
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return result;
    }
    const auto &ter_obj = tile->sm->get_ter( tile->local ).obj();
    const auto &furn_obj = tile->sm->get_furn( tile->local ).obj();
    bool smash_ter = false;
    const map_bash_info *bash = nullptr;

    if( furn_obj.id && furn_obj.bash.str_max != -1 ) {
        bash = &furn_obj.bash;
        soundfxvariant = furn_obj.id.str();
    } else if( ter_obj.bash.str_max != -1 ) {
        bash = &ter_obj.bash;
        smash_ter = true;
        soundfxvariant = ter_obj.id.str();
    }

    // Floor bashing check
    // Only allow bashing floors when we want to bash floors and we're in z-level mode
    // Unless we're destroying, then it gets a little weird
    if( smash_ter && bash->bash_below && !params.bash_floor ) {
        if( !params.destroy ) {
            smash_ter = false;
            bash = nullptr;
        } else if( !bash->ter_set ) {
            // HACK: A hack for destroy && !bash_floor
            // We have to check what would we create and cancel if it is what we have now
            auto below = p + tripoint_rel_ms::below();
            const auto roof = get_roof( below, false, options );
            if( roof == ter( p, options ) ) {
                smash_ter = false;
                bash = nullptr;
            }
        } else if( !bash->ter_set && ter( p, options ) == t_dirt ) {
            // As above, except for no-z-levels case
            smash_ter = false;
            bash = nullptr;
        }
    }

    // TODO: what if silent is true?
    if( has_flag( "ALARMED", p, options ) && !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
        sound_event se;
        se.origin = p;
        se.volume = 90;
        se.category = sounds::sound_t::alarm;
        se.description = _( "an alarm go off!" );
        se.id = "environment";
        se.variant = "alarm";
        sounds::sound( se );
        // Blame nearby player
        if( rl_dist( g->u.abs_pos(), p ) <= 3 ) {
            g->events().send<event_type::triggers_alarm>( g->u.getID() );
            const auto abs = project_to<coords::sm>( p.xy() );
            g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0,
                                 tripoint_abs_sm( abs, p.z() ) );
        }
    }

    if( bash == nullptr || ( bash->destroy_only && !params.destroy ) ) {
        // Nothing bashable here
        if( !passable( p, options ) ) {
            if( !params.silent ) {
                sound_event se;
                se.origin = p;
                se.volume = 80;
                se.category = sounds::sound_t::combat;
                se.description = _( "thump!" );
                se.id = "smash_fail";
                se.variant = "default";
                set_bash_sound_source( se, params );
                sounds::sound( se );
            }

            result.did_bash = true;
            result.bashed_solid = true;
        }

        return result;
    }

    result.did_bash = true;
    result.bashed_solid = true;
    result.success = params.destroy;

    int smin = bash->str_min;
    int smax = bash->str_max;
    if( !params.destroy ) {
        if( bash->str_min_blocked != -1 || bash->str_max_blocked != -1 ) {
            if( furn_is_supported( *this, p ) ) {
                if( bash->str_min_blocked != -1 ) {
                    smin = bash->str_min_blocked;
                }
                if( bash->str_max_blocked != -1 ) {
                    smax = bash->str_max_blocked;
                }
            }
        }

        if( bash->str_min_supported != -1 || bash->str_max_supported != -1 ) {
            auto below = p + tripoint_rel_ms::below();
            if( has_flag( TFLAG_SUPPORTS_ROOF, below, options ) ) {
                if( bash->str_min_supported != -1 ) {
                    smin = bash->str_min_supported;
                }
                if( bash->str_max_supported != -1 ) {
                    smax = bash->str_max_supported;
                }
            }
        }
        // Linear interpolation from str_min to str_max
        const int resistance = smin + ( params.roll * ( smax - smin ) );
        if( params.strength >= resistance ) {
            result.success = true;
        }
    }

    if( !result.success ) {
        // Cap out bash volume to 120dB for sanity checking.
        const auto sound_volume =
            std::min( 120, units::to_decibel( bash->sound_fail_vol.value_or( 70_dB ) ) );

        result.did_bash = true;
        if( !params.silent ) {
            sound_event se;
            se.origin = p;
            se.volume = sound_volume;
            se.category = sounds::sound_t::combat;
            se.description = bash->sound_fail.translated();
            se.id = "smash_fail";
            se.variant = soundfxvariant;
            set_bash_sound_source( se, params );
            sounds::sound( se );
        }

        if( !smash_ter && smax > 0 ) {
            const auto flipped_version = get_furn_transforms_into( p );
            if( flipped_version != furn_str_id::NULL_ID() ) {
                const int damage_percent = ( params.strength * 100 ) / smax;
                if( rng( 1, 100 ) <= damage_percent ) {
                    set_furn( p, flipped_version, options );
                }
            }
        }
        // Hard impacts have a chance to dislodge targets perching above
        if( params.strength >= smin / 2 && one_in( smin / 2 ) ) {
            auto above = p + tripoint_rel_ms::above();
            Character *character = g->critter_at<Character>( above );
            if( has_flag( TFLAG_UNSTABLE, above, options ) && character != nullptr ) {
                character->add_msg_if_player( m_warning,
                                              _( "You feel the ground beneath you shake from the impact!" ) );

                if( character->stability_roll() < rng( 1, params.strength - ( smin / 2 ) ) ) {
                    character->add_msg_player_or_npc( m_bad, _( "You lose your balance!" ),
                                                      _( "<npcname> loses their balance!" ) );

                    g->fling_creature( character, rng_float( 0_degrees, 360_degrees ), 10 );
                }

            }
        }
    } else {
        if( smash_ter ) {
            result |= bash_ter_success( p, params, options );
        } else {
            result |= bash_furn_success( p, params, options );
        }
    }

    return result;
}

bash_results mapbuffer::bash( const tripoint_abs_ms &p, const int str,
                              bool silent, bool destroy, bool bash_floor,
                              const vehicle *bashing_vehicle,
                              mapbuffer_lookup_options options )
{
    const auto bsh = bash_params{
        .strength = str,
        .silent = silent,
        .destroy = destroy,
        .bash_floor = bash_floor,
        .roll = static_cast<float>( rng_float( 0, 1.0f ) ),
        .bashing_from_above = false,
        .do_recurse = true
    };
    return bash( p, bsh, bashing_vehicle, options );
}

bash_results mapbuffer::bash( const tripoint_abs_ms &p, const bash_params &bsh,
                              const vehicle *bashing_vehicle,
                              mapbuffer_lookup_options options )
{
    bash_results result;

    // Dimension bounds cannot be bashed - show message from boundary terrain
    if( is_outside_pocket_dimension_bounds( p ) ) {
        const auto &pocket_info = get_pocket_info();
        if( !bsh.silent && pocket_info ) {
            const ter_t &boundary_ter = pocket_info->bounds.boundary_terrain.obj();
            if( !boundary_ter.bash.sound_fail.empty() ) {
                add_msg( m_info, boundary_ter.bash.sound_fail.translated() );
            }
        }
        return result;  // Cannot bash dimension boundary
    }

    bool bashed_sealed = false;
    if( has_flag( "SEALED", p, options ) ) {
        result |= bash_ter_furn( p, bsh, options );
        bashed_sealed = true;
    }

    result |= bash_field( p, bsh, options );

    // Don't bash items inside terrain/furniture with SEALED flag
    if( !bashed_sealed ) {
        result |= bash_items( p, bsh, options );
    }
    // Don't bash the vehicle doing the bashing
    const vehicle *veh = veh_pointer_or_null( veh_at( p, options ) );
    if( veh != nullptr && veh != bashing_vehicle ) {
        result |= bash_vehicle( p, bsh, options );
    }

    // If we still didn't bash anything solid (a vehicle) or a tile with SEALED flag, bash ter/furn
    if( !result.bashed_solid && !bashed_sealed ) {
        result |= bash_ter_furn( p, bsh, options );
    }

    return result;
}

bash_results mapbuffer::bash_items( const tripoint_abs_ms &p, const bash_params &params,
                                    mapbuffer_lookup_options options )
{
    bash_results result;
    if( !has_items( p, options ) ) {
        return result;
    }

    std::vector<detached_ptr<item>> smashed_contents;
    auto &bashed_items = *get_items( p, options );
    bool smashed_glass = false;
    for( auto bashed_item = bashed_items.begin(); bashed_item != bashed_items.end(); ) {
        // the check for active suppresses Molotovs smashing themselves with their own explosion
        if( ( *bashed_item )->can_shatter() && !( *bashed_item )->is_active() &&
            one_in( 2 ) ) {
            result.did_bash = true;
            smashed_glass = true;
            for( detached_ptr<item> &bashed_content : ( *bashed_item )->contents.clear_items() ) {
                smashed_contents.push_back( std::move( bashed_content ) );
            }
            bashed_item = bashed_items.erase( bashed_item );
        } else {
            ++bashed_item;
        }
    }
    // Now plunk in the contents of the smashed items.
    spawn_items( p, std::move( smashed_contents ), options );

    // Add a glass sound even when something else also breaks
    if( smashed_glass && !params.silent ) {
        sound_event se;
        se.origin = p;
        se.volume = 70;
        se.category = sounds::sound_t::combat;
        se.description = _( "glass shattering" );
        se.id = "smash_success";
        se.variant = "smash_glass_contents";
        set_bash_sound_source( se, params );
        sounds::sound( se );
    }
    return result;
}

bash_results mapbuffer::bash_vehicle( const tripoint_abs_ms &p, const bash_params &params,
                                      mapbuffer_lookup_options options )
{
    bash_results result;
    // Smash vehicle if present
    if( const optional_vpart_position vp = veh_at( p, options ) ) {
        vp->vehicle().damage( vp->part_index(), params.strength, DT_BASH, true );
        if( !params.silent ) {
            sound_event se;
            se.origin = p;
            se.volume = 70;
            se.category = sounds::sound_t::combat;
            se.description = _( "crash!" );
            se.id = "smash_success";
            se.variant = "hit_vehicle";
            set_bash_sound_source( se, params );
            sounds::sound( se );
        }

        result.did_bash = true;
        result.success = true;
        result.bashed_solid = true;
    }
    return result;
}

bash_results mapbuffer::bash_field( const tripoint_abs_ms &p, const bash_params &,
                                    mapbuffer_lookup_options options )
{
    bash_results result;
    if( get_field_entry( p, fd_web, options ) != nullptr ) {
        result.did_bash = true;
        result.bashed_solid = true; // To prevent bashing furniture/vehicles
        remove_field( p, fd_web, options );
    }

    return result;
}

auto mapbuffer::destroy( const tripoint_abs_ms &p, bool silent,
                         const mapbuffer_lookup_options options ) -> void
{
    // Dimension bounds cannot be destroyed
    if( is_outside_pocket_dimension_bounds( p ) ) {
        return;
    }

    const auto tile = abs_tile_handle::fetch( *this, p, options );
    if( !tile ) {
        return;
    }

    // Break if it takes more than 25 destructions to remove to prevent infinite loops
    // Example: A bashes to B, B bashes to A leads to A->B->A->...

    // If we were destroying a floor, allow destroying floors
    // If we were destroying something unpassable, destroy only that
    bool was_impassable = !passable( p, options );
    int count = 0;
    while( count <= 25
           && bash( p, 999, silent, true, false, nullptr, options ).success
           && ( !was_impassable || !passable( p, options ) ) ) {
        count++;
    }
}

auto mapbuffer::board_vehicle( const tripoint_abs_ms &p, Character &who,
                               const mapbuffer_lookup_options options ) -> bool
{
    auto vp = veh_at( p, options ).part_with_feature( VPFLAG_BOARDABLE, true );
    if( !vp ) {
        return false;
    }
    if( vp->part().has_flag( vehicle_part::passenger_flag ) ) {
        Character *existing = g->critter_by_id<Character>( vp->part().passenger_id );
        if( existing ) {
            unboard_vehicle( p, false, options );
        }
    }
    vp->part().set_flag( vehicle_part::passenger_flag );
    vp->part().passenger_id = who.getID();
    vp->vehicle().invalidate_mass();
    who.in_vehicle = true;
    return true;
}

auto mapbuffer::unboard_vehicle( const tripoint_abs_ms &p, bool dead_passenger,
                                 const mapbuffer_lookup_options options ) -> void
{
    unboard_vehicle( p, {
        .dead_passenger = dead_passenger,
        .lookup = options,
    } );
}

auto mapbuffer::unboard_vehicle( const tripoint_abs_ms &p,
                                 const mapbuffer_unboard_vehicle_options &options ) -> void
{
    const auto vehicle_part_at_position = veh_at( p, options.lookup );
    if( !vehicle_part_at_position ) {
        if( options.passenger != nullptr ) {
            options.passenger->in_vehicle = false;
            options.passenger->controlling_vehicle = false;
        }
        return;
    }

    auto &veh = vehicle_part_at_position->vehicle();
    std::optional<vpart_reference> passenger_part;
    for( const auto &candidate : veh.get_any_parts( VPFLAG_BOARDABLE ) ) {
        if( !candidate.part().has_flag( vehicle_part::passenger_flag ) ) {
            continue;
        }
        if( options.passenger == nullptr ||
            candidate.part().passenger_id == options.passenger->getID() ) {
            passenger_part = candidate;
            break;
        }
    }
    if( !passenger_part ) {
        return;
    }

    auto &part = passenger_part->part();
    auto *passenger = options.passenger != nullptr ? options.passenger :
                      g->critter_by_id<Character>( part.passenger_id );
    part.remove_flag( vehicle_part::passenger_flag );
    veh.invalidate_mass();
    if( passenger && !options.dead_passenger ) {
        passenger->in_vehicle = false;
        if( passenger->controlling_vehicle ) {
            veh.skidding = true;
        }
        passenger->controlling_vehicle = false;
    }
}

auto mapbuffer::creature_in_field( Creature &critter,
                                   const mapbuffer_lookup_options options ) -> void
{
    const auto pos = critter.abs_pos();
    auto *curfield = get_field( pos, options );
    if( !curfield ) {
        return;
    }

    // Check vehicle status
    bool in_vehicle = false;
    if( const auto *u = critter.as_player() ) {
        in_vehicle = u->in_vehicle;
        if( in_vehicle ) {
            if( const auto vp = veh_at( pos, options ) ) {
                if( vp->part_with_feature( VPFLAG_NOFIELDS, true ) ) {
                    return;
                }
            }
        }
    }

    for( auto &field_entry_it : *curfield ) {
        field_entry &cur = field_entry_it.second;
        if( !cur.is_field_alive() ) {
            continue;
        }
        const field_type_id cur_field_id = cur.get_field_type();

        for( const auto &fe : cur.field_effects() ) {
            if( in_vehicle && fe.immune_in_vehicle ) {
                continue;
            }
            if( critter.is_immune_field( cur_field_id ) ||
                critter.is_immune_effect( fe.get_effect().get_id() ) ) {
                continue;
            }
            const effect field_fx = fe.get_effect();
            critter.add_effect( field_fx.get_id(), field_fx.get_duration(),
                                field_fx.get_bp(), field_fx.get_intensity() );
        }
    }
}

auto mapbuffer::for_each_vehicle( const std::function<void( vehicle & )> &fn ) -> void
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    for( vehicle *veh : loaded_vehicles_ ) {
        if( veh ) {
            fn( *veh );
        }
    }
}

auto mapbuffer::for_each_vehicle( const std::function<void( const vehicle & )> &fn ) const -> void
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    for( const vehicle *veh : loaded_vehicles_ ) {
        if( veh ) {
            fn( *veh );
        }
    }
}

auto mapbuffer::cheap_light_at( const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> float
{
    // Skip pocket dimension bounds
    if( is_outside_pocket_dimension_bounds( p ) ) {
        return 0.0f;
    }

    // Step 1: Natural light level (coordinate-independent, based on z-level + time)
    const float natural_light = g != nullptr ? g->natural_light_level( p.z() ) : LIGHT_AMBIENT_MINIMAL;

    // Step 2: Check if tile is outside for sky access
    const bool outside = is_outside( p, options );

    // Base ambient light: outside gets natural light, inside gets minimal
    float ambient = outside ? natural_light : LIGHT_AMBIENT_MINIMAL;

    // Step 3: Scan nearby luminous items (r=5) if we have luminous submap index
    const auto sm_pos = project_to<coords::sm>( p );
    const auto &luminous_submaps = get_submaps_with_luminous_items();
    if( luminous_submaps.contains( sm_pos ) ) {
        // Check items on this tile for luminance
        auto *items = get_items( p, options );
        if( items ) {
            for( const auto &it : *items ) {
                if( it->is_active() && it->getlight_emit() > 0 ) {
                    const float light_val = it->getlight_emit();
                    const int dist = 1; // same tile
                    ambient = std::max( ambient, light_val / ( dist * dist ) );
                }
            }
        }
    }

    // Step 4: Check terrain/furniture luminance
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p, options );
    if( tile ) {
        const auto lum = tile->lum();
        if( lum > 0 ) {
            ambient = std::max( ambient, static_cast<float>( lum ) );
        }
    }

    return ambient;
}

auto mapbuffer::get_heat_radiation( const tripoint_abs_ms &location, const bool direct,
                                    const mapbuffer_lookup_options options ) -> int
{
    int temp_mod = 0;
    int best_fire = 0;

    const auto visit_tile = [&]( const abs_tile_handle & tile ) {
        const auto dest = tile.abs_pos();
        int heat_intensity = 0;
        if( const auto *fire = tile.get_field_entry( fd_fire ) ) {
            heat_intensity = fire->get_field_intensity();
        } else {
            heat_intensity = tile.ter_obj().heat_radiation;
        }
        if( heat_intensity == 0 || !sees( location, dest, -1, options ) ) {
            return;
        }

        const int fire_dist = std::max( 1, square_dist( dest, location ) );
        temp_mod += 6 * heat_intensity * heat_intensity / fire_dist;
        best_fire = std::max( best_fire, heat_intensity );
    };

    if( options.mode == mapbuffer_lookup_mode::simulated_only ) {
        for( const auto &tile : simulated_tiles_in_radius( *this, location, 6 ) ) {
            visit_tile( tile );
        }
    } else {
        for( const tripoint_abs_ms &dest : points_in_radius( location, 6 ) ) {
            const auto tile = abs_tile_handle::fetch_terrain_only( *this, dest, options );
            if( tile ) {
                visit_tile( *tile );
            }
        }
    }

    return direct ? best_fire : temp_mod;
}

auto mapbuffer::get_convection_temperature( const tripoint_abs_ms &location,
        const mapbuffer_lookup_options options ) -> int
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, location, options );
    if( !tile ) {
        return 0;
    }

    int lava_mod = 0;
    if( const auto trap = get_trap( location, options ); trap && *trap == tr_lava ) {
        lava_mod = fd_fire.obj().get_convection_temperature_mod();
    }

    int temp_mod = 0;
    if( const auto *fields = get_field( location, options ) ) {
        for( const auto &entry : *fields ) {
            if( entry.first.obj().has_fire ) {
                lava_mod = 0;
            }
            temp_mod += entry.second.convection_temperature_mod();
        }
    }

    return temp_mod + lava_mod;
}

// ----- Field operations -----

auto mapbuffer::add_splatter( const field_type_id &type, const tripoint_abs_ms &where,
                              const int intensity,
                              const mapbuffer_lookup_options options ) -> void
{
    if( !type.id() || intensity <= 0 ) {
        return;
    }
    const auto existing = get_field_intensity( where, type, options );
    if( existing.has_value() && existing.value() > 0 ) {
        mod_field_intensity( where, {
            .type = type,
            .intensity = intensity,
            .isoffset = true,
            .lookup = options,
        } );
    } else {
        add_field( where, {
            .type = type,
            .intensity = intensity,
            .age = 0_turns,
            .lookup = options,
        } );
    }
}

auto mapbuffer::add_splatter_trail( const field_type_id &type, const tripoint_abs_ms &from,
                                    const tripoint_abs_ms &to,
                                    const mapbuffer_lookup_options options ) -> void
{
    const auto trail = line_to( from.xy(), to.xy() );
    for( const auto &p : trail ) {
        const tripoint_abs_ms pos( p, from.z() );
        if( is_column_state( project_to<coords::sm>( pos ).xy(),
                             submap_column_load_state::resident ) ) {
            add_splatter( type, pos, 1, options );
        }
    }
}

auto mapbuffer::add_splash( const field_type_id &type, const tripoint_abs_ms &center,
                            const int radius, const int intensity,
                            const mapbuffer_lookup_options options ) -> void
{
    for( const auto &pt : simulated_tiles_in_radius( *this, center, radius ) ) {
        add_splatter( type, pt.abs_pos(), intensity, options );
    }
}

auto mapbuffer::propagate_field( const tripoint_abs_ms &center, const field_type_id &type,
                                 int amount, const int max_intensity,
                                 const mapbuffer_lookup_options options ) -> void
{
    using gas_blast = std::pair<float, tripoint_abs_ms>;
    auto cmp = []( const gas_blast & a, const gas_blast & b ) {
        return a.first > b.first;
    };
    std::priority_queue<gas_blast, std::vector<gas_blast>, decltype( cmp )> open( cmp );
    std::unordered_set<tripoint_abs_ms> closed;
    open.emplace( 0.0f, center );

    const bool not_gas = type.obj().phase != GAS;

    while( amount > 0 && !open.empty() ) {
        tripoint_abs_ms cur_pos = open.top().second;
        if( closed.contains( cur_pos ) ) {
            open.pop();
            continue;
        }

        // All points with equal gas intensity should propagate at the same time
        std::list<gas_blast> gas_front;
        gas_front.push_back( open.top() );
        const int cur_intensity = get_field_intensity( cur_pos, type, options ).value_or( 0 );
        open.pop();
        while( !open.empty() ) {
            tripoint_abs_ms next_pos = open.top().second;
            if( get_field_intensity( next_pos, type, options ).value_or( 0 ) != cur_intensity ) {
                break;
            }
            if( !closed.contains( next_pos ) ) {
                gas_front.push_back( open.top() );
            }
            open.pop();
        }

        int increment = std::max<int>( 1, amount / static_cast<int>( gas_front.size() ) );

        while( !gas_front.empty() ) {
            gas_blast gp = random_entry_removed( gas_front );
            const tripoint_abs_ms &gp_pos = gp.second;
            closed.insert( gp_pos );
            const int cur_intensity = get_field_intensity( gp_pos, type, options ).value_or( 0 );
            if( cur_intensity < max_intensity ) {
                const int bonus = std::min( max_intensity - cur_intensity, increment );
                mod_field_intensity( gp_pos, {type, bonus, false, options} );
                amount -= bonus;
            } else {
                amount--;
            }

            if( amount <= 0 ) {
                return;
            }

            for( const auto &pt : simulated_tiles_in_radius( *this, gp_pos, 1 ) ) {
                const auto pt_pos = pt.abs_pos();
                if( pt_pos == gp_pos || closed.contains( pt_pos ) ) {
                    continue;
                }

                if( pt.impassable() && ( not_gas || !pt.has_flag( TFLAG_PERMEABLE ) ) ) {
                    closed.insert( pt_pos );
                    continue;
                }
                if( !obstructed_by_vehicle_rotation( gp_pos, pt_pos ) ) {
                    open.emplace( static_cast<float>( rl_dist( center, pt_pos ) ), pt_pos );
                }
            }
        }
    }
}

// ----- Item mutations -----

auto mapbuffer::spawn_item( const tripoint_abs_ms &p, const itype_id &type_id,
                            const unsigned quantity, const int charges,
                            const time_point &birthday, const int damlevel,
                            const mapbuffer_lookup_options options ) -> void
{
    for( unsigned i = 0; i < quantity; ++i ) {
        auto new_item = item::spawn( type_id, birthday, charges > 0 ? ( i == 0 ? charges : 0 ) : 0 );
        if( damlevel > 0 ) {
            new_item->set_damage( damlevel );
        }
        add_item( p, std::move( new_item ), options );
    }
}

auto mapbuffer::spawn_items( const tripoint_abs_ms &p,
                             std::vector<detached_ptr<item>> new_items,
                             const mapbuffer_lookup_options options )
-> std::vector<detached_ptr<item>> // *NOPAD*
{
    std::vector<detached_ptr<item>> remaining;
    for( auto &it : new_items ) {
        detached_ptr<item> leftover = add_item_or_charges( p, std::move( it ), {
            .lookup = options,
        } );
        if( !!leftover ) {
            remaining.emplace_back( std::move( leftover ) );
        }
    }
    return remaining;
}

// ----- Mutations -----

auto mapbuffer::make_rubble( const tripoint_abs_ms &p, const furn_id &rubble_type,
                             const ter_id &floor_type, const bool overwrite,
                             const mapbuffer_lookup_options options ) -> void
{
    if( overwrite ) {
        set_ter( p, floor_type, options );
        set_furn( p, rubble_type, options );
    } else {
        // First see if there is existing furniture to destroy
        if( is_bashable_furn( p, options ) ) {
            set_furn( p, f_null, options );
        }
        // Leave the terrain alone unless it interferes with furniture placement
        {
            auto h_mc = abs_tile_handle::fetch( *this, p, options );
            if( h_mc && h_mc->move_cost() == 0 && is_bashable_ter( p, true, options ) ) {
                set_ter( p, floor_type, options );
            }
        }
        // Check again for new terrain after potential destruction
        {
            auto h_mc2 = abs_tile_handle::fetch( *this, p, options );
            if( h_mc2 && h_mc2->move_cost() == 0 ) {
                set_ter( p, floor_type, options );
            }
        }

        set_furn( p, rubble_type, options );
    }
}

std::optional<tripoint_bub_ms>
mapbuffer::active_reality_bubble_local( const tripoint_abs_ms &p ) const
{
    if( g == nullptr ) {
        return std::nullopt;
    }

    if( g->m.get_bound_dimension() != dimension_id_ ) {
        return std::nullopt;
    }

    const auto local = abs_to_map_local( g->m, p );
    if( !g->m.inbounds( local ) ) {
        return std::nullopt;
    }

    return local;
}

auto mapbuffer::invalidate_active_terrain_set_caches( const tripoint_abs_ms &p,
        const ter_id &old_id,
        const ter_id &new_id ) const -> void
{
    const auto local = active_reality_bubble_local( p );
    if( !local ) {
        return;
    }

    auto &here = get_map();
    const auto &old_terrain = old_id.obj();
    const auto &new_terrain = new_id.obj();

    if( old_terrain.transparent != new_terrain.transparent ) {
        here.set_transparency_cache_dirty( *local );
        here.set_seen_cache_dirty( *local );
    }

    if( new_terrain.has_flag( TFLAG_NO_FLOOR ) != old_terrain.has_flag( TFLAG_NO_FLOOR ) ) {
        here.set_floor_cache_dirty( *local );
        here.support_cache_dirty.insert( *local );
        here.set_seen_cache_dirty( local->z() );
        here.set_seen_cache_dirty( local->z() - 1 );
        here.set_absorption_cache_dirty( *local );
        here.set_absorption_cache_dirty( local->z() - 1 );
    }

    if( new_terrain.has_flag( TFLAG_Z_TRANSPARENT ) != old_terrain.has_flag( TFLAG_Z_TRANSPARENT ) ) {
        here.set_floor_cache_dirty( *local );
        here.set_seen_cache_dirty( local->z() );
        here.set_seen_cache_dirty( local->z() - 1 );
    }

    if( new_terrain.has_flag( TFLAG_SUSPENDED ) != old_terrain.has_flag( TFLAG_SUSPENDED ) ) {
        here.set_suspension_cache_dirty( local->z() );
        if( new_terrain.has_flag( TFLAG_SUSPENDED ) ) {
            here.get_cache( local->z() ).suspension_cache.emplace_back( p.xy() );
        }
    }

    if( new_terrain.has_flag( TFLAG_BLOCK_WIND ) != old_terrain.has_flag( TFLAG_BLOCK_WIND ) ) {
        here.set_absorption_cache_dirty( *local );
    }

    if( new_terrain.has_flag( TFLAG_CONNECT_TO_WALL ) != old_terrain.has_flag(
            TFLAG_CONNECT_TO_WALL ) ) {
        here.set_absorption_cache_dirty( *local );
    }

    here.invalidate_max_populated_zlev( local->z() );
    here.set_memory_seen_cache_dirty( *local );
    here.set_pathfinding_cache_dirty( *local );
    here.support_dirty( tripoint_bub_ms( local->xy(), local->z() + 1 ) );
    here.invalidate_lightmap_caches();
    here.set_absorption_cache_dirty( *local );
}

auto mapbuffer::sync_furniture_change_side_tables( const tripoint_abs_ms &p, submap &sm,
        const point_sm_ms &local, const furn_id &old_id, const furn_id &new_id,
        const cata::poly_serialized<active_tile_data> *new_active ) const -> void
{
    const auto &old_furniture = old_id.obj();
    const auto &new_furniture = new_id.obj();
    auto *const tracker = get_distribution_grid_tracker_for( dimension_id_ );

    if( old_furniture.active ) {
        sm.active_furniture.erase( local );
        if( tracker != nullptr ) {
            tracker->on_changed( p );
        }
    }

    if( new_furniture.active || ( new_active != nullptr && *new_active ) ) {
        cata::poly_serialized<active_tile_data> atd;
        if( new_active != nullptr && *new_active ) {
            atd = *new_active;
        } else {
            atd.reset( new_furniture.active->clone() );
            atd->set_last_updated( calendar::turn );
        }
        sm.active_furniture[local] = atd;
        if( tracker != nullptr ) {
            tracker->on_changed( p );
        }
    }

    if( g != nullptr && g->m.get_bound_dimension() == dimension_id_ &&
        ( old_furniture.fluid_grid || new_furniture.fluid_grid ) ) {
        fluid_grid::on_structure_changed( p );
    }
}

auto mapbuffer::invalidate_active_furniture_set_caches( const tripoint_abs_ms &p,
        const furn_id &old_id, const furn_id &new_id ) const -> void
{
    const auto local = active_reality_bubble_local( p );
    if( !local ) {
        return;
    }

    auto &here = get_map();
    const auto &old_furniture = old_id.obj();
    const auto &new_furniture = new_id.obj();

    if( old_furniture.transparent != new_furniture.transparent ) {
        here.set_transparency_cache_dirty( *local );
        here.set_seen_cache_dirty( *local );
    }

    if( old_furniture.light_emitted != new_furniture.light_emitted ) {
        here.invalidate_lightmap_caches();
    }

    if( old_furniture.has_flag( TFLAG_NO_FLOOR ) != new_furniture.has_flag( TFLAG_NO_FLOOR ) ||
        old_furniture.has_flag( TFLAG_Z_TRANSPARENT ) != new_furniture.has_flag( TFLAG_Z_TRANSPARENT ) ) {
        here.set_floor_cache_dirty( *local );
        here.set_seen_cache_dirty( local->z() );
        here.set_seen_cache_dirty( local->z() - 1 );
    }

    if( old_furniture.has_flag( TFLAG_SUN_ROOF_ABOVE ) !=
        new_furniture.has_flag( TFLAG_SUN_ROOF_ABOVE ) ) {
        here.set_floor_cache_dirty( tripoint_bub_ms( local->xy(), local->z() + 1 ) );
    }

    if( old_furniture.has_flag( TFLAG_BLOCK_WIND ) != new_furniture.has_flag( TFLAG_BLOCK_WIND ) ||
        old_furniture.has_flag( TFLAG_CONNECT_TO_WALL ) !=
        new_furniture.has_flag( TFLAG_CONNECT_TO_WALL ) ) {
        here.set_absorption_cache_dirty( *local );
    }

    here.invalidate_max_populated_zlev( local->z() );
    here.set_memory_seen_cache_dirty( *local );
    here.set_pathfinding_cache_dirty( *local );
    here.support_dirty( *local );
    here.support_dirty( tripoint_bub_ms( local->xy(), local->z() + 1 ) );
    here.set_absorption_cache_dirty( *local );
}

auto mapbuffer::sync_active_trap_change_side_tables( const tripoint_abs_ms &p,
        const point_sm_ms &local_tile, const trap_id &old_id, const trap_id &new_id ) const -> void
{
    const auto local = active_reality_bubble_local( p );
    if( !local ) {
        return;
    }

    auto &here = g->m;
    const auto sm_abs = project_to<coords::sm>( p );

    if( old_id != tr_null ) {
        g->u.add_known_trap( bub_to_abs( *local ), tr_null.obj() );
        if( old_id.obj().is_funnel() ) {
            std::erase_if( here.funnel_locations_, [&]( const auto & entry ) {
                return entry.first == sm_abs && entry.second == local_tile;
            } );
        }
    }

    if( new_id.obj().is_funnel() ) {
        here.funnel_locations_.emplace_back( sm_abs, local_tile );
    }
}

auto mapbuffer::invalidate_active_field_add_caches( const tripoint_abs_ms &p,
        const field_type_id &type ) const -> void
{
    const auto local = active_reality_bubble_local( p );
    if( !local ) {
        return;
    }

    auto &here = g->m;
    const auto &field_type = type.obj();
    here.invalidate_max_populated_zlev( local->z() );

    if( field_type.dirty_transparency_cache || !field_type.is_transparent() ) {
        here.set_transparency_cache_dirty( *local );
        here.set_seen_cache_dirty( *local );
    }

    if( field_type.is_dangerous() ) {
        here.set_pathfinding_cache_dirty( *local );
    }

    if( field_type.accelerated_decay ) {
        here.support_dirty( *local );
    }
}

auto mapbuffer::invalidate_active_field_remove_caches( const tripoint_abs_ms &p,
        const field_type_id &type ) const -> void
{
    const auto local = active_reality_bubble_local( p );
    if( !local ) {
        return;
    }

    auto &here = g->m;
    const auto &field_type = type.obj();
    if( field_type.dirty_transparency_cache || !field_type.is_transparent() ) {
        here.set_transparency_cache_dirty( *local );
        here.set_seen_cache_dirty( *local );
    }

    if( field_type.is_dangerous() ) {
        here.set_pathfinding_cache_dirty( *local );
    }
}

void mapbuffer::sync_active_item_submap_index( const tripoint_abs_ms &p,
        const submap &sm )
{
    const auto abs_submap = project_to<coords::sm>( p );
    if( sm.active_items.empty() ) {
        submaps_with_active_items_.erase( abs_submap );
    } else {
        submaps_with_active_items_.insert( abs_submap );
    }
}

void mapbuffer::invalidate_active_item_luminance_cache( const tripoint_abs_ms &p ) const
{
    if( active_reality_bubble_local( p ) ) {
        g->m.invalidate_lightmap_caches();
    }
}

void mapbuffer::save( bool delete_after_save, bool notify_tracker, bool show_progress )
{
    const int num_total_submaps = static_cast<int>( submaps.size() );

    // Serial collection of unique OMT addresses with per-omt delete flags.
    // The UI progress popup runs here on the main thread only (show_progress=true).
    // When save() is dispatched from a worker thread (show_progress=false), the popup
    // is skipped to avoid calling UI functions off the main thread.
    struct omt_entry {
        tripoint_abs_omt omt_addr;
        bool     delete_after;
    };
    std::vector<omt_entry> omts_to_process;
    {
        std::set<tripoint_abs_omt> seen_omts;
        int num_processed = 0;
        std::unique_ptr<static_popup> popup;
        if( show_progress ) {
            popup = std::make_unique<static_popup>();
        }
        static constexpr std::chrono::milliseconds update_interval( 500 );
        auto last_update = std::chrono::steady_clock::now();

        for( auto &[pos, sm_ptr] : submaps ) {
            if( show_progress ) {
                const auto now = std::chrono::steady_clock::now();
                if( last_update + update_interval < now ) {
                    popup->message( _( "Please wait as the map saves [%d/%d]" ),
                                    num_processed, num_total_submaps );
                    ui_manager::redraw();
                    refresh_display();
                    inp_mngr.pump_events();
                    last_update = now;
                }
            }
            ++num_processed;

            const auto omt_addr = project_to<coords::omt>( pos );
            if( !seen_omts.insert( omt_addr ).second ) {
                continue;
            }

            const bool omt_delete = delete_after_save;

            omts_to_process.push_back( { omt_addr, omt_delete } );
        }
    }

    // Write non-uniform omts in parallel. Each write targets a distinct file/key,
    // so there are no shared-state concerns between concurrent save_omt() calls.
    // save_omt() uses submaps.find() for read-only access (safe for concurrent reads).
    // Per-task local_delete lists are merged into the shared list under a mutex.
    std::list<tripoint_abs_sm> submaps_to_delete;
    std::mutex delete_mutex;

    parallel_for( 0, static_cast<int>( omts_to_process.size() ), [&]( int i ) {
        std::list<tripoint_abs_sm> local_delete;
        save_omt( omts_to_process[i].omt_addr, local_delete, omts_to_process[i].delete_after );
        if( !local_delete.empty() ) {
            std::lock_guard<std::mutex> lk( delete_mutex );
            submaps_to_delete.splice( submaps_to_delete.end(), local_delete );
        }
    } );

    // Evict submaps from memory. std::unordered_map mutation is not thread-safe,
    // so this is done serially after the parallel write phase completes.
    for( const auto &pos : submaps_to_delete ) {
        remove_submap( pos );
    }

    // Notify the distribution grid tracker for each evicted submap.
    if( notify_tracker ) {
        auto &tracker = get_distribution_grid_tracker();
        for( const auto &pos : submaps_to_delete ) {
            tracker.on_submap_unloaded( tripoint_abs_sm( pos ), dimension_id() );
        }
    }

    // Flush the pending-writes cache to disk.  These are omts that were
    // serialised in memory by unload_omt() but not yet written.
    // Omts still resident in submaps were already handled by save_omt() above;
    // only evicted omts need to be written here.
    //
    // Snapshot under the lock so disk I/O is not performed while holding it.
    std::map<tripoint_abs_omt, std::string> pending_snapshot;
    {
        std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
        pending_snapshot = std::move( pending_writes_ );
    }
    std::ranges::for_each( pending_snapshot, [&]( auto & entry ) {
        const auto &[omt_addr, data] = entry;
        const auto base = project_to<coords::sm>( omt_addr );
        const bool in_memory =
            submaps.contains( base ) ||
            submaps.contains( base + point_east ) ||
            submaps.contains( base + point_south ) ||
            submaps.contains( base + point_south_east );
        if( !in_memory ) {
            g->get_active_world()->write_map_omt( dimension_id_.str(), omt_addr,
            [&data]( std::ostream & fout ) {
                fout << data;
            } );
        }
    } );
}

void mapbuffer::save_omt( const tripoint_abs_omt &omt_addr,
                          std::list<tripoint_abs_sm> &submaps_to_delete,
                          bool delete_after_save )
{
    ZoneScoped;
    // Build the 4 submap addresses that form this OMT omt.
    std::vector<tripoint_abs_sm> submap_addrs;
    submap_addrs.reserve( 4 );
    for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
        auto submap_addr = project_to<coords::sm>( omt_addr );
        submap_addr += off;
        submap_addrs.push_back( submap_addr );
    }

    // Use find() throughout (not operator[]) so this function is safe to call
    // from multiple threads concurrently for distinct omt_addr values.
    // operator[] would insert a default entry for missing keys, mutating the map.
    bool all_uniform = true;
    for( const tripoint_abs_sm &submap_addr : submap_addrs ) {
        const auto it = submaps.find( submap_addr );
        if( it != submaps.end() && it->second && !it->second->is_uniform ) {
            all_uniform = false;
            break;
        }
    }

    if( all_uniform ) {
        // Nothing to save — this omt will be regenerated faster than it would be re-read.
        if( delete_after_save ) {
            for( const tripoint_abs_sm &submap_addr : submap_addrs ) {
                const auto it = submaps.find( submap_addr );
                if( it != submaps.end() && it->second ) {
                    submaps_to_delete.push_back( submap_addr );
                }
            }
        }
        return;
    }

    if( disable_mapgen ) {
        return;
    }

    g->get_active_world()->write_map_omt( dimension_id_.str(), omt_addr, [&]( std::ostream & fout ) {
        JsonOut jsout( fout );
        jsout.start_array();
        for( const tripoint_abs_sm &submap_addr : submap_addrs ) {
            const auto it = submaps.find( submap_addr );
            if( it == submaps.end() ) {
                continue;
            }

            submap *sm = it->second.get();
            if( sm == nullptr ) {
                continue;
            }

            jsout.start_object();

            jsout.member( "version", savegame_version );
            jsout.member( "coordinates" );

            jsout.start_array();
            jsout.write( submap_addr.x() );
            jsout.write( submap_addr.y() );
            jsout.write( submap_addr.z() );
            jsout.end_array();

            sm->store( jsout );

            jsout.end_object();

            if( delete_after_save ) {
                submaps_to_delete.push_back( submap_addr );
            }
        }

        jsout.end_array();
    } );
}

void mapbuffer::deserialize_into_vec(
    JsonIn &jsin,
    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> &out,
    const std::function<bool( const tripoint_abs_sm & )> &skip_if )
{
    jsin.start_array();
    while( !jsin.end_array() ) {
        std::unique_ptr<submap> sm;
        tripoint_abs_sm submap_coordinates;
        jsin.start_object();
        auto version = 0;
        auto skip = false;
        while( !jsin.end_object() ) {
            auto submap_member_name = jsin.get_member_name();
            if( submap_member_name == "version" ) {
                version = jsin.get_int();
            } else if( submap_member_name == "coordinates" ) {
                jsin.start_array();
                auto i = jsin.get_int();
                auto j = jsin.get_int();
                auto k = jsin.get_int();
                tripoint_abs_sm loc{ i, j, k };
                jsin.end_array();
                submap_coordinates = loc;
                if( skip_if && skip_if( loc ) ) {
                    skip = true;
                } else {
                    sm = std::make_unique<submap>( submap_coordinates, get_dimension_id() );
                }
            } else if( skip ) {
                jsin.skip_value();
            } else {
                if( !sm ) { //This whole thing is a nasty hack that relys on coordinates coming first...
                    debugmsg( "coordinates was not at the top of submap json" );
                }
                sm->load( jsin, submap_member_name, version, project_to<coords::ms>( submap_coordinates ),
                          get_dimension_id() );
            }
        }
        if( !skip ) {
            out.emplace_back( submap_coordinates, std::move( sm ) );
        }
    }
}

bool mapbuffer::preload_omt( const tripoint_abs_omt &omt_addr )
{
    ZoneScoped;
    // Disk I/O and JSON parsing — runs outside submaps_mutex_ so
    // different omts can be prefetched concurrently on worker threads.
    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> loaded;
    // Skip submaps already resident in memory during deserialization.
    // This avoids the expensive sm->load() (items, vehicles, terrain construction)
    // for submaps that were already loaded by a prior lazy-border or sync pass.
    auto already_loaded = [this]( const tripoint_abs_sm & p ) {
        return lookup_submap_in_memory( p ) != nullptr;
    };

    // Check the in-memory write-back cache before going to disk.  A omt that
    // was presaved but not yet explicitly saved lives here instead of on disk.
    std::string pending_data;
    bool from_cache = false;
    {
        std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
        const auto it = pending_writes_.find( omt_addr );
        if( it != pending_writes_.end() ) {
            pending_data = std::move( it->second );
            pending_writes_.erase( it );
            from_cache = true;
        }
    }

    if( !pending_data.empty() ) {
        std::istringstream iss( pending_data );
        JsonIn jsin( iss );
        deserialize_into_vec( jsin, loaded, already_loaded );
    } else {
        g->get_active_world()->read_map_omt( dimension_id_.str(), omt_addr,
        [this, &loaded, &already_loaded]( JsonIn & jsin ) {
            deserialize_into_vec( jsin, loaded, already_loaded );
        } );
    }

    // Add parsed submaps to the in-memory buffer under submaps_mutex_.
    // add_submap() handles concurrent duplicate-add gracefully (keeps in-memory version).
    for( auto &[pos, sm] : loaded ) {
        if( !add_submap( pos, sm ) ) {
            DebugLog( DL::Warn, DC::Map ) << string_format(
                                              "preload_omt: submap %d,%d,%d already loaded; keeping in-memory version",
                                              pos.x(), pos.y(), pos.z() );
            // Do NOT let sm destruct here on the worker thread.  Submap/item destruction
            // touches safe_reference<T>::records_by_pointer, which remains main-thread-only.
            // Defer to drain_pending_submap_destroy(), called on the main thread after join.
            if( sm ) {
                auto lk = std::lock_guard( pending_destroy_mutex_ );
                pending_destroy_submaps_.push_back( std::move( sm ) );
            }
        }
    }
    return from_cache;
}

auto mapbuffer::generate_omt( const tripoint_abs_omt &omt_addr,
                              const mapbuffer_generate_omt_options &options ) -> mapgen_result
{
    ZoneScopedN( "mapbuffer_generate_omt" );
    const auto base = project_to<coords::sm>( omt_addr );
    const auto all_loaded =
        lookup_submap_in_memory( base )
        && lookup_submap_in_memory( base + point_east )
        && lookup_submap_in_memory( base + point_south )
        && lookup_submap_in_memory( base + point_south_east );
    if( all_loaded ) {
        return {};
    }

    if( const auto uniform_terrain = uniform_terrain_for_omt( dimension_id_, omt_addr ) ) {
        ZoneScopedN( "mapbuffer_generate_uniform_omt" );
        const auto generated = add_uniform_omt( *this, base, *uniform_terrain );
        if( generated ) {
            run_omt_pillar_post_pass_if_complete( omt_addr.xy() );
        }
        return {
            .status = generated ? mapgen_result_status::generated : mapgen_result_status::not_generated,
            .selected_mapgen = nullptr,
        };
    }

    {
        ZoneScopedN( "mapbuffer_generate_mapgen_constructor" );
        auto constructor = mapgen_constructor( *this );
        const auto generate_result = constructor.generate( omt_addr, calendar::turn, {
            .defer_postprocess_hooks = options.defer_postprocess_hooks,
            .worker_safe = options.worker_safe,
            .use_selected_mapgen = options.use_selected_mapgen,
            .selected_mapgen = options.selected_mapgen,
        } );
        if( generate_result.needs_main_thread() ) {
            return generate_result;
        }
        if( !generate_result.is_generated() ) {
            return generate_result;
        }
    }
    run_omt_pillar_post_pass_if_complete( omt_addr.xy() );
    return { .status = mapgen_result_status::generated, .selected_mapgen = nullptr };
}

auto mapbuffer::run_omt_pillar_post_pass_if_complete( const point_abs_omt &omt_pos ) -> bool
{
    ZoneScopedN( "mapbuffer_omt_pillar_post_pass_if_complete" );
    const auto offsets = std::to_array<point_rel_sm>( {
        point_rel_sm::zero(),
        point_rel_sm::east(),
        point_rel_sm::south(),
        point_rel_sm::south_east(),
    } );

    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );
    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        const auto base = project_to<coords::sm>( tripoint_abs_omt( omt_pos, zlev ) );
        const auto missing = std::ranges::any_of( offsets, [&]( const point_rel_sm & offset ) {
            return lookup_submap_in_memory( base + offset ) == nullptr;
        } );
        if( missing ) {
            return false;
        }
    }

    run_omt_pillar_post_pass( omt_pos );
    return true;
}

auto mapbuffer::run_omt_pillar_post_pass( const point_abs_omt &omt_pos ) -> void
{
    ZoneScopedN( "mapbuffer_omt_pillar_post_pass" );
    const auto offsets = std::to_array<point_rel_sm>( {
        point_rel_sm::zero(),
        point_rel_sm::east(),
        point_rel_sm::south(),
        point_rel_sm::south_east(),
    } );

    auto lk = std::lock_guard<std::recursive_mutex>( submaps_mutex_ );
    struct vertical_transition_link_request {
        tripoint_abs_sm target_pos;
        point_sm_ms local;
        ter_id desired;
    };
    const auto ensure_vertical_transition_link = [&]( const vertical_transition_link_request &
    request ) {
        auto *const target_sm = lookup_submap_in_memory( request.target_pos );
        if( target_sm == nullptr ||
            !can_replace_with_vertical_transition( *target_sm, request.local, request.desired ) ) {
            return;
        }
        target_sm->set_ter( request.local, request.desired );
        mark_post_pass_changed( *this, *target_sm );
    };

    for( const auto zlev : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        const auto omt_addr = tripoint_abs_omt( omt_pos, zlev );
        const auto base = project_to<coords::sm>( omt_addr );
        for( const auto &offset : offsets ) {
            const auto sm_pos = base + offset;
            auto *const sub_here = lookup_submap_in_memory( sm_pos );
            if( sub_here == nullptr ) {
                continue;
            }

            auto *const sub_below = zlev > -OVERMAP_DEPTH ?
                                    lookup_submap_in_memory( sm_pos + tripoint_below ) : nullptr;

            auto changed = false;
            for( const auto local : ::submap_tiles() ) {
                const auto terrain_here = sub_here->get_ter( local );
                /* DO NOT UNCOMMENT THIS UNTIL IT IS MADE TOGGLEABLE BY OMT OR TILE
                   BADLY EFFECTED MAPGENS INCLUDE: REF CENTER, LMOE SHELTER, and multiple modded OMTs
                if( const auto target = vertical_transition_target_below( terrain_here );
                    target && zlev > -OVERMAP_DEPTH ) {
                    ensure_vertical_transition_link( {
                        .target_pos = sm_pos + tripoint_below,
                        .local = local,
                        .desired = *target,
                    } );
                }
                if( const auto target = vertical_transition_target_above( terrain_here );
                    target && zlev < OVERMAP_HEIGHT ) {
                    ensure_vertical_transition_link( {
                        .target_pos = sm_pos + tripoint_above,
                        .local = local,
                        .desired = *target,
                    } );
                }
                */
                if( terrain_here != t_open_air ) {
                    continue;
                }
                if( zlev <= -OVERMAP_DEPTH ) {
                    sub_here->set_ter( local, t_rock_floor );
                    changed = true;
                    continue;
                }
                if( sub_below == nullptr ) {
                    continue;
                }

                const auto &ter_below = sub_below->get_ter( local ).obj();
                if( ter_below.roof ) {
                    sub_here->set_ter( local, ter_below.roof.id() );
                    changed = true;
                }
            }

            if( changed ) {
                mark_post_pass_changed( *this, *sub_here );
            }
        }
    }
}

auto mapbuffer::actualize_submap( const tripoint_abs_sm &pos ) -> void
{
    ZoneScopedN( "mapbuffer_actualize_submap" );

    auto *const tmpsub = lookup_submap_in_memory( pos );
    if( tmpsub == nullptr ) {
        debugmsg( "actualize_submap called on null submap %s", pos.to_string() );
        return;
    }

    if( tmpsub->last_touched == calendar::turn ) {
        ZoneScopedN( "mapbuffer_actualize_skip_current_turn" );
        return;
    }

    const auto last_touched = tmpsub->last_touched;
    const auto elapsed = calendar::turn - last_touched;

    if( last_touched < calendar::turn ) {
        ZoneScopedN( "mapbuffer_actualize_batch_turns" );
        const auto missed = to_turns<int>( elapsed );
        ::run_submap_batch_turns( *tmpsub, missed );
    }

    // Uniform submaps (empty rock, open air, boundary fill) have no items,
    // furniture, fields, or plants. Avoid the tile loop and just stamp time.
    if( tmpsub->is_uniform ) {
        tmpsub->last_touched = calendar::turn;
        return;
    }

    const auto do_funnels = pos.z() >= 0;
    const auto lookup_options = mapbuffer_lookup_options {
        .mode = mapbuffer_lookup_mode::resident_only,
    };

    for( const auto p : ::submap_tiles() ) {
        const auto abs_pos = project_combine( pos, p );
        const auto options = actualize_tile_options {
            .buffer = *this,
            .sm = *tmpsub,
            .local = p,
            .abs_pos = abs_pos,
            .active_bubble_pos = active_reality_bubble_local( abs_pos ),
            .last_touched = last_touched,
            .elapsed = elapsed,
            .lookup = lookup_options,
        };
        auto &items = tmpsub->get_items( p );
        if( !items.empty() ) {
            const auto &furn = tmpsub->get_furn( p ).obj();
            if( !furn.has_flag( "DONT_REMOVE_ROTTEN" ) ) {
                remove_rotten_items( options, items );
            }
        }

        if( do_funnels ) {
            fill_funnels( options );
        }

        grow_plant( options );
        restock_fruits( options );
        produce_sap( options );
        rad_scorch( options );
        decay_cosmetic_fields( options );
    }

    tmpsub->last_touched = calendar::turn;
}

auto mapbuffer::drain_pending_submap_destroy() -> void
{
    auto to_destroy = std::vector<std::unique_ptr<submap>> {};
    {
        auto lk = std::lock_guard( pending_destroy_mutex_ );
        to_destroy = std::move( pending_destroy_submaps_ );
    }
    // unique_ptrs destruct here, on the main thread.
}
