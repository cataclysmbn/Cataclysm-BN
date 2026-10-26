#include "editor/mapgen_levels.h"

#include "game_constants.h"
#include "overmap/omdata.h"
#include "overmap/overmap_special.h"

#include <ranges>

namespace editor {
namespace {

/// A written terrain as placed: its name without its direction, and whether that turns it.
auto placed(const oter_str_id& terrain) -> std::optional<placed_terrain> {
    if (terrain.is_null() || !terrain.is_valid()) { return std::nullopt; }
    return placed_terrain{
        .name = terrain.obj().get_type_id().str(),
        .turned = terrain.obj().get_dir() != om_direction::type::north};
}

/// A square of a layout.
struct layout_place {
    const special_layout* layout = nullptr;
    tripoint at;
};

/// Every square a layout places `om_terrain` on, in layout order.
auto places_of(const level_index& index, const std::string& om_terrain)
    -> std::vector<layout_place> {
    auto result = std::vector<layout_place>{};
    for (const auto& layout : index.layouts) {
        for (const auto& [at, placed] : layout) {
            if (placed.name == om_terrain) { result.push_back({.layout = &layout, .at = at}); }
        }
    }
    return result;
}

/// An overmap terrain an entry makes and where its square starts in the rows.
struct named_square {
    std::string name;
    point origin;
};

/// Every overmap terrain an entry's `om_terrain` makes: each of a list of names on the whole rows.
auto entry_names(const nlohmann::ordered_json& value) -> std::vector<named_square> {
    if (value.contains("om_terrain") && value["om_terrain"].is_array()
        && !value["om_terrain"].empty() && value["om_terrain"].front().is_string()) {
        return value["om_terrain"] | std::views::filter(&nlohmann::ordered_json::is_string)
             | std::views::transform([](const auto& name) {
                   return named_square{
                       .name = name.template get<std::string>(), .origin = point_zero};
               })
             | std::ranges::to<std::vector>();
    }
    auto result = std::vector<named_square>{};
    for (const auto [y, row] : std::views::enumerate(entry_terrains(value))) {
        for (const auto [x, name] : std::views::enumerate(row)) {
            result.push_back(
                {.name = name,
                 .origin = point(static_cast<int>(x) * SEEX * 2, static_cast<int>(y) * SEEY * 2)});
        }
    }
    return result;
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

auto index_levels() -> level_index {
    auto index = level_index{};
    for (const auto& special : overmap_specials::get_all()) {
        auto& layout = index.layouts.emplace_back();
        for (const auto& location : special.required_locations()) {
            if (auto terrain = placed(special.get_terrain_at(location.p))) {
                layout[location.p.raw()] = std::move(*terrain);
            }
        }
    }
    return index;
}

auto terrain_above(const level_index& index, const std::string& om_terrain, const int dz)
    -> std::optional<std::string> {
    for (const auto& place : places_of(index, om_terrain)) {
        const auto found = place.layout->find(place.at + tripoint(0, 0, dz));
        if (found != place.layout->end() && found->second.name != om_terrain) {
            return found->second.name;
        }
    }
    return std::nullopt;
}

auto terrain_level(const level_index& index, const std::string& om_terrain) -> std::optional<int> {
    const auto places = places_of(index, om_terrain);
    return places.empty() ? std::nullopt : std::optional(places.front().at.z);
}

auto square_making(
    const std::vector<document_object>& entries, const std::string& om_terrain,
    const std::optional<std::size_t> skipped) -> std::optional<entry_square> {
    for (const auto [index, entry] : std::views::enumerate(entries)) {
        if (skipped == static_cast<std::size_t>(index)) { continue; }
        for (const auto& square : entry_names(entry.value)) {
            if (square.name == om_terrain) {
                return entry_square{
                    .entry = static_cast<std::size_t>(index), .origin = square.origin};
            }
        }
    }
    return std::nullopt;
}

auto stacked_squares(
    const level_index& index, const std::vector<document_object>& entries, const std::size_t entry,
    const int dz) -> std::vector<stacked_square> {
    auto result = std::vector<stacked_square>{};
    const auto rows = entry_terrains(entries.at(entry).value);
    for (const auto [y, row] : std::views::enumerate(rows)) {
        for (const auto [x, name] : std::views::enumerate(row)) {
            const auto above = terrain_above(index, name, dz);
            if (!above) { continue; }
            // A level of the entry itself is the canvas already.
            if (const auto other = square_making(entries, *above, entry)) {
                result.push_back(
                    {.origin = point(static_cast<int>(x) * SEEX * 2, static_cast<int>(y) * SEEY * 2),
                     .other = *other});
            }
        }
    }
    return result;
}

} // namespace editor
