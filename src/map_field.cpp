#include <algorithm>
#include <array>
#include <ranges>
#include <bitset>
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "coordinate_conversions.h"
#include "creature.h"
#include "damage.h"
#include "debug.h"
#include "effect.h"
#include "emit.h"
#include "enums.h"
#include "field.h"
#include "field_type.h"
#include "fire.h"
#include "fungal_effects.h"
#include "game.h"
#include "game_constants.h"
#include "int_id.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "line.h"
#include "make_static.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "material.h"
#include "messages.h"
#include "mongroup.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "mapbuffer.h"
#include "overmapbuffer.h"
#include "submap_fields.h"
#include "player.h"
#include "pldata.h"
#include "point.h"
#include "rng.h"
#include "scent_block.h"
#include "string_id.h"
#include "submap.h"
#include "teleport.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "weather.h"
#include "profile.h"

static const itype_id itype_rm13_armor_on( "rm13_armor_on" );
static const itype_id itype_rock( "rock" );

static const species_id FUNGUS( "FUNGUS" );
static const species_id INSECT( "INSECT" );
static const species_id SPIDER( "SPIDER" );

static const bionic_id bio_heatsink( "bio_heatsink" );

static const efftype_id effect_badpoison( "badpoison" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_corroding( "corroding" );
static const efftype_id effect_fungus( "fungus" );
static const efftype_id effect_onfire( "onfire" );
static const efftype_id effect_poison( "poison" );
static const efftype_id effect_stung( "stung" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_teargas( "teargas" );
static const efftype_id effect_webbed( "webbed" );

static const std::string flag_FUNGUS( "FUNGUS" );

static const trait_id trait_ACIDPROOF( "ACIDPROOF" );
static const trait_id trait_ELECTRORECEPTORS( "ELECTRORECEPTORS" );
static const trait_id trait_M_IMMUNE( "M_IMMUNE" );
static const trait_id trait_M_SKIN2( "M_SKIN2" );
static const trait_id trait_M_SKIN3( "M_SKIN3" );
static const trait_id trait_THRESH_MARLOSS( "THRESH_MARLOSS" );
static const trait_id trait_THRESH_MYCUS( "THRESH_MYCUS" );
static const trait_id trait_WEB_WALKER( "WEB_WALKER" );

void create_burnproducts( std::vector<detached_ptr<item>> &out, const item &fuel,
                          const units::mass &burned_mass )
{
    auto all_mats = fuel.made_of();
    if( all_mats.empty() ) {
        return;
    }
    const auto by_weight = burned_mass / all_mats.size();
    std::ranges::for_each( all_mats, [&]( material_id & mat ) {
        std::ranges::for_each( mat->burn_products(), [&]( const auto & bp ) {
            const auto id = bp.first;
            if( fuel.typeId() == id ) {
                return;
            }
            const auto n = static_cast<int>( std::floor( bp.second * ( by_weight / id->weight ) ) );
            if( n > 0 ) {
                out.push_back( item::spawn( id ) );
            }
        } );
    } );
}

// Use a helper for a bit less boilerplate
int map::burn_body_part( player &u, field_entry &cur, body_part bp_token, const int scale )
{
    bodypart_str_id bp = convert_bp( bp_token );
    int total_damage = 0;
    const int intensity = cur.get_field_intensity();
    const int damage = rng( 1, ( scale + intensity ) / 2 );
    // A bit ugly, but better than being annoyed by acid when in hazmat
    if( u.get_armor_type( DT_ACID, bp ) < damage ) {
        const dealt_damage_instance ddi = u.deal_damage( nullptr, bp.id(),
                                          damage_instance( DT_ACID, damage ) );
        total_damage += ddi.total_damage();
    }
    // Represents acid seeping in rather than being splashed on
    u.add_env_effect( effect_corroding, bp, 2 + intensity,
                      time_duration::from_turns( rng( 2, 1 + intensity ) ), bp, 0 );
    return total_damage;
}


bool ter_furn_has_flag( const ter_t &ter, const furn_t &furn, const ter_bitflags flag )
{
    return ter.has_flag( flag ) || furn.has_flag( flag );
}

static int ter_furn_movecost( const ter_t &ter, const furn_t &furn )
{
    if( ter.movecost == 0 ) {
        return 0;
    }

    if( furn.movecost < 0 ) {
        return 0;
    }

    return ter.movecost + furn.movecost;
}

static inline bool check_flammable( const map_data_common_t &t )
{
    return t.has_flag( TFLAG_FLAMMABLE ) || t.has_flag( TFLAG_FLAMMABLE_ASH ) ||
           t.has_flag( TFLAG_FLAMMABLE_HARD );
}

/*
Helper function that encapsulates the logic involved in creating hot air.
*/
void map::create_hot_air( const tripoint &p, int intensity )
{
    field_type_id hot_air;
    switch( intensity ) {
        case 1:
            hot_air = fd_hot_air1;
            break;
        case 2:
            hot_air = fd_hot_air2;
            break;
        case 3:
            hot_air = fd_hot_air3;
            break;
        case 4:
            hot_air = fd_hot_air4;
            break;
        default:
            debugmsg( "Tried to spread hot air with intensity %d", intensity );
            return;
    }

    for( int counter = 0; counter < 5; counter++ ) {
        tripoint dst( p + point( rng( -1, 1 ), rng( -1, 1 ) ) );
        add_field( dst, hot_air, 1 );
    }
}


// This entire function makes very little sense. Why are the rules the way they are? Why does walking into some things destroy them but not others?

/*
Function: step_in_field
Triggers any active abilities a field effect would have. Fire burns you, acid melts you, etc.
If you add a field effect that interacts with the player place a case statement in the switch here.
If you wish for a field effect to do something over time (propagate, interact with terrain, etc) place it in process_subfields
*/
void map::player_in_field( player &u )
{
    // A copy of the current field for reference. Do not add fields to it, use map::add_field
    field &curfield = get_field( u.pos() );
    // Are we inside?
    bool inside = false;
    // If we are in a vehicle figure out if we are inside (reduces effects usually)
    // and what part of the vehicle we need to deal with.
    if( u.in_vehicle ) {
        if( const optional_vpart_position vp = veh_at( u.pos() ) ) {
            inside = vp->is_inside();
        }
    }

    // Iterate through all field effects on this tile.
    // Do not remove the field with remove_field, instead set it's intensity to 0. It will be removed
    // later by the field processing, which will also adjust field_count accordingly.
    for( auto &field_list_it : curfield ) {
        field_entry &cur = field_list_it.second;
        if( !cur.is_field_alive() ) {
            continue;
        }

        // Do things based on what field effect we are currently in.
        const field_type_id ft = cur.get_field_type();
        if( ft == fd_web ) {
            // If we are in a web, can't walk in webs or are in a vehicle, get webbed maybe.
            // Moving through multiple webs stacks the effect.
            if( !u.has_trait( trait_WEB_WALKER ) && !u.in_vehicle ) {
                // Between 5 and 15 minus your current web level.
                u.add_effect( effect_webbed, 1_turns, bodypart_str_id::NULL_ID(), cur.get_field_intensity() );
                // It is spent.
                cur.set_field_intensity( 0 );
                continue;
                // If you are in a vehicle destroy the web.
                // It should of been destroyed when you ran over it anyway.
            } else if( u.in_vehicle ) {
                cur.set_field_intensity( 0 );
                continue;
            }
        }
        if( ft == fd_acid ) {
            // Assume vehicles block acid damage entirely,
            // you're certainly not standing in it.
            if( !u.in_vehicle && !u.has_trait( trait_ACIDPROOF ) ) {
                int total_damage = 0;
                total_damage += burn_body_part( u, cur, bp_foot_l, 2 );
                total_damage += burn_body_part( u, cur, bp_foot_r, 2 );
                const bool on_ground = u.is_on_ground();
                if( on_ground ) {
                    // Apply the effect to the remaining body parts
                    total_damage += burn_body_part( u, cur, bp_leg_l, 2 );
                    total_damage += burn_body_part( u, cur, bp_leg_r, 2 );
                    total_damage += burn_body_part( u, cur, bp_hand_l, 2 );
                    total_damage += burn_body_part( u, cur, bp_hand_r, 2 );
                    total_damage += burn_body_part( u, cur, bp_torso, 2 );
                    // Less arms = less ability to keep upright
                    if( ( !u.has_two_arms() && one_in( 4 ) ) || one_in( 2 ) ) {
                        total_damage += burn_body_part( u, cur, bp_arm_l, 1 );
                        total_damage += burn_body_part( u, cur, bp_arm_r, 1 );
                        total_damage += burn_body_part( u, cur, bp_head, 1 );
                    }
                }

                if( on_ground && total_damage > 0 ) {
                    u.add_msg_player_or_npc( m_bad, _( "The acid burns your body!" ),
                                             _( "The acid burns <npcname>s body!" ) );
                } else if( total_damage > 0 ) {
                    u.add_msg_player_or_npc( m_bad, _( "The acid burns your legs and feet!" ),
                                             _( "The acid burns <npcname>s legs and feet!" ) );
                } else if( on_ground ) {
                    u.add_msg_if_player( m_warning, _( "You're lying in a pool of acid" ) );
                } else {
                    u.add_msg_if_player( m_warning, _( "You're standing in a pool of acid" ) );
                }

                u.check_dead_state();
            }
        }
        if( ft == fd_sap ) {
            // Sap does nothing to cars.
            if( !u.in_vehicle ) {
                // Use up sap.
                cur.set_field_intensity( cur.get_field_intensity() - 1 );
            }
        }
        if( ft == fd_sludge ) {
            // Sludge is on the ground, but you are above the ground when boarded on a vehicle
            if( !u.in_vehicle ) {
                u.add_msg_if_player( m_bad, _( "The sludge is thick and sticky.  You struggle to pull free." ) );
                u.moves -= cur.get_field_intensity() * 300;
                cur.set_field_intensity( 0 );
            }
        }
        if( ft == fd_fire ) {
            // Heatsink or suit prevents ALL fire damage.
            if( !u.has_active_bionic( bio_heatsink ) && !u.is_wearing( itype_rm13_armor_on ) ) {

                // To modify power of a field based on... whatever is relevant for the effect.
                int adjusted_intensity = cur.get_field_intensity();
                // Burn the player. Less so if you are in a car or ON a car.
                if( u.in_vehicle ) {
                    if( inside ) {
                        adjusted_intensity -= 2;
                    } else {
                        adjusted_intensity -= 1;
                    }
                }

                if( adjusted_intensity >= 1 ) {
                    // Burn message by intensity
                    static const std::array<std::string, 4> player_burn_msg = { {
                            translate_marker( "You burn your legs and feet!" ),
                            translate_marker( "You're burning up!" ),
                            translate_marker( "You're set ablaze!" ),
                            translate_marker( "Your whole body is burning!" )
                        }
                    };
                    static const std::array<std::string, 4> npc_burn_msg = { {
                            translate_marker( "<npcname> burns their legs and feet!" ),
                            translate_marker( "<npcname> is burning up!" ),
                            translate_marker( "<npcname> is set ablaze!" ),
                            translate_marker( "<npcname>s whole body is burning!" )
                        }
                    };
                    static const std::array<std::string, 4> player_warn_msg = { {
                            translate_marker( "You're standing in a fire!" ),
                            translate_marker( "You're waist-deep in a fire!" ),
                            translate_marker( "You're surrounded by raging fire!" ),
                            translate_marker( "You're lying in fire!" )
                        }
                    };

                    const int burn_min = adjusted_intensity;
                    const int burn_max = 3 * adjusted_intensity + 3;
                    std::list<bodypart_id> parts_burned;
                    int msg_num = adjusted_intensity - 1;
                    if( !u.is_on_ground() ) {
                        switch( adjusted_intensity ) {
                            case 3:
                                parts_burned.emplace_back( "hand_l" );
                                parts_burned.emplace_back( "hand_r" );
                                parts_burned.emplace_back( "arm_l" );
                                parts_burned.emplace_back( "arm_r" );
                            /* fallthrough */
                            case 2:
                                parts_burned.emplace_back( "torso" );
                            /* fallthrough */
                            case 1:
                                parts_burned.emplace_back( "foot_l" );
                                parts_burned.emplace_back( "foot_r" );
                                parts_burned.emplace_back( "leg_l" );
                                parts_burned.emplace_back( "leg_r" );
                        }
                    } else {
                        // Lying in the fire is BAAAD news, hits every body part.
                        msg_num = 3;
                        const auto &all_bps = u.get_all_body_parts( true );
                        for( const auto &bp : all_bps ) {
                            parts_burned.emplace_back( bp );
                        }
                    }

                    int total_damage = 0;
                    for( const bodypart_id part_burned : parts_burned ) {
                        const dealt_damage_instance dealt = u.deal_damage( nullptr, part_burned,
                                                            damage_instance( DT_HEAT, rng( burn_min, burn_max ) ) );
                        total_damage += dealt.type_damage( DT_HEAT );
                    }
                    if( total_damage > 0 ) {
                        u.add_msg_player_or_npc( m_bad, _( player_burn_msg[msg_num] ), _( npc_burn_msg[msg_num] ) );
                    } else {
                        u.add_msg_if_player( m_warning, _( player_warn_msg[msg_num] ) );
                    }
                    u.check_dead_state();
                }
            }

        }
        if( ft == fd_tear_gas ) {
            // Tear gas will both give you teargas disease and/or blind you.
            if( ( cur.get_field_intensity() > 1 || !one_in( 3 ) ) && ( !inside || one_in( 3 ) ) ) {
                u.add_env_effect( effect_teargas, body_part_eyes, 5, 20_seconds );
            }
            if( cur.get_field_intensity() > 1 && ( !inside || one_in( 3 ) ) ) {
                u.add_env_effect( effect_blind, body_part_eyes, cur.get_field_intensity() * 2, 10_seconds );
            }
        }
        if( ft == fd_fungal_haze ) {
            if( !u.has_trait( trait_M_IMMUNE ) && ( !inside || one_in( 4 ) ) ) {
                u.add_env_effect( effect_fungus, body_part_mouth, 4, 10_minutes, bodypart_str_id::NULL_ID() );
                u.add_env_effect( effect_fungus, body_part_eyes, 4, 10_minutes, bodypart_str_id::NULL_ID() );
            }
        }
        if( ft == fd_dazzling ) {
            if( cur.get_field_intensity() > 1 || one_in( 5 ) ) {
                u.add_env_effect( effect_blind, body_part_eyes, 10, 10_turns );
            } else {
                u.add_env_effect( effect_blind, body_part_eyes, 2, 2_turns );
            }
        }

        if( cur.extra_radiation_min() > 0 ) {
            // Get irradiated by the nuclear fallout.
            const float rads = rng( cur.extra_radiation_min() + 1,
                                    cur.extra_radiation_max() * ( cur.extra_radiation_max() + 1 ) );
            const bool rad_proof = !u.irradiate( rads );
            // TODO: Reduce damage for rad resistant?
            if( cur.radiation_hurt_damage_min() > 0 && !rad_proof ) {
                u.add_msg_if_player( m_bad, cur.radiation_hurt_message() );
                u.hurtall( rng( cur.radiation_hurt_damage_min(), cur.radiation_hurt_damage_max() ), nullptr );
            }
        }
        if( ft == fd_flame_burst ) {
            // A burst of flame? Only hits the legs and torso.
            if( !inside ) {
                // Fireballs can't touch you inside a car.
                // Heatsink or suit stops fire.
                if( !u.has_active_bionic( bio_heatsink ) &&
                    !u.is_wearing( itype_rm13_armor_on ) ) {
                    u.add_msg_player_or_npc( m_bad, _( "You're torched by flames!" ),
                                             _( "<npcname> is torched by flames!" ) );
                    u.deal_damage( nullptr, bodypart_id( "leg_l" ), damage_instance( DT_HEAT, rng( 2, 6 ) ) );
                    u.deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( DT_HEAT, rng( 2, 6 ) ) );
                    u.deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_HEAT, rng( 4, 9 ) ) );
                    u.check_dead_state();
                } else {
                    u.add_msg_player_or_npc( _( "These flames do not burn you." ),
                                             _( "Those flames do not burn <npcname>." ) );
                }
            }
        }
        if( ft == fd_electricity ) {
            // Small universal damage based on intensity, only if not electroproofed.
            if( !u.is_elec_immune() ) {
                int total_damage = 0;

                for( const bodypart_id &bp : u.get_all_body_parts( true ) ) {
                    const int dmg = rng( 1, cur.get_field_intensity() );
                    total_damage += u.deal_damage( nullptr, bp, damage_instance( DT_ELECTRIC, dmg ) ).total_damage();
                }

                if( total_damage > 0 ) {
                    if( u.has_trait( trait_ELECTRORECEPTORS ) ) {
                        u.add_msg_player_or_npc( m_bad, _( "You're painfully electrocuted!" ),
                                                 _( "<npcname> is shocked!" ) );
                        u.mod_pain( total_damage / 2 );
                    } else {
                        u.add_msg_player_or_npc( m_bad, _( "You're shocked!" ), _( "<npcname> is shocked!" ) );
                    }
                } else {
                    u.add_msg_player_or_npc( _( "The electric cloud doesn't affect you." ),
                                             _( "The electric cloud doesn't seem to affect <npcname>." ) );
                }
            }
        }
        if( ft == fd_fatigue ) {
            // Assume the rift is on the ground for now to prevent issues with the player being unable access vehicle controls on the same tile due to teleportation.
            if( !u.in_vehicle ) {
                // Teleports you... somewhere.
                if( rng( 0, 2 ) < cur.get_field_intensity() && u.is_player() ) {
                    add_msg( m_bad, _( "You're violently teleported!" ) );
                    u.hurtall( cur.get_field_intensity(), nullptr );
                    teleport::teleport( u );
                }
            }
        }
        // Why do these get removed???
        // Stepping on a shock vent shuts it down.
        if( ft == fd_shock_vent || ft == fd_acid_vent ) {
            cur.set_field_intensity( 0 );
        }
        if( ft == fd_bees ) {
            // Player is immune to bees while underwater.
            if( !u.is_underwater() ) {
                const int intensity = cur.get_field_intensity();
                // Bees will try to sting you in random body parts, up to 8 times.
                for( int i = 0; i < rng( 1, 7 ); i++ ) {
                    bodypart_id bp = u.get_random_body_part();
                    int sum_cover = 0;
                    for( const item * const &i : u.worn ) {
                        if( i->covers( bp ) ) {
                            sum_cover += i->get_coverage( bp );
                        }
                    }
                    // Get stung if [clothing on a body part isn't thick enough (like t-shirt) OR clothing covers less than 100% of body part]
                    // AND clothing on affected body part has low environmental protection value
                    if( ( u.get_armor_cut( bp ) <= 1 || ( sum_cover < 100 && x_in_y( 100 - sum_cover, 100 ) ) ) &&
                        u.add_env_effect( effect_stung, bp.id(), intensity, 9_minutes ) ) {
                        u.add_msg_if_player( m_bad, _( "The bees sting you in %s!" ),
                                             body_part_name_accusative( bp->token ) );
                    }
                }
            }
        }
        if( ft == fd_incendiary ) {
            // Mysterious incendiary substance melts you horribly.
            if( u.has_trait( trait_M_SKIN2 ) ||
                u.has_trait( trait_M_SKIN3 ) ||
                cur.get_field_intensity() == 1 ) {
                u.add_msg_player_or_npc( m_bad, _( "The incendiary burns you!" ),
                                         _( "The incendiary burns <npcname>!" ) );
                u.hurtall( rng( 1, 3 ), nullptr );
            } else {
                u.add_msg_player_or_npc( m_bad, _( "The incendiary melts into your skin!" ),
                                         _( "The incendiary melts into <npcname>s skin!" ) );
                u.add_effect( effect_onfire, 8_turns, body_part_torso );
                u.hurtall( rng( 2, 6 ), nullptr );
            }
        }
        // Both gases are unhealthy and become deadly if you cross a related threshold.
        if( ft == fd_fungicidal_gas || ft == fd_insecticidal_gas ) {
            // The gas won't harm you inside a vehicle.
            if( !inside ) {
                // Full body suits protect you from the effects of the gas.
                if( !( u.worn_with_flag( STATIC( flag_id( "GAS_PROOF" ) ) ) &&
                       u.get_env_resist( bodypart_id( "mouth" ) ) >= 15 &&
                       u.get_env_resist( bodypart_id( "eyes" ) ) >= 15 ) ) {
                    const int intensity = cur.get_field_intensity();
                    bool inhaled = u.add_env_effect( effect_poison, body_part_mouth, 5, intensity * 10_seconds );
                    if( u.has_trait( trait_THRESH_MYCUS ) || u.has_trait( trait_THRESH_MARLOSS ) ||
                        ( ft == fd_insecticidal_gas &&
                          ( u.get_highest_category() == mutation_category_id( "INSECT" ) ||
                            u.get_highest_category() == mutation_category_id( "SPIDER" ) ) ) ) {
                        inhaled |= u.add_env_effect( effect_badpoison, body_part_mouth, 5, intensity * 10_seconds );
                        u.hurtall( rng( intensity, intensity * 2 ), nullptr );
                        u.add_msg_if_player( m_bad, _( "The %s burns your skin." ), cur.name() );
                    }

                    if( inhaled ) {
                        u.add_msg_if_player( m_bad, _( "The %s makes you feel sick." ), cur.name() );
                    }
                }
            }
        }
    }
}

