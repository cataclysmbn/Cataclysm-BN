#include "melee.h"

#include <algorithm>
#include <numeric>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "avatar_functions.h"
#include "bodypart.h"
#include "bionics.h"
#include "cached_options.h"
#include "calendar.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "cata_utility.h"
#include "character.h"
#include "character_functions.h"
#include "character_martial_arts.h"
#include "creature.h"
#include "damage.h"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "line.h"
#include "magic_enchantment.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "martialarts.h"
#include "messages.h"
#include "monattack.h"
#include "monster.h"
#include "mtype.h"
#include "mutation.h"
#include "npc.h"
#include "output.h"
#include "player.h"
#include "pldata.h"
#include "point.h"
#include "projectile.h"
#include "rng.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

static const bionic_id bio_cqb( "bio_cqb" );
static const bionic_id bio_memory( "bio_memory" );

static const itype_id itype_fur( "fur" );
static const itype_id itype_leather( "leather" );
static const itype_id itype_rag( "rag" );

static const matec_id tec_none( "tec_none" );
static const matec_id WBLOCK_1( "WBLOCK_1" );
static const matec_id WBLOCK_2( "WBLOCK_2" );
static const matec_id WBLOCK_3( "WBLOCK_3" );

static const skill_id skill_stabbing( "stabbing" );
static const skill_id skill_cutting( "cutting" );
static const skill_id skill_unarmed( "unarmed" );
static const skill_id skill_bashing( "bashing" );
static const skill_id skill_melee( "melee" );

