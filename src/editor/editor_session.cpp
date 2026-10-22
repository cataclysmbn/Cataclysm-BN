#include "editor/editor_session.h"

#include "avatar.h"
#include "catacharset.h"
#include "color.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/json_pointer.h"
#include "editor/mapgen_check.h"
#include "editor/mapgen_legend.h"
#include "editor/mapgen_legend_edit.h"
#include "editor/mapgen_placement.h"
#include "editor/mapgen_preview.h"
#include "editor/mapgen_stroke.h"
#include "editor/vehicle_session.h"
#include "engine_client_component.h"
#include "engine_client_world.h"
#include "input.h"
#include "item.h"
#include "json_formatter.h"
#include "map/mapdata.h"
#include "output.h"
#include "path_info.h"
#include "regional_settings.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "ui.h"
#include "ui_manager.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
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
constexpr auto save_as_id = "editor:save_as";
constexpr auto new_id = "editor:new";
constexpr auto new_set_id = "editor:new_set";
constexpr auto new_apply_id = "editor:new_apply";
constexpr auto object_id = "editor:object";
constexpr auto tool_id = "editor:tool";
constexpr auto symbol_id = "editor:symbol";
constexpr auto paint_id = "editor:paint";
constexpr auto undo_id = "editor:undo";
constexpr auto redo_id = "editor:redo";
constexpr auto save_id = "editor:save";
constexpr auto preview_id = "editor:preview";
constexpr auto mapping_id = "editor:mapping";
constexpr auto catalog_kinds_id = "editor:catalog_kinds";
constexpr auto catalog_id = "editor:catalog";
constexpr auto catalog_kind_id = "editor:catalog_kind";
constexpr auto catalog_filter_id = "editor:catalog_filter";
constexpr auto catalog_page_id = "editor:catalog_page";
constexpr auto catalog_pick_id = "editor:catalog_pick";
constexpr auto placements_id = "editor:placements";
constexpr auto placement_id = "editor:placement";
constexpr auto diagnostic_id = "editor:diagnostic";
constexpr auto mapping_set_id = "editor:mapping_set";
constexpr auto mapping_apply_id = "editor:mapping_apply";
constexpr auto move_id = "editor:move";
constexpr auto move_set_id = "editor:move_set";
constexpr auto move_apply_id = "editor:move_apply";
constexpr auto field_id = "editor:field";
constexpr auto field_set_id = "editor:field_set";
constexpr auto field_apply_id = "editor:field_apply";
constexpr auto vehicle_id = "editor:vehicle";

