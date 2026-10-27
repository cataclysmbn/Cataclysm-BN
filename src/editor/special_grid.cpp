#include "editor/special_grid.h"

#include <algorithm>
#include <ranges>

auto editor::is_special(const nlohmann::ordered_json& value) -> bool {
    const auto type = value.value("type", "");
    return type == "overmap_special" || type == "city_building";
}

auto editor::special_squares(const nlohmann::ordered_json& value) -> std::vector<special_square> {
    if (value.value("subtype", "fixed") != "fixed" || !value.contains("overmaps")
        || !value["overmaps"].is_array()) {
        return {};
    }
    return value["overmaps"] | std::views::filter([](const auto& entry) {
               return entry.is_object() && entry.contains("point") && entry["point"].is_array()
                   && entry["point"].size() == 3
                   && std::ranges::all_of(entry["point"], &nlohmann::ordered_json::is_number_integer)
                   && entry.contains("overmap") && entry["overmap"].is_string();
           })
         | std::views::transform([](const auto& entry) {
               const auto& at = entry["point"];
               return special_square{
                   .at = tripoint_rel_omt(
                       at[0].template get<int>(), at[1].template get<int>(),
                       at[2].template get<int>()),
                   .overmap = entry["overmap"].template get<std::string>()};
           })
         | std::ranges::to<std::vector>();
}
