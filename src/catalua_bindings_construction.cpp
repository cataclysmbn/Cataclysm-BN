#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "construction.h"
#include "construction_group.h"
#include "requirements.h"
#include "skill.h"

void cata::detail::reg_construction( sol::state &lua )
{
#define UT_CLASS construction
    {
        DOC( "A construction from the construction menu (building, digging, deconstructing...)" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        SET_MEMB_RO( id );

        DOC( "Terrain needed before building, or an empty id if any terrain works" );
        SET_MEMB_RO( pre_terrain );
        DOC( "Furniture needed before building, or an empty id if any furniture works" );
        SET_MEMB_RO( pre_furniture );
        DOC( "Terrain after building, or an empty id if the terrain does not change" );
        SET_MEMB_RO( post_terrain );
        DOC( "Furniture after building, or an empty id if the furniture does not change" );
        SET_MEMB_RO( post_furniture );

        DOC( "Skills and levels needed to build" );
        SET_MEMB_RO( required_skills );
        DOC( "Base time to build" );
        SET_MEMB_RO( time );

        DOC( "Id of the construction group (the entry shown in the construction menu)" );
        luna::set_fx( ut, "group_id", []( const UT_CLASS & c ) -> std::string {
            return c.group.str();
        } );
        DOC( "Translated name of the construction group" );
        luna::set_fx( ut, "group_name", []( const UT_CLASS & c ) -> std::string {
            return c.group.is_valid() ? c.group->name() : std::string();
        } );

        DOC( "Returns the tools, qualities and components needed to build this once, "
             "including requirements shared through \"using\"." );
        luna::set_fx( ut, "get_requirements", []( const UT_CLASS & c ) -> const requirement_data & {
            return c.requirements.obj();
        } );

        DOC( "Returns the ids of all constructions, in construction menu order." );
        luna::set_fx( ut, "get_all", []() -> std::vector<construction_str_id> {
            std::vector<construction_str_id> ret;
            for( const construction_id &id : constructions::get_all_sorted() )
            {
                ret.push_back( id.id() );
            }
            return ret;
        } );
    }
#undef UT_CLASS
}