void map::creature_in_field( Creature &critter )
{
    ZoneScoped;

    bool in_vehicle = false;
    bool inside_vehicle = false;
    player *u = critter.as_player();
    if( critter.is_monster() ) {
        monster_in_field( *static_cast<monster *>( &critter ) );
    } else {
        if( u ) {
            in_vehicle = u->in_vehicle;
            // If we are in a vehicle figure out if we are inside (reduces effects usually)
            // and what part of the vehicle we need to deal with.
            if( in_vehicle ) {
                if( const optional_vpart_position vp = veh_at( u->pos() ) ) {
                    if( vp->is_inside() ) {
                        inside_vehicle = true;
                    }
                    if( vp->part_with_feature( VPFLAG_NOFIELDS, true ) ) {
                        // Same as just skipping each time in the loop below
                        return;
                    }
                }
            }
            player_in_field( *u );
        }
    }

    field &curfield = get_field( critter.pos() );
    for( auto &field_entry_it : curfield ) {
        field_entry &cur_field_entry = field_entry_it.second;
        if( !cur_field_entry.is_field_alive() ) {
            continue;
        }
        const field_type_id cur_field_id = cur_field_entry.get_field_type();

        for( const auto &fe : cur_field_entry.field_effects() ) {
            if( in_vehicle && fe.immune_in_vehicle ) {
                continue;
            }
            if( inside_vehicle && fe.immune_inside_vehicle ) {
                continue;
            }
            if( !inside_vehicle && fe.immune_outside_vehicle ) {
                continue;
            }
            if( in_vehicle && !one_in( fe.chance_in_vehicle ) ) {
                continue;
            }
            if( inside_vehicle && !one_in( fe.chance_inside_vehicle ) ) {
                continue;
            }
            if( !inside_vehicle && !one_in( fe.chance_outside_vehicle ) ) {
                continue;
            }

            const effect field_fx = fe.get_effect();
            if( critter.is_immune_field( cur_field_id ) || critter.is_immune_effect( field_fx.get_id() ) ) {
                continue;
            }
            bool effect_added = false;
            if( fe.is_environmental ) {
                effect_added = critter.add_env_effect( fe.id, fe.bp, fe.intensity, fe.get_duration() );
            } else {
                effect_added = true;
                critter.add_effect( field_fx );
            }
            if( effect_added ) {
                critter.add_msg_player_or_npc( fe.env_message_type, fe.get_message(), fe.get_message_npc() );
            }
        }
    }
}

