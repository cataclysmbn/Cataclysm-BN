#include "editor/map_session.h"

#include "avatar.h"
#include "catacharset.h"
#include "color.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/history_label.h"
#include "editor/json_pointer.h"
#include "editor/mapgen_check.h"
#include "editor/mapgen_legend.h"
#include "editor/mapgen_legend_edit.h"
#include "editor/mapgen_legend_text.h"
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
#include "engine_client_part_type.h"
#include "engine_client_world.h"
#include "filesystem.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "item_group.h"
#include "itype.h"
#include "json_formatter.h"
#include "map/field_type.h"
#include "map/mapdata.h"
#include "mongroup.h"
#include "mtype.h"
#include "overmap/omdata.h"
#include "overmap/overmap_special.h"
#include "path_info.h"
#include "player_activity.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "vehicle/veh_type.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <span>
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

/// The built-in or Lua mapgen an entry names, where `path` holds it from byte `begin`; none
/// for any other entry.
auto code_mapgen_in(const nlohmann::ordered_json& value, const std::string& path, std::size_t begin)
    -> std::optional<code_mapgen> {
    if (!value.is_object() || value.value("type", "") != "mapgen") { return std::nullopt; }
    const auto method = value.value("method", "");
    const auto name =
        method == "builtin" ? value.value("name", "")
        : method == "lua"
            ? value.value("luamethod", "")
            : std::string{};
    if (name.empty()) { return std::nullopt; }
    return code_mapgen{
        .method = method,
        .name = name,
        .declared = source_line(path, static_cast<int>(begin)),
        .defined = method == "lua" ? lua_mapgen_defined_at(name) : std::nullopt};
}

auto is_oter(const nlohmann::ordered_json& value) -> bool {
    return value.is_object() && value.value("type", "") == "overmap_terrain";
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

/// The farthest `editor:placement_span` moves a corner, past any mapgen's rows.
constexpr auto max_span_move = 1000;

/// The JSON file at `path` with the ids of the entries the editor edits in it; none when it holds
/// no such entry.
auto editable_file(const std::filesystem::path& path) -> std::optional<data_file> {
    const auto text = read_entire_file(path.string());
    // Most files name none of the types; only those that do are parsed.
    if (!std::ranges::any_of(
            std::array{"\"mapgen\"", "\"palette\"", "\"overmap_special\"", "\"city_building\"",
                       "\"overmap_terrain\""},
            [&](const char* type) { return text.contains(type); })) {
        return std::nullopt;
    }
    const auto parsed = nlohmann::ordered_json::parse(text, nullptr, false);
    const auto editable = [](const nlohmann::ordered_json& value) {
        return is_json_mapgen(value) || is_palette(value) || is_special(value) || is_oter(value);
    };
    const auto entries = parsed.is_array() ? parsed : nlohmann::ordered_json::array({parsed});
    auto edited = entries | std::views::filter(editable) | std::views::common;
    if (std::ranges::empty(edited)) { return std::nullopt; }
    return data_file{
        .path = path,
        .ids = edited | std::views::transform(entry_ids) | std::views::join
             | std::ranges::to<std::vector>()};
}

/// The JSON files of the game's data, its mods and the user's mods that hold an entry the editor
/// edits, in path order.
auto data_files() -> std::vector<data_file> {
    auto files = std::vector<data_file>{};
    for (const auto& root :
         {std::filesystem::path(PATH_INFO::datadir()) / "json",
          std::filesystem::path(PATH_INFO::datadir()) / "mods",
          std::filesystem::path(PATH_INFO::user_moddir())}) {
        auto error = std::error_code{};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (!entry.is_regular_file(error) || entry.path().extension() != ".json") { continue; }
            if (auto file = editable_file(entry.path())) { files.push_back(std::move(*file)); }
        }
    }
    std::ranges::sort(files, {}, &data_file::path);
    return files;
}

