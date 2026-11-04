#pragma once

#include "localised_text.h"

class monster;

namespace localised
{

/// The monster's name in its form for `quantity`, as `monster::name` shows it.
auto name_of( const monster &critter, unsigned int quantity = 1 ) -> name;

} // namespace localised