/// The members of a mapgen's object that map its symbols, as the loader reads them.
constexpr auto legend_layers = std::array{
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

const auto catalog_uses = std::array{
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

auto catalog_label(const catalog_use& use) -> std::string {
    switch (use.kind) {
        case catalog_kind::terrain:
            return _("Terrain");
        case catalog_kind::furniture:
            return _("Furniture");
        case catalog_kind::trap:
            return _("Traps");
        case catalog_kind::field:
            return _("Fields");
        case catalog_kind::item:
            return _("Items");
        case catalog_kind::item_group:
            return _("Item groups");
        case catalog_kind::monster:
            return _("Monsters");
        case catalog_kind::monster_group:
            return _("Monster groups");
        case catalog_kind::vehicle:
            return _("Vehicles");
        case catalog_kind::npc_template:
            return _("NPCs");
        case catalog_kind::nested_mapgen:
            return _("Nested chunks");
        case catalog_kind::palette:
            return _("Palettes");
        case catalog_kind::overmap_terrain:
            return _("Overmap terrain");
    }
    return {};
}

/// The mapping of `use`'s layer that places `id`, as the legend form writes it.
auto mapping_of(const catalog_use& use, const std::string& id) -> std::string {
    if (!use.key) { return id; }
    const auto named =
        use.listed ? nlohmann::ordered_json::array({id}) : nlohmann::ordered_json(id);
    return nlohmann::ordered_json{{use.key, named}}.dump();
}

constexpr auto catalog_page_size = std::size_t{50};

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
auto inside(const std::filesystem::path& path, const std::string& root) -> bool {
    auto error = std::error_code{};
    const auto base = std::filesystem::weakly_canonical(root, error);
    const auto relative = std::filesystem::weakly_canonical(path, error).lexically_relative(base);
    return !error && !relative.empty() && *relative.begin() != "..";
}

auto editable_place(const std::filesystem::path& path) -> bool {
    return path.extension() == ".json"
        && (inside(path, PATH_INFO::datadir()) || inside(path, PATH_INFO::user_moddir()));
}

/// What the new mapgen form writes: a file of the user mods holding one mapgen of an overmap
/// terrain, its squares all one terrain.
struct new_mapgen {
    std::string path;
    std::string om_terrain;
    std::string fill = "t_floor";
};

/// Writes `made`'s file; the error says why it was not written.
auto create_mapgen(const new_mapgen& made) -> std::expected<void, std::string> {
    const auto path = std::filesystem::path(made.path);
    if (path.extension() != ".json" || !inside(path, PATH_INFO::user_moddir())) {
        return std::unexpected(string_format(
            _("A new file goes in your mods, %s, and ends in .json."), PATH_INFO::user_moddir()));
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected(string_format(_("%s already exists."), made.path));
    }
    if (made.om_terrain.empty()) {
        return std::unexpected(_("A mapgen names its overmap terrain."));
    }
    if (!ter_str_id(made.fill).is_valid()) {
        return std::unexpected(string_format(_("%s is no terrain."), made.fill));
    }
    const auto rows = std::vector<std::string>(24, std::string(24, '.'));
    const auto entry = nlohmann::ordered_json::array(
        {{{"type", "mapgen"},
          {"method", "json"},
          {"om_terrain", made.om_terrain},
          {"object", {{"rows", rows}, {"terrain", {{".", made.fill}}}}}}});
    auto error = std::error_code{};
    std::filesystem::create_directories(path.parent_path(), error);
    auto out = std::ofstream(path, std::ios::binary);
    out << format_json(entry.dump()).value_or(entry.dump(2));
    if (error || !out.flush()) { return std::unexpected(_("The file could not be written.")); }
    return {};
}

auto range_text(const placement_range& range) -> std::string {
    return range.min == range.max
             ? std::to_string(range.min)
             : string_format("%d-%d", range.min, range.max);
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
    /// The square a placement or a diagnostic that was picked names.
    std::optional<point> focus = std::nullopt;
    /// The placement picked last, which the move form moves.
    std::optional<placement> picked = std::nullopt;
    std::string move_x;
    std::string move_y;
    /// What the field form sets: a pointer into the entry and a value as JSON or bare text; an
    /// empty value removes it.
    std::string field_pointer;
    std::string field_value;
    std::vector<legend_symbol> legend;
    std::vector<diagnostic> diagnostics;
    /// The seed of the preview while one is shown; it follows every edit.
    std::optional<unsigned int> seed = std::nullopt;
    std::optional<mapgen_preview> preview = std::nullopt;
    /// The mapping the legend form sets: one symbol, one layer, and what it places there as JSON
    /// or as a bare id; empty removes it.
    std::string mapping_symbol;
    std::string mapping_layer = "terrain";
    std::string mapping_value;
    new_mapgen made;
    const catalog_use* catalog = catalog_uses.data();
    std::string catalog_filter;
    std::size_t catalog_page = 0;

    auto catalog_entries() const -> editor::catalog_page {
        return search_catalog(
            {.kind = catalog->kind,
             .filter = catalog_filter,
             .page = catalog_page,
             .page_size = catalog_page_size});
    }

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
        preview.reset();
        if (seed) {
            auto shown = preview_mapgen(text, {.at = get_avatar().abs_omt_pos(), .seed = *seed});
            if (shown) {
                preview = std::move(*shown);
            } else {
                notice = shown.error();
            }
        }
        if (std::ranges::none_of(legend, [&](const auto& entry) {
                return entry.symbol == symbol;
            })) {
            symbol = legend.empty() ? std::string{} : legend.front().symbol;
        }
    }

    /// A file changed on disk since it was opened: reloaded while nothing is edited, else reported,
    /// since saving would overwrite that change.
    auto watch() -> void {
        if (!doc || !doc->changed_on_disk()) { return; }
        const auto name = doc->path().string();
        if (doc->dirty()) {
            notice =
                string_format(_("%s changed on disk; saving would overwrite that change."), name);
            return;
        }
        reopen();
        if (notice.empty()) {
            notice = string_format(_("%s changed on disk and was reloaded."), name);
        }
    }
    /// Reads the open file again, on the object it showed while the file still has it.
    auto reopen() -> void {
        const auto kept = object;
        open(doc->path().string());
        if (std::ranges::contains(mapgens(), kept)) {
            object = kept;
            reload();
        }
    }
    /// Writes the file; false, with the notice saying why, when it was not written.
    auto save() -> bool { return written(doc->save()); }
    /// Writes the document to a new file in the user mods, which it is from then on.
    auto save_as(const std::string& name) -> void {
        const auto path = std::filesystem::path(name);
        if (path.extension() != ".json" || !inside(path, PATH_INFO::user_moddir())) {
            notice = string_format(
                _("Save as writes a .json file in your mods, %s."), PATH_INFO::user_moddir());
            return;
        }
        written(doc->save_as(path));
    }
    auto written(const std::expected<void, save_error>& result) -> bool {
        if (!result) {
            switch (result.error()) {
                case save_error::changed_on_disk:
                    notice = _("The file changed on disk since it was opened; nothing was saved.");
                    break;
                case save_error::exists:
                    notice = _("That file already exists; nothing was saved.");
                    break;
                case save_error::unwritable:
                    notice = _("The file could not be written.");
                    break;
            }
        }
        return result.has_value();
    }
    /// Whether to leave: at once with nothing unsaved, else as the player answers save, discard
    /// (which reads the file again) or stay.
    auto leave() -> bool {
        if (!doc || !doc->dirty()) { return true; }
        auto menu = uilist{};
        menu.text = _("Your changes to this file are not saved.");
        menu.addentry(0, true, 's', _("Save and leave"));
        menu.addentry(1, true, 'd', _("Leave without saving"));
        menu.addentry(2, true, 'c', _("Stay"));
        menu.query();
        if (menu.ret == 1) {
            reopen();
            // A file that cannot be read again is closed, so its edits do not come back.
            if (doc->dirty()) { doc.reset(); }
        }
        return menu.ret == 1 || (menu.ret == 0 && save());
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
        focus.reset();
        picked.reset();
        const auto found = mapgens();
        object = found.empty() ? doc->objects().size() : found.front();
        notice = found.empty() ? string_format(_("%s holds no JSON mapgen."), name) : "";
        reload();
        mapping_symbol = symbol;
    }

    auto set_mapping(const std::string& field, const std::string& text) -> void {
        if (field == "symbol") {
            mapping_symbol = text;
        } else if (field == "layer") {
            mapping_layer = text;
        } else if (field == "value") {
            mapping_value = text;
        }
    }

    auto apply_mapping() -> void {
        if (utf8_width(mapping_symbol) != 1) {
            notice = _("A symbol is one character wide.");
            return;
        }
        if (!std::ranges::contains(legend_layers, mapping_layer)) {
            notice = string_format(_("%s maps no symbols."), mapping_layer);
            return;
        }
        auto value = std::optional<nlohmann::ordered_json>{};
        if (!mapping_value.empty()) {
            auto parsed = nlohmann::ordered_json::parse(mapping_value, nullptr, false);
            value = parsed.is_discarded() ? nlohmann::ordered_json(mapping_value) : parsed;
        }
        const auto set = set_legend(
            *doc,
            {.object = object, .layer = mapping_layer, .symbol = mapping_symbol, .value = value});
        if (!set) { notice = set.error(); }
        reload();
    }

    /// The square of the rows nearest to where the value at `pointer` of the object stands: its
    /// row, or its placement's first square; none for any other value.
    auto square_named(const std::string& pointer) const -> std::optional<point> {
        const auto grid = symbols_of(*doc, object);
        if (grid.empty()) { return std::nullopt; }
        const auto inside = [&](const point& at) {
            const auto y = std::clamp(at.y, 0, static_cast<int>(grid.size()) - 1);
            return point(std::clamp(at.x, 0, std::max(0, static_cast<int>(grid[y].size()) - 1)), y);
        };
        auto row = 0;
        if (std::sscanf(pointer.c_str(), "/object/rows/%d", &row) == 1) {
            return inside(point(0, row));
        }
        const auto placed = placements_of(*doc, object);
        const auto found = std::ranges::find_if(placed, [&](const placement& entry) {
            const auto own = string_format("/object/%s/%d", entry.kind, entry.index);
            return pointer == own || pointer.starts_with(own + "/");
        });
        if (found == placed.end()) { return std::nullopt; }
        return inside(point(found->x.min, found->y.min));
    }

    auto pick(const placement& entry) -> void {
        picked = entry;
        move_x = range_text(entry.x);
        move_y = range_text(entry.y);
        field_pointer = string_format("/object/%s/%d", entry.kind, entry.index);
        field_value = entry.value.dump();
        focus = square_named(field_pointer);
    }

    auto move_picked() -> void {
        const auto number = [](std::string_view text) -> std::optional<int> {
            auto value = 0;
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size()) { return std::nullopt; }
            return value;
        };
        const auto range_of = [&](std::string_view text) -> std::optional<placement_range> {
            const auto dash = text.find('-', 1);
            const auto min = number(text.substr(0, dash));
            const auto max = dash == std::string_view::npos ? min : number(text.substr(dash + 1));
            if (!min || !max || *max < *min) { return std::nullopt; }
            return placement_range{.min = *min, .max = *max};
        };
        const auto x = range_of(move_x);
        const auto y = range_of(move_y);
        if (!x || !y) {
            notice = _("A place is a number or two joined by a dash, the smaller first.");
            return;
        }
        const auto moved = move_placement(
            *doc,
            {.object = object, .kind = picked->kind, .index = picked->index, .x = *x, .y = *y});
        if (!moved) {
            notice = moved.error();
            return;
        }
        reload();
        const auto placed = placements_of(*doc, object);
        const auto found = std::ranges::find_if(placed, [&](const placement& entry) {
            return entry.kind == picked->kind && entry.index == picked->index;
        });
        if (found != placed.end()) { pick(*found); }
    }

    auto set_picked_field() -> void {
        auto value = std::optional<nlohmann::ordered_json>{};
        if (!field_value.empty()) {
            auto parsed = nlohmann::ordered_json::parse(field_value, nullptr, false);
            value = parsed.is_discarded() ? nlohmann::ordered_json(field_value) : parsed;
        }
        const auto set =
            set_field(*doc, {.object = object, .pointer = field_pointer, .value = value});
        if (!set) { notice = set.error(); }
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
                 .id = string_format("preview:%d,%d", x, y),
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

/// What each symbol puts on a square from the bottom up, as the map shows it: its terrain, else the
/// entry's fill, then its furniture and trap.
auto symbol_looks(const session& state) -> std::map<std::string, std::vector<engine_client::look>> {
    namespace world = engine_client::world;
    const auto& object = state.doc->objects()[state.object].value;
    const auto fill =
        object.contains("object") && object["object"].contains("fill_ter")
                && object["object"]["fill_ter"].is_string()
            ? std::optional(object["object"]["fill_ter"].get<std::string>())
            : std::nullopt;
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
    for (const auto& entry : state.legend) {
        const auto named = [&](const char* layer) -> std::optional<std::string> {
            const auto line = std::ranges::find(entry.lines, layer, &legend_line::layer);
            return line == entry.lines.end() ? std::nullopt : first_id(line->value);
        };
        auto looks = std::vector<engine_client::look>{};
        if (const auto ter = ter_str_id(
                named("terrain").or_else([&] { return fill; }).value_or(""));
            ter.is_valid() && !ter.is_null()) {
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
        result[entry.symbol] = std::move(looks);
    }
    return result;
}

auto canvas(const session& state) -> ec::component {
    const auto grid = symbols_of(*state.doc, state.object);
    auto palette = ec::look_palette{};
    const auto looks =
        symbol_looks(state) | std::views::transform([&](const auto& entry) {
            return std::pair{entry.first, palette.indexes(entry.second)};
        })
        | std::ranges::to<std::map>();
    const auto cursor = state.anchor ? state.anchor : state.focus;
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
                 .selected = state.anchor == at,
                 .looks = looks.contains(grid[y][x]) ? looks.at(grid[y][x]) : std::vector<int>{}});
        }
    }
    return {
        .id = canvas_id,
        .affordances = {paint_id},
        .body = ec::grid_body{
            .cols = cols,
            .rows = static_cast<int>(grid.size()),
            .cursor = cursor ? std::optional(ec::grid_point{cursor->x, cursor->y}) : std::nullopt,
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas"}};
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
                   .id = string_format("placement:%s:%d", entry.kind, entry.index),
                   .label = entry.kind,
                   .description = string_format("/object/%s/%d", entry.kind, entry.index),
                   .columns =
                       {{.key = "x", .label = "x", .value = range_text(entry.x)},
                        {.key = "y", .label = "y", .value = range_text(entry.y)},
                        {.key = "value", .label = _("Places"), .value = placement_text(what)}}};
           })
         | std::ranges::to<std::vector>();
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
                        .label = _("File"),
                        .value = state.doc ? state.doc->path().generic_string() : ""}},
            .submit = open_id}});
    const auto row_of = [](const char* const component) {
        return ec::arg_shape{.row = ec::row_shape{.component_id = component}};
    };
    parts.push_back(ec::component{
        .id = new_id,
        .affordances = {new_set_id, new_apply_id},
        .body = ec::form_body{
            .fields =
                {{.id = "path", .label = _("New file"), .value = state.made.path},
                 {.id = "om_terrain", .label = _("Overmap terrain"), .value = state.made.om_terrain},
                 {.id = "fill", .label = _("Terrain"), .value = state.made.fill}},
            .submit = new_apply_id}});
    affordances.push_back(ec::invoke_affordance(
        {.id = new_set_id,
         .label = _("New file"),
         .args = {.row = ec::row_shape{.component_id = new_id}, .text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance({.id = new_apply_id, .label = _("Create")}));
    affordances.push_back(ec::invoke_affordance({.id = vehicle_id, .label = _("Vehicle editor")}));
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
        if (state.seed) {
            stats.push_back(
                {.key = "seed", .label = _("Seed"), .value = std::to_string(*state.seed)});
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
                       | std::views::transform([&](const auto& entry) {
                             const auto& [index, message] = entry;
                             return ec::row{
                                 .id = string_format("message:%d", index),
                                 .label = message.text,
                                 .description = message.pointer,
                                 .selectable = state.square_named(message.pointer).has_value()};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "single",
                 .activate = diagnostic_id}));
            parts.push_back(ec::list(
                {.id = placements_id,
                 .rows = placement_rows(state),
                 .selection = "single",
                 .activate = placement_id}));
            affordances.push_back(ec::invoke_affordance(
                {.id = diagnostic_id, .label = _("Show"), .args = row_of(diagnostics_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = placement_id, .label = _("Show"), .args = row_of(placements_id)}));
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
                     .args = {.row = ec::row_shape{.component_id = move_id},
                              .text = ec::text_shape{}}}));
                affordances.push_back(
                    ec::invoke_affordance({.id = move_apply_id, .label = _("Move")}));
            }
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
            affordances.push_back(ec::invoke_affordance(
                {.id = tool_id, .label = _("Tool"), .args = row_of(tools_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = symbol_id, .label = _("Symbol"), .args = row_of(legend_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = paint_id, .label = _("Draw"), .args = row_of(canvas_id)}));
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
                                  return ec::form_option{.id = layer, .label = layer};
                              })
                              | std::ranges::to<std::vector>()},
                         {.id = "value", .label = _("Places"), .value = state.mapping_value}},
                    .submit = mapping_apply_id}});
            const auto found = state.catalog_entries();
            const auto pages =
                std::max<std::size_t>(1, (found.total + catalog_page_size - 1) / catalog_page_size);
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
                 .child = ec::list(
                     {.id = catalog_id,
                      .rows = found.entries | std::views::transform([](const catalog_entry& entry) {
                                  return ec::row{
                                      .id = "entry:" + entry.id,
                                      .label = entry.name,
                                      .columns =
                                          {{.key = "id", .label = _("ID"), .value = entry.id},
                                           {.key = "mod", .label = _("Mod"), .value = entry.mod}}};
                              })
                            | std::ranges::to<std::vector>(),
                      .total = found.total,
                      .offset = state.catalog_page * catalog_page_size,
                      .selection = "single",
                      .activate = catalog_pick_id,
                      .filterable = true})}));
            affordances.push_back(ec::invoke_affordance(
                {.id = catalog_kind_id, .label = _("Kind"), .args = row_of(catalog_kinds_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = catalog_filter_id,
                 .label = _("Filter"),
                 .args = {.text = ec::text_shape{}}}));
            affordances.push_back(ec::invoke_affordance(
                {.id = catalog_page_id,
                 .label = _("Page"),
                 .args = {
                     .count = ec::count_shape{
                         .min = 0,
                         .max = static_cast<std::int64_t>(pages - 1),
                         .initial = static_cast<std::int64_t>(state.catalog_page)}}}));
            affordances.push_back(ec::invoke_affordance(
                {.id = catalog_pick_id, .label = _("Use"), .args = row_of(catalog_id)}));
            affordances.push_back(ec::invoke_affordance(
                {.id = mapping_set_id,
                 .label = _("Value"),
                 .args = {.row = ec::row_shape{.component_id = mapping_id},
                          .text = ec::text_shape{}}}));
            affordances.push_back(
                ec::invoke_affordance({.id = mapping_apply_id, .label = _("Set mapping")}));
            affordances.push_back(ec::invoke_affordance(
                {.id = preview_id,
                 .label = _("Preview"),
                 .args = {
                     .count = ec::count_shape{
                         .min = 1,
                         .max = std::numeric_limits<unsigned int>::max(),
                         .initial = state.seed.value_or(1)}}}));
            if (state.preview) { parts.push_back(preview_grid(*state.preview)); }
        }
    }
    if (!state.notice.empty()) {
        parts.push_back(
            {.id = "editor:notice", .body = ec::text_body{.markup = state.notice, .tone = "bad"}});
    }
    return {
        .title = _("Map Editor"),
        .root = ec::split(
            {.id = "editor:body",
             .active = state.doc ? canvas_id : "",
             .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = state.doc ? canvas_id : "",
        .focus_row = state.focus ? square_id(*state.focus) : ""};
}

/// The square a canvas row names.
auto square_of(const std::string& id) -> std::optional<point> {
    auto at = point{};
    if (std::sscanf(id.c_str(), "square:%d,%d", &at.x, &at.y) != 2) { return std::nullopt; }
    return at;
}

/// The documents and view the editor keeps between visits.
auto kept_session = std::optional<session>{};
/// Whether the next visit is to the vehicle editor.
auto vehicle_first = false;

} // namespace