void map::monster_in_field( monster &z )
{
    if( z.digging() ) {
        // Digging monsters are immune to fields
        return;
    }
    if( veh_at( z.pos() ) ) {
        // FIXME: Immune when in a vehicle for now.
        return;
    }
    field &curfield = get_field( z.pos() );

    int dam = 0;
    // Iterate through all field effects on this tile.
    // Do not remove the field with remove_field, instead set it's intensity to 0. It will be removed
    // later by the field processing, which will also adjust field_count accordingly.
    for( auto &field_list_it : curfield ) {
        field_entry &cur = field_list_it.second;
        if( !cur.is_field_alive() ) {
            continue;
        }
        const field_type_id cur_field_type = cur.get_field_type();
        if( cur_field_type == fd_web ) {
            if( !z.has_flag( MF_WEBWALK ) ) {
                z.add_effect( effect_webbed, 1_turns, bodypart_str_id::NULL_ID(), cur.get_field_intensity() );
                cur.set_field_intensity( 0 );
            }
        }
        if( cur_field_type == fd_acid ) {
            if( !z.flies() ) {
                const int d = rng( cur.get_field_intensity(), cur.get_field_intensity() * 3 );
                z.deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_ACID, d ) );
                z.check_dead_state();
                if( d > 0 ) {
                    z.add_effect( effect_corroding, 1_turns * rng( d / 2, d * 2 ) );
                }
            }

        }
        if( cur_field_type == fd_sap ) {
            z.moves -= cur.get_field_intensity() * 5;
            cur.set_field_intensity( cur.get_field_intensity() - 1 );
        }
        if( cur_field_type == fd_sludge ) {
            if( !z.digs() && !z.flies() &&
                !z.has_flag( MF_SLUDGEPROOF ) ) {
                z.moves -= cur.get_field_intensity() * 300;
                cur.set_field_intensity( 0 );
            }
        }
        if( cur_field_type == fd_fire ) {
            // TODO: MATERIALS Use fire resistance
            if( z.has_flag( MF_FIREPROOF ) || z.has_flag( MF_FIREY ) ) {
                return;
            }
            // TODO: Replace the section below with proper json values
            if( z.made_of_any( Creature::cmat_flesh ) ) {
                dam += 3;
            }
            if( z.made_of( material_id( "veggy" ) ) ) {
                dam += 12;
            }
            if( z.made_of( LIQUID ) || z.made_of_any( Creature::cmat_flammable ) ) {
                dam += 20;
            }
            if( z.made_of_any( Creature::cmat_flameres ) ) {
                dam += -20;
            }
            if( z.flies() ) {
                dam -= 15;
            }
            dam -= z.get_armor_type( DT_HEAT, bodypart_id( "torso" ) );

            if( cur.get_field_intensity() == 1 ) {
                dam += rng( 2, 6 );
            } else if( cur.get_field_intensity() == 2 ) {
                dam += rng( 6, 12 );
                if( !z.flies() ) {
                    z.moves -= 20;
                    if( dam > 0 ) {
                        z.add_effect( effect_onfire, 1_turns * rng( dam / 2, dam * 2 ) );
                    }
                }
            } else if( cur.get_field_intensity() == 3 ) {
                dam += rng( 10, 20 );
                if( !z.flies() || one_in( 3 ) ) {
                    z.moves -= 40;
                    if( dam > 0 ) {
                        z.add_effect( effect_onfire, 1_turns * rng( dam / 2, dam * 2 ) );
                    }
                }
            }
        }
        if( cur_field_type == fd_smoke ) {
            if( !z.has_flag( MF_NO_BREATHE ) ) {
                if( cur.get_field_intensity() == 3 ) {
                    z.moves -= rng( 10, 20 );
                }
                // Plants suffer from smoke even worse
                if( z.made_of( material_id( "veggy" ) ) ) {
                    z.moves -= rng( 1, cur.get_field_intensity() * 12 );
                }
            }

        }
        if( cur_field_type == fd_tear_gas ) {
            if( z.made_of_any( Creature::cmat_fleshnveg ) && !z.has_flag( MF_NO_BREATHE ) ) {
                if( cur.get_field_intensity() == 3 ) {
                    z.add_effect( effect_stunned, rng( 1_minutes, 2_minutes ) );
                    dam += rng( 4, 10 );
                } else if( cur.get_field_intensity() == 2 ) {
                    z.add_effect( effect_stunned, rng( 5_turns, 10_turns ) );
                    dam += rng( 2, 5 );
                } else {
                    z.add_effect( effect_stunned, rng( 1_turns, 5_turns ) );
                }
                if( z.made_of( material_id( "veggy" ) ) ) {
                    z.moves -= rng( cur.get_field_intensity() * 5, cur.get_field_intensity() * 12 );
                    dam += cur.get_field_intensity() * rng( 8, 14 );
                }
                if( z.has_flag( MF_SEES ) ) {
                    z.add_effect( effect_blind, cur.get_field_intensity() * 8_turns );
                }
            }

        }
        if( cur_field_type == fd_relax_gas ) {
            if( z.made_of_any( Creature::cmat_fleshnveg ) && !z.has_flag( MF_NO_BREATHE ) ) {
                z.add_effect( effect_stunned, rng( cur.get_field_intensity() * 4_turns,
                                                   cur.get_field_intensity() * 8_turns ) );
            }
        }
        if( cur_field_type == fd_dazzling ) {
            if( z.has_flag( MF_SEES ) && !z.has_flag( MF_ELECTRONIC ) ) {
                z.add_effect( effect_blind, cur.get_field_intensity() * 12_turns );
                z.add_effect( effect_stunned, cur.get_field_intensity() * rng( 5_turns, 12_turns ) );
            }

        }
        if( cur_field_type == fd_toxic_gas ) {
            if( !z.has_flag( MF_NO_BREATHE ) ) {
                dam += cur.get_field_intensity();
                z.moves -= cur.get_field_intensity();
            }

        }
        if( cur_field_type == fd_nuke_gas ) {
            if( !z.has_flag( MF_NO_BREATHE ) ) {
                if( cur.get_field_intensity() == 3 ) {
                    z.moves -= rng( 60, 120 );
                    dam += rng( 30, 50 );
                } else if( cur.get_field_intensity() == 2 ) {
                    z.moves -= rng( 20, 50 );
                    dam += rng( 10, 25 );
                } else {
                    z.moves -= rng( 0, 15 );
                    dam += rng( 0, 12 );
                }
                if( z.made_of( material_id( "veggy" ) ) ) {
                    z.moves -= rng( cur.get_field_intensity() * 5, cur.get_field_intensity() * 12 );
                    dam *= cur.get_field_intensity();
                }
            }

        }
        if( cur_field_type == fd_flame_burst ) {
            // TODO: MATERIALS Use fire resistance
            if( z.has_flag( MF_FIREPROOF ) || z.has_flag( MF_FIREY ) ) {
                return;
            }
            if( z.made_of_any( Creature::cmat_flesh ) ) {
                dam += 3;
            }
            if( z.made_of( material_id( "veggy" ) ) ) {
                dam += 12;
            }
            if( z.made_of( LIQUID ) || z.made_of_any( Creature::cmat_flammable ) ) {
                dam += 50;
            }
            if( z.made_of_any( Creature::cmat_flameres ) ) {
                dam += -25;
            }
            dam += rng( 0, 8 );
            z.moves -= 20;
        }
        if( cur_field_type == fd_electricity ) {
            // We don't want to increase dam, but deal a separate hit so that it can apply effects
            z.deal_damage( nullptr, bodypart_id( "torso" ),
                           damage_instance( DT_ELECTRIC, rng( 1, cur.get_field_intensity() * 3 ) ) );
        }
        if( cur_field_type == fd_fatigue ) {
            if( rng( 0, 2 ) < cur.get_field_intensity() ) {
                dam += cur.get_field_intensity();
                teleport::teleport( z );
            }
        }
        if( cur_field_type == fd_incendiary ) {
            // TODO: MATERIALS Use fire resistance
            if( z.has_flag( MF_FIREPROOF ) || z.has_flag( MF_FIREY ) ) {
                return;
            }
            if( z.made_of_any( Creature::cmat_flesh ) ) {
                dam += 3;
            }
            if( z.made_of( material_id( "veggy" ) ) ) {
                dam += 12;
            }
            if( z.made_of( LIQUID ) || z.made_of_any( Creature::cmat_flammable ) ) {
                dam += 20;
            }
            if( z.made_of_any( Creature::cmat_flameres ) ) {
                dam += -5;
            }

            if( cur.get_field_intensity() == 1 ) {
                dam += rng( 2, 6 );
            } else if( cur.get_field_intensity() == 2 ) {
                dam += rng( 6, 12 );
                z.moves -= 20;
                if( !z.made_of( LIQUID ) && !z.made_of_any( Creature::cmat_flameres ) ) {
                    z.add_effect( effect_onfire, rng( 8_turns, 12_turns ) );
                }
            } else if( cur.get_field_intensity() == 3 ) {
                dam += rng( 10, 20 );
                z.moves -= 40;
                if( !z.made_of( LIQUID ) && !z.made_of_any( Creature::cmat_flameres ) ) {
                    z.add_effect( effect_onfire, rng( 12_turns, 16_turns ) );
                }
            }
        }
        if( cur_field_type == fd_fungal_haze ) {
            if( !z.type->in_species( FUNGUS ) &&
                !z.type->has_flag( MF_NO_BREATHE ) &&
                !z.make_fungus() ) {
                // Don't insta-kill jabberwocks, that's silly
                const int intensity = cur.get_field_intensity();
                z.moves -= rng( 10 * intensity, 30 * intensity );
                dam += rng( 0, 10 * intensity );
            }
        }
        if( cur_field_type == fd_fungicidal_gas ) {
            if( z.type->in_species( FUNGUS ) ) {
                const int intensity = cur.get_field_intensity();
                z.moves -= rng( 10 * intensity, 30 * intensity );
                dam += rng( 4, 7 * intensity );
            }
        }
        if( cur_field_type == fd_insecticidal_gas ) {
            if( z.type->in_species( INSECT ) || z.type->in_species( SPIDER ) ) {
                const int intensity = cur.get_field_intensity();
                z.moves -= rng( 10 * intensity, 30 * intensity );
                dam += rng( 4, 7 * intensity );
            }
        }
    }

    if( dam > 0 ) {
        z.apply_damage( nullptr, bodypart_id( "torso" ), dam, true );
        z.check_dead_state();
    }
}

