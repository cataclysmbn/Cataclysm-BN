#include "catalua_item_filters.h"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "catalua_sol.h"

#include "debug.h"
#include "flag.h"
#include "item.h"
#include "profile.h"
#include "type_id.h"

using LuaValue = sol::basic_object<sol::basic_reference<>>;
using ItemPredicate = item_predicate;

namespace
{

template<typename T>
auto lua_value_to_id( LuaValue &val ) -> T
{
    if( val.is<std::string>() ) {
        return T( val.as<std::string>() );
    }
    return val.as<T>();
}

template<typename T>
auto lua_value_to_set( LuaValue &val ) -> std::unordered_set<T>
{
    if( val.is<sol::table>() ) {
        std::unordered_set<T> out;
        for( auto &&[key, value] : val.as<sol::table>() ) {
            out.insert( lua_value_to_id<T>( value ) );
        }
        return out;
    }
    return { lua_value_to_id<T>( val ) };
}

const std::unordered_map<std::string, std::function<ItemPredicate( LuaValue &val )>>
handlers = {
    {
        "category", []( LuaValue & val ) -> ItemPredicate {
            const auto categories = lua_value_to_set<std::string>( val );
            return [categories]( const item & it ) -> bool { return categories.contains( it.get_category_id() ); };
        }
    },
    {
        "type", []( LuaValue & val ) -> ItemPredicate {
            const auto types = lua_value_to_set<itype_id>( val );
            return [types]( const item & it ) -> bool { return types.contains( it.typeId() ); };
        }
    },
    {
        "is_food", []( LuaValue & val ) -> ItemPredicate {
            const auto expected = val.as<bool>();
            return [expected]( const item & it ) -> bool { return it.is_food() == expected; };
        }
    },
    {
        "flags", []( LuaValue & val ) -> ItemPredicate {
            const auto flags = lua_value_to_set<flag_id>( val );
            return [flags]( const item & it ) -> bool {
                return std::ranges::any_of( flags, [&it]( const flag_id & flag ) { return it.has_flag( flag ); } );
            };
        }
    },
};

} // namespace

auto item_filters_from_lua( const sol::table &filters ) -> std::vector<ItemPredicate>
{
    ZoneScoped;
    std::vector<ItemPredicate> predicates;
    for( auto &&[key, value] : filters ) {
        const auto str_key = key.as<std::string>();
        if( const auto it = handlers.find( str_key ); it != handlers.end() ) {
            predicates.push_back( it->second( value ) );
        } else {
            debugmsg( "Unknown filter %s", str_key.c_str() );
        }
    }
    return predicates;
}
