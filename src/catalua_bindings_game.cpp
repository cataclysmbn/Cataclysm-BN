#include "catalua_bindings.h"
#include "catalua_coord.h"
#include "catalua_bindings_utils.h"
#include "catalua_bindings_game_internal.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "avatar.h"
#include "creature_tracker.h"
#include "dimension_info.h"
#include "distribution_grid.h"
#include "game.h"
#include "game_constants.h"
#include "iexamine.h"
#include "lightmap.h"
#include "map.h"
#include "map_iterator.h"
#include "catalua_log.h"
#include "messages.h"
#include "npc.h"
#include "monster.h"
#include "overmap.h"
#include "overmap_special.h"
#include "overmapbuffer.h"
#include "overmapbuffer_registry.h"
#include "point.h"
#include "weather.h"
#include "line.h"
#include "lua_action_menu.h"

namespace
{

struct overmap_terrain_entry {
    tripoint_rel_omt offset;
    oter_str_id terrain;
};

struct dimension_travel_options {
    dimension_id dim_id;
    tripoint_abs_omt target_omt;
    std::optional<tripoint_abs_ms> target_ms;
    bool has_target = false;
    std::optional<std::string> world_type;
    std::optional<tripoint_abs_omt> bounds_min_omt;
    std::optional<tripoint_abs_omt> bounds_max_omt;
    std::optional<std::string> boundary_terrain;
    std::optional<std::string> boundary_overmap_terrain;
    std::optional<std::vector<overmap_terrain_entry>> overmap_terrain;
    std::optional<std::string> pregen_special_id;
    std::optional<tripoint_abs_omt> pregen_special_omt;
};

auto add_msg_lua( game_message_type t, sol::variadic_args va ) -> void
{
    if( va.size() == 0 ) {
        // Nothing to print
        return;
    }

    std::string msg = cata::detail::fmt_lua_va( va );
    add_msg( t, msg );
}

auto place_lua_monster_around( const mtype_id &id, const tripoint_bub_ms &center,
                               const int radius ) -> monster * // *NOPAD*
{
    const auto placed = g->place_critter_around( id, center, radius );
    if( placed != nullptr ) {
        placed->try_upgrade( true );
    }
    return placed;
}



auto read_optional_string( const sol::table &opts, const char *key ) -> std::optional<std::string>
{
    const auto value = opts.get<sol::optional<std::string>>( key );
    if( !value || value->empty() ) {
        return std::nullopt;
    }
    return *value;
}

auto read_optional_abs_omt( const sol::table &opts,
                            const char *key ) -> std::optional<tripoint_abs_omt>
{
    const auto value = opts.get<sol::optional<tripoint_abs_omt>>( key );
    if( !value ) {
        return std::nullopt;
    }
    return *value;
}

auto read_optional_abs_ms( const sol::table &opts,
                           const char *key ) -> std::optional<tripoint_abs_ms>
{
    const auto value = opts.get<sol::optional<tripoint_abs_ms>>( key );
    if( !value ) {
        return std::nullopt;
    }
    return *value;
}

auto is_dense_lua_array( const sol::table &table ) -> bool
{
    const auto length = static_cast<int>( table.size() );
    auto key_count = 0;
    auto valid = true;
    table.for_each( [&]( const sol::object & key, const sol::object & /*value*/ ) {
        if( !valid ) {
            return;
        }
        if( !key.is<int>() ) {
            valid = false;
            return;
        }
        const auto index = key.as<int>();
        if( index < 1 || index > length ) {
            valid = false;
            return;
        }
        ++key_count;
    } );
    return valid && key_count == length;
}

auto read_optional_overmap_terrain_layout( const sol::table &opts ) ->
std::optional<std::vector<overmap_terrain_entry>>
{
    const auto value = opts.get<sol::object>( "overmap_terrain" );
    if( value == sol::nil ) {
        return std::vector<overmap_terrain_entry> {};
    }
    if( !value.is<sol::table>() ) {
        return std::nullopt;
    }

    const auto layers = value.as<sol::table>();
    if( !is_dense_lua_array( layers ) ) {
        return std::nullopt;
    }
    auto entries = std::vector<overmap_terrain_entry> {};
    auto invalid = false;
    std::ranges::for_each( std::views::iota( 1, static_cast<int>( layers.size() ) + 1 ),
    [&]( const int z_idx ) {
        if( invalid ) {
            return;
        }
        const auto layer_obj = layers.get<sol::object>( z_idx );
        if( !layer_obj.is<sol::table>() ) {
            invalid = true;
            return;
        }
        const auto rows = layer_obj.as<sol::table>();
        if( !is_dense_lua_array( rows ) ) {
            invalid = true;
            return;
        }
        std::ranges::for_each( std::views::iota( 1, static_cast<int>( rows.size() ) + 1 ),
        [&]( const int y_idx ) {
            if( invalid ) {
                return;
            }
            const auto row_obj = rows.get<sol::object>( y_idx );
            if( !row_obj.is<sol::table>() ) {
                invalid = true;
                return;
            }
            const auto row = row_obj.as<sol::table>();
            if( !is_dense_lua_array( row ) ) {
                invalid = true;
                return;
            }
            std::ranges::for_each( std::views::iota( 1, static_cast<int>( row.size() ) + 1 ),
            [&]( const int x_idx ) {
                if( invalid ) {
                    return;
                }
                const auto terrain_obj = row.get<sol::object>( x_idx );
                if( !terrain_obj.is<std::string>() ) {
                    invalid = true;
                    return;
                }
                const auto terrain = oter_str_id( terrain_obj.as<std::string>() );
                if( !terrain.is_valid() ) {
                    invalid = true;
                    return;
                }
                entries.push_back( overmap_terrain_entry{
                    .offset = tripoint_rel_omt( x_idx - 1, y_idx - 1, z_idx - 1 ),
                    .terrain = terrain,
                } );
            } );
        } );
    } );

    if( invalid || ( entries.empty() && layers.size() > 0 ) ) {
        return std::nullopt;
    }
    return entries;
}

auto get_dimension_entry_point( const tripoint_abs_omt &target_omt ) -> tripoint_abs_ms
{
    return project_combine( target_omt, point_omt_ms( SEEX, SEEY ) );
}

auto get_dimension_entry_point( const dimension_travel_options &opts ) -> tripoint_abs_ms
{
    return opts.target_ms.value_or( get_dimension_entry_point( opts.target_omt ) );
}

auto parse_dimension_travel_options( const sol::table &opts ) -> dimension_travel_options
{
    const auto target_ms = read_optional_abs_ms( opts, "target_ms" );
    const auto target_omt = read_optional_abs_omt( opts, "target_omt" );
    return {
        .dim_id = dimension_id( opts.get_or( "dimension_id", std::string{} ) ),
        .target_omt = target_omt.value_or(
            target_ms ? project_to<coords::omt>( *target_ms ) : tripoint_abs_omt( tripoint_zero ) ),
        .target_ms = target_ms,
        .has_target = target_ms.has_value() || target_omt.has_value(),
        .world_type = read_optional_string( opts, "world_type" ),
        .bounds_min_omt = read_optional_abs_omt( opts, "bounds_min_omt" ),
        .bounds_max_omt = read_optional_abs_omt( opts, "bounds_max_omt" ),
        .boundary_terrain = read_optional_string( opts, "boundary_terrain" ),
        .boundary_overmap_terrain = read_optional_string( opts, "boundary_overmap_terrain" ),
        .overmap_terrain = read_optional_overmap_terrain_layout( opts ),
        .pregen_special_id = read_optional_string( opts, "pregen_special_id" ),
        .pregen_special_omt = read_optional_abs_omt( opts, "pregen_special_omt" ),
    };
}

auto make_pocket_dimension_data( const dimension_travel_options &opts ) ->
std::optional<pocket_dimension_data>
{
    if( !opts.bounds_min_omt && !opts.bounds_max_omt ) {
        return std::nullopt;
    }
    if( !opts.bounds_min_omt || !opts.bounds_max_omt ) {
        return std::nullopt;
    }

    auto pocket_data = pocket_dimension_data{};
    pocket_data.entry_point = get_dimension_entry_point( opts );
    pocket_data.bounds = dimension_bounds{
        .min_bound = project_to<coords::sm>( *opts.bounds_min_omt ),
        .max_bound = project_to<coords::sm>( *opts.bounds_max_omt ) + point_rel_sm::south_east(),
        .boundary_terrain = ter_str_id( opts.boundary_terrain.value_or( "t_pd_border" ) ),
        .boundary_overmap_terrain = oter_str_id( opts.boundary_overmap_terrain.value_or( "pd_border" ) ),
    };
    return pocket_data;
}

auto overmap_terrain_layout_fits_bounds( const dimension_travel_options &opts ) -> bool
{
    if( !opts.overmap_terrain || opts.overmap_terrain->empty() ) {
        return true;
    }
    if( !opts.bounds_min_omt || !opts.bounds_max_omt ) {
        return false;
    }

    return std::ranges::all_of( *opts.overmap_terrain, [&]( const overmap_terrain_entry & entry ) {
        const auto pos = *opts.bounds_min_omt + entry.offset;
        return pos.x() >= opts.bounds_min_omt->x() && pos.x() <= opts.bounds_max_omt->x() &&
               pos.y() >= opts.bounds_min_omt->y() && pos.y() <= opts.bounds_max_omt->y() &&
               pos.z() >= opts.bounds_min_omt->z() && pos.z() <= opts.bounds_max_omt->z();
    } );
}

auto is_valid_dimension_travel_config( const dimension_travel_options &opts ) -> bool
{
    if( !opts.has_target ) {
        return false;
    }

    if( opts.bounds_min_omt.has_value() != opts.bounds_max_omt.has_value() ) {
        return false;
    }

    if( !opts.overmap_terrain || !overmap_terrain_layout_fits_bounds( opts ) ) {
        return false;
    }

    if( opts.pregen_special_id ) {
        const auto special_id = overmap_special_id( *opts.pregen_special_id );
        if( !special_id.is_valid() ) {
            return false;
        }
    }

    return true;
}

auto find_safe_spawn( const tripoint_bub_ms &target ) -> tripoint_bub_ms
{
    auto &here = get_map();

    if( here.passable( target ) && !g->critter_at( target ) ) {
        return target;
    }

    for( auto radius = 1; radius <= 10; radius++ ) {
        for( const auto &point : here.points_in_radius( target, radius ) ) {
            if( here.passable( point ) && !g->critter_at( point ) ) {
                return point;
            }
        }
    }

    return target;
}

auto get_dimension_load_position( const dimension_travel_options &opts ) -> tripoint_abs_sm
{
    const auto target_sm = project_to<coords::sm>( get_dimension_entry_point( opts ) );
    return target_sm - tripoint_rel_sm( g_half_mapsize, g_half_mapsize, 0 );
}

auto build_dimension_preload_callback( const dimension_travel_options &opts ) ->
std::function<void()>
{
    const auto has_overmap_terrain = opts.overmap_terrain && !opts.overmap_terrain->empty();
    if( !opts.pregen_special_id && !has_overmap_terrain ) {
        return {};
    }

    const auto special_id = opts.pregen_special_id ? std::optional<overmap_special_id>(
                                *opts.pregen_special_id ) : std::nullopt;
    const auto special_omt = opts.pregen_special_omt.value_or( opts.target_omt );
    const auto dim_id = opts.dim_id;
    const auto overmap_terrain_origin = opts.bounds_min_omt.value_or( opts.target_omt );
    const auto overmap_terrain = opts.overmap_terrain.value_or( std::vector<overmap_terrain_entry> {} );

    return [dim_id, special_id, special_omt, overmap_terrain_origin, overmap_terrain]() {
        auto &dim_omb = get_overmapbuffer( dim_id );
        if( special_id ) {
            auto global_location = dim_omb.get_om_global( special_omt );
            auto &om = *global_location.om;
            om.place_special_forced( *special_id, global_location.local, om_direction::type::north );
        }
        std::ranges::for_each( overmap_terrain, [&]( const overmap_terrain_entry & entry ) {
            dim_omb.ter_set( overmap_terrain_origin + entry.offset, entry.terrain.id() );
        } );
    };
}

auto place_player_dimension_at( const dimension_travel_options &opts ) -> bool
{
    if( !is_valid_dimension_travel_config( opts ) ) {
        return false;
    }

    auto pocket_data = make_pocket_dimension_data( opts );
    const auto has_bounds = opts.bounds_min_omt.has_value() || opts.bounds_max_omt.has_value();
    if( has_bounds && !pocket_data ) {
        return false;
    }

    auto world_type = world_type_id{};
    if( opts.world_type ) {
        world_type = world_type_id( *opts.world_type );
    }

    const auto preload_callback = build_dimension_preload_callback( opts );
    const auto load_pos = get_dimension_load_position( opts );
    if( !g->travel_to_dimension( opts.dim_id, world_type, pocket_data, load_pos,
                                 preload_callback ) ) {
        return false;
    }

    auto &avatar = get_avatar();
    const auto target_local = abs_to_bub( get_dimension_entry_point( opts ) );
    avatar.setpos( find_safe_spawn( target_local ) );
    g->update_map( avatar );
    return true;
}

} // namespace

