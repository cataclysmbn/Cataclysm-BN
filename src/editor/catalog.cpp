#include "editor/catalog.h"

#include "item_factory.h"
#include "itype.h"
#include "map/field_type.h"
#include "map/mapdata.h"
#include "mongroup.h"
#include "monstergenerator.h"
#include "mtype.h"
#include "overmap/omdata.h"
#include "string_utils.h"
#include "translations.h"
#include "trap.h"
#include "vehicle/veh_type.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <ranges>
#include <string_view>
#include <tuple>

namespace editor {
namespace {

template <typename Source> auto mod_of(const Source& src) -> std::string {
    return src.empty() ? std::string{} : src.back().second.str();
}

auto entries_of(const catalog_kind kind) -> std::vector<catalog_entry> {
    using namespace std::views;
    namespace ranges = std::ranges;
    const auto unnamed = [](const std::string& id) { return catalog_entry{.id = id, .name = id}; };
    switch (kind) {
        case catalog_kind::terrain:
            return ter_t::get_all() | transform([](const ter_t& ter) {
                       return catalog_entry{
                           .id = ter.id.str(), .name = ter.name(), .mod = mod_of(ter.src)};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::furniture:
            return furn_t::get_all() | transform([](const furn_t& furn) {
                       return catalog_entry{
                           .id = furn.id.str(), .name = furn.name(), .mod = mod_of(furn.src)};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::trap:
            return iota(std::size_t{0}, trap::count()) | transform([](const std::size_t index) {
                       const auto& found = trap_id(static_cast<int>(index)).obj();
                       return catalog_entry{.id = found.id.str(), .name = found.name()};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::field:
            return field_types::get_all() | transform([](const field_type& field) {
                       return catalog_entry{.id = field.id.str(), .name = field.get_name()};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::item:
            return item_controller->all() | transform([](const itype* type) {
                       return catalog_entry{
                           .id = type->get_id().str(),
                           .name = type->nname(1),
                           .mod = mod_of(type->src)};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::item_group:
            return item_controller->get_all_group_names()
                 | transform([&](const item_group_id& id) { return unnamed(id.str()); })
                 | ranges::to<std::vector>();
        case catalog_kind::monster:
            return MonsterGenerator::generator().get_all_mtypes()
                 | transform([](const mtype& type) {
                       return catalog_entry{
                           .id = type.id.str(), .name = type.nname(), .mod = mod_of(type.src)};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::monster_group:
            return MonsterGroupManager::get_all_group_ids()
                 | transform([&](const mongroup_id& id) { return unnamed(id.str()); })
                 | ranges::to<std::vector>();
        case catalog_kind::vehicle:
            return vehicle_prototype::get_all() | transform([](const vproto_id& id) {
                       return catalog_entry{.id = id.str(), .name = _(id.obj().name)};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::npc_template:
            return npc_template_ids() | transform(unnamed) | ranges::to<std::vector>();
        case catalog_kind::nested_mapgen:
            return nested_mapgen_ids() | transform(unnamed) | ranges::to<std::vector>();
        case catalog_kind::palette:
            return palette_ids() | transform(unnamed) | ranges::to<std::vector>();
        case catalog_kind::overmap_terrain:
            return overmap_terrains::get_all() | transform([](const oter_t& oter) {
                       return catalog_entry{.id = oter.id.str(), .name = oter.get_name()};
                   })
                 | ranges::to<std::vector>();
    }
    return {};
}

} // namespace

auto search_catalog(const catalog_query& query) -> catalog_page {
    namespace ranges = std::ranges;
    const auto matches = [&](const catalog_entry& entry) {
        return (query.mod.empty() || entry.mod == query.mod)
            && (query.filter.empty() || lcmatch(entry.id, query.filter)
                || lcmatch(entry.name, query.filter));
    };
    // What is searched for comes first: the name itself, a name or a word of it starting with it,
    // a name holding it, and last an id holding it; each by id.
    const auto wanted = to_lower_case(query.filter);
    const auto rank = [&](const catalog_entry& entry) {
        const auto name = to_lower_case(entry.name);
        if (wanted.empty() || name == wanted) { return 0; }
        if (name.starts_with(wanted)) { return 1; }
        auto from = name.find(wanted);
        while (
            from != std::string::npos && std::isalnum(static_cast<unsigned char>(name[from - 1]))) {
            from = name.find(wanted, from + 1);
        }
        if (from != std::string::npos) { return 2; }
        return name.contains(wanted) ? 3 : 4;
    };
    struct ranked_entry {
        int rank;
        catalog_entry entry;
    };
    auto ranked =
        entries_of(query.kind) | std::views::filter(matches)
        | std::views::transform([&](catalog_entry entry) {
              const auto at = rank(entry);
              return ranked_entry{.rank = at, .entry = std::move(entry)};
          })
        | ranges::to<std::vector>();
    ranges::sort(ranked, {}, [](const ranked_entry& found) {
        return std::tuple(found.rank, std::string_view(found.entry.id));
    });
    auto found = ranked | std::views::transform(&ranked_entry::entry) | ranges::to<std::vector>();
    // Entries of one name are told apart by their ids.
    auto named = std::map<std::string, int>{};
    for (const auto& entry : found) { ++named[entry.name]; }
    for (auto& entry : found) {
        if (named.at(entry.name) > 1) { entry.distinct = entry.id; }
    }
    const auto total = found.size();
    const auto begin = std::min(query.page * query.page_size, total);
    const auto end = std::min(begin + query.page_size, total);
    return {.entries = {std::make_move_iterator(found.begin() + begin),
                        std::make_move_iterator(found.begin() + end)},
            .total = total};
}

} // namespace editor
