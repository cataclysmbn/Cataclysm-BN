#include "mondefense.h"

#include <algorithm>
#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "ballistics.h"
#include "bodypart.h"
#include "creature.h"
#include "creature_tracker.h"
#include "damage.h"
#include "effect.h"
#include "game.h"
#include "map.h"
#include "map_iterator.h"
#include "dispersion.h"
#include "enums.h"
#include "gun_mode.h"
#include "item.h"
#include "line.h"
#include "mattack_actors.h"
#include "mattack_common.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "player.h"
#include "point.h"
#include "projectile.h"
#include "rng.h"
#include "sounds.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"

static const ammo_effect_str_id ammo_effect_DRAW_AS_LINE( "DRAW_AS_LINE" );
static const ammo_effect_str_id ammo_effect_NO_DAMAGE_SCALING( "NO_DAMAGE_SCALING" );
static const efftype_id effect_command_buff( "command_buff" );

void mdefense::none( monster &, Creature *, const dealt_projectile_attack * )
{
}

void mdefense::zapback( monster &m, Creature *const source,
                        dealt_projectile_attack const *proj )
{
    if( source == nullptr ) {
        return;
    }
    // If we have a projectile, we're a ranged attack, no zapback.
    if( proj != nullptr ) {
        return;
    }

    if( const player *const foe = dynamic_cast<player *>( source ) ) {
        // Players/NPCs can avoid the shock if they wear non-conductive gear on their hands
        for( const item * const &i : foe->worn ) {
            if( !i->conductive()
                && ( ( i->get_coverage( bodypart_id( "hand_l" ) ) >= 95 ) ||
                     i->get_coverage( bodypart_id( "hand_r" ) ) >= 95 ) ) {
                return;
            }
        }
        // Players/NPCs can avoid the shock by using non-conductive weapons

        if( !foe->primary_weapon().conductive() ) {
            if( foe->reach_attacking ) {
                return;
            }
            if( !foe->primary_weapon().is_null() ) {
                return;
            }
        }
    }

    if( source->is_elec_immune() ) {
        return;
    }

    if( get_avatar().sees( source->pos() ) ) {
        const auto msg_type = source == &get_avatar() ? m_bad : m_info;
        add_msg( msg_type, _( "Striking %1$s shocks %2$s!" ),
                 m.disp_name(), source->disp_name() );
    }

    const damage_instance shock {
        DT_ELECTRIC, static_cast<float>( rng( 1, 5 ) )
    };
    source->deal_damage( &m, bodypart_id( "arm_l" ), shock );
    source->deal_damage( &m, bodypart_id( "arm_r" ), shock );

    source->check_dead_state();
}

void mdefense::acidsplash( monster &m, Creature *const source,
                           dealt_projectile_attack const *const proj )
{
    if( source == nullptr ) {
        return;
    }
    size_t num_drops = rng( 4, 6 );
    // Would be useful to have the attack data here, for cutting vs. bashing etc.
    if( proj ) {
        // Projectile didn't penetrate the target, no acid will splash out of it.
        if( proj->dealt_dam.total_damage() <= 0 ) {
            return;
        }
        // Less likely for a projectile to deliver enough force
        if( !one_in( 3 ) ) {
            return;
        }
    } else {
        if( const player *const foe = dynamic_cast<player *>( source ) ) {
            if( foe->primary_weapon().is_melee( DT_CUT ) || foe->primary_weapon().is_melee( DT_STAB ) ) {
                num_drops += rng( 3, 4 );
            }
            if( foe->unarmed_attack() ) {
                const damage_instance acid_burn{
                    DT_ACID, static_cast<float>( rng( 1, 5 ) )
                };
                source->deal_damage( &m, one_in( 2 ) ? bodypart_id( "hand_l" ) : bodypart_id( "hand_r" ),
                                     acid_burn );
                source->add_msg_if_player( m_bad, _( "Acid covering %s burns your hand!" ), m.disp_name() );
            }
        }
    }

    // Don't splatter directly on the `m`, that doesn't work well
    std::vector<tripoint> pts = closest_points_first( source->pos(), 1 );
    pts.erase( std::remove( pts.begin(), pts.end(), m.pos() ), pts.end() );

    projectile prj;
    prj.speed = 10;
    prj.range = 4;
    prj.add_effect( ammo_effect_DRAW_AS_LINE );
    prj.add_effect( ammo_effect_NO_DAMAGE_SCALING );
    prj.impact.add_damage( DT_ACID, rng( 1, 3 ) );
    for( size_t i = 0; i < num_drops; i++ ) {
        const tripoint &target = random_entry( pts );
        projectile_attack( prj, m.pos(), target, dispersion_sources{ 1200 }, &m );
    }

    if( get_avatar().sees( m.pos() ) ) {
        add_msg( m_warning, _( "Acid sprays out of %s as it is hit!" ), m.disp_name() );
    }
}

