#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

/// What the game's loader said about a mapgen entry.
struct loader_message {
    std::string text;
    /// The byte of the loaded text it names, when it names one.
    std::optional<std::size_t> offset = std::nullopt;
};

/// Loads one mapgen entry (`"type": "mapgen"`) from `text` with the game's own loader, finalizes
/// and checks it as the game does after loading data, and registers nothing; what the loader
/// reported, in order. A palette entry is loaded and added with every palette it names, as a
/// mapgen using it would, so that a loop through it is reported.
auto check_mapgen(std::string_view text) -> std::vector<loader_message>;

} // namespace editor
