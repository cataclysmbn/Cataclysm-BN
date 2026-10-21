#include "editor/editor_session.h"

#include "editor/document.h"
#include "editor/json_pointer.h"
#include "editor/mapgen_check.h"
#include "editor/mapgen_legend.h"
#include "editor/mapgen_stroke.h"
#include "engine_client_component.h"
#include "input.h"
#include "output.h"
#include "path_info.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui_manager.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace editor {
namespace {

namespace ec = engine_client;

constexpr auto file_id = "editor:file";
constexpr auto path_id = "path";
constexpr auto canvas_id = "editor:canvas";
constexpr auto tools_id = "editor:tools";
constexpr auto legend_id = "editor:legend";
constexpr auto objects_id = "editor:objects";
constexpr auto diagnostics_id = "editor:diagnostics";

constexpr auto open_id = "editor:open";
constexpr auto object_id = "editor:object";
constexpr auto tool_id = "editor:tool";
constexpr auto symbol_id = "editor:symbol";
constexpr auto paint_id = "editor:paint";
constexpr auto undo_id = "editor:undo";
constexpr auto redo_id = "editor:redo";
constexpr auto save_id = "editor:save";

struct tool {
    std::string id;
    /// None: the eyedropper, which picks the symbol it is used on.
    std::optional<shape_kind> kind;
    /// Whether it takes two squares, the first of them its anchor.
    bool spans = false;
};

const auto tools = std::array{
    tool{.id = "tool:point", .kind = shape_kind::point},
    tool{.id = "tool:line", .kind = shape_kind::line, .spans = true},
    tool{.id = "tool:rect", .kind = shape_kind::rect, .spans = true},
    tool{.id = "tool:rect_filled", .kind = shape_kind::rect_filled, .spans = true},
    tool{.id = "tool:ellipse", .kind = shape_kind::ellipse, .spans = true},
    tool{.id = "tool:fill", .kind = shape_kind::fill},
    tool{.id = "tool:pick", .kind = std::nullopt},
};

auto tool_label(const tool& value) -> std::string {
    if (!value.kind) { return _("Eyedropper"); }
    switch (*value.kind) {
        case shape_kind::point:
            return _("Pencil");
        case shape_kind::line:
            return _("Line");
        case shape_kind::rect:
            return _("Rectangle");
        case shape_kind::rect_filled:
            return _("Filled rectangle");
        case shape_kind::ellipse:
            return _("Ellipse");
        case shape_kind::fill:
            return _("Fill");
        case shape_kind::room:
            return _("Room");
    }
    return {};
}

auto square_id(const point& at) -> std::string { return string_format("square:%d,%d", at.x, at.y); }

auto is_json_mapgen(const nlohmann::ordered_json& value) -> bool {
    return value.is_object() && value.value("type", "") == "mapgen"
        && value.value("method", "") == "json";
}

/// What names a mapgen entry: its overmap terrain, or the id it is nested or updated by.
auto mapgen_name(const nlohmann::ordered_json& value) -> std::string {
    for (const auto* const key : {"om_terrain", "nested_mapgen_id", "update_mapgen_id"}) {
        if (!value.contains(key)) { continue; }
        const auto& name = value[key];
        return name.is_string() ? name.get<std::string>() : name.dump();
    }
    return _("(unnamed)");
}

/// Whether `path` lies inside the game's data or the player's mods, where the editor may write.
auto editable_place(const std::filesystem::path& path) -> bool {
    const auto inside = [&](const std::string& root) {
        auto error = std::error_code{};
        const auto base = std::filesystem::weakly_canonical(root, error);
        const auto relative =
            std::filesystem::weakly_canonical(path, error).lexically_relative(base);
        return !error && !relative.empty() && *relative.begin() != "..";
    };
    return path.extension() == ".json"
        && (inside(PATH_INFO::datadir()) || inside(PATH_INFO::user_moddir()));
}

struct diagnostic {
    std::string text;
    std::string pointer;
};

struct session {
    std::optional<document> doc = std::nullopt;
    /// What the last command did not do, and why; empty when it did.
    std::string notice;
    /// The object of `doc` on the canvas.
    std::size_t object = 0;
    std::string symbol;
    const tool* current = tools.data();
    std::optional<point> anchor = std::nullopt;
    std::vector<legend_symbol> legend;
    std::vector<diagnostic> diagnostics;

    auto mapgens() const -> std::vector<std::size_t> {
        if (!doc) { return {}; }
        return std::views::iota(std::size_t{0}, doc->objects().size())
             | std::views::filter([&](const auto index) {
                   return is_json_mapgen(doc->objects()[index].value);
               })
             | std::ranges::to<std::vector>();
    }

