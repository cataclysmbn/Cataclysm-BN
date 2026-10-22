#pragma once

namespace editor {

/// The vehicle editor screen a protocol client draws: opens a vehicle prototype's data file by the
/// prototype's id, shows the vehicle the game builds from it, installs and removes parts, sets any
/// field, undoes and saves; returns when the player leaves it, keeping the open document for the
/// next visit.
auto run_vehicle_editor() -> void;

/// Forgets the document the vehicle editor keeps between visits, for a new game.
auto reset_vehicle_editor() -> void;

} // namespace editor