static const efftype_id effect_badpoison( "badpoison" );
static const efftype_id effect_beartrap( "beartrap" );
static const efftype_id effect_bouldering( "bouldering" );
static const efftype_id effect_contacts( "contacts" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_drunk( "drunk" );
static const efftype_id effect_grabbed( "grabbed" );
static const efftype_id effect_grabbing( "grabbing" );
static const efftype_id effect_heavysnare( "heavysnare" );
static const efftype_id effect_hit_by_player( "hit_by_player" );
static const efftype_id effect_lightsnare( "lightsnare" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_poison( "poison" );
static const efftype_id effect_stunned( "stunned" );

static const trait_id trait_ARM_TENTACLES( "ARM_TENTACLES" );
static const trait_id trait_ARM_TENTACLES_4( "ARM_TENTACLES_4" );
static const trait_id trait_ARM_TENTACLES_8( "ARM_TENTACLES_8" );
static const trait_id trait_BEAK_PECK( "BEAK_PECK" );
static const trait_id trait_CLAWS_TENTACLE( "CLAWS_TENTACLE" );
static const trait_id trait_DEBUG_NIGHTVISION( "DEBUG_NIGHTVISION" );
static const trait_id trait_DEFT( "DEFT" );
static const trait_id trait_DRUNKEN( "DRUNKEN" );
static const trait_id trait_HYPEROPIC( "HYPEROPIC" );
static const trait_id trait_POISONOUS2( "POISONOUS2" );
static const trait_id trait_POISONOUS( "POISONOUS" );
static const trait_id trait_PROF_SKATER( "PROF_SKATER" );
static const trait_id trait_VINES2( "VINES2" );
static const trait_id trait_VINES3( "VINES3" );

static const trait_flag_str_id trait_flag_NEED_ACTIVE_TO_MELEE( "NEED_ACTIVE_TO_MELEE" );
static const trait_flag_str_id trait_flag_UNARMED_BONUS( "UNARMED_BONUS" );

static const efftype_id effect_amigara( "amigara" );

static const species_id HUMAN( "HUMAN" );

void player_hit_message( Character *attacker, const std::string &message,
                         Creature &t, int dam, bool crit = false );
int  stumble( Character &u, const item &weap );
std::string melee_message( const ma_technique &tec, Character &p,
                           const dealt_damage_instance &ddi );

/* Melee Functions!
 * These all belong to class player.
 *
 * STATE QUERIES
 * bool is_armed() - True if we are armed with any item.
 * bool unarmed_attack() - True if we are attacking with a "fist" weapon
 * (cestus, bionic claws etc.) or no weapon.
 *
 * HIT DETERMINATION
 * int hit_roll() - The player's hit roll, to be compared to a monster's or
 *   player's dodge_roll().  This handles weapon bonuses, weapon-specific
 *   skills, torso encumbrance penalties and drunken master bonuses.
 */

item &Character::used_weapon() const
{
    return martial_arts_data->selected_force_unarmed() ? null_item_reference() : primary_weapon();
}

item &Character::primary_weapon() const
{
    if( !get_body().contains( body_part_arm_r ) ) {
        return null_item_reference();
    }
    return *get_part( body_part_arm_r ).wielding.wielded;
}

std::vector<item *> Character::wielded_items() const
{
    if( !get_body().contains( body_part_arm_r ) ) {
        return {};
    }

    if( !get_part( body_part_arm_r ).wielding.wielded ) {
        return {};
    }

    return {& *get_part( body_part_arm_r ).wielding.wielded};
}

detached_ptr<item> Character::set_primary_weapon( detached_ptr<item> &&new_weapon )
{
    auto &body = get_body();
    auto iter = body.find( body_part_arm_r );
    if( iter != body.end() ) {
        bodypart &part = get_part( body_part_arm_r );
        detached_ptr<item> d = part.wielding.wielded.release();
        part.wielding.wielded = std::move( new_weapon );
        return d;
    }
    return std::move( new_weapon );
}

detached_ptr<item> Character::remove_primary_weapon()
{
    auto &body = get_body();
    auto iter = body.find( body_part_arm_r );
    detached_ptr<item> ret;
    if( iter != body.end() ) {
        ret = get_part( body_part_arm_r ).wielding.wielded.release();
        clear_npc_ai_info_cache( npc_ai_info::ideal_weapon_value );
    }
    return ret;
}

bool Character::is_armed() const
{
    return !primary_weapon().is_null();
}

bool Character::unarmed_attack() const
{
    const item &weap = used_weapon();
    return weap.is_null() || weap.has_flag( flag_UNARMED_WEAPON );
}

bool Character::handle_melee_wear( item &shield, float wear_multiplier )
{
    if( wear_multiplier <= 0.0f ) {
        return false;
    }
    // Here is where we handle wear and tear on things we use as melee weapons or shields.
    if( shield.is_null() ) {
        return false;
    }

    // UNBREAKABLE_MELEE items can't be damaged through melee combat usage.
    if( shield.has_flag( flag_UNBREAKABLE_MELEE ) ) {
        return false;
    }

    /** @EFFECT_DEX reduces chance of damaging your melee weapon */

    /** @EFFECT_STR increases chance of damaging your melee weapon (NEGATIVE) */

    /** @EFFECT_MELEE reduces chance of damaging your melee weapon */
    const float stat_factor = dex_cur / 2.0f
                              + get_skill_level( skill_melee )
                              + ( 64.0f / std::max( str_cur, 4 ) );

    float material_factor;

    itype_id weak_comp;
    itype_id big_comp = itype_id::NULL_ID();
    // Fragile items that fall apart easily when used as a weapon due to poor construction quality
    if( shield.has_flag( flag_FRAGILE_MELEE ) ) {
        const float fragile_factor = 6;
        int weak_chip = INT_MAX;
        units::volume big_vol = 0_ml;

        // Items that should have no bearing on durability
        const std::set<itype_id> blacklist = { itype_rag, itype_leather, itype_fur };

        for( auto &comp : shield.get_components() ) {
            if( !blacklist.contains( comp->typeId() ) ) {
                if( weak_chip > comp->chip_resistance() ) {
                    weak_chip = comp->chip_resistance();
                    weak_comp = comp->typeId();
                }
            }
            if( comp->volume() > big_vol ) {
                big_vol = comp->volume();
                big_comp = comp->typeId();
            }
        }
        material_factor = ( weak_chip < INT_MAX ? weak_chip : shield.chip_resistance() ) / fragile_factor;
    } else {
        material_factor = shield.chip_resistance();
    }
    int damage_chance = static_cast<int>( stat_factor * material_factor / wear_multiplier );
    // DURABLE_MELEE items are made to hit stuff and they do it well, so they're considered to be a lot tougher
    // than other weapons made of the same materials.
    if( shield.has_flag( flag_DURABLE_MELEE ) ) {
        damage_chance *= 4;
    }

    if( damage_chance > 0 && !one_in( damage_chance ) ) {
        return false;
    }

    auto str = shield.tname(); // save name before we apply damage

    if( !shield.inc_damage() ) {
        add_msg_player_or_npc( m_bad, _( "Your %s is damaged by the force of the blow!" ),
                               _( "<npcname>'s %s is damaged by the force of the blow!" ),
                               str );
        return false;
    }

    // Dump its contents on the ground
    // Destroy irremovable mods, if any
    shield.contents.remove_top_items_with( []( detached_ptr<item> &&mod ) {
        if( mod->is_gunmod() && !mod->is_irremovable() ) {
            return detached_ptr<item>();
        }
        return std::move( mod );
    } );

    shield.contents.spill_contents( pos() );

    shield.detach();

    // Breakdown fragile weapons into components
    if( shield.has_flag( flag_FRAGILE_MELEE ) && !shield.get_components().empty() ) {
        add_msg_player_or_npc( m_bad, _( "Your %s breaks apart!" ),
                               _( "<npcname>'s %s breaks apart!" ),
                               str );

        for( detached_ptr<item> &comp : shield.remove_components() ) {
            int break_chance = comp->typeId() == weak_comp ? 2 : 8;

            if( one_in( break_chance ) ) {
                add_msg_if_player( m_bad, _( "The %s is destroyed!" ), comp->tname() );
                continue;
            }

            if( comp->typeId() == big_comp && !is_armed() ) {
                wield( std::move( comp ) );
            } else {
                g->m.add_item_or_charges( pos(), std::move( comp ) );
            }
        }
    } else {
        add_msg_player_or_npc( m_bad, _( "Your %s is destroyed by the blow!" ),
                               _( "<npcname>'s %s is destroyed by the blow!" ),
                               str );
    }

    return true;
}

float Character::get_hit_weapon( const item &weap, const attack_statblock &attack ) const
{
    /** @EFFECT_UNARMED improves hit chance for unarmed weapons */
    /** @EFFECT_BASHING improves hit chance for bashing weapons */
    /** @EFFECT_CUTTING improves hit chance for cutting weapons */
    /** @EFFECT_STABBING improves hit chance for piercing weapons */
    auto skill = get_skill_level( weap.melee_skill() );

    // CQB bionic acts as a lower bound providing item uses a weapon skill
    if( skill < BIO_CQB_LEVEL && has_active_bionic( bio_cqb ) ) {
        skill = BIO_CQB_LEVEL;
    }

    /** @EFFECT_MELEE improves hit chance for all items (including non-weapons) */
    return ( skill / 3.0f ) + ( get_skill_level( skill_melee ) / 2.0f ) + attack.to_hit +
           weap.get_melee_hit_bonus();
}

float Character::get_melee_hit( const item &weapon, const attack_statblock &attack ) const
{
    // Dexterity, skills, weapon and martial arts
    float hit = Character::get_hit_base() + get_hit_weapon( weapon, attack ) + mabuff_tohit_bonus();

    // Farsightedness makes us hit worse
    if( has_trait( trait_HYPEROPIC ) && !worn_with_flag( flag_FIX_FARSIGHT ) &&
        !has_effect( effect_contacts ) ) {
        hit -= 2.0f;
    }

    //Unstable ground chance of failure
    if( has_effect( effect_bouldering ) ) {
        hit *= 0.75f;
    }

    hit *= std::max( 0.25f, 1.0f - encumb( body_part_torso ) / 100.0f );

    return hit;
}

float Character::hit_roll( const item &weapon, const attack_statblock &attack ) const
{
    return melee::melee_hit_range( get_melee_hit( weapon, attack ) );
}

void Character::add_miss_reason( const std::string &reason, const unsigned int weight )
{
    melee_miss_reasons.add( reason, weight );

}

void Character::clear_miss_reasons()
{
    melee_miss_reasons.clear();
}

std::string Character::get_miss_reason()
{
    // everything that lowers accuracy in player::hit_roll()
    // adding it in hit_roll() might not be safe if it's called multiple times
    // in one turn
    add_miss_reason(
        _( "Your torso encumbrance throws you off-balance." ),
        roll_remainder( encumb( body_part_torso ) / 10.0 ) );
    const int farsightedness = 2 * ( has_trait( trait_HYPEROPIC ) &&
                                     !worn_with_flag( flag_FIX_FARSIGHT ) &&
                                     !has_effect( effect_contacts ) );
    add_miss_reason(
        _( "You can't hit reliably due to your farsightedness." ),
        farsightedness );

    const std::string *const reason = melee_miss_reasons.pick();
    if( reason == nullptr ) {
        return std::string();
    }
    return *reason;
}

void melee::roll_all_damage( const Character &c, bool crit, damage_instance &di, bool average,
                             const item &weap, const attack_statblock &attack )
{
    roll_bash_damage( c, crit, di, average, weap, attack );
    roll_cut_damage( c, crit, di, average, weap, attack );
    roll_stab_damage( c, crit, di, average, weap, attack );
    for( int i = 0; i < NUM_DT; i++ ) {
        auto dt = static_cast<damage_type>( i );
        if( dt == DT_NULL ) {
            continue; // Escape from DT_NULL
        }
        roll_non_physical_damage( c, crit, di, average, weap, attack, dt );
    }
}

static void melee_train( Character &p, int lo, int hi, const item &weap )
{
    player &u = *p.as_player();
    u.practice( skill_melee, std::ceil( rng( lo, hi ) / 2.0 ), hi );

    // allocate XP proportional to damage stats
    // Pure unarmed needs a special case because it has 0 weapon damage
    int cut = weap.damage_melee( DT_CUT );
    int stab = weap.damage_melee( DT_STAB );
    int bash = weap.damage_melee( DT_BASH ) + ( weap.is_null() ? 1 : 0 );

    float total = std::max( cut + stab + bash, 1 );

    // Unarmed may deal cut, stab, and bash damage depending on the weapon
    if( weap.is_unarmed_weapon() ) {
        u.practice( skill_unarmed, std::ceil( 1 * rng( lo, hi ) ), hi );
    } else {
        u.practice( skill_cutting, std::ceil( cut / total * rng( lo, hi ) ), hi );
        u.practice( skill_stabbing, std::ceil( stab / total * rng( lo, hi ) ), hi );
        u.practice( skill_bashing, std::ceil( bash / total * rng( lo, hi ) ), hi );
    }
}

// Broken out of below function for use by other places in the code
int Character::get_melee_stamina_cost( const item &weapon )
{
    const int melee = get_skill_level( skill_melee );

    // Previously calculated as 2_gram * std::max( 1, str_cur )
    const int weight_cost = weapon.weight() / ( 16_gram );
    const int encumbrance_cost = roll_remainder( ( encumb( body_part_arm_l ) + encumb(
                                     body_part_arm_r ) ) *
                                 2.0f );
    const int deft_bonus = has_trait( trait_DEFT ) ? 50 : 0;
    const float strbonus = 1 / ( 2 + ( str_cur * 0.25f ) );
    const float skill_cost = std::max( 0.667f, ( ( 30.0f - melee ) / 30.0f ) );
    /** @EFFECT_MELEE and @EFFECT_STR reduce stamina cost of melee attacks */
    return ( weight_cost + encumbrance_cost - deft_bonus + 50 ) * skill_cost *
           ( 0.75f + strbonus );
}

// Melee calculation is in parts. This sets up the attack, then in deal_melee_attack,
// we calculate if we would hit. In Creature::deal_melee_hit, we calculate if the target dodges.
void Character::melee_attack( Creature &t, bool allow_special, const matec_id *force_technique,
                              bool allow_unarmed )
{
    melee::melee_stats.attack_count += 1;
    // Old check for if the target is player retained in case you somehow hit yourself
    if( !t.is_player() && is_player() ) {
        t.add_effect( effect_hit_by_player, 10_minutes ); // Flag as attacked by us for AI
    }
    if( is_mounted() ) {
        auto mons = mounted_creature.get();
        if( mons->has_flag( MF_RIDEABLE_MECH ) ) {
            if( !mons->check_mech_powered() ) {
                add_msg( m_bad, _( "The %s has dead batteries and will not move its arms." ),
                         mons->get_name() );
                return;
            }
            if( mons->type->has_special_attack( "SMASH" ) && one_in( 3 ) ) {
                add_msg( m_info, _( "The %s hisses as its hydraulic arm pumps forward!" ),
                         mons->get_name() );
                mattack::smash_specific( mons, &t );
            } else {
                mons->use_mech_power( -2 );
                mons->melee_attack( t );
            }
            mod_moves( -mons->type->attack_cost );
            return;
        }
    }
    item &cur_weapon = allow_unarmed ? used_weapon() : primary_weapon();
    const attack_statblock &attack = melee::pick_attack( *this, cur_weapon, t );
    int hit_spread = t.deal_melee_attack( this, hit_roll( cur_weapon, attack ) );
    const bool attack_hit = hit_spread >= 0;

    if( cur_weapon.attack_cost() > attack_cost( cur_weapon ) * 20 ) {
        add_msg( m_bad, _( "This weapon is too unwieldy to attack with!" ) );
        return;
    }

    int move_cost = attack_cost( cur_weapon );

    if( !attack_hit ) {
        int stumble_pen = stumble( *this, cur_weapon );
        sfx::generate_melee_sound( pos(), t.pos(), false, false );
        if( is_player() ) { // Only display messages if this is the player

            if( one_in( 2 ) ) {
                const std::string reason_for_miss = get_miss_reason();
                if( !reason_for_miss.empty() ) {
                    add_msg( reason_for_miss );
                }
            }

            if( can_miss_recovery( cur_weapon ) ) {
                ma_technique tec = martial_arts_data->get_miss_recovery_tec( cur_weapon );
                add_msg( _( tec.avatar_message ), t.disp_name() );
            } else if( stumble_pen >= 60 ) {
                add_msg( m_bad, _( "You miss and stumble with the momentum." ) );
            } else if( stumble_pen >= 10 ) {
                add_msg( _( "You swing wildly and miss." ) );
            } else {
                add_msg( _( "You miss." ) );
            }
        } else if( g->u.sees( *this ) ) {
            if( stumble_pen >= 60 ) {
                add_msg( _( "%s misses and stumbles with the momentum." ), name );
            } else if( stumble_pen >= 10 ) {
                add_msg( _( "%s swings wildly and misses." ), name );
            } else {
                add_msg( _( "%s misses." ), name );
            }
        }


        // Practice melee and relevant weapon skill (if any) except when using CQB bionic
        if( !has_active_bionic( bio_cqb ) ) {
            melee_train( *this, 2, 5, cur_weapon );
        }

        // Cap stumble penalty, heavy weapons are quite weak already
        move_cost += std::min( 60, stumble_pen );
        if( martial_arts_data->has_miss_recovery_tec( cur_weapon ) ) {
            move_cost /= 2;
        }

        // trigger martial arts on-miss effects
        martial_arts_data->ma_onmiss_effects( *this );
    } else {
        melee::melee_stats.hit_count += 1;
        // Remember if we see the monster at start - it may change
        const bool seen = g->u.sees( t );
        // Start of attacks.
        const bool critical_hit = scored_crit( t.dodge_roll(), cur_weapon, attack );
        if( critical_hit ) {
            melee::melee_stats.actual_crit_count += 1;
        }
        damage_instance d;
        melee::roll_all_damage( *this, critical_hit, d, false, cur_weapon, attack );

        const bool has_force_technique = force_technique;

        // Pick one or more special attacks
        matec_id technique_id;
        if( allow_special && !has_force_technique ) {
            technique_id = pick_technique( t, cur_weapon, critical_hit, false, false );
        } else if( has_force_technique ) {
            technique_id = *force_technique;
        } else {
            technique_id = tec_none;
        }

        // if you have two broken arms you aren't doing any martial arts
        // and your hits are not going to hurt very much
        if( get_working_arm_count() < 1 ) {
            technique_id = tec_none;
            d.mult_damage( 0.1 );
        }
        // polearms and pikes (but not spears) do less damage to adjacent targets
        if( cur_weapon.reach_range( *this ) > 1 && !reach_attacking &&
            cur_weapon.has_flag( flag_POLEARM ) ) {
            d.mult_damage( 0.7 );
        }
        // If stamina is below 25%, damage starts to scale down linearly until reaching 50% at 0 stamina
        if( get_stamina() < get_stamina_max() / 4 ) {
            d.mult_damage( 0.5 + ( static_cast<float>( get_stamina() ) / static_cast<float>
                                   ( get_stamina_max() ) * 2.0f ) );
        }

        const ma_technique &technique = technique_id.obj();

        // Handles effects as well; not done in melee_affect_*
        if( technique.id != tec_none ) {
            perform_technique( technique, t, d, move_cost );
        }

        // Proceed with melee attack.
        if( !t.is_dead_state() ) {
            // Handles speed penalties to monster & us, etc
            std::string specialmsg = melee_special_effects( t, d, cur_weapon );

            // gets overwritten with the dealt damage values
            dealt_damage_instance dealt_dam;
            dealt_damage_instance dealt_special_dam;
            if( allow_special ) {
                perform_special_attacks( t, dealt_special_dam );
            }
            t.deal_melee_hit( this, &cur_weapon, hit_spread, critical_hit, d, dealt_dam );
            if( dealt_special_dam.type_damage( DT_CUT ) > 0 ||
                dealt_special_dam.type_damage( DT_STAB ) > 0 ||
                ( cur_weapon.is_null() && ( dealt_dam.type_damage( DT_CUT ) > 0 ||
                                            dealt_dam.type_damage( DT_STAB ) > 0 ) ) ) {
                if( has_trait( trait_POISONOUS ) ) {
                    add_msg_if_player( m_good, _( "You poison %s!" ), t.disp_name() );
                    t.add_effect( effect_poison, 6_turns );
                } else if( has_trait( trait_POISONOUS2 ) ) {
                    add_msg_if_player( m_good, _( "You inject your venom into %s!" ),
                                       t.disp_name() );
                    t.add_effect( effect_badpoison, 6_turns );
                }
            }

            // Make a rather quiet sound, to alert any nearby monsters
            if( !is_quiet() ) { // check martial arts silence
                //sound generated later
                sounds::sound( pos(), 8, sounds::sound_t::combat, "whack!" );
            }
            std::string material = "flesh";
            if( t.is_monster() ) {
                const monster *m = dynamic_cast<const monster *>( &t );
                if( m->made_of( material_id( "steel" ) ) ) {
                    material = "steel";
                }
            }
            sfx::generate_melee_sound( pos(), t.pos(), true, t.is_monster(), material );
            int dam = dealt_dam.total_damage();
            melee::melee_stats.damage_amount += dam;

            // Practice melee and relevant weapon skill (if any) except when using CQB bionic
            if( !has_active_bionic( bio_cqb ) ) {
                melee_train( *this, 5, 10, cur_weapon );
            }

            if( dam >= 5 && has_artifact_with( AEP_SAP_LIFE ) ) {
                healall( rng( dam / 10, dam / 5 ) );
            }

            // Treat monster as seen if we see it before or after the attack
            if( seen || g->u.sees( t ) ) {
                std::string message = melee_message( technique, *this, dealt_dam );
                player_hit_message( this, message, t, dam, critical_hit );
            } else {
                add_msg_player_or_npc( m_good, _( "You hit something." ),
                                       _( "<npcname> hits something." ) );
            }

            if( !specialmsg.empty() ) {
                add_msg_if_player( m_neutral, specialmsg );
            }

            if( critical_hit ) {
                // trigger martial arts on-crit effects
                martial_arts_data->ma_oncrit_effects( *this );
            }

        }

        t.check_dead_state();
        did_hit( t );

        if( t.is_dead_state() ) {
            // trigger martial arts on-kill effects
            martial_arts_data->ma_onkill_effects( *this );
        }
    }

    const int mod_sta = -get_melee_stamina_cost( cur_weapon );
    mod_stamina( std::min( -50, mod_sta ) );
    add_msg( m_debug, "Stamina burn: %d", std::min( -50, mod_sta ) );
    mod_moves( -move_cost );
    // trigger martial arts on-attack effects
    martial_arts_data->ma_onattack_effects( *this );
    // some things (shattering weapons) can harm the attacking creature.
    check_dead_state();
    if( t.as_character() ) {
        dealt_projectile_attack dp = dealt_projectile_attack();
        t.as_character()->on_hit( this, bodypart_str_id::NULL_ID().id(), &dp );
    }

    cata::run_hooks( "on_creature_melee_attacked", [ &, this]( auto & params ) {
        params["char"] = this;
        params["target"] = &t;
        params["success"] = attack_hit;
    } );

}

void Character::reach_attack( const tripoint &p )
{
    matec_id force_technique = tec_none;
    /** @EFFECT_MELEE >5 allows WHIP_DISARM technique */
    if( primary_weapon().has_flag( flag_WHIP ) && ( get_skill_level( skill_melee ) > 5 ) &&
        one_in( 3 ) ) {
        force_technique = matec_id( "WHIP_DISARM" );
    }

    map &here = get_map();
    Creature *critter = g->critter_at( p );
    // Original target size, used when there are monsters in front of our target
    const int target_size = critter != nullptr ? static_cast<int>( critter->get_size() + 1 ) : 2;
    // Reset last target pos
    last_target_pos = std::nullopt;
    // Max out recoil
    recoil = MAX_RECOIL;

    int move_cost = attack_cost( primary_weapon() );
    int skill = std::min( 10, get_skill_level( skill_stabbing ) );
    int t = 0;
    std::vector<tripoint> path = line_to( pos(), p, t, 0 );
    tripoint last_point = pos();
    path.pop_back(); // Last point is our critter
    for( const tripoint &path_point : path ) {
        // Possibly hit some unintended target instead
        Creature *inter = g->critter_at( path_point );
        int inter_block_size = inter != nullptr ? ( inter->get_size() + 1 ) : 2;
        /** @EFFECT_STABBING decreases chance of hitting intervening target on reach attack */
        if( inter != nullptr &&
            !x_in_y( ( target_size * target_size + 1 ) * skill,
                     ( inter_block_size * inter_block_size + 1 ) * 10 ) ) {
            // Even if we miss here, low roll means weapon is pushed away or something like that
            critter = inter;
            break;
        } else if( here.obstructed_by_vehicle_rotation( last_point, path_point ) ) {
            tripoint rand = path_point;
            if( one_in( 2 ) ) {
                rand.x = last_point.x;
            } else {
                rand.y = last_point.y;
            }

            here.bash( rand, str_cur + primary_weapon().damage_melee( DT_BASH ) );
            handle_melee_wear( primary_weapon() );
            mod_moves( -move_cost );
            return;
            /** @EFFECT_STABBING increases ability to reach attack through fences */
        } else if( here.impassable( path_point ) &&
                   // Fences etc. Spears can stab through those
                   !( primary_weapon().has_flag( flag_SPEAR ) &&
                      g->m.has_flag( "THIN_OBSTACLE", path_point ) &&
                      x_in_y( skill, 10 ) ) ) {
            /** @EFFECT_STR increases bash effects when reach attacking past something */
            here.bash( path_point, str_cur + primary_weapon().damage_melee( DT_BASH ) );
            handle_melee_wear( primary_weapon() );
            mod_moves( -move_cost );
            return;
        }
        last_point = path_point;
    }

    if( here.obstructed_by_vehicle_rotation( last_point, p ) ) {
        tripoint rand = p;
        if( one_in( 2 ) ) {
            rand.x = last_point.x;
        } else {
            rand.y = last_point.y;
        }

        here.bash( rand, str_cur + primary_weapon().damage_melee( DT_BASH ) );
        handle_melee_wear( primary_weapon() );
        mod_moves( -move_cost );
        return;
    }

    if( critter == nullptr ) {
        add_msg_if_player( _( "You swing at the air." ) );
        if( martial_arts_data->has_miss_recovery_tec( primary_weapon() ) ) {
            move_cost /= 3; // "Probing" is faster than a regular miss
        }

        mod_moves( -move_cost );
        return;
    }

    reach_attacking = true;
    melee_attack( *critter, false, &force_technique, false );
    reach_attacking = false;
}

int stumble( Character &u, const item &weap )
{
    if( u.has_trait( trait_DEFT ) ) {
        return 0;
    }

    // Examples:
    // 10 str with a hatchet: 4 + 8 = 12
    // 5 str with a battle axe: 26 + 49 = 75
    // Fist: 0

    /** @EFFECT_STR reduces chance of stumbling with heavier weapons */
    return ( weap.volume() / 125_ml ) +
           ( weap.weight() / ( u.str_cur * 10_gram + 13.0_gram ) );
}

bool Character::scored_crit( float target_dodge, const item &weap,
                             const attack_statblock &attack ) const
{
    return rng_float( 0, 1.0 ) < crit_chance( hit_roll( weap, attack ), target_dodge, weap, attack );
}

/**
 * Limits a probability to be between 0.0 and 1.0
 */
inline double limit_probability( double unbounded_probability )
{
    return std::max( std::min( unbounded_probability, 1.0 ), 0.0 );
}

double Character::crit_chance( float roll_hit, float target_dodge, const item &weap,
                               const attack_statblock &attack ) const
{
    // Weapon to-hit roll
    double weapon_crit_chance = 0.5;
    if( weap.is_unarmed_weapon() ) {
        // Unarmed attack: 1/2 of unarmed skill is to-hit
        /** @EFFECT_UNARMED increases critical chance with UNARMED_WEAPON */
        weapon_crit_chance = 0.5 + 0.05 * get_skill_level( skill_unarmed );
    }

    int attack_to_hit = attack.to_hit + weap.get_melee_hit_bonus();
    if( attack_to_hit > 0 ) {
        weapon_crit_chance = std::max( weapon_crit_chance, 0.5 + 0.1 * attack_to_hit );
    } else if( attack_to_hit < 0 ) {
        weapon_crit_chance += 0.1 * attack_to_hit;
    }
    weapon_crit_chance = limit_probability( weapon_crit_chance );

    // Dexterity and perception
    /** @EFFECT_DEX increases chance for critical hits */

    /** @EFFECT_PER increases chance for critical hits */
    const double stat_crit_chance = limit_probability( 0.25 + 0.01 * dex_cur + ( 0.02 * per_cur ) );

    /** @EFFECT_BASHING increases critical chance with bashing weapons */
    /** @EFFECT_CUTTING increases critical chance with cutting weapons */
    /** @EFFECT_STABBING increases critical chance with piercing weapons */
    /** @EFFECT_UNARMED increases critical chance with unarmed weapons */
    int sk = get_skill_level( weap.melee_skill() );
    if( has_active_bionic( bio_cqb ) ) {
        sk = std::max( sk, BIO_CQB_LEVEL );
    }

    /** @EFFECT_MELEE slightly increases critical chance with any item */
    sk += get_skill_level( skill_melee ) / 2.5;

    const double skill_crit_chance = limit_probability( 0.25 + sk * 0.025 );

    // Examples (survivor stats/chances of each critical):
    // Fresh (skill-less) 8/8/8/8, unarmed:
    //  50%, 49%, 25%; ~1/16 guaranteed critical + ~1/8 if roll>dodge*1.5
    // Expert (skills 10) 10/10/10/10, unarmed:
    //  100%, 55%, 60%; ~1/3 guaranteed critical + ~4/10 if roll>dodge*1.5
    // Godlike with combat CBM 20/20/20/20, pipe (+1 accuracy):
    //  60%, 100%, 42%; ~1/4 guaranteed critical + ~3/8 if roll>dodge*1.5

    // Note: the formulas below are only valid if none of the 3 critical chance values go above 1.0
    // It is therefore important to limit them to between 0.0 and 1.0

    // Chance to get all 3 criticals (a guaranteed critical regardless of hit/dodge)
    const double chance_triple = weapon_crit_chance * stat_crit_chance * skill_crit_chance;
    // Only check double critical (one that requires hit/dodge comparison) if we have good
    // hit vs dodge
    if( roll_hit > target_dodge * 3 / 2 ) {
        const double chance_double = 0.5 * (
                                         weapon_crit_chance * stat_crit_chance +
                                         stat_crit_chance * skill_crit_chance +
                                         weapon_crit_chance * skill_crit_chance -
                                         ( 3 * chance_triple ) );
        // Because chance_double already removed the triples with -( 3 * chance_triple ),
        // chance_triple and chance_double are mutually exclusive probabilities and can just
        // be added together.
        melee::melee_stats.double_crit_count += 1;
        melee::melee_stats.double_crit_chance += chance_double + chance_triple;
        return chance_triple + chance_double;
    }
    melee::melee_stats.crit_count += 1;
    melee::melee_stats.crit_chance += chance_triple;
    return chance_triple;
}

float Character::get_dodge() const
{
    //If we're asleep, busy, or out of stamina we can't dodge
    if( in_sleep_state() || has_effect( effect_narcosis ) || get_stamina() == 0 ) {
        return 0.0f;
    }

    float ret = Creature::get_dodge();
    // If stamina is below 50%, suffer progressively worse dodge until unable to dodge at zero
    if( get_stamina() < get_stamina_max() / 2 ) {
        ret *= static_cast<float>( get_stamina() ) / static_cast<float>( get_stamina_max() ) * 2.0f;
    }

    // Chop in half if we are unable to move
    if( has_effect( effect_beartrap ) || has_effect( effect_lightsnare ) ||
        has_effect( effect_heavysnare ) ) {
        ret /= 2;
    }

    if( has_effect( effect_grabbed ) ) {
        int zed_number = 0;
        for( auto &dest : g->m.points_in_radius( pos(), 1, 0 ) ) {
            const monster *const mon = g->critter_at<monster>( dest );
            if( mon && mon->has_effect( effect_grabbing ) ) {
                zed_number++;
            }
        }
        ret *= 1.0f - ( 0.25f * zed_number );
    }

    if( worn_with_flag( flag_ROLLER_INLINE ) ||
        worn_with_flag( flag_ROLLER_QUAD ) ||
        worn_with_flag( flag_ROLLER_ONE ) ) {
        ret /= has_trait( trait_PROF_SKATER ) ? 2 : 5;
    }

    if( has_effect( effect_bouldering ) ) {
        ret /= 4;
    }

    // Each dodge after the first subtracts equivalent of 2 points of dodge skill
    if( dodges_left <= 0 ) {
        ret += dodges_left * 2 - 2;
    }

    return std::max( 0.0f, ret );
}

float Character::dodge_roll()
{
    return get_dodge() * 5;
}

float Character::get_melee() const
{
    return get_skill_level( skill_id( "melee" ) );
}

float Character::bonus_damage( bool random ) const
{
    /** @EFFECT_STR increases bashing damage */
    if( random ) {
        return rng_float( get_str() / 2.0f, get_str() );
    }

    return get_str() * 0.75f;
}

void melee::roll_bash_damage( const Character &c, bool crit, damage_instance &di, bool average,
                              const item &weap, const attack_statblock &attack )
{
    float bash_dam = 0.0f;

    const bool unarmed = weap.is_unarmed_weapon();
    int skill = c.get_skill_level( unarmed ? skill_unarmed : skill_bashing );
    if( c.has_active_bionic( bio_cqb ) ) {
        skill = BIO_CQB_LEVEL;
    }

    const int stat = c.get_str();
    /** @EFFECT_STR increases bashing damage */
    float stat_bonus = c.bonus_damage( !average );
    stat_bonus += c.mabuff_damage_bonus( DT_BASH );

    // Drunken Master damage bonuses
    if( c.has_trait( trait_DRUNKEN ) && c.has_effect( effect_drunk ) ) {
        // Remember, a single drink gives 600 levels of "drunk"
        int mindrunk = 0;
        int maxdrunk = 0;
        const time_duration drunk_dur = c.get_effect_dur( effect_drunk );
        if( unarmed ) {
            mindrunk = drunk_dur / 1_hours;
            maxdrunk = drunk_dur / 25_minutes;
        } else {
            mindrunk = drunk_dur / 90_minutes;
            maxdrunk = drunk_dur / 40_minutes;
        }

        bash_dam += average ? ( mindrunk + maxdrunk ) * 0.5f : rng( mindrunk, maxdrunk );
    }

    if( unarmed ) {
        const bool left_empty = !c.natural_attack_restricted_on( bodypart_id( "hand_l" ) );
        const bool right_empty = !c.natural_attack_restricted_on( bodypart_id( "hand_r" ) ) &&
                                 weap.is_null();
        if( left_empty || right_empty ) {
            float per_hand = 0.0f;
            for( const trait_id &mut : c.get_mutations() ) {
                if( mut->flags.contains( trait_flag_NEED_ACTIVE_TO_MELEE ) &&
                    !c.has_active_mutation( mut ) ) {
                    continue;
                }
                float unarmed_bonus = 0.0f;
                const int bash_bonus = mut->bash_dmg_bonus;
                if( mut->flags.contains( trait_flag_UNARMED_BONUS ) && bash_bonus > 0 ) {
                    unarmed_bonus += std::min( c.get_skill_level( skill_unarmed ) / 2, 4 );
                }
                per_hand += bash_bonus + unarmed_bonus;
                const std::pair<int, int> rand_bash = mut->rand_bash_bonus;
                per_hand += average ? ( rand_bash.first + rand_bash.second ) / 2.0f : rng( rand_bash.first,
                            rand_bash.second );
            }
            bash_dam += per_hand; // First hand
            if( left_empty && right_empty ) {
                // Second hand
                bash_dam += per_hand;
            }
        }

    }

    /** @EFFECT_STR increases bashing damage */
    float weap_dam = weap.damage_melee( attack, DT_BASH ) + stat_bonus;
    /** @EFFECT_UNARMED caps bash damage with unarmed weapons */

    if( unarmed ) {
        /** @EFFECT_UNARMED defines weapon damage of unarmed attacks */
        weap_dam += skill;
    }

    /** @EFFECT_BASHING caps bash damage with bashing weapons */
    float bash_cap = 2 * stat + 2 * skill;
    float bash_mul = 1.0f;

    // 80%, 88%, 96%, 104%, 112%, 116%, 120%, 124%, 128%, 132%
    if( skill < 5 ) {
        bash_mul = 0.8 + 0.08 * skill;
    } else {
        bash_mul = 0.96 + 0.04 * skill;
    }

    if( bash_cap < weap_dam && !weap.is_null() ) {
        // If damage goes over cap due to low stats/skills,
        // scale the post-armor damage down halfway between damage and cap
        bash_mul *= ( 1.0f + ( bash_cap / weap_dam ) ) / 2.0f;
    }

    /** @EFFECT_STR boosts low cap on bashing damage */
    const float low_cap = std::min( 1.0f, stat / 20.0f );
    const float bash_min = low_cap * weap_dam;
    weap_dam = average ? ( bash_min + weap_dam ) * 0.5f : rng_float( bash_min, weap_dam );

    bash_dam += weap_dam;
    bash_mul *= c.mabuff_damage_mult( DT_BASH );

    float armor_mult = attack.damage.get_armor_mult( DT_BASH );
    int arpen = attack.damage.get_armor_pen( DT_BASH );
    arpen += c.mabuff_arpen_bonus( DT_BASH );
    armor_mult *= c.mabuff_tg_armor_mult( DT_BASH );

    // Finally, extra critical effects
    if( crit ) {
        bash_mul *= 1.5f;
        // 50% armor penetration
        armor_mult *= 0.5f;
    }

    di.add_damage( DT_BASH, bash_dam, arpen, armor_mult, bash_mul );
}

void melee::roll_cut_damage( const Character &c, bool crit, damage_instance &di, bool average,
                             const item &weap, const attack_statblock &attack )
{
    float cut_dam = c.mabuff_damage_bonus( DT_CUT ) + weap.damage_melee( attack, DT_CUT );
    float cut_mul = 1.0f;

    int cutting_skill = c.get_skill_level( skill_cutting );

    if( c.has_active_bionic( bio_cqb ) ) {
        cutting_skill = BIO_CQB_LEVEL;
    }

    if( weap.is_unarmed_weapon() ) {
        // TODO: 1-handed weapons that aren't unarmed attacks
        const bool left_empty = !c.natural_attack_restricted_on( bodypart_id( "hand_l" ) );
        const bool right_empty = !c.natural_attack_restricted_on( bodypart_id( "hand_r" ) ) &&
                                 weap.is_null();
        if( left_empty || right_empty ) {
            float per_hand = 0.0f;
            if( c.has_bionic( bionic_id( "bio_razors" ) ) ) {
                per_hand += 2;
            }

            for( const trait_id &mut : c.get_mutations() ) {
                if( mut->flags.contains( trait_flag_NEED_ACTIVE_TO_MELEE ) &&
                    !c.has_active_mutation( mut ) ) {
                    continue;
                }
                float unarmed_bonus = 0.0f;
                const int cut_bonus = mut->cut_dmg_bonus;
                if( mut->flags.contains( trait_flag_UNARMED_BONUS ) && cut_bonus > 0 ) {
                    unarmed_bonus += std::min( c.get_skill_level( skill_unarmed ) / 2, 4 );
                }
                per_hand += cut_bonus + unarmed_bonus;
                const std::pair<int, int> rand_cut = mut->rand_cut_bonus;
                per_hand += average ? ( rand_cut.first + rand_cut.second ) / 2.0f : rng( rand_cut.first,
                            rand_cut.second );
            }
            // TODO: add acidproof check back to slime hands (probably move it elsewhere)

            cut_dam += per_hand; // First hand
            if( left_empty && right_empty ) {
                // Second hand
                cut_dam += per_hand;
            }
        }
    }

    if( cut_dam <= 0.0f ) {
        return; // No negative damage!
    }

    int arpen = attack.damage.get_armor_pen( DT_CUT );
    float armor_mult = attack.damage.get_armor_mult( DT_CUT );

    // 80%, 88%, 96%, 104%, 112%, 116%, 120%, 124%, 128%, 132%
    /** @EFFECT_CUTTING increases cutting damage multiplier */
    if( cutting_skill < 5 ) {
        cut_mul *= 0.8 + 0.08 * cutting_skill;
    } else {
        cut_mul *= 0.96 + 0.04 * cutting_skill;
    }

    arpen += c.mabuff_arpen_bonus( DT_CUT );
    armor_mult *= c.mabuff_tg_armor_mult( DT_CUT );

    cut_mul *= c.mabuff_damage_mult( DT_CUT );
    if( crit ) {
        cut_mul *= 1.25f;
        arpen += 5;
        armor_mult = 0.75f; //25% armor penetration
    }

    di.add_damage( DT_CUT, cut_dam, arpen, armor_mult, cut_mul );
}

void melee::roll_stab_damage( const Character &c, bool crit, damage_instance &di, bool /*average*/,
                              const item &weap, const attack_statblock &attack )
{
    float stab_dam = c.mabuff_damage_bonus( DT_STAB ) + weap.damage_melee( attack, DT_STAB );

    int unarmed_skill = c.get_skill_level( skill_unarmed );
    int stabbing_skill = c.get_skill_level( skill_stabbing );

    if( c.has_active_bionic( bio_cqb ) ) {
        stabbing_skill = BIO_CQB_LEVEL;
    }

    if( weap.is_unarmed_weapon() ) {
        const bool left_empty = !c.natural_attack_restricted_on( bodypart_id( "hand_l" ) );
        const bool right_empty = !c.natural_attack_restricted_on( bodypart_id( "hand_r" ) ) &&
                                 weap.is_null();
        if( left_empty || right_empty ) {
            float per_hand = 0.0f;

            for( const trait_id &mut : c.get_mutations() ) {
                int stab_bonus = mut->pierce_dmg_bonus;
                int unarmed_bonus = 0;
                if( mut->flags.contains( trait_flag_UNARMED_BONUS ) && stab_bonus > 0 ) {
                    unarmed_bonus = std::min( unarmed_skill / 2, 4 );
                }

                per_hand += stab_bonus + unarmed_bonus;
            }

            if( c.has_bionic( bionic_id( "bio_razors" ) ) ) {
                per_hand += 2;
            }

            stab_dam += per_hand; // First hand
            if( left_empty && right_empty ) {
                // Second hand
                stab_dam += per_hand;
            }
        }
    }

    if( stab_dam <= 0 ) {
        return; // No negative stabbing!
    }

    float stab_mul = 1.0f;
    // 66%, 76%, 86%, 96%, 106%, 116%, 122%, 128%, 134%, 140%
    /** @EFFECT_STABBING increases stabbing damage multiplier */
    if( stabbing_skill <= 5 ) {
        stab_mul = 0.66 + 0.1 * stabbing_skill;
    } else {
        stab_mul = 0.86 + 0.06 * stabbing_skill;
    }
    stab_mul *= c.mabuff_damage_mult( DT_STAB );

    float armor_mult = attack.damage.get_armor_mult( DT_STAB );
    int arpen = attack.damage.get_armor_pen( DT_STAB );
    arpen += c.mabuff_arpen_bonus( DT_STAB );
    armor_mult *= c.mabuff_tg_armor_mult( DT_STAB );

    if( crit ) {
        // Critical damage bonus for stabbing scales with skill
        stab_mul *= 1.0 + ( stabbing_skill / 10.0 );
        // Stab criticals have extra %arpen
        armor_mult *= 0.66f;
    }

    di.add_damage( DT_STAB, stab_dam, arpen, armor_mult, stab_mul );
}

void melee::roll_non_physical_damage( const Character &c, bool crit, damage_instance &di,
                                      bool /*average*/,
                                      const item &weap, const attack_statblock &attack, damage_type dt )
{
    if( dt == DT_BASH || dt == DT_CUT || dt == DT_STAB ) {
        return; // This function is not for physical DTs.
    }
    float type_dam = c.mabuff_damage_bonus( dt ) + weap.damage_melee( attack, dt );

    if( type_dam <= 0 ) {
        return; // No negative damage!
    }

    float type_mul = 1.0f;
    type_mul *= c.mabuff_damage_mult( dt );

    float armor_mult = attack.damage.get_armor_mult( dt );
    int arpen = attack.damage.get_armor_pen( dt );
    arpen += c.mabuff_arpen_bonus( dt );
    armor_mult *= c.mabuff_tg_armor_mult( dt );

    if( crit ) {
        // Critical damage bonus for stabbing scales with skill
        type_mul *= 1.5;
    }

    di.add_damage( dt, type_dam, arpen, armor_mult, type_mul );
}

matec_id Character::pick_technique( Creature &t, const item &weap,
                                    bool crit, bool dodge_counter, bool block_counter )
{

    const std::vector<matec_id> all = martial_arts_data->get_all_techniques( weap );

    std::vector<matec_id> possible;

    bool downed = t.has_effect( effect_downed );
    bool stunned = t.has_effect( effect_stunned );
    bool wall_adjacent = g->m.is_wall_adjacent( pos() );

    // first add non-aoe tecs
    for( const matec_id &tec_id : all ) {
        const ma_technique &tec = tec_id.obj();

        // ignore "dummy" techniques like WBLOCK_1
        if( tec.dummy ) {
            continue;
        }

        // skip defensive techniques
        if( tec.defensive ) {
            continue;
        }

        // skip wall adjacent techniques if not next to a wall
        if( tec.wall_adjacent && !wall_adjacent ) {
            continue;
        }

        // skip dodge counter techniques
        if( dodge_counter != tec.dodge_counter ) {
            continue;
        }

        // skip block counter techniques
        if( block_counter != tec.block_counter ) {
            continue;
        }

        // if critical then select only from critical tecs
        // but allow the technique if its crit ok
        if( !tec.crit_ok && ( crit != tec.crit_tec ) ) {
            continue;
        }

        // don't apply downing techniques to someone who's already downed
        if( downed && tec.down_dur > 0 ) {
            continue;
        }

        // don't apply "downed only" techniques to someone who's not downed
        if( !downed && tec.downed_target ) {
            continue;
        }

        // don't apply "stunned only" techniques to someone who's not stunned
        if( !stunned && tec.stunned_target ) {
            continue;
        }

        // don't apply disarming techniques to someone without a weapon
        // TODO: these are the stat requirements for tec_disarm
        // dice(   dex_cur +    get_skill_level("unarmed"),  8) >
        // dice(p->dex_cur + p->get_skill_level("melee"),   10))
        if( tec.disarms && !t.has_weapon() ) {
            continue;
        }

        if( ( tec.take_weapon && ( has_weapon() || !t.has_weapon() ) ) ) {
            continue;
        }

        // Don't apply humanoid-only techniques to non-humanoids
        if( tec.human_target && !t.in_species( HUMAN ) ) {
            continue;
        }
        // if aoe, check if there are valid targets
        if( !tec.aoe.empty() && !valid_aoe_technique( t, tec ) ) {
            continue;
        }

        // If we have negative weighting then roll to see if it's valid this time
        if( tec.weighting < 0 && !one_in( std::abs( tec.weighting ) ) ) {
            continue;
        }

        if( tec.is_valid_character( *this ) ) {
            possible.push_back( tec.id );

            //add weighted options into the list extra times, to increase their chance of being selected
            if( tec.weighting > 1 ) {
                for( int i = 1; i < tec.weighting; i++ ) {
                    possible.push_back( tec.id );
                }
            }
        }
    }

    return random_entry( possible, tec_none );
}

bool Character::valid_aoe_technique( Creature &t, const ma_technique &technique )
{
    std::vector<Creature *> dummy_targets;
    return valid_aoe_technique( t, technique, dummy_targets );
}

bool Character::valid_aoe_technique( Creature &t, const ma_technique &technique,
                                     std::vector<Creature *> &targets )
{
    if( technique.aoe.empty() ) {
        return false;
    }

    // pre-computed matrix of adjacent squares
    std::array<int, 9> offset_a = { {0, -1, -1, 1, 0, -1, 1, 1, 0 } };
    std::array<int, 9> offset_b = { {-1, -1, 0, -1, 0, 1, 0, 1, 1 } };

    // filter the values to be between -1 and 1 to avoid indexing the array out of bounds
    int dy = std::max( -1, std::min( 1, t.posy() - posy() ) );
    int dx = std::max( -1, std::min( 1, t.posx() - posx() ) );
    int lookup = dy + 1 + 3 * ( dx + 1 );

    //wide hits all targets adjacent to the attacker and the target
    if( technique.aoe == "wide" ) {
        //check if either (or both) of the squares next to our target contain a possible victim
        //offsets are a pre-computed matrix allowing us to quickly lookup adjacent squares
        tripoint left = pos() + tripoint( offset_a[lookup], offset_b[lookup], 0 );
        tripoint right = pos() + tripoint( offset_b[lookup], -offset_a[lookup], 0 );

        monster *const mon_l = g->critter_at<monster>( left );
        if( mon_l && mon_l->friendly == 0 ) {
            targets.push_back( mon_l );
        }
        monster *const mon_r = g->critter_at<monster>( right );
        if( mon_r && mon_r->friendly == 0 ) {
            targets.push_back( mon_r );
        }

        npc *const npc_l = g->critter_at<npc>( left );
        npc *const npc_r = g->critter_at<npc>( right );
        if( npc_l && npc_l->is_enemy() ) {
            targets.push_back( npc_l );
        }
        if( npc_r && npc_r->is_enemy() ) {
            targets.push_back( npc_r );
        }
        if( !targets.empty() ) {
            return true;
        }
    }

    if( technique.aoe == "impale" ) {
        // Impale hits the target and a single target behind them
        // Check if the square cardinally behind our target, or to the left / right,
        // contains a possible target.
        tripoint left = t.pos() + tripoint( offset_a[lookup], offset_b[lookup], 0 );
        tripoint target_pos = t.pos() + ( t.pos() - pos() );
        tripoint right = t.pos() + tripoint( offset_b[lookup], -offset_b[lookup], 0 );

        monster *const mon_l = g->critter_at<monster>( left );
        monster *const mon_t = g->critter_at<monster>( target_pos );
        monster *const mon_r = g->critter_at<monster>( right );
        if( mon_l && mon_l->friendly == 0 ) {
            targets.push_back( mon_l );
        }
        if( mon_t && mon_t->friendly == 0 ) {
            targets.push_back( mon_t );
        }
        if( mon_r && mon_r->friendly == 0 ) {
            targets.push_back( mon_r );
        }

        npc *const npc_l = g->critter_at<npc>( left );
        npc *const npc_t = g->critter_at<npc>( target_pos );
        npc *const npc_r = g->critter_at<npc>( right );
        if( npc_l && npc_l->is_enemy() ) {
            targets.push_back( npc_l );
        }
        if( npc_t && npc_t->is_enemy() ) {
            targets.push_back( npc_t );
        }
        if( npc_r && npc_r->is_enemy() ) {
            targets.push_back( npc_r );
        }
        if( !targets.empty() ) {
            return true;
        }
    }

    if( targets.empty() && technique.aoe == "spin" ) {
        for( const tripoint &tmp : g->m.points_in_radius( pos(), 1 ) ) {
            if( tmp == t.pos() ) {
                continue;
            }
            monster *const mon = g->critter_at<monster>( tmp );
            if( mon && mon->friendly == 0 ) {
                targets.push_back( mon );
            }
            npc *const np = g->critter_at<npc>( tmp );
            if( np && np->is_enemy() ) {
                targets.push_back( np );
            }
        }
        //don't trigger circle for fewer than 2 targets
        if( targets.size() < 2 ) {
            targets.clear();
        } else {
            return true;
        }
    }
    return false;
}

bool character_martial_arts::has_technique( const Character &guy, const matec_id &id,
        const item &weap ) const
{
    return weap.has_technique( id ) ||
           style_selected->has_technique( guy, id );
}

static damage_unit &get_damage_unit( std::vector<damage_unit> &di, const damage_type dt )
{
    static damage_unit nullunit( DT_NULL, 0, 0, 0, 0 );
    for( auto &du : di ) {
        if( du.type == dt && du.amount > 0 ) {
            return du;
        }
    }

    return nullunit;
}

static void print_damage_info( const damage_instance &di )
{
    if( !debug_mode ) {
        return;
    }

    int total = 0;
    std::string ss;
    for( auto &du : di.damage_units ) {
        int amount = di.type_damage( du.type );
        total += amount;
        ss += name_by_dt( du.type ) + ":" + std::to_string( amount ) + ",";
    }

    add_msg( m_debug, "%stotal: %d", ss, total );
}

void Character::perform_technique( const ma_technique &technique, Creature &t, damage_instance &di,
                                   int &move_cost )
{
    add_msg( m_debug, "dmg before tec:" );
    print_damage_info( di );

    for( damage_unit &du : di.damage_units ) {
        // TODO: Allow techniques to add more damage types to attacks
        if( du.amount <= 0 ) {
            continue;
        }

        du.amount += technique.damage_bonus( *this, du.type );
        du.damage_multiplier *= technique.damage_multiplier( *this, du.type );
        du.res_pen += technique.armor_penetration( *this, du.type );
    }

    add_msg( m_debug, "dmg after tec:" );
    print_damage_info( di );

    move_cost *= technique.move_cost_multiplier( *this );
    move_cost += technique.move_cost_penalty( *this );

    if( technique.down_dur > 0 ) {
        t.add_effect( effect_downed, rng( 1_turns, time_duration::from_turns( technique.down_dur ) ) );
        auto &bash = get_damage_unit( di.damage_units, DT_BASH );
        if( bash.amount > 0 ) {
            bash.amount += 3;
        }
    }

    if( technique.side_switch ) {
        const tripoint b = t.pos();
        int newx;
        int newy;

        if( b.x > posx() ) {
            newx = posx() - 1;
        } else if( b.x < posx() ) {
            newx = posx() + 1;
        } else {
            newx = b.x;
        }

        if( b.y > posy() ) {
            newy = posy() - 1;
        } else if( b.y < posy() ) {
            newy = posy() + 1;
        } else {
            newy = b.y;
        }

        const tripoint &dest = tripoint( newx, newy, b.z );
        if( g->is_empty( dest ) ) {
            t.setpos( dest );
        }
    }

    if( technique.stun_dur > 0 && !technique.powerful_knockback ) {
        t.add_effect( effect_stunned, rng( 1_turns, time_duration::from_turns( technique.stun_dur ) ) );
    }

    if( technique.knockback_dist ) {
        const tripoint prev_pos = t.pos(); // track target startpoint for knockback_follow
        const int kb_offset_x = rng( -technique.knockback_spread, technique.knockback_spread );
        const int kb_offset_y = rng( -technique.knockback_spread, technique.knockback_spread );
        tripoint kb_point( posx() + kb_offset_x, posy() + kb_offset_y, posz() );
        for( int dist = rng( 1, technique.knockback_dist ); dist > 0; dist-- ) {
            t.knock_back_from( kb_point );
        }
        // This technique makes the player follow into the tile the target was knocked from
        if( technique.knockback_follow ) {
            const optional_vpart_position vp0 = g->m.veh_at( pos() );
            vehicle *const veh0 = veh_pointer_or_null( vp0 );
            bool to_swimmable = g->m.has_flag( "SWIMMABLE", prev_pos );
            bool to_deepwater = g->m.has_flag( TFLAG_DEEP_WATER, prev_pos );

            // Check if it's possible to move to the new tile
            bool move_issue =
                g->is_dangerous_tile( prev_pos ) || // Tile contains fire, etc
                ( to_swimmable && to_deepwater ) || // Dive into deep water
                is_mounted() ||
                ( veh0 != nullptr && std::abs( veh0->velocity ) > 100 ) || // Diving from moving vehicle
                ( veh0 != nullptr && veh0->player_in_control( g->u ) ) || // Player is driving
                has_effect( effect_amigara );

            if( !move_issue ) {
                if( t.pos() != prev_pos ) {
                    g->place_player( prev_pos );
                    g->on_move_effects();
                }
            }
        }
    }

    player *p = dynamic_cast<player *>( &t );

    if( technique.take_weapon && !has_weapon() && p != nullptr && p->is_armed() ) {
        if( p->is_player() ) {
            add_msg_if_npc( _( "<npcname> disarms you and takes your weapon!" ) );
        } else {
            add_msg_player_or_npc( _( "You disarm %s and take their weapon!" ),
                                   _( "<npcname> disarms %s and takes their weapon!" ),
                                   p->name );
        }

        wield( p->remove_primary_weapon() );
    }

    if( technique.disarms && p != nullptr && p->is_armed() ) {
        g->m.add_item_or_charges( p->pos(), p->remove_primary_weapon() );
        if( p->is_player() ) {
            add_msg_if_npc( _( "<npcname> disarms you!" ) );
        } else {
            add_msg_player_or_npc( _( "You disarm %s!" ),
                                   _( "<npcname> disarms %s!" ),
                                   p->name );
        }
    }

    //AOE attacks, feel free to skip over this lump
    if( !technique.aoe.empty() ) {
        // Remember out moves and stamina
        // We don't want to consume them for every attack!
        const int temp_moves = moves;
        const int temp_stamina = get_stamina();

        std::vector<Creature *> targets;

        valid_aoe_technique( t, technique, targets );

        //hit only one valid target (pierce through doesn't spread out)
        if( technique.aoe == "impale" ) {
            // TODO: what if targets is empty
            Creature *const v = random_entry( targets );
            targets.clear();
            targets.push_back( v );
        }

        //hit the targets in the lists (all candidates if wide or burst, or just the unlucky sod if deep)
        int count_hit = 0;
        for( Creature *const c : targets ) {
            melee_attack( *c, false );
        }

        t.add_msg_if_player( m_good, vgettext( "%d enemy hit!", "%d enemies hit!", count_hit ), count_hit );
        // Extra attacks are free of charge (otherwise AoE attacks would SUCK)
        moves = temp_moves;
        set_stamina( temp_stamina );
    }

    //player has a very small chance, based on their intelligence, to learn a style whilst using the CQB bionic
    if( has_active_bionic( bio_cqb ) && !martial_arts_data->knows_selected_style() ) {
        /** @EFFECT_INT slightly increases chance to learn techniques when using CQB bionic */
        // Enhanced Memory Banks bionic doubles chance to learn martial art
        const int bionic_boost = has_active_bionic( bionic_id( bio_memory ) ) ? 2 : 1;
        if( one_in( ( 1400 - ( get_int() * 50 ) ) / bionic_boost ) ) {
            martial_arts_data->learn_current_style_CQB( is_player() );
        }
    }

    cata::run_hooks( "on_creature_performed_technique", [ &, this]( auto & params ) {
        params["char"] = this;
        params["technique"] = &technique;
        params["target"] = &t;
        params["damage_instance"] = &di;
        params["move_cost"] = move_cost;
    } );
}

static int blocking_ability( const item &shield )
{
    int block_bonus = 0;
    if( shield.has_technique( WBLOCK_3 ) ) {
        block_bonus = 8;
    } else if( shield.has_technique( WBLOCK_2 ) ) {
        block_bonus = 6;
    } else if( shield.has_technique( WBLOCK_1 ) ) {
        block_bonus = 4;
    } else if( shield.has_flag( flag_BLOCK_WHILE_WORN ) ) {
        block_bonus = 2;
    }
    return block_bonus;
}

item &Character::best_shield()
{
    // Note: wielded weapon, not one used for attacks
    int best_value = blocking_ability( primary_weapon() );
    // "BLOCK_WHILE_WORN" without a blocking tech need to be worn for the bonus
    best_value = best_value == 2 ? 0 : best_value;
    item *best = best_value > 0 ? &primary_weapon() : &null_item_reference();
    for( item * const &shield : worn ) {
        if( shield->has_flag( flag_BLOCK_WHILE_WORN ) && blocking_ability( *shield ) >= best_value ) {
            // in case a mod adds a shield that protects only one arm, the corresponding arm needs to be working
            if( shield->covers( bodypart_str_id( "arm_l" ) ) || shield->covers( bodypart_str_id( "arm_r" ) ) ) {
                if( shield->covers( bodypart_id( "arm_l" ) ) && !is_limb_disabled( bodypart_id( "arm_l" ) ) ) {
                    best = shield;
                } else if( shield->covers( bodypart_id( "arm_r" ) ) &&
                           !is_limb_disabled( bodypart_id( "arm_r" ) ) ) {
                    best = shield;
                }
                // leg guards
            } else if( ( shield->covers( bodypart_str_id( "leg_l" ) ) ||
                         shield->covers( bodypart_str_id( "leg_r" ) ) ) &&
                       get_working_leg_count() >= 1 ) {
                best = shield;
                // in case a mod adds an unusual worn blocking item, like a magic bracelet/crown, it's handled here
            } else {
                best = shield;
            }
        }
    }

    return *best;
}

bool Character::block_hit( Creature *source, bodypart_id &bp_hit, damage_instance &dam )
{
    // Shouldn't block if player is asleep
    if( in_sleep_state() || has_effect( effect_narcosis ) ) {
        return false;
    }

    // fire martial arts on-getting-hit-triggered effects
    // these fire even if the attack is blocked (you still got hit)
    martial_arts_data->ma_ongethit_effects( *this );

    if( blocks_left < 1 ) {
        return false;
    }

    blocks_left--;

    // This bonus absorbs damage from incoming attacks before they land,
    // but it still counts as a block even if it absorbs all the damage.
    float total_phys_block = mabuff_block_bonus();

    // Extract this to make it easier to implement shields/multiwield later
    item &shield = best_shield();
    block_bonus = blocking_ability( shield );
    bool conductive_shield = shield.conductive();
    bool unarmed = primary_weapon().has_flag( flag_UNARMED_WEAPON ) || primary_weapon().is_null();
    bool force_unarmed = martial_arts_data->is_force_unarmed();

    int melee_skill = get_skill_level( skill_melee );
    int unarmed_skill = get_skill_level( skill_unarmed );

    // Check if we are going to block with an item. This could
    // be worn equipment with the BLOCK_WHILE_WORN flag.
    const bool has_shield = !shield.is_null();

    // boolean check if blocking is being done with unarmed or not
    const bool item_blocking = !force_unarmed && has_shield && !unarmed;

    int block_score = 1;

    /** @EFFECT_STR increases attack blocking effectiveness with a limb or worn/wielded item */
    /** @EFFECT_UNARMED increases attack blocking effectiveness with a limb or worn/wielded item */
    if( ( unarmed || force_unarmed ) ) {
        if( martial_arts_data->can_limb_block( *this ) ) {
            // block_bonus for limb blocks will be added when the limb is decided
            block_score = str_cur + melee_skill + unarmed_skill;
        } else if( has_shield ) {
            // We can still block with a worn item while unarmed. Use higher of melee and unarmed
            block_score = str_cur + block_bonus + std::max( melee_skill, unarmed_skill );
        }
    } else if( has_shield ) {
        block_score = str_cur + block_bonus + get_skill_level( skill_melee );
    } else {
        // Can't block with limbs or items (do not block)
        return false;
    }

    // weapon blocks are preferred to limb blocks
    std::string thing_blocked_with;
    if( !force_unarmed && has_shield ) {
        thing_blocked_with = shield.tname();
        // TODO: Change this depending on damage blocked
        float wear_modifier = 1.0f;
        if( source != nullptr && source->is_hallucination() ) {
            wear_modifier = 0.0f;
        }

        handle_melee_wear( shield, wear_modifier );
    } else {
        std::vector<bodypart_id> block_parts;
        if( martial_arts_data->can_leg_block( *this ) ) {
            block_parts.emplace_back( "leg_l" );
            block_parts.emplace_back( "leg_r" );
        }
        // If you have no martial arts you can still try to block with your arms.
        // But martial arts with leg blocks only don't magically get arm blocks.
        // Edge case: Leg block only martial arts gain arm blocks if both legs broken.
        if( martial_arts_data->can_arm_block( *this ) || block_parts.empty() ) {
            block_parts.emplace_back( "arm_l" );
            block_parts.emplace_back( "arm_r" );
        }
        block_parts.erase( std::remove_if( block_parts.begin(),
        block_parts.end(), [this]( bodypart_id & bpid ) {
            return get_part_hp_cur( bpid ) <= 0;
        } ), block_parts.end() );

        const auto part_hp_cmp = [this]( const bodypart_id & lhs, const bodypart_id & rhs ) {
            return get_part_hp_cur( lhs ) < get_part_hp_cur( rhs );
        };
        auto healthiest = std::max_element( block_parts.begin(), block_parts.end(), part_hp_cmp );
        if( healthiest == block_parts.end() ) {
            // We have no parts with HP to block with.
            blocks_left = 0;
            return false;
        }
        bp_hit = *healthiest;

        thing_blocked_with = body_part_name( bp_hit->token );
    }

    bool shield_roll = false;
    if( has_shield && bp_hit != bodypart_str_id( "foot_l" ) && bp_hit != bodypart_str_id( "foot_r" ) ) {
        // We have a chance of applying the shield's direct damage reduction to other parts.
        if( shield.has_flag( flag_BLOCK_WHILE_WORN ) ) {
            float shield_block_chance = shield.get_avg_coverage();
            bool leg_hit = bp_hit == bodypart_str_id( "leg_l" ) || bp_hit == bodypart_str_id( "leg_r" );
            // Start with the same modifiers as ranged blocking.
            if( shield.has_technique( WBLOCK_3 ) ) {
                shield_block_chance *= leg_hit ? 0.75f : 0.9f;
            } else if( shield.has_technique( WBLOCK_2 ) ) {
                shield_block_chance *= leg_hit ? 0.5f : 0.8f;
            } else if( shield.has_technique( WBLOCK_1 ) ) {
                shield_block_chance *= leg_hit ? 0.25f : 0.7f;
            }
            // Melee skill directly buffs block chance, enough to ensure 100% chance of blocking a torso/head hit with a riot shield at 10 skill.
            shield_block_chance += get_skill_level( skill_melee );
            if( rng( 1, 100 ) <= shield_block_chance ) {
                shield_roll = true;
            }
        }

    }

    // Map block_score to the logistic curve for a number between 1 and 0.
    // Basic beginner character (str 8, skill 0, basic weapon)
    // Will have a score around 10 and block about %15 of incoming damage.
    // More proficient melee character (str 10, skill 4, wbock_2 weapon)
    // will have a score of 20 and block about 45% of damage.
    // A highly expert character (str 14, skill 8 wblock_2)
    // will have a score in the high 20s and will block about 80% of damage.
    // As the block score approaches 40, damage making it through will dwindle
    // to nothing, at which point we're relying on attackers hitting enough to drain blocks.
    const float physical_block_multiplier = logarithmic_range( 0, 40, block_score );

    float total_damage = 0.0;
    float damage_blocked = 0.0;

    for( auto &elem : dam.damage_units ) {
        total_damage += elem.amount;

        // block physical damage "normally"
        if( elem.type == DT_BASH || elem.type == DT_CUT || elem.type == DT_STAB ) {
            // use up our flat block bonus first
            float block_amount = std::min( total_phys_block, elem.amount );
            total_phys_block -= block_amount;
            elem.amount -= block_amount;
            damage_blocked += block_amount;

            if( elem.amount <= std::numeric_limits<float>::epsilon() ) {
                continue;
            }

            float previous_amount = elem.amount;
            // If we have a shield and it covers the limb we're hitting, we already rolled for armor.
            // Instead, roll to apply bonus damage reduction to limbs NOT already covered by it, based on melee skill.
            if( shield_roll && ( !shield.covers( bp_hit ) || !is_wearing( shield ) ) ) {
                elem.amount -= this->get_block_amount( shield, elem );
            }
            elem.amount *= physical_block_multiplier;
            damage_blocked += previous_amount - elem.amount;
        }

        // non-electrical "elemental" damage types do their full damage if unarmed,
        // but severely mitigated damage if not
        else if( elem.type == DT_HEAT || elem.type == DT_ACID || elem.type == DT_COLD ||
                 elem.type == DT_DARK || elem.type == DT_LIGHT || elem.type == DT_PSI ) {
            // Unarmed weapons won't block those
            if( item_blocking ) {
                float previous_amount = elem.amount;
                elem.amount /= 5;
                damage_blocked += previous_amount - elem.amount;
            }
            // electrical damage deals full damage if unarmed OR wielding a
            // conductive weapon
        } else if( elem.type == DT_ELECTRIC ) {
            // Unarmed weapons and conductive weapons won't block this
            if( item_blocking && !conductive_shield ) {
                float previous_amount = elem.amount;
                elem.amount /= 5;
                damage_blocked += previous_amount - elem.amount;
            }
        }
    }

    std::string damage_blocked_description;
    // good/bad/ugly add_msg color code?
    // none, hardly any, a little, some, most, all
    float blocked_ratio = 0.0f;
    if( total_damage > std::numeric_limits<float>::epsilon() ) {
        blocked_ratio = ( total_damage - damage_blocked ) / total_damage;
    }
    if( damage_blocked >= total_damage ) {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "all of the damage" );
    } else if( blocked_ratio < 0.2 ) {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "nearly all of the damage" );
    } else if( blocked_ratio < 0.4 ) {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "most of the damage" );
    } else if( blocked_ratio < 0.6 ) {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "a lot of the damage" );
    } else if( blocked_ratio < 0.8 ) {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "some of the damage" );
    } else if( blocked_ratio > std::numeric_limits<float>::epsilon() ) {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "a little of the damage" );
    } else {
        //~ Damage amount in "You block <damage amount> with your <weapon>."
        damage_blocked_description = pgettext( "block amount", "none of the damage" );
    }
    add_msg_player_or_npc(
        //~ %1$s is damage amount string (e.g. "most of the damage"), %2$s is weapon name
        _( "You block %1$s with your %2$s!" ),
        //~ %1$s is damage amount string (e.g. "most of the damage"), %2$s is weapon name
        _( "<npcname> blocks %1$s with their %2$s!" ),
        damage_blocked_description, thing_blocked_with );

    // fire martial arts block-triggered effects
    martial_arts_data->ma_onblock_effects( *this );

    // Check if we have any block counters
    matec_id tec = pick_technique( *source, shield, false, false, true );

    if( tec != tec_none && !is_dead_state() ) {
        if( get_stamina() < get_stamina_max() / 3 ) {
            add_msg( m_bad, _( "You try to counterattack but you are too exhausted!" ) );
        } else if( primary_weapon().can_shatter() ) {
            add_msg( m_bad, _( "The item you are wielding is too fragile to counterattack with!" ) );
        } else {
            melee_attack( *source, false, &tec );
        }
    }

    cata::run_hooks( "on_creature_blocked", [ &, this]( auto & params ) {
        params["char"] = this;
        params["source"] = source;
        params["bodypart_id"] = bp_hit;
        params["damage_instance"] = dam;
        params["damage_blocked"] = damage_blocked;
    } );

    return true;
}

