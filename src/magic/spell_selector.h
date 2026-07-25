#pragma once

#include "type_id.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class spell_selector_category_kind {
    all,
    favorites,
    spell_class,
    other,
};

struct spell_selector_category_id {
    spell_selector_category_kind kind = spell_selector_category_kind::all;
    trait_id spell_class;

    auto operator==(const spell_selector_category_id&) const -> bool = default;
};

struct spell_selector_category {
    spell_selector_category_id id;
    std::string name;
};

struct spell_selector_spell {
    trait_id spell_class;
    std::optional<spell_selector_category_id> primary_category;
    bool favorite = false;
};

struct spell_selector_categories {
    std::vector<spell_selector_category> categories;
    std::vector<spell_selector_spell> spells;
    std::size_t favorite_spell_count = 0;
};

struct spell_selector_category_names {
    std::string all;
    std::string favorites;
    std::string other;
};

auto normalize_spell_selector_category(spell_selector_category_id category)
    -> spell_selector_category_id;

auto make_spell_selector_categories(
    std::vector<spell_selector_spell> spells, const spell_selector_category_names& names)
    -> spell_selector_categories;

/// Updates favorite membership and reports whether the Favorites category must be added or removed.
auto set_spell_selector_favorite(
    spell_selector_categories& categories, std::size_t spell_index, bool favorite) -> bool;

auto spell_selector_category_labels(const spell_selector_categories& categories)
    -> std::vector<std::string>;

auto spell_selector_category_at(
    const spell_selector_categories& categories, std::size_t category_index)
    -> spell_selector_category_id;

auto spell_selector_category_index(
    const spell_selector_categories& categories, const spell_selector_category_id& category)
    -> std::size_t;

auto spell_matches_spell_selector_category(
    const spell_selector_categories& categories, int spell_index, std::size_t category_index)
    -> bool;
