#pragma once
#include "sol/forward.hpp"

#include <vector>

class monster;

std::vector<monster*> filter_monsters_from_lua(const sol::table& filters);