void Character::perform_special_attacks( Creature &t, dealt_damage_instance &dealt_dam )
{
    std::vector<special_attack> special_attacks = mutation_attacks( t );

    bool practiced = false;
    for( const auto &att : special_attacks ) {
        if( t.is_dead_state() ) {
            break;
        }

        const item &cur_weapon = used_weapon();
        const attack_statblock &attack = melee::default_attack( cur_weapon );
        // TODO: Make this hit roll use unarmed skill, not weapon skill + weapon to_hit
        int hit_spread = t.deal_melee_attack( this, hit_roll( cur_weapon, attack ) * 0.8 );
        if( hit_spread >= 0 ) {
            t.deal_melee_hit( this, hit_spread, false, att.damage, dealt_dam );
            if( !practiced ) {
                // Practice unarmed, at most once per combo
                practiced = true;
                as_player()->practice( skill_unarmed, rng( 0, 10 ) );
            }
        }
        int dam = dealt_dam.total_damage();
        if( dam > 0 ) {
            player_hit_message( this, att.text, t, dam );
        }
    }
}

std::string Character::melee_special_effects( Creature &t, damage_instance &d, item &weap )
{
    std::string dump;

    std::string target = t.disp_name();

    const bionic_id bio_shock( "bio_shock" );
    if( has_active_bionic( bio_shock ) && get_power_level() >= bio_shock->power_trigger &&
        ( !is_armed() || primary_weapon().conductive() ) ) {
        mod_power_level( -bio_shock->power_trigger );
        d.add_damage( DT_ELECTRIC, rng( 2, 10 ) );

        if( is_player() ) {
            dump += string_format( _( "You shock %s." ), target ) + "\n";
        } else {
            add_msg_if_npc( _( "<npcname> shocks %s." ), target );
        }
    }

    const bionic_id bio_heat_absorb( "bio_heat_absorb" );
    if( has_active_bionic( bio_heat_absorb ) && !is_armed() && t.is_warm() ) {
        mod_power_level( bio_heat_absorb->power_trigger );
        d.add_damage( DT_COLD, 3 );
        if( is_player() ) {
            dump += string_format( _( "You drain %s's body heat." ), target ) + "\n";
        } else {
            add_msg_if_npc( _( "<npcname> drains %s's body heat!" ), target );
        }
    }

    if( primary_weapon().has_flag( flag_FLAMING ) ) {
        d.add_damage( DT_HEAT, rng( 1, 8 ) );

        if( is_player() ) {
            dump += string_format( _( "You burn %s." ), target ) + "\n";
        } else {
            add_msg_player_or_npc( _( "<npcname> burns %s." ), target );
        }
    }

    if( primary_weapon().has_flag( flag_SHOCKING ) ) {
        d.add_damage( DT_ELECTRIC, rng( 1, 8 ) );

        if( is_player() ) {
            dump += string_format( _( "You shock %s." ), target ) + "\n";
        } else {
            add_msg_player_or_npc( _( "<npcname> shocks %s." ), target );
        }
    }

    if( primary_weapon().has_flag( flag_ACIDIC ) ) {
        d.add_damage( DT_ACID, rng( 1, 8 ) );

        if( is_player() ) {
            dump += string_format( _( "You chemically burn %s." ), target ) + "\n";
        } else {
            add_msg_player_or_npc( _( "<npcname> chemically burns %s." ), target );
        }
    }

    //Hurting the wielder from poorly-chosen weapons
    if( weap.has_flag( flag_HURT_WHEN_WIELDED ) && x_in_y( 2, 3 ) ) {
        add_msg_if_player( m_bad, _( "The %s cuts your hand!" ), weap.tname() );
        deal_damage( nullptr, bodypart_id( "hand_r" ), damage_instance::physical( 0,
                     weap.damage_melee( DT_CUT ), 0 ) );
        if( weap.is_two_handed( *this ) ) { // Hurt left hand too, if it was big
            deal_damage( nullptr, bodypart_id( "hand_l" ), damage_instance::physical( 0,
                         weap.damage_melee( DT_CUT ), 0 ) );
        }
    }

    const int vol = weap.volume() / 250_ml;
    // Glass weapons and stuff with SHATTERS flag can shatter sometimes
    if( weap.can_shatter() &&
        /** @EFFECT_STR increases chance of breaking glass weapons (NEGATIVE) */
        rng( 0, vol + 8 ) < vol + str_cur ) {
        if( is_player() ) {
            dump += string_format( _( "Your %s shatters!" ), weap.tname() ) + "\n";
        } else {
            add_msg_player_or_npc( m_bad, _( "Your %s shatters!" ),
                                   _( "<npcname>'s %s shatters!" ),
                                   weap.tname() );
        }

        sounds::sound( pos(), 16, sounds::sound_t::combat, "Crack!", true, "smash_success",
                       "smash_glass_contents" );
        // Dump its contents on the ground
        weap.contents.spill_contents( pos() );
        // Take damage
        deal_damage( nullptr, bodypart_id( "arm_r" ), damage_instance::physical( 0, rng( 0, vol * 2 ),
                     0 ) );
        if( weap.is_two_handed( *this ) ) { // Hurt left arm too, if it was big
            //redeclare shatter_dam because deal_damage mutates it
            deal_damage( nullptr, bodypart_id( "arm_l" ), damage_instance::physical( 0, rng( 0, vol * 2 ),
                         0 ) );
        }
        d.add_damage( DT_CUT, rng( 0, 5 + static_cast<int>( vol * 1.5 ) ) ); // Hurt the monster extra
        remove_primary_weapon();
    }

    if( !t.is_hallucination() ) {
        handle_melee_wear( weap );
    }

    // on-hit effects for martial arts
    martial_arts_data->ma_onhit_effects( *this );

    return dump;
}

