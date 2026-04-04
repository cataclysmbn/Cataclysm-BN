#include "catalua_threaded_hooks.h"

#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catalua_worker.h"
#include "debug.h"
#include "string_formatter.h"

#include <atomic>
#include <cstddef>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

struct threaded_hook_entry {
    uint64_t    pre_fn_id  = 0;
    uint64_t    post_fn_id = 0;  ///< 0 = isolated-writer (no post-pass)
    int         priority   = 0;
    std::string mod_id;
    std::vector<std::byte> pre_bytecode;
};

/// Global registry: hook_name → priority-sorted entries.
/// Written on the main thread during mod loading; read from worker threads during gameplay.
std::unordered_map<std::string, std::vector<threaded_hook_entry>> g_registry;
std::shared_mutex g_registry_mutex;

std::atomic<uint64_t> g_next_fn_id{ 1 };

/// Cached presence flag for on_mapgen_postprocess threaded hooks.
/// Written on main thread; read from worker threads via has_threaded_mapgen_hooks().
std::atomic<bool> g_has_threaded_mapgen_hooks{ false };

/// Dump a Lua function to bytecode via lua_dump.
/// Returns empty vector on failure (e.g. C function, unsupported closure).
auto dump_lua_fn( lua_State *L, const sol::protected_function &fn ) -> std::vector<std::byte>
{
    auto result = std::vector<std::byte>{};

    fn.push();  // pushes the function onto the Lua stack

    const int status = lua_dump(
        L,
        []( lua_State *, const void *data, size_t sz, void *ud ) -> int {
            auto *vec = static_cast<std::vector<std::byte> *>( ud );
            const auto *bytes = static_cast<const std::byte *>( data );
            vec->insert( vec->end(), bytes, bytes + sz );
            return 0;
        },
        &result,
        /*strip=*/0
    );

    lua_pop( L, 1 );  // pop the function that fn.push() left on the stack

    if( status != 0 ) {
        return {};
    }
    return result;
}

} // namespace

