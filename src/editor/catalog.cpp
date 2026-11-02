#include "editor/catalog.h"

#include "editor/session_file.h"
#include "filesystem.h"
#include "item_factory.h"
#include "item_group.h"
#include "itype.h"
#include "map/field_type.h"
#include "map/mapdata.h"
#include "mod_manager.h"
#include "mongroup.h"
#include "monstergenerator.h"
#include "mtype.h"
#include "overmap/omdata.h"
#include "path_info.h"
#include "string_utils.h"
#include "translations.h"
#include "trap.h"
#include "vehicle/veh_type.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace editor {
namespace {

template <typename Source> auto mod_of(const Source& src) -> std::string {
    return src.empty() ? std::string{} : src.back().second.str();
}

/// How much of a group's giving reaches an entry, and how many groups deep it may still go.
struct group_reach {
    double share = 1.0;
    int depth = 0;
};

/// Adds to `shares` how often `data` gives each item.
auto gather_shares(
    const Item_spawn_data& data, const group_reach& reach, std::map<const itype*, double>& shares)
    -> void {
    if (reach.depth == 0) { return; }
    if (const auto* const single = dynamic_cast<const Single_item_creator*>(&data)) {
        if (single->type == Single_item_creator::S_ITEM) {
            if (itype_id(single->id).is_valid()) {
                shares[&itype_id(single->id).obj()] += reach.share;
            }
        } else if (single->type == Single_item_creator::S_ITEM_GROUP) {
            const auto* const named = item_controller->get_group(item_group_id(single->id));
            if (named != nullptr) {
                gather_shares(*named, {.share = reach.share, .depth = reach.depth - 1}, shares);
            }
        }
        return;
    }
    const auto* const group = dynamic_cast<const Item_group*>(&data);
    if (group == nullptr) { return; }
    const auto& entries = group->get_items();
    // A distribution's entries share it by weight; a collection's each have a chance in 100.
    const auto whole =
        group->type == Item_group::G_DISTRIBUTION
            ? std::ranges::fold_left(
                  entries, 0,
                  [](const int sum, const auto& entry) { return sum + entry->probability; })
            : 100;
    for (const auto& entry : entries) {
        gather_shares(
            *entry,
            {.share = reach.share * entry->probability / std::max(whole, 1),
             .depth = reach.depth - 1},
            shares);
    }
}

auto entries_of(const catalog_kind kind) -> std::vector<catalog_entry> {
    using namespace std::views;
    namespace ranges = std::ranges;
    const auto unnamed = [](const std::string& id) { return catalog_entry{.id = id, .name = id}; };
    const auto grouped = [](const std::string& id) {
        return catalog_entry{.id = id, .name = group_name(id)};
    };
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
                 | transform([&](const item_group_id& id) { return grouped(id.str()); })
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
                 | transform([&](const mongroup_id& id) { return grouped(id.str()); })
                 | ranges::to<std::vector>();
        case catalog_kind::vehicle:
            return vehicle_prototype::get_all() | transform([](const vproto_id& id) {
                       return catalog_entry{.id = id.str(), .name = _(id.obj().name)};
                   })
                 | ranges::to<std::vector>();
        case catalog_kind::npc_template:
            return npc_template_ids() | transform(unnamed) | ranges::to<std::vector>();
        case catalog_kind::nested_mapgen:
            return nested_mapgen_ids() | transform(grouped) | ranges::to<std::vector>();
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

auto quoted_strings(const std::string_view text) -> std::vector<std::string_view> {
    auto found = std::vector<std::string_view>{};
    auto start = std::optional<std::size_t>{};
    for (auto at = std::size_t{0}; at < text.size(); ++at) {
        if (start && text[at] == '\\') {
            ++at;
        } else if (text[at] == '"') {
            if (start) { found.push_back(text.substr(*start + 1, at - *start - 1)); }
            start = start ? std::nullopt : std::optional(at);
        }
    }
    return found;
}

namespace {

/// How many times the game's own JSON names each id-like string.
auto uses_in_data() -> const std::unordered_map<std::string, int>& {
    static const auto counted = [] {
        const auto id_like = [](const std::string_view text) {
            return !text.empty() && text.size() < 64
                && std::ranges::all_of(
                    text,
                    [](const char c) {
                        return std::islower(static_cast<unsigned char>(c))
                            || std::isdigit(static_cast<unsigned char>(c)) || c == '_';
                    });
        };
        auto counts = std::unordered_map<std::string, int>{};
        for (const auto& path :
             get_files_from_path(".json", PATH_INFO::datadir() + "json", true, true)) {
            auto in = std::ifstream(path, std::ios::binary);
            const auto text = std::string(std::istreambuf_iterator<char>(in), {});
            for (const auto quoted : quoted_strings(text)) {
                if (id_like(quoted)) { ++counts[std::string(quoted)]; }
            }
        }
        return counts;
    }();
    return counted;
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
    auto listed = entries_of(query.kind);
    for (const auto& id : query.extra) {
        if (!ranges::contains(listed, id, &catalog_entry::id)) {
            listed.push_back({.id = id, .name = group_name(id)});
        }
    }
    auto ranked =
        listed | std::views::filter(matches) | std::views::transform([&](catalog_entry entry) {
            const auto at = rank(entry);
            return ranked_entry{.rank = at, .entry = std::move(entry)};
        })
        | ranges::to<std::vector>();
    // Among names that match alike, the game's own come before a mod's, and what its data names
    // most, then the shortest name, first.
    const auto core = mod_management::get_default_core_content_pack().str();
    const auto& uses = uses_in_data();
    ranges::sort(ranked, {}, [&](const ranked_entry& found) {
        const auto& [id, name, mod, distinct] = found.entry;
        const auto used = uses.find(id);
        return std::tuple(
            found.rank, !mod.empty() && mod != core, used == uses.end() ? 0 : -used->second,
            name.size(), std::string_view(id));
    });
    auto found = ranked | std::views::transform(&ranked_entry::entry) | ranges::to<std::vector>();
    // Entries of one name are told apart by the rest of their ids after the start all of them
    // share, which is the start the first and last of those ids share. The one that is all of that
    // start, else the first found, what the game uses most, keeps the plain name.
    struct id_bounds {
        std::string first;
        std::string last;
    };
    auto bounds = std::map<std::string, id_bounds>{};
    for (const auto& entry : found) {
        const auto [at, added] =
            bounds.try_emplace(entry.name, id_bounds{.first = entry.id, .last = entry.id});
        if (!added) {
            at->second.first = std::min(at->second.first, entry.id);
            at->second.last = std::max(at->second.last, entry.id);
        }
    }
    const auto rest_of = [&](const catalog_entry& entry) {
        const auto& [first, last] = bounds.at(entry.name);
        const auto shared = ranges::mismatch(first, last).in1 - first.begin();
        const auto rest = std::string_view(entry.id).substr(
            first == last ? entry.id.size() : shared);
        const auto from = rest.find_first_not_of('_');
        return from == std::string_view::npos ? std::string{} : std::string(rest.substr(from));
    };
    auto plain = std::map<std::string, std::string>{};
    for (const auto& entry : found) {
        if (rest_of(entry).empty()) {
            plain[entry.name] = entry.id;
        } else {
            plain.try_emplace(entry.name, entry.id);
        }
    }
    for (auto& entry : found) {
        if (plain.at(entry.name) != entry.id) { entry.distinct = rest_of(entry); }
    }
    const auto total = found.size();
    const auto begin = std::min(query.page * query.page_size, total);
    const auto end = std::min(begin + query.page_size, total);
    return {.entries = {std::make_move_iterator(found.begin() + begin),
                        std::make_move_iterator(found.begin() + end)},
            .total = total};
}

auto group_name(const std::string_view id) -> std::string {
    auto lower = std::string(id);
    std::ranges::transform(lower, lower.begin(), [](const unsigned char letter) {
        return static_cast<char>(std::tolower(letter));
    });
    return words_of_id(
        lower.starts_with("group_") ? std::string_view(lower).substr(6) : std::string_view(lower));
}

auto likely_items(const item_group_id& group) -> std::vector<const itype*> {
    const auto* const data = item_controller->get_group(group);
    if (data == nullptr) { return {}; }
    auto shares = std::map<const itype*, double>{};
    gather_shares(*data, {.share = 1.0, .depth = 16}, shares);
    auto items = shares | std::views::keys | std::ranges::to<std::vector>();
    std::ranges::sort(items, [&](const itype* const a, const itype* const b) {
        return shares[a] != shares[b]
                 ? shares[a] > shares[b]
                 : a->get_id().str() < b->get_id().str();
    });
    return items;
}

} // namespace editor