static damage_instance hardcoded_mutation_attack( const Character &u, const trait_id &id )
{
    if( id == trait_BEAK_PECK ) {
        // method open to improvement, please feel free to suggest
        // a better way to simulate target's anti-peck efforts
        /** @EFFECT_DEX increases number of hits with BEAK_PECK */

        /** @EFFECT_UNARMED increases number of hits with BEAK_PECK */
        int num_hits = std::max( 1, std::min<int>( 6,
                                 u.get_dex() + u.get_skill_level( skill_unarmed ) - rng( 4, 10 ) ) );
        return damage_instance::physical( 0, 0, num_hits * 10 );
    }

    if( id == trait_ARM_TENTACLES || id == trait_ARM_TENTACLES_4 || id == trait_ARM_TENTACLES_8 ) {
        int num_attacks = 1;
        if( id == trait_ARM_TENTACLES_4 ) {
            num_attacks = 3;
        } else if( id == trait_ARM_TENTACLES_8 ) {
            num_attacks = 7;
        }
        // Note: we're counting arms, so we want wielded item here, not weapon used for attack
        if( u.primary_weapon().is_two_handed( u ) || !u.has_two_arms() ||
            u.worn_with_flag( flag_RESTRICT_HANDS ) ) {
            num_attacks--;
        }

        if( num_attacks <= 0 ) {
            return damage_instance();
        }

        const bool rake = u.has_trait( trait_CLAWS_TENTACLE );

        /** @EFFECT_STR increases damage with ARM_TENTACLES* */
        damage_instance ret;
        if( rake ) {
            ret.add_damage( DT_CUT, u.get_str() / 2.0f + 1.0f, 0, 1.0f, num_attacks );
        } else {
            ret.add_damage( DT_BASH, u.get_str() / 3.0f + 1.0f, 0, 1.0f, num_attacks );
        }

        return ret;
    }

    if( id == trait_VINES2 || id == trait_VINES3 ) {
        const int num_attacks = id == trait_VINES2 ? 2 : 3;
        /** @EFFECT_STR increases damage with VINES* */
        damage_instance ret;
        ret.add_damage( DT_BASH, u.get_str() / 2.0f, 0, 1.0f, num_attacks );
        return ret;
    }

    debugmsg( "Invalid hardcoded mutation id: %s", id.c_str() );
    return damage_instance();
}

