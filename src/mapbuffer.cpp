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
#include "detached_ptr.h"
#include "distribution_grid.h"
#include "filesystem.h"
#include "field_type.h"
#include "fluid_grid.h"
#include "flag.h"
#include "fstream_utils.h"
#include "game.h"
#include "game_constants.h"
#include "harvest.h"
#include "iexamine.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "map.h"
#include "mapdata.h"
#include "mapgen_constructor.h"
#include "map_iterator.h"
#include "map_mutation_hooks.h"
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
#include "skill.h"
#include "string_formatter.h"
#include "submap.h"
#include "submap_load_manager.h"
#include "thread_pool.h"
#include "translations.h"
#include "trap.h"
#include "ui_manager.h"
#include "units_mass.h"
#include "veh_type.h"
#include "vehicle.h"
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
        added_any |= dest.add_submap( pos, sm );
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

    auto is_outside = options.sm.outside_cache[options.local.x()][options.local.y()];
    if( options.active_bubble_pos && g != nullptr ) {
        is_outside = g->m.is_outside( *options.active_bubble_pos );
    }
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

auto mark_post_pass_changed( submap &sm ) -> void
{
    sm.transparency_dirty = true;
    sm.floor_dirty = true;
    sm.outside_dirty = true;
    sm.absorption_dirty = true;
    sm.pf_dirty = true;
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
    return sm_->get_ter( local_ );
}

auto abs_tile_handle::furn() const -> furn_id
{
    return sm_->get_furn( local_ );
}

::trap_id abs_tile_handle::trap_id() const
{
    return sm_->get_trap( local_ );
}

auto abs_tile_handle::ter_obj() const -> const ter_t &
{
    return ter().obj();
}

auto abs_tile_handle::furn_obj() const -> const furn_t &
{
    return furn().obj();
}

auto abs_tile_handle::trap_obj() const -> const trap &
{
    return trap_id().obj();
}

auto abs_tile_handle::field() const -> const class field &
{
        return sm_->get_field( local_ );
}

auto abs_tile_handle::items() const -> const location_vector<item> &
{
    return sm_->get_items( local_ );
}

auto abs_tile_handle::furn_vars() const -> const data_vars::data_set &
{
    return sm_->get_furn_vars( local_ );
}

auto abs_tile_handle::radiation() const -> int
{
    return sm_->get_radiation( local_ );
}

auto abs_tile_handle::lum() const -> std::uint8_t
{
    return sm_->get_lum( local_ );
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
    return sm_->field_count > 0;
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
    return sm_->has_graffiti( local_ );
}

auto abs_tile_handle::graffiti_at() const -> const std::string &
{
    return sm_->get_graffiti( local_ );
}

auto abs_tile_handle::has_signage() const -> bool
{
    return sm_->has_signage( local_ );
}

auto abs_tile_handle::get_signage() const -> std::string
{
    return sm_->get_signage( local_ );
}

auto abs_tile_handle::has_computer() const -> bool
{
    return sm_->has_computer( local_ );
}

