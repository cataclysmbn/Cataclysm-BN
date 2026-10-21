#pragma once

#include "point.h"

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace editor {

class document;

/// The symbols of a mapgen's rows, row by row; each one is a key of its legend.
using symbol_grid = std::vector<std::vector<std::string>>;

enum class shape_kind {
    point,
    line,
    /// The outline of the rectangle with corners `from` and `to`.
    rect,
    rect_filled,
    /// The ellipse inscribed in that rectangle, filled.
    ellipse,
    /// The squares joined to `from` that hold its symbol.
    fill,
};

struct shape {
    shape_kind kind = shape_kind::point;
    point from;
    point to;
};

/// The rows of object `object` of `doc`; empty when it has no rows.
auto symbols_of(const document& doc, std::size_t object) -> symbol_grid;

/// The squares of `grid` that `drawn` covers, each once, in reading order.
auto squares_of(const shape& drawn, const symbol_grid& grid) -> std::vector<point>;

struct stroke_options {
    std::size_t object = 0;
    shape drawn;
    /// One symbol of the legend, a single column wide.
    std::string symbol;
};

/// Sets the squares `options.drawn` covers to `options.symbol` as one undo step of `doc`; how many
/// squares changed. Nothing changes, and no step is added, when none would.
auto stroke(document& doc, const stroke_options& options)
    -> std::expected<std::size_t, std::string>;

} // namespace editor
