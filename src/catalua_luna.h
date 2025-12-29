#pragma once

#include <array>
#include <functional>
#include <map>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "catalua_sol.h"
#include "catalua_readonly.h"
#include "debug.h"
#include "string_formatter.h"

#define LUNA_VAL( Class, Name )                         \
    namespace luna::detail {                            \
    template<>                                          \
    struct luna_traits<Class> {                         \
        constexpr static bool impl = true;              \
        constexpr static std::string_view name = Name;  \
    };                                                  \
    } // namespace luna::detail

#define LUNA_DOC( Class, Name ) LUNA_VAL( Class, Name )

#define LUNA_ID( Class, Name )                  \
    LUNA_DOC( Class, Name "Raw" )               \
    LUNA_VAL( string_id<Class>, Name "Id" )     \
    LUNA_VAL( int_id<Class>, Name "IntId" )

#define LUNA_ENUM( Class, Name ) LUNA_VAL( Class, Name )

namespace luna
{

struct no_bases_t {};
constexpr static no_bases_t no_bases;

template<typename ...Args>
using bases = sol::bases<Args...>;

constexpr sol::no_construction no_constructor;

template<typename ...Args>
using constructors = sol::constructors<Args...>;

namespace detail
{

constexpr std::string_view KEY_TYPES = "#types";
constexpr std::string_view KEY_ENUMS = "#enums";
constexpr std::string_view KEY_LIBS = "#libs";
constexpr std::string_view KEY_LIB_IMPL = "#lib_impl";
constexpr std::string_view KEY_LIB_COMMENT = "lib_comment";
constexpr std::string_view KEY_ENUM_ENTRIES = "entries";
constexpr std::string_view KEY_TYPE_IMPL = "#type_impl";
constexpr std::string_view KEY_DOCTABLE = "catadoc";
constexpr std::string_view KEY_BASES = "#bases";
constexpr std::string_view KEY_CONSTRUCT = "#construct";
constexpr std::string_view KEY_MEMBER = "#member";
constexpr std::string_view KEY_MEMBER_TYPE = "type";
constexpr std::string_view KEY_MEMBER_COMMENT = "comment";
constexpr std::string_view KEY_MEMBER_VARIABLE_TYPE = "vartype";
constexpr std::string_view KEY_MEMBER_VARIABLE_HAS_VALUE = "hasval";
constexpr std::string_view KEY_MEMBER_VARIABLE_VALUE = "varval";
constexpr std::string_view KEY_MEMBER_ARGS = "args";
constexpr std::string_view KEY_MEMBER_OVERLOADS = "overloads";
constexpr std::string_view KEY_MEMBER_RETVAL = "retval";
constexpr std::string_view KEY_MEMBER_NAME = "name";
constexpr std::string_view KEY_GET_TYPE = "get_luna_type";
constexpr std::string_view KEY_USERTYPE_COMMENT = "type_comment";

constexpr std::string_view MEMBER_IS_VAR = "var";
constexpr std::string_view MEMBER_IS_FUNC = "func";
constexpr std::string_view MEMBER_IS_CONST_FUNC = "const_func";

template<typename T>
struct luna_traits {
    constexpr static bool impl = false;
    constexpr static std::string_view name = "<value>";
};

extern std::string_view current_comment;

template<typename T>
using remove_cv_ref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename Signature>
using fx_traits = sol::meta::meta_detail::fx_traits<Signature>;

inline void add_comment( sol::table &dt, std::string_view key )
{
    if( !current_comment.empty() ) {
        dt[key] = current_comment;
        current_comment = "";
    }
}

template<typename Val>
std::string doc_value_impl()
{
    //static_assert(luna_traits<Val>::impl, "Type must implement luna_traits" );
    if constexpr( luna_traits<Val>::impl ) {
        return std::string( luna_traits<Val>::name );
    } else {
        using ValNoPtr = std::remove_pointer_t<Val>;
        using ValBare = remove_cv_ref_t<ValNoPtr>;
        if constexpr( luna_traits<ValBare>::impl ) {
            return std::string( luna_traits<ValBare>::name );
        } else {
            return std::string( "<cppval: " ) + typeid( ValBare ).name() + " >";
        }
    }
}

template<typename Val>
std::string doc_value( sol::types<Val> );

template<typename ...Args>
std::string doc_value( sol::types<std::tuple<Args...>> )
{
    std::string ret = "(";
    bool is_first = true;
    ( [&]() {
        if( is_first ) {
            is_first = false;
        } else {
            ret += ",";
        }
        ret += " ";
        ret += doc_value( sol::types<Args>() );
    }
    (), ... );
    if( !is_first ) {
        ret += " ";
    }
    return ret + ")";
}

template<typename Val>
std::string doc_value( sol::types<sol::optional<Val>> )
{
    std::string ret = "Opt( ";
    ret += doc_value( sol::types<Val>() );
    return ret + " )";
}

template<typename Val, std::size_t N>
std::string doc_value( sol::types<std::array<Val, N>> )
{
    std::string ret = "Array( ";
    ret += doc_value( sol::types<Val>() );
    ret += ", ";
    ret += std::to_string( N );
    return ret + " )";
}

template<typename Val>
std::string doc_value( sol::types<std::vector<Val>> )
{
    std::string ret = "Vector( ";
    ret += doc_value( sol::types<Val>() );
    return ret + " )";
}

template<typename Val>
std::string doc_value( sol::types<std::set<Val>> )
{
    std::string ret = "Set( ";
    ret += doc_value( sol::types<Val>() );
    return ret + " )";
}

template<typename Key, typename Val>
std::string doc_value( sol::types<std::map<Key, Val>> )
{
    std::string ret = "Map( ";
    ret += doc_value( sol::types<Key>() );
    ret += ", ";
    ret += doc_value( sol::types<Val>() );
    return ret + " )";
}

template<typename Val>
std::string doc_value( sol::types<Val> )
{
    return doc_value_impl<Val>();
}

template<typename ...Args>
std::vector<std::string> doc_arg_list()
{
    std::vector<std::string> ret;

    ( (
          ret.push_back(
              doc_value( sol::types<Args>() ) )
      ), ... );

    return ret;
}

template<typename RetVal, typename ...Args>
std::vector<std::string> doc_one_constructor( std::function<RetVal( Args... )> )
{
    return doc_arg_list<Args...>();
}

template<typename ...Args>
void doc_constructors( sol::table &dt, const sol::constructor_list<Args...> & )
{
    std::vector<std::vector<std::string>> ctors;

    ( (
          ctors.push_back( doc_one_constructor( std::function<Args>( nullptr ) ) )
      ), ... );

    dt[KEY_CONSTRUCT] = ctors;
}

inline void doc_constructors( sol::table &dt, const sol::no_construction & )
{
    std::vector<std::vector<std::string>> ctors;
    dt[KEY_CONSTRUCT] = ctors;
}

template<typename ...Args>
void doc_bases( sol::table &dt, const sol::bases<Args...> & )
{
    std::vector<std::string> bases;

    ( (
          bases.push_back( doc_value( sol::types<Args>() ) )
      ), ... );

    dt[KEY_BASES] = bases;
}

inline void doc_bases( sol::table &dt, const no_bases_t & )
{
    std::vector<std::string> ctors;
    dt[KEY_BASES] = ctors;
}

inline sol::table get_global_doctable( sol::state_view &lua )
{
    sol::object obj = lua[detail::KEY_DOCTABLE];
    if( obj.valid() ) {
        return obj;
    } else {
        sol::table dt = lua.create_table();
        dt[detail::KEY_TYPES] = lua.create_table();
        dt[detail::KEY_ENUMS] = lua.create_table();
        dt[detail::KEY_LIBS] = lua.create_table();
        lua[detail::KEY_DOCTABLE] = dt;
        return dt;
    }
}

template<typename Class>
inline sol::table get_type_doctable( sol::state_view &lua )
{
    static_assert( detail::luna_traits<Class>::impl, "Type must implement luna_traits<T>" );

    sol::table gdt = get_global_doctable( lua );
    return gdt[detail::KEY_TYPES][detail::luna_traits<Class>::name];
}

template<typename Class>
inline sol::table get_enum_doctable( sol::state_view &lua )
{
    static_assert( detail::luna_traits<Class>::impl, "Type must implement luna_traits<T>" );

    sol::table gdt = get_global_doctable( lua );
    return gdt[detail::KEY_ENUMS][detail::luna_traits<Class>::name];
}

template<typename Class, typename Value>
void doc_member( sol::table &dt, sol::types<Value Class::*> && )
{
    dt[KEY_MEMBER_TYPE] = MEMBER_IS_VAR;
    add_comment( dt, KEY_MEMBER_COMMENT );
    dt[KEY_MEMBER_VARIABLE_TYPE] = doc_value( sol::types<Value>() );
}

// Olanti! Curse thee for what I must do!
// NOTE: This only works with read-only properties (for now).
// It also has some pretty significant issues with intuiting the type of the
// property it's working with.
// TODO: Resolve these issues.
template<typename GetClass, typename GetVal>
void doc_member( sol::table &dt,
                 sol::types<sol::property_wrapper<GetVal GetClass::*, sol::detail::no_prop>> && )
{
    dt[KEY_MEMBER_TYPE] = MEMBER_IS_VAR;
    add_comment( dt, KEY_MEMBER_COMMENT );
    /* TODO: Why does this work HERE but not when it's run in doc_value_impl?!
     * This may prove problematic in the future. Implementing luna_traits might
     * help avert it for certain types, but I would much prefer the root problem
     * solved.
     */
    dt[KEY_MEMBER_VARIABLE_TYPE] = doc_value( sol::types<std::remove_const<GetVal>>() );
}

template<typename Class, typename Value>
void doc_member( sol::table &dt, sol::types<sol::readonly_wrapper<Value Class::*>> && )
{
    dt[KEY_MEMBER_TYPE] = MEMBER_IS_VAR;
    add_comment( dt, KEY_MEMBER_COMMENT );
    dt[KEY_MEMBER_VARIABLE_TYPE] = doc_value( sol::types<Value>() );
}

template<typename Class, bool add_self_arg, typename RetVal, typename ...Args>
void doc_member_fx_impl2( sol::table &dt, sol::types<RetVal> &&, sol::types<Args...> && )
{
    dt[KEY_MEMBER_RETVAL] = doc_value( sol::types<RetVal>() );
    if constexpr( add_self_arg ) {
        dt[KEY_MEMBER_ARGS] = doc_arg_list<Class, Args...>();
    } else {
        dt[KEY_MEMBER_ARGS] = doc_arg_list<Args...>();
    }
}

template<typename Class, typename Function>
void doc_member_fx_overload( sol::table &dt, std::vector<sol::table> &overloads )
{
    sol::state_view lua( dt.lua_state() );
    sol::table overload = lua.create_table();
    overloads.push_back( overload );
    using RetVal = typename fx_traits<Function>::return_type;
    using Args = typename fx_traits<Function>::args_list;
    constexpr bool add_self_arg = !fx_traits<Function>::is_member_function;
    doc_member_fx_impl2<Class, add_self_arg>( overload, sol::types<RetVal>(), Args() );
}

template<typename Class, typename ...Functions>
void doc_member_fx_impl( sol::table &dt, sol::types<Functions...> && )
{
    dt[KEY_MEMBER_TYPE] = MEMBER_IS_FUNC;
    add_comment( dt, KEY_MEMBER_COMMENT );
    std::vector<sol::table> overloads;

    ( [&]() {
        doc_member_fx_overload<Class, Functions>( dt, overloads );
    }
    (), ... );
    dt[KEY_MEMBER_OVERLOADS] = overloads;
}

template<typename Class, typename ...Functions>
void doc_member_fx( sol::table &dt, sol::types<sol::overload_set<Functions...>> && )
{
    doc_member_fx_impl<Class>( dt, sol::types<Functions...>() );
}

template<typename Class, typename Func>
void doc_member_fx( sol::table &dt, sol::types<Func> && )
{
    doc_member_fx_impl<Class>( dt, sol::types<Func>() );
}

template<typename Value>
void doc_free( sol::table &dt, Value val )
{
    dt[KEY_MEMBER_TYPE] = MEMBER_IS_VAR;
    add_comment( dt, KEY_MEMBER_COMMENT );
    dt[KEY_MEMBER_VARIABLE_TYPE] = doc_value( sol::types<Value>() );
    dt[KEY_MEMBER_VARIABLE_HAS_VALUE] = true;
    dt[KEY_MEMBER_VARIABLE_VALUE] = val;
}

template<typename RetVal, typename ...Args>
void doc_free_fx_impl2( sol::table &dt, sol::types<RetVal> &&, sol::types<Args...> && )
{
    dt[KEY_MEMBER_RETVAL] = doc_value( sol::types<RetVal>() );
    dt[KEY_MEMBER_ARGS] = doc_arg_list<Args...>();
}

template<typename Function>
void doc_free_fx_overload( sol::table &dt, std::vector<sol::table> &overloads )
{
    sol::state_view lua( dt.lua_state() );
    sol::table overload = lua.create_table();
    overloads.push_back( overload );
    using RetVal = typename fx_traits<Function>::return_type;
    using Args = typename fx_traits<Function>::args_list;
    doc_free_fx_impl2( overload, sol::types<RetVal>(), Args() );
}

template<typename ...Functions>
void doc_free_fx_impl( sol::table &dt, sol::types<Functions...> && )
{
    dt[KEY_MEMBER_TYPE] = MEMBER_IS_FUNC;
    add_comment( dt, KEY_MEMBER_COMMENT );
    std::vector<sol::table> overloads;
    ( [&]() {
        doc_free_fx_overload<Functions>( dt, overloads );
    }
    (), ... );
    dt[KEY_MEMBER_OVERLOADS] = overloads;
}

template<typename ...Functions>
void doc_free_fx( sol::table &dt, sol::types<sol::overload_set<Functions...>> && )
{
    doc_free_fx_impl( dt, sol::types<Functions...>() );
}

template<typename Func>
void doc_free_fx( sol::table &dt, sol::types<Func> && )
{
    doc_free_fx_impl( dt, sol::types<Func>() );
}

template<typename Key>
sol::table make_type_member_doctable( sol::table type_dt, const Key &key )
{
    sol::state_view lua( type_dt.lua_state() );
    std::vector<sol::object> &members = type_dt[ detail::KEY_MEMBER ];
    sol::table member_dt = lua.create_table();
    member_dt[detail::KEY_MEMBER_NAME] = key;
    members.push_back( member_dt );
    return member_dt;
}

} // namespace detail

template<typename Class, typename ConstructorScheme, typename Bases>
sol::usertype<Class> new_usertype(
    sol::state_view &lua,
    Bases &&bases,
    ConstructorScheme &&constructor
)
{
    static_assert( detail::luna_traits<Class>::impl, "Type must implement luna_traits<T>" );

    // Ensure global doctable exists
    sol::table global_dt = detail::get_global_doctable( lua );

    constexpr std::string_view name = detail::luna_traits<Class>::name;

    // Register Sol usertype
    sol::usertype<Class> ut;
    using BasesBare = detail::remove_cv_ref_t<Bases>;
    if constexpr( std::is_same_v<BasesBare, no_bases_t> ) {
        ut = lua.new_usertype<Class>( name, constructor );
    } else {
        ut = lua.new_usertype<Class>( name, constructor, sol::base_classes, bases );
    }

    // Create doctable for this type
    sol::table type_dt = lua.create_table();
    global_dt[detail::KEY_TYPES][name] = type_dt;

    // Link relevant Sol usertype in docs
    type_dt[detail::KEY_TYPE_IMPL] = ut;

    // Doc comment
    detail::add_comment( type_dt, detail::KEY_USERTYPE_COMMENT );

    // Init members table
    type_dt[detail::KEY_MEMBER] = std::vector<sol::object>();

    // Document constructors (or lack thereof)
    detail::doc_constructors( type_dt, constructor );

    // Document bases (or lack thereof)
    detail::doc_bases( type_dt, bases );

    // Add helper method to get name under which usertype is bound in Lua
    ut[detail::KEY_GET_TYPE] = []() -> std::string_view {
        return detail::luna_traits<Class>::name;
    };

    return ut;
}

template<typename Class, typename Key, typename Func >
void set_fx(
    sol::usertype<Class> &ut,
    Key &&key,
    Func value
)
{
    // Due to a bug in sol2, on GCC build protected function call may call wrong lambda
    // https://github.com/ThePhD/sol2/issues/1444
    // This happens if we register with table.set( key, func ), but for
    // some reason table[key] = func makes it work fine.
    ut[ key ] = std::forward<Func>( value );

    sol::state_view lua( ut.lua_state() );
    sol::table type_dt = detail::get_type_doctable<Class>( lua );
    sol::table member_dt = detail::make_type_member_doctable( type_dt, key );
    detail::doc_member_fx<Class>( member_dt, sol::types<Func>() );
}

template<typename Class, typename Key, typename Value>
void set(
    sol::usertype<Class> &ut,
    Key &&key,
    Value &&value
)
{
    ut[ key ] = std::forward<Value>( value );

    sol::state_view lua( ut.lua_state() );
    sol::table type_dt = detail::get_type_doctable<Class>( lua );
    sol::table member_dt = detail::make_type_member_doctable( type_dt, key );
    detail::doc_member<Class>( member_dt, sol::types<Value>() );
}

template<typename E>
struct userenum {
    sol::table t;
    bool finalized = false;

