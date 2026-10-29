#include "editor/mapgen_export.h"

#include "editor/mapgen_legend.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map/field.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapdata.h"
#include "map/submap.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "string_utils.h"
#include "trap.h"
#include "type_id.h"
#include "vehicle/vehicle.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <tuple>
#include <vector>

namespace editor {
namespace {

/// The symbols a mapgen's rows can use, the most common square getting the first.
constexpr auto symbols = std::string_view(
    ".#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,;:!%&*+-/<>=?@^~_()[]{}|$");

struct laid_field {
    int type;
    int intensity;
    auto operator<=>(const laid_field&) const = default; // *NOPAD*
};

struct kind {
    ter_id ter;
    furn_id furn;
    trap_id trap;
    std::vector<laid_field> fields;
    auto operator<(const kind& other) const -> bool {
        return std::tuple(ter.to_i(), furn.to_i(), trap.to_i(), fields)
             < std::tuple(other.ter.to_i(), other.furn.to_i(), other.trap.to_i(), other.fields);
    }
};

/// What a palette lends an export: a symbol for each kind of square one of its symbols maps
/// alone, and every symbol it maps, which the export's own symbols leave to it.
struct lent_symbols {
    std::map<kind, char> reused;
    std::set<std::string> taken;
};

/// The kind of square that `symbol` maps alone, terrain and at most furniture and a trap, each
/// a plain id once; none when it maps anything else.
auto plain_kind(const legend_symbol& symbol) -> std::optional<kind> {
    auto found = kind{.ter = ter_id(), .furn = furn_id("f_null"), .trap = tr_null};
    auto has_terrain = false;
    auto layers = std::set<std::string>{};
    for (const auto& line : symbol.lines) {
        // Two lines of a layer: palettes the data chooses among, or one over the other.
        if (!layers.insert(line.layer).second) { return std::nullopt; }
        const auto value = nlohmann::ordered_json::parse(line.value, nullptr, false);
        if (!value.is_string()) { return std::nullopt; }
        const auto id = value.get<std::string>();
        if (line.layer == "terrain" && ter_str_id(id).is_valid()) {
            found.ter = ter_str_id(id).id();
            has_terrain = true;
        } else if (line.layer == "furniture" && furn_str_id(id).is_valid()) {
            found.furn = furn_str_id(id).id();
        } else if (line.layer == "traps" && trap_str_id(id).is_valid()) {
            found.trap = trap_str_id(id).id();
        } else {
            return std::nullopt;
        }
    }
    return has_terrain ? std::optional(found) : std::nullopt;
}

auto lent_by(const std::string& palette) -> std::expected<lent_symbols, std::string> {
    auto lent = lent_symbols{};
    if (palette.empty()) { return lent; }
    if (!palette_id(palette).is_valid()) {
        return std::unexpected("the game has no palette " + palette);
    }
    const auto legend = mapgen_legend(nlohmann::ordered_json{
        {"type", "palette"},
        {"id", "editor:export"},
        {"palettes",
         nlohmann::ordered_json::array(
             {palette})}}.dump());
    if (!legend) { return std::unexpected(legend.error()); }
    for (const auto& symbol : *legend) {
        lent.taken.insert(symbol.symbol);
        const auto found = symbol.symbol.size() == 1 ? plain_kind(symbol) : std::nullopt;
        if (found) { lent.reused.emplace(*found, symbol.symbol[0]); }
    }
    return lent;
}

/// A `place_item` entry per kind of item on the square at `x`, `y`, in the order they lie there.
auto items_at(const location_vector<item>& items, int x, int y)
    -> std::vector<nlohmann::ordered_json> {
    auto kinds = std::vector<itype_id>{};
    auto amounts = std::map<itype_id, int>{};
    for (const auto* const it : items) {
        if (amounts[it->typeId()]++ == 0) { kinds.push_back(it->typeId()); }
    }
    return kinds | std::views::transform([&](const itype_id& id) {
               auto entry = nlohmann::ordered_json{{"item", id.str()}, {"x", x}, {"y", y}};
               if (amounts[id] > 1) { entry["amount"] = amounts[id]; }
               return entry;
           })
         | std::ranges::to<std::vector>();
}

/// The fields on a square, in the order the game keeps them.
auto fields_of(const field& on) -> std::vector<laid_field> {
    return on | std::views::transform([](const auto& entry) {
               return laid_field{
                   .type = entry.first.to_i(), .intensity = entry.second.get_field_intensity()};
           })
         | std::ranges::to<std::vector>();
}

/// The JSON mapgen object of the live map's squares from `corner`, `size` squares east and south.
auto export_object(
    const tripoint_abs_ms& corner, const point_rel_ms& size, const std::string& palette)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto named = trim(palette);
    const auto lent = lent_by(named);
    if (!lent) { return std::unexpected(lent.error()); }
    auto& here = get_map();
    auto kinds = std::vector<std::vector<kind>>{};
    auto counts = std::map<kind, std::size_t>{};
    auto placed_items = nlohmann::ordered_json::array();
    for (auto y = 0; y < size.y(); ++y) {
        auto& row = kinds.emplace_back();
        for (auto x = 0; x < size.x(); ++x) {
            auto in_sm = tripoint_abs_sm{};
            auto local = point_sm_ms{};
            std::tie(in_sm, local) = project_remain<coords::sm>(corner + tripoint_rel_ms(x, y, 0));
            const auto* const sm = here.get_mapbuffer().lookup_submap_in_memory(in_sm);
            if (sm == nullptr) {
                return std::unexpected("the squares are not all loaded in the map");
            }
            row.push_back(
                {.ter = sm->get_ter(local),
                 .furn = sm->get_furn(local),
                 .trap = sm->get_trap(local),
                 .fields = fields_of(sm->get_field(local))});
            ++counts[row.back()];
            for (auto& entry : items_at(sm->get_items(local), x, y)) {
                placed_items.push_back(std::move(entry));
            }
        }
    }
    auto ordered = counts | std::views::keys | std::ranges::to<std::vector>();
    std::ranges::stable_sort(ordered, [&](const kind& a, const kind& b) {
        return counts[a] > counts[b];
    });
    auto own =
        symbols | std::views::filter([&](const char symbol) {
            return !lent->taken.contains(std::string(1, symbol));
        });
    auto next = own.begin();
    auto terrain = nlohmann::ordered_json::object();
    auto furniture = nlohmann::ordered_json::object();
    auto traps = nlohmann::ordered_json::object();
    auto fields = nlohmann::ordered_json::object();
    auto symbol_of = std::map<kind, char>{};
    for (const auto& entry : ordered) {
        if (const auto reused = lent->reused.find(entry); reused != lent->reused.end()) {
            symbol_of[entry] = reused->second;
            continue;
        }
        if (next == own.end()) {
            return std::unexpected("the squares hold too many kinds of squares");
        }
        const auto symbol = std::string(1, *next);
        symbol_of[entry] = *next++;
        terrain[symbol] = entry.ter.id().str();
        if (entry.furn != furn_id("f_null")) { furniture[symbol] = entry.furn.id().str(); }
        if (entry.trap != tr_null) { traps[symbol] = entry.trap.id().str(); }
        if (!entry.fields.empty()) {
            fields[symbol] =
                entry.fields | std::views::transform([](const laid_field& laid) {
                    return nlohmann::ordered_json{
                        {"field", field_type_id(laid.type).id().str()},
                        {"intensity", laid.intensity}};
                })
                | std::ranges::to<std::vector>();
        }
    }
    auto rows = nlohmann::ordered_json::array();
    for (const auto& row : kinds) {
        rows.push_back(
            row | std::views::transform([&](const kind& entry) { return symbol_of[entry]; })
            | std::ranges::to<std::string>());
    }
    auto object = nlohmann::ordered_json{{"rows", rows}};
    if (!named.empty()) { object["palettes"] = nlohmann::ordered_json::array({named}); }
    object["terrain"] = terrain;
    if (!furniture.empty()) { object["furniture"] = furniture; }
    if (!traps.empty()) { object["traps"] = traps; }
    if (!fields.empty()) { object["fields"] = fields; }
    if (!placed_items.empty()) { object["place_item"] = placed_items; }
    const auto inside = [&](const monster& mon) {
        const auto offset = mon.abs_pos() - corner;
        return offset.z() == 0 && offset.x() >= 0 && offset.y() >= 0 && offset.x() < size.x()
            && offset.y() < size.y();
    };
    auto monsters =
        g->all_monsters() | std::views::filter(inside)
        | std::views::transform([&](const monster& mon) {
              const auto offset = mon.abs_pos() - corner;
              auto entry = nlohmann::ordered_json{
                  {"monster", mon.type->id.str()}, {"x", offset.x()}, {"y", offset.y()}};
              if (mon.friendly != 0) { entry["friendly"] = true; }
              return entry;
          })
        | std::ranges::to<std::vector>();
    std::ranges::sort(monsters, {}, [](const nlohmann::ordered_json& entry) {
        return std::tuple(entry["y"].get<int>(), entry["x"].get<int>());
    });
    if (!monsters.empty()) { object["place_monster"] = monsters; }
    const auto offset_of = [&](const tripoint_abs_ms& pos) -> std::optional<point> {
        const auto offset = pos - corner;
        if (offset.z() != 0 || offset.x() < 0 || offset.y() < 0 || offset.x() >= size.x()
            || offset.y() >= size.y()) {
            return std::nullopt;
        }
        return point(offset.x(), offset.y());
    };
    // A vehicle and an NPC go in by the prototype the game spawns them from, as designs: those
    // without one are left out.
    auto vehicles = nlohmann::ordered_json::array();
    for (const auto& wrapped : here.get_vehicles()) {
        const auto& veh = *wrapped.v;
        // Mapgen sets a vehicle down by its origin mount; a driven one turns about another pivot.
        const auto offset = offset_of(veh.mount_to_abs(tripoint_mnt_veh::zero()));
        if (!offset || !veh.type.is_valid() || veh.type.is_null() || veh.type == vproto_id("none")
            || veh.type == vproto_id("custom")) {
            continue;
        }
        vehicles.push_back(
            {{"vehicle", veh.type.str()},
             {"x", offset->x},
             {"y", offset->y},
             {"rotation", static_cast<int>(std::lround(to_degrees(veh.face.dir())))},
             {"chance", 100},
             {"status", 0}});
    }
    if (!vehicles.empty()) { object["place_vehicles"] = vehicles; }
    auto npcs = nlohmann::ordered_json::array();
    for (const auto& guy : g->all_npcs()) {
        const auto offset = offset_of(guy.abs_pos());
        if (!offset || !npc_template_id(guy.idz).is_valid()) { continue; }
        npcs.push_back({{"class", guy.idz}, {"x", offset->x}, {"y", offset->y}});
    }
    if (!npcs.empty()) { object["place_npcs"] = npcs; }
    return object;
}

} // namespace

