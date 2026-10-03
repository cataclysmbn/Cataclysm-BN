#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "inventory.h"
#include "itype.h"
#include "requirements.h"

void cata::detail::reg_requirement( sol::state &lua )
{
#define UT_CLASS item_comp
    {
        DOC( "A component consumed by a crafting requirement" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        // type/count/recoverable live in the `component` base struct, so expose them as properties.
        DOC( "Item type of the component" );
        luna::set_prop( ut, "type", []( const UT_CLASS & c ) -> itype_id { return c.type; } );
        DOC( "Amount needed (items, or charges for items counted by charges)" );
        luna::set_prop( ut, "count", []( const UT_CLASS & c ) -> int { return c.count; } );
        DOC( "Whether the component can be recovered by disassembly" );
        luna::set_prop( ut, "recoverable", []( const UT_CLASS & c ) -> bool { return c.recoverable; } );
    }
#undef UT_CLASS

#define UT_CLASS tool_comp
    {
        DOC( "A tool used by a crafting requirement" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        DOC( "Item type of the tool" );
        luna::set_prop( ut, "type", []( const UT_CLASS & c ) -> itype_id { return c.type; } );
        DOC( "Charges needed, or -1 if the tool is not used up by charges" );
        luna::set_prop( ut, "count", []( const UT_CLASS & c ) -> int { return c.count; } );
        DOC( "Whether the tool needs charges" );
        SET_FX( by_charges );
    }
#undef UT_CLASS

#define UT_CLASS quality_requirement
    {
        DOC( "A tool quality needed by a crafting requirement" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        DOC( "Quality type" );
        SET_MEMB_RO( type );
        DOC( "Number of tools with this quality needed" );
        SET_MEMB_RO( count );
        DOC( "Minimum quality level" );
        SET_MEMB_RO( level );
    }
#undef UT_CLASS

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

    DOC( "Requirement definitions and lookup helpers." );
    luna::userlib lib = luna::begin_lib( lua, "requirements" );

    DOC( "Look up requirement_data by ID string. Returns nil if not found." );
    luna::set_fx( lib, "get", []( const std::string & id_str ) -> std::optional<requirement_data> {
        requirement_id id( id_str );
        if( id.is_valid() )
        {
            return *id;
        }
        return std::nullopt;
    } );

    luna::finalize_lib( lib );
}
