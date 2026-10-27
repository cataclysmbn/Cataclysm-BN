#include "editor/map_session.h"

#include "avatar.h"
#include "catacharset.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/json_pointer.h"
#include "editor/mapgen_check.h"
#include "editor/mapgen_legend.h"
#include "editor/mapgen_legend_edit.h"
#include "editor/mapgen_levels.h"
#include "editor/mapgen_placement.h"
#include "editor/mapgen_preview.h"
#include "editor/mapgen_source.h"
#include "editor/mapgen_stamp.h"
#include "editor/mapgen_stroke.h"
#include "editor/recent_documents.h"
#include "editor/session_file.h"
#include "editor/session_page.h"
#include "editor/special_grid.h"
#include "engine_client_world.h"
#include "game.h"
#include "json_formatter.h"
#include "map/mapdata.h"
#include "overmap/omdata.h"
#include "path_info.h"
#include "player_activity.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor::map_editor {

namespace ec = engine_client;

namespace {

auto is_json_mapgen(const nlohmann::ordered_json& value) -> bool {
    return value.is_object() && value.value("type", "") == "mapgen"
        && value.value("method", "") == "json";
}

auto is_palette(const nlohmann::ordered_json& value) -> bool {
    return value.is_object() && value.value("type", "") == "palette";
}

/// Whether a mapping places what a mapgen parameter says, which only that mapgen has.
auto reads_parameter(const nlohmann::ordered_json& mapping) -> bool {
    if (mapping.is_object() && mapping.contains("param")) { return true; }
    return mapping.is_structured() && std::ranges::any_of(mapping, reads_parameter);
}

/// Whether `path` lies inside the game's data or the player's mods, where the editor may write.
auto editable_place(const std::filesystem::path& path) -> bool {
    return path.extension() == ".json"
        && (inside(path, PATH_INFO::datadir()) || inside(path, PATH_INFO::user_moddir()));
}

constexpr auto max_chunk = 24;
/// The farthest `editor:placement_span` moves a corner, past any mapgen's rows.
constexpr auto max_span_move = 1000;

/// The JSON files of the game's data, its mods and the user's mods, in path order.
auto data_files() -> std::vector<std::filesystem::path> {
    auto files = std::vector<std::filesystem::path>{};
    for (const auto& root :
         {std::filesystem::path(PATH_INFO::datadir()) / "json",
          std::filesystem::path(PATH_INFO::datadir()) / "mods",
          std::filesystem::path(PATH_INFO::user_moddir())}) {
        auto error = std::error_code{};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (entry.is_regular_file(error) && entry.path().extension() == ".json") {
                files.push_back(entry.path());
            }
        }
    }
    std::ranges::sort(files);
    return files;
}

/// The range `range_text` wrote: a number or two joined by a dash, the smaller first.
auto range_read(std::string_view text) -> std::optional<placement_range> {
    const auto number = [](std::string_view part) -> std::optional<int> {
        auto value = 0;
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
        if (error != std::errc{} || end != part.data() + part.size()) { return std::nullopt; }
        return value;
    };
    const auto dash = text.find('-', 1);
    const auto min = number(text.substr(0, dash));
    const auto max = dash == std::string_view::npos ? min : number(text.substr(dash + 1));
    if (!min || !max || *max < *min) { return std::nullopt; }
    return placement_range{.min = *min, .max = *max};
}

} // namespace

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

auto mapping_of(const catalog_use& use, const std::string& id) -> std::string {
    if (!use.key) { return id; }
    const auto named =
        use.listed ? nlohmann::ordered_json::array({id}) : nlohmann::ordered_json(id);
    return nlohmann::ordered_json{{use.key, named}}.dump();
}