    ~userenum() {
        if( !finalized ) {
            debugmsg( "Userenum<%s> has not been finalized!", detail::luna_traits<E>::name );
            std::abort();
        }
    }
};

template<typename Enum>
userenum<Enum> begin_enum(
    sol::state_view &lua
)
{
    static_assert( detail::luna_traits<Enum>::impl, "Type must implement luna_traits<T>" );

    sol::table ut = lua.create_table();
    return userenum<Enum> { ut };
}

template<typename Enum, typename Key>
void add_val(
    userenum<Enum> &e,
    const Key &key,
    const Enum &value
)
{
    e.t[key] = value;
}

template<typename Enum>
void finalize_enum(
    userenum<Enum> &e
)
{
    sol::state_view lua( e.t.lua_state() );
    constexpr std::string_view name = detail::luna_traits<Enum>::name;

    // Ensure global doctable exists
    sol::table global_dt = detail::get_global_doctable( lua );

    // Create doctable for this enum
    sol::table enum_dt = lua.create_table();
    global_dt[detail::KEY_ENUMS][name] = enum_dt;

    // Link to original list of entries
    enum_dt[detail::KEY_ENUM_ENTRIES] = e.t;

    // Make read-only so Lua code doesn't mess with it
    sol::table et = cata::make_readonly_table(
                        lua, e.t,
                        string_format( "Tried to modify enum table %s.", name )
                    );
    lua.globals()[name] = et;
    e.finalized = true;
}

struct userlib {
    sol::table t;
    std::string_view name;
    sol::table dt;
    bool finalized = false;