std::vector<special_attack> Character::mutation_attacks( Creature &t ) const
{
    std::vector<special_attack> ret;

    std::string target = t.disp_name();

    const body_part_set usable_body_parts = exclusive_flag_coverage( flag_ALLOWS_NATURAL_ATTACKS );
    const int unarmed = get_skill_level( skill_unarmed );

    for( const trait_id &pr : get_mutations() ) {
        const mutation_branch &branch = pr.obj();
        for( const mut_attack &mut_atk : branch.attacks_granted ) {
            // Covered body part
            if( mut_atk.bp != num_bp && !usable_body_parts.test( convert_bp( mut_atk.bp ) ) ) {
                continue;
            }

            /** @EFFECT_UNARMED increases chance of attacking with mutated body parts */
            /** @EFFECT_DEX increases chance of attacking with mutated body parts */

            // Calculate actor ability value to be compared against mutation attack difficulty and add debug message
            const int proc_value = get_dex() + unarmed;
            add_msg( m_debug, "%s proc chance: %d in %d", pr.c_str(), proc_value, mut_atk.chance );
            // If the mutation attack fails to proc, bail out
            if( !x_in_y( proc_value, mut_atk.chance ) ) {
                continue;
            }

            // If player has any blocker, bail out
            if( std::any_of( mut_atk.blocker_mutations.begin(), mut_atk.blocker_mutations.end(),
            [this]( const trait_id & blocker ) {
            return has_trait( blocker );
            } ) ) {
                add_msg( m_debug, "%s not procing: blocked", pr.c_str() );
                continue;
            }

            // Player must have all needed traits
            if( !std::all_of( mut_atk.required_mutations.begin(), mut_atk.required_mutations.end(),
            [this]( const trait_id & need ) {
            return has_trait( need );
            } ) ) {
                add_msg( m_debug, "%s not procing: unmet req", pr.c_str() );
                continue;
            }

            special_attack tmp;
            // Ugly special case: player's strings have only 1 variable, NPC have 2
            // Can't use <npcname> here
            // TODO: Fix
            if( is_player() ) {
                tmp.text = string_format( _( mut_atk.attack_text_u ), target );
            } else {
                tmp.text = string_format( _( mut_atk.attack_text_npc ), name, target );
            }

            // Attack starts here
            if( mut_atk.hardcoded_effect ) {
                tmp.damage = hardcoded_mutation_attack( *this, pr );
            } else {
                damage_instance dam = mut_atk.base_damage;
                damage_instance scaled = mut_atk.strength_damage;
                scaled.mult_damage( std::min<float>( 15.0f, get_str() ), true );
                dam.add( scaled );

                tmp.damage = dam;
            }

            if( tmp.damage.total_damage() > 0.0f ) {
                ret.emplace_back( tmp );
            } else {
                add_msg( m_debug, "%s not procing: zero damage", pr.c_str() );
            }
        }
    }

    return ret;
}

