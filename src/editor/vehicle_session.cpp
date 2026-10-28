#include "editor/vehicle_session.h"

#include "avatar.h"
#include "calendar.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/editor_access.h"
#include "editor/history_label.h"
#include "editor/recent_documents.h"
#include "editor/row_id.h"
#include "editor/session_file.h"
#include "editor/session_page.h"
#include "editor/vehicle_design.h"
#include "editor/workspace.h"
#include "engine_client_component.h"
#include "engine_client_part_type.h"
#include "engine_client_vehicle_look.h"
#include "engine_client_world.h"
#include "input.h"
#include "item.h"
#include "item_group.h"
#include "json_formatter.h"
#include "line.h"
#include "map/map.h"
#include "output.h"
#include "path_info.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui_manager.h"
#include "units_utility.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {
namespace {

namespace ec = engine_client;

constexpr auto prototypes_id = "vehicle:prototypes";
constexpr auto grid_id = "vehicle:grid";
constexpr auto turned_id = "vehicle:turned";
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
constexpr auto choose_id = "vehicle:choose";
constexpr auto canvas_tools_id = "vehicle:canvas_tools";
constexpr auto canvas_tool_id = "vehicle:canvas_tool";
constexpr auto performance_id = "vehicle:performance";
constexpr auto problems_id = "vehicle:problems";
constexpr auto new_id = "vehicle:new";
constexpr auto turn_cw_id = "vehicle:turn_cw";
constexpr auto turn_ccw_id = "vehicle:turn_ccw";
/// The affordance that shows a tab of the design or vehicles column is this and the tab's key.
constexpr auto show_prefix = std::string_view("vehicle:show:");
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
constexpr auto history_id = "vehicle:history";
constexpr auto jump_id = "vehicle:jump";
constexpr auto item_kinds_id = "vehicle:item_kinds";
constexpr auto item_catalog_id = "vehicle:item_catalog";
constexpr auto item_kind_id = "vehicle:item_kind";
constexpr auto item_filter_id = "vehicle:item_filter";
constexpr auto item_page_id = "vehicle:item_page";
constexpr auto item_pick_id = "vehicle:item_pick";

/// How far east of the avatar a spawned vehicle stands, clear of it.
constexpr auto spawn_distance = 8;
/// How far around the square a vehicle goes to first it looks for room.
constexpr auto spawn_search = 6;
/// How many free mounts the grid shows around the vehicle when it is first drawn or has to grow.
constexpr auto grid_margin = 2;

/// The mounts the grid shows, from its north-west corner to its south-east one.
struct mount_frame {
    point low;
    point high;
    auto operator==(const mount_frame&) const -> bool = default; // *NOPAD*
};

/// The frame of `points` and `margin` mounts around them.
auto frame_around(const std::vector<point>& points, const int margin) -> mount_frame {
    const auto [min_x, max_x] = std::ranges::minmax(points | std::views::transform(&point::x));
    const auto [min_y, max_y] = std::ranges::minmax(points | std::views::transform(&point::y));
    return {.low = point(min_x - margin, min_y - margin),
            .high = point(max_x + margin, max_y + margin)};
}

/// The name of the part `id`, or the id when the game has no such part.
auto part_name(const std::string& id) -> std::string {
    const auto part = vpart_id(id);
    return part.is_valid() ? part->name() : id;
}

/// One change of a world's vehicle edited in place: its saved state on the other side of it, and
/// what it did.
struct live_step {
    std::string state;
    std::string label;
};

/// What a click on a mount of the grid does, the affordance that picks it and its key.
struct canvas_tool {
    const char* key;
    const char* affordance;
    const char* hotkey;
};
constexpr auto canvas_tools = std::array{
    canvas_tool{.key = "select", .affordance = "vehicle:tool_select", .hotkey = "s"},
    canvas_tool{.key = "install", .affordance = "vehicle:tool_install", .hotkey = "i"},
    canvas_tool{.key = "remove", .affordance = "vehicle:tool_remove", .hotkey = "e"}};

/// The tabs of the design column, in order, with the key that shows each; a world's vehicle
/// edited in place has the first three.
struct design_tab {
    const char* key;
    const char* hotkey;
};
constexpr auto design_tabs = std::array{
    design_tab{.key = "mount", .hotkey = "1"},   design_tab{.key = "part", .hotkey = "2"},
    design_tab{.key = "vehicle", .hotkey = "3"}, design_tab{.key = "cargo", .hotkey = "4"},
    design_tab{.key = "spawn", .hotkey = "5"},   design_tab{.key = "json", .hotkey = "6"}};
constexpr auto live_tab_count = std::size_t{3};


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

/// The choices of a field that names one of `items`, by name; the one it names now leads when it
/// is none of them, so that the field shows it.
auto item_options(const std::vector<itype_id>& items, const std::string& current)
    -> std::vector<ec::form_option> {
    auto options =
        items | std::views::transform([](const itype_id& id) {
            return ec::form_option{.id = id.str(), .label = item::nname(id)};
        })
        | std::ranges::to<std::vector>();
    if (!current.empty() && !std::ranges::contains(options, current, &ec::form_option::id)) {
        options.insert(options.begin(), {.id = current, .label = current});
    }
    return options;
}

/// The file a name the player typed stands for: a relative one outside the player's mods is in
/// them, and one with no extension a .json file.
auto in_mods(const std::string& name) -> std::filesystem::path {
    auto path = std::filesystem::path(name);
    if (path.is_relative() && !inside(path, PATH_INFO::user_moddir())) {
        path = std::filesystem::path(PATH_INFO::user_moddir()) / path;
    }
    if (!path.has_extension()) { path += ".json"; }
    return path;
}

/// The squares at least `spawn_distance` from the avatar around the one `toward` it, nearest that
/// one first.
auto spawn_spots(const tripoint_rel_ms& toward) -> std::vector<tripoint_bub_ms> {
    const auto here = get_avatar().bub_pos();
    const auto first = here + toward;
    auto spots = std::vector<tripoint_bub_ms>{};
    for (auto dy = -spawn_search; dy <= spawn_search; ++dy) {
        for (auto dx = -spawn_search; dx <= spawn_search; ++dx) {
            const auto at = first + tripoint_rel_ms(dx, dy, 0);
            if (square_dist(at.raw(), here.raw()) >= spawn_distance) { spots.push_back(at); }
        }
    }
    std::ranges::stable_sort(spots, {}, [&](const tripoint_bub_ms& at) {
        return rl_dist(at.raw(), first.raw());
    });
    return spots;
}

/// Places a vehicle by `place` on the first of `spawn_spots(toward)` where it fits clear of the
/// avatar; none where it fits nowhere, or the error of `place` other than that.
auto place_clear(
    const tripoint_rel_ms& toward,
    const std::function<auto(const tripoint_bub_ms&)->std::expected<vehicle*, std::string>>& place)
    -> std::expected<vehicle*, std::string> {
    const auto in_the_way = std::string(_("Something is in the way of the vehicle there."));
    const auto avatar_at = get_avatar().bub_pos();
    for (const auto& at : spawn_spots(toward)) {
        const auto placed = place(at);
        if (!placed && placed.error() != in_the_way) { return placed; }
        if (!placed) { continue; }
        if (veh_pointer_or_null(get_map().veh_at(avatar_at)) == *placed) {
            get_map().destroy_vehicle(*placed);
            continue;
        }
        return placed;
    }
    return std::unexpected(_("There is no room for the vehicle near you."));
}

/// What the editor says after it spawned or, `pasted`, pasted `veh`: its name, how far and which
/// way it stands from the avatar.
auto placed_sentence(const bool pasted, const vehicle& veh) -> std::string {
    const auto here = get_avatar().bub_pos().xy().raw();
    const auto at = veh.bub_ms_location().xy().raw();
    const auto distance = rl_dist(here, at);
    const auto* const format =
        pasted
            ? vgettext("Pasted %1$s %2$d square %3$s of you.",
                       "Pasted %1$s %2$d squares %3$s of you.", distance)
            : vgettext("Spawned %1$s %2$d square %3$s of you.",
                       "Spawned %1$s %2$d squares %3$s of you.", distance);
    return string_format(format, veh.name, distance, direction_name(direction_from(here, at)));
}

auto copied_vehicle_file() -> std::filesystem::path {
    return std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
}

/// What the player's actions can change: the design open and its steps, the world's vehicle edited
/// in place and its steps, how a vehicle spawns, what a click on the grid does, and whether the
/// world was told something.
struct session_footprint {
    std::string design;
    std::size_t steps = 0;
    std::size_t depth = 0;
    std::optional<std::size_t> saved;
    const vehicle* live = nullptr;
    std::array<int, 3> spawning{};
    std::string tool;
    std::string brush;
    bool said = false;
    auto operator==(const session_footprint&) const -> bool = default; // *NOPAD*
};

struct session {
    std::optional<document> doc = std::nullopt;
    /// The prototype's object in `doc`.
    std::size_t object = 0;
    std::string notice;
    /// What the last action did to the world, when it did something the editor does not show.
    std::string said;
    /// What the items palette lists: items or item groups, those that match the filter, and the
    /// page shown.
    catalog_kind item_kind = catalog_kind::item;
    std::string item_filter;
    std::size_t item_page = 0;
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
    /// The tab of the right column: `mount`, `part`, `vehicle`, `cargo`, `spawn` or `json`.
    std::string tool_tab = "mount";
    /// What a click on the grid does: `select`, `install` or `remove`.
    std::string canvas_tool = "select";
    /// The part the install tool puts on a clicked mount; empty before one is chosen.
    std::string brush;
    /// The world's vehicle edited in place, the part picked on it, and its saved states before
    /// each change to undo and after each undone one to redo.
    vehicle* live = nullptr;
    int live_part = -1;
    std::vector<live_step> live_undo;
    std::vector<live_step> live_redo;
    /// The mounts the grid shows, which only grow while the same vehicle is edited, so that a
    /// cell stays under the pointer; what that vehicle is.
    mount_frame frame;
    std::string framed;

