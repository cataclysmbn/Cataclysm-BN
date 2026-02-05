#pragma once

class Character;
class recipe;
struct requirement_data;

auto crafting_tools_speed_multiplier( const Character &who, const recipe &rec ) -> float;
auto crafting_tools_speed_multiplier( const Character &who, const requirement_data &requirements ) -> float;
