#pragma once

#include "client_interaction.h"
#include "engine_client_contract.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class input_context;
struct input_event;

/// The 0.2 interaction model: a stack of screens, each a tree of generic components and the affordances
/// valid on it.
namespace engine_client
{

/// The `row` component ids of the avatar's ground and inventory rows, which item affordances name.
inline constexpr auto ground_rows = "avatar.ground";
inline constexpr auto inventory_rows = "avatar.inventory";

struct row_shape {
    std::string component_id = {};
};
struct rows_shape {
    std::string component_id = {};
    bool counts = false;
};
struct count_shape {
    std::optional<std::int64_t> min = std::nullopt;
    std::optional<std::int64_t> max = std::nullopt;
    std::optional<std::int64_t> initial = std::nullopt;
};
struct text_shape {
    /// Empty: the field the invoke's `row` names.
    std::string field_id = {};
    /// The text a client puts in the box when it asks for one; empty for none.
    std::string initial = {};
};
struct pos_shape {
    /// `square` or `overmap`.
    std::string unit = "square";
    /// The `map` component whose cursor bounds and range the square must lie within; empty for any square.
    std::string component_id = {};
};
struct pane_shape {
    std::string component_id = {};
};
/// The arguments an invoke must carry: every member present, no other.
struct arg_shape {
    std::optional<row_shape> row = std::nullopt;
    std::optional<rows_shape> rows = std::nullopt;
    std::optional<count_shape> count = std::nullopt;
    std::optional<text_shape> text = std::nullopt;
    std::optional<pos_shape> pos = std::nullopt;
    std::optional<pane_shape> pane = std::nullopt;
};

/// The native input an affordance runs, as the 0.1 operation it stands for. Not on the wire.
enum class invoke_kind {
    choose, fill, submit, set_count, cancel, action, travel, context, pickup, drop,
    /// The screen's own loop runs it, from the recorded `invoke` input (see `invoked`).
    screen,
};

struct affordance {
    std::string id = {};
    std::string label = {};
    /// One line on what it does, localized, for a tooltip or a menu; empty when the label says it all.
    std::string description = {};
    std::vector<std::string> keys = {};
    bool enabled = true;
    std::string denial = {};
    /// `confirm`, `cancel`, `any_key`, `layout`, `edit`; empty for none.
    std::string role = {};
    arg_shape args = {};
    invoke_kind runs = invoke_kind::action;
    /// The registered action this affordance performs on the screen, which is then not offered beside it
    /// unless it carries a denial; empty for none. Not on the wire.
    std::string covers = {};
    /// The registered action of `action`; the choice of a `choose` without `args.row`.
    std::string target = {};
    /// The client may hold the key down with `bn.hold`, and the engine takes one step of it per input prompt
    /// until `bn.release`: only a step of the map, which is a registered action.
    bool holdable = false;
    /// The menu of a workspace it sits in, localized, such as "File"; empty for none.
    std::string group = {};
};
/// The expectation an invoke of `value` carries: its screen's when it takes no `row`, `rows`, `pos` or
/// `text`, so a held key may queue behind another; the boundary's when it names rows or squares.
auto scope_of( const affordance &value ) -> expect_scope;

struct list_column {
    std::string key = {};
    std::string label = {};
};
/// One row of a list. The id is the row's identity on its screen, never its position or its text.
struct row {
    std::string id = {};
    /// The id the native command names when it differs from `id`; not on the wire.
    std::string native_id = {};
    std::string label = {};
    std::string description = {};
    std::string denial = {};
    /// The key that chooses the row natively; empty when it has none.
    std::string hotkey = {};
    bool enabled = true;
    bool selectable = true;
    bool selected = false;
    std::vector<game_client::interaction_column> columns = {};
    bool favorite = false;
    std::string color = {};
    std::string key_color = {};
    std::string markup = {};
    std::optional<std::string> parent_id = std::nullopt;
    std::string nest_color = {};
    std::optional<game_client::interaction_mark> mark = std::nullopt;
    std::optional<std::uint64_t> selected_count = std::nullopt;
    std::optional<std::uint64_t> minimum_count = std::nullopt;
    /// The most a count for the row can be, in its native units; none: the row takes no count.
    std::optional<std::uint64_t> max = std::nullopt;
    /// A category heading over the rows after it.
    bool header = false;
    /// The row switches something on or off, and this is which it is now; absent for a row that is no switch.
    std::optional<bool> toggled = std::nullopt;
    /// What the row stands for, for a client to draw.
    std::optional<engine_client::look> look = std::nullopt;
    /// The affordance of the screen that runs what the row does, a registered action; empty for none.
    std::string affordance = {};
};
struct list_body {
    std::vector<list_column> columns = {};
    std::vector<row> rows = {};
    std::size_t total = 0;
    std::size_t offset = 0;
    /// `none`, `single`, `multi` or `count`.
    std::string selection = "none";
    std::string activate = {};
    /// The list takes a filter text; its filter affordance is among the component's.
    bool filterable = false;
    /// The filter text the list holds; empty when it shows every row.
    std::string filter = {};
    /// What the list says while it has no rows, localized; empty for nothing.
    std::string empty = {};
    /// The most rows a client shows at once, scrolling the rest; zero for no limit.
    std::size_t rows_shown = 0;
};
struct tab_entry {
    std::string id = {};
    std::string label = {};
    bool selected = false;
    bool enabled = true;
    std::string denial = {};
    /// Something in the tab is new to the player.
    bool unread = false;
    /// The key that shows the tab; empty when it has none.
    std::string hotkey = {};
};
/// The selected tab's content is the component's one child.
struct tabs_body {
    std::vector<tab_entry> tabs = {};
    std::string select = {};
};
struct split_body {
    std::string active = {};
    /// `peers`, or `detail` when the later parts describe the first.
    std::string relation = "peers";
};
struct panel_stat {
    std::string key = {};
    std::string label = {};
    std::string value = {};
    /// What the value can reach; empty when unbounded.
    std::string max = {};
    std::string unit = {};
    /// `good`, `bad` or empty.
    std::string tone = {};
};
/// The pane's content is the component's one child.
struct panel_body {
    std::string title = {};
    std::string subtitle = {};
    std::vector<panel_stat> stats = {};
    std::string filter = {};
};
struct form_option {
    std::string id = {};
    std::string label = {};
};
struct form_field {
    std::string id = {};
    std::string label = {};
    std::string description = {};
    std::string value = {};
    /// `text`, `integer`, `float`, `key`, `bool` or `select`.
    std::string type = "text";
    int max_length = -1;
    bool printable = false;
    /// The text may run over several lines.
    bool multiline = false;
    /// The form's submit refuses while the field is empty, which a client may mark.
    bool required = false;
    /// Where the cursor stands in a text, in characters from its start; none: the field has no cursor.
    std::optional<int> cursor = std::nullopt;
    std::vector<form_option> options = {};
    std::optional<double> min = std::nullopt;
    std::optional<double> max = std::nullopt;
};
struct form_body {
    std::vector<form_field> fields = {};
    std::string submit = {};
};
struct text_body {
    std::string markup = {};
    /// `good`, `bad` or empty.
    std::string tone = {};
    /// `terminal_output`, `banner`, `prompt` or empty.
    std::string role = {};
};
struct grid_point {
    int x = 0;
    int y = 0;
    auto operator<=>( const grid_point & ) const = default; // *NOPAD*
};
/// A rectangle of a grid's squares from `from` to `to`, both inside it.
struct grid_region {
    std::string id = {};
    grid_point from = {};
    grid_point to = {};
    std::string label = {};
    bool selected = false;
};
struct grid_cell {
    grid_point at = {};
    std::string id = {};
    std::string glyph = {};
    std::string label = {};
    std::string color = {};
    std::string affordance = {};
    bool enabled = true;
    std::string denial = {};
    bool selected = false;
    /// What the cell shows from the bottom up, as indexes into the grid's `looks`, for a client that draws
    /// sprites; the glyph stands for it otherwise.
    std::vector<int> looks = {};
    /// Whether it shows another level, which a client draws faded.
    bool faded = false;
    /// Why the tool in hand cannot act on the cell, which a client draws dimmed and shows on hover;
    /// the cell still takes a click. Empty when it can. On the wire it is an index into the
    /// grid's `refusals`, which lists each reason once.
    std::string refused = {};
    auto operator==( const grid_cell & ) const -> bool = default; // *NOPAD*
};
struct grid_body {
    int cols = 0;
    int rows = 0;
    std::optional<grid_point> cursor = std::nullopt;
    std::vector<grid_cell> cells = {};
    /// The looks the cells name, each once.
    std::vector<engine_client::look> looks = {};
    /// `canvas` for squares the player draws on, which take whatever room a client has; empty for none.
    std::string role = {};
    /// `trail` for a drag whose every crossed square runs the cells' affordance, `span` for one whose press and
    /// release squares do; empty for none, a click only.
    std::string drag = {};
    /// The affordances a client runs on press and after release of a drag, to make it one step; empty for none.
    std::string drag_begin = {};
    std::string drag_end = {};
    /// The squares a client outlines as one, such as an overmap square's; none while zero.
    grid_point block = {};
    /// Rectangles of squares a client outlines over the cells, such as where a placement may land.
    std::vector<grid_region> regions = {};
    /// The affordance a client runs after dragging a region: its inside moves it, a corner
    /// stretches it; empty for none.
    std::string region_drag = {};
    /// The affordance a client runs with a cell's id as the row on a secondary click of that cell,
    /// such as a right click; empty for none.
    std::string secondary = {};
    /// `trail` for a secondary drag, such as a right drag, whose every crossed square runs
    /// `secondary`, between `drag_begin` and `drag_end`; empty for none.
    std::string secondary_drag = {};
    /// The cell every square shows that the wire lists no cell for, as most of a canvas's empty
    /// squares are. A grid with one names every square by `square_id` on the wire, and sends
    /// each look once among its `styles` and the other squares as runs east of one style.
    std::optional<grid_cell> blank = std::nullopt;
    /// The squares a client outlines around the square under the pointer, as offsets east and
    /// south of it, such as where a paste lands; none for none.
    std::vector<grid_point> stamp = {};
};
/// The row id `square:<x>,<y>` a client gives the square `at` of a grid that lists no cell there.
auto square_id( grid_point at ) -> std::string;
/// Whether the wire leaves out `cell` of `body`: it shows the grid's blank cell, whatever its id,
/// so that a client names it by `square_id`, which `cell_of` reads back to it.
auto blank_square( const grid_body &body, const grid_cell &cell ) -> bool;
/// The cell of `body` that `row` names: the one of that id, else, in a grid with a blank cell,
/// the one at the square a `square:<x>,<y>` id names; nullptr when there is none.
auto cell_of( const grid_body &body, std::string_view row ) -> const grid_cell *; // *NOPAD*
/// The looks of a grid, each once, which its cells name by index.
struct look_palette {
    std::vector<engine_client::look> looks = {};
    /// The indexes of `values` in the palette, adding those it lacks.
    auto indexes( const std::vector<engine_client::look> &values ) -> std::vector<int>;
};
/// Positions stay bubble-relative here, as natively; they become absolute on the wire.
/// A list of the avatar's items the map shows beside it.
struct item_list {
    /// The avatar member that holds the rows, such as `avatar.ground`.
    std::string id = {};
    /// The heading, localized.
    std::string label = {};
    /// What moving a row out of the list does, localized.
    std::string verb = {};
};
struct map_body {
    /// `world` or `overmap`.
    std::string mode = "world";
    std::optional<game_client::interaction_target> target = std::nullopt;
    std::optional<game_client::interaction_overmap> overmap = std::nullopt;
    std::vector<item_list> item_lists = {};
};
using component_body =
    std::variant<list_body, tabs_body, split_body, panel_body, form_body, text_body, grid_body,
    map_body, game_client::interaction_canvas>;

struct component {
    std::string id = {};
    std::string label = {};
    /// Ids of the screen's affordances that act on this component.
    std::vector<std::string> affordances = {};
    component_body body = text_body{};
    /// A split's parts; the one child of tabs and panel.
    std::vector<component> children = {};
    /// What a part of a workspace's root is for: `tools`, `canvas`, `palette`, `outline`, `inspector`, `log` or
    /// `status`; empty for none.
    std::string slot = {};
    /// The part starts folded to its heading, which the player unfolds.
    bool collapsed = false;
};
/// The wire name of the component's kind.
auto component_kind( const component &value ) -> std::string;
/// The component with `id` in the tree under `root`, `root` included; nullptr when none.
auto find_component( const component &root, std::string_view id ) -> const component *; // *NOPAD*
/// The ids an affordance's `row` may name in `value` and the parts inside it: not a heading, nor a row, tab or
/// cell the screen does not let be picked; nullopt when it holds only some of them (a paged list).
auto row_ids( const component &value ) -> std::optional<std::vector<std::string>>;

/// Children moved in; a braced list would copy whole subtrees.
template<typename... Parts>
auto children_of( Parts &&... parts ) -> std::vector<component>
{
    auto result = std::vector<component> {};
    result.reserve( sizeof...( parts ) );
    ( result.push_back( std::forward<Parts>( parts ) ), ... );
    return result;
}

struct list_options {
    std::string id = {};
    std::vector<row> rows = {};
    /// The rows of the whole list; none: `rows` are all of it.
    std::optional<std::size_t> total = std::nullopt;
    /// How many rows of the list come before `rows`.
    std::size_t offset = 0;
    /// Empty: `none` when no row can be picked, `multi` when rows are marked or counted, else `single`.
    std::string selection = {};
    /// The affordance that picks a row.
    std::string activate = {};
    bool filterable = false;
    /// The filter text the list holds; empty when it shows every row.
    std::string filter = {};
    /// The affordance that sets the filter text, listed among the component's.
    std::string search = {};
    /// The affordance that turns its pages, listed among the component's.
    std::string page = {};
    /// What the list says while it has no rows; empty for nothing.
    std::string empty = {};
    /// The most rows a client shows at once, scrolling the rest; zero for no limit.
    std::size_t rows_shown = 0;
};
/// A list over `rows`, with the column header they use.
auto list( list_options options ) -> component;

struct panel_options {
    std::string id = {};
    std::string title = {};
    std::string subtitle = {};
    std::vector<panel_stat> stats = {};
    std::string filter = {};
    std::optional<component> child = std::nullopt;
};
auto panel( panel_options options ) -> component;

struct split_options {
    std::string id = {};
    /// `peers` or `detail`.
    std::string relation = "peers";
    std::string active = {};
    std::vector<component> children = {};
};
auto split( split_options options ) -> component;

struct tabs_options {
    std::string id = {};
    std::vector<tab_entry> tabs = {};
    /// The affordance that switches tab.
    std::string select = {};
    component child = {};
};
auto tabs( tabs_options options ) -> component;

struct invoke_options {
    std::string id = {};
    std::string label = {};
    /// See `affordance::description`.
    std::string description = {};
    arg_shape args = {};
    std::vector<std::string> keys = {};
    bool enabled = true;
    std::string denial = {};
    std::string role = {};
    /// The registered action it performs: see `affordance::covers`.
    std::string covers = {};
    std::string group = {};
};
/// An affordance the screen runs itself: its loop reads the invoke with `invoked`.
auto invoke_affordance( invoke_options options ) -> affordance;

/// The affordance a client invoked in the input read that just returned, with its arguments.
struct invoked_affordance {
    std::string affordance_id = {};
    /// The invoke runs the row, like Enter on it; false for a `select` that only moves the cursor to it.
    bool activates = false;
    game_client::interaction_arguments args = {};
};
auto invoked( const input_event &event ) -> std::optional<invoked_affordance>;

/// An action of the screen's context that cannot run now, and why. The registered affordance of that id is published disabled.
struct denial {
    std::string id = {};
    std::string reason = {};
};

/// A button of the map's shortcut bar: the registered action it runs, named and keyed as the map's input
/// context has it, and the icon the data gives it.
struct shortcut {
    /// The id of the registered action; the affordance of that id runs it while the map is on top.
    std::string affordance_id = {};
    std::string label = {};
    std::vector<std::string> keys = {};
    std::string icon = {};
    std::string glyph = {};
    /// The map is on top and the action can run now.
    bool enabled = true;
};

/// What a screen publishes. A provider fills everything but `screen_id`, `role` and `context`, which
/// come from the screen's scope; `root` is absent for a screen that has no components yet.
struct screen_view {
    std::string screen_id = {};
    /// What screen this is, the same at every opening and in every language; empty for a read without a scope.
    std::string kind = {};
    /// `map`, `screen`, `dialog` or `workspace`.
    std::string role = "dialog";
    std::string context = {};
    std::string title = {};
    std::optional<component> root = std::nullopt;
    std::vector<affordance> affordances = {};
    std::vector<denial> denials = {};
    std::string focus_component = {};
    std::string focus_row = {};
    int refresh_ms = 0;
    /// Where the player asked the window to sit: `left`, `right`, `overlapping` or `cursor`; empty for none.
    std::string dock = {};
    /// The map's shortcut bar; empty on every other screen.
    std::vector<shortcut> shortcuts = {};
};

/// A const read of the screen's model state: it changes nothing and draws nothing.
using view_provider = std::function < auto( const boundary_state & ) -> screen_view >;

struct screen_options {
    /// `screen` or `dialog`; the map is not a scope.
    std::string role = "dialog";
    /// Absent: the generic path describes the screen from its 0.1 interaction.
    view_provider view = {};
};

/// Alive for as long as the screen is open, not only while it reads input. Scopes nest as screens do,
/// and the live ones make up the interaction stack. Its serial names the screen: new when the screen
/// is reopened. `kind` names what screen it is, such as `crafting`, and stays the same at every opening.
class screen_scope
{
    public:
        screen_scope( const input_context &context, std::string kind, screen_options options = {} );
        ~screen_scope();
        screen_scope( const screen_scope & ) = delete;
        auto operator=( const screen_scope & ) -> screen_scope & = delete; // *NOPAD*