    auto prototype() const -> const nlohmann::ordered_json& { // *NOPAD*
        return doc->objects()[object].value;
    }

    auto items_found() const -> catalog_page {
        return search_catalog(
            {.kind = item_kind, .filter = item_filter, .page = item_page, .page_size = page_size});
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
        if (doc && doc->path() == *path && prototype().value("id", "") == id) {
            // It is open already, with whatever it has unsaved.
            live = nullptr;
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
        // Before the document moves, which takes `objects` with it.
        const auto index = static_cast<std::size_t>(found - objects.begin());
        if (!leave()) { return; }
        take(std::move(*opened), index);
    }

    /// Edits the prototype `index` of `opened` from now on, with the select tool.
    auto take(document opened, const std::size_t index) -> void {
        live = nullptr;
        object = index;
        doc = std::move(opened);
        remember();
        cursor = point{};
        field_pointer = "/name";
        canvas_tool = "select";
        reload();
        pick_top();
    }

    /// Puts the open design first among the documents the main menu offers again, once its file
    /// is there, with the mod that holds it.
    auto remember() const -> void {
        if (!std::filesystem::exists(doc->path())) { return; }
        remember_document(
            {.editor = "vehicle",
             .name = prototype().value("id", ""),
             .mods = mods_for(doc->path())});
    }

    /// The file a new design suggests: `base` in the mod folder of the open file, else in
    /// my_mod, with a number when that file is there or the open design, unsaved, takes it.
    auto suggested(const std::string& base) const -> std::string {
        const auto folder = doc ? mod_folder_name(doc->path()) : std::nullopt;
        return free_name(folder.value_or("my_mod") + "/" + base, [&](const std::string& name) {
            const auto path = in_mods(name);
            return std::filesystem::exists(path)
                || (doc && path.lexically_normal() == doc->path().lexically_normal());
        });
    }

    /// Reads the file again when it changed on disk and holds no edit; says so either way.
    auto watch() -> void {
        if (!doc || !doc->changed_on_disk()) { return; }
        if (doc->dirty()) {
            notice = changed_on_disk_notice(*doc);
            return;
        }
        const auto kept_cursor = cursor;
        const auto kept_pointer = field_pointer;
        const auto kept_tool = canvas_tool;
        const auto path = doc->path();
        const auto id = prototype().value("id", "");
        open_file(path, id);
        if (!notice.empty()) {
            // Nothing is edited, so a file that lost the prototype closes it.
            doc.reset();
            notice =
                string_format(_("%s changed on disk and no longer defines %s."), path.string(), id);
            return;
        }
        cursor = kept_cursor;
        field_pointer = kept_pointer;
        canvas_tool = kept_tool;
        reload();
        notice = changed_on_disk_notice(*doc);
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

    /// The indexes in `design_parts` of the prototype's parts on the picked mount, in the order
    /// the game installs them.
    auto parts_here() const -> std::vector<std::size_t> {
        const auto design = design_parts(prototype());
        return std::views::iota(std::size_t{0}, design.size())
             | std::views::filter([&](const std::size_t index) {
                   return design[index].mount == cursor;
               })
             | std::ranges::to<std::vector>();
    }

    /// The world's vehicle's parts on the picked mount, in the order the game installed them.
    auto live_here() const -> std::vector<int> {
        return live->parts_at_relative(tripoint_mnt_veh(cursor.x, cursor.y, 0), false);
    }

    /// The picked part when it is on the picked mount, as an index of `design_parts`.
    auto picked_here() const -> std::optional<std::size_t> {
        const auto index = picked_part();
        return index && std::ranges::contains(parts_here(), *index) ? index : std::nullopt;
    }

    /// The name of the picked part on the picked mount; none when the mount has none.
    auto picked_name() const -> std::optional<std::string> {
        if (live) {
            return std::ranges::contains(live_here(), live_part)
                     ? std::optional(live->cpart(live_part).info().name())
                     : std::nullopt;
        }
        return picked_here().transform([&](const std::size_t index) {
            const auto part = design_parts(prototype())[index].part;
            const auto id = vpart_id(part);
            return id.is_valid() ? id->name() : part;
        });
    }

    /// `format` with the name `what` and the picked mount, as "Install %s at %d,%d".
    auto at_mount(const char* const format, const std::string& what) const -> std::string {
        return string_format(format, what, cursor.x, cursor.y);
    }

    /// Picks the part on top of the picked mount, the one installed last.
    auto pick_top() -> void {
        if (live) {
            const auto here = live_here();
            live_part = here.empty() ? -1 : here.back();
            return;
        }
        if (const auto here = parts_here(); !here.empty()) {
            field_pointer = design_parts(prototype())[here.back()].pointer;
            field_value = value_at(field_pointer);
        } else if (picked_part()) {
            // A part of another mount stays picked no longer, nor one an index now names anew.
            field_pointer = "/name";
            field_value = value_at(field_pointer);
        }
    }

    /// Picks the top part of the picked mount unless a part on it is picked.
    auto keep_pick() -> void {
        if (live ? !std::ranges::contains(live_here(), live_part) : !picked_here()) { pick_top(); }
    }

    /// Whether the picked part has a part installed before it on its mount, for a negative
    /// `step`, or after it.
    auto can_move(const int step) const -> bool {
        const auto here = parts_here();
        const auto picked = picked_here();
        const auto found = picked ? std::ranges::find(here, *picked) : here.end();
        if (found == here.end()) { return false; }
        return step < 0 ? found != here.begin() : std::next(found) != here.end();
    }

    auto remove_picked() -> void {
        if (live) {
            if (std::ranges::contains(live_here(), live_part)) {
                live_change(at_mount(_("Remove %s at %d,%d"), live->cpart(live_part).info().name()),
                            [&](vehicle& veh) { return remove_live_part(veh, live_part); });
            } else {
                notice = _("This mount has no part to remove.");
            }
        } else if (const auto index = picked_here()) {
            remove(*index);
        } else {
            notice = _("This mount has no part to remove.");
        }
        if (notice.empty()) { pick_top(); }
    }

    /// Moves the picked part one place earlier in its mount's install order, for a negative
    /// `step`, or later, keeping it picked.
    auto move_picked(const int step) -> void {
        const auto index = picked_here();
        if (!index) {
            notice = _("This mount has no part to move.");
            return;
        }
        const auto part = design_parts(prototype())[*index].part;
        change(move_part(prototype(), *index, step));
        const auto design = design_parts(prototype());
        const auto moved = std::ranges::find_if(design, [&](const design_part& entry) {
            return entry.mount == cursor && entry.part == part;
        });
        if (moved != design.end()) {
            field_pointer = moved->pointer;
            field_value = value_at(field_pointer);
        }
    }

    /// Picks the mount `at` and does there what the canvas tool does.
    auto click(const point& at) -> void {
        cursor = at;
        // What the click changes shows at once.
        if (canvas_tool != "select") { tool_tab = "mount"; }
        if (canvas_tool == "remove") {
            pick_top();
            remove_picked();
            return;
        }
        if (canvas_tool == "install" && brush.empty()) {
            notice = _("Choose a part from the Parts palette first.");
        } else if (canvas_tool == "install" && live) {
            live_change(at_mount(_("Install %s at %d,%d"), part_name(brush)), [&](vehicle& veh) {
                return install_live_part(veh, {.mount = cursor, .part = brush})
                    .transform([](const int /*index*/) {});
            });
        } else if (canvas_tool == "install") {
            install(brush);
        }
        pick_top();
    }

    /// Shows the tab `key` of the vehicles column or of the design column, when it has one.
    auto show(const std::string& key) -> void {
        if (key == "prototypes" || key == "world") {
            source_tab = key;
        } else if (
            std::ranges::contains(
                design_tabs | std::views::take(shown_tab_count())
                    | std::views::transform(&design_tab::key),
                std::string_view(key))) {
            tool_tab = key;
        }
    }

    /// How many tabs the design column has: a world's vehicle edited in place has fewer.
    auto shown_tab_count() const -> std::size_t {
        return live ? live_tab_count : design_tabs.size();
    }

    /// Turns the facing the vehicle spawns with by quarters clockwise, and shows it.
    auto turn(const int quarters) -> void {
        spawning.facing = ((spawning.facing + 90 * quarters) % 360 + 360) % 360;
        tool_tab = "spawn";
    }

    auto set_field() -> void {
        auto next = prototype();
        if (!field_pointer.starts_with('/')) {
            notice = _("A pointer starts with /, as /name does.");
            return;
        }
        try {
            const auto at = nlohmann::ordered_json::json_pointer(field_pointer);
            const auto parent = at.parent_pointer();
            // As a JSON patch adds: into an object or array that is there.
            if (!next.contains(parent) || !(next[parent].is_object() || next[parent].is_array())) {
                notice = string_format(
                    _("The prototype has nothing at %s to hold %s."),
                    parent.empty() ? std::string("/") : parent.to_string(), at.back());
                return;
            }
            if (field_value.empty()) {
                if (!next.contains(at)) {
                    notice = string_format(
                        _("The prototype has nothing at %s to remove."), field_pointer);
                    return;
                }
                // `contains` took the index of a list as a number within it.
                if (auto& holder = next.at(parent); holder.is_array()) {
                    holder.erase(static_cast<std::size_t>(std::stoull(at.back())));
                } else {
                    holder.erase(at.back());
                }
            } else {
                const auto parsed = nlohmann::ordered_json::parse(field_value, nullptr, false);
                // Bare text is a string; text that starts as JSON does, after any space, is meant
                // as JSON.
                const auto start = field_value.find_first_not_of(" \t\r\n");
                if (parsed.is_discarded() && start != std::string::npos
                    && std::string_view("{[\"").contains(field_value[start])) {
                    notice =
                        string_format(_("%s is no JSON value; nothing was changed."), field_value);
                    return;
                }
                next[at] = parsed.is_discarded() ? nlohmann::ordered_json(field_value) : parsed;
            }
        } catch (const nlohmann::ordered_json::exception& /*error*/) {
            notice = string_format(
                _("%s names no place in the prototype: a list takes a number or -."),
                field_pointer);
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
        if (field == "flags") {
            const auto names = listed(text).value_or(nlohmann::ordered_json::array());
            const auto unknown =
                std::ranges::find_if(names, [](const nlohmann::ordered_json& name) {
                    return !flag_id(name.get<std::string>()).is_valid();
                });
            if (unknown != names.end()) {
                notice =
                    string_format(_("%s is no flag the game knows."), unknown->get<std::string>());
                return;
            }
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
        const auto index = picked_here();
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
        if (key == "fuel" && !text.empty()
            && !std::ranges::contains(tank_fuels(), itype_id(text))) {
            notice = string_format(_("%s is no liquid a tank holds."), text);
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

    /// Adds a spawn of the item or item group `id` on the picked mount, every time; `kind` says
    /// which, else an id that names an item group stands for the group.
    auto add_item(const std::string& id, const std::optional<catalog_kind> kind = std::nullopt)
        -> void {
        const auto defined_group = item_group::group_is_defined(item_group_id(id));
        const auto group = kind ? *kind == catalog_kind::item_group : defined_group;
        if (group ? !defined_group : !itype_id(id).is_valid()) {
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

    /// Opens the prototype a new design makes, whose id is the file's name, for the new file
    /// `name` of the player's mods, which saving writes; `write` writes it at once.
    auto create(
        const std::string& name,
        const std::function<auto(const std::string&)->nlohmann::ordered_json>& prototype_of,
        const bool write = false) -> void {
        const auto path = in_mods(name);
        if (!is_mod_file(path)) {
            notice = string_format(
                _("A new design goes in a new .json file in your mods, %s, inside a mod's folder "
                  "such as my_mod/."),
                PATH_INFO::user_moddir());
            return;
        }
        if (std::filesystem::exists(path)) {
            notice = _("That file already exists; nothing was saved.");
            return;
        }
        if (!leave()) { return; }
        // Saving the design it left may have made that file.
        if (std::filesystem::exists(path)) {
            notice = _("That file already exists; nothing was saved.");
            return;
        }
        const auto design = nlohmann::ordered_json::array({prototype_of(path.stem().string())});
        auto made = document::create(path, format_json(design.dump()).value_or(design.dump(2)));
        if (!made) {
            notice = made.error();
            return;
        }
        take(std::move(*made), 0);
        if (write) { save(); }
    }

    /// Writes the design of the world's vehicle `index` as a prototype in a new file of the user
    /// mods, and opens it.
    auto export_vehicle(const std::size_t index, const std::string& name) -> void {
        const auto vehicles = get_map().get_vehicles();
        if (index >= vehicles.size()) { return; }
        create(
            name, [&](const std::string& id) { return export_design(*vehicles[index].v, id); },
            true);
    }

    /// Starts a vehicle with no parts in a new file of the user mods, named after the file.
    auto new_vehicle(const std::string& name) -> void {
        create(name, [](const std::string& id) {
            return nlohmann::ordered_json{
                {"type", "vehicle"},
                {"id", id},
                {"name", id},
                {"parts", nlohmann::ordered_json::array()}};
        });
    }

    /// Keeps the world's vehicle `index` with its whole state in a file of the user directory, so
    /// any world can paste it.
    auto copy_vehicle(const std::size_t index) -> void {
        const auto vehicles = get_map().get_vehicles();
        if (index >= vehicles.size()) { return; }
        auto out = std::ofstream(copied_vehicle_file(), std::ios::binary);
        out << vehicle_state(*vehicles[index].v);
        if (!out.flush()) {
            notice = _("The file could not be written.");
            return;
        }
        said = string_format(_("Copied %s; paste it in any world."), vehicles[index].v->name);
    }

    /// Places an exact copy of the copied vehicle south of the avatar, clear of a spawned one.
    auto paste_vehicle() -> void {
        auto in = std::ifstream(copied_vehicle_file(), std::ios::binary);
        if (!in) {
            notice = _("No vehicle is copied.");
            return;
        }
        const auto state = std::string(std::istreambuf_iterator<char>(in), {});
        const auto placed =
            place_clear(tripoint_rel_ms(0, spawn_distance, 0), [&](const tripoint_bub_ms& at) {
                return place_vehicle_state(state, at);
            });
        if (!placed) {
            notice = placed.error();
        } else {
            said = placed_sentence(true, **placed);
        }
    }

    /// Edits the world's vehicle `index` in place.
    auto edit_vehicle(const std::size_t index) -> void {
        const auto vehicles = get_map().get_vehicles();
        if (index >= vehicles.size()) { return; }
        live = vehicles[index].v;
        canvas_tool = "select";
        framed.clear();
        cursor = point{};
        live_part = -1;
        pick_top();
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

    /// Runs `edit` on the vehicle edited in place as one undo step, which `label` names.
    auto live_change(
        const std::string& label,
        const std::function<auto(vehicle&)->std::expected<void, std::string>>& edit) -> void {
        auto before = vehicle_state(*live);
        if (const auto done = edit(*live); !done) {
            notice = done.error();
            return;
        }
        live_undo.push_back({.state = std::move(before), .label = label});
        live_redo.clear();
    }

    /// Puts the vehicle edited in place back to the last state of `from`, keeping the current one
    /// in `to`.
    auto step_live(std::vector<live_step>& from, std::vector<live_step>& to) -> void {
        if (from.empty() || !live) { return; }
        auto now = vehicle_state(*live);
        auto step = std::move(from.back());
        from.pop_back();
        const auto restored = restore_vehicle_state(*live, step.state);
        if (!restored) {
            notice = restored.error();
            live = nullptr;
            return;
        }
        live = *restored;
        live_part = -1;
        to.push_back({.state = std::move(now), .label = std::move(step.label)});
    }

    /// The steps of the vehicle edited in place, or of the open document.
    auto history() const -> history_steps {
        if (!live) { return document_steps(*doc, design_step_label); }
        auto labels =
            live_undo | std::views::transform(&live_step::label) | std::ranges::to<std::vector>();
        std::ranges::
            copy(live_redo | std::views::reverse | std::views::transform(&live_step::label),
                 std::back_inserter(labels));
        return {.labels = std::move(labels), .depth = live_undo.size()};
    }

    /// What the next action may change, to tell whether it changed anything.
    auto footprint() const -> session_footprint {
        const auto steps = live || doc ? history() : history_steps{};
        return {
            .design = doc ? doc->path().string() + "#" + prototype().value("id", "") : "",
            .steps = steps.labels.size(),
            .depth = steps.depth,
            .saved = steps.saved,
            .live = live,
            .spawning = {spawning.facing, spawning.fuel, spawning.condition},
            .tool = canvas_tool,
            .brush = brush,
            .said = !said.empty()};
    }

    /// Undoes or redoes until `depth` steps are done.
    auto jump(const std::size_t depth) -> void {
        if (!live) {
            if (doc->jump(depth)) { reload(); }
        } else {
            while (live && live_undo.size() > depth) { step_live(live_undo, live_redo); }
            while (live && live_undo.size() < depth && !live_redo.empty()) {
                step_live(live_redo, live_undo);
            }
        }
        if (live || doc) { keep_pick(); }
    }

    /// The mounts the vehicle shown has.
    auto mounts() const -> std::vector<point> {
        if (live) {
            return std::views::iota(0, live->part_count())
                 | std::views::filter([&](const int index) { return !live->cpart(index).removed; })
                 | std::views::transform([&](const int index) {
                       return live->cpart(index).mount.xy().raw();
                   })
                 | std::ranges::to<std::vector>();
        }
        return design_parts(prototype()) | std::views::transform(&design_part::mount)
             | std::ranges::to<std::vector>();
    }

    /// Fits the grid to the vehicle shown: a margin of free mounts around it when it is first
    /// drawn; after that it grows, never shrinks or moves, when the vehicle or the picked mount
    /// leaves no free mount at its edge.
    auto fit_frame() -> void {
        if (!live && !doc) { return; }
        auto points = mounts();
        if (points.empty()) { points.push_back(cursor); }
        // The picked mount needs no free mounts beyond it.
        const auto around = [&](const int margin) {
            const auto bounds = frame_around(points, margin);
            return mount_frame{
                .low = point(std::min(bounds.low.x, cursor.x), std::min(bounds.low.y, cursor.y)),
                .high =
                    point(std::max(bounds.high.x, cursor.x), std::max(bounds.high.y, cursor.y))};
        };
        // Undo puts the vehicle edited in place back as a new one, which is still the same.
        const auto shown =
            live ? std::string("live") : doc->path().string() + "#" + prototype().value("id", "");
        if (shown != framed) {
            framed = shown;
            frame = around(grid_margin);
            return;
        }
        const auto needed = around(1);
        if (needed.low.x < frame.low.x || needed.low.y < frame.low.y || needed.high.x > frame.high.x
            || needed.high.y > frame.high.y) {
            const auto grown = around(grid_margin);
            frame =
                {.low =
                     point(std::min(frame.low.x, grown.low.x), std::min(frame.low.y, grown.low.y)),
                 .high = point(
                     std::max(frame.high.x, grown.high.x), std::max(frame.high.y, grown.high.y))};
        }
    }


    /// Sets the field `field` of the live form.
    auto set_live(const std::string& field, const std::string& text) -> void {
        if (field == "name") {
            live_change(
                string_format(_("Rename to %s"), text),
                [&](vehicle& veh) -> std::expected<void, std::string> {
                    veh.name = text;
                    return {};
                });
            return;
        }
        if (live_part < 0 || live_part >= live->part_count()) {
            notice = _("Pick a part first.");
            return;
        }
        const auto name = live->cpart(live_part).info().name();
        const auto turret = live->cpart(live_part).is_turret();
        if (field == "cargo") {
            const auto type = itype_id(text);
            live_change(
                string_format(_("Put %s in %s"), type.is_valid() ? item::nname(type) : text, name),
                [&](vehicle& veh) { return add_part_cargo(veh, live_part, text); });
            return;
        }
        if (field == "fuel_type") {
            const auto type = itype_id(text);
            live_change(
                string_format(turret ? _("Load %s with %s") : _("Fill %s with %s"), name,
                              type.is_valid() ? item::nname(type) : text),
                [&](vehicle& veh) {
                    return set_part_fuel(
                        veh, live_part,
                        {.fuel = text,
                         .amount = std::max(1, veh.part(live_part).ammo_remaining())});
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
            live_change(string_format(_("Set health of %s to %d%%"), name, value),
                        [&](vehicle& veh) { return set_part_health(veh, live_part, value); });
        } else if (field == "fuel") {
            live_change(
                string_format(
                    turret ? _("Set rounds of %s to %d") : _("Set fuel of %s to %d"), name, value),
                [&](vehicle& veh) {
                    return set_part_fuel(
                        veh, live_part,
                        {.fuel = veh.part(live_part).ammo_current().str(), .amount = value});
                });
        }
    }

    /// Puts the design in the game in place of its prototype and spawns it east of the avatar; true
    /// when it is there.
    auto spawn() -> bool {
        const auto applied = apply_design(prototype().dump());
        if (!applied) {
            notice = applied.error();
            return false;
        }
        const auto spawned = place_clear(
            tripoint_rel_ms(spawn_distance, 0, 0),
            [&](const tripoint_bub_ms& at) -> std::expected<vehicle*, std::string> {
                auto* const placed = spawn_design(*applied, at, spawning);
                if (!placed) {
                    return std::unexpected(_("Something is in the way of the vehicle there."));
                }
                return placed;
            });
        if (!spawned) {
            notice = spawned.error();
            return false;
        }
        said = placed_sentence(false, **spawned);
        return true;
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

    auto told(const std::expected<void, std::string>& result) -> bool {
        if (!result) { notice = result.error(); }
        return result.has_value();
    }

    auto save() -> bool {
        if (!told(save_document(*doc))) { return false; }
        remember();
        return true;
    }

    auto save_as(const std::string& name) -> void {
        if (told(save_document_as(*doc, in_mods(name)))) { remember(); }
    }

    /// Whether to leave: at once with nothing unsaved, else as the player answers.
    auto leave() -> bool {
        if (!doc || !doc->dirty()) { return true; }
        const auto answer = ask_leave();
        if (answer == leave_answer::discard) { doc.reset(); }
        return answer == leave_answer::discard || (answer == leave_answer::save && save());
    }
};

/// The mounts of `frame` and any of `mounts` or the picked one outside it, a cell each, west to
/// east and north to south as its blueprint reads, each showing every part of the vehicle `veh`
/// on it.
auto grid_of(const vehicle* const veh, const mount_frame& frame, const point& cursor)
    -> ec::component {
    const auto low = point(std::min(frame.low.x, cursor.x), std::min(frame.low.y, cursor.y));
    const auto high = point(std::max(frame.high.x, cursor.x), std::max(frame.high.y, cursor.y));
    auto palette = ec::look_palette{};
    auto cells = std::vector<ec::grid_cell>{};
    for (auto y = low.y; y <= high.y; ++y) {
        for (auto x = low.x; x <= high.x; ++x) {
            const auto at = point(x, y);
            auto cell = ec::grid_cell{
                .at = {x - low.x, y - low.y},
                .id = point_row("mount", at),
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
                cell.looks = palette.indexes(mount_looks(*veh, at, 0));
            }
            cells.push_back(std::move(cell));
        }
    }
    return {
        .id = grid_id,
        .affordances = {mount_id},
        .body = ec::grid_body{
            .cols = high.x - low.x + 1,
            .rows = high.y - low.y + 1,
            .cursor = ec::grid_point{cursor.x - low.x, cursor.y - low.y},
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas"}};
}

/// The squares the vehicle `veh` covers on the map when it faces `facing` degrees clockwise from
/// east, as the game turns it, each showing the parts of the mount that lands there.
auto turned_grid(const vehicle& veh, const int facing) -> ec::component {
    const auto dir = units::from_degrees(facing);
    // The game spawns a vehicle turning it about its (0,0) mount, which lands on the spawn square.
    const auto pivot = tripoint_mnt_veh::zero();
    struct landed {
        point at;
        int part;
    };
    auto squares = std::vector<landed>{};
    for (auto index = 0; index < veh.part_count(); ++index) {
        if (veh.cpart(index).removed) { continue; }
        const auto mount = veh.cpart(index).mount;
        const auto shown = veh.part_displayed_at(mount);
        if (shown < 0 || std::ranges::contains(squares, shown, &landed::part)) { continue; }
        auto at = point_rel_ms{};
        veh.coord_translate(dir, pivot, mount, at);
        squares.push_back({.at = at.raw(), .part = shown});
    }
    if (squares.empty()) { return {.id = turned_id, .body = ec::grid_body{}}; }
    const auto [min_x, max_x] = std::ranges::minmax(
        squares | std::views::transform([](const landed& square) { return square.at.x; }));
    const auto [min_y, max_y] = std::ranges::minmax(
        squares | std::views::transform([](const landed& square) { return square.at.y; }));
    auto palette = ec::look_palette{};
    const auto cells =
        squares | std::views::transform([&](const landed& square) {
            return ec::grid_cell{
                .at = {square.at.x - min_x, square.at.y - min_y},
                .glyph = std::string(1, veh.part_sym(square.part)),
                .label = remove_color_tags(veh.cpart(square.part).name(false)),
                .color = string_from_color(veh.part_color(square.part)),
                .looks = palette.indexes(
                    mount_looks(veh, veh.cpart(square.part).mount.xy().raw(), facing))};
        })
        | std::ranges::to<std::vector>();
    return {
        .id = turned_id,
        .label = _("Facing"),
        .body = ec::grid_body{
            .cols = max_x - min_x + 1,
            .rows = max_y - min_y + 1,
            .cells = cells,
            .looks = std::move(palette.looks)}};
}

/// The parts the world's vehicle has on the picked mount, in the order the game installed them.
auto live_stack(const session& state) -> ec::component {
    const auto& veh = *state.live;
    const auto mount = tripoint_mnt_veh(state.cursor.x, state.cursor.y, 0);
    return ec::list(
        {.id = stack_id,
         .rows =
             veh.parts_at_relative(mount, false) | std::views::transform([&](const int index) {
                 const auto& part = veh.cpart(index);
                 auto columns = std::vector<game_client::interaction_column>{
                     {.key = "health",
                      .label = _("Health %"),
                      .value = std::to_string(
                          static_cast<int>(std::round(part.health_percent() * 100))),
                      .color = ""}};
                 if (part.is_fuel_store(false) || part.is_turret()) {
                     columns.push_back(
                         {.key = "fuel",
                          .label = part.is_turret() ? _("Rounds") : _("Fuel"),
                          .value = std::to_string(part.ammo_remaining()),
                          .color = ""});
                 }
                 return ec::row{
                     .id = index_row("part", index),
                     .label = part.info().name(),
                     .selected = index == state.live_part,
                     .columns = std::move(columns),
                     .look = engine_client::world::vehicle_part_own_appearance(veh, index, 270)};
             })
             | std::ranges::to<std::vector>(),
         .selection = "single",
         .activate = pick_id});
}

/// The picked part's health, fuel and cargo, for `part`, or else the vehicle's name.
auto live_form(const session& state, const bool part_fields) -> ec::component {
    const auto& veh = *state.live;
    auto fields = std::vector<ec::form_field>{};
    if (!part_fields) {
        fields.push_back({.id = "name", .label = _("Name"), .value = veh.name});
    } else if (state.live_part >= 0 && state.live_part < veh.part_count()) {
        const auto& part = veh.cpart(state.live_part);
        fields.push_back(
            {.id = "health",
             .label = _("Health %"),
             .value = std::to_string(static_cast<int>(std::round(part.health_percent() * 100))),
             .type = "integer",
             .min = 0,
             .max = 100});
        if (part.is_fuel_store(false) || part.is_turret()) {
            fields.push_back(
                {.id = "fuel",
                 .label = part.is_turret() ? _("Rounds") : _("Fuel"),
                 .value = std::to_string(part.ammo_remaining()),
                 .type = "integer",
                 .min = 0});
            const auto current =
                part.ammo_current().is_null() ? std::string{} : part.ammo_current().str();
            fields.push_back(
                {.id = "fuel_type",
                 .label = part.is_turret() ? _("Ammunition") : _("Fuel type"),
                 .value = current,
                 .type = "select",
                 .options = item_options(part_fuels(veh, state.live_part), current)});
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
    const auto index = state.picked_here();
    if (!index) { return std::nullopt; }
    const auto design = design_parts(state.prototype());
    const auto id = vpart_id(design[*index].part);
    const auto value = [&](const char* const key) {
        return state.value_at(design[*index].pointer + "/" + key);
    };
    auto fields = std::vector<ec::form_field>{};
    if (id.is_valid() && id->has_flag(VPFLAG_FLUIDTANK)) {
        auto options = item_options(tank_fuels(), value("fuel"));
        options.insert(options.begin(), {.id = "", .label = _("None")});
        fields.push_back(
            {.id = "fuel",
             .label = _("Fuel"),
             .value = value("fuel"),
             .type = "select",
             .options = std::move(options)});
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

auto parts_view(const session& state) -> ec::screen_view {
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
    if (!state.said.empty()) {
        parts.push_back(
            {.id = "vehicle:status", .body = ec::text_body{.markup = state.said, .tone = "good"}});
    }
    const auto found = state.prototypes();
    const auto vehicles = get_map().get_vehicles();
    const auto here = get_avatar().bub_pos().xy().raw();
    // The world's vehicles nearest first, the nearest picked, each with how far and which way it
    // stands, which tells apart two of one name.
    const auto distance_of = [&](const std::size_t index) {
        return rl_dist(here, vehicles[index].pos.xy().raw());
    };
    auto nearest =
        std::views::iota(std::size_t{0}, vehicles.size()) | std::ranges::to<std::vector>();
    std::ranges::stable_sort(nearest, {}, distance_of);
    auto world_rows =
        nearest | std::views::transform([&](const std::size_t index) {
            const auto at = vehicles[index].pos.xy().raw();
            const auto distance = distance_of(index);
            return ec::row{
                .id = index_row("world", index),
                .label = vehicles[index].v->name,
                .columns = {
                    {.key = "distance",
                     .label = _("Distance"),
                     .value = string_format(vgettext("%d square", "%d squares", distance), distance),
                     .color = ""},
                    {.key = "direction",
                     .label = _("Direction"),
                     .value =
                         distance == 0 ? std::string{} : direction_name(direction_from(here, at)),
                     .color = ""}}};
        })
        | std::ranges::to<std::vector>();
    if (!world_rows.empty()) { world_rows.front().selected = true; }
    // The left column: the prototypes to open, or the world's vehicles to export, copy or edit.
    parts.push_back(ec::tabs(
        {.id = sources_id,
         .tabs = {tab_of("source:prototypes", _("Prototypes"), state.source_tab),
                  tab_of("source:world", _("World"), state.source_tab)},
         .select = source_id,
         .child =
             state.source_tab == "world"
                 ? ec::list({.id = world_id,
                             .rows = world_rows,
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
                        .filter = state.filter,
                        .search = filter_id,
                        .page = page_id})}));
    affordances.push_back(
        ec::invoke_affordance({.id = source_id, .label = _("Show"), .args = row_of(sources_id)}));
    if (state.source_tab == "world") {
        const auto none =
            vehicles.empty() ? std::string(_("No vehicle is near you.")) : std::string{};
        affordances.push_back(ec::invoke_affordance(
            {.id = export_id,
             .label = _("Export design"),
             .args = {.row = ec::row_shape{.component_id = world_id},
                      .text = ec::text_shape{.initial = state.suggested("my_vehicle")}},
             .enabled = none.empty(),
             .denial = none}));
        affordances.push_back(ec::invoke_affordance(
            {.id = copy_id,
             .label = _("Copy vehicle"),
             .args = row_of(world_id),
             .enabled = none.empty(),
             .denial = none}));
        affordances.push_back(ec::invoke_affordance(
            {.id = edit_id,
             .label = _("Edit in place"),
             .args = row_of(world_id),
             .enabled = none.empty(),
             .denial = none}));
    } else {
        affordances.push_back(ec::invoke_affordance(
            {.id = open_id, .label = _("Open"), .args = row_of(prototypes_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = filter_id, .label = _("Filter"), .args = {.text = ec::text_shape{}}}));
        affordances.push_back(page_affordance(
            {.id = page_id, .label = _("Page"), .total = found.total, .page = state.page}));
    }
    affordances.push_back(
        ec::invoke_affordance({.id = paste_id, .label = _("Paste vehicle"), .keys = {"CTRL+V"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = new_id,
         .label = _("New vehicle"),
         .args = {.text = ec::text_shape{.initial = state.suggested("my_vehicle")}},
         .keys = {"CTRL+N"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = std::string(show_prefix) + "prototypes", .label = _("Prototypes"), .keys = {"p"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = std::string(show_prefix) + "world", .label = _("World vehicles"), .keys = {"w"}}));
    if (!state.live && !state.doc) {
        return {
            .title = _("Vehicle Editor"),
            .root = ec::split({.id = "vehicle:body", .children = std::move(parts)}),
            .affordances = std::move(affordances),
            .focus_component = state.source_tab == "world" ? world_id : prototypes_id,
            .focus_row =
                state.source_tab == "world" && !world_rows.empty()
                    ? world_rows.front().id
                    : std::string{}};
    }
    const auto* const veh = state.live ? state.live : state.built.blueprint.get();
    const auto mount = tripoint_mnt_veh(state.cursor.x, state.cursor.y, 0);
    const auto picked = state.picked_name();
    // The status line: the vehicle, its file, whether it is saved, and the picked mount.
    auto status = std::vector<ec::panel_stat>{};
    if (!state.live) {
        status.push_back(
            {.key = "file", .label = _("File"), .value = shown_path(state.doc->path())});
        status.push_back(
            {.key = "saved",
             .label = _("Saved"),
             .value = state.doc->dirty() ? _("No") : _("Yes"),
             .tone = state.doc->dirty() ? "bad" : "good"});
    }
    status.push_back(
        {.key = "mount",
         .label = _("Mount"),
         .value = string_format("%d,%d", state.cursor.x, state.cursor.y)});
    parts.push_back(ec::panel(
        {.id = state.live ? "vehicle:live" : "vehicle:document",
         .title = state.live ? state.live->name
                             : state.prototype().value("name", state.prototype().value("id", "")),
         .stats = std::move(status)}));
    if (!state.live && !state.built.messages.empty()) {
        auto problems =
            state.built.messages | std::views::enumerate
            | std::views::transform([](const auto& entry) {
                  const auto& [index, message] = entry;
                  return ec::row{.id = index_row("message", index), .label = message.text};
              })
            | std::ranges::to<std::vector>();
        if (!state.built.blueprint && state.doc->can_undo()) {
            problems.push_back(
                {.id = "message:undo",
                 .label = _("Undo puts the prototype back as it was before this change.")});
        }
        parts.push_back(
            ec::list({.id = problems_id, .rows = std::move(problems), .selection = "none"}));
    }
    // Every step of the open document or of the vehicle edited in place, to go back to.
    parts.push_back(ec::list(
        {.id = history_id,
         .rows = step_rows(state.history()),
         .selection = "single",
         .activate = jump_id}));
    affordances.push_back(ec::invoke_affordance(
        {.id = jump_id, .label = _("Go to step"), .args = row_of(history_id), .role = "select"}));
    // The canvas tools, and the mounts they act on.
    parts.push_back(ec::list(
        {.id = canvas_tools_id,
         .rows = canvas_tools | std::views::transform([&](const canvas_tool& entry) {
                     const auto key = std::string_view(entry.key);
                     const auto brush = vpart_id(state.brush);
                     return ec::row{
                         .id = std::string("canvas_tool:") + entry.key,
                         .label =
                             key == "select"   ? _("Select")
                             : key == "remove" ? _("Remove")
                             : brush.is_valid()
                                 ? string_format(_("Install %s"), brush->name())
                                 : std::string(_("Install")),
                         .hotkey = entry.hotkey,
                         .selected = state.canvas_tool == key};
                 })
               | std::ranges::to<std::vector>(),
         .selection = "single",
         .activate = canvas_tool_id}));
    affordances.push_back(ec::invoke_affordance(
        {.id = canvas_tool_id,
         .label = _("Use"),
         .args = row_of(canvas_tools_id),
         .role = "select"}));
    for (const auto& entry : canvas_tools) {
        const auto key = std::string_view(entry.key);
        affordances.push_back(ec::invoke_affordance(
            {.id = entry.affordance,
             .label = key == "select"  ? _("Select tool")
                    : key == "install" ? _("Install tool")
                                       : _("Remove tool"),
             .keys = {entry.hotkey}}));
    }
    parts.push_back(grid_of(veh, state.frame, state.cursor));
    // The design column: what the picked mount holds, the settings of the picked part and the
    // vehicle's, its cargo and spawning, and its JSON.
    const auto settings =
        state.live ? state.live_part >= 0 && state.live_part < veh->part_count()
                   : part_form(state).has_value();
    const auto tab_label = [&](const std::string_view key) -> std::string {
        if (key == "mount") { return _("Parts here"); }
        if (key == "part") { return state.live ? _("Part condition") : _("Part settings"); }
        if (key == "vehicle") { return state.live ? _("Name") : _("Name and flags"); }
        if (key == "cargo") { return _("Items here"); }
        if (key == "spawn") { return _("Test spawn"); }
        return _("Raw JSON");
    };
    const auto settings_denial =
        settings ? std::string{}
        : state.live || picked
            ? std::string(_("The picked part has nothing to set."))
            : std::string(_("Pick a part under Parts here first."));
    const auto shown_tabs = design_tabs | std::views::take(state.shown_tab_count());
    // A tab that has nothing to show, or that the vehicle lacks, shows the mount's parts instead.
    const auto shown_tab =
        (state.tool_tab == "part" && !settings)
                || !std::ranges::contains(shown_tabs | std::views::transform(&design_tab::key),
                                          std::string_view(state.tool_tab))
            ? std::string("mount")
            : state.tool_tab;
    auto tabs =
        shown_tabs | std::views::transform([&](const design_tab& entry) {
            const auto key = std::string_view(entry.key);
            const auto enabled = key != "part" || settings;
            return ec::tab_entry{
                .id = std::string("tool:") + entry.key,
                .label = tab_label(key),
                .selected = key == shown_tab,
                .enabled = enabled,
                .denial = enabled ? std::string{} : settings_denial,
                .hotkey = entry.hotkey};
        })
        | std::ranges::to<std::vector>();
    for (const auto& entry : shown_tabs) {
        const auto key = std::string_view(entry.key);
        const auto enabled = key != "part" || settings;
        affordances.push_back(ec::invoke_affordance(
            {.id = std::string(show_prefix) + entry.key,
             .label = tab_label(key),
             .keys = {entry.hotkey},
             .enabled = enabled,
             .denial = enabled ? std::string{} : settings_denial}));
    }
    const auto installable = [&] {
        const auto shown = state.parts();
        return ec::list(
            {.id = parts_id,
             .rows = shown.shown | std::views::transform([&](const vpart_info* info) {
                         // Whether it fits the picked mount, and why not, as the game rules.
                         const auto refused =
                             veh ? mount_refusal(*veh, state.cursor, info->get_id()) : std::nullopt;
                         return ec::row{
                             .id = "vpart:" + info->get_id().str(),
                             .label = info->name(),
                             .description = refused.value_or(""),
                             .selected = info->get_id().str() == state.brush,
                             .columns =
                                 {{.key = "id",
                                   .label = _("ID"),
                                   .value = info->get_id().str(),
                                   .color = ""}},
                             .look = engine_client::world::vehicle_part_type_appearance(*info)};
                     })
                   | std::ranges::to<std::vector>(),
             .total = shown.total,
             .offset = state.part_page * page_size,
             .selection = "single",
             .activate = choose_id,
             .filterable = true,
             .filter = state.part_filter,
             .search = part_filter_id,
             .page = part_page_id});
    };
    // Remove and the moves act on the picked part, which they name.
    const auto stack_actions = [&] {
        auto actions = std::vector<ec::affordance>{ec::invoke_affordance(
            {.id = remove_id,
             .label = picked ? string_format(_("Remove %s"), *picked) : _("Remove part"),
             .keys = {"DELETE"},
             .enabled = picked.has_value(),
             .denial = picked ? std::string{} : _("This mount has no part to remove.")})};
        if (!state.live) {
            const auto move = [&](const char* const id, const int step) {
                const auto can = state.can_move(step);
                const auto label =
                    step < 0
                        ? (picked ? string_format(_("Move %s up"), *picked) : _("Move up"))
                        : (picked ? string_format(_("Move %s down"), *picked) : _("Move down"));
                return ec::invoke_affordance(
                    {.id = id,
                     .label = label,
                     .keys = {step < 0 ? "[" : "]"},
                     .enabled = can,
                     .denial = can ? std::string{}
                                   : _("No part on this mount is installed on that side of it.")});
            };
            actions.push_back(move(earlier_id, -1));
            actions.push_back(move(later_id, 1));
        }
        return actions;
    }();
    const auto with_actions = [&](ec::component stack) {
        std::ranges::copy(stack_actions | std::views::transform(&ec::affordance::id),
                          std::back_inserter(stack.affordances));
        return stack;
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
                             .id = index_row("part", index),
                             .label = id.is_valid() ? id->name() : part.part,
                             .selected = state.field_pointer == part.pointer,
                             .look = id.is_valid()
                                       ? std::optional{engine_client::world::
                                                           vehicle_part_type_appearance(*id)}
                                       : std::nullopt};
                     })
                   | std::ranges::to<std::vector>(),
             .selection = "single",
             .activate = pick_id});
    };
    const auto tool = [&]() -> ec::component {
        const auto& tab = shown_tab;
        if (state.live) {
            if (tab == "part" || tab == "vehicle") { return live_form(state, tab == "part"); }
            return with_actions(live_stack(state));
        }
        if (tab == "part") { return std::move(*part_form(state)); }
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
            return ec::list(
                {.id = items_id,
                 .rows = state.item_spawns() | std::views::transform([&](const std::size_t index) {
                             const auto& entry = items[index];
                             return ec::row{
                                 .id = index_row("item", index),
                                 .label = item_spawn_name(entry),
                                 .description =
                                     entry.contains("item_groups")
                                         ? std::string(_("Item group"))
                                         : std::string{},
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
            auto form = ec::component{
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
            if (!state.built.blueprint) { return form; }
            return ec::split(
                {.id = "vehicle:spawn",
                 .children = {std::move(form),
                              turned_grid(*state.built.blueprint, state.spawning.facing)}});
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
        return with_actions(document_stack());
    };
    parts.push_back(
        ec::tabs({.id = tools_id, .tabs = std::move(tabs), .select = tool_id, .child = tool()}));
    // The vehicle's figures, under the design they follow from.
    if (veh) { parts.push_back(ec::panel({.id = performance_id, .stats = design_stats(*veh)})); }
    parts.push_back(installable());
    // The items an item spawn puts, while the item spawns show.
    if (!state.live && shown_tab == "cargo") {
        const auto found = state.items_found();
        const auto group = state.item_kind == catalog_kind::item_group;
        parts.push_back(ec::tabs(
            {.id = item_kinds_id,
             .tabs = {{.id = "kind:item", .label = _("Items"), .selected = !group},
                      {.id = "kind:item_group", .label = _("Item groups"), .selected = group}},
             .select = item_kind_id,
             .child = ec::list(
                 {.id = item_catalog_id,
                  .rows =
                      found.entries | std::views::transform([&](const catalog_entry& entry) {
                          const auto type = itype_id(entry.id);
                          return ec::row{
                              .id = (group ? "group:" : "item:") + entry.id,
                              .label = entry.name,
                              .columns =
                                  {{.key = "id", .label = _("ID"), .value = entry.id, .color = ""}},
                              .look = !group && type.is_valid()
                                        ? std::optional(engine_client::world::item_appearance(
                                              *item::spawn_temporary(type, calendar::turn)))
                                        : std::nullopt};
                      })
                      | std::ranges::to<std::vector>(),
                  .total = found.total,
                  .offset = state.item_page * page_size,
                  .selection = "single",
                  .activate = item_pick_id,
                  .filterable = true,
                  .filter = state.item_filter,
                  .search = item_filter_id,
                  .page = item_page_id})}));
        affordances.push_back(ec::invoke_affordance(
            {.id = item_kind_id, .label = _("Show"), .args = row_of(item_kinds_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = item_filter_id, .label = _("Filter items"), .args = {.text = ec::text_shape{}}}));
        affordances.push_back(page_affordance(
            {.id = item_page_id,
             .label = _("Items page"),
             .total = found.total,
             .page = state.item_page}));
        affordances.push_back(ec::invoke_affordance(
            {.id = item_pick_id, .label = _("Add here"), .args = row_of(item_catalog_id)}));
    }
    affordances.push_back(
        ec::invoke_affordance({.id = tool_id, .label = _("Tab"), .args = row_of(tools_id)}));
    affordances.push_back(
        ec::invoke_affordance({.id = mount_id, .label = _("Mount"), .args = row_of(grid_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = part_filter_id, .label = _("Filter parts"), .args = {.text = ec::text_shape{}}}));
    affordances.push_back(
        ec::invoke_affordance({.id = choose_id, .label = _("Choose"), .args = row_of(parts_id)}));
    affordances.push_back(page_affordance(
        {.id = part_page_id,
         .label = _("Parts page"),
         .total = state.parts().total,
         .page = state.part_page}));
    /// Offers `value` while the tool tab `tab` is shown, the one whose parts it acts on.
    const auto on = [&](const std::string_view tab, ec::affordance value) {
        if (shown_tab == tab) { affordances.push_back(std::move(value)); }
    };
    on("mount",
       ec::invoke_affordance(
           {.id = pick_id, .label = _("Pick"), .args = row_of(stack_id), .role = "select"}));
    std::ranges::copy(stack_actions, std::back_inserter(affordances));
    if (state.live) {
        for (const auto* const tab : {"part", "vehicle"}) {
            on(tab,
               ec::invoke_affordance(
                   {.id = live_set_id, .label = _("Value"), .args = text_of(live_form_id)}));
        }
        std::ranges::move(
            undo_affordances(
                {.ids = {.undo = undo_id, .redo = redo_id},
                 .can_undo = !state.live_undo.empty(),
                 .can_redo = !state.live_redo.empty()}),
            std::back_inserter(affordances));
        affordances.push_back(
            ec::invoke_affordance({.id = done_id, .label = _("Done"), .keys = {"CTRL+D"}}));
    } else {
        const auto& doc = *state.doc;
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
        affordances.push_back(
            ec::invoke_affordance({.id = spawn_id, .label = _("Spawn"), .keys = {"F5"}}));
        affordances.push_back(
            ec::invoke_affordance({.id = turn_cw_id, .label = _("Turn clockwise"), .keys = {"r"}}));
        affordances.push_back(ec::invoke_affordance(
            {.id = turn_ccw_id, .label = _("Turn counterclockwise"), .keys = {"R"}}));
        std::ranges::
            move(history_affordances(doc, {.undo = undo_id, .redo = redo_id, .save = save_id}),
                 std::back_inserter(affordances));
        affordances.push_back(ec::invoke_affordance(
            {.id = save_as_id,
             .label = _("Save as"),
             .args =
                 {.text = ec::
                      text_shape{.initial = state.suggested(state.prototype().value("id", ""))}},
             .keys = {"F12"}}));
    }
    return {
        .title = _("Vehicle Editor"),
        .root = ec::split({.id = "vehicle:body", .active = grid_id, .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = grid_id,
        .focus_row = point_row("mount", state.cursor)};
}

/// The screen with what each part is for, and the menu of each action that is no part's own: with
/// nothing open, the vehicles to open are the page.
auto session_view(const session& state) -> ec::screen_view {
    const auto file = std::string(_("File"));
    const auto edit = std::string(_("Edit"));
    const auto view_menu = std::string(_("View"));
    const auto world = std::string(_("World"));
    auto view = parts_view(state);
    auto groups = std::map<std::string, std::string, std::less<>>{
        {new_id, file},    {save_id, file},     {save_as_id, file},   {undo_id, edit},
        {redo_id, edit},   {remove_id, edit},   {earlier_id, edit},   {later_id, edit},
        {spawn_id, world}, {turn_cw_id, world}, {turn_ccw_id, world}, {paste_id, world},
        {done_id, world}};
    for (const auto& entry : canvas_tools) { groups.emplace(entry.affordance, edit); }
    for (const auto& entry : view.affordances) {
        if (entry.id.starts_with(show_prefix)) { groups.emplace(entry.id, view_menu); }
    }
    lay_out(
        view,
        {.slots =
             {{sources_id,
               {.slot = state.live || state.doc ? "outline" : "canvas", .label = _("Vehicles")}},
              {"vehicle:document", {.slot = "status"}},
              {"vehicle:live", {.slot = "status"}},
              {grid_id, {.slot = "canvas"}},
              {canvas_tools_id, {.slot = "tools", .label = _("Tools")}},
              {tools_id, {.slot = "inspector", .label = _("Design")}},
              {performance_id, {.slot = "inspector", .label = _("Performance")}},
              {parts_id, {.slot = "palette", .label = _("Parts")}},
              {problems_id, {.slot = "log", .label = _("Problems")}},
              {history_id, {.slot = "log", .label = _("History")}},
              {"vehicle:notice", {.slot = "log"}},
              {"vehicle:status", {.slot = "log"}},
              {item_kinds_id, {.slot = "palette", .label = _("Items to add")}}},
         .groups = std::move(groups),
         .menus = {file, edit, view_menu, world}});
    return view;
}

auto kept_session = std::optional<session>{};

/// Runs the affordance `id` on the world's vehicle the editor edits in place.
auto live_invoke(
    session& state, const std::string& id, const std::string& row, const std::string& text)
    -> void {
    const auto index = row_index(row, "part").transform([](const std::size_t at) {
        return static_cast<int>(at);
    });
    if (id == done_id) {
        state.live = nullptr;
    } else if (id == pick_id && index) {
        state.live_part = *index;
    } else if (id == live_set_id) {
        state.set_live(row, text);
    } else if (id == undo_id) {
        state.step_live(state.live_undo, state.live_redo);
    } else if (id == redo_id) {
        state.step_live(state.live_redo, state.live_undo);
    } else if (id == jump_id) {
        if (const auto depth = row_index(row, "step")) { state.jump(*depth); }
    }
    if (state.live && (id == undo_id || id == redo_id)) { state.keep_pick(); }
}

} // namespace

auto reset_vehicle_editor() -> void { kept_session.reset(); }

auto run_vehicle_editor(const std::string& prototype) -> bool {
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto& state = kept_session ? *kept_session : kept_session.emplace();
    state.notice.clear();
    if (!prototype.empty()) { state.open(prototype); }
    const auto screen = ec::screen_scope(
        ctxt, "vehicle_editor",
        {.role = "workspace",
         .view = [&](const ec::boundary_state& /*boundary*/) { return session_view(state); }});
    while (true) {
        state.check_live();
        state.fit_frame();
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto invoked = ec::invoked(ctxt.get_raw_input());
        state.check_live();
        if (!invoked) {
            if (action == "QUIT" && state.leave()) {
                state.live = nullptr;
                return false;
            }
            continue;
        }
        const auto& id = invoked->affordance_id;
        const auto row = invoked->args.row.value_or("");
        const auto text = invoked->args.text.value_or("");
        // A notice stays until an action changes something or brings a notice of its own.
        state.said.clear();
        const auto before = state.footprint();
        auto kept = std::exchange(state.notice, {});
        state.watch();
        const auto tool = std::ranges::find(canvas_tools, id, &canvas_tool::affordance);
        if (id == source_id || id == tool_id) {
            state.show(row.substr(row.find(':') + 1));
        } else if (id.starts_with(show_prefix)) {
            state.show(id.substr(show_prefix.size()));
        } else if (id == canvas_tool_id) {
            state.canvas_tool = std::string(row_rest(row, "canvas_tool").value_or("select"));
        } else if (tool != canvas_tools.end()) {
            state.canvas_tool = tool->key;
        } else if (id == choose_id) {
            state.brush = std::string(row_rest(row, "vpart").value_or(""));
            state.canvas_tool = "install";
        } else if (id == new_id) {
            state.new_vehicle(text);
        } else if (id == open_id) {
            state.open(std::string(row_rest(row, "prototype").value_or("")));
        } else if (id == filter_id) {
            state.filter = text;
            state.page = 0;
        } else if (id == page_id) {
            state.page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == export_id) {
            if (const auto index = row_index(row, "world")) { state.export_vehicle(*index, text); }
        } else if (id == copy_id) {
            if (const auto index = row_index(row, "world")) { state.copy_vehicle(*index); }
        } else if (id == paste_id) {
            state.paste_vehicle();
        } else if (id == edit_id) {
            if (const auto index = row_index(row, "world")) { state.edit_vehicle(*index); }
        } else if (id == part_filter_id) {
            state.part_filter = text;
            state.part_page = 0;
        } else if (id == part_page_id) {
            state.part_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (!state.live && !state.doc) {
            // Nothing is open to act on.
        } else if (id == mount_id) {
            if (const auto at = row_point(row, "mount")) { state.click(*at); }
        } else if (id == remove_id) {
            state.remove_picked();
        } else if (state.live) {
            live_invoke(state, id, row, text);
        } else if (!state.doc) {
            // The vehicle edited in place left the map.
        } else if (id == pick_id) {
            if (const auto index = row_index(row, "part")) {
                const auto design = design_parts(state.prototype());
                if (*index < design.size()) {
                    state.field_pointer = design[*index].pointer;
                    state.field_value = state.value_at(state.field_pointer);
                }
            }
        } else if (id == earlier_id || id == later_id) {
            state.move_picked(id == earlier_id ? -1 : 1);
        } else if (id == turn_cw_id || id == turn_ccw_id) {
            state.turn(id == turn_cw_id ? 1 : -1);
        } else if (id == prop_set_id) {
            state.set_prop(row, text);
        } else if (id == part_set_id) {
            state.set_part(row, text);
        } else if (id == item_add_id) {
            state.tool_tab = "cargo";
            state.add_item(text);
        } else if (id == item_kind_id) {
            state.item_kind =
                row == "kind:item_group" ? catalog_kind::item_group : catalog_kind::item;
            state.item_page = 0;
        } else if (id == item_filter_id) {
            state.item_filter = text;
            state.item_page = 0;
        } else if (id == item_page_id) {
            state.item_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == item_pick_id) {
            if (const auto item = row_rest(row, "item")) {
                state.add_item(std::string(*item), catalog_kind::item);
            } else if (const auto group = row_rest(row, "group")) {
                state.add_item(std::string(*group), catalog_kind::item_group);
            }
        } else if (id == item_remove_id || id == item_chance_id) {
            if (const auto index = row_index(row, "item")) {
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
        } else if (id == jump_id) {
            if (const auto depth = row_index(row, "step")) { state.jump(*depth); }
        } else if (id == undo_id || id == redo_id) {
            if (id == undo_id ? state.doc->undo() : state.doc->redo()) {
                state.reload();
                state.keep_pick();
            }
        } else if (id == save_id) {
            state.save();
        } else if (id == save_as_id) {
            state.save_as(text);
        } else if (id == spawn_set_id) {
            state.set_spawning(row, text);
        } else if (id == spawn_id && state.spawn() && in_editor_game()) {
            // The editor game shows no world behind the editor: the avatar goes to try it.
            state.live = nullptr;
            return true;
        }
        if (state.notice.empty() && state.footprint() == before) { state.notice = std::move(kept); }
    }
}

} // namespace editor
