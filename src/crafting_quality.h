#pragma once

class Character;
class recipe;

auto crafting_quality_speed_multiplier( const Character &who, const recipe &rec ) -> float;
