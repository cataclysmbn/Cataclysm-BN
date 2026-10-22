#pragma once

#include "coordinates.h"
#include "editor/mapgen_check.h"
#include "engine_client_component.h"
#include "point.h"
#include "type_id.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class vehicle;

namespace editor {

/// One part of a vehicle prototype as its JSON names it, in the order the loader installs them.
struct design_part {
    point mount;
    std::string part;
    /// The JSON pointer of the value that names it: a `parts` entry, a member of one, or a
    /// palette entry.
    std::string pointer;
    /// Whether the blueprint's palette places it, so it is changed through the palette, not here.
    bool from_palette = false;
};

auto design_parts(const nlohmann::ordered_json& prototype) -> std::vector<design_part>;

/// What the game builds from a prototype's JSON, without registering it.
struct built_design {
    /// The blueprint vehicle; none when the JSON cannot be read as a prototype.
    std::unique_ptr<vehicle> blueprint;
    /// What the loader and the finalizing said, in order.
    std::vector<loader_message> messages;
};

auto build_design(std::string_view text) -> built_design;

/// Registers the prototype the JSON defines in place of the game's one of that id, as loading it
/// again would, so that whatever spawns that id from then on builds this design; the error says
/// why the game cannot build it.
auto apply_design(std::string_view text) -> std::expected<vproto_id, std::string>;

struct spawn_options {
    /// Degrees clockwise from east, as the game turns vehicles.
    int facing = 0;
    /// Percent of each tank and battery filled.
    int fuel = 100;
    /// The game's spawn condition: -1 lightly damaged, 0 intact, 1 disabled.
    int condition = 0;
};

/// Spawns the prototype `id` on the map at `at`; none when something there is in the way.
auto spawn_design(const vproto_id& id, const tripoint_bub_ms& at, const spawn_options& options)
    -> vehicle*;

struct part_install {
    point mount;
    std::string part;
};

/// The prototype with the part installed at its mount, after the parts already there, its
/// `blueprint` is drawn again; the error says why the game would not mount it on `blueprint`, the
/// prototype's vehicle now.
auto install_part(
    const nlohmann::ordered_json& prototype, const vehicle& blueprint, const part_install& install)
    -> std::expected<nlohmann::ordered_json, std::string>;

/// The prototype without the part `index` of `design_parts`, its `blueprint` drawn again; the
/// error says why it stays.
auto remove_part(
    const nlohmann::ordered_json& prototype, const vehicle& blueprint, std::size_t index)
    -> std::expected<nlohmann::ordered_json, std::string>;

/// The rows of a blueprint drawn from the parts the vehicle shows, west to east and north to
/// south.
auto blueprint_rows(const vehicle& veh) -> std::vector<std::string>;

/// The facts of a prototype's vehicle a designer weighs: speed, power, mass, cargo.
auto design_stats(const vehicle& veh) -> std::vector<engine_client::panel_stat>;

/// The data file of the loaded mods that defines the prototype `id` last, as the game loads them.
auto prototype_file(const std::string& id) -> std::optional<std::filesystem::path>;

} // namespace editor
