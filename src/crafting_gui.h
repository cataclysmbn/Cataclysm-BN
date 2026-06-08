#pragma once

#include "units.h"

class Character;
class recipe;
class JsonObject;

const recipe *select_crafting_recipe( int &batch_size_out, Character &crafter,
                                       const recipe *resume_recipe = nullptr,
                                       int resume_batch_size = 0 );

void load_recipe_category( const JsonObject &jsobj );
void reset_recipe_categories();

auto query_large_volume( units::volume total_volume ) -> bool;


