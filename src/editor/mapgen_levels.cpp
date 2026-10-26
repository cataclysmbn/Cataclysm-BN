#include "editor/mapgen_levels.h"

#include "game_constants.h"
#include "overmap/omdata.h"
#include "overmap/overmap_special.h"

#include <ranges>

namespace editor {
namespace {

/// Where an overmap terrain stands in a building or overmap special, as written facing north.
struct special_place {
    const overmap_special* special = nullptr;
    tripoint_rel_omt at;
};

/// The name of a written terrain without its direction.
auto type_name(const oter_str_id& terrain) -> std::optional<std::string> {
    if (terrain.is_null() || !terrain.is_valid()) { return std::nullopt; }
    return terrain.obj().get_type_id().str();
}

auto place_of(const std::string& om_terrain) -> std::optional<special_place> {
    for (const auto& special : overmap_specials::get_all()) {
        for (const auto& location : special.required_locations()) {
            if (type_name(special.get_terrain_at(location.p)) == om_terrain) {
                return special_place{.special = &special, .at = location.p};
            }
        }
    }
    return std::nullopt;
}

} // namespace

auto entry_terrains(const nlohmann::ordered_json& value) -> std::vector<std::vector<std::string>> {
    if (!value.contains("om_terrain")) { return {}; }
    const auto& names = value["om_terrain"];
    if (names.is_string()) { return {{names.get<std::string>()}}; }
    if (!names.is_array() || names.empty()) { return {}; }
    if (names.front().is_string()) { return {{names.front().get<std::string>()}}; }
    auto rows = std::vector<std::vector<std::string>>{};
    for (const auto& row : names) {
        if (!row.is_array()) { return {}; }
        auto& added = rows.emplace_back();
        for (const auto& name : row) {
            if (!name.is_string()) { return {}; }
            added.push_back(name.get<std::string>());
        }
    }
    return rows;
}

auto terrain_above(const std::string& om_terrain, const int dz) -> std::optional<std::string> {
    const auto place = place_of(om_terrain);
    if (!place) { return std::nullopt; }
    return type_name(place->special->get_terrain_at(place->at + tripoint_rel_omt(0, 0, dz)));
}

auto terrain_level(const std::string& om_terrain) -> std::optional<int> {
    const auto place = place_of(om_terrain);
    return place ? std::optional(place->at.z()) : std::nullopt;
}

auto square_making(const std::vector<document_object>& entries, const std::string& om_terrain)
    -> std::optional<entry_square> {
    for (const auto [index, entry] : std::views::enumerate(entries)) {
        const auto rows = entry_terrains(entry.value);
        for (const auto [y, row] : std::views::enumerate(rows)) {
            if (const auto found = std::ranges::find(row, om_terrain); found != row.end()) {
                const auto x = static_cast<int>(std::ranges::distance(row.begin(), found));
                return entry_square{
                    .entry = static_cast<std::size_t>(index),
                    .origin = point(x * SEEX * 2, static_cast<int>(y) * SEEY * 2)};
            }
        }
    }
    return std::nullopt;
}

auto stacked_squares(
    const std::vector<document_object>& entries, const std::size_t entry, const int dz)
    -> std::vector<stacked_square> {
    auto result = std::vector<stacked_square>{};
    const auto rows = entry_terrains(entries.at(entry).value);
    for (const auto [y, row] : std::views::enumerate(rows)) {
        for (const auto [x, name] : std::views::enumerate(row)) {
            const auto above = terrain_above(name, dz);
            if (!above) { continue; }
            if (const auto other = square_making(entries, *above)) {
                result.push_back(
                    {.origin = point(static_cast<int>(x) * SEEX * 2, static_cast<int>(y) * SEEY * 2),
                     .other = *other});
            }
        }
    }
    return result;
}

} // namespace editor