std::string melee_message( const ma_technique &tec, Character &p, const dealt_damage_instance &ddi )
{
    // Those could be extracted to a json

    // Three last values are for low damage
    static const std::array<std::string, 6> player_stab = { {
            translate_marker( "You impale %s" ),
            translate_marker( "You gouge %s" ),
            translate_marker( "You run %s through" ),
            translate_marker( "You puncture %s" ),
            translate_marker( "You pierce %s" ),
            translate_marker( "You poke %s" )
        }
    };
    static const std::array<std::string, 6> npc_stab = { {
            translate_marker( "<npcname> impales %s" ),
            translate_marker( "<npcname> gouges %s" ),
            translate_marker( "<npcname> runs %s through" ),
            translate_marker( "<npcname> punctures %s" ),
            translate_marker( "<npcname> pierces %s" ),
            translate_marker( "<npcname> pokes %s" )
        }
    };
    // First 5 are for high damage, next 2 for medium, then for low and then for v. low
    static const std::array<std::string, 9> player_cut = { {
            translate_marker( "You gut %s" ),
            translate_marker( "You chop %s" ),
            translate_marker( "You slash %s" ),
            translate_marker( "You mutilate %s" ),
            translate_marker( "You maim %s" ),
            translate_marker( "You stab %s" ),
            translate_marker( "You slice %s" ),
            translate_marker( "You cut %s" ),
            translate_marker( "You nick %s" )
        }
    };
    static const std::array<std::string, 9> npc_cut = { {
            translate_marker( "<npcname> guts %s" ),
            translate_marker( "<npcname> chops %s" ),
            translate_marker( "<npcname> slashes %s" ),
            translate_marker( "<npcname> mutilates %s" ),
            translate_marker( "<npcname> maims %s" ),
            translate_marker( "<npcname> stabs %s" ),
            translate_marker( "<npcname> slices %s" ),
            translate_marker( "<npcname> cuts %s" ),
            translate_marker( "<npcname> nicks %s" )
        }
    };

    // Three last values are for low damage
    static const std::array<std::string, 6> player_bash = { {
            translate_marker( "You clobber %s" ),
            translate_marker( "You smash %s" ),
            translate_marker( "You thrash %s" ),
            translate_marker( "You batter %s" ),
            translate_marker( "You whack %s" ),
            translate_marker( "You hit %s" )
        }
    };
    static const std::array<std::string, 6> npc_bash = { {
            translate_marker( "<npcname> clobbers %s" ),
            translate_marker( "<npcname> smashes %s" ),
            translate_marker( "<npcname> thrashes %s" ),
            translate_marker( "<npcname> batters %s" ),
            translate_marker( "<npcname> whacks %s" ),
            translate_marker( "<npcname> hits %s" )
        }
    };

    const int bash_dam = ddi.type_damage( DT_BASH );
    const int cut_dam = ddi.type_damage( DT_CUT );
    const int stab_dam = ddi.type_damage( DT_STAB );

    if( tec.id != tec_none ) {
        std::string message;
        if( p.is_npc() ) {
            message = _( tec.npc_message );
        } else {
            message = _( tec.avatar_message );
        }
        if( !message.empty() ) {
            return message;
        }
    }

    damage_type dominant_type = DT_BASH;
    if( cut_dam + stab_dam > bash_dam ) {
        dominant_type = cut_dam >= stab_dam ? DT_CUT : DT_STAB;
    }

    const bool npc = p.is_npc();

    // Cutting has more messages and so needs different handling
    const bool cutting = dominant_type == DT_CUT;
    size_t index;
    const int total_dam = bash_dam + stab_dam + cut_dam;
    if( total_dam > 30 ) {
        index = cutting ? rng( 0, 4 ) : rng( 0, 2 );
    } else if( total_dam > 20 ) {
        index = cutting ? rng( 5, 6 ) : 3;
    } else if( total_dam > 10 ) {
        index = cutting ? 7 : 4;
    } else {
        index = cutting ? 8 : 5;
    }

    if( dominant_type == DT_STAB ) {
        return npc ? _( npc_stab[index] ) : _( player_stab[index] );
    } else if( dominant_type == DT_CUT ) {
        return npc ? _( npc_cut[index] ) : _( player_cut[index] );
    } else if( dominant_type == DT_BASH ) {
        return npc ? _( npc_bash[index] ) : _( player_bash[index] );
    }

    return _( "The bugs attack %s" );
}

