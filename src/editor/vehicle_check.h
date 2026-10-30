#pragma once

#include "editor/vehicle_design.h"
#include "type_id.h"

#include <expected>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class vehicle;

namespace editor {

/// What a click does to make a design pass a check it fails.
enum class drive_fix {
    none,
    /// Installs wheels, with their hubs, on the corners of the frame.
    add_wheels,
    /// Swaps the hubs of the front wheels for their steerable kind.
    steer_front,
    /// Shows the engines among the parts, for the player to pick one.
    show_engines,
    /// Fills the tanks with the engines' fuel, or installs a tank of it when there is none.
    fill_tanks,
    add_battery,
    /// Installs controls on a seat, and the seat when there is none.
    add_controls,
};

/// One line of whether a vehicle can be driven away, and how to make it so when it cannot.
struct drive_check {
    std::string key;
    bool ok = false;
    std::string text;
    drive_fix fix = drive_fix::none;
    /// What the fix's button says, localized; empty for none.
    std::string action = {};
};

/// What the vehicle needs to be driven: wheels it stands and steers on, an engine strong enough
/// with its fuel and a battery to start it, and controls by a seat; `held` names the fuel the
/// vehicle's part of that index holds, which a prototype's blueprint does not hold itself.
auto drive_checks(const vehicle& veh, const std::function<auto(int)->itype_id>& held)
    -> std::vector<drive_check>;

/// Whether every check passed.
auto ready_to_drive(const std::vector<drive_check>& checks) -> bool;

/// The prototype with what `fix` installs or sets on it, its blueprint drawn again; the error
/// says why it cannot. `show_engines` changes nothing.
auto fixed_design(
    const nlohmann::ordered_json& prototype, const part_catalog& catalog, drive_fix fix)
    -> std::expected<nlohmann::ordered_json, std::string>;

} // namespace editor