auto reset_editor_session() -> void { kept_session.reset(); }

auto open_vehicle_editor_first() -> void { vehicle_first = true; }

auto run_editor_session() -> void {
    if (std::exchange(vehicle_first, false)) {
        run_vehicle_editor();
        return;
    }
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto& state = kept_session ? *kept_session : kept_session.emplace();
    state.notice.clear();
    const auto screen = ec::screen_scope(
        ctxt, "editor",
        {.role = "screen",
         .view = [&](const ec::boundary_state& /*boundary*/) { return session_view(state); }});
    while (true) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto invoked = ec::invoked(ctxt.get_raw_input());
        if (!invoked) {
            if (action == "QUIT" && state.leave()) { return; }
            continue;
        }
        const auto& id = invoked->affordance_id;
        const auto row = invoked->args.row.value_or("");
        state.notice.clear();
        state.watch();
        if (id == open_id) {
            state.open(invoked->args.text.value_or(""));
        } else if (id == vehicle_id) {
            run_vehicle_editor();
        } else if (id == new_set_id) {
            const auto text = invoked->args.text.value_or("");
            (row == "path" ? state.made.path
             : row == "om_terrain"
                 ? state.made.om_terrain
                 : state.made.fill) = text;
        } else if (id == new_apply_id) {
            const auto made = create_mapgen(state.made);
            if (made) {
                state.open(state.made.path);
            } else {
                state.notice = made.error();
            }
        } else if (!state.doc) {
            continue;
        } else if (id == object_id) {
            auto index = std::size_t{0};
            if (std::sscanf(row.c_str(), "object:%zu", &index) == 1) {
                state.object = index;
                state.anchor.reset();
                state.focus.reset();
                state.picked.reset();
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
            state.mapping_symbol = state.symbol;
        } else if (id == paint_id) {
            if (const auto at = square_of(row)) { state.paint(*at); }
        } else if (id == undo_id || id == redo_id) {
            const auto moved = id == undo_id ? state.doc->undo() : state.doc->redo();
            if (moved) { state.reload(); }
        } else if (id == placement_id) {
            const auto placed = placements_of(*state.doc, state.object);
            const auto found = std::ranges::find(placed, row, [](const placement& entry) {
                return string_format("placement:%s:%d", entry.kind, entry.index);
            });
            if (found != placed.end()) { state.pick(*found); }
        } else if (id == move_set_id) {
            (row == "x" ? state.move_x : state.move_y) = invoked->args.text.value_or("");
        } else if (id == move_apply_id && state.picked) {
            state.move_picked();
        } else if (id == field_set_id) {
            (row == "pointer" ? state.field_pointer : state.field_value) =
                invoked->args.text.value_or("");
        } else if (id == field_apply_id) {
            state.set_picked_field();
        } else if (id == diagnostic_id) {
            auto index = std::size_t{0};
            if (std::sscanf(row.c_str(), "message:%zu", &index) == 1
                && index < state.diagnostics.size()) {
                state.focus = state.square_named(state.diagnostics[index].pointer);
            }
        } else if (id == catalog_kind_id) {
            const auto found = std::ranges::find_if(catalog_uses, [&](const catalog_use& use) {
                return row == std::string("catalog:") + use.id;
            });
            if (found != catalog_uses.end()) {
                state.catalog = &*found;
                state.catalog_page = 0;
            }
        } else if (id == catalog_filter_id) {
            state.catalog_filter = invoked->args.text.value_or("");
            state.catalog_page = 0;
        } else if (id == catalog_page_id) {
            state.catalog_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == catalog_pick_id) {
            const auto entries = state.catalog_entries().entries;
            const auto found = std::ranges::find(entries, row, [](const catalog_entry& entry) {
                return "entry:" + entry.id;
            });
            if (found != entries.end()) {
                state.mapping_layer = state.catalog->layer;
                state.mapping_value = mapping_of(*state.catalog, found->id);
            }
        } else if (id == mapping_set_id) {
            state.set_mapping(row, invoked->args.text.value_or(""));
        } else if (id == mapping_apply_id) {
            state.apply_mapping();
        } else if (id == preview_id) {
            state.seed = static_cast<unsigned int>(invoked->args.count.value_or(1));
            state.reload();
        } else if (id == save_id) {
            state.save();
        } else if (id == save_as_id) {
            state.save_as(invoked->args.text.value_or(""));
        }
    }
}

} // namespace editor
