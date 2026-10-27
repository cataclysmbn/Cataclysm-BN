#include "editor/special_grid.h"

#include <algorithm>
#include <optional>
#include <ranges>

namespace {

/// The square an entry of `overmaps` places; none for one of another shape.
auto square_of(const nlohmann::ordered_json& entry) -> std::optional<editor::special_square> {
    if (!entry.is_object() || !entry.contains("point") || !entry["point"].is_array()
        || entry["point"].size() != 3
        || !std::ranges::all_of(entry["point"], &nlohmann::ordered_json::is_number_integer)
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
    const auto type = value.value("type", "");
    return type == "overmap_special" || type == "city_building";
}

auto editor::special_squares(const nlohmann::ordered_json& value) -> std::vector<special_square> {
    if (value.value("subtype", "fixed") != "fixed" || !value.contains("overmaps")
        || !value["overmaps"].is_array()) {
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