/// `text` in lower case, as a filter compares it.
auto lower(std::string text) -> std::string {
    std::ranges::transform(text, text.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
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

/// The symbols a new mapping falls back on, as an export uses them.
constexpr auto spare_symbols = std::string_view(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,;:!%&*+-/<>=?@^~_()[]{}|$");

/// The symbol a new mapping of what shows as `glyph`, named `name`, takes: that glyph, else the
/// first letter of the name in either case, else a spare symbol; the first not `taken`.
auto free_symbol(
    const std::vector<std::string>& taken, const std::string& glyph, const std::string& name)
    -> std::optional<std::string> {
    const auto printable = [](const std::string& text) {
        return text.size() == 1 && text[0] > ' ' && text[0] < '\x7f' && text[0] != '\\';
    };
    auto wanted = std::vector{glyph};
    if (!name.empty()) {
        const auto first = static_cast<unsigned char>(name.front());
        wanted.emplace_back(1, static_cast<char>(std::tolower(first)));
        wanted.emplace_back(1, static_cast<char>(std::toupper(first)));
    }
    std::ranges::
        copy(spare_symbols | std::views::transform([](const char c) { return std::string(1, c); }),
             std::back_inserter(wanted));
    const auto found = std::ranges::find_if(wanted, [&](const std::string& candidate) {
        return printable(candidate) && !std::ranges::contains(taken, candidate);
    });
    return found == wanted.end() ? std::nullopt : std::optional(*found);
}

/// Whether the terrain `id` is flat ground, and with `indoor` a floor a roof stands on.
auto is_floor(const std::string& id, const bool indoor) -> bool {
    const auto terrain = ter_str_id(id);
    return terrain.is_valid() && terrain->has_flag(TFLAG_FLAT)
        && (!indoor || terrain->has_flag(TFLAG_SUPPORTS_ROOF));
}

/// The symbol of `legend` that maps such a floor alone and that `grid` uses most; none when no
/// symbol maps one.
auto most_used_floor(
    const std::vector<legend_symbol>& legend, const symbol_grid& grid, const bool indoor)
    -> std::optional<std::string> {
    const auto floor = [&](const legend_symbol& entry) {
        if (entry.lines.size() != 1 || entry.lines.front().layer != "terrain") { return false; }
        const auto value = nlohmann::ordered_json::parse(entry.lines.front().value, nullptr, false);
        return value.is_string() && is_floor(value.get<std::string>(), indoor);
    };
    const auto uses = [&](const legend_symbol& entry) {
        return std::ranges::count(grid | std::views::join, entry.symbol);
    };
    const auto floors = legend | std::views::filter(floor) | std::ranges::to<std::vector>();
    if (floors.empty()) { return std::nullopt; }
    return std::ranges::max(floors, {}, uses).symbol;
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

auto catalog_row_label(const catalog_entry& entry) -> std::string {
    return entry.distinct.empty()
             ? entry.name
             : string_format("%s (%s)", entry.name, entry.distinct);
}

auto layer_look(const std::string_view layer, const std::string& id)
    -> std::optional<engine_client::look> {
    namespace world = engine_client::world;
    const auto creature = [](const mtype& type) {
        return engine_client::look{
            .kind = "monster",
            .id = type.id.str(),
            .name = type.nname(),
            .glyph = type.sym,
            .color = string_from_color(type.color),
            .looks_like =
                type.looks_like.empty() ? std::vector<std::string>{} : std::vector{type.looks_like}};
    };
    if (layer == "terrain" && ter_str_id(id).is_valid()) {
        return world::terrain_appearance(ter_str_id(id).id());
    }
    if (layer == "furniture" && furn_str_id(id).is_valid()) {
        return world::furniture_appearance(furn_str_id(id).id());
    }
    if (layer == "traps" && trap_str_id(id).is_valid()) {
        return world::trap_appearance(trap_str_id(id).obj());
    }
    if (layer == "fields" && field_type_str_id(id).is_valid()) {
        const auto& type = field_type_str_id(id).obj();
        return engine_client::look{
            .kind = "field",
            .id = id,
            .name = type.get_name(),
            .glyph = type.get_symbol(),
            .color = string_from_color(type.get_color())};
    }
    if ((layer == "item" || layer == "sealed_item") && itype_id(id).is_valid()) {
        return world::item_appearance(*item::spawn_temporary(itype_id(id), calendar::turn));
    }
    if (layer == "items" && item_group::group_is_defined(item_group_id(id))) {
        const auto possible = item_group::every_possible_item_from(item_group_id(id));
        if (possible.empty()) { return std::nullopt; }
        const auto* const first = std::ranges::min(possible, {}, [](const itype* type) {
            return type->get_id().str();
        });
        return layer_look("item", first->get_id().str());
    }
    if (layer == "monster" && mtype_id(id).is_valid()) { return creature(mtype_id(id).obj()); }
    if (layer == "monsters" && MonsterGroupManager::isValidMonsterGroup(mongroup_id(id))) {
        const auto& usual = MonsterGroupManager::GetMonsterGroup(mongroup_id(id)).defaultMonster;
        return usual.is_valid() && !usual.is_null()
                 ? std::optional(creature(usual.obj()))
                 : std::nullopt;
    }
    if (layer == "npcs") {
        return engine_client::
            look{.kind = "npc", .id = id, .glyph = "@", .color = string_from_color(c_white)};
    }
    if (layer == "vehicles" && vproto_id(id).is_valid() && !vproto_id(id)->parts.empty()) {
        const auto part = vproto_id(id)->parts.front().part;
        return part.is_valid()
                 ? std::optional(world::vehicle_part_type_appearance(part.obj()))
                 : std::nullopt;
    }
    return std::nullopt;
}

auto catalog_look(const catalog_kind kind, const std::string& id)
    -> std::optional<engine_client::look> {
    const auto use = std::ranges::find(catalog_uses, kind, &catalog_use::kind);
    return use == catalog_uses.end() ? std::nullopt : layer_look(use->layer, id);
}

auto first_named(const std::string_view layer, const nlohmann::ordered_json& value)
    -> std::optional<std::string> {
    if (value.is_string()) { return value.get<std::string>(); }
    if (value.is_array() && !value.empty()) { return first_named(layer, value.front()); }
    const auto use = std::ranges::find(catalog_uses, layer, &catalog_use::layer);
    if (!value.is_object() || use == catalog_uses.end() || !use->key || !value.contains(use->key)) {
        return std::nullopt;
    }
    return first_named(layer, value[use->key]);
}

auto mapping_of(const catalog_use& use, const std::string& id) -> std::string {
    if (!use.key) { return id; }
    const auto named =
        use.listed ? nlohmann::ordered_json::array({id}) : nlohmann::ordered_json(id);
    return nlohmann::ordered_json{{use.key, named}}.dump();
}

auto mapping_json_of(const catalog_use& use, const std::string& id) -> nlohmann::ordered_json {
    return use.key ? nlohmann::ordered_json::parse(mapping_of(use, id)) : nlohmann::ordered_json(id);
}

auto tool_label(const tool& value) -> std::string {
    if (value.id == "tool:open") { return _("Open mapgen"); }
    if (value.id == "tool:erase") { return _("Eraser"); }
    if (value.id == "tool:connect") { return _("Connect"); }
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

namespace {
/// The names in a name or a list of them, nested as a mapgen of several overmap squares has it.
auto names_in(const nlohmann::ordered_json& name) -> std::vector<std::string> {
    if (name.is_string()) { return {name.get<std::string>()}; }
    if (!name.is_array()) { return {name.dump()}; }
    return name | std::views::transform(names_in) | std::views::join
         | std::ranges::to<std::vector>();
}
} // namespace

auto entry_ids(const nlohmann::ordered_json& value) -> std::vector<std::string> {
    for (const auto* const key : {"om_terrain", "nested_mapgen_id", "update_mapgen_id", "id"}) {
        if (value.contains(key)) { return names_in(value[key]); }
    }
    return {};
}

auto entry_name(const nlohmann::ordered_json& value) -> std::string {
    const auto names = join(entry_ids(value), ", ");
    return names.empty() ? _("(unnamed)") : names;
}

auto check_new_path(const std::string& made) -> std::expected<void, std::string> {
    const auto path = std::filesystem::path(made);
    if (!is_mod_file(path)) {
        return std::unexpected(string_format(
            _("A new file goes in your mods, %s, inside a mod's folder such as my_mod/, and ends in "
              ".json."),
            PATH_INFO::user_moddir()));
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected(string_format(_("%s already exists."), made));
    }
    return check_mod_folder(path);
}

auto write_new_file(const std::string& made, const nlohmann::ordered_json& entries)
    -> std::expected<void, std::string> {
    const auto path = std::filesystem::path(made);
    auto error = std::error_code{};
    std::filesystem::create_directories(path.parent_path(), error);
    auto out = std::ofstream(path, std::ios::binary);
    out << format_json(entries.dump()).value_or(entries.dump(2));
    if (error || !out.flush()) { return std::unexpected(_("The file could not be written.")); }
    return make_mod_of(path);
}

auto create_mapgen(const new_mapgen& made) -> std::expected<void, std::string> {
    if (auto checked = check_new_path(made.path); !checked) { return checked; }
    if (!std::ranges::
            contains(std::array{"mapgen", "nested", "palette", "special", "building"}, made.kind)) {
        return std::unexpected(_(
            "A new file holds a mapgen, a nested chunk, a palette, an overmap special or a "
            "building."));
    }
    if (made.om_terrain.empty()) {
        return std::unexpected(_("A mapgen names its overmap terrain."));
    }
    if (made.kind == "special" || made.kind == "building") {
        // Buildings are specials to the loader, which keeps one of an id.
        if (overmap_special_id(made.om_terrain).is_valid()) {
            return std::unexpected(string_format(
                _("%s names an overmap special or building already."), made.om_terrain));
        }
        if (!oter_str_id(made.fill).is_valid()) {
            return std::unexpected(
                oter_str_id(made.fill + "_north").is_valid()
                    ? string_format(
                          _("%1$s is no overmap terrain. Did you mean %1$s_north?"), made.fill)
                    : string_format(_("%s is no overmap terrain."), made.fill));
        }
        auto entry = nlohmann::ordered_json{
            {"type", made.kind == "special" ? "overmap_special" : "city_building"},
            {"id", made.om_terrain},
            {"overmaps", {{{"point", {0, 0, 0}}, {"overmap", made.fill}}}},
            {"locations", {"land"}}};
        if (made.kind == "special") { entry["occurrences"] = {0, 1}; }
        return write_new_file(made.path, nlohmann::ordered_json::array({entry}));
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
    // A map outdoors keeps a floor at hand for the rooms built on it.
    if (!nested && !is_floor(made.fill, true)) { object["terrain"]["_"] = "t_floor"; }
    // A chunk keeps the map under a symbol nothing maps; a mapgen fills it, so a new symbol needs
    // no terrain of its own.
    if (nested) {
        object["mapgensize"] = {side, side};
    } else {
        object["fill_ter"] = made.fill;
    }
    const auto entry = nlohmann::ordered_json::array(
        {{{"type", "mapgen"},
          {"method", "json"},
          {nested ? "nested_mapgen_id" : "om_terrain", made.om_terrain},
          {"object", object}}});
    return write_new_file(made.path, entry);
}

auto new_entry_id(const new_mapgen& made) -> std::string {
    if (!made.om_terrain.empty()) { return made.om_terrain; }
    // Words of the file's name, lower case, joined by single underscores.
    auto id = std::string{};
    for (const auto c : std::filesystem::path(made.path).stem().string()) {
        const auto letter = static_cast<unsigned char>(c);
        if (std::isalnum(letter)) {
            id += static_cast<char>(std::tolower(letter));
        } else if (!id.empty() && id.back() != '_') {
            id += '_';
        }
    }
    while (!id.empty() && id.back() == '_') { id.pop_back(); }
    return id;
}

auto free_new_path() -> std::string {
    // A file loose in the mods is no mod's, so the suggestion is in a mod's folder.
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    return (mods
            / (free_name("new_mapgen",
                         [&](const std::string& name) {
                             return std::filesystem::exists(mods / (name + ".json"));
                         })
               + ".json"))
        .string();
}

auto range_text(const placement_range& range) -> std::string {
    return range.min == range.max
             ? std::to_string(range.min)
             : string_format("%d-%d", range.min, range.max);
}

auto session::browser_entries() const -> std::vector<browser_entry> {
    if (!files) { files = data_files(); }
    const auto wanted = lower(browser_filter);
    // An id matches by itself or by the name the overmap shows for the terrain it is.
    const auto matches = [&](const std::string& id) {
        const auto terrain = oter_type_str_id(id);
        return lower(id).contains(wanted)
            || (terrain.is_valid() && lower(_(terrain->name)).contains(wanted));
    };
    auto result = std::vector<browser_entry>{};
    for (const auto& file : *files) {
        if (lower(file.path.generic_string()).contains(wanted)) {
            result.push_back({.path = file.path});
            continue;
        }
        auto matched = file.ids | std::views::filter(matches) | std::ranges::to<std::vector>();
        if (!matched.empty()) {
            result.push_back({.path = file.path, .matched = std::move(matched)});
        }
    }
    return result;
}

auto session::refresh_file(const std::filesystem::path& path) -> void {
    if (!files) { return; }
    // The file is listed as the browser found it, whichever way it was named.
    auto error = std::error_code{};
    const auto found = std::ranges::find_if(*files, [&](const data_file& file) {
        return std::filesystem::equivalent(file.path, path, error);
    });
    const auto listed = found == files->end() ? path : found->path;
    if (found != files->end()) { files->erase(found); }
    if (auto file = editable_file(listed)) {
        files->insert(
            std::ranges::upper_bound(*files, listed, {}, &data_file::path), std::move(*file));
    }
}

namespace {
/// The mapgen ids that make what `name` names, as the game finds a square's mapgen; `named` counts
/// the overmap terrain types a name the overmap shows names.
struct named_mapgens {
    std::vector<std::string> ids;
    std::size_t named = 0;
};

/// An overmap terrain, its type without a direction, a special or building by its squares from its
/// origin on, or the name the overmap shows, each ignoring case.
auto mapgens_named(const std::string& name) -> named_mapgens {
    if (const auto id = oter_str_id(name); id.is_valid()) { return {.ids = {id->get_mapgen_id()}}; }
    if (oter_type_str_id(name).is_valid()) { return {.ids = terrain_mapgen_ids(name)}; }
    if (const auto special = overmap_special_id(name); special.is_valid()) {
        auto terrains = special->all_terrains();
        const auto& origin = special->get_terrain_at(tripoint_rel_omt(0, 0, 0));
        std::ranges::stable_partition(terrains, [&](const auto& terrain) {
            return terrain == origin;
        });
        return {
            .ids = terrains | std::views::filter(&oter_str_id::is_valid)
                 | std::views::transform([](const auto& terrain) {
                       return terrain->get_mapgen_id();
                   })
                 | std::ranges::to<std::vector>()};
    }
    const auto lower = [](std::string text) {
        std::ranges::transform(text, text.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    };
    const auto wanted = lower(name);
    auto types =
        overmap_terrains::get_all() | std::views::filter([&](const oter_t& terrain) {
            return lower(terrain.get_name()) == wanted
                || lower(terrain.get_type_id()->name) == wanted;
        })
        | std::views::transform([](const oter_t& terrain) { return terrain.get_type_id().str(); })
        | std::ranges::to<std::vector>();
    std::ranges::sort(types);
    const auto [last, end] = std::ranges::unique(types);
    types.erase(last, end);
    return {.ids = types | std::views::transform(&terrain_mapgen_ids) | std::views::join
                 | std::ranges::to<std::vector>(),
            .named = types.size()};
}
} // namespace

auto session::open_mapgen_of(const std::string& name) -> void {
    const auto named = mapgens_named(name);
    const auto made = std::ranges::find_if(named.ids, [](const std::string& id) {
        return mapgen_source_of(id).has_value() || !code_mapgens_of(id).empty();
    });
    const auto mapgen_id =
        made != named.ids.end() ? *made
        : named.ids.empty()
            ? name
            : named.ids.front();
    // What a type, building or shown name opened is said, since the file names another id.
    const auto opened =
        oter_str_id(name).is_valid() || mapgen_id == name ? std::string{}
        : named.named > 1
            ? string_format(
                  vgettext("%1$d overmap terrain is named %2$s; opened %3$s.",
                           "%1$d overmap terrains are named %2$s; opened %3$s.", named.named),
                  static_cast<int>(named.named), name, mapgen_id)
            : string_format(_("Opened %1$s, the mapgen of %2$s."), mapgen_id, name);
    const auto source = mapgen_source_of(mapgen_id);
    if (!source) {
        // Code has no squares to draw; where it is declared and defined is what there is to show.
        code_mapgens = code_mapgens_of(mapgen_id);
        if (code_mapgens.empty()) {
            notice =
                string_format(_("%s has no mapgen in a file of the game or of your mods."), name);
        } else {
            status = opened;
        }
        return;
    }
    open(source->path);
    if (!doc || !notice.empty()) { return; }
    status = opened;
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
        {.kind = on_special() ? catalog_kind::overmap_terrain : catalog->kind,
         .filter = catalog_filter,
         .page = catalog_page,
         .page_size = page_size});
}

auto session::entries() const -> std::vector<std::size_t> {
    if (!doc) { return {}; }
    return std::views::iota(std::size_t{0}, doc->objects().size())
         | std::views::filter([&](const auto index) {
               const auto& value = doc->objects()[index].value;
               return is_json_mapgen(value) || is_palette(value) || is_special(value)
                   || is_oter(value);
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

auto session::on_update() const -> bool {
    return on_mapgen() && doc->objects()[object].value.contains("update_mapgen_id");
}

auto session::has_rows() const -> bool {
    if (!on_mapgen()) { return false; }
    const auto& value = doc->objects()[object].value;
    return value.contains("object") && value["object"].is_object()
        && value["object"].contains("rows");
}

auto session::square_refusal() const -> std::optional<std::string> {
    if (!on_mapgen()) { return std::nullopt; }
    if (doc->objects()[object].value.contains("nested_mapgen_id")) {
        return _(
            "A nested chunk goes on a map with the mapgen that places it; open that mapgen to "
            "stamp, test or preview it.");
    }
    if (on_update()) {
        return _(
            "An update mapgen changes squares the game made already, when a mission or event "
            "runs it; it is not stamped, applied, tested or previewed.");
    }
    return std::nullopt;
}

auto session::on_special() const -> bool {
    return doc && object < doc->objects().size() && is_special(doc->objects()[object].value);
}

auto session::on_oter() const -> bool {
    return doc && object < doc->objects().size() && is_oter(doc->objects()[object].value);
}

auto session::terrain_mapgens() const -> std::vector<std::string> {
    if (!on_oter()) { return {}; }
    const auto& entry = doc->objects()[object].value;
    if (!entry.contains("id")) { return {}; }
    const auto& id = entry["id"];
    const auto types =
        id.is_string() ? std::vector{id.get<std::string>()}
        : id.is_array()
            ? id | std::views::filter(&nlohmann::ordered_json::is_string)
                  | std::views::transform([](const auto& type) {
                        return type.template get<std::string>();
                    })
                  | std::ranges::to<std::vector>()
            : std::vector<std::string>{};
    return types | std::views::transform(&terrain_mapgen_ids) | std::views::join
         | std::ranges::to<std::vector>();
}

auto session::level_beside(const int dz) const -> std::optional<int> {
    if (!on_special()) { return std::nullopt; }
    // Drawing reaches levels the special has no squares on yet.
    if (special_tool->kind && !drawing_refusal()) {
        const auto next = special_level + (dz > 0 ? 1 : -1);
        return next >= -OVERMAP_DEPTH && next <= OVERMAP_HEIGHT ? std::optional(next) : std::nullopt;
    }
    const auto levels = special_levels(doc->objects()[object].value);
    if (dz > 0) {
        const auto found = std::ranges::upper_bound(levels, special_level);
        return found == levels.end() ? std::nullopt : std::optional(*found);
    }
    const auto found = std::ranges::lower_bound(levels, special_level);
    return found == levels.begin() ? std::nullopt : std::optional(*std::prev(found));
}

auto session::can_climb(const int dz) const -> bool {
    if (!on_special()) { return (dz > 0 ? level_up : level_down).has_value(); }
    return level_beside(dz).has_value();
}

auto session::climb(const int dz) -> void {
    if (!can_climb(dz)) { return; }
    if (on_special()) {
        special_level = *level_beside(dz);
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

auto session::roll_loot_in(const selection& squares) -> void {
    const auto stand_ins = file_stand_ins(stand_in_texts());
    const auto rolled = roll_loot(
        doc->objects()[object].value.dump(),
        {.preview = preview_options_of(seed.value_or(1)),
         .from = {std::min(squares.from.x, squares.to.x), std::min(squares.from.y, squares.to.y)},
         .to = {std::max(squares.from.x, squares.to.x), std::max(squares.from.y, squares.to.y)},
         .runs = loot_runs});
    if (rolled) {
        loot = *rolled;
        const auto count =
            (std::abs(squares.to.x - squares.from.x) + 1)
            * (std::abs(squares.to.y - squares.from.y) + 1);
        // Each sentence translates whole, with one count to pick its plural by.
        status =
            loot->empty()
                ? string_format(vgettext("%d square marked: nothing can spawn there.",
                                         "%d squares marked: nothing can spawn there.", count),
                                count)
                : string_format(vgettext("%d square marked.", "%d squares marked.", count), count)
                      + " "
                      + string_format(vgettext("%d kind of item can spawn there.",
                                               "%d kinds of items can spawn there.", loot->size()),
                                      loot->size());
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
    held.reset();
    special_level = ground_level(special_levels(doc->objects()[index].value));
    anchor.reset();
    selected.reset();
    focus.reset();
    picked.reset();
    reload();
}

auto session::step_history(const bool back) -> void {
    const auto steps = doc->history();
    const auto depth = doc->depth();
    if (back ? depth == 0 : depth >= steps.size()) { return; }
    const auto& step = steps[back ? depth - 1 : depth];
    if (!(back ? doc->undo() : doc->redo())) { return; }
    status = string_format(
        back ? _("Undid: %s") : _("Redid: %s"),
        step_name(step, [&](const document_edit& edit) { return edit_title(edit); }));
    // An entry an undo removed is gone; the one it changed otherwise is what to look at.
    const auto changed = std::ranges::find_if(step.edits, [&](const document_edit& edit) {
        return edit.object < doc->objects().size();
    });
    if (changed != step.edits.end()
        && !std::ranges::contains(step.edits, object, &document_edit::object)) {
        show_object(changed->object);
        return;
    }
    if (object >= doc->objects().size() && !doc->objects().empty()) {
        object = doc->objects().size() - 1;
    }
    reload();
}

auto session::reload() -> void {
    legend.clear();
    legend_error.clear();
    diagnostics.clear();
    level.reset();
    level_up.reset();
    level_down.reset();
    below.clear();
    beside.clear();
    other_legends.clear();
    if (!doc || object >= doc->objects().size()) { return; }
    // The catalog lists overmap terrains for a special and what a mapgen places otherwise.
    if (std::exchange(catalog_of_special, on_special()) != on_special()) {
        catalog_filter.clear();
        catalog_page = 0;
    }
    const auto text = doc->objects()[object].value.dump();
    if (on_special() || on_oter()) {
        diagnostics =
            (on_special() ? check_special(text) : check_oter(text))
            | std::views::transform([](const loader_message& message) {
                  return diagnostic{.text = message.text};
              })
            | std::ranges::to<std::vector>();
        preview.reset();
        parameters.clear();
        loot.reset();
        return;
    }
    // An entry without rows is edited by its placements alone, so that tool is the one shown until
    // an entry with rows gives back the one the player had.
    if (on_mapgen() && !has_rows() && current->region != region_use::placements) {
        unforced = current;
        current = &*std::ranges::find(tools, region_use::placements, &tool::region);
        anchor.reset();
    } else if (on_mapgen() && has_rows() && unforced != nullptr) {
        current = std::exchange(unforced, nullptr);
        anchor.reset();
    }
    const auto stand_ins = file_stand_ins(stand_in_texts());
    const auto read = mapgen_legend(text);
    legend = read.value_or(std::vector<legend_symbol>{});
    legend_error = read ? std::string{} : read.error();
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
    if (seed && on_mapgen() && !square_refusal()) {
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
    // What the catalog put in hand stays there until it is drawn, and after an undo of that.
    if (std::ranges::none_of(legend, [&](const auto& entry) { return entry.symbol == symbol; })
        && !(held && held->symbol == symbol)) {
        symbol = legend.empty() ? std::string{} : legend.front().symbol;
    }
    if (std::ranges::none_of(legend, [&](const auto& entry) {
            return entry.symbol == room_fill;
        })) {
        room_fill =
            most_used_floor(legend, symbols_of(*doc, object), true)
                .value_or(legend.empty() ? std::string{} : legend.front().symbol);
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
        if (kept && applies_on_save()) { apply(); }
    }
}

auto session::reopen() -> bool {
    const auto kept = object;
    const auto kept_level = special_level;
    const auto kept_of_special = catalog_of_special;
    const auto kept_filter = catalog_filter;
    const auto kept_page = catalog_page;
    open(doc->path().string());
    if (!std::ranges::contains(entries(), kept)) { return false; }
    object = kept;
    const auto levels = special_levels(doc->objects()[object].value);
    special_level = std::ranges::contains(levels, kept_level) ? kept_level : ground_level(levels);
    reload();
    // The file's first entry may have shown another catalog on the way.
    if (catalog_of_special == kept_of_special) {
        catalog_filter = kept_filter;
        catalog_page = kept_page;
    }
    return true;
}

auto session::save() -> bool { return written(save_document(*doc)); }

auto session::save_as(const std::string& name) -> void { written(save_document_as(*doc, name)); }

auto session::written(const std::expected<void, std::string>& result) -> bool {
    //~ %s is the file the map editor wrote, such as my_mod/house.json.
    if (result) { status = string_format(_("Saved %s."), mods_path(doc->path())); }
    if (told(result) && applies_on_save()) { apply(); }
    if (result) { refresh_file(doc->path()); }
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
        status = string_format(_("The game generates %s with this entry now."), entry_name(entry));
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
    held.reset();
    refresh_file(path);
    ++openings;
    remember_document({.editor = "map", .name = path.generic_string(), .mods = mods_for(path)});
    anchor.reset();
    selected.reset();
    focus.reset();
    picked.reset();
    const auto found = entries();
    code_mapgens =
        doc->objects() | std::views::transform([&](const document_object& entry) {
            return code_mapgen_in(entry.value, path.string(), entry.begin);
        })
        | std::views::filter([](const auto& code) { return code.has_value(); })
        | std::views::transform([](const auto& code) { return *code; })
        | std::ranges::to<std::vector>();
    object = found.empty() ? doc->objects().size() : found.front();
    special_level = found.empty() ? 0 : ground_level(special_levels(doc->objects()[object].value));
    notice =
        found.empty() && code_mapgens.empty()
            ? string_format(
                  _("%s holds no JSON mapgen, palette, overmap special or building."), name)
            : "";
    reload();
    mapping_symbol = symbol;
}

auto session::palette_taken(const std::string& name) const -> bool {
    return palette_id(name).is_valid()
        || std::ranges::any_of(doc->objects(), [&](const document_object& entry) {
               return is_palette(entry.value) && entry.value.value("id", "") == name;
           });
}

auto session::free_palette_id() const -> std::string {
    const auto ids = entry_ids(doc->objects()[object].value);
    return free_name((ids.empty() ? std::string("new") : ids.front()) + "_palette",
                     [&](const std::string& name) { return palette_taken(name); });
}

auto session::free_export_path() const -> std::string {
    const auto folder = doc ? mod_folder_name(doc->path()) : std::nullopt;
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir()) / folder.value_or("my_mod");
    return (mods
            / (free_name("exported_mapgen",
                         [&](const std::string& name) {
                             const auto path = mods / (name + ".json");
                             return std::filesystem::exists(path)
                                 || (doc
                                     && path.lexically_normal() == doc->path().lexically_normal());
                         })
               + ".json"))
        .string();
}

auto session::extract_palette(const std::string& name) -> void {
    if (!on_mapgen()) { return; }
    if (name.empty() || palette_taken(name)) {
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
    doc->begin_group(true, _("Extract palette"));
    const auto added = doc->add(std::move(palette));
    const auto edited = added ? doc->edit(object, patch) : std::unexpected(added.error());
    doc->end_group();
    if (!edited) { notice = edited.error(); }
    reload();
}

auto session::pick_symbol(const std::string& picked) -> void {
    symbol = picked;
    mapping_symbol = picked;
    held.reset();
    const auto found = std::ranges::find(legend, picked, &legend_symbol::symbol);
    const auto lines = found == legend.end() ? std::span<const legend_line>{} : found->lines;
    const auto own = std::ranges::find_last_if(lines, &std::string::empty, &legend_line::source);
    const auto* const top = !own.empty() ? &own.front() : lines.empty() ? nullptr : &lines.back();
    if (on_mapgen()) {
        arm();
        //~ %1$s is what the symbol drawn with places, such as brick wall; %2$s is the symbol.
        status =
            string_format(_("Painting %1$s (%2$s)."), symbol_name(picked), shown_symbol(picked));
    }
    if (top == nullptr) {
        mapping_layer = "terrain";
        mapping_value.clear();
        return;
    }
    mapping_layer = top->layer;
    // A plain id reads as the form takes it, bare.
    const auto value = nlohmann::ordered_json::parse(top->value, nullptr, false);
    mapping_value = value.is_string() ? value.get<std::string>() : top->value;
}

auto session::take(const catalog_use& use, const std::string& id, const std::string& name) -> void {
    if (!legend_error.empty()) {
        //~ %s is what is wrong with the entry's legend, as the game's loader says it.
        status = string_format(_("The legend cannot be read: %s"), plain_loader_text(legend_error));
        return;
    }
    const auto value = mapping_json_of(use, id);
    const auto mapped = std::ranges::find_if(legend, [&](const legend_symbol& entry) {
        return entry.lines.size() == 1 && entry.lines.front().layer == use.layer
            && nlohmann::ordered_json::parse(entry.lines.front().value, nullptr, false) == value;
    });
    if (mapped != legend.end()) {
        pick_symbol(mapped->symbol);
        return;
    }
    const auto look = catalog_look(use.kind, id);
    const auto free = free_symbol(taken_symbols(), look ? look->glyph : "", name);
    if (!free) {
        notice = _("Every symbol is taken; map one in the Mapping form.");
        return;
    }
    held = held_mapping{.symbol = *free, .layer = use.layer, .value = value, .name = name};
    symbol = *free;
    mapping_symbol = *free;
    mapping_layer = use.layer;
    mapping_value = mapping_of(use, id);
    arm();
    status = string_format(_("Painting %1$s (%2$s)."), name, shown_symbol(*free));
}

auto session::symbol_name(const std::string& glyph) const -> std::string {
    // What the catalog put in hand names its symbol until the legend has it.
    const auto found = std::ranges::find(legend, glyph, &legend_symbol::symbol);
    if (found == legend.end() && held && held->symbol == glyph) { return held->name; }
    if (found == legend.end() || found->lines.empty()) { return shown_symbol(glyph); }
    const auto& lines = found->lines;
    const auto on = [&](const char* const layer) {
        return std::ranges::find(lines | std::views::reverse, layer, &legend_line::layer);
    };
    const auto furniture = on("furniture");
    const auto terrain = on("terrain");
    const auto& top =
        furniture != lines.rend() ? *furniture
        : terrain != lines.rend()
            ? *terrain
            : lines.front();
    return legend_value(top.layer, top.value).names;
}

auto session::in_hand() const -> std::optional<held_mapping> {
    const auto found = std::ranges::find(legend, symbol, &legend_symbol::symbol);
    if (found == legend.end() && held && held->symbol == symbol) { return held; }
    if (found == legend.end() || found->lines.size() != 1) { return std::nullopt; }
    const auto& line = found->lines.front();
    return held_mapping{
        .symbol = symbol,
        .layer = line.layer,
        .value = nlohmann::ordered_json::parse(line.value, nullptr, false)};
}

auto session::taken_symbols() const -> std::vector<std::string> {
    // A symbol the file maps keeps its mapping, whatever of it the legend could read.
    auto taken =
        legend | std::views::transform(&legend_symbol::symbol) | std::ranges::to<std::vector>();
    const auto& own = doc->objects()[object].value;
    if (own.contains("object") && own["object"].is_object()) {
        for (const auto* const layer : legend_layers) {
            if (!own["object"].contains(layer) || !own["object"][layer].is_object()) { continue; }
            for (const auto& member : own["object"][layer].items()) {
                taken.push_back(member.key());
            }
        }
    }
    return taken;
}

auto session::layered(const held_mapping& brush, const std::vector<std::string>& under) const
    -> std::expected<layered_ink, std::string> {
    // What a symbol places, whatever order or palette maps it.
    const auto places = [](const std::vector<legend_line>& lines) {
        auto key =
            lines | std::views::transform([](const legend_line& line) {
                return line.layer + '\n'
                     + nlohmann::ordered_json::parse(line.value, nullptr, false).dump();
            })
            | std::ranges::to<std::vector>();
        std::ranges::sort(key);
        return key;
    };
    // The lines of a layer a palette and the mapgen both map, as one line placing the same: terrain
    // and furniture laid last stand, the other layers place each.
    constexpr auto laid_last = std::array<std::string_view, 3>{"terrain", "furniture", "traps"};
    const auto merged = [&](const std::vector<legend_line>& lines) {
        auto one = std::vector<legend_line>{};
        for (const auto& line : lines) {
            const auto found = std::ranges::find(one, line.layer, &legend_line::layer);
            if (found == one.end()) {
                one.push_back({.layer = line.layer, .value = line.value});
            } else if (std::ranges::contains(laid_last, line.layer)) {
                found->value = line.value;
            } else {
                const auto listed = [](const std::string& text) {
                    const auto value = nlohmann::ordered_json::parse(text, nullptr, false);
                    return value.is_array() ? value : nlohmann::ordered_json::array({value});
                };
                auto both = listed(found->value);
                for (const auto& value : listed(line.value)) { both.push_back(value); }
                found->value = both.dump();
            }
        }
        return one;
    };
    auto taken = taken_symbols();
    taken.push_back(brush.symbol);
    auto inks = layered_ink{};
    for (const auto& old : under) {
        if (old == brush.symbol || inks.over.contains(old)) { continue; }
        const auto found = std::ranges::find(legend, old, &legend_symbol::symbol);
        if (found == legend.end()) { continue; }
        auto lines =
            found->lines
            | std::views::filter([&](const legend_line& line) { return line.layer != brush.layer; })
            | std::views::transform([](const legend_line& line) {
                  return legend_line{.layer = line.layer, .value = line.value};
              })
            | std::ranges::to<std::vector>();
        if (lines.empty()) { continue; }
        lines.push_back({.layer = brush.layer, .value = brush.value.dump()});
        lines = merged(lines);
        const auto wanted = places(lines);
        // Drawn again with what it places already, a square is drawn over whole by terrain, so a
        // floor drawn on a table on that floor takes the table away, and kept by the rest.
        if (wanted == places(merged(found->lines))) {
            if (brush.layer != "terrain") { inks.over[old] = old; }
            continue;
        }
        const auto same = [&](const legend_symbol& entry) {
            return places(merged(entry.lines)) == wanted;
        };
        if (const auto mapped = std::ranges::find_if(legend, same); mapped != legend.end()) {
            inks.over[old] = mapped->symbol;
            continue;
        }
        if (const auto added = std::ranges::find_if(inks.added, same); added != inks.added.end()) {
            inks.over[old] = added->symbol;
            continue;
        }
        const auto free = free_symbol(taken, "", symbol_name(brush.symbol));
        if (!free) {
            return std::unexpected(_("Every symbol is taken; map one in the Mapping form."));
        }
        taken.push_back(*free);
        inks.over[old] = *free;
        inks.added.push_back({.symbol = *free, .lines = std::move(lines)});
    }
    return inks;
}

auto session::edit_title(const document_edit& edit) const -> std::string {
    const auto rows = [](const nlohmann::ordered_json& value) {
        const auto pointer = nlohmann::ordered_json::json_pointer("/object/rows");
        auto grid = symbol_grid{};
        if (!value.is_object() || !value.contains(pointer) || !value[pointer].is_array()) {
            return grid;
        }
        for (const auto& row : value[pointer]) {
            grid.push_back(
                row.is_string()
                    ? utf8_display_split(row.get<std::string>())
                    : std::vector<std::string>{});
        }
        return grid;
    };
    const auto before = rows(edit.before);
    const auto after = rows(edit.after);
    auto painted = std::map<std::string, int>{};
    for (auto y = 0; y < static_cast<int>(std::min(before.size(), after.size())); ++y) {
        for (auto x = 0; x < static_cast<int>(after[y].size()); ++x) {
            if (symbol_at(before, point(x, y)) != after[y][x]) { ++painted[after[y][x]]; }
        }
    }
    if (painted.empty()) { return step_label(edit); }
    if (painted.size() == 1) {
        //~ A step of the map editor's history: %1$s is what was painted, such as brick wall, and
        //~ %2$d on how many squares.
        return string_format(
            _("Paint %1$s ×%2$d"), symbol_name(painted.begin()->first), painted.begin()->second);
    }
    const auto squares = std::ranges::fold_left(painted | std::views::values, 0, std::plus{});
    return string_format(vgettext("Paint %d square", "Paint %d squares", squares), squares);
}

auto session::arm() -> void {
    if (current->kind || !has_rows()) { return; }
    current = tools.data();
    unforced = nullptr;
    anchor.reset();
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
        _("%s has no terrain under it: map its terrain first, or set /object/fill_ter in the JSON "
          "value form."),
        glyph);
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
    // A symbol just mapped is what to draw with next.
    if (set && value && has_rows()) { pick_symbol(mapping_symbol); }
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
    // What is cut away leaves the ground most of the rows stand on.
    const auto ground =
        most_used_floor(legend, symbols_of(*doc, object), false).value_or(room_fill);
    if (const auto refused = bare(ground, true)) {
        notice = *refused;
        return;
    }
    copy();
    doc->begin_group(false, _("Cut"));
    const auto filled = stroke(
        *doc,
        {.object = object,
         .drawn = {.kind = shape_kind::rect_filled, .from = selected->from, .to = selected->to},
         .symbol = ground});
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
    doc->begin_group(false, _("Paste"));
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
    if (stamped && notice.empty()) { status = _("Stamped over the overmap square you stand on."); }
}

auto session::place_special() -> void {
    if (!on_special()) { return; }
    const auto applied = apply_special(doc->objects()[object].value.dump());
    if (!applied) {
        notice = applied.error();
        return;
    }
    const auto at = place_special_near(*applied, get_avatar().abs_omt_pos());
    if (!at) {
        notice = _("No land near you that is still to be generated holds the special.");
        return;
    }
    placed = placed_special{.id = applied->str(), .at = *at};
    status = string_format(_("Placed %s nearby; Test takes you there."), applied->str());
}

auto session::placed_here() const -> std::optional<tripoint_abs_omt> {
    if (!on_special() || !placed) { return std::nullopt; }
    const auto& entry = doc->objects()[object].value;
    const auto named =
        entry.contains("id") && entry["id"].is_string()
        && entry["id"].get<std::string>() == placed->id;
    return named ? std::optional(placed->at) : std::nullopt;
}

auto session::import_pasted() -> void {
    const auto parsed = nlohmann::ordered_json::parse(import_text, nullptr, false);
    if (parsed.is_discarded()) {
        notice = _("The pasted text is not JSON.");
        return;
    }
    const auto found =
        parsed.is_array() ? std::ranges::find_if(parsed, is_json_mapgen) : parsed.end();
    if (parsed.is_array() && found == parsed.end()) {
        notice = _("The pasted text holds no mapgen entry.");
        return;
    }
    stamp_text(parsed.is_array() ? found->dump() : import_text);
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

auto session::drawing_refusal() const -> std::optional<std::string> {
    const auto& value = doc->objects()[object].value;
    if (!is_fixed(value)) {
        return _("A mutable special has no squares to draw; edit its fields.");
    }
    if (value.contains("copy-from") || value.contains("abstract")) {
        return _(
            "A special that copies another, or is abstract, takes its squares from the game's data; edit its fields.");
    }
    if (value.contains("overmaps") && !value["overmaps"].is_array()) {
        return _("Its overmaps are no list of squares; edit its fields.");
    }
    return std::nullopt;
}

auto session::paint_square(const point& at) -> void {
    const auto& value = doc->objects()[object].value;
    if (const auto refused = drawing_refusal()) {
        notice = *refused;
        return;
    }
    if (std::max(std::abs(at.x), std::abs(at.y)) >= OMAPX) {
        notice = _("A special reaches no farther than an overmap.");
        return;
    }
    const auto erasing = special_tool->id == std::string("tool:erase");
    if (special_tool->id == std::string("tool:point") && special_terrain.empty()) {
        notice = _("Pick an overmap terrain in the catalog to draw with.");
        return;
    }
    const auto square = tripoint_rel_omt(at.x, at.y, special_level);
    // A connection already there is taken away whatever it sits on.
    if (special_tool->id == std::string("tool:connect") && !has_connection(value, square)) {
        if (special_connection.empty()) {
            notice = _("Name the overmap connection to join.");
            return;
        }
        if (std::ranges::contains(special_squares(value), square, &special_square::at)) {
            notice = _("A connection joins beside the special's squares, not on one.");
            return;
        }
    }
    const auto patch =
        special_tool->id == std::string("tool:connect")
            ? connection_patch(value, square, special_connection)
            : square_patch(value, square, erasing ? "" : special_terrain);
    if (patch.empty()) { return; }
    if (const auto edited = doc->edit(object, patch); !edited) { notice = edited.error(); }
    reload();
}

auto session::paint(const point& at) -> void {
    if (on_special()) {
        if (special_tool->kind) {
            paint_square(at);
        } else {
            open_square(at);
        }
        return;
    }
    const auto grid = symbols_of(*doc, object);
    if (current->region == region_use::select) {
        // What was counted over the squares marked before no longer stands.
        loot.reset();
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
        if (found != std::ranges::rend(placed)) {
            pick(*found);
        } else {
            status = _(
                "No placement here. Mark squares with the Select tool, then add one under New "
                "placement.");
        }
        return;
    }
    if (!current->kind) {
        symbol = symbol_at(grid, at);
        held.reset();
        return;
    }
    // What the catalog put in hand is mapped by the stroke that first draws it.
    const auto maps =
        held && held->symbol == symbol
        && !std::ranges::contains(legend, symbol, &legend_symbol::symbol);
    // One layer in hand goes over what the squares place on the others; a symbol of several
    // layers, and the inside of a room, take the squares whole.
    const auto hand = in_hand();
    const auto grounded = [&](const std::string& ink) {
        const auto own_terrain = maps && ink == held->symbol && held->layer == "terrain";
        const auto refused = own_terrain ? std::nullopt : bare(ink, true);
        if (refused) { notice = *refused; }
        return !refused;
    };
    if (*current->kind == shape_kind::room && !grounded(room_fill)) { return; }
    if (!hand && !grounded(symbol)) { return; }
    if (current->spans && !anchor) {
        anchor = at;
        return;
    }
    const auto drawing = shape{
        .kind = *current->kind,
        .from = anchor.value_or(at),
        .to = at,
        .size = brush,
        .mirror_x = mirror_x,
        .mirror_y = mirror_y};
    anchor.reset();
    const auto under =
        squares_of(drawing, grid)
        | std::views::transform([&](const point& p) { return symbol_at(grid, p); })
        | std::ranges::to<std::vector>();
    const auto inks = hand ? layered(*hand, under) : layered_ink{};
    if (!inks) {
        notice = inks.error();
        return;
    }
    const auto direct = !hand || std::ranges::any_of(under, [&](const std::string& old) {
        return old != symbol && !inks->over.contains(old);
    });
    if (hand && direct && !grounded(symbol)) { return; }
    // A room and a fill are named by what they are; any other drawing by what it painted.
    const auto title =
        *current->kind == shape_kind::room ? std::string(_("Room"))
        : *current->kind == shape_kind::fill
            //~ A step of the map editor's history: %s is what was filled in, such as dirt.
            ? string_format(_("Fill %s"), symbol_name(symbol))
            : std::string{};
    const auto grouped =
        ((maps && direct) || !inks->added.empty() || !title.empty()) && !doc->grouping();
    if (grouped) { doc->begin_group(false, title); }
    auto mapped =
        maps && direct
            ? set_legend(
                  *doc,
                  {.object = object, .layer = held->layer, .symbol = symbol, .value = held->value})
            : std::expected<void, std::string>{};
    for (const auto& added : inks->added) {
        for (const auto& line : added.lines) {
            mapped = mapped.and_then([&] {
                return set_legend(
                    *doc,
                    {.object = object,
                     .layer = line.layer,
                     .symbol = added.symbol,
                     .value = nlohmann::ordered_json::parse(line.value, nullptr, false)});
            });
        }
    }
    const auto drawn = mapped.and_then([&] {
        return stroke(
            *doc,
            {.object = object,
             .drawn = drawing,
             .symbol = symbol,
             .fill_symbol = room_fill,
             .over = inks->over});
    });
    if (grouped) { doc->end_group(); }
    notice = drawn ? "" : drawn.error();
    reload();
}

} // namespace editor::map_editor
