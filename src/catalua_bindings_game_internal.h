#pragma once

#include "catalua_luna.h"

namespace cata::detail
{

auto reg_game_api_creature_queries( luna::userlib &lib ) -> void;
auto reg_game_api_world_helpers( luna::userlib &lib ) -> void;

} // namespace cata::detail
