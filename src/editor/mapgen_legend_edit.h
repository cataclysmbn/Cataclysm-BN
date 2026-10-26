#pragma once

#include <cstddef>
#include <expected>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace editor {

class document;

struct legend_edit {
    std::size_t object = 0;
    /// The member of the mapgen's object, or of the palette: "terrain", "furniture", "items", ...
    std::string layer;
    std::string symbol;
    /// What the symbol maps to on that layer, as the data writes it; none removes the mapping, and
    /// the layer with its last one.
    std::optional<nlohmann::ordered_json> value = std::nullopt;
};

/// Sets one mapping of the mapgen's or palette's own legend as one undo step of `doc`.
auto set_legend(document& doc, const legend_edit& edit) -> std::expected<void, std::string>;

} // namespace editor
