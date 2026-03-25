#pragma once

#include <string>

#include "procgen/proc_schema.h"

namespace proc
{

auto slot_indicator( const slot_data &slot, int picked ) -> std::string;

} // namespace proc
