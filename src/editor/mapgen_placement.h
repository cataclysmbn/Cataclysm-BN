#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class document;

/// The squares a placement's `x` or `y` may land on, both ends included.
struct placement_range {
    int min = 0;
    int max = 0;
    auto operator<=>(const placement_range&) const = default; // *NOPAD*
};

/// One entry of a `place_*` array of a mapgen's object.
struct placement {
    /// The array: "place_items", "place_monster", ...
    std::string kind;
    std::size_t index = 0;
    placement_range x;
    placement_range y;
    nlohmann::ordered_json value;
};

/// Every placement of object `object` of `doc`, array by array in the object's order.
auto placements_of(const document& doc, std::size_t object) -> std::vector<placement>;

struct placement_move {
    std::size_t object = 0;
    std::string kind;
    std::size_t index = 0;
    placement_range x;
    placement_range y;
};

/// Moves or resizes a placement as one undo step; a range of one square is written as a number.
auto move_placement(document& doc, const placement_move& move) -> std::expected<void, std::string>;

/// The `place_*` arrays the loader reads, in its order.
inline constexpr auto placement_kinds = std::to_array<std::string_view>(
    {"place_item",
     "place_fields",
     "place_npcs",
     "place_signs",
     "place_vendingmachines",
     "place_toilets",
     "place_liquids",
     "place_gaspumps",
     "place_items",
     "place_loot",
     "place_artifact",
     "place_monsters",
     "place_vehicles",
     "place_traps",
     "place_furniture",
     "place_terrain",
     "place_monster",
     "place_rubble",
     "place_computers",
     "place_nested",
     "place_graffiti",
     "place_zones",
     "place_ter_furn_transforms",
     "place_remove_all"});

struct placement_add {
    std::size_t object = 0;
    std::string kind;
    placement_range x;
    placement_range y;
    /// Its members other than `x` and `y`.
    nlohmann::ordered_json value;
};

/// Adds a placement at the end of its array as one undo step, unless the loader then says
/// something it did not say before; the error names what it says.
auto add_placement(document& doc, const placement_add& add) -> std::expected<void, std::string>;

struct placement_at {
    std::size_t object = 0;
    std::string kind;
    std::size_t index = 0;
};

/// Removes a placement as one undo step, and its array when it was the last.
auto remove_placement(document& doc, const placement_at& at) -> std::expected<void, std::string>;

struct field_edit {
    std::size_t object = 0;
    /// RFC 6901 pointer into the entry.
    std::string pointer;
    /// The new value; none removes the member or element.
    std::optional<nlohmann::ordered_json> value = std::nullopt;
};

/// Sets any value of a mapgen entry as one undo step, unless the game's loader then says something
/// it did not say before; the error names what it says.
auto set_field(document& doc, const field_edit& edit) -> std::expected<void, std::string>;

} // namespace editor