// display the hit message for an attack
void player_hit_message( Character *attacker, const std::string &message,
                         Creature &t, int dam, bool crit )
{
    std::string msg;
    game_message_type msgtype = m_good;
    std::string sSCTmod;
    game_message_type gmtSCTcolor = m_good;

    if( dam <= 0 ) {
        if( attacker->is_npc() ) {
            //~ NPC hits something but does no damage
            msg = string_format( _( "%s but does no damage." ), message );
        } else {
            //~ someone hits something but do no damage
            msg = string_format( _( "%s but do no damage." ), message );
        }
        msgtype = m_neutral;
    } else if(
        crit ) { //Player won't see exact numbers of damage dealt by NPC unless player has DEBUG_NIGHTVISION trait
        if( attacker->is_npc() && !g->u.has_trait( trait_DEBUG_NIGHTVISION ) ) {
            //~ NPC hits something (critical)
            msg = string_format( _( "%s. Critical!" ), message );
        } else {
            //~ someone hits something for %d damage (critical)
            msg = string_format( _( "%s for %d damage.  Critical!" ), message, dam );
        }
        sSCTmod = _( "Critical!" );
        gmtSCTcolor = m_critical;
    } else {
        if( attacker->is_npc() && !g->u.has_trait( trait_DEBUG_NIGHTVISION ) ) {
            //~ NPC hits something
            msg = string_format( _( "%s." ), message );
        } else {
            //~ someone hits something for %d damage
            msg = string_format( _( "%s for %d damage." ), message, dam );
        }
    }

    if( dam > 0 && attacker->is_player() ) {
        //player hits monster melee
        SCT.add( point( t.posx(), t.posy() ),
                 direction_from( point_zero, point( t.posx() - attacker->posx(), t.posy() - attacker->posy() ) ),
                 get_hp_bar( dam, t.get_hp_max(), true ).first, m_good,
                 sSCTmod, gmtSCTcolor );

        if( t.get_hp() > 0 ) {
            SCT.add( point( t.posx(), t.posy() ),
                     direction_from( point_zero, point( t.posx() - attacker->posx(), t.posy() - attacker->posy() ) ),
                     get_hp_bar( t.get_hp(), t.get_hp_max(), true ).first, m_good,
                     //~ "hit points", used in scrolling combat text
                     _( "hp" ), m_neutral,
                     "hp" );
        } else {
            SCT.removeCreatureHP();
        }
    }

    // same message is used for player and npc,
    // just using this for the <npcname> substitution.
    attacker->add_msg_player_or_npc( msgtype, msg, msg, t.disp_name() );
}

