#pragma once

#include <filesystem>

#include "catalua_impl.h"
#include "catalua_loader.h"
#include "init.h"

namespace proc_test
{

inline auto load_procgen_runtime( cata::lua_state &state ) -> void
{
    state.lua = make_lua_state();
    auto game = state.lua.create_table();
    game["current_mod_path"] = std::filesystem::absolute( "data/json" ).string();
    state.lua["game"] = game;
    const auto path = std::filesystem::absolute( "data/json/main.lua" );
    const auto guard = cata::lua_loader::script_context_guard { path };
    auto require_fn = state.lua["require"].get<sol::protected_function>();
    auto exec_res = require_fn( "./procgen" );
    check_func_result( exec_res );
    REQUIRE( exec_res.valid() );
    state.lua["procgen"] = exec_res.get<sol::object>();
}

struct scoped_loader_lua {
    std::unique_ptr<cata::lua_state, cata::lua_state_deleter> prev;

    scoped_loader_lua() {
        auto fresh = std::unique_ptr<cata::lua_state, cata::lua_state_deleter>( new cata::lua_state );
        load_procgen_runtime( *fresh );
        prev = std::move( DynamicDataLoader::get_instance().lua );
        DynamicDataLoader::get_instance().lua = std::move( fresh );
    }

    ~scoped_loader_lua() {
        DynamicDataLoader::get_instance().lua = std::move( prev );
    }
};

} // namespace proc_test
