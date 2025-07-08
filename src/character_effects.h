#pragma once

class Character;

struct stat_mod {
    int strength = 0;
    int dexterity = 0;
    int intelligence = 0;
    int perception = 0;

    int speed = 0;
};

namespace character_effects
{

/** Returns the effect of pain on stats */
stat_mod get_pain_penalty( const Character &ch );

/** Returns the penalty to speed from starvation */
int get_kcal_speed_penalty( float kcal_percent );

/** Returns the penalty to speed from thirst */
int get_thirst_speed_penalty( int thirst );

/** Calculates character's morale cap due to fatigue */
int calc_morale_fatigue_cap( int fatigue );

/** Returns the modifier value used for vomiting effects */
double vomit_mod( const Character &ch );

/** Returns a value used when attempting to convince NPC's of something */
int talk_skill( const Character &ch );

/** Returns a value used when attempting to intimidate NPC's */
int intimidation( const Character &ch );

/** Uses morale, pain and fatigue to return the player's focus target goto value */
int calc_focus_equilibrium( const Character &who );

/** Calculates actual focus gain/loss value from focus equilibrium*/
int calc_focus_change( const Character &who );

} // namespace character_effects


