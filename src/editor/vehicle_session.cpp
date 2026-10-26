#include "editor/vehicle_session.h"

#include "avatar.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/vehicle_design.h"
#include "engine_client_component.h"
#include "engine_client_world.h"
#include "input.h"
#include "item_group.h"
#include "json_formatter.h"
#include "line.h"
#include "map/map.h"
#include "output.h"
#include "path_info.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "units_utility.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace editor {
namespace {

namespace ec = engine_client;

constexpr auto prototypes_id = "vehicle:prototypes";
constexpr auto grid_id = "vehicle:grid";
constexpr auto stack_id = "vehicle:stack";
constexpr auto parts_id = "vehicle:parts";
constexpr auto field_id = "vehicle:field";
constexpr auto sources_id = "vehicle:sources";
constexpr auto tools_id = "vehicle:tools";
constexpr auto props_id = "vehicle:props";
constexpr auto part_form_id = "vehicle:part_form";
constexpr auto items_id = "vehicle:items";

constexpr auto open_id = "vehicle:open";
constexpr auto filter_id = "vehicle:filter";
constexpr auto page_id = "vehicle:page";
constexpr auto mount_id = "vehicle:mount";
constexpr auto pick_id = "vehicle:pick";
constexpr auto remove_id = "vehicle:remove";
constexpr auto earlier_id = "vehicle:earlier";
constexpr auto later_id = "vehicle:later";
constexpr auto install_id = "vehicle:install";
constexpr auto part_filter_id = "vehicle:part_filter";
constexpr auto part_page_id = "vehicle:part_page";
constexpr auto field_set_id = "vehicle:field_set";
constexpr auto field_apply_id = "vehicle:field_apply";
constexpr auto undo_id = "vehicle:undo";
constexpr auto redo_id = "vehicle:redo";
constexpr auto save_id = "vehicle:save";
constexpr auto save_as_id = "vehicle:save_as";
constexpr auto world_id = "vehicle:world";
constexpr auto export_id = "vehicle:export";
constexpr auto spawn_form_id = "vehicle:spawn_form";
constexpr auto spawn_set_id = "vehicle:spawn_set";
constexpr auto spawn_id = "vehicle:spawn";
constexpr auto copy_id = "vehicle:copy";
constexpr auto paste_id = "vehicle:paste";
constexpr auto edit_id = "vehicle:edit";
constexpr auto done_id = "vehicle:done";
constexpr auto live_form_id = "vehicle:live_form";
constexpr auto live_set_id = "vehicle:live_set";
constexpr auto source_id = "vehicle:source";
constexpr auto tool_id = "vehicle:tool";
constexpr auto prop_set_id = "vehicle:prop_set";
constexpr auto part_set_id = "vehicle:part_set";
constexpr auto item_add_id = "vehicle:item_add";
constexpr auto item_remove_id = "vehicle:item_remove";
constexpr auto item_chance_id = "vehicle:item_chance";

/// How far east of the avatar a spawned vehicle stands, clear of it.
constexpr auto spawn_distance = 8;

constexpr auto page_size = std::size_t{50};

auto pages_of(const std::size_t total) -> std::int64_t {
    return static_cast<std::int64_t>(std::max<std::size_t>(1, (total + page_size - 1) / page_size));
}

auto mount_text(const point& at) -> std::string { return string_format("mount:%d,%d", at.x, at.y); }

auto mount_of(const std::string& id) -> std::optional<point> {
    auto at = point{};
    if (std::sscanf(id.c_str(), "mount:%d,%d", &at.x, &at.y) != 2) { return std::nullopt; }
    return at;
}

/// Whether `path` lies inside `root`.
auto inside(const std::filesystem::path& path, const std::string& root) -> bool {
    auto error = std::error_code{};
    const auto base = std::filesystem::weakly_canonical(root, error);
    const auto relative = std::filesystem::weakly_canonical(path, error).lexically_relative(base);
    return !error && !relative.empty() && *relative.begin() != "..";
}

/// `path` from the game's directory, the one that holds `data`, when it lies inside it, as a
/// modder finds it there.
auto shown_path(const std::filesystem::path& path) -> std::string {
    const auto game = (std::filesystem::absolute(PATH_INFO::datadir()) / "..").lexically_normal();
    const auto relative = std::filesystem::absolute(path).lexically_normal().lexically_proximate(
        game);
    return (relative.empty() || *relative.begin() == ".." ? path : relative).generic_string();
}

/// The file a copied vehicle waits in, outside every world so that another world can paste it.
/// The trimmed entries of a comma-separated list, as a JSON array; none when it lists nothing.
auto listed(const std::string& text) -> std::optional<nlohmann::ordered_json> {
    auto entries =
        string_split(text, ',') | std::views::transform(trim)
        | std::views::filter([](const std::string& entry) { return !entry.empty(); })
        | std::ranges::to<std::vector>();
    if (entries.empty()) { return std::nullopt; }
    return nlohmann::ordered_json(std::move(entries));
}

/// A JSON array of strings as a comma-separated list.
auto list_text(const nlohmann::ordered_json& value) -> std::string {
    if (!value.is_array()) { return {}; }
    return value | std::views::transform([](const nlohmann::ordered_json& entry) {
               return entry.is_string() ? entry.get<std::string>() : entry.dump();
           })
         | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>();
}

auto copied_vehicle_file() -> std::filesystem::path {
    return std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
}

struct session {
    std::optional<document> doc = std::nullopt;
    /// The prototype's object in `doc`.
    std::size_t object = 0;
    std::string notice;
    /// What the game builds from the object now.
    built_design built;
    point cursor;
    std::string filter;
    std::size_t page = 0;
    std::string part_filter;
    std::size_t part_page = 0;
    /// What the field form sets: a pointer into the prototype and a value as JSON or bare text;
    /// an empty value removes it.
    std::string field_pointer = "/name";
    std::string field_value;
    spawn_options spawning;
    /// The tab of the left column: `prototypes` or `world`.
    std::string source_tab = "prototypes";
    /// The tab of the right column: `mount`, `install`, `part`, `vehicle`, `spawn` or `json`.
    std::string tool_tab = "mount";
    /// The world's vehicle edited in place, the part picked on it, and its saved states before
    /// each change to undo and after each undone one to redo.
    vehicle* live = nullptr;
    int live_part = -1;
    std::vector<std::string> live_undo;
    std::vector<std::string> live_redo;

    auto prototype() const -> const nlohmann::ordered_json& { // *NOPAD*
        return doc->objects()[object].value;
    }

    auto prototypes() const -> catalog_page {
        return search_catalog(
            {.kind = catalog_kind::vehicle, .filter = filter, .page = page, .page_size = page_size});
    }

