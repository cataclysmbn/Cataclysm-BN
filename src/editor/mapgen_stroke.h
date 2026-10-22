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
    /// The rectangle's outline in the stroke's symbol around its inside in the fill symbol.
    room,
};

struct shape {
    shape_kind kind = shape_kind::point;
    point from;
    point to;
    /// The side of the square of squares each square of the shape stands for.
    int size = 1;
    /// Whether the shape is also drawn mirrored across the middle column, or the middle row.
    bool mirror_x = false;
    bool mirror_y = false;
};

/// The rows of object `object` of `doc`; empty when it has no rows.
auto symbols_of(const document& doc, std::size_t object) -> symbol_grid;

/// The squares of `grid` that `drawn` covers, each once, in reading order; a room covers its
/// outline.
auto squares_of(const shape& drawn, const symbol_grid& grid) -> std::vector<point>;

/// The symbol at `at`, as the eyedropper picks it; empty outside the rows.
auto symbol_at(const symbol_grid& grid, const point& at) -> std::string;

struct stroke_options {
    std::size_t object = 0;
    shape drawn;
    /// One symbol of the legend, a single column wide.
    std::string symbol;
    /// The inside of a room.
    std::string fill_symbol = {};
};

/// Sets the squares `options.drawn` covers to `options.symbol` as one undo step of `doc`; how many
/// squares changed. Nothing changes, and no step is added, when none would.
auto stroke(document& doc, const stroke_options& options)
    -> std::expected<std::size_t, std::string>;

/// The symbols of the rectangle with corners `from` and `to`, row by row, cut at the edge of the
/// rows; empty when no square of it is inside them.
auto copy_region(const symbol_grid& grid, const point& from, const point& to) -> symbol_grid;

/// `clip` turned a quarter clockwise.
auto rotated(const symbol_grid& clip) -> symbol_grid;

struct paste_options {
    std::size_t object = 0;
    symbol_grid clip;
    /// Where the top-left square of the clip lands.
    point at;
};

/// Sets the squares under `options.clip` to its symbols as one undo step of `doc`, dropping the
/// part beyond the rows; how many squares changed. Nothing changes, and no step is added, when
/// none would.
auto paste(document& doc, const paste_options& options) -> std::expected<std::size_t, std::string>;

} // namespace editor
