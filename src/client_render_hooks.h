#pragma once

#include <string>
#include <vector>

class JsonObject;

namespace game_client
{

/// Loads a client-specific mod tileset declaration while loading game data.
/// Its signature mirrors DynamicDataLoader's four-argument loader callback.
auto load_mod_tileset( const JsonObject &jsobj, const std::string &src,
                       const std::string &base_path, const std::string &full_path ) -> void;

/// Clears client-specific mod tileset state when game data is unloaded.
auto reset_mod_tileset() -> void;

/// A `mod_tileset` of the loaded game data, whichever client draws it.
struct mod_tileset_declaration {
    std::string base_path;
    std::string full_path;
    /// Which `mod_tileset` of the file this is, counting from 1.
    int index = 1;
    std::vector<std::string> compatibility;
};

/// The `mod_tileset` objects of the loaded game data, in load order.
auto declared_mod_tilesets() -> const std::vector<mod_tileset_declaration> &; // *NOPAD*

} // namespace game_client
