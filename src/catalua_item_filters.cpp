#include "catalua_item_filters.h"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "catalua_sol.h"

#include "character.h"
#include "debug.h"
#include "flag.h"
#include "item.h"
#include "itype.h"
#include "profile.h"
#include "type_id.h"

using LuaValue = sol::basic_object<sol::basic_reference<>>;

namespace
{

template<typename T>
auto lua_value_to_id( const LuaValue &val ) -> T
{
    // unwrap into a plain object first: referenced objects can misreport type queries
    const sol::object obj = val;
    if( obj.get_type() == sol::type::string ) {
        return T( obj.as<std::string>() );
    }
    return obj.as<T>();
}

template<typename T>
auto lua_value_to_set( const LuaValue &val ) -> std::unordered_set<T>
{
    const sol::object obj = val;
    if( obj.get_type() == sol::type::table ) {
        std::unordered_set<T> out;
        for( auto &&[key, value] : obj.as<sol::table>() ) {
            out.insert( lua_value_to_id<T>( value ) );
        }
        return out;
    }
    return { lua_value_to_id<T>( obj ) };
}

const std::unordered_map<std::string, std::function<item_predicate( LuaValue &val )>>
handlers = {
    {
        "category", []( LuaValue & val ) -> item_predicate {
            const auto categories = lua_value_to_set<std::string>( val );
            return [categories]( const item & it ) -> bool { return categories.contains( it.get_category_id() ); };
        }
    },
    {
        "type", []( LuaValue & val ) -> item_predicate {
            const auto types = lua_value_to_set<itype_id>( val );
            return [types]( const item & it ) -> bool { return types.contains( it.typeId() ); };
        }
    },
    {
        "is_food", []( LuaValue & val ) -> item_predicate {
            const auto expected = val.as<bool>();
            return [expected]( const item & it ) -> bool { return it.is_food() == expected; };
        }
    },
    {
        "flags", []( LuaValue & val ) -> item_predicate {
            const auto flags = lua_value_to_set<flag_id>( val );
            return [flags]( const item & it ) -> bool {
                return std::ranges::any_of( flags, [&it]( const flag_id & flag ) { return it.has_flag( flag ); } );
            };
        }
    },
    {
        "need_charges", []( LuaValue & val ) -> item_predicate {
            const auto expected = val.as<bool>();
            return [expected]( const item & it ) -> bool {
                if( expected && it.is_tool() && it.type->tool->max_charges )
                {
                    return it.charges > 0;
                }
                return true;
            };
        }
    },
    {
        "has_charges", []( LuaValue & val ) -> item_predicate {
            const sol::object obj = val;
            auto tbl = obj.as<sol::table>();
            LuaValue id_val = tbl["id"];
            const auto id = lua_value_to_id<itype_id>( id_val );
            const auto amount = tbl["amount"].get<int>();
            return [id, amount]( const item & it ) -> bool {
                return it.typeId() == id && it.charges >= amount;
            };
        }
    },
};

} // namespace

auto item_filters_from_lua( const sol::table &filters,
                            const std::vector<std::string> &ignored_keys ) -> lua_item_filters
{
    ZoneScoped;
    lua_item_filters out;
    for( auto &&[key, value] : filters ) {
        const auto str_key = key.as<std::string>();
        if( std::ranges::any_of( ignored_keys, [&str_key]( const std::string & k ) { return k == str_key; } ) ) {
            continue;
        }
        if( str_key == "worn" || str_key == "wielded" ) {
            const auto expected = value.as<bool>();
            if( str_key == "worn" ) {
                out.worn = expected;
            } else {
                out.wielded = expected;
            }
            continue;
        }
        if( const auto it = handlers.find( str_key ); it != handlers.end() ) {
            out.predicates.push_back( it->second( value ) );
        } else {
            debugmsg( "Unknown filter %s", str_key.c_str() );
        }
    }
    return out;
}

auto character_item_predicates_from_lua( const Character &c, const sol::table &filters,
        const std::vector<std::string> &ignored_keys ) -> std::vector<item_predicate>
{
    auto parsed = item_filters_from_lua( filters, ignored_keys );
    auto predicates = std::move( parsed.predicates );
    if( parsed.worn ) {
        const auto expected = *parsed.worn;
        predicates.push_back( [&c, expected]( const item & it ) -> bool { return c.is_worn( it ) == expected; } );
    }
    if( parsed.wielded ) {
        const auto expected = *parsed.wielded;
        predicates.push_back( [&c, expected]( const item & it ) -> bool { return c.is_wielding( it ) == expected; } );
    }
    return predicates;
}

auto matches_item_filters( const lua_item_filters &filters, const item &it ) -> bool
{
    return std::ranges::all_of( filters.predicates, [&it]( const item_predicate & pred ) { return pred( it ); } );
}