auto abs_tile_handle::get_computer() const -> const computer *
{
    return sm_->get_computer( local_ );
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
    return sm_->get_ter_vars( local_ );
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
- > std::optional<abs_tile_handle>
{
    const auto split = project_remain<coords::sm>( p );
    auto *const sm = buf.lookup_submap_in_memory( split.quotient_tripoint );
    if( sm == nullptr ) {
        return std::nullopt;
    }
    return abs_tile_handle( *sm, split.quotient_tripoint, split.remainder,
                            buf.vehicle_part_at_loaded_tile( p ) );
}

auto abs_tile_handle::fetch_terrain_only( mapbuffer &buf, const tripoint_abs_ms p )
- > std::optional<abs_tile_handle>
{
    const auto split = project_remain<coords::sm>( p );
    auto *const sm = buf.lookup_submap_in_memory( split.quotient_tripoint );
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
- > std::optional<submap_tile_range>
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
- > std::vector<point_abs_sm>
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
- > std::vector<simulated_island>
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
- > submap_tile_iterator_range
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
- > submap_tile_iterator_range
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
- > submap_tile_iterator_range
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
        handle_ = submap_loader.request_load( source_,
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
    }
}

auto mapbuffer::unregister_submap_vehicles( const tripoint_abs_sm &p ) -> void
{
    for( auto iter = loaded_vehicles_.begin(); iter != loaded_vehicles_.end(); ) {
        const auto *const veh = *iter;
        if( veh == nullptr || veh->abs_sm_pos == p ) {
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

    // For newly-simulated columns: refresh vehicle registry and active item
    // index for every z-level so they are immediately queryable.
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
            }
        }
    }

    // For newly-demoted columns: stamp last_touched so actualize() computes
    // the correct time-since-simulated on the next load.
    for( const point_abs_sm &col : last_demoted_columns_ ) {
        for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            const tripoint_abs_sm sm_pos{ col, z };
            const auto it = submaps.find( sm_pos );
            if( it != submaps.end() && it->second ) {
                it->second->last_touched = calendar::turn;
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
        sm->outside_dirty = sm->outside_dirty || options.outside;
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
    mark_post_pass_changed( *tile->sm );
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
    mark_post_pass_changed( *tile->sm );
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
    if( const auto local = active_reality_bubble_local( p ) ) {
        return g->m.veh_at( *local );
    }

    const auto target_sm = project_to<coords::sm>( p );
    if( get_submap( target_sm, options ) == nullptr ) {
        return optional_vpart_position( std::nullopt );
    }

    return vehicle_part_at_loaded_tile( p );
}

auto mapbuffer::vehicle_part_at_loaded_tile( const tripoint_abs_ms &p ) -> optional_vpart_position
{
    if( const auto local = active_reality_bubble_local( p ) ) {
        return g->m.veh_at( *local );
    }

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
        const auto target_tile = abs_tile_handle::fetch( *this, to );
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

    const auto up_tile = abs_tile_handle::fetch_terrain_only( *this, up_p );
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

    const auto down_tile = abs_tile_handle::fetch_terrain_only( *this, down_p );
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

    const auto up_vehicle = vehicle_part_at_loaded_tile( up_p );
    if( up_vehicle && !up_vehicle.part_with_feature( VPFLAG_NOCOLLIDEBELOW, false ) ) {
        return false;
    }

    const auto down_vehicle = vehicle_part_at_loaded_tile( down_p );
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

    const auto center_tile = abs_tile_handle::fetch_terrain_only( *this, p );
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

    for( const auto &pt : points_in_radius( p, 1 ) ) {
        const auto tile = abs_tile_handle::fetch( *this, pt );
        if( !tile || !tile->passable() ) {
            best_difficulty = std::min( best_difficulty, 10 );
            blocks_movement++;
        } else if( tile->vehicle_part() ) {
            best_difficulty = std::min( best_difficulty, 7 );
        }

        if( best_difficulty > 5 && tile && has_flag( *tile, "CLIMBABLE" ) ) {
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
        [this, &is_clear, cost_min, cost_max, &t, &last_point]( const point & new_point ) {
            // Exit before checking the last square, it's still reachable even if it is an obstacle.
            if( new_point == t.xy().raw() ) {
                return false;
            }

            const tripoint_abs_ms p( point_abs_ms( new_point ), t.z() );
            const tripoint_abs_ms lp( point_abs_ms( last_point ), t.z() );
            const int cost = move_cost( p );
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
    [this, &is_clear, cost_min, cost_max, t, &last_point]( const tripoint & new_point ) {
        // Exit before checking the last square, it's still reachable even if it is an obstacle.
        if( new_point == t.raw() ) {
            return false;
        }

        const tripoint_abs_ms pt( new_point );
        // We have to check a weird case where the move is both vertical and horizontal
        if( new_point.z == last_point.z() ) {
            const int cost = move_cost( pt );
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
                const int cost = move_cost( from_prev_z );
                if( cost > cost_min && cost < cost_max &&
                    !obstructed_by_vehicle_rotation( last_point, pt ) ) {
                    this_clear = true;
                }
            }

            if( !this_clear ) {
                const tripoint_abs_ms floor_check( last_point.xy(), max_z );
                if( has_floor_or_support( floor_check ) ) {
                    const tripoint_abs_ms from_new_z( last_point.xy(), new_point.z );
                    const int cost = move_cost( from_new_z );
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
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_lum( tile->local );
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
                          const mapbuffer_lookup_options options ) -> std::optional<bool>
{
    const auto tile = abs_tile_handle::fetch( *this, p );
    if( !tile ) {
        return std::nullopt;
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

    auto valid_tile = [&]( const tripoint_abs_ms & target ) -> std::optional<mapbuffer_tile_lookup> {
        auto tile = lookup_tile( *this, target, options.lookup );
        if( !tile )
        {
            return std::nullopt;
        }
        if( tile_has_flag( *tile, "DESTROY_ITEM" ) )
        {
            return std::nullopt;
        }
        if( new_item->made_of( LIQUID ) && tile_has_flag( *tile, "SWIMMABLE" ) )
        {
            return std::nullopt;
        }
        return tile;
    };

    auto valid_limits = [&]( const mapbuffer_tile_lookup & tile ) {
        const auto max_volume = tile.sm->get_furn( tile.local ) != f_null ?
                                tile.sm->get_furn( tile.local ).obj().max_volume :
                                tile.sm->get_ter( tile.local ).obj().max_volume;
        auto stored_volume = 0_ml;
        for( const auto *const existing : tile.sm->get_items( tile.local ) ) {
            stored_volume += existing->volume();
        }
        return new_item->volume() <= max_volume - stored_volume &&
               tile.sm->get_items( tile.local ).size() < MAX_ITEM_IN_SQUARE;
    };

    auto call_active_drop_hook = [&]( const tripoint_abs_ms & target ) {
        const auto local = active_reality_bubble_local( target );
        if( !local ) {
            if( new_item->made_of( LIQUID ) && !new_item->has_own_flag( flag_DIRTY ) ) {
                new_item->set_flag( flag_DIRTY );
            }
            return false;
        }
        if( new_item->made_of( LIQUID ) || !new_item->has_flag( flag_DROP_ACTION_ONLY_IF_LIQUID ) ) {
            return new_item->on_drop( *local, g->m );
        }
        return false;
    };

    auto route_allows_overflow = [&]( const tripoint_abs_ms & target ) {
        const auto source_local = active_reality_bubble_local( p );
        const auto target_local = active_reality_bubble_local( target );
        if( !source_local || !target_local ) {
            return false;
        }
        PathfindingSettings pf_settings;
        pf_settings.bash_strength_val = 0;
        RouteSettings rt_settings;
        rt_settings.max_dist = 2;
        rt_settings.max_s_coeff = 4.0f;
        auto &pf_buffer = MAPBUFFER_REGISTRY.get( g->m.get_bound_dimension() );
        auto abs_route = Pathfinding::route( pf_buffer,
                                             bub_to_abs( *source_local ),
                                             bub_to_abs( *target_local ),
                                             pf_settings, rt_settings );
        return !abs_route.empty();
    };

    auto place_item = [&]( const tripoint_abs_ms & target, mapbuffer_tile_lookup & tile ) {
        auto &items = tile.sm->get_items( tile.local );
        if( new_item->count_by_charges() ) {
            for( auto &existing : items ) {
                if( existing->merge_charges( std::move( new_item ) ) ) {
                    return;
                }
            }
        }

        if( const auto local = active_reality_bubble_local( target ) ) {
            g->m.support_dirty( *local );
        }
        new_item = add_item( target, std::move( new_item ), options.lookup );
    };

    auto try_place = [&]( const tripoint_abs_ms & target, const bool reject_noitem,
    const bool call_drop_hook_first ) {
        auto tile = valid_tile( target );
        if( !tile ) {
            return false;
        }
        if( reject_noitem && ( tile_has_flag( *tile, "NOITEM" ) || tile_has_flag( *tile, "SEALED" ) ) ) {
            return false;
        }
        if( call_drop_hook_first && call_active_drop_hook( target ) ) {
            return true;
        }
        if( ( !tile_has_flag( *tile, "NOITEM" ) ||
              tile_allows_item_despite_noitem_flag( *new_item, *tile ) ) &&
            valid_limits( *tile ) ) {
            if( !call_drop_hook_first && call_active_drop_hook( target ) ) {
                return true;
            }
            place_item( target, *tile );
            return true;
        }
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

    if( !map_mutation_hooks::prepare_item_for_placement( {
    .dim_id = dimension_id_,
    .p = p,
    .item_to_place = new_item,
} ) ) {
        return std::move( new_item );
    }

    if( new_item->is_map() && !new_item->has_var( "reveal_map_center_omt" ) ) {
        new_item->set_var( "reveal_map_center_omt", project_to<coords::omt>( p ) );
    }

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

    tile->sm->get_items( tile->local ).push_back( std::move( new_item ) );
    return detached_ptr<item>();
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
            auto h = abs_tile_handle::fetch_terrain_only( *this, pt );
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
            auto h = abs_tile_handle::fetch_terrain_only( *this, pt );
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
            auto h = abs_tile_handle::fetch_terrain_only( *this, pt );
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
    auto h = abs_tile_handle::fetch_terrain_only( *this, p );
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
    auto h = abs_tile_handle::fetch_terrain_only( *this, p );
    return h && ( !h->has_flag( "SEALED" ) || h->has_flag( "LIQUIDCONT" ) );
}

auto mapbuffer::is_wall_adjacent( const tripoint_abs_ms &p,
                                  const mapbuffer_lookup_options options ) -> bool
{
    for( const tripoint_abs_ms &pt : points_in_radius( p, 1 ) ) {
        if( pt != p ) {
            auto h = abs_tile_handle::fetch( *this, pt );
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
        auto h = abs_tile_handle::fetch_terrain_only( *this, p );
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
                      mapbuffer_lookup_options options ) -> const bool
{
    int dummy = 0;
    return sees( F, T, range, dummy );
}

/**
 * This one is internal-only, we don't want to expose the slope tweaking ickiness outside the map class.
 **/
auto mapbuffer::sees( const tripoint_abs_ms &F, const tripoint_abs_ms &T, const int range,
                      int &bresenham_slope, mapbuffer_lookup_options options ) -> const bool
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
        [this, &visible, &T, &last_point]( const point & new_point ) {
            // Exit before checking the last square, it's still visible even if opaque.
            if( new_point.x == T.x() && new_point.y == T.y() ) {
                return false;
            }
            const auto new_tripoint = tripoint_abs_ms( point_abs_ms( new_point ), T.z() );
            if( !is_transparent( new_tripoint ) ||
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
    [this, &visible, &T, &last_point]( const tripoint & new_point ) {
        // Exit before checking the last square if it's not a vertical transition,
        // it's still visible even if opaque.
        if( new_point == T.raw() && last_point.z() == T.z() ) {
            return false;
        }

        // TODO: Allow transparent floors (and cache them!)
        if( new_point.z == last_point.z() ) {
            if( !is_transparent( tripoint_abs_ms( new_point ) ) ||
                obstructed_by_vehicle_rotation( last_point, tripoint_abs_ms( new_point ) ) ) {
                visible = false;
                return false;
            }
        } else {
            const int max_z = std::max( new_point.z, last_point.z() );
            if( ( has_floor( tripoint_abs_ms{ new_point.x, new_point.y, max_z }, true ) ||
                  !is_transparent( tripoint_abs_ms{ new_point.x, new_point.y, last_point.z() } ) ) &&
                ( has_floor( {last_point.xy(), max_z}, true ) ||
                  !is_transparent( {last_point.xy(), new_point.z} ) ) ) {
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
                                   const mapbuffer_lookup_options options ) -> const int
{
    const auto tile1 = lookup_tile( *this, loc1, options );
    const auto tile2 = lookup_tile( *this, loc2, options );
    if( !tile1 || !tile2 ) {
        return 100;
    }
    // Can't hide if you are standing on furniture, or non-flat slowing-down terrain tile.
    if( tile1->sm->get_furn( tile1->local ).obj().id || ( move_cost( loc2 ) > 2 &&
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
    if( const auto vp = veh_at( obstaclepos ) ) {
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
    auto h = abs_tile_handle::fetch( *this, p );
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
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return std::nullopt;
    }
    return tile->ter();
}

auto mapbuffer::furn( const tripoint_abs_ms &p,
                      const mapbuffer_lookup_options options ) -> std::optional<furn_id>
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return std::nullopt;
    }
    return tile->furn();
}

auto mapbuffer::furnname( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return std::string();
    }
    return tile->furnname();
}

auto mapbuffer::has_flag( const std::string &flag, const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }
    return tile->has_flag( flag );
}

auto mapbuffer::has_flag( ter_bitflags flag, const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }
    return tile->has_flag( flag );
}

auto mapbuffer::has_flag_ter( const std::string &flag, const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }
    return tile->has_flag_ter( flag );
}

auto mapbuffer::has_flag_ter_or_furn( const std::string &flag, const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }
    return tile->has_flag_ter_or_furn( flag );
}

auto mapbuffer::has_flag_ter_or_furn( ter_bitflags flag, const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }
    return tile->has_flag_ter_or_furn( flag );
}

auto mapbuffer::has_floor_or_support( const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }
    // Check terrain/furniture has_floor property + submap floor cache fallback
    // has_floor is a property derived from the terrain type and floor cache
    return !tile->ter_obj().has_flag( TFLAG_NO_FLOOR );
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
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
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
    const auto split = project_remain<coords::sm>( p );
    return sm->outside_cache[split.remainder.x()][split.remainder.y()];
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
    if( !valid_move( from, to, { .flying = flying, .via_ramp = via_ramp } ) ) {
        return 0;
    }
    return ( cost1 + cost2 + modifier ) * mults[match] / 2;
}

auto mapbuffer::move_cost( const tripoint_abs_ms &p, const vehicle *ignored_vehicle,
                           const mapbuffer_lookup_options options ) -> int
{
    const auto tile = abs_tile_handle::fetch( *this, p );
    if( !tile ) {
        return 0;
    }
    return tile->move_cost( ignored_vehicle );
}

auto mapbuffer::obstructed_by_vehicle_rotation( const tripoint_abs_ms &from,
        const tripoint_abs_ms &to,
        const mapbuffer_lookup_options options ) -> bool
{
    // Only meaningful in the player bubble where vehicles actually move each turn
    if( const auto local_from = active_reality_bubble_local( from ) ) {
        if( const auto local_to = active_reality_bubble_local( to ) ) {
            return g->m.obstructed_by_vehicle_rotation( *local_from, *local_to );
        }
    }
    return false;
}

auto mapbuffer::hit_with_acid( const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = abs_tile_handle::fetch( *this, p );
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
    const auto tile = abs_tile_handle::fetch( *this, p );
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
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
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
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return false;
    }

    // Try terrain door
    const auto &ter = tile->ter_obj();
    if( ter.open ) {
        if( has_flag( str_OPENCLOSE_INSIDE, p, options ) && !inside ) {
            return false;
        }
        return set_ter( p, ter.open, options );
    }

    // Try furniture door
    const auto &furn = tile->furn_obj();
    if( furn.open ) {
        if( has_flag( str_OPENCLOSE_INSIDE, p, options ) && !inside ) {
            return false;
        }
        return set_furn( p, furn.open, options );
    }

    // Try vehicle door
    return false;
}

auto mapbuffer::bash( const tripoint_abs_ms &p, int str, bool silent,
                      const mapbuffer_lookup_options options ) -> int
{
    // Dimension bounds cannot be bashed
    if( is_outside_pocket_dimension_bounds( p ) ) {
        return 0;
    }

    // Bash field (mostly fire/acid-based field removal)
    const auto field_p = get_field( p, options );
    if( field_p ) {
        for( auto &field_entry_it : *field_p ) {
            field_entry &cur = field_entry_it.second;
            if( cur.is_field_alive() ) {
                // Reduce field intensity based on bash strength
                const int reduction = str / 10;
                if( reduction > 0 ) {
                    cur.set_field_intensity( std::max( 0, cur.get_field_intensity() - reduction ) );
                }
            }
        }
    }

    // Bash items
    auto *items = get_items( p, options );
    if( items ) {
        for( auto it = items->begin(); it != items->end(); ) {
            if( ( *it )->can_shatter() && one_in( 2 ) ) {
                it = items->erase( it );
            } else {
                ++it;
            }
        }
    }

    // Bash terrain/furniture
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( !tile ) {
        return 0;
    }

    if( !is_bashable_ter_furn( p, false, options ) ) {
        return 0;
    }

    const auto &ter = tile->ter_obj();
    if( is_bashable_ter( p, false, options ) && ter.bash.str_max >= 0 ) {
        if( str >= rng( ter.bash.str_min, ter.bash.str_max ) ) {
            set_ter( p, ter_id( ter.bash.ter_set ), options );
            if( !ter.bash.furn_set.is_null() ) {
                set_furn( p, furn_id( ter.bash.furn_set ), options );
            }
            return str;
        }
    }

    const auto &furn = tile->furn_obj();
    if( is_bashable_furn( p, options ) && furn.bash.str_max >= 0 ) {
        if( str >= rng( furn.bash.str_min, furn.bash.str_max ) ) {
            set_furn( p, furn_id( furn.bash.furn_set ), options );
            if( !furn.bash.ter_set.is_null() ) {
                set_ter( p, ter_id( furn.bash.ter_set ), options );
            }
            return str;
        }
    }

    return 0;
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
    auto vp = veh_at( p, options ).part_with_feature( VPFLAG_BOARDABLE, false );
    if( !vp ) {
        return;
    }
    vp->part().remove_flag( vehicle_part::passenger_flag );
    vp->vehicle().invalidate_mass();
    Character *passenger = g->critter_by_id<Character>( vp->part().passenger_id );
    if( passenger && !dead_passenger ) {
        passenger->in_vehicle = false;
        if( passenger->controlling_vehicle ) {
            vp->vehicle().skidding = true;
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
    const auto tile = abs_tile_handle::fetch_terrain_only( *this, p );
    if( tile ) {
        const auto lum = tile->lum();
        if( lum > 0 ) {
            ambient = std::max( ambient, static_cast<float>( lum ) );
        }
    }

    return ambient;
}

// ----- Field operations -----

auto mapbuffer::add_splatter( const field_type_id &type, const tripoint_abs_ms &where,
                              const int intensity,
                              const mapbuffer_lookup_options options ) -> void
{
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
        add_splatter( type, tripoint_abs_ms( p, from.z() ), 1, options );
    }
}

auto mapbuffer::add_splash( const field_type_id &type, const tripoint_abs_ms &center,
                            const int radius, const int intensity,
                            const mapbuffer_lookup_options options ) -> void
{
    for( const tripoint_abs_ms &p : points_in_radius( center, radius ) ) {
        add_splatter( type, p, intensity, options );
    }
}

auto mapbuffer::propagate_field( const tripoint_abs_ms &center, const field_type_id &type,
                                 const int amount, const int max_intensity,
                                 const mapbuffer_lookup_options options ) -> void
{
    // Propagate to all adjacent tiles
    for( const tripoint_abs_ms &pt : points_in_radius( center, 1 ) ) {
        if( pt == center ) {
            continue;
        }
        const auto existing = get_field_intensity( pt, type, options );
        const int cur_intensity = existing.value_or( 0 );

        if( cur_intensity > 0 && cur_intensity < max_intensity ) {
            set_field_intensity( pt, {
                .type = type,
                .intensity = cur_intensity + 1,
                .isoffset = false,
                .lookup = options,
            } );
        } else if( cur_intensity == 0 ) {
            add_field( pt, {
                .type = type,
                .intensity = std::min( 1, max_intensity ),
                .age = 0_turns,
                .lookup = options,
            } );
        }

        if( amount > 1 ) {
            propagate_field( pt, type, amount - 1, max_intensity, options );
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
- > std::vector<detached_ptr<item>>
{
    std::vector<detached_ptr<item>> remaining;
    for( auto &it : new_items ) {
        detached_ptr<item> leftover = add_item( p, std::move( it ), options );
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
            auto h_mc = abs_tile_handle::fetch( *this, p );
            if( h_mc && h_mc->move_cost() == 0 && is_bashable_ter( p, true, options ) ) {
                set_ter( p, floor_type, options );
            }
        }
        // Check again for new terrain after potential destruction
        {
            auto h_mc2 = abs_tile_handle::fetch( *this, p );
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

    auto &here = g->m;
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

    auto &here = g->m;
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
        mark_post_pass_changed( *target_sm );
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
                mark_post_pass_changed( *sub_here );
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
