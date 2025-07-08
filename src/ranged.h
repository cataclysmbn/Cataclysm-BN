#pragma once

#include <map>
#include <optional>
#include <vector>

#include "game_constants.h"
#include "type_id.h"

class aim_activity_actor;
class avatar;
class Character;
class Creature;
class dispersion_sources;
class gun_mode;
class item;
class item_location;
class map;
class player;
class spell;
class turret_data;
class vehicle;
class shape;
class shape_factory;
struct itype;
struct tripoint;
struct projectile;
struct vehicle_part;
struct dealt_damage_instance;
struct dealt_projectile_attack;
struct damage_instance;
template<typename T>
class detached_ptr;

namespace target_handler
{
// Trajectory to target. Empty if selection was aborted or player ran out of moves
using trajectory = std::vector<tripoint>;

/**
 * Firing ranged weapon. This mode allows spending moves on aiming.
 */
trajectory mode_fire( avatar &you, aim_activity_actor &activity );

/** Throwing item */
trajectory mode_throw( avatar &you, item &relevant, bool blind_throwing );

/** Reach attacking */
trajectory mode_reach( avatar &you, item &weapon );

/** Manually firing vehicle turret */
trajectory mode_turret_manual( avatar &you, turret_data &turret );

/** Selecting target for turrets (when using vehicle controls) */
trajectory mode_turrets( avatar &you, vehicle &veh, const std::vector<vehicle_part *> &turrets );

/** Casting a spell */
trajectory mode_spell( avatar &you, spell &casting, bool no_fail, bool no_mana );

/** Executing an AoE attack given by shape. */
trajectory mode_shaped( avatar &you, shape_factory &shape_fac, aim_activity_actor &activity );

} // namespace target_handler

int range_with_even_chance_of_good_hit( int dispersion );

namespace ranged
{

/**
 * Common checks for gunmode (when firing a weapon / manually firing turret)
 * @param messages Used to store messages describing failed checks
 * @return True if all conditions are true
 */
bool gunmode_checks_common( avatar &you, const map &m, std::vector<std::string> &messages,
                            const gun_mode &gmode );

/**
 * Various checks for gunmode when firing a weapon
 * @param messages Used to store messages describing failed checks
 * @return True if all conditions are true
 */
bool gunmode_checks_weapon( avatar &you, const map &m, std::vector<std::string> &messages,
                            const gun_mode &gmode );

std::vector<Creature *> targetable_creatures( const Character &c, int range );
std::vector<Creature *> targetable_creatures( const Character &c, int range,
        const turret_data &turret );

int burst_penalty( const Character &p, const item &gun, int gun_recoil );

/** How much dispersion does one point of target's dodge add when throwing at said target? */
int throw_dispersion_per_dodge( const Character &c, bool add_encumbrance );
/** Dispersion of a thrown item, against a given target, taking into account whether or not the throw was blind. */
int throwing_dispersion( const Character &c, const item &to_throw, Creature *critter,
                         bool is_blind_throw );
int throw_cost( const Character &c, const item &to_throw );

/** Penalties potentially incurred by STR_DRAW weapons */
float get_str_draw_penalty( const item &it, const Character &p );
float str_draw_damage_modifier( const item &it, const Character &p );
float str_draw_dispersion_modifier( const item &it, const Character &p );
float str_draw_range_modifier( const item &it, const Character &p );

/** Returns shaped attack used by the gun+ammo, if set */
std::optional<shape_factory> get_shape_factory( const item &gun );

/** AoE attack, with area given by shape */
void execute_shaped_attack( const shape &sh, const projectile &proj, Creature &attacker,
                            item *source_weapon, const vehicle *in_veh = nullptr );

std::map<tripoint, double> expected_coverage( const shape &sh, const map &here, int bash_power );

void draw_cone_aoe( const tripoint &origin, const std::map<tripoint, double> &aoe );

enum class hit_tier : int {
    grazing = 0,
    normal,
    critical
};

void print_dmg_msg( Creature &target, Creature *source, const dealt_damage_instance &dealt_dam,
                    hit_tier ht = hit_tier::normal );

/**
 * Prompts to select default ammo compatible with provided gun.
 */
void prompt_select_default_ammo_for( avatar &u, item &w );

/** Returns false if a gun misfires, jams, or has other problems, else returns true. */
bool handle_gun_damage( Character &shooter, item &it );

/* Adjusts provided sight dispersion to account for character stats */
int effective_dispersion( const Character &who, int dispersion );

/** Get weapon's dispersion value modified accoring to character stats */
dispersion_sources get_weapon_dispersion( const Character &who, const item &obj );

struct aim_type {
    std::string name;
    std::string action;
    std::string help;
    bool has_threshold;
    int threshold;
};

/* Accessors for aspects of aim speed. */
std::vector<aim_type> get_aim_types( const Character &who, const item &gun );
std::pair<int, int> get_fastest_sight( const Character &who, const item &gun, double recoil );
int get_most_accurate_sight( const Character &who, const item &gun );
double aim_speed_skill_modifier( const Character &who, const skill_id &gun_skill );
double aim_speed_dex_modifier( const Character &who );
double aim_speed_encumbrance_modifier( const Character &who );
double aim_multiplier_from_volume( const item &gun );

/** Calculates aim improvement per move spent aiming at a given @param recoil */
double aim_per_move( const Character &who, const item &gun, double recoil );

/** Get maximum recoil penalty due to vehicle motion */
double recoil_vehicle( const Character &who );

/** Current total maximum recoil penalty from all sources */
double recoil_total( const Character &who );

/** How many moves does it take to aim gun to the target accuracy. */
int gun_engagement_moves( const Character &who, const item &gun, int target = 0,
                          int start = MAX_RECOIL );

/** Calculates time taken to fire gun */
int time_to_attack( const Character &p, const item &firing, const item *loc );

void make_gun_sound_effect( const Character &who, bool burst, const item &gun );

/**
 * Fire wielded gun or auxiliary gunmod (ignoring any current mode)
 * @param who Character whose stats to use (must be wielding a gun)
 * @param target Where the first shot is aimed at (may vary for later shots)
 * @param shots Maximum number of shots to fire (less may be fired in some circumstances)
 * @return Number of shots actually fired
 */
int fire_gun( Character &who, const tripoint &target, int shots = 1 );

/**
 * Fire a gun or auxiliary gunmod (ignoring any current mode)
 * @param who Character whose stats to use
 * @param target Where the first shot is aimed at (may vary for later shots)
 * @param shots Maximum number of shots to fire (less may be fired in some circumstances)
 * @param gun Item to fire (which does not necessary have to be in the characters possession)
 * @return Number of shots actually fired
 */
int fire_gun( Character &who, const tripoint &target, int shots, item &gun,
              item *ammo );

/** Expected thrown damage with a given item, given the thrower's effective strength and skill. */
auto throw_damage( const item &it, const int skill, const int str ) -> int;

/**
 * Execute a throw.
 * @param who Character whose stats to use
 * @param to_throw Item being thrown
 * @param blind_throw_from_pos Position of blind throw (if blind throwing)
 */
auto throw_item( Character &who, const tripoint &target,
                 detached_ptr<item> &&to_throw,
                 std::optional<tripoint> blind_throw_from_pos ) -> dealt_projectile_attack;

} // namespace ranged


