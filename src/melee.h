#pragma once

class Character;
class Creature;
class monster;
class item;
struct damage_instance;
struct attack_statblock;

/*
 * statistics data for melee attacks, used for test purposes
 */
struct melee_statistic_data {
    int attack_count = 0;
    int hit_count = 0;
    int double_crit_count = 0;
    int crit_count = 0;
    int actual_crit_count = 0;
    double double_crit_chance = 0.0;
    double crit_chance = 0.0;
    int damage_amount = 0;
};

namespace melee
{

constexpr double accuracy_roll_stddev = 5.0;

float melee_hit_range( float accuracy );
melee_statistic_data get_stats();
void clear_stats();
extern melee_statistic_data melee_stats;

/**
 * What is the chance that melee attack with given accuracy hits?
 * @param acc Final to-hit of the attack minus attacker's dodge
 * See @ref melee_hit_range
 */
float hit_chance( float acc );

// If average == true, adds expected values of random rolls instead of rolling.
/** Adds all 3 types of physical damage to instance */
void roll_all_damage( const Character &c, bool crit, damage_instance &di, bool average,
                      const item &weap, const attack_statblock &attack );
/** Adds player's total bash damage to the damage instance */
void roll_bash_damage( const Character &c, bool crit, damage_instance &di, bool average,
                       const item &weap, const attack_statblock &attack );
/** Adds player's total cut damage to the damage instance */
void roll_cut_damage( const Character &c, bool crit, damage_instance &di, bool average,
                      const item &weap, const attack_statblock &attack );
/** Adds player's total stab damage to the damage instance */
void roll_stab_damage( const Character &c, bool crit, damage_instance &di, bool average,
                       const item &weap, const attack_statblock &attack );

// Temporary function that returns any attack from the weapon
// TODO: Remove
const attack_statblock &default_attack( const item &it );

const attack_statblock &pick_attack( const Character &c, const item &weapon,
                                     const Creature &target );
const attack_statblock &pick_attack( const Character &c, const item &weapon,
                                     const monster &target );
const attack_statblock &pick_attack( const Character &c, const item &weapon,
                                     const Character &target );

} // namespace melee


