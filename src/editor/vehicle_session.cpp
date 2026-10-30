#include "editor/vehicle_session.h"

#include "avatar.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/editor_mode.h"
#include "editor/editor_session.h"
#include "editor/history_label.h"
#include "editor/mapgen_stroke.h"
#include "editor/recent_documents.h"
#include "editor/row_id.h"
#include "editor/session_file.h"
#include "editor/vehicle_design.h"
#include "editor/vehicle_session_state.h"
#include "engine_client_component.h"
#include "engine_client_part_type.h"
#include "engine_client_vehicle_look.h"
#include "engine_client_world.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "item_group.h"
#include "json_formatter.h"
#include "line.h"
#include "map/map.h"
#include "messages.h"
#include "path_info.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui_manager.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor::vehicle_editor {

namespace ec = engine_client;

namespace {

/// How far east of the avatar a spawned vehicle stands, clear of it.
constexpr auto spawn_distance = 8;
/// How far around the square a vehicle goes to first it looks for room.
constexpr auto spawn_search = 6;
/// How many free mounts the grid keeps around the vehicle on every side, room to draw it out.
constexpr auto grid_margin = 12;
/// The fewest mounts across and down the grid shows a vehicle in.
constexpr auto grid_cols = 41;
constexpr auto grid_rows = 31;

/// The name of the part `id`, or the id when the game has no such part.
auto part_name(const std::string& id) -> std::string {
    const auto part = vpart_id(id);
    return part.is_valid() ? part->name() : id;
}

/// `frame` widened about its middle to at least `grid_cols` by `grid_rows` mounts.
auto roomy(mount_frame frame) -> mount_frame {
    const auto wide = std::max(0, grid_cols - (frame.high.x - frame.low.x + 1));
    const auto tall = std::max(0, grid_rows - (frame.high.y - frame.low.y + 1));
    frame.low = point(frame.low.x - wide / 2, frame.low.y - tall / 2);
    frame.high = point(frame.high.x + wide - wide / 2, frame.high.y + tall - tall / 2);
    return frame;
}

/// The whole of `text` read as a number from `low` to `high`; none for anything else.
auto number_in(const std::string& text, const int low, const int high) -> std::optional<int> {
    auto value = 0;
    const auto* const end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && ptr == end && value >= low && value <= high
             ? std::optional(value)
             : std::nullopt;
}

/// The trimmed entries of a comma-separated list, as a JSON array; none when it lists nothing.
auto listed(const std::string& text) -> std::optional<nlohmann::ordered_json> {
    auto entries =
        string_split(text, ',') | std::views::transform(trim)
        | std::views::filter([](const std::string& entry) { return !entry.empty(); })
        | std::ranges::to<std::vector>();
    if (entries.empty()) { return std::nullopt; }
    return nlohmann::ordered_json(std::move(entries));
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

/// An id made of the vehicle name `name`: its letters and digits in lower case, words joined by
/// `_`; `my_vehicle` when it has none of them.
auto id_of(const std::string& name) -> std::string {
    auto id = std::string{};
    for (const auto letter : name) {
        const auto lower = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) {
            id += lower;
        } else if (!id.empty() && !id.ends_with('_')) {
            id += '_';
        }
    }
    while (id.ends_with('_')) { id.pop_back(); }
    return id.empty() ? std::string("my_vehicle") : id;
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

/// How many parts of a paste or removal refused and why the first did, after a space; empty when
/// none did.
auto refused_parts(const stroke_tally& tally) -> std::string {
    if (tally.refused == 0) { return {}; }
    //~ How many parts of a paste or removal refused, and why the first did.
    return " "
         + string_format(
               vgettext("%1$d part refused: %2$s", "%1$d parts refused: %2$s", tally.refused),
               tally.refused, tally.reason);
}

/// The file a copied vehicle waits in, outside every world so that another world can paste it.
auto copied_vehicle_file() -> std::filesystem::path {
    return std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
}

} // namespace

auto clip_json(const std::vector<clip_mount>& clip) -> nlohmann::ordered_json {
    auto mounts = nlohmann::ordered_json::array();
    for (const auto& [offset, parts] : clip) {
        mounts.push_back({{"x", offset.x}, {"y", offset.y}, {"parts", parts}});
    }
    return {{"mounts", mounts}};
}

auto frame_around(const std::vector<point>& points, const int margin) -> mount_frame {
    const auto [min_x, max_x] = std::ranges::minmax(points | std::views::transform(&point::x));
    const auto [min_y, max_y] = std::ranges::minmax(points | std::views::transform(&point::y));
    return {.low = point(min_x - margin, min_y - margin),
            .high = point(max_x + margin, max_y + margin)};
}

auto session::prototype() const -> const nlohmann::ordered_json& { // *NOPAD*
    return doc->objects()[object].value;
}

auto session::shown_group() const -> part_category {
    if (part_group) { return *part_group; }
    const auto shapes = catalog.shapes_of(vpart_id(brush));
    return shapes.empty() ? part_category::frames : category_of(*shapes.front());
}

auto session::value_at(const std::string& pointer) const -> std::string {
    try {
        const auto at = nlohmann::ordered_json::json_pointer(pointer);
        if (!prototype().contains(at)) { return {}; }
        const auto& value = prototype()[at];
        return value.is_string() ? value.get<std::string>() : value.dump();
    } catch (const nlohmann::ordered_json::exception&) { return {}; }
}

auto session::suggested(const std::string& base, const bool new_id) const -> std::string {
    const auto folder = doc ? mod_folder_name(doc->path()) : std::nullopt;
    return free_name(folder.value_or("my_mod") + "/" + base, [&](const std::string& name) {
        const auto path = in_mods(name);
        return std::filesystem::exists(path)
            || (doc && path.lexically_normal() == doc->path().lexically_normal())
            || (new_id && vproto_id(path.stem().string()).is_valid());
    });
}

auto session::parts_here() const -> std::vector<std::size_t> {
    if (live) {
        return live->parts_at_relative(tripoint_mnt_veh(cursor.x, cursor.y, 0), false)
             | std::views::transform([](const int index) {
                   return static_cast<std::size_t>(index);
               })
             | std::ranges::to<std::vector>();
    }
    const auto design = design_parts(prototype());
    return std::views::iota(std::size_t{0}, design.size())
         | std::views::filter([&](const std::size_t index) {
               return design[index].mount == cursor;
           })
         | std::ranges::to<std::vector>();
}

auto session::picked_here() const -> std::optional<std::size_t> {
    const auto index =
        !live ? picked_part()
        : live_part >= 0
            ? std::optional(static_cast<std::size_t>(live_part))
            : std::nullopt;
    return index && std::ranges::contains(parts_here(), *index) ? index : std::nullopt;
}

auto session::picked_name() const -> std::optional<std::string> {
    return picked_here().transform([&](const std::size_t index) {
        return live ? live->cpart(static_cast<int>(index)).info().name()
                    : part_name(design_parts(prototype())[index].part);
    });
}

auto session::kind_name(const vpart_id& id) const -> std::string {
    const auto shapes = catalog.shapes_of(id);
    return shapes.empty() ? id.str() : shapes.front()->get_id().str();
}

auto session::marked_mounts() const -> std::vector<point> {
    const auto [low, high] = selection.value_or(mount_frame{.low = cursor, .high = cursor});
    auto marked = std::vector<point>{};
    for (auto y = low.y; y <= high.y; ++y) {
        for (auto x = low.x; x <= high.x; ++x) { marked.emplace_back(x, y); }
    }
    return marked;
}

auto session::tool() const -> const struct canvas_tool& { // *NOPAD*
    const auto found = std::ranges::find_if(canvas_tools, [&](const struct canvas_tool& entry) {
        return canvas_tool == entry.key;
    });
    return found != canvas_tools.end() ? *found : canvas_tools.front();
}

auto session::shown_vehicle() const -> const vehicle* {
    return live ? live : built.blueprint.get();
}

auto session::canvas_bounds() const -> mount_frame {
    return frame_around({frame.low, frame.high, cursor}, 0);
}

auto session::shown_tab_count() const -> std::size_t {
    // Raw JSON, the last, is advanced.
    return live ? live_tab_count : design_tabs.size() - (advanced_mode() ? 0 : 1);
}

auto session::picked_part() const -> std::optional<std::size_t> {
    const auto design = design_parts(prototype());
    const auto here = std::ranges::find_if(design, [&](const design_part& part) {
        return part.pointer == field_pointer && part.mount == cursor;
    });
    const auto found =
        here != design.end()
            ? here
            : std::ranges::find(design, field_pointer, &design_part::pointer);
    return found == design.end()
             ? std::nullopt
             : std::optional(static_cast<std::size_t>(found - design.begin()));
}

auto session::item_spawns() const -> std::vector<std::size_t> {
    const auto items = prototype().value("items", nlohmann::ordered_json::array());
    if (!items.is_array()) { return {}; }
    return std::views::iota(std::size_t{0}, items.size())
         | std::views::filter([&](const std::size_t index) {
               return items[index].value("x", 0) == cursor.x
                   && items[index].value("y", 0) == cursor.y;
           })
         | std::ranges::to<std::vector>();
}

auto session::drive() const -> std::vector<drive_check> {
    const auto* const veh = shown_vehicle();
    if (!veh) { return {}; }
    if (live) {
        return drive_checks(*veh, [&](const int index) {
            return veh->cpart(index).ammo_current();
        });
    }
    // The blueprint holds the parts in the order the prototype lists them, without their fuel.
    const auto design = design_parts(prototype());
    return drive_checks(*veh, [&](const int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= design.size()) {
            return itype_id::NULL_ID();
        }
        const auto at = nlohmann::ordered_json::json_pointer(design[index].pointer);
        const auto& entry = prototype().contains(at) ? prototype()[at] : nlohmann::ordered_json();
        return entry.is_object() && entry.contains("fuel") && entry["fuel"].is_string()
                 ? itype_id(entry["fuel"].get<std::string>())
                 : itype_id::NULL_ID();
    });
}

