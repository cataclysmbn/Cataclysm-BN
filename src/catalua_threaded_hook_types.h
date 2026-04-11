#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cata
{

/// Primitive value that can cross the worker→main-thread boundary.
/// Complex types (usertypes, functions, tables) are intentionally excluded.
using hook_intent_value = std::variant<std::monostate, int, double, bool, std::string>;

/// Flat key→primitive map passed from a threaded hook pre-pass to its post-pass.
/// No Lua objects cross the state boundary — only these serialized primitives.
using hook_intent = std::unordered_map<std::string, hook_intent_value>;

/// Result from one threaded hook entry's pre-pass.
struct hook_pre_result {
    bool        run_post   = false; ///< If false, post-pass is skipped for this entry.
    uint64_t    post_fn_id = 0;     ///< ID of the post function stored in global Lua state.
    std::string mod_id;
    hook_intent intent;
};

} // namespace cata
