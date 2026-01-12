#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "game.h"
#include "distribution_grid.h"
#include "field.h"
#include "map.h"
#include "map_iterator.h"
#include "trap.h"
#include "detached_ptr.h"

namespace sol
{
template <>
struct is_container<item_stack> : std::false_type {};
template <>
struct is_container<map_stack> : std::false_type {};
} // namespace sol

namespace
{

struct item_stack_lua_it_state {
    item_stack *stack;
    size_t index;

    item_stack_lua_it_state( item_stack &stk )
        : stack( &stk ), index( 0 ) {
    }
};

static std::tuple<sol::object, sol::object>
item_stack_lua_next(
    sol::user<item_stack_lua_it_state &> user_it_state,
    sol::this_state l )
{
    // this gets called
    // to start the first iteration, and every
    // iteration there after

    // the state you passed in item_stack_lua_pairs is argument 1
    // the key value is argument 2, but we do not
    // care about the key value here
    item_stack_lua_it_state &it_state = user_it_state;
    if( it_state.index >= it_state.stack->size() ) {
        // return nil to signify that
        // there's nothing more to work with.
        return std::make_tuple( sol::object( sol::lua_nil ),
                                sol::object( sol::lua_nil ) );
    }
    auto it = it_state.stack->begin();
    std::advance( it, it_state.index );
    item *elem = *it;
    // 2 values are returned (pushed onto the stack):
    // the key and the value
    // the state is left alone
    auto r = std::make_tuple(
                 sol::object( l,  sol::in_place, it_state.index ),
                 sol::object( l, sol::in_place, elem ) );
    // the iterator must be moved forward one before we return
    it_state.index++;
    return r;
}

auto item_stack_lua_pairs( item_stack &stk )
{
    // pairs expects 3 returns:
    // the "next" function on how to advance,
    // the "table" itself or some state,
    // and an initial key value (can be nil)

    // prepare our state
    item_stack_lua_it_state it_state( stk );
    // sol::user is a space/time optimization over regular
    // usertypes, it's incompatible with regular usertypes and
    // stores the type T directly in lua without any pretty
    // setup saves space allocation and a single dereference
    return std::make_tuple( &item_stack_lua_next,
                            sol::user<item_stack_lua_it_state>( std::move( it_state ) ),
                            sol::lua_nil );
}

auto item_stack_lua_length( const item_stack &stk )
{
    return stk.size();
}


item *item_stack_lua_index( item_stack &stk, int i )
{
    --i;
    if( i < 0 || i >= static_cast<int>( stk.size() ) ) {
        return nullptr;
    }
    auto it = stk.begin();
    std::advance( it, i );
    return *it;
}

} // namespace

