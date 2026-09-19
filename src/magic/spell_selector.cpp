#include "magic/spell_selector.h"

#include "mutation.h"
#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <ranges>
#include <utility>

namespace {

const auto trait_none = trait_id("NONE");

struct spell_class_summary {
    std::string name;
    std::size_t spell_count = 0;
    bool displayed = false;
};

auto class_category_id(const trait_id& spell_class) -> spell_selector_category_id {
    return {
        .kind = spell_selector_category_kind::spell_class,
        .spell_class = spell_class,
    };
}

} // namespace

auto normalize_spell_selector_category(spell_selector_category_id category)
    -> spell_selector_category_id {
    switch (category.kind) {
        case spell_selector_category_kind::all:
        case spell_selector_category_kind::favorites:
        case spell_selector_category_kind::other:
            category.spell_class = trait_id{};
            break;
        case spell_selector_category_kind::spell_class:
            if (category.spell_class == trait_none || category.spell_class.str().empty()) {
                category = {};
            }
            break;
        default:
            category = {};
            break;
    }
    return category;
}

auto make_spell_selector_categories(
    std::vector<spell_selector_spell> spells, const spell_selector_category_names& names)
    -> spell_selector_categories {
    auto classes_by_id = std::map<trait_id, spell_class_summary>{};
    for (const auto& spell : spells) {
        if (spell.spell_class == trait_none || !spell.spell_class.is_valid()) { continue; }

        const auto [iter, inserted] = classes_by_id.try_emplace(spell.spell_class);
        auto& summary = iter->second;
        if (inserted) { summary.name = spell.spell_class->name(); }
        ++summary.spell_count;
    }

    using namespace std::views;
    auto navigable_classes =
        classes_by_id | filter([spell_count = spells.size()](const auto& entry) {
            const auto& summary = entry.second;
            return !summary.name.empty() && summary.spell_count >= 2
                && summary.spell_count < spell_count;
        })
        | transform([](auto& entry) { return &entry; }) | std::ranges::to<std::vector>();
    std::ranges::sort(navigable_classes, [](const auto* lhs, const auto* rhs) {
        if (localized_compare(lhs->second.name, rhs->second.name)) { return true; }
        if (localized_compare(rhs->second.name, lhs->second.name)) { return false; }
        return lhs->first.str() < rhs->first.str();
    });

    auto visible_name_counts = std::map<std::string, std::size_t>{};
    for (const auto* entry : navigable_classes) { ++visible_name_counts[entry->second.name]; }

    auto result = spell_selector_categories{
        .categories = {{
            .id =
                {
                    .kind = spell_selector_category_kind::all,
                    .spell_class = trait_id{},
                },
            .name = names.all,
        }},
        .spells = std::move(spells),
    };
    result.favorite_spell_count = static_cast<std::size_t>(
        std::ranges::count_if(result.spells, &spell_selector_spell::favorite));
    result.categories.reserve(navigable_classes.size() + 3);

    if (result.favorite_spell_count > 0 && !names.favorites.empty()) {
        result.categories.push_back({
            .id =
                {
                    .kind = spell_selector_category_kind::favorites,
                    .spell_class = trait_id{},
                },
            .name = names.favorites,
        });
    }

    for (auto* const entry : navigable_classes) {
        entry->second.displayed = true;
        const auto& raw_id = entry->first.str();
        const auto conflicts_with_special_category =
            entry->second.name == names.all || entry->second.name == names.favorites
            || entry->second.name == names.other;
        const auto display_name =
            visible_name_counts[entry->second.name] > 1 || conflicts_with_special_category
                ? string_format(pgettext("spell selector category", "%1$s (%2$s)"),
                                entry->second.name, raw_id)
                : entry->second.name;
        result.categories.push_back({
            .id = class_category_id(entry->first),
            .name = display_name,
        });
    }

    for (const auto spell_index : iota(std::size_t{}, result.spells.size())) {
        auto& spell = result.spells[spell_index];
        spell.primary_category.reset();
        const auto found = classes_by_id.find(spell.spell_class);
        if (found != classes_by_id.end() && found->second.displayed) {
            spell.primary_category = class_category_id(found->first);
        }
    }

    const auto is_uncategorized = [](const auto& spell) { return !spell.primary_category; };
    const auto other_count = std::ranges::count_if(result.spells, is_uncategorized);
    if (other_count >= 2 && static_cast<std::size_t>(other_count) < result.spells.size()
        && !names.other.empty()) {
        const auto other_id = spell_selector_category_id{
            .kind = spell_selector_category_kind::other,
            .spell_class = trait_id{},
        };
        result.categories.push_back({
            .id = other_id,
            .name = names.other,
        });
        for (auto& spell : result.spells) {
            if (!spell.primary_category) { spell.primary_category = other_id; }
        }
    }
    return result;
}

auto set_spell_selector_favorite(
    spell_selector_categories& categories, const std::size_t spell_index, const bool favorite)
    -> bool {
    if (spell_index >= categories.spells.size()) { return false; }

    auto& spell = categories.spells[spell_index];
    if (spell.favorite == favorite) { return false; }

    const auto previously_had_favorites = categories.favorite_spell_count > 0;
    spell.favorite = favorite;
    if (favorite) {
        ++categories.favorite_spell_count;
    } else if (categories.favorite_spell_count > 0) {
        --categories.favorite_spell_count;
    }
    return previously_had_favorites != (categories.favorite_spell_count > 0);
}

auto spell_selector_category_labels(const spell_selector_categories& categories)
    -> std::vector<std::string> {
    using namespace std::views;
    return categories.categories | transform(&spell_selector_category::name)
         | std::ranges::to<std::vector>();
}

auto spell_selector_category_at(
    const spell_selector_categories& categories, const std::size_t category_index)
    -> spell_selector_category_id {
    return category_index < categories.categories.size()
             ? normalize_spell_selector_category(categories.categories[category_index].id)
             : spell_selector_category_id{};
}

auto spell_selector_category_index(
    const spell_selector_categories& categories, const spell_selector_category_id& category)
    -> std::size_t {
    const auto normalized_category = normalize_spell_selector_category(category);
    const auto found =
        std::ranges::find(categories.categories, normalized_category, &spell_selector_category::id);
    return found == categories.categories.end()
             ? 0
             : static_cast<std::size_t>(std::ranges::distance(categories.categories.begin(), found));
}

auto spell_matches_spell_selector_category(
    const spell_selector_categories& categories, const int spell_index,
    const std::size_t category_index) -> bool {
    if (spell_index < 0 || category_index >= categories.categories.size()) { return false; }

    const auto known_spell_index = static_cast<std::size_t>(spell_index);
    if (known_spell_index >= categories.spells.size()) { return false; }

    const auto& category = categories.categories[category_index].id;
    const auto& spell = categories.spells[known_spell_index];
    switch (category.kind) {
        case spell_selector_category_kind::all:
            return true;
        case spell_selector_category_kind::favorites:
            return spell.favorite;
        case spell_selector_category_kind::spell_class:
        case spell_selector_category_kind::other:
            return spell.primary_category && *spell.primary_category == category;
    }
    return false;
}
