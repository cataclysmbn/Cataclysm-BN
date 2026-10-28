#pragma once

#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/mapgen_legend.h"
#include "editor/mapgen_levels.h"
#include "editor/mapgen_placement.h"
#include "editor/mapgen_preview.h"
#include "editor/mapgen_source.h"
#include "editor/mapgen_stamp.h"
#include "editor/mapgen_stroke.h"
#include "engine_client_component.h"

#include <array>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

/// The map editor screen's state and view, which editor_session.cpp runs.
namespace editor::map_editor {


inline constexpr auto file_id = "editor:file";
inline constexpr auto path_id = "path";
inline constexpr auto canvas_id = "editor:canvas";
inline constexpr auto tools_id = "editor:tools";
inline constexpr auto legend_id = "editor:legend";
inline constexpr auto objects_id = "editor:objects";
inline constexpr auto diagnostics_id = "editor:diagnostics";

inline constexpr auto open_id = "editor:open";
inline constexpr auto save_as_id = "editor:save_as";
inline constexpr auto new_id = "editor:new";
inline constexpr auto new_set_id = "editor:new_set";
inline constexpr auto new_apply_id = "editor:new_apply";
inline constexpr auto object_id = "editor:object";
inline constexpr auto tool_id = "editor:tool";
inline constexpr auto symbol_id = "editor:symbol";
inline constexpr auto paint_id = "editor:paint";
inline constexpr auto stroke_begin_id = "editor:stroke_begin";
inline constexpr auto stroke_end_id = "editor:stroke_end";
inline constexpr auto extract_palette_id = "editor:extract_palette";
inline constexpr auto undo_id = "editor:undo";
inline constexpr auto redo_id = "editor:redo";
inline constexpr auto save_id = "editor:save";
inline constexpr auto preview_id = "editor:preview";
inline constexpr auto pins_id = "editor:pins";
inline constexpr auto pin_set_id = "editor:pin_set";
inline constexpr auto rotation_id = "editor:rotation";
inline constexpr auto turn_id = "editor:turn";
inline constexpr auto reroll_id = "editor:reroll";
inline constexpr auto loot_id = "editor:loot";
inline constexpr auto mapping_id = "editor:mapping";
inline constexpr auto catalog_kinds_id = "editor:catalog_kinds";
inline constexpr auto catalog_id = "editor:catalog";
inline constexpr auto catalog_kind_id = "editor:catalog_kind";
inline constexpr auto catalog_filter_id = "editor:catalog_filter";
inline constexpr auto catalog_page_id = "editor:catalog_page";
inline constexpr auto catalog_pick_id = "editor:catalog_pick";
inline constexpr auto placements_id = "editor:placements";
inline constexpr auto placement_id = "editor:placement";
inline constexpr auto placement_span_id = "editor:placement_span";
inline constexpr auto diagnostic_id = "editor:diagnostic";
inline constexpr auto mapping_set_id = "editor:mapping_set";
inline constexpr auto mapping_apply_id = "editor:mapping_apply";
inline constexpr auto move_id = "editor:move";
inline constexpr auto move_set_id = "editor:move_set";
inline constexpr auto move_apply_id = "editor:move_apply";
inline constexpr auto placement_delete_id = "editor:placement_delete";
inline constexpr auto place_id = "editor:place";
inline constexpr auto place_set_id = "editor:place_set";
inline constexpr auto place_apply_id = "editor:place_apply";
inline constexpr auto field_id = "editor:field";
inline constexpr auto field_set_id = "editor:field_set";
inline constexpr auto field_apply_id = "editor:field_apply";
inline constexpr auto inside_id = "editor:inside";
inline constexpr auto brush_id = "editor:brush";
inline constexpr auto mirror_x_id = "editor:mirror_x";
inline constexpr auto mirror_y_id = "editor:mirror_y";
inline constexpr auto export_id = "editor:export";
inline constexpr auto export_area_id = "editor:export_area";
inline constexpr auto export_width_id = "editor:export_width";
inline constexpr auto export_height_id = "editor:export_height";
inline constexpr auto export_palette_id = "editor:export_palette";
inline constexpr auto export_options_id = "editor:export_options";
inline constexpr auto import_id = "editor:import";
inline constexpr auto import_set_id = "editor:import_set";
inline constexpr auto import_form_id = "editor:import_form";
inline constexpr auto advance_id = "editor:advance";
inline constexpr auto copy_id = "editor:copy";
inline constexpr auto rotate_id = "editor:rotate";
inline constexpr auto cut_id = "editor:cut";
inline constexpr auto flip_x_id = "editor:flip_x";
inline constexpr auto flip_y_id = "editor:flip_y";
inline constexpr auto history_id = "editor:history";
inline constexpr auto jump_id = "editor:jump";
inline constexpr auto open_oter_id = "editor:open_oter";
inline constexpr auto open_here_id = "editor:open_here";
inline constexpr auto level_up_id = "editor:level_up";
inline constexpr auto level_down_id = "editor:level_down";
inline constexpr auto open_square_id = "editor:open_square";
inline constexpr auto terrains_id = "editor:terrains";
inline constexpr auto open_terrain_id = "editor:open_terrain";
inline constexpr auto connection_id = "editor:connection";
inline constexpr auto connection_set_id = "editor:connection_set";
inline constexpr auto browser_id = "editor:browser";
inline constexpr auto browser_open_id = "editor:browser_open";
inline constexpr auto browser_filter_id = "editor:browser_filter";
inline constexpr auto browser_page_id = "editor:browser_page";
inline constexpr auto recent_id = "editor:recent";
inline constexpr auto recent_open_id = "editor:recent_open";
inline constexpr auto apply_id = "editor:apply";
inline constexpr auto apply_on_save_id = "editor:apply_on_save";
inline constexpr auto stamp_id = "editor:stamp";
inline constexpr auto unstamp_id = "editor:unstamp";
inline constexpr auto test_id = "editor:test";
inline constexpr auto place_special_id = "editor:place_special";
inline constexpr auto max_brush = 9;
/// The most overmap squares an export spans each way: what the loaded map holds around the avatar.
inline constexpr auto max_export = 3;
/// The most turns the editor lets pass at once: an hour.
inline constexpr auto max_advance = 3600;
inline constexpr auto vehicle_id = "editor:vehicle";

/// A turn of the preview in degrees clockwise, and the direction the game generates it for.
struct preview_turn {
    int degrees;
    const char* direction;
};
inline constexpr auto preview_turns = std::array{
    preview_turn{0, "north"}, preview_turn{90, "east"}, preview_turn{180, "south"},
    preview_turn{270, "west"}};

/// How many seeds a loot roll counts over.
inline constexpr auto loot_runs = 50;

/// The members of a mapgen's object that map its symbols, as the loader reads them.
inline constexpr auto legend_layers = std::array{
    "terrain",
    "furniture",
    "fields",
    "npcs",
    "signs",
    "vendingmachines",
    "toilets",
    "gaspumps",
    "items",
    "monsters",
    "vehicles",
    "item",
    "artifact",
    "artifacts",
    "traps",
    "monster",
    "rubble",
    "computers",
    "sealed_item",
    "nested",
    "liquids",
    "graffiti",
    "translate",
    "zones",
    "ter_furn_transforms",
    "faction_owner_character",
    "remove_all"};

/// What a tool that works on a region instead of a shape does with the square it is used on.
enum class region_use { none, select, paste, placements };

struct tool {
    std::string id;
    /// None: the eyedropper, which picks the symbol it is used on, unless it works on a region.
    std::optional<shape_kind> kind;
    /// Whether it takes two squares, the first of them its anchor.
    bool spans = false;
    region_use region = region_use::none;
    /// The key that picks it, as image editors have it.
    const char* hotkey = "";
};

inline const auto tools = std::array{
    tool{.id = "tool:point", .kind = shape_kind::point, .hotkey = "b"},
    tool{.id = "tool:line", .kind = shape_kind::line, .spans = true, .hotkey = "l"},
    tool{.id = "tool:rect", .kind = shape_kind::rect, .spans = true, .hotkey = "r"},
    tool{.id = "tool:rect_filled", .kind = shape_kind::rect_filled, .spans = true, .hotkey = "u"},
    tool{.id = "tool:ellipse", .kind = shape_kind::ellipse, .spans = true, .hotkey = "o"},
    tool{.id = "tool:room", .kind = shape_kind::room, .spans = true, .hotkey = "m"},
    tool{.id = "tool:fill", .kind = shape_kind::fill, .hotkey = "f"},
    tool{.id = "tool:pick", .kind = std::nullopt, .hotkey = "i"},
    tool{.id = "tool:select", .region = region_use::select, .hotkey = "s"},
    tool{.id = "tool:paste", .region = region_use::paste, .hotkey = "v"},
    tool{.id = "tool:placements", .region = region_use::placements, .hotkey = "p"},
};

/// The tools of an overmap special's canvas: open a square's mapgen, draw squares with the terrain
/// picked, or erase them.
inline const auto special_tools = std::array{
    tool{.id = "tool:open", .hotkey = "o"},
    tool{.id = "tool:point", .kind = shape_kind::point, .hotkey = "b"},
    tool{.id = "tool:erase", .kind = shape_kind::point, .hotkey = "e"},
    tool{.id = "tool:connect", .kind = shape_kind::point, .hotkey = "c"},
};

/// The corners of the rectangle the select tool marked.
struct selection {
    point from;
    point to;
};

/// What of the catalog a legend maps, and how a mapping names it.
struct catalog_use {
    catalog_kind kind;
    const char* id;
    /// The legend layer that places it.
    const char* layer;
    /// The member that names it in a mapping; none: the mapping is the bare id.
    const char* key = nullptr;
    /// Whether that member is a list.
    bool listed = false;
};

inline const auto catalog_uses = std::array{
    catalog_use{.kind = catalog_kind::terrain, .id = "terrain", .layer = "terrain"},
    catalog_use{.kind = catalog_kind::furniture, .id = "furniture", .layer = "furniture"},
    catalog_use{.kind = catalog_kind::trap, .id = "trap", .layer = "traps"},
    catalog_use{.kind = catalog_kind::field, .id = "field", .layer = "fields", .key = "field"},
    catalog_use{.kind = catalog_kind::item, .id = "item", .layer = "item", .key = "item"},
    catalog_use{
        .kind = catalog_kind::item_group, .id = "item_group", .layer = "items", .key = "item"},
    catalog_use{
        .kind = catalog_kind::monster, .id = "monster", .layer = "monster", .key = "monster"},
    catalog_use{
        .kind = catalog_kind::monster_group,
        .id = "monster_group",
        .layer = "monsters",
        .key = "monster"},
    catalog_use{
        .kind = catalog_kind::vehicle, .id = "vehicle", .layer = "vehicles", .key = "vehicle"},
    catalog_use{
        .kind = catalog_kind::npc_template, .id = "npc_template", .layer = "npcs", .key = "class"},
    catalog_use{
        .kind = catalog_kind::nested_mapgen,
        .id = "nested_mapgen",
        .layer = "nested",
        .key = "chunks",
        .listed = true},
};


/// A special the editor placed in the world.
struct placed_special {
    std::string id;
    tripoint_abs_omt at;
};

/// What the new mapgen form writes: a file of the user mods holding one mapgen of an overmap
/// terrain, one nested chunk of that id, its squares all one terrain, one palette of that id
/// mapping a symbol to that terrain, or one overmap special or building of that id placing an
/// overmap terrain on one square.
struct new_mapgen {
    std::string path;
    /// The id of the overmap terrain, of the chunk or of the palette.
    std::string om_terrain;
    std::string fill = "t_floor";
    /// `mapgen`, `nested`, `palette`, `special` or `building`.
    std::string kind = "mapgen";
    /// The side of a nested chunk, in squares.
    std::string size = "4";
};

/// Squares of the live world: the north-west one and how many east and south.
struct export_squares {
    tripoint_abs_ms corner;
    point_rel_ms size;
};

struct diagnostic {
    std::string text;
    std::string pointer;
};

auto catalog_label(const catalog_use& use) -> std::string;
/// The mapping of `use`'s layer that places `id`, as the legend form writes it.
auto mapping_of(const catalog_use& use, const std::string& id) -> std::string;
auto tool_label(const tool& value) -> std::string;
/// What names an entry: a mapgen's overmap terrain, or the id it is nested or updated by; a
/// palette's id.
auto entry_name(const nlohmann::ordered_json& value) -> std::string;
auto range_text(const placement_range& range) -> std::string;
/// The row that names `entry` in the placements list and the canvas.
auto placement_row(const placement& entry) -> std::string;
/// Whether a new file can be written at `made`; the error says why not.
auto check_new_path(const std::string& made) -> std::expected<void, std::string>;
/// Writes `entries` as the new file `made`; the error says why it was not written.
auto write_new_file(const std::string& made, const nlohmann::ordered_json& entries)
    -> std::expected<void, std::string>;
/// Writes `made`'s file; the error says why it was not written.
auto create_mapgen(const new_mapgen& made) -> std::expected<void, std::string>;

struct session {
    std::optional<document> doc = std::nullopt;
    /// What the last command did not do, and why; empty when it did.
    std::string notice;
    /// What went right, said until the next affordance.
    std::string status;
    /// The mapgen text `editor:import` stamps.
    std::string import_text;
    /// The object of `doc` on the canvas.
    std::size_t object = 0;
    /// The level the canvas entry stands on in the building or overmap special that stacks it, and
    /// the file's entries of the levels above and below.
    std::optional<int> level = std::nullopt;
    std::optional<std::size_t> level_up = std::nullopt;
    std::optional<std::size_t> level_down = std::nullopt;
    /// The level of an overmap special or building the canvas shows.
    int special_level = 0;
    const tool* special_tool = special_tools.data();
    /// The overmap terrain a special's squares are drawn with.
    std::string special_terrain;
    /// The overmap connection the connect tool joins a special to.
    std::string special_connection = "local_road";
    /// The overmap squares of the canvas entry over a level the file makes, the squares beside it
    /// the file makes, and the legends of the entries that make them.
    std::vector<stacked_square> below;
    std::vector<neighbour_square> beside;
    std::map<std::size_t, std::vector<legend_symbol>> other_legends;
    std::string symbol;
    /// The symbol inside a room.
    std::string room_fill;
    /// The side of the square of squares a pencil, line or outline draws with.
    int brush = 1;
    bool mirror_x = false;
    bool mirror_y = false;
    /// Whether saving the file, or reading it again after it changed on disk, applies the entry.
    bool apply_on_save = false;
    const tool* current = tools.data();
    std::optional<point> anchor = std::nullopt;
    /// The region the select tool marked, which `editor:copy` copies.
    std::optional<selection> selected = std::nullopt;
    /// The symbols copied last, which the paste tool pastes and `editor:rotate` turns.
    symbol_grid clip;
    /// The placements wholly inside what was copied, relative to its top-left square.
    std::vector<placement> clip_placed;
    /// What the copied symbols place, as the entry they were copied from maps them.
    std::map<std::string, std::vector<legend_line>> clip_legend;
    /// The square a placement or a diagnostic that was picked names.
    std::optional<point> focus = std::nullopt;
    /// The placement picked last, which the move form moves.
    std::optional<placement> picked = std::nullopt;
    std::string move_x;
    std::string move_y;
    /// What the add form places: a `place_*` kind, its squares, and its other members as JSON.
    std::string place_kind = "place_items";
    std::string place_x = "0";
    std::string place_y = "0";
    std::string place_value;
    /// What the field form sets: a pointer into the entry and a value as JSON or bare text; an
    /// empty value removes it.
    std::string field_pointer;
    std::string field_value;
    std::vector<legend_symbol> legend;
    std::vector<diagnostic> diagnostics;
    /// The seed of the preview while one is shown; it follows every edit.
    std::optional<unsigned int> seed = std::nullopt;
    std::optional<mapgen_preview> preview = std::nullopt;
    /// The direction the preview is generated for, and the parameters it fixes.
    std::string direction = "north";
    std::map<std::string, std::string> pins;
    /// The parameters the entry has, with the values they take, while a preview is shown.
    std::vector<mapgen_parameter_choice> parameters;
    /// What can spawn on the square last asked about, as of the last edit.
    std::optional<std::vector<loot_share>> loot = std::nullopt;
    /// The mapping the legend form sets: one symbol, one layer, and what it places there as JSON
    /// or as a bare id; empty removes it.
    std::string mapping_symbol;
    std::string mapping_layer = "terrain";
    std::string mapping_value;
    new_mapgen made;
    /// How many overmap squares an export spans east and south, centred on the avatar's.
    point_rel_omt export_size = point_rel_omt(1, 1);
    /// The squares of the live world the player marked to export instead, until a size is set.
    std::optional<export_squares> export_area = std::nullopt;
    /// The palette whose symbols an export reuses; none when empty.
    std::string export_palette;
    /// The entry as it stands in the live world, while it is there.
    std::optional<stamp> stamped = std::nullopt;
    /// The built-in and Lua mapgens last looked up or found in the file opened, read-only.
    std::vector<code_mapgen> code_mapgens;
    /// The special last placed in the world, and its origin there.
    std::optional<placed_special> placed = std::nullopt;
    /// The files the browser lists; read again after a file is made.
    mutable std::optional<std::vector<std::filesystem::path>> files = std::nullopt;
    std::string browser_filter;
    std::size_t browser_page = 0;
    const catalog_use* catalog = catalog_uses.data();
    std::string catalog_filter;
    std::size_t catalog_page = 0;
    /// Whether the catalog's filter and page are those of a special's.
    bool catalog_of_special = false;