void cata::detail::reg_map( sol::state &lua )
{
    // Register 'map' class to be used in Lua
    {
        sol::usertype<map> ut = luna::new_usertype<map>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Convert local ms -> absolute ms" );
        luna::set_fx( ut, "get_abs_ms", sol::resolve<tripoint( const tripoint & ) const>( &map::getabs ) );
        DOC( "Convert absolute ms -> local ms" );
        luna::set_fx( ut, "get_local_ms",
                      sol::resolve<tripoint( const tripoint & ) const>( &map::getlocal ) );

        luna::set_fx( ut, "get_map_size_in_submaps", &map::getmapsize );
        DOC( "In map squares" );
        luna::set_fx( ut, "get_map_size", []( const map & m ) -> int { return m.getmapsize() * SEEX; } );
        luna::set_fx( ut, "ambient_light_at", &map::ambient_light_at );

        DOC( "Creates a new item(s) at a position on the map." );
        luna::set_fx( ut, "create_item_at", []( map & m, const tripoint & p, const itype_id & itype,
        int count ) -> item* {
            detached_ptr<item> new_item = item::spawn( itype, calendar::turn, count );
            return m.add_item_or_charges( p, std::move( new_item ) ).get();
        } );

        DOC( "Creates a new corpse at a position on the map. You can skip `Opt` ones by omitting them or passing `nil`. `MtypeId` specifies which monster's body it is, `TimePoint` indicates when it died, `string` gives it a custom name, and `int` determines the revival time if the monster has the `REVIVES` flag." );
        luna::set_fx( ut, "create_corpse_at", []( map & m, const tripoint & p,
                      sol::optional<mtype_id> mtype,
                      sol::optional<time_point> turn, sol::optional<std::string> name,
        sol::optional<int> upgrade_time ) -> void {
            mtype_id the_id = mtype.value_or( mtype_id::NULL_ID() );
            time_point the_tp = turn.value_or( calendar::turn );
            std::string the_name = name.value_or( "" );
            int the_upgrade = upgrade_time.value_or( -1 );

            detached_ptr<item> new_corpse = item::make_corpse( the_id, the_tp, the_name, the_upgrade );
            m.add_item_or_charges( p, std::move( new_corpse ) );
        } );

        luna::set_fx( ut, "has_items_at", &map::has_items );
        luna::set_fx( ut, "remove_item_at", []( map & m, const tripoint & p, item * it ) -> void { m.i_rem( p, it ); } );

        DOC( "Removes an item from the map and returns it as a detached_ptr. The item is now owned by Lua - store it in a table to keep it alive, or let it be GC'd to destroy it. Use add_item to place it back on a map." );
        luna::set_fx( ut, "detach_item_at", []( map & m, const tripoint & p,
        item * it ) -> detached_ptr<item> {
            return m.i_rem( p, it );
        } );

        DOC( "Places a detached item onto the map. Returns nil on success (item now owned by map), or returns the item back if placement failed." );
        luna::set_fx( ut, "add_item", []( map & m, const tripoint & p,
        detached_ptr<item> &it ) -> detached_ptr<item> {
            return m.add_item_or_charges( p, std::move( it ) );
        } );
        luna::set_fx( ut, "clear_items_at", []( map & m, const tripoint & p ) -> void { m.i_clear( p ); } );

        luna::set_fx( ut, "get_items_at", []( map & m, const tripoint & p ) {
            return m.i_at( p );
        } );
        luna::set_fx( ut, "get_items_in_radius", []( map & m, const tripoint & p,
        int radius ) -> std::vector<map_stack> {
            std::vector<map_stack> items;
            for( const auto pt : m.points_in_radius( p, radius ) )
            {
                items.push_back( m.i_at( pt ) );
            }
            return items;
        } );

        DOC( "Returns all points within a radius from the center point. `radiusz` defaults to 0." );
        luna::set_fx( ut, "points_in_radius", []( const map & m, const tripoint & center,
        int radius, sol::optional<int> radiusz ) -> std::vector<tripoint> {
            std::vector<tripoint> points;
            for( const auto pt : m.points_in_radius( center, radius, radiusz.value_or( 0 ) ) )
            {
                points.push_back( pt );
            }
            return points;
        } );

        DOC( "Moves an item from one position to another, preserving all item state including contents." );
        luna::set_fx( ut, "move_item_to", []( map & m, const tripoint & from, item * it,
        const tripoint & to ) -> void {
            detached_ptr<item> detached = m.i_rem( from, it );
            if( detached )
            {
                m.add_item_or_charges( to, std::move( detached ) );
            }
        } );

        luna::set_fx( ut, "get_ter_at", sol::resolve<ter_id( const tripoint & )const>( &map::ter ) );
        luna::set_fx( ut, "set_ter_at",
                      sol::resolve<bool( const tripoint &, const ter_id & )>( &map::ter_set ) );

        luna::set_fx( ut, "get_furn_at", sol::resolve<furn_id( const tripoint & )const>( &map::furn ) );
        luna::set_fx( ut, "set_furn_at", []( map & m, const tripoint & p, const furn_id & id ) { m.furn_set( p, id ); } );

        luna::set_fx( ut, "has_field_at",
                      []( const map & m, const tripoint & p, const field_type_id & fid ) -> bool { return !!m.field_at( p ).find_field( fid ); } );
        luna::set_fx( ut, "get_field_int_at", &map::get_field_intensity );
        luna::set_fx( ut, "get_field_age_at", &map::get_field_age );
        luna::set_fx( ut, "mod_field_int_at", &map::mod_field_intensity );
        luna::set_fx( ut, "mod_field_age_at", &map::mod_field_age );
        luna::set_fx( ut, "set_field_int_at", &map::set_field_intensity );
        luna::set_fx( ut, "set_field_age_at", &map::set_field_age );
        luna::set_fx( ut, "add_field_at", []( map & m, const tripoint & p, const field_type_id & fid,
        int intensity, const time_duration & age ) -> bool {
            return m.add_field( p, fid, intensity, age );
        } );
        luna::set_fx( ut, "remove_field_at", &map::remove_field );
        luna::set_fx( ut, "get_field_name_at", []( map & m, const tripoint & p,
        const field_type_id & fid ) -> std::string {
            field_entry *fe = m.get_field( p, fid );
            return fe ? fe->name() : std::string();
        } );
        luna::set_fx( ut, "get_trap_at", []( map & m, const tripoint & p ) -> trap_id { return m.tr_at( p ).loadid; } );
        DOC( "Set a trap at a position on the map. It can also replace existing trap, even with `trap_null`." );
        luna::set_fx( ut, "set_trap_at", &map::trap_set );
        DOC( "Disarms a trap using your skills and stats, with consequences depending on success or failure." );
        luna::set_fx( ut, "disarm_trap_at", &map::disarm_trap );
        DOC( "Simpler version of `set_trap_at` with `trap_null`." );
        luna::set_fx( ut, "remove_trap_at", &map::remove_trap );

        luna::set_fx( ut, "is_outside", sol::resolve<bool( const tripoint & ) const>( &map::is_outside ) );
        // Actually sheltered or in sunlight doesn't need map, but it's convenient to have it here
        luna::set_fx( ut, "is_sheltered", []( map & m, tripoint & pos ) -> bool { return g->is_sheltered( pos ); } );

        luna::set_fx( ut, "is_in_sunlight", []( map & m, tripoint & pos ) -> bool { return g->is_in_sunlight( pos ); } );
    }

    // Register 'tinymap' class to be used in Lua
    {
        luna::new_usertype<tinymap>( lua, luna::bases<map>(), luna::no_constructor );
    }

    // Register 'item_stack' class to be used in Lua
#define UT_CLASS item_stack
    {
        DOC( "Iterate over this using pairs() for reading. Can also be indexed." );
        sol::usertype<item_stack> ut = luna::new_usertype<item_stack>( lua, luna::no_bases,
                                       luna::no_constructor );

        luna::set_fx( ut, sol::meta_function::pairs, item_stack_lua_pairs );
        luna::set_fx( ut, sol::meta_function::length, item_stack_lua_length );
        luna::set_fx( ut, sol::meta_function::index, item_stack_lua_index );

        DOC( "Modifying the stack while iterating may cause problems. This returns a frozen copy of the items in the stack for safe modification of the stack (eg. removing items while iterating)." );
        luna::set_fx( ut, "items", []( UT_CLASS & c ) {
            std::vector<item *> ret{};
            std::ranges::copy( c, std::back_inserter( ret ) );
            return ret;
        } );
        SET_FX( remove );
        luna::set_fx( ut, "insert", []( UT_CLASS & c, detached_ptr<item> &i ) {
            c.insert( std::move( i ) );
        } );
        SET_FX( clear );
        SET_FX_N( size, "count" );
        SET_FX( amount_can_fit );
        SET_FX( count_limit );
        SET_FX( free_volume );
        SET_FX( stored_volume );
        SET_FX( max_volume );
        SET_FX( move_all_to );
        SET_FX( only_item );
        SET_FX_T( stacks_with, item * ( const item & ) );
    }
#undef UT_CLASS

    // Register 'map_stack' class to be used in Lua
    {
        sol::usertype<map_stack> ut = luna::new_usertype<map_stack>( lua, luna::bases<item_stack>(),
                                      luna::no_constructor );

        luna::set_fx( ut, "as_item_stack", []( map_stack & ref ) -> item_stack& { return ref; } );

        luna::set_fx( ut, sol::meta_function::pairs, item_stack_lua_pairs );
        luna::set_fx( ut, sol::meta_function::length, item_stack_lua_length );
        luna::set_fx( ut, sol::meta_function::index, item_stack_lua_index );
    }
}

