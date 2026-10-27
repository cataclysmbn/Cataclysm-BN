#include "editor/special_grid.h"

#include "game_constants.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>

namespace {

/// The point an entry names as `key`; none for one out of an overmap's reach or of another shape.
auto point_of(const nlohmann::ordered_json& entry, const char* const key)
    -> std::optional<tripoint_rel_omt> {
    // An overmap is as far as a special can reach.
    const auto within = [](const nlohmann::ordered_json& coordinate) {
        return coordinate.is_number_integer() && coordinate.get<std::int64_t>() > -OMAPX
            && coordinate.get<std::int64_t>() < OMAPX;
    };
    if (!entry.is_object() || !entry.contains(key) || !entry[key].is_array()
        || entry[key].size() != 3 || !std::ranges::all_of(entry[key], within)) {
        return std::nullopt;
    }
    const auto& at = entry[key];
    return tripoint_rel_omt(at[0].get<int>(), at[1].get<int>(), at[2].get<int>());
}

auto json_point(const tripoint_rel_omt& at) -> nlohmann::ordered_json {
    return nlohmann::ordered_json::array({at.x(), at.y(), at.z()});
}

/// The square an entry of `overmaps` places; none for one of another shape.
auto square_of(const nlohmann::ordered_json& entry) -> std::optional<editor::special_square> {
    const auto at = point_of(entry, "point");
    if (!at || !entry.contains("overmap") || !entry["overmap"].is_string()) { return std::nullopt; }
    return editor::special_square{.at = *at, .overmap = entry["overmap"].get<std::string>()};
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
    const auto placed = nlohmann::ordered_json{{"point", json_point(at)}, {"overmap", overmap}};
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

auto editor::special_connections(const nlohmann::ordered_json& value)
    -> std::vector<special_connection> {
    if (!value.is_object() || !value.contains("connections") || !value["connections"].is_array()) {
        return {};
    }
    return value["connections"] | std::views::filter([](const auto& entry) {
               return point_of(entry, "point") && entry.contains("connection")
                   && entry["connection"].is_string();
           })
         | std::views::transform([](const auto& entry) {
               return special_connection{
                   .at = *point_of(entry, "point"),
                   .connection = entry["connection"].template get<std::string>()};
           })
         | std::ranges::to<std::vector>();
}

auto editor::connection_patch(
    const nlohmann::ordered_json& value, const tripoint_rel_omt& at, const std::string& connection)
    -> nlohmann::ordered_json {
    const auto listed = value.contains("connections") && value["connections"].is_array();
    if (listed) {
        const auto& connections = value["connections"];
        const auto found = std::ranges::find_if(connections, [&](const auto& entry) {
            return point_of(entry, "point") == at;
        });
        if (found != connections.end()) {
            return nlohmann::ordered_json::array(
                {{{"op", "remove"},
                  {"path", "/connections/" + std::to_string(found - connections.begin())}}});
        }
    }
    auto joined = nlohmann::ordered_json{{"point", json_point(at)}, {"connection", connection}};
    const auto squares = special_squares(value);
    for (const auto& side : {point_north, point_east, point_south, point_west}) {
        const auto from = at + tripoint_rel_omt(side.x, side.y, 0);
        if (std::ranges::contains(squares, from, &special_square::at)) {
            joined["from"] = json_point(from);
            break;
        }
    }
    return nlohmann::ordered_json::array(
        {listed
             ? nlohmann::ordered_json{{"op", "add"}, {"path", "/connections/-"}, {"value", joined}}
             : nlohmann::ordered_json{
                   {"op", "add"},
                   {"path", "/connections"},
                   {"value", nlohmann::ordered_json::array({joined})}}});
}
