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

} // namespace editor