auto tool_label(const tool& value) -> std::string {
    if (value.region == region_use::select) { return _("Select"); }
    if (value.region == region_use::paste) { return _("Paste"); }
    if (value.region == region_use::placements) { return _("Placements"); }
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

auto entry_name(const nlohmann::ordered_json& value) -> std::string {
    for (const auto* const key : {"om_terrain", "nested_mapgen_id", "update_mapgen_id", "id"}) {
        if (!value.contains(key)) { continue; }
        const auto& name = value[key];
        return name.is_string() ? name.get<std::string>() : name.dump();
    }
    return _("(unnamed)");
}

auto check_new_path(const std::string& made) -> std::expected<void, std::string> {
    const auto path = std::filesystem::path(made);
    if (!is_mod_file(path)) {
        return std::unexpected(string_format(
            _("A new file goes in your mods, %s, and ends in .json."), PATH_INFO::user_moddir()));
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected(string_format(_("%s already exists."), made));
    }
    return {};
}

auto write_new_file(const std::string& made, const nlohmann::ordered_json& entries)
    -> std::expected<void, std::string> {
    const auto path = std::filesystem::path(made);
    auto error = std::error_code{};
    std::filesystem::create_directories(path.parent_path(), error);
    auto out = std::ofstream(path, std::ios::binary);
    out << format_json(entries.dump()).value_or(entries.dump(2));
    if (error || !out.flush()) { return std::unexpected(_("The file could not be written.")); }
    return {};
}

auto create_mapgen(const new_mapgen& made) -> std::expected<void, std::string> {
    if (auto checked = check_new_path(made.path); !checked) { return checked; }
    if (made.kind != "mapgen" && made.kind != "nested" && made.kind != "palette") {
        return std::unexpected(_("A new file holds a mapgen, a nested chunk or a palette."));
    }
    if (made.om_terrain.empty()) {
        return std::unexpected(_("A mapgen names its overmap terrain."));
    }
    if (!ter_str_id(made.fill).is_valid()) {
        return std::unexpected(string_format(_("%s is no terrain."), made.fill));
    }
    if (made.kind == "palette") {
        // The loader keeps one palette of an id.
        if (palette_id(made.om_terrain).is_valid()) {
            return std::unexpected(string_format(_("%s is a palette already."), made.om_terrain));
        }
        return write_new_file(
            made.path,
            nlohmann::ordered_json::array(
                {{{"type", "palette"}, {"id", made.om_terrain}, {"terrain", {{".", made.fill}}}}}));
    }
    const auto nested = made.kind == "nested";
    auto side = 24;
    if (nested) {
        const auto [end, error] =
            std::from_chars(made.size.data(), made.size.data() + made.size.size(), side);
        if (error != std::errc{} || end != made.size.data() + made.size.size() || side < 1
            || side > max_chunk) {
            return std::unexpected(string_format(_("A chunk is 1 to %d squares wide."), max_chunk));
        }
    }
    const auto rows = std::vector<std::string>(side, std::string(side, '.'));
    auto object = nlohmann::ordered_json{{"rows", rows}, {"terrain", {{".", made.fill}}}};
    if (nested) { object["mapgensize"] = {side, side}; }
    const auto entry = nlohmann::ordered_json::array(
        {{{"type", "mapgen"},
          {"method", "json"},
          {nested ? "nested_mapgen_id" : "om_terrain", made.om_terrain},
          {"object", object}}});
    return write_new_file(made.path, entry);
}

auto range_text(const placement_range& range) -> std::string {
    return range.min == range.max
             ? std::to_string(range.min)
             : string_format("%d-%d", range.min, range.max);
}

auto session::browser_entries() const -> std::vector<std::filesystem::path> {
    if (!files) { files = data_files(); }
    const auto lower = [](std::string text) {
        std::ranges::transform(text, text.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    };
    const auto wanted = lower(browser_filter);
    return *files | std::views::filter([&](const std::filesystem::path& path) {
        return lower(path.generic_string()).contains(wanted);
    }) | std::ranges::to<std::vector>();
}

auto session::open_mapgen_of(const std::string& name) -> void {
    const auto id = oter_str_id(name);
    const auto source = mapgen_source_of(id.is_valid() ? id.obj().get_mapgen_id() : name);
    if (!source) {
        notice = string_format(_("%s has no mapgen in a file of the game or of your mods."), name);
        return;
    }
    open(source->path);
    if (!doc || !notice.empty()) { return; }
    const auto& objects = doc->objects();
    const auto found = std::ranges::find_if(objects, [&](const document_object& entry) {
        return entry.begin <= static_cast<std::size_t>(source->offset)
            && static_cast<std::size_t>(source->offset) < entry.end;
    });
    if (found != objects.end() && is_json_mapgen(found->value)) {
        object = static_cast<std::size_t>(found - objects.begin());
        reload();
    }
}

auto session::catalog_entries() const -> editor::catalog_page {
    return search_catalog(
        {.kind = catalog->kind,
         .filter = catalog_filter,
         .page = catalog_page,
         .page_size = page_size});
}

auto session::entries() const -> std::vector<std::size_t> {
    if (!doc) { return {}; }
    return std::views::iota(std::size_t{0}, doc->objects().size())
         | std::views::filter([&](const auto index) {
               const auto& value = doc->objects()[index].value;
               return is_json_mapgen(value) || is_palette(value) || is_special(value);
           })
         | std::ranges::to<std::vector>();
}

auto session::stand_in_texts() const -> std::vector<std::string> {
    return doc->objects() | std::views::filter([](const document_object& entry) {
               return is_palette(entry.value)
                   || (is_json_mapgen(entry.value) && entry.value.contains("nested_mapgen_id"));
           })
         | std::views::transform([](const document_object& entry) { return entry.value.dump(); })
         | std::ranges::to<std::vector>();
}

auto session::on_mapgen() const -> bool {
    return doc && object < doc->objects().size() && is_json_mapgen(doc->objects()[object].value);
}

auto session::on_special() const -> bool {
    return doc && object < doc->objects().size() && is_special(doc->objects()[object].value);
}

auto session::can_climb(const int dz) const -> bool {
    if (!on_special()) { return (dz > 0 ? level_up : level_down).has_value(); }
    return std::ranges::contains(
        special_squares(doc->objects()[object].value), special_level + dz,
        [](const special_square& square) { return square.at.z(); });
}

auto session::climb(const int dz) -> void {
    if (!can_climb(dz)) { return; }
    if (on_special()) {
        special_level += dz;
        reload();
    } else {
        show_object(*(dz > 0 ? level_up : level_down));
    }
}

auto session::preview_options_of(const unsigned int with_seed) const -> preview_options {
    return {
        .at = get_avatar().abs_omt_pos(), .seed = with_seed, .direction = direction, .pins = pins};
}

auto session::pin(const std::string& name, const std::string& value) -> void {
    const auto was = pins;
    const auto was_seed = seed;
    if (value.empty()) {
        pins.erase(name);
    } else {
        pins[name] = value;
    }
    seed = seed.value_or(1);
    reload();
    if (preview) { return; }
    const auto reason = notice;
    pins = was;
    seed = was_seed;
    reload();
    notice = reason;
}

auto session::roll_loot_at(const point& at) -> void {
    const auto stand_ins = file_stand_ins(stand_in_texts());
    const auto rolled = roll_loot(
        doc->objects()[object].value.dump(),
        {.preview = preview_options_of(seed.value_or(1)),
         .from = {at.x, at.y},
         .to = {at.x, at.y},
         .runs = loot_runs});
    if (rolled) {
        loot = *rolled;
    } else {
        notice = rolled.error();
    }
}

auto session::reload_levels() -> void {
    const auto& objects = doc->objects();
    const auto names = entry_terrains(objects[object].value);
    if (names.empty() || names.front().empty()) { return; }
    const auto& first = names.front().front();
    const auto index = index_levels();
    const auto entry_at = [&](const int dz) {
        return terrain_above(index, first, dz)
            .and_then([&](const auto& name) { return square_making(objects, name); })
            .transform([](const entry_square& square) { return square.entry; });
    };
    level = terrain_level(index, first);
    level_up = entry_at(1);
    level_down = entry_at(-1);
    below = stacked_squares(index, objects, object, -1);
    beside = neighbour_squares(index, objects, object);
    const auto read_legend = [&](const entry_square& other) {
        if (other_legends.contains(other.entry)) { return; }
        other_legends[other.entry] =
            mapgen_legend(objects[other.entry].value.dump()).value_or(std::vector<legend_symbol>{});
    };
    std::ranges::for_each(below, read_legend, &stacked_square::other);
    std::ranges::for_each(beside, read_legend, &neighbour_square::other);
}

auto session::open_square(const point& at) -> void {
    if (!on_special()) { return; }
    const auto squares = special_squares(doc->objects()[object].value);
    const auto found = std::ranges::
        find(squares, tripoint_rel_omt(at.x, at.y, special_level), &special_square::at);
    if (found != squares.end()) { open_mapgen_of(found->overmap); }
}

auto session::show_object(const std::size_t index) -> void {
    object = index;
    special_level = 0;
    anchor.reset();
    selected.reset();
    focus.reset();
    picked.reset();
    reload();
}

auto session::reload() -> void {
    legend.clear();
    diagnostics.clear();
    level.reset();
    level_up.reset();
    level_down.reset();
    below.clear();
    beside.clear();
    other_legends.clear();
    if (!doc || object >= doc->objects().size()) { return; }
    const auto text = doc->objects()[object].value.dump();
    if (on_special()) {
        diagnostics =
            check_special(text) | std::views::transform([](const loader_message& message) {
                return diagnostic{.text = message.text};
            })
            | std::ranges::to<std::vector>();
        preview.reset();
        parameters.clear();
        loot.reset();
        return;
    }
    const auto stand_ins = file_stand_ins(stand_in_texts());
    legend = mapgen_legend(text).value_or(std::vector<legend_symbol>{});
    reload_levels();
    diagnostics =
        check_mapgen(text) | std::views::transform([&](const auto& message) {
            return diagnostic{.text = message.text,
                              .pointer = message.offset ? pointer_at(text, *message.offset) : ""};
        })
        | std::ranges::to<std::vector>();
    preview.reset();
    parameters.clear();
    loot.reset();
    if (seed && on_mapgen()) {
        if (const auto loaded = load_detached_mapgens(text)) {
            for (const auto& function : loaded->functions) {
                for (auto& choice : parameters_of(*function)) {
                    if (!std::ranges::
                            contains(parameters, choice.name, &mapgen_parameter_choice::name)) {
                        parameters.push_back(std::move(choice));
                    }
                }
            }
        }
        auto shown = preview_mapgen(text, preview_options_of(*seed));
        if (shown) {
            preview = std::move(*shown);
        } else {
            notice = shown.error();
        }
    }
    if (std::ranges::none_of(legend, [&](const auto& entry) { return entry.symbol == symbol; })) {
        symbol = legend.empty() ? std::string{} : legend.front().symbol;
    }
    if (std::ranges::none_of(legend, [&](const auto& entry) {
            return entry.symbol == room_fill;
        })) {
        room_fill = legend.empty() ? std::string{} : legend.front().symbol;
    }
}

auto session::watch() -> void {
    if (!doc || !doc->changed_on_disk()) { return; }
    if (doc->dirty()) {
        notice = changed_on_disk_notice(*doc);
        return;
    }
    const auto kept = reopen();
    if (notice.empty()) {
        notice = changed_on_disk_notice(*doc);
        // Another entry shown in place of a gone one was not what was being applied.
        if (kept && apply_on_save && on_mapgen()) { apply(); }
    }
}

auto session::reopen() -> bool {
    const auto kept = object;
    open(doc->path().string());
    if (!std::ranges::contains(entries(), kept)) { return false; }
    object = kept;
    reload();
    return true;
}

auto session::save() -> bool { return written(save_document(*doc)); }

auto session::save_as(const std::string& name) -> void { written(save_document_as(*doc, name)); }

auto session::written(const std::expected<void, std::string>& result) -> bool {
    if (told(result) && apply_on_save && on_mapgen()) { apply(); }
    return result.has_value();
}

auto session::apply() -> void {
    auto stand_ins = file_stand_ins(stand_in_texts());
    const auto& entry = doc->objects()[object].value;
    // A chunk the loader refuses stands in for nothing, so what it said is why.
    const auto applied =
        !entry.contains("nested_mapgen_id") ? apply_mapgen(entry.dump())
        : diagnostics.empty()
            ? std::expected<void, std::string>{}
            : std::expected<void, std::string>(std::unexpect, diagnostics.front().text);
    if (applied) {
        stand_ins.keep_chunks();
    } else {
        notice = applied.error();
    }
}

auto session::told(const std::expected<void, std::string>& result) -> bool {
    if (!result) { notice = result.error(); }
    return result.has_value();
}

auto session::leave() -> bool {
    if (!doc || !doc->dirty()) { return true; }
    const auto answer = ask_leave();
    if (answer == leave_answer::discard) {
        reopen();
        // A file that cannot be read again is closed, so its edits do not come back.
        if (doc->dirty()) { doc.reset(); }
    }
    return answer == leave_answer::discard || (answer == leave_answer::save && save());
}

auto session::open(const std::string& name) -> void {
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
    remember_document({.editor = "map", .name = path.generic_string(), .mods = loaded_mods()});
    anchor.reset();
    selected.reset();
    focus.reset();
    picked.reset();
    const auto found = entries();
    object = found.empty() ? doc->objects().size() : found.front();
    notice =
        found.empty()
            ? string_format(
                  _("%s holds no JSON mapgen, palette, overmap special or building."), name)
            : "";
    reload();
    mapping_symbol = symbol;
}

auto session::extract_palette(const std::string& name) -> void {
    if (!on_mapgen()) { return; }
    const auto taken =
        palette_id(name).is_valid()
        || std::ranges::any_of(doc->objects(), [&](const document_object& entry) {
               return is_palette(entry.value) && entry.value.value("id", "") == name;
           });
    if (name.empty() || taken) {
        notice =
            name.empty()
                ? _("A palette has an id.")
                : string_format(_("%s is a palette already."), name);
        return;
    }
    const auto& entry = doc->objects()[object];
    const auto& own = entry.value["object"];
    // Refused before anything is added, so that the edit after the palette is added holds.
    if (entry.duplicate_key) {
        notice = string_format(_("The key \"%s\" appears twice."), *entry.duplicate_key);
        return;
    }
    if (own.contains("palettes") && !own["palettes"].is_array()) {
        notice = _("The mapgen's palettes are not a list.");
        return;
    }
    auto palette = nlohmann::ordered_json{{"type", "palette"}, {"id", name}};
    auto patch = nlohmann::ordered_json::array();
    for (const auto& [layer, mapping] : own.items()) {
        if (!std::ranges::contains(legend_layers, layer)) { continue; }
        if (reads_parameter(mapping)) {
            notice = _("The legend reads the mapgen's parameters, which a palette does not have.");
            return;
        }
        palette[layer] = mapping;
        patch.push_back({{"op", "remove"}, {"path", "/object/" + layer}});
    }
    if (patch.empty()) {
        notice = _("The mapgen maps no symbols itself.");
        return;
    }
    patch.push_back(
        own.contains("palettes")
            ? nlohmann::ordered_json{{"op", "add"}, {"path", "/object/palettes/-"}, {"value", name}}
            : nlohmann::ordered_json{
                  {"op", "add"},
                  {"path", "/object/palettes"},
                  {"value", nlohmann::ordered_json::array({name})}});
    doc->begin_group(true);
    const auto added = doc->add(std::move(palette));
    const auto edited = added ? doc->edit(object, patch) : std::unexpected(added.error());
    doc->end_group();
    if (!edited) { notice = edited.error(); }
    reload();
}

auto session::set_mapping(const std::string& field, const std::string& text) -> void {
    if (field == "symbol") {
        mapping_symbol = text;
    } else if (field == "layer") {
        mapping_layer = text;
    } else if (field == "value") {
        mapping_value = text;
    }
}

auto session::bare(const std::string& glyph, const bool own) const -> std::optional<std::string> {
    const auto& entry = doc->objects()[object].value;
    const auto filled =
        entry.contains("object")
        && (entry["object"].contains("fill_ter") || entry["object"].contains("predecessor_mapgen"));
    const auto found = std::ranges::find(legend, glyph, &legend_symbol::symbol);
    const auto under =
        found != legend.end() && std::ranges::any_of(found->lines, [&](const legend_line& line) {
            return line.layer == "terrain" && (own || !line.source.empty());
        });
    if (!entry.contains("om_terrain") || filled || under) { return std::nullopt; }
    return string_format(
        _("%s has no terrain under it: map its terrain first, or set fill_ter."), glyph);
}

auto session::apply_mapping() -> void {
    if (utf8_width(mapping_symbol) != 1) {
        notice = _("A symbol is one character wide.");
        return;
    }
    if (!std::ranges::contains(legend_layers, mapping_layer)) {
        notice = string_format(_("%s maps no symbols."), mapping_layer);
        return;
    }
    const auto found = std::ranges::find(legend, mapping_symbol, &legend_symbol::symbol);
    const auto used = found != legend.end() && found->used;
    const auto removes_terrain = mapping_layer == "terrain" && mapping_value.empty();
    const auto adds_other = mapping_layer != "terrain" && !mapping_value.empty();
    if (const auto refused = bare(mapping_symbol, !removes_terrain);
        refused && ((removes_terrain && used) || adds_other)) {
        notice = *refused;
        return;
    }
    auto value = std::optional<nlohmann::ordered_json>{};
    if (!mapping_value.empty()) {
        auto parsed = nlohmann::ordered_json::parse(mapping_value, nullptr, false);
        value = parsed.is_discarded() ? nlohmann::ordered_json(mapping_value) : parsed;
    }
    const auto set = set_legend(
        *doc, {.object = object, .layer = mapping_layer, .symbol = mapping_symbol, .value = value});
    if (!set) { notice = set.error(); }
    reload();
}

auto session::square_named(const std::string& pointer) const -> std::optional<point> {
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

auto session::pick(const placement& entry) -> void {
    picked = entry;
    move_x = range_text(entry.x);
    move_y = range_text(entry.y);
    field_pointer = string_format("/object/%s/%d", entry.kind, entry.index);
    field_value = entry.value.dump();
    focus = square_named(field_pointer);
}

auto placement_row(const placement& entry) -> std::string {
    return string_format("placement:%s:%d", entry.kind, entry.index);
}

auto session::span_placement(const std::string& row, const std::string& text) -> void {
    const auto placed = placements_of(*doc, object);
    const auto found = std::ranges::find(placed, row, placement_row);
    if (found == placed.end()) { return; }
    auto moves = std::array<int, 4>{};
    auto rest = std::string_view(text);
    for (auto& value : moves) {
        const auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
        rest = std::string_view(end, rest.data() + rest.size());
        if (error == std::errc{} && rest.starts_with(',') && &value != &moves.back()) {
            rest.remove_prefix(1);
        } else if (error != std::errc{} || &value != &moves.back() || !rest.empty()) {
            notice = _(
                "A placement moves by four whole numbers: how far its first and last squares "
                "go east and south.");
            return;
        }
    }
    if (std::ranges::any_of(moves, [](const int value) {
            return std::abs(value) > max_span_move;
        })) {
        notice = string_format(_("A placement moves at most %d squares."), max_span_move);
        return;
    }
    const auto x = placement_range{.min = found->x.min + moves[0], .max = found->x.max + moves[2]};
    const auto y = placement_range{.min = found->y.min + moves[1], .max = found->y.max + moves[3]};
    if (x.max < x.min || y.max < y.min) {
        notice = _("A placement cannot end before it starts.");
        return;
    }
    pick(*found);
    move_x = range_text(x);
    move_y = range_text(y);
    move_picked();
}

auto session::move_picked() -> void {
    const auto x = range_read(move_x);
    const auto y = range_read(move_y);
    if (!x || !y) {
        notice = _("A place is a number or two joined by a dash, the smaller first.");
        return;
    }
    const auto moved = move_placement(
        *doc, {.object = object, .kind = picked->kind, .index = picked->index, .x = *x, .y = *y});
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

auto session::clip_size_now() const -> clip_size {
    return {.columns = clip.empty() ? 0 : static_cast<int>(clip.front().size()),
            .rows = static_cast<int>(clip.size())};
}

auto session::copy() -> void {
    const auto grid = symbols_of(*doc, object);
    clip = copy_region(grid, selected->from, selected->to);
    clip_legend =
        legend | std::views::filter([&](const legend_symbol& entry) {
            return !entry.lines.empty()
                && std::ranges::contains(clip | std::views::join, entry.symbol);
        })
        | std::views::transform([](const legend_symbol& entry) {
              return std::pair{entry.symbol, entry.lines};
          })
        | std::ranges::to<std::map>();
    const auto low =
        point(std::max(std::min(selected->from.x, selected->to.x), 0),
              std::max(std::min(selected->from.y, selected->to.y), 0));
    const auto size = clip_size_now();
    clip_placed = placements_inside(
        *doc, object,
        {.x = {.min = low.x, .max = low.x + size.columns - 1},
         .y = {.min = low.y, .max = low.y + size.rows - 1}});
}

auto session::cut() -> void {
    if (const auto refused = bare(room_fill, true)) {
        notice = *refused;
        return;
    }
    copy();
    doc->begin_group();
    const auto filled = stroke(
        *doc,
        {.object = object,
         .drawn = {.kind = shape_kind::rect_filled, .from = selected->from, .to = selected->to},
         .symbol = room_fill});
    // Later entries go first, so the indices of the earlier ones still hold.
    auto removed = std::expected<void, std::string>{};
    for (const auto& entry : clip_placed | std::views::reverse) {
        if (!removed) { break; }
        removed =
            remove_placement(*doc, {.object = object, .kind = entry.kind, .index = entry.index});
    }
    doc->end_group();
    notice = !filled ? filled.error() : !removed ? removed.error() : "";
    reload();
}

auto session::paste_at(const point& at) -> void {
    // Only symbols that land on the rows matter here; the rest of the clipboard falls past them.
    const auto grid = symbols_of(*doc, object);
    auto landing = std::set<std::string>{};
    for (auto y = 0; y < static_cast<int>(clip.size()); ++y) {
        for (auto x = 0; x < static_cast<int>(clip[y].size()); ++x) {
            const auto to = at + point(x, y);
            if (to.y >= 0 && to.y < static_cast<int>(grid.size()) && to.x >= 0
                && to.x < static_cast<int>(grid[to.y].size())) {
                landing.insert(clip[y][x]);
            }
        }
    }
    const auto mapped = [](const legend_line& line) { return std::pair{line.layer, line.value}; };
    // A symbol the entry maps or uses keeps its meaning here; one it lacks comes with what it
    // placed where it was copied, unless that reads the parameters it had there.
    auto kept = std::vector<std::string>{};
    auto brought = std::vector<std::pair<std::string, legend_line>>{};
    auto reading = std::vector<std::string>{};
    for (const auto& [glyph, lines] : clip_legend) {
        if (!landing.contains(glyph)) { continue; }
        const auto own = std::ranges::find(legend, glyph, &legend_symbol::symbol);
        if (own != legend.end()) {
            if (!std::ranges::equal(own->lines, lines, {}, mapped, mapped)) {
                kept.push_back(glyph);
            }
            continue;
        }
        if (std::ranges::any_of(lines, [](const legend_line& line) {
                return reads_parameter(nlohmann::ordered_json::parse(line.value, nullptr, false));
            })) {
            reading.push_back(glyph);
            continue;
        }
        for (const auto& line : lines) { brought.emplace_back(glyph, line); }
    }
    if (!reading.empty()) {
        notice = string_format(
            _("These symbols read parameters of the entry they were copied from; map them here "
              "first: %s"),
            reading | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>());
        return;
    }
    doc->begin_group();
    const auto depth = doc->depth();
    auto done = std::expected<void, std::string>{};
    for (const auto& [glyph, line] : brought) {
        if (!done) { break; }
        const auto parsed = nlohmann::ordered_json::parse(line.value, nullptr, false);
        done = set_legend(
            *doc,
            {.object = object,
             .layer = line.layer,
             .symbol = glyph,
             .value = parsed.is_discarded() ? nlohmann::ordered_json(line.value) : parsed});
    }
    if (done) {
        done = editor::paste(*doc, {.object = object, .clip = clip, .at = at})
                   .transform([](std::size_t /*changed*/) {});
    }
    for (const auto& entry : clip_placed) {
        if (!done) { break; }
        done = add_placement(
            *doc,
            {.object = object,
             .kind = entry.kind,
             .x = {.min = entry.x.min + at.x, .max = entry.x.max + at.x},
             .y = {.min = entry.y.min + at.y, .max = entry.y.max + at.y},
             .value = entry.value});
    }
    doc->end_group();
    if (!done && doc->depth() > depth) { doc->undo(); }
    notice =
        !done ? done.error()
        : kept.empty()
            ? ""
            : string_format(_("Pasted symbols keep what this entry maps them to: %s"),
                            kept | std::views::join_with(std::string_view(", "))
                                | std::ranges::to<std::string>());
    reload();
}

auto session::add_placed() -> void {
    const auto x = range_read(place_x);
    const auto y = range_read(place_y);
    if (!x || !y) {
        notice = _("A place is a number or two joined by a dash, the smaller first.");
        return;
    }
    const auto value =
        place_value.empty()
            ? nlohmann::ordered_json::object()
            : nlohmann::ordered_json::parse(place_value, nullptr, false);
    const auto added = add_placement(
        *doc, {.object = object, .kind = place_kind, .x = *x, .y = *y, .value = value});
    if (!added) {
        notice = added.error();
        return;
    }
    notice.clear();
    reload();
    const auto placed = placements_of(*doc, object);
    const auto found = std::ranges::find_last(placed, place_kind, &placement::kind);
    if (!found.empty()) { pick(found.front()); }
}

auto session::delete_picked() -> void {
    const auto removed =
        remove_placement(*doc, {.object = object, .kind = picked->kind, .index = picked->index});
    if (!removed) {
        notice = removed.error();
        return;
    }
    picked.reset();
    focus.reset();
    reload();
}

auto session::set_picked_field() -> void {
    auto value = std::optional<nlohmann::ordered_json>{};
    if (!field_value.empty()) {
        auto parsed = nlohmann::ordered_json::parse(field_value, nullptr, false);
        value = parsed.is_discarded() ? nlohmann::ordered_json(field_value) : parsed;
    }
    const auto set = set_field(*doc, {.object = object, .pointer = field_pointer, .value = value});
    if (!set) { notice = set.error(); }
    reload();
}

auto session::stamp_here() -> void {
    const auto stand_ins = file_stand_ins(stand_in_texts());
    stamp_text(doc->objects()[object].value.dump());
}

auto session::import_pasted() -> void {
    const auto parsed = nlohmann::ordered_json::parse(import_text, nullptr, false);
    const auto found =
        parsed.is_array() ? std::ranges::find_if(parsed, is_json_mapgen) : parsed.end();
    stamp_text(parsed.is_array() && found != parsed.end() ? found->dump() : import_text);
    if (stamped && notice.empty()) { status = _("Stamped."); }
}

auto session::stamp_text(const std::string& text) -> void {
    unstamp();
    auto made = stamp_mapgen(text, {.at = get_avatar().abs_omt_pos(), .seed = seed.value_or(1)});
    if (made) {
        stamped = std::move(*made);
    } else {
        notice = made.error();
    }
}

auto session::unstamp() -> void {
    if (!stamped) { return; }
    swap_stamp(*stamped);
    stamped.reset();
}

auto session::paint(const point& at) -> void {
    const auto grid = symbols_of(*doc, object);
    if (current->region == region_use::select) {
        selected = anchor ? std::optional(selection{.from = *anchor, .to = at}) : std::nullopt;
        anchor = anchor ? std::nullopt : std::optional(at);
        if (selected) {
            place_x = range_text(
                {.min = std::min(selected->from.x, selected->to.x),
                 .max = std::max(selected->from.x, selected->to.x)});
            place_y = range_text(
                {.min = std::min(selected->from.y, selected->to.y),
                 .max = std::max(selected->from.y, selected->to.y)});
        }
        return;
    }
    if (current->region == region_use::paste) {
        paste_at(at);
        return;
    }
    if (current->region == region_use::placements) {
        // The one listed last is drawn on top.
        const auto placed = placements_of(*doc, object);
        const auto found =
            std::ranges::find_if(placed | std::views::reverse, [&](const auto& entry) {
                return entry.x.min <= at.x && at.x <= entry.x.max && entry.y.min <= at.y
                    && at.y <= entry.y.max;
            });
        if (found != std::ranges::rend(placed)) { pick(*found); }
        return;
    }
    if (!current->kind) {
        symbol = symbol_at(grid, at);
        return;
    }
    const auto inks =
        *current->kind == shape_kind::room ? std::vector{symbol, room_fill} : std::vector{symbol};
    for (const auto& ink : inks) {
        if (const auto refused = bare(ink, true)) {
            notice = *refused;
            return;
        }
    }
    if (current->spans && !anchor) {
        anchor = at;
        return;
    }
    const auto drawn = stroke(
        *doc,
        {.object = object,
         .drawn =
             {.kind = *current->kind,
              .from = anchor.value_or(at),
              .to = at,
              .size = brush,
              .mirror_x = mirror_x,
              .mirror_y = mirror_y},
         .symbol = symbol,
         .fill_symbol = room_fill});
    anchor.reset();
    notice = drawn ? "" : drawn.error();
    reload();
}

} // namespace editor::map_editor
