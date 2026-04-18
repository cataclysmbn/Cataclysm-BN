#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include <ranges>

#include "avatar.h"
#include "dimension_bounds.h"
#include "distribution_grid.h"
#include "game.h"
#include "game_constants.h"
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
#include "line.h"
#include "lua_action_menu.h"

namespace
{

struct dimension_travel_options {
    std::string dimension_id;
    tripoint target_omt;
    std::optional<std::string> world_type;
    std::optional<tripoint> bounds_min_omt;
    std::optional<tripoint> bounds_max_omt;
    std::optional<std::string> boundary_terrain;
    std::optional<std::string> boundary_overmap_terrain;
    std::optional<std::string> pregen_special_id;
    std::optional<tripoint> pregen_special_omt;
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

auto read_optional_string( const sol::table &opts, const char *key ) -> std::optional<std::string>
{
    const auto value = opts.get<sol::optional<std::string>>( key );
    if( !value || value->empty() ) {
        return std::nullopt;
    }
    return *value;
}

auto read_optional_tripoint( const sol::table &opts, const char *key ) -> std::optional<tripoint>
{
    const auto value = opts.get<sol::optional<tripoint>>( key );
    if( !value ) {
        return std::nullopt;
    }
    return *value;
}

auto parse_dimension_travel_options( const sol::table &opts ) -> dimension_travel_options
{
    return {
        .dimension_id = opts.get_or( "dimension_id", std::string{} ),
        .target_omt = read_optional_tripoint( opts, "target_omt" ).value_or( tripoint_zero ),
        .world_type = read_optional_string( opts, "world_type" ),
        .bounds_min_omt = read_optional_tripoint( opts, "bounds_min_omt" ),
        .bounds_max_omt = read_optional_tripoint( opts, "bounds_max_omt" ),
        .boundary_terrain = read_optional_string( opts, "boundary_terrain" ),
        .boundary_overmap_terrain = read_optional_string( opts, "boundary_overmap_terrain" ),
        .pregen_special_id = read_optional_string( opts, "pregen_special_id" ),
        .pregen_special_omt = read_optional_tripoint( opts, "pregen_special_omt" ),
    };
}

auto make_dimension_bounds( const dimension_travel_options &opts ) ->
std::optional<dimension_bounds>
{
    if( !opts.bounds_min_omt && !opts.bounds_max_omt ) {
        return std::nullopt;
    }
    if( !opts.bounds_min_omt || !opts.bounds_max_omt ) {
        return std::nullopt;
    }

    return dimension_bounds{
        .min_bound = project_to<coords::sm>( tripoint_abs_omt( *opts.bounds_min_omt ) ),
        .max_bound = tripoint_abs_sm( project_to<coords::sm>( tripoint_abs_omt( *opts.bounds_max_omt ) ).raw() +
                                      point( 1, 1 ) ),
        .boundary_terrain = ter_str_id( opts.boundary_terrain.value_or( "t_pd_border" ) ),
        .boundary_overmap_terrain = oter_str_id( opts.boundary_overmap_terrain.value_or( "pd_border" ) ),
    };
}

auto is_valid_dimension_travel_config( const dimension_travel_options &opts ) -> bool
{
    if( opts.bounds_min_omt.has_value() != opts.bounds_max_omt.has_value() ) {
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

auto find_safe_spawn( const tripoint &target ) -> tripoint
{
    auto &here = get_map();

    if( here.passable( target ) && !g->critter_at( target ) ) {
        return target;
    }

    for( int radius = 1; radius <= 10; radius++ ) {
        for( const auto &point : here.points_in_radius( target, radius ) ) {
            if( here.passable( point ) && !g->critter_at( point ) ) {
                return point;
            }
        }
    }

    return target;
}

auto get_dimension_load_position( const tripoint &target_omt ) -> tripoint_abs_sm
{
    const auto target_sm = project_to<coords::sm>( tripoint_abs_omt( target_omt ) );
    return tripoint_abs_sm( target_sm.raw() - tripoint( g_half_mapsize, g_half_mapsize, 0 ) );
}

auto build_dimension_preload_callback( const dimension_travel_options &opts ) ->
std::function<void()>
{
    if( !opts.pregen_special_id ) {
        return {};
    }

    const auto special_id = overmap_special_id( *opts.pregen_special_id );

    const auto special_omt = tripoint_abs_omt( opts.pregen_special_omt.value_or( opts.target_omt ) );
    const auto dimension_id = opts.dimension_id;

    return [dimension_id, special_id, special_omt]() {
        auto &dim_omb = get_overmapbuffer( dimension_id );
        auto global_location = dim_omb.get_om_global( special_omt );
        overmap &om = *global_location.om;
        om.place_special_forced( special_id, global_location.local, om_direction::type::north );
    };
}

auto place_player_dimension_at( const dimension_travel_options &opts ) -> bool
{
    if( !is_valid_dimension_travel_config( opts ) ) {
        return false;
    }

    auto bounds = make_dimension_bounds( opts );
    const auto has_bounds = opts.bounds_min_omt.has_value() || opts.bounds_max_omt.has_value();
    if( has_bounds && !bounds ) {
        return false;
    }

    auto world_type = world_type_id{};
    if( opts.world_type ) {
        world_type = world_type_id( *opts.world_type );
    }

    const auto preload_callback = build_dimension_preload_callback( opts );
    const auto load_pos = get_dimension_load_position( opts.target_omt );
    if( !g->travel_to_dimension( opts.dimension_id, world_type, bounds, load_pos, preload_callback ) ) {
        return false;
    }

    auto target_abs_ms = project_to<coords::ms>( tripoint_abs_omt( opts.target_omt ) );
    target_abs_ms += tripoint( SEEX, SEEY, 0 );

    auto &avatar = get_avatar();
    const auto target_local = get_map().getlocal( target_abs_ms );
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
    luna::set_fx( lib, "get_distribution_grid_tracker", &get_distribution_grid_tracker );
    luna::set_fx( lib, "light_ambient_lit", []() -> float { return LIGHT_AMBIENT_LIT; } );
    luna::set_fx( lib, "add_msg", sol::overload(
    add_msg_lua, []( sol::variadic_args va ) { add_msg_lua( game_message_type::m_neutral, va ); }
                  ) );
    DOC( "Teleports player to absolute coordinate in overmap" );
    luna::set_fx( lib, "place_player_overmap_at", []( const tripoint & p ) -> void { g->place_player_overmap( tripoint_abs_omt( p ) ); } );
    DOC( "Teleports player to local coordinates within active map" );
    luna::set_fx( lib, "place_player_local_at", []( const tripoint & p ) -> void { g->place_player( p ); } );
    DOC( "Returns the current dimension id. Empty string means the overworld." );
    luna::set_fx( lib, "get_current_dimension_id", []() -> std::string { return g->get_current_dimension_id(); } );
    DOC( "Moves the player into another dimension and loads the destination around the requested OMT." );
    luna::set_fx( lib, "place_player_dimension_at", []( sol::table opts ) -> bool {
        return place_player_dimension_at( parse_dimension_travel_options( opts ) );
    } );
    luna::set_fx( lib, "current_turn", []() -> time_point { return calendar::turn; } );
    luna::set_fx( lib, "turn_zero", []() -> time_point { return calendar::turn_zero; } );
    luna::set_fx( lib, "before_time_starts", []() -> time_point { return calendar::before_time_starts; } );
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

    luna::set_fx( lib, "get_creature_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Creature * { return g->critter_at<Creature>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "get_monster_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> monster * { return g->critter_at<monster>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "place_monster_at", []( const mtype_id & id, const tripoint & p ) { return g->place_critter_at( id, p ); } );
    luna::set_fx( lib, "place_monster_around", []( const mtype_id & id, const tripoint & p,
    const int radius ) { return g->place_critter_around( id, p, radius ); } );
    luna::set_fx( lib, "spawn_hallucination", []( const tripoint & p ) -> bool { return g->spawn_hallucination( p ); } );
    luna::set_fx( lib, "get_character_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Character * { return g->critter_at<Character>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "get_npc_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> npc * { return g->critter_at<npc>( p, allow_hallucination.value_or( false ) ); } );

    luna::set_fx( lib, "choose_adjacent",
    []( const std::string & message, sol::optional<bool> allow_vertical ) -> sol::optional<tripoint> {
        std::optional<tripoint> stdOpt = choose_adjacent( message, allow_vertical.value_or( false ) );

        if( stdOpt.has_value() )
        {
            return sol::optional<tripoint>( *stdOpt );
        }
        return sol::optional<tripoint>();
    } );
    luna::set_fx( lib, "choose_direction", []( const std::string & message,
    sol::optional<bool> allow_vertical ) -> sol::optional<tripoint> {
        std::optional<tripoint> stdOpt = choose_direction( message, allow_vertical.value_or( false ) );

        if( stdOpt.has_value() )
        {
            return sol::optional<tripoint>( *stdOpt );
        }
        return sol::optional<tripoint>();
    } );
    luna::set_fx( lib, "look_around", []() {
        auto result = g->look_around();
        if( result.has_value() ) {
            return sol::optional<tripoint>( *result );
        }
        return sol::optional<tripoint>();
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

    DOC( "Returns all active creatures (monsters, NPCs, and the player) as a Lua array." );
    luna::set_fx( lib, "get_all_creatures", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto npc_rng = g->all_npcs();
        auto mon_rng = g->all_monsters();
        int idx = 1;
        out[idx++] = static_cast<Creature *>( &g->u );
        std::ranges::for_each(
            npc_rng.items
        | std::views::transform( []( const weak_ptr_fast<npc> &wp ) { return wp.lock(); } )
        | std::views::filter( []( const shared_ptr_fast<npc> &sp ) -> bool { return sp && !sp->is_dead(); } ),
        [&out, &idx]( const shared_ptr_fast<npc> &sp ) { out[idx++] = static_cast<Creature *>( sp.get() ); } );
        std::ranges::for_each(
            mon_rng.items
        | std::views::transform( []( const weak_ptr_fast<monster> &wp ) { return wp.lock(); } )
        | std::views::filter( []( const shared_ptr_fast<monster> &sp ) -> bool { return sp && !sp->is_dead(); } ),
        [&out, &idx]( const shared_ptr_fast<monster> &sp ) { out[idx++] = static_cast<Creature *>( sp.get() ); } );
        return out;
    } );

    DOC( "Returns all active NPCs as a Lua array." );
    luna::set_fx( lib, "get_all_npcs", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_npcs();
        int idx = 1;
        std::ranges::for_each(
            rng.items
        | std::views::transform( []( const weak_ptr_fast<npc> &wp ) { return wp.lock(); } )
        | std::views::filter( []( const shared_ptr_fast<npc> &sp ) -> bool { return sp && !sp->is_dead(); } ),
        [&out, &idx]( const shared_ptr_fast<npc> &sp ) { out[idx++] = sp.get(); } );
        return out;
    } );

    DOC( "Returns all active monsters as a Lua array." );
    luna::set_fx( lib, "get_all_monsters", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_monsters();
        int idx = 1;
        std::ranges::for_each(
            rng.items
        | std::views::transform( []( const weak_ptr_fast<monster> &wp ) { return wp.lock(); } )
        | std::views::filter( []( const shared_ptr_fast<monster> &sp ) -> bool { return sp && !sp->is_dead(); } ),
        [&out, &idx]( const shared_ptr_fast<monster> &sp ) { out[idx++] = sp.get(); } );
        return out;
    } );

    DOC( "Returns NPCs in simulated (fully loaded, AI-eligible) submaps as a Lua array." );
    luna::set_fx( lib, "get_simulated_npcs", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_npcs();
        int idx = 1;
        std::ranges::for_each(
            rng.items
        | std::views::transform( []( const weak_ptr_fast<npc> &wp ) { return wp.lock(); } )
        | std::views::filter( []( const shared_ptr_fast<npc> &sp ) -> bool {
            return sp && !sp->is_dead() && sp->is_simulated();
        } ),
        [&out, &idx]( const shared_ptr_fast<npc> &sp ) { out[idx++] = sp.get(); } );
        return out;
    } );

    DOC( "Get the global overmap buffer" );
    luna::set_fx( lib, "get_overmap_buffer", []() -> overmapbuffer & { return get_active_overmapbuffer(); } );

    DOC( "Get direction from a tripoint delta" );
    luna::set_fx( lib, "direction_from", []( const tripoint & delta ) -> direction { return direction_from( delta ); } );

    DOC( "Get direction name from direction enum" );
    luna::set_fx( lib, "direction_name", []( direction dir ) -> std::string { return direction_name( dir ); } );

    DOC( "Get the six cardinal directions (N, S, E, W, Up, Down)" );
    luna::set_fx( lib, "six_cardinal_directions", []() -> std::vector<tripoint> {
        return std::vector<tripoint>{
            tripoint_north, tripoint_south, tripoint_east,
            tripoint_west, tripoint_above, tripoint_below
        };
    } );

    luna::finalize_lib( lib );
}
