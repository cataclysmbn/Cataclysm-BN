#pragma once

#include <optional>

#include "detached_ptr.h"
#include "point.h"
#include "units.h"

class avatar;
class Character;
class item;
class map;
class turret_data;

namespace avatar_action
{

/** Eat food or fuel  'E' (or 'a') */
void eat( avatar &you );
void eat( avatar &you, item *loc );
// special rules for eating: grazing etc
// returns false if no rules are needed
bool eat_here( avatar &you );

// Standard movement; handles attacks, traps, &c. Returns false if auto move
// should be canceled
bool move( avatar &you, map &m, const tripoint &d );
inline bool move( avatar &you, map &m, point d )
{
    return move( you, m, tripoint( d, 0 ) );
}

// Handle moving from a ramp
bool ramp_move( avatar &you, map &m, const tripoint &dest );

/** Handles swimming by the player. Called by avatar_action::move(). */
void swim( map &m, avatar &you, const tripoint &p );

void autoattack( avatar &you, map &m );

void mend( avatar &you, item *loc );

/** Prompt to wield some item. */
void wield();
/** Wield specified item. */
void wield( item &loc );

/** Reload specified item. */
void reload( item &loc, bool prompt = false, bool empty = true );
/** Prompt to reload some item. */
void reload_item();
/** Reload wielded item. */
void reload_wielded( bool prompt = false );
/** Reload a wielded gun/tool */
void reload_weapon( bool try_everything = true );

/**
 * @brief Prompts to unload some item.
 *
 * The item can be a container, gun or tool.
 * If it's a gun, some gunmods can also be loaded.
 */
void unload( avatar &you );

/**
 * Checks if the weapon is valid and if the player meets certain conditions for firing it.
 * Used for validating ACT_AIM and turret weapon
 * @return True if all conditions are true, otherwise false.
 */
bool can_fire_weapon( avatar &you, const map &m, const item &weapon );

/**
 * Checks if the player meets certain conditions for firing it.
 * Only call outside of can_fire_turret if using turret from vehicle controls.
 * As can_fire_turret also checks things like "do you have two hands to fire the M2HB?"
 */
bool will_fire_turret( avatar &you, const turret_data &turret );

/**
 * Checks if the turret is valid and if the player meets certain conditions for manually firing it.
 * @param turret Turret to check.
 * @return True if all conditions are true, otherwise false.
 */
bool can_fire_turret( avatar &you, const map &m, const turret_data &turret );

/** Checks if the wielded weapon is a gun and can be fired then starts interactive aiming */
void fire_wielded_weapon( avatar &you );

/** Stores fake gun specified by the mutation and starts interactive aiming */
void fire_ranged_mutation( avatar &you, detached_ptr<item> &&fake_gun );

/** Stores fake gun specified by the bionic and starts interactive aiming */
void fire_ranged_bionic( avatar &you, detached_ptr<item> &&fake_gun,
                         const units::energy &cost_per_shot );

/**
 * Checks if the player can manually (with their 2 hands, not via vehicle controls)
 * fire a turret and then starts interactive aiming.
 * Assumes that the turret is on player position.
 */
void fire_turret_manual( avatar &you, map &m, turret_data &turret );

// Throw an item  't'
void plthrow( avatar &you, item *loc,
              const std::optional<tripoint> &blind_throw_from_pos = std::nullopt );

// Use item; also tries E,R,W  'a'
void use_item( avatar &you, item *loc = nullptr );
} // namespace avatar_action


