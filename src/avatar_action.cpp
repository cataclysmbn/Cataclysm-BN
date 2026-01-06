#include "avatar_action.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

#include "action.h"
#include "activity_actor_definitions.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "character_functions.h"
#include "character_martial_arts.h"
#include "character_turn.h"
#include "creature.h"
#include "cursesdef.h"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "gun_mode.h"
#include "int_id.h"
#include "inventory.h"
#include "item.h"
#include "item_functions.h"
#include "item_contents.h"
#include "iuse_actor.h"
#include "itype.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "math_defines.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "player_activity.h"
#include "projectile.h"
#include "ranged.h"
#include "ret_val.h"
#include "rng.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

class player;

static const efftype_id effect_amigara( "amigara" );
static const efftype_id effect_glowing( "glowing" );
static const efftype_id effect_harnessed( "harnessed" );
static const efftype_id effect_hit_by_player( "hit_by_player" );
static const efftype_id effect_onfire( "onfire" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_relax_gas( "relax_gas" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_stunned( "stunned" );

static const itype_id itype_grass( "grass" );
static const itype_id itype_swim_fins( "swim_fins" );
static const itype_id itype_underbrush( "underbrush" );

static const skill_id skill_swimming( "swimming" );

static const trait_id trait_BRAWLER( "BRAWLER" );
static const trait_id trait_BURROW( "BURROW" );
static const trait_id trait_GRAZER( "GRAZER" );
static const trait_id trait_RUMINANT( "RUMINANT" );
static const trait_id trait_SHELL2( "SHELL2" );

static const std::string flag_RAMP_END( "RAMP_END" );
static const std::string flag_SWIMMABLE( "SWIMMABLE" );
static const std::string flag_LADDER( "LADDER" );

#define dbg(x) DebugLog((x), DC::SDL)

bool avatar_action::move( avatar &you, map &m, const tripoint &d )
{
    if( ( !g->check_safe_mode_allowed() ) || you.has_active_mutation( trait_SHELL2 ) ) {
        if( you.has_active_mutation( trait_SHELL2 ) ) {
            add_msg( m_warning, _( "You can't move while in your shell.  Deactivate it to go mobile." ) );
        }
        return false;
    }
    const bool is_riding = you.is_mounted();
    tripoint dest_loc;
    if( d.z == 0 && you.has_effect( effect_stunned ) ) {
        dest_loc.x = rng( you.posx() - 1, you.posx() + 1 );
        dest_loc.y = rng( you.posy() - 1, you.posy() + 1 );
        dest_loc.z = you.posz();
    } else {
        dest_loc = you.pos() + d;
    }

    if( dest_loc == you.pos() ) {
        // Well that sure was easy
        return true;
    }

    const bool auto_features = get_option<bool>( "AUTO_FEATURES" );
    const bool auto_mine = auto_features && get_option<bool>( "AUTO_MINING" );

    const auto can_use_ladder = [&]() -> bool {
        if( is_riding || m.has_floor_or_support( dest_loc ) )
        {
            return false;
        }
        return m.has_flag( flag_LADDER, dest_loc + tripoint_below );
    };

    bool via_ramp = false;
    if( m.has_flag( TFLAG_RAMP_UP, dest_loc ) ) {
        dest_loc.z += 1;
        via_ramp = true;
    } else if( m.has_flag( TFLAG_RAMP_DOWN, dest_loc ) || can_use_ladder() ) {
        dest_loc.z -= 1;
        via_ramp = true;
    }

    if( m.has_flag( TFLAG_MINEABLE, dest_loc ) && g->mostseen == 0 &&
        auto_mine && !m.veh_at( dest_loc ) && !you.is_underwater() && !you.has_effect( effect_stunned ) &&
        !is_riding ) {
        item &digging_tool = you.primary_weapon();
        if( digging_tool.has_flag( flag_DIG_TOOL ) ) {
            if( digging_tool.type->can_use( "JACKHAMMER" ) && digging_tool.ammo_sufficient() ) {
                you.invoke_item( &digging_tool, "JACKHAMMER", dest_loc );
                // don't move into the tile until done mining
                you.defer_move( dest_loc );
                return true;
            } else if( digging_tool.type->can_use( "PICKAXE" ) ) {
                you.invoke_item( &digging_tool, "PICKAXE", dest_loc );
                // don't move into the tile until done mining
                you.defer_move( dest_loc );
                return true;
            }
        }
        if( you.has_trait( trait_BURROW ) ) {
            item *burrowing_item = item::spawn_temporary( itype_id( "fake_burrowing" ) );
            you.invoke_item( burrowing_item, "BURROW", dest_loc );
            // don't move into the tile until done mining
            you.defer_move( dest_loc );
            return true;
        }
    }

    // If the player is *attempting to* move on the X axis, update facing direction of their sprite to match.
    point new_d( dest_loc.xy() + point( -you.posx(), -you.posy() ) );

    if( !tile_iso ) {
        if( new_d.x > 0 ) {
            you.facing = FD_RIGHT;
            if( is_riding ) {
                you.mounted_creature->facing = FD_RIGHT;
            }
        } else if( new_d.x < 0 ) {
            you.facing = FD_LEFT;
            if( is_riding ) {
                you.mounted_creature->facing = FD_LEFT;
            }
        }
    } else {
        //
        // iso:
        //
        // right key            =>  +x -y       FD_RIGHT
        // left key             =>  -x +y       FD_LEFT
        // up key               =>  +x +y       ______
        // down key             =>  -x -y       ______
        // y: left-up key       =>  __ +y       FD_LEFT
        // u: right-up key      =>  +x __       FD_RIGHT
        // b: left-down key     =>  -x __       FD_LEFT
        // n: right-down key    =>  __ -y       FD_RIGHT
        //
        // right key            =>  +x -y       FD_RIGHT
        // u: right-up key      =>  +x __       FD_RIGHT
        // n: right-down key    =>  __ -y       FD_RIGHT
        // up key               =>  +x +y       ______
        // down key             =>  -x -y       ______
        // left key             =>  -x +y       FD_LEFT
        // y: left-up key       =>  __ +y       FD_LEFT
        // b: left-down key     =>  -x __       FD_LEFT
        //
        // right key            =>  +x +y       FD_RIGHT
        // u: right-up key      =>  +x __       FD_RIGHT
        // n: right-down key    =>  __ +y       FD_RIGHT
        // up key               =>  +x -y       ______
        // left key             =>  -x -y       FD_LEFT
        // b: left-down key     =>  -x __       FD_LEFT
        // y: left-up key       =>  __ -y       FD_LEFT
        // down key             =>  -x +y       ______
        //
        if( new_d.x >= 0 && new_d.y >= 0 ) {
            you.facing = FD_RIGHT;
            if( is_riding ) {
                auto mons = you.mounted_creature.get();
                mons->facing = FD_RIGHT;
            }
        }
        if( new_d.y <= 0 && new_d.x <= 0 ) {
            you.facing = FD_LEFT;
            if( is_riding ) {
                auto mons = you.mounted_creature.get();
                mons->facing = FD_LEFT;
            }
        }
    }

    if( you.has_effect( effect_amigara ) ) {
        int curdist = INT_MAX;
        int newdist = INT_MAX;
        const tripoint minp = tripoint( 0, 0, you.posz() );
        const tripoint maxp = tripoint( MAPSIZE_X, MAPSIZE_Y, you.posz() );
        for( const tripoint &pt : m.points_in_rectangle( minp, maxp ) ) {
            if( m.ter( pt ) == t_fault ) {
                int dist = rl_dist( pt, you.pos() );
                if( dist < curdist ) {
                    curdist = dist;
                }
                dist = rl_dist( pt, dest_loc );
                if( dist < newdist ) {
                    newdist = dist;
                }
            }
        }
        if( newdist > curdist ) {
            add_msg( m_info, _( "You cannot pull yourself away from the faultline…" ) );
            return false;
        }
    }

    dbg( DL::Debug ) << "game:plmove: From " << you.pos() << " to " << dest_loc;

    // Check if our movement is actually an attack on a monster or npc
    // Are we displacing a monster?

    bool attacking = false;
    if( g->critter_at( dest_loc ) ) {
        attacking = true;
    }

    if( !you.move_effects( attacking ) ) {
        you.moves -= 100;
        return false;
    }

    if( m.obstructed_by_vehicle_rotation( you.pos(), dest_loc ) ) {
        add_msg( _( "You can't walk through that vehicle's wall." ) );
        return false;
    }

    if( monster *const mon_ptr = g->critter_at<monster>( dest_loc, true ) ) {
        monster &critter = *mon_ptr;
        // Additional checking to make sure we won't take a swing at friendly monsters.
        Character &u = get_player_character();
        monster_attitude att = critter.attitude( ( &u ) );
        if( critter.friendly == 0 &&
            !critter.has_effect( effect_pet ) && att != MATT_FRIEND ) {
            if( you.is_auto_moving() ) {
                add_msg( m_warning, _( "Monster in the way.  Auto-move canceled." ) );
                add_msg( m_info, _( "Move into the monster to attack." ) );
                you.clear_destination();
                return false;
            } else {
            }
            if( you.has_effect( effect_relax_gas ) ) {
                if( one_in( 8 ) ) {
                    add_msg( m_good, _( "Your willpower asserts itself, and so do you!" ) );
                } else {
                    you.moves -= rng( 2, 8 ) * 10;
                    add_msg( m_bad, _( "You're too pacified to strike anything…" ) );
                    return false;
                }
            }
            // Ask for confirmation before attacking a neutral creature unless we've already taken a swing at it
            if( ( att == MATT_IGNORE || att == MATT_FLEE ) &&
                get_option<bool>( "QUERY_BEFORE_ATTACKING_NEUTRAL" ) &&
                !critter.has_effect( effect_hit_by_player ) &&
                !query_yn( _( "You may be attacked!  Proceed?" ) ) ) {
                return false;
            }

            you.melee_attack( critter, true );
            if( critter.is_hallucination() ) {
                critter.die( &you );
            }
            g->draw_hit_mon( dest_loc, critter, critter.is_dead() );
            return false;
        } else if( critter.has_flag( MF_IMMOBILE ) || critter.has_effect( effect_harnessed ) ||
                   critter.has_effect( effect_ridden ) ) {
            add_msg( m_info, _( "You can't displace your %s." ), critter.name() );
            return false;
        }
        // Successful displacing is handled (much) later
    }
    // If not a monster, maybe there's an NPC there
    if( npc *const np_ = g->critter_at<npc>( dest_loc ) ) {
        npc &np = *np_;
        if( you.is_auto_moving() ) {
            add_msg( _( "NPC in the way, Auto-move canceled." ) );
            add_msg( m_info, _( "Move into the NPC to interact or attack." ) );
            you.clear_destination();
            return false;
        }

        if( !np.is_enemy() ) {
            g->npc_menu( np );
            return false;
        }

        you.melee_attack( np, true );
        np.make_angry();
        return false;
    }

    // GRAB: pre-action checking.
    int door_part = -1;
    const optional_vpart_position vp_src = m.veh_at( you.pos() );
    const optional_vpart_position vp_dst = m.veh_at( dest_loc );
    vehicle *const src_veh = veh_pointer_or_null( vp_src );
    vehicle *const dst_veh = veh_pointer_or_null( vp_dst );

    bool veh_closed_door = false;
    bool outside_vehicle = ( src_veh == nullptr || src_veh != dst_veh );
    if( dst_veh != nullptr ) {
        door_part = dst_veh->next_part_to_open( vp_dst->part_index(), outside_vehicle );
        veh_closed_door = door_part >= 0 && !dst_veh->part( door_part ).open;
    }

    if( src_veh != nullptr && std::abs( src_veh->velocity ) > 100 ) {
        if( dst_veh == nullptr ) {
            if( query_yn( _( "Dive from moving vehicle?" ) ) ) {
                g->moving_vehicle_dismount( dest_loc );
            }
            return false;
        } else if( dst_veh != src_veh ) {
            add_msg( m_info, _( "There is another vehicle in the way." ) );
            return false;
        } else if( !vp_dst.part_with_feature( "BOARDABLE", true ) ) {
            add_msg( m_info, _( "That part of the vehicle is currently unsafe." ) );
            return false;
        }
    }
    bool toSwimmable = m.has_flag( flag_SWIMMABLE, dest_loc );
    bool toDeepWater = m.has_flag( TFLAG_DEEP_WATER, dest_loc );
    bool fromSwimmable = m.has_flag( flag_SWIMMABLE, you.pos() );
    bool fromDeepWater = m.has_flag( TFLAG_DEEP_WATER, you.pos() );
    bool fromBoat = src_veh != nullptr;
    bool toBoat = dst_veh != nullptr;
    if( is_riding ) {
        if( !you.check_mount_will_move( dest_loc ) ) {
            if( you.is_auto_moving() ) {
                you.clear_destination();
            }
            you.moves -= 20;
            return false;
        }
    }
    // Dive into water!
    if( toSwimmable && toDeepWater && !toBoat ) {
        // Requires confirmation if we were on dry land previously
        if( is_riding ) {
            auto mon = you.mounted_creature.get();
            if( !mon->swims() || mon->get_size() < you.get_size() + 2 ) {
                add_msg( m_warning, _( "The %s cannot swim while it is carrying you!" ), mon->get_name() );
                return false;
            }
        }
        if( ( fromSwimmable && fromDeepWater && !fromBoat ) || query_yn( _( "Dive into the water?" ) ) ) {
            if( ( !fromDeepWater || fromBoat ) && you.swim_speed() < 500 ) {
                add_msg( _( "You start swimming." ) );
                add_msg( m_info, _( "%s to dive underwater." ),
                         press_x( ACTION_MOVE_DOWN ) );
            }
            avatar_action::swim( get_map(), get_avatar(), dest_loc );
        }

        g->on_move_effects();
        return true;
    }

    // Vehicle Openable
    if( dst_veh ) {
        if( veh_closed_door ) {
            if( !m.open_door( &you, dest_loc, !outside_vehicle ) ) {
                return false;
            }

            add_msg( _( "You open the %1$s's %2$s." ), dst_veh->name,
                     dst_veh->part_info( door_part ).name() );
            you.moves -= 100;
            // if auto-move is on, continue moving next turn
            if( you.is_auto_moving() ) {
                you.defer_move( dest_loc );
            }
            return true;
        }

        if( g->walk_move( dest_loc, via_ramp ) ) {
            return true;
        }
    }

    // Wooden Fence Gate (or equivalently walkable doors):
    // open it if we are walking
    // vault over it if we are running
    if( m.passable_ter_furn( dest_loc )
        && you.movement_mode_is( CMM_WALK )
        && m.open_door( &you, dest_loc, !m.is_outside( you.pos() ) ) ) {
        you.moves -= 100;
        // if auto-move is on, continue moving next turn
        if( you.is_auto_moving() ) {
            you.defer_move( dest_loc );
        }
        return true;
    }

    // Openable furniture (wardrobes, etc)
    if( m.furn( dest_loc ) != f_safe_c
        && m.open_door( &you, dest_loc, !m.is_outside( you.pos() ) ) ) {
        you.moves -= 100;
        // if auto-move is on, continue moving next turn
        if( you.is_auto_moving() ) {
            you.defer_move( dest_loc );
        }
        return true;
    }

    // Ladder - only try to climb up if:
    // 1. Standing on a ladder
    // 2. The destination at current z-level is impassable (blocked by something)
    // 3. There's a valid destination above
    if( !is_riding
        && m.has_flag( flag_LADDER, you.pos() )
        && !m.passable( dest_loc )
        && g->walk_move( dest_loc + tripoint_above ) ) {
        return true;
    }

    // Regular Move
    if( g->walk_move( dest_loc, via_ramp ) ) {
        return true;
    }

    // Invalid move
    const bool waste_moves = ( you.is_blind() && you.clairvoyance() < 1 ) ||
                             you.has_effect( effect_stunned );
    if( waste_moves || dest_loc.z != you.posz() ) {
        add_msg( _( "You bump into the %s!" ), m.obstacle_name( dest_loc ) );
        // Only lose movement if we're blind
        if( waste_moves ) {
            you.moves -= 100;
        }
        // Add to map memory if blind
        if( you.is_blind() ) {
            std::string obstacle;
            if( m.veh_at( dest_loc ) ) {
                char part_mod = 0;
                const vpart_id &vp_id = m.veh_at( dest_loc )->vehicle().part_id_string( m.veh_at(
                                            dest_loc )->part_index(), false, part_mod );
                obstacle = "vp_" + vp_id.str();
            } else {
                obstacle = m.has_furn( dest_loc ) ? m.furn( dest_loc ).id().str() : m.ter(
                               dest_loc ).id().str();
            }
            // TODO: Figure out how to make subtile and rotation work right here
            you.memorize_tile( m.getabs( dest_loc ), obstacle, 0, 0 );
        }
    } else if( m.ter( dest_loc ) == t_door_locked || m.ter( dest_loc ) == t_door_locked_peep ||
               m.ter( dest_loc ) == t_door_locked_alarm || m.ter( dest_loc ) == t_door_locked_interior ) {
        // Don't drain move points for learning something you could learn just by looking
        add_msg( _( "That door is locked!" ) );
    } else if( m.ter( dest_loc ) == t_door_bar_locked ) {
        add_msg( _( "You rattle the bars but the door is locked!" ) );
    }
    return false;
}

bool avatar_action::ramp_move( avatar &you, map &m, const tripoint &dest_loc )
{
    if( dest_loc.z != you.posz() ) {
        // No recursive ramp_moves
        return false;
    }

    // We're moving onto a tile with no support, check if it has a ramp below
    if( !m.has_floor_or_support( dest_loc ) ) {
        tripoint below( dest_loc.xy(), dest_loc.z - 1 );
        if( m.has_flag( TFLAG_RAMP, below ) ) {
            // But we're moving onto one from above
            const tripoint dp = dest_loc - you.pos();
            move( you, m, tripoint( dp.xy(), -1 ) );
            // No penalty for misaligned stairs here
            // Also cheaper than climbing up
            return true;
        }

        return false;
    }

    if( !m.has_flag( TFLAG_RAMP, you.pos() ) ||
        m.passable( dest_loc ) ) {
        return false;
    }

    // Try to find an aligned end of the ramp that will make our climb faster
    // Basically, finish walking on the stairs instead of pulling self up by hand
    bool aligned_ramps = false;
    for( const tripoint &pt : m.points_in_radius( you.pos(), 1 ) ) {
        if( rl_dist( pt, dest_loc ) < 2 && m.has_flag( flag_RAMP_END, pt ) ) {
            aligned_ramps = true;
            break;
        }
    }

    const tripoint above_u( you.posx(), you.posy(), you.posz() + 1 );
    if( m.has_floor_or_support( above_u ) ) {
        add_msg( m_warning, _( "You can't climb here - there's a ceiling above." ) );
        return false;
    }

    const tripoint dp = dest_loc - you.pos();
    const tripoint old_pos = you.pos();
    move( you, m, tripoint( dp.xy(), 1 ) );
    // We can't just take the result of the above function here
    if( you.pos() != old_pos ) {
        you.moves -= 50 + ( aligned_ramps ? 0 : 50 );
    }

    return true;
}

void avatar_action::swim( map &m, avatar &you, const tripoint &p )
{
    if( !m.has_flag( flag_SWIMMABLE, p ) ) {
        debugmsg( "Tried to swim in %s!", m.tername( p ) );
        return;
    }
    if( you.has_effect( effect_onfire ) ) {
        add_msg( _( "The water puts out the flames!" ) );
        you.remove_effect( effect_onfire );
        if( you.is_mounted() ) {
            monster *mon = you.mounted_creature.get();
            if( mon->has_effect( effect_onfire ) ) {
                mon->remove_effect( effect_onfire );
            }
        }
    }
    if( you.has_effect( effect_glowing ) ) {
        add_msg( _( "The water washes off the glowing goo!" ) );
        you.remove_effect( effect_glowing );
    }
    if( m.has_flag( TFLAG_WATER_CUBE, p ) && !you.is_underwater() ) {
        you.oxygen = 30 + 2 * you.str_cur;
        you.set_underwater( true );
    }
    int movecost = you.swim_speed();
    if( !you.worn_with_flag( flag_FLOTATION ) && x_in_y( 3, 4 ) ) {
        you.practice( skill_swimming, 1 );
    }
    if( movecost >= 500 ) {
        if( !you.is_underwater() &&
            !( you.shoe_type_count( itype_swim_fins ) == 2 ||
               ( you.shoe_type_count( itype_swim_fins ) == 1 && one_in( 2 ) ) ) ) {
            add_msg( m_bad, _( "You sink like a rock!" ) );
            you.set_underwater( true );
            ///\EFFECT_STR increases breath-holding capacity while sinking
            you.oxygen = 30 + 2 * you.str_cur;
        }
    }
    if( you.oxygen <= 5 && you.is_underwater() ) {
        if( movecost < 500 ) {
            popup( _( "You need to breathe!  (%s to surface.)" ), press_x( ACTION_MOVE_UP ) );
        } else {
            popup( _( "You need to breathe but you can't swim!  Get to dry land, quick!" ) );
        }
    }
    bool diagonal = ( p.x != you.posx() && p.y != you.posy() );
    if( you.in_vehicle ) {
        m.unboard_vehicle( you.pos() );
    }
    if( you.is_mounted() && m.veh_at( you.pos() ).part_with_feature( VPFLAG_BOARDABLE, true ) ) {
        add_msg( m_warning, _( "You cannot board a vehicle while mounted." ) );
        return;
    }
    if( const auto vp = m.veh_at( p ).part_with_feature( VPFLAG_BOARDABLE, true ) ) {
        if( !vp->vehicle().handle_potential_theft( you ) ) {
            return;
        }
    }
    if( m.has_flag_ter_or_furn( TFLAG_HIDE_PLACE, p ) && you.get_size() <= creature_size::medium ) {
        add_msg( m_good, _( "You are hiding in the %s." ), m.name( p ) );
    }
    you.setpos( p );
    g->update_map( you );

    cata_event_dispatch::avatar_moves( you, m, p );

    if( m.veh_at( you.pos() ).part_with_feature( VPFLAG_BOARDABLE, true ) ) {
        m.board_vehicle( you.pos(), &you );
    }
    you.moves -= ( movecost > 200 ? 200 : movecost ) * ( trigdist && diagonal ? M_SQRT2 : 1 );
    you.rust_iron_items();

    if( !you.is_mounted() ) {
        you.burn_move_stamina( movecost );
    }

    body_part_set drenchFlags{ {
            bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ), bodypart_str_id( "torso" ), bodypart_str_id( "arm_l" ),
            bodypart_str_id( "arm_r" ), bodypart_str_id( "foot_l" ), bodypart_str_id( "foot_r" ), bodypart_str_id( "hand_l" ), bodypart_str_id( "hand_r" )
        }
    };

    if( you.is_underwater() ) {
        drenchFlags.unify_set( { { bodypart_str_id( "head" ), bodypart_str_id( "eyes" ), bodypart_str_id( "mouth" ) } } );
    }
    you.drench( 100, drenchFlags, true );
}

static float rate_critter( const Creature &c )
{
    const npc *np = dynamic_cast<const npc *>( &c );
    if( np != nullptr ) {
        return npc_ai::wielded_value( *np );
    }

    const monster *m = dynamic_cast<const monster *>( &c );
    return m->type->difficulty;
}

void avatar_action::autoattack( avatar &you, map &m )
{
    int reach = you.primary_weapon().reach_range( you );
    std::vector<Creature *> critters = ranged::targetable_creatures( you, reach );
    critters.erase( std::remove_if( critters.begin(), critters.end(), []( const Creature * c ) {
        if( !c->is_npc() ) {
            return false;
        }
        return !dynamic_cast<const npc *>( c )->is_enemy();
    } ), critters.end() );
    if( critters.empty() ) {
        add_msg( m_info, _( "No hostile creature in reach.  Waiting a turn." ) );
        if( g->check_safe_mode_allowed() ) {
            character_funcs::do_pause( you );
        }
        return;
    }

    Creature &best = **std::max_element( critters.begin(), critters.end(),
    []( const Creature * l, const Creature * r ) {
        return rate_critter( *l ) > rate_critter( *r );
    } );

    const tripoint diff = best.pos() - you.pos();
    if( std::abs( diff.x ) <= 1 && std::abs( diff.y ) <= 1 && diff.z == 0 ) {
        move( you, m, tripoint( diff.xy(), 0 ) );
        return;
    }

    you.reach_attack( best.pos() );
}

bool avatar_action::can_fire_weapon( avatar &you, const map &m, const item &weapon )
{
    if( !weapon.is_gun() ) {
        debugmsg( "Expected item to be a gun" );
        return false;
    }

    if( you.has_trait( trait_BRAWLER ) ) {
        add_msg( m_good, _( "You refuse to use this ranged weapon." ) );
        return false;
    }

    if( you.has_effect( effect_relax_gas ) ) {
        if( one_in( 5 ) ) {
            add_msg( m_good, _( "Your eyes steel, and you raise your weapon!" ) );
        } else {
            you.moves -= rng( 2, 5 ) * 10;
            add_msg( m_bad, _( "You can't fire your weapon, it's too heavy…" ) );
            return false;
        }
    }

    std::vector<std::string> messages;

    for( const std::pair<const gun_mode_id, gun_mode> &mode_map : weapon.gun_all_modes() ) {
        const bool check_common = ranged::gunmode_checks_common( you, m, messages, mode_map.second );
        const bool check_weapon = ranged::gunmode_checks_weapon( you, m, messages, mode_map.second );
        const bool can_use_mode = check_common && check_weapon;
        if( can_use_mode ) {
            return true;
        }
    }

    for( const std::string &message : messages ) {
        add_msg( m_info, message );
    }
    return false;
}

bool avatar_action::will_fire_turret( avatar &you )
{
    if( you.has_trait( trait_BRAWLER ) ) {
        add_msg( m_bad, _( "You refuse to use this ranged weapon" ) );
        return false;
    }

    if( you.has_effect( effect_relax_gas ) ) {
        if( one_in( 5 ) ) {
            add_msg( m_good, _( "Your eyes steel, and you aim your weapon!" ) );
        } else {
            you.moves -= rng( 2, 5 ) * 10;
            add_msg( m_bad, _( "You are too pacified to aim the turret…" ) );
            return false;
        }
    }
    return true;
}

bool avatar_action::can_fire_turret( avatar &you, const map &m, const turret_data &turret )
{
    const item &weapon = turret.base();
    if( !weapon.is_gun() ) {
        debugmsg( "Expected turret base to be a gun." );
        return false;
    }

    if( !will_fire_turret( you ) ) {
        return false;
    }

    switch( turret.query() ) {
        case turret_data::status::no_ammo:
            add_msg( m_bad, _( "The %s is out of ammo." ), turret.name() );
            return false;

        case turret_data::status::no_power:
            add_msg( m_bad, _( "The %s is not powered." ), turret.name() );
            return false;

        case turret_data::status::ready:
            break;

        default:
            debugmsg( "Unknown turret status" );
            return false;
    }

    std::vector<std::string> messages;

    for( const std::pair<const gun_mode_id, gun_mode> &mode_map : weapon.gun_all_modes() ) {
        bool can_use_mode = ranged::gunmode_checks_common( you, m, messages, mode_map.second );
        if( can_use_mode ) {
            return true;
        }
    }

    for( const std::string &message : messages ) {
        add_msg( m_info, message );
    }
    return false;
}

void avatar_action::fire_wielded_weapon( avatar &you )
{
    item &weapon = you.primary_weapon();
    if( weapon.is_gunmod() ) {
        add_msg( m_info,
                 _( "The %s must be attached to a gun, it can not be fired separately." ),
                 weapon.tname() );
        return;
    } else if( !weapon.is_gun() ) {
        return;
    } else if( weapon.ammo_data() && weapon.type->gun &&
               !weapon.ammo_types().contains( weapon.ammo_data()->ammo->type ) ) {
        std::string ammoname = weapon.ammo_current()->nname( 1 );
        add_msg( m_info, _( "The %s can't be fired while loaded with incompatible ammunition %s" ),
                 weapon.tname(), ammoname );
        return;
    }

    you.assign_activity( std::make_unique<player_activity>
                         ( aim_activity_actor::use_wielded() ), false );
}

void avatar_action::fire_ranged_mutation( avatar &you, detached_ptr<item> &&fake_gun )
{
    you.assign_activity( std::make_unique<player_activity>( aim_activity_actor::use_mutation(
                             std::move( fake_gun ) ) ), false );
}

void avatar_action::fire_ranged_bionic( avatar &you, detached_ptr<item> &&fake_gun,
                                        const units::energy &cost_per_shot )
{
    you.assign_activity( std::make_unique<player_activity>( aim_activity_actor::use_bionic(
                             std::move( fake_gun ), cost_per_shot ) ),
                         false );
}

void avatar_action::fire_turret_manual( avatar &you, map &m, turret_data &turret )
{
    if( !can_fire_turret( you, m, turret ) ) {
        return;
    }

    g->temp_exit_fullscreen();
    target_handler::trajectory trajectory = target_handler::mode_turret_manual( you, turret );

    if( !trajectory.empty() ) {
        turret.fire( you, trajectory.back() );
    }
    g->reenter_fullscreen();
}

void avatar_action::mend( avatar &you, item *loc )
{
    if( !loc ) {
        if( you.is_armed() ) {
            loc = &you.primary_weapon();
        } else {
            add_msg( m_info, _( "You're not wielding anything." ) );
            return;
        }
    }

    if( you.has_item( *loc ) ) {
        avatar_funcs::mend_item( you, *loc );
    }
}

bool avatar_action::eat_here( avatar &you )
{
    map &here = get_map();
    if( ( you.has_active_mutation( trait_RUMINANT ) || you.has_active_mutation( trait_GRAZER ) ) &&
        ( here.ter( you.pos() ) == t_underbrush || here.ter( you.pos() ) == t_shrub ) ) {
        item &food = *item::spawn_temporary( itype_underbrush, calendar::turn, 1 );
        if( you.get_stored_kcal() > you.max_stored_kcal() -
            food.get_comestible()->default_nutrition.kcal ) {
            add_msg( _( "You're too full to eat the leaves from the %s." ), here.ter( you.pos() )->name() );
            return true;
        } else {
            you.moves -= 400;
            here.ter_set( you.pos(), t_grass );
            add_msg( _( "You eat the underbrush." ) );
            you.eat( food );
            return true;
        }
    }
    if( you.has_active_mutation( trait_GRAZER ) && ( here.ter( you.pos() ) == t_grass ||
            here.ter( you.pos() ) == t_grass_long || here.ter( you.pos() ) == t_grass_tall ) ) {
        item &food = *item::spawn_temporary( itype_grass, calendar::turn, 1 );
        if( you.get_stored_kcal() > you.max_stored_kcal() -
            food.get_comestible()->default_nutrition.kcal ) {
            add_msg( _( "You're too full to graze." ) );
            return true;
        } else {
            you.moves -= 400;
            add_msg( _( "You eat the grass." ) );
            you.eat( food );
            if( here.ter( you.pos() ) == t_grass_tall ) {
                here.ter_set( you.pos(), t_grass_long );
            } else if( here.ter( you.pos() ) == t_grass_long ) {
                here.ter_set( you.pos(), t_grass );
            } else {
                here.ter_set( you.pos(), t_dirt );
            }
            return true;
        }
    }
    if( you.has_active_mutation( trait_GRAZER ) ) {
        if( here.ter( you.pos() ) == t_grass_golf ) {
            add_msg( _( "This grass is too short to graze." ) );
            return true;
        } else if( here.ter( you.pos() ) == t_grass_dead ) {
            add_msg( _( "This grass is dead and too mangled for you to graze." ) );
            return true;
        } else if( here.ter( you.pos() ) == t_grass_white ) {
            add_msg( _( "This grass is tainted with paint and thus inedible." ) );
            return true;
        }
    }
    return false;
}

void avatar_action::eat( avatar &you )
{
    item *loc = game_menus::inv::consume( you );
    avatar_action::eat( you, loc );
}

void avatar_action::eat( avatar &you, item *loc )
{
    if( !loc ) {
        you.cancel_activity();
        add_msg( _( "Never mind." ) );
        return;
    }
    if( loc->where() == item_location_type::character ) {
        you.consume( *loc );

    } else {

        loc->attempt_detach( [&you]( detached_ptr<item> &&it ) {
            return you.consume_item( std::move( it ) );
        } );
        if( !loc->is_food_container() && !you.can_consume_as_is( *loc ) ) {
            add_msg( _( "You leave the empty %s." ), loc->tname() );
        }
    }
    if( g->u.get_value( "THIEF_MODE_KEEP" ) != "YES" ) {
        g->u.set_value( "THIEF_MODE", "THIEF_ASK" );
    }
}

void avatar_action::plthrow( avatar &you, item *loc,
                             const std::optional<tripoint> &blind_throw_from_pos )
{
    if( you.has_active_mutation( trait_SHELL2 ) ) {
        add_msg( m_info, _( "You can't effectively throw while you're in your shell." ) );
        return;
    }
    if( you.is_mounted() ) {
        monster *mons = get_player_character().mounted_creature.get();
        if( mons->has_flag( MF_RIDEABLE_MECH ) ) {
            if( !mons->check_mech_powered() ) {
                add_msg( m_bad, _( "Your %s refuses to move as its batteries have been drained." ),
                         mons->get_name() );
                return;
            }
        }
    }

    if( !loc ) {
        loc = game_menus::inv::titled_menu( you,  _( "Throw item" ),
                                            _( "You don't have any items to throw." ) );
    }

    if( !loc ) {
        add_msg( _( "Never mind." ) );
        return;
    }

    int range = you.throw_range( *loc );
    if( range < 0 ) {
        add_msg( m_info, _( "You don't have that item." ) );
        return;
    } else if( range == 0 ) {
        add_msg( m_info, _( "That is too heavy to throw." ) );
        return;
    }

    if( you.is_wielding( *loc ) && loc->has_flag( flag_NO_UNWIELD ) ) {
        // pos == -1 is the weapon, NO_UNWIELD is used for bio_claws_weapon
        add_msg( m_info, _( "That's part of your body, you can't throw that!" ) );
        return;
    }

    if( you.has_effect( effect_relax_gas ) ) {
        if( one_in( 5 ) ) {
            add_msg( m_good, _( "You concentrate mightily, and your body obeys!" ) );
        } else {
            you.moves -= rng( 2, 5 ) * 10;
            add_msg( m_bad, _( "You can't muster up the effort to throw anything…" ) );
            return;
        }
    }
    // if you're wearing the item you need to be able to take it off
    if( you.is_wearing( loc->typeId() ) ) {
        ret_val<bool> ret = you.can_takeoff( *loc );
        if( !ret.success() ) {
            add_msg( m_info, "%s", ret.c_str() );
            return;
        }
    }
    // you must wield the item to throw it
    // But only if you don't have enough free hands
    const auto &wielded = you.wielded_items();
    int required_arms = std::accumulate( wielded.begin(), wielded.end(), 0,
    [&you]( int acc, const item * it ) {
        return acc + ( it->is_two_handed( you ) ? 2 : 1 );
    } );
    if( !you.is_wielding( *loc ) &&
        ( you.get_working_arm_count() < required_arms ) ) {
        if( !you.wield( *loc ) ) {
            add_msg( m_info, _( "You do not have enough free hands to throw %s without wielding it." ),
                     loc->tname() );
            return;
        }
    }

    you.assign_activity( std::make_unique<player_activity>( std::make_unique<throw_activity_actor>
                         ( *loc, blind_throw_from_pos ) ), false );
}

void avatar_action::use_item( avatar &you, item *loc )
{

    if( !loc ) {
        loc = game_menus::inv::use( you );

        if( !loc ) {
            add_msg( _( "Never mind." ) );
            return;
        }

        if( !loc->has_flag( flag_ALLOWS_REMOTE_USE ) ) {
            const int obtain_cost = loc->obtain_cost( you );
            if( !loc->has_flag( flag_TEMPORARY_ITEM ) ) {
                loc->obtain( you );
            }

            // TODO: the following comment is inaccurate and this mechanic needs to be rexamined
            // This method only handles items in the inventory, so refund the obtain cost.
            you.mod_moves( obtain_cost );
        }
    }

    avatar_funcs::use_item( you, *loc );

    you.invalidate_crafting_inventory();
}

void avatar_action::wield()
{
    item *loc = game_menus::inv::wield( get_avatar() );

    if( loc ) {
        wield( *loc );
    } else {
        add_msg( _( "Never mind." ) );
    }
}

void avatar_action::wield( item &loc )
{
    avatar &u = get_avatar();
    map &here = get_map();
    if( u.is_armed() ) {
        const bool is_unwielding = u.is_wielding( loc );
        const auto ret = u.can_unwield( loc );

        if( !ret.success() ) {
            add_msg( m_info, "%s", ret.c_str() );
        }

        u.unwield();

        if( is_unwielding ) {
            if( !u.martial_arts_data->selected_is_none() ) {
                u.martial_arts_data->martialart_use_message( u );
            }
            return;
        }
    }

    const auto ret = u.can_wield( loc );
    if( !ret.success() ) {
        add_msg( m_info, "%s", ret.c_str() );
    }

    // Need to do this here because holster_actor::use() checks if/where the item is worn
    item &target = loc;
    if( target.get_use( "holster" ) && !target.contents.empty() ) {
        //~ %1$s: weapon name, %2$s: holster name
        if( query_yn( pgettext( "holster", "Draw %1$s from %2$s?" ), target.get_contained().tname(),
                      target.tname() ) ) {
            u.invoke_item( &target );
            return;
        }
    }

    // Can't use loc.obtain() here because that would cause things to spill.
    item *to_wield = &loc;
    item_location_type location_type = loc.where();
    tripoint pos = loc.position();
    int worn_index = INT_MIN;
    if( u.is_worn( loc ) ) {
        auto ret = u.can_takeoff( loc );
        if( !ret.success() ) {
            add_msg( m_info, "%s", ret.c_str() );
            return;
        }
        int item_pos = u.get_item_position( &loc );
        if( item_pos != INT_MIN ) {
            worn_index = Character::worn_position_to_index( item_pos );
        }
    }
    if( !u.wield( *to_wield ) ) {
        switch( location_type ) {
            case item_location_type::container:
                // this will not cause things to spill, as it is inside another item
                loc.obtain( u );
                wield( loc );
                break;
            case item_location_type::character:
                if( worn_index != INT_MIN ) {
                    auto it = u.worn.begin();
                    std::advance( it, worn_index );
                    u.worn.insert( it, to_wield->detach() );
                } else {
                    u.i_add( to_wield->detach() );
                }
                break;
            case item_location_type::map:
                here.add_item( pos, to_wield->detach() );
                break;
            case item_location_type::vehicle: {
                const std::optional<vpart_reference> vp = here.veh_at( pos ).part_with_feature( "CARGO", false );
                detached_ptr<item> detached = to_wield->detach();
                // If we fail to return the item to the vehicle for some reason, add it to the map instead.
                if( vp ) {
                    vp->vehicle().add_item( vp->part_index(), std::move( detached ) );

                }

                // NOLINTNEXTLINE(bugprone-use-after-move)
                if( detached ) {
                    here.add_item( pos, std::move( detached ) );
                }
                break;
            }
            case item_location_type::monster: {
                debugmsg( "Failed wield from monster item location" );
                break;
            }
            case item_location_type::invalid:
                debugmsg( "Failed wield from invalid item location" );
                break;
        }
        return;
    }
}

static item_reload_option favorite_ammo_or_select(
    const player &u, item &it, bool empty, bool prompt )
{
    if( u.ammo_location ) {
        std::vector<item_reload_option> ammo_list;
        if( character_funcs::list_ammo( u, it, ammo_list, empty, false ) ) {
            const auto is_favorite_and_compatible = [&it, &u]( const item_reload_option & opt ) {
                return opt.ammo == &*u.ammo_location && it.can_reload_with( opt.ammo->typeId() );
            };
            auto iter = std::find_if( ammo_list.begin(), ammo_list.end(), is_favorite_and_compatible );
            if( iter != ammo_list.end() ) {
                return *iter;
            }
        }
    }
    return character_funcs::select_ammo( u, it, prompt, empty );
}

static bool can_reload_item_or_mods( const avatar &you, const item &itm )
{
    for( const item *mod : itm.gunmods() ) {
        if( can_reload_item_or_mods( you, *mod ) ) {
            return true;
        }
    }
    return itm.is_reloadable() && you.can_reload( itm );
}


void avatar_action::reload( item &loc, bool prompt, bool empty )
{
    avatar &u = get_avatar();
    item *it = &loc;

    // bows etc. do not need to reload. select favorite ammo for them instead
    if( it->has_flag( flag_RELOAD_AND_SHOOT ) ) {
        ranged::prompt_select_default_ammo_for( u, *it );
        return;
    }

    if( !can_reload_item_or_mods( u, *it ) ) {
        if( ( it->is_ammo_container() || it->is_magazine() ) && it->ammo_remaining() > 0 &&
            it->ammo_remaining() == it->ammo_capacity() ) {
            add_msg( m_info, _( "The %s is already fully loaded!" ), it->tname() );
            return;
        }
        if( it->is_ammo_belt() ) {
            const auto &linkage = it->type->magazine->linkage;
            if( linkage && !u.has_charges( *linkage, 1 ) ) {
                add_msg( m_info, _( "You need at least one %s to reload the %s!" ),
                         item::nname( *linkage, 1 ), it->tname() );
                return;
            }
        }
        if( it->is_watertight_container() && it->is_container_full() ) {
            add_msg( m_info, _( "The %s is already full!" ), it->tname() );
            return;
        }
        add_msg( m_info, _( "You can't reload a %s!" ), it->tname() );
        return;
    }

    bool use_loc = true;
    if( !it->has_flag( flag_ALLOWS_REMOTE_USE ) ) {
        loc.obtain( u );

        use_loc = false;
    }

    if( it->is_holster() ) {
        auto ptr = dynamic_cast<const holster_actor *>
                   ( it->type->get_use( "holster" )->get_actor_ptr() );
        if( static_cast<int>( it->contents.num_item_stacks() ) < ptr->multi ) {
            item *loc = game_menus::inv::holster( u, *it );

            if( !loc ) {
                u.add_msg_if_player( _( "Never mind." ) );
                return;
            }
            ptr->store( u, *it, loc->detach() );
            return;
        }
    }

    // for bandoliers we currently defer to iuse_actor methods
    if( it->is_bandolier() ) {
        auto ptr = dynamic_cast<const bandolier_actor *>
                   ( it->type->get_use( "bandolier" )->get_actor_ptr() );
        ptr->reload( u, *it );
        return;
    }

    item_reload_option opt = favorite_ammo_or_select( u, *it, empty, prompt );

    if( opt.ammo == nullptr ) {
        return;
    }

    if( opt ) {
        int moves = opt.moves();
        if( it->get_var( "dirt", 0 ) > 7800 ) {
            add_msg( m_warning, _( "You struggle to reload the fouled %s." ), it->tname() );
            moves += 2500;
        }

        u.assign_activity( activity_id( "ACT_RELOAD" ), moves, opt.qty() );
        if( use_loc ) {
            u.activity->targets.emplace_back( loc );
        } else {
            u.activity->targets.emplace_back( opt.target );
        }
        u.activity->targets.emplace_back( opt.ammo );
    }
}

void avatar_action::reload_item()
{
    item *item_loc = g->inv_map_splice( []( const item & it ) {
        return can_reload_item_or_mods( get_avatar(), it );
    }, _( "Reload item" ), 1, _( "You have nothing to reload." ) );

    if( !item_loc ) {
        add_msg( _( "Never mind." ) );
        return;
    }

    reload( *item_loc );
}

void avatar_action::reload_wielded( bool prompt )
{
    avatar &u = get_avatar();
    for( item *it : u.wielded_items() ) {
        if( it->is_reloadable() ) {
            reload( *it, prompt );
            return;
        }
    }

    add_msg( _( "You aren't holding something you can reload." ) );
}

void avatar_action::reload_weapon( bool try_everything )
{
    // As a special streamlined activity, hitting reload repeatedly should:
    // Reload wielded gun
    // First reload a magazine if necessary.
    // Then load said magazine into gun.
    // Reload magazines that are compatible with the current gun.
    // Reload other guns in inventory.
    // Reload misc magazines in inventory.
    avatar &u = get_avatar();
    map &here = get_map();
    std::set<itype_id> compatible_magazines;
    for( const item *gun : u.wielded_items() ) {
        const std::set<itype_id> &mags = gun->magazine_compatible();
        compatible_magazines.insert( mags.begin(), mags.end() );
    }
    std::vector<item *> reloadables = character_funcs::find_reloadables( u );
    std::sort( reloadables.begin(), reloadables.end(),
    [&u, &compatible_magazines]( const item * ap, const item * bp ) {
        // Current wielded weapon comes first.
        if( u.is_wielding( *bp ) ) {
            return false;
        }
        if( u.is_wielding( *ap ) ) {
            return true;
        }
        // Second sort by affiliation with wielded gun
        const bool mag_ap = compatible_magazines.contains( ap->typeId() );
        const bool mag_bp = compatible_magazines.contains( bp->typeId() );
        if( mag_ap != mag_bp ) {
            return mag_ap;
        }
        // Third sort by gun vs magazine,
        if( ap->is_gun() != bp->is_gun() ) {
            return ap->is_gun();
        }
        // Finally sort by speed to reload.
        return ( ap->get_reload_time() * ( ap->ammo_capacity() - ap->ammo_remaining() ) ) <
               ( bp->get_reload_time() * ( bp->ammo_capacity() - bp->ammo_remaining() ) );
    } );
    for( item *&candidate : reloadables ) {
        std::vector<item_reload_option> ammo_list;
        character_funcs::list_ammo( u, *candidate, ammo_list, false, false );
        if( !ammo_list.empty() ) {
            reload( *candidate, false, false );
            return;
        }
    }
    // Just for testing, bail out here to avoid unwanted side effects.
    if( !try_everything ) {
        return;
    }
    // If we make it here and haven't found anything to reload, start looking elsewhere.
    vehicle *veh = veh_pointer_or_null( here.veh_at( u.pos() ) );
    turret_data turret;
    if( veh && ( turret = veh->turret_query( u.pos() ) ) && turret.can_reload() ) {
        item_reload_option opt = character_funcs::select_ammo( u, turret.base(), true );
        if( opt ) {
            u.assign_activity( std::make_unique<player_activity>( activity_id( "ACT_RELOAD" ), opt.moves(),
                               opt.qty() ) );
            u.activity->targets.emplace_back( turret.base() );
            u.activity->targets.emplace_back( opt.ammo );
        }
        return;
    }

    reload_item();
}

void avatar_action::unload( avatar &you )
{
    item *loc = g->inv_map_splice( []( const item & it ) {
        return item_funcs::can_be_unloaded( it );
    }, _( "Unload item" ), 1, _( "You have nothing to unload." ) );

    if( !loc ) {
        add_msg( _( "Never mind." ) );
        return;
    }
    avatar_funcs::unload_item( you, *loc );
}
