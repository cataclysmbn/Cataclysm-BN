#pragma once

#include "procgen/proc_types.h"
#include "requirements.h"

#include <string>
#include <vector>

class recipe;

namespace proc {

auto recipe_requirements(const recipe& rec, const std::vector<part_fact>& facts)
    -> requirement_data;
auto recipe_preview_description(const recipe& rec) -> std::string;

} // namespace proc
