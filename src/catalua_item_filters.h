#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "sol/forward.hpp"

class Character;
class item;

using item_predicate = std::function<bool( const item & )>;

/// Result of parsing a Lua filter table for item queries.
struct lua_item_filters {
    std::vector<item_predicate> predicates;
    /// Location filters, only meaningful for items on a character; see @ref character_item_predicates_from_lua.
    std::optional<bool> worn;
    std::optional<bool> wielded;
};

/// Parses a Lua filter table into item predicates for @ref Character::get_items_if and gapi.get_items_if.
/// Unknown keys show a debug message and are ignored; @p ignored_keys are skipped silently.
auto item_filters_from_lua( const sol::table &filters,
const std::vector<std::string> &ignored_keys = {} ) -> lua_item_filters;

/// Same as @ref item_filters_from_lua, additionally turning the `worn`/`wielded` keys into
/// predicates bound to the given character.
auto character_item_predicates_from_lua( const Character &c, const sol::table &filters,
const std::vector<std::string> &ignored_keys = {} ) -> std::vector<item_predicate>;

/// Whether the item matches every predicate in the set.
auto matches_item_filters( const lua_item_filters &filters, const item &it ) -> bool;
