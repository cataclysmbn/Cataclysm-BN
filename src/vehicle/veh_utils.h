#pragma once

#include "requirements.h"
#include "type_id.h"

#include <functional>
#include <vector>

class Character;
class item;
class vehicle;
class vpart_info;
struct vehicle_part;

namespace veh_utils {
/** Calculates xp for interacting with given part. */
int calc_xp_gain(const vpart_info& vp, const skill_id& sk, const Character& who);
/**
 * Crafting-component filter for installing this part.
 * Fluid tanks also accept their base container when it only holds liquid.
 */
auto install_component_filter(const vpart_info& vp) -> std::function<bool(const item&)>;
/**
 * False when this requirement slot is the fluid-tank base item, so consume
 * must leave the liquid in the container instead of dumping it.
 */
auto should_unload_install_component(const vpart_info& vp, const std::vector<item_comp>& comps)
    -> bool;
/**
 * Returns a part on a given vehicle that a given character can repair.
 * Prefers the most damaged parts that don't need replacements.
 * If no such part exists, returns a null part.
 */
vehicle_part& most_repairable_part(vehicle& veh, Character& who_arg, bool only_repairable = false);
/**
 * Repairs a given part on a given vehicle by given character.
 * Awards xp and consumes components.
 */
bool repair_part(vehicle& veh, vehicle_part& pt, Character& who);
} // namespace veh_utils
