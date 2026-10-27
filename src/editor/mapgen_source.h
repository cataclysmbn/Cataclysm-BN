#pragma once

#include <optional>
#include <string>

namespace editor {

/// Where the game loaded a mapgen's object from.
struct mapgen_source {
    std::string path;
    /// The byte of the file where the object starts.
    int offset = 0;
};

/// The source of a mapgen the game registered for `mapgen_id`, the overmap terrain's id without a
/// direction; none when it has no JSON mapgen, or none the game read from a file.
auto mapgen_source_of(const std::string& mapgen_id) -> std::optional<mapgen_source>;

/// A mapgen the game runs as code rather than draws from JSON.
struct code_mapgen {
    /// `builtin` for a C++ function of the game, `lua` for a Lua one.
    std::string method;
    /// The function's name: the built-in's `name` or the Lua `luamethod`.
    std::string name;
    /// Where the JSON naming it is, as `path:line`; none when it was read from no file.
    std::optional<std::string> declared;
    /// Where Lua defines the function, as `path:line`; none for a built-in.
    std::optional<std::string> defined;
};

/// The built-in and Lua mapgens the game registered for `mapgen_id`.
auto code_mapgens_of(const std::string& mapgen_id) -> std::vector<code_mapgen>;

/// Where Lua defines the mapgen function `name` of `game.mapgen_functions`; none when it does not.
auto lua_mapgen_defined_at(const std::string& name) -> std::optional<std::string>;

/// `path:line` of the byte `offset` of the file `path`.
auto source_line(const std::string& path, int offset) -> std::string;

} // namespace editor