    /// The parts that match the filter, ordered by id: every part on all pages, and those on this
    /// one.
    struct part_page_entries {
        std::vector<const vpart_info*> shown;
        std::size_t total = 0;
    };
    auto parts() const -> part_page_entries {
        auto found =
            vpart_info::get_all() | std::views::filter([&](const vpart_info& info) {
                return part_filter.empty() || lcmatch(info.get_id().str(), part_filter)
                    || lcmatch(info.name(), part_filter);
            })
            | std::views::transform([](const vpart_info& info) { return &info; })
            | std::ranges::to<std::vector>();
        std::ranges::sort(found, {}, [](const vpart_info* info) { return info->get_id().str(); });
        const auto total = found.size();
        const auto first = std::min(total, part_page * page_size);
        return {.shown = {found.begin() + first, found.begin() + std::min(total, first + page_size)},
                .total = total};
    }

    auto reload() -> void {
        built = build_design(prototype().dump());
        field_value = value_at(field_pointer);
    }

    /// The JSON at `pointer` in the prototype; empty when there is none.
    auto value_at(const std::string& pointer) const -> std::string {
        try {
            const auto at = nlohmann::ordered_json::json_pointer(pointer);
            if (!prototype().contains(at)) { return {}; }
            const auto& value = prototype()[at];
            return value.is_string() ? value.get<std::string>() : value.dump();
        } catch (const nlohmann::ordered_json::exception&) { return {}; }
    }

    auto open(const std::string& id) -> void {
        const auto path = prototype_file(id);
        if (!path) {
            notice = string_format(_("No data file of the loaded mods defines %s."), id);
            return;
        }
        open_file(*path, id);
    }

    /// Opens the prototype `id` of the data file at `path`.
    auto open_file(const std::filesystem::path& path, const std::string& id) -> void {
        auto opened = document::open(path);
        if (!opened) {
            notice = opened.error();
            return;
        }
        const auto& objects = opened->objects();
        const auto found = std::ranges::find_if(objects, [&](const document_object& entry) {
            return entry.value.is_object() && entry.value.value("type", "") == "vehicle"
                && entry.value.value("id", "") == id;
        });
        if (found == objects.end()) {
            notice = string_format(_("No data file of the loaded mods defines %s."), id);
            return;
        }
        object = static_cast<std::size_t>(found - objects.begin());
        doc = std::move(*opened);
        cursor = point{};
        field_pointer = "/name";
        reload();
    }

    /// Replaces the prototype by `next` as one undo step.
    auto change(const std::expected<nlohmann::ordered_json, std::string>& next) -> void {
        if (!next) {
            notice = next.error();
            return;
        }
        const auto edited = doc->edit(object, nlohmann::ordered_json::diff(prototype(), *next));
        if (!edited) { notice = edited.error(); }
        reload();
    }

    auto install(const std::string& part) -> void {
        if (!built.blueprint) {
            notice = _("The game cannot build this prototype; fix what it reports first.");
            return;
        }
        change(install_part(prototype(), *built.blueprint, {.mount = cursor, .part = part}));
    }

    auto remove(const std::size_t index) -> void {
        if (!built.blueprint) {
            notice = _("The game cannot build this prototype; fix what it reports first.");
            return;
        }
        change(remove_part(prototype(), *built.blueprint, index));
    }

    auto set_field() -> void {
        auto next = prototype();
        try {
            const auto at = nlohmann::ordered_json::json_pointer(field_pointer);
            if (field_value.empty()) {
                next.at(at.parent_pointer()).erase(at.back());
            } else {
                const auto parsed = nlohmann::ordered_json::parse(field_value, nullptr, false);
                next[at] = parsed.is_discarded() ? nlohmann::ordered_json(field_value) : parsed;
            }
        } catch (const nlohmann::ordered_json::exception& e) {
            notice = e.what();
            return;
        }
        if (!next.is_object() || next.value("type", "") != "vehicle" || !next.contains("id")) {
            notice = _("A prototype keeps its type and id.");
            return;
        }
        change(next);
    }

    /// Sets the prototype's name, id, colour palette or flags; empty removes the last two.
    auto set_prop(const std::string& field, const std::string& text) -> void {
        const auto optional = field == "color_palette" || field == "flags";
        if (text.empty() && !optional) {
            notice = _("A prototype keeps its name and id.");
            return;
        }
        auto next = prototype();
        const auto value =
            field == "flags" ? listed(text)
            : text.empty()
                ? std::nullopt
                : std::optional(nlohmann::ordered_json(text));
        if (value) {
            next[field] = *value;
        } else {
            next.erase(field);
        }
        const auto before = build_design(prototype().dump()).messages.size();
        if (const auto built = build_design(next.dump()); built.messages.size() > before) {
            notice = built.messages.back().text;
            return;
        }
        change(next);
    }

    /// The index in `design_parts` of the part the field form points at; none when it points at
    /// no part.
    auto picked_part() const -> std::optional<std::size_t> {
        const auto design = design_parts(prototype());
        const auto found = std::ranges::find(design, field_pointer, &design_part::pointer);
        return found == design.end()
                 ? std::nullopt
                 : std::optional(static_cast<std::size_t>(found - design.begin()));
    }

    /// Sets the member `key` of the picked part from the part form: a number for `ammo`, a list
    /// for `ammo_types`, either end of `ammo_qty` for `ammo_min` and `ammo_max`, text for the
    /// rest; empty removes it.
    auto set_part(const std::string& key, const std::string& text) -> void {
        const auto index = picked_part();
        if (!index) {
            notice = _("Pick a part first.");
            return;
        }
        if (key == "ammo_types") {
            change(
                set_part_member(prototype(), {.index = *index, .key = key, .value = listed(text)}));
            return;
        }
        if (key == "ammo_min" || key == "ammo_max") {
            set_ammo_qty(*index, key == "ammo_max", text);
            return;
        }
        auto value = std::optional<nlohmann::ordered_json>{};
        if (!text.empty()) {
            value = nlohmann::ordered_json(text);
            if (key == "ammo") {
                auto number = 0;
                const auto* const end = text.data() + text.size();
                if (const auto [ptr, error] = std::from_chars(text.data(), end, number);
                    error != std::errc{} || ptr != end || number < 0 || number > 100) {
                    notice = _("That value is out of range.");
                    return;
                }
                value = number;
            }
        }
        change(set_part_member(prototype(), {.index = *index, .key = key, .value = value}));
    }

