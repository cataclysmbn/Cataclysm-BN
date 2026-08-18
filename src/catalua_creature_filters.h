#pragma once
#include <vector>
#include "sol/forward.hpp"

class monster;

std::vector<monster *> filter_monsters_from_lua( const sol::table &filters );