    private:
        std::uint64_t serial_ = 0;
};

/// The open screens, outermost first. The map is the first while the game runs, and a view that reads
/// input without a scope is the last, with a new id on every read. Only the last has affordances.
auto interaction_stack( const boundary_state &state ) -> std::vector<screen_view>;
/// The last of `interaction_stack`; the screen input goes to.
auto top_screen( const boundary_state &state ) -> screen_view;
struct rows_request {
    std::string epoch = {};
    std::string boundary_id = {};
    /// A `list` of the top screen.
    std::string component_id = {};
    std::size_t offset = 0;
    std::size_t limit = 100;
};
struct rows_page {
    std::string boundary_id = {};
    std::string component_id = {};
    /// The rows of the whole list.
    std::size_t total = 0;
    std::vector<row> rows = {};
};
/// A window of one list's rows at the live boundary. A list that holds all its rows is sliced from them;
/// one that holds a page is read natively, so each list pages within its own rows.
auto read_rows( const rows_request &request,
                const boundary_state &state ) -> std::expected<rows_page, error>;
/// The native id of the row of the live generic list that the 0.2 id `row_id` names, read from the whole native
/// list and not the published page; nullopt when no row has that id or the id is the native one.
auto native_row_id( const std::string &row_id ) -> std::optional<std::string>;
auto serialize_rows( const rows_page &page ) -> std::string;

/// The `interaction_boundary` value of the 0.2 schema: the 0.1 members and the `stack`.
auto serialize_interaction_boundary( const boundary_state &state ) -> std::string;

} // namespace engine_client
