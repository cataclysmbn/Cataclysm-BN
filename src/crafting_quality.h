#pragma once

class Character;
class recipe;

auto crafting_tools_speed_multiplier( const Character &who, const recipe &rec ) -> float;
