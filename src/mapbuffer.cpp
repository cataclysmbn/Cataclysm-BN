#include "mapbuffer.h"

#include <algorithm>
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
#include <ranges>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "avatar.h"
#include "batch_turns.h"
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
static const auto itype_burnt_out_bionic = itype_id( "burnt_out_bionic" );

auto lookup_tile( mapbuffer &buffer, const tripoint_abs_ms &p,
                  const mapbuffer_lookup_options options ) -> std::optional<mapbuffer_tile_lookup>
{
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
    if( sm.get_ter( local ) == t_rootcellar ) {
        return temperature_flag::TEMP_ROOT_CELLAR;
    }
    const auto &furn = sm.get_furn( local ).obj();
    if( furn.has_flag( TFLAG_FRIDGE ) ) {
        return temperature_flag::TEMP_FRIDGE;
    }
    if( furn.has_flag( TFLAG_FREEZER ) ) {
        return temperature_flag::TEMP_FREEZER;
    }

    return temperature_flag::TEMP_NORMAL;
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
    if( options.active_bubble_pos && g != nullptr &&
        g->critter_at( *options.active_bubble_pos ) != nullptr ) {
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

    if( !options.active_bubble_pos || g == nullptr || !g->u.sees( *options.active_bubble_pos ) ) {
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

} // namespace

mapbuffer_abs_tile_view::mapbuffer_abs_tile_view( const tripoint_abs_sm &abs_sm,
        const point_sm_ms &local, const submap &sm ) :
    abs_sm_( abs_sm ),
    local_( local ),
    sm_( &sm )
{
}

mapbuffer_abs_tile_view::operator bool() const
{
    return sm_ != nullptr;
}

auto mapbuffer_abs_tile_view::abs_pos() const -> tripoint_abs_ms
{
    return project_combine( abs_sm_, local_ );
}

auto mapbuffer_abs_tile_view::abs_submap_pos() const -> tripoint_abs_sm
{
    return abs_sm_;
}

auto mapbuffer_abs_tile_view::submap_pos() const -> point_sm_ms
{
    return local_;
}

auto mapbuffer_abs_tile_view::get_ter() const -> ter_id
{
    return sm_->get_ter( local_ );
}

auto mapbuffer_abs_tile_view::get_furn() const -> furn_id
{
    return sm_->get_furn( local_ );
}

auto mapbuffer_abs_tile_view::get_trap() const -> trap_id
{
    return sm_->get_trap( local_ );
}

auto mapbuffer_abs_tile_view::get_ter_t() const -> const ter_t &
{
    return get_ter().obj();
}

auto mapbuffer_abs_tile_view::get_furn_t() const -> const furn_t &
{
    return get_furn().obj();
}

auto mapbuffer_abs_tile_view::get_trap_t() const -> const trap &
{
    return get_trap().obj();
}

auto mapbuffer_abs_tile_view::get_field() const -> const field &
{
    return sm_->get_field( local_ );
}

auto mapbuffer_abs_tile_view::get_items() const -> const location_vector<item> &
{
    return sm_->get_items( local_ );
}

auto mapbuffer_abs_tile_view::get_furn_vars() const -> const data_vars::data_set &
{
    return sm_->get_furn_vars( local_ );
}

auto mapbuffer_abs_tile_view::get_radiation() const -> int
{
    return sm_->get_radiation( local_ );
}

auto mapbuffer_abs_tile_view::get_lum() const -> std::uint8_t
{
    return sm_->get_lum( local_ );
}

auto mapbuffer_abs_tile_view::move_cost_ter_furn() const -> int
{
    const auto &terrain = get_ter_t();
    const auto &furniture = get_furn_t();
    if( terrain.movecost == 0 || ( furniture.id && furniture.movecost < 0 ) ) {
        return 0;
    }
    if( furniture.id ) {
        return std::max( terrain.movecost + furniture.movecost, 0 );
    }
    return std::max( terrain.movecost, 0 );
}

auto mapbuffer_abs_tile_view::passable_ter_furn() const -> bool
{
    return move_cost_ter_furn() != 0;
}

auto mapbuffer_abs_tile_view::move_cost_with_vehicle(
    const optional_vpart_position &vp ) const -> int
{
    return move_cost_from_tile_parts( get_ter(), get_furn(), vp );
}

auto mapbuffer_abs_tile_view::passable_with_vehicle(
    const optional_vpart_position &vp ) const -> bool
{
    return move_cost_with_vehicle( vp ) != 0;
}

mapbuffer_abs_tile_with_vehicle_view::mapbuffer_abs_tile_with_vehicle_view(
    const mapbuffer_abs_tile_view &tile,
    const optional_vpart_position &vehicle_part ) :
    tile_( tile ),
    vehicle_part_( vehicle_part )
{
}

mapbuffer_abs_tile_with_vehicle_view::operator bool() const
{
    return static_cast<bool>( tile_ );
}

auto mapbuffer_abs_tile_with_vehicle_view::tile()
const -> const mapbuffer_abs_tile_view &
{
    return tile_;
}

auto mapbuffer_abs_tile_with_vehicle_view::vehicle_part()
const -> const optional_vpart_position &
{
    return vehicle_part_;
}

auto mapbuffer_abs_tile_with_vehicle_view::move_cost() const -> int
{
    return tile_.move_cost_with_vehicle( vehicle_part_ );
}

auto mapbuffer_abs_tile_with_vehicle_view::passable() const -> bool
{
    return move_cost() != 0;
}

mapbuffer_abs_submap_view::mapbuffer_abs_submap_view( const tripoint_abs_sm &abs_sm,
        const submap &sm ) :
    abs_sm_( abs_sm ),
    sm_( &sm )
{
}

mapbuffer_abs_submap_view::operator bool() const
{
    return sm_ != nullptr;
}

auto mapbuffer_abs_submap_view::abs_pos() const -> tripoint_abs_sm
{
    return abs_sm_;
}

auto mapbuffer_abs_submap_view::get_submap() const -> const submap &
{
    return *sm_;
}

auto mapbuffer_abs_submap_view::tile( const point_sm_ms &local )
const -> mapbuffer_abs_tile_view
{
    return mapbuffer_abs_tile_view( abs_sm_, local, *sm_ );
}

auto mapbuffer_abs_submap_view::tiles() const -> point_range<point_sm_ms>
{
    return submap_tiles();
}

mapbuffer_abs_omt_view::mapbuffer_abs_omt_view( const tripoint_abs_omt &abs_omt,
        const std::array<const submap *, 4> &submaps ) :
    abs_omt_( abs_omt ),
    submaps_( submaps )
{
}

mapbuffer_abs_omt_view::operator bool() const
{
    return has_any_submap();
}

auto mapbuffer_abs_omt_view::has_any_submap() const -> bool
{
    return std::ranges::any_of( submaps_, []( const auto * sm ) {
        return sm != nullptr;
    } );
}

auto mapbuffer_abs_omt_view::abs_pos() const -> tripoint_abs_omt
{
    return abs_omt_;
}

auto mapbuffer_abs_omt_view::is_complete() const -> bool
{
    return std::ranges::all_of( submaps_, []( const auto * sm ) {
        return sm != nullptr;
    } );
}

auto mapbuffer_abs_omt_view::get_submap_view( const point_omt_sm &local )
const -> std::optional<mapbuffer_abs_submap_view>
{
    const auto index = omt_submap_index( local );
    if( !index || submaps_[*index] == nullptr ) {
        return std::nullopt;
    }
    return mapbuffer_abs_submap_view( project_combine( abs_omt_, local ), *submaps_[*index] );
}

mapbuffer_abs_tile_reader::mapbuffer_abs_tile_reader( mapbuffer &buffer,
        const mapbuffer_lookup_options options ) :
    buffer_( &buffer ),
    options_( options )
{
}

auto mapbuffer_abs_tile_reader::get_tile( const tripoint_abs_ms &p )
const -> std::optional<mapbuffer_abs_tile_view>
{
    return buffer_->get_abs_tile( p, options_ );
}

auto mapbuffer_abs_tile_reader::get_tile_with_vehicle( const tripoint_abs_ms &p )
const -> std::optional<mapbuffer_abs_tile_with_vehicle_view>
{
    return buffer_->get_abs_tile_with_vehicle( p, options_ );
}

auto mapbuffer_abs_tile_reader::get_submap_view( const tripoint_abs_sm &p )
const -> std::optional<mapbuffer_abs_submap_view>
{
    return buffer_->get_abs_submap_view( p, options_ );
}

auto mapbuffer_abs_tile_reader::get_omt_view( const tripoint_abs_omt &p )
const -> std::optional<mapbuffer_abs_omt_view>
{
    return buffer_->get_abs_omt_view( p, options_ );
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
        submaps.clear();
    }
    std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
    pending_writes_.clear();
}

bool mapbuffer::add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm )
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    if( submaps.contains( p ) ) {
        return false;
    }

    submaps[p] = std::move( sm );
    register_submap_vehicles( p, *submaps[p] );

    return true;
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
        dest.submaps.emplace( kv.first, std::move( kv.second ) );
    }
    loaded_vehicles_.clear();
    vehicle_footprint_by_location_.clear();
    vehicle_footprint_locations_.clear();
    submaps.clear();
}

