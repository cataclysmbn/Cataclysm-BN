#pragma once

class Character;
class recipe;
class JsonObject;

const recipe *select_crafting_recipe( int &batch_size_out, Character &crafter );

void load_recipe_category( const JsonObject &jsobj );
void reset_recipe_categories();