    /// What the loader makes of the object now: its legend and what it reports.
    auto reload() -> void {
        legend.clear();
        diagnostics.clear();
        if (!doc || object >= doc->objects().size()) { return; }
        const auto text = doc->objects()[object].value.dump();
        legend = mapgen_legend(text).value_or(std::vector<legend_symbol>{});
        diagnostics =
            check_mapgen(text) | std::views::transform([&](const auto& message) {
                return diagnostic{
                    .text = message.text,
                    .pointer = message.offset ? pointer_at(text, *message.offset) : ""};
            })
            | std::ranges::to<std::vector>();
        if (std::ranges::none_of(legend, [&](const auto& entry) {
                return entry.symbol == symbol;
            })) {
            symbol = legend.empty() ? std::string{} : legend.front().symbol;
        }
    }

    auto open(const std::string& name) -> void {
        const auto path = std::filesystem::path(name);
        if (!editable_place(path)) {
            notice = string_format(_("%s is no data file of the game or of your mods."), name);
            return;
        }
        auto opened = document::open(path);
        if (!opened) {
            notice = opened.error();
            return;
        }
        doc = std::move(*opened);
        anchor.reset();
        const auto found = mapgens();
        object = found.empty() ? doc->objects().size() : found.front();
        notice = found.empty() ? string_format(_("%s holds no JSON mapgen."), name) : "";
        reload();
    }

