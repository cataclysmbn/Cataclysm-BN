#pragma once

#include "catalua_sol_fwd.h"
#include "catalua_threaded_hook_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace cata
{

/// Opaque per-thread Lua state for worker threads.
/// Defined privately in catalua_worker.cpp.
struct worker_lua_state;

/// Options for calling a pre/fn function in a worker state.
struct pre_fn_call_opts {
    uint64_t                    fn_id      = 0;
    const std::vector<std::byte> *bytecode = nullptr;
    std::string_view            debug_name;
};

/// Call a Lua function in the calling thread's worker state.
/// The function is loaded from bytecode on first call and cached by fn_id.
/// @param opts     Identifies the function to call.
/// @param init     Populates the params table passed to the Lua function (primitives only).
/// @param result   Receives the deserialized intent if the function returns a non-nil table.
/// @return true if the function returned a non-nil table (post-pass should run).
/// Must be called from a worker thread (is_pool_worker_thread() must be true).
auto call_pre_fn_in_worker(
    const pre_fn_call_opts &opts,
    std::function<void( sol::table & )> init,
    hook_intent &result
) -> bool;

/// Signal all worker states to reinitialize on next use.
/// Call on the main thread after hook registrations change (e.g. after mod reload).
void invalidate_worker_states();

} // namespace cata
