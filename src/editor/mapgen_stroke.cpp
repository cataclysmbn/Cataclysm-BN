#include "editor/mapgen_stroke.h"

#include "catacharset.h"
#include "editor/document.h"
#include "line.h"

#include <algorithm>
#include <iterator>
#include <nlohmann/json.hpp>
#include <ranges>
#include <set>

namespace editor {
namespace {

auto inside(const point& p, const symbol_grid& grid) -> bool {
    return p.y >= 0 && p.y < static_cast<int>(grid.size()) && p.x >= 0
        && p.x < static_cast<int>(grid[p.y].size());
}

auto flood(const point& from, const symbol_grid& grid) -> std::set<point> {
    auto filled = std::set<point>{};
    if (!inside(from, grid)) { return filled; }
    const auto& symbol = grid[from.y][from.x];
    auto pending = std::vector{from};
    while (!pending.empty()) {
        const auto p = pending.back();
        pending.pop_back();
        if (!inside(p, grid) || grid[p.y][p.x] != symbol || !filled.insert(p).second) { continue; }
        for (const auto& step : four_adjacent_offsets) { pending.push_back(p + step); }
    }
    return filled;
}

/// The squares the shape itself covers, before its size and mirrors.
auto shape_squares(const shape& drawn, const symbol_grid& grid) -> std::set<point> {
    const auto low = point(std::min(drawn.from.x, drawn.to.x), std::min(drawn.from.y, drawn.to.y));
    const auto high = point(std::max(drawn.from.x, drawn.to.x), std::max(drawn.from.y, drawn.to.y));
    const auto in_box = [&](const auto& keep) {
        auto squares = std::set<point>{};
        for (auto y = low.y; y <= high.y; ++y) {
            for (auto x = low.x; x <= high.x; ++x) {
                if (keep(point(x, y))) { squares.insert(point(x, y)); }
            }
        }
        return squares;
    };
    auto squares = std::set<point>{};
    switch (drawn.kind) {
        case shape_kind::point:
            squares = {drawn.from};
            break;
        case shape_kind::line:
            squares = line_to(drawn.from, drawn.to) | std::ranges::to<std::set>();
            squares.insert(drawn.from);
            break;
        case shape_kind::rect:
        case shape_kind::room:
            squares = in_box([&](const point& p) {
                return p.x == low.x || p.x == high.x || p.y == low.y || p.y == high.y;
            });
            break;
        case shape_kind::rect_filled:
            squares = in_box([](const point&) { return true; });
            break;
        case shape_kind::ellipse: {
            // Twice the centre and the radii, so that an even span has its centre between squares.
            const auto centre = low + high;
            const auto radii = point(high.x - low.x + 1, high.y - low.y + 1);
            squares = in_box([&](const point& p) {
                const auto dx = static_cast<double>(2 * p.x - centre.x) / radii.x;
                const auto dy = static_cast<double>(2 * p.y - centre.y) / radii.y;
                return dx * dx + dy * dy <= 1.0;
            });
            break;
        }
        case shape_kind::fill:
            squares = flood(drawn.from, grid);
            break;
    }
    return squares;
}

/// The squares of `drawn` each widened to its size, then mirrored.
auto widened(const shape& drawn, const std::set<point>& squares, const symbol_grid& grid)
    -> std::set<point> {
    const auto width = grid.empty() ? 0 : static_cast<int>(grid.front().size());
    const auto height = static_cast<int>(grid.size());
    const auto reach = std::max(drawn.size, 1);
    auto out = std::set<point>{};
    for (const auto& p : squares) {
        for (auto dy = 0; dy < reach; ++dy) {
            for (auto dx = 0; dx < reach; ++dx) {
                const auto q = p + point(dx - (reach - 1) / 2, dy - (reach - 1) / 2);
                out.insert(q);
                if (drawn.mirror_x) { out.insert(point(width - 1 - q.x, q.y)); }
                if (drawn.mirror_y) { out.insert(point(q.x, height - 1 - q.y)); }
                if (drawn.mirror_x && drawn.mirror_y) {
                    out.insert(point(width - 1 - q.x, height - 1 - q.y));
                }
            }
        }
    }
    return out;
}

auto in_reading_order(const std::set<point>& squares, const symbol_grid& grid)
    -> std::vector<point> {
    auto ordered =
        squares | std::views::filter([&](const point& p) { return inside(p, grid); })
        | std::ranges::to<std::vector>();
    std::ranges::sort(ordered, [](const point& a, const point& b) {
        return std::tie(a.y, a.x) < std::tie(b.y, b.x);
    });
    return ordered;
}

/// Writes the rows of `grid` that `changed_rows` names as one undo step of `doc`.
auto commit_rows(
    document& doc, const std::size_t object, const symbol_grid& grid,
    const std::set<int>& changed_rows) -> std::expected<void, std::string> {
    auto patch = nlohmann::ordered_json::array();
    for (const auto y : changed_rows) {
        auto row = std::string{};
        for (const auto& symbol : grid[y]) { row += symbol; }
        patch.push_back(
            {{"op", "replace"}, {"path", "/object/rows/" + std::to_string(y)}, {"value", row}});
    }
    return doc.edit(object, patch);
}

} // namespace

auto symbols_of(const document& doc, const std::size_t object) -> symbol_grid {
    const auto& value = doc.objects().at(object).value;
    if (!value.contains("object") || !value["object"].contains("rows")) { return {}; }
    return value["object"]["rows"] | std::views::transform([](const auto& row) {
               return utf8_display_split(row.template get<std::string>());
           })
         | std::ranges::to<std::vector>();
}

auto symbol_at(const symbol_grid& grid, const point& at) -> std::string {
    return inside(at, grid) ? grid[at.y][at.x] : std::string{};
}

auto squares_of(const shape& drawn, const symbol_grid& grid) -> std::vector<point> {
    return in_reading_order(widened(drawn, shape_squares(drawn, grid), grid), grid);
}

auto stroke(document& doc, const stroke_options& options)
    -> std::expected<std::size_t, std::string> {
    if (options.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(options.object));
    }
    const auto room = options.drawn.kind == shape_kind::room;
    auto symbols =
        room ? std::vector{options.symbol, options.fill_symbol} : std::vector{options.symbol};
    std::ranges::copy(options.over | std::views::values, std::back_inserter(symbols));
    for (const auto& symbol : symbols) {
        if (utf8_display_split(symbol).size() != 1 || utf8_width(symbol) != 1) {
            return std::unexpected("\"" + symbol + "\" is not one symbol");
        }
    }
    auto grid = symbols_of(doc, options.object);
    if (grid.empty()) { return std::unexpected(std::string("the object has no rows")); }
    auto changed_rows = std::set<int>{};
    auto changed = std::size_t{0};
    // What a square held before the stroke picks its symbol, though a room's inside came first.
    const auto before = grid;
    const auto paint = [&](const shape& drawn, const std::string& symbol, const auto& over) {
        for (const auto& p : squares_of(drawn, grid)) {
            const auto found = over.find(before[p.y][p.x]);
            const auto& ink = found == over.end() ? symbol : found->second;
            if (grid[p.y][p.x] == ink) { continue; }
            grid[p.y][p.x] = ink;
            changed_rows.insert(p.y);
            ++changed;
        }
    };
    if (room) {
        const auto& [kind, from, to, size, mirror_x, mirror_y] = options.drawn;
        const auto inside_from = point(std::min(from.x, to.x) + 1, std::min(from.y, to.y) + 1);
        const auto inside_to = point(std::max(from.x, to.x) - 1, std::max(from.y, to.y) - 1);
        if (inside_from.x <= inside_to.x && inside_from.y <= inside_to.y) {
            paint({.kind = shape_kind::rect_filled,
                   .from = inside_from,
                   .to = inside_to,
                   .size = size,
                   .mirror_x = mirror_x,
                   .mirror_y = mirror_y},
                  options.fill_symbol, std::map<std::string, std::string>{});
        }
    }
    paint(options.drawn, options.symbol, options.over);
    if (changed == 0) { return 0; }
    if (auto edited = commit_rows(doc, options.object, grid, changed_rows); !edited) {
        return std::unexpected(edited.error());
    }
    return changed;
}

