#pragma once

#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"
#include "type_id.h"

#include <string>

namespace cata::detail
{

auto reg_game_ids_primary( sol::state &lua ) -> void;
auto reg_game_ids_traits( sol::state &lua ) -> void;
auto reg_game_ids_world( sol::state &lua ) -> void;
auto reg_game_ids_misc( sol::state &lua ) -> void;

template<typename T, bool do_int_id>
auto reg_id( sol::state &lua ) -> void
{
    using SID = string_id<T>;
    using IID = int_id<T>;
#define UT_CLASS SID
    {
        auto ut = [&lua]() {
            if constexpr( do_int_id ) {
                return luna::new_usertype<SID>( lua, luna::no_bases, luna::constructors <
                                                SID(),
                                                SID( const SID & ),
                                                SID( const IID & ),
                                                SID( std::string )
                                                > ()
                                              );
            } else {
                return luna::new_usertype<SID>( lua, luna::no_bases, luna::constructors <
                                                SID(),
                                                SID( const SID & ),
                                                SID( std::string )
                                                > ()
                                              );
            }
        }
        ();

        luna::set_fx( ut, "obj", []( const SID & sid ) -> const T* { return &sid.obj(); } );
        if constexpr( do_int_id ) {
            luna::set_fx( ut, "int_id", &SID::id );
            luna::set_fx( ut, "implements_int_id", []() -> bool { return true; } );
        } else {
            luna::set_fx( ut, "implements_int_id", []() -> bool { return false; } );
        }
        SET_FX( is_null );
        SET_FX( is_valid );
        luna::set_fx( ut, "str", &SID::c_str );
        SET_FX( NULL_ID );
        luna::set_fx( ut, sol::meta_function::to_string, []( const SID & id ) -> std::string {
            return string_format( "%s[%s]", luna::detail::luna_traits<SID>::name, id.c_str() );
        } );

        luna::set_fx( ut, "serialize", []( const SID & id, JsonOut & jsout ) -> void {
            jsout.write( id.str() );
        } );
        luna::set_fx( ut, "deserialize", []( SID & id, JsonIn & jsin ) -> void {
            id = SID( jsin.get_string() );
        } );
    }
#undef UT_CLASS

#define UT_CLASS IID
    if constexpr( do_int_id ) {
        auto ut = luna::new_usertype<IID>( lua, luna::no_bases, luna::constructors <
                                           IID(),
                                           IID( const IID & ),
                                           IID( const SID & )
                                           > () );

        luna::set_fx( ut, "obj", []( const IID & iid ) -> const T* { return &iid.obj(); } );
        luna::set_fx( ut, "str_id", &IID::id );
        SET_FX( is_valid );
        luna::set_fx( ut, sol::meta_function::to_string, []( const IID & id ) -> std::string {
            return string_format( "%s[%d][%s]", luna::detail::luna_traits<IID>::name, id.to_i(),
                                  id.is_valid() ? id.id().c_str() : "<invalid>" );
        } );
    }
#undef UT_CLASS
}

} // namespace cata::detail
