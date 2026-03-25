#pragma once

#include <fstream>
#include <sstream>

#include "catalua_impl.h"
#include "catalua_sol.h"

namespace proc_test
{

inline auto load_procgen_runtime( cata::lua_state &state ) -> void
{
    state.lua.open_libraries( sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string,
                              sol::lib::table );

    auto file = std::ifstream( "data/json/procgen.lua", std::ios::binary );
    REQUIRE( file.is_open() );

    auto script = std::ostringstream {};
    script << file.rdbuf();
    state.lua["procgen"] = state.lua.script( script.str() );
}

} // namespace proc_test
