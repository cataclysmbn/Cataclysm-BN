#pragma once

#include "client_interaction_data.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class input_context;

namespace catacurses
{
class window;
} // namespace catacurses

namespace game_client
{

enum class interaction_kind : int {
    custom,
    choices,
    field,
    inventory,
    target,
};

/// One value of a row. `key` is the stable, untranslated name clients look the column up by; `label` is
/// the translated caption; `color` is the game color the native screen prints the value in, if any.
struct interaction_column {
    std::string key;
    std::string label;
    std::string value;
    std::string color;
};

/// The selection glyph a native multi-select list prints before a row's name, and its game color.
struct interaction_mark {
    std::string glyph;
    std::string color;
};

struct interaction_editor_value {
    std::string id;
    std::string label;
};

/// A value the client may set on a choice directly with `fill`, instead of cycling it natively.
/// `type` is `bool`, `select`, `integer`, `float` or `text`.
struct interaction_editor {
    std::string type;
    std::string value;
    std::vector<interaction_editor_value> values;
    std::optional<double> minimum;
    std::optional<double> maximum;
    int max_length = -1;
};

struct interaction_choice {
    std::string id;
    std::string label;
    std::string description;
    std::string denial;
    /// The key that chooses the row natively, as the native list prints it before the label; empty when none.
    std::string hotkey;
    std::optional<std::string> pane_id;
    std::optional<std::string> area_id;
    std::string storage_kind;
    bool enabled = true;
    bool selectable = true;
    bool selected = false;
    bool highlighted = false;
    std::vector<interaction_column> columns;
    std::optional<interaction_editor> editor;
    std::optional<std::uint64_t> selected_count;
    std::optional<std::uint64_t> minimum_count;
    std::optional<std::uint64_t> available_count;
    /// The row is a favorite, as the native list marks it.
    bool favorite = false;
    /// The game color of the row's text, and of its key; empty when the native list uses its default.
    std::string color;
    std::string key_color;
    /// The row's name with the native color tags, when it differs from `label`.
    std::string markup;
    /// The registered action the row runs, such as `open_options` on the in-game main menu; empty for other rows.
    std::string action_id;
    /// The row this one is nested under, when that row is listed.
    std::optional<std::string> parent_id;
    /// The game color of the nesting slash the native list draws before a nested row; empty for a plain row.
    std::string nest_color;
    /// The selection mark the native list prints; absent when it prints none.
    std::optional<interaction_mark> mark;
};

/// What a pane holds against what it can hold, as the native screen prints it beside the area name:
/// weight and volume in the pane's own units, with a maximum where the native screen shows one.
struct interaction_totals {
    std::string weight;
    std::string weight_max;
    std::string weight_unit;
    std::string volume;
    std::string volume_max;
    std::string volume_unit;
    bool over_weight = false;
    bool over_volume = false;
};

/// The page a paged native list shows, counted from 1.
struct interaction_page_position {
    int number = 1;
    int count = 1;
};

struct interaction_pane {
    std::string id;
    std::string label;
    std::string role;
    std::string area_id;
    std::string area_label;
    std::string area_description;
    std::string filter;
    std::string storage_kind;
    /// Registered actions of the areas this pane cannot put items in, which the native screen draws red.
    std::vector<std::string> blocked_areas;
    /// The native page indicator; absent for a list that does not page.
    std::optional<interaction_page_position> page;
};

struct interaction_field {
    std::string id;
    std::string label;
    std::string description;
    std::string value;
    std::string type = "text";
    int max_length = -1;
    bool printable = false;
};

struct interaction_target_candidate {
    std::string id;
    std::string label;
    std::string description;
    interaction_position position;
    bool creature = false;
};

struct interaction_target {
    std::string coordinate_space = "bubble_ms";
    interaction_position source;
    interaction_position cursor;
    std::optional<interaction_position> minimum_position;
    std::optional<interaction_position> maximum_position;
    int range = 0;
    std::string distance_metric;
    std::string status;
    bool limit_to_reality_bubble = false;
    std::vector<interaction_target_candidate> candidates;
};

struct interaction_overmap_note {
    interaction_position position;
    std::string text;
};

/// The tile the native tiles view draws for an overmap square.
struct interaction_overmap_tile {
    std::string id;
    int rotation = 0;
    /// The multitile key (`edge`, `corner`, ...), empty for a plain tile.
    std::string subtile;
};

/// The native overmap window as drawn: one glyph and curses colors per cell, row-major, with
/// the sidebar text. Positions are absolute overmap terrain squares.
struct interaction_overmap {
    /// The square drawn in the top-left cell.
    interaction_position origin;
    int cols = 0;
    int rows = 0;
    std::vector<std::string> glyphs;
    std::vector<int> foreground;
    std::vector<int> background;
    interaction_position player;
    std::vector<std::string> legend;
    /// What the native sidebar says of the cursor square, without the window frame or the square's glyph.
    std::string cursor_description;
    std::vector<interaction_overmap_note> notes;
    /// The planned travel route, nearest square first.
    std::vector<interaction_position> path;
    /// Row-major like `glyphs`; empty unless the client renders tiles.
    std::vector<interaction_overmap_tile> tiles;
    /// For each tile id used, the ids to try next, nearest first.
    std::map<std::string, std::vector<std::string>> looks_like;
};

/// A heading of the requirement list, in the order the native pane prints them.
struct interaction_requirement_group {
    /// `skill`, `component`, `tool` or `extra`.
    std::string kind;
    std::string heading;
    /// What the pane prints under the heading when no row has this kind, such as `NONE`; empty when the group is then left out.
    std::string empty_text;
};

/// One requirement of the vehicle task being previewed. `kind` is `skill`, `component`, `tool` or `extra`;
/// a component or tool row lists its alternatives joined with `OR`.
struct interaction_requirement {
    std::string kind;
    std::string text;
    bool met = true;
};

/// What the vehicle screen's detail pane shows for the part under its list cursor.
struct interaction_vehicle_detail {
    std::string name;
    std::string description;
    /// How long the task takes, as the native pane prints it; empty when the pane has no time line.
    std::string time;
    /// The heading of the time line.
    std::string time_heading;
    std::vector<interaction_requirement_group> groups;
    std::vector<interaction_column> stats;
    std::vector<interaction_requirement> requirements;
    /// Whether the task can be started now: every requirement is met.
    bool can_do = true;
};

/// A square of the vehicle grid, relative to the cursor in screen cells (x right, y down).
struct interaction_vehicle_cell {
    int dx = 0;
    int dy = 0;
    std::string glyph;
    std::string color;
};

/// One part mounted on the cursor square, topmost first, as the native part list prints it.
struct interaction_vehicle_part {
    /// The name with its contents or cargo load, such as `battery (200/400 charge)`.
    std::string name;
    /// The marks the native list puts around the name: `[` `]` for structure, `(` `)` for armor, `-` `-` otherwise.
    std::string open;
    std::string close;
    /// Remaining health, 0 to 100.
    int health = 100;
    bool broken = false;
};

/// The tabs of the install list.
struct interaction_vehicle_tabs {
    std::vector<std::string> labels;
    int selected = 0;
    std::string filter;
};

/// The native vehicle interaction window (`veh_interact`): the grid around the cursor part, the parts on
/// the cursor square, the vehicle stats, and in a task list the preview of the highlighted part.
struct interaction_vehicle {
    std::string name;
    std::string owner;
    bool owned = true;
    /// `main`, `install`, `repair`, `remove`, `mend`, `refill`, or `select` for another pick-a-part list.
    std::string mode;
    /// What the task does to the highlighted part, such as `Install`; empty in the main screen.
    std::string verb;
    /// The cursor in the vehicle's own frame.
    interaction_position cursor;
    /// The cursor square cannot take a part (terrain, furniture or a creature is in the way).
    bool blocked = false;
    /// Occupied squares, including the cursor square when a part is on it.
    std::vector<interaction_vehicle_cell> cells;
    std::vector<interaction_vehicle_part> parts;
    /// The native stat lines, in reading order; an empty line is a blank slot.
    std::vector<std::string> stats;
    std::optional<interaction_vehicle_tabs> tabs;
    std::optional<interaction_vehicle_detail> detail;
};

/// A run of cells of one window row that share their colors, as curses color indexes 0-15.
struct interaction_canvas_run {
    std::string text;
    int foreground = 7;
    int background = 0;
};

/// A native window as drawn: one list of color runs per row, without the blank cells that end a row.
struct interaction_canvas {
    int cols = 0;
    std::vector<std::vector<interaction_canvas_run>> rows;
};

/// Reads the cells the windows currently hold, as the first one frames them and each later one is
/// drawn over the earlier ones; empty when the first holds none.
auto window_canvas( const std::vector<catacurses::window> &layers ) -> interaction_canvas;

/// A square of the advanced inventory's area picker, placed as the native header draws it.
struct interaction_area {
    /// The registered action that points the active pane at this area.
    std::string action_id;
    std::string label;
    /// The key bound to the action; empty when none is.
    std::string key;
    /// Character cells from the picker's top-left corner.
    int x = 0;
    int y = 0;
    /// The area exists now: the vehicle is grabbed or on the square, the level is there.
    bool available = true;
};

/// Pure data describing the interaction currently waiting at an input boundary.
struct interaction_snapshot {
    std::uint64_t input_id = 0;
    std::string schema_id;
    std::string context;
    interaction_kind kind = interaction_kind::custom;
    std::string title;
    std::string message;
    /// `good` or `bad` where the native screen colors the message line green or red; empty otherwise.
    std::string message_tone;
    /// The state of the native category-selection mode; absent where the screen has none.
    std::optional<bool> category_mode;
    /// Lines drawn above the menu in a fixed-width font, with color tags: the main menu's title art and version.
    std::vector<std::string> banner;
    bool structured = false;
    bool actions_only = true;
    bool allow_cancel = false;
    std::vector<interaction_pane> panes;
    std::vector<interaction_area> areas;
    std::vector<interaction_choice> choices;
    std::optional<interaction_field> field;
    std::optional<interaction_target> target;
    std::optional<interaction_overmap> overmap;
    std::optional<interaction_vehicle> vehicle;
    /// The window of a screen the engine draws itself, such as a software mini-game.
    std::optional<interaction_canvas> canvas;
    /// Nonzero where the engine redraws the screen on its own every this many milliseconds, with no input.
    int refresh_ms = 0;
    bool allow_set_count = false;
    std::size_t choice_offset = 0;
    std::size_t choice_total = 0;
    /// The native list cursor, reported before paging so it survives an off-page row.
    std::string focus_choice_id;
    std::optional<std::string> focus_pane_id;
};

struct interaction_page {
    std::size_t offset = 0;
    std::size_t limit = 100;
};

/// Retains one client's bounded choice page across interaction state updates.
struct interaction_page_state {
    std::size_t offset = 0;
    std::size_t limit = 100;
    std::string schema_id;
};

using interaction_provider = std::function < auto()->interaction_snapshot >;

/// Binds a value-only provider to one concrete input context for this synchronous widget read.
class interaction_scope
{
    public:
        interaction_scope( const input_context &context, interaction_provider provider );
        ~interaction_scope();
        interaction_scope( const interaction_scope & ) = delete;
        auto operator=( const interaction_scope& ) -> interaction_scope& = delete; // *NOPAD*

    private:
        std::size_t token_ = 0;
};

/// Whether the connected client renders tiles, so the overmap carries tile ids (set by `bn.hello`).
auto overmap_tiles() -> bool;
auto set_overmap_tiles( bool tiles ) -> void;

auto current_interaction( interaction_page page = {} ) -> interaction_snapshot;
auto paginated_interaction( interaction_page_state &state ) -> interaction_snapshot;
auto resolve_interaction_command( const interaction_command &command )
-> std::expected<interaction_event, std::string>;

/// Validates a live or replayed semantic event against the widget bound to this context.
auto validate_interaction_event( const input_context &context, const interaction_event &event )
-> std::expected<void, std::string>;

auto interaction_kind_name( interaction_kind kind ) -> std::string;
auto opaque_interaction_id( const std::string &prefix, const std::vector<std::string> &identity )
-> std::string;
auto serialize_interaction( const interaction_snapshot &snapshot ) -> std::string;

} // namespace game_client
