#pragma once
#include "sol/forward.hpp"

#include <vector>

class monster;

auto filter_monsters_from_lua(const sol::table& filters) -> std::vector<monster*>;