void mdefense::return_fire( monster &m, Creature *source, const dealt_projectile_attack *proj )
{
    // No return fire for untargeted projectiles, i.e. from explosions.
    if( source == nullptr ) {
        return;
    }

    // No return fire from dead monsters.
    // Second set is due to how the EMP projector kills critters
    if( m.is_dead_state() || !g->critter_tracker->find( m.pos() ) ) {
        return;
    }

    // If target actually was not damaged by projectile - then do not bother
    // Also it covers potential exploit - peek throwing potential can be used to exhaust turret ammo
    if( proj != nullptr && proj->dealt_dam.total_damage() == 0 ) {
        return;
    }

    // No return fire if attacker is seen
    if( m.sees( *source ) ) {
        return;
    }

    const int distance_to_source = rl_dist( m.pos(), source->pos() );

    // TODO: implement different rule, dependent on sound and probably some other things
    const tripoint fire_point = source->pos();
    // Add some innacuracy since it is blind fire
    int dispersion = 150;

    for( const std::pair<const std::string, mtype_special_attack> &attack : m.type->special_attacks ) {
        if( attack.second->id == "gun" ) {

            const gun_actor *gunactor = dynamic_cast<const gun_actor *>( attack.second.get() );
            if( gunactor->get_max_range() < distance_to_source ) {
                continue;
            }
            sounds::sound( m.pos(), 50, sounds::sound_t::alert,
                           _( "Detected shots from unseen attacker, return fire mode engaged." ) );

            gunactor->shoot( m, fire_point, gun_mode_id( "DEFAULT" ), dispersion );

            // We only return fire once with one gun.
            return;
        }
    }
}

void mdefense::revenge_aggro( monster &m, Creature *source, const dealt_projectile_attack * )
{
    // @todo Have it use monattack instead of re-implementing it
    if( source == nullptr ) {
        return;
    }

    size_t aggroed = 0;
    for( monster &ally : g->all_monsters() ) {
        if( rl_dist_fast( ally.pos(), m.pos() ) <= 40 &&
            ally.attitude_to( m ) == Attitude::A_FRIENDLY ) {
            ally.anger = std::max( ally.anger, 100 );
            ally.morale = std::max( ally.morale, 100 );
            if( ally.move_target() != source->pos() &&
                ally.attitude_to( *source ) == Attitude::A_HOSTILE ) {
                ally.set_dest( source->pos() );
                aggroed++;
            }

            time_duration buff_dur = ally.get_effect_dur( effect_command_buff );
            if( buff_dur < effect_command_buff->get_max_duration() ) {
                ally.add_effect( effect_command_buff,
                                 effect_command_buff->get_max_duration() - buff_dur );
            }
        }
    }

    if( source->is_avatar() ) {
        if( aggroed > 25 ) {
            add_msg( m_bad, _( "You feel intensely hated for a moment." ) );
        } else if( aggroed > 5 ) {
            add_msg( m_warning, _( "You feel an angry presence." ) );
        }
    }
}