void cata::detail::reg_distribution_grid( sol::state &lua )
{
    {
        DOC( "Power generation and consumption statistics for a grid" );
        sol::usertype<power_stat> ut =
            luna::new_usertype<power_stat>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        DOC( "Power generation in watts" );
        luna::set( ut, "gen_w", &power_stat::gen_w );
        DOC( "Power consumption in watts" );
        luna::set( ut, "use_w", &power_stat::use_w );
        DOC( "Net power (generation - consumption) in watts" );
        luna::set_fx( ut, "net_w", &power_stat::net_w );

        luna::set_fx( ut, sol::meta_function::addition, &power_stat::operator+ );
    }

    {
        DOC( "A grid that organizes producers, storage and consumers of a resource like electricity" );
        sol::usertype<distribution_grid> ut =
            luna::new_usertype<distribution_grid>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        DOC( "Check if grid is empty" );
        luna::set_fx( ut, "empty", &distribution_grid::empty );
        DOC( "Check if grid is valid" );
        luna::set_fx( ut, "is_valid",
                      []( const distribution_grid & g ) -> bool { return static_cast<bool>( g ); } );
        DOC( "Update the grid to the given time point" );
        luna::set_fx( ut, "update", &distribution_grid::update );
        DOC( "Modify resource amount. First argument is amount, second (optional) controls recursive behavior (default true)" );
        luna::set_fx( ut, "get_resource",
        []( const distribution_grid & g, sol::optional<bool> recurse ) { return g.get_resource( recurse.value_or( true ) ); } );
        luna::set_fx( ut, "mod_resource",
        []( distribution_grid & g, int amt, sol::optional<bool> recurse ) { return g.mod_resource( amt, recurse.value_or( true ) ); } );
        DOC( "Get current resource amount. Boolean argument (optional) controls recursive behavior (default true)" );
        DOC( "Get vector of absolute map square coordinates of grid contents" );
        luna::set_fx( ut, "get_contents", &distribution_grid::get_contents );
        DOC( "Get power generation and consumption statistics for the grid" );
        luna::set_fx( ut, "get_power_stat", &distribution_grid::get_power_stat );
    }

    {
        DOC( "Manages all active distribution grids" );
        sol::usertype<distribution_grid_tracker> ut =
            luna::new_usertype<distribution_grid_tracker>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        DOC( "Get grid at absolute map square position" );
        luna::set_fx( ut, "grid_at",
                      sol::overload(
                          []( distribution_grid_tracker & tr, const tripoint & p ) -> distribution_grid& { return tr.grid_at( tripoint_abs_ms( p ) ); }, // *NOPAD*
                          []( const distribution_grid_tracker & tr, const tripoint & p ) -> const distribution_grid& { return tr.grid_at( tripoint_abs_ms( p ) ); } // *NOPAD*
                      ) );
        DOC( "Get unique identifier for grid at given overmap tile (for debug purposes, returns 0 if no grid)" );
        luna::set_fx( ut, "debug_grid_id",
                      []( const distribution_grid_tracker & tr, const tripoint & omt_pos ) -> std::uintptr_t { return tr.debug_grid_id( tripoint_abs_omt( omt_pos ) ); } );
        DOC( "Update all grids to the given time point" );
        luna::set_fx( ut, "update", &distribution_grid_tracker::update );
        DOC( "Load grids for the given map" );
        luna::set_fx( ut, "load", sol::resolve<void( const map & )>( &distribution_grid_tracker::load ) );
        DOC( "Notify tracker that a tile at the given position has changed" );
        luna::set_fx( ut, "on_changed",
        []( distribution_grid_tracker & tr, const tripoint & p ) {
            tr.on_changed( tripoint_abs_ms( p ) );
        } );
        DOC( "Notify tracker that the game has been saved" );
        luna::set_fx( ut, "on_saved", &distribution_grid_tracker::on_saved );
        DOC( "Notify tracker that game options have changed" );
        luna::set_fx( ut, "on_options_changed", &distribution_grid_tracker::on_options_changed );
    }

}