    /// The files whose path holds the filter, ignoring case.
    auto browser_entries() const -> std::vector<std::filesystem::path>;

    /// Opens the mapgen the game registered for an overmap terrain, on the entry that holds it.
    auto open_mapgen_of(const std::string& name) -> void;

    auto catalog_entries() const -> editor::catalog_page;

    /// The objects the editor edits: JSON mapgens, palettes, overmap specials and buildings.
    auto entries() const -> std::vector<std::size_t>;
    /// The palettes and nested chunks of the file, which stand in for the game's while a mapgen of
    /// it is read.
    auto stand_in_texts() const -> std::vector<std::string>;
    /// Whether the object edited is a mapgen, which has rows to draw on and a preview.
    auto on_mapgen() const -> bool;
    /// Why the mapgen edited cannot be stamped, previewed or tested as an overmap square: a nested
    /// chunk or an update mapgen; none for any other entry.
    auto square_refusal() const -> std::optional<std::string>;
    /// Whether the mapgen edited is an update mapgen, which the game runs on squares it made.
    auto on_update() const -> bool;
    /// Whether the object edited is an overmap special or a building, which places overmap squares.
    auto on_special() const -> bool;
    /// Whether the object edited is an overmap terrain, which the game draws its squares with.
    auto on_oter() const -> bool;
    /// Where the special edited was placed in the world; none when it was not, or another was.
    auto placed_here() const -> std::optional<tripoint_abs_omt>;
    /// The mapgen ids the overmap terrain edited is made with, of the types the game knows.
    auto terrain_mapgens() const -> std::vector<std::string>;
    /// Whether the canvas can show the level `dz` above: another entry's, or the special's own.
    auto can_climb(int dz) const -> bool;
    /// The special's nearest level above, or below, the one shown that it places squares on.
    auto level_beside(int dz) const -> std::optional<int>;
    auto climb(int dz) -> void;
    /// Opens the mapgen of the overmap terrain the special places at `at` on the level shown.
    auto open_square(const point& at) -> void;

