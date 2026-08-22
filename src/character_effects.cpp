#include "character_effects.h"

#include "bionics.h"
#include "calendar.h"
#include "character_martial_arts.h"
#include "character.h"
#include "creature.h"
#include "handle_liquid.h"
#include "itype.h"
#include "make_static.h"
#include "map_iterator.h"
#include "player.h"
#include "player_activity.h"
#include "rng.h"
#include "skill.h"
#include "submap.h"
#include "trap.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "weather_gen.h"
#include "weather.h"

static const trait_id trait_CENOBITE( "CENOBITE" );
static const trait_id trait_INT_SLIME( "INT_SLIME" );
static const trait_id trait_NAUSEA( "NAUSEA" );
static const trait_id trait_STRONGSTOMACH( "STRONGSTOMACH" );
static const trait_id trait_VOMITOUS( "VOMITOUS" );
static const trait_id trait_WEAKSTOMACH( "WEAKSTOMACH" );

static const efftype_id effect_drunk( "drunk" );
static const efftype_id effect_nausea( "nausea" );
static const efftype_id effect_weed_high( "weed_high" );

static const enchantment_value_id ench_val_PAIN_PENALTY_STR( "PAIN_PENALTY_STR" );
static const enchantment_value_id ench_val_PAIN_PENALTY_DEX( "PAIN_PENALTY_DEX" );
static const enchantment_value_id ench_val_PAIN_PENALTY_INT( "PAIN_PENALTY_INT" );
static const enchantment_value_id ench_val_PAIN_PENALTY_PER( "PAIN_PENALTY_PER" );
static const enchantment_value_id ench_val_PAIN_PENALTY_SPD( "PAIN_PENALTY_SPD" );
static const enchantment_value_id ench_val_VOMIT_MOD( "VOMIT_MOD" );

