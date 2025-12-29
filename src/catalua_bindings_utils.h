#pragma once

#include "catalua_luna.h"
#include "json.h"

template<typename T>
void reg_serde_functions( sol::usertype<T> &ut )
{
    luna::set_fx( ut, "serialize", sol::resolve< void( JsonOut & ) const >( &T::serialize ) );
    luna::set_fx( ut, "deserialize", sol::resolve< void( JsonIn & ) >( &T::deserialize ) );
}

#define DOC( x ) luna::doc( x )

// Utility macros for binding C++ to Lua for a predefined C++ class.
// Use by defining UT_CLASS in a code block. Once done, undefine UT_CLASS.
// Example use is in catalua_bindings_creature.cpp
// SET MEMBer
#define SET_MEMB(prop_name) luna::set( ut, #prop_name, &UT_CLASS::prop_name )
// SET MEMBer with Name
//#define SET_MEMB_N(prop_name, lua_name_str) luna::set( ut, lua_name_str, &UT_CLASS::prop_name )
// SET MEMBer, Read-Only
#define SET_MEMB_RO(prop_name) luna::set( ut, #prop_name, sol::readonly( &UT_CLASS::prop_name ) )
// SET MEMBer with Name, Read-Only
#define SET_MEMB_N_RO(prop_name, lua_name_str) luna::set( ut, lua_name_str, sol::readonly( &UT_CLASS::prop_name ) )
// SET FX (function)
#define SET_FX(func_name) luna::set_fx ( ut, #func_name, &UT_CLASS::func_name)
// SET FX (function) with Type
#define SET_FX_T(func_name, func_type) luna::set_fx( ut, #func_name, \
        sol::resolve< func_type >( &UT_CLASS::func_name))
// SET FX (function) with Name
//#define SET_FX_N(func_name, lua_name_str) luna::set_fx ( ut, lua_name_str, &UT_CLASS::func_name)
// SET FX (function) with Name and Type
#define SET_FX_N_T(func_name, lua_name_str, func_type) luna::set_fx( ut, lua_name_str, \
        sol::resolve< func_type >( &UT_CLASS::func_name))


