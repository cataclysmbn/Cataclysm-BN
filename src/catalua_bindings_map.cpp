#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "distribution_grid.h"
#include "field.h"
#include "map.h"
#include "trap.h"

namespace sol
{
template <>
struct is_container<item_stack> : std::false_type {};
template <>
struct is_container<map_stack> : std::false_type {};
} // namespace sol

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


namespace
{

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

        DOC( "Creates a new item(s) at a position on the map." );
        luna::set_fx( ut, "create_item_at", []( map & m, const tripoint & p, const itype_id & itype,
        int count ) -> void {
            detached_ptr<item> new_item = item::spawn( itype, calendar::turn, count );
            m.add_item_or_charges( p, std::move( new_item ) );
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
        luna::set_fx( ut, "get_items_at", []( map & m, const tripoint & p ) -> std::unique_ptr<map_stack> { return std::make_unique<map_stack>( m.i_at( p ) ); } );
        luna::set_fx( ut, "remove_item_at", []( map & m, const tripoint & p, item * it ) -> void { m.i_rem( p, it ); } );
        luna::set_fx( ut, "clear_items_at", []( map & m, const tripoint & p ) -> void { m.i_clear( p ); } );


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
        luna::set_fx( ut, "get_trap_at", []( map & m, const tripoint & p ) -> trap_id { return m.tr_at( p ).loadid; } );
        DOC( "Set a trap at a position on the map. It can also replace existing trap, even with `trap_null`." );
        luna::set_fx( ut, "set_trap_at", &map::trap_set );
        DOC( "Disarms a trap using your skills and stats, with consequences depending on success or failure." );
        luna::set_fx( ut, "disarm_trap_at", &map::disarm_trap );
        DOC( "Simpler version of `set_trap_at` with `trap_null`." );
        luna::set_fx( ut, "remove_trap_at", &map::remove_trap );
    }

    // Register 'tinymap' class to be used in Lua
    {
        luna::new_usertype<tinymap>( lua, luna::bases<map>(), luna::no_constructor );
    }

    // Register 'item_stack' class to be used in Lua
    {
        DOC( "Iterate over this using pairs()" );
        sol::usertype<item_stack> ut = luna::new_usertype<item_stack>( lua, luna::no_bases,
                                       luna::no_constructor );

        luna::set_fx( ut, sol::meta_function::pairs, item_stack_lua_pairs );
    }

    // Register 'map_stack' class to be used in Lua
    {
        sol::usertype<map_stack> ut = luna::new_usertype<map_stack>( lua, luna::bases<item_stack>(),
                                      luna::no_constructor );

        luna::set_fx( ut, "as_item_stack", []( map_stack & ref ) -> item_stack& { return ref; } );
    }
}

void cata::detail::reg_distribution_grid( sol::state &lua )
{
    {
        sol::usertype<distribution_grid> ut =
            luna::new_usertype<distribution_grid>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        DOC( "Boolean argument controls recursive behavior" );
        luna::set_fx( ut, "get_resource", &distribution_grid::get_resource );
        DOC( "Boolean argument controls recursive behavior" );
        luna::set_fx( ut, "mod_resource", &distribution_grid::mod_resource );
    }

    {
        sol::usertype<distribution_grid_tracker> ut =
            luna::new_usertype<distribution_grid_tracker>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "get_grid_at_abs_ms",
                      []( distribution_grid_tracker & tr, const tripoint & p ) -> distribution_grid& { return tr.grid_at( tripoint_abs_ms( p ) ); } );
    }

}