auto export_region(
    const tripoint_abs_omt& at, const point_rel_omt& size, const std::string& palette)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto object = export_object(
        project_to<coords::ms>(at), point_rel_ms(2 * SEEX * size.x(), 2 * SEEY * size.y()),
        palette);
    if (!object) { return std::unexpected(object.error()); }
    auto& overmap = get_overmapbuffer(get_map().get_bound_dimension());
    const auto name = [&](const tripoint_abs_omt& omt) {
        return nlohmann::ordered_json(overmap.ter(omt)->get_type_id().str());
    };
    auto om_terrain = nlohmann::ordered_json::array();
    for (auto y = 0; y < size.y(); ++y) {
        auto& row = om_terrain.emplace_back(nlohmann::ordered_json::array());
        for (auto x = 0; x < size.x(); ++x) { row.push_back(name(at + point_rel_omt(x, y))); }
    }
    return nlohmann::ordered_json{
        {"type", "mapgen"},
        {"method", "json"},
        {"om_terrain", size == point_rel_omt(1, 1) ? name(at) : om_terrain},
        {"object", *object}};
}

auto export_chunk(
    const tripoint_abs_ms& corner, const point_rel_ms& size, const std::string& palette,
    const std::string& id) -> std::expected<nlohmann::ordered_json, std::string> {
    const auto object = export_object(corner, size, palette);
    if (!object) { return std::unexpected(object.error()); }
    auto sized = nlohmann::ordered_json{{"mapgensize", {size.x(), size.y()}}};
    sized.update(*object);
    return nlohmann::ordered_json{
        {"type", "mapgen"}, {"method", "json"}, {"nested_mapgen_id", id}, {"object", sized}};
}