auto copy_region(const symbol_grid& grid, const point& from, const point& to) -> symbol_grid {
    auto clip = symbol_grid{};
    for (auto y = std::max(std::min(from.y, to.y), 0);
         y <= std::min(std::max(from.y, to.y), static_cast<int>(grid.size()) - 1); ++y) {
        auto row = std::vector<std::string>{};
        for (auto x = std::max(std::min(from.x, to.x), 0);
             x <= std::min(std::max(from.x, to.x), static_cast<int>(grid[y].size()) - 1); ++x) {
            row.push_back(grid[y][x]);
        }
        if (!row.empty()) { clip.push_back(std::move(row)); }
    }
    return clip;
}

auto rotated(const symbol_grid& clip) -> symbol_grid {
    if (clip.empty()) { return {}; }
    auto turned = symbol_grid(clip.front().size());
    for (const auto& row : clip | std::views::reverse) {
        for (auto x = std::size_t{0}; x < turned.size(); ++x) { turned[x].push_back(row[x]); }
    }
    return turned;
}

auto mirrored_x(const symbol_grid& clip) -> symbol_grid {
    return clip | std::views::transform([](const auto& row) {
               return row | std::views::reverse | std::ranges::to<std::vector>();
           })
         | std::ranges::to<symbol_grid>();
}

auto mirrored_y(const symbol_grid& clip) -> symbol_grid {
    return clip | std::views::reverse | std::ranges::to<symbol_grid>();
}

auto paste(document& doc, const paste_options& options) -> std::expected<std::size_t, std::string> {
    if (options.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(options.object));
    }
    auto grid = symbols_of(doc, options.object);
    auto changed_rows = std::set<int>{};
    auto changed = std::size_t{0};
    for (const auto& [dy, row] : options.clip | std::views::enumerate) {
        for (const auto& [dx, symbol] : row | std::views::enumerate) {
            if (utf8_display_split(symbol).size() != 1 || utf8_width(symbol) != 1) {
                return std::unexpected("\"" + symbol + "\" is not one symbol");
            }
            const auto at = options.at + point(dx, dy);
            if (!inside(at, grid) || grid[at.y][at.x] == symbol) { continue; }
            grid[at.y][at.x] = symbol;
            changed_rows.insert(at.y);
            ++changed;
        }
    }
    if (changed == 0) { return 0; }
    if (auto edited = commit_rows(doc, options.object, grid, changed_rows); !edited) {
        return std::unexpected(edited.error());
    }
    return changed;
}

} // namespace editor
