#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "inventory.h"
#include "item.h"

void cata::detail::reg_inventory( sol::state &lua )
{
#define UT_CLASS inventory
    {
        DOC( "Represents a character's inventory" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors<UT_CLASS()>()
        );

        DOC( "Get the number of item stacks in the inventory" );
        SET_FX( size );

        DOC( "Clear all items from the inventory" );
        SET_FX( clear );

        DOC( "Check if inventory has the specified tool" );
        luna::set_fx( ut, "has_tools",
        []( const inventory & inv, const itype_id & id, int quantity ) -> bool {
            return inv.has_tools( id, quantity, return_true<item> );
        } );

        DOC( "Check if inventory has the specified components" );
        luna::set_fx( ut, "has_components",
        []( const inventory & inv, const itype_id & id, int quantity ) -> bool {
            return inv.has_components( id, quantity, return_true<item> );
        } );

        DOC( "Check if inventory has the specified charges" );
        luna::set_fx( ut, "has_charges",
        []( const inventory & inv, const itype_id & id, int quantity ) -> bool {
            return inv.has_charges( id, quantity, return_true<item> );
        } );

        DOC( "Get the total weight of the inventory" );
        SET_FX( weight );

        DOC( "Get the total volume of the inventory" );
        SET_FX( volume );

        DOC( "Count items of a specific type" );
        SET_FX( count_item );

        DOC( "Find item at position" );
        luna::set_fx( ut, "find_item",
                      sol::resolve<item &( int )>( &inventory::find_item ) );

        DOC( "Get item position by type" );
        SET_FX( position_by_type );
    }
#undef UT_CLASS
}
