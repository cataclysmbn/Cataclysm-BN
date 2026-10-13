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

/// The 0.2 interaction model: a tree of generic components and the affordances valid at a boundary.
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
};
struct pos_shape {
    /// `square` or `overmap`.
    std::string unit = "square";
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
    choose, fill, submit, set_count, set_target, target_candidate, cancel, action, travel, context, pickup, drop,
    /// The registered action `args.row` names, such as an area of the advanced inventory.
    row_action,
};

/// Runs an affordance its screen handles itself, once its arguments match its shape.
using invoke_handler = std::function < auto( const invoke_args & ) ->
                       std::expected<command_operation, error >>;

struct affordance {
    std::string id = {};
    std::string label = {};
    std::vector<std::string> keys = {};
    bool enabled = true;
    std::string denial = {};
    /// `confirm`, `cancel`, `any_key`; empty for none.
    std::string role = {};
    arg_shape args = {};
    invoke_kind runs = invoke_kind::action;
    /// The registered action of `action`; the choice of a `choose` without `args.row`.
    std::string target = {};
    /// When set, runs the affordance in place of `runs`. Not on the wire.
    invoke_handler handler = {};
};

struct list_column {
    std::string key = {};
    std::string label = {};
};
struct list_row {
    game_client::interaction_choice choice = {};
    /// A category heading over the rows after it.
    bool header = false;
};
struct list_body {
    std::vector<list_column> columns = {};
    std::vector<list_row> rows = {};
    std::size_t total = 0;
    std::size_t offset = 0;
    /// `none`, `single`, `multi` or `count`.
    std::string selection = "none";
    std::string activate = {};
};
struct tab_entry {
    std::string id = {};
    std::string label = {};
    bool selected = false;
    bool enabled = true;
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
    /// `terminal_output`, `banner` or empty.
    std::string role = {};
};
struct grid_point {
    int x = 0;
    int y = 0;
    auto operator<=>( const grid_point & ) const = default; // *NOPAD*
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
};
struct grid_body {
    int cols = 0;
    int rows = 0;
    std::optional<grid_point> cursor = std::nullopt;
    std::vector<grid_cell> cells = {};
};
/// Positions stay bubble-relative here, as natively; they become absolute on the wire.
struct map_body {
    /// `world` or `overmap`.
    std::string mode = "world";
    std::optional<game_client::interaction_target> target = std::nullopt;
    std::optional<game_client::interaction_overmap> overmap = std::nullopt;
};
using component_body =
    std::variant<list_body, tabs_body, split_body, panel_body, form_body, text_body, grid_body,
    map_body, game_client::interaction_canvas>;

struct component {
    std::string id = {};
    std::string label = {};
    /// Ids of the interaction's affordances that act on this component.
    std::vector<std::string> affordances = {};
    component_body body = text_body{};
    /// A split's parts; the one child of tabs and panel.
    std::vector<component> children = {};
};
/// The wire name of the component's kind.
auto component_kind( const component &value ) -> std::string;
/// The component with `id` in the tree under `root`, `root` included; nullptr when none.
auto find_component( const component &root, std::string_view id ) -> const component *; // *NOPAD*
/// The ids an affordance's `row` may name in `value`; nullopt when it holds only some of them (a paged list).
auto row_ids( const component &value ) -> std::optional<std::vector<std::string>>;

struct interaction_view {
    /// Empty at the map, where a submit expects no schema.
    std::string schema_id = {};
    std::string context = {};
    std::string title = {};
    component root = {};
    std::vector<affordance> affordances = {};
    std::string focus_component = {};
    std::string focus_row = {};
    int refresh_ms = 0;
};

using view_provider = std::function < auto( const boundary_state & ) -> interaction_view >;

/// While alive, the screen waiting on `context` publishes its own components and affordances in
/// place of the generic ones; the registered actions still follow them. Consulted only while
/// `context` is the active input context, so the provider and its handlers may refer to the
/// screen's state.
class view_scope
{
    public:
        view_scope( const input_context &context, view_provider provider );
        ~view_scope();
        view_scope( const view_scope & ) = delete;
        auto operator=( const view_scope & ) -> view_scope & = delete; // *NOPAD*

    private:
        const input_context *context;
};

/// The components and affordances of a boundary; nullopt where it offers nothing. At the map, a
/// world `map` root with the map's actions.
auto interaction_components( const boundary_state &state ) -> std::optional<interaction_view>;
/// The `interaction_boundary` value of the 0.2 schema.
auto serialize_interaction_boundary( const boundary_state &state ) -> std::string;

} // namespace engine_client