auto keep_symbols(nlohmann::ordered_json exported, const std::vector<legend_symbol>& legend)
    -> nlohmann::ordered_json {
    const auto layers = std::array{"terrain", "furniture", "traps", "fields"};
    const auto mapped = [&](const std::string& layer) -> const nlohmann::ordered_json& {
        static const auto none = nlohmann::ordered_json::object();
        return exported.contains(layer) ? exported[layer] : none;
    };
    // The kind of square each own symbol of the export draws; one with fields is no plain kind.
    const auto kind_of = [&](const std::string& symbol) -> std::optional<kind> {
        const auto id = [&](const char* const layer) {
            return mapped(layer).value(symbol, std::string{});
        };
        if (id("terrain").empty() || mapped("fields").contains(symbol)) { return std::nullopt; }
        return kind{
            .ter = ter_str_id(id("terrain")).id(),
            .furn = id("furniture").empty() ? furn_id("f_null") : furn_str_id(id("furniture")).id(),
            .trap = id("traps").empty() ? tr_null : trap_str_id(id("traps")).id()};
    };
    const auto same = [](const kind& a, const kind& b) { return !(a < b) && !(b < a); };
    const auto swap = [&](const char a, const char b) {
        for (auto& row : exported["rows"]) {
            auto text = row.get<std::string>();
            std::ranges::transform(text, text.begin(), [&](const char c) {
                return c == a ? b : c == b ? a : c;
            });
            row = text;
        }
        for (const auto* const layer : layers) {
            if (!exported.contains(layer)) { continue; }
            auto& members = exported[layer];
            const auto at = std::string(1, a);
            const auto bt = std::string(1, b);
            auto moved = nlohmann::ordered_json::object();
            for (const auto& [key, value] : members.items()) {
                moved[key == at ? bt : key == bt ? at : key] = value;
            }
            members = std::move(moved);
        }
    };
    // A symbol kept keeps its kind, though a later symbol of the legend maps it too.
    auto kept = std::vector<std::string>{};
    for (const auto& symbol : legend) {
        const auto wanted = symbol.symbol.size() == 1 ? plain_kind(symbol) : std::nullopt;
        if (!wanted) { continue; }
        const auto own =
            mapped("terrain").items()
            | std::views::transform([](const auto& member) { return member.key(); })
            | std::ranges::to<std::vector>();
        const auto found = std::ranges::find_if(own, [&](const std::string& key) {
            const auto drawn = kind_of(key);
            return drawn && same(*drawn, *wanted) && !std::ranges::contains(kept, key);
        });
        if (found == own.end()) { continue; }
        if (*found != symbol.symbol) { swap(found->front(), symbol.symbol.front()); }
        kept.push_back(symbol.symbol);
    }
    return exported;
}

} // namespace editor
