#pragma once

#include <optional>
#include <string>
#include <vector>

#include "translations.h"

namespace cata
{

/// Specification for a string_select option choice.
struct lua_option_choice {
    std::string id;
    translation name;
};

/// Type of Lua mod option.
enum class lua_option_type {
    boolean,
    integer,
    floating,
    string_select,
    string_input
};

/// Specification for a Lua mod option, as provided by the mod.
struct lua_option_spec {
    /// Option ID (without mod prefix).
    std::string id;
    /// Display name.
    translation name;
    /// Tooltip/description.
    translation tooltip;
    /// Option type.
    lua_option_type type = lua_option_type::boolean;

    /// Default value for bool options.
    bool default_bool = false;

    /// Default/min/max for int options.
    int default_int = 0;
    int min_int = 0;
    int max_int = 100;

    /// Default/min/max/step for float options.
    float default_float = 0.0f;
    float min_float = 0.0f;
    float max_float = 1.0f;
    float step_float = 0.1f;

    /// Choices for string_select options.
    std::vector<lua_option_choice> choices;
    /// Default value for string_select/string_input options.
    std::string default_string;
    /// Max length for string_input options.
    int max_length = 64;
};

/// Register a Lua mod option with the options manager.
/// @param mod_ident The mod's unique identifier (used for namespacing).
/// @param spec The option specification.
/// @returns true if registration succeeded, false if option already exists or spec is invalid.
auto register_lua_option( const std::string &mod_ident, const lua_option_spec &spec ) -> bool;

/// Get the full option ID for a Lua mod option.
/// @param mod_ident The mod's unique identifier.
/// @param option_id The option's local ID.
/// @returns The full namespaced option ID (e.g., "LUA_mymod_MY_OPTION").
auto get_lua_option_id( const std::string &mod_ident, const std::string &option_id ) -> std::string;

/// Clear all Lua mod options. Called when reloading mods.
auto clear_lua_options() -> void;

} // namespace cata
