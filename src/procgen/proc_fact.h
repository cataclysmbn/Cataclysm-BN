#pragma once

#include <vector>

#include "procgen/proc_types.h"

class item;

namespace proc
{

struct normalize_options {
    part_ix ix = invalid_part_ix;
    int charges = 0;
    int uses = 1;
};

auto normalize_part_fact( const item &it, const normalize_options &opts ) -> part_fact;
auto normalize_part_facts( const std::vector<const item *> &items ) -> std::vector<part_fact>;

} // namespace proc
