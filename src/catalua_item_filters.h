#pragma once

#include <functional>
#include <vector>

#include "sol/forward.hpp"

class item;

using item_predicate = std::function<bool( const item & )>;

/// Parses a Lua filter table into item predicates for @ref Character::get_items_if.
/// Unknown keys show a debug message and are ignored.
auto item_filters_from_lua( const sol::table &filters ) -> std::vector<item_predicate>;
