#pragma once

#include "client_interaction_data.h"
#include "tile_parts.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class input_context;

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
    /// The row's identity in the 0.2 components when `id` is a position; empty when `id` is the identity.
    std::string identity;
    /// The row switches something on or off, and this is which it is now; absent for a row that is no switch.
    std::optional<bool> toggled;
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

/// Squares a target screen highlights besides the cursor: `squares` lists them, or `rect` gives its inclusive corners.
struct interaction_target_mark {
    /// `selection`, `omt_border` or `stamp`.
    std::string role;
    std::vector<interaction_position> squares;
    struct corners {
        interaction_position min;
        interaction_position max;
    };
    std::optional<corners> rect;
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
    /// The status in words, localized; empty where the screen has none.
    std::string status_text;
    bool limit_to_reality_bubble = false;
    std::vector<interaction_target_candidate> candidates;
    /// The squares of the line of fire in order, the source left out; empty where the screen draws none.
    std::vector<interaction_position> trajectory;
    /// How far the cursor is from the source as the screen measures range; absent: by `distance_metric`.
    std::optional<int> distance;
    /// What the native look window prints of the cursor square; empty where the screen prints nothing.
    std::vector<engine_client::tile_part> parts;
    /// What the native screen highlights besides the cursor; presentation only.
    std::vector<interaction_target_mark> marks;
};

struct interaction_overmap_note {
    interaction_position position;
    std::string text;
};

/// A name the native window prints over an overmap square, in curses color indexes 0-15.
struct interaction_overmap_label {
    interaction_position position;
    std::string text;
    int foreground = 7;
    int background = 0;
};

/// The tile the native tiles view draws for an overmap square.
struct interaction_overmap_tile {
    std::string id;
    int rotation = 0;
    /// The multitile key (`edge`, `corner`, ...), empty for a plain tile.
    std::string subtile;
};

/// Overmap squares as the player knows them, row-major, with what the screen says of the cursor
/// square. Positions are absolute overmap terrain squares.
struct interaction_overmap {
    /// The square in the top-left cell.
    interaction_position origin;
    int cols = 0;
    int rows = 0;
    /// What each square shows: its terrain glyph, or what lies over it, with curses color indexes 0-15.
    std::vector<std::string> glyphs;
    std::vector<int> foreground;
    std::vector<int> background;
    interaction_position player;
    /// What the sidebar says besides the cursor square's description, one localized line each.
    std::vector<std::string> legend;
    /// What the native sidebar says of the cursor square, without the window frame or the square's glyph.
    std::string cursor_description;
    /// `You are here.`, localized, when the cursor is on the player's square; empty otherwise.
    std::string cursor_here;
    /// The note the native window prints in its corner for the cursor square, as one localized line; empty when none.
    std::string cursor_note;
    /// The length of the planned travel route as one localized line; empty without a route.
    std::string route_text;
    std::vector<interaction_overmap_note> notes;
    /// The heading of the notes, localized.
    std::string notes_title;
    /// The names of cities and labelled squares the native window prints over the squares, while its toggle is on.
    std::vector<interaction_overmap_label> labels;
    /// The target of the active mission while the overlays show it; the native window marks it at its edge when it lies outside.
    std::optional<interaction_position> mission_target;
    /// The planned travel route, nearest square first.
    std::vector<interaction_position> path;
    /// Row-major like `glyphs`; empty unless the client renders tiles.
    std::vector<interaction_overmap_tile> tiles;
    /// For each tile id used, the ids to try next, nearest first.
    std::map<std::string, std::vector<std::string>> looks_like;
};

/// A run of cells of one window row that share their colors, as curses color indexes 0-15.
struct interaction_canvas_run {
    std::string text;
    int foreground = 7;
    int background = 0;
    auto operator<=>( const interaction_canvas_run & ) const = default; // *NOPAD*
};

/// A native window as drawn: one list of color runs per row, without the blank cells that end a row.
struct interaction_canvas {
    int cols = 0;
    std::vector<std::vector<interaction_canvas_run>> rows;
    auto operator<=>( const interaction_canvas & ) const = default; // *NOPAD*
};

/// What the terminal shows now, as the windows composed it for the memory display.
auto composed_canvas() -> interaction_canvas;
/// The cells of the terminal inside `area`, as the windows composed them.
auto terminal_canvas( const rectangle<point> &area ) -> interaction_canvas;

/// A key the screen's own handler tests, which a client presses by invoking the `id` affordance.
struct interaction_key {
    std::string id;
    std::string label;
    std::vector<std::string> keys;
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
    bool structured = false;
    bool actions_only = true;
    bool allow_cancel = false;
    std::vector<interaction_choice> choices;
    /// The keys the screen's handler tests besides its actions; they are screen buttons, never rows.
    std::vector<interaction_key> keys;
    std::optional<interaction_field> field;
    std::optional<interaction_target> target;
    /// Nonzero where the engine redraws the screen on its own every this many milliseconds, with no input.
    int refresh_ms = 0;
    bool allow_set_count = false;
    std::size_t choice_offset = 0;
    std::size_t choice_total = 0;
    /// The native list cursor, reported before paging so it survives an off-page row.
    std::string focus_choice_id;
    /// The choice any key press acknowledges, for a "press any key" screen that binds no key; empty otherwise.
    std::string any_key_choice_id;
    /// The list takes a filter text (its screen's FILTER action).
    bool filterable = false;
    /// The filter text the list holds; empty when it shows every row.
    std::string filter;
};

struct interaction_page {
    std::size_t offset = 0;
    std::size_t limit = 100;
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
auto resolve_interaction_command( const interaction_command &command )
-> std::expected<interaction_event, std::string>;

/// Validates a live or replayed semantic event against the widget bound to this context.
auto validate_interaction_event( const input_context &context, const interaction_event &event )
-> std::expected<void, std::string>;

auto interaction_kind_name( interaction_kind kind ) -> std::string;
/// How far `to` is from the target source by the screen's metric, as the game measures range:
/// rounded Euclidean for `trig`, Chebyshev otherwise.
auto target_distance( const interaction_target &target, const interaction_position &to ) -> int;
/// Whether the cursor may be sent to `to`: inside the coordinate bounds. Past the range the screen holds the
/// cursor at the range's end, as it does for the native mouse.
auto target_bounds_contain( const interaction_target &target,
                            const interaction_position &to ) -> bool;

auto opaque_interaction_id( const std::string &prefix, const std::vector<std::string> &identity )
-> std::string;

} // namespace game_client
