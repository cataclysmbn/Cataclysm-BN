#include <vector>
#include <string>
#include <ranges>
#include <algorithm>
#include "catalua_bindings.h"

#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_log.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"

#include "itype.h"
#include "skill.h"

void cata::detail::reg_recipe( sol::state &lua )
{
#define UT_CLASS recipe
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        SET_MEMB( category );
        SET_MEMB( subcategory );
        SET_MEMB( time );
        SET_MEMB( skill_used );
        SET_MEMB( difficulty );
        SET_MEMB( required_skills );
        SET_MEMB( learn_by_disassembly );
        SET_MEMB( booksets );

        DOC( "DEPRECATED: use recipe_id instead" );
        SET_FX_T( ident, const recipe_id & () const );

        SET_FX_N( ident, "recipe_id" );
        SET_FX( result );
        SET_FX( result_name );
        SET_FX( has_flag );

        DOC( "Returns the tools, qualities and components needed to craft this recipe once. "
             "Alternatives are listed in each group, and requirements shared between recipes are included." );
        luna::set_fx( ut, "get_requirements", []( const recipe & r ) -> const requirement_data & {
            return r.simple_requirements();
        } );

        namespace views = std::views;
        namespace ranges = std::ranges;

        luna::set_fx( ut, "get_from_skill_used", []( const skill_id & sk ) -> std::vector<recipe> {
            return recipe_dict
            | views::values
            | views::filter( [&]( const recipe & r ) { return r.skill_used == sk; } )
            | ranges::to<std::vector<recipe>>();
        } );
        luna::set_fx( ut, "get_from_flag", []( const std::string & flag_name ) -> std::vector<recipe> {
            return recipe_dict
            | views::values
            | views::filter( [&]( const recipe & r ) { return r.has_flag( flag_name ); } )
            | ranges::to<std::vector<recipe>>();
        } );
        DOC( "Returns all recipes whose result is the given item type." );
        luna::set_fx( ut, "get_from_result", []( const itype_id & result ) -> std::vector<recipe> {
            return recipe_dict
            | views::values
            | views::filter( [&]( const recipe & r ) { return r.result() == result; } )
            | ranges::to<std::vector<recipe>>();
        } );
        luna::set_fx( ut, "get_all", []() -> std::vector<recipe> { return recipe_dict | views::values | ranges::to<std::vector<recipe>>(); } );
    }
#undef UT_CLASS // #define UT_CLASS recipe

#define UT_CLASS book_recipe
    {
        auto ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB( name );
        SET_MEMB( skill_level );
        SET_MEMB( recipe );
        SET_MEMB( hidden );
    }
#undef UT_CLASS
}
