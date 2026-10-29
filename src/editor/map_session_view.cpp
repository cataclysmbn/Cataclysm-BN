#include "color.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/editor_access.h"
#include "editor/history_label.h"
#include "editor/map_session.h"
#include "editor/mapgen_legend.h"
#include "editor/mapgen_legend_edit.h"
#include "editor/mapgen_legend_text.h"
#include "editor/mapgen_levels.h"
#include "editor/mapgen_placement.h"
#include "editor/mapgen_preview.h"
#include "editor/mapgen_stroke.h"
#include "editor/recent_documents.h"
#include "editor/row_id.h"
#include "editor/session_file.h"
#include "editor/session_page.h"
#include "editor/special_grid.h"
#include "editor/workspace.h"
#include "engine_client_component.h"
#include "engine_client_world.h"
#include "game.h"
#include "item.h"
#include "map/mapdata.h"
#include "map_perception.h"
#include "output.h"
#include "overmap/omdata.h"
#include "overmap/overmap_connection.h"
#include "overmap/overmapbuffer.h"
#include "path_info.h"
#include "regional_settings.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor::map_editor {
namespace {

namespace ec = engine_client;

/// What a square holds from the bottom up, as the map shows it: terrain, furniture, trap, top item.
auto square_looks(const preview_square& square) -> std::vector<engine_client::look> {
    namespace world = engine_client::world;
    auto looks = std::vector<engine_client::look>{};
    if (const auto ter = ter_str_id(square.ter); ter.is_valid()) {
        looks.push_back(world::terrain_appearance(ter.id()));
    }
    if (const auto furn = furn_str_id(square.furn); furn.is_valid() && !furn.is_null()) {
        looks.push_back(world::furniture_appearance(furn.id()));
    }
    if (const auto tr = trap_str_id(square.trap); tr.is_valid() && !tr.is_null()) {
        looks.push_back(world::trap_appearance(tr.obj()));
    }
    if (!square.items.empty()) {
        if (const auto type = itype_id(square.items.back()); type.is_valid()) {
            looks.push_back(world::item_appearance(*item::spawn_temporary(type, calendar::turn)));
        }
    }
    return looks;
}

/// The values a parameter can be pinned to, after the choice of none.
auto pin_options(const std::vector<std::string>& values) -> std::vector<ec::form_option> {
    auto options = std::vector{ec::form_option{.id = "", .label = _("Any")}};
    std::ranges::copy(
        values | std::views::transform([](const auto& value) {
            return ec::form_option{.id = value, .label = value};
        }),
        std::back_inserter(options));
    return options;
}

/// An overmap connection as a player knows it: by the terrain it lays; else its id.
auto connection_name(const std::string& id) -> std::string {
    const auto connection = overmap_connection_id(id);
    return connection.is_valid() && connection->default_terrain.is_valid()
             ? _(connection->default_terrain->name)
             : id;
}

/// An overmap terrain as the game names it, with the id that tells it from others of its name.
auto overmap_terrain_name(const std::string& id) -> std::string {
    const auto terrain = oter_str_id(id);
    return terrain.is_valid() ? string_format("%s (%s)", terrain->get_name(), id) : id;
}

/// The overmap connections a special can join, each named by the terrain it lays.
auto connection_options() -> std::vector<ec::form_option> {
    return overmap_connections::get_all() | std::views::transform([](const auto& connection) {
               return ec::form_option{
                   .id = connection.id.str(), .label = connection_name(connection.id.str())};
           })
         | std::ranges::to<std::vector>();
}

/// What the game generates from the entry, a square per cell: its furniture, else its terrain.
auto preview_grid(const mapgen_preview& shown) -> ec::component {
    auto cells = std::vector<ec::grid_cell>{};
    auto palette = ec::look_palette{};
    for (auto y = 0; y < shown.size.y(); ++y) {
        for (auto x = 0; x < shown.size.x(); ++x) {
            const auto& square = shown.squares[y * shown.size.x() + x];
            const auto& ter = ter_str_id(square.ter).obj();
            const auto furn = furn_str_id(square.furn);
            const auto& top =
                furn.is_null()
                    ? static_cast<const map_data_common_t&>(ter)
                    : static_cast<const map_data_common_t&>(furn.obj());
            cells.push_back(
                {.at = {x, y},
                 .id = point_row("preview", {x, y}),
                 .glyph = string_from_int(top.symbol()),
                 .label = furn.is_null() ? ter.name() : ter.name() + ", " + furn.obj().name(),
                 .color = string_from_color(top.color()),
                 .looks = palette.indexes(square_looks(square))});
        }
    }
    return {
        .id = "editor:preview",
        .body = ec::grid_body{
            .cols = shown.size.x(),
            .rows = shown.size.y(),
            .cells = std::move(cells),
            .looks = std::move(palette.looks)}};
}

/// The first id a legend value names: the id itself, or the first of a list or weighted list; none
/// for what only generation decides, such as a parameter.
auto first_id(const std::string& value) -> std::optional<std::string> {
    auto json = nlohmann::ordered_json::parse(value, nullptr, false);
    while (json.is_array() && !json.empty()) { json = json.front(); }
    return json.is_string() ? std::optional(json.get<std::string>()) : std::nullopt;
}

/// The likeliest terrain or furniture a region id such as `t_region_groundcover_urban` stands for
/// in the default region; any other id itself.
template <typename Id>
auto regional(const Id& id, const std::map<Id, weighted_int_list<Id>>& choices) -> Id {
    const auto found = choices.find(id);
    if (found == choices.end() || found->second.empty()) { return id; }
    return std::ranges::max(found->second, {}, &weighted_object<int, Id>::weight).obj;
}

/// The id a symbol's legend names on `layer`, if any.
auto named_on(const legend_symbol& entry, const char* layer) -> std::optional<std::string> {
    const auto line = std::ranges::find(entry.lines, layer, &legend_line::layer);
    return line == entry.lines.end() ? std::nullopt : first_id(line->value);
}

/// The terrain the entry `object` puts where nothing maps one, if any.
auto fill_terrain(const nlohmann::ordered_json& object) -> std::optional<std::string> {
    return object.contains("object") && object["object"].contains("fill_ter")
                && object["object"]["fill_ter"].is_string()
             ? std::optional(object["object"]["fill_ter"].get<std::string>())
             : std::nullopt;
}

/// The terrain a symbol of the entry `object` puts on a square: its own, else the entry's fill.
auto symbol_terrain(const nlohmann::ordered_json& object, const legend_symbol& entry)
    -> ter_str_id {
    return ter_str_id(
        named_on(entry, "terrain").or_else([&] { return fill_terrain(object); }).value_or(""));
}

/// What each symbol of the entry `object` puts on a square from the bottom up, as the map shows
/// it: its terrain, then its furniture and trap.
auto symbol_looks(const nlohmann::ordered_json& object, const std::vector<legend_symbol>& legend)
    -> std::map<std::string, std::vector<engine_client::look>> {
    namespace world = engine_client::world;
    const auto region = region_settings_map.find("default");
    const auto resolve = [&](const auto& id) {
        return region == region_settings_map.end() ? id : regional(id, [&] {
            if constexpr (std::is_same_v<std::decay_t<decltype(id)>, ter_id>) {
                return region->second.region_terrain_and_furniture.terrain;
            } else {
                return region->second.region_terrain_and_furniture.furniture;
            }
        }());
    };
    auto result = std::map<std::string, std::vector<engine_client::look>>{};
    for (const auto& entry : legend) {
        const auto named = [&](const char* layer) { return named_on(entry, layer); };
        auto looks = std::vector<engine_client::look>{};
        if (const auto ter = symbol_terrain(object, entry); ter.is_valid() && !ter.is_null()) {
            looks.push_back(world::terrain_appearance(resolve(ter.id())));
        }
        if (const auto furn = furn_str_id(named("furniture").value_or(""));
            furn.is_valid() && !furn.is_null()) {
            looks.push_back(world::furniture_appearance(resolve(furn.id())));
        }
        if (const auto tr = trap_str_id(named("traps").value_or(""));
            tr.is_valid() && !tr.is_null()) {
            looks.push_back(world::trap_appearance(tr.obj()));
        }
        // What spawns there goes over the square, as the map draws it.
        for (const auto& line : entry.lines) {
            if (std::ranges::contains(std::array{"terrain", "furniture", "traps"}, line.layer)) {
                continue;
            }
            const auto id =
                first_named(line.layer, nlohmann::ordered_json::parse(line.value, nullptr, false));
            if (auto look = id ? layer_look(line.layer, *id) : std::nullopt) {
                looks.push_back(std::move(*look));
            }
        }
        result[entry.symbol] = std::move(looks);
    }
    return result;
}

/// What each symbol of the entry `object` places on a square from the bottom up, as its cell names
/// it: terrain and furniture by name, anything else after its layer, with its chance; the entry's
/// fill terrain under a symbol that maps none.
auto symbol_labels(const nlohmann::ordered_json& object, const std::vector<legend_symbol>& legend)
    -> std::map<std::string, std::string> {
    const auto fill = fill_terrain(object);
    auto result = std::map<std::string, std::string>{};
    for (const auto& entry : legend) {
        auto parts = std::vector<std::string>{};
        if (fill
            && !std::ranges::contains(entry.lines, std::string("terrain"), &legend_line::layer)) {
            parts.push_back(legend_value("terrain", nlohmann::ordered_json(*fill).dump()).names);
        }
        for (const auto& line : entry.lines) {
            const auto names = legend_value(line.layer, line.value).names;
            const auto value = nlohmann::ordered_json::parse(line.value, nullptr, false);
            const auto named =
                line.layer == "terrain" || line.layer == "furniture"
                    ? names
                    //~ %1$s is a legend layer, such as Items; %2$s is what it places there.
                    : string_format(_("%1$s: %2$s"), legend_layer_label(line.layer), names);
            parts.push_back(
                value.is_object() && value.contains("chance") && value["chance"].is_number_integer()
                    //~ %1$s is what a symbol places; %2$d is the chance it is placed, in percent.
                    ? string_format(_("%1$s %2$d%%"), named, value["chance"].get<int>())
                    : named);
        }
        result[entry.symbol] = join(parts, "; ");
    }
    return result;
}

/// Whether the square is inside the rectangle the select tool marked.
auto in_selection(const session& state, const point& at) -> bool {
    return state.selected && at.x >= std::min(state.selected->from.x, state.selected->to.x)
        && at.x <= std::max(state.selected->from.x, state.selected->to.x)
        && at.y >= std::min(state.selected->from.y, state.selected->to.y)
        && at.y <= std::max(state.selected->from.y, state.selected->to.y);
}

/// What the level below shows through the canvas entry's open air, by square.
struct faded_square {
    std::string glyph;
    std::vector<int> looks;
};

auto faded_squares(const session& state, ec::look_palette& palette)
    -> std::map<point, faded_square> {
    const auto& objects = state.doc->objects();
    const auto& object = objects[state.object].value;
    const auto open_air =
        state.legend | std::views::filter([&](const legend_symbol& entry) {
            // Open air that places anything else shows what it places.
            const auto ter = symbol_terrain(object, entry);
            return ter.is_valid() && ter.obj().has_flag(TFLAG_NO_FLOOR)
                && std::ranges::all_of(entry.lines, [](const legend_line& line) {
                       return line.layer == "terrain";
                   });
        })
        | std::views::transform(&legend_symbol::symbol) | std::ranges::to<std::set>();
    const auto grid = symbols_of(*state.doc, state.object);
    auto result = std::map<point, faded_square>{};
    for (const auto& square : state.below) {
        const auto& other = objects[square.other.entry].value;
        const auto looks = symbol_looks(other, state.other_legends.at(square.other.entry));
        const auto other_grid = symbols_of(*state.doc, square.other.entry);
        for (auto y = 0; y < SEEY * 2; ++y) {
            for (auto x = 0; x < SEEX * 2; ++x) {
                const auto at = square.origin + point(x, y);
                if (!open_air.contains(symbol_at(grid, at))) { continue; }
                const auto glyph = symbol_at(other_grid, square.other.origin + point(x, y));
                if (glyph.empty()) { continue; }
                const auto found = looks.find(glyph);
                result[at] = {
                    .glyph = glyph,
                    .looks =
                        found == looks.end() ? std::vector<int>{} : palette.indexes(found->second)};
            }
        }
    }
    return result;
}

/// The most legend rows shown at once.
constexpr auto legend_rows_shown = std::size_t{6};

/// How far into the overmap squares beside the canvas entry the canvas shows them.
constexpr auto neighbour_strip = 4;

/// The edges of the overmap squares beside the canvas entry, of `size`, that other entries make,
/// faded, by square of its rows; each opens the entry that makes it.
auto neighbour_cells(const session& state, const point& size, ec::look_palette& palette)
    -> std::vector<ec::grid_cell> {
    const auto& objects = state.doc->objects();
    const auto shown = [&](const point& at) {
        return at.x >= -neighbour_strip && at.y >= -neighbour_strip
            && at.x < size.x + neighbour_strip && at.y < size.y + neighbour_strip;
    };
    auto result = std::vector<ec::grid_cell>{};
    for (const auto& square : state.beside) {
        const auto looks = symbol_looks(
            objects[square.other.entry].value, state.other_legends.at(square.other.entry));
        const auto other_grid = symbols_of(*state.doc, square.other.entry);
        for (auto y = 0; y < SEEY * 2; ++y) {
            for (auto x = 0; x < SEEX * 2; ++x) {
                const auto at = square.origin + point(x, y);
                const auto glyph = symbol_at(other_grid, square.other.origin + point(x, y));
                if (!shown(at) || glyph.empty()) { continue; }
                const auto found = looks.find(glyph);
                result.push_back(
                    {.at = {at.x, at.y},
                     .id = index_row("object", square.other.entry),
                     .glyph = glyph,
                     .affordance = object_id,
                     .looks =
                         found == looks.end() ? std::vector<int>{} : palette.indexes(found->second),
                     .faded = true});
            }
        }
    }
    return result;
}

auto canvas(const session& state) -> ec::component {
    const auto& object = state.doc->objects()[state.object].value;
    const auto grid = symbols_of(*state.doc, state.object);
    auto palette = ec::look_palette{};
    const auto looks =
        symbol_looks(object, state.legend) | std::views::transform([&](const auto& entry) {
            return std::pair{entry.first, palette.indexes(entry.second)};
        })
        | std::ranges::to<std::map>();
    const auto faded = faded_squares(state, palette);
    const auto labels = symbol_labels(object, state.legend);
    const auto cursor = state.anchor ? state.anchor : state.focus;
    const auto cols = std::ranges::fold_left(
        grid | std::views::transform([](const auto& row) { return static_cast<int>(row.size()); }),
        0, [](const int a, const int b) { return std::max(a, b); });
    const auto rows = static_cast<int>(grid.size());
    auto beside = neighbour_cells(state, point(cols, rows), palette);
    // The entry moves a whole overmap square for squares drawn beside it west or north, so its
    // outline stays where the grid's falls.
    const auto shift = point(
        std::ranges::any_of(beside, [](const auto& cell) { return cell.at.x < 0; }) ? SEEX * 2 : 0,
        std::ranges::any_of(beside, [](const auto& cell) { return cell.at.y < 0; }) ? SEEY * 2 : 0);
    auto cells = std::vector<ec::grid_cell>{};
    for (auto y = 0; y < rows; ++y) {
        for (auto x = 0; x < static_cast<int>(grid[y].size()); ++x) {
            const auto at = point(x, y);
            const auto under = faded.find(at);
            cells.push_back(
                {.at = {x + shift.x, y + shift.y},
                 .id = point_row("square", at),
                 .glyph = under != faded.end() ? under->second.glyph : grid[y][x],
                 .label = under == faded.end() && labels.contains(grid[y][x])
                            ? labels.at(grid[y][x])
                            : std::string{},
                 .affordance = paint_id,
                 .selected = state.anchor == at || in_selection(state, at),
                 .looks = under != faded.end() ? under->second.looks
                        : looks.contains(grid[y][x])
                            ? looks.at(grid[y][x])
                            : std::vector<int>{},
                 .faded = under != faded.end()});
        }
    }
    auto width = shift.x + cols;
    auto height = shift.y + rows;
    const auto opens = !beside.empty();
    for (auto& cell : beside) {
        cell.at = {cell.at.x + shift.x, cell.at.y + shift.y};
        width = std::max(width, cell.at.x + 1);
        height = std::max(height, cell.at.y + 1);
        cells.push_back(std::move(cell));
    }
    const auto regions =
        placements_of(*state.doc, state.object)
        | std::views::transform([&](const placement& entry) {
              return ec::grid_region{
                  .id = placement_row(entry),
                  .from = {entry.x.min + shift.x, entry.y.min + shift.y},
                  .to = {entry.x.max + shift.x, entry.y.max + shift.y},
                  .label = entry.kind,
                  .selected = state.picked && placement_row(*state.picked) == placement_row(entry)};
          })
        | std::ranges::to<std::vector>();
    const auto drags = state.current->region == region_use::placements;
    return {
        .id = canvas_id,
        // The drag runs the placement's move; it is no command of the canvas to list.
        .affordances =
            opens ? std::vector<std::string>{paint_id, object_id}
                  : std::vector<std::string>{paint_id},
        .body = ec::grid_body{
            .cols = width,
            .rows = height,
            .cursor =
                cursor ? std::optional(ec::grid_point{cursor->x + shift.x, cursor->y + shift.y})
                       : std::nullopt,
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas",
            .drag = state.current->spans ? "span"
                  : state.current->kind == shape_kind::point
                      ? "trail"
                      : "",
            .drag_begin = state.current->kind == shape_kind::point ? stroke_begin_id : "",
            .drag_end = state.current->kind == shape_kind::point ? stroke_end_id : "",
            .block = object.contains("om_terrain")
                       ? ec::grid_point{SEEX * 2, SEEY * 2}
                       : ec::grid_point{},
            .regions = regions,
            .region_drag = drags ? placement_span_id : ""}};
}

/// What a placement places, a `key: value` for each member: a string as it reads, anything else as
/// JSON.
auto placement_text(const nlohmann::ordered_json& what) -> std::string {
    if (!what.is_object()) { return what.dump(); }
    return what.items() | std::views::transform([](const auto& member) {
               const auto& value = member.value();
               return member.key() + ": "
                    + (value.is_string() ? value.template get<std::string>() : value.dump());
           })
         | std::views::join_with(std::string{", "}) | std::ranges::to<std::string>();
}

/// A row per placement: where it lands, and what it places.
auto placement_rows(const session& state) -> std::vector<ec::row> {
    return placements_of(*state.doc, state.object)
         | std::views::transform([](const placement& entry) {
               auto what = entry.value;
               what.erase("x");
               what.erase("y");
               return ec::row{
                   .id = placement_row(entry),
                   .label = entry.kind,
                   .description = string_format("/object/%s/%d", entry.kind, entry.index),
                   .columns =
                       {{.key = "x", .label = "x", .value = range_text(entry.x)},
                        {.key = "y", .label = "y", .value = range_text(entry.y)},
                        {.key = "value", .label = _("Places"), .value = placement_text(what)}}};
           })
         | std::ranges::to<std::vector>();
}

/// What each layer of the symbol places by name, and the ids it names, in the order the loader
/// applies them; the entry's fill terrain for a symbol the rows use that nothing maps.
auto symbol_columns(const nlohmann::ordered_json& object, const legend_symbol& entry)
    -> std::vector<game_client::interaction_column> {
    auto columns = std::vector<game_client::interaction_column>{};
    auto ids = std::vector<std::string>{};
    const auto fill = fill_terrain(object);
    const auto lines =
        entry.used && entry.lines.empty() && fill
            ? std::vector{legend_line{
                  .layer = "terrain", .value = nlohmann::ordered_json(*fill).dump()}}
            : entry.lines;
    for (const auto& line : lines) {
        const auto read = legend_value(line.layer, line.value);
        const auto found =
            std::ranges::find(columns, line.layer, &game_client::interaction_column::key);
        if (found != columns.end()) {
            found->value += ", " + read.names;
        } else {
            columns.push_back(
                {.key = line.layer, .label = legend_layer_label(line.layer), .value = read.names});
        }
        if (!read.ids.empty()) { ids.push_back(read.ids); }
    }
    if (!ids.empty()) {
        columns.push_back({.key = "id", .label = _("ID"), .value = join(ids, ", ")});
    }
    return columns;
}

/// A symbol as the status line names it: its mark and what it places on top.
auto symbol_summary(const session& state, const std::string& symbol) -> std::string {
    const auto& object = state.doc->objects()[state.object].value;
    const auto entry = std::ranges::find(state.legend, symbol, &legend_symbol::symbol);
    if (state.held && state.held->symbol == symbol && entry == state.legend.end()) {
        return shown_symbol(symbol) + " " + state.held->name;
    }
    if (entry == state.legend.end()) { return shown_symbol(symbol); }
    const auto columns = symbol_columns(object, *entry);
    const auto top = std::ranges::find_if(columns, [](const auto& column) {
        return column.key == "furniture";
    });
    const auto shown =
        top != columns.end()
            ? top
            : std::ranges::
                  find(columns, std::string("terrain"), &game_client::interaction_column::key);
    return shown == columns.end() ? shown_symbol(symbol) : shown_symbol(symbol) + " " + shown->value;
}

/// `rows` with the same columns each, in the order that keeps what matters most when room runs
/// short: the flag, what each layer places in the loader's order, then the ids.
auto uniform_columns(std::vector<ec::row> rows) -> std::vector<ec::row> {
    using column = game_client::interaction_column;
    auto keys = std::vector<std::string>{};
    for (const auto& row : rows) {
        for (const auto& entry : row.columns) {
            if (!std::ranges::contains(keys, entry.key)) { keys.push_back(entry.key); }
        }
    }
    const auto rank = [](const std::string& key) {
        if (key == "use") { return -1; }
        if (key == "id") { return static_cast<int>(legend_layers.size()) + 1; }
        return static_cast<int>(std::ranges::find(legend_layers, key) - legend_layers.begin());
    };
    std::ranges::stable_sort(keys, {}, rank);
    for (auto& row : rows) {
        row.columns =
            keys | std::views::transform([&](const std::string& key) {
                const auto found = std::ranges::find(row.columns, key, &column::key);
                if (found != row.columns.end()) { return *found; }
                return column{
                    .key = key,
                    .label = key == "use" ? _("Use")
                           : key == "id"  ? _("ID")
                                          : legend_layer_label(key)};
            })
            | std::ranges::to<std::vector>();
    }
    return rows;
}

auto legend_rows(const session& state) -> std::vector<ec::row> {
    const auto& object = state.doc->objects()[state.object].value;
    const auto looks = symbol_looks(object, state.legend);
    const auto fill = fill_terrain(object);
    return uniform_columns(
        state.legend | std::views::transform([&](const legend_symbol& entry) {
            auto columns = symbol_columns(object, entry);
            // A symbol the rows never use, or one they use that nothing maps, is flagged: the
            // loader fills the latter with the entry's fill terrain, and a chunk or update mapgen
            // leaves the map under it as it is.
            if (state.on_mapgen() && (!entry.used || entry.lines.empty())) {
                columns.push_back(
                    {.key = "use",
                     .label = _("Use"),
                     .value =
                         !entry.used ? _("Unused")
                         : fill      ? _("Fill")
                         : state.square_refusal()
                             ? _("Keeps the map")
                             : _("Undefined")});
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
                .label = shown_symbol(entry.symbol),
                .description =
                    sources.empty() ? std::string{}
                                    : string_format(_("From %s"), enumerate_as_string(sources)),
                .selected = entry.symbol == state.symbol,
                .columns = std::move(columns),
                .look = looks.at(entry.symbol).empty()
                          ? std::nullopt
                          : std::optional(looks.at(entry.symbol).back())};
        })
        | std::ranges::to<std::vector>());
}

/// Begin and End of a stroke, while the canvas drags one.
auto stroke_affordances(std::vector<ec::affordance>& affordances, const bool drags) -> void {
    if (!drags) { return; }
    affordances.push_back(
        ec::invoke_affordance({.id = stroke_begin_id, .label = _("Begin stroke")}));
    affordances.push_back(ec::invoke_affordance({.id = stroke_end_id, .label = _("End stroke")}));
}

/// The argument of an affordance that takes a row of .
auto row_of(const char* const component) -> ec::arg_shape {
    return ec::arg_shape{.row = ec::row_shape{.component_id = component}};
}

/// The stats of the document panel: the drawing state only on a mapgen.
auto document_stats(const session& state) -> std::vector<ec::panel_stat> {
    const auto& doc = *state.doc;
    auto stats = std::vector<ec::panel_stat>{
        {.key = "saved",
         .label = _("Saved"),
         .value = doc.dirty() ? _("No") : _("Yes"),
         .tone = doc.dirty() ? "bad" : "good"},
    };
    // A palette has no rows to draw on, and no drawing state.
    if (state.on_mapgen()) {
        if (!state.symbol.empty()) {
            stats.push_back(
                {.key = "symbol",
                 .label = _("Symbol"),
                 .value = symbol_summary(state, state.symbol)});
        }
        if (!state.room_fill.empty()) {
            stats.push_back(
                {.key = "inside",
                 .label = _("Inside"),
                 .value = symbol_summary(state, state.room_fill)});
        }
        stats.push_back(
            {.key = "brush", .label = _("Brush"), .value = std::to_string(state.brush)});
        stats.push_back(
            {.key = "mirror_x",
             .label = _("Mirror across"),
             .value = state.mirror_x ? _("On") : _("Off")});
        if (!state.clip.empty()) {
            stats.push_back(
                {.key = "clipboard",
                 .label = _("Clipboard"),
                 .value = string_format("%zux%zu", state.clip.front().size(), state.clip.size())});
        }
        stats.push_back(
            {.key = "mirror_y",
             .label = _("Mirror down"),
             .value = state.mirror_y ? _("On") : _("Off")});
        stats.push_back(
            {.key = "apply_on_save",
             .label = _("Apply on save"),
             .value = state.apply_on_save ? _("On") : _("Off")});
        if (state.level) {
            stats.push_back(
                {.key = "level", .label = _("Level"), .value = std::to_string(*state.level)});
        }
        if (state.seed) {
            stats.push_back(
                {.key = "seed", .label = _("Seed"), .value = std::to_string(*state.seed)});
        }
    }
    if (state.on_special()) {
        stats.push_back(
            {.key = "level", .label = _("Level"), .value = std::to_string(state.special_level)});
        if (!state.special_terrain.empty()) {
            stats.push_back(
                {.key = "terrain",
                 .label = _("Terrain"),
                 .value = overmap_terrain_name(state.special_terrain)});
        }
    }
    // The stamp stays in the world whichever entry is open.
    if (state.stamped) {
        stats.push_back({.key = "stamped", .label = _("Stamped"), .value = _("Yes")});
    }
    return stats;
}

/// The forms that move the picked placement and add one.
auto placement_forms(
    const session& state, std::vector<ec::component>& parts,
    std::vector<ec::affordance>& affordances) -> void {
    if (state.picked) {
        parts.push_back(ec::component{
            .id = move_id,
            .affordances = {move_set_id, move_apply_id},
            .body = ec::form_body{
                .fields = {{.id = "x", .label = "x", .value = state.move_x},
                           {.id = "y", .label = "y", .value = state.move_y}},
                .submit = move_apply_id}});
        affordances.push_back(ec::invoke_affordance(
            {.id = move_set_id,
             .label = _("Value"),
             .args = {.row = ec::row_shape{.component_id = move_id}, .text = ec::text_shape{}}}));
        affordances.push_back(ec::invoke_affordance({.id = move_apply_id, .label = _("Move")}));
        affordances.push_back(
            ec::invoke_affordance({.id = placement_delete_id, .label = _("Delete placement")}));
    }
    parts.push_back(ec::component{
        .id = place_id,
        .affordances = {place_set_id, place_apply_id},
        .body = ec::form_body{
            .fields =
                {{.id = "kind",
                  .label = _("Kind"),
                  .value = state.place_kind,
                  .type = "select",
                  .options =
                      placement_kinds | std::views::transform([](const std::string_view kind) {
                          return ec::
                              form_option{.id = std::string(kind), .label = std::string(kind)};
                      })
                      | std::ranges::to<std::vector>()},
                 {.id = "x", .label = "x", .value = state.place_x},
                 {.id = "y", .label = "y", .value = state.place_y},
                 {.id = "value", .label = _("Value"), .value = state.place_value, .multiline = true}},
            .submit = place_apply_id}});
    affordances.push_back(ec::invoke_affordance(
        {.id = place_set_id,
         .label = _("Value"),
         .args = {.row = ec::row_shape{.component_id = place_id}, .text = ec::text_shape{}}}));
    affordances.push_back(
        ec::invoke_affordance({.id = place_apply_id, .label = _("Add placement")}));
}

/// The legend form and the catalog that fills it.
/// The catalog's page of `found`, with the affordances that filter, page and pick it.
auto catalog_list(
    const session& state, const catalog_page& found, std::vector<ec::affordance>& affordances)
    -> ec::component {
    affordances.push_back(ec::invoke_affordance(
        {.id = catalog_filter_id, .label = _("Filter"), .args = {.text = ec::text_shape{}}}));
    affordances.push_back(page_affordance(
        {.id = catalog_page_id,
         .label = _("Page"),
         .total = found.total,
         .page = state.catalog_page}));
    // A click takes an entry in hand, as Factorio's palettes do.
    affordances.push_back(ec::invoke_affordance(
        {.id = catalog_pick_id, .label = _("Use"), .args = row_of(catalog_id), .role = "select"}));
    const auto kind = state.on_special() ? catalog_kind::overmap_terrain : state.catalog->kind;
    const auto hand = state.on_special() ? std::nullopt : state.in_hand();
    return ec::list(
        {.id = catalog_id,
         .rows = found.entries | std::views::transform([&](const catalog_entry& entry) {
                     const auto held =
                         state.on_special()
                             ? entry.id == state.special_terrain
                             : hand && hand->layer == state.catalog->layer
                                   && hand->value == mapping_json_of(*state.catalog, entry.id);
                     return ec::row{
                         .id = "entry:" + entry.id,
                         .label = catalog_row_label(entry),
                         .selected = held,
                         .columns = {{.key = "id", .label = _("ID"), .value = entry.id},
                                     {.key = "mod", .label = _("Mod"), .value = entry.mod}},
                         .look = catalog_look(kind, entry.id)};
                 })
               | std::ranges::to<std::vector>(),
         .total = found.total,
         .offset = state.catalog_page * page_size,
         .selection = "single",
         .activate = catalog_pick_id,
         .filterable = true,
         .filter = state.catalog_filter,
         .search = catalog_filter_id,
         .page = catalog_page_id,
         .empty =
             state.catalog_filter.empty()
                 ? _("The catalog has nothing of this kind.")
                 //~ %s is the text the player searched the catalog for.
                 : string_format(_("Nothing in the catalog matches “%s”."), state.catalog_filter)});
}

auto mapping_parts(
    const session& state, std::vector<ec::component>& parts,
    std::vector<ec::affordance>& affordances) -> void {
    parts.push_back(ec::component{
        .id = mapping_id,
        .affordances = {mapping_set_id, mapping_apply_id},
        .body = ec::form_body{
            .fields =
                {{.id = "symbol",
                  .label = _("Symbol"),
                  .value = state.mapping_symbol,
                  .max_length = 1},
                 {.id = "layer",
                  .label = _("Layer"),
                  .value = state.mapping_layer,
                  .type = "select",
                  .options =
                      legend_layers | std::views::transform([](const char* const layer) {
                          return ec::form_option{.id = layer, .label = legend_layer_label(layer)};
                      })
                      | std::ranges::to<std::vector>()},
                 {.id = "value", .label = _("Places"), .value = state.mapping_value}},
            .submit = mapping_apply_id}});
    const auto found = state.catalog_entries();
    parts.push_back(ec::tabs(
        {.id = catalog_kinds_id,
         .tabs = catalog_uses | std::views::transform([&](const catalog_use& use) {
                     return ec::tab_entry{
                         .id = std::string("catalog:") + use.id,
                         .label = catalog_label(use),
                         .selected = &use == state.catalog};
                 })
               | std::ranges::to<std::vector>(),
         .select = catalog_kind_id,
         .child = catalog_list(state, found, affordances)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = catalog_kind_id, .label = _("Kind"), .args = row_of(catalog_kinds_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = mapping_set_id,
         .label = _("Value"),
         .args = {.row = ec::row_shape{.component_id = mapping_id}, .text = ec::text_shape{}}}));
    affordances.push_back(
        ec::invoke_affordance({.id = mapping_apply_id, .label = _("Set mapping")}));
}

/// What puts the entry in the game: apply, stamp, place, test and the preview with its controls.
auto game_parts(
    const session& state, std::vector<ec::component>& parts,
    std::vector<ec::affordance>& affordances) -> void {
    if (state.on_special()) {
        affordances.push_back(ec::invoke_affordance(
            {.id = place_special_id,
             .label = _("Place in world"),
             .description = _("Places the special on land near you that is not generated yet."),
             .keys = {"F6"}}));
        affordances.push_back(ec::invoke_affordance(
            {.id = test_id,
             .label = _("Test"),
             .description = _("Takes you to where the special was placed, to play there."),
             .keys = {"F5"},
             .enabled = state.placed_here().has_value(),
             .denial = state.placed_here() ? "" : _("Place the special in the world first.")}));
    }
    if (state.on_mapgen()) {
        const auto refused = state.square_refusal();
        const auto squared = !refused.has_value();
        affordances.push_back(ec::invoke_affordance(
            {.id = apply_id,
             .label = _("Apply to game"),
             .description = _(
                 "The game makes this terrain's new squares with this entry from now on. "
                 "Squares already made and the file stay as they are."),
             .keys = {"F7"},
             .enabled = !state.on_update(),
             .denial = state.on_update() ? *refused : ""}));
        affordances.push_back(ec::invoke_affordance(
            {.id = apply_on_save_id,
             .label = _("Apply on save"),
             .description = _(
                 "Switches whether each save of the file also applies the entry to "
                 "the game, as Apply to game does."),
             .enabled = !state.on_update(),
             .denial = state.on_update() ? *refused : ""}));
        affordances.push_back(ec::invoke_affordance(
            {.id = stamp_id,
             .label = _("Stamp"),
             .description = _(
                 "Generates the entry now over the overmap square you stand on, "
                 "replacing what is there. Take back stamp undoes it."),
             .keys = {"F6"},
             .enabled = squared,
             .denial = refused.value_or("")}));
        // In the editor's own world testing stamps first; in a player's, only what was stamped.
        const auto testable = squared && (in_editor_game() || state.stamped.has_value());
        affordances.push_back(ec::invoke_affordance(
            {.id = test_id,
             .label = _("Test"),
             .description = _(
                 "Puts you in the middle of the stamped entry to play there; in the "
                 "editor's own world it stamps the entry first."),
             .keys = {"F5"},
             .enabled = testable,
             .denial = testable ? ""
                     : refused  ? *refused
                                : _("Stamp the entry first: Test plays what was stamped.")}));
        affordances.push_back(ec::invoke_affordance(
            {.id = preview_id,
             .label = _("Preview"),
             .args = {.count = ec::count_shape{.min = 1,
                                               .max = std::numeric_limits<unsigned int>::max(),
                                               .initial = state.seed.value_or(1)}},
             .keys = {"F8"},
             .enabled = squared,
             .denial = refused.value_or("")}));
        const auto lootable = squared && state.selected.has_value();
        affordances.push_back(ec::invoke_affordance(
            {.id = loot_id,
             .label = _("What can spawn here"),
             .enabled = lootable,
             .denial = lootable ? ""
                     : refused  ? *refused
                                : _("Mark squares with the Select tool first.")}));
        if (state.seed && squared) {
            affordances.push_back(ec::invoke_affordance({.id = reroll_id, .label = _("Re-roll")}));
            affordances.push_back(ec::invoke_affordance(
                {.id = turn_id, .label = _("Turn"), .args = row_of(rotation_id)}));
            parts.push_back(ec::list(
                {.id = rotation_id,
                 .rows = preview_turns | std::views::transform([&](const preview_turn& turn) {
                             return ec::row{
                                 .id = string_format("turn:%d", turn.degrees),
                                 .label = string_format("%d°", turn.degrees),
                                 .selected = state.direction == turn.direction};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "single",
                 .activate = turn_id}));
            if (!state.parameters.empty()) {
                affordances.push_back(ec::invoke_affordance(
                    {.id = pin_set_id,
                     .label = _("Pin"),
                     .args = {.row = ec::row_shape{.component_id = pins_id},
                              .text = ec::text_shape{}}}));
                parts.push_back(ec::component{
                    .id = pins_id,
                    .affordances = {pin_set_id},
                    .body = ec::form_body{
                        .fields =
                            state.parameters | std::views::transform([&](const auto& choice) {
                                const auto pinned = state.pins.find(choice.name);
                                return ec::form_field{
                                    .id = choice.name,
                                    .label = choice.name,
                                    .value =
                                        pinned == state.pins.end() ? std::string{} : pinned->second,
                                    .type = "select",
                                    .options = pin_options(choice.values)};
                            })
                            | std::ranges::to<std::vector>()}});
            }
        }
        if (state.preview) { parts.push_back(preview_grid(*state.preview)); }
        if (state.loot) {
            parts.push_back(ec::list(
                {.id = loot_id,
                 .rows = *state.loot | std::views::transform([](const loot_share& share) {
                     const auto type = itype_id(share.item);
                     return ec::row{
                         .id = "loot:" + share.item,
                         .label = type.is_valid() ? item::nname(type) : share.item,
                         .columns =
                             {{.key = "id", .label = _("ID"), .value = share.item},
                              {.key = "chance",
                               .label = _("Chance"),
                               .value = string_format("%.0f%%", share.chance * 100)},
                              {.key = "mean",
                               .label = _("Average"),
                               .value = string_format("%.2f", share.mean)}}};
                 }) | std::ranges::to<std::vector>(),
                 .empty = _("Nothing can spawn on the marked squares.")}));
        }
    }
}

/// The form that creates a new file.
auto new_file_form(const session& state) -> ec::component {
    const auto& kind = state.made.kind;
    const auto special = kind == "special" || kind == "building";
    return ec::component{
        .id = new_id,
        .affordances = {new_set_id, new_apply_id},
        .body = ec::form_body{
            .fields =
                {{.id = "path",
                  .label = _("Path"),
                  .description = _("The new .json file, inside your mods."),
                  .value = state.made.path,
                  .required = true},
                 {.id = "om_terrain",
                  .label = kind == "palette"  ? _("Palette id")
                         : kind == "nested"   ? _("Chunk id")
                         : kind == "special"  ? _("Special id")
                         : kind == "building" ? _("Building id")
                                              : _("Overmap terrain"),
                  .description =
                      kind == "palette"  ? _("The id mapgens name the palette by.")
                      : kind == "nested" ? _("The id mapgens place the chunk by.")
                      : special          ? _("The id the game places it by.")
                                         : _("The id of the overmap terrain the mapgen makes."),
                  .value = state.made.om_terrain,
                  .required = true},
                 {.id = "fill",
                  // A special's first square places an overmap terrain.
                  .label = special ? _("Overmap terrain") : _("Terrain"),
                  .description =
                      special             ? _("The overmap terrain of its first square.")
                      : kind == "palette" ? _("The terrain its symbol . places.")
                      : kind == "nested"
                          ? _("The terrain of every square.")
                          : _("The terrain of every square, and under any symbol that "
                              "maps none."),
                  .value = state.made.fill,
                  .required = true},
                 {.id = "kind",
                  .label = _("Kind"),
                  .description = _("What the file holds."),
                  .value = state.made.kind,
                  .type = "select",
                  .options =
                      {{.id = "mapgen", .label = _("Mapgen")},
                       {.id = "nested", .label = _("Nested chunk")},
                       {.id = "palette", .label = _("Palette")},
                       {.id = "special", .label = _("Overmap special")},
                       {.id = "building", .label = _("Building")}}},
                 {.id = "size",
                  .label = _("Chunk size"),
                  .description = _("The side of a nested chunk, in squares."),
                  .value = state.made.size,
                  .type = "integer",
                  .min = 1,
                  .max = max_chunk}},
            .submit = new_apply_id}};
}

/// The page of data files shown.
auto browser_list(const session& state, const std::vector<browser_entry>& listed) -> ec::component {
    const auto shown =
        std::views::drop(listed, state.browser_page * page_size) | std::views::take(page_size);
    const auto data = std::filesystem::path(PATH_INFO::datadir());
    return ec::list(
        {.id = browser_id,
         .rows =
             shown | std::views::transform([&](const browser_entry& entry) {
                 const auto& path = entry.path;
                 return ec::row{
                     .id = "file:" + path.generic_string(),
                     .label =
                         (inside(path, PATH_INFO::datadir()) ? path.lexically_relative(data) : path)
                             .generic_string(),
                     .description = join(entry.matched, ", ")};
             })
             | std::ranges::to<std::vector>(),
         .total = listed.size(),
         .offset = state.browser_page * page_size,
         .selection = "single",
         .activate = browser_open_id,
         .filterable = true,
         .filter = state.browser_filter,
         .search = browser_filter_id,
         .page = browser_page_id,
         .empty = state.browser_filter.empty()
                    ? _("No data file is there to edit.")
                    //~ %s is the text the player searched the data files for.
                    : string_format(_("No file holds “%s”."), state.browser_filter)});
}

/// The affordances that pick what to draw with and draw; a palette only picks its symbol.
auto drawing_affordances(const session& state) -> std::vector<ec::affordance> {
    // A click on a legend row makes it the pen.
    const auto symbol = ec::invoke_affordance(
        {.id = symbol_id, .label = _("Symbol"), .args = row_of(legend_id), .role = "select"});
    if (!state.on_mapgen()) { return {symbol}; }
    auto affordances = std::vector<ec::affordance>{};
    affordances.push_back(
        ec::invoke_affordance({.id = tool_id, .label = _("Tool"), .args = row_of(tools_id)}));
    affordances.push_back(symbol);
    affordances.push_back(
        ec::invoke_affordance({.id = inside_id, .label = _("Inside"), .args = row_of(legend_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = brush_id,
         .label = _("Brush"),
         .args = {.count = ec::count_shape{.min = 1, .max = max_brush, .initial = state.brush}}}));
    affordances.push_back(ec::invoke_affordance({.id = mirror_x_id, .label = _("Mirror across")}));
    affordances.push_back(ec::invoke_affordance({.id = mirror_y_id, .label = _("Mirror down")}));
    const auto unmarked = state.selected ? "" : _("Mark squares with the Select tool first.");
    const auto unclipped = state.clip.empty() ? _("Copy or cut squares first.") : "";
    affordances.push_back(ec::invoke_affordance(
        {.id = copy_id,
         .label = _("Copy"),
         .keys = {"CTRL+C"},
         .enabled = state.selected.has_value(),
         .denial = unmarked}));
    affordances.push_back(ec::invoke_affordance(
        {.id = cut_id,
         .label = _("Cut"),
         .keys = {"CTRL+X"},
         .enabled = state.selected.has_value(),
         .denial = unmarked}));
    affordances.push_back(ec::invoke_affordance(
        {.id = rotate_id,
         .label = _("Rotate"),
         .keys = {"z"},
         .enabled = !state.clip.empty(),
         .denial = unclipped}));
    affordances.push_back(ec::invoke_affordance(
        {.id = flip_x_id,
         .label = _("Flip across"),
         .keys = {"x"},
         .enabled = !state.clip.empty(),
         .denial = unclipped}));
    affordances.push_back(ec::invoke_affordance(
        {.id = flip_y_id,
         .label = _("Flip down"),
         .keys = {"y"},
         .enabled = !state.clip.empty(),
         .denial = unclipped}));
    affordances.push_back(
        ec::invoke_affordance({.id = paint_id, .label = _("Draw"), .args = row_of(canvas_id)}));
    if (state.current->region == region_use::placements) {
        affordances.push_back(ec::invoke_affordance(
            {.id = placement_span_id,
             .label = _("Move placement"),
             .args = {.row = ec::row_shape{.component_id = canvas_id}, .text = ec::text_shape{}}}));
    }
    stroke_affordances(affordances, state.current->kind == shape_kind::point);
    affordances.push_back(ec::invoke_affordance(
        {.id = extract_palette_id,
         .label = _("Extract legend to palette"),
         .description = _(
             "Moves the entry's symbols into a new palette in this file, which the "
             "entry then uses."),
         .args = {.text = ec::text_shape{.initial = state.free_palette_id()}}}));
    return affordances;
}

auto level_affordances(const session& state) -> std::vector<ec::affordance> {
    return {
        ec::invoke_affordance(
            {.id = level_up_id,
             .label = _("Level up"),
             .keys = {"<"},
             .enabled = state.can_climb(1),
             .denial = state.can_climb(1) ? "" : _("No level above in this file.")}),
        ec::invoke_affordance(
            {.id = level_down_id,
             .label = _("Level down"),
             .keys = {">"},
             .enabled = state.can_climb(-1),
             .denial = state.can_climb(-1) ? "" : _("No level below in this file.")})};
}

/// `cell` showing where a special joins `joined`: faded, as the terrain the connection lays.
auto connection_cell(
    ec::grid_cell cell, const special_connection& joined, ec::look_palette& palette)
    -> ec::grid_cell {
    const auto id = overmap_connection_id(joined.connection);
    cell.label = string_format(_("%s connection"), connection_name(joined.connection));
    cell.glyph = "+";
    cell.faded = true;
    if (!id.is_valid() || !id->default_terrain.is_valid()) { return cell; }
    const auto& terrain = id->default_terrain.obj();
    cell.glyph = terrain.get_symbol();
    cell.color = string_from_color(terrain.color);
    cell.looks = palette.indexes(
        {{.kind = "overmap_terrain",
          .id = terrain.id.str(),
          .glyph = cell.glyph,
          .color = cell.color,
          .looks_like = terrain.looks_like}});
    return cell;
}

/// The overmap squares a special or building places on the level shown, over the rectangle of all
/// its levels and, while drawing, a square of room around it; each opens its terrain's mapgen or
/// runs the tool.
auto special_canvas(const session& state) -> ec::component {
    const auto& value = state.doc->objects()[state.object].value;
    const auto squares = special_squares(value);
    const auto connections = special_connections(value);
    const auto drawing = state.special_tool->kind.has_value() && !state.drawing_refusal();
    // Where a connection joins counts, so that it shows beside the squares.
    auto points =
        squares | std::views::transform(&special_square::at) | std::ranges::to<std::vector>();
    std::ranges::copy(
        connections | std::views::transform(&special_connection::at), std::back_inserter(points));
    if (points.empty() && !drawing) {
        return {.id = canvas_id, .body = ec::grid_body{.role = "canvas"}};
    }
    const auto xs =
        points | std::views::transform([](const auto& at) { return at.x(); })
        | std::ranges::to<std::vector>();
    const auto ys =
        points | std::views::transform([](const auto& at) { return at.y(); })
        | std::ranges::to<std::vector>();
    // While drawing, a square of room around it, as far as an overmap reaches.
    const auto room = point(drawing ? 1 : 0, drawing ? 1 : 0);
    const auto reach = OMAPX - 1;
    const auto clamp = [&](const point& at) {
        return point(std::clamp(at.x, -reach, reach), std::clamp(at.y, -reach, reach));
    };
    const auto min =
        points.empty()
            ? point_zero
            : clamp(point(std::ranges::min(xs), std::ranges::min(ys)) - room);
    const auto max =
        points.empty()
            ? point_zero
            : clamp(point(std::ranges::max(xs), std::ranges::max(ys)) + room);
    auto palette = ec::look_palette{};
    auto cells = std::vector<ec::grid_cell>{};
    for (auto y = min.y; y <= max.y; ++y) {
        for (auto x = min.x; x <= max.x; ++x) {
            const auto found = std::ranges::
                find(squares, tripoint_rel_omt(x, y, state.special_level), &special_square::at);
            const auto joined = std::ranges::find(
                connections, tripoint_rel_omt(x, y, state.special_level), &special_connection::at);
            if (found == squares.end() && joined != connections.end()) {
                cells.push_back(connection_cell(
                    {.at = {x - min.x, y - min.y},
                     .id = point_row("square", point(x, y)),
                     .affordance = drawing ? paint_id : ""},
                    *joined, palette));
                continue;
            }
            if (found == squares.end() && !drawing) { continue; }
            auto cell = ec::grid_cell{
                .at = {x - min.x, y - min.y},
                .id = point_row("square", point(x, y)),
                .glyph = found == squares.end() ? " " : "?",
                .label = found == squares.end() ? "" : found->overmap,
                .affordance = drawing ? paint_id : open_square_id};
            if (found != squares.end() && oter_str_id(found->overmap).is_valid()) {
                const auto& terrain = oter_str_id(found->overmap).obj();
                cell.glyph = terrain.get_symbol();
                // Squares of one name, as a big special has, are told apart by their ids.
                cell.label = overmap_terrain_name(found->overmap);
                cell.color = string_from_color(terrain.get_color());
                // As the overmap's tiles show it.
                auto rotation = 0;
                auto subtile = 0;
                terrain.get_rotation_and_subtile(rotation, subtile);
                const auto shape = map_perception::multitile_key(subtile);
                cell.looks = palette.indexes(
                    {{.kind = "overmap_terrain",
                      .id = terrain.get_type_id().str(),
                      .name = terrain.get_name(),
                      .glyph = cell.glyph,
                      .color = cell.color,
                      .looks_like = terrain.get_type_id()->looks_like,
                      .subtile = shape.empty() ? std::nullopt : std::optional(shape),
                      .rotation = rotation}});
            }
            cells.push_back(std::move(cell));
        }
    }
    return {
        .id = canvas_id,
        .affordances = {drawing ? paint_id : open_square_id},
        .body = ec::grid_body{
            .cols = max.x - min.x + 1,
            .rows = max.y - min.y + 1,
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas",
            .drag = drawing ? "trail" : "",
            .drag_begin = drawing ? stroke_begin_id : "",
            .drag_end = drawing ? stroke_end_id : ""}};
}

} // namespace

namespace {
/// What kind of entry `value` is, in words; empty for one the editor does not name.
auto entry_kind(const nlohmann::ordered_json& value) -> std::string {
    const auto type = value.is_object() ? value.value("type", "") : std::string{};
    if (type == "mapgen") {
        return value.contains("nested_mapgen_id") ? _("Nested chunk")
             : value.contains("update_mapgen_id")
                 ? _("Update mapgen")
                 : _("Overmap terrain mapgen");
    }
    return type == "palette"         ? _("Palette")
         : type == "overmap_special" ? _("Overmap special")
         : type == "city_building"   ? _("City building")
         : type == "overmap_terrain"
             ? _("Overmap terrain")
             : std::string{};
}

/// An entry's name and kind, which together tell entries apart.
struct named_entry {
    std::string name;
    std::string kind;
    auto operator==(const named_entry&) const -> bool = default; // *NOPAD*
};

/// A row per entry of the file, by name, saying its kind; entries of the same name and kind, such
/// as the variants the game picks among by weight, are told apart by their place among them.
auto entry_rows(const session& state, const std::vector<std::size_t>& entries)
    -> std::vector<ec::row> {
    const auto& objects = state.doc->objects();
    const auto names =
        entries | std::views::transform([&](const auto index) {
            return named_entry{
                .name = entry_name(objects[index].value), .kind = entry_kind(objects[index].value)};
        })
        | std::ranges::to<std::vector>();
    return std::views::zip(entries, names) | std::views::enumerate
         | std::views::transform([&](const auto& numbered) {
               const auto& [at, entry] = numbered;
               const auto& [index, named] = entry;
               const auto same = std::ranges::count(names, named);
               const auto place = std::ranges::count(names | std::views::take(at + 1), named);
               return ec::row{
                   .id = index_row("object", index),
                   .label = same > 1 ? string_format(_("%s (%d of %d)"), named.name, place, same)
                                     : named.name,
                   .description = named.kind,
                   .selected = index == state.object};
           })
         | std::ranges::to<std::vector>();
}

/// What each part is for, and the menu of each action that is no part's own: with nothing open,
/// the files to open are the page.
auto layout(const session& state) -> workspace_layout {
    const auto file = std::string(_("File"));
    const auto edit = std::string(_("Edit"));
    const auto view = std::string(_("View"));
    const auto world = std::string(_("World"));
    // While a document is open, the start page folds away beside it.
    const auto opened = state.doc.has_value();
    return {
        .slots =
            {{tools_id, {.slot = "canvas"}},
             {legend_id, {.slot = "palette", .label = _("Legend")}},
             // Picking from the catalog maps what it draws; the form renames a symbol by hand.
             {mapping_id, {.slot = "palette", .label = _("Mapping"), .collapsed = true}},
             {catalog_kinds_id, {.slot = "palette", .label = _("Catalog")}},
             {catalog_id, {.slot = "palette", .label = _("Catalog")}},
             {file_id, {.slot = "outline", .label = _("File path"), .collapsed = opened}},
             {objects_id, {.slot = "outline", .label = _("Entries")}},
             {placements_id, {.slot = "outline", .label = _("Placed by this entry")}},
             {terrains_id, {.slot = "outline", .label = _("Mapgens")}},
             {"editor:code_mapgens", {.slot = "outline", .label = _("Mapgens in code")}},
             {browser_id,
              {.slot = opened ? "outline" : "canvas", .label = _("Files"), .collapsed = opened}},
             {recent_id, {.slot = "canvas", .label = _("Recent")}},
             {diagnostics_id, {.slot = "log", .label = _("Problems")}},
             {history_id, {.slot = "log", .label = _("History")}},
             {"editor:notice", {.slot = "log"}},
             {"editor:status", {.slot = "log"}},
             {"editor:document", {.slot = "status"}},
             {field_id, {.slot = "inspector", .label = _("JSON value")}},
             {move_id, {.slot = "inspector", .label = _("Placement position")}},
             {place_id, {.slot = "inspector", .label = _("New placement")}},
             {pins_id, {.slot = "inspector", .label = _("Pinned choices")}},
             {rotation_id, {.slot = "inspector", .label = _("Preview facing")}},
             {connection_id, {.slot = "inspector", .label = _("Connect tool")}},
             {"editor:preview", {.slot = "inspector", .label = _("Generated map")}},
             {loot_id, {.slot = "inspector", .label = _("Spawn chances")}},
             {new_id, {.slot = "inspector", .label = _("New mapgen file"), .collapsed = opened}},
             {export_options_id, {.slot = "inspector", .label = _("Export")}},
             {import_form_id,
              {.slot = "inspector", .label = _("Stamp mapgen text"), .collapsed = opened}}},
        .groups =
            {{save_id, file},
             {save_as_id, file},
             {export_id, file},
             {export_area_id, file},
             {extract_palette_id, file},
             {undo_id, edit},
             {redo_id, edit},
             {copy_id, edit},
             {cut_id, edit},
             {rotate_id, edit},
             {flip_x_id, edit},
             {flip_y_id, edit},
             {brush_id, edit},
             {mirror_x_id, edit},
             {mirror_y_id, edit},
             {placement_delete_id, edit},
             {level_up_id, view},
             {level_down_id, view},
             {preview_id, view},
             {reroll_id, view},
             {loot_id, view},
             {apply_id, world},
             {apply_on_save_id, world},
             {stamp_id, world},
             {unstamp_id, world},
             {test_id, world},
             {place_special_id, world},
             {advance_id, world},
             {open_here_id, world},
             {open_oter_id, world},
             {vehicle_id, world}},
        .menus = {file, edit, view, world}};
}
} // namespace

auto session_view(const session& state) -> ec::screen_view {
    auto affordances = std::vector<ec::affordance>{ec::invoke_affordance(
        {.id = open_id, .label = _("Open"), .args = {.text = ec::text_shape{.field_id = path_id}}})};
    if (state.doc) {
        affordances.push_back(ec::invoke_affordance(
            {.id = save_as_id,
             .label = _("Save as"),
             .args = {.text = ec::text_shape{.field_id = path_id}}}));
    }
    auto parts = ec::children_of(ec::component{
        .id = file_id,
        .affordances =
            state.doc ? std::vector<std::string>{open_id, save_as_id}
                      : std::vector<std::string>{open_id},
        .body = ec::form_body{
            .fields = {{.id = path_id,
                        .label = _("Path"),
                        .value = state.doc ? state.doc->path().generic_string() : ""}},
            .submit = open_id}});
    affordances.push_back(ec::invoke_affordance(
        {.id = new_set_id,
         .label = _("New file"),
         .args = {.row = ec::row_shape{.component_id = new_id}, .text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance({.id = new_apply_id, .label = _("Create")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = open_oter_id,
         .label = _("Open overmap terrain"),
         .args = {.text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = open_here_id,
         .label = _("Open mapgen here"),
         .description = _("Opens the mapgen of the overmap square you stand on.")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = export_id,
         .label = state.export_area
                    ? string_format(_("Export %dx%d squares"), state.export_area->size.x(),
                                    state.export_area->size.y())
                    : _("Export mapgen here"),
         .description = _(
             "Writes the map around you, or the marked squares, as a mapgen in a "
             "new file and opens it."),
         .args = {.text = ec::text_shape{.initial = state.free_export_path()}}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = export_area_id,
         .label = _("Mark squares to export"),
         .description = _(
             "Picks a rectangle of squares on the map for Export to write instead "
             "of whole overmap squares.")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = export_set_id,
         .label = _("Value"),
         .args =
             {.row = ec::row_shape{.component_id = export_options_id}, .text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = unstamp_id,
         .label = _("Take back stamp"),
         .description = _("Puts back what the stamp replaced in the world."),
         .enabled = state.stamped.has_value(),
         .denial = state.stamped ? "" : _("Nothing is stamped.")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = advance_id,
         .label = _("Let turns pass"),
         .args = {.count = ec::count_shape{.min = 1, .max = max_advance, .initial = 60}}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = import_set_id,
         .label = _("Mapgen to stamp"),
         .args = {.row = ec::row_shape{.component_id = import_form_id}, .text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance({.id = import_id, .label = _("Stamp text")}));
    const auto listed = state.browser_entries();
    affordances.push_back(ec::invoke_affordance(
        {.id = browser_open_id, .label = _("Open"), .args = row_of(browser_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = browser_filter_id, .label = _("Filter files"), .args = {.text = ec::text_shape{}}}));
    affordances.push_back(page_affordance(
        {.id = browser_page_id,
         .label = _("Files page"),
         .total = listed.size(),
         .page = state.browser_page}));
    affordances.push_back(ec::invoke_affordance({.id = vehicle_id, .label = _("Vehicle editor")}));
    if (state.doc) {
        const auto& doc = *state.doc;
        parts.push_back(ec::panel(
            {.id = "editor:document",
             .title = shown_path(doc.path()),
             .stats = document_stats(state)}));
        const auto entries = state.entries();
        parts.push_back(ec::list(
            {.id = objects_id,
             .rows = entry_rows(state, entries),
             .selection = "single",
             .activate = object_id,
             .empty = _("The file holds no entry.")}));
        affordances.push_back(ec::invoke_affordance(
            {.id = object_id, .label = _("Edit"), .args = row_of(objects_id)}));
        std::ranges::
            move(history_affordances(doc, {.undo = undo_id, .redo = redo_id, .save = save_id}),
                 std::back_inserter(affordances));
        parts.push_back(ec::list(
            {.id = history_id,
             .rows = step_rows(document_steps(doc)),
             .selection = "single",
             .activate = jump_id,
             .empty = _("No edits yet.")}));
        affordances.push_back(ec::invoke_affordance(
            {.id = jump_id,
             .label = _("Go to step"),
             .args = row_of(history_id),
             .role = "select"}));
        if (std::ranges::contains(entries, state.object)) {
            if (state.on_mapgen()) {
                // An entry without rows, such as an update mapgen placing chunks, has no squares to
                // draw or mark; its placements are what it places.
                const auto rowless = !state.has_rows();
                parts.push_back(ec::tabs(
                    {.id = tools_id,
                     .tabs =
                         tools | std::views::transform([&](const tool& entry) {
                             const auto usable = !rowless || entry.region == region_use::placements;
                             return ec::tab_entry{
                                 .id = entry.id,
                                 .label = tool_label(entry),
                                 .selected = &entry == state.current,
                                 .enabled = usable,
                                 .denial =
                                     usable ? ""
                                            : _("The entry has no rows to draw on; edit its "
                                                "placements."),
                                 .hotkey = entry.hotkey};
                         })
                         | std::ranges::to<std::vector>(),
                     .select = tool_id,
                     .child = canvas(state)}));
            }
            if (state.on_special()) {
                parts.push_back(ec::tabs(
                    {.id = tools_id,
                     .tabs = special_tools | std::views::transform([&](const tool& entry) {
                                 return ec::tab_entry{
                                     .id = entry.id,
                                     .label = tool_label(entry),
                                     .selected = &entry == state.special_tool,
                                     .hotkey = entry.hotkey};
                             })
                           | std::ranges::to<std::vector>(),
                     .select = tool_id,
                     .child = special_canvas(state)}));
                parts.push_back(catalog_list(state, state.catalog_entries(), affordances));
                parts.push_back(ec::component{
                    .id = connection_id,
                    .affordances = {connection_set_id},
                    .body = ec::form_body{
                        .fields =
                            {{.id = "connection",
                              .label = _("Connection"),
                              .value = state.special_connection,
                              .type = "select",
                              .options = connection_options()}},
                        .submit = connection_set_id}});
                affordances.push_back(ec::invoke_affordance(
                    {.id = connection_set_id,
                     .label = _("Connection"),
                     .args = {.text = ec::text_shape{}}}));
                affordances.push_back(ec::invoke_affordance(
                    {.id = open_square_id,
                     .label = _("Open its mapgen"),
                     .args = row_of(canvas_id)}));
                affordances.push_back(ec::invoke_affordance(
                    {.id = paint_id, .label = _("Draw"), .args = row_of(canvas_id)}));
                affordances.push_back(ec::invoke_affordance(
                    {.id = tool_id, .label = _("Tool"), .args = row_of(tools_id)}));
                stroke_affordances(
                    affordances, state.special_tool->kind.has_value() && !state.drawing_refusal());
            } else if (state.on_oter()) {
                const auto mapgens = state.terrain_mapgens();
                if (!mapgens.empty()) {
                    parts.push_back(ec::list(
                        {.id = terrains_id,
                         .rows = mapgens | std::views::transform([](const std::string& name) {
                                     return ec::row{.id = "terrain:" + name, .label = name};
                                 })
                               | std::ranges::to<std::vector>(),
                         .selection = "single",
                         .activate = open_terrain_id}));
                    affordances.push_back(ec::invoke_affordance(
                        {.id = open_terrain_id,
                         .label = _("Open its mapgen"),
                         .args = row_of(terrains_id)}));
                }
            } else {

                // A long legend scrolls, so the catalog under it keeps its rows.
                parts.push_back(ec::list(
                    {.id = legend_id,
                     .rows = legend_rows(state),
                     .selection = "single",
                     .activate = symbol_id,
                     .empty = _("No symbol is mapped yet."),
                     .rows_shown = legend_rows_shown}));
            }
            parts.push_back(ec::list(
                {.id = diagnostics_id,
                 .rows = state.diagnostics | std::views::enumerate
                       | std::views::transform([&](const auto& entry) {
                             const auto& [index, message] = entry;
                             return ec::row{
                                 .id = index_row("message", index),
                                 .label = message.text,
                                 .description = message.pointer,
                                 .selectable = state.square_named(message.pointer).has_value()};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "single",
                 .activate = diagnostic_id,
                 .empty = _("No problems.")}));
            affordances.push_back(ec::invoke_affordance(
                {.id = diagnostic_id, .label = _("Show"), .args = row_of(diagnostics_id)}));
            if (state.on_mapgen()) {
                parts.push_back(ec::list(
                    {.id = placements_id,
                     .rows = placement_rows(state),
                     .selection = "single",
                     .activate = placement_id,
                     .empty = _("This entry places nothing.")}));
                affordances.push_back(ec::invoke_affordance(
                    {.id = placement_id, .label = _("Show"), .args = row_of(placements_id)}));
            }
            if (state.on_mapgen()) { placement_forms(state, parts, affordances); }
            parts.push_back(ec::component{
                .id = field_id,
                .affordances = {field_set_id, field_apply_id},
                .body = ec::form_body{
                    .fields =
                        {{.id = "pointer", .label = _("Pointer"), .value = state.field_pointer},
                         {.id = "value",
                          .label = _("Value"),
                          .value = state.field_value,
                          .multiline = true}},
                    .submit = field_apply_id}});
            affordances.push_back(ec::invoke_affordance(
                {.id = field_set_id,
                 .label = _("Value"),
                 .args =
                     {.row = ec::row_shape{.component_id = field_id}, .text = ec::text_shape{}}}));
            affordances.push_back(ec::invoke_affordance({.id = field_apply_id, .label = _("Set")}));
            std::ranges::move(level_affordances(state), std::back_inserter(affordances));
            if (!state.on_special() && !state.on_oter()) {
                std::ranges::move(drawing_affordances(state), std::back_inserter(affordances));
                mapping_parts(state, parts, affordances);
            }
            game_parts(state, parts, affordances);
        }
    }
    parts.push_back(new_file_form(state));
    parts.push_back(ec::component{
        .id = export_options_id,
        .affordances = {export_set_id},
        .body = ec::form_body{
            .fields = {
                {.id = "palette",
                 .label = _("Export palette"),
                 .description = _("A palette of the game whose symbols the export reuses; empty "
                                  "for none."),
                 .value = state.export_palette},
                {.id = "width",
                 .label = _("Width"),
                 .description = _("Overmap squares across, around yours."),
                 .value = std::to_string(state.export_size.x()),
                 .type = "integer",
                 .min = 1,
                 .max = max_export},
                {.id = "height",
                 .label = _("Height"),
                 .description = _("Overmap squares down, around yours."),
                 .value = std::to_string(state.export_size.y()),
                 .type = "integer",
                 .min = 1,
                 .max = max_export}}}});
    parts.push_back(ec::component{
        .id = import_form_id,
        .affordances = {import_set_id, import_id},
        .body = ec::form_body{
            .fields =
                {{.id = "import",
                  .label = _("Mapgen to stamp"),
                  .value = state.import_text,
                  .multiline = true}},
            .submit = import_id}});
    if (!state.doc) {
        const auto recent =
            recent_documents()
            | std::views::filter([](const recent_document& entry) { return entry.editor == "map"; })
            | std::ranges::to<std::vector>();
        if (!recent.empty()) {
            parts.push_back(ec::list(
                {.id = recent_id,
                 .rows = recent | std::views::transform([](const recent_document& entry) {
                             return ec::row{.id = "file:" + entry.name, .label = entry.name};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "single",
                 .activate = recent_open_id}));
            affordances.push_back(ec::invoke_affordance(
                {.id = recent_open_id, .label = _("Open"), .args = row_of(recent_id)}));
        }
    }
    parts.push_back(browser_list(state, listed));
    if (!state.code_mapgens.empty()) {
        parts.push_back(ec::list(
            {.id = "editor:code_mapgens",
             .rows =
                 std::views::iota(std::size_t{0}, state.code_mapgens.size())
                 | std::views::transform([&](const std::size_t index) {
                       const auto& code = state.code_mapgens[index];
                       const auto where =
                           std::array{code.declared, code.defined}
                           | std::views::filter([](const auto& at) { return at.has_value(); })
                           | std::views::transform([](const auto& at) { return *at; })
                           | std::ranges::to<std::vector>();
                       return ec::row{
                           .id = string_format("code:%d", index),
                           .label = string_format(
                               code.method == "lua" ? _("Lua mapgen %s") : _("Built-in mapgen %s"),
                               code.name),
                           .description = join(where, "\n")};
                   })
                 | std::ranges::to<std::vector>()}));
    }
    if (!state.notice.empty()) {
        parts.push_back(
            {.id = "editor:notice", .body = ec::text_body{.markup = state.notice, .tone = "bad"}});
    }
    if (!state.status.empty()) {
        parts.push_back(
            {.id = "editor:status", .body = ec::text_body{.markup = state.status, .tone = "good"}});
    }
    // A stroke left open by a tool or view changed mid-drag can still be ended.
    if (state.doc && state.doc->grouping()
        && std::ranges::none_of(affordances, [](const ec::affordance& entry) {
               return entry.id == stroke_end_id;
           })) {
        affordances.push_back(
            ec::invoke_affordance({.id = stroke_end_id, .label = _("End stroke")}));
    }
    // A palette has no canvas; its legend is what it edits.
    const auto* const active =
        state.on_mapgen() || state.on_special() ? canvas_id
        : state.on_oter() ? (state.terrain_mapgens().empty() ? objects_id : terrains_id)
        : state.doc       ? legend_id
                          : "";
    // Saving leads the File menu, as editors have it.
    std::ranges::stable_sort(affordances, {}, [](const ec::affordance& entry) {
        return entry.id == save_id ? 0 : entry.id == save_as_id ? 1 : 2;
    });
    auto view = ec::screen_view{
        .title = _("Map Editor"),
        .root = ec::split({.id = "editor:body", .active = active, .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = active,
        .focus_row = state.focus ? point_row("square", *state.focus) : ""};
    lay_out(view, layout(state));
    return view;
}

} // namespace editor::map_editor
