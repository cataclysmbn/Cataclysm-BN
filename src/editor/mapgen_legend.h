#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

/// One thing a symbol places, as the data writes it.
struct legend_line {
    /// The member that maps it: "terrain", "furniture", "items", ...
    std::string layer;
    /// Its JSON text.
    std::string value;
    /// The palette that maps it; "" for the mapgen's own legend.
    std::string source;
};

/// What a symbol of a mapgen's rows places, in the order the loader applies it.
struct legend_symbol {
    std::string symbol;
    std::vector<legend_line> lines;
    /// Whether the rows use it.
    bool used = false;
};

/// The legend of one `"type": "mapgen"` entry in `text`, resolved through its own legend and the
/// palettes it names as the loader resolves it: every symbol mapped or used, ordered by symbol. The
/// error is what the loader threw.
auto mapgen_legend(std::string_view text) -> std::expected<std::vector<legend_symbol>, std::string>;

} // namespace editor