    auto paint(const point& at) -> void {
        const auto grid = symbols_of(*doc, object);
        if (!current->kind) {
            symbol = symbol_at(grid, at);
            return;
        }
        if (current->spans && !anchor) {
            anchor = at;
            return;
        }
        const auto drawn = stroke(
            *doc,
            {.object = object,
             .drawn = {.kind = *current->kind, .from = anchor.value_or(at), .to = at},
             .symbol = symbol});
        anchor.reset();
        notice = drawn ? "" : drawn.error();
        reload();
    }
};

auto canvas(const session& state) -> ec::component {
    const auto grid = symbols_of(*state.doc, state.object);
    auto cells = std::vector<ec::grid_cell>{};
    auto cols = 0;
    for (auto y = 0; y < static_cast<int>(grid.size()); ++y) {
        cols = std::max(cols, static_cast<int>(grid[y].size()));
        for (auto x = 0; x < static_cast<int>(grid[y].size()); ++x) {
            const auto at = point(x, y);
            cells.push_back(
                {.at = {x, y},
                 .id = square_id(at),
                 .glyph = grid[y][x],
                 .affordance = paint_id,
                 .selected = state.anchor == at});
        }
    }
    return {
        .id = canvas_id,
        .affordances = {paint_id},
        .body = ec::grid_body{
            .cols = cols,
            .rows = static_cast<int>(grid.size()),
            .cursor = state.anchor ? std::optional(ec::grid_point{state.anchor->x, state.anchor->y})
                                   : std::nullopt,
            .cells = std::move(cells)}};
}

auto legend_rows(const session& state) -> std::vector<ec::row> {
    return state.legend | std::views::transform([&](const legend_symbol& entry) {
               auto columns = std::vector<game_client::interaction_column>{};
               for (const auto& line : entry.lines) {
                   const auto found = std::ranges::
                       find(columns, line.layer, &game_client::interaction_column::key);
                   if (found != columns.end()) {
                       found->value += ", " + line.value;
                   } else {
                       columns.push_back(
                           {.key = line.layer, .label = line.layer, .value = line.value});
                   }
               }
               auto sources =
                   entry.lines | std::views::transform(&legend_line::source)
                   | std::views::filter([](const auto& source) { return !source.empty(); })
                   | std::ranges::to<std::vector>();
               std::ranges::sort(sources);
               const auto unique = std::ranges::unique(sources);
               sources.erase(unique.begin(), unique.end());
               return ec::row{
                   .id = "symbol:" + entry.symbol,
                   .label = entry.symbol,
                   .description =
                       sources.empty() ? std::string{}
                                       : string_format(_("From %s"), enumerate_as_string(sources)),
                   .selected = entry.symbol == state.symbol,
                   .columns = std::move(columns)};
           })
         | std::ranges::to<std::vector>();
}

auto session_view(const input_context& ctxt, const session& state) -> ec::screen_view {
    auto affordances = std::vector<ec::affordance>{ec::invoke_affordance(
        {.id = open_id, .label = _("Open"), .args = {.text = ec::text_shape{.field_id = path_id}}})};
    auto parts = ec::children_of(ec::component{
        .id = file_id,
        .affordances = {open_id},
        .body = ec::form_body{
            .fields = {{.id = path_id,
                        .label = _("File"),
                        .value = state.doc ? state.doc->path().generic_string() : ""}},
            .submit = open_id}});
    const auto row_of = [](const char* const component) {
        return ec::arg_shape{.row = ec::row_shape{.component_id = component}};
    };
    if (state.doc) {
        const auto& doc = *state.doc;
        auto stats = std::vector<ec::panel_stat>{
            {.key = "saved",
             .label = _("Saved"),
             .value = doc.dirty() ? _("No") : _("Yes"),
             .tone = doc.dirty() ? "bad" : "good"},
        };
        if (!state.symbol.empty()) {
            stats.push_back({.key = "symbol", .label = _("Symbol"), .value = state.symbol});
        }
        parts.push_back(ec::panel(
            {.id = "editor:document",
             .title = doc.path().generic_string(),
             .stats = std::move(stats)}));
        const auto mapgens = state.mapgens();
        parts.push_back(ec::list(
            {.id = objects_id,
             .rows = mapgens | std::views::transform([&](const auto index) {
                         return ec::row{
                             .id = string_format("object:%d", index),
                             .label = mapgen_name(doc.objects()[index].value),
                             .selected = index == state.object};
                     })
                   | std::ranges::to<std::vector>(),
             .selection = "single",
             .activate = object_id}));
        affordances.push_back(ec::invoke_affordance(
            {.id = object_id, .label = _("Edit"), .args = row_of(objects_id)}));
        affordances.push_back(
            ec::invoke_affordance({.id = undo_id, .label = _("Undo"), .enabled = doc.can_undo()}));
        affordances.push_back(
            ec::invoke_affordance({.id = redo_id, .label = _("Redo"), .enabled = doc.can_redo()}));
        affordances.push_back(
            ec::invoke_affordance({.id = save_id, .label = _("Save"), .enabled = doc.dirty()}));
        if (std::ranges::contains(mapgens, state.object)) {
            parts.push_back(ec::tabs(
                {.id = tools_id,
                 .tabs = tools | std::views::transform([&](const tool& entry) {
                             return ec::tab_entry{
                                 .id = entry.id,
                                 .label = tool_label(entry),
                                 .selected = &entry == state.current};
                         })
                       | std::ranges::to<std::vector>(),
                 .select = tool_id,
                 .child = canvas(state)}));
            parts.push_back(ec::list(
                {.id = legend_id,
                 .rows = legend_rows(state),
                 .selection = "single",
                 .activate = symbol_id}));
            parts.push_back(ec::list(
                {.id = diagnostics_id,
                 .rows = state.diagnostics | std::views::enumerate
                       | std::views::transform([](const auto& entry) {
                             const auto& [index, message] = entry;
                             return ec::row{
                                 .id = string_format("message:%d", index),
                                 .label = message.text,
                                 .description = message.pointer,
                                 .selectable = false};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "none"}));
            affordances.push_back(ec::invoke_affordance(
                {.id = tool_id, .label = _("Tool"), .args = row_of(tools_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = symbol_id, .label = _("Symbol"), .args = row_of(legend_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = paint_id, .label = _("Draw"), .args = row_of(canvas_id)}));
        }
    }
    if (!state.notice.empty()) {
        parts.push_back(
            {.id = "editor:notice", .body = ec::text_body{.markup = state.notice, .tone = "bad"}});
    }
    return {
        .title = ctxt.get_action_name("editor"),
        .root = ec::split(
            {.id = "editor:body",
             .active = state.doc ? canvas_id : "",
             .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = state.doc ? canvas_id : ""};
}

/// The square a canvas row names.
auto square_of(const std::string& id) -> std::optional<point> {
    auto at = point{};
    if (std::sscanf(id.c_str(), "square:%d,%d", &at.x, &at.y) != 2) { return std::nullopt; }
    return at;
}

} // namespace

auto run_editor_session() -> void {
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto state = session{};
    const auto screen = ec::screen_scope(
        ctxt, "editor",
        {.role = "screen", .view = [&](const ec::boundary_state& /*boundary*/) {
             return session_view(ctxt, state);
         }});
    while (true) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto invoked = ec::invoked(ctxt.get_raw_input());
        if (!invoked) {
            if (action == "QUIT"
                && (!state.doc || !state.doc->dirty()
                    || query_yn(_("Leave the editor without saving your changes?")))) {
                return;
            }
            continue;
        }
        const auto& id = invoked->affordance_id;
        const auto row = invoked->args.row.value_or("");
        state.notice.clear();
        if (id == open_id) {
            state.open(invoked->args.text.value_or(""));
        } else if (!state.doc) {
            continue;
        } else if (id == object_id) {
            auto index = std::size_t{0};
            if (std::sscanf(row.c_str(), "object:%zu", &index) == 1) {
                state.object = index;
                state.anchor.reset();
                state.reload();
            }
        } else if (id == tool_id) {
            const auto found = std::ranges::find(tools, row, &tool::id);
            if (found != tools.end()) {
                state.current = &*found;
                state.anchor.reset();
            }
        } else if (id == symbol_id) {
            state.symbol = row.substr(std::string_view("symbol:").size());
        } else if (id == paint_id) {
            if (const auto at = square_of(row)) { state.paint(*at); }
        } else if (id == undo_id || id == redo_id) {
            const auto moved = id == undo_id ? state.doc->undo() : state.doc->redo();
            if (moved) { state.reload(); }
        } else if (id == save_id) {
            const auto saved = state.doc->save();
            if (!saved) {
                state.notice =
                    saved.error() == save_error::changed_on_disk
                        ? _("The file changed on disk since it was opened; nothing was saved.")
                        : _("The file could not be written.");
            }
        }
    }
}

} // namespace editor
