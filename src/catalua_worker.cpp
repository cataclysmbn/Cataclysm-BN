#include "catalua_worker.h"

#include "catalua_sol.h"
#include "debug.h"
#include "string_formatter.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace
{

/// Monotonically increasing generation counter.
/// Incremented by invalidate_worker_states(); worker states rebuild when their
/// cached generation doesn't match.
std::atomic<uint64_t> g_generation{ 1 };

struct worker_lua_state {
    sol::state lua;
    std::unordered_map<uint64_t, sol::protected_function> fn_cache;
    uint64_t generation = 0;
};

auto make_worker_lua_state() -> std::unique_ptr<worker_lua_state>
{
    auto ws = std::make_unique<worker_lua_state>();
    ws->lua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );
    ws->generation = g_generation.load( std::memory_order_relaxed );
    return ws;
}

auto get_or_create_worker_state() -> worker_lua_state &
{
    thread_local std::unique_ptr<worker_lua_state> tl_state;
    const auto cur_gen = g_generation.load( std::memory_order_relaxed );
    if( !tl_state || tl_state->generation != cur_gen ) {
        tl_state = make_worker_lua_state();
    }
    return *tl_state;
}

/// Deserialize a flat Lua table of primitives into a hook_intent.
/// Non-string keys and non-primitive values are silently skipped.
auto table_to_intent( const sol::table &t ) -> cata::hook_intent
{
    auto result = cata::hook_intent{};
    t.for_each( [&]( const sol::object &key, const sol::object &val ) {
        if( !key.is<std::string>() ) {
            return;
        }
        const auto k = key.as<std::string>();
        if( val.is<bool>() ) {
            result[k] = val.as<bool>();
        } else if( val.is<int>() ) {
            result[k] = val.as<int>();
        } else if( val.is<double>() ) {
            result[k] = val.as<double>();
        } else if( val.is<std::string>() ) {
            result[k] = val.as<std::string>();
        }
        // Non-primitive types silently ignored — documented constraint of threaded hooks.
    } );
    return result;
}

} // namespace

namespace cata
{

auto call_pre_fn_in_worker(
    const pre_fn_call_opts &opts,
    std::function<void( sol::table & )> init,
    hook_intent &result ) -> bool
{
    auto &ws = get_or_create_worker_state();

    // Load function from bytecode into cache on first use.
    auto it = ws.fn_cache.find( opts.fn_id );
    if( it == ws.fn_cache.end() ) {
        if( !opts.bytecode || opts.bytecode->empty() ) {
            debugmsg( "call_pre_fn_in_worker: no bytecode for fn_id %llu ('%s')",
                      opts.fn_id, opts.debug_name );
            return false;
        }
        auto *L = ws.lua.lua_state();
        const int status = luaL_loadbuffer(
            L,
            reinterpret_cast<const char *>( opts.bytecode->data() ),
            opts.bytecode->size(),
            std::string( opts.debug_name ).c_str()
        );
        if( status != 0 ) {
            const char *err_msg = lua_tostring( L, -1 );
            lua_pop( L, 1 );
            debugmsg( "call_pre_fn_in_worker: failed to load '%s': %s",
                      opts.debug_name, err_msg ? err_msg : "unknown error" );
            return false;
        }
        // Stack top is now the loaded function.  protected_function(L, idx) calls
        // lua_pushvalue + luaL_ref internally, leaving the original on the stack.
        sol::protected_function fn( L, lua_gettop( L ) );
        lua_pop( L, 1 );
        auto [ins_it, ok] = ws.fn_cache.emplace( opts.fn_id, std::move( fn ) );
        it = ins_it;
    }

    // Build params table and populate via caller-provided init.
    auto params = ws.lua.create_table();
    if( init ) {
        init( params );
    }

    auto res = it->second( params );
    if( !res.valid() ) {
        sol::error err = res;
        debugmsg( "call_pre_fn_in_worker '%s': runtime error: %s",
                  opts.debug_name, err.what() );
        return false;
    }

    auto retval = res.get<sol::object>();
    if( retval == sol::nil || retval.get_type() == sol::type::lua_nil ) {
        return false;
    }
    if( !retval.is<sol::table>() ) {
        debugmsg( "call_pre_fn_in_worker '%s': expected nil or table return, got %s",
                  opts.debug_name,
                  sol::type_name( ws.lua.lua_state(), retval.get_type() ) );
        return false;
    }

    result = table_to_intent( retval.as<sol::table>() );
    return true;
}

void invalidate_worker_states()
{
    g_generation.fetch_add( 1, std::memory_order_relaxed );
}

} // namespace cata
