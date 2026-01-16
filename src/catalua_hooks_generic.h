#pragma once

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "catalua_hooks.h"

#include "catalua_sol.h"

namespace cata
{

template<typename T = bool>
auto run_hooks( std::string_view hook_name,
                std::function < auto( sol::table &params ) -> void > init = nullptr,
hook_opts opts = {} ) -> std::optional<T> {
    if constexpr( std::is_same_v<T, bool> )
    {
        return cata::run_hooks( hook_name, std::move( init ), std::move( opts ) );
    } else
    {
        auto result = std::optional<T> {};
        lua_hooks_detail::run_hooks( hook_name, opts, init, [&]( const sol::object & res ) -> bool {
            if( res.is<T>() )
            {
                result = res.as<T>();
                return true;
            }
            return false;
        } );
        return result;
    }
}

} // namespace cata
