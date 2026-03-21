#pragma once

#include <filesystem>

struct lua_State;

namespace cata::lua_loader
{

// RAII guard for managing script loading context
// Automatically pushes/pops the current script path to/from the loading stack
// Ensures exception safety during module loading
class script_context_guard
{
    public:
        explicit script_context_guard( std::filesystem::path path );
        ~script_context_guard();

        // Delete copy/move to prevent stack corruption
        script_context_guard( script_context_guard const & ) = delete;
        script_context_guard( script_context_guard && ) = delete;
        auto operator=( script_context_guard const & ) -> script_context_guard & = delete;
        auto operator=( script_context_guard && ) -> script_context_guard & = delete;
};

// Register custom searcher - call once during lua state init
auto register_searcher( lua_State *L ) -> void;

} // namespace cata::lua_loader