auto session::history() const -> history_steps {
    if (!live) { return document_steps(*doc, design_step_label); }
    auto labels =
        live_undo | std::views::transform(&live_step::label) | std::ranges::to<std::vector>();
    std::ranges::copy(live_redo | std::views::reverse | std::views::transform(&live_step::label),
                      std::back_inserter(labels));
    return {.labels = std::move(labels), .depth = live_undo.size()};
}

namespace {

/// The session as the player's actions change it.
struct editing: session {
    /// Takes the part `id` in hand, and shows its category, from the first page when that is
    /// another one.
    auto hold(std::string id) -> void {
        const auto before = shown_group();
        brush = std::move(id);
        part_group.reset();
        if (part_filter.empty() && shown_group() != before) { part_page = 0; }
    }

    auto reload() -> void {
        built = build_design(prototype().dump());
        field_value = value_at(field_pointer);
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

    /// Replaces the prototype by `next` as one undo step; whether it did.
    auto change(const std::expected<nlohmann::ordered_json, std::string>& next) -> bool {
        if (!next) {
            notice = next.error();
            return false;
        }
        // Frames and armour join the ones beside them as parts come and go.
        const auto edited = doc->edit(
            object,
            nlohmann::ordered_json::diff(prototype(), joined_shapes(prototype(), *next, catalog)));
        if (!edited) { notice = edited.error(); }
        reload();
        return edited.has_value();
    }

    auto remove(const std::size_t index) -> bool {
        if (!built.blueprint) {
            notice = _("The game cannot build this prototype; fix what it reports first.");
            return false;
        }
        return change(remove_part(prototype(), *built.blueprint, index));
    }

    /// Points the field form at `pointer`, with the value there.
    auto point_field(std::string pointer) -> void {
        field_pointer = std::move(pointer);
        field_value = value_at(field_pointer);
    }

    /// Picks the part `index` as `parts_here` names it.
    auto pick(const std::size_t index) -> void {
        if (live) {
            live_part = static_cast<int>(index);
        } else if (const auto design = design_parts(prototype()); index < design.size()) {
            point_field(design[index].pointer);
        }
    }

    /// `format` with the name `what` and the picked mount, as "Install %s at %d,%d".
    auto at_mount(const char* const format, const std::string& what) const -> std::string {
        return string_format(format, what, cursor.x, cursor.y);
    }

    /// Picks the part on top of the picked mount, the one installed last.
    auto pick_top() -> void {
        if (const auto here = parts_here(); !here.empty()) {
            pick(here.back());
        } else if (live) {
            live_part = -1;
        } else if (picked_part()) {
            // A part of another mount stays picked no longer, nor one an index now names anew.
            point_field("/name");
        }
    }

    /// Picks the top part of the picked mount unless a part on it is picked.
    auto keep_pick() -> void {
        if (!picked_here()) { pick_top(); }
    }

    auto remove_picked() -> void {
        const auto name = picked_name();
        const auto index = picked_here();
        const auto removed =
            !name ? false
            : live
                ? live_change(
                      at_mount(_("Remove %s at %d,%d"), *name),
                      [&](vehicle& veh) { return remove_live_part(veh, static_cast<int>(*index)); })
                : remove(*index);
        if (!name) { notice = _("This mount has no part to remove."); }
        if (removed) {
            //~ What the vehicle editor did: the part's name and the mount it was on.
            said = at_mount(_("Removed %s at %d,%d."), *name);
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
        if (moved != design.end()) { point_field(moved->pointer); }
    }

    /// Twice the middle of the vehicle's mounts, so that an even span has it between mounts.
    auto mirror_axis() const -> point {
        const auto points = mounts();
        if (points.empty()) { return point_zero; }
        const auto [low, high] = frame_around(points, 0);
        return low + high;
    }

    /// `squares` and, for each mirror on, their images across the middle of the vehicle's
    /// mounts, each once, the squares given first.
    auto mirrored(std::vector<point> squares) const -> std::vector<point> {
        const auto twice = stroke_axis.value_or(mirror_axis());
        const auto drawn = squares.size();
        for (auto index = std::size_t{0}; index < drawn; ++index) {
            const auto at = squares[index];
            if (mirror_ends) { squares.emplace_back(twice.x - at.x, at.y); }
            if (mirror_sides) { squares.emplace_back(at.x, twice.y - at.y); }
            if (mirror_ends && mirror_sides) {
                squares.emplace_back(twice.x - at.x, twice.y - at.y);
            }
        }
        std::ranges::
            sort(squares.begin() + drawn, squares.end(), [](const point& a, const point& b) {
                return std::tie(a.y, a.x) < std::tie(b.y, b.x);
            });
        auto unique = std::vector<point>{};
        for (const auto& square : squares) {
            if (!std::ranges::contains(unique, square)) { unique.push_back(square); }
        }
        return unique;
    }

    /// The mounts the drawing tool in hand covers from the anchor to `at`, as the map editor's
    /// shapes cover squares; a fill spreads over the mounts of the canvas that show a part of the
    /// kind `at` shows, in any shape, or none.
    auto stroke_squares(const point& at) const -> std::vector<point> {
        const auto bounds = canvas_bounds();
        const auto* const veh = shown_vehicle();
        auto grid = symbol_grid(
            bounds.high.y - bounds.low.y + 1,
            std::vector<std::string>(bounds.high.x - bounds.low.x + 1));
        for (auto y = bounds.low.y; y <= bounds.high.y; ++y) {
            for (auto x = bounds.low.x; x <= bounds.high.x; ++x) {
                const auto shown = veh ? veh->part_displayed_at(tripoint_mnt_veh(x, y, 0)) : -1;
                if (shown >= 0) {
                    grid[y - bounds.low.y][x - bounds.low.x] = kind_name(
                        veh->part_info(shown).get_id());
                }
            }
        }
        const auto drawn = squares_of(
            {.kind = tool().kind.value_or(shape_kind::point),
             .from = anchor.value_or(at) - bounds.low,
             .to = at - bounds.low},
            grid);
        return mirrored(
            drawn | std::views::transform([&](const point& square) { return square + bounds.low; })
            | std::ranges::to<std::vector>());
    }

    /// Whether a stroke is open, so that its changes make one step.
    auto in_stroke() const -> bool { return live ? live_stroke : doc && doc->grouping(); }
    auto begin_stroke() -> void {
        stroke_axis = mirror_axis();
        stroke_erased.clear();
        if (live) {
            live_stroke = true;
            live_stroke_made = false;
            live_stroke_before = export_design(*live, "");
        } else if (doc) {
            doc->begin_group();
        }
    }
    auto end_stroke() -> void {
        // As a design's stroke, the step names all the stroke did.
        if (live && live_stroke_made && !live_undo.empty()) {
            live_undo.back().label = design_step_label(
                {.before = live_stroke_before, .after = export_design(*live, "")});
        }
        live_stroke = false;
        live_stroke_made = false;
        stroke_axis.reset();
        stroke_erased.clear();
        if (doc) { doc->end_group(); }
        // A selection spans one drag.
        if (canvas_tool == "select") { anchor.reset(); }
    }

    /// A change of the parts of the vehicle shown: `live` on the vehicle edited in place, else
    /// `design` on the prototype; `nothing` says why no part changed when none refused.
    struct parts_change {
        std::function<
            auto(const nlohmann::ordered_json&)->std::expected<stroke_design, std::string>>
            design;
        std::function<auto(vehicle&)->std::expected<stroke_tally, std::string>> live;
        std::string nothing;
    };

    /// Makes `parts` as one step named `label`, which is read once the change ran; a design's step
    /// without a name is named by what it changed. How it went; none, with the notice saying why,
    /// when no part changed.
    auto change_parts(const std::string& label, const parts_change& parts)
        -> std::optional<stroke_tally> {
        auto tally = stroke_tally{};
        const auto counted = [&](const stroke_tally& done) -> std::expected<void, std::string> {
            tally = done;
            if (tally.done > 0) { return {}; }
            return std::unexpected(tally.refused > 0 ? tally.reason : parts.nothing);
        };
        auto made = false;
        if (live) {
            made = live_change(label, [&](vehicle& veh) {
                return parts.live(veh).and_then(counted);
            });
        } else {
            const auto open = !in_stroke();
            if (open) { doc->begin_group(false, label); }
            made = change(parts.design(prototype()).and_then([&](const stroke_design& done) {
                return counted(done.tally).transform([&] { return done.prototype; });
            }));
            if (open) { doc->end_group(); }
        }
        return made ? std::optional(tally) : std::nullopt;
    }

    /// Installs the part in hand on `squares` as one step, each after the part it needs on its
    /// square first, as a wheel its hub, and says how it went.
    auto install_stroke(const std::vector<point>& squares) -> void {
        const auto* const veh = shown_vehicle();
        auto paste = paste_install{.layers = {{}, {}}};
        // The names of what goes on, the parts needed first leading, each once.
        auto names = std::vector<std::string>{};
        for (const auto& at : squares) {
            const auto first =
                veh ? prerequisite_of(catalog, *veh, {.mount = at, .part = brush}) : std::nullopt;
            if (first) {
                paste.layers[0].push_back({.mount = at, .part = first->str()});
                if (!std::ranges::contains(names, (*first)->name())) {
                    names.push_back((*first)->name());
                }
            }
            paste.layers[1].push_back({.mount = at, .part = brush});
        }
        const auto name = part_name(brush);
        names.push_back(name);
        const auto together = named_together(names);
        // How many squares have the part in hand, in any shape, to tell how many the stroke gave
        // it.
        const auto holding = [&] {
            const auto* const shown = shown_vehicle();
            return shown
                     ? std::ranges::count_if(
                           squares,
                           [&](const point& at) {
                               return std::ranges::any_of(
                                   shown->parts_at_relative(tripoint_mnt_veh(at.x, at.y, 0), false),
                                   [&](const int index) {
                                       return kind_name(shown->part_info(index).get_id())
                                           == kind_name(vpart_id(brush));
                                   });
                           })
                     : 0;
        };
        const auto held_before = holding();
        // The step of the vehicle edited in place names what the stroke installed, known once it
        // ran.
        auto label = std::string{};
        const auto named = [&](const stroke_tally& done) {
            label =
                squares.size() == 1
                    ? at_mount(_("Install %s at %d,%d"), together)
                    //~ A step of the vehicle editor that installed one part on many mounts:
                    //~ its name and how many.
                    : string_format(_("Install %1$s ×%2$d"), together, holding() - held_before);
            return done;
        };
        // A new tank comes filled with the fuel the engine burns.
        const auto fuel = built.blueprint ? engine_liquid(*built.blueprint) : std::nullopt;
        const auto tally = change_parts(
            label,
            {.design =
                 [&](const nlohmann::ordered_json& from) {
                     auto next = paste_parts(from, built.blueprint.get(), paste);
                     if (!next || !fuel) { return next; }
                     return filled_tanks(from, next->prototype, *fuel)
                         .transform([&](nlohmann::ordered_json filled) {
                             return stroke_design{
                                 .prototype = std::move(filled), .tally = next->tally};
                         });
                 },
             .live = [&](vehicle& veh) { return paste_live_parts(veh, paste).transform(named); },
             // Every square has the part already.
             .nothing = squares.size() == 1
                          ? string_format(_("This mount already has %s."), name)
                          : string_format(_("Every square of the stroke already has %s."), name)});
        if (!tally) { return; }
        const auto held = vpart_id(brush);
        // A tank comes empty but in a design whose engine burns a liquid, which the part's
        // settings change.
        const auto tank = held.is_valid() && held->has_flag(VPFLAG_FLUIDTANK);
        if (squares.size() == 1) {
            said =
                !tank ? at_mount(_("Installed %s at %d,%d."), together)
                : !live && fuel
                    //~ What the vehicle editor did: the tank's name, the mount it is on and the
                    //~ fuel it is filled with.
                    ? string_format(_("Installed %1$s at %2$d,%3$d, filled with %4$s."), together,
                                    cursor.x, cursor.y, item::nname(*fuel))
                    //~ What the vehicle editor did: the part's name and the mount it is on.
                    : at_mount(_("Installed %s at %d,%d. Set its fuel under Fuel."), together);
            return;
        }
        const auto given = static_cast<int>(holding() - held_before);
        //~ What a stroke of the vehicle editor did: the part's name and how many squares took it.
        said = string_format(
            vgettext("Installed %1$s on %2$d square.", "Installed %1$s on %2$d squares.", given),
            together, given);
        if (tally->refused > 0) {
            //~ How many squares of a stroke would not take the part, and why the first would not.
            said +=
                " "
                + string_format(
                    vgettext("%1$d square refused: %2$s", "%1$d squares refused: %2$s",
                             tally->refused),
                    tally->refused, tally->reason);
        }
        if (tank) { said += " " + std::string(_("Set their fuel under Fuel.")); }
    }

    /// Removes the top part of the mount `at`, and of its mirror images that have one, as one
    /// step; a stroke takes one part off each square however often it crosses it.
    auto erase(const point& at) -> void {
        const auto open = !in_stroke();
        if (open) { begin_stroke(); }
        auto removed = 0;
        auto crossed = false;
        auto first_said = std::string{};
        auto first_notice = std::string{};
        // How far taking parts away moved the mounts, which the squares still to erase follow.
        auto moved = point_zero;
        for (const auto& mirror : mirrored({at})) {
            const auto square = mirror - moved;
            if (std::ranges::contains(stroke_erased, square)) {
                crossed = true;
                continue;
            }
            const auto* const veh = shown_vehicle();
            if (!veh
                || veh->parts_at_relative(tripoint_mnt_veh(square.x, square.y, 0), false).empty()) {
                continue;
            }
            said.clear();
            notice.clear();
            cursor = square;
            tool_tab = "mount";
            pick_top();
            remove_picked();
            // The cursor followed the mounts if taking the part away moved them.
            moved += square - cursor;
            if (!said.empty()) {
                ++removed;
                stroke_erased.push_back(cursor);
                if (first_said.empty()) { first_said = said; }
            } else if (first_notice.empty()) {
                first_notice = notice;
            }
        }
        if (open) { end_stroke(); }
        cursor = at - moved;
        pick_top();
        notice =
            removed == 0 && first_notice.empty() && !crossed
                ? std::string(_("This mount has no part to remove."))
                : first_notice;
        said =
            removed == 1 ? first_said
            //~ What the vehicle editor's eraser took away, with its mirrors: how many parts.
            : removed > 1
                ? string_format(vgettext("Removed %d part.", "Removed %d parts.", removed), removed)
                : std::string{};
    }

    /// Takes the part the mount `at` shows in hand, and the pencil to draw it, as Factorio's
    /// pipette does.
    auto eyedrop(const point& at) -> void {
        cursor = at;
        const auto* const veh = shown_vehicle();
        const auto shown = veh ? veh->part_displayed_at(tripoint_mnt_veh(at.x, at.y, 0)) : -1;
        if (shown < 0) {
            notice = _("This mount has no part to take.");
        } else {
            hold(veh->part_info(shown).get_id().str());
            canvas_tool = "install";
        }
        pick_top();
    }

    /// Picks the mount `at` and does there what the canvas tool does.
    auto click(const point& at) -> void {
        const auto& used = tool();
        if (used.key == std::string_view("remove")) {
            erase(at);
            return;
        }
        if (used.key == std::string_view("pick")) {
            eyedrop(at);
            return;
        }
        if (used.key == std::string_view("paste")) {
            paste_at(at);
            return;
        }
        if (used.key == std::string_view("select")) {
            // A drag marks the mounts from where it was pressed to where it was released; a
            // click marks none.
            selection =
                in_stroke() && anchor && *anchor != at
                    ? std::optional(frame_around({*anchor, at}, 0))
                    : std::nullopt;
            anchor = in_stroke() ? std::optional(anchor.value_or(at)) : std::optional<point>{};
        }
        cursor = at;
        if (used.kind) {
            // What the click changes shows at once.
            tool_tab = "mount";
            if (brush.empty()) {
                notice = _("Choose a part from the Parts palette first.");
            } else if (used.spans && !anchor) {
                anchor = at;
            } else {
                const auto squares = stroke_squares(at);
                anchor.reset();
                install_stroke(squares);
            }
        }
        pick_top();
    }

    /// The parts on the marked mounts, by mount in install order, with the settings the design
    /// gives them; none, with a notice that says so, when they have none.
    auto marked_clip() -> std::optional<std::vector<clip_mount>> {
        const auto* const veh = shown_vehicle();
        const auto marked = marked_mounts();
        const auto design = live ? std::vector<design_part>{} : design_parts(prototype());
        auto taken = std::vector<clip_mount>{};
        for (const auto& at : marked) {
            const auto here =
                veh ? veh->parts_at_relative(tripoint_mnt_veh(at.x, at.y, 0), false)
                    : std::vector<int>{};
            if (here.empty()) { continue; }
            auto parts =
                here | std::views::transform([&](const int index) {
                    return nlohmann::ordered_json(veh->part_info(index).get_id().str());
                })
                | std::ranges::to<std::vector>();
            // The design's entries of the mount, when they are the parts the vehicle shows.
            const auto entries =
                design
                | std::views::filter([&](const design_part& entry) { return entry.mount == at; })
                | std::ranges::to<std::vector>();
            if (entries.size() == parts.size()
                && std::ranges::equal(
                    entries, parts, {},
                    [](const design_part& entry) { return entry.from_palette ? "" : entry.part; },
                    [](const nlohmann::ordered_json& part) { return part.get<std::string>(); })) {
                parts =
                    entries | std::views::transform([&](const design_part& entry) {
                        return prototype()[nlohmann::ordered_json::json_pointer(entry.pointer)];
                    })
                    | std::ranges::to<std::vector>();
            }
            taken.push_back({.offset = at - marked.front(), .parts = std::move(parts)});
        }
        if (taken.empty()) {
            notice = selection ? _("The selection has no part to copy.")
                               : _("This mount has no part to copy.");
            return std::nullopt;
        }
        return taken;
    }

    /// Takes the parts on the marked mounts as the clip.
    auto copy_mounts() -> void {
        auto taken = marked_clip();
        if (!taken) { return; }
        clip = std::move(*taken);
        const auto count = static_cast<int>(std::ranges::distance(
            clip | std::views::transform(&clip_mount::parts) | std::views::join));
        //~ What the vehicle editor's Copy took: how many parts.
        said = string_format(vgettext("Copied %d part.", "Copied %d parts.", count), count);
    }

    /// Removes the parts on the marked mounts as one step, top down, after copying them for a
    /// `cut`.
    auto delete_mounts(const bool cut) -> void {
        // A cut takes what it removes as the clip, once something went.
        auto taken = cut ? marked_clip() : std::nullopt;
        if (cut && !taken) { return; }
        const auto marked = marked_mounts();
        const auto tally = change_parts(
            cut ? _("Cut") : _("Delete"),
            {.design =
                 [&](const nlohmann::ordered_json& from) { return remove_parts(from, marked); },
             .live = [&](vehicle& veh) -> std::expected<stroke_tally, std::string> {
                 return remove_live_parts(veh, marked);
             },
             .nothing = selection ? _("The selection has no part to delete.")
                                  : _("This mount has no part to remove.")});
        pick_top();
        if (!tally) { return; }
        if (taken) {
            // The clip holds what went: the top parts of each mount, as many as it lost.
            for (auto& [offset, parts] : *taken) {
                const auto went = std::ranges::count(tally->removed, marked.front() + offset);
                parts.erase(parts.begin(), parts.end() - went);
            }
            std::erase_if(*taken, [](const clip_mount& mount) { return mount.parts.empty(); });
            clip = std::move(*taken);
        }
        said =
            cut
                //~ What the vehicle editor's Cut took away: how many parts.
                ? string_format(vgettext("Cut %d part.", "Cut %d parts.", tally->done), tally->done)
                //~ What the vehicle editor's Delete took away: how many parts.
                : string_format(
                      vgettext("Deleted %d part.", "Deleted %d parts.", tally->done), tally->done);
        said += refused_parts(*tally);
    }

    /// Installs the clip with its north-west corner on the mount `at`, a layer of parts at a
    /// time, as one step.
    auto paste_at(const point& at) -> void {
        cursor = at;
        tool_tab = "mount";
        auto paste = paste_install{};
        auto placed = std::vector<placed_entry>{};
        for (const auto& [offset, parts] : clip) {
            for (auto layer = std::size_t{0}; layer < parts.size(); ++layer) {
                if (paste.layers.size() <= layer) { paste.layers.emplace_back(); }
                paste.layers[layer].push_back(
                    {.mount = at + offset, .part = entry_part(parts[layer])});
                placed.push_back({.mount = at + offset, .entry = parts[layer]});
            }
        }
        const auto tally = change_parts(
            _("Paste"),
            {.design =
                 [&](const nlohmann::ordered_json& from) {
                     // The parts keep the settings they had, such as a tank's fuel.
                     return paste_parts(from, built.blueprint.get(), paste)
                         .transform([&](stroke_design done) {
                             done.prototype = given_members(from, done.prototype, placed);
                             return done;
                         });
                 },
             .live = [&](vehicle& veh) { return paste_live_parts(veh, paste); },
             .nothing = _("Copy or cut squares first.")});
        if (tally) {
            //~ What the vehicle editor's Paste installed: how many parts.
            said = string_format(
                       vgettext("Pasted %d part.", "Pasted %d parts.", tally->done), tally->done)
                 + refused_parts(*tally);
        }
        pick_top();
    }

    /// Moves each mount of the clip to where `moved` takes its offset, its north-west corner
    /// again at none, and says what it did.
    auto reshape_clip(const std::function<auto(const point&)->point>& moved, const char* const what)
        -> void {
        if (clip.empty()) {
            notice = _("Copy or cut squares first.");
            return;
        }
        for (auto& mount : clip) { mount.offset = moved(mount.offset); }
        const auto low = point(
            std::ranges::min(
                clip
                | std::views::transform([](const clip_mount& mount) { return mount.offset.x; })),
            std::ranges::min(
                clip
                | std::views::transform([](const clip_mount& mount) { return mount.offset.y; })));
        for (auto& mount : clip) { mount.offset -= low; }
        // Frames and armour join their new neighbours once pasted.
        said = what;
        take_tool("paste");
    }

    /// Takes what `clip_json` made as the clip, with Paste in hand; whether it held parts.
    auto take_clip(const nlohmann::ordered_json& data) -> bool {
        auto taken = std::vector<clip_mount>{};
        const auto mounts = data.value("mounts", nlohmann::ordered_json::array());
        for (const auto& mount : mounts.is_array() ? mounts : nlohmann::ordered_json::array()) {
            const auto parts =
                mount.is_object()
                    ? mount.value("parts", nlohmann::ordered_json())
                    : nlohmann::ordered_json();
            if (!parts.is_array() || !mount.value("x", nlohmann::ordered_json()).is_number_integer()
                || !mount.value("y", nlohmann::ordered_json()).is_number_integer()) {
                continue;
            }
            auto kept =
                parts | std::views::filter([](const nlohmann::ordered_json& part) {
                    return (part.is_string() || part.is_object())
                        && vpart_id(entry_part(part)).is_valid();
                })
                | std::ranges::to<std::vector>();
            if (!kept.empty()) {
                taken.push_back(
                    {.offset = {mount["x"].get<int>(), mount["y"].get<int>()},
                     .parts = std::move(kept)});
            }
        }
        if (taken.empty()) { return false; }
        clip = std::move(taken);
        take_tool("paste");
        return true;
    }

    /// Keeps the clip in the library as `name`.
    auto save_to_library(const std::string& name) -> void {
        if (clip.empty()) {
            notice = _("Copy or cut squares first.");
            return;
        }
        if (name.empty()) {
            notice = _("Give the piece a name.");
            return;
        }
        keep_piece("vehicle", {.name = name, .data = clip_json(clip)});
        library = library_pieces("vehicle");
        //~ %s is the name the player gave a piece kept in the vehicle editor's library.
        said = string_format(_("Saved %s to the library."), name);
    }

    /// Takes the library's piece `index` as the clip.
    auto take_from_library(const std::size_t index) -> void {
        if (index >= library.size()) { return; }
        if (!take_clip(library[index].data)) {
            notice = _("This piece has no part the game knows.");
            return;
        }
        //~ %s is the name of a piece kept in the vehicle editor's library.
        said = string_format(_("Click where %s goes."), library[index].name);
    }

    /// Takes the clip shared as `text`.
    auto paste_text(const std::string& text) -> void {
        const auto data = piece_of_text("vehicle", text);
        if (!data || !take_clip(*data)) {
            notice = _("That text holds no vehicle parts.");
            return;
        }
        clip_text_shown = false;
        said = _("Click where the parts go.");
    }

    /// The design's parts on the marked mounts of the kind `kind`, last first.
    auto marked_of_kind(const std::string& kind) const -> std::vector<std::size_t> {
        const auto marked = marked_mounts();
        const auto design = design_parts(prototype());
        return std::views::iota(std::size_t{0}, design.size()) | std::views::reverse
             | std::views::filter([&](const std::size_t index) {
                   return !design[index].from_palette
                       && std::ranges::contains(marked, design[index].mount)
                       && kind_name(vpart_id(design[index].part)) == kind;
               })
             | std::ranges::to<std::vector>();
    }

    /// Swaps every part of the kind `kind` on the marked mounts for the part in hand, as one
    /// step.
    auto replace_kind(const std::string& kind) -> void {
        const auto held = vpart_id(brush);
        if (!held.is_valid()) {
            notice = _("Choose the part to put in their place under Parts first.");
            return;
        }
        const auto targets = marked_of_kind(kind);
        if (targets.empty()) { return; }
        auto next = prototype();
        for (const auto index : targets) {
            auto swapped = set_part_member(next, {.index = index, .key = "part", .value = brush});
            if (!swapped) {
                notice = swapped.error();
                return;
            }
            next = std::move(*swapped);
        }
        doc->begin_group(false, _("Replace"));
        const auto changed = change(next);
        doc->end_group();
        if (changed) {
            //~ What the vehicle editor's Replace did: the parts it swapped, how many and the
            //~ part that took their place.
            said = string_format(
                _("Replaced %1$s ×%2$d with %3$s."), part_name(kind), targets.size(),
                part_name(brush));
        }
    }

    /// Removes every part of the kind `kind` on the marked mounts that can go, as one step.
    auto delete_kind(const std::string& kind) -> void {
        auto next = prototype();
        auto done = 0;
        auto refused = stroke_tally{};
        for (const auto index : marked_of_kind(kind)) {
            // Each removal checks the vehicle the ones before it left.
            const auto now = build_design(next.dump());
            if (!now.blueprint) { break; }
            auto removed = remove_part(next, *now.blueprint, index);
            if (!removed) {
                if (refused.refused++ == 0) { refused.reason = removed.error(); }
                continue;
            }
            next = std::move(*removed);
            ++done;
        }
        if (done == 0) {
            notice =
                refused.refused > 0
                    ? refused.reason
                    : _("The game cannot build this prototype; fix what it reports first.");
            return;
        }
        doc->begin_group(false, _("Delete"));
        const auto changed = change(next);
        doc->end_group();
        pick_top();
        if (changed) {
            //~ What the vehicle editor's Delete took away: how many parts.
            said = string_format(vgettext("Deleted %d part.", "Deleted %d parts.", done), done)
                 + refused_parts(refused);
        }
    }

    /// Takes the canvas tool `key` in hand, which Paste only is with something copied.
    auto take_tool(const std::string& key) -> void {
        if (key == "paste" && clip.empty()) {
            notice = _("Copy or cut squares first.");
            return;
        }
        canvas_tool = key;
        anchor.reset();
    }

    /// Turns the canvas a quarter clockwise, or counterclockwise for a negative `quarters`.
    auto turn_view(const int quarters) -> void {
        view_quarters = ((view_quarters + quarters) % 4 + 4) % 4;
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

    /// The parts a setting of the picked part `index` goes to: with the picked mount inside the
    /// selection, every part of its kind there, else the picked part alone.
    auto setting_targets(const std::size_t index) const -> std::vector<std::size_t> {
        const auto design = design_parts(prototype());
        const auto inside = [&](const point& at) {
            return selection && at.x >= selection->low.x && at.x <= selection->high.x
                && at.y >= selection->low.y && at.y <= selection->high.y;
        };
        if (!inside(design[index].mount)) { return {index}; }
        const auto kind = kind_name(vpart_id(design[index].part));
        return std::views::iota(std::size_t{0}, design.size())
             | std::views::filter([&](const std::size_t other) {
                   return !design[other].from_palette && inside(design[other].mount)
                       && kind_name(vpart_id(design[other].part)) == kind;
               })
             | std::ranges::to<std::vector>();
    }

    /// `from` with the member `key` of its part `index` set from the part form: a number for
    /// `ammo`, a list for `ammo_types`, either end of `ammo_qty` for `ammo_min` and `ammo_max`,
    /// another shape of it for `shape`, text for the rest; empty removes it.
    auto part_setting(
        const nlohmann::ordered_json& from, const std::size_t index, const std::string& key,
        const std::string& text) const -> std::expected<nlohmann::ordered_json, std::string> {
        const auto design = design_parts(from);
        if (key == "ammo_types") {
            return set_part_member(from, {.index = index, .key = key, .value = listed(text)});
        }
        if (key == "ammo_min" || key == "ammo_max") {
            // A turret with no range is full, so a first end sets both; empty makes it full again.
            auto value = std::optional<nlohmann::ordered_json>{};
            if (!text.empty()) {
                const auto read = number_in(text, 0, std::numeric_limits<int>::max());
                if (!read) { return std::unexpected(_("That value is out of range.")); }
                const auto at = nlohmann::ordered_json::json_pointer(design[index].pointer);
                const auto current =
                    from[at].is_object()
                        ? from[at].value("ammo_qty", nlohmann::ordered_json())
                        : nlohmann::ordered_json();
                auto range =
                    current.is_array() && current.size() == 2 && current[0].is_number_integer()
                            && current[1].is_number_integer()
                        ? std::array{current[0].get<int>(), current[1].get<int>()}
                        : std::array{*read, *read};
                range[key == "ammo_max" ? 1 : 0] = *read;
                if (range[0] > range[1]) {
                    return std::unexpected(
                        _("A turret cannot load fewer rounds at most than at least."));
                }
                value = range;
            }
            return set_part_member(from, {.index = index, .key = "ammo_qty", .value = value});
        }
        if (key == "shape") {
            const auto& current = design[index].part;
            if (!std::ranges::contains(
                    catalog.shapes_of(vpart_id(current)), vpart_id(text), &vpart_info::get_id)) {
                //~ %1$s is the part id the player gave, %2$s the name of the part picked.
                return std::unexpected(
                    string_format(_("%1$s is no shape of %2$s."), text, part_name(current)));
            }
            return set_part_member(from, {.index = index, .key = "part", .value = text});
        }
        if (key == "fuel" && !text.empty()
            && !std::ranges::contains(tank_fuels(), itype_id(text))) {
            return std::unexpected(string_format(_("%s is no liquid a tank holds."), text));
        }
        auto value = std::optional<nlohmann::ordered_json>{};
        if (!text.empty()) {
            value = nlohmann::ordered_json(text);
            if (key == "ammo") {
                const auto number = number_in(text, 0, 100);
                if (!number) { return std::unexpected(_("That value is out of range.")); }
                value = *number;
            }
        }
        return set_part_member(from, {.index = index, .key = key, .value = value});
    }

    /// Sets the member `key` of the picked part from the part form, as `part_setting` reads
    /// `text`, and of every part of its kind in the selection around it, as one step.
    auto set_part(const std::string& key, const std::string& text) -> void {
        const auto index = picked_here();
        if (!index) {
            notice = _("Pick a part first.");
            return;
        }
        const auto targets = setting_targets(*index);
        auto next = prototype();
        for (const auto target : targets) {
            auto set = part_setting(next, target, key, text);
            if (!set) {
                notice = set.error();
                return;
            }
            next = std::move(*set);
        }
        if (change(next) && (key == "fuel" || targets.size() > 1)) { say_step(); }
    }

    /// Keeps the settings of the picked part for Paste settings.
    auto copy_settings() -> void {
        const auto index = picked_here();
        if (live || !index) {
            notice = _("Pick a part first.");
            return;
        }
        const auto design = design_parts(prototype());
        const auto& entry =
            prototype()[nlohmann::ordered_json::json_pointer(design[*index].pointer)];
        auto members = entry.is_object() ? entry : nlohmann::ordered_json::object();
        members.erase("part");
        const auto name = part_name(design[*index].part);
        copied = part_settings{
            .kind = kind_name(vpart_id(design[*index].part)), .name = name, .members = members};
        //~ %s is the part whose settings the vehicle editor copied.
        said = string_format(_("Copied the settings of %s."), name);
    }

    /// Gives the picked part, and every part of its kind in the selection around it, the settings
    /// Copy settings took, as one step.
    auto paste_settings() -> void {
        const auto index = picked_here();
        if (live || !index || !copied) { return; }
        const auto design = design_parts(prototype());
        if (kind_name(vpart_id(design[*index].part)) != copied->kind) {
            //~ %s is the part whose settings the vehicle editor holds.
            notice = string_format(_("Pick a %s to paste its settings on."), copied->name);
            return;
        }
        auto next = prototype();
        for (const auto target : setting_targets(*index)) {
            const auto& entry = next[nlohmann::ordered_json::json_pointer(design[target].pointer)];
            auto keys = std::set<std::string>{};
            if (entry.is_object()) {
                for (const auto& [key, value] : entry.items()) { keys.insert(key); }
            }
            for (const auto& [key, value] : copied->members.items()) { keys.insert(key); }
            keys.erase("part");
            for (const auto& key : keys) {
                auto set = set_part_member(
                    next,
                    {.index = target,
                     .key = key,
                     .value = copied->members.contains(key)
                                ? std::optional(copied->members[key])
                                : std::nullopt});
                if (!set) {
                    notice = set.error();
                    return;
                }
                next = std::move(*set);
            }
        }
        if (change(next)) { say_step(); }
    }

    /// Says what the step just made did, as the history names it.
    auto say_step() -> void {
        const auto steps = history();
        if (steps.depth == 0) { return; }
        //~ What the vehicle editor did, as its history names the step.
        said = string_format(_("%s."), steps.labels[steps.depth - 1]);
    }

    /// Makes the design pass its check `key` as the check's fix does, as one step named by it.
    auto fix_drive(const std::string& key) -> void {
        const auto checks = drive();
        const auto found = std::ranges::find(checks, key, &drive_check::key);
        if (found == checks.end() || found->fix == drive_fix::none) { return; }
        if (found->fix == drive_fix::show_engines) {
            part_group = part_category::engines;
            part_filter.clear();
            part_page = 0;
            canvas_tool = "install";
            anchor.reset();
            said = _("Choose an engine under Parts, then click a square of the frame.");
            return;
        }
        doc->begin_group(false, found->action);
        const auto changed = change(fixed_design(prototype(), catalog, found->fix));
        doc->end_group();
        keep_pick();
        if (changed) { say_step(); }
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
    /// `name` of the player's mods, which saving writes; `write` writes it at once. Whether it
    /// opened it.
    auto create(
        const std::string& name,
        const std::function<auto(const std::string&)->nlohmann::ordered_json>& prototype_of,
        const bool write = false) -> bool {
        const auto path = in_mods(name);
        if (!is_mod_file(path)) {
            notice = string_format(
                _("A new design goes in a new .json file in your mods, %s, inside a mod's folder "
                  "such as my_mod/."),
                PATH_INFO::user_moddir());
            return false;
        }
        if (std::filesystem::exists(path)) {
            notice = _("That file already exists; nothing was saved.");
            return false;
        }
        if (const auto checked = check_mod_folder(path); !checked) {
            notice = checked.error();
            return false;
        }
        if (!leave()) { return false; }
        // Saving the design it left may have made that file.
        if (std::filesystem::exists(path)) {
            notice = _("That file already exists; nothing was saved.");
            return false;
        }
        const auto design = nlohmann::ordered_json::array({prototype_of(path.stem().string())});
        auto made = document::create(path, format_json(design.dump()).value_or(design.dump(2)));
        if (!made) {
            notice = made.error();
            return false;
        }
        take(std::move(*made), 0);
        if (write) { save(); }
        return true;
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

    /// Starts a vehicle on a frame in a new file of the user mods, with the install tool and a
    /// frame in hand: `text` is its name, which the file follows, or the path of that .json file,
    /// whose stem names it; either way the id follows the name and is no prototype's.
    auto new_vehicle(const std::string& text) -> void {
        const auto typed = trim(text);
        // A name may hold any character; only a file name says where the file goes.
        const auto is_path = typed.ends_with(".json");
        const auto file = std::filesystem::path(typed).filename().string();
        const auto name =
            is_path ? trim(file.substr(0, file.size() - std::string_view(".json").size())) : typed;
        if (name.empty()) {
            notice = _("A new vehicle needs a name.");
            return;
        }
        const auto free = suggested(id_of(name), true);
        const auto id = std::filesystem::path(free).filename().string();
        const auto frame = catalog.frame;
        const auto made = create(is_path ? typed : free, [&](const std::string& /*stem*/) {
            auto parts = nlohmann::ordered_json::array();
            if (frame) {
                parts.push_back(
                    {{"x", 0}, {"y", 0}, {"parts", nlohmann::ordered_json::array({frame->str()})}});
            }
            return nlohmann::ordered_json{
                {"type", "vehicle"}, {"id", id}, {"name", name}, {"parts", std::move(parts)}};
        });
        if (!made || !frame) { return; }
        canvas_tool = "install";
        hold(frame->str());
        said = _("Pick a part under Parts, then click a square next to the frame.");
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

    /// Runs `edit` on the vehicle edited in place as one undo step, which `label` names; whether
    /// it did.
    auto live_change(
        const std::string& label,
        const std::function<auto(vehicle&)->std::expected<void, std::string>>& edit) -> bool {
        auto before = vehicle_state(*live);
        const auto anchor = live->pivot_anchor[0];
        if (const auto done = edit(*live); !done) {
            notice = done.error();
            return false;
        }
        follow_mounts((anchor - live->pivot_anchor[0]).raw().xy());
        // A stroke is one step, the one its first change made.
        if (!live_stroke || !live_stroke_made) {
            live_undo.push_back({.state = std::move(before), .label = label});
        }
        live_stroke_made = live_stroke;
        live_redo.clear();
        return true;
    }

    /// Moves what the session holds in mounts by `moved` as the vehicle edited in place moved its
    /// mounts, as taking away the part at 0,0 does, so that it stays over the same parts.
    auto follow_mounts(const point& moved) -> void {
        if (moved == point_zero) { return; }
        cursor -= moved;
        frame = {.low = frame.low - moved, .high = frame.high - moved};
        if (selection) {
            selection = {.low = selection->low - moved, .high = selection->high - moved};
        }
        if (anchor) { *anchor -= moved; }
        if (stroke_axis) { *stroke_axis -= moved * 2; }
        for (auto& square : stroke_erased) { square -= moved; }
        if (!live_stroke) { return; }
        for (auto& entry : live_stroke_before["parts"]) {
            entry["x"] = entry.value("x", 0) - moved.x;
            entry["y"] = entry.value("y", 0) - moved.y;
        }
    }

    /// Puts the vehicle edited in place back as it was before its last step, or after the next
    /// step undone when `!back`.
    auto step_live(const bool back) -> void {
        auto& from = back ? live_undo : live_redo;
        auto& to = back ? live_redo : live_undo;
        if (from.empty() || !live) { return; }
        auto now = vehicle_state(*live);
        const auto anchor = live->pivot_anchor[0];
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
        follow_mounts((anchor - live->pivot_anchor[0]).raw().xy());
        // As a design's undo ends its group, the stroke's later changes make steps of their own.
        live_stroke = false;
        live_stroke_made = false;
        to.push_back({.state = std::move(now), .label = std::move(step.label)});
    }

    /// Undoes or redoes until `depth` steps are done.
    auto jump(const std::size_t depth) -> void {
        if (!live) {
            if (doc->jump(depth)) { reload(); }
        } else {
            while (live && live_undo.size() > depth) { step_live(true); }
            while (live && live_undo.size() < depth && !live_redo.empty()) { step_live(false); }
        }
        if (live || doc) { keep_pick(); }
    }

    /// Undoes the last step of the vehicle edited in place or of the open document, or redoes
    /// the next one when `!back`.
    auto step_history(const bool back) -> void {
        if (live) {
            step_live(back);
            if (live) { keep_pick(); }
        } else if (back ? doc->undo() : doc->redo()) {
            reload();
            keep_pick();
        }
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

    /// Fits the grid to the vehicle shown: a margin of free mounts around it, and room to build
    /// it out, when it is first drawn; after that it grows, never shrinks or moves, to keep the
    /// margin around the vehicle and the picked mount, once a stroke ends.
    auto fit_frame() -> void {
        // A stroke keeps the squares a client names by their place where they are.
        if ((!live && !doc) || in_stroke()) { return; }
        auto points = mounts();
        if (points.empty()) { points.push_back(cursor); }
        // The picked mount needs no free mounts beyond it.
        const auto around = frame_around(points, grid_margin);
        const auto needed = frame_around({around.low, around.high, cursor}, 0);
        // Undo puts the vehicle edited in place back as a new one, which is still the same.
        const auto shown =
            live ? std::string("live") : doc->path().string() + "#" + prototype().value("id", "");
        if (shown == framed) {
            frame = frame_around({frame.low, frame.high, needed.low, needed.high}, 0);
            return;
        }
        framed = shown;
        frame = roomy(needed);
        selection.reset();
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
        const auto read =
            number_in(text, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        if (!read) {
            notice = _("That value is out of range.");
            return;
        }
        const auto value = *read;
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

    /// Puts the design in the game in place of its prototype and spawns it east of the avatar;
    /// the vehicle spawned, none when it could not.
    auto spawn() -> vehicle* {
        const auto applied = apply_design(prototype().dump());
        if (!applied) {
            notice = applied.error();
            return nullptr;
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
            return nullptr;
        }
        //~ %s says where the vehicle spawned.
        said = string_format(
            _("%s Vehicles > World > Edit in place changes it where it stands."),
            placed_sentence(false, **spawned));
        return *spawned;
    }

    /// Spawns the design as Spawn does and seats the avatar at its controls, its engines
    /// starting, for the editor key to bring the editor back with the design open; whether it
    /// did.
    auto test_drive() -> bool {
        if (!ready_to_drive(drive())) { return false; }
        auto* const veh = spawn();
        if (!veh) { return false; }
        auto& here = get_map();
        auto seat = std::optional<tripoint_bub_ms>{};
        for (const vpart_reference& part : veh->get_avail_parts(VPFLAG_CONTROLS)) {
            if (here.veh_at(part.pos()).part_with_feature(VPFLAG_BOARDABLE, true)) {
                seat = part.pos();
                break;
            }
        }
        if (!seat) {
            here.destroy_vehicle(veh);
            notice = _("No seat has controls to drive it from.");
            return false;
        }
        auto& you = get_avatar();
        veh->set_owner(you);
        // Stepping onto the seat boards it.
        g->place_player(*seat);
        if (!you.in_vehicle) { here.board_vehicle(*seat, &you); }
        veh->start_engines(true);
        open_vehicle_editor_first();
        //~ %s is the vehicle the editor's test drive put the avatar in.
        add_msg(_("Test driving the %s: the editor key brings you back to its design."), veh->name);
        return true;
    }

    auto set_spawning(const std::string& field, const std::string& text) -> void {
        auto& value =
            field == "facing" ? spawning.facing
            : field == "fuel"
                ? spawning.fuel
                : spawning.condition;
        const auto read =
            field == "facing" ? number_in(text, 0, 359)
            : field == "fuel"
                ? number_in(text, 0, 100)
                : number_in(text, -1, 1);
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
        saved();
        return true;
    }

    auto save_as(const std::string& name) -> void {
        if (told(save_document_as(*doc, in_mods(name)))) { saved(); }
    }

    /// Remembers the file just written and says where it is.
    auto saved() -> void {
        remember();
        //~ %s is the file the vehicle editor wrote, such as my_mod/my_vehicle.json.
        said = string_format(_("Saved %s."), mods_path(doc->path()));
    }

    /// Whether to leave: at once with nothing unsaved, else as the player answers.
    auto leave() -> bool {
        if (!doc || !doc->dirty()) { return true; }
        const auto answer = ask_leave();
        if (answer == leave_answer::discard) { doc.reset(); }
        return answer == leave_answer::discard || (answer == leave_answer::save && save());
    }
};

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
    std::optional<mount_frame> selection;
    bool said = false;
    auto operator==(const session_footprint&) const -> bool = default; // *NOPAD*
};

/// What the next action may change, to tell whether it changed anything.
auto footprint(const session& state) -> session_footprint {
    const auto steps = state.live || state.doc ? state.history() : history_steps{};
    return {
        .design =
            state.doc ? state.doc->path().string() + "#" + state.prototype().value("id", "") : "",
        .steps = steps.labels.size(),
        .depth = steps.depth,
        .saved = steps.saved,
        .live = state.live,
        .spawning = {state.spawning.facing, state.spawning.fuel, state.spawning.condition},
        .tool = state.canvas_tool,
        .brush = state.brush,
        .selection = state.selection,
        .said = !state.said.empty()};
}

auto kept_session = std::optional<editing>{};

} // namespace

} // namespace editor::vehicle_editor

namespace editor {

auto reset_vehicle_editor() -> void { vehicle_editor::kept_session.reset(); }

auto run_vehicle_editor(const std::string& prototype) -> bool {
    using namespace vehicle_editor;
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto& state = kept_session ? *kept_session : kept_session.emplace();
    // What it said on an earlier visit is old news.
    state.notice.clear();
    state.said.clear();
    state.catalog = read_part_catalog();
    state.library = library_pieces("vehicle");
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
        // A notice stays until an action changes something or brings a notice of its own; what a
        // drag did stays past the drag's end.
        if (id != stroke_end_id) { state.said.clear(); }
        const auto before = footprint(state);
        auto kept = std::exchange(state.notice, {});
        state.watch();
        const auto tool = std::ranges::find(canvas_tools, id, &canvas_tool::affordance);
        if (id == source_id || id == tool_id) {
            state.show(row.substr(row.find(':') + 1));
        } else if (id.starts_with(show_prefix)) {
            state.show(id.substr(show_prefix.size()));
        } else if (id == advanced_id) {
            set_advanced_mode(!advanced_mode());
        } else if (id == canvas_tool_id || id == mirror_sides_id || id == mirror_ends_id) {
            const auto key =
                id == canvas_tool_id
                    ? std::string(row_rest(row, "canvas_tool").value_or("select"))
                    : std::string(row_rest(id, "vehicle").value_or(""));
            if (key == "mirror_sides") {
                state.mirror_sides = !state.mirror_sides;
            } else if (key == "mirror_ends") {
                state.mirror_ends = !state.mirror_ends;
            } else {
                state.take_tool(key);
            }
        } else if (tool != canvas_tools.end()) {
            state.take_tool(tool->key);
        } else if (id == view_cw_id || id == view_ccw_id) {
            state.turn_view(id == view_cw_id ? 1 : -1);
        } else if (id == choose_id) {
            state.hold(std::string(row_rest(row, "vpart").value_or("")));
            // A drawing tool in hand draws the part taken; another takes the pencil.
            if (!state.tool().kind) { state.canvas_tool = "install"; }
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
        } else if (id == part_group_id) {
            if (const auto group = row_index(row, "group")) {
                state.part_group = static_cast<part_category>(*group);
                state.part_page = 0;
            }
        } else if (!state.live && !state.doc) {
            // Nothing is open to act on.
        } else if (id == mount_id) {
            if (const auto at = row_point(row, "mount")) { state.click(*at); }
        } else if (id == clear_id) {
            if (const auto at = row_point(row, "mount")) { state.erase(*at); }
        } else if (id == stroke_begin_id) {
            state.begin_stroke();
        } else if (id == stroke_end_id) {
            state.end_stroke();
        } else if (id == copy_mounts_id) {
            state.copy_mounts();
        } else if (id == clip_turn_id) {
            state.reshape_clip(
                [](const point& at) { return point(-at.y, at.x); }, _("Turned the copy."));
        } else if (id == clip_flip_sides_id) {
            state.reshape_clip(
                [](const point& at) { return point(at.x, -at.y); },
                _("Flipped the copy left and right."));
        } else if (id == clip_flip_ends_id) {
            state.reshape_clip(
                [](const point& at) { return point(-at.x, at.y); },
                _("Flipped the copy front and back."));
        } else if (id == show_clip_text_id) {
            state.clip_text_shown = !state.clip_text_shown;
        } else if (id == paste_text_id) {
            state.paste_text(text);
        } else if (id == library_save_id) {
            state.save_to_library(text);
        } else if (id == library_take_id) {
            if (const auto index = row_index(row, "piece")) { state.take_from_library(*index); }
        } else if (id == cut_mounts_id || id == delete_mounts_id) {
            state.delete_mounts(id == cut_mounts_id);
        } else if (id == remove_id) {
            state.remove_picked();
        } else if (id == pick_id) {
            if (const auto index = row_index(row, "part")) { state.pick(*index); }
        } else if (id == undo_id || id == redo_id) {
            state.step_history(id == undo_id);
        } else if (id == jump_id) {
            if (const auto depth = row_index(row, "step")) { state.jump(*depth); }
        } else if (state.live) {
            // The world's vehicle edited in place takes these alone.
            if (id == done_id) {
                state.live = nullptr;
            } else if (id == live_set_id) {
                state.set_live(row, text);
            }
        } else if (id == earlier_id || id == later_id) {
            state.move_picked(id == earlier_id ? -1 : 1);
        } else if (id == turn_cw_id || id == turn_ccw_id) {
            state.turn(id == turn_cw_id ? 1 : -1);
        } else if (id == prop_set_id) {
            state.set_prop(row, text);
        } else if (id == part_set_id) {
            state.set_part(row, text);
        } else if (id == replace_kind_id || id == delete_kind_id) {
            if (const auto kind = row_rest(row, "kind")) {
                if (id == replace_kind_id) {
                    state.replace_kind(std::string(*kind));
                } else {
                    state.delete_kind(std::string(*kind));
                }
            }
        } else if (id == copy_settings_id) {
            state.copy_settings();
        } else if (id == paste_settings_id) {
            state.paste_settings();
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
                state.point_field(text);
            } else {
                state.field_value = text;
            }
        } else if (id == field_apply_id) {
            state.set_field();
        } else if (id == save_id) {
            state.save();
        } else if (id == save_as_id) {
            state.save_as(text);
        } else if (id == spawn_set_id) {
            state.set_spawning(row, text);
        } else if (id == spawn_id) {
            state.spawn();
        } else if (id == test_drive_id) {
            if (state.test_drive()) { return true; }
        } else if (id.starts_with(fix_prefix)) {
            state.fix_drive(id.substr(fix_prefix.size()));
        }
        if (state.notice.empty() && footprint(state) == before) { state.notice = std::move(kept); }
    }
}

} // namespace editor