    /// Sets the fewest (or, `most`, the most) rounds a turret spawns loaded with; a turret with
    /// no range is full, so a first end sets both, and empty makes it full again.
    auto set_ammo_qty(const std::size_t index, const bool most, const std::string& text) -> void {
        auto value = std::optional<nlohmann::ordered_json>{};
        if (!text.empty()) {
            auto number = 0;
            const auto* const end = text.data() + text.size();
            if (const auto [ptr, error] = std::from_chars(text.data(), end, number);
                error != std::errc{} || ptr != end || number < 0) {
                notice = _("That value is out of range.");
                return;
            }
            const auto current = nlohmann::ordered_json::parse(
                value_at(design_parts(prototype())[index].pointer + "/ammo_qty"), nullptr, false);
            auto range =
                current.is_array() && current.size() == 2 && current[0].is_number_integer()
                        && current[1].is_number_integer()
                    ? std::array{current[0].get<int>(), current[1].get<int>()}
                    : std::array{number, number};
            range[most ? 1 : 0] = number;
            if (range[0] > range[1]) {
                notice = _("A turret cannot load fewer rounds at most than at least.");
                return;
            }
            value = range;
        }
        change(set_part_member(prototype(), {.index = index, .key = "ammo_qty", .value = value}));
    }

    /// The indexes in the prototype's `items` of the item spawns on the picked mount.
    auto item_spawns() const -> std::vector<std::size_t> {
        const auto items = prototype().value("items", nlohmann::ordered_json::array());
        if (!items.is_array()) { return {}; }
        return std::views::iota(std::size_t{0}, items.size())
             | std::views::filter([&](const std::size_t index) {
                   return items[index].value("x", 0) == cursor.x
                       && items[index].value("y", 0) == cursor.y;
               })
             | std::ranges::to<std::vector>();
    }

    /// Adds a spawn of the item or item group `id` on the picked mount, every time.
    auto add_item(const std::string& id) -> void {
        const auto group = item_group::group_is_defined(item_group_id(id));
        if (!group && !itype_id(id).is_valid()) {
            notice = _("No item or item group has that id.");
            return;
        }
        auto next = prototype();
        auto entry = nlohmann::ordered_json{{"x", cursor.x}, {"y", cursor.y}, {"chance", 100}};
        entry[group ? "item_groups" : "items"] = nlohmann::ordered_json::array({id});
        next["items"].push_back(std::move(entry));
        change(next);
    }

    /// Removes the item spawn `index` of the prototype's `items`, or sets the percent chance it
    /// spawns.
    auto edit_item(const std::size_t index, const std::optional<int> chance) -> void {
        if (!std::ranges::contains(item_spawns(), index)) { return; }
        auto next = prototype();
        if (chance) {
            next["items"][index]["chance"] = *chance;
        } else {
            next["items"].erase(index);
        }
        change(next);
    }

    /// Writes the design of the world's vehicle `index` as a prototype in a new file of the user
    /// mods, and opens it.
    auto export_vehicle(const std::size_t index, const std::string& name) -> void {
        const auto vehicles = get_map().get_vehicles();
        const auto path = std::filesystem::path(name);
        if (index >= vehicles.size()) { return; }
        if (path.extension() != ".json" || !inside(path, PATH_INFO::user_moddir())) {
            notice = string_format(
                _("A design is exported to a new .json file in your mods, %s."),
                PATH_INFO::user_moddir());
            return;
        }
        if (std::filesystem::exists(path)) {
            notice = _("That file already exists; nothing was saved.");
            return;
        }
        const auto id = path.stem().string();
        const auto design = nlohmann::ordered_json::array({export_design(*vehicles[index].v, id)});
        auto error = std::error_code{};
        std::filesystem::create_directories(path.parent_path(), error);
        auto out = std::ofstream(path, std::ios::binary);
        out << format_json(design.dump()).value_or(design.dump(2));
        if (error || !out.flush()) {
            notice = _("The file could not be written.");
            return;
        }
        out.close();
        open_file(path, id);
    }

    /// Keeps the world's vehicle `index` with its whole state in a file of the user directory, so
    /// any world can paste it.
    auto copy_vehicle(const std::size_t index) -> void {
        const auto vehicles = get_map().get_vehicles();
        if (index >= vehicles.size()) { return; }
        auto out = std::ofstream(copied_vehicle_file(), std::ios::binary);
        out << vehicle_state(*vehicles[index].v);
        if (!out.flush()) { notice = _("The file could not be written."); }
    }

    /// Places an exact copy of the copied vehicle south of the avatar, clear of a spawned one.
    auto paste_vehicle() -> void {
        auto in = std::ifstream(copied_vehicle_file(), std::ios::binary);
        if (!in) {
            notice = _("No vehicle is copied.");
            return;
        }
        const auto state = std::string(std::istreambuf_iterator<char>(in), {});
        const auto at = get_avatar().bub_pos() + tripoint_rel_ms(0, spawn_distance, 0);
        if (const auto placed = place_vehicle_state(state, at); !placed) {
            notice = placed.error();
        }
    }

    /// Edits the world's vehicle `index` in place.
    auto edit_vehicle(const std::size_t index) -> void {
        const auto vehicles = get_map().get_vehicles();
        if (index >= vehicles.size()) { return; }
        live = vehicles[index].v;
        cursor = point{};
        live_part = -1;
        live_undo.clear();
        live_redo.clear();
    }

    /// Forgets the vehicle edited in place once it left the map.
    auto check_live() -> void {
        if (live && std::ranges::none_of(get_map().get_vehicles(), [&](const auto& placed) {
                return placed.v == live;
            })) {
            live = nullptr;
        }
    }

    /// Runs `edit` on the vehicle edited in place as one undo step.
    auto live_change(const std::function<auto(vehicle&)->std::expected<void, std::string>>& edit)
        -> void {
        auto before = vehicle_state(*live);
        if (const auto done = edit(*live); !done) {
            notice = done.error();
            return;
        }
        live_undo.push_back(std::move(before));
        live_redo.clear();
    }

    /// Puts the vehicle edited in place back to the last state of `from`, keeping the current one
    /// in `to`.
    auto live_step(std::vector<std::string>& from, std::vector<std::string>& to) -> void {
        if (from.empty()) { return; }
        auto now = vehicle_state(*live);
        const auto restored = restore_vehicle_state(*live, from.back());
        from.pop_back();
        if (!restored) {
            notice = restored.error();
            live = nullptr;
            return;
        }
        live = *restored;
        live_part = -1;
        to.push_back(std::move(now));
    }

