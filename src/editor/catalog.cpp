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
#include <map>
#include <ranges>

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
    auto found = entries_of(query.kind) | std::views::filter(matches) | ranges::to<std::vector>();
    ranges::sort(found, {}, &catalog_entry::id);
    // Ordered by id, the first and last id of a name bound the start all its ids share.
    struct id_bounds {
        std::string first;
        std::string last;
    };
    auto bounds = std::map<std::string, id_bounds>{};
    for (const auto& entry : found) {
        const auto [at, added] =
            bounds.try_emplace(entry.name, id_bounds{.first = entry.id, .last = entry.id});
        if (!added) { at->second.last = entry.id; }
    }
    for (auto& entry : found) {
        const auto& [first, last] = bounds.at(entry.name);
        if (first == last) { continue; }
        const auto shared = ranges::mismatch(first, last).in1 - first.begin();
        const auto rest = std::string_view(entry.id).substr(shared);
        const auto from = rest.find_first_not_of('_');
        entry.distinct = from == std::string_view::npos ? "" : std::string(rest.substr(from));
    }
    const auto total = found.size();
    const auto begin = std::min(query.page * query.page_size, total);
    const auto end = std::min(begin + query.page_size, total);
    return {.entries = {std::make_move_iterator(found.begin() + begin),
                        std::make_move_iterator(found.begin() + end)},
            .total = total};
}

} // namespace editor
