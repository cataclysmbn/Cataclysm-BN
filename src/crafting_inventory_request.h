#pragma once

#include "coordinates.h"
#include "game_constants.h"
#include "recipe_dictionary.h"

#include <vector>

class Character;
class inventory;
class map;
class npc;

struct crafting_inventory_request {
    tripoint_bub_ms origin = tripoint_bub_ms::zero();
    int radius = PICKUP_RANGE;
    bool clear_path = true;
    bool observation = false;
};

struct inventory_from_map_request {
    map &world;
    tripoint_bub_ms origin;
    int radius;
    const Character *actor = nullptr;
    bool assign_invlet = true;
    bool clear_path = true;
    bool observation = false;
};

struct inventory_from_map_points_request {
    map &world;
    std::vector<tripoint_bub_ms> points;
    const Character *actor = nullptr;
    bool assign_invlet = true;
    bool observation = false;
};

struct available_recipes_request {
    const inventory &crafting_inv;
    const std::vector<npc *> *helpers = nullptr;
    recipe_filter filter = nullptr;
    bool observation = false;
};
