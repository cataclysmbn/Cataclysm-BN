#include "catalua_loader.h"

#include "catalua_sol.h"
#include "filesystem.h"
#include "path_info.h"

#include <algorithm>
#include <ranges>

namespace fs = std::filesystem;

namespace cata::lua_loader
{

namespace
{

// Stack of files currently being loaded (for relative path resolution)
thread_local std::vector<fs::path> loading_stack;

auto push_path( fs::path path ) -> void
{
    loading_stack.push_back( std::move( path ) );
}

auto pop_path() -> void
{
    if( !loading_stack.empty() ) {
        loading_stack.pop_back();
    }
}

auto get_current_path() -> std::optional<fs::path>
{
    if( loading_stack.empty() ) {
        return std::nullopt;
    }
    return loading_stack.back();
}

auto get_current_mod_path( lua_State *L ) -> std::string
{
    // Get current mod path from game.current_mod_path global
    lua_getglobal( L, "game" );
    if( lua_istable( L, -1 ) ) {
        lua_getfield( L, -1, "current_mod_path" );
        if( lua_isstring( L, -1 ) ) {
            const auto path = std::string{ lua_tostring( L, -1 ) };
            lua_pop( L, 2 ); // pop path and game table
            return path;
        }
        lua_pop( L, 1 ); // pop nil/non-string
    }
    lua_pop( L, 1 ); // pop game table
    return "";
}

auto normalize_module_name( std::string_view name ) -> std::string
{
    // pl.utils -> pl/utils
    auto result = std::string{ name };
    std::ranges::replace( result, '.', '/' );
    return result;
}

auto resolve_relative_path( std::string_view modname ) -> std::optional<fs::path>
{
    const auto current = get_current_path();
    if( !current ) {
        return std::nullopt;
    }
    return ( current->parent_path() / modname ).lexically_normal();
}

auto is_within_allowed_paths( fs::path const &resolved ) -> bool
{
    const auto data_dir = fs::path{ PATH_INFO::datadir() }.lexically_normal();
    const auto base_dir = fs::path{ PATH_INFO::base_path() }.lexically_normal();
    const auto resolved_norm = resolved.lexically_normal().string();
    return resolved_norm.starts_with( data_dir.string() ) ||
           resolved_norm.starts_with( base_dir.string() );
}

auto try_file_with_suffixes( fs::path const &base ) -> std::optional<fs::path>
{
    for( const auto &suffix : { ".lua", "/init.lua" } ) {
        auto path = base;
        path += suffix;
        if( file_exist( path.string() ) ) {
            return path;
        }
    }
    return std::nullopt;
}

auto search_module( lua_State *L, std::string_view modname ) -> std::optional<fs::path>
{
    // Relative import: ./foo or ../bar
    if( modname.starts_with( "./" ) || modname.starts_with( "../" ) ) {
        auto resolved = resolve_relative_path( modname );
        if( !resolved || !is_within_allowed_paths( *resolved ) ) {
            return std::nullopt;
        }
        return try_file_with_suffixes( *resolved );
    }

    // Absolute import: pl.utils -> pl/utils
    const auto normalized = normalize_module_name( modname );

    // lib.* or bn.lib.* -> data/lua/lib/* (shared standard library)
    if( modname.starts_with( "lib." ) || modname.starts_with( "bn.lib." ) ) {
        const auto prefix_len = modname.starts_with( "lib." ) ? 4 : 7; // "lib." or "bn.lib."
        const auto without_prefix = normalize_module_name( modname.substr( prefix_len ) );
        const auto base_path = fs::path{ PATH_INFO::datadir() } / "lua" / "lib" / without_prefix;
        return try_file_with_suffixes( base_path );
    }

    // Regular absolute import: search mod-local, then base_path (for tests)
    const auto mod_path_str = get_current_mod_path( L );
    const auto mod_path = mod_path_str.empty() ? fs::path{} :
                          fs::path{ mod_path_str };
    const auto base_path = fs::path{ PATH_INFO::base_path() };

    // Search order: mod-local first, then base_path (for tests)
    for( const auto &search_base : { mod_path, base_path } ) {
        if( search_base.empty() ) {
            continue;
        }
        const auto candidate = search_base / normalized;
        if( auto found = try_file_with_suffixes( candidate ) ) {
            return found;
        }
    }
    return std::nullopt;
}

// The actual loader that executes the module
// Upvalue 1: path string
auto module_loader( lua_State *L ) -> int
{
    // Get path from upvalue (captured when searcher created this closure)
    const auto path = fs::path{ lua_tostring( L, lua_upvalueindex( 1 ) ) };

    // RAII guard ensures stack cleanup even if Lua error occurs
    const auto guard = script_context_guard{ path };

    // Load the file
    const auto status = luaL_loadfile( L, path.string().c_str() );
    if( status != LUA_OK ) {
        return lua_error( L );
    }

    // Call the loaded chunk (execute module)
    lua_call( L, 0, 1 );

    return 1; // Return module result
}

// Searcher function for package.searchers
auto module_searcher( lua_State *L ) -> int
{
    const auto modname = std::string_view{ lua_tostring( L, 1 ) };

    const auto found = search_module( L, modname );
    if( !found ) {
        lua_pushfstring( L, "\n\tno file for module '%s'", modname.data() );
        return 1;
    }

    // Push path as upvalue, return loader closure
    lua_pushstring( L, found->string().c_str() );
    lua_pushcclosure( L, module_loader, 1 );

    // Second return: file path (for debug/error messages)
    lua_pushstring( L, found->string().c_str() );

    return 2;
}

} // namespace

// script_context_guard implementation
script_context_guard::script_context_guard( fs::path path )
{
    push_path( std::move( path ) );
}

script_context_guard::~script_context_guard()
{
    pop_path();
}

auto register_searcher( lua_State *L ) -> void
{
    // Insert our searcher at position 2 (after preload, before default Lua searcher)
    lua_getglobal( L, "package" );
    lua_getfield( L, -1, "searchers" );

    // Shift existing searchers down
    const auto len = static_cast<int>( lua_rawlen( L, -1 ) );
    for( auto i = len; i >= 2; --i ) {
        lua_rawgeti( L, -1, i );
        lua_rawseti( L, -2, i + 1 );
    }

    // Insert our searcher at position 2
    lua_pushcfunction( L, module_searcher );
    lua_rawseti( L, -2, 2 );

    lua_pop( L, 2 ); // pop searchers and package
}

} // namespace cata::lua_loader