namespace cata
{

void define_threaded_hooks( lua_state &state )
{
    sol::state &lua = state.lua;

    // Ensure the internal post-function registry exists.
    sol::table it = lua.globals()["game"]["cata_internal"];
    it["threaded_hook_post_fns"] = lua.create_table();

    // Informational table — modders can read registered hook names from it.
    sol::table gt = lua.globals()["game"];
    gt["threaded_hooks"] = lua.create_table();

    gt["register_threaded_hook"] = [&lua]( const std::string & hook_name,
    const sol::table & entry ) {
        const auto mod_id   = entry.get<sol::optional<std::string>>( "mod_id" ).value_or( "<unknown>" );
        const auto priority = entry.get<sol::optional<int>>( "priority" ).value_or( 0 );
        const auto pre_fn_opt  = entry.get<sol::optional<sol::protected_function>>( "pre" );
        const auto fn_fn_opt   = entry.get<sol::optional<sol::protected_function>>( "fn" );

        sol::protected_function pre_fn;
        bool is_pre_post = false;

        if( pre_fn_opt ) {
            pre_fn      = *pre_fn_opt;
            is_pre_post = true;
        } else if( fn_fn_opt ) {
            pre_fn      = *fn_fn_opt;
            is_pre_post = false;
        } else {
            debugmsg( "register_threaded_hook '%s' (mod '%s'): entry must have a 'pre' or 'fn' function",
                      hook_name.c_str(), mod_id.c_str() );
            return;
        }

        auto bytecode = dump_lua_fn( lua.lua_state(), pre_fn );
        if( bytecode.empty() ) {
            debugmsg( "register_threaded_hook '%s' (mod '%s'): failed to serialize pre/fn to bytecode",
                      hook_name.c_str(), mod_id.c_str() );
            return;
        }

        const auto pre_fn_id = g_next_fn_id.fetch_add( 1, std::memory_order_relaxed );
        auto       post_fn_id = uint64_t{ 0 };

        if( is_pre_post ) {
            const auto post_fn_opt = entry.get<sol::optional<sol::protected_function>>( "post" );
            if( post_fn_opt ) {
                post_fn_id = g_next_fn_id.fetch_add( 1, std::memory_order_relaxed );
                sol::table post_fns = lua["game"]["cata_internal"]["threaded_hook_post_fns"];
                post_fns[post_fn_id] = *post_fn_opt;
            }
            // Post is optional — absence means pre-only (filter that never triggers a post-pass).
        }

        {
            std::unique_lock lock( g_registry_mutex );
            auto &entries = g_registry[hook_name];
            entries.emplace_back();
            auto &e       = entries.back();
            e.pre_fn_id   = pre_fn_id;
            e.post_fn_id  = post_fn_id;
            e.priority    = priority;
            e.mod_id      = mod_id;
            e.pre_bytecode = std::move( bytecode );
            std::ranges::stable_sort( entries, std::ranges::greater{},
                []( const threaded_hook_entry & e2 ) { return e2.priority; } );
        }
    };
}

auto intent_to_table( sol::state_view lua, const hook_intent &intent ) -> sol::table
{
    auto t = lua.create_table();
    std::ranges::for_each( intent, [&t]( const auto &kv ) {
        std::visit( [&t, &kv]( const auto &v ) {
            using V = std::decay_t<decltype( v )>;
            if constexpr( !std::is_same_v<V, std::monostate> ) {
                t[kv.first] = v;
            }
        }, kv.second );
    } );
    return t;
}

auto run_threaded_hook_pre(
    std::string_view hook_name,
    std::function<void( sol::table & )> init
) -> std::vector<hook_pre_result>
{
    auto results = std::vector<hook_pre_result>{};

    std::shared_lock lock( g_registry_mutex );
    const auto it = g_registry.find( std::string( hook_name ) );
    if( it == g_registry.end() ) {
        return results;
    }

    results.reserve( it->second.size() );

    std::ranges::for_each( it->second, [&]( const threaded_hook_entry &entry ) {
        auto intent = hook_intent{};
        const bool fired = cata::call_pre_fn_in_worker(
            pre_fn_call_opts{
                .fn_id      = entry.pre_fn_id,
                .bytecode   = &entry.pre_bytecode,
                .debug_name = string_format( "%s/%s", entry.mod_id, hook_name ),
            },
            init,
            intent
        );

        results.push_back( hook_pre_result{
            .run_post   = fired && entry.post_fn_id != 0,
            .post_fn_id = entry.post_fn_id,
            .mod_id     = entry.mod_id,
            .intent     = std::move( intent ),
        } );
    } );

    return results;
}

void run_threaded_hook_post(
    lua_state &global,
    const std::vector<hook_pre_result> &results,
    std::function<void( sol::table & )> init )
{
    sol::state &lua = global.lua;
    sol::table post_fns = lua["game"]["cata_internal"]["threaded_hook_post_fns"];

    std::ranges::for_each( results, [&]( const hook_pre_result &r ) {
        if( !r.run_post || r.post_fn_id == 0 ) {
            return;
        }

        const auto maybe_fn = post_fns.get<sol::optional<sol::protected_function>>( r.post_fn_id );
        if( !maybe_fn ) {
            debugmsg( "run_threaded_hook_post: post fn id %llu not found (mod '%s')",
                      r.post_fn_id, r.mod_id.c_str() );
            return;
        }

        auto params = lua.create_table();
        if( init ) {
            init( params );
        }
        auto intent_tbl = intent_to_table( lua, r.intent );

        auto res = ( *maybe_fn )( params, intent_tbl );
        if( !res.valid() ) {
            sol::error err = res;
            debugmsg( "run_threaded_hook_post: mod '%s' post error: %s",
                      r.mod_id.c_str(), err.what() );
        }
    } );
}

auto has_threaded_mapgen_hooks() -> bool
{
    return g_has_threaded_mapgen_hooks.load( std::memory_order_relaxed );
}

void refresh_threaded_mapgen_hook_presence()
{
    std::shared_lock lock( g_registry_mutex );
    const auto it = g_registry.find( "on_mapgen_postprocess" );
    g_has_threaded_mapgen_hooks.store(
        it != g_registry.end() && !it->second.empty(),
        std::memory_order_relaxed
    );
}

} // namespace cata