    /// Sets the field `field` of the live form.
    auto set_live(const std::string& field, const std::string& text) -> void {
        if (field == "name") {
            live_change([&](vehicle& veh) -> std::expected<void, std::string> {
                veh.name = text;
                return {};
            });
            return;
        }
        if (live_part < 0 || live_part >= live->part_count()) {
            notice = _("Pick a part first.");
            return;
        }
        if (field == "cargo") {
            live_change([&](vehicle& veh) { return add_part_cargo(veh, live_part, text); });
            return;
        }
        if (field == "fuel_type") {
            live_change([&](vehicle& veh) {
                return set_part_fuel(
                    veh, live_part,
                    {.fuel = text, .amount = std::max(1, veh.part(live_part).ammo_remaining())});
            });
            return;
        }
        auto value = 0;
        const auto* const end = text.data() + text.size();
        if (const auto [ptr, error] = std::from_chars(text.data(), end, value);
            error != std::errc{} || ptr != end) {
            notice = _("That value is out of range.");
            return;
        }
        if (field == "health") {
            live_change([&](vehicle& veh) { return set_part_health(veh, live_part, value); });
        } else if (field == "fuel") {
            live_change([&](vehicle& veh) {
                return set_part_fuel(
                    veh, live_part,
                    {.fuel = veh.part(live_part).ammo_current().str(), .amount = value});
            });
        }
    }

    /// Puts the design in the game in place of its prototype and spawns it east of the avatar.
    auto spawn() -> void {
        const auto applied = apply_design(prototype().dump());
        if (!applied) {
            notice = applied.error();
            return;
        }
        const auto at = get_avatar().bub_pos() + tripoint_rel_ms(spawn_distance, 0, 0);
        if (!spawn_design(*applied, at, spawning)) {
            notice = _("Something is in the way of the vehicle there.");
        }
    }

    auto set_spawning(const std::string& field, const std::string& text) -> void {
        const auto number = [&](const int low, const int high) -> std::optional<int> {
            auto value = 0;
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || value < low
                || value > high) {
                return std::nullopt;
            }
            return value;
        };
        auto& value =
            field == "facing" ? spawning.facing
            : field == "fuel"
                ? spawning.fuel
                : spawning.condition;
        const auto read =
            field == "facing" ? number(0, 359)
            : field == "fuel"
                ? number(0, 100)
                : number(-1, 1);
        if (read) {
            value = *read;
        } else {
            notice = _("That value is out of range.");
        }
    }

    auto written(const std::expected<void, save_error>& result) -> bool {
        if (!result) {
            switch (result.error()) {
                case save_error::changed_on_disk:
                    notice = _("The file changed on disk since it was opened; nothing was saved.");
                    break;
                case save_error::exists:
                    notice = _("That file already exists; nothing was saved.");
                    break;
                case save_error::unwritable:
                    notice = _("The file could not be written; save it as a file in your mods.");
                    break;
            }
        }
        return result.has_value();
    }

    auto save() -> bool { return written(doc->save()); }

    auto save_as(const std::string& name) -> void {
        const auto path = std::filesystem::path(name);
        if (path.extension() != ".json" || !inside(path, PATH_INFO::user_moddir())) {
            notice = string_format(
                _("Save as writes a .json file in your mods, %s."), PATH_INFO::user_moddir());
            return;
        }
        written(doc->save_as(path));
    }

    /// Whether to leave: at once with nothing unsaved, else as the player answers.
    auto leave() -> bool {
        if (!doc || !doc->dirty()) { return true; }
        auto menu = uilist{};
        menu.text = _("Your changes to this file are not saved.");
        menu.addentry(0, true, 's', _("Save and leave"));
        menu.addentry(1, true, 'd', _("Leave without saving"));
        menu.addentry(2, true, 'c', _("Stay"));
        menu.query();
        if (menu.ret == 1) { doc.reset(); }
        return menu.ret == 1 || (menu.ret == 0 && save());
    }
};

