#include "editor/mapgen_export.h"

#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map/field.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/submap.h"
#include "monster.h"
#include "mtype.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "trap.h"
#include "type_id.h"

#include <algorithm>
#include <map>
#include <ranges>
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

} // namespace

auto export_region(const tripoint_abs_omt& at, const point_rel_omt& size)
    -> std::expected<nlohmann::ordered_json, std::string> {
    auto& here = get_map();
    const auto base = project_to<coords::sm>(at);
    auto kinds = std::vector<std::vector<kind>>{};
    auto counts = std::map<kind, std::size_t>{};
    auto placed_items = nlohmann::ordered_json::array();
    for (auto y = 0; y < 2 * SEEY * size.y(); ++y) {
        auto& row = kinds.emplace_back();
        for (auto x = 0; x < 2 * SEEX * size.x(); ++x) {
            const auto* const sm = here.get_mapbuffer().lookup_submap_in_memory(
                base + point_rel_sm(x / SEEX, y / SEEY));
            if (sm == nullptr) {
                return std::unexpected("the overmap squares are not all loaded in the map");
            }
            const auto local = point_sm_ms(x % SEEX, y % SEEY);
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
    if (ordered.size() > symbols.size()) {
        return std::unexpected("the overmap squares hold too many kinds of squares");
    }
    auto terrain = nlohmann::ordered_json::object();
    auto furniture = nlohmann::ordered_json::object();
    auto traps = nlohmann::ordered_json::object();
    auto fields = nlohmann::ordered_json::object();
    auto symbol_of = std::map<kind, char>{};
    for (const auto& [index, entry] : ordered | std::views::enumerate) {
        const auto symbol = std::string(1, symbols[index]);
        symbol_of[entry] = symbols[index];
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
    auto object = nlohmann::ordered_json{{"rows", rows}, {"terrain", terrain}};
    if (!furniture.empty()) { object["furniture"] = furniture; }
    if (!traps.empty()) { object["traps"] = traps; }
    if (!fields.empty()) { object["fields"] = fields; }
    if (!placed_items.empty()) { object["place_item"] = placed_items; }
    const auto corner = project_to<coords::ms>(at);
    const auto inside = [&](const monster& mon) {
        const auto offset = mon.abs_pos() - corner;
        return offset.z() == 0 && offset.x() >= 0 && offset.y() >= 0
            && offset.x() < 2 * SEEX * size.x() && offset.y() < 2 * SEEY * size.y();
    };
    auto monsters =
        g->all_monsters() | std::views::filter(inside)
        | std::views::transform([&](const monster& mon) {
              const auto offset = mon.abs_pos() - corner;
              return nlohmann::ordered_json{
                  {"monster", mon.type->id.str()}, {"x", offset.x()}, {"y", offset.y()}};
          })
        | std::ranges::to<std::vector>();
    std::ranges::sort(monsters, {}, [](const nlohmann::ordered_json& entry) {
        return std::tuple(entry["y"].get<int>(), entry["x"].get<int>());
    });
    if (!monsters.empty()) { object["place_monster"] = monsters; }
    auto& overmap = get_overmapbuffer(here.get_bound_dimension());
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
        {"object", object}};
}

} // namespace editor
