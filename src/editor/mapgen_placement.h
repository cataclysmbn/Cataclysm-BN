#pragma once

#include <cstddef>
#include <expected>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
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
