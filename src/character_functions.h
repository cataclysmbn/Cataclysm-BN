#pragma once

#include "type_id.h"

#include <optional>
#include <string>

enum body_part : int;
class player;
class Character;
class Creature;
class item;
class item_reload_option;
class item_location;
class npc;
class time_duration;
class vehicle;
struct damage_unit;
struct tripoint;

template<typename T>
class detached_ptr;

namespace character_funcs
{

/** Estimate effect duration based on relevant skill */
time_duration estimate_effect_dur( int skill_lvl, const efftype_id &effect,
                                   const time_duration &error_magnitude, int threshold, const Creature &target );

/**
 * Siphons fuel (if available) from the specified vehicle into container or
 * similar via @ref game::handle_liquid. May start a character activity.
 */
void siphon( Character &ch, vehicle &veh, const itype_id &desired_liquid );

bool can_noclip( const Character &ch );

bool can_fly( Character &ch );

auto is_driving( const Character &p ) -> bool;


/** Checks for trait interactions that affect both get_book_fun_for and is_fun_to_read */
bool is_book_morale_boosted( const Character &ch, const item &book );

/** Returns enjoyability value of a book for given character. */
int get_book_fun_for( const Character &ch, const item &book );

/** Returns whether character considers it fun to read the book. */
bool is_fun_to_read( const Character &ch, const item &book );

/** Threshold for fine detail vision mod. */
constexpr float FINE_VISION_THRESHOLD = 4.0f;
/** Value for perfect fine detail vision. */
constexpr float FINE_VISION_PERFECT = 1.0f;

/**
 * @brief Calculates multiplier for the time taken to perform tasks that
 * require detail vision (reading, crafting, etc.).
 *
 * @param who Character to check for
 * @param pos Check remote spot as if the character were standing there
 *
 * @returns A value from 1.0 (unimpeded vision) to 11.0 (totally blind).
 *
 * Examples of produced values:
 *   1.0 is LIGHT_AMBIENT_LIT or brighter
 *   4.0 is a dark clear night, barely bright enough for reading and crafting
 *   6.0 is LIGHT_AMBIENT_DIM
 *   7.3 is LIGHT_AMBIENT_MINIMAL, a dark cloudy night, unlit indoors
 *   11.0 is zero light or blindness
 *
 * @{
 */
float fine_detail_vision_mod( const Character &who );
float fine_detail_vision_mod( const Character &who, const tripoint &p );
/** @} */

/**
 * @brief Checks whether character vision at given pos is good enough to see fine details.
 *
 * Fine vision is required for some interactions (reading, crafting, etc.)
 *
 * @param who Character to check for
 * @param pos Check remote spot as if the character were standing there
 *
 * @{
 */
bool can_see_fine_details( const Character &who );
bool can_see_fine_details( const Character &who, const tripoint &p );
/** @} */

enum class comfort_level {
    impossible = -999,
    uncomfortable = -7,
    neutral = 0,
    slightly_comfortable = 3,
    comfortable = 5,
    very_comfortable = 10
};

struct comfort_response_t {
    comfort_level level = comfort_level::neutral;
    std::vector<item *> aid;
};

/** Rate point's ability to serve as a bed. Only takes certain mutations into account, and not fatigue nor stimulants. */
comfort_response_t base_comfort_value( const Character &who, const tripoint &p );

/** Rate point's ability to serve as a bed. Takes all mutations, fatigue and stimulants into account. */
int rate_sleep_spot( const Character &who, const tripoint &p );

/** Checked each turn during "lying_down", returns true if the avatar falls asleep */
bool roll_can_sleep( Character &who );

/** Check whether character has an active bionic capable of interfacing with power armor. */
bool can_interface_armor( const Character &who );

/** Get the formatted name of the currently wielded item (if any) with current gun mode (if gun) */
std::string fmt_wielded_weapon( const Character &who );

/**
 * Add message describing how character feels pain.
 * @param who Character that feels the pain
 * @param val Amount of pain
 * @param bp Target body part, use bodypart_str_id::NULL_ID() if no specific body part. @todo Consider std::optional?
 */
void add_pain_msg( const Character &who, int val, const bodypart_str_id &bp );

/** Reset Character's weapon and body state (limb hp, stamina, active martial art) */
void normalize( Character &who );

/**
 * Stores an item inside another consuming moves proportional to weapon skill and volume
 * @param who Character doing the storing
 * @param container Container in which to store the item
 * @param put Item to add to the container
 * @param penalties Whether item volume and temporary effects (e.g. GRABBED, DOWNED) should be considered
 * @param base_cost Cost due to storage type
 */
void store_in_container( Character &who, item &container, detached_ptr<item> &&put, bool penalties,
                         int base_cost );

/**
 * Try to wield a contained item consuming moves proportional to weapon skill and volume.
 * @param who Character doing the wielding
 * @param container Container containing the item to be wielded
 * @param internal_item Reference to contained item to wield
 * @param penalties Whether item volume and temporary effects (e.g. GRABBED, DOWNED) should be considered
 * @param base_cost Cost due to storage type
 */
bool try_wield_contents( Character &who, item &container, item *internal_item, bool penalties,
                         int base_cost );

/**
 * Try to execute an uncanny dodge bionic ability.
 * @param who Character doing the dodging
 */
bool try_uncanny_dodge( Character &who );

/** Returns an unoccupied, safe adjacent point. */
std::optional<tripoint> pick_safe_adjacent_tile( const Character &who );

/**
 * Check if character's body part is immune to given damage.
 *
 * Note that this refers only to reduction of hp on the body part,
 * it does not account for clothing damage, pain, status effects, etc.
 *
 * @param who Character to check for
 * @param bp Body part to perform the check on
 * @param dam Damage unit to check for
 * @returns true if given damage can not reduce hp of given body part
 */
bool is_bp_immune_to( const Character &who, body_part bp, damage_unit dam );

/**
 * Returns nearby NPCs ready and willing to help with crafting or some other manual task.
 * @param who Character to be assited
 * @param max If set, limits number of helpers to that value
 */
std::vector<npc *> get_crafting_helpers( const Character &who, int max = -1 );

/** Returns Character lift strength unassisted by helpers or equipment. */
int get_lift_strength( const Character &who );

/** Returns Character lift strength without equipment including bonus from nearby helpers. */
int get_lift_strength_with_helpers( const Character &who );

/** Returns whether character can lift given value (includes bonus from helpers). */
bool can_lift_with_helpers( const Character &who, int lift_required );

/**
 * List ammo suitable for given item.
 * @param who Character who looks for ammo
 * @param base Item to select ammo for
 * @param[out] ammo_list Output
 * @param include_empty_mags Whether to include empty magazines
 * @param include_potential Include ammo that can potentially be used, but not right now
 */
bool list_ammo( const Character &who, item &base, std::vector<item_reload_option> &ammo_list,
                bool include_empty_mags, bool include_potential );

/**
 * Select suitable ammo with which to reload the item
 * @param who Character who looks for ammo
 * @param base Item to select ammo for
 * @param prompt Force display of the menu even if only one choice
 * @param include_empty_mags Allow selection of empty magazines
 * @param include_potential Include ammo that can potentially be used, but not right now
 */
item_reload_option select_ammo( const player &who, item &base, bool prompt = false,
                                bool include_empty_mags = true, bool include_potential = false );

/** Select ammo from the provided options */
item_reload_option select_ammo( const player &who, item &base,
                                std::vector<item_reload_option> opts );

/** Returns character's items that are ammo and have the matching ammo type. */
std::vector<item *> get_ammo_items( const Character &who, const ammotype &at );

/**
 * Searches for ammo or magazines that can be used to reload given item
 * @param who Character looking for ammo
 * @param obj item to be reloaded. By design any currently loaded ammunition or magazine is ignored
 * @param empty whether empty magazines should be considered as possible ammo
 * @param radius adjacent map/vehicle tiles to search. 0 for only character tile, -1 for only inventory
 */
std::vector<item *> find_ammo_items_or_mags( const Character &who, const item &obj,
        bool empty = true, int radius = 1 );

/** Searches for weapons and magazines that can be reloaded. */
std::vector<item *> find_reloadables( Character &who );

/** Counts ammo and UPS charges (lower of) for a given gun on the character. */
int ammo_count_for( const Character &who, const item &gun );

/** This shows warning to the player that their current activity will not give them xp */
void show_skill_capped_notice( const Character &who, const skill_id &id );

/** Returns true if the character has a bionic listed in the entered bionic id's available_upgrades field **/
auto has_upgraded_bionic( const Character &c, const bionic_id &b ) -> bool;
} // namespace character_funcs


