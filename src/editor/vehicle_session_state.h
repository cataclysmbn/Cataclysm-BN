#pragma once

#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/history_label.h"
#include "editor/mapgen_stroke.h"
#include "editor/vehicle_check.h"
#include "editor/vehicle_design.h"
#include "engine_client_component.h"
#include "point.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// The vehicle editor screen's state and view, which vehicle_session.cpp runs.
namespace editor::vehicle_editor {

inline constexpr auto prototypes_id = "vehicle:prototypes";
inline constexpr auto grid_id = "vehicle:grid";
inline constexpr auto turned_id = "vehicle:turned";
inline constexpr auto stack_id = "vehicle:stack";
inline constexpr auto parts_id = "vehicle:parts";
inline constexpr auto part_groups_id = "vehicle:part_groups";
inline constexpr auto part_group_id = "vehicle:part_group";
inline constexpr auto field_id = "vehicle:field";
inline constexpr auto sources_id = "vehicle:sources";
inline constexpr auto tools_id = "vehicle:tools";
inline constexpr auto props_id = "vehicle:props";
inline constexpr auto part_form_id = "vehicle:part_form";
inline constexpr auto items_id = "vehicle:items";

inline constexpr auto open_id = "vehicle:open";
inline constexpr auto filter_id = "vehicle:filter";
inline constexpr auto page_id = "vehicle:page";
inline constexpr auto mount_id = "vehicle:mount";
inline constexpr auto pick_id = "vehicle:pick";
inline constexpr auto remove_id = "vehicle:remove";
inline constexpr auto clear_id = "vehicle:clear";
inline constexpr auto earlier_id = "vehicle:earlier";
inline constexpr auto later_id = "vehicle:later";
inline constexpr auto choose_id = "vehicle:choose";
inline constexpr auto canvas_tools_id = "vehicle:canvas_tools";
inline constexpr auto canvas_tool_id = "vehicle:canvas_tool";
inline constexpr auto stroke_begin_id = "vehicle:stroke_begin";
inline constexpr auto stroke_end_id = "vehicle:stroke_end";
inline constexpr auto mirror_sides_id = "vehicle:mirror_sides";
inline constexpr auto mirror_ends_id = "vehicle:mirror_ends";
inline constexpr auto copy_mounts_id = "vehicle:copy_mounts";
inline constexpr auto copy_settings_id = "vehicle:copy_settings";
inline constexpr auto paste_settings_id = "vehicle:paste_settings";
inline constexpr auto cut_mounts_id = "vehicle:cut_mounts";
inline constexpr auto marked_kinds_id = "vehicle:marked_kinds";
inline constexpr auto replace_kind_id = "vehicle:replace_kind";
inline constexpr auto delete_kind_id = "vehicle:delete_kind";
inline constexpr auto delete_mounts_id = "vehicle:delete_mounts";
inline constexpr auto view_cw_id = "vehicle:view_cw";
inline constexpr auto view_ccw_id = "vehicle:view_ccw";
inline constexpr auto performance_id = "vehicle:performance";
inline constexpr auto problems_id = "vehicle:problems";
inline constexpr auto new_id = "vehicle:new";
inline constexpr auto turn_cw_id = "vehicle:turn_cw";
inline constexpr auto turn_ccw_id = "vehicle:turn_ccw";
/// The affordance that shows a tab of the design or vehicles column is this and the tab's key.
inline constexpr auto advanced_id = "vehicle:advanced";
inline constexpr auto show_prefix = std::string_view("vehicle:show:");
inline constexpr auto part_filter_id = "vehicle:part_filter";
inline constexpr auto part_page_id = "vehicle:part_page";
inline constexpr auto field_set_id = "vehicle:field_set";
inline constexpr auto field_apply_id = "vehicle:field_apply";
inline constexpr auto undo_id = "vehicle:undo";
inline constexpr auto redo_id = "vehicle:redo";
inline constexpr auto save_id = "vehicle:save";
inline constexpr auto save_as_id = "vehicle:save_as";
inline constexpr auto world_id = "vehicle:world";
inline constexpr auto export_id = "vehicle:export";
inline constexpr auto spawn_form_id = "vehicle:spawn_form";
inline constexpr auto spawn_set_id = "vehicle:spawn_set";
inline constexpr auto spawn_id = "vehicle:spawn";
inline constexpr auto test_drive_id = "vehicle:test_drive";
inline constexpr auto copy_id = "vehicle:copy";
inline constexpr auto paste_id = "vehicle:paste";
inline constexpr auto edit_id = "vehicle:edit";
inline constexpr auto done_id = "vehicle:done";
inline constexpr auto live_form_id = "vehicle:live_form";
inline constexpr auto live_set_id = "vehicle:live_set";
inline constexpr auto source_id = "vehicle:source";
inline constexpr auto tool_id = "vehicle:tool";
inline constexpr auto prop_set_id = "vehicle:prop_set";
inline constexpr auto part_set_id = "vehicle:part_set";
inline constexpr auto item_add_id = "vehicle:item_add";
inline constexpr auto item_remove_id = "vehicle:item_remove";
inline constexpr auto item_chance_id = "vehicle:item_chance";
inline constexpr auto history_id = "vehicle:history";
inline constexpr auto jump_id = "vehicle:jump";
inline constexpr auto item_kinds_id = "vehicle:item_kinds";
inline constexpr auto item_catalog_id = "vehicle:item_catalog";
inline constexpr auto item_kind_id = "vehicle:item_kind";
inline constexpr auto item_filter_id = "vehicle:item_filter";
inline constexpr auto item_page_id = "vehicle:item_page";
inline constexpr auto item_pick_id = "vehicle:item_pick";
inline constexpr auto check_id = "vehicle:check";
/// The affordance that fixes a failed check of `check_id` is this and the check's key.
inline constexpr auto fix_prefix = std::string_view("vehicle:fix:");

/// The mounts the grid shows, from its north-west corner to its south-east one.
struct mount_frame {
    point low;
    point high;
    auto operator==(const mount_frame&) const -> bool = default; // *NOPAD*
};

/// The settings of a part Copy settings took, for Paste settings to give parts of its kind.
struct part_settings {
    /// The id of the first shape of its kind.
    std::string kind;
    std::string name;
    /// Its members but its `part`.
    nlohmann::ordered_json members = nlohmann::ordered_json::object();
};

/// The frame of `points` and `margin` mounts around them.
auto frame_around(const std::vector<point>& points, int margin) -> mount_frame;

/// One change of a world's vehicle edited in place: its saved state on the other side of it, and
/// what it did.
struct live_step {
    std::string state;
    std::string label;
};

/// A mount of what was copied: where it lies from the north-west corner of the copy, and its
/// parts in the order they were installed.
struct clip_mount {
    point offset;
    std::vector<std::string> parts;
};

/// What a click on a mount of the grid does, the affordance that picks it and its keys, as the
/// map editor's tools have them; a tool that draws the part in hand has the shape it draws, which
/// takes two mounts when it spans.
struct canvas_tool {
    const char* key;
    const char* affordance;
    const char* hotkey;
    const char* other_key = "";
    std::optional<shape_kind> kind = std::nullopt;
    bool spans = false;
};
inline constexpr auto canvas_tools = std::array{
    canvas_tool{.key = "select", .affordance = "vehicle:tool_select", .hotkey = "s", .spans = true},
    canvas_tool{
        .key = "install",
        .affordance = "vehicle:tool_install",
        .hotkey = "b",
        .kind = shape_kind::point},
    canvas_tool{
        .key = "line",
        .affordance = "vehicle:tool_line",
        .hotkey = "l",
        .kind = shape_kind::line,
        .spans = true},
    canvas_tool{
        .key = "rect",
        .affordance = "vehicle:tool_rect",
        .hotkey = "r",
        .kind = shape_kind::rect,
        .spans = true},
    canvas_tool{
        .key = "rect_filled",
        .affordance = "vehicle:tool_rect_filled",
        .hotkey = "u",
        .kind = shape_kind::rect_filled,
        .spans = true},
    canvas_tool{
        .key = "fill", .affordance = "vehicle:tool_fill", .hotkey = "f", .kind = shape_kind::fill},
    canvas_tool{.key = "remove", .affordance = "vehicle:tool_remove", .hotkey = "e"},
    // Q as Factorio's pipette.
    canvas_tool{.key = "pick", .affordance = "vehicle:tool_pick", .hotkey = "i", .other_key = "q"},
    canvas_tool{
        .key = "paste", .affordance = "vehicle:tool_paste", .hotkey = "v", .other_key = "CTRL+V"}};

/// The tabs of the design column, in order, with the key that shows each; a world's vehicle
/// edited in place has the first three.
struct design_tab {
    const char* key;
    const char* hotkey;
};
inline constexpr auto design_tabs = std::array{
    design_tab{.key = "mount", .hotkey = "1"},   design_tab{.key = "part", .hotkey = "2"},
    design_tab{.key = "vehicle", .hotkey = "3"}, design_tab{.key = "cargo", .hotkey = "4"},
    design_tab{.key = "spawn", .hotkey = "5"},   design_tab{.key = "json", .hotkey = "6"}};
inline constexpr auto live_tab_count = std::size_t{3};

/// What the vehicle editor holds and what the view reads of it; vehicle_session.cpp changes it.
struct session {
    std::optional<document> doc = std::nullopt;
    /// The prototype's object in `doc`.
    std::size_t object = 0;
    std::string notice;
    /// What the last action did, when it did something the editor does not show by itself: to
    /// the world, or a part installed or removed.
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
    /// The category of parts the palette shows, as the player picked it; none for the one of the
    /// part in hand.
    std::optional<part_category> part_group;
    part_catalog catalog;
    /// What the field form sets: a pointer into the prototype and a value as JSON or bare text;
    /// an empty value removes it.
    std::string field_pointer = "/name";
    std::string field_value;
    spawn_options spawning;
    /// The tab of the left column: `prototypes` or `world`.
    std::string source_tab = "prototypes";
    /// The tab of the right column: `mount`, `part`, `vehicle`, `cargo`, `spawn` or `json`.
    std::string tool_tab = "mount";
    /// What a click on the grid does: the key of one of `canvas_tools`.
    std::string canvas_tool = "select";
    /// Where a tool that spans two mounts was first used, until its second.
    std::optional<point> anchor;
    /// The mounts the Select tool marked, which Copy, Cut and Delete act on.
    std::optional<mount_frame> selection;
    /// What Copy or Cut took, which the Paste tool installs where it clicks.
    std::vector<clip_mount> clip;
    /// What Copy settings took.
    std::optional<part_settings> copied;
    /// Whether a drawing tool also draws side to side across the vehicle's middle, or end to
    /// end, as the map editor's mirrors.
    bool mirror_sides = false;
    bool mirror_ends = false;
    /// How many quarters clockwise the canvas is turned.
    int view_quarters = 0;
    /// Whether a stroke on the vehicle edited in place is open, and whether it has made its step.
    bool live_stroke = false;
    bool live_stroke_made = false;
    /// The vehicle edited in place as the open stroke found it, to name the step it made.
    nlohmann::ordered_json live_stroke_before;
    /// Twice the middle the mirrors reflect across while a stroke is open, as it was when the
    /// stroke began, so that what the stroke changes does not move it.
    std::optional<point> stroke_axis;
    /// The squares the open stroke's eraser took a part off, which it takes no more off.
    std::vector<point> stroke_erased;
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
    auto prototype() const -> const nlohmann::ordered_json&; // *NOPAD*
    /// The category the palette shows: the one picked, else the part in hand's, else frames.
    auto shown_group() const -> part_category;
    /// The JSON at `pointer` in the prototype; empty when there is none.
    auto value_at(const std::string& pointer) const -> std::string;
    /// The file a new design suggests: `base` in the mod folder of the open file, else in
    /// my_mod, with a number when that file is there or the open design, unsaved, takes it, or,
    /// for a `new_id`, the game has a prototype of that id.
    auto suggested(const std::string& base, bool new_id = false) const -> std::string;
    /// The parts on the picked mount in the order the game installs them: indexes of the vehicle
    /// edited in place, else of `design_parts`.
    auto parts_here() const -> std::vector<std::size_t>;
    /// The picked part when it is on the picked mount, as `parts_here` names it.
    auto picked_here() const -> std::optional<std::size_t>;
    /// The name of the picked part on the picked mount; none when the mount has none.
    auto picked_name() const -> std::optional<std::string>;
    /// What tells the kind of the part `id` from others: the id of its first shape.
    auto kind_name(const vpart_id& id) const -> std::string;
    /// The mounts Copy, Cut and Delete act on: those marked, else the picked one, north to south
    /// and west to east.
    auto marked_mounts() const -> std::vector<point>;
    /// The canvas tool in hand.
    auto tool() const -> const struct canvas_tool&; // *NOPAD*
    /// The vehicle the canvas shows.
    auto shown_vehicle() const -> const vehicle*;
    /// The mounts the canvas shows, from its north-west corner to its south-east one.
    auto canvas_bounds() const -> mount_frame;
    /// How many tabs the design column has: a world's vehicle edited in place has fewer.
    auto shown_tab_count() const -> std::size_t;
    /// The index in `design_parts` of the part the field form points at, the one on the picked
    /// mount when a palette symbol draws it on several; none when it points at no part.
    auto picked_part() const -> std::optional<std::size_t>;
    /// The indexes in the prototype's `items` of the item spawns on the picked mount.
    auto item_spawns() const -> std::vector<std::size_t>;
    /// The steps of the vehicle edited in place, or of the open document.
    auto history() const -> history_steps;
    /// Whether the vehicle shown can be driven away, check by check.
    auto drive() const -> std::vector<drive_check>;
};

/// The screen the session shows.
auto session_view(const session& state) -> engine_client::screen_view;

} // namespace editor::vehicle_editor
