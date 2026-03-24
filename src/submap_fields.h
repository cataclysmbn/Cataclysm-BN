#pragma once

#include <vector>

#include "coordinates.h"
#include "item.h"
#include "units.h"

class submap;
class mapbuffer;

/**
 * Create byproducts of item combustion.  Pure function — no map context needed.
 * Appends newly spawned items to @p out.
 */
void create_burnproducts( std::vector<detached_ptr<item>> &out, const item &fuel,
                          const units::mass &burned_mass );

/**
 * Process all fields in a single loaded submap.
 *
 * This is the canonical field-processing entry point for ALL loaded submaps,
 * regardless of whether they are inside or outside the player's render bubble.
 * It is called by game::world_tick() every turn for every loaded submap.
 *
 * Handles:
 *   • Field aging and half-life decay.
 *   • Fire: terrain fuel consumption, item burning, intensity growth,
 *     horizontal/z-level spreading.
 *   • Gas fields: simplified spreading (no wind).
 *   • Electricity, fire vents, incendiary, shock vents, acid vents, bees,
 *     push-items, fungicidal gas, radiation, wandering fields.
 *
 * Known omissions (render-context-dependent):
 *   • Vehicle fire damage      (TODO: requires coordinate translation).
 *   • NPC complaints / scent   (player-centric, handled by creature_in_field).
 *   • Transparency cache flush (handled on next in-bubble load).
 *   • Monster spawning         (TODO: deferred to submap spawn system).
 *   • Fungal haze spreading    (TODO: requires fungal_effects with map context).
 *   • Item detonation in fire  (TODO: explosion creation needs map).
 *
 * @return  true if any fire field remains alive after processing.
 *          world_tick() uses this to request adjacent submap loading.
 */
auto process_fields_in_submap( submap &sm,
                               const tripoint_abs_sm &pos,
                               mapbuffer &mb ) -> bool;
