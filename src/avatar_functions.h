#pragma once

#include "calendar.h"
#include "type_id.h"

class avatar;
class npc;
class item;
class item_location;

namespace avatar_funcs
{

/** Handles sleep attempts by the player, starts ACT_TRY_SLEEP activity */
void try_to_sleep( avatar &you, const time_duration &dur = 30_minutes );

/**
 * Try to disarm the NPC. May result in fail attempt, you receiving the wepon and instantly wielding it,
 * or the weapon falling down on the floor nearby. NPC is always getting angry with you.
 * @param target Target NPC to disarm
 */
void try_disarm_npc( avatar &you, npc &target );

/**
 * Try to steal an item from the NPC's inventory. May result in fail attempt, when NPC not notices you,
 * notices your steal attempt and getting angry with you, and you successfully stealing the item.
 * @param target Target NPC to steal from
 */
void try_steal_from_npc( avatar &you, npc &target );

/**
 * Attempt to start an activity to mend an item (fix any current faults)
 * @param obj Object to mend
 * @param interactive If true prompts player when there are multiple faults, otherwise mends the first
 */
void mend_item( avatar &you, item &obj, bool interactive = true );

/** Starts activity to install gunmod having warned user about any risk of failure or irremovable mods */
void gunmod_add( avatar &you, item &gun, item &mod );

/** Removes gunmod after first unloading any contained ammo and returns true on success */
bool gunmod_remove( avatar &you, item &gun, item &mod );

/** Returns odds for success (pair.first) and gunmod damage (pair.second) */
std::pair<int, int> gunmod_installation_odds( const avatar &you, const item &gun, const item &mod );

/** Starts activity to install toolmod */
void toolmod_add( avatar &you, item &tool, item &mod );

/** Use a tool at given location */
void use_item( avatar &you, item &used );

/** Unload an item at given location */
bool unload_item( avatar &you, item &loc );

/** List potential theft witnesses */
std::vector<npc *> list_potential_theft_witnesses( avatar &you, const faction_id &owners );

/**
 * Handle NPCs witnessing theft of their stuff.
 * @param you The dirty thief
 * @param owners Object owners
 * @return Whether there were any witnesses
 */
bool handle_theft_witnesses( avatar &you, const faction_id &owners );

} // namespace avatar_funcs


