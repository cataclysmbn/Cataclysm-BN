#pragma once

#include <string>
#include <vector>

#include "procgen/proc_types.h"
#include "requirements.h"

class recipe;

namespace proc
{

auto recipe_requirements( const recipe &rec,
                          const std::vector<part_fact> &facts ) -> requirement_data;
auto recipe_preview_description( const recipe &rec ) -> std::string;

} // namespace proc