namespace character_effects
{

stat_mod get_pain_penalty( const Character &ch )
{
    stat_mod ret;
    int pain = ch.get_perceived_pain();
    if( pain <= 0 ) {
        return ret;
    }

    int stat_penalty = std::floor( std::pow( pain, 0.8f ) / 10.0f );

    ret.speed = std::pow( pain, 0.7f );

    bool ceno = ch.has_trait( trait_CENOBITE );
    ret.strength = stat_penalty + ch.bonus_from_enchantments( stat_penalty, ench_val_PAIN_PENALTY_STR );
    ret.dexterity = stat_penalty + ch.bonus_from_enchantments( stat_penalty,
                    ench_val_PAIN_PENALTY_DEX );
    ret.intelligence = stat_penalty + ch.bonus_from_enchantments( stat_penalty,
                       ench_val_PAIN_PENALTY_INT );
    ret.perception = stat_penalty + ch.bonus_from_enchantments( stat_penalty,
                     ench_val_PAIN_PENALTY_PER );
    ret.speed += ch.bonus_from_enchantments( ret.speed, ench_val_PAIN_PENALTY_SPD );

    ret.strength = std::max( ret.strength, 0 );
    ret.dexterity = std::max( ret.dexterity, 0 );
    ret.intelligence = std::max( ret.intelligence, 0 );
    ret.perception = std::max( ret.perception, 0 );
    ret.speed = std::min( ret.speed, 30 );
    return ret;
}

int get_kcal_speed_penalty( float kcal_percent )
{
    static const std::vector<std::pair<float, float>> starv_thresholds = { {
            std::make_pair( 0.0f, -90.0f ),
            std::make_pair( 0.1f, -50.f ),
            std::make_pair( 0.3f, -25.0f ),
            std::make_pair( 0.5f, 0.0f )
        }
    };
    if( kcal_percent > 0.95f ) {
        return 0;
    } else {
        return std::round( multi_lerp( starv_thresholds, kcal_percent ) );
    }
}

int get_thirst_speed_penalty( int thirst )
{
    // We die at 1200 thirst
    // Start by dropping speed really fast, but then level it off a bit
    static const std::vector<std::pair<float, float>> thirst_thresholds = {{
            std::make_pair( static_cast<float>( thirst_levels::very_thirsty ), 0.0f ),
            std::make_pair( static_cast<float>( thirst_levels::dehydrated ), -25.0f ),
            std::make_pair( static_cast<float>( thirst_levels::parched ), -50.0f ),
            std::make_pair( static_cast<float>( thirst_levels::dead ), -75.0f )
        }
    };
    return static_cast<int>( multi_lerp( thirst_thresholds, thirst ) );
}

int calc_morale_fatigue_cap( int fatigue )
{
    if( fatigue >= fatigue_levels::massive ) {
        return 20;
    } else if( fatigue >= fatigue_levels::exhausted ) {
        return 40;
    } else if( fatigue >= fatigue_levels::dead_tired ) {
        return 60;
    } else if( fatigue >= fatigue_levels::tired ) {
        return 80;
    }
    return 0;
}

double vomit_mod( const Character &ch )
{
    double mod = 1;
    mod += ch.bonus_from_enchantments( mod, ench_val_VOMIT_MOD );
    // If you're already nauseous, any food in your stomach greatly
    // increases chance of vomiting. Water doesn't provoke vomiting, though.
    if( ch.stomach.get_calories() > 0 && ch.has_effect( effect_nausea ) ) {
        mod *= 5 * ch.get_effect_int( effect_nausea );
    }
    return mod;
}

int talk_skill( const Character &ch )
{
    /** @EFFECT_INT slightly increases talking skill */

    /** @EFFECT_PER slightly increases talking skill */

    /** @EFFECT_SPEECH increases talking skill */
    int ret = ch.get_int() + ch.get_per() + ch.get_skill_level( skill_id( "speech" ) ) * 3;
    return ret;
}

int intimidation( const Character &ch )
{
    /** @EFFECT_STR increases intimidation factor */
    int ret = ch.get_str() * 2;
    if( ch.primary_weapon().is_gun() ) {
        ret += 10;
    }
    if( ch.primary_weapon().damage_melee( DT_BASH ) >= 12 ||
        ch.primary_weapon().damage_melee( DT_CUT ) >= 12 ||
        ch.primary_weapon().damage_melee( DT_STAB ) >= 12 ) {
        ret += 5;
    }

    if( ch.get_stim() > 20 ) {
        ret += 2;
    }
    if( ch.has_effect( effect_drunk ) ) {
        ret -= 4;
    }

    return ret;
}

int calc_focus_equilibrium( const Character &who )
{
    int focus_equilibrium = 100;

    int eff_morale = who.get_morale_level();
    // Factor in perceived pain, since it's harder to rest your mind while your body hurts.
    // Cenobites don't mind, though
    if( !who.has_trait( trait_CENOBITE ) ) {
        eff_morale = eff_morale - who.get_perceived_pain();
    }

    // as baseline morale is 100, calc_fatigue_cap() has to -100 to apply accurate penalties.
    if( calc_morale_fatigue_cap( who.get_fatigue() ) != 0 &&
        eff_morale > calc_morale_fatigue_cap( who.get_fatigue() ) - 100 ) {
        eff_morale = calc_morale_fatigue_cap( who.get_fatigue() ) - 100;
    }

    if( eff_morale < -99 ) {
        // At very low morale, focus is at it's minimum
        focus_equilibrium = 1;
    } else if( eff_morale <= 50 ) {
        // At -99 to +50 morale, each point of morale gives or takes 1 point of focus
        focus_equilibrium += eff_morale;
    } else {
        /* Above 50 morale, we apply strong diminishing returns.
        * Each block of 50 takes twice as many morale points as the previous one:
        * 150 focus at 50 morale (as before)
        * 200 focus at 150 morale (100 more morale)
        * 250 focus at 350 morale (200 more morale)
        * ...
        * Cap out at 400% focus gain with 3,150+ morale, mostly as a sanity check.
        */

        int block_multiplier = 1;
        int morale_left = eff_morale;
        while( focus_equilibrium < 400 ) {
            if( morale_left > 50 * block_multiplier ) {
                // We can afford the entire block.  Get it and continue.
                morale_left -= 50 * block_multiplier;
                focus_equilibrium += 50;
                block_multiplier *= 2;
            } else {
                // We can't afford the entire block.  Each block_multiplier morale
                // points give 1 focus, and then we're done.
                focus_equilibrium += morale_left / block_multiplier;
                break;
            }
        }
    }

    // This should be redundant, but just in case...
    if( focus_equilibrium < 1 ) {
        focus_equilibrium = 1;
    } else if( focus_equilibrium > 400 ) {
        focus_equilibrium = 400;
    }
    return focus_equilibrium;
}

int calc_focus_change( const Character &who )
{
    int focus_gap = calc_focus_equilibrium( who ) - who.focus_pool;

    // handle negative gain rates in a symmetric manner
    int base_change = 1;
    if( focus_gap < 0 ) {
        base_change = -1;
        focus_gap = -focus_gap;
    }

    // for every 10 points, we have a flat gain of 1 focus.
    // for every n points left over, we have an n% chance of 1 focus
    int gain = focus_gap / 10;
    if( rng( 1, 100 ) <= focus_gap % 100 ) {
        gain++;
    }

    gain *= base_change;

    return gain;
}

} // namespace character_effects