std::tuple<maptile, maptile, maptile> map::get_wind_blockers( const int &winddirection,
        const tripoint &pos )
{
    static const std::array<std::pair<int, std::tuple< point, point, point >>, 9> outputs = {{
            { 330, std::make_tuple( point_east, point_north_east, point_south_east ) },
            { 301, std::make_tuple( point_south_east, point_east, point_south ) },
            { 240, std::make_tuple( point_south, point_south_west, point_south_east ) },
            { 211, std::make_tuple( point_south_west, point_west, point_south ) },
            { 150, std::make_tuple( point_west, point_north_west, point_south_west ) },
            { 121, std::make_tuple( point_north_west, point_north, point_west ) },
            { 60, std::make_tuple( point_north, point_north_west, point_north_east ) },
            { 31, std::make_tuple( point_north_east, point_east, point_north ) },
            { 0, std::make_tuple( point_east, point_north_east, point_south_east ) }
        }
    };

    tripoint removepoint;
    tripoint removepoint2;
    tripoint removepoint3;
    for( const std::pair<int, std::tuple< point, point, point >> &val : outputs ) {
        if( winddirection >= val.first ) {
            removepoint = pos + std::get<0>( val.second );
            removepoint2 = pos + std::get<1>( val.second );
            removepoint3 = pos + std::get<2>( val.second );
            break;
        }
    }

    const maptile remove_tile = maptile_at( removepoint );
    const maptile remove_tile2 = maptile_at( removepoint2 );
    const maptile remove_tile3 = maptile_at( removepoint3 );
    return std::make_tuple( remove_tile, remove_tile2, remove_tile3 );
}

