#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct lua_State;

namespace cata::lua_loader
{

// Stack of files currently being loaded (for relative path resolution)
// Managed automatically by the loader - no manual intervention needed
inline thread_local std::vector<std::filesystem::path> loading_stack;

// Register custom searcher - call once during lua state init
auto register_searcher( lua_State *L ) -> void;

} // namespace cata::lua_loader