int Character::attack_cost( const item &weap ) const
{
    const int base_move_cost = weap.attack_cost() / 2;
    const int melee_skill = has_active_bionic( bionic_id( bio_cqb ) ) ? BIO_CQB_LEVEL : get_skill_level(
                                skill_melee );
    /** @EFFECT_MELEE increases melee attack speed */
    const int skill_cost = ( base_move_cost * ( 15 - melee_skill ) / 15 );
    /** @EFFECT_DEX increases attack speed */
    const int dexbonus = dex_cur;
    const int encumbrance_penalty = encumb( body_part_torso ) +
                                    ( encumb( body_part_hand_l ) + encumb( body_part_hand_r ) ) / 2;
    const int ma_move_cost = mabuff_attack_cost_penalty();
    const float stamina_ratio = static_cast<float>( get_stamina() ) / static_cast<float>
                                ( get_stamina_max() );
    // Increase cost multiplier linearly from 1.0 to 2.0 as stamina goes from 25% to 0%.
    const float stamina_penalty = 1.0 + std::max( ( 0.25f - stamina_ratio ) * 4.0f, 0.0f );
    const float ma_mult = mabuff_attack_cost_mult();

    int move_cost = base_move_cost;
    // Stamina penalty only affects base/2 and encumbrance parts of the cost
    move_cost += encumbrance_penalty;
    move_cost *= stamina_penalty;
    move_cost += skill_cost;
    move_cost -= dexbonus;

    // If we're strong enough to use this weapon one-handed but commit to two-handing it anyway, make it easier to swing.
    // Limit to weapons over 100 base movecost so you can't do this with weapons that're too small.
    // Exponential bonus so heavier weapons benefit more than small ones.
    if( weap.attack_cost() > 100 && !weap.is_two_handed( *this ) &&
        has_two_arms() && !worn_with_flag( flag_RESTRICT_HANDS ) ) {
        move_cost = std::pow( move_cost, 0.975f );
    }

    move_cost += bonus_from_enchantments( move_cost, enchant_vals::mod::ATTACK_COST, true );

    // Martial arts last. Flat has to be after mult, because comments say so.
    move_cost *= ma_mult;
    move_cost += ma_move_cost;

    move_cost *= mutation_value( "attackcost_modifier" );

    if( move_cost < 25 ) {
        return 25;
    }

    return move_cost;
}

double npc_ai::wielded_value( const Character &who )
{
    const double cached = *who.get_npc_ai_info_cache( npc_ai_info::ideal_weapon_value );
    if( cached >= 0.0 ) {
        add_msg( m_debug, "%s ideal sum value: %.1f", who.primary_weapon().type->get_id().str(), cached );
        return cached;
    }

    item &ideal_weapon = *item::spawn_temporary( who.primary_weapon() );
    if( !ideal_weapon.ammo_default().is_null() ) {
        ideal_weapon.ammo_set( ideal_weapon.ammo_default(), -1 );
    }
    double weap_val = weapon_value( who, ideal_weapon, ideal_weapon.ammo_capacity() );
    who.set_npc_ai_info_cache( npc_ai_info::ideal_weapon_value, weap_val );
    return weap_val;
}

double npc_ai::weapon_value( const Character &who, const item &weap, int ammo )
{
    const double val_gun = gun_value( who, weap, ammo );
    const double val_melee = melee_value( who, weap );
    const double more = std::max( val_gun, val_melee );
    const double less = std::min( val_gun, val_melee );

    // A small bonus for guns you can also use to hit stuff with (bayonets etc.)
    const double my_val = ( more + ( less / 2.0 ) );
    add_msg( m_debug, "%s (%ld ammo) sum value: %.1f", weap.type->get_id().str(), ammo, my_val );
    return my_val;
}

double npc_ai::melee_value( const Character &who, const item &weap )
{
    item &weapon = *item::spawn_temporary( weap );
    if( weapon.has_flag( flag_COMBAT_NPC_USE ) && !weapon.has_flag( flag_COMBAT_NPC_ON ) ) {
        if( weapon.get_use( "transform" ) ) {
            const use_function *use = weapon.type->get_use( "transform" );
            if( use->can_call( who, weapon, false, who.pos() ).success() ) {
                // Stolen from item.cpp
                weapon.convert( dynamic_cast<const iuse_transform *>
                                ( use->get_actor_ptr() )->target );
            }
        } else if( weapon.get_use( "fireweapon_off" ) ) {
            const use_function *use = weapon.type->get_use( "fireweapon_off" );
            if( use->can_call( who, weapon, false, who.pos() ).success() ) {
                weapon.convert( dynamic_cast<const fireweapon_off_actor *>
                                ( use->get_actor_ptr() )->target_id );
            }
        }

    }
    // start with average effective dps against a range of enemies
    double my_value = weapon.average_dps( *who.as_player(), melee::default_attack( weapon ) );

    float reach = weapon.reach_range( who );
    // value reach weapons more
    if( reach > 1.0f ) {
        my_value *= 1.0f + 0.5f * ( std::sqrt( reach ) - 1.0f );
    }
    // value polearms less to account for the trickiness of keeping the right range
    if( weapon.has_flag( flag_POLEARM ) ) {
        my_value *= 0.8;
    }

    // value style weapons more
    if( !who.martial_arts_data->enumerate_known_styles( weapon.type->get_id() ).empty() ) {
        my_value *= 1.5;
    }

    add_msg( m_debug, "%s as melee: %.1f", weapon.type->get_id().str(), my_value );

    return std::max( 0.0, my_value );
}

double npc_ai::unarmed_value( const Character &who )
{
    // TODO: Martial arts
    return melee_value( who, null_item_reference() );
}

void avatar_funcs::try_disarm_npc( avatar &you, npc &target )
{
    if( !target.is_armed() ) {
        return;
    }

    if( target.is_hallucination() ) {
        target.on_attacked( you );
        return;
    }

    /** @EFFECT_STR increases chance to disarm, primary stat */
    /** @EFFECT_DEX increases chance to disarm, secondary stat */
    int my_roll = dice( 3, 2 * you.get_str() + you.get_dex() );

    /** @EFFECT_MELEE increases chance to disarm */
    my_roll += dice( 3, you.get_skill_level( skill_melee ) );

    int their_roll = dice( 3, 2 * target.get_str() + target.get_dex() );
    their_roll += dice( 3, target.get_per() );
    their_roll += dice( 3, target.get_skill_level( skill_melee ) );

    item &it = target.primary_weapon();

    // roll your melee and target's dodge skills to check if grab/smash attack succeeds
    int hitspread = target.deal_melee_attack( &you, you.hit_roll( it, melee::default_attack( it ) ) );
    if( hitspread < 0 ) {
        add_msg( _( "You lunge for the %s, but miss!" ), it.tname() );
        you.mod_moves( -100 - stumble( you,
                                       you.primary_weapon() ) - you.attack_cost( you.primary_weapon() ) );
        target.on_attacked( you );
        return;
    }

    // hitspread >= 0, which means we are going to disarm by grabbing target by their weapon
    if( !you.is_armed() ) {
        /** @EFFECT_UNARMED increases chance to disarm, bonus when nothing wielded */
        my_roll += dice( 3, you.get_skill_level( skill_unarmed ) );

        if( my_roll >= their_roll ) {
            //~ %s: weapon name
            add_msg( _( "You grab at %s and pull with all your force!" ), it.tname() );
            //~ %1$s: weapon name, %2$s: NPC name
            add_msg( _( "You forcefully take %1$s from %2$s!" ), it.tname(), target.name );
            // wield() will deduce our moves, consider to deduce more/less moves for balance
            you.wield( it.detach( ) );
        } else if( my_roll >= their_roll / 2 ) {
            add_msg( _( "You grab at %s and pull with all your force, but it drops nearby!" ),
                     it.tname() );
            const tripoint tp = target.pos() + tripoint( rng( -1, 1 ), rng( -1, 1 ), 0 );
            g->m.add_item_or_charges( tp, it.detach( ) );
            you.mod_moves( -100 );
        } else {
            add_msg( _( "You grab at %s and pull with all your force, but in vain!" ), it.tname() );
            you.mod_moves( -100 );
        }

        target.on_attacked( you );
        return;
    }

    // Make their weapon fall on floor if we've rolled enough.
    you.mod_moves( -100 - you.attack_cost( you.primary_weapon() ) );
    if( my_roll >= their_roll ) {
        add_msg( _( "You smash %s with all your might forcing their %s to drop down nearby!" ),
                 target.name, it.tname() );
        const tripoint tp = target.pos() + tripoint( rng( -1, 1 ), rng( -1, 1 ), 0 );
        g->m.add_item_or_charges( tp, it.detach( ) );
    } else {
        add_msg( _( "You smash %s with all your might but %s remains in their hands!" ),
                 target.name, it.tname() );
    }

    target.on_attacked( you );
}

void avatar_funcs::try_steal_from_npc( avatar &you, npc &target )
{
    if( target.is_enemy() ) {
        add_msg( _( "%s is hostile!" ), target.name );
        return;
    }

    item *loc = game_menus::inv::steal( you, target );
    if( !loc ) {
        return;
    }

    /** @EFFECT_DEX defines the chance to steal */
    int my_roll = dice( 3, you.get_dex() );

    /** @EFFECT_UNARMED adds bonus to stealing when wielding nothing */
    if( !you.is_armed() ) {
        my_roll += dice( 4, 3 );
    }
    if( you.has_trait( trait_DEFT ) ) {
        my_roll += dice( 2, 6 );
    }

    int their_roll = dice( 5, target.get_per() );

    item *it = loc;
    if( my_roll >= their_roll ) {
        add_msg( _( "You sneakily steal %1$s from %2$s!" ),
                 it->tname(), target.name );
        you.i_add( it->detach( ) );
    } else if( my_roll >= their_roll / 2 ) {
        add_msg( _( "You failed to steal %1$s from %2$s, but did not attract attention." ),
                 it->tname(), target.name );
    } else {
        add_msg( _( "You failed to steal %1$s from %2$s." ),
                 it->tname(), target.name );
        target.on_attacked( you );
    }

    // consider to deduce less/more moves for balance
    you.mod_moves( -200 );
}

/**
 * Once the accuracy (sum of modifiers) of an attack has been determined,
 * this is used to actually roll the "hit value" of the attack to be compared to dodge.
 */
float melee::melee_hit_range( float accuracy )
{
    return normal_roll( accuracy * accuracy_roll_stddev, accuracy_roll_stddev * accuracy_roll_stddev );
}

float melee::hit_chance( float accuracy )
{
    return 0.5 * ( 1 + erf( -( accuracy * accuracy_roll_stddev ) / ( accuracy_roll_stddev *
                            M_SQRT2 ) ) );
}

melee_statistic_data melee::get_stats()
{
    return melee_stats;
}

void melee::clear_stats()
{
    melee_stats = melee_statistic_data{};
}

namespace melee
{
double expected_damage( const Character &c, const item &weapon, const attack_statblock &attack,
                        const monster &target );
} // namespace melee

double melee::expected_damage( const Character &c, const item &weapon,
                               const attack_statblock &attack, const monster &target )
{
    float to_hit = c.get_melee_hit( weapon, attack );
    float hit_difference = to_hit - target.get_dodge();
    float chance = hit_chance( hit_difference );
    resistances resists = target.resists();

    // TODO: Proper calculation here
    // TODO: Crits
    float reduced_damage_sum = std::accumulate( attack.damage.begin(), attack.damage.end(),
    0.0, [&resists]( double acc, const damage_unit & du ) {
        return acc + std::max( 0.0f, du.amount - resists.get_effective_resist( du ) );
    } );

    float capped_near_hp = std::min( 2.0f + 1.1f * target.get_hp(), reduced_damage_sum );

    return chance * capped_near_hp;
}

const attack_statblock &melee::default_attack( const item &it )
{
    assert( !it.type->attacks.empty() );
    return it.type->attacks.begin()->second;
}

const attack_statblock &melee::pick_attack( const Character &c, const item &weapon,
        const Creature &target )
{
    if( weapon.type->attacks.size() < 2 ) {
        return melee::default_attack( weapon );
    }

    if( target.is_monster() ) {
        return melee::pick_attack( c, weapon, *target.as_monster() );
    }

    return melee::pick_attack( c, weapon, *target.as_character() );
}

const attack_statblock &melee::pick_attack( const Character &c, const item &weapon,
        const monster &target )
{
    if( weapon.type->attacks.size() < 2 ) {
        return default_attack( weapon );
    }

    double best_dmg = 0.0;
    const attack_statblock *best_attack = nullptr;
    for( const auto &pr : weapon.type->attacks ) {
        double attack_dmg = melee::expected_damage( c, weapon, pr.second, target );
        if( attack_dmg > best_dmg ) {
            best_dmg = attack_dmg;
            best_attack = &pr.second;
        }
    }

    assert( best_attack != nullptr );
    return *best_attack;
}

const attack_statblock &melee::pick_attack( const Character &c, const item &weapon,
        const Character &target )
{
    ( void )c;
    ( void )target;
    return default_attack( weapon );
}

namespace melee
{
melee_statistic_data melee_stats;
} // namespace melee
