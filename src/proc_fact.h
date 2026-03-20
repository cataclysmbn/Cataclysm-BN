#pragma once

#include <vector>

#include "proc_types.h"

class item;

namespace proc
{

auto normalize_part_fact( const item &it, part_ix ix ) -> part_fact;
auto normalize_part_facts( const std::vector<const item *> &items ) -> std::vector<part_fact>;

} // namespace proc
