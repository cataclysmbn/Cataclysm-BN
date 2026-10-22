#include "editor/mapgen_export.h"

#include "game_constants.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/submap.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "type_id.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <tuple>
#include <vector>

namespace editor {
namespace {

/// The symbols a mapgen's rows can use, the most common square getting the first.
constexpr auto symbols = std::string_view(
    ".#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,;:!%&*+-/<>=?@^~_()[]{}|$");

struct kind {
    ter_id ter;
    furn_id furn;
    auto operator<(const kind& other) const -> bool {
        return std::tuple(ter.to_i(), furn.to_i())
             < std::tuple(other.ter.to_i(), other.furn.to_i());
    }
};

} // namespace

auto export_omt(const tripoint_abs_omt& at) -> std::expected<nlohmann::ordered_json, std::string> {
    auto& here = get_map();
    const auto base = project_to<coords::sm>(at);
    auto kinds = std::vector<std::vector<kind>>{};
    auto counts = std::map<kind, std::size_t>{};
    for (auto y = 0; y < 2 * SEEY; ++y) {
        auto& row = kinds.emplace_back();
        for (auto x = 0; x < 2 * SEEX; ++x) {
            const auto* const sm = here.get_mapbuffer().lookup_submap_in_memory(
                base + point_rel_sm(x / SEEX, y / SEEY));
            if (sm == nullptr) {
                return std::unexpected("the overmap square is not loaded in the map");
            }
            const auto local = point_sm_ms(x % SEEX, y % SEEY);
            row.push_back({.ter = sm->get_ter(local), .furn = sm->get_furn(local)});
            ++counts[row.back()];
        }
    }
    auto ordered = counts | std::views::keys | std::ranges::to<std::vector>();
    std::ranges::stable_sort(ordered, [&](const kind& a, const kind& b) {
        return counts[a] > counts[b];
    });
    if (ordered.size() > symbols.size()) {
        return std::unexpected("the overmap square holds too many kinds of squares");
    }
    auto terrain = nlohmann::ordered_json::object();
    auto furniture = nlohmann::ordered_json::object();
    auto symbol_of = std::map<kind, char>{};
    for (const auto& [index, entry] : ordered | std::views::enumerate) {
        const auto symbol = std::string(1, symbols[index]);
        symbol_of[entry] = symbols[index];
        terrain[symbol] = entry.ter.id().str();
        if (entry.furn != furn_id("f_null")) { furniture[symbol] = entry.furn.id().str(); }
    }
    auto rows = nlohmann::ordered_json::array();
    for (const auto& row : kinds) {
        rows.push_back(
            row | std::views::transform([&](const kind& entry) { return symbol_of[entry]; })
            | std::ranges::to<std::string>());
    }
    auto object = nlohmann::ordered_json{{"rows", rows}, {"terrain", terrain}};
    if (!furniture.empty()) { object["furniture"] = furniture; }
    return nlohmann::ordered_json{
        {"type", "mapgen"},
        {"method", "json"},
        {"om_terrain", get_overmapbuffer(here.get_bound_dimension()).ter(at)->get_type_id().str()},
        {"object", object}};
}

} // namespace editor
