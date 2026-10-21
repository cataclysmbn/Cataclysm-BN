#include "editor/mapgen_stroke.h"

#include "catacharset.h"
#include "editor/document.h"
#include "line.h"

#include <algorithm>
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

} // namespace

auto symbols_of(const document& doc, const std::size_t object) -> symbol_grid {
    const auto& value = doc.objects().at(object).value;
    if (!value.contains("object") || !value["object"].contains("rows")) { return {}; }
    return value["object"]["rows"] | std::views::transform([](const auto& row) {
               return utf8_display_split(row.template get<std::string>());
           })
         | std::ranges::to<std::vector>();
}

auto squares_of(const shape& drawn, const symbol_grid& grid) -> std::vector<point> {
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
    auto ordered =
        squares | std::views::filter([&](const point& p) { return inside(p, grid); })
        | std::ranges::to<std::vector>();
    std::ranges::sort(ordered, [](const point& a, const point& b) {
        return std::tie(a.y, a.x) < std::tie(b.y, b.x);
    });
    return ordered;
}

auto stroke(document& doc, const stroke_options& options)
    -> std::expected<std::size_t, std::string> {
    if (options.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(options.object));
    }
    if (utf8_display_split(options.symbol).size() != 1 || utf8_width(options.symbol) != 1) {
        return std::unexpected("\"" + options.symbol + "\" is not one symbol");
    }
    auto grid = symbols_of(doc, options.object);
    if (grid.empty()) { return std::unexpected(std::string("the object has no rows")); }
    auto changed_rows = std::set<int>{};
    auto changed = std::size_t{0};
    for (const auto& p : squares_of(options.drawn, grid)) {
        if (grid[p.y][p.x] != options.symbol) {
            grid[p.y][p.x] = options.symbol;
            changed_rows.insert(p.y);
            ++changed;
        }
    }
    if (changed == 0) { return 0; }
    auto patch = nlohmann::ordered_json::array();
    for (const auto y : changed_rows) {
        auto row = std::string{};
        for (const auto& symbol : grid[y]) { row += symbol; }
        patch.push_back(
            {{"op", "replace"}, {"path", "/object/rows/" + std::to_string(y)}, {"value", row}});
    }
    if (auto edited = doc.edit(options.object, patch); !edited) {
        return std::unexpected(edited.error());
    }
    return changed;
}

} // namespace editor
