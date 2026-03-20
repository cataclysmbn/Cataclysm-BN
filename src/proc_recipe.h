#pragma once

#include <vector>

#include "proc_types.h"
#include "requirements.h"

class recipe;

namespace proc
{

auto recipe_requirements( const recipe &rec,
                          const std::vector<part_fact> &facts ) -> requirement_data;

} // namespace proc