    ~userlib() {
        if( !finalized ) {
            debugmsg( "Userlib has not been finalized!" );
            std::abort();
        }
    }
};

inline userlib begin_lib(
    sol::state_view &lua,
    std::string_view name
)
{
    // Ensure global doctable exists
    sol::table global_dt = detail::get_global_doctable( lua );

    // Create doctable for this lib
    sol::table lib_dt = lua.create_table();
    global_dt[detail::KEY_LIBS][name] = lib_dt;

    // Init members table
    lib_dt[detail::KEY_MEMBER] = std::vector<sol::object>();

    // Library comment
    detail::add_comment( lib_dt, detail::KEY_LIB_COMMENT );

    // Create library itself
    sol::table t = lua.create_table();

    // Link to original library from docs
    lib_dt[detail::KEY_LIB_IMPL] = t;

    return userlib { t, name, lib_dt };
}

template<typename Key, typename Func >
void set_fx(
    userlib &lib,
    Key &&key,
    Func value
)
{
    // Due to a bug in sol2, on GCC build protected function call may call wrong lambda
    // https://github.com/ThePhD/sol2/issues/1444
    // This happens if we register with table.set( key, func ), but for
    // some reason table[key] = func makes it work fine.
    lib.t[ key ] = std::forward<Func>( value );

    sol::state_view lua( lib.t.lua_state() );
    sol::table member_dt = detail::make_type_member_doctable( lib.dt, key );
    detail::doc_free_fx( member_dt, sol::types<Func>() );
}

template<typename Key, typename Value>
void set(
    userlib &lib,
    Key &&key,
    Value &&value
)
{
    lib.t[ key ] = value;

    sol::state_view lua( lib.t.lua_state() );
    sol::table member_dt = detail::make_type_member_doctable( lib.dt, key );
    detail::doc_free( member_dt, value );
}

inline void finalize_lib(
    userlib &lib
)
{
    sol::state_view lua( lib.t.lua_state() );

    // Make read-only so Lua code doesn't mess with it
    sol::table et = cata::make_readonly_table(
                        lua, lib.t,
                        string_format( "Tried to modify library %s.", lib.name )
                    );
    lua.globals()[lib.name] = et;
    lib.finalized = true;
}

inline void doc( std::string_view doc )
{
    detail::current_comment = doc;
}

} // namespace luna