void cata::detail::reg_game_api( sol::state &lua )
{
    DOC( "Global game methods" );
    luna::userlib lib = luna::begin_lib( lua, "gapi" );

    luna::set_fx( lib, "get_avatar", &get_avatar );
    luna::set_fx( lib, "get_map", &get_map );
    luna::set_fx( lib, "bub_to_abs",
                  sol::overload(
                      []( const tripoint_bub_ms & p ) -> tripoint_abs_ms { return bub_to_abs( p ); },
                      []( const tripoint_bub_sm & p ) -> tripoint_abs_sm { return bub_to_abs( p ); }
                  ) );
    luna::set_fx( lib, "abs_to_bub",
                  sol::overload(
                      []( const tripoint_abs_ms & p ) -> tripoint_bub_ms { return abs_to_bub( p ); },
                      []( const tripoint_abs_sm & p ) -> tripoint_bub_sm { return abs_to_bub( p ); }
                  ) );
    luna::set_fx( lib, "get_distribution_grid_tracker",
                  []() -> distribution_grid_tracker & { return get_distribution_grid_tracker(); } );
    luna::set_fx( lib, "light_ambient_lit", []() -> float { return LIGHT_AMBIENT_LIT; } );
    luna::set_fx( lib, "add_msg", sol::overload(
    add_msg_lua, []( sol::variadic_args va ) { add_msg_lua( game_message_type::m_neutral, va ); }
                  ) );
    DOC( "Teleports player to absolute coordinate in overmap" );
    luna::set_fx( lib, "place_player_overmap_at", []( const tripoint_abs_omt & p ) -> void { g->place_player_overmap( p ); } );
    DOC( "Teleports player to local coordinates within active map" );
    luna::set_fx( lib, "place_player_local_at", []( const tripoint_bub_ms & p ) -> void { g->place_player( p ); } );
    DOC( "Returns the current dimension id. Empty string means the overworld." );
    luna::set_fx( lib, "get_current_dimension_id", []() -> std::string { return g->get_current_dimension_id().str(); } );
    DOC( "Moves the player into another dimension and loads the destination around required target_ms or target_omt." );
    DOC( "@alias OvermapTerrainLayout string[][][]" );
    DOC( "@class DimensionTravelOptions" );
    DOC( "@field dimension_id? string Target dimension id. Empty string means the overworld." );
    DOC( "@field target_ms? TripointAbsMs Absolute map-square destination. Required when target_omt is absent." );
    DOC( "@field target_omt? TripointAbsOmt Absolute overmap-terrain destination. Required when target_ms is absent." );
    DOC( "@field world_type? string World type for creating a new non-overworld dimension." );
    DOC( "@field bounds_min_omt? TripointAbsOmt Inclusive pocket dimension minimum overmap-terrain bound." );
    DOC( "@field bounds_max_omt? TripointAbsOmt Inclusive pocket dimension maximum overmap-terrain bound." );
    DOC( "@field boundary_terrain? string Map terrain used outside pocket bounds." );
    DOC( "@field boundary_overmap_terrain? string Overmap terrain used outside pocket bounds." );
    DOC( "@field overmap_terrain? OvermapTerrainLayout Optional z/y/x table anchored at bounds_min_omt." );
    DOC( "@field pregen_special_id? string Overmap special to force before loading the destination." );
    DOC( "@field pregen_special_omt? TripointAbsOmt Overmap-terrain position for pregen_special_id; defaults to target_omt." );
    DOC_PARAMS( "opts: DimensionTravelOptions" );
    luna::set_fx( lib, "place_player_dimension_at", []( sol::table opts ) -> bool {
        return place_player_dimension_at( parse_dimension_travel_options( opts ) );
    } );
    luna::set_fx( lib, "current_turn", []() -> time_point { return calendar::turn; } );
    luna::set_fx( lib, "turn_zero", []() -> time_point { return calendar::turn_zero; } );
    luna::set_fx( lib, "before_time_starts", []() -> time_point { return calendar::before_time_starts; } );
    luna::set_fx( lib, "bodytemp_cold", []() -> int { return BODYTEMP_COLD; } );
    luna::set_fx( lib, "bodytemp_norm", []() -> int { return BODYTEMP_NORM; } );
    luna::set_fx( lib, "bodytemp_hot", []() -> int { return BODYTEMP_HOT; } );
    luna::set_fx( lib, "rng", sol::resolve<int( int, int )>( &rng ) );
    DOC( "Get recent player message log entries. Returns array of { time=string, text=string }." );
    luna::set_fx( lib, "get_messages", []( sol::this_state lua_this, const int count ) {
        sol::state_view lua( lua_this );
        auto out = lua.create_table();
        const auto clamped = std::max( 0, count );
        auto entries = Messages::recent_messages( static_cast<size_t>( clamped ) );
        auto indices = std::views::iota( size_t{ 0 }, entries.size() );
        std::ranges::for_each( indices, [&]( const size_t idx ) {
            const auto &entry = entries[idx];
            auto row = lua.create_table_with(
                           "time", entry.first,
                           "text", entry.second
                       );
            out[idx + 1] = row;
        } );
        return out;
    } );
    DOC( "Get recent Lua console log entries. Returns array of { level=string, text=string, from_user=bool }." );
    luna::set_fx( lib, "get_lua_log", []( sol::this_state lua_this, const int count ) {
        sol::state_view lua( lua_this );
        auto out = lua.create_table();
        const auto clamped = std::max( 0, count );
        const auto &entries = cata::get_lua_log_instance().get_entries();
        const auto take = std::min( static_cast<size_t>( clamped ), entries.size() );
        auto indices = std::views::iota( size_t{ 0 }, take );
        const auto level_name = []( const cata::LuaLogLevel level ) -> std::string {
            switch( level )
            {
                case cata::LuaLogLevel::Input:
                    return "input";
                case cata::LuaLogLevel::Info:
                    return "info";
                case cata::LuaLogLevel::Warn:
                    return "warn";
                case cata::LuaLogLevel::Error:
                    return "error";
                case cata::LuaLogLevel::DebugMsg:
                    return "debug";
            }
            return "unknown";
        };
        std::ranges::for_each( indices, [&]( const size_t idx ) {
            const auto &entry = entries[idx];
            auto row = lua.create_table_with(
                           "level", level_name( entry.level ),
                           "text", entry.text,
                           "from_user", entry.level == cata::LuaLogLevel::Input
                       );
            out[idx + 1] = row;
        } );
        return out;
    } );
    luna::set_fx( lib, "add_on_every_x_hook",
    []( sol::this_state lua_this, time_duration interval, sol::protected_function f ) {
        sol::state_view lua( lua_this );
        std::vector<on_every_x_hooks> &hooks = lua["game"]["cata_internal"]["on_every_x_hooks"];
        for( auto &entry : hooks ) {
            if( entry.interval == interval ) {
                entry.functions.push_back( f );
                return;
            }
        }
        std::vector<sol::protected_function> vec;
        vec.push_back( f );
        hooks.push_back( on_every_x_hooks{ interval, vec } );
    } );

    DOC( "Register a Lua-defined action menu entry in the in-game action menu." );
    luna::set_fx( lib, "register_action_menu_entry", []( sol::table opts ) -> void {
        auto id = opts.get_or( "id", std::string{} );
        auto name = opts.get_or( "name", std::string{} );
        auto category_id = opts.get_or( "category", std::string{ "misc" } );
        auto hotkey = opts.get<sol::optional<std::string>>( "hotkey" );
        auto hotkey_value = std::optional<std::string>{};
        if( hotkey )
        {
            hotkey_value = std::move( *hotkey );
        }
        auto fn = opts.get_or<sol::protected_function>( "fn", sol::lua_nil );
        cata::lua_action_menu::register_entry( {
            .id = std::move( id ),
            .name = std::move( name ),
            .category_id = std::move( category_id ),
            .hotkey = std::move( hotkey_value ),
            .fn = std::move( fn ),
        } );
    } );

    DOC( "Spawns a new item. Same as Item::spawn " );
    luna::set_fx( lib, "create_item", []( const itype_id & itype, int count ) -> detached_ptr<item> {
        return item::spawn( itype, calendar::turn, count );
    } );

    luna::set_fx( lib, "get_creature_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> Creature * {
        return g->critter_at<Creature>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Creature * {
        return g->critter_at<Creature>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );
    luna::set_fx( lib, "get_monster_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> monster * {
        return g->critter_at<monster>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> monster * {
        return g->critter_at<monster>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );
    luna::set_fx( lib, "place_monster_at", sol::overload(
    []( const mtype_id & id, const tripoint_bub_ms & p ) { return place_lua_monster_around( id, p, 0 ); },
    []( const mtype_id & id, const tripoint & p ) { return place_lua_monster_around( id, tripoint_bub_ms( p ), 0 ); } ) );
    luna::set_fx( lib, "place_monster_around", sol::overload(
    []( const mtype_id & id, const tripoint_bub_ms & p, const int radius ) {
        return place_lua_monster_around( id, p, radius );
    },
    []( const mtype_id & id, const tripoint & p, const int radius ) {
        return place_lua_monster_around( id, tripoint_bub_ms( p ), radius );
    } ) );
    luna::set_fx( lib, "spawn_hallucination", sol::overload(
                      []( const tripoint_bub_ms & p ) -> bool { return g->spawn_hallucination( p ); },
                      []( const tripoint & p ) -> bool { return g->spawn_hallucination( tripoint_bub_ms( p ) ); } ) );
    luna::set_fx( lib, "get_character_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> Character * {
        return g->critter_at<Character>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Character * {
        return g->critter_at<Character>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );
    luna::set_fx( lib, "get_npc_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> npc * {
        return g->critter_at<npc>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> npc * {
        return g->critter_at<npc>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );

    luna::set_fx( lib, "choose_adjacent",
                  []( const std::string & message,
    sol::optional<bool> allow_vertical ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent( message, allow_vertical.value_or( false ) );
    } );
    luna::set_fx( lib, "choose_adjacent_highlight", sol::overload(
                      []( const std::string & message, const std::string & failure_message, const action_id & actionId,
    sol::optional<bool> allow_vertical ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent_highlight( message, failure_message, actionId, allow_vertical.value_or( false ) );
    }, [](
        const std::string & message,
        const std::string & failure_message,
        const std::function < auto( const tripoint_bub_ms & ) -> bool > &allowed,
        sol::optional<bool> allow_vertical
    ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent_highlight( message, failure_message, allowed, allow_vertical.value_or( false ) );
    } ) );
    luna::set_fx( lib, "choose_adjacent_uilist", [](
                      const std::string & message,
                      const std::string & failure_message,
                      const sol::protected_function & allowed,
                      const sol::protected_function & name
    ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent_uilist( message, failure_message, allowed, name );
    } );
    luna::set_fx( lib, "choose_area", [](
                      const std::string & message,
                      const tripoint_bub_ms & start_pos,
                      const bool allow_vertical
    ) -> std::optional<std::pair<tripoint_bub_ms, tripoint_bub_ms>> {
        return choose_area( message, start_pos, allow_vertical );
    } );

    luna::set_fx( lib, "choose_direction", []( const std::string & message,
    sol::optional<bool> allow_vertical ) -> std::optional<tripoint_rel_ms> {
        return choose_direction( message, allow_vertical.value_or( false ) );
    } );
    luna::set_fx( lib, "look_around", []() {
        return g->look_around();
    } );

    luna::set_fx( lib, "play_variant_sound",
                  sol::overload(
                      sol::resolve<void( const std::string &, const std::string &, int )>( &sfx::play_variant_sound ),
                      sol::resolve<void( const std::string &, const std::string &, int,
                                         units::angle, double, double )>( &sfx::play_variant_sound )
                  ) );
    luna::set_fx( lib, "play_ambient_variant_sound", &sfx::play_ambient_variant_sound );

    luna::set_fx( lib, "add_npc_follower", []( npc & p ) { g->add_npc_follower( p.getID() ); } );
    luna::set_fx( lib, "remove_npc_follower", []( npc & p ) { g->remove_npc_follower( p.getID() ); } );

    reg_game_api_creature_queries( lib );
    reg_game_api_world_helpers( lib );

    luna::finalize_lib( lib );
}