/// The `mounts` of the vehicle `veh` shows and a ring of free ones around them, a cell each, west
/// to east and north to south as its blueprint reads.
auto grid_of(const vehicle* const veh, std::vector<point> mounts, const point& cursor)
    -> ec::component {
    mounts.push_back(cursor);
    const auto [min_x, max_x] = std::ranges::minmax(mounts | std::views::transform(&point::x));
    const auto [min_y, max_y] = std::ranges::minmax(mounts | std::views::transform(&point::y));
    const auto origin = point(min_x - 1, min_y - 1);
    auto palette = ec::look_palette{};
    auto cells = std::vector<ec::grid_cell>{};
    for (auto y = min_y - 1; y <= max_y + 1; ++y) {
        for (auto x = min_x - 1; x <= max_x + 1; ++x) {
            const auto at = point(x, y);
            auto cell = ec::grid_cell{
                .at = {x - origin.x, y - origin.y},
                .id = mount_text(at),
                .glyph = " ",
                .affordance = mount_id,
                .selected = at == cursor};
            const auto shown = veh ? veh->part_displayed_at(tripoint_mnt_veh(x, y, 0)) : -1;
            if (shown >= 0) {
                cell.glyph = std::string(1, veh->part_sym(shown));
                // Every part on the mount, in the order the game installed them.
                cell.label = remove_color_tags(join(
                    veh->parts_at_relative(tripoint_mnt_veh(x, y, 0), false)
                        | std::views::transform([&](const int index) {
                              return veh->cpart(index).name(false);
                          })
                        | std::ranges::to<std::vector>(),
                    ", "));
                cell.color = string_from_color(veh->part_color(shown));
                cell.looks = palette.indexes({engine_client::world::vehicle_part_appearance(
                    *veh, shown,
                    static_cast<int>(std::round(to_degrees(
                        normalize(veh->part_display_direction(shown) - veh->face.dir())))))});
            }
            cells.push_back(std::move(cell));
        }
    }
    return {
        .id = grid_id,
        .affordances = {mount_id},
        .body = ec::grid_body{
            .cols = max_x - min_x + 3,
            .rows = max_y - min_y + 3,
            .cursor = ec::grid_point{cursor.x - origin.x, cursor.y - origin.y},
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas"}};
}

/// The parts the world's vehicle has on the picked mount, in the order the game installed them.
auto live_stack(const session& state) -> ec::component {
    const auto& veh = *state.live;
    const auto mount = tripoint_mnt_veh(state.cursor.x, state.cursor.y, 0);
    return ec::list(
        {.id = stack_id,
         .rows = veh.parts_at_relative(mount, false) | std::views::transform([&](const int index) {
                     const auto& part = veh.cpart(index);
                     auto columns = std::vector<game_client::interaction_column>{
                         {.key = "health",
                          .label = _("Health %"),
                          .value = std::to_string(
                              static_cast<int>(std::round(part.health_percent() * 100))),
                          .color = ""}};
                     if (part.is_fuel_store(false)) {
                         columns.push_back(
                             {.key = "fuel",
                              .label = _("Fuel"),
                              .value = std::to_string(part.ammo_remaining()),
                              .color = ""});
                     }
                     return ec::row{
                         .id = string_format("part:%d", index),
                         .label = part.info().name(),
                         .selected = index == state.live_part,
                         .columns = std::move(columns)};
                 })
               | std::ranges::to<std::vector>(),
         .selection = "single",
         .activate = pick_id});
}

/// The vehicle's name and the picked part's health, fuel and cargo.
auto live_form(const session& state) -> ec::component {
    const auto& veh = *state.live;
    auto fields = std::vector<ec::form_field>{
        {.id = "name", .label = _("Name"), .value = veh.name}};
    if (state.live_part >= 0 && state.live_part < veh.part_count()) {
        const auto& part = veh.cpart(state.live_part);
        fields.push_back(
            {.id = "health",
             .label = _("Health %"),
             .value = std::to_string(static_cast<int>(std::round(part.health_percent() * 100))),
             .type = "integer",
             .min = 0,
             .max = 100});
        if (part.is_fuel_store(false)) {
            fields.push_back(
                {.id = "fuel",
                 .label = _("Fuel"),
                 .value = std::to_string(part.ammo_remaining()),
                 .type = "integer",
                 .min = 0});
            fields.push_back(
                {.id = "fuel_type",
                 .label = _("Fuel type"),
                 .value =
                     part.ammo_current().is_null() ? std::string{} : part.ammo_current().str()});
        }
        if (veh.part_flag(state.live_part, VPFLAG_CARGO)) {
            fields.push_back({.id = "cargo", .label = _("Add item"), .value = ""});
        }
    }
    return {.id = live_form_id,
            .affordances = {live_set_id},
            .body = ec::form_body{.fields = std::move(fields)}};
}

/// A tab of a tab strip whose selected tab is `selected`.
auto tab_of(const char* const id, const std::string& label, const std::string& selected)
    -> ec::tab_entry {
    const auto key = std::string_view(id).substr(std::string_view(id).find(':') + 1);
    return {.id = id, .label = label, .selected = key == selected};
}

/// The prototype part the field form points at, with the members a modder sets on it: the fuel
/// of a tank, the ammunition of a turret.
auto part_form(const session& state) -> std::optional<ec::component> {
    const auto index = state.picked_part();
    if (!index) { return std::nullopt; }
    const auto design = design_parts(state.prototype());
    const auto id = vpart_id(design[*index].part);
    const auto value = [&](const char* const key) {
        return state.value_at(design[*index].pointer + "/" + key);
    };
    auto fields = std::vector<ec::form_field>{};
    if (id.is_valid() && id->has_flag(VPFLAG_FLUIDTANK)) {
        fields.push_back({.id = "fuel", .label = _("Fuel"), .value = value("fuel")});
    }
    if (id.is_valid() && id->has_flag("TURRET")) {
        fields.push_back(
            {.id = "ammo",
             .label = _("Ammo %"),
             .value = value("ammo"),
             .type = "integer",
             .min = 0,
             .max = 100});
        const auto pointer = design[*index].pointer;
        const auto at = nlohmann::ordered_json::json_pointer(pointer);
        const auto& entry =
            state.prototype().contains(at)
                ? state.prototype()[at]
                : nlohmann::ordered_json::object();
        const auto qty =
            entry.is_object()
                ? entry.value("ammo_qty", nlohmann::ordered_json())
                : nlohmann::ordered_json();
        const auto end = [&](const std::size_t side) {
            return qty.is_array() && qty.size() == 2 ? qty[side].dump() : std::string{};
        };
        fields.push_back(
            {.id = "ammo_types",
             .label = _("Ammo types"),
             .value = entry.is_object()
                        ? list_text(entry.value("ammo_types", nlohmann::ordered_json()))
                        : std::string{}});
        fields.push_back(
            {.id = "ammo_min",
             .label = _("Rounds, fewest"),
             .value = end(0),
             .type = "integer",
             .min = 0});
        fields.push_back(
            {.id = "ammo_max",
             .label = _("Rounds, most"),
             .value = end(1),
             .type = "integer",
             .min = 0});
    }
    if (fields.empty()) { return std::nullopt; }
    return ec::component{
        .id = part_form_id,
        .label = id.is_valid() ? id->name() : design[*index].part,
        .affordances = {part_set_id},
        .body = ec::form_body{.fields = std::move(fields)}};
}

auto session_view(const session& state) -> ec::screen_view {
    const auto row_of = [](const char* const component) {
        return ec::arg_shape{.row = ec::row_shape{.component_id = component}};
    };
    const auto text_of = [](const char* const component) {
        return ec::
            arg_shape{.row = ec::row_shape{.component_id = component}, .text = ec::text_shape{}};
    };
    auto parts = std::vector<ec::component>{};
    auto affordances = std::vector<ec::affordance>{};
    if (!state.notice.empty()) {
        parts.push_back(
            {.id = "vehicle:notice",
             .body = ec::text_body{.markup = state.notice, .tone = "bad", .role = "prompt"}});
    }
    const auto found = state.prototypes();
    const auto vehicles = get_map().get_vehicles();
    const auto here = get_avatar().bub_pos();
    // The left column: the prototypes to open, or the world's vehicles to export, copy or edit.
    parts.push_back(ec::tabs(
        {.id = sources_id,
         .tabs = {tab_of("source:prototypes", _("Prototypes"), state.source_tab),
                  tab_of("source:world", _("World"), state.source_tab)},
         .select = source_id,
         .child =
             state.source_tab == "world"
                 ? ec::list(
                       {.id = world_id,
                        .rows = vehicles | std::views::enumerate
                              | std::views::transform([&](const auto& entry) {
                                    const auto& [index, placed] = entry;
                                    return ec::row{
                                        .id = string_format("world:%d", index),
                                        .label = placed.v->name,
                                        .columns = {
                                            {.key = "distance",
                                             .label = _("Distance"),
                                             .value = std::to_string(
                                                 rl_dist(here.raw(), placed.pos.raw())),
                                             .color = ""}}};
                                })
                              | std::ranges::to<std::vector>(),
                        .selection = "single",
                        .activate = edit_id})
                 : ec::list(
                       {.id = prototypes_id,
                        .rows =
                            found.entries | std::views::transform([&](const catalog_entry& entry) {
                                return ec::row{
                                    .id = "prototype:" + entry.id,
                                    .label = entry.name,
                                    .selected =
                                        state.doc && state.prototype().value("id", "") == entry.id,
                                    .columns = {
                                        {.key = "id",
                                         .label = _("ID"),
                                         .value = entry.id,
                                         .color = ""}}};
                            })
                            | std::ranges::to<std::vector>(),
                        .total = found.total,
                        .offset = state.page * page_size,
                        .selection = "single",
                        .activate = open_id,
                        .filterable = true,
                        .filter = state.filter})}));
    affordances.push_back(
        ec::invoke_affordance({.id = source_id, .label = _("Show"), .args = row_of(sources_id)}));
    if (state.source_tab == "world") {
        affordances.push_back(ec::invoke_affordance(
            {.id = export_id, .label = _("Export design"), .args = text_of(world_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = copy_id, .label = _("Copy vehicle"), .args = row_of(world_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = edit_id, .label = _("Edit in place"), .args = row_of(world_id)}));
    } else {
        affordances.push_back(ec::invoke_affordance(
            {.id = open_id, .label = _("Open"), .args = row_of(prototypes_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = filter_id, .label = _("Filter"), .args = {.text = ec::text_shape{}}}));
        affordances.push_back(ec::invoke_affordance(
            {.id = page_id,
             .label = _("Page"),
             .args = {
                 .count = ec::count_shape{
                     .min = 0,
                     .max = pages_of(found.total) - 1,
                     .initial = static_cast<std::int64_t>(state.page)}}}));
    }
    affordances.push_back(ec::invoke_affordance({.id = paste_id, .label = _("Paste vehicle")}));
    if (!state.live && !state.doc) {
        return {.title = _("Vehicle Editor"),
                .root = ec::split({.id = "vehicle:body", .children = std::move(parts)}),
                .affordances = std::move(affordances),
                .focus_component = state.source_tab == "world" ? world_id : prototypes_id};
    }
    const auto* const veh = state.live ? state.live : state.built.blueprint.get();
    const auto mount = tripoint_mnt_veh(state.cursor.x, state.cursor.y, 0);
    // The centre: the vehicle's facts over its mounts, the canvas.
    auto stats = std::vector<ec::panel_stat>{};
    if (!state.live) {
        stats.push_back(
            {.key = "saved",
             .label = _("Saved"),
             .value = state.doc->dirty() ? _("No") : _("Yes"),
             .tone = state.doc->dirty() ? "bad" : "good"});
    }
    stats.push_back(
        {.key = "mount",
         .label = _("Mount"),
         .value = string_format("%d,%d", state.cursor.x, state.cursor.y)});
    if (veh) { std::ranges::move(design_stats(*veh), std::back_inserter(stats)); }
    if (!state.live) {
        std::ranges::move(
            state.built.messages | std::views::enumerate
                | std::views::transform([](const auto& entry) {
                      const auto& [index, message] = entry;
                      return ec::panel_stat{
                          .key = string_format("message:%d", index),
                          .label = message.text,
                          .tone = "bad"};
                  }),
            std::back_inserter(stats));
    }
    const auto mounts =
        state.live
            ? std::views::iota(0, veh->part_count()) | std::views::transform([&](const int index) {
                  return veh->cpart(index).mount.xy().raw();
              }) | std::ranges::to<std::vector>()
            : design_parts(state.prototype()) | std::views::transform(&design_part::mount)
                  | std::ranges::to<std::vector>();
    parts.push_back(ec::panel(
        {.id = state.live ? "vehicle:live" : "vehicle:document",
         .title = state.live ? state.live->name
                             : state.prototype().value("name", state.prototype().value("id", "")),
         .subtitle = state.live ? std::string{} : shown_path(state.doc->path()),
         .stats = std::move(stats),
         .child = grid_of(veh, mounts, state.cursor)}));
    // The right column: what the picked mount holds, what it takes, and the fields of a part, the
    // vehicle and its spawning.
    auto tabs = std::vector<ec::tab_entry>{
        tab_of("tool:mount", _("Mount"), state.tool_tab),
        tab_of("tool:install", _("Install"), state.tool_tab),
        tab_of("tool:part", _("Part"), state.tool_tab),
        tab_of("tool:vehicle", _("Vehicle"), state.tool_tab)};
    if (!state.live) {
        tabs.push_back(tab_of("tool:cargo", _("Cargo"), state.tool_tab));
        tabs.push_back(tab_of("tool:spawn", _("Spawn"), state.tool_tab));
        tabs.push_back(tab_of("tool:json", _("JSON"), state.tool_tab));
    }
    const auto installable = [&] {
        const auto shown = state.parts();
        return ec::list(
            {.id = parts_id,
             .rows = shown.shown | std::views::transform([&](const vpart_info* info) {
                         const auto mounts = veh && veh->can_mount(mount, info->get_id());
                         return ec::row{
                             .id = "vpart:" + info->get_id().str(),
                             .label = info->name(),
                             .denial = mounts ? std::string{} : _("It cannot be mounted here."),
                             .enabled = mounts,
                             .columns = {
                                 {.key = "id",
                                  .label = _("ID"),
                                  .value = info->get_id().str(),
                                  .color = ""}}};
                     })
                   | std::ranges::to<std::vector>(),
             .total = shown.total,
             .offset = state.part_page * page_size,
             .selection = "single",
             .activate = install_id,
             .filterable = true,
             .filter = state.part_filter});
    };
    const auto document_stack = [&] {
        const auto design = design_parts(state.prototype());
        return ec::list(
            {.id = stack_id,
             .rows = design | std::views::enumerate | std::views::filter([&](const auto& entry) {
                         return std::get<1>(entry).mount == state.cursor;
                     })
                   | std::views::transform([&](const auto& entry) {
                         const auto& [index, part] = entry;
                         const auto id = vpart_id(part.part);
                         return ec::row{
                             .id = string_format("part:%d", index),
                             .label = id.is_valid() ? id->name() : part.part,
                             .description = part.pointer,
                             .selected = state.field_pointer == part.pointer};
                     })
                   | std::ranges::to<std::vector>(),
             .selection = "single",
             .activate = pick_id});
    };
    const auto tool = [&]() -> ec::component {
        const auto& tab = state.tool_tab;
        if (tab == "install") { return installable(); }
        if (state.live) {
            return tab == "part" || tab == "vehicle" ? live_form(state) : live_stack(state);
        }
        if (tab == "part") {
            if (auto form = part_form(state)) { return std::move(*form); }
            return {.id = part_form_id, .body = ec::form_body{}};
        }
        if (tab == "vehicle") {
            return {
                .id = props_id,
                .affordances = {prop_set_id},
                .body = ec::form_body{
                    .fields = {
                        {.id = "name",
                         .label = _("Name"),
                         .value = state.prototype().value("name", "")},
                        {.id = "id", .label = _("ID"), .value = state.prototype().value("id", "")},
                        {.id = "color_palette",
                         .label = _("Colour palette"),
                         .value = state.prototype().value("color_palette", "")},
                        {.id = "flags",
                         .label = _("Flags"),
                         .value = list_text(
                             state.prototype().value("flags", nlohmann::ordered_json()))}}}};
        }
        if (tab == "cargo") {
            const auto& items = state.prototype()["items"];
            /// What a spawn's `items` or `item_groups` names, as written.
            const auto named = [](const nlohmann::ordered_json& value) {
                const auto one = [](const nlohmann::ordered_json& entry) {
                    return entry.is_string() ? entry.get<std::string>() : entry.dump();
                };
                return value.is_array()
                         ? join(value | std::views::transform(one) | std::ranges::to<std::vector>(),
                                ", ")
                         : one(value);
            };
            return ec::list(
                {.id = items_id,
                 .rows = state.item_spawns() | std::views::transform([&](const std::size_t index) {
                             const auto& entry = items[index];
                             const auto names =
                                 std::array{"items", "item_groups"}
                                 | std::views::filter([&](const char* const key) {
                                       return entry.contains(key);
                                   })
                                 | std::views::transform([&](const char* const key) {
                                       return named(entry[key]);
                                   })
                                 | std::ranges::to<std::vector>();
                             return ec::row{
                                 .id = string_format("item:%d", index),
                                 .label = join(names, ", "),
                                 .columns = {
                                     {.key = "chance",
                                      .label = _("Chance %"),
                                      .value = std::to_string(entry.value("chance", 0)),
                                      .color = ""}}};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "single"});
        }
        if (tab == "spawn") {
            return {
                .id = spawn_form_id,
                .affordances = {spawn_set_id, spawn_id},
                .body = ec::form_body{
                    .fields =
                        {{.id = "facing",
                          .label = _("Facing"),
                          .value = std::to_string(state.spawning.facing),
                          .type = "integer",
                          .min = 0,
                          .max = 359},
                         {.id = "fuel",
                          .label = _("Fuel %"),
                          .value = std::to_string(state.spawning.fuel),
                          .type = "integer",
                          .min = 0,
                          .max = 100},
                         {.id = "condition",
                          .label = _("Condition"),
                          .value = std::to_string(state.spawning.condition),
                          .type = "select",
                          .options = {{.id = "0", .label = _("Intact")},
                                      {.id = "-1", .label = _("Lightly damaged")},
                                      {.id = "1", .label = _("Disabled")}}}},
                    .submit = spawn_id}};
        }
        if (tab == "json") {
            return {
                .id = field_id,
                .affordances = {field_set_id, field_apply_id},
                .body = ec::form_body{
                    .fields =
                        {{.id = "pointer", .label = _("Pointer"), .value = state.field_pointer},
                         {.id = "value",
                          .label = _("Value"),
                          .value = state.field_value,
                          .multiline = true}},
                    .submit = field_apply_id}};
        }
        return document_stack();
    };
    parts.push_back(
        ec::tabs({.id = tools_id, .tabs = std::move(tabs), .select = tool_id, .child = tool()}));
    affordances.push_back(
        ec::invoke_affordance({.id = tool_id, .label = _("Tool"), .args = row_of(tools_id)}));
    affordances.push_back(
        ec::invoke_affordance({.id = mount_id, .label = _("Mount"), .args = row_of(grid_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = part_filter_id, .label = _("Filter parts"), .args = {.text = ec::text_shape{}}}));
    /// Offers `value` while the tool tab `tab` is shown, the one whose parts it acts on.
    const auto on = [&](const std::string_view tab, ec::affordance value) {
        if (state.tool_tab == tab) { affordances.push_back(std::move(value)); }
    };
    on("mount",
       ec::invoke_affordance({.id = pick_id, .label = _("Edit"), .args = row_of(stack_id)}));
    on("mount",
       ec::invoke_affordance({.id = remove_id, .label = _("Remove"), .args = row_of(stack_id)}));
    on("install",
       ec::invoke_affordance({.id = install_id, .label = _("Install"), .args = row_of(parts_id)}));
    on("install",
       ec::invoke_affordance(
           {.id = part_page_id,
            .label = _("Parts page"),
            .args = {
                .count = ec::count_shape{
                    .min = 0,
                    .max = pages_of(state.parts().total) - 1,
                    .initial = static_cast<std::int64_t>(state.part_page)}}}));
    if (state.live) {
        for (const auto* const tab : {"part", "vehicle"}) {
            on(tab,
               ec::invoke_affordance(
                   {.id = live_set_id, .label = _("Value"), .args = text_of(live_form_id)}));
        }
        affordances.push_back(ec::invoke_affordance(
            {.id = undo_id,
             .label = _("Undo"),
             .keys = {"CTRL+Z"},
             .enabled = !state.live_undo.empty()}));
        affordances.push_back(ec::invoke_affordance(
            {.id = redo_id,
             .label = _("Redo"),
             .keys = {"CTRL+Y"},
             .enabled = !state.live_redo.empty()}));
        affordances.push_back(ec::invoke_affordance({.id = done_id, .label = _("Done")}));
    } else {
        const auto& doc = *state.doc;
        on("mount",
           ec::invoke_affordance(
               {.id = earlier_id, .label = _("Earlier"), .args = row_of(stack_id)}));
        on("mount",
           ec::invoke_affordance({.id = later_id, .label = _("Later"), .args = row_of(stack_id)}));
        on("vehicle",
           ec::invoke_affordance(
               {.id = prop_set_id, .label = _("Value"), .args = text_of(props_id)}));
        on("part",
           ec::invoke_affordance(
               {.id = part_set_id, .label = _("Value"), .args = text_of(part_form_id)}));
        on("cargo",
           ec::invoke_affordance(
               {.id = item_add_id, .label = _("Add item"), .args = {.text = ec::text_shape{}}}));
        on("cargo",
           ec::invoke_affordance(
               {.id = item_remove_id, .label = _("Remove item"), .args = row_of(items_id)}));
        on("cargo",
           ec::invoke_affordance(
               {.id = item_chance_id,
                .label = _("Chance"),
                .args = {.row = ec::row_shape{.component_id = items_id},
                         .count = ec::count_shape{.min = 1, .max = 100, .initial = 100}}}));
        on("json",
           ec::invoke_affordance(
               {.id = field_set_id, .label = _("Value"), .args = text_of(field_id)}));
        on("json", ec::invoke_affordance({.id = field_apply_id, .label = _("Set")}));
        on("spawn",
           ec::invoke_affordance(
               {.id = spawn_set_id, .label = _("Value"), .args = text_of(spawn_form_id)}));
        affordances.push_back(ec::invoke_affordance({.id = spawn_id, .label = _("Spawn")}));
        affordances.push_back(ec::invoke_affordance(
            {.id = undo_id, .label = _("Undo"), .keys = {"CTRL+Z"}, .enabled = doc.can_undo()}));
        affordances.push_back(ec::invoke_affordance(
            {.id = redo_id, .label = _("Redo"), .keys = {"CTRL+Y"}, .enabled = doc.can_redo()}));
        affordances.push_back(ec::invoke_affordance(
            {.id = save_id, .label = _("Save"), .keys = {"CTRL+S"}, .enabled = doc.dirty()}));
        affordances.push_back(ec::invoke_affordance(
            {.id = save_as_id, .label = _("Save as"), .args = {.text = ec::text_shape{}}}));
    }
    return {
        .title = _("Vehicle Editor"),
        .root = ec::split({.id = "vehicle:body", .active = grid_id, .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = grid_id,
        .focus_row = mount_text(state.cursor)};
}

auto kept_session = std::optional<session>{};

/// The index a row id names after its prefix.
auto index_of(const std::string& row, const char* const format) -> std::optional<std::size_t> {
    auto index = std::size_t{0};
    return std::sscanf(row.c_str(), format, &index) == 1 ? std::optional(index) : std::nullopt;
}

/// Runs the affordance `id` on the world's vehicle the editor edits in place.
auto live_invoke(
    session& state, const std::string& id, const std::string& row, const std::string& text)
    -> void {
    const auto index = index_of(row, "part:%zu").transform([](const std::size_t at) {
        return static_cast<int>(at);
    });
    if (id == done_id) {
        state.live = nullptr;
    } else if (id == pick_id && index) {
        state.live_part = *index;
    } else if (id == remove_id && index) {
        state.live_change([&](vehicle& veh) { return remove_live_part(veh, *index); });
        state.live_part = -1;
    } else if (id == install_id) {
        const auto part = row.substr(std::string_view("vpart:").size());
        state.live_change([&](vehicle& veh) {
            return install_live_part(veh, {.mount = state.cursor, .part = part})
                .transform([](const int /*index*/) {});
        });
    } else if (id == live_set_id) {
        state.set_live(row, text);
    } else if (id == undo_id) {
        state.live_step(state.live_undo, state.live_redo);
    } else if (id == redo_id) {
        state.live_step(state.live_redo, state.live_undo);
    }
}

} // namespace

auto reset_vehicle_editor() -> void { kept_session.reset(); }

auto run_vehicle_editor() -> void {
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto& state = kept_session ? *kept_session : kept_session.emplace();
    state.notice.clear();
    const auto screen = ec::screen_scope(
        ctxt, "vehicle_editor",
        {.role = "screen",
         .view = [&](const ec::boundary_state& /*boundary*/) { return session_view(state); }});
    while (true) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto invoked = ec::invoked(ctxt.get_raw_input());
        state.check_live();
        if (!invoked) {
            if (action == "QUIT" && state.leave()) {
                state.live = nullptr;
                return;
            }
            continue;
        }
        const auto& id = invoked->affordance_id;
        const auto row = invoked->args.row.value_or("");
        const auto text = invoked->args.text.value_or("");
        state.notice.clear();
        if (id == source_id || id == tool_id) {
            const auto tab = row.substr(row.find(':') + 1);
            (id == source_id ? state.source_tab : state.tool_tab) = tab;
        } else if (id == open_id) {
            state.live = nullptr;
            state.open(row.substr(std::string_view("prototype:").size()));
        } else if (id == filter_id) {
            state.filter = text;
            state.page = 0;
        } else if (id == page_id) {
            state.page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == export_id) {
            if (const auto index = index_of(row, "world:%zu")) {
                state.export_vehicle(*index, text);
            }
        } else if (id == copy_id) {
            if (const auto index = index_of(row, "world:%zu")) { state.copy_vehicle(*index); }
        } else if (id == paste_id) {
            state.paste_vehicle();
        } else if (id == edit_id) {
            if (const auto index = index_of(row, "world:%zu")) { state.edit_vehicle(*index); }
        } else if (id == mount_id) {
            if (const auto at = mount_of(row)) { state.cursor = *at; }
        } else if (id == part_filter_id) {
            state.part_filter = text;
            state.part_page = 0;
            state.tool_tab = "install";
        } else if (id == part_page_id) {
            state.part_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (state.live) {
            live_invoke(state, id, row, text);
            if (id == install_id) { state.tool_tab = "mount"; }
        } else if (!state.doc) {
            continue;
        } else if (id == pick_id) {
            if (const auto index = index_of(row, "part:%zu")) {
                const auto design = design_parts(state.prototype());
                if (*index < design.size()) {
                    state.field_pointer = design[*index].pointer;
                    state.field_value = state.value_at(state.field_pointer);
                }
            }
        } else if (id == remove_id) {
            if (const auto index = index_of(row, "part:%zu")) { state.remove(*index); }
        } else if (id == earlier_id || id == later_id) {
            if (const auto index = index_of(row, "part:%zu")) {
                state.change(move_part(state.prototype(), *index, id == earlier_id ? -1 : 1));
            }
        } else if (id == install_id) {
            state.install(row.substr(std::string_view("vpart:").size()));
            state.tool_tab = "mount";
        } else if (id == prop_set_id) {
            state.set_prop(row, text);
        } else if (id == part_set_id) {
            state.set_part(row, text);
        } else if (id == item_add_id) {
            state.tool_tab = "cargo";
            state.add_item(text);
        } else if (id == item_remove_id || id == item_chance_id) {
            if (const auto index = index_of(row, "item:%zu")) {
                state.edit_item(
                    *index,
                    id == item_chance_id
                        ? std::optional(static_cast<int>(invoked->args.count.value_or(100)))
                        : std::nullopt);
            }
        } else if (id == field_set_id) {
            if (row == "pointer") {
                state.field_pointer = text;
                state.field_value = state.value_at(text);
            } else {
                state.field_value = text;
            }
        } else if (id == field_apply_id) {
            state.set_field();
        } else if (id == undo_id || id == redo_id) {
            if (id == undo_id ? state.doc->undo() : state.doc->redo()) { state.reload(); }
        } else if (id == save_id) {
            state.save();
        } else if (id == save_as_id) {
            state.save_as(text);
        } else if (id == spawn_set_id) {
            state.set_spawning(row, text);
        } else if (id == spawn_id) {
            state.spawn();
        }
    }
}

} // namespace editor