    /// The options the entry is previewed with over the avatar's overmap square.
    auto preview_options_of(unsigned int with_seed) const -> preview_options;

    /// Fixes a parameter to a value, or frees it when the value is empty; a pin the preview
    /// refuses is taken back and its reason shown.
    auto pin(const std::string& name, const std::string& value) -> void;

    /// Counts what the entry puts on the squares over several seeds, from the seed shown on.
    auto roll_loot_in(const selection& squares) -> void;

    /// The levels around the canvas entry that the file makes.
    auto reload_levels() -> void;

    /// Puts the file's entry `index` on the canvas.
    auto show_object(std::size_t index) -> void;
    /// Undoes the last step, or redoes the next one when `!back`, shows the entry it changed and
    /// says what it did.
    auto step_history(bool back) -> void;

    /// What the loader makes of the object now: its legend and what it reports.
    auto reload() -> void;

    /// A file changed on disk since it was opened: reloaded while nothing is edited, else reported,
    /// since saving would overwrite that change.
    auto watch() -> void;
    /// Reads the open file again, on the object it showed while the file still has it; whether
    /// it does.
    auto reopen() -> bool;
    /// Writes the file; false, with the notice saying why, when it was not written.
    auto save() -> bool;
    auto save_as(const std::string& name) -> void;
    auto written(const std::expected<void, std::string>& result) -> bool;
    /// Registers the entry as the game's: a mapgen as that of its overmap terrains, and the file's
    /// nested chunks, which a nested chunk is applied as, as the game's chunks of their ids.
    auto apply() -> void;
    auto told(const std::expected<void, std::string>& result) -> bool;
    /// Whether to leave: at once with nothing unsaved, else as the player answers save, discard
    /// (which reads the file again) or stay.
    auto leave() -> bool;
    auto open(const std::string& name) -> void;