void map::emit_field( const tripoint &pos, const emit_id &src, float mul )
{
    if( !src.is_valid() ) {
        return;
    }

    const float chance = src->chance() * mul;
    if( src.is_valid() &&  x_in_y( chance, 100 ) ) {
        const int qty = chance > 100.0f ? roll_remainder( src->qty() * chance / 100.0f ) : src->qty();
        propagate_field( pos, src->field(), qty, src->intensity() );
    }
}

void map::propagate_field( const tripoint &center, const field_type_id &type, int amount,
                           int max_intensity )
{
    using gas_blast = std::pair<float, tripoint>;
    std::priority_queue<gas_blast, std::vector<gas_blast>, pair_greater_cmp_first> open;
    std::set<tripoint> closed;
    open.emplace( 0.0f, center );

    const bool not_gas = type.obj().phase != GAS;

    while( amount > 0 && !open.empty() ) {
        if( closed.contains( open.top().second ) ) {
            open.pop();
            continue;
        }

        // All points with equal gas intensity should propagate at the same time
        std::list<gas_blast> gas_front;
        gas_front.push_back( open.top() );
        const int cur_intensity = get_field_intensity( open.top().second, type );
        open.pop();
        while( !open.empty() && get_field_intensity( open.top().second, type ) == cur_intensity ) {
            if( !closed.contains( open.top().second ) ) {
                gas_front.push_back( open.top() );
            }

            open.pop();
        }

        int increment = std::max<int>( 1, amount / gas_front.size() );

        while( !gas_front.empty() ) {
            gas_blast gp = random_entry_removed( gas_front );
            closed.insert( gp.second );
            const int cur_intensity = get_field_intensity( gp.second, type );
            if( cur_intensity < max_intensity ) {
                const int bonus = std::min( max_intensity - cur_intensity, increment );
                mod_field_intensity( gp.second, type, bonus );
                amount -= bonus;
            } else {
                amount--;
            }

            if( amount <= 0 ) {
                return;
            }

            static const std::array<int, 8> x_offset = {{ -1, 1,  0, 0,  1, -1, -1, 1  }};
            static const std::array<int, 8> y_offset = {{  0, 0, -1, 1, -1,  1, -1, 1  }};
            for( size_t i = 0; i < 8; i++ ) {
                tripoint pt = gp.second + point( x_offset[ i ], y_offset[ i ] );
                if( closed.contains( pt ) ) {
                    continue;
                }

                if( impassable( pt ) && ( not_gas || !has_flag( TFLAG_PERMEABLE, pt ) ) ) {
                    closed.insert( pt );
                    continue;
                }
                if( !obstructed_by_vehicle_rotation( gp.second, pt ) ) {
                    open.emplace( static_cast<float>( rl_dist( center, pt ) ), pt );
                }
            }
        }
    }
}

// ============================================================================
// Canonical field processing — works for any loaded submap.
// Called by world_tick() for ALL loaded submaps every turn.
// ============================================================================

namespace
{

// Lightweight tile handle to any loaded submap.
struct SubTile {
    submap *sm    = nullptr;
    point   local;

    [[nodiscard]] auto valid()      const -> bool                    { return sm != nullptr; }
    [[nodiscard]] auto get_field()  const -> field                 & { return sm->get_field( local ); }
    [[nodiscard]] auto get_ter_t()  const -> const ter_t           & { return sm->get_ter( local ).obj(); }
    [[nodiscard]] auto get_furn_t() const -> const furn_t          & { return sm->get_furn( local ).obj(); }
    [[nodiscard]] auto get_items()  const -> location_vector<item> & { return sm->get_items( local ); } // *NOPAD*
};

// Resolve `local + delta` crossing submap boundaries via mapbuffer.
// Returns an invalid SubTile if the neighbour is not loaded.
auto neighbor_tile( submap *base, const tripoint_abs_sm &base_pos,
                    const point &local, const point &delta,
                    mapbuffer &mb ) -> SubTile
{
    const auto nx  = local.x + delta.x;
    const auto ny  = local.y + delta.y;
    const auto dsx = nx < 0 ? -1 : ( nx >= SEEX ? 1 : 0 );
    const auto dsy = ny < 0 ? -1 : ( ny >= SEEY ? 1 : 0 );
    if( dsx == 0 && dsy == 0 ) {
        return { base, { nx, ny } };
    }
    const tripoint_abs_sm nbr_pos( base_pos.raw() + tripoint{ dsx, dsy, 0 } );
    auto *nbr = mb.lookup_submap_in_memory( nbr_pos.raw() );
    if( !nbr ) {
        return {};
    }
    return { nbr, { ( nx + SEEX ) % SEEX, ( ny + SEEY ) % SEEY } };
}

// Add a field to dst, maintaining field_count and field_cache.
auto sub_add_field( SubTile &dst, field_type_id type, int intensity,
                    time_duration age ) -> field_entry *
{
    if( !dst.valid() ) {
        return nullptr;
    }
    if( dst.get_field().add_field( type, intensity, age ) ) {
        ++dst.sm->field_count;
        dst.sm->field_cache.push_back( dst.local );
        dst.sm->is_uniform = false;
    }
    return dst.get_field().find_field( type );
}

// True if the tile allows movement (movecost > 0).
auto sub_passable( const SubTile &tile ) -> bool
{
    if( !tile.valid() ) {
        return false;
    }
    const auto &ter = tile.get_ter_t();
    const auto &frn = tile.get_furn_t();
    if( ter.movecost == 0 ) {
        return false;
    }
    if( frn.movecost < 0 ) {
        return false;
    }
    return true;
}

// Simplified gas spread check (no wind / vehicle-rotation).
auto gas_can_spread_sub( const field_entry &cur, const SubTile &dst ) -> bool
{
    if( !dst.valid() ) {
        return false;
    }
    const auto *f = dst.get_field().find_field( cur.get_field_type() );
    if( f != nullptr && f->get_field_intensity() >= cur.get_field_intensity() ) {
        return false;
    }
    const auto &ter = dst.get_ter_t();
    const auto &frn = dst.get_furn_t();
    if( ter.movecost == 0 || frn.movecost < 0 ) {
        return ter_furn_has_flag( ter, frn, TFLAG_PERMEABLE );
    }
    return true;
}

// Transfer gas from cur's tile into dst.
auto gas_spread_sub( field_entry &cur, SubTile &dst ) -> void
{
    const auto type   = cur.get_field_type();
    const auto age    = cur.get_field_age();
    const auto intens = cur.get_field_intensity();
    const auto age_frac = age / intens;
    auto *f = dst.get_field().find_field( type );
    if( f != nullptr ) {
        f->set_field_intensity( f->get_field_intensity() + 1 );
        cur.set_field_intensity( intens - 1 );
        f->set_field_age( f->get_field_age() + age_frac );
        cur.set_field_age( age - age_frac );
    } else if( dst.get_field().add_field( type, 1, 0_turns ) ) {
        ++dst.sm->field_count;
        dst.sm->field_cache.push_back( dst.local );
        dst.sm->is_uniform = false;
        f = dst.get_field().find_field( type );
        if( f ) {
            f->set_field_age( age_frac );
        }
        cur.set_field_intensity( intens - 1 );
        cur.set_field_age( age - age_frac );
    }
}

} // anonymous namespace

static const std::array<point, 8> eight_dirs_sm = {{
        { -1, -1 }, {  0, -1 }, {  1, -1 },
        { -1,  0 },             {  1,  0 },
        { -1,  1 }, {  0,  1 }, {  1,  1 }
    }
};

