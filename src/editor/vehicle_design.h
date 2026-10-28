#pragma once

#include "coordinates.h"
#include "editor/document.h"
#include "editor/mapgen_check.h"
#include "engine_client_component.h"
#include "engine_client_state.h"
#include "point.h"
#include "type_id.h"

#include <expected>
#include <filesystem>
#include <functional>
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

/// The mounts a stroke installs one part on.
struct stroke_install {
    std::vector<point> mounts;
    std::string part;
};
/// How a stroke went: the mounts that took the part, those that refused it, and why the first of
/// them did.
struct stroke_tally {
    int installed = 0;
    int refused = 0;
    std::string reason = {};
};
struct stroke_design {
    nlohmann::ordered_json prototype;
    stroke_tally tally;
};

/// The prototype with the part installed on each mount of the stroke the game takes it on, again
/// and again while one more does, so that frames spread from the frame there is; a mount that
/// already has the part is left as it is. Its `blueprint` is drawn again.
auto install_parts(const nlohmann::ordered_json& prototype, const stroke_install& install)
    -> std::expected<stroke_design, std::string>;

/// The prototype without the part `index` of `design_parts`, its `blueprint` drawn again; the
/// error says why it stays.
auto remove_part(
    const nlohmann::ordered_json& prototype, const vehicle& blueprint, std::size_t index)
    -> std::expected<nlohmann::ordered_json, std::string>;

/// The prototype with the part `index` of `design_parts` swapped with the part installed before
/// it on its mount, for a negative `step`, or after it; the error says why it stays.
auto move_part(const nlohmann::ordered_json& prototype, std::size_t index, int step)
    -> std::expected<nlohmann::ordered_json, std::string>;

struct part_member {
    /// The part, as an index of `design_parts`.
    std::size_t index = 0;
    /// The member of its JSON, such as `fuel` or `ammo`.
    std::string key;
    /// None removes the member.
    std::optional<nlohmann::ordered_json> value = std::nullopt;
};

/// The prototype with the member of a part set or removed: a part named by its id alone becomes
/// an object, and one left with its id alone is named by it again; the error says why the game
/// would not load it.
auto set_part_member(const nlohmann::ordered_json& prototype, const part_member& change)
    -> std::expected<nlohmann::ordered_json, std::string>;

/// The rows of a blueprint drawn from the parts the vehicle shows, whole and closed, west to east
/// and north to south.
auto blueprint_rows(const vehicle& veh) -> std::vector<std::string>;

/// The facts of a prototype's vehicle a designer weighs: speed, power, mass, cargo.
auto design_stats(const vehicle& veh) -> std::vector<engine_client::panel_stat>;

/// A prototype `id` of the vehicle's design, as the game would build it again: its name, parts by
/// mount in the order they were installed, each tank's fuel, and a blueprint; not its state, such
/// as damage, fill levels or cargo.
auto export_design(const vehicle& veh, const std::string& id) -> nlohmann::ordered_json;

/// The vehicle as the save stores it, with its whole state: parts, damage, fuel, cargo, name.
auto vehicle_state(const vehicle& veh) -> std::string;

/// An exact copy of the saved vehicle `state` placed on the map at `at`, facing as it faced; the
/// error says why it stays off the map.
auto place_vehicle_state(const std::string& state, const tripoint_bub_ms& at)
    -> std::expected<vehicle*, std::string>;

/// The saved `state` in place of the vehicle, where it stands; the error says why it stays off
/// the map, and the vehicle is gone then.
auto restore_vehicle_state(vehicle& veh, const std::string& state)
    -> std::expected<vehicle*, std::string>;

/// Why the game would not mount the part `id` at `at` on the vehicle, in words; none when it
/// would.
auto mount_refusal(const vehicle& veh, const point& at, const vpart_id& id)
    -> std::optional<std::string>;

/// `mount_refusal` for many mounts of `veh`, which stays as it is while the result is used.
auto mount_refusals(const vehicle& veh, const vpart_id& id)
    -> std::function<auto(const point&)->std::optional<std::string>>;