    /// Moves the mapgen's own legend into a new palette `name` at the end of the file, which the
    /// mapgen then names last so that its symbols place what they did, as one undo step.
    auto extract_palette(const std::string& name) -> void;

    auto set_mapping(const std::string& field, const std::string& text) -> void;

    /// Why the rows cannot use `glyph`, counting its own terrain mapping only when `own`: the
    /// loader wants terrain under every symbol of a whole map unless fill_ter or predecessor_mapgen
    /// gives it.
    auto bare(const std::string& glyph, bool own) const -> std::optional<std::string>;

    auto apply_mapping() -> void;

    /// The square of the rows nearest to where the value at `pointer` of the object stands: its
    /// row, or its placement's first square; none for any other value.
    auto square_named(const std::string& pointer) const -> std::optional<point>;

    auto pick(const placement& entry) -> void;

    auto move_picked() -> void;
    /// Moves the corners of the placement `row` names by `text`, `dx0,dy0,dx1,dy1`, as one step,
    /// and picks it.
    auto span_placement(const std::string& row, const std::string& text) -> void;

    auto clip_size_now() const -> clip_size;

    /// Copies the selection's symbols and the placements wholly inside it.
    auto copy() -> void;

    /// Copies the selection, then leaves the inside symbol on it and removes the placements
    /// copied, as one undo step.
    auto cut() -> void;

    /// Pastes the clip's symbols and placements with its top-left square at `at`, as one undo step.
    auto paste_at(const point& at) -> void;

    /// Adds what the add form names and picks it.
    auto add_placed() -> void;

    auto delete_picked() -> void;

    auto set_picked_field() -> void;

    /// Generates the entry over the avatar's overmap square, in place of what the world has there.
    auto stamp_here() -> void;

    /// Places the special in the world on new land near the avatar, as it stands in the document.
    auto place_special() -> void;

    /// Stamps the pasted text: a mapgen entry, or the first mapgen of a file's array of entries.
    auto import_pasted() -> void;

    /// Stamps the mapgen entry in `text` over the avatar's overmap square, in place of any stamp.
    auto stamp_text(const std::string& text) -> void;

    /// Puts back what the stamp replaced.
    auto unstamp() -> void;

    /// Why the special's squares cannot be drawn on; none when they can.
    auto drawing_refusal() const -> std::optional<std::string>;
    /// Draws, erases or connects the special's square `at` on the level shown, as the tool picked
    /// does.
    auto paint_square(const point& at) -> void;

    auto paint(const point& at) -> void;
};

/// The screen the session shows.
auto session_view(const session& state) -> engine_client::screen_view;

} // namespace editor::map_editor
