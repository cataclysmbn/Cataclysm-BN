#include "editor/special_grid.h"

#include "game_constants.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>

namespace {

/// The square an entry of `overmaps` places; none for one of another shape.
auto square_of(const nlohmann::ordered_json& entry) -> std::optional<editor::special_square> {
    // An overmap is as far as a special can reach.
    const auto within = [](const nlohmann::ordered_json& coordinate) {
        return coordinate.is_number_integer() && coordinate.get<std::int64_t>() > -OMAPX
            && coordinate.get<std::int64_t>() < OMAPX;
    };
    if (!entry.is_object() || !entry.contains("point") || !entry["point"].is_array()
        || entry["point"].size() != 3 || !std::ranges::all_of(entry["point"], within)
        || !entry.contains("overmap") || !entry["overmap"].is_string()) {
        return std::nullopt;
    }
    const auto& at = entry["point"];
    return editor::special_square{
        .at = tripoint_rel_omt(at[0].get<int>(), at[1].get<int>(), at[2].get<int>()),
        .overmap = entry["overmap"].get<std::string>()};
}

} // namespace

auto editor::is_special(const nlohmann::ordered_json& value) -> bool {
    return value.is_object() && value.contains("type")
        && (value["type"] == "overmap_special" || value["type"] == "city_building");
}

auto editor::is_fixed(const nlohmann::ordered_json& value) -> bool {
    return value.is_object() && (!value.contains("subtype") || value["subtype"] == "fixed");
}

auto editor::special_levels(const nlohmann::ordered_json& value) -> std::vector<int> {
    auto levels =
        special_squares(value)
        | std::views::transform([](const special_square& square) { return square.at.z(); })
        | std::ranges::to<std::vector>();
    std::ranges::sort(levels);
    const auto [last, end] = std::ranges::unique(levels);
    levels.erase(last, end);
    return levels;
}

auto editor::ground_level(const std::vector<int>& levels) -> int {
    if (levels.empty()) { return 0; }
    // The lowest level from the ground up, else the highest under it.
    const auto above = std::ranges::lower_bound(levels, 0);
    return above != levels.end() ? *above : levels.back();
}

auto editor::special_squares(const nlohmann::ordered_json& value) -> std::vector<special_square> {
    if (!is_fixed(value) || !value.contains("overmaps") || !value["overmaps"].is_array()) {
        return {};
    }
    return value["overmaps"] | std::views::transform(square_of)
         | std::views::filter([](const auto& square) { return square.has_value(); })
         | std::views::transform([](const auto& square) { return *square; })
         | std::ranges::to<std::vector>();
}

auto editor::square_patch(
    const nlohmann::ordered_json& value, const tripoint_rel_omt& at, const std::string& overmap)
    -> nlohmann::ordered_json {
    const auto listed = value.contains("overmaps") && value["overmaps"].is_array();
    const auto found =
        listed
            ? std::ranges::find_if(
                  value["overmaps"],
                  [&](const auto& entry) {
                      const auto square = square_of(entry);
                      return square && square->at == at;
                  })
            : value.end();
    const auto pointer =
        !listed || found == value["overmaps"].end()
            ? std::string()
            : "/overmaps/" + std::to_string(found - value["overmaps"].begin());
    const auto placed =
        nlohmann::ordered_json{{"point", {at.x(), at.y(), at.z()}}, {"overmap", overmap}};
    if (overmap.empty()) {
        if (pointer.empty()) { return nlohmann::ordered_json::array(); }
        return nlohmann::ordered_json::array({{{"op", "remove"}, {"path", pointer}}});
    }
    if (pointer.empty()) {
        return nlohmann::ordered_json::array(
            {listed
                 ? nlohmann::ordered_json{{"op", "add"}, {"path", "/overmaps/-"}, {"value", placed}}
                 : nlohmann::ordered_json{
                       {"op", "add"},
                       {"path", "/overmaps"},
                       {"value", nlohmann::ordered_json::array({placed})}}});
    }
    if ((*found)["overmap"] == overmap) { return nlohmann::ordered_json::array(); }
    return nlohmann::ordered_json::array(
        {{{"op", "replace"}, {"path", pointer + "/overmap"}, {"value", overmap}}});
}