/// Installs the part on a mount of the world's vehicle at once, without the game's requirements;
/// the part's index, or why the game would not mount it.
auto install_live_part(vehicle& veh, const part_install& install)
    -> std::expected<int, std::string>;

/// `install_parts` on the world's vehicle, at once, without the game's requirements.
auto install_live_parts(vehicle& veh, const stroke_install& install)
    -> std::expected<stroke_tally, std::string>;

/// Removes the world's vehicle's part `index` at once; the error says why it stays.
auto remove_live_part(vehicle& veh, int index) -> std::expected<void, std::string>;

/// Sets the health of the vehicle's part `index` in percent of its durability.
auto set_part_health(vehicle& veh, int index, int percent) -> std::expected<void, std::string>;

struct part_fuel {
    /// The fuel, battery charge or ammunition item.
    std::string fuel;
    /// How much of it, capped by what the part holds; 0 empties it.
    int amount = 0;
};

/// The liquids a prototype's tank may hold, by name.
auto tank_fuels() -> std::vector<itype_id>;

/// The rounds a prototype's turret part fires, by name; none for a part that is no turret.
auto turret_rounds(const vpart_id& id) -> std::vector<itype_id>;

/// What the vehicle's part `index` may be filled with, by name: a tank's liquids, else the
/// charges or rounds its item takes.
auto part_fuels(const vehicle& veh, int index) -> std::vector<itype_id>;

/// Fills the tank, battery or magazine of the vehicle's part `index`.
auto set_part_fuel(vehicle& veh, int index, const part_fuel& fuel)
    -> std::expected<void, std::string>;

/// Puts an item of the type `item_id` in the cargo space of the vehicle's part `index`.
auto add_part_cargo(vehicle& veh, int index, const std::string& item_id)
    -> std::expected<void, std::string>;

/// What the parts on the mount `at` of the vehicle look like from the bottom up: in the order the
/// game installed them, the one the game shows on top; turned `facing` degrees clockwise.
auto mount_looks(const vehicle& veh, const point& at, int facing)
    -> std::vector<engine_client::look>;

/// What an item spawn of a prototype's `items` puts, as the game names it: an item by its name,
/// a group by its id.
auto item_spawn_name(const nlohmann::ordered_json& entry) -> std::string;

/// What an edit of a prototype did, in a designer's words: the part installed, removed, reordered
/// or set and its mount, or the item spawn added or removed; else what `step_label` says.
auto design_step_label(const document_edit& edit) -> std::string;

/// What a vehicle part is for, as the editor's palette heads its parts: the game names no
/// categories of its own, so they follow from where a part goes and its flags.
enum class part_category {
    frames,
    wheels,
    engines,
    fuel,
    seats,
    doors,
    lights,
    storage,
    weapons,
    other,
};

auto category_of(const vpart_info& part) -> part_category;

/// The heading of `category`, localized.
auto category_name(part_category category) -> std::string;

/// One part as a designer picks it: its shapes, the parts of one name and item that differ only in
/// how they look, as the game's install menu takes them, the one the game's prototypes install
/// most first.
struct part_kind {
    std::vector<const vpart_info*> shapes;
    part_category category = part_category::other;
};

/// The game's parts as a designer picks them, read once for an editor visit, since the game's data
/// stays as it is while the editor is open.
struct part_catalog {
    /// Every kind of part the game has, by category and then by name.
    std::vector<part_kind> kinds;
    /// The liquids the game's prototypes fill their parts with, most often first.
    std::vector<itype_id> tank_fuels;
    /// The frame the game's prototypes install most, which a new design starts with; none when
    /// they install none.
    std::optional<vpart_id> frame;
    /// The shapes of the part `id`, as its kind has them; none for a part the game lacks.
    auto shapes_of(const vpart_id& id) const -> std::vector<const vpart_info*>;
};

auto read_part_catalog() -> part_catalog;

/// The names of `parts` as a list of them shows them: a name two of them share is followed by
/// what tells their ids apart.
auto part_labels(const std::vector<const vpart_info*>& parts) -> std::vector<std::string>;

/// The data file of the loaded mods that defines the prototype `id` last, as the game loads them.
auto prototype_file(const std::string& id) -> std::optional<std::filesystem::path>;

} // namespace editor
