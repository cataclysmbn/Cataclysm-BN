#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "requirements.h"
#include "inventory.h"

void cata::detail::reg_requirement( sol::state &lua )
{
#define UT_CLASS requirement_data
    {
        DOC( "Represents crafting requirements (tools, components, qualities)" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        DOC( "Get the requirement ID as string" );
        luna::set_fx( ut, "id", []( const requirement_data & req ) -> std::string {
            return req.id().str();
        } );

        DOC( "Check if this is a null requirement" );
        SET_FX( is_null );

        DOC( "Check if this requirement is empty" );
        SET_FX( is_empty );

        DOC( "Check if this requirement is blacklisted" );
        SET_FX( is_blacklisted );

        DOC( "Get list of all required tools" );
        SET_FX( get_tools );

        DOC( "Get list of all required qualities" );
        SET_FX( get_qualities );

        DOC( "Get list of all required components" );
        luna::set_fx( ut, "get_components",
                      sol::resolve<const requirement_data::alter_item_comp_vector &() const>
                      ( &requirement_data::get_components ) );

        DOC( "Get a formatted list of all requirements" );
        SET_FX( list_all );

        DOC( "Get a formatted list of missing requirements" );
        SET_FX( list_missing );

        DOC( "Check if requirements can be made with given inventory" );
        luna::set_fx( ut, "can_make_with_inventory",
        []( const requirement_data & req, const inventory & inv ) -> bool {
            return req.can_make_with_inventory( inv, return_true<item>, 1 );
        } );

        DOC( "Multiply requirements by a scalar (e.g. for batch crafting)" );
        ut[sol::meta_function::multiplication] = []( const requirement_data & req, unsigned scalar ) -> requirement_data {
            return req * scalar;
        };

        reg_serde_functions( ut );
    }
#undef UT_CLASS

    // Global function to look up requirement by ID
    DOC( "Look up requirement_data by ID string. Returns nil if not found." );
    lua.set_function( "get_requirement", []( const std::string & id_str ) ->
    std::optional<requirement_data> {
        requirement_id id( id_str );
        if( id.is_valid() )
        {
            return *id;
        }
        return std::nullopt;
    } );
}