submap *mapbuffer::load_submap( const tripoint_abs_sm &pos )
{
    ZoneScoped;
    // lookup_submap already handles the disk-read path transparently.
    return lookup_submap( pos );
}

void mapbuffer::unload_omt( const tripoint_abs_omt &omt_addr, bool save )
{
    // Hold the mutex for the entire save+erase so that background lazy-border
    // preload_omt() workers (which acquire the mutex per add_submap()) cannot
    // race with our submaps.find()/erase() calls.
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    std::list<tripoint_abs_sm> to_delete;
    if( save ) {
        // Serialise into the pending-writes cache (no disk I/O).  The data will
        // be flushed to disk on the next explicit save.
        const auto base = project_to<coords::sm>( omt_addr );
        const std::array<tripoint_abs_sm, 4> addrs = { {
                base,
                base + point_rel_sm::east(),
                base + point_rel_sm::south(),
                base + point_rel_sm::south_east()
            }
        };

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
        // Border-only omt: content is identical to what is already on disk.
        // Skip serialisation; just collect the four submap addresses to discard.
        const auto base = project_to<coords::sm>( omt_addr );
        for( const auto &off : { point_rel_sm::zero(), point_rel_sm::south(), point_rel_sm::east(), point_rel_sm::south_east() } ) {
            to_delete.push_back( base + off );
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
        submaps.erase( p );
    }
}

submap *mapbuffer::lookup_submap( const tripoint_abs_sm &p )
{
    ZoneScopedN( "mapbuffer_lookup_submap" );
    // Fast path: submap already resident in memory.
    auto *resident_sm = static_cast<submap *>( nullptr );
    {
        ZoneScopedN( "lookup_memory" );
        resident_sm = lookup_submap_in_memory( p );
    }
    if( resident_sm != nullptr ) {
        ZoneScopedN( "lookup_memory_hit" );
        return resident_sm;
    }
    {
        ZoneScopedN( "lookup_memory_miss" );
    }

    // Cache miss — perform disk I/O outside submaps_mutex_ so that concurrent
    // preload_omt() workers on other omts are not stalled behind this call.
    const tripoint_abs_omt omt_addr = project_to<coords::omt>( p );

    std::string pending_data;
    {
        ZoneScopedN( "lookup_pending_writes" );
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
            ZoneScopedN( "lookup_pending_deserialize" );
            std::istringstream iss( pending_data );
            JsonIn jsin( iss );
            deserialize_into_vec( jsin, loaded, already_loaded );
            found = true;
        } else {
            ZoneScopedN( "lookup_disk_read" );
            found = g->get_active_world()->read_map_omt( dimension_id_.str(), omt_addr,
            [this, &loaded, &already_loaded]( JsonIn & jsin ) {
                ZoneScopedN( "lookup_disk_deserialize" );
                deserialize_into_vec( jsin, loaded, already_loaded );
            } );
        }
        if( !found ) {
            ZoneScopedN( "lookup_not_found" );
            return nullptr;
        }
    } catch( const std::exception &err ) {
        debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        return nullptr;
    }

    {
        ZoneScopedN( "lookup_add_loaded" );
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
        ZoneScopedN( "lookup_final_memory" );
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

auto mapbuffer::get_abs_tile( const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options )
-> std::optional<mapbuffer_abs_tile_view> // *NOPAD*
{
    const auto split = project_remain<coords::sm>( p );
    auto *const sm = get_submap( split.quotient_tripoint, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }

    return mapbuffer_abs_tile_view( split.quotient_tripoint, split.remainder, *sm );
}

auto mapbuffer::get_abs_tile_with_vehicle( const tripoint_abs_ms &p,
        const mapbuffer_lookup_options options )
-> std::optional<mapbuffer_abs_tile_with_vehicle_view> // *NOPAD*
{
    const auto tile = get_abs_tile( p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return mapbuffer_abs_tile_with_vehicle_view( *tile, vehicle_part_at_loaded_tile( p ) );
}

auto mapbuffer::get_abs_submap_view( const tripoint_abs_sm &p,
                                     const mapbuffer_lookup_options options )
-> std::optional<mapbuffer_abs_submap_view> // *NOPAD*
{
    auto *const sm = get_submap( p, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }

    return mapbuffer_abs_submap_view( p, *sm );
}

auto mapbuffer::get_abs_omt_view( const tripoint_abs_omt &p,
                                  const mapbuffer_lookup_options options )
-> std::optional<mapbuffer_abs_omt_view> // *NOPAD*
{
    auto submaps = std::array<const submap *, 4> {};
    auto found_any = false;
    for( const auto &local : omt_submap_offsets() ) {
        const auto index = omt_submap_index( local );
        auto *const sm = get_submap( project_combine( p, local ), options );
        if( !index ) {
            continue;
        }
        submaps[*index] = sm;
        found_any |= sm != nullptr;
    }

    if( !found_any ) {
        return std::nullopt;
    }

    return mapbuffer_abs_omt_view( p, submaps );
}

auto mapbuffer::make_abs_tile_reader(
    const mapbuffer_lookup_options options )
-> mapbuffer_abs_tile_reader // *NOPAD*
{
    return mapbuffer_abs_tile_reader( *this, options );
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
const -> const Creature * // *NOPAD*
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
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    return loaded_vehicles_.contains( const_cast<vehicle *>( veh ) );
}

auto mapbuffer::register_vehicle( vehicle *veh ) -> void
{
    if( veh == nullptr ) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
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

auto mapbuffer::get_ter( const tripoint_abs_ms &p,
                         const mapbuffer_lookup_options options ) -> std::optional<ter_id>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_ter( tile->local );
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
    return true;
}

auto mapbuffer::get_furn( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::optional<furn_id>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_furn( tile->local );
}

auto mapbuffer::set_furn( const tripoint_abs_ms &p, const furn_id furn,
                          const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return false;
    }

    const auto old_id = tile->sm->get_furn( tile->local );
    if( old_id == furn ) {
        return false;
    }

    tile->sm->set_furn( tile->local, furn );
    sync_furniture_change_side_tables( p, *tile->sm, tile->local, old_id, furn );
    invalidate_active_furniture_set_caches( p, old_id, furn );
    map_mutation_hooks::on_furniture_changed( {
        .dim_id = dimension_id_,
        .p = p,
        .old_furniture = old_id,
        .new_furniture = furn,
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

auto mapbuffer::move_cost( const tripoint_abs_ms &p,
                           const mapbuffer_lookup_options options ) -> std::optional<int>
{
    const auto tile = get_abs_tile_with_vehicle( p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->move_cost();
}

auto mapbuffer::passable( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::optional<bool>
{
    const auto tile = get_abs_tile_with_vehicle( p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->passable();
}

auto mapbuffer::valid_move( const tripoint_abs_ms &from, const tripoint_abs_ms &to,
                            const mapbuffer_valid_move_options options ) -> bool
{
    assert( to.z() > std::numeric_limits<int>::min() );
    if( std::abs( from.x() - to.x() ) > 1 || std::abs( from.y() - to.y() ) > 1 ||
        std::abs( from.z() - to.z() ) > 1 ) {
        return false;
    }

    const auto tile_reader = make_abs_tile_reader( options.lookup );

    if( from.z() == to.z() ) {
        const auto target_tile = tile_reader.get_tile_with_vehicle( to );
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

    const auto up_tile = tile_reader.get_tile( up_p );
    if( !up_tile ) {
        return false;
    }
    const auto &up_ter = up_tile->get_ter_t();
    if( up_ter.id.is_null() ) {
        return false;
    }
    const auto &up_furn = up_tile->get_furn_t();
    const auto up_trap_id = up_tile->get_trap();
    const auto up_is_ledge = up_ter.trap == tr_ledge || up_trap_id == tr_ledge;

    if( up_ter.movecost == 0 ) {
        return false;
    }

    const auto down_tile = tile_reader.get_tile( down_p );
    if( !down_tile ) {
        return false;
    }
    const auto &down_ter = down_tile->get_ter_t();
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

    const auto center_tile = get_abs_tile( p, options );
    if( !center_tile ) {
        return std::nullopt;
    }
    const auto has_flag = []( const mapbuffer_abs_tile_view & tile, const auto & flag ) {
        return tile.get_ter_t().has_flag( flag ) || tile.get_furn_t().has_flag( flag );
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
        const auto tile = get_abs_tile_with_vehicle( pt, options );
        if( !tile || !tile->tile().passable_ter_furn() ) {
            best_difficulty = std::min( best_difficulty, 10 );
            blocks_movement++;
        } else if( tile->vehicle_part() ) {
            best_difficulty = std::min( best_difficulty, 7 );
        }

        if( best_difficulty > 5 && tile && has_flag( tile->tile(), "CLIMBABLE" ) ) {
            best_difficulty = 5;
        }
    }

    return std::max( 0, best_difficulty - blocks_movement );
}

auto mapbuffer::has_flag( const std::string &flag, const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    return has_flag_ter_or_furn( flag, p, options );
}

auto mapbuffer::has_flag_ter( const std::string &flag, const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->get_ter( tile->local ).obj().has_flag( flag );
}

auto mapbuffer::has_flag_furn( const std::string &flag, const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->get_furn( tile->local ).obj().has_flag( flag );
}

auto mapbuffer::has_flag_vpart( const std::string &flag, const tripoint_abs_ms &p,
                                const mapbuffer_lookup_options options ) -> bool
{
    const auto vp = veh_at( p, options );
    return static_cast<bool>( vp.part_with_feature( flag, true ) );
}

auto mapbuffer::has_flag_furn_or_vpart( const std::string &flag, const tripoint_abs_ms &p,
                                        const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = get_abs_tile( p, options );
    if( !tile ) {
        return false;
    }
    if( tile->get_furn_t().has_flag( flag ) ) {
        return true;
    }
    return static_cast<bool>( vehicle_part_at_loaded_tile( p ).part_with_feature( flag, true ) );
}

auto mapbuffer::has_flag_ter_or_furn( const std::string &flag, const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile &&
           ( tile->sm->get_ter( tile->local ).obj().has_flag( flag ) ||
             tile->sm->get_furn( tile->local ).obj().has_flag( flag ) );
}

auto mapbuffer::has_flag( const ter_bitflags flag, const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> bool
{
    return has_flag_ter_or_furn( flag, p, options );
}

auto mapbuffer::has_flag_ter( const ter_bitflags flag, const tripoint_abs_ms &p,
                              const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->get_ter( tile->local ).obj().has_flag( flag );
}

auto mapbuffer::has_flag_furn( const ter_bitflags flag, const tripoint_abs_ms &p,
                               const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile && tile->sm->get_furn( tile->local ).obj().has_flag( flag );
}

auto mapbuffer::has_flag_ter_or_furn( const ter_bitflags flag, const tripoint_abs_ms &p,
                                      const mapbuffer_lookup_options options ) -> bool
{
    const auto tile = lookup_tile( *this, p, options );
    return tile &&
           ( tile->sm->get_ter( tile->local ).obj().has_flag( flag ) ||
             tile->sm->get_furn( tile->local ).obj().has_flag( flag ) );
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

auto mapbuffer::furnname( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::string
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        debugmsg( "Invalid tile at (%d, %d, %d)", p.x(), p.y(), p.z() );
        return "null";
    }
    const auto sm = tile->sm;
    const auto local = tile->local;
    const furn_t &f = sm->get_furn( local ).obj();
    if( f.has_flag( "PLANT" ) ) {
        auto items = &sm->get_items( local );
        for( auto it = items->begin(); it != items->end(); ++it ) {
            if( ( *it )->is_seed() ) {
                const std::string &plant = ( *it )->get_plant_name();
                return string_format( "%s (%s)", f.name(), plant );
            }
        }
        debugmsg( "Missing seed for plant at (%d, %d, %d)", p.x(), p.y(), p.z() );
        return "null";
    } else {
        return f.name();
    }
}

auto mapbuffer::get_trap( const tripoint_abs_ms &p,
                          const mapbuffer_lookup_options options ) -> std::optional<trap_id>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_trap( tile->local );
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

auto mapbuffer::creature_on_trap( Creature &critter, const bool may_avoid ) -> void
{
    const auto pos = critter.abs_pos();
    const auto tile = get_abs_tile( pos );
    if( !tile ) {
        return;
    }

    auto trap_here = tile->get_ter_t().trap;
    if( trap_here == tr_null ) {
        trap_here = tile->get_trap();
    }

    const auto &tr = trap_here.obj();
    if( tr.is_null() ) {
        return;
    }
    const player *const pl = critter.as_player();
    if( pl != nullptr && pl->in_vehicle ) {
        return;
    }

    const auto bubble_pos = abs_to_bub( pos );
    if( may_avoid && critter.avoid_trap( bubble_pos, tr ) ) {
        return;
    }
    tr.trigger( bubble_pos, &critter );
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

auto mapbuffer::get_lum( const tripoint_abs_ms &p,
                         const mapbuffer_lookup_options options ) -> std::optional<std::uint8_t>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return std::nullopt;
    }

    return tile->sm->get_lum( tile->local );
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
    if( active_reality_bubble_local( p ) ) {
        g->m.invalidate_lightmap_caches();
    }
    return true;
}

auto mapbuffer::get_temperature( const tripoint_abs_ms &p,
                                 const mapbuffer_lookup_options options ) -> std::optional<int>
{
    const auto split = project_to<coords::sm>( p );
    auto *const sm = get_submap( split, options );
    if( sm == nullptr ) {
        return std::nullopt;
    }

    return sm->get_temperature();
}

auto mapbuffer::set_temperature( const tripoint_abs_ms &p, const int temperature,
                                 const mapbuffer_lookup_options options ) -> bool
{
    const auto split = project_to<coords::sm>( p );
    auto *const sm = get_submap( split, options );
    if( sm == nullptr ) {
        return false;
    }

    sm->set_temperature( temperature );
    return true;
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
        debugmsg( "Tried to add null field" );
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

auto mapbuffer::water_from( const tripoint_abs_ms &p,
                            const mapbuffer_lookup_options options ) -> detached_ptr<item>
{
    const auto tile = lookup_tile( *this, p, options );
    if( !tile ) {
        return detached_ptr<item>();
    }

    if( has_flag( "SALT_WATER", p ) ) {
        return item::spawn( "salt_water", calendar::start_of_cataclysm, item::INFINITE_CHARGES );
    }

    auto sm = tile->sm;
    auto local = tile->local;

    const ter_id terrain_id = sm->get_ter( local );
    if( terrain_id == t_sewage ) {
        detached_ptr<item> ret = item::spawn( "water_sewage", calendar::start_of_cataclysm,
                                              item::INFINITE_CHARGES );
        ret->poison = rng( 1, 7 );
        return ret;
    }


    // iexamine::water_source requires a valid liquid from this function.
    if( terrain_id.obj().examine == &iexamine::water_source ) {
        detached_ptr<item> ret = item::spawn( "water", calendar::start_of_cataclysm,
                                              item::INFINITE_CHARGES );
        int poison_chance = 0;
        if( terrain_id.obj().has_flag( TFLAG_DEEP_WATER ) ) {
            if( terrain_id.obj().has_flag( TFLAG_CURRENT ) ) {
                poison_chance = 20;
            } else {
                poison_chance = 4;
            }
        } else {
            if( terrain_id.obj().has_flag( TFLAG_CURRENT ) ) {
                poison_chance = 10;
            } else {
                poison_chance = 3;
            }
        }
        if( one_in( poison_chance ) ) {
            ret->poison = rng( 1, 4 );
        }
        return ret;
    }
    const furn_t ob = sm->get_furn( local ).obj();
    if( ob.examine == &iexamine::water_source ) {
        return item::spawn( "water", calendar::start_of_cataclysm, item::INFINITE_CHARGES );
    }
    if( ob.examine == &iexamine::clean_water_source ||
        terrain_id.obj().examine == &iexamine::clean_water_source ) {
        return item::spawn( "water_clean", calendar::start_of_cataclysm, item::INFINITE_CHARGES );
    }
    if( ob.examine == &iexamine::liquid_source ) {
        // Terrains have no "provides_liquids" to work with generic source
        return item::spawn( ob.provides_liquids, calendar::turn, item::INFINITE_CHARGES );
    }
    return detached_ptr<item>();
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
        const auto max_dist = 2;
        const auto max_path_length = 4 * max_dist;
        const auto setting = pathfinding_settings( 0, max_dist, max_path_length, 0, false, true, false,
                             false, false );
        return !g->m.route( *source_local, *target_local, setting ).empty();
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
    invalidate_active_item_luminance_cache( p );
    return true;
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
}

auto mapbuffer::sync_furniture_change_side_tables( const tripoint_abs_ms &p, submap &sm,
        const point_sm_ms &local, const furn_id &old_id, const furn_id &new_id ) const -> void
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

    if( new_furniture.active ) {
        cata::poly_serialized<active_tile_data> atd;
        atd.reset( new_furniture.active->clone() );
        atd->set_last_updated( calendar::turn );
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
        g->u.add_known_trap( *local, tr_null.obj() );
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
        const submap &sm ) const
{
    if( g == nullptr || g->m.get_bound_dimension() != dimension_id_ ) {
        return;
    }

    auto &active_submaps = g->m.submaps_with_active_items;
    const auto abs_submap = project_to<coords::sm>( p );
    if( sm.active_items.empty() ) {
        active_submaps.erase( abs_submap );
    } else {
        active_submaps.insert( abs_submap );
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
    const bool all_loaded =
        lookup_submap_in_memory( base )
        && lookup_submap_in_memory( base + point_east )
        && lookup_submap_in_memory( base + point_south )
        && lookup_submap_in_memory( base + point_south_east );
    if( all_loaded ) {
        return {};
    }

    if( const auto uniform_terrain = uniform_terrain_for_omt( dimension_id_, omt_addr ) ) {
        ZoneScopedN( "mapbuffer_generate_uniform_omt" );
        return {
            .status = add_uniform_omt( *this, base, *uniform_terrain )
            ? mapgen_result_status::generated
            : mapgen_result_status::not_generated,
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
    return { .status = mapgen_result_status::generated, .selected_mapgen = nullptr };
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
        run_submap_batch_turns( *tmpsub, missed );
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

    for( const auto p : submap_tiles() ) {
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
