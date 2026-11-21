#pragma once

#include "coordinates.h"
#include "point.h"
#include "type_id.h"

#include <compare>
#include <map>
#include <optional>
#include <set>
#include <string>

class Character;
class JsonIn;
class JsonOut;

/// Translocator gate position. Same OMT coordinates in other dimensions are different gates.
struct translocator_location {
    dimension_id dim;
    tripoint_abs_omt pos;
    auto operator<=>(const translocator_location&) const -> std::weak_ordering = default;
};

class teleporter_list {
private:
    // OMT locations of all known teleporters
    std::map<translocator_location, std::string> known_teleporters;
    // ui for selection of desired teleport location.
    // returns overmap tripoint, or nullopt if canceled
    auto choose_teleport_location() -> std::optional<tripoint_abs_omt>;
    // returns true if a teleport is successful
    // does not do any loading or unloading
    auto place_avatar_overmap(Character& you, const tripoint_abs_omt& omt_pt) const -> bool;

public:
    auto knows_translocator(const translocator_location& loc) const -> bool;
    // adds teleporter to known_teleporters and does any other activation necessary
    auto activate_teleporter(const translocator_location& loc) -> bool;
    void deactivate_teleporter(const translocator_location& loc);

    // calls the necessary functions to select translocator location
    // and teleports the target(s) there
    void translocate(const std::set<tripoint_bub_ms>& targets);

    void serialize(JsonOut& json) const;
    void deserialize(JsonIn& jsin);
};