auto process_fields_in_submap( submap &sm,
                               const tripoint_abs_sm &pos,
                               mapbuffer &mb ) -> bool
{
    ZoneScopedN( "process_fields_in_submap" );
    if( sm.field_count == 0 ) {
        return false;
    }

    auto has_fire = false;

    // Snapshot before iterating: wandering-field spread can push_back to sm.field_cache
    // within the same submap (line ~1742), which would invalidate the range iterators.
    // Newly-added entries are newborn (age 0) and skip all effects anyway, so processing
    // them next tick is correct behaviour.
    const auto field_positions = sm.field_cache;
    std::ranges::for_each( field_positions, [&]( const point & local ) {
        auto &curfield = sm.get_field( local );

        if( !curfield.displayed_field_type() ) {
            return;
        }

        for( auto it = curfield.begin(); it != curfield.end(); ) {
            auto &cur = it->second;

            // Dead entries — clean up.
            if( !cur.is_field_alive() ) {
                --sm.field_count;
                curfield.remove_field( it++ );
                continue;
            }

            auto cur_fd_type_id = cur.get_field_type();

            // Track fire before the newborn suppression below.
            if( cur_fd_type_id.obj().has_fire ) {
                has_fire = true;
            }

            // Newborn fields skip effects this tick.
            const auto is_newborn = ( cur.get_field_age() == 0_turns );
            if( is_newborn ) {
                cur_fd_type_id = fd_null;
            }

            const auto &cur_fd_type = *cur_fd_type_id;

            // Intensity upgrade.
            if( !is_newborn &&
                cur.intensity_upgrade_chance() > 0 &&
                one_in( cur.intensity_upgrade_chance() ) &&
                cur.intensity_upgrade_duration() > 0_turns &&
                calendar::once_every( cur.intensity_upgrade_duration() ) ) {
                cur.set_field_intensity( cur.get_field_intensity() + 1 );
            }

            const auto &ter = sm.get_ter( local ).obj();
            const auto &frn = sm.get_furn( local ).obj();

            // Dissipate faster in water.
            if( ter.has_flag( TFLAG_SWIMMABLE ) ) {
                cur.mod_field_age( cur.get_underwater_age_speedup() );
            }

            // ---- fd_fire ------------------------------------------------
            if( cur_fd_type_id == fd_fire ) {
                cur.set_field_age( std::max( -24_hours, cur.get_field_age() ) );

                const auto can_spread = !ter_furn_has_flag( ter, frn, TFLAG_FIRE_CONTAINER );
                const auto no_floor   = ter.has_flag( TFLAG_NO_FLOOR );
                const auto can_burn   = !no_floor && can_spread &&
                                        ( check_flammable( ter ) || check_flammable( frn ) );
                const auto is_sealed  = ter_furn_has_flag( ter, frn, TFLAG_SEALED ) &&
                                        !ter_furn_has_flag( ter, frn, TFLAG_ALLOW_FIELD_EFFECT );

                auto time_added = 0_turns;

                // --- Item burning ---
                auto &items_here = sm.get_items( local );
                if( !is_sealed && !items_here.empty() ) {
                    std::vector<detached_ptr<item>> new_content;
                    // NOTE: item detonation skipped — requires map context for explosions.
                    auto frd          = fire_data( cur.get_field_intensity(), !can_spread );
                    const auto max_c  = cur.get_field_intensity() * 2;
                    auto consumed     = 0;
                    auto fuel_it      = items_here.begin();
                    while( fuel_it != items_here.end() && consumed < max_c ) {
                        auto *fuel            = *fuel_it;
                        const auto old_weight = fuel->weight( false );
                        const auto destroyed  = fuel->burn( frd );
                        const auto new_weight = destroyed ? 0_gram : fuel->weight( false );
                        if( old_weight != new_weight ) {
                            create_burnproducts( new_content, *fuel, old_weight - new_weight );
                        }
                        if( destroyed ) {
                            std::ranges::for_each( fuel->contents.clear_items(),
                            [&]( detached_ptr<item> &ci ) {
                                if( !ci->is_irremovable() ) {
                                    new_content.push_back( std::move( ci ) );
                                }
                            } );
                            fuel_it = items_here.erase( fuel_it );
                            ++consumed;
                        } else {
                            ++fuel_it;
                        }
                    }
                    std::ranges::for_each( new_content, [&]( detached_ptr<item> &prod ) {
                        items_here.push_back( std::move( prod ) );
                    } );
                    time_added = 1_turns * roll_remainder( frd.fuel_produced );
                }

                // --- Vehicle fire damage (TODO: requires coordinate translation) ---

                // --- Terrain fuel consumption ---
                if( can_burn ) {
                    if( ter.has_flag( TFLAG_SWIMMABLE ) ) {
                        cur.set_field_age( cur.get_field_age() + 4_minutes );
                    }
                    if( ter_furn_has_flag( ter, frn, TFLAG_FLAMMABLE ) ) {
                        time_added += 1_turns * ( 5 - cur.get_field_intensity() );
                        if( cur.get_field_intensity() > 1 &&
                            one_in( 200 - cur.get_field_intensity() * 50 ) ) {
                            sm.set_ter( local, t_dirt );
                        }
                    } else if( ter_furn_has_flag( ter, frn, TFLAG_FLAMMABLE_HARD ) && one_in( 3 ) ) {
                        time_added += 1_turns * ( 4 - cur.get_field_intensity() );
                        if( cur.get_field_intensity() > 1 &&
                            one_in( 200 - cur.get_field_intensity() * 50 ) ) {
                            sm.set_ter( local, t_dirt );
                        }
                    } else if( ter.has_flag( TFLAG_FLAMMABLE_ASH ) ) {
                        time_added += 1_turns * ( 5 - cur.get_field_intensity() );
                        if( cur.get_field_intensity() > 1 &&
                            one_in( 200 - cur.get_field_intensity() * 50 ) ) {
                            sm.set_ter( local, t_dirt );
                        }
                    } else if( frn.has_flag( TFLAG_FLAMMABLE_ASH ) ) {
                        time_added += 1_turns * ( 5 - cur.get_field_intensity() );
                        if( cur.get_field_intensity() > 1 &&
                            one_in( 200 - cur.get_field_intensity() * 50 ) ) {
                            sm.set_furn( local, f_ash );
                        }
                    }
                }

                if( time_added != 0_turns ) {
                    cur.set_field_age( cur.get_field_age() - time_added );
                } else if( can_burn ) {
                    cur.mod_field_age( 10_seconds * cur.get_field_intensity() );
                }

                // --- Z-rise: level-3 fire spreads upward ---
                if( pos.z() < OVERMAP_HEIGHT && cur.get_field_intensity() == 3 ) {
                    const tripoint_abs_sm above_pos( pos.raw() + tripoint{ 0, 0, 1 } );
                    auto *above_sm = mb.lookup_submap_in_memory( above_pos.raw() );
                    if( above_sm ) {
                        const auto &above_ter = above_sm->get_ter( local ).obj();
                        if( above_ter.has_flag( TFLAG_NO_FLOOR ) ||
                            above_ter.has_flag( TFLAG_FLAMMABLE ) ||
                            above_ter.has_flag( TFLAG_FLAMMABLE_ASH ) ||
                            above_ter.has_flag( TFLAG_FLAMMABLE_HARD ) ) {
                            auto *fire_above = above_sm->get_field( local ).find_field( fd_fire );
                            if( fire_above ) {
                                fire_above->mod_field_age( -2_turns );
                            } else if( above_sm->get_field( local ).add_field( fd_fire, 1, 0_turns ) ) {
                                ++above_sm->field_count;
                                above_sm->field_cache.push_back( local );
                                above_sm->is_uniform = false;
                            }
                        }
                    }
                }

                // --- Neighbor scan for flashpoint / intensity growth / spreading ---
                const auto get_nb = [&]( const point & d ) {
                    return neighbor_tile( &sm, pos, local, d, mb );
                };
                const auto in_pit = can_spread && ter.id.id() == t_pit;
                auto adjacent_fires = 0;

                if( can_spread && cur.get_field_intensity() > 1 && one_in( 3 ) ) {
                    // Flashpoint: fuel adjacent fires from our excess age.
                    const auto end_it = static_cast<size_t>( rng( 0, 7 ) );
                    std::ranges::for_each( std::views::iota( 0u, 8u ), [&]( size_t c ) {
                        if( cur.get_field_age() >= 0_turns ) {
                            return;
                        }
                        const auto i   = ( end_it + 1 + c ) % 8;
                        auto dst       = get_nb( eight_dirs_sm[i] );
                        if( !dst.valid() ) {
                            return;
                        }
                        auto *dstfld = dst.get_field().find_field( fd_fire );
                        if( dstfld &&
                            ( dstfld->get_field_intensity() <= cur.get_field_intensity() ||
                              dstfld->get_field_age() > cur.get_field_age() ) &&
                            ( in_pit == ( dst.get_ter_t().id.id() == t_pit ) ) ) {
                            if( dstfld->get_field_intensity() < 2 ) {
                                dstfld->set_field_intensity( dstfld->get_field_intensity() + 1 );
                            }
                            dstfld->set_field_age( dstfld->get_field_age() - 5_minutes );
                            cur.set_field_age( cur.get_field_age() + 5_minutes );
                        }
                        if( dstfld ) {
                            ++adjacent_fires;
                        }
                    } );
                } else if( cur.get_field_age() < 0_turns && cur.get_field_intensity() < 3 ) {
                    // Intensity growth from neighbours.
                    auto maximum_intensity = 1;
                    if( cur.get_field_age() < -500_minutes ) {
                        maximum_intensity = 3;
                    } else {
                        std::ranges::for_each( eight_dirs_sm, [&]( const point & d ) {
                            auto dst = get_nb( d );
                            if( dst.valid() && dst.get_field().find_field( fd_fire ) ) {
                                ++adjacent_fires;
                            }
                        } );
                        maximum_intensity = 1 + ( adjacent_fires >= 3 ) + ( adjacent_fires >= 7 );
                        if( maximum_intensity < 2 && cur.get_field_age() < -50_minutes ) {
                            maximum_intensity = 2;
                        }
                    }
                    if( cur.get_field_intensity() < maximum_intensity ) {
                        cur.set_field_intensity( cur.get_field_intensity() + 1 );
                        cur.set_field_age( cur.get_field_age() +
                                           10_minutes * cur.get_field_intensity() );
                    }
                }

                // Fire spreading to adjacent tiles.
                if( can_spread ) {
                    const auto end_i = static_cast<size_t>( rng( 0, 7 ) );
                    std::ranges::for_each( std::views::iota( 0u, 8u ), [&]( size_t c ) {
                        if( one_in( cur.get_field_intensity() * 2 ) ) {
                            return;
                        }
                        auto dst = get_nb( eight_dirs_sm[( end_i + 1 + c ) % 8] );
                        if( !dst.valid() ) {
                            return;
                        }
                        if( dst.get_field().find_field( fd_fire ) ) {
                            return;
                        }
                        const auto &dter = dst.get_ter_t();
                        const auto &dfur = dst.get_furn_t();
                        if( in_pit != ( dter.id.id() == t_pit ) ) {
                            return;
                        }
                        auto *nearwebfld     = dst.get_field().find_field( fd_web );
                        auto  spread_chance  = 25 * ( cur.get_field_intensity() - 1 );
                        if( nearwebfld ) {
                            spread_chance = 50 + spread_chance / 2;
                        }
                        const auto dst_has_flammable_items = std::ranges::any_of(
                                dst.get_items().as_vector(),
                        []( const item * i ) { return i && i->flammable(); } );
                        const auto power = cur.get_field_intensity() + ( one_in( 5 ) ? 1 : 0 );
                        const auto can_ignite =
                            rng( 1, 100 ) < spread_chance &&
                            ( check_flammable( dter ) || check_flammable( dfur ) || nearwebfld ) &&
                            ( ( power >= 3 && cur.get_field_age() < 0_turns && one_in( 20 ) ) ||
                              ( power >= 2 && ter_furn_has_flag( dter, dfur, TFLAG_FLAMMABLE ) && one_in( 2 ) ) ||
                              ( power >= 2 && ter_furn_has_flag( dter, dfur, TFLAG_FLAMMABLE_ASH ) && one_in( 2 ) ) ||
                              ( power >= 3 && ter_furn_has_flag( dter, dfur, TFLAG_FLAMMABLE_HARD ) && one_in( 5 ) ) ||
                              nearwebfld ||
                              ( dst_has_flammable_items && one_in( 5 ) ) );
                        if( can_ignite ) {
                            auto *newfire = sub_add_field( dst, fd_fire, 1, 0_turns );
                            if( newfire ) {
                                newfire->set_field_age( 2_minutes );
                                cur.set_field_age( cur.get_field_age() + 1_minutes );
                            }
                            if( nearwebfld ) {
                                nearwebfld->set_field_intensity( 0 );
                            }
                        }
                    } );
                }

                // --- Z-fall: fire on open-air tile falls to z-level below ---
                if( no_floor && pos.z() > -OVERMAP_DEPTH ) {
                    const tripoint_abs_sm below_pos( pos.raw() + tripoint{ 0, 0, -1 } );
                    auto *below_sm = mb.lookup_submap_in_memory( below_pos.raw() );
                    if( below_sm ) {
                        auto *fire_below = below_sm->get_field( local ).find_field( fd_fire );
                        if( !fire_below ) {
                            if( below_sm->get_field( local ).add_field( fd_fire, 1, 0_turns ) ) {
                                ++below_sm->field_count;
                                below_sm->field_cache.push_back( local );
                                below_sm->is_uniform = false;
                            }
                            cur.set_field_intensity( cur.get_field_intensity() - 1 );
                        } else {
                            auto new_i = std::max( cur.get_field_intensity(),
                                                   fire_below->get_field_intensity() );
                            if( new_i < 3 &&
                                cur.get_field_intensity() == fire_below->get_field_intensity() ) {
                                ++new_i;
                            }
                            if( fire_below->get_field_intensity() < 3 || one_in( 10 ) ) {
                                cur.set_field_intensity( cur.get_field_intensity() - 1 );
                            }
                            fire_below->set_field_intensity( new_i );
                        }
                    }
                }
            } // end fd_fire

            // ---- Gas spreading (simplified — no wind) --------------------
            if( !is_newborn && cur.gas_can_spread() ) {
                const auto gas_pct = cur_fd_type.percent_spread;
                if( gas_pct > 0 && cur.get_field_intensity() > 1 &&
                    rng( 1, 100 ) <= gas_pct ) {
                    // Try to fall first.
                    auto spread_done = false;
                    if( pos.z() > -OVERMAP_DEPTH ) {
                        const tripoint_abs_sm below_pos( pos.raw() + tripoint{ 0, 0, -1 } );
                        auto *below_sm = mb.lookup_submap_in_memory( below_pos.raw() );
                        if( below_sm ) {
                            auto dst = SubTile{ below_sm, local };
                            if( gas_can_spread_sub( cur, dst ) ) {
                                gas_spread_sub( cur, dst );
                                spread_done = true;
                            }
                        }
                    }
                    if( !spread_done ) {
                        const auto start = static_cast<size_t>( rng( 0, 7 ) );
                        std::ranges::for_each( std::views::iota( 0u, 8u ), [&]( size_t c ) {
                            if( spread_done ) {
                                return;
                            }
                            auto dst = neighbor_tile( &sm, pos, local,
                                                      eight_dirs_sm[( start + c ) % 8], mb );
                            if( gas_can_spread_sub( cur, dst ) ) {
                                gas_spread_sub( cur, dst );
                                spread_done = true;
                            }
                        } );
                    }
                    // Outdoor age speedup (simplified — skip wind/shelter check).
                    const auto outdoor_speedup = cur_fd_type.outdoor_age_speedup;
                    if( outdoor_speedup > 0_turns ) {
                        cur.mod_field_age( outdoor_speedup );
                    }
                }
            }

            // ---- fd_fungicidal_gas ----------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_fungicidal_gas ) {
                const auto intensity = cur.get_field_intensity();
                if( ter.has_flag( flag_FUNGUS ) && one_in( 10 / intensity ) ) {
                    sm.set_ter( local, t_dirt );
                }
                if( frn.has_flag( flag_FUNGUS ) && one_in( 10 / intensity ) ) {
                    sm.set_furn( local, f_null );
                }
            }

            // ---- fd_electricity ------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_electricity && !one_in( 5 ) ) {
                auto self = SubTile{ &sm, local };
                if( !sub_passable( self ) && cur.get_field_intensity() > 1 ) {
                    auto tries = 0;
                    while( tries < 10 &&
                           cur.get_field_age() < 5_minutes &&
                           cur.get_field_intensity() > 1 ) {
                        const auto dx = rng( -1, 1 );
                        const auto dy = rng( -1, 1 );
                        if( dx == 0 && dy == 0 ) {
                            ++tries;
                            continue;
                        }
                        auto dst = neighbor_tile( &sm, pos, local, { dx, dy }, mb );
                        if( sub_passable( dst ) ) {
                            sub_add_field( dst, fd_electricity, 1,
                                           cur.get_field_age() + 1_turns );
                            cur.set_field_intensity( cur.get_field_intensity() - 1 );
                            tries = 0;
                        } else {
                            ++tries;
                        }
                    }
                } else {
                    std::vector<point> grounded;
                    std::ranges::for_each( eight_dirs_sm, [&]( const point & d ) {
                        auto dst = neighbor_tile( &sm, pos, local, d, mb );
                        if( dst.valid() && !sub_passable( dst ) ) {
                            grounded.push_back( d );
                        }
                    } );
                    if( grounded.empty() ) {
                        const auto dx  = rng( -1, 1 );
                        const auto dy  = rng( -1, 1 );
                        auto dst       = neighbor_tile( &sm, pos, local, { dx, dy }, mb );
                        auto *elec     = dst.valid() ?
                                         dst.get_field().find_field( fd_electricity ) : nullptr;
                        if( sub_passable( dst ) && elec && elec->get_field_intensity() < 3 ) {
                            elec->set_field_intensity( elec->get_field_intensity() + 1 );
                            cur.set_field_intensity( cur.get_field_intensity() - 1 );
                        } else if( sub_passable( dst ) ) {
                            sub_add_field( dst, fd_electricity, 1,
                                           cur.get_field_age() + 1_turns );
                        }
                        cur.set_field_intensity( cur.get_field_intensity() - 1 );
                    }
                    while( !grounded.empty() && cur.get_field_intensity() > 1 ) {
                        const auto d = random_entry_removed( grounded );
                        auto dst = neighbor_tile( &sm, pos, local, d, mb );
                        sub_add_field( dst, fd_electricity, 1, cur.get_field_age() + 1_turns );
                        cur.set_field_intensity( cur.get_field_intensity() - 1 );
                    }
                }
            }

            // ---- fd_fire_vent -------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_fire_vent ) {
                if( cur.get_field_intensity() > 1 ) {
                    if( one_in( 3 ) ) {
                        cur.set_field_intensity( cur.get_field_intensity() - 1 );
                    }
                    // create_hot_air() skipped — render/audio effect only.
                } else {
                    auto self = SubTile{ &sm, local };
                    sub_add_field( self, fd_flame_burst, 1, cur.get_field_age() );
                    cur.set_field_intensity( 0 );
                }
            }

            // ---- fd_flame_burst -----------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_flame_burst ) {
                if( cur.get_field_intensity() > 1 ) {
                    cur.set_field_intensity( cur.get_field_intensity() - 1 );
                } else {
                    cur.set_field_intensity( 0 );
                }
            }

            // ---- fd_incendiary ------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_incendiary ) {
                const auto dx  = rng( -1, 1 );
                const auto dy  = rng( -1, 1 );
                auto dst       = neighbor_tile( &sm, pos, local, { dx, dy }, mb );
                if( dst.valid() ) {
                    const auto &dter = dst.get_ter_t();
                    const auto &dfrn = dst.get_furn_t();
                    if( ter_furn_has_flag( dter, dfrn, TFLAG_FLAMMABLE ) ||
                        ter_furn_has_flag( dter, dfrn, TFLAG_FLAMMABLE_ASH ) ||
                        ter_furn_has_flag( dter, dfrn, TFLAG_FLAMMABLE_HARD ) ) {
                        sub_add_field( dst, fd_fire, 1, 0_turns );
                    }
                    const auto dst_has_flammable = std::ranges::any_of(
                                                       dst.get_items().as_vector(),
                    []( const item * i ) { return i && i->flammable(); } );
                    if( dst_has_flammable ) {
                        sub_add_field( dst, fd_fire, 1, 0_turns );
                    }
                }
                // create_hot_air() skipped — render/audio effect only.
            }

            // ---- fd_shock_vent ------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_shock_vent ) {
                if( cur.get_field_intensity() > 1 ) {
                    if( one_in( 5 ) ) {
                        cur.set_field_intensity( cur.get_field_intensity() - 1 );
                    }
                } else {
                    cur.set_field_intensity( 3 );
                    const auto num_bolts = rng( 3, 6 );
                    for( auto b = 0; b < num_bolts; ++b ) {
                        auto xdir = 0;
                        auto ydir = 0;
                        while( xdir == 0 && ydir == 0 ) {
                            xdir = rng( -1, 1 );
                            ydir = rng( -1, 1 );
                        }
                        const auto dist = rng( 4, 12 );
                        auto cx         = local.x;
                        auto cy         = local.y;
                        for( auto n = 0; n < dist; ++n ) {
                            cx += xdir;
                            cy += ydir;
                            const auto delta = point{ cx - local.x, cy - local.y };
                            auto bolt_dst    = neighbor_tile( &sm, pos, local, delta, mb );
                            sub_add_field( bolt_dst, fd_electricity, rng( 2, 3 ), 0_turns );
                            if( one_in( 4 ) ) {
                                xdir = xdir == 0 ? rng( 0, 1 ) * 2 - 1 : 0;
                            }
                            if( one_in( 4 ) ) {
                                ydir = ydir == 0 ? rng( 0, 1 ) * 2 - 1 : 0;
                            }
                        }
                    }
                }
            }

            // ---- fd_acid_vent ------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_acid_vent ) {
                if( cur.get_field_intensity() > 1 ) {
                    if( cur.get_field_age() >= 1_minutes ) {
                        cur.set_field_intensity( cur.get_field_intensity() - 1 );
                        cur.set_field_age( 0_turns );
                    }
                } else {
                    cur.set_field_intensity( 3 );
                    for( auto dx = -5; dx <= 5; ++dx ) {
                        for( auto dy = -5; dy <= 5; ++dy ) {
                            if( std::abs( dx ) + std::abs( dy ) > 5 ) {
                                continue;
                            }
                            auto dst = neighbor_tile( &sm, pos, local, { dx, dy }, mb );
                            if( !dst.valid() ) {
                                continue;
                            }
                            const auto *acid = dst.get_field().find_field( fd_acid );
                            if( acid && acid->get_field_intensity() == 0 ) {
                                auto new_intensity = 3 - ( std::abs( dx ) + std::abs( dy ) ) / 2 +
                                                     ( one_in( 3 ) ? 1 : 0 );
                                new_intensity = std::clamp( new_intensity, 0, 3 );
                                if( new_intensity > 0 ) {
                                    sub_add_field( dst, fd_acid, new_intensity, 0_turns );
                                }
                            }
                        }
                    }
                }
            }

            // ---- fd_push_items ------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_push_items ) {
                auto &items = sm.get_items( local );
                auto push_it = items.begin();
                while( push_it != items.end() ) {
                    auto *it = *push_it;
                    if( it->typeId() != itype_rock || it->age() < 1_turns ) {
                        ++push_it;
                        continue;
                    }
                    it->set_age( 0_turns );
                    std::vector<point> valid_dirs;
                    std::ranges::for_each( eight_dirs_sm, [&]( const point & d ) {
                        auto dst = neighbor_tile( &sm, pos, local, d, mb );
                        if( dst.valid() && dst.get_field().find_field( fd_push_items ) ) {
                            valid_dirs.push_back( d );
                        }
                    } );
                    if( !valid_dirs.empty() ) {
                        const auto target_d = random_entry( valid_dirs );
                        auto dst            = neighbor_tile( &sm, pos, local, target_d, mb );
                        if( dst.valid() ) {
                            detached_ptr<item> detached;
                            push_it = items.erase( push_it, &detached );
                            dst.get_items().push_back( std::move( detached ) );
                            // Creature interactions skipped (no creatures outside render bubble).
                            continue;
                        }
                    }
                    ++push_it;
                }
            }

            // ---- fd_bees ------------------------------------------------
            if( !is_newborn && cur_fd_type_id == fd_bees ) {
                static const std::array<field_type_id, 18> bee_killers = {{
                        fd_web, fd_fire, fd_smoke, fd_toxic_gas, fd_tear_gas,
                        fd_relax_gas, fd_nuke_gas, fd_gas_vent, fd_smoke_vent,
                        fd_fungicidal_gas, fd_insecticidal_gas, fd_fire_vent,
                        fd_flame_burst, fd_electricity, fd_fatigue, fd_shock_vent,
                        fd_plasma, fd_laser
                    }
                };
                const auto killed = std::ranges::any_of( bee_killers,
                [&]( const field_type_id & k ) {
                    return curfield.find_field( k ) != nullptr;
                } );
                if( killed ) {
                    cur.set_field_intensity( 0 );
                } else {
                    // Wander randomly (player-chasing skipped — no player outside bubble).
                    const auto start = static_cast<size_t>( rng( 0, 7 ) );
                    std::ranges::for_each( std::views::iota( 0u, 8u ), [&]( size_t c ) {
                        auto dst = neighbor_tile( &sm, pos, local,
                                                  eight_dirs_sm[( start + c ) % 8], mb );
                        if( dst.valid() && !dst.get_field().find_field( fd_bees ) ) {
                            if( dst.get_field().add_field( fd_bees, cur.get_field_intensity(),
                                                           cur.get_field_age() ) ) {
                                ++dst.sm->field_count;
                                dst.sm->field_cache.push_back( dst.local );
                                dst.sm->is_uniform = false;
                            }
                            cur.set_field_intensity( 0 );
                        }
                    } );
                }
            }

            // ---- Radiation ----------------------------------------------
            if( !is_newborn && cur.extra_radiation_max() > 0 ) {
                const auto extra = rng( cur.extra_radiation_min(), cur.extra_radiation_max() );
                sm.set_radiation( local, sm.get_radiation( local ) + extra );
            }

            // ---- Wandering fields (in-submap only) ----------------------
            if( !is_newborn && cur_fd_type.wandering_field ) {
                const auto wtype   = cur_fd_type.wandering_field;
                const auto wintens = cur.get_field_intensity();
                const auto wradius = wintens - 1;
                for( auto wx = -wradius; wx <= wradius; ++wx ) {
                    for( auto wy = -wradius; wy <= wradius; ++wy ) {
                        const auto wlocal = local + point{ wx, wy };
                        if( wlocal.x < 0 || wlocal.x >= SEEX ||
                            wlocal.y < 0 || wlocal.y >= SEEY ) {
                            continue;
                        }
                        auto *wfld = sm.get_field( wlocal ).find_field( wtype );
                        if( wfld ) {
                            if( wfld->get_field_intensity() < wintens ) {
                                wfld->set_field_intensity( wfld->get_field_intensity() + 1 );
                            }
                        } else if( sm.get_field( wlocal ).add_field( wtype, wintens, 0_turns ) ) {
                            ++sm.field_count;
                            sm.field_cache.push_back( wlocal );
                        }
                    }
                }
            }

            // ---- Aging + half-life decay --------------------------------
            cur.set_field_age( cur.get_field_age() + 1_turns );
            const auto &fdata = cur.get_field_type().obj();
            if( fdata.half_life > 0_turns && cur.get_field_age() > 0_turns &&
                dice( 2, to_turns<int>( cur.get_field_age() ) ) >
                to_turns<int>( fdata.half_life ) ) {
                cur.set_field_age( 0_turns );
                cur.set_field_intensity( cur.get_field_intensity() - 1 );
            }

            if( !cur.is_field_alive() ) {
                --sm.field_count;
                curfield.remove_field( it++ );
            } else {
                ++it;
            }
        } // end field-entry loop
    } );

    if( sm.field_count == 0 ) {
        sm.field_cache.clear();
    }

    return has_fire;
}
