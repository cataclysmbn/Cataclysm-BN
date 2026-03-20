#pragma once

#include "proc_types.h"

class item;

namespace proc
{

auto normalize_part_fact( const item &it, part_ix ix ) -> part_fact;

} // namespace proc
