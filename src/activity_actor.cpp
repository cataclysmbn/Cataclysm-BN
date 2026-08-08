#include "activity_actor.h"
#include "activity_actor_definitions.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "action_time_scale.h"
#include "activity_handlers.h" // put_into_vehicle_or_drop and drop_on_map
#include "distribution_grid.h"
#include "game_inventory.h"
#include "ui.h"
#include "character_martial_arts.h"
#include "martialarts.h"
#include "skill.h"
#include "veh_interact.h"
#include "activity_speed.h"
#include "advanced_inv.h"
#include "avatar.h"
#include "avatar_action.h"
#include "bionics.h"
#include "calendar.h"
#include "character.h"
#include "character_functions.h"
#include "clzones.h"
#include "construction.h"
#include "construction_partial.h"
#include "craft_command.h"
#include "crafting.h"
#include "debug.h"
#include "enchantments/enchanter.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "gates.h"
#include "iexamine.h"
#include "int_id.h"
#include "item.h"
#include "item_group.h"
#include "item_hauling.h"
#include "json.h"
#include "line.h"
#include "locations.h"
#include "map.h"
#include "mapbuffer.h"
#include "map_iterator.h"
#include "map_selector.h"
#include "mapdata.h"
#include "messages.h"
#include "material.h"
#include "morale_types.h"
#include "mtype.h"
#include "monster.h"
#include "npc.h"
#include "options.h"
#include "pickup.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "ranged.h"
#include "crafting_quality.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "rng.h"
#include "sounds.h"
#include "string_utils.h"
#include "timed_event.h"
#include "translations.h"
#include "uistate.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "handle_liquid.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "output.h"
#include "omdata.h"
#include "overmapbuffer.h"
#include "text_snippets.h"

#define dbg(x) DebugLog((x),DC::Game)

static const construction_str_id deconstruct_simple( "constr_deconstruct_simple" );
static const construction_str_id deconstruct( "constr_deconstruct" );
static const construction_group_str_id
advanced_object_deconstruction( "advanced_object_deconstruction" );

static const itype_id itype_bone_human( "bone_human" );
static const itype_id itype_electrohack( "electrohack" );
static const itype_id itype_wool_staple( "wool_staple" );

static const efftype_id effect_sheared( "sheared" );
static const efftype_id effect_tied( "tied" );

static const skill_id skill_computer( "computer" );
static const skill_id skill_mechanics( "mechanics" );

static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_fat( "mon_zombie_fat" );
static const mtype_id mon_zombie_rot( "mon_zombie_rot" );
static const mtype_id mon_skeleton( "mon_skeleton" );
static const mtype_id mon_zombie_crawler( "mon_zombie_crawler" );

static const quality_id qual_LOCKPICK( "LOCKPICK" );

static const quality_id qual_BUTCHER( "BUTCHER" );
static const quality_id qual_CUT_FINE( "CUT_FINE" );

static const skill_id skill_survival( "survival" );
static const skill_id skill_firstaid( "firstaid" );
static const skill_id skill_electronics( "electronics" );

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
static const trait_id trait_SPIRITUAL( "SPIRITUAL" );

static const efftype_id effect_ai_waiting( "ai_waiting" );

namespace
{

auto restore_legacy_progress( activity_actor &actor, const JsonObject &data,
                              std::string_view task_name ) -> void
{
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 && actor.progress.empty() ) {
        actor.progress.emplace( std::string( task_name ), moves_total,
                                std::max( 0, data.get_int( "moves_left", moves_total ) ) );
    }
}

} // namespace
static const efftype_id effect_well_fed( "well_fed" );

static const itype_id itype_animal( "animal" );
static const itype_id itype_muscle( "muscle" );
static const itype_id itype_UPS( "UPS" );

static const zone_type_id zone_type_LOOT_IGNORE( "LOOT_IGNORE" );
static const zone_type_id zone_type_LOOT_IGNORE_FAVORITES( "LOOT_IGNORE_FAVORITES" );
static const zone_type_id zone_type_LOOT_UNSORTED( "LOOT_UNSORTED" );

static const activity_id ACT_BLEED( "ACT_BLEED" );
static const activity_id ACT_BUTCHER( "ACT_BUTCHER" );
static const activity_id ACT_BUTCHER_FULL( "ACT_BUTCHER_FULL" );
static const activity_id ACT_DISMEMBER( "ACT_DISMEMBER" );
static const activity_id ACT_DISSECT( "ACT_DISSECT" );
static const activity_id ACT_FIELD_DRESS( "ACT_FIELD_DRESS" );
static const activity_id ACT_MOVE_LOOT( "ACT_MOVE_LOOT" );
static const activity_id ACT_QUARTER( "ACT_QUARTER" );
static const activity_id ACT_SKIN( "ACT_SKIN" );

int simple_task::to_counter() const
{
    double ret = 10'000'000.0 / moves_total * ( moves_total - moves_left );
    return std::round( ret );
}

inline void progress_counter::pop()
{
    if( empty() ) {
        dbg( DL::Error ) << "task was popped out of empty progress queue";
        return;
    }
    moves_left -= targets.front().moves_left;
    targets.pop_front();
    idx++;
}

inline void progress_counter::purge()
{
    if( empty() ) {
        dbg( DL::Error ) << "task was purged out of empty progress queue";
        return;
    }
    moves_left -= targets.front().moves_left;
    moves_total -= targets.front().moves_total;
    total_tasks--;
    targets.pop_front();
}

inline void activity_actor::calc_all_moves( player_activity &act, Character &who )
{
    act.speed.calc_all_moves( who );
}

aim_activity_actor::aim_activity_actor() : fake_weapon( new fake_item_location() )
{
    initial_view_offset = get_avatar().view_offset;
}

std::unique_ptr<aim_activity_actor> aim_activity_actor::use_wielded()
{
    return std::make_unique<aim_activity_actor>();
}

std::unique_ptr<aim_activity_actor> aim_activity_actor::use_bionic( detached_ptr<item> &&fake_gun,
        const units::energy &cost_per_shot )
{
    std::unique_ptr<aim_activity_actor> act( new aim_activity_actor() );
    act->bp_cost_per_shot = cost_per_shot;
    act->fake_weapon = std::move( fake_gun );
    return act;
}

std::unique_ptr<aim_activity_actor> aim_activity_actor::use_gear( item *gun )
{
    std::unique_ptr<aim_activity_actor> act( new aim_activity_actor() );
    act->weapon = safe_reference<item>( gun );
    return act;
}

std::unique_ptr<aim_activity_actor> aim_activity_actor::use_mutation( detached_ptr<item>
        &&fake_gun )
{
    std::unique_ptr<aim_activity_actor> act( new aim_activity_actor() );
    act->fake_weapon = std::move( fake_gun );
    return act;
}

void aim_activity_actor::start( player_activity &/*act*/, Character &/*who*/ )
{
    // Time spent on aiming is determined on the go by the player
    // Dummy progress task to indicate ongoing activity
    progress.dummy();
}

void aim_activity_actor::do_turn( player_activity &act, Character &who )
{
    if( !who.is_avatar() ) {
        debugmsg( "ACT_AIM not implemented for NPCs" );
        aborted = true;
        progress.pop();
        return;
    }
    avatar &you = get_avatar();

    item *weapon = get_weapon();
    if( !weapon || !avatar_action::can_fire_weapon( you, *weapon ) ) {
        aborted = true;
        progress.pop();
        return;
    }

    gun_mode gun = weapon->gun_current_mode();
    if( !gun->ammo_remaining() && !reload_loc && gun->has_flag( flag_RELOAD_AND_SHOOT ) ) {
        if( !load_RAS_weapon() ) {
            aborted = true;
            progress.pop();
            return;
        }
    }
    g->temp_exit_fullscreen();
    target_handler::trajectory trajectory;
    if( const auto shape_gen = ranged::get_shape_factory( *weapon ) ) {
        trajectory = target_handler::mode_shaped( you, *shape_gen, *this );
    } else {
        trajectory = target_handler::mode_fire( you, *this );
    }
    g->reenter_fullscreen();

    if( aborted ) {
        progress.pop();
    } else {
        if( !trajectory.empty() ) {
            fin_trajectory = trajectory;
            progress.pop();
        }

        // Allow interrupting activity only during 'aim and fire'.
        // Prevents '.' key for 'aim for 10 turns' from conflicting with '.' key for 'interrupt activity'
        // in case of high input lag (curses, sdl sometimes...), but allows to interrupt aiming
        // if a bug happens / stars align to cause an endless aiming loop.
        act.interruptable_with_kb = action != "AIM";
    }
}

void aim_activity_actor::finish( player_activity &act, Character &who )
{
    act.set_to_null();
    item *weapon = get_weapon();
    if( !weapon ) {
        restore_view();
        return;
    }
    if( aborted ) {
        if( reload_requested ) {
            // Reload the gun / select different arrows
            // May assign ACT_RELOAD
            avatar_action::reload_wielded( true );
        }
        restore_view();
        return;
    }

    // Fire!
    gun_mode gun = weapon->gun_current_mode();

    item *ammo_loc = reload_loc ? &*reload_loc : nullptr;

    int shots_fired = ranged::fire_gun( who, fin_trajectory.back(), gun.qty, *gun, ammo_loc );

    if( shots_fired > 0 ) {
        // TODO: bionic power cost of firing should be derived from a value of the relevant weapon.
        if( bp_cost_per_shot > 0_J ) {
            who.mod_power_level( -bp_cost_per_shot * shots_fired );
        }
        if( stamina_cost_per_shot > 0 ) {
            who.mod_stamina( -stamina_cost_per_shot * shots_fired );
        }
    }

    if( !get_option<bool>( "AIM_AFTER_FIRING" ) ||
        who.recoil <= ranged::calculate_aim_cap( who, fin_trajectory.back() ) ) {
        restore_view();
        return;
    }

    // re-enter aiming UI with same parameters
    std::unique_ptr<aim_activity_actor> aim_actor = std::make_unique<aim_activity_actor>();
    aim_actor->abort_if_no_targets = true;
    aim_actor->fake_weapon = std::move( this->fake_weapon );
    aim_actor->bp_cost_per_shot = this->bp_cost_per_shot;
    aim_actor->initial_view_offset = this->initial_view_offset;

    // if invalid target or it's dead - reset it so a new one is acquired
    shared_ptr_fast<Creature> last_target = who.last_target.lock();
    if( last_target && last_target->is_dead_state() ) {
        who.last_target.reset();
    }
    who.assign_activity( std::make_unique<player_activity>( std::move( aim_actor ) ), false );
}

void aim_activity_actor::canceled( player_activity &/*act*/, Character &/*who*/ )
{
    restore_view();
}

void aim_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "fake_weapon", fake_weapon ? *fake_weapon : null_item_reference() );
    jsout.member( "bp_cost_per_shot", bp_cost_per_shot );
    jsout.member( "stamina_cost_per_shot", stamina_cost_per_shot );
    jsout.member( "action", action );
    jsout.member( "aif_duration", aif_duration );
    jsout.member( "aiming_at_critter", aiming_at_critter );
    jsout.member( "snap_to_target", snap_to_target );
    jsout.member( "shifting_view", shifting_view );
    jsout.member( "initial_view_offset", initial_view_offset );
    jsout.member( "loaded_RAS_weapon", loaded_RAS_weapon );
    jsout.member( "reload_loc", reload_loc );
    jsout.member( "aborted", aborted );
    jsout.member( "reload_requested", reload_requested );
    jsout.member( "abort_if_no_targets", abort_if_no_targets );

    jsout.end_object();
}

std::unique_ptr<activity_actor> aim_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<aim_activity_actor> actor( new aim_activity_actor() );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "fake_weapon", actor->fake_weapon );
    data.read( "bp_cost_per_shot", actor->bp_cost_per_shot );
    data.read( "stamina_cost_per_shot", actor->stamina_cost_per_shot );
    data.read( "action", actor->action );
    data.read( "aif_duration", actor->aif_duration );
    data.read( "aiming_at_critter", actor->aiming_at_critter );
    data.read( "snap_to_target", actor->snap_to_target );
    data.read( "shifting_view", actor->shifting_view );
    data.read( "initial_view_offset", actor->initial_view_offset );
    data.read( "loaded_RAS_weapon", actor->loaded_RAS_weapon );
    data.read( "reload_loc", actor->reload_loc );
    data.read( "aborted", actor->aborted );
    data.read( "reload_requested", actor->reload_requested );
    data.read( "abort_if_no_targets", actor->abort_if_no_targets );

    return actor;
}

item *aim_activity_actor::get_weapon()
{
    if( weapon ) {
        return &*weapon;
    }
    if( fake_weapon ) {
        // TODO: check if the player lost relevant bionic/mutation
        return &*fake_weapon;
    } else {
        // Check for lost gun (e.g. yanked by zombie technician)
        // TODO: check that this is the same gun that was used to start aiming
        item *weapon = &get_player_character().primary_weapon();
        return weapon->is_null() ? nullptr : weapon;
    }
}

void aim_activity_actor::restore_view()
{
    avatar &player_character = get_avatar();
    bool changed_z = player_character.view_offset.z() != initial_view_offset.z();
    player_character.view_offset = initial_view_offset;
    if( changed_z ) {
        get_map().invalidate_map_cache( player_character.view_offset.z() );
        g->invalidate_main_ui_adaptor();
    }
}

bool aim_activity_actor::load_RAS_weapon()
{
    // TODO: use activity for fetching ammo and loading weapon
    player &you = get_avatar();
    item *weapon = get_weapon();
    gun_mode gun = weapon->gun_current_mode();

    // Will burn (0.2% max base stamina * the strength required to fire)
    stamina_cost_per_shot = gun->get_min_str() * static_cast<int>
                            ( 0.002f * get_option<int>( "PLAYER_MAX_STAMINA" ) );
    if( you.get_stamina() < stamina_cost_per_shot ) {
        you.add_msg_if_player( m_bad, _( "You're too tired to draw your %s." ), weapon->tname() );
        return false;
    }

    const auto ammo_location_is_valid = [&]() -> bool {
        if( !you.ammo_location )
        {
            return false;
        }
        if( !gun->can_reload_with( you.ammo_location->typeId() ) )
        {
            return false;
        }
        if( square_dist( you.abs_pos(), you.ammo_location->abs_pos() ) > 1 )
        {
            return false;
        }
        return true;
    };
    item_reload_option opt = ammo_location_is_valid() ? item_reload_option( &you, weapon,
                             weapon, *you.ammo_location ) : character_funcs::select_ammo( you, *gun );
    if( !opt ) {
        // Menu canceled
        return false;
    }

    reload_loc = opt.ammo;
    loaded_RAS_weapon = true;
    return true;
}

void autodrive_activity_actor::start( player_activity &/* act */, Character &who )
{
    const bool in_vehicle = who.in_vehicle && who.controlling_vehicle;
    const optional_vpart_position vp = get_map().veh_at( who.bub_pos() );
    if( !( vp && in_vehicle ) ) {
        who.cancel_activity();
        return;
    }

    player_vehicle = &vp->vehicle();
    if( player_vehicle->is_flying_in_air() ) {
        int min_speed = player_vehicle->get_takeoff_speed( "t/t" );
        if( player_vehicle->velocity * 0.8 < min_speed * vehicles::cmps_per_tile ) {
            if( !g->u.query_yn( "Warning: Current Speed is below recommened values, proceed?" ) ) {
                who.cancel_activity();
                return;
            }
        }
        if( player_vehicle->min_autodrive_speed * 0.8 < min_speed ) {
            if( !g->u.query_yn( "Warning: Min Autodrive Speed is below recommened values, proceed?" ) ) {
                who.cancel_activity();
                return;
            }
        }
        if( player_vehicle->max_autodrive_speed * 0.5 < min_speed ) {
            if( !g->u.query_yn( "Warning: Max Autodrive Speed is below recommened values, proceed?" ) ) {
                who.cancel_activity();
                return;
            }
        }
    }
    player_vehicle->is_autodriving = true;
    progress.dummy();
}

void autodrive_activity_actor::do_turn( player_activity &/* act */, Character &who )
{
    if( who.in_vehicle && who.controlling_vehicle && player_vehicle ) {
        if( who.moves <= 0 ) {
            // out of moves? the driver's not doing anything this turn
            // (but the vehicle will continue moving)
            return;
        }
        switch( player_vehicle->do_autodrive( who ) ) {
            case autodrive_result::ok:
                if( who.moves > 0 ) {
                    // if do_autodrive() didn't eat up all our moves, end the turn
                    // equivalent to player pressing the "pause" button
                    who.moves = 0;
                }
                sounds::reset_markers();
                break;
            case autodrive_result::abort:
                who.cancel_activity();
                break;
            case autodrive_result::finished:
                progress.pop();
                break;
        }
    } else {
        who.cancel_activity();
    }
}

void autodrive_activity_actor::canceled( player_activity &act, Character &who )
{
    who.add_msg_if_player( m_info, _( "Auto-drive canceled." ) );
    who.omt_path.clear();
    if( player_vehicle ) {
        player_vehicle->stop_autodriving( false );
    }
    act.set_to_null();
}

void autodrive_activity_actor::finish( player_activity &act, Character &who )
{
    who.add_msg_if_player( m_info, _( "You have reached your destination." ) );
    player_vehicle->stop_autodriving( false );
    act.set_to_null();
}

void autodrive_activity_actor::serialize( JsonOut &jsout ) const
{
    // Activity is not being saved but still provide some valid json if called.
    jsout.write_null();
}

std::unique_ptr<activity_actor> autodrive_activity_actor::deserialize( JsonIn & )
{
    return std::make_unique<autodrive_activity_actor>();
}

void dig_activity_actor::start( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, location );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    ter_id ter_here = handle->ter();
    const bool grave = ter_here == t_grave;
    const std::string name = grave
                             ? "grave"
                             : ter_here->name();
    progress.emplace( name, moves_total );
}

void dig_activity_actor::do_turn( player_activity &/*act*/, Character &who )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
    sfx::play_activity_sound( "tool", "shovel", sfx::get_heard_volume( abs_to_bub( location ), 60 ) );
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        //~ Sound of a shovel digging a pit at work!
        sound_event se;
        se.origin = location;
        se.volume = 60;
        se.category = sounds::sound_t::activity;
        se.description = _( "hsh!" );
        se.id =  "tool";
        se.variant = "shovel";
        se.from_player = who.is_player();
        se.from_npc = !se.from_player;
        se.faction = who.get_faction()->id;
        se.monfaction = who.get_faction()->mon_faction;
        sounds::sound( se );
    }
}

void dig_activity_actor::finish( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, location );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    const bool grave = handle->ter() == t_grave;

    if( grave ) {
        if( one_in( 10 ) ) {
            static const std::array<mtype_id, 5> monids = { {
                    mon_zombie, mon_zombie_fat,
                    mon_zombie_rot, mon_skeleton,
                    mon_zombie_crawler
                }
            };

            here.place_critter_at( random_entry( monids ), byproducts_location );
            here.set_furn( location, f_coffin_o );
            who.add_msg_if_player( m_warning, _( "Something crawls out of the coffin!" ) );
        } else {
            here.spawn_item( location, itype_bone_human, rng( 5, 15 ) );
            here.set_furn( location, f_coffin_c );
        }
        std::vector<item *> dropped = here.place_items( item_group_id( "allclothes" ), 50, location,
                                      location, false,
                                      calendar::turn );
        here.place_items( item_group_id( "grave" ), 25, location, location, false, calendar::turn );
        here.place_items( item_group_id( "jewelry_front" ), 20, location, location, false,
                          calendar::turn );
        for( item * const &it : dropped ) {
            if( it->is_armor() ) {
                it->set_damage( rng( 1, it->max_damage() - 1 ) );
            }
        }
        g->events().send<event_type::exhumes_grave>( who.getID() );
    }

    here.set_ter( location, ter_id( result_terrain ) );

    here.spawn_items( byproducts_location,
                      item_group::items_from( item_group_id( byproducts_item_group ),
                              calendar::turn ) );

    const int act_exertion = moves_total;

    who.mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 80_seconds ) ) );
    who.mod_thirst( std::max( 1, act_exertion / to_moves<int>( 12_minutes ) ) );
    who.mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
    who.mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 10_seconds ) ) );
    if( grave ) {
        who.add_msg_if_player( m_good, _( "You finish exhuming a grave." ) );
    } else {
        who.add_msg_if_player( m_good, _( "You finish digging the %s." ),
                               handle->tername() );
    }

    act.set_to_null();
}

void dig_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "moves", moves_total );
    jsout.member( "location", location );
    jsout.member( "result_terrain", result_terrain );
    jsout.member( "byproducts_location", byproducts_location );
    jsout.member( "byproducts_item_group", byproducts_item_group );

    jsout.end_object();
}

std::unique_ptr<activity_actor> dig_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<dig_activity_actor> actor( new dig_activity_actor( 0, tripoint_abs_ms::zero(),
            {}, tripoint_abs_ms::zero(), {} ) );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "moves", actor->moves_total );
    data.read( "location", actor->location );
    data.read( "result_terrain", actor->result_terrain );
    data.read( "byproducts_location", actor->byproducts_location );
    data.read( "byproducts_item_group", actor->byproducts_item_group );

    return actor;
}

void dig_channel_activity_actor::start( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, location );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    progress.emplace( handle->tername(), moves_total );
}

void dig_channel_activity_actor::do_turn( player_activity &/*act*/, Character &who )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
    sfx::play_activity_sound( "tool", "shovel", sfx::get_heard_volume( abs_to_bub( location ), 70 ) );
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        //~ Sound of a shovel digging a pit at work!
        sound_event se;
        se.origin = location;
        se.volume = 70;
        se.category = sounds::sound_t::activity;
        se.description = _( "hsh!" );
        se.id =  "tool";
        se.variant =  "shovel";
        se.from_player = who.is_player();
        se.from_npc = !se.from_player;
        se.faction = who.get_faction()->id;
        se.monfaction = who.get_faction()->mon_faction;
        sounds::sound( se );
    }
}

void dig_channel_activity_actor::finish( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, location );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    here.set_ter( location, ter_id( result_terrain ) );

    here.spawn_items( byproducts_location,
                      item_group::items_from( item_group_id( byproducts_item_group ),
                              calendar::turn ) );

    const int act_exertion = moves_total;

    who.mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 80_seconds ) ) );
    who.mod_thirst( std::max( 1, act_exertion / to_moves<int>( 12_minutes ) ) );
    who.mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
    who.mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 10_seconds ) ) );
    who.add_msg_if_player( m_good, _( "You finish digging up %s." ),
                           here.ter( location )->obj().name() );

    act.set_to_null();
}

void dig_channel_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "moves", moves_total );
    jsout.member( "location", location );
    jsout.member( "result_terrain", result_terrain );
    jsout.member( "byproducts_location", byproducts_location );
    jsout.member( "byproducts_item_group", byproducts_item_group );

    jsout.end_object();
}

std::unique_ptr<activity_actor> dig_channel_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<dig_channel_activity_actor> actor( new dig_channel_activity_actor( 0,
            tripoint_abs_ms::zero(),
            {}, tripoint_abs_ms::zero(), {} ) );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "moves", actor->moves_total );
    data.read( "location", actor->location );
    data.read( "result_terrain", actor->result_terrain );
    data.read( "byproducts_location", actor->byproducts_location );
    data.read( "byproducts_item_group", actor->byproducts_item_group );

    return actor;
}

bool disassemble_activity_actor::try_start_single( player_activity &/* act */, Character &who )
{
    if( targets.empty() ) {
        return false;
    }
    const iuse_location &target = targets.front();
    if( !target.loc ) {
        debugmsg( "Lost target of ACT_DISASSEMBLE" );
        targets.clear();
        return false;
    }
    const item &itm = *target.loc;

    // Have to check here again in case we ran out of tools
    const ret_val<bool> can_do = crafting::can_disassemble( who, itm, who.crafting_inventory() );
    if( !can_do.success() ) {
        who.add_msg_if_player( m_info, "%s", can_do.c_str() );
        return false;
    }
    return true;
}

inline void disassemble_activity_actor::process_target( player_activity &/*act*/,
        iuse_location &target )
{
    const item &itm = *target.loc;
    const recipe &dis = recipe_dictionary::get_uncraft( itm.typeId() );
    int moves_needed = dis.time * target.count;
    progress.emplace( itm.tname( target.count ), moves_needed );
}

inline void disassemble_activity_actor::calc_all_moves( player_activity &act, Character &who )
{
    const auto &target = targets.front().loc;
    auto reqs = activity_reqs_adapter( recipe_dictionary::get_uncraft( target->typeId() ),
                                       std::make_pair( target->weight(), target->volume() ) );
    act.speed.calc_all_moves( who, reqs );
}

void disassemble_activity_actor::start( player_activity &act, Character &who )
{
    if( !who.is_avatar() ) {
        debugmsg( "ACT_DISASSEMBLE is not implemented for NPCs" );
        act.set_to_null();
    } else if( !try_start_single( act, who ) ) {
        act.set_to_null();
    }
    for( auto &target : targets ) {
        process_target( act, target );
    }
}

void disassemble_activity_actor::do_turn( player_activity &act, Character &who )
{
    if( progress.front().complete() ) {
        const iuse_location &target = targets.front();
        if( !target.loc ) {
            debugmsg( "Lost target of ACT_DISASSEMBLY" );
        } else {
            crafting::complete_disassemble( who, target, abs_to_bub( pos ) );
        }
        targets.erase( targets.begin() );
        progress.pop();
        if( !progress.empty() ) {
            if( try_start_single( act, who ) ) {
                calc_all_moves( act, who );
            } else {
                act.set_to_null();
            }
        }
    }
}

void disassemble_activity_actor::finish( player_activity &act, Character &who )
{
    if( try_start_single( act, who ) ) {
        debugmsg( "disassemble_activity_actor call finish function while able to start new disassembly" );
    }
    // Make a copy to avoid use-after-free
    bool recurse = this->recursive;

    act.set_to_null();

    if( recurse ) {
        crafting::disassemble_all( *who.as_avatar(), recurse );
    }
}

void disassemble_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "targets", targets );
    jsout.member( "pos", pos );
    jsout.member( "recursive", recursive );

    jsout.end_object();
}

std::unique_ptr<activity_actor> disassemble_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<disassemble_activity_actor> actor( new disassemble_activity_actor() );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "targets", actor->targets );
    data.read( "pos", actor->pos );
    data.read( "recursive", actor->recursive );

    return actor;
}

drop_activity_actor::drop_activity_actor( Character &ch, const drop_locations &items,
        bool force_ground, const tripoint_rel_ms &relpos )
    : force_ground( force_ground ), relpos( relpos )
{
    this->items = pickup::reorder_for_dropping( ch, items );
}

void drop_activity_actor::start( player_activity &/* act */, Character & )
{
    // Dummy progress task to indicate ongoing activity
    progress.dummy();
}

void drop_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "items", items );
    jsout.member( "force_ground", force_ground );
    jsout.member( "relpos", relpos );

    jsout.end_object();
}

std::unique_ptr<activity_actor> drop_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<drop_activity_actor> actor( new drop_activity_actor() );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "items", actor->items );
    data.read( "force_ground", actor->force_ground );
    data.read( "relpos", actor->relpos );

    return actor;
}

enum hack_result {
    HACK_UNABLE,
    HACK_FAIL,
    HACK_NOTHING,
    HACK_SUCCESS
};

enum hack_type {
    HACK_SAFE,
    HACK_DOOR,
    HACK_GAS,
    HACK_NULL
};

static hack_type get_hack_type( tripoint_bub_ms examp )
{
    hack_type type = HACK_NULL;
    const map &here = get_map();
    const furn_t &xfurn_t = *here.furn( examp );
    const ter_t &xter_t = *here.ter( examp );
    if( xter_t.examine == &iexamine::pay_gas || xfurn_t.examine == &iexamine::pay_gas ) {
        type = HACK_GAS;
    } else if( xter_t.examine == &iexamine::cardreader || xfurn_t.examine == &iexamine::cardreader ) {
        type = HACK_DOOR;
    } else if( xter_t.examine == &iexamine::gunsafe_el || xfurn_t.examine == &iexamine::gunsafe_el ) {
        type = HACK_SAFE;
    }
    return type;
}

void hacking_activity_actor::start( player_activity &, Character & )
{
    hack_type type = get_hack_type( abs_to_bub( target_pos ) );
    std::string name;

    switch( type ) {
        case hack_type::HACK_SAFE:
            name = "safe";
            break;
        case hack_type::HACK_DOOR:
            name = "door panel";
            break;
        case hack_type::HACK_GAS:
            name = "gas pump";
            break;
        default:
            name = "";
            break;
    }

    progress.emplace( name, to_moves<int>( 5_minutes ) );
}

void hacking_activity_actor::do_turn( player_activity &/*act*/, Character & )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
}

static int hack_level( const Character &who )
{
    ///\EFFECT_COMPUTER increases success chance of hacking card readers
    // odds go up with int>8, down with int<8
    // 4 int stat is worth 1 computer skill here
    ///\EFFECT_INT increases success chance of hacking card readers
    return who.get_skill_level( skill_computer ) + ( who.int_cur - 8 ) / 4;
}

static hack_result hack_attempt( Character &who, const bool using_bionic )
{
    who.practice( skill_computer, 20 );
    // only skilled supergenius never cause short circuits, but the odds are low for people
    // with moderate skills
    const int hack_stddev = 5;
    int success = std::ceil( normal_roll( hack_level( who ), hack_stddev ) );
    if( success < 0 ) {
        who.add_msg_if_player( _( "You cause a short circuit!" ) );
        if( using_bionic ) {
            who.mod_power_level( -25_kJ );
        } else {
            who.use_charges( itype_electrohack, 25 );
        }

        if( success <= -5 ) {
            if( !using_bionic ) {
                who.add_msg_if_player( m_bad, _( "Your electrohack is ruined!" ) );
                who.use_amount( itype_electrohack, 1 );
            } else {
                who.add_msg_if_player( m_bad, _( "Your power is drained!" ) );
                who.mod_power_level( units::from_kilojoule( -rng( 25,
                                     units::to_kilojoule( who.get_power_level() ) ) ) );
            }
        }
        return HACK_FAIL;
    } else if( success < 6 ) {
        return HACK_NOTHING;
    } else {
        return HACK_SUCCESS;
    }
}

hacking_activity_actor::hacking_activity_actor( use_bionic )
    : using_bionic( true )
{
}

hacking_activity_actor::hacking_activity_actor( use_bionic, const tripoint_abs_ms &pos )
    : using_bionic( true ), target_pos( pos )
{
}

hacking_activity_actor::hacking_activity_actor( const tripoint_abs_ms &pos )
    : target_pos( pos )
{
}

void hacking_activity_actor::finish( player_activity &act, Character &who )
{
    tripoint_bub_ms examp = abs_to_bub( target_pos );
    hack_type type = get_hack_type( examp );
    map &here = get_map();
    sound_event se;
    switch( hack_attempt( who, using_bionic ) ) {
        case HACK_UNABLE:
            who.add_msg_if_player( _( "You cannot hack this." ) );
            break;
        case HACK_FAIL:
            // currently all things that can be hacked have equivalent alarm failure states.
            // this may not always be the case with new hackable things.
            g->events().send<event_type::triggers_alarm>( who.getID() );
            se.origin = who.abs_pos();
            se.volume = 120;
            se.category = sounds::sound_t::music;
            se.description = _( "an alarm sound!" );
            se.id = "environment";
            se.variant = "alarm";
            sounds::sound( se );
            if( examp.z() > 0 && !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
                g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0,
                                     who.abs_sm_pos() );
            }
            break;
        case HACK_NOTHING:
            who.add_msg_if_player( _( "You fail the hack, but no alarms are triggered." ) );
            break;
        case HACK_SUCCESS:
            if( type == HACK_GAS ) {
                int tankGasUnits;
                const std::optional<tripoint_bub_ms> pTank_ = iexamine::getNearFilledGasTank( examp, tankGasUnits );
                if( !pTank_ ) {
                    break;
                }
                const tripoint_bub_ms pTank = *pTank_;
                const std::optional<tripoint_bub_ms> pGasPump = iexamine::getGasPumpByNumber( examp,
                        uistate.ags_pay_gas_selected_pump );
                if( pGasPump && iexamine::toPumpFuel( pTank, *pGasPump, tankGasUnits ) ) {
                    who.add_msg_if_player( _( "You hack the terminal and route all available fuel to your pump!" ) );
                    se.origin = bub_to_abs( examp );
                    se.volume = 40;
                    se.category = sounds::sound_t::activity;
                    se.description = _( "Glug Glug Glug Glug Glug Glug Glug Glug Glug" );
                    se.id = "tool";
                    se.variant =  "gaspump";
                    se.from_player = who.is_player();
                    se.from_npc = !se.from_player;
                    se.faction = who.get_faction()->id;
                    se.monfaction = who.get_faction()->mon_faction;
                    sounds::sound( se );
                } else {
                    who.add_msg_if_player( _( "Nothing happens." ) );
                }
            } else if( type == HACK_SAFE ) {
                who.add_msg_if_player( m_good, _( "The door on the safe swings open." ) );
                here.furn_set( examp, furn_str_id( "f_gunsafe_o" ) );
            } else if( type == HACK_DOOR ) {
                who.add_msg_if_player( _( "You activate the panel!" ) );
                who.add_msg_if_player( m_good, _( "The nearby doors unlock." ) );
                here.ter_set( examp, t_card_reader_broken );
                for( const tripoint_bub_ms &tmp : here.points_in_radius( ( examp ), 3 ) ) {
                    if( here.ter( tmp ) == t_door_metal_locked ) {
                        here.ter_set( tmp, t_door_metal_c );
                    }
                }
            }
            break;
    }
    act.set_to_null();
}

void hacking_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "using_bionic", using_bionic );
    jsout.member( "target_pos", target_pos );

    jsout.end_object();
}

std::unique_ptr<activity_actor> hacking_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<hacking_activity_actor> actor( new hacking_activity_actor() );
    if( jsin.test_null() ) {
        // Old saves might contain a null instead of an object.
        // Since we do not know whether a bionic or an item was chosen we assume
        // it was an item.
        actor->using_bionic = false;
    } else {
        JsonObject jsobj = jsin.get_object();
        jsobj.read( "using_bionic", actor->using_bionic );
        jsobj.read( "progress", actor->progress );
        jsobj.read( "target_pos", actor->target_pos );
    }
    return actor;
}

std::unique_ptr<activity_actor> hacking_activity_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<hacking_activity_actor>();
    data.read( "placement", actor->target_pos );
    return actor;
}

void move_items_activity_actor::do_turn( player_activity &act, Character &who )
{
    if( target_items.empty() || target_items.size() != quantities.size() ) {
        act.set_to_null();
        return;
    }

    const auto dest = relative_destination + who.abs_pos();

    while( who.moves > 0 && !target_items.empty() ) {
        safe_reference<item> target = std::move( target_items.back() );
        const int quantity = quantities.back();
        target_items.pop_back();
        quantities.pop_back();

        if( !target ) {
            //TODO!: might not be appropriate to debugmsg just because something was destroyed/unloaded
            debugmsg( "Lost target item of ACT_MOVE_ITEMS" );
            continue;
        }

        // Check that we can pick it up.
        if( target->made_of( LIQUID ) ) {
            continue;
        }

        // This is for hauling across zlevels, remove when going up and down stairs
        // is no longer teleportation
        // Also ignores items owned by other NPCs, unless they'd already attack on sight
        if( target->is_owned_by( who, true ) || target->get_owner()->likes_u < -10 ) {
            target->set_owner( who );
        } else {
            continue;
        }

        const auto src = target->abs_pos();
        detached_ptr<item> newit = quantity == 0 ? target->detach() : target->split( quantity );

        const int distance = src.z() == dest.z() ? std::max( rl_dist( src, dest ), 1 ) : 1;
        who.mod_moves( -pickup::cost_to_move_item( who, *newit ) * distance );

        std::vector<detached_ptr<item>> vec;
        vec.push_back( std::move( newit ) );
        if( to_vehicle ) {
            put_into_vehicle_or_drop( who, item_drop_reason::deliberate, vec, abs_to_bub( dest ) );
        } else {
            drop_on_map( who, item_drop_reason::deliberate, vec, abs_to_bub( dest ) );
        }
    }

    if( target_items.empty() ) {
        // Nuke the current activity, leaving the backlog alone.
        act.set_to_null();
        if( who.is_hauling() && !has_haulable_items( who.bub_pos() ) ) {
            who.stop_hauling();
        }
    }
}

void move_items_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "target_items", target_items );
    jsout.member( "quantities", quantities );
    jsout.member( "to_vehicle", to_vehicle );
    jsout.member( "relative_destination", relative_destination );

    jsout.end_object();
}

std::unique_ptr<activity_actor> move_items_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<move_items_activity_actor> actor( new move_items_activity_actor( {}, {}, false,
            tripoint_rel_ms::zero() ) );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "target_items", actor->target_items );
    data.read( "quantities", actor->quantities );
    data.read( "to_vehicle", actor->to_vehicle );
    data.read( "relative_destination", actor->relative_destination );

    return actor;
}

void pickup_activity_actor::do_turn( player_activity &act, Character &who )
{
    // If we don't have target items bail out
    if( target_items.empty() ) {
        who.cancel_activity();
        return;
    }

    // If the player moves while picking up (i.e.: in a moving vehicle) cancel
    // the activity, only populate starting_pos when grabbing from the ground
    if( starting_pos && *starting_pos != who.abs_pos() ) {
        who.cancel_activity();
        who.add_msg_if_player( _( "Moving canceled auto-pickup." ) );
        return;
    }

    // Auto_resume implies autopickup.
    const bool autopickup = who.activity->auto_resume;

    // False indicates that the player canceled pickup when met with some prompt
    const bool keep_going = pickup::do_pickup( target_items, autopickup );

    // Check thievey witness
    npc *witness = nullptr;
    if( thievery_witness ) {
        for( auto &guy : who.get_mapbuffer().all_npcs() ) {
            if( guy->get_attitude() == NPCATT_RECOVER_GOODS ) {
                witness = guy.get();
                break;
            }
        }
    }

    // If there are items left we ran out of moves, so continue the activity
    // Otherwise, we are done.
    if( !keep_going || target_items.empty() || witness ) {
        who.cancel_activity();

        if( who.get_value( "THIEF_MODE_KEEP" ) != "YES" ) {
            who.set_value( "THIEF_MODE", "THIEF_ASK" );
        }

        if( !keep_going ) {
            // The user canceled the activity, so we're done
            // AIM might have more pickup activities pending, also cancel them.
            // TODO: Move this to advanced inventory instead of hacking it in here
            cancel_aim_processing();
        }

        if( witness ) {
            witness->talk_to_u();
            thievery_witness = false;
        }
    }
}

void pickup_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "target_items", target_items );
    jsout.member( "starting_pos", starting_pos );
    jsout.member( "thievery_witness", thievery_witness );

    jsout.end_object();
}

std::unique_ptr<activity_actor> pickup_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<pickup_activity_actor> actor( new pickup_activity_actor( {}, std::nullopt ) );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "target_items", actor->target_items );
    data.read( "starting_pos", actor->starting_pos );
    data.read( "thievery_witness", actor->thievery_witness );

    return actor;
}

void hacksaw_activity_actor::start( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, target );
    if( !handle ) {
        act.set_to_null();
        return;
    }

    if( handle->furn() != f_null ) {
        const furn_id furn_type = handle->furn();
        if( !furn_type->hacksaw->valid() ) {
            if( !testing ) {
                debugmsg( "%s hacksaw is invalid", furn_type.id().str() );
            }
            act.set_to_null();
            return;
        }
        progress.emplace( furn_type->name(), to_moves<int>( furn_type->hacksaw->duration() ) );
    } else if( !handle->ter()->is_null() ) {
        const ter_id ter_type = handle->ter();
        if( !ter_type->hacksaw->valid() ) {
            if( !testing ) {
                debugmsg( "%s hacksaw is invalid", ter_type.id().str() );
            }
            act.set_to_null();
            return;
        }
        progress.emplace( ter_type->name(), to_moves<int>( ter_type->hacksaw->duration() ) );
    } else {
        if( !testing ) {
            debugmsg( "hacksaw activity called on invalid terrain" );
        }
        act.set_to_null();
        return;
    }
}

void hacksaw_activity_actor::do_turn( player_activity &/* act */, Character &who )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
    if( tool->ammo_sufficient() ) {
        tool->ammo_consume( tool->ammo_required() );
        sfx::play_activity_sound( "tool", "hacksaw", sfx::get_heard_volume( abs_to_bub( target ), 80 ) );
        if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
            //~ Sound of a metal sawing tool at work!
            sound_event se;
            se.origin = target;
            se.volume = 80;
            se.category = sounds::sound_t::destructive_activity;
            se.description = _( "grnd grnd grnd" );
            se.id = "tool";
            se.variant = "hacksaw";
            se.from_player = who.is_player();
            se.from_npc = !se.from_player;
            se.faction = who.get_faction()->id;
            se.monfaction = who.get_faction()->mon_faction;
            sounds::sound( se );
        }
    } else {
        if( who.is_avatar() ) {
            who.add_msg_if_player( m_bad, _( "Your %1$s ran out of charges." ), tool->tname() );
        } else { // who.is_npc()
            if( get_avatar().sees( who.abs_pos() ) ) {
                add_msg( _( "%1$s %2$s ran out of charges." ), who.disp_name( false,
                         true ), tool->tname() );
            }
        }
        who.cancel_activity();
    }
}

void hacksaw_activity_actor::finish( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, target );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    const activity_data_common *data;

    if( handle->furn() != f_null ) {
        const furn_id furn_type = handle->furn();
        if( !furn_type->hacksaw->valid() ) {
            if( !testing ) {
                debugmsg( "%s hacksaw is invalid", furn_type.id().str() );
            }
            act.set_to_null();
            return;
        }

        const furn_str_id new_furn = furn_type->hacksaw->result();
        if( !new_furn.is_valid() ) {
            if( !testing ) {
                debugmsg( "hacksaw furniture: %s invalid furniture", new_furn.str() );
            }
            act.set_to_null();
            return;
        }

        data = static_cast<const activity_data_common *>( &*furn_type->hacksaw );
        here.set_furn( target, new_furn );
    } else if( !here.ter( target )->obj().is_null() ) {
        const ter_id ter_type = *here.ter( target );
        if( !ter_type->hacksaw->valid() ) {
            if( !testing ) {
                debugmsg( "%s hacksaw is invalid", ter_type.id().str() );
            }
            act.set_to_null();
            return;
        }

        const ter_str_id new_ter = ter_type->hacksaw->result();
        if( !new_ter.is_valid() ) {
            if( !testing ) {
                debugmsg( "hacksaw terrain: %s invalid terrain", new_ter.str() );
            }
            act.set_to_null();
            return;
        }

        data = static_cast<const activity_data_common *>( &*ter_type->hacksaw );
        here.set_ter( target, new_ter );
    } else {
        if( !testing ) {
            debugmsg( "hacksaw activity finished on invalid terrain" );
        }
        act.set_to_null();
        return;
    }

    for( const activity_byproduct &byproduct : data->byproducts() ) {
        const int amount = byproduct.roll();
        if( byproduct.item->count_by_charges() ) {
            here.add_item_or_charges( target, item::spawn( byproduct.item, calendar::turn, amount ) );
        } else {
            for( int i = 0; i < amount; ++i ) {
                here.add_item_or_charges( target, item::spawn( byproduct.item, calendar::turn ) );
            }
        }
    }

    if( !data->message().empty() ) {
        who.add_msg_if_player( m_info, data->message().translated() );
    }

    act.set_to_null();
}

void hacksaw_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "target", target );
    jsout.member( "tool", tool );

    jsout.end_object();
}

std::unique_ptr<activity_actor> hacksaw_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<hacksaw_activity_actor> actor( new hacksaw_activity_actor(
                tripoint_abs_ms::zero(), safe_reference<item>() ) );
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->progress );
    data.read( "target", actor->target );
    data.read( "tool", actor->tool );
    return actor;
}

void boltcutting_activity_actor::start( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, target );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    if( handle->furn() != f_null ) {
        const furn_id furn_type = handle->furn();
        if( !furn_type->boltcut->valid() ) {
            if( !testing ) {
                debugmsg( "%s boltcut is invalid", furn_type.id().str() );
            }
            act.set_to_null();
            return;
        }
        progress.emplace( furn_type->name(), to_moves<int>( furn_type->boltcut->duration() ) );
    } else if( !handle->ter()->is_null() ) {
        const ter_id ter_type = handle->ter();
        if( !ter_type->boltcut->valid() ) {
            if( !testing ) {
                debugmsg( "%s boltcut is invalid", ter_type.id().str() );
            }
            act.set_to_null();
            return;
        }
        progress.emplace( ter_type->name(), to_moves<int>( ter_type->boltcut->duration() ) );
    } else {
        if( !testing ) {
            debugmsg( "boltcut activity called on invalid terrain" );
        }
        act.set_to_null();
        return;
    }
}

void boltcutting_activity_actor::do_turn( player_activity &/* act */, Character &who )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
    if( tool->ammo_sufficient() ) {
        tool->ammo_consume( tool->ammo_required() );
    } else {
        if( who.is_avatar() ) {
            who.add_msg_if_player( m_bad, _( "Your %1$s ran out of charges." ), tool->tname() );
        } else { // who.is_npc()
            if( get_avatar().sees( who.abs_pos() ) ) {
                add_msg( _( "%1$s %2$s ran out of charges." ), who.disp_name( false,
                         true ), tool->tname() );
            }
        }
        who.cancel_activity();
    }
}

void boltcutting_activity_actor::finish( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, target );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    const activity_data_common *data;

    if( handle->furn() != f_null ) {
        const furn_id furn_type = handle->furn();
        if( !furn_type->boltcut->valid() ) {
            if( !testing ) {
                debugmsg( "%s boltcut is invalid", furn_type.id().str() );
            }
            act.set_to_null();
            return;
        }

        const furn_str_id new_furn = furn_type->boltcut->result();
        if( !new_furn.is_valid() ) {
            if( !testing ) {
                debugmsg( "boltcut furniture: %s invalid furniture", new_furn.str() );
            }
            act.set_to_null();
            return;
        }

        data = static_cast<const activity_data_common *>( &*furn_type->boltcut );
        here.set_furn( target, new_furn );
    } else if( !handle->ter()->is_null() ) {
        const ter_id ter_type = handle->ter();
        if( !ter_type->boltcut->valid() ) {
            if( !testing ) {
                debugmsg( "%s boltcut is invalid", ter_type.id().str() );
            }
            act.set_to_null();
            return;
        }

        const ter_str_id new_ter = ter_type->boltcut->result();
        if( !new_ter.is_valid() ) {
            if( !testing ) {
                debugmsg( "boltcut terrain: %s invalid terrain", new_ter.str() );
            }
            act.set_to_null();
            return;
        }

        data = static_cast<const activity_data_common *>( &*ter_type->boltcut );
        here.set_ter( target, new_ter );
    } else {
        if( !testing ) {
            debugmsg( "boltcut activity finished on invalid terrain" );
        }
        act.set_to_null();
        return;
    }
    sound_event se;
    se.origin = target;
    se.volume = 60;
    se.category = sounds::sound_t::combat;
    se.id = "tool";
    se.variant = "boltcutters";
    se.from_player = who.is_player();
    se.from_npc = !se.from_player;
    se.faction = who.get_faction()->id;
    se.monfaction = who.get_faction()->mon_faction;
    if( data->sound().empty() ) {
        se.description = _( "Snick, snick, gachunk!" );
        sounds::sound( se );
    } else {
        se.description = data->sound().translated();
        sounds::sound( se );
    }


    for( const activity_byproduct &byproduct : data->byproducts() ) {
        const int amount = byproduct.roll();
        if( byproduct.item->count_by_charges() ) {
            here.add_item_or_charges( target, item::spawn( byproduct.item, calendar::turn, amount ) );
        } else {
            for( int i = 0; i < amount; ++i ) {
                here.add_item_or_charges( target, item::spawn( byproduct.item, calendar::turn ) );
            }
        }
    }

    if( !data->message().empty() ) {
        who.add_msg_if_player( m_info, data->message().translated() );
    }

    act.set_to_null();
}

void boltcutting_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "target", target );
    jsout.member( "tool", tool );

    jsout.end_object();
}

std::unique_ptr<activity_actor> boltcutting_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<boltcutting_activity_actor> actor( new boltcutting_activity_actor(
                tripoint_abs_ms::zero(), safe_reference<item>() ) );

    JsonObject data = jsin.get_object();
    data.read( "progress", actor->progress );
    data.read( "target", actor->target );
    data.read( "tool", actor->tool );
    return actor;
}

std::unique_ptr<lockpick_activity_actor> lockpick_activity_actor::use_item(
    int moves_total,
    item &lockpick,
    const tripoint_abs_ms &target
)
{
    return std::unique_ptr<lockpick_activity_actor> ( new lockpick_activity_actor(
                moves_total,
                safe_reference<item>( lockpick ),
                detached_ptr<item>(),
                target
            ) );
}

std::unique_ptr<lockpick_activity_actor> lockpick_activity_actor::use_bionic(
    detached_ptr<item> &&fake_lockpick,
    const tripoint_abs_ms &target
)
{
    return std::unique_ptr<lockpick_activity_actor>( new lockpick_activity_actor(
                to_moves<int>( 5_seconds ),
                safe_reference<item>(),
                std::move( fake_lockpick ),
                target
            ) );
}

void lockpick_activity_actor::start( player_activity &/*act*/, Character &who )
{
    const auto target = abs_to_bub( this->target );
    const ter_id ter_type = get_map().ter( target );
    const furn_id furn_type = get_map().furn( target );
    const optional_vpart_position veh = get_map().veh_at( target );
    const auto door_lock = veh.part_with_feature( "DOOR_LOCKING", true );

    if( furn_type != f_null && !furn_type->lockpick_result.is_null() ) {
        progress.emplace( furn_type->name(), moves_total );
    } else if( veh && door_lock ) {
        progress.emplace( veh->vehicle().name, moves_total );
    } else {
        if( ter_type->lockpick_result.is_null() ) {
            debugmsg( "%s lockpick_result is null", ter_type.id().str() );
            return;
        }
        progress.emplace( ter_type->name(), moves_total );
    }
}

void lockpick_activity_actor::do_turn( player_activity &/* act */, Character & )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
}

void lockpick_activity_actor::finish( player_activity &act, Character &who )
{
    act.set_to_null();

    item *it = nullptr;
    if( lockpick ) {
        it = &*lockpick;
    } else if( fake_lockpick ) {
        it = &*fake_lockpick;
    }

    if( !it ) {
        debugmsg( "Lost ACT_LOCKPICK item" );
        return;
    }

    const auto target = abs_to_bub( this->target );
    const ter_id ter_type = get_map().ter( target );
    const furn_id furn_type = get_map().furn( target );
    const optional_vpart_position veh = get_map().veh_at( target );
    const auto door_lock = veh.part_with_feature( "DOOR_LOCKING", true );

    ter_id new_ter_type = t_null;
    furn_id new_furn_type = f_null;
    std::string open_message = _( "The lock opens…" );

    if( furn_type != f_null ) {
        if( furn_type->lockpick_result.is_null() ) {
            debugmsg( "%s lockpick_result is null", furn_type.id().str() );
            return;
        }

        new_furn_type = furn_type->lockpick_result;
        if( !furn_type->lockpick_message.empty() ) {
            open_message = furn_type->lockpick_message.translated();
        }
    } else if( veh ) {
        if( !door_lock ) {
            debugmsg( "%s has no pickable part", furn_type.id().str() );
            return;
        }
    } else {
        if( ter_type->lockpick_result.is_null() ) {
            debugmsg( "%s lockpick_result is null", ter_type.id().str() );
            return;
        }

        new_ter_type = ter_type->lockpick_result;
        if( !ter_type->lockpick_message.empty() ) {
            open_message = ter_type->lockpick_message.translated();
        }
    }

    bool perfect = it->has_flag( flag_PERFECT_LOCKPICK );
    bool durable = it->has_flag( flag_DURABLE_LOCKPICK );
    bool destroy = false;

    /** @EFFECT_DEX improves chances of successfully picking door lock, reduces chances of bad outcomes */
    /** @EFFECT_MECHANICS improves chances of successfully picking door lock, reduces chances of bad outcomes */
    int pick_roll = 5 *
                    ( std::pow( 1.3, who.get_skill_level( skill_mechanics ) ) +
                      it->get_quality( qual_LOCKPICK ) - it->damage() / 2000.0 ) +
                    who.dex_cur / 4.0;
    int lock_roll = rng( 1, 120 );
    int xp_gain = 0;
    if( perfect || ( pick_roll >= lock_roll ) ) {
        xp_gain += lock_roll;

        if( furn_type != f_null ) {
            get_map().furn_set( target, new_furn_type );
        } else if( door_lock ) {
            door_lock->part().enabled = false;
        } else {
            get_map().ter_set( target, new_ter_type );
        }

        who.add_msg_if_player( m_good, open_message );
    } else if( lock_roll > ( 1.5 * pick_roll ) && !durable ) {
        // damage lockpick on a low result, unless it's durable
        if( it->inc_damage() ) {
            who.add_msg_if_player( m_bad,
                                   _( "The lock stumps your efforts to pick it, and you destroy your tool." ) );
            destroy = true;
        } else {
            who.add_msg_if_player( m_bad,
                                   _( "The lock stumps your efforts to pick it, and you damage your tool." ) );
        }
    } else {
        who.add_msg_if_player( m_bad, _( "The lock stumps your efforts to pick it." ) );
    }

    if( !perfect ) {
        // You don't gain much skill since the item does all the hard work for you
        xp_gain += std::pow( 2, who.get_skill_level( skill_mechanics ) ) + 1;
    }
    who.practice( skill_mechanics, xp_gain );

    if( !perfect
        && ( lock_roll + dice( 1, 30 ) ) > pick_roll ) {

        if( get_map().has_flag( "ALARMED", target ) ) {
            sound_event se;
            se.origin = who.abs_pos();
            se.volume = 90;
            se.category = sounds::sound_t::alarm;
            se.description = _( "an alarm sound!" );
            se.id = "environment";
            se.variant = "alarm";
            sounds::sound( se );
            if( !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
                g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0,
                                     who.abs_sm_pos() );
            }
        } else if( veh && veh->vehicle().has_security_working() ) {
            veh->vehicle().is_alarm_on = true;
        }
    }

    if( destroy && lockpick ) {
        lockpick->detach();
    }
}

bool lockpick_activity_actor::is_pickable( mapbuffer &here, const tripoint_abs_ms &p )
{
    auto handle = abs_tile_handle::fetch( here, p );
    if( !handle ) {
        return false;
    }
    const ter_id ter_type = handle->ter();
    const furn_id furn_type = handle->furn();
    const optional_vpart_position veh = handle->vehicle_part();
    const auto door_lock = veh.part_with_feature( "DOOR_LOCKING", true );

    bool result;
    if( furn_type != f_null ) {
        result = !furn_type->lockpick_result.is_null();
    } else if( door_lock ) {
        result = door_lock.value().part().enabled;
    } else {
        result = !ter_type->lockpick_result.is_null();
    }

    return result;
}

std::optional<tripoint_abs_ms> lockpick_activity_actor::select_location( avatar &you )
{
    if( you.is_mounted() ) {
        you.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return std::nullopt;
    }
    auto &here = you.get_mapbuffer();

    const std::optional<tripoint_bub_ms> target_ = choose_adjacent_highlight(
                _( "Use your lockpick where?" ), _( "There is nothing to lockpick nearby." ), [&here]
    ( const tripoint_bub_ms & p ) { return is_pickable( here, bub_to_abs( p ) ); }, false );
    if( !target_ ) {
        return std::nullopt;
    }
    auto target = bub_to_abs( *target_ );

    if( is_pickable( here, target ) ) {
        return target;
    }

    const ter_id terr_type = *here.ter( target );
    if( target == you.abs_pos() ) {
        you.add_msg_if_player( m_info, _( "You pick your nose and your sinuses swing open." ) );
    } else if( here.creature_at( target ) && here.creature_at( target )->is_npc() ) {
        you.add_msg_if_player( m_info,
                               _( "You can pick your friends, and you can pick your nose, but you can't pick your friend's nose." ) );
    } else if( !terr_type->open.is_null() ) {
        you.add_msg_if_player( m_info, _( "That door isn't locked." ) );
    } else {
        you.add_msg_if_player( m_info, _( "That cannot be picked." ) );
    }
    return std::nullopt;
}

void lockpick_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "moves_total", moves_total );
    jsout.member( "lockpick", lockpick );
    jsout.member( "fake_lockpick", fake_lockpick );
    jsout.member( "target", target );

    jsout.end_object();
}

std::unique_ptr<activity_actor> lockpick_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<lockpick_activity_actor> actor( new lockpick_activity_actor( 0,
            safe_reference<item>(), detached_ptr<item>(), tripoint_abs_ms::zero() ) );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "moves_total", actor->moves_total );
    data.read( "lockpick", actor->lockpick );
    data.read( "fake_lockpick", actor->fake_lockpick );
    data.read( "target", actor->target );

    return actor;
}

void oxytorch_activity_actor::start( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, target );
    if( !handle ) {
        act.set_to_null();
        return;
    }

    if( handle->furn() != f_null ) {
        const furn_id furn_type = handle->furn();
        if( !furn_type->oxytorch->valid() ) {
            if( !testing ) {
                debugmsg( "%s oxytorch is invalid", furn_type.id().str() );
            }
            act.set_to_null();
            return;
        }
        progress.emplace( furn_type->name(), to_moves<int>( furn_type->oxytorch->duration() ) );
    } else if( !handle->ter()->is_null() ) {
        const ter_id ter_type = handle->ter();
        if( !ter_type->oxytorch->valid() ) {
            if( !testing ) {
                debugmsg( "%s oxytorch is invalid", ter_type.id().str() );
            }
            act.set_to_null();
            return;
        }
        progress.emplace( ter_type->name(), to_moves<int>( ter_type->oxytorch->duration() ) );
    } else {
        if( !testing ) {
            debugmsg( "oxytorch activity called on invalid terrain" );
        }
        act.set_to_null();
        return;
    }
}

void oxytorch_activity_actor::do_turn( player_activity &/*act*/, Character &who )
{
    // We check available charges when first starting the cut, but this prevents abnormal behavior if torch status changes mid-activity.
    if( tool->ammo_sufficient() ) {
        tool->ammo_consume( tool->ammo_required() );
        sfx::play_activity_sound( "tool", "oxytorch", sfx::get_heard_volume( abs_to_bub( target ), 65 ) );
        if( action_time_scale::once_every_this_tick( 2_turns ) ) {
            sound_event se;
            se.origin = target;
            se.volume = 65;
            se.category = sounds::sound_t::destructive_activity;
            se.description = _( "hissssssssss!" );
            se.id = "tool";
            se.variant = "oxytorch";
            se.from_player = who.is_player();
            se.from_npc = !se.from_player;
            se.faction = who.get_faction()->id;
            se.monfaction = who.get_faction()->mon_faction;
            sounds::sound( se );
        }
    } else {
        if( who.is_avatar() ) {
            who.add_msg_if_player( m_bad, _( "Your %1$s ran out of charges." ), tool->tname() );
        } else { // who.is_npc()
            if( get_avatar().sees( who.abs_pos() ) ) {
                add_msg( _( "%1$s %2$s ran out of charges." ), who.disp_name( false,
                         true ), tool->tname() );
            }
        }
        who.cancel_activity();
    }
    if( progress.front().complete() ) {
        progress.pop();
    }
}

void oxytorch_activity_actor::finish( player_activity &act, Character &who )
{
    auto &here = who.get_mapbuffer();
    auto handle = abs_tile_handle::fetch( here, target );
    if( !handle ) {
        act.set_to_null();
        return;
    }
    const activity_data_common *data;

    if( handle->furn() != f_null ) {
        const furn_id furn_type = handle->furn();
        if( !furn_type->oxytorch->valid() ) {
            if( !testing ) {
                debugmsg( "%s oxytorch is invalid", furn_type.id().str() );
            }
            act.set_to_null();
            return;
        }

        const furn_str_id new_furn = furn_type->oxytorch->result();
        if( !new_furn.is_valid() ) {
            if( !testing ) {
                debugmsg( "oxytorch furniture: %s invalid furniture", new_furn.str() );
            }
            act.set_to_null();
            return;
        }

        data = static_cast<const activity_data_common *>( &*furn_type->oxytorch );
        here.set_furn( target, new_furn );
    } else if( !handle->ter()->is_null() ) {
        const ter_id ter_type = handle->ter();
        if( !ter_type->oxytorch->valid() ) {
            if( !testing ) {
                debugmsg( "%s oxytorch is invalid", ter_type.id().str() );
            }
            act.set_to_null();
            return;
        }

        const ter_str_id new_ter = ter_type->oxytorch->result();
        if( !new_ter.is_valid() ) {
            if( !testing ) {
                debugmsg( "oxytorch terrain: %s invalid terrain", new_ter.str() );
            }
            act.set_to_null();
            return;
        }

        data = static_cast<const activity_data_common *>( &*ter_type->oxytorch );
        here.set_ter( target, new_ter );
    } else {
        if( !testing ) {
            debugmsg( "oxytorch activity finished on invalid terrain" );
        }
        act.set_to_null();
        return;
    }

    for( const activity_byproduct &byproduct : data->byproducts() ) {
        const int amount = byproduct.roll();
        if( byproduct.item->count_by_charges() ) {
            here.add_item_or_charges( target, item::spawn( byproduct.item, calendar::turn, amount ) );
        } else {
            for( int i = 0; i < amount; ++i ) {
                here.add_item_or_charges( target, item::spawn( byproduct.item, calendar::turn ) );
            }
        }
    }

    // 50% chance of starting a fire.
    if( one_in( 2 ) && here.flammable_items_at( target ) ) {
        here.add_field( target, {fd_fire, 1, 10_minutes} );
    }

    if( !data->message().empty() ) {
        who.add_msg_if_player( m_info, data->message().translated() );
    }

    act.set_to_null();
}

void oxytorch_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", progress );
    jsout.member( "target", target );
    jsout.member( "tool", tool );
    jsout.end_object();
}

std::unique_ptr<activity_actor> oxytorch_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<oxytorch_activity_actor> actor( new oxytorch_activity_actor(
                tripoint_abs_ms::zero(), safe_reference<item>() ) );
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->progress );
    data.read( "target", actor->target );
    data.read( "tool", actor->tool );
    return actor;
}

void migration_cancel_activity_actor::do_turn( player_activity &act, Character &who )
{
    // Stop the activity
    act.set_to_null();

    // Ensure that neither avatars nor npcs end up in an invalid state
    if( who.is_npc() ) {
        npc &npc_who = dynamic_cast<npc &>( who );
        npc_who.revert_after_activity();
    } else {
        avatar &avatar_who = dynamic_cast<avatar &>( who );
        avatar_who.clear_destination();
        avatar_who.backlog.clear();
    }
}

void migration_cancel_activity_actor::serialize( JsonOut &jsout ) const
{
    // This will probably never be called, but write null to avoid invalid json in
    // the case that it is
    jsout.write_null();
}

std::unique_ptr<activity_actor> migration_cancel_activity_actor::deserialize( JsonIn & )
{
    return std::unique_ptr<migration_cancel_activity_actor>();
}

void toggle_gate_activity_actor::start( player_activity &, Character & )
{
    progress.emplace( "gate", moves_total );
}

void toggle_gate_activity_actor::do_turn( player_activity &, Character & )
{
    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
}

void toggle_gate_activity_actor::finish( player_activity &act, Character &who )
{
    gates::toggle_gate( who.get_mapbuffer(), placement );
    act.set_to_null();
}

void toggle_gate_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "moves", moves_total );
    jsout.member( "placement", placement );

    jsout.end_object();
}

std::unique_ptr<activity_actor> toggle_gate_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<toggle_gate_activity_actor> actor( new toggle_gate_activity_actor( 0,
            tripoint_abs_ms::zero() ) );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "moves", actor->moves_total );
    data.read( "placement", actor->placement );

    return actor;
}


stash_activity_actor::stash_activity_actor( Character &ch, const drop_locations &items,
        const tripoint_rel_ms &relpos ) : relpos( relpos )
{
    this->items = pickup::reorder_for_dropping( ch, items );
}

void stash_activity_actor::start( player_activity &, Character & )
{
    // Dummy progress task to indicate ongoing activity
    progress.dummy();
}

void stash_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "items", items );
    jsout.member( "relpos", relpos );

    jsout.end_object();
}

std::unique_ptr<activity_actor> stash_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<stash_activity_actor> actor( new stash_activity_actor() );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "items", actor->items );
    data.read( "relpos", actor->relpos );

    return actor;
}

void throw_activity_actor::do_turn( player_activity &act, Character &who )
{
    // Make copies of relevant values since the class would
    // not be available after act.set_to_null()
    if( !target ) {
        debugmsg( "Lost weapon while throwing" );
        act.set_to_null();
        return;
    }

    item *it = &*target;
    std::optional<tripoint_abs_ms> blind_throw_pos = blind_throw_from_pos;

    // Stop the activity. Whether we will or will not throw doesn't matter.
    act.set_to_null();
    if( !who.is_avatar() ) {
        // Sanity check
        debugmsg( "ACT_THROW is not applicable for NPCs." );
        return;
    }

    // Shift our position to our peeking position so the target UI can see from there.
    const auto original_player_position = who.abs_pos();
    if( blind_throw_pos ) {
        who.setpos( *blind_throw_pos );
    }

    target_handler::trajectory trajectory = target_handler::mode_throw( *who.as_avatar(), *it,
                                            blind_throw_pos.has_value() );

    // If we previously shifted our position, put ourselves back now that we've picked our target.
    if( blind_throw_pos ) {
        who.setpos( original_player_position );
    }

    if( trajectory.empty() ) {
        return;
    }

    if( it != &who.primary_weapon() ) {
        // This is to represent "implicit offhand wielding"
        int extra_cost = who.item_handling_cost( *it, true, INVENTORY_HANDLING_PENALTY / 2 );
        who.mod_moves( -extra_cost );
    }
    detached_ptr<item> det = target->split( 1 );
    ranged::throw_item( who, trajectory.back(), std::move( det ), blind_throw_pos );
}

void throw_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "target_loc", target );
    jsout.member( "blind_throw_from_pos", blind_throw_from_pos );

    jsout.end_object();
}

std::unique_ptr<activity_actor> throw_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<throw_activity_actor> actor( new throw_activity_actor() );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "target_loc", actor->target );
    data.read( "blind_throw_from_pos", actor->blind_throw_from_pos );

    return actor;
}


// ---- craft_activity_actor ----

craft_activity_actor::craft_activity_actor(
    const recipe *rec,
    int batch_size,
    int craft_counter,
    const tripoint_abs_ms &location,
    bench_type bench,
    int tools_mult_percent,
    const tripoint_abs_ms &bench_pos,
    std::vector<comp_selection<item_comp>> item_selections,
    std::vector<comp_selection<tool_comp>> tool_selections,
    bool tools_prepaid,
    bool is_long
) : rec( rec ), batch_size( batch_size ), craft_counter( craft_counter ),
    location( location ),
    bench( bench ),
    tools_mult_percent( tools_mult_percent ),
    bench_pos( bench_pos ),
    item_selections( std::move( item_selections ) ),
    tool_selections( std::move( tool_selections ) ),
    tools_prepaid( tools_prepaid ),
    is_long( is_long ),
    is_valid( rec != nullptr )
{}

auto craft_activity_actor::find_in_progress_craft( const player_activity &act,
        Character &who ) const -> item * // *NOPAD*
{
    for( const auto &target : act.targets ) {
        if( target && target->is_craft() && &target->get_making() == rec ) {
            return target.get();
        }
    }

    item *result = nullptr;
    who.visit_items( [&]( item * it ) {
        if( it->is_craft() && &it->get_making() == rec ) {
            result = it;
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );
    if( result ) {
        return result;
    }
    // If not in inventory, check the map at the crafter's feet — set_item_inventory
    // may have placed it there if the NPC was over their carry capacity.
    map_selector sel( who.bub_pos(), 0 );
    sel.visit_items( [&]( item * it ) {
        if( it->is_craft() && &it->get_making() == rec ) {
            result = it;
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );
    return result;
}

void craft_activity_actor::calc_all_moves( player_activity &act, Character &who )
{
    if( !rec || !is_valid ) {
        act.set_to_null();
        return;
    }

    const int current_turn = to_turn<int>( calendar::turn );

    // Catch-up: apply time elapsed while NPC was outside the reality bubble.
    // last_turn_nr >= 0 means start() already ran in a previous session.
    if( last_turn_nr >= 0 && current_turn > last_turn_nr ) {
        item *craft_item = find_in_progress_craft( act, who );
        if( craft_item ) {
            const int elapsed_turns = current_turn - last_turn_nr;
            const double base_total_moves = std::max( 1, rec->batch_time( batch_size, 1.0f, 0 ) );
            // No live crafting modifiers are applied while outside the reality bubble.
            const auto moves_elapsed = action_time_scale::activity_progress_for_turns( elapsed_turns );
            const int old_counter = craft_item->get_counter();
            const int new_counter = std::min(
                                        static_cast<int>( old_counter + moves_elapsed / base_total_moves * 10'000'000.0 ),
                                        10'000'000 );
            craft_item->set_counter( new_counter );
            craft_counter = new_counter;

            const int five_percent_steps = new_counter / 500'000 - old_counter / 500'000;
            if( five_percent_steps > 0 ) {
                who.craft_skill_gain( *craft_item, five_percent_steps );
            }

            // Re-build progress counter to match updated craft state
            const int remaining = std::max( 0, static_cast<int>(
                                                base_total_moves * ( 1.0 - new_counter / 10'000'000.0 ) ) );
            if( !activity_actor::progress.empty() ) {
                activity_actor::progress.mod_moves_left(
                    remaining - activity_actor::progress.get_moves_left() );
            } else {
                activity_actor::progress.emplace( craft_item->tname(),
                                                  static_cast<int>( base_total_moves ), remaining );
            }

            if( new_counter >= 10'000'000 ) {
                // Drain so complete() fires on the next do_turn check
                activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
            }
        }
    }

    last_turn_nr = current_turn;

    // Re-build progress counter after deserialization if catch-up didn't already do it
    if( activity_actor::progress.empty() ) {
        item *craft_item = find_in_progress_craft( act, who );
        const std::string name = craft_item ? craft_item->tname() : rec->result_name();
        const int base_total = std::max( 1, rec->batch_time( batch_size, 1.0f, 0 ) );
        const int remaining = std::max( 1, static_cast<int>(
                                            base_total * ( 1.0 - craft_counter / 10'000'000.0 ) ) );
        activity_actor::progress.emplace( name, base_total, remaining );
    }

    item *craft_item = find_in_progress_craft( act, who );
    if( craft_item ) {
        refresh_speed( act, who, *craft_item );
    }
}

void craft_activity_actor::refresh_speed( player_activity &act, const Character &who,
        const item &craft_item, std::optional<bench_location> bench ) const
{
    const bench_location resolved_bench = bench ? *bench : find_best_bench( who, craft_item );
    const recipe &making = *rec;
    const float tools_mult = cached_tools_mult != 0.0f ? cached_tools_mult
                             : crafting_tools_speed_multiplier( who, making );
    act.speed.light        = lighting_crafting_speed_multiplier( who, making );
    act.speed.bench_factor = workbench_crafting_speed_multiplier( craft_item, resolved_bench );
    act.speed.morale       = morale_crafting_speed_multiplier( who, making );
    act.speed.tools        = tools_mult;
    act.speed.player_speed = who.get_speed() / 100.0f;
    const int assistants   = who.available_assistant_count( making );
    if( assistants > 0 ) {
        const double base_no_assist   = std::max( 1, making.batch_time( batch_size, 1.0f, 0 ) );
        const double base_with_assist = std::max( 1, making.batch_time( batch_size, 1.0f, assistants ) );
        act.speed.assist = static_cast<float>( base_no_assist / base_with_assist );
    } else {
        act.speed.assist = 1.0f;
    }
    // Mutation and game-option multipliers have no dedicated speed field; fold them
    // into skills so act.speed.total() matches the actual crafting rate.
    const float mutation_mult = who.mutation_value( "crafting_speed_modifier" );
    const float game_opt_mult = get_option<int>( "CRAFTING_SPEED_MULT" ) == 0
                                ? 9999.0f
                                : 100.0f / static_cast<float>( get_option<int>( "CRAFTING_SPEED_MULT" ) );
    act.speed.skills = mutation_mult * game_opt_mult;
}

void craft_activity_actor::start( player_activity &act, Character &who )
{
    if( !rec || !is_valid ) {
        act.set_to_null();
        return;
    }

    item *craft_item = find_in_progress_craft( act, who );
    if( !craft_item ) {
        who.add_msg_player_or_npc(
            _( "You lost your in progress %s and had to stop crafting." ),
            _( "<npcname> lost the in progress %s and had to stop crafting." ),
            rec->result_name() );
        act.set_to_null();
        return;
    }

    cached_tools_mult = crafting_tools_speed_multiplier( who, *rec );
    craft_counter = craft_item->get_counter();
    last_turn_nr = to_turn<int>( calendar::turn );  // mark fresh start so calc_all_moves skips catch-up
    const int base_total = std::max( 1, rec->batch_time( batch_size, 1.0f, 0 ) );
    const int remaining = craft_counter == 0
                          ? base_total
                          : std::max( 1, static_cast<int>( base_total * ( 1.0 - craft_counter / 10'000'000.0 ) ) );
    activity_actor::progress.emplace( craft_item->tname(), base_total, remaining );
}

void craft_activity_actor::do_turn( player_activity &act, Character &who )
{
    if( !rec || !is_valid ) {
        act.set_to_null();
        return;
    }

    item *craft_item = find_in_progress_craft( act, who );
    if( !craft_item ) {
        who.add_msg_player_or_npc(
            _( "You no longer have the in progress craft in your possession.  "
               "You stop crafting.  "
               "Reactivate the in progress craft to continue crafting." ),
            _( "<npcname> no longer has the in progress craft in their possession.  "
               "<npcname> stops crafting." ) );
        act.set_to_null();
        return;
    }

    const recipe &making = *rec;
    if( cached_tools_mult == 0.0f ) {
        cached_tools_mult = crafting_tools_speed_multiplier( who, making );
    }
    const bench_location bench = find_best_bench( who, *craft_item );
    refresh_speed( act, who, *craft_item, bench );
    const float crafting_speed = crafting_speed_multiplier( who, *craft_item, bench, act.speed.tools );
    const int assistants = who.available_assistant_count( making );

    if( crafting_speed <= 0.0f ) {
        who.add_msg_player_or_npc( m_bad,
                                   _( "You cannot continue crafting." ),
                                   _( "<npcname> cannot continue crafting." ) );
        act.set_to_null();
        return;
    }

    const int old_counter = craft_item->get_counter();
    const double base_total_moves = std::max( 1, making.batch_time( batch_size, 1.0f, 0 ) );
    const double cur_total_moves = std::max( 1, making.batch_time( batch_size, crafting_speed,
                                   assistants ) );
    const auto scaled_moves = action_time_scale::activity_progress_from_actor_moves( who );
    const auto delta_progress = scaled_moves * base_total_moves / cur_total_moves;
    const double current_progress = old_counter * base_total_moves / 10'000'000.0 + delta_progress;
    const int new_counter = std::min(
                                static_cast<int>( std::round( current_progress / base_total_moves * 10'000'000.0 ) ),
                                10'000'000 );
    const int five_percent_steps = new_counter / 500'000 - old_counter / 500'000;
    craft_item->set_counter( new_counter );
    craft_counter = new_counter;

    who.set_moves( 0 );

    if( five_percent_steps > 0 ) {
        who.craft_skill_gain( *craft_item, five_percent_steps );

        if( !tools_prepaid && !who.craft_consume_tools( *craft_item, five_percent_steps, false ) ) {
            act.set_to_null();
            return;
        }
    }

    // Keep the progress_counter in sync so the UI shows correct values
    if( !activity_actor::progress.empty() ) {
        const int new_moves_left = static_cast<int>(
                                       base_total_moves * ( 1.0 - static_cast<double>( new_counter ) / 10'000'000.0 ) );
        const int delta = new_moves_left - activity_actor::progress.get_moves_left();
        if( delta != 0 ) {
            activity_actor::progress.mod_moves_left( delta );
        }
    }

    last_turn_nr = to_turn<int>( calendar::turn );

    if( new_counter >= 10'000'000 ) {
        // Signal completion so player_activity::do_turn calls finish()
        if( !activity_actor::progress.empty() ) {
            activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
        }
    } else if( new_counter >= craft_item->get_next_failure_point() ) {
        const bool destroy = craft_item->handle_craft_failure( who );
        if( destroy ) {
            who.add_msg_player_or_npc(
                _( "There is nothing left of the %s to craft from." ),
                _( "There is nothing left of the %s <npcname> was crafting." ),
                craft_item->tname() );
            craft_item->detach();
            act.set_to_null();
        }
        // If !destroy, handle_craft_failure may have called cancel_activity already
    }
}

void craft_activity_actor::finish( player_activity &act, Character &who )
{
    act.set_to_null();
    do_complete_craft( act, who );
}

void craft_activity_actor::do_complete_craft( player_activity &act, Character &who )
{
    item *craft_item = find_in_progress_craft( act, who );
    if( !craft_item ) {
        debugmsg( "craft_activity_actor::do_complete_craft: no craft item found for %s",
                  rec ? rec->result_name() : "unknown" );
        return;
    }
    ::complete_craft( who, *craft_item );
    craft_item->detach();
    if( is_long && rec ) {
        if( who.making_would_work( rec->ident(), batch_size ) ) {
            who.last_craft->execute( abs_to_bub( location ) );
        }
    }
}

act_progress_message craft_activity_actor::get_progress_message(
    const player_activity &act, const Character &who ) const
{
    if( !rec || !is_valid ) {
        return act_progress_message::make_empty();
    }

    const int assistants = who.available_assistant_count( *rec );
    const double base_total_moves = std::max( 1, rec->batch_time( batch_size, 1.0f, 0 ) );
    const double remaining_pct = 1.0 - craft_counter / 10'000'000.0;
    const auto total_mult = act.speed.total();
    const auto remaining_moves = static_cast<int>( std::ceil( remaining_pct * base_total_moves ) );
    const auto remaining_turns = action_time_scale::turns_for_progress( remaining_moves,
                                 act.speed.calendar_moves_per_turn() );

    const std::string time_desc = string_format( _( "Time left: %s" ),
                                  to_string( time_duration::from_turns( remaining_turns ) ) );

    const auto fmt_spd = [&]( float level, const std::string & name ) -> std::string {
        const int pct = static_cast<int>( level * 100 );
        if( pct == 100 )
        {
            return "";
        }
        nc_color col = pct > 100 ? c_green : c_red;
        return string_format( " - %s: %s\n", name,
                              colorize( std::to_string( pct ) + '%', col ) );
    };

    std::string mults_desc = _( "Crafting speed multipliers:\n" );
    const int total_pct = static_cast<int>( total_mult * 100 );
    nc_color total_col = total_pct > 100 ? c_green : c_red;
    mults_desc += string_format( " - %s: %s\n", _( "Total" ),
                                 colorize( std::to_string( total_pct ) + '%', total_col ) );
    mults_desc += fmt_spd( act.speed.player_speed, _( "Speed" ) );
    mults_desc += fmt_spd( act.speed.light, _( "Light" ) );
    mults_desc += fmt_spd( act.speed.bench_factor, _( "Workbench" ) );
    mults_desc += fmt_spd( act.speed.morale, _( "Morale" ) );
    mults_desc += fmt_spd( act.speed.tools, _( "Tools" ) );
    if( assistants > 0 ) {
        mults_desc += fmt_spd( act.speed.assist, _( "Assistants" ) );
    }

    return act_progress_message::make_full(
               string_format( _( "%s: %s\n\n%s\n\n%s" ),
                              act.get_verb().translated(), rec->result_name(),
                              time_desc, mults_desc ) );
}

void craft_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "recipe", rec ? rec->ident().str() : std::string() );
    jsout.member( "batch_size", batch_size );
    jsout.member( "craft_counter", craft_counter );
    jsout.member( "location", location );
    jsout.member( "bench", static_cast<int>( bench ) );
    jsout.member( "tools_mult_percent", tools_mult_percent );
    jsout.member( "bench_pos", bench_pos );
    jsout.member( "item_selections", item_selections );
    jsout.member( "tool_selections", tool_selections );
    jsout.member( "tools_prepaid", tools_prepaid );
    jsout.member( "is_long", is_long );
    jsout.member( "last_turn_nr", last_turn_nr );
    jsout.end_object();
}

std::unique_ptr<activity_actor> craft_activity_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<craft_activity_actor>();
    JsonObject data = jsin.get_object();

    data.read( "progress", actor->activity_actor::progress );
    std::string recipe_str;
    data.read( "recipe", recipe_str );
    if( !recipe_str.empty() ) {
        const recipe_id rid( recipe_str );
        if( rid.is_valid() ) {
            actor->rec = &*rid;
            actor->is_valid = true;
        }
    }
    data.read( "batch_size", actor->batch_size );
    data.read( "craft_counter", actor->craft_counter );
    data.read( "location", actor->location );

    int bt = 0;
    data.read( "bench", bt );
    actor->bench = static_cast<bench_type>( bt );
    data.read( "tools_mult_percent", actor->tools_mult_percent );
    data.read( "bench_pos", actor->bench_pos );

    data.read( "item_selections", actor->item_selections );
    data.read( "tool_selections", actor->tool_selections );
    data.read( "tools_prepaid", actor->tools_prepaid );
    data.read( "is_long", actor->is_long );
    data.read( "last_turn_nr", actor->last_turn_nr );

    return actor;
}

std::unique_ptr<activity_actor> craft_activity_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<craft_activity_actor>();

    // bench type from values[1]
    auto values = data.get_int_array( "values" );
    if( values.size() >= 2 ) {
        actor->bench = static_cast<bench_type>( values[1] );
    }
    // tools_mult_percent from values[2] (optional)
    if( values.size() >= 3 ) {
        actor->tools_mult_percent = values[2];
    }
    // bench_pos from coords[0]
    auto coords = std::vector<tripoint_abs_ms>();
    data.read( "coords", coords );
    if( !coords.empty() ) {
        actor->bench_pos = coords[0];
    }

    // Note: rec, batch_size, location, item_selections, tool_selections, etc.
    // are set by the existing ACT_CRAFT creation path via do_activity in crafting.cpp.
    // Legacy saves have them in the player_activity fields which get read separately.
    // This only handles the fields we're migrating INTO the actor.

    return actor;
}

inline void construction_activity_actor::calc_all_moves( player_activity &act, Character &who )
{
    // Check if pc was lost for some reason, but actually still exists on map, e.g. save/load
    if( !pc ) {
        map &here = get_map();
        auto local = abs_to_bub( target );
        pc = here.partial_con_at( tripoint_bub_ms( local ) );
    }
    //if something goes terribly wrong we don't CTD
    if( !pc ) {
        act.set_to_null();
        return;
    }
    auto reqs = activity_reqs_adapter( *pc->id );
    act.speed.calc_all_moves( who, reqs );
}

void construction_activity_actor::start( player_activity &/*act*/, Character &/*who*/ )
{
    map &here = get_map();
    auto local = abs_to_bub( target );
    pc = here.partial_con_at( tripoint_bub_ms( local ) );
    auto &built = *pc->id;

    std::string name;

    if( pc->id == deconstruct || pc->id == deconstruct_simple ||
        built.group == advanced_object_deconstruction ) {
        if( here.has_furn( local ) ) {
            const furn_id furn_type = here.furn( local );
            name = furn_type->name();
        } else if( !here.ter( local )->is_null() ) {
            const ter_id ter_type = here.ter( local );
            name = ter_type->name();
        }
    } else {
        name = built.post_furniture.is_empty()
               ? ""
               : built.post_furniture->name();
        name = built.post_terrain.is_empty()
               ? name
               : built.post_terrain->name();
    }

    int total_time = std::max( 1, built.adjusted_time() );
    int left = pc->counter == 0
               ? total_time
               : total_time - pc->counter / 10'000'000.0 * total_time;

    progress.emplace( name, total_time, left );
}

void construction_activity_actor::do_turn( player_activity &act, Character &who )
{
    // Check if pc was lost for some reason, but actually still exists on map, e.g. save/load
    if( !pc ) {
        map &here = get_map();
        auto local = abs_to_bub( target );
        pc = here.partial_con_at( tripoint_bub_ms( local ) );
    }

    // Maybe the player and the NPC are working on the same construction at the same time or toubles during load
    if( !pc ) {
        act.set_to_null();
        add_msg( m_info, _( "%s did not find an unfinished construction at the activity spot." ),
                 who.disp_name() );
        return;
    }

    pc->counter = progress.front().to_counter();

    if( progress.front().complete() ) {
        progress.pop();
        return;
    } else {
        auto &built = *pc->id;
        if( !who.has_trait( trait_DEBUG_HS ) && !who.meets_skill_requirements( built ) ) {
            add_msg( m_info, _( "%s can't work on this construction anymore." ), who.disp_name() );
            act.set_to_null();
            return;
        }
    }
}

void construction_activity_actor::finish( player_activity &act, Character &who )
{
    complete_construction( who, target );
    act.set_to_null();
}

void construction_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", progress );
    jsout.member( "target", target );
    jsout.end_object();
}

std::unique_ptr<activity_actor> construction_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<construction_activity_actor> actor( new construction_activity_actor(
                tripoint_abs_ms( tripoint_zero ) ) );
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->progress );
    data.read( "target", actor->target );
    return actor;
}

void assist_activity_actor::start( player_activity &/*act*/, Character &/*who*/ )
{
    progress.dummy();
}

void assist_activity_actor::serialize( JsonOut &jsout ) const
{
    // Activity is not being saved but still provide some valid json if called.
    jsout.write_null();
}

std::unique_ptr<activity_actor> assist_activity_actor::deserialize( JsonIn & )
{
    return std::make_unique<assist_activity_actor>();
}

std::unique_ptr<activity_actor> salvage_activity_actor::deserialize( JsonIn &jsin )
{
    std::unique_ptr<salvage_activity_actor> actor( new salvage_activity_actor() );

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "targets", actor->targets );
    data.read( "pos", actor->pos );
    data.read( "mute_prompts", actor->mute_prompts );

    return actor;
}

// ─────────────────────────────────────────────────────────────────────────────
// liquid_transfer_actor (ACT_FILL_LIQUID)
// ─────────────────────────────────────────────────────────────────────────────

liquid_transfer_actor::liquid_transfer_actor(
    liquid_source_type src_type,
    const tripoint_abs_ms &src_pos,
    int src_part_index,
    liquid_target_type tgt_type,
    const tripoint_abs_ms &tgt_pos,
    safe_reference<item> tgt_container
) : source_type( src_type )
    , source_pos( src_pos )
    , source_part_index( src_part_index )
    , target_type( tgt_type )
    , target_pos( tgt_pos )
    , target_container( std::move( tgt_container ) )
{}

void liquid_transfer_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "source_type", static_cast<int>( source_type ) );
    jsout.member( "source_pos", source_pos );
    jsout.member( "source_part_index", source_part_index );
    jsout.member( "target_type", static_cast<int>( target_type ) );
    jsout.member( "target_pos", target_pos );
    jsout.member( "target_container", target_container );
    jsout.end_object();
}

std::unique_ptr<activity_actor> liquid_transfer_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<liquid_transfer_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );

    int st = 0;
    data.read( "source_type", st );
    actor->source_type = static_cast<liquid_source_type>( st );

    data.read( "source_pos", actor->source_pos );
    data.read( "source_part_index", actor->source_part_index );

    int tt = 0;
    data.read( "target_type", tt );
    actor->target_type = static_cast<liquid_target_type>( tt );

    data.read( "target_pos", actor->target_pos );
    data.read( "target_container", actor->target_container );

    return actor;
}

std::unique_ptr<activity_actor> liquid_transfer_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<liquid_transfer_actor>();

    auto values = data.get_int_array( "values" );
    if( values.size() >= 1 ) {
        actor->source_type = static_cast<liquid_source_type>( values[0] );
    }
    if( values.size() >= 2 ) {
        actor->source_part_index = values[1];
    }
    if( values.size() >= 3 ) {
        actor->target_type = static_cast<liquid_target_type>( values[2] );
    }

    auto coords = std::vector<tripoint_abs_ms>();
    data.read( "coords", coords );
    if( coords.size() >= 1 ) {
        actor->source_pos = coords[0];
    }
    if( coords.size() >= 2 ) {
        actor->target_pos = coords[1];
    }

    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    if( !targets.empty() ) {
        actor->target_container = std::move( targets[0] );
    }

    restore_legacy_progress( *actor, data, "transferring liquid" );
    return actor;
}

void liquid_transfer_actor::start( player_activity &, Character & ) {}
void liquid_transfer_actor::do_turn( player_activity &act, Character &who )
{

    map &here = get_map();
    try {

        auto transfer = [this, &here]( const std::function < detached_ptr<item>
        ( detached_ptr<item> &&it ) > & cb ) {
            static const units::volume volume_per_second = units::from_liter( 4.0F / 6.0F );
            int charges;
            detached_ptr<item> source;
            switch( source_type ) {
                case LST_INFINITE_MAP:
                    source = here.water_from( abs_to_bub( source_pos ) );
                    charges = std::max( 1, source->charges_per_volume( volume_per_second ) );
                    source->charges = charges;
                    source = cb( std::move( source ) );
                    return source && source->charges == charges;
                case LST_VEHICLE:
                    auto vp = here.veh_at( source_pos );
                    if( !vp ) {
                        throw std::runtime_error( "could not find vehicle source for liquid transfer" );
                    }
                    if( source_part_index < 0 || source_part_index >= vp->vehicle().part_count() ) {
                        throw std::runtime_error( "invalid vehicle source part for liquid transfer" );
                    }
                    item &base = vp->vehicle().part( source_part_index ).get_base();
                    if( base.contents.empty() ) {
                        return true;
                    }
                    item &source_it = base.contents.back();
                    charges = std::max( 1, source_it.charges_per_volume( volume_per_second ) );
                    int orig = source_it.charges;
                    source_it.attempt_split( charges, cb );
                    return source_it.charges == 0 || source_it.charges == orig;
            }
            return false;
        };
        bool finished = true;
        switch( target_type ) {
            case LTT_VEHICLE:
                if( const optional_vpart_position vp = here.veh_at( target_pos ) ) {
                    finished = transfer( [&who, &vp]( detached_ptr<item> &&it ) {
                        return who.pour_into( vp->vehicle(), std::move( it ) );
                    } );
                } else {
                    throw std::runtime_error( "could not find target vehicle for liquid transfer" );
                }
                break;
            case LTT_MAP: {
                const auto bub_loc = abs_to_bub( target_pos );
                if( iexamine::has_keg( bub_loc ) ) {
                    finished = transfer( [&bub_loc]( detached_ptr<item> &&it ) {
                        return iexamine::pour_into_keg( bub_loc, std::move( it ) );
                    } );
                } else {
                    finished = transfer( [&who, this]( detached_ptr<item> &&it ) {
                        who.add_msg_if_player( _( "You pour %1$s onto the ground." ), it->tname() );
                        who.get_mapbuffer().add_item_or_charges( target_pos, std::move( it ) );
                        return detached_ptr<item>();
                    } );
                }
            }
            break;
            case LTT_MONSTER:
                break;
            case LTT_CONTAINER:
                if( !target_container ) {
                    throw std::runtime_error( "could not find target container for liquid transfer" );
                }
                finished = transfer( [&who, this]( detached_ptr<item> &&it ) {
                    return who.pour_into( *target_container, std::move( it ) );
                } );
                break;
        }
        if( finished ) {
            act.set_to_null();
        }

    } catch( const std::runtime_error &err ) {
        debugmsg( "error in activity data: \"%s\"", err.what() );
        act.set_to_null();
        return;
    }
}
void liquid_transfer_actor::finish( player_activity &, Character & ) {}

// ─────────────────────────────────────────────────────────────────────────────
// vehicle_work_actor (ACT_VEHICLE)
// ─────────────────────────────────────────────────────────────────────────────

vehicle_work_actor::vehicle_work_actor( vehicle_work_actor_options options )
    : command( options.command )
    , part_pos( options.part_pos )
    , cursor_mount( options.cursor_mount )
    , part_type( options.part_type )
    , part_index( options.part_index )
    , moves_total( options.moves_total )
    , vehicle_points( std::move( options.vehicle_points ) )
{}

void vehicle_work_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "command", static_cast<int>( command ) );
    jsout.member( "part_pos", part_pos );
    jsout.member( "cursor_mount", cursor_mount );
    jsout.member( "part_type", part_type );
    jsout.member( "part_index", part_index );
    jsout.member( "moves_total", moves_total );
    jsout.member( "vehicle_points", vehicle_points );
    jsout.end_object();
}

std::unique_ptr<activity_actor> vehicle_work_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<vehicle_work_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );

    int cmd = 0;
    data.read( "command", cmd );
    actor->command = static_cast<char>( cmd );

    data.read( "part_pos", actor->part_pos );
    data.read( "cursor_mount", actor->cursor_mount );
    data.read( "part_type", actor->part_type );
    data.read( "part_index", actor->part_index );
    data.read( "moves_total", actor->moves_total );
    data.read( "vehicle_points", actor->vehicle_points );

    return actor;
}

std::unique_ptr<activity_actor> vehicle_work_actor::legacy_deserialize( const JsonObject &data )
{
    auto values = data.get_int_array( "values" );

    // Need at least 7 values for either format
    if( values.size() < 7 ) {
        return nullptr;
    }

    const bool very_old = values.size() == 8;

    // Part position — values[0], values[1] are bubble in very old format, abs otherwise
    tripoint_abs_ms part_pos;
    if( very_old ) {
        part_pos = bub_to_abs( tripoint_bub_ms( values[0], values[1], values[7] ) );
    } else {
        part_pos = tripoint_abs_ms( values[0], values[1], values[2] );
    }

    // Cursor mount
    tripoint_mnt_veh cursor_mount{ 0, 0, 0 };
    if( very_old ) {
        cursor_mount = tripoint_mnt_veh( -values[4], -values[5], 0 );
    } else {
        cursor_mount = tripoint_mnt_veh( values[3], values[4], values[5] );
    }

    int part_index = values[6];
    const int moves_total = data.get_int( "moves_total", 0 );
    const int moves_left = data.get_int( "moves_left", moves_total );

    // Part type from str_values
    auto str_values = data.get_string_array( "str_values" );
    vpart_id part_type;
    if( !str_values.empty() ) {
        part_type = vpart_id( str_values[0] );
    }

    // Command from index
    char command = static_cast<char>( data.get_int( "index" ) );

    // Vehicle points from coord_set
    auto vehicle_points = std::unordered_set<tripoint_abs_ms>();
    data.read( "coord_set", vehicle_points );

    auto actor = std::make_unique<vehicle_work_actor>( vehicle_work_actor_options{
        .command = command,
        .part_pos = part_pos,
        .cursor_mount = cursor_mount,
        .part_type = part_type,
        .part_index = part_index,
        .moves_total = moves_total,
        .vehicle_points = std::move( vehicle_points ),
    } );
    if( moves_total > 0 ) {
        actor->progress.emplace( part_type.str().empty() ? "vehicle part" : part_type.str(),
                                 moves_total, std::max( 0, moves_left ) );
    }
    return actor;
}

void vehicle_work_actor::start( player_activity &/*act*/, Character &/*who*/ )
{
    if( progress.empty() ) {
        progress.emplace( part_type.str().empty() ? "vehicle part" : part_type.str(),
                          moves_total );
    }
}
void vehicle_work_actor::do_turn( player_activity &act, Character &who ) {}

void vehicle_work_actor::finish( player_activity &act, Character &who )
{
    map &here = get_map();
    auto &p = static_cast<player &>( who );

    // Bridge: populate legacy fields for complete_vehicle()
    act.values = {
        part_pos.x(), part_pos.y(), part_pos.z(),
        cursor_mount.x(), cursor_mount.y(), cursor_mount.z(),
        part_index
    };
    act.str_values = { part_type.str() };
    act.index = static_cast<int>( command );
    act.coord_set = vehicle_points;

    veh_interact::complete_vehicle( who );
    // complete_vehicle set activity type to NULL if the vehicle
    // was completely dismantled, otherwise the vehicle still exists and
    // is to be examined again.
    if( act.is_null() ) {
        if( npc *guy = dynamic_cast<npc *>( &p ) ) {
            guy->revert_after_activity();
            guy->set_moves( 0 );
        }
        return;
    }
    act.set_to_null();
    if( !p.is_npc() ) {
        const auto find_vehicle = [&]() -> vehicle * {
            if( const optional_vpart_position current = here.veh_at( part_pos ) )
            {
                return &current->vehicle();
            }
            for( const tripoint_abs_ms &point : vehicle_points )
            {
                if( const optional_vpart_position current = here.veh_at( point ) ) {
                    return &current->vehicle();
                }
            }
            return nullptr;
        };
        if( vehicle *const current_vehicle = find_vehicle() ) {
            here.invalidate_map_cache( g->get_levz() );
            if( !activity_handlers::resume_for_multi_activities( who ) ) {
                g->exam_vehicle( *current_vehicle, cursor_mount );
            }
        } else {
            debugmsg( "process_activity ACT_VEHICLE: vehicle not found" );
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// train_skill_activity_actor (ACT_TRAIN_SKILL)
// ─────────────────────────────────────────────────────────────────────────────

train_skill_activity_actor::train_skill_activity_actor(
    train_skill_activity_actor_options options
) : training_skill( std::move( options.training_skill ) )
    , training_skill_xp( options.training_skill_xp )
    , training_skill_xp_chance( options.training_skill_xp_chance )
    , training_skill_max_level( options.training_skill_max_level )
    , training_skill_fatigue( options.training_skill_fatigue )
    , training_skill_interval( options.training_skill_interval )
    , moves_total( options.moves_total )
    , tool( std::move( options.tool ) )
    , pseudo_tool( options.pseudo_tool )
    , pseudo_tool_pos( options.pseudo_tool_pos )
    , pseudo_tool_type( std::move( options.pseudo_tool_type ) )
{}

namespace
{

auto get_train_skill_iuse( const itype_id &tool_type ) -> const train_skill_actor *
{
    if( !tool_type.is_valid() ) {
        return nullptr;
    }
    const use_function *use = tool_type->get_use( "train_skill" );
    if( use == nullptr ) {
        return nullptr;
    }
    return dynamic_cast<const train_skill_actor *>( use->get_actor_ptr() );
}

auto train_skill_options_from_iuse( const train_skill_actor &iuse,
                                    int duration ) -> train_skill_activity_actor_options
{
    return {
        .training_skill = iuse.training_skill,
        .training_skill_xp = iuse.training_skill_xp,
        .training_skill_xp_chance = iuse.training_skill_xp_chance,
        .training_skill_max_level = iuse.training_skill_max_level,
        .training_skill_fatigue = iuse.training_skill_fatigue,
        .training_skill_interval = iuse.training_skill_interval,
        .moves_total = duration,
    };
}

} // namespace

auto train_skill_activity_actor::get_tool( Character &who ) const -> item *
{
    if( !pseudo_tool ) {
        return tool ? &*tool : nullptr;
    }

    mapbuffer &here = who.get_mapbuffer();
    const auto furniture = here.furn( pseudo_tool_pos );
    if( !furniture ) {
        debugmsg( "Lost furniture for skill training at %s", pseudo_tool_pos.to_string() );
        return nullptr;
    }

    const auto &item_types = furniture->obj().crafting_pseudo_item_types();
    const bool is_valid_tool = std::ranges::any_of( item_types,
    [this]( const itype & item_type ) {
        return item_type.get_id() == pseudo_tool_type;
    } );
    if( !is_valid_tool ) {
        debugmsg( "Lost pseudo training tool %s at %s", pseudo_tool_type.c_str(),
                  pseudo_tool_pos.to_string() );
        return nullptr;
    }

    item *fake_tool = item::spawn_temporary( pseudo_tool_type, calendar::turn, 0 );
    fake_tool->set_flag( flag_PSEUDO );
    if( fake_tool->has_flag( flag_USES_GRID_POWER ) ) {
        auto *tracker = get_distribution_grid_tracker_for( who.get_dimension() );
        if( tracker == nullptr ) {
            debugmsg( "Lost power grid for pseudo training tool %s at %s",
                      pseudo_tool_type.c_str(), pseudo_tool_pos.to_string() );
            return nullptr;
        }
        fake_tool->charges = tracker->grid_at( pseudo_tool_pos ).get_resource();
    }
    return fake_tool;
}

auto train_skill_activity_actor::apply_training( Character &who, item &training_tool ) const -> bool
{
    const skill_id skill( training_skill );
    if( !skill.is_valid() ) {
        debugmsg( "Invalid skill %s for ACT_TRAIN_SKILL", training_skill );
        return false;
    }

    who.mod_fatigue( training_skill_fatigue );
    who.mod_stamina( -training_skill_fatigue * 36 );

    if( training_tool.ammo_remaining() > 0 ) {
        const int original_charges = training_tool.charges;
        training_tool.ammo_consume( 1 );
        if( pseudo_tool ) {
            const int discharged = original_charges - training_tool.charges;
            if( discharged > 0 ) {
                auto *tracker = get_distribution_grid_tracker_for( who.get_dimension() );
                if( tracker != nullptr ) {
                    const int unfulfilled_demand = tracker->grid_at( pseudo_tool_pos ).mod_resource( -discharged );
                    if( unfulfilled_demand != 0 ) {
                        debugmsg( "Pseudo training tool discharged more power than available: %d kJ",
                                  unfulfilled_demand );
                    }
                }
            }
        }
    } else if( training_tool.ammo_required() > 0 ) {
        who.add_msg_if_player( m_info, _( "The %s runs out of power." ), training_tool.tname() );
        return false;
    }

    if( who.get_skill_level( skill ) >= training_skill_max_level ) {
        who.add_msg_if_player( m_info, _( "You can no longer learn anything from this." ) );
        return false;
    }

    if( rng( 1, 100 ) < training_skill_xp_chance ) {
        who.practice( skill, training_skill_xp, training_skill_max_level );
    }
    return true;
}

auto train_skill_activity_actor::start( player_activity &, Character & ) -> void
{
    if( progress.empty() ) {
        progress.emplace( "training", moves_total );
    }
}

auto train_skill_activity_actor::do_turn( player_activity &act, Character &who ) -> void
{
    if( training_skill_interval <= 0 ) {
        debugmsg( "Invalid training interval for ACT_TRAIN_SKILL: %d", training_skill_interval );
        act.set_to_null();
        return;
    }

    if( action_time_scale::once_every_this_tick( 1_minutes * training_skill_interval ) ) {
        item *training_tool = get_tool( who );
        if( training_tool == nullptr || !apply_training( who, *training_tool ) ) {
            act.set_to_null();
            return;
        }
    }

    if( who.get_fatigue() >= fatigue_levels::dead_tired ) {
        who.add_msg_if_player( _( "You're too tired to continue." ) );
        act.set_to_null();
    }
}

auto train_skill_activity_actor::finish( player_activity &act, Character &who ) -> void
{
    who.add_msg_if_player( m_good, _( "You feel like you've learned a little bit." ) );
    act.set_to_null();
}

auto train_skill_activity_actor::serialize( JsonOut &jsout ) const -> void
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "training_skill", training_skill );
    jsout.member( "training_skill_xp", training_skill_xp );
    jsout.member( "training_skill_xp_chance", training_skill_xp_chance );
    jsout.member( "training_skill_max_level", training_skill_max_level );
    jsout.member( "training_skill_fatigue", training_skill_fatigue );
    jsout.member( "training_skill_interval", training_skill_interval );
    jsout.member( "moves_total", moves_total );
    jsout.member( "tool", tool );
    jsout.member( "pseudo_tool", pseudo_tool );
    jsout.member( "pseudo_tool_pos", pseudo_tool_pos );
    jsout.member( "pseudo_tool_type", pseudo_tool_type );
    jsout.end_object();
}

auto train_skill_activity_actor::deserialize( JsonIn &jsin ) -> std::unique_ptr<activity_actor>
{
    auto actor = std::make_unique<train_skill_activity_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "training_skill", actor->training_skill );
    data.read( "training_skill_xp", actor->training_skill_xp );
    data.read( "training_skill_xp_chance", actor->training_skill_xp_chance );
    data.read( "training_skill_max_level", actor->training_skill_max_level );
    data.read( "training_skill_fatigue", actor->training_skill_fatigue );
    data.read( "training_skill_interval", actor->training_skill_interval );
    data.read( "moves_total", actor->moves_total );
    data.read( "tool", actor->tool );
    data.read( "pseudo_tool", actor->pseudo_tool );
    data.read( "pseudo_tool_pos", actor->pseudo_tool_pos );
    data.read( "pseudo_tool_type", actor->pseudo_tool_type );
    return actor;
}

auto train_skill_activity_actor::legacy_deserialize( const JsonObject &data )
-> std::unique_ptr<activity_actor> // *NOPAD*
{
    auto str_values = data.get_string_array( "str_values" );
    auto values = data.get_int_array( "values" );
    if( str_values.empty() ) {
        debugmsg( "Legacy ACT_TRAIN_SKILL has no tool type" );
        return nullptr;
    }

    const bool is_furniture = values.size() >= 3 &&
                              values[2] == static_cast<int>( hack_type_t::furniture );
    const itype_id tool_type( is_furniture && str_values.size() >= 2 ? str_values[1] : str_values[0] );
    const train_skill_actor *iuse = get_train_skill_iuse( tool_type );
    if( iuse == nullptr ) {
        debugmsg( "Legacy ACT_TRAIN_SKILL tool %s has no train_skill use", tool_type.c_str() );
        return nullptr;
    }

    auto options = train_skill_options_from_iuse( *iuse, data.get_int( "moves_total", 0 ) );
    options.pseudo_tool = is_furniture;
    options.pseudo_tool_type = tool_type;
    if( is_furniture ) {
        auto coords = std::vector<tripoint_abs_ms>();
        data.read( "coords", coords );
        if( coords.empty() ) {
            debugmsg( "Legacy ACT_TRAIN_SKILL furniture activity has no position" );
            return nullptr;
        }
        options.pseudo_tool_pos = coords.front();
    } else {
        auto tools = std::vector<safe_reference<item>>();
        data.read( "tools", tools );
        if( !tools.empty() ) {
            options.tool = std::move( tools.front() );
        }
    }

    auto actor = std::make_unique<train_skill_activity_actor>( std::move( options ) );
    restore_legacy_progress( *actor, data, "training" );
    return actor;
}

// ─────────────────────────────────────────────────────────────────────────────
// enchant_activity_actor (ACT_ENCHANT)
// ─────────────────────────────────────────────────────────────────────────────

void enchant_activity_actor::start( player_activity &act, Character & )
{
    if( !target ) {
        debugmsg( "Lost object being enchanted" );
        act.set_to_null();
        return;
    }
    auto name = string_format( "Enchant %s", target->display_name() );
    progress.emplace( name, moves_total );
}

void enchant_activity_actor::do_turn( player_activity &act, Character & )
{
    if( !target ) {
        debugmsg( "Lost object being enchanted" );
        act.set_to_null();
        return;
    }

    if( progress.front().complete() ) {
        progress.pop();
        return;
    }
}

void enchant_activity_actor::finish( player_activity &act, Character &who )
{
    if( !target ) {
        debugmsg( "Lost object being enchanted" );
        act.set_to_null();
        return;
    }
    if( !furn.is_valid() ) {
        debugmsg( "The furniture that was used is invalid" );
        act.set_to_null();
        return;
    }
    auto info = enchant_info();
    auto found_info = false;
    for( const enchant_info &ench_info : furn->enchanter ) {
        if( ench_info.id == enchanter_id ) {
            info = ench_info;
            found_info = true;
            break;
        }
    }
    if( !found_info ) {
        debugmsg( "The enchantment could not be found in furniture definition." );
        act.set_to_null();
        return;
    }

    auto total_reqs =
        enchanter::total_requirements( info )
        * std::max(
            1, ( info.volume_batch_effect ? int( target->base_volume() / info.volume_per_batch ) : 1 ) );
    for( const auto &comp : total_reqs.get_components() ) { who.consume_items( comp ); }
    for( const auto &comp : total_reqs.get_tools() ) { who.consume_tools( comp ); }
    target->add_enchantment( info.to_enchant_with );
    target->set_var( info.count_var, target->get_var<int>( info.count_var, 0 ) + 1 );
    if( info.applied_flag_id.is_valid() ) { target->set_flag( info.applied_flag_id ); }
    act.set_to_null();
}

void enchant_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "target_obj", target );
    jsout.member( "furn", furn );
    jsout.member( "enchanter_id", enchanter_id );

    jsout.end_object();
}

std::unique_ptr<activity_actor> enchant_activity_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<enchant_activity_actor>();

    JsonObject data = jsin.get_object();

    data.read( "progress", actor->progress );
    data.read( "target_obj", actor->target );
    data.read( "furn", actor->furn );
    data.read( "enchanter_id", actor->enchanter_id );

    return actor;
}

// ─────────────────────────────────────────────────────────────────────────────
// repair_actor (ACT_REPAIR_ITEM)
// ─────────────────────────────────────────────────────────────────────────────

repair_actor::repair_actor(
    hack_type_t htype,
    const tripoint_abs_ms &tpos,
    const itype_id &ttool,
    int cpart_idx
) : hack_type( htype )
    , target_pos( tpos )
    , tool_type( ttool )
    , crafter_part_index( cpart_idx )
{}

repair_actor::repair_actor(
    const std::string &name,
    safe_reference<item> tool_ref,
    int pos
) : is_hack( false )
    , iuse_name( name )
    , tool( tool_ref )
    , item_pos( pos )
{}

void repair_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "hack_type", static_cast<int>( hack_type ) );
    jsout.member( "target_pos", target_pos );
    jsout.member( "tool_type", tool_type );
    jsout.member( "crafter_part_index", crafter_part_index );
    jsout.member( "iuse_name", iuse_name );
    jsout.member( "item_pos", item_pos );
    jsout.member( "tool", tool );
    jsout.member( "repeat", repeat );
    jsout.member( "fix_target", fix_target );
    jsout.end_object();
}

std::unique_ptr<activity_actor> repair_actor::deserialize( JsonIn &jsin )
{
    auto data = jsin.get_object();
    auto actor = std::make_unique<repair_actor>();
    data.read( "progress", actor->activity_actor::progress );
    if( data.has_member( "hack_type" ) ) {
        actor->is_hack = true;
        int ht = 0;
        data.read( "hack_type", ht );
        actor->hack_type = static_cast<hack_type_t>( ht );
        data.read( "target_pos", actor->target_pos );
        data.read( "tool_type", actor->tool_type );
        data.read( "crafter_part_index", actor->crafter_part_index );
    } else {
        actor->is_hack = false;
        data.read( "iuse_name", actor->iuse_name );
        data.read( "item_pos", actor->item_pos );
        data.read( "tool", actor->tool );
        data.read( "repeat", actor->repeat );
        data.read( "fix_target", actor->fix_target );
    }
    return actor;
}

std::unique_ptr<activity_actor> repair_actor::legacy_deserialize( const JsonObject &data )
{
    // Determine if hack path: has values[2] (hack_type) set to non-zero
    auto values = data.get_int_array( "values" );
    if( values.size() >= 3 && values[2] != 0 ) {
        // Hack path
        auto actor = std::make_unique<repair_actor>();
        actor->is_hack = true;
        if( values.size() >= 2 ) { actor->crafter_part_index = values[1]; }
        actor->hack_type = static_cast<hack_type_t>( values[2] );

        // tool_type from str_values[1]
        auto str_values = data.get_string_array( "str_values" );
        if( str_values.size() >= 2 ) { actor->tool_type = itype_id( str_values[1] ); }

        // target_pos from coords[0]
        auto coords = std::vector<tripoint_abs_ms>();
        data.read( "coords", coords );
        if( !coords.empty() ) { actor->target_pos = coords[0]; }
        const int moves_total = data.get_int( "moves_total", 0 );
        if( moves_total > 0 ) {
            actor->progress.emplace( "repairing", moves_total,
                                     std::max( 0, data.get_int( "moves_left", moves_total ) ) );
        }
        return actor;
    }

    // Core path
    auto actor = std::make_unique<repair_actor>();
    actor->is_hack = false;
    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() ) { actor->iuse_name = str_values[0]; }
    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    if( !targets.empty() ) { actor->tool = std::move( targets[0] ); }
    actor->item_pos = data.get_int( "index", -1 );
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 ) {
        actor->progress.emplace( "repairing", moves_total,
                                 std::max( 0, data.get_int( "moves_left", moves_total ) ) );
    }
    return actor;
}

static const itype_id itype_battery( "battery" );

// ─── Repair helpers (moved from activity_handlers.cpp) ─────────────────────

static repeat_type repeat_menu( const std::string &title, repeat_type last_selection )
{
    uilist rmenu;
    rmenu.text = title;

    rmenu.addentry( REPEAT_ONCE, true, '1', _( "Repeat once" ) );
    rmenu.addentry( REPEAT_FOREVER, true, '2', _( "Repeat until reinforced" ) );
    rmenu.addentry( REPEAT_FULL, true, '3', _( "Repeat until fully repaired, but don't reinforce" ) );
    rmenu.addentry( REPEAT_EVENT, true, '4', _( "Repeat until success/failure/level up" ) );
    rmenu.addentry( REPEAT_INIT, true, '5', _( "Back to item selection" ) );

    rmenu.selected = last_selection - REPEAT_ONCE;
    rmenu.query();

    if( rmenu.ret >= REPEAT_INIT && rmenu.ret <= REPEAT_EVENT ) {
        return static_cast<repeat_type>( rmenu.ret );
    }

    return REPEAT_CANCEL;
}

static item *get_fake_tool_for_repair( hack_type_t hack_type,
                                       const tripoint_bub_ms &position,
                                       const itype_id &tool_type,
                                       const tripoint_abs_ms &abs_pos )
{
    const map &m = get_map();
    item *fake_item = &null_item_reference();

    switch( hack_type ) {
        case hack_type_t::vehicle: {
            const optional_vpart_position pos = m.veh_at( position );
            if( !pos ) {
                debugmsg( "Failed to find vehicle while using it for repair at %s", position.to_string() );
                return fake_item;
            }
            const vehicle &veh = pos->vehicle();
            fake_item = item::spawn_temporary( tool_type, calendar::turn, 0 );
            fake_item->charges = veh.fuel_left( itype_battery );
            break;
        }
        case hack_type_t::furniture: {
            if( !m.has_furn( position ) ) {
                debugmsg( "Failed to find furniture while using it for repair at %s", position.to_string() );
                return fake_item;
            }
            const furn_t &furniture = m.furn( position ).obj();
            const std::vector<itype> item_type_list = furniture.crafting_pseudo_item_types();
            if( item_type_list.empty() ) {
                return fake_item;
            }
            for( const itype &item_type : item_type_list ) {
                if( item_type.get_id() == tool_type ) {
                    const distribution_grid &grid = get_distribution_grid_tracker().grid_at( abs_pos );
                    fake_item = item::spawn_temporary( item_type.get_id(), calendar::turn, 0 );
                    fake_item->charges = grid.get_resource( true );
                    break;
                }
            }
            break;
        }
    }

    fake_item->set_flag( flag_PSEUDO );
    return fake_item;
}

static void discharge_real_power_for_repair( hack_type_t hack_type,
        const tripoint_bub_ms &position,
        item &tool, int original_charges )
{
    const int used_charges = original_charges - tool.charges;
    if( used_charges <= 0 ) {
        return;
    }
    const map &m = get_map();
    int unfulfilled_demand = 0;
    switch( hack_type ) {
        case hack_type_t::vehicle: {
            optional_vpart_position pos = m.veh_at( position );
            if( !pos ) {
                return;
            }
            vehicle &veh = pos->vehicle();
            unfulfilled_demand = veh.discharge_battery( used_charges );
            break;
        }
        case hack_type_t::furniture: {
            const auto abspos = bub_to_abs( position );
            distribution_grid &grid = get_distribution_grid_tracker().grid_at( abspos );
            unfulfilled_demand = grid.mod_resource( -used_charges );
            break;
        }
    }
    if( unfulfilled_demand != 0 ) {
        debugmsg(
            "Fake tool discharged grid/veh more than grid/veh had!  Unfulfilled demand %d kJ",
            unfulfilled_demand
        );
    }
}

// ─── repair_actor method implementations ────────────────────────────────────

void repair_actor::start( player_activity &, Character & ) {}
void repair_actor::do_turn( player_activity &act, Character &who )
{
    if( activity_actor::progress.invalid() ) {
        finish( act, who );
        return;
    }
    auto &p = static_cast<player &>( who );
    const float vision_mod = character_funcs::fine_detail_vision_mod( who );
    const auto effective_moves = static_cast<int>(
                                     action_time_scale::activity_progress_from_actor_moves( who ) / vision_mod );
    if( effective_moves <= activity_actor::progress.get_moves_left() ) {
        activity_actor::progress.mod_moves_left( -effective_moves );
        who.moves = 0;
    } else {
        who.moves -= action_time_scale::actor_moves_for_activity_progress( who,
                     activity_actor::progress.get_moves_left() * vision_mod );
        activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
    }
}
void repair_actor::finish( player_activity &act, Character &who )
{
    // Determine if this is a hack path (vehicle/furniture) or core path (inventory tool)
    const std::string iuse_name_string = this->iuse_name.empty() ? "repair_item" : this->iuse_name;
    repeat_type repeat = static_cast<repeat_type>( this->repeat );

    // nullopt if using real tool (core path)
    std::optional<hack_type_t> hack_type;
    if( is_hack ) {
        hack_type = this->hack_type;
    }
    item *fake_tool = nullptr;
    item *ploc = nullptr;

    if( hack_type ) {
        fake_tool = get_fake_tool_for_repair( hack_type.value(), abs_to_bub( this->target_pos ),
                                              this->tool_type, this->target_pos );
    } else if( this->tool ) {
        ploc = &*this->tool;
    }
    const auto hack_position = hack_type ? abs_to_bub( this->target_pos ) : tripoint_bub_ms{};
    const int hack_original_charges = fake_tool ? fake_tool->charges : 0;

    item *main_tool = nullptr;
    if( hack_type.has_value() ) {
        main_tool = fake_tool;
    }
    if( main_tool == nullptr && ploc ) {
        main_tool = ploc;
    }
    if( main_tool == nullptr ) {
        main_tool = &who.i_at( this->item_pos );
    }
    if( main_tool == nullptr ) {
        debugmsg( "Failed to get main_tool for long repair" );
        act.set_to_null();
        return;
    }

    item *used_tool = main_tool->get_usable_item( iuse_name_string );
    if( used_tool == nullptr ) {
        debugmsg( "Lost tool used for long repair" );
        act.set_to_null();
        return;
    }

    const use_function *use_fun = used_tool->get_use( iuse_name_string );
    if( use_fun == nullptr ) {
        debugmsg( "Lost repair use function" );
        act.set_to_null();
        return;
    }
    const repair_item_actor *repair_iuse = dynamic_cast<const repair_item_actor *>
                                           ( use_fun->get_actor_ptr() );
    if( repair_iuse == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act.set_to_null();
        return;
    }

    // Valid Repeat choice and target, attempt repair.
    if( repeat != REPEAT_INIT && this->fix_target ) {
        item &fix_location = *this->fix_target;

        // Remember our level: we want to stop retrying on level up
        const int old_level = who.get_skill_level( repair_iuse->used_skill );
        const repair_item_actor::attempt_hint attempt = repair_iuse->repair( who, *used_tool,
                fix_location );
        if( attempt != repair_item_actor::AS_CANT ) {
            if( ploc && ploc->where() == item_location_type::map ) {
                used_tool->ammo_consume( used_tool->ammo_required() );
            } else {
                who.consume_charges( *used_tool, used_tool->ammo_required() );
            }
            if( hack_type.has_value() ) {
                discharge_real_power_for_repair(
                    hack_type.value(),
                    hack_position,
                    *used_tool,
                    hack_original_charges
                );
            }
        }

        // TODO: Allow setting this in the actor
        // TODO: Don't use charges_to_use: welder has 50 charges per use, soldering iron has 1
        if( !used_tool->units_sufficient( who ) ) {
            who.add_msg_if_player( _( "Your %s ran out of charges" ), used_tool->tname() );
            act.set_to_null();
            return;
        }

        // Print message explaining why we stopped
        // But only if we didn't destroy the item (because then it's obvious)
        const bool destroyed = attempt == repair_item_actor::AS_DESTROYED;
        const bool cannot_continue_repair = attempt == repair_item_actor::AS_CANT ||
                                            destroyed || !repair_iuse->can_repair_target( who, fix_location, !destroyed );
        if( cannot_continue_repair ) {
            // Cannot continue to repair target, select another target.
            // **Warning**: as soon as the item is popped, it is destroyed and can't be used anymore!
            this->fix_target = safe_reference<item>();
        }

        const bool event_happened = attempt == repair_item_actor::AS_FAILURE ||
                                    attempt == repair_item_actor::AS_SUCCESS ||
                                    old_level != who.get_skill_level( repair_iuse->used_skill );

        const bool need_input =
            ( repeat == REPEAT_ONCE ) ||
            ( repeat == REPEAT_EVENT && event_happened ) ||
            ( repeat == REPEAT_FULL && ( cannot_continue_repair || fix_location.damage() <= 0 ) );
        if( need_input ) {
            repeat = REPEAT_INIT;
        }
    }

    // Check tool is valid before we query target and Repeat choice.
    if( !repair_iuse->can_use_tool( who, *used_tool, true ) ) {
        act.set_to_null();
        return;
    }

    // target selection and validation.
    while( !this->fix_target ) {
        if( !who.is_player() ) {
            act.set_to_null();
            return;
        }
        item *item_loc = game_menus::inv::repair( *who.as_player(), repair_iuse, main_tool );

        if( item_loc == nullptr ) {
            who.add_msg_if_player( m_info, _( "Never mind." ) );
            act.set_to_null();
            return;
        }
        if( repair_iuse->can_repair_target( who, *item_loc, true ) ) {
            this->fix_target = safe_reference<item>( item_loc );
            repeat = REPEAT_INIT;
        }
    }

    item &fix = *this->fix_target;

    if( repeat == REPEAT_INIT ) {
        const int level = who.get_skill_level( repair_iuse->used_skill );
        repair_item_actor::repair_type action_type = repair_iuse->default_action( fix, level );
        if( action_type == repair_item_actor::RT_NOTHING ) {
            who.add_msg_if_player( _( "You won't learn anything more by doing that." ) );
        }

        const std::pair<float, float> chance = repair_iuse->repair_chance( who, fix, action_type );
        if( chance.first <= 0.0f ) {
            action_type = repair_item_actor::RT_PRACTICE;
        }

        int items_needed = repair_iuse->get_material_amt_needed( fix, true );
        const auto valid_materials = repair_iuse->get_valid_materials( fix );
        const auto &crafting_inv = who.crafting_inventory();
        auto listed_components = std::set<itype_id> {};
        auto material_list = std::vector<std::string> {};

        for( const auto &entry : valid_materials ) {
            const auto &component_id = entry.obj().repaired_with();
            if( listed_components.find( component_id ) != listed_components.end() ) {
                continue;
            }
            listed_components.emplace( component_id );
            int nearby_amount = 0;
            if( item::count_by_charges( component_id ) ) {
                if( crafting_inv.has_charges( component_id, 1 ) ) {
                    nearby_amount = crafting_inv.charges_of( component_id );
                }
            } else if( crafting_inv.has_amount( component_id, 1, false, is_crafting_component ) ) {
                nearby_amount = crafting_inv.amount_of( component_id, false );
            }
            std::string color = nearby_amount < items_needed ? "red" : "light_blue";
            material_list.emplace_back( string_format( _( "%s (<color_%s>%d</color>)" ),
                                        item::nname( component_id ), color, nearby_amount ) );
        }
        std::string material_list_string = join( material_list, ", " );

        std::string title = string_format( _( "%s %s\n" ),
                                           repair_item_actor::action_description( action_type ),
                                           fix.tname() );
        title += string_format( _( "Charges: <color_light_blue>%s/%s</color> %s (%s per use)\n" ),
                                used_tool->ammo_remaining(), used_tool->ammo_capacity(),
                                item::nname( used_tool->ammo_current() ),
                                used_tool->ammo_required() );
        title += string_format( _( "Materials available: %s\n" ),
                                material_list_string );
        title += string_format( _( "Materials needed: <color_light_blue>%d</color>\n" ),
                                items_needed );
        title += string_format( _( "Skill used: <color_light_blue>%s (%s)</color>\n" ),
                                repair_iuse->used_skill->name(), level );
        title += string_format( _( "Success chance: <color_light_blue>%.1f</color>%%\n" ),
                                100.0f * chance.first );
        title += string_format( _( "Damage chance: <color_light_blue>%.1f</color>%%" ),
                                100.0f * chance.second );

        do {
            repeat = repeat_menu( title, repeat );

            if( repeat == REPEAT_CANCEL ) {
                act.set_to_null();
                return;
            }
            this->repeat = static_cast<int>( repeat );
            // BACK selected, redo target selection next.
            if( repeat == REPEAT_INIT ) {
                this->fix_target = safe_reference<item>();
                return;
            }
            if( repeat == REPEAT_FULL && fix.damage() <= 0 ) {
                who.add_msg_if_player( m_info, _( "Your %s is already fully repaired." ), fix.tname() );
                repeat = REPEAT_INIT;
            }
        } while( repeat == REPEAT_INIT );
    }

    // Otherwise keep retrying.  The next repair is an actor-owned task; the
    // legacy activity countdown is intentionally not used here.
    if( !activity_actor::progress.invalid() && activity_actor::progress.complete() ) {
        activity_actor::progress.pop();
    }
    activity_actor::progress.emplace( "repairing", repair_iuse->move_cost );
}

// ─────────────────────────────────────────────────────────────────────────────
// wear_actor (ACT_WEAR)
// ─────────────────────────────────────────────────────────────────────────────

wear_actor::wear_actor( std::vector<wear_target> targets )
    : to_wear( std::move( targets ) )
{}

void wear_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "to_wear" );
    jsout.start_array();
    for( const auto &wt : to_wear ) {
        jsout.start_object();
        jsout.member( "item", wt.item_ref );
        jsout.member( "quantity", wt.quantity );
        jsout.end_object();
    }
    jsout.end_array();
    jsout.end_object();
}

std::unique_ptr<activity_actor> wear_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<wear_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );

    JsonArray arr = data.get_array( "to_wear" );
    for( JsonObject wt : arr ) {
        wear_actor::wear_target target;
        wt.read( "item", target.item_ref );
        wt.read( "quantity", target.quantity );
        actor->to_wear.push_back( std::move( target ) );
    }

    return actor;
}

std::unique_ptr<activity_actor> wear_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<wear_actor>();

    auto targets_vec = std::vector<safe_reference<item>>();
    data.read( "targets", targets_vec );

    auto values = data.get_int_array( "values" );

    size_t count = std::min( targets_vec.size(), values.size() );
    for( size_t i = 0; i < count; i++ ) {
        actor->to_wear.push_back( {
            .item_ref = std::move( targets_vec[i] ),
            .quantity = values[i]
        } );
    }

    restore_legacy_progress( *actor, data, "wearing" );
    return actor;
}

void wear_actor::start( player_activity &act, Character &who ) {}
void wear_actor::do_turn( player_activity &act, Character &who )
{


    while( who.moves > 0 && !to_wear.empty() ) {
        wear_target wt = std::move( to_wear.back() );
        to_wear.pop_back();

        if( !wt.item_ref ) {
            debugmsg( "Lost target item of ACT_WEAR" );
            continue;
        }
        ret_val<bool> ret = who.can_wear( *wt.item_ref );
        if( ret.success() && ret.value() ) {
            detached_ptr<item> newit = wt.item_ref->split( wt.quantity );
            who.wear_item( std::move( newit ) );
        }
    }

    // If there are no items left we are done
    if( to_wear.empty() ) {
        who.cancel_activity();
    }
}
void wear_actor::finish( player_activity &act, Character &who ) {}

// ─────────────────────────────────────────────────────────────────────────────
// wait_stamina_actor (ACT_WAIT_STAMINA)
// ─────────────────────────────────────────────────────────────────────────────

wait_stamina_actor::wait_stamina_actor( int threshold )
    : stamina_threshold( threshold )
{}

void wait_stamina_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "stamina_threshold", stamina_threshold );
    jsout.member( "stamina_initial", stamina_initial );
    jsout.end_object();
}

std::unique_ptr<activity_actor> wait_stamina_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<wait_stamina_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "stamina_threshold", actor->stamina_threshold );
    data.read( "stamina_initial", actor->stamina_initial );
    return actor;
}

std::unique_ptr<activity_actor> wait_stamina_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<wait_stamina_actor>();

    auto values = data.get_int_array( "values" );
    if( !values.empty() ) {
        actor->stamina_threshold = values[0];
    }
    if( values.size() >= 2 ) {
        actor->stamina_initial = values[1];
    }

    return actor;
}

void wait_stamina_actor::start( player_activity &, Character & )
{
    if( progress.invalid() ) {
        progress.dummy();
    }
}

void wait_stamina_actor::do_turn( player_activity &act, Character &who )
{

    // If no explicit threshold, wait until full stamina
    int effective_threshold = stamina_threshold > 0 ? stamina_threshold : who.get_stamina_max();
    // Remember initial stamina on first tick
    if( stamina_initial < 0 ) {
        stamina_initial = who.get_stamina();
    }
    if( who.get_stamina() >= effective_threshold ) {
        finish( act, who );
    }
}

void wait_stamina_actor::finish( player_activity &act, Character &who )
{

    if( stamina_threshold > 0 ) {
        if( who.get_stamina() < stamina_threshold && who.get_stamina() <= stamina_initial ) {
            debugmsg( "Failed to wait until stamina threshold %d reached, only at %d. You may not be regaining stamina.",
                      stamina_threshold, who.get_stamina() );
        }
    } else if( who.get_stamina() < who.get_stamina_max() ) {
        who.add_msg_if_player( _( "You are bored of waiting, so you stop." ) );
    } else {
        who.add_msg_if_player( _( "You finish waiting and feel refreshed." ) );
    }
    act.set_to_null();
}

// ─────────────────────────────────────────────────────────────────────────────
// hand_crank_charge_actor (ACT_HAND_CRANK)
// ─────────────────────────────────────────────────────────────────────────────

hand_crank_charge_actor::hand_crank_charge_actor(
    int interval_turns,
    int charges,
    int fatigue,
    const itype_id &ammo
) : charge_interval_turns( interval_turns )
    , charge_amount( charges )
    , fatigue_amount( fatigue )
    , ammo_type( ammo )
{}

void hand_crank_charge_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "charge_interval_turns", charge_interval_turns );
    jsout.member( "charge_amount", charge_amount );
    jsout.member( "fatigue_amount", fatigue_amount );
    jsout.member( "ammo_type", ammo_type );
    jsout.end_object();
}

std::unique_ptr<activity_actor> hand_crank_charge_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<hand_crank_charge_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "charge_interval_turns", actor->charge_interval_turns );
    data.read( "charge_amount", actor->charge_amount );
    data.read( "fatigue_amount", actor->fatigue_amount );
    data.read( "ammo_type", actor->ammo_type );
    return actor;
}

std::unique_ptr<activity_actor> hand_crank_charge_actor::legacy_deserialize(
    const JsonObject &data )
{
    auto actor = std::make_unique<hand_crank_charge_actor>();

    auto values = data.get_int_array( "values" );
    if( values.size() >= 1 ) {
        actor->charge_interval_turns = values[0];
    }
    if( values.size() >= 2 ) {
        actor->charge_amount = std::max( 1, values[1] );
    }
    if( values.size() >= 3 ) {
        actor->fatigue_amount = std::max( 0, values[2] );
    }

    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() && !str_values[0].empty() ) {
        actor->ammo_type = itype_id( str_values[0] );
    }

    return actor;
}

void hand_crank_charge_actor::start( player_activity &act, Character &who ) {}

void hand_crank_charge_actor::do_turn( player_activity &act, Character &who )
{

    // Hand-crank chargers: time-based, not speed-based
    if( act.get_tools().empty() || !act.get_tools().front() ) {
        debugmsg( "Hand-crank activity lost its tool" );
        act.set_to_null();
        return;
    }
    auto &hand_crank_item = *act.get_tools().front();

    auto charge_interval = charge_interval_turns > 0
                           ? time_duration::from_turns( charge_interval_turns )
                           : 144_seconds;

    if( action_time_scale::once_every_this_tick( charge_interval ) ) {
        who.mod_fatigue( fatigue_amount );
        if( hand_crank_item.ammo_capacity() > hand_crank_item.ammo_remaining() ) {
            const auto current = hand_crank_item.ammo_remaining();
            const auto capacity = hand_crank_item.ammo_capacity();
            const auto next_charges = std::min( capacity, current + charge_amount );
            hand_crank_item.ammo_set( ammo_type, next_charges );
            if( next_charges >= capacity ) {
                activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
                add_msg( m_info, _( "You've charged the battery completely." ) );
            }
        } else {
            activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
            add_msg( m_info, _( "You've charged the battery completely." ) );
        }
    }
    if( who.get_fatigue() >= fatigue_levels::dead_tired ) {
        activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
        add_msg( m_info, _( "You're too exhausted to keep cranking." ) );
    }
}

void hand_crank_charge_actor::finish( player_activity &act, Character & )
{
    act.set_to_null();
}

// ─────────────────────────────────────────────────────────────────────────────
// wait_npc_actor (ACT_WAIT_NPC)
// ─────────────────────────────────────────────────────────────────────────────

wait_npc_actor::wait_npc_actor( const std::string &name )
    : npc_name( name )
{}

void wait_npc_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "npc_name", npc_name );
    jsout.end_object();
}

std::unique_ptr<activity_actor> wait_npc_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<wait_npc_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "npc_name", actor->npc_name );
    return actor;
}

std::unique_ptr<activity_actor> wait_npc_actor::legacy_deserialize( const JsonObject &data )
{
    auto str_values = data.get_string_array( "str_values" );
    if( str_values.empty() ) {
        return nullptr;
    }
    auto actor = std::make_unique<wait_npc_actor>( str_values[0] );
    restore_legacy_progress( *actor, data, "waiting for NPC" );
    return actor;
}

void wait_npc_actor::start( player_activity &act, Character &who ) {}
void wait_npc_actor::do_turn( player_activity &act, Character &who ) {}
void wait_npc_actor::finish( player_activity &act, Character &who )
{

    who.add_msg_if_player( _( "%s finishes with you…" ), npc_name );
    act.set_to_null();
}

// ─────────────────────────────────────────────────────────────────────────────
// clear_rubble_actor (ACT_CLEAR_RUBBLE)
// ─────────────────────────────────────────────────────────────────────────────

clear_rubble_actor::clear_rubble_actor( const tripoint_abs_ms &pos )
    : rubble_pos( pos )
{}

void clear_rubble_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "rubble_pos", rubble_pos );
    jsout.end_object();
}

std::unique_ptr<activity_actor> clear_rubble_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<clear_rubble_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "rubble_pos", actor->rubble_pos );
    return actor;
}

std::unique_ptr<activity_actor> clear_rubble_actor::legacy_deserialize( const JsonObject &data )
{
    auto coords = std::vector<tripoint_abs_ms>();
    data.read( "coords", coords );
    if( coords.empty() ) {
        return nullptr;
    }
    auto actor = std::make_unique<clear_rubble_actor>( coords[0] );
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 ) {
        actor->progress.emplace( "clearing rubble", moves_total,
                                 std::max( 0, data.get_int( "moves_left", moves_total ) ) );
    }
    return actor;
}

void clear_rubble_actor::start( player_activity &act, Character &who ) {}
void clear_rubble_actor::do_turn( player_activity &act, Character &who ) {}
void clear_rubble_actor::finish( player_activity &act, Character &who )
{

    mapbuffer &here = who.get_mapbuffer();
    const auto furniture = here.furn( rubble_pos );
    if( !furniture ) {
        act.set_to_null();
        return;
    }
    const map_bash_info &bash = furniture->obj().bash;
    who.add_msg_if_player( m_info, _( "You clear up the %s." ),
                           here.furnname( rubble_pos ) );
    here.spawn_items( rubble_pos, item_group::items_from( bash.drop_group, calendar::turn ) );
    here.set_furn( rubble_pos, f_null );
    act.set_to_null();
}

// ─────────────────────────────────────────────────────────────────────────────
// read_activity_actor (ACT_READ)
// ─────────────────────────────────────────────────────────────────────────────

read_activity_actor::read_activity_actor(
    safe_reference<item> book_ref,
    std::vector<npc_learner> npcs,
    bool martial_arts,
    int total_moves_
) : book( std::move( book_ref ) )
    , learners( std::move( npcs ) )
    , is_martial_arts( martial_arts )
    , total_moves( total_moves_ )
{}

void read_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "book", book );
    jsout.member( "is_martial_arts", is_martial_arts );
    jsout.member( "stamina_at_start", stamina_at_start );
    jsout.member( "total_moves", total_moves );
    jsout.member( "continuous_reader_id", continuous_reader_id );
    jsout.member( "learners" );
    jsout.start_array();
    for( const auto &l : learners ) {
        jsout.start_object();
        jsout.member( "id", l.id );
        jsout.member( "penalty", l.penalty );
        jsout.end_object();
    }
    jsout.end_array();
    jsout.end_object();
}

std::unique_ptr<activity_actor> read_activity_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<read_activity_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "book", actor->book );
    data.read( "is_martial_arts", actor->is_martial_arts );
    data.read( "stamina_at_start", actor->stamina_at_start );
    data.read( "total_moves", actor->total_moves );
    data.read( "continuous_reader_id", actor->continuous_reader_id );

    JsonArray arr = data.get_array( "learners" );
    for( JsonObject lobj : arr ) {
        npc_learner l;
        lobj.read( "id", l.id );
        lobj.read( "penalty", l.penalty );
        actor->learners.push_back( l );
    }

    return actor;
}

std::unique_ptr<activity_actor> read_activity_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<read_activity_actor>();

    data.read( "index", actor->continuous_reader_id );

    // Check for martial arts flag first
    auto str_values = data.get_string_array( "str_values" );
    if( str_values.size() == 1 && str_values[0] == "martial_art" ) {
        actor->is_martial_arts = true;
        auto values = data.get_int_array( "values" );
        if( !values.empty() ) {
            actor->stamina_at_start = values[0];
        }
    } else {
        // Read NPC learners from values[] and str_values[]
        auto values = data.get_int_array( "values" );
        size_t count = std::min( values.size(), str_values.size() );
        for( size_t i = 0; i < count; i++ ) {
            npc_learner l;
            l.id = character_id( values[i] );
            l.penalty = std::stof( str_values[i] );
            actor->learners.push_back( l );
        }
    }

    // Book from targets[0]
    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    if( !targets.empty() ) {
        actor->book = std::move( targets[0] );
    }

    return actor;
}

void read_activity_actor::start( player_activity &act, Character &who )
{
    ( void )act;
    ( void )who;
    if( total_moves > 0 ) {
        progress.emplace( "reading", total_moves );
    } else {
        progress.dummy();
    }
}

void read_activity_actor::do_turn( player_activity &act, Character &who )
{
    if( who.is_player() ) {
        if( is_martial_arts && one_in( 3 ) ) {
            if( stamina_at_start == 0 ) {
                stamina_at_start = who.get_stamina();
            }
            who.set_stamina( stamina_at_start - 1 );
            stamina_at_start = who.get_stamina();
        }
    } else {
        who.moves = 0;
    }

    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        if( !book || !book->is_book() ) {
            who.add_msg_if_player( m_bad, _( "You lost your book!  You stop reading." ) );
            act.set_to_null();
        }
    }
}

auto read_activity_actor::get_progress_message(
    const player_activity &, const Character &who ) const -> act_progress_message
{
    if( !book || !book->type->book ) {
        return act_progress_message::make_empty();
    }

    const auto &reading = book->type->book;
    const auto &skill = reading->skill;
    if( skill && who.get_skill_level( skill ) < reading->level &&
        who.get_skill_level_object( skill ).can_train() ) {
        const auto &skill_level = who.get_skill_level_object( skill );
        return act_progress_message::make_extra_info( string_format(
                    pgettext( "reading progress", "%s %d -> %d (%d%%)" ),
                    skill->name(), skill_level.level(), skill_level.level() + 1,
                    skill_level.exercise() ) );
    }

    return act_progress_message::make_empty();
}

void read_activity_actor::finish( player_activity &act, Character &who )
{
    if( !act || !book ) {
        debugmsg( "Lost target of ACT_READ" );
        if( act ) {
            act.set_to_null();
        }
        return;
    }

    if( who.is_avatar() ) {
        // Build learners vector for do_read from typed actor fields
        std::vector<std::pair<character_id, float>> learner_data;
        for( const auto &l : learners ) {
            learner_data.emplace_back( l.id, l.penalty );
        }
        g->u.do_read( &*book, learner_data, continuous_reader_id );
    } else if( who.is_npc() ) {
        npc *guy = who.as_npc();
        guy->finish_read( &*book );
    } else {
        act.set_to_null();
        who.add_msg_if_player( m_info, _( "You finish reading." ) );
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// move_loot_activity_actor (ACT_MOVE_LOOT)
// ─────────────────────────────────────────────────────────────────────────────

move_loot_activity_actor::move_loot_activity_actor(
    int processed,
    int init_stage,
    const std::unordered_set<tripoint_abs_ms> &zpoints
) : items_processed( processed )
    , stage( init_stage )
    , zone_points( zpoints )
{}

void move_loot_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "items_processed", items_processed );
    jsout.member( "stage", stage );
    jsout.member( "current_src", current_src );
    jsout.member( "zone_points", zone_points );
    jsout.end_object();
}

std::unique_ptr<activity_actor> move_loot_activity_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<move_loot_activity_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "items_processed", actor->items_processed );
    data.read( "stage", actor->stage );
    data.read( "current_src", actor->current_src );
    data.read( "zone_points", actor->zone_points );
    return actor;
}

std::unique_ptr<activity_actor> move_loot_activity_actor::legacy_deserialize(
    const JsonObject &data )
{
    auto actor = std::make_unique<move_loot_activity_actor>();

    auto values = data.get_int_array( "values" );
    if( !values.empty() ) {
        actor->items_processed = values[0];
    }
    actor->stage = data.get_int( "index", 0 );

    data.read( "coord_set", actor->zone_points );

    return actor;
}

void move_loot_activity_actor::start( player_activity &act, Character &who ) {}
void move_loot_activity_actor::do_turn( player_activity &act, Character &who )
{
    enum activity_stage : int {
        //Initial stage
        INIT = 0,
        //Think about what to do first: choose destination
        THINK,
        //Do activity
        DO,
    };



    //Prepare activity stage
    if( stage < 0 ) {
        stage = INIT;
        items_processed = 0;
    }

    map &here = get_map();
    const auto abspos = who.abs_pos();
    auto &mgr = zone_manager::get_manager();
    if( here.check_vehicle_zones( g->get_levz() ) ) {
        mgr.cache_vzones();
    }

    if( stage == INIT ) {
        zone_points = mgr.get_near( zone_type_LOOT_UNSORTED, abspos, ACTIVITY_SEARCH_DISTANCE );
        stage = THINK;
    }

    if( stage == THINK ) {
        //initialize num_processed
        items_processed = 0;
        // sort source tiles by distance
        const auto &src_sorted = get_sorted_tiles_by_distance( abspos, zone_points );

        for( auto &src : src_sorted ) {
            current_src = src;
            zone_points.erase( src );

            const auto src_loc = abs_to_bub( src );
            if( !here.inbounds( src_loc ) ) {
                if( !here.inbounds( who.bub_pos() ) ) {
                    // who is implicitly an NPC that has been moved off the map, so reset the activity
                    // and unload them
                    who.cancel_activity();
                    who.assign_activity( std::make_unique<player_activity>(
                                             std::make_unique<move_loot_activity_actor>() ) );
                    who.set_moves( 0 );
                    g->reload_npcs();
                    return;
                }
                auto &pf_buffer = MAPBUFFER_REGISTRY.get( who.get_dimension() );
                const auto pair = who.get_pathfinding_pair();
                auto route = Pathfinding::route( pf_buffer, who.abs_pos(), bub_to_abs( src_loc ),
                                                 pair.first, pair.second );
                stage = DO;
                who.set_destination( route, who.remove_activity() );
                return;
            }

            // skip tiles in IGNORE zone and tiles on fire
            // (to prevent taking out wood off the lit brazier)
            // and inaccessible furniture, like filled charcoal kiln
            if( mgr.has( zone_type_LOOT_IGNORE, src ) ||
                here.get_field( src_loc, fd_fire ) != nullptr ||
                !here.can_put_items_ter_furn( src_loc ) ) {
                continue;
            }

            //nothing to sort?
            const std::optional<vpart_reference> vp = here.veh_at( src_loc ).part_with_feature( "CARGO",
                    false );
            if( ( !vp || vp->vehicle().get_items( vp->part_index() ).empty() )
                && here.i_at( src_loc ).empty() ) {
                continue;
            }

            bool is_adjacent_or_closer = square_dist( who.bub_pos(), src_loc ) <= 1;
            // before we move any item, check if player is at or
            // adjacent to the loot source tile
            if( !is_adjacent_or_closer ) {
                std::vector<tripoint_abs_ms> route;
                bool adjacent = false;

                // get either direct route or route to nearest adjacent tile if
                // source tile is impassable
                if( here.passable( src_loc ) ) {
                    auto &pf_buffer2 = MAPBUFFER_REGISTRY.get( who.get_dimension() );
                    const auto pair2 = who.get_pathfinding_pair();
                    route = Pathfinding::route( pf_buffer2, who.abs_pos(), bub_to_abs( src_loc ),
                                                pair2.first, pair2.second );
                } else {
                    // impassable source tile (locker etc.),
                    // get route to nearest adjacent tile instead
                    route = route_adjacent( who, bub_to_abs( src_loc ) );
                    adjacent = true;
                }

                // check if we found path to source / adjacent tile
                if( route.empty() ) {
                    add_msg( m_info, _( "%s can't reach the source tile.  Try to sort out loot without a cart." ),
                             who.disp_name() );
                    continue;
                }

                // shorten the route to adjacent tile, if necessary
                if( !adjacent ) {
                    route.pop_back();
                }

                // set the destination and restart activity after player arrives there
                // we don't need to check for safe mode,
                // activity will be restarted only if
                // player arrives on destination tile
                stage = DO;
                who.set_destination( route, who.remove_activity() );
                return;
            }
            stage = DO;
            break;
        }
    }
    if( stage == DO ) {
        const auto &src = current_src;
        const auto src_loc = abs_to_bub( src );

        bool is_adjacent_or_closer = square_dist( who.bub_pos(), src_loc ) <= 1;
        // before we move any item, check if player is at or
        // adjacent to the loot source tile
        if( !is_adjacent_or_closer ) {
            stage = THINK;
            return;
        }

        // the boolean in this pair being true indicates the item is from a vehicle storage space
        auto items = std::vector<std::pair<item *, bool>>();
        vehicle *src_veh, *dest_veh;
        int src_part, dest_part;

        //Check source for cargo part
        //map_stack and vehicle_stack are different types but inherit from item_stack
        // TODO: use one for loop
        if( const std::optional<vpart_reference> vp = here.veh_at( src_loc ).part_with_feature( "CARGO",
                false ) ) {
            src_veh = &vp->vehicle();
            src_part = vp->part_index();
            for( auto &it : src_veh->get_items( src_part ) ) {
                if( !it->is_owned_by( who, true ) && it->get_owner()->likes_u >= -10 ) {
                    continue;
                }
                it->set_owner( who );
                items.emplace_back( it, true );
            }
        } else {
            src_veh = nullptr;
            src_part = -1;
        }
        for( auto &it : here.i_at( src_loc ) ) {
            if( !it->is_owned_by( who, true ) && it->get_owner()->likes_u >= -10 ) {
                continue;
            }
            it->set_owner( who );
            items.emplace_back( it, false );
        }

        //Skip items that have already been processed
        for( auto it = items.begin() + items_processed; it < items.end(); ++it ) {
            ++items_processed;
            item &thisitem = *it->first;

            // skip unpickable liquid
            if( thisitem.made_of( LIQUID ) ) {
                continue;
            }

            // skip favorite items in ignore favorite zones
            if( thisitem.is_favorite && mgr.has( zone_type_LOOT_IGNORE_FAVORITES, src ) ) {
                continue;
            }

            const zone_type_id id = mgr.get_near_zone_type_for_item( thisitem, abspos,
                                    ACTIVITY_SEARCH_DISTANCE );

            // checks whether the item is already on correct loot zone or not
            // if it is, we can skip such item, if not we move the item to correct pile
            // think empty bag on food pile, after you ate the content
            if( mgr.has( id, src ) ) {
                continue;
            }

            const std::unordered_set<tripoint_abs_ms> &dest_set = mgr.get_near( id, abspos,
                    ACTIVITY_SEARCH_DISTANCE,
                    &thisitem );
            for( const auto &dest : dest_set ) {
                const auto dest_loc = abs_to_bub( dest );

                //Check destination for cargo part
                if( const std::optional<vpart_reference> vp = here.veh_at( dest_loc ).part_with_feature( "CARGO",
                        false ) ) {
                    dest_veh = &vp->vehicle();
                    dest_part = vp->part_index();
                } else {
                    dest_veh = nullptr;
                    dest_part = -1;
                }

                // skip tiles with inaccessible furniture, like filled charcoal kiln
                if( !here.can_put_items_ter_furn( dest_loc ) ||
                    static_cast<int>( here.i_at( dest_loc ).size() ) >= MAX_ITEM_IN_SQUARE ) {
                    continue;
                }

                units::volume free_space;
                // if there's a vehicle with space do not check the tile beneath
                if( dest_veh ) {
                    free_space = dest_veh->free_volume( dest_part );
                } else {
                    free_space = here.free_volume( dest_loc );
                }
                // check free space at destination
                if( free_space >= thisitem.volume() ) {
                    move_item( who, thisitem, thisitem.count(), src_loc, dest_loc );

                    // moved item away from source so decrement
                    if( items_processed > 0 ) {
                        --items_processed;
                    }
                    break;
                }
            }
            if( who.moves <= 0 ) {
                return;
            }
        }

        //this location is sorted
        stage = THINK;
        return;
    }

    // If we got here without restarting the activity, it means we're done
    add_msg( m_info, _( "%s sorted out every item possible." ), who.disp_name( false, true ) );
    if( who.is_npc() ) {
        who.as_npc()->revert_after_activity();
    }
    who.activity->set_to_null();
}
void move_loot_activity_actor::finish( player_activity &act, Character &who ) {}

// ─────────────────────────────────────────────────────────────────────────────
// fetch_required_actor (ACT_FETCH_REQUIRED)
// ─────────────────────────────────────────────────────────────────────────────

fetch_required_actor::fetch_required_actor(
    do_activity_reason reason,
    const requirement_data &reqs,
    const tripoint_abs_ms &placement,
    const tripoint_abs_ms &source_zone
) : reason( reason )
    , fetch_requirements( reqs )
    , placement_pos( placement )
    , source_zone_pos( source_zone )
{}

void fetch_required_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "reason", static_cast<int>( reason ) );
    jsout.member( "fetch_requirements", fetch_requirements );
    jsout.member( "placement_pos", placement_pos );
    jsout.member( "source_zone_pos", source_zone_pos );
    jsout.end_object();
}

std::unique_ptr<activity_actor> fetch_required_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<fetch_required_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );

    int r = 0;
    data.read( "reason", r );
    actor->reason = static_cast<do_activity_reason>( r );

    data.read( "fetch_requirements", actor->fetch_requirements );
    data.read( "placement_pos", actor->placement_pos );
    data.read( "source_zone_pos", actor->source_zone_pos );
    return actor;
}

std::unique_ptr<activity_actor> fetch_required_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<fetch_required_actor>();

    auto values = data.get_int_array( "values" );
    if( !values.empty() ) {
        actor->reason = static_cast<do_activity_reason>( values[0] );
    }

    // requirement string from str_values[0]
    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() ) {
        requirement_id req_id( str_values[0] );
        if( req_id.is_valid() ) {
            actor->fetch_requirements = req_id.obj();
        }
    }

    // placement_pos from coords[0]
    auto coords = std::vector<tripoint_abs_ms>();
    data.read( "coords", coords );
    if( !coords.empty() ) {
        actor->placement_pos = coords[0];
    }

    // source_zone_pos from placement
    tripoint_abs_ms pl;
    data.read( "placement", pl );
    actor->source_zone_pos = pl;

    return actor;
}

void fetch_required_actor::start( player_activity &act, Character &who ) {}
void fetch_required_actor::do_turn( player_activity &act, Character &who )
{
    generic_multi_activity_handler( act, static_cast<player &>( who ) );
}
void fetch_required_actor::finish( player_activity &act, Character &who ) {}

// ─── tree_communion_actor ────────────────────────────────────────────────────

tree_communion_actor::tree_communion_actor( int turns ) : startup_turns( turns ) {}
void tree_communion_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "startup_turns", startup_turns );
    jsout.end_object();
}
std::unique_ptr<activity_actor> tree_communion_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<tree_communion_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "startup_turns", actor->startup_turns );
    return actor;
}
std::unique_ptr<activity_actor> tree_communion_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<tree_communion_actor>();
    auto values = data.get_int_array( "values" );
    if( !values.empty() ) { actor->startup_turns = values[0]; }
    return actor;
}
void tree_communion_actor::start( player_activity &, Character & )
{
    if( activity_actor::progress.invalid() ) {
        activity_actor::progress.dummy();
    }
}
void tree_communion_actor::do_turn( player_activity &act, Character &who )
{

    // There's an initial rooting process.
    if( startup_turns > 0 ) {
        startup_turns -= 1;
        if( startup_turns == 0 ) {
            if( who.has_trait( trait_id( trait_SPIRITUAL ) ) ) {
                who.add_msg_if_player( m_good, _( "The ancient tree spirits answer your call." ) );
            } else {
                who.add_msg_if_player( m_good, _( "Your communion with the trees has begun." ) );
            }
        }
        return;
    }
    // Information is received every minute.
    if( !action_time_scale::once_every_this_tick( 1_minutes ) ) {
        return;
    }
    // Breadth-first search forest tiles until one reveals new overmap tiles.
    std::queue<tripoint_abs_omt> q;
    std::unordered_set<tripoint_abs_omt> seen;
    tripoint_abs_omt loc = who.abs_omt_pos();
    q.push( loc );
    seen.insert( loc );
    const std::function<bool( const oter_id & )> filter = []( const oter_id & ter ) {
        return ter.obj().is_wooded() || ter.obj().get_name() == "field";
    };
    while( !q.empty() ) {
        tripoint_abs_omt tpt = q.front();
        if( get_overmapbuffer( who.get_dimension() ).reveal( tpt, 3, filter ) ) {
            if( who.has_trait( trait_SPIRITUAL ) ) {
                who.add_morale( MORALE_TREE_COMMUNION, 2, 30, 8_hours, 6_hours );
            } else {
                who.add_morale( MORALE_TREE_COMMUNION, 1, 15, 2_hours, 1_hours );
            }
            if( one_in( 128 ) ) {
                who.add_msg_if_player( "%s", SNIPPET.random_from_category( "tree_communion" ).value_or(
                                           translation() ) );
            }
            return;
        }
        for( const tripoint_abs_omt &neighbor : points_in_radius( tpt, 1 ) ) {
            if( seen.contains( neighbor ) ) {
                continue;
            }
            seen.insert( neighbor );
            if( !get_overmapbuffer( who.get_dimension() ).ter( neighbor ).obj().is_wooded() ) {
                continue;
            }
            q.push( neighbor );
        }
        q.pop();
    }
    who.add_msg_if_player( m_info, _( "The trees have shown you what they will." ) );
    act.set_to_null();
}
void tree_communion_actor::finish( player_activity &, Character & ) {}

// ─── shear_actor ─────────────────────────────────────────────────────────────

shear_actor::shear_actor( const tripoint_abs_ms &pos,
                          const std::string &tied,
                          safe_reference<item> shears_ref )
    : target_pos( pos ), tied_state( tied ), shears( shears_ref ) {}
void shear_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "target_pos", target_pos );
    jsout.member( "tied_state", tied_state );
    jsout.member( "shears", shears );
    jsout.end_object();
}
std::unique_ptr<activity_actor> shear_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<shear_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "target_pos", actor->target_pos );
    data.read( "tied_state", actor->tied_state );
    data.read( "shears", actor->shears );
    return actor;
}
std::unique_ptr<activity_actor> shear_actor::legacy_deserialize( const JsonObject &data )
{
    auto coords = std::vector<tripoint_abs_ms>();
    data.read( "coords", coords );
    if( coords.empty() ) { return nullptr; }
    std::string tied;
    auto str_values = std::vector<std::string>();
    data.read( "str_values", str_values );
    if( !str_values.empty() && str_values[0] == "temp_tie" ) {
        tied = "temp_tie";
    }
    auto actor = std::make_unique<shear_actor>( coords[0], tied, safe_reference<item>() );
    restore_legacy_progress( *actor, data, "shearing" );
    return actor;
}
void shear_actor::start( player_activity &, Character & ) {}
void shear_actor::do_turn( player_activity &, Character & )
{
}
void shear_actor::finish( player_activity &act, Character &who )
{


    if( !shears ) {
        debugmsg( "shearing activity with no shears" );
        act.set_to_null();
        return;
    }
    item *shears_item = &*shears;
    auto *source_mon = g->critter_at<monster>( target_pos );
    if( source_mon == nullptr ) {
        debugmsg( "could not find source creature for shearing" );
        act.set_to_null();
        return;
    }
    // 22 wool staples corresponds to an average wool-producing sheep yield of 10 lbs or so
    for( int i = 0; i != 22; ++i ) {
        detached_ptr<item> wool_staple = item::spawn( itype_wool_staple, calendar::turn );
        who.get_mapbuffer().add_item_or_charges( who.abs_pos(), std::move( wool_staple ) );
    }
    source_mon->add_effect( effect_sheared, calendar::season_length() );
    if( tied_state == "temp_tie" ) {
        source_mon->remove_effect( effect_tied );
    }
    act.set_to_null();
    if( shears_item->type->can_have_charges() ) {
        who.consume_charges( *shears_item, shears_item->type->charges_to_use() );
    }
}

// ─── milk_actor ──────────────────────────────────────────────────────────────

milk_actor::milk_actor( const tripoint_abs_ms &pos,
                        const std::string &tied )
    : target_pos( pos ), tied_state( tied ) {}
void milk_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "target_pos", target_pos );
    jsout.member( "tied_state", tied_state );
    jsout.end_object();
}
std::unique_ptr<activity_actor> milk_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<milk_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "target_pos", actor->target_pos );
    data.read( "tied_state", actor->tied_state );
    return actor;
}
std::unique_ptr<activity_actor> milk_actor::legacy_deserialize( const JsonObject &data )
{
    auto coords = std::vector<tripoint_abs_ms>();
    data.read( "coords", coords );
    if( coords.empty() ) { return nullptr; }
    std::string tied;
    auto str_values = std::vector<std::string>();
    data.read( "str_values", str_values );
    if( !str_values.empty() && str_values[0] == "temp_tie" ) {
        tied = "temp_tie";
    }
    auto actor = std::make_unique<milk_actor>( coords[0], tied );
    restore_legacy_progress( *actor, data, "milking" );
    return actor;
}
void milk_actor::start( player_activity &, Character & ) {}
void milk_actor::do_turn( player_activity &, Character & )
{
}
void milk_actor::finish( player_activity &act, Character &who )
{
    auto *source_mon = g->critter_at<monster>( target_pos );
    if( source_mon == nullptr ) {
        debugmsg( "could not find source creature for liquid transfer" );
        act.set_to_null();
        return;
    }
    auto milked_item = source_mon->ammo.find( source_mon->type->starting_ammo.begin()->first );
    if( milked_item == source_mon->ammo.end() ) {
        debugmsg( "animal has no milkable ammo type" );
        act.set_to_null();
        return;
    }
    if( milked_item->second <= 0 ) {
        debugmsg( "started milking but udders are now empty before milking finishes" );
        act.set_to_null();
        return;
    }
    detached_ptr<item> milk = item::spawn( milked_item->first, calendar::turn, milked_item->second );
    liquid_handler::handle_liquid( std::move( milk ) );
    // NOLINTNEXTLINE(bugprone-use-after-move)
    if( !milk ) {
        milked_item->second = 0;
        who.add_msg_if_player( _( "The %s's udders run dry." ), source_mon->get_name() );
    } else {
        milked_item->second = milk->charges;
    }
    // if the monster was not manually tied up, but needed to be fixed in place temporarily then
    // remove that now.
    if( tied_state == "temp_tie" ) {
        source_mon->remove_effect( effect_tied );
    }
    act.set_to_null();
}

// ─── pulp_actor ──────────────────────────────────────────────────────────────

pulp_actor::pulp_actor( const tripoint_abs_ms &pos, bool auto_no_acid, int num_corpses )
    : target_pos( pos ), auto_pulp_no_acid( auto_no_acid ), num_corpses( num_corpses ) {}
void pulp_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "target_pos", target_pos );
    jsout.member( "auto_pulp_no_acid", auto_pulp_no_acid );
    jsout.member( "num_corpses", num_corpses );
    jsout.end_object();
}
std::unique_ptr<activity_actor> pulp_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<pulp_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "target_pos", actor->target_pos );
    data.read( "auto_pulp_no_acid", actor->auto_pulp_no_acid );
    data.read( "num_corpses", actor->num_corpses );
    return actor;
}
std::unique_ptr<activity_actor> pulp_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<pulp_actor>();
    tripoint_abs_ms pl;
    data.read( "placement", pl );
    actor->target_pos = pl;
    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() && str_values[0] == "auto_pulp_no_acid" ) {
        actor->auto_pulp_no_acid = true;
    }
    return actor;
}
void pulp_actor::start( player_activity &, Character & )
{
    if( activity_actor::progress.invalid() ) {
        activity_actor::progress.dummy();
    }
}

void pulp_actor::do_turn( player_activity &act, Character &who )
{

    mapbuffer &here = who.get_mapbuffer();
    const tripoint_abs_ms pos = target_pos;

    // Stabbing weapons are a lot less effective at pulping
    const auto cut_power = std::max( who.primary_weapon().damage_melee( DT_CUT ),
                                     who.primary_weapon().damage_melee( DT_STAB ) / 2 );

    ///\EFFECT_STR increases pulping power, with diminishing returns
    const auto pulp_effort = std::max( 0, who.str_cur + who.primary_weapon().damage_melee( DT_BASH ) );
    auto pulp_power = std::sqrt( pulp_effort * std::max( 0.0f, cut_power + 1.0f ) );
    // Multiplier to get the chance right + some bonus for survival skill
    pulp_power *= 40 + who.get_skill_level( skill_survival ) * 5;

    if( pulp_power <= 0.0f || !std::isfinite( pulp_power ) ) {
        who.add_msg_player_or_npc( m_bad, _( "You are unable to pulp the corpse." ),
                                   _( "<npcname> is unable to pulp the corpse." ) );
        activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
        return;
    }

    const auto mess_radius = who.primary_weapon().has_flag( flag_MESSY ) ? 2 : 1;

    int moves = 0;
    const auto *corpse_pile = here.get_items( pos );
    if( corpse_pile == nullptr ) {
        activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
        return;
    }
    for( item *corpse : *corpse_pile ) {
        const mtype *corpse_mtype = corpse->get_mtype();
        if( !corpse->is_corpse() || ( !corpse_mtype->has_flag( MF_REVIVES ) &&
                                      !corpse_mtype->zombify_into ) ||
            ( auto_pulp_no_acid && corpse_mtype->bloodType().obj().has_acid ) ) {
            continue;
        }

        while( corpse->damage() < corpse->max_damage() ) {
            if( x_in_y( pulp_power, corpse->volume() / units::legacy_volume_factor ) ) {
                corpse->inc_damage( DT_BASH );
                if( corpse->damage() == corpse->max_damage() ) {
                    num_corpses++;
                }
            }

            if( x_in_y( pulp_power, corpse->volume() / units::legacy_volume_factor ) ) {
                const int radius = mess_radius + x_in_y( pulp_power, 500 ) + x_in_y( pulp_power, 1000 );
                const tripoint_abs_ms dest( pos + tripoint( rng( -radius, radius ),
                                            rng( -radius, radius ), 0 ) );
                const field_type_id type_blood = ( mess_radius > 1 && x_in_y( pulp_power, 10000 ) ) ?
                                                 corpse->get_mtype()->gibType() :
                                                 corpse->get_mtype()->bloodType();
                here.add_splatter_trail( type_blood, pos, dest );
            }

            who.mod_stamina( -pulp_effort );

            if( one_in( 4 ) ) {
                who.practice( skill_survival, 2, 2 );
            }

            float stamina_ratio = static_cast<float>( who.get_stamina() ) / who.get_stamina_max();
            moves += 100 / std::max( 0.25f, stamina_ratio );
            if( stamina_ratio < 0.33 || who.is_npc() ) {
                who.moves = std::min( 0, who.moves - moves );
                return;
            }
            if( moves >= who.moves ) {
                who.moves -= moves;
                return;
            }
        }
        corpse->set_flag( flag_PULPED );
    }
    // If we reach this, all corpses have been pulped, finish the activity
    activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
    if( num_corpses == 0 ) {
        who.add_msg_if_player( m_bad, _( "The corpse moved before you could finish smashing it!" ) );
        return;
    }
    who.add_msg_player_or_npc( vgettext( "The corpse is thoroughly pulped.",
                                         "The corpses are thoroughly pulped.", num_corpses ),
                               vgettext( "<npcname> finished pulping the corpse.",
                                         "<npcname> finished pulping the corpses.", num_corpses ) );
}

void pulp_actor::finish( player_activity &act, Character &who )
{

    if( who.is_npc() ) {
        who.as_npc()->revert_after_activity();
    } else {
        act.set_to_null();
    }
}

// ─── hotwire_car_actor ───────────────────────────────────────────────────────

hotwire_car_actor::hotwire_car_actor( const tripoint_abs_ms &pos, int skill, int moves )
    : veh_pos( pos ), mechanics_skill( skill ), moves_total( moves ) {}
void hotwire_car_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "veh_pos", veh_pos );
    jsout.member( "mechanics_skill", mechanics_skill );
    jsout.member( "moves_total", moves_total );
    jsout.end_object();
}
std::unique_ptr<activity_actor> hotwire_car_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<hotwire_car_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "veh_pos", actor->veh_pos );
    data.read( "mechanics_skill", actor->mechanics_skill );
    data.read( "moves_total", actor->moves_total );
    return actor;
}
std::unique_ptr<activity_actor> hotwire_car_actor::legacy_deserialize( const JsonObject &data )
{
    auto values = data.get_int_array( "values" );
    if( values.size() < 3 ) { return nullptr; }
    auto actor = std::make_unique<hotwire_car_actor>(
                     tripoint_abs_ms( values[0], values[1], 0 ), values[2],
                     data.get_int( "moves_total", 0 ) );
    const auto moves_left = data.get_int( "moves_left", actor->moves_total );
    if( actor->moves_total > 0 ) {
        actor->progress.emplace( "hotwiring", actor->moves_total, moves_left );
    }
    return actor;
}
void hotwire_car_actor::start( player_activity &, Character & )
{
    if( progress.empty() ) {
        progress.emplace( "hotwiring", moves_total );
    }
}
void hotwire_car_actor::do_turn( player_activity &act, Character &who ) {}
void hotwire_car_actor::finish( player_activity &act, Character &who )
{

    if( const optional_vpart_position vp = g->m.veh_at( veh_pos ) ) {
        vehicle *const veh = &vp->vehicle();
        if( mechanics_skill > rng( 1, 6 ) ) {
            veh->is_locked = false;
            add_msg( _( "This wire will start the engine." ) );
        } else if( mechanics_skill > rng( 0, 4 ) ) {
            veh->is_locked = false;
            veh->is_alarm_on = veh->has_security_working();
            add_msg( _( "This wire will probably start the engine." ) );
        } else if( veh->is_alarm_on ) {
            veh->is_locked = false;
            add_msg( _( "By process of elimination, this wire will start the engine." ) );
        } else {
            veh->is_alarm_on = veh->has_security_working();
            add_msg( _( "The red wire always starts the engine, doesn't it?" ) );
        }
    } else {
        debugmsg( "process_activity ACT_HOTWIRE_CAR: vehicle not found" );
    }
    act.set_to_null();
}

// ─── start_engines_actor ─────────────────────────────────────────────────────

start_engines_actor::start_engines_actor( int control, const tripoint_abs_ms &pos, int moves )
    : take_control( control ), placement( pos ), moves_total( moves ) {}
void start_engines_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "take_control", take_control );
    jsout.member( "placement", placement );
    jsout.member( "moves_total", moves_total );
    jsout.end_object();
}
std::unique_ptr<activity_actor> start_engines_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<start_engines_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "take_control", actor->take_control );
    data.read( "placement", actor->placement );
    data.read( "moves_total", actor->moves_total );
    return actor;
}
std::unique_ptr<activity_actor> start_engines_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<start_engines_actor>();
    auto values = data.get_int_array( "values" );
    if( !values.empty() ) { actor->take_control = values[0]; }
    data.read( "placement", actor->placement );
    actor->moves_total = data.get_int( "moves_total", 0 );
    const auto moves_left = data.get_int( "moves_left", actor->moves_total );
    if( actor->moves_total > 0 ) {
        actor->progress.emplace( "starting vehicle", actor->moves_total, moves_left );
    }
    return actor;
}
void start_engines_actor::start( player_activity &, Character & )
{
    if( progress.empty() ) {
        progress.emplace( "starting vehicle", moves_total );
    }
}
void start_engines_actor::do_turn( player_activity &act, Character &who ) {}
void start_engines_actor::finish( player_activity &act, Character &who )
{

    act.set_to_null();
    vehicle *veh = g->remoteveh();
    map &here = get_map();
    if( !veh ) {
        veh = veh_pointer_or_null( here.veh_at( placement ) );
        if( !veh ) {
            return;
        }
    }

    int attempted = 0;
    int non_muscle_attempted = 0;
    int started = 0;
    int non_muscle_started = 0;
    int non_combustion_started = 0;

    for( size_t e = 0; e < veh->engines.size(); ++e ) {
        if( veh->is_engine_on( e ) ) {
            attempted++;
            if( !veh->is_engine_type( e, itype_muscle ) &&
                !veh->is_engine_type( e, itype_animal ) ) {
                non_muscle_attempted++;
            }
            if( veh->start_engine( e ) ) {
                started++;
                if( !veh->is_engine_type( e, itype_muscle ) &&
                    !veh->is_engine_type( e, itype_animal ) ) {
                    non_muscle_started++;
                } else {
                    non_combustion_started++;
                }
            }
        }
    }

    veh->engine_on = started;
    sfx::do_vehicle_engine_sfx();

    if( attempted == 0 ) {
        add_msg( m_info, _( "The %s doesn't have an engine!" ), veh->name );
    } else if( non_muscle_attempted > 0 ) {
        if( non_muscle_attempted == non_muscle_started ) {
            add_msg( vgettext( "The %s's engine starts up.",
                               "The %s's engines start up.", non_muscle_started ), veh->name );
        } else if( non_muscle_started > 0 ) {
            add_msg( vgettext( "One of the %s's engines start up.",
                               "Some of the %s's engines start up.", non_muscle_started ), veh->name );
        } else if( non_combustion_started > 0 ) {
            add_msg( _( "The %s is ready for movement." ), veh->name );
        } else {
            add_msg( m_bad, vgettext( "The %s's engine fails to start.",
                                      "The %s's engines fail to start.", non_muscle_attempted ), veh->name );
        }
    }

    if( !veh->engine_on ) {
        who.add_msg_if_player( _( "You try to start the engine, but fail." ) );
    }
}

// ─── start_fire_actor ────────────────────────────────────────────────────────

start_fire_actor::start_fire_actor( int light, const tripoint_abs_ms &pos, int difficulty )
    : light_level( light ), placement( pos ), practice_difficulty( difficulty ) {}
void start_fire_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "light_level", light_level );
    jsout.member( "placement", placement );
    jsout.member( "practice_difficulty", practice_difficulty );
    jsout.end_object();
}
std::unique_ptr<activity_actor> start_fire_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<start_fire_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "light_level", actor->light_level );
    data.read( "placement", actor->placement );
    data.read( "practice_difficulty", actor->practice_difficulty );
    return actor;
}
std::unique_ptr<activity_actor> start_fire_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<start_fire_actor>();
    auto values = data.get_int_array( "values" );
    if( !values.empty() ) { actor->light_level = values[0]; }
    data.read( "placement", actor->placement );
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 ) {
        actor->progress.emplace( "starting fire", moves_total,
                                 std::max( 0, data.get_int( "moves_left", moves_total ) ) );
    }
    return actor;
}
void start_fire_actor::start( player_activity &, Character & ) {}

void start_fire_actor::do_turn( player_activity &act, Character &who )
{
    if( activity_actor::progress.invalid() ) {
        debugmsg( "Starting fire without an initialized progress counter" );
        act.set_to_null();
        return;
    }
    if( act.get_tools().empty() || !act.get_tools().front() ) {
        debugmsg( "Starting fire activity lost its tool" );
        act.set_to_null();
        return;
    }
    auto &here = who.get_mapbuffer();
    item &firestarter = *act.get_tools().front();
    if( !here.is_flammable( placement ) || ( firestarter.has_flag( flag_REQUIRES_TINDER ) &&
            !here.tinder_at( placement ) ) ) {
        try_fuel_fire( act, who, true );
        if( !here.is_flammable( placement ) ) {
            who.add_msg_if_player( m_info, _( "There's nothing to light there." ) );
            who.cancel_activity();
            return;
        }
    }

    if( firestarter.has_flag( flag_REQUIRES_TINDER ) ) {
        if( !here.tinder_at( placement ) ) {
            who.add_msg_if_player( m_info, _( "This item requires tinder to light." ) );
            who.cancel_activity();
            return;
        }
    }

    const use_function *usef = firestarter.type->get_use( "firestarter" );
    if( usef == nullptr || usef->get_actor_ptr() == nullptr ) {
        add_msg( m_bad, _( "You have lost the item you were using to start the fire." ) );
        who.cancel_activity();
        return;
    }

    who.mod_moves( -who.moves );
    const firestarter_actor *actor = dynamic_cast<const firestarter_actor *>( usef->get_actor_ptr() );
    if( actor == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act.set_to_null();
        return;
    }
    const float light = actor->light_mod( who, placement );
    const int progress = static_cast<int>( light *
                                           action_time_scale::activity_progress_per_tick() );
    activity_actor::progress.mod_moves_left( -std::min( progress,
            activity_actor::progress.get_moves_left() ) );
    if( light < 0.1 ) {
        add_msg( m_bad, _( "There is not enough sunlight to start a fire now.  You stop trying." ) );
        who.cancel_activity();
    }
}

void start_fire_actor::finish( player_activity &act, Character &who )
{

    static const std::string iuse_name_string( "firestarter" );

    if( act.get_tools().empty() || !act.get_tools().front() ) {
        debugmsg( "Starting fire activity lost its tool" );
        act.set_to_null();
        return;
    }
    item &it = *act.get_tools().front();
    item *used_tool = it.get_usable_item( iuse_name_string );
    if( used_tool == nullptr ) {
        debugmsg( "Lost tool used for starting fire" );
        act.set_to_null();
        return;
    }

    const use_function *use_fun = used_tool->get_use( iuse_name_string );
    if( use_fun == nullptr ) {
        debugmsg( "Lost firestarter use function" );
        act.set_to_null();
        return;
    }
    const firestarter_actor *actor = dynamic_cast<const firestarter_actor *>
                                     ( use_fun->get_actor_ptr() );
    if( actor == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act.set_to_null();
        return;
    }

    if( it.type->can_have_charges() ) {
        if( it.has_flag( flag_USE_UPS ) ) {
            who.use_charges( itype_UPS, it.type->charges_to_use() );
        }
        who.consume_charges( it, it.type->charges_to_use() );
    }
    who.practice( skill_survival, practice_difficulty, 5 );

    map &here = get_map();
    firestarter_actor::resolve_firestarter_use( who, placement );
    act.set_to_null();
}

// ─── make_zlave_actor ────────────────────────────────────────────────────────

make_zlave_actor::make_zlave_actor( int success, const std::string &name,
                                    safe_reference<item> corpse_ref )
    : success_chance( success ), corpse_name( name ), corpse( std::move( corpse_ref ) ) {}
void make_zlave_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "success_chance", success_chance );
    jsout.member( "corpse_name", corpse_name );
    jsout.member( "corpse", corpse );
    jsout.end_object();
}
std::unique_ptr<activity_actor> make_zlave_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<make_zlave_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "success_chance", actor->success_chance );
    data.read( "corpse_name", actor->corpse_name );
    data.read( "corpse", actor->corpse );
    return actor;
}
std::unique_ptr<activity_actor> make_zlave_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<make_zlave_actor>();
    auto values = data.get_int_array( "values" );
    if( !values.empty() ) { actor->success_chance = values[0]; }
    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() ) { actor->corpse_name = str_values[0]; }
    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    if( !targets.empty() ) { actor->corpse = std::move( targets.front() ); }
    restore_legacy_progress( *actor, data, "enslaving corpse" );
    return actor;
}
void make_zlave_actor::start( player_activity &, Character & ) {}
void make_zlave_actor::do_turn( player_activity &act, Character &who ) {}
void make_zlave_actor::finish( player_activity &act, Character &who )
{

    act.set_to_null();
    item *body = corpse.get();

    if( body == nullptr ) {
        add_msg( m_info, _( "There's no corpse to make into a zombie slave!" ) );
        return;
    }

    if( success_chance > 0 ) {
        who.practice( skill_firstaid, rng( 2, 5 ) );
        who.practice( skill_survival, rng( 2, 5 ) );
        who.add_msg_if_player( m_good,
                               _( "You slice muscles and tendons, and remove body parts until you're confident the zombie won't be able to attack you when it reanimates." ) );
        body->set_var( "zlave", "zlave" );
        if( one_in( 10 ) ) {
            body->set_var( "zlave", "mutilated" );
        }
    } else if( success_chance > -20 ) {
        who.practice( skill_firstaid, rng( 3, 6 ) );
        who.practice( skill_survival, rng( 3, 6 ) );
        who.add_msg_if_player( m_warning,
                               _( "You hack into the corpse and chop off some body parts.  You think the zombie won't be able to attack when it reanimates." ) );
        int success = success_chance + rng( 1, 20 );
        if( success > 0 && !one_in( 5 ) ) {
            body->set_var( "zlave", "zlave" );
        } else {
            body->set_var( "zlave", "mutilated" );
        }
    } else {
        who.practice( skill_firstaid, rng( 1, 8 ) );
        who.practice( skill_survival, rng( 1, 8 ) );
        body->mod_damage( rng( 0, body->max_damage() - body->damage() ), DT_STAB );
        if( body->damage() == body->max_damage() ) {
            body->deactivate();
            who.add_msg_if_player( m_warning, _( "You cut up the corpse too much, it is thoroughly pulped." ) );
        } else {
            who.add_msg_if_player( m_warning,
                                   _( "You cut into the corpse trying to make it unable to attack, but you don't think you have it right." ) );
        }
    }
}

// ─── study_spell_actor ───────────────────────────────────────────────────────

study_spell_actor::study_spell_actor( const std::string &type, const std::string &mode,
                                      const std::string &gain )
    : spell_type( type ), study_mode( mode ), gain_level_flag( gain ) {}
void study_spell_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "spell_type", spell_type );
    jsout.member( "study_mode", study_mode );
    jsout.member( "gain_level_flag", gain_level_flag );
    jsout.member( "total_xp", total_xp );
    jsout.member( "total_levels", total_levels );
    jsout.member( "dark", dark );
    jsout.member( "tick_counter", tick_counter );
    jsout.member( "xp_snapshot", xp_snapshot );
    jsout.end_object();
}
std::unique_ptr<activity_actor> study_spell_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<study_spell_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "spell_type", actor->spell_type );
    data.read( "study_mode", actor->study_mode );
    data.read( "gain_level_flag", actor->gain_level_flag );
    data.read( "total_xp", actor->total_xp );
    data.read( "total_levels", actor->total_levels );
    data.read( "dark", actor->dark );
    data.read( "tick_counter", actor->tick_counter );
    data.read( "xp_snapshot", actor->xp_snapshot );
    return actor;
}
std::unique_ptr<activity_actor> study_spell_actor::legacy_deserialize( const JsonObject &data )
{
    auto str_values = data.get_string_array( "str_values" );
    if( str_values.empty() ) { return nullptr; }
    auto actor = std::make_unique<study_spell_actor>( str_values[0] );
    if( str_values.size() >= 2 ) { actor->study_mode = str_values[1]; }
    if( str_values.size() >= 1 && str_values[0] == "gain_level" ) {
        actor->gain_level_flag = "gain_level";
    }
    auto values = data.get_int_array( "values" );
    if( values.size() >= 1 ) { actor->total_xp = values[0]; }
    if( values.size() >= 2 ) { actor->total_levels = values[1]; }
    if( values.size() >= 3 ) { actor->dark = values[2]; }
    if( values.size() >= 4 ) { actor->tick_counter = values[3]; }
    if( values.size() >= 5 ) { actor->xp_snapshot = values[4]; }
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 ) {
        actor->progress.emplace( "studying spell", moves_total,
                                 std::max( 0, data.get_int( "moves_left", moves_total ) ) );
    }
    return actor;
}
void study_spell_actor::start( player_activity &, Character & ) {}

void study_spell_actor::do_turn( player_activity &act, Character &who )
{

    if( !character_funcs::can_see_fine_details( who ) ) {
        dark = -1;
        activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
        return;
    }
    if( study_mode == "study" ) {
        spell &studying = who.magic->get_spell( spell_id( spell_type ) );
        const int old_level = studying.get_level();
        const int xp = roll_remainder( studying.exp_modifier( who ) / to_turns<float>( 6_seconds ) );

        total_xp += xp;
        studying.gain_exp( xp );

        if( tick_counter % 600 == 599 ) {
            who.add_msg_if_player( m_good, _( "You gained %i experience in %s" ),
                                   total_xp - xp_snapshot, studying.name() );
            xp_snapshot = total_xp;
        }

        const int new_level = studying.get_level();

        if( new_level > old_level ) {
            total_levels += new_level - old_level;
            g->events().send<event_type::player_levels_spell>( studying.id(), new_level );
            if( gain_level_flag == "gain_level" ) {
                activity_actor::progress.mod_moves_left( -activity_actor::progress.get_moves_left() );
            }
        } else if( gain_level_flag == "gain_level" ) {
            if( activity_actor::progress.complete() ) {
                activity_actor::progress.pop();
                activity_actor::progress.emplace( "studying spell", 1000000 );
            }
        }
    }
    tick_counter += 1;
}

void study_spell_actor::finish( player_activity &act, Character &who )
{

    act.set_to_null();

    if( study_mode == "study" ) {
        std::string level_string;
        if( total_levels > 0 ) {
            level_string = string_format( vgettext( " and %d level", " and %d levels", total_levels ),
                                          total_levels );
        }
        who.add_msg_if_player( m_good, _( "You gained %i experience%s from your study session." ),
                               total_xp, level_string );
        const spell &sp = who.magic->get_spell( spell_id( spell_type ) );
        who.practice( sp.skill(), total_xp, sp.get_difficulty() );
    } else if( study_mode == "learn" && dark == 0 ) {
        who.magic->learn_spell( spell_type, who );
    }
    if( dark == -1 ) {
        who.add_msg_if_player( m_bad, _( "It's too dark to read." ) );
    }
}

// ─── firstaid_actor ──────────────────────────────────────────────────────────

firstaid_actor::firstaid_actor( const std::string &type, safe_reference<item> target )
    : heal_type( type ), target_item( std::move( target ) ) {}
void firstaid_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "heal_type", heal_type );
    jsout.member( "target_item", target_item );
    jsout.end_object();
}
std::unique_ptr<activity_actor> firstaid_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<firstaid_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "heal_type", actor->heal_type );
    data.read( "target_item", actor->target_item );
    return actor;
}
std::unique_ptr<activity_actor> firstaid_actor::legacy_deserialize( const JsonObject &data )
{
    auto str_values = data.get_string_array( "str_values" );
    if( str_values.empty() ) { return nullptr; }
    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    safe_reference<item> target;
    if( targets.size() >= 1 ) {
        target = std::move( targets[0] );
    }
    auto actor = std::make_unique<firstaid_actor>( str_values[0], std::move( target ) );
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 ) {
        actor->progress.emplace( "first aid", moves_total,
                                 std::max( 0, data.get_int( "moves_left", moves_total ) ) );
    }
    return actor;
}
void firstaid_actor::start( player_activity &, Character & ) {}
void firstaid_actor::do_turn( player_activity &, Character & ) {}

void firstaid_actor::finish( player_activity &act, Character &who )
{
    static const std::string iuse_name_string( "heal" );

    if( !target_item ) {
        debugmsg( "Lost target of ACT_FIRSTAID" );
        act.set_to_null();
        return;
    }

    item &it = *target_item;
    item *used_tool = it.get_usable_item( iuse_name_string );
    if( used_tool == nullptr ) {
        debugmsg( "Lost tool used for healing" );
        act.set_to_null();
        return;
    }

    const use_function *use_fun = used_tool->get_use( iuse_name_string );
    if( use_fun == nullptr ) {
        debugmsg( "Lost first aid use function" );
        act.set_to_null();
        return;
    }
    const heal_actor *heal = dynamic_cast<const heal_actor *>( use_fun->get_actor_ptr() );
    if( heal == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act.set_to_null();
        return;
    }
    const bodypart_str_id healed = bodypart_str_id( heal_type );
    const int charges_consumed = heal->finish_using( who, who, *used_tool, healed );
    who.consume_charges( it, charges_consumed );

    act.set_to_null();
}

// ─── play_with_pet_actor ─────────────────────────────────────────────────────

play_with_pet_actor::play_with_pet_actor( weak_ptr_fast<monster> pet_ref, const std::string &name )
    : pet( std::move( pet_ref ) ), pet_name( name ) {}
void play_with_pet_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "pet_name", pet_name );
    jsout.end_object();
}
std::unique_ptr<activity_actor> play_with_pet_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<play_with_pet_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "pet_name", actor->pet_name );
    return actor;
}
std::unique_ptr<activity_actor> play_with_pet_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<play_with_pet_actor>();
    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() ) { actor->pet_name = str_values[0]; }
    // monster weak_ptr is runtime-only; re-acquired at activity start
    restore_legacy_progress( *actor, data, "playing with pet" );
    return actor;
}
void play_with_pet_actor::start( player_activity &, Character & ) {}
void play_with_pet_actor::do_turn( player_activity &act, Character &who ) {}
void play_with_pet_actor::finish( player_activity &act, Character &who )
{

    auto mon = pet.lock();
    if( !mon ) {
        debugmsg( "Lost pet target during ACT_PLAY_WITH_PET" );
        act.set_to_null();
        return;
    }
    mon->remove_effect( effect_ai_waiting );
    who.add_morale( MORALE_PLAY_WITH_PET, rng( 3, 10 ), 10, 5_hours, 25_minutes );
    who.add_msg_if_player( m_good, _( "Playing with your %s has lifted your spirits a bit." ),
                           pet_name );
    act.set_to_null();
}

// ─── train_pet_actor ─────────────────────────────────────────────────────────

train_pet_actor::train_pet_actor( weak_ptr_fast<monster> pet_ref, const std::string &name )
    : pet( std::move( pet_ref ) ), pet_name( name ) {}
void train_pet_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "pet_name", pet_name );
    jsout.end_object();
}
std::unique_ptr<activity_actor> train_pet_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<train_pet_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "pet_name", actor->pet_name );
    return actor;
}
std::unique_ptr<activity_actor> train_pet_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<train_pet_actor>();
    auto str_values = data.get_string_array( "str_values" );
    if( !str_values.empty() ) { actor->pet_name = str_values[0]; }
    // monster weak_ptr is runtime-only; re-acquired at activity start
    restore_legacy_progress( *actor, data, "training pet" );
    return actor;
}
void train_pet_actor::start( player_activity &, Character & ) {}
void train_pet_actor::do_turn( player_activity &act, Character &who ) {}
void train_pet_actor::finish( player_activity &act, Character &who )
{

    auto mon = pet.lock();
    if( mon && mon->type->pet_training &&
        who.get_skill_level( skill_survival ) < mon->type->pet_training->min_skill ) {
        who.add_msg_if_player( m_bad,
                               _( "You lack the skill to train %s effectively." ),
                               pet_name );
        act.set_to_null();
        return;
    }
    if( !mon ) {
        act.set_to_null();
        return;
    }
    mon->remove_effect( effect_well_fed );
    mon->remove_effect( effect_ai_waiting );
    auto const bonded = who.getID() == mon->bonded_character_id;
    auto skill_rating = 10 * who.get_skill_level( skill_survival );
    if( bonded ) { skill_rating *= 2; }
    if( skill_rating >= 100 || skill_rating >= rng( 0, 100 ) ) {
        if( mon && mon->type->pet_training ) {
            mon->training_level = std::min( mon->training_level + 1, mon->type->pet_training->max_level );
            for( const auto &lf : mon->type->pet_training->level_flags ) {
                if( lf.level == mon->training_level ) {
                    for( const m_flag f : lf.flags ) {
                        mon->monster_flags.insert( f );
                    }
                }
            }
            who.add_msg_if_player( m_good,
                                   _( "Training your %s has paid off!  They are now at training level %d/%d." ),
                                   pet_name, mon->training_level,
                                   mon->type->pet_training->max_level );
        }
    } else {
        who.add_msg_if_player( m_neutral,
                               _( "Training your %s takes time, it seems they are making a bit of progress at least." ),
                               pet_name );
    }
    act.set_to_null();
    mon->on_pet_bonding( who.as_character() );
}

// ─── socialize_actor ─────────────────────────────────────────────────────────

socialize_actor::socialize_actor( const std::string &name ) : npc_name( name ) {}
void socialize_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "npc_name", npc_name );
    jsout.end_object();
}
std::unique_ptr<activity_actor> socialize_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<socialize_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "npc_name", actor->npc_name );
    return actor;
}
std::unique_ptr<activity_actor> socialize_actor::legacy_deserialize( const JsonObject &data )
{
    auto str_values = data.get_string_array( "str_values" );
    if( str_values.empty() ) { return nullptr; }
    auto actor = std::make_unique<socialize_actor>( str_values[0] );
    restore_legacy_progress( *actor, data, "socializing" );
    return actor;
}
void socialize_actor::start( player_activity &, Character & ) {}
void socialize_actor::do_turn( player_activity &act, Character &who ) {}
void socialize_actor::finish( player_activity &act, Character &who )
{

    who.add_msg_if_player( _( "%s finishes chatting with you." ), npc_name );
    act.set_to_null();
}

// ─── train_actor ─────────────────────────────────────────────────────────────

train_actor::train_actor( const std::string &name, int expert, int trainer )
    : skill_name( name ), expert_multiplier( expert ), trainer_id( trainer ) {}
void train_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "skill_name", skill_name );
    jsout.member( "expert_multiplier", expert_multiplier );
    jsout.member( "trainer_id", trainer_id );
    jsout.end_object();
}
std::unique_ptr<activity_actor> train_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<train_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "skill_name", actor->skill_name );
    data.read( "expert_multiplier", actor->expert_multiplier );
    data.read( "trainer_id", actor->trainer_id );
    return actor;
}
std::unique_ptr<activity_actor> train_actor::legacy_deserialize( const JsonObject &data )
{
    int trainer = data.get_int( "index", -1 );
    auto actor = std::make_unique<train_actor>( data.get_string( "name" ), 0, trainer );
    restore_legacy_progress( *actor, data, "training" );
    return actor;
}
void train_actor::start( player_activity &, Character & ) {}
void train_actor::do_turn( player_activity &, Character & ) {}
void train_actor::finish( player_activity &act, Character &who )
{
    auto &p = static_cast<player &>( who );
    const skill_id sk( skill_name );
    if( sk.is_valid() ) {
        const Skill &skill = sk.obj();
        std::string skill_name_str = skill.name();
        int old_skill_level = who.get_skill_level( sk );
        who.get_skill_level_object( sk ).train( 100 * ( old_skill_level + 1 ), true );
        int new_skill_level = who.get_skill_level( sk );
        if( old_skill_level != new_skill_level ) {
            add_msg( m_good, _( "You finish training %s to level %d." ),
                     skill_name_str, new_skill_level );
            g->events().send<event_type::gains_skill_level>( who.getID(), sk, new_skill_level );
        } else {
            add_msg( m_good, _( "You get some training in %s." ), skill_name_str );
        }
        act.set_to_null();
        return;
    }

    const matype_id &ma_id = matype_id( skill_name );
    if( ma_id.is_valid() ) {
        const martialart &mastyle = ma_id.obj();
        // Trained martial arts,
        g->events().send<event_type::learns_martial_art>( who.getID(), ma_id );
        who.martial_arts_data->learn_style( mastyle.id, who.is_avatar() );
        act.set_to_null();
        return;
    }

    // Spell training (formerly magic_train)
    const spell_id &sp_id = spell_id( skill_name );
    if( sp_id.is_valid() ) {
        const bool knows = g->u.magic->knows_spell( sp_id );
        if( knows ) {
            spell &studying = who.magic->get_spell( sp_id );
            const int xp = roll_remainder( studying.exp_modifier( who ) * expert_multiplier );
            studying.gain_exp( xp );
            who.add_msg_if_player( m_good, _( "You learn a little about the spell: %s" ),
                                   sp_id->name );
        } else {
            who.magic->learn_spell( skill_name, who );
            if( who.magic->knows_spell( sp_id ) ) {
                add_msg( m_good, _( "You learn %s." ), sp_id->name.translated() );
            } else {
                act.set_to_null();
            }
        }
        act.set_to_null();
        return;
    }

    debugmsg( "train_finish without a valid skill or style or spell name" );
    act.set_to_null();
}

// ─── butcher_actor ───────────────────────────────────────────────────────────

butcher_actor::butcher_actor( const activity_id &type, safe_reference<item> corpse_ref )
    : act_type( type ), corpse( std::move( corpse_ref ) ) {}
void butcher_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "act_type", act_type );
    jsout.member( "corpse", corpse );
    jsout.member( "ready_for_next", ready_for_next );
    jsout.member( "extra_corpses", extra_corpses );
    jsout.end_object();
}
std::unique_ptr<activity_actor> butcher_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<butcher_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "act_type", actor->act_type );
    data.read( "corpse", actor->corpse );
    data.read( "ready_for_next", actor->ready_for_next );
    data.read( "extra_corpses", actor->extra_corpses );
    return actor;
}
std::unique_ptr<activity_actor> butcher_actor::legacy_deserialize( const JsonObject &data )
{
    // Read activity type from the save data itself
    activity_id act_type( data.get_string( "type" ) );
    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    if( targets.empty() ) { return nullptr; }
    auto actor = std::make_unique<butcher_actor>( act_type, std::move( targets[0] ) );
    // Store extra corpses from legacy targets[1..n] into extra_corpses
    for( size_t i = 1; i < targets.size(); ++i ) {
        actor->extra_corpses.push_back( std::move( targets[i] ) );
    }
    const int moves_total = data.get_int( "moves_total", 0 );
    if( moves_total > 0 ) {
        actor->progress.emplace( "butchering", moves_total,
                                 std::max( 0, data.get_int( "moves_left", moves_total ) ) );
        actor->ready_for_next = false;
    }
    return actor;
}
void butcher_actor::start( player_activity &, Character & ) {}

void butcher_actor::do_turn( player_activity &act, Character &who )
{
    if( !corpse && !act.targets.empty() ) {
        corpse = std::move( act.targets.front() );
        for( auto target = act.targets.begin() + 1; target != act.targets.end(); ++target ) {
            extra_corpses.push_back( std::move( *target ) );
        }
        act.targets.clear();
    }
    // Check if the current corpse has rotted away

    bool corpse_destroyed = corpse.is_destroyed();
    if( corpse_destroyed ) {
        who.add_msg_if_player( m_bad, _( "The corpse completely rotted away!" ) );
        act.set_to_null();
        return;
    }
    who.mod_stamina( -20 );
}

void butcher_actor::finish( player_activity &act, Character &who )
{
    // Helper lambda to load the next corpse from extra_corpses, returns false if none remain
    auto load_next_corpse = [&]() -> bool {
        if( extra_corpses.empty() )
        {
            return false;
        }
        corpse = std::move( extra_corpses.back() );
        extra_corpses.pop_back();
        return true;
    };

    // If we have no active corpse, try to load one from extras
    if( !corpse ) {
        if( !load_next_corpse() ) {
            act.set_to_null();
            activity_handlers::resume_for_multi_activities( who );
            return;
        }
    }

    // Check if the current corpse is still valid
    if( !corpse || !corpse->is_corpse() ) {
        who.add_msg_if_player( m_info, _( "There's no corpse to butcher!" ) );
        act.set_to_null();
        return;
    }

    // Determine the butcher action type from the activity id
    butcher_type action = BUTCHER;
    if( act_type == ACT_BUTCHER ) {
        action = BUTCHER;
    } else if( act_type == ACT_BUTCHER_FULL ) {
        action = BUTCHER_FULL;
    } else if( act_type == ACT_FIELD_DRESS ) {
        action = F_DRESS;
    } else if( act_type == ACT_QUARTER ) {
        action = QUARTER;
    } else if( act_type == ACT_BLEED ) {
        action = BLEED;
    } else if( act_type == ACT_DISSECT ) {
        action = DISSECT;
    } else if( act_type == ACT_SKIN ) {
        action = SKIN;
    } else if( act_type == ACT_DISMEMBER ) {
        action = DISMEMBER;
    }

    // If ready_for_next, set up the next target (set moves, etc.)
    if( ready_for_next ) {
        if( !who.is_player() ) {
            act.set_to_null();
            return;
        }
        const butchery_setup setup = consider_butchery( *corpse, *who.as_player(), action );

        const auto print_reasons = [&who, &setup]() {
            for( const std::string &prob : setup.problems ) {
                who.add_msg_if_player( m_bad, prob );
            }
            if( setup.problems.empty() ) {
                for( const std::string &info : setup.info ) {
                    who.add_msg_if_player( m_info, info );
                }
            }
        };

        if( setup.can_do == butchery_possibility::never ) {
            act.set_to_null();
            print_reasons();
            return;
        }
        if( setup.can_do == butchery_possibility::not_this ) {
            // Try the next corpse
            if( !load_next_corpse() ) {
                act.set_to_null();
                return;
            }
            print_reasons();
            // Try again with the new corpse
            // Need recursion or loop — simple: set ready and return, next tick will re-enter
            ready_for_next = true;
            return;
        }
        if( setup.can_do == butchery_possibility::need_confirmation ) {
            if( who.is_player() ) {
                if( query_yn( _( "Would you dare desecrate the mortal remains of a fellow human being?" ) ) ) {
                    switch( rng( 1, 3 ) ) {
                        case 1:
                            who.add_msg_if_player( m_bad, _( "You clench your teeth at the prospect of this gruesome job." ) );
                            break;
                        case 2:
                            who.add_msg_if_player( m_bad, _( "This will haunt you in your dreams." ) );
                            break;
                        case 3:
                            who.add_msg_if_player( m_bad,
                                                   _( "You try to look away, but this gruesome image will stay on your mind for some time." ) );
                            break;
                    }
                    g->u.add_morale( MORALE_BUTCHER, -50, 0, 2_days, 3_hours );
                } else {
                    who.add_msg_if_player( m_good, _( "It needs a coffin, not a knife." ) );
                    if( !load_next_corpse() ) {
                        act.set_to_null();
                        return;
                    }
                    ready_for_next = true;
                    return;
                }
            } else {
                who.add_morale( MORALE_BUTCHER, -50, 0, 2_days, 3_hours );
            }
        }

        print_reasons();
        act.get_tools_mut().clear();
        act.speed.calc_all_moves( who );
        if( !activity_actor::progress.invalid() && activity_actor::progress.complete() ) {
            activity_actor::progress.pop();
        }
        activity_actor::progress.emplace( corpse->tname(), setup.move_cost );
        ready_for_next = false;
        return;
    }

    // ──── Not ready_for_next: actually process the corpse ────

    item &corpse_item = *corpse;
    const mtype *corpse_mtype = corpse_item.get_mtype();
    const field_type_id type_blood = corpse_mtype->bloodType();
    const field_type_id type_gib = corpse_mtype->gibType();
    map &here = get_map();
    const inventory &inv = who.crafting_inventory();

    if( action == QUARTER ) {
        butchery_quarter( &corpse_item, who );
        ready_for_next = true;
        return;
    }

    int skill_level = who.get_skill_level( skill_survival );
    int factor = inv.max_quality( action == DISSECT ? qual_CUT_FINE : qual_BUTCHER );

    if( action == DISSECT ) {
        skill_level = who.get_skill_level( skill_firstaid ) / 2;
        skill_level += who.get_skill_level( skill_electronics ) / 2;
        skill_level += inv.max_quality( qual_CUT_FINE );
        add_msg( m_debug, _( "Skill: %s" ), skill_level );
    }

    const auto roll_butchery = [&]() {
        double skill_shift = 0.0;
        skill_shift += skill_level;
        skill_shift += rng_float( 0, who.get_dex() - 8 ) / 4.0;
        if( factor < 0 ) {
            skill_shift -= rng_float( 0, -factor / 5.0 );
        }
        return static_cast<int>( std::round( skill_shift ) );
    };

    if( action == DISMEMBER ) {
        here.add_splatter( type_gib, who.bub_pos(), rng( corpse_mtype->size + 2,
                           ( corpse_mtype->size + 1 ) * 2 ) );
    }

    // FATAL FAILURE
    if( action != DISSECT && roll_butchery() <= ( -15 ) && one_in( 2 ) ) {
        switch( rng( 1, 3 ) ) {
            case 1:
                who.add_msg_if_player( m_warning,
                                       _( "You hack up the corpse so unskillfully, that there is nothing left to salvage from this bloody mess." ) );
                break;
            case 2:
                who.add_msg_if_player( m_warning,
                                       _( "You wanted to cut the corpse, but instead you hacked the meat, spilled the guts all over it, and made a bloody mess." ) );
                break;
            case 3:
                who.add_msg_if_player( m_warning,
                                       _( "You made so many mistakes during the process that you doubt even vultures will be interested in what's left of it." ) );
                break;
        }

        corpse->detach();
        corpse = safe_reference<item>();

        here.add_splatter( type_gib, who.bub_pos(), rng( corpse_mtype->size + 2,
                           ( corpse_mtype->size + 1 ) * 2 ) );
        here.add_splatter( type_blood, who.bub_pos(), rng( corpse_mtype->size + 2,
                           ( corpse_mtype->size + 1 ) * 2 ) );
        for( int i = 1; i <= corpse_mtype->size; i++ ) {
            here.add_splatter_trail( type_gib, who.bub_pos(),
                                     random_entry( here.points_in_radius( who.bub_pos(),
                                                   corpse_mtype->size + 1 ) ) );
            here.add_splatter_trail( type_blood, who.bub_pos(),
                                     random_entry( here.points_in_radius( who.bub_pos(),
                                                   corpse_mtype->size + 1 ) ) );
        }

        ready_for_next = true;
        return;
    }

    const auto roll_drops = [&]() {
        factor = std::max( factor, -50 );
        return 0.5 * skill_level / 10 + 0.3 * ( factor + 50 ) / 100 + 0.2 * who.dex_cur / 20;
    };

    butchery_drops_harvest( &corpse_item, *corpse_mtype, who, roll_butchery, action, roll_drops );

    if( action == DISSECT ) {
        int roll = roll_butchery() - corpse_item.damage_level( 4 );
        roll = roll < 1 ? 1 : roll;
        add_msg( m_debug, _( "Roll penalty for corpse damage = %s" ), 0 - corpse_item.damage_level( 4 ) );
        std::vector<detached_ptr<item>> cbms = corpse_item.remove_components();
        std::vector<detached_ptr<item>> contents = corpse_item.contents.clear_items();
        for( detached_ptr<item> &it : contents ) {
            cbms.push_back( std::move( it ) );
        }
        extract_or_wreck_cbms( cbms, roll, who );
        int time_to_cut = butcher_time_to_cut( corpse_item, action ) / 100;
        int level_cap = std::min<int>( MAX_SKILL,
                                       ( static_cast<int>( corpse_mtype->size ) + ( cbms.size() * 2 + 1 ) ) );
        int size_mult = corpse_mtype->size > creature_size::medium ?
                        ( corpse_mtype->size * corpse_mtype->size ) : 8;
        int practice_amt = ( size_mult + 1 ) * ( ( time_to_cut / 150 ) + 1 ) *
                           ( cbms.size() * cbms.size() / 2 + 1 );
        who.practice( skill_firstaid, practice_amt, level_cap );
        add_msg( m_debug, "Experience: %d, Level cap: %d, Time to cut: %d", practice_amt, level_cap,
                 time_to_cut );
    }

    // End messages and effects
    switch( action ) {
        case QUARTER:
            break;
        case BUTCHER:
            who.add_msg_if_player( m_good,
                                   _( "You apply few quick cuts to the %s and leave what's left of it for scavengers." ),
                                   corpse_item.tname() );
            corpse->detach();
            corpse = safe_reference<item>();
            break;
        case BUTCHER_FULL:
            who.add_msg_if_player( m_good, _( "You finish butchering the %s." ), corpse_item.tname() );
            corpse->detach();
            corpse = safe_reference<item>();
            break;
        case F_DRESS:
            if( roll_butchery() < 0 ) {
                switch( rng( 1, 3 ) ) {
                    case 1:
                        who.add_msg_if_player( m_warning,
                                               _( "You unskillfully hack up the corpse and chop off some excess body parts.  You're left wondering how you did so poorly." ) );
                        break;
                    case 2:
                        who.add_msg_if_player( m_warning,
                                               _( "Your unskilled hands slip and damage the corpse.  You still hope it's not a total waste though." ) );
                        break;
                    case 3:
                        who.add_msg_if_player( m_warning,
                                               _( "You did something wrong and hacked the corpse badly.  Maybe it's still recoverable." ) );
                        break;
                }
                corpse_item.set_flag( flag_FIELD_DRESS_FAILED );
                here.add_splatter( type_gib, who.bub_pos(), rng( corpse_mtype->size + 2,
                                   ( corpse_mtype->size + 1 ) * 2 ) );
                here.add_splatter( type_blood, who.bub_pos(), rng( corpse_mtype->size + 2,
                                   ( corpse_mtype->size + 1 ) * 2 ) );
                for( int i = 1; i <= corpse_mtype->size; i++ ) {
                    here.add_splatter_trail( type_gib, who.bub_pos(),
                                             random_entry( here.points_in_radius( who.bub_pos(),
                                                           corpse_mtype->size + 1 ) ) );
                    here.add_splatter_trail( type_blood, who.bub_pos(),
                                             random_entry( here.points_in_radius( who.bub_pos(),
                                                           corpse_mtype->size + 1 ) ) );
                }
            } else {
                switch( rng( 1, 3 ) ) {
                    case 1:
                        who.add_msg_if_player( m_good, _( "You field dress the %s." ), corpse_mtype->nname() );
                        break;
                    case 2:
                        who.add_msg_if_player( m_good,
                                               _( "You slice the corpse's belly and remove intestines and organs, until you're confident that it will not rot from inside." ) );
                        break;
                    case 3:
                        who.add_msg_if_player( m_good,
                                               _( "You remove guts and excess parts, preparing the corpse for later use." ) );
                        break;
                }
                corpse_item.set_flag( flag_FIELD_DRESS );
                here.add_splatter( type_gib, who.bub_pos(), rng( corpse_mtype->size + 2,
                                   ( corpse_mtype->size + 1 ) * 2 ) );
                here.add_splatter( type_blood, who.bub_pos(), rng( corpse_mtype->size + 2,
                                   ( corpse_mtype->size + 1 ) * 2 ) );
                for( int i = 1; i <= corpse_mtype->size; i++ ) {
                    here.add_splatter_trail( type_gib, who.bub_pos(),
                                             random_entry( here.points_in_radius( who.bub_pos(),
                                                           corpse_mtype->size + 1 ) ) );
                    here.add_splatter_trail( type_blood, who.bub_pos(),
                                             random_entry( here.points_in_radius( who.bub_pos(),
                                                           corpse_mtype->size + 1 ) ) );
                }
            }
            corpse = safe_reference<item>();
            break;
        case BLEED:
            who.add_msg_if_player( m_good, _( "You bleed the %s." ), corpse_mtype->nname() );
            corpse_item.set_flag( flag_BLED );
            corpse = safe_reference<item>();
            break;
        case SKIN:
            switch( rng( 1, 4 ) ) {
                case 1:
                    who.add_msg_if_player( m_good, _( "You skin the %s." ), corpse_mtype->nname() );
                    break;
                case 2:
                    who.add_msg_if_player( m_good, _( "You carefully remove the hide from the %s" ),
                                           corpse_mtype->nname() );
                    break;
                case 3:
                    who.add_msg_if_player( m_good,
                                           _( "The %s is challenging to skin, but you get a good hide from it." ),
                                           corpse_mtype->nname() );
                    break;
                case 4:
                    who.add_msg_if_player( m_good, _( "With a few deft slices you take the skin from the %s" ),
                                           corpse_mtype->nname() );
                    break;
            }
            corpse_item.set_flag( flag_SKINNED );
            corpse = safe_reference<item>();
            break;
        case DISMEMBER:
            switch( rng( 1, 3 ) ) {
                case 1:
                    who.add_msg_if_player( m_good, _( "You hack the %s apart." ), corpse_item.tname() );
                    break;
                case 2:
                    who.add_msg_if_player( m_good, _( "You lop the limbs off the %s." ), corpse_item.tname() );
                    break;
                case 3:
                    who.add_msg_if_player( m_good, _( "You cleave the %s into pieces." ), corpse_item.tname() );
            }
            corpse->detach();
            corpse = safe_reference<item>();
            break;
        case DISSECT:
            who.add_msg_if_player( m_good, _( "You finish dissecting the %s." ), corpse_item.tname() );
            corpse->detach();
            corpse = safe_reference<item>();
            break;
    }

    // Ready to move on to the next item, if there is one (multi-butchering)
    ready_for_next = true;
    activity_handlers::resume_for_multi_activities( who );
}

// ─── operation_actor ─────────────────────────────────────────────────────────

operation_actor::operation_actor( const std::string &type, const std::string &bid,
                                  const std::string &installer, bool adoc, int diff, int succ, int cap, int skill )
    : op_type( type ), bionic_id( bid ), installer_name( installer ),
      autodoc( adoc ), difficulty( diff ), success( succ ), capacity( cap ), pl_skill( skill ) {}
void operation_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "op_type", op_type );
    jsout.member( "bionic_id", bionic_id );
    jsout.member( "installer_name", installer_name );
    jsout.member( "autodoc", autodoc );
    jsout.member( "difficulty", difficulty );
    jsout.member( "success", success );
    jsout.member( "capacity", capacity );
    jsout.member( "pl_skill", pl_skill );
    jsout.member( "operation_attempted", operation_attempted );
    jsout.end_object();
}
std::unique_ptr<activity_actor> operation_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<operation_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "op_type", actor->op_type );
    data.read( "bionic_id", actor->bionic_id );
    data.read( "installer_name", actor->installer_name );
    data.read( "autodoc", actor->autodoc );
    data.read( "difficulty", actor->difficulty );
    data.read( "success", actor->success );
    data.read( "capacity", actor->capacity );
    data.read( "pl_skill", actor->pl_skill );
    data.read( "operation_attempted", actor->operation_attempted );
    return actor;
}
std::unique_ptr<activity_actor> operation_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<operation_actor>();
    auto values = data.get_int_array( "values" );
    if( values.size() >= 4 ) {
        actor->difficulty = values[0];
        actor->success = values[1];
        actor->capacity = values[2];
        actor->pl_skill = values[3];
    }
    auto str_values = data.get_string_array( "str_values" );
    if( str_values.size() >= 1 ) { actor->op_type = str_values[0]; }
    if( str_values.size() >= 2 ) { actor->bionic_id = str_values[1]; }
    if( str_values.size() >= 3 ) { actor->installer_name = str_values[2]; }
    if( str_values.size() >= 4 ) { actor->autodoc = ( str_values[3] == "true" ); }
    const int moves_total = data.get_int( "moves_total", 0 );
    const int moves_left = data.get_int( "moves_left", moves_total );
    if( moves_total > 0 ) {
        actor->progress.emplace( "bionic operation", moves_total, std::max( 0, moves_left ) );
    }
    // operation_attempted defaults to 0 for legacy saves (old code would re-attempt)
    return actor;
}
void operation_actor::start( player_activity &, Character & ) {}
void operation_actor::do_turn( player_activity &act, Character &who )
{
    auto &p = static_cast<player &>( who );
    const ::bionic_id bid( this->bionic_id );
    const ::bionic_id upbid = bid->upgraded_bionic;
    const efftype_id effect_narcosis( "narcosis" );
    const ::bionic_id bp_id( "bio_painkiller" );
    const trait_id trait_NOPAIN( "NOPAIN" );
    const bool u_see = g->u.sees( who.abs_pos() ) && ( !g->u.has_effect( effect_narcosis ) ||
                       g->u.has_bionic( bp_id ) || g->u.has_trait( trait_NOPAIN ) );

    const std::vector<bodypart_id> bps = get_occupied_bodyparts( bid );

    const auto half_op_moves = to_moves<int>( difficulty * 10_minutes );
    const time_duration message_freq = difficulty * 2_minutes;
    const efftype_id effect_under_op( "under_operation" );
    if( progress.invalid() ) {
        debugmsg( "Bionic operation started without an actor progress counter" );
        act.set_to_null();
        who.remove_effect( effect_under_op );
        return;
    }
    const int current_moves_left = progress.get_moves_left();
    const auto time_left = time_duration::from_turns(
                               action_time_scale::activity_turns_for_progress( current_moves_left ) );

    map &here = get_map();

    const efftype_id effect_bleed( "bleed" );
    const efftype_id effect_blind( "blind" );
    const efftype_id effect_sleep( "sleep" );
    const std::string flag_AUTODOC_COUCH( "AUTODOC_COUCH" );

    // check if player is on an autodoc couch
    if( autodoc && here.inbounds( who.bub_pos() ) ) {
        const std::string flag_AUTODOC( "AUTODOC" );
        const auto autodocs = here.find_furnitures_or_vparts_with_flag_in_radius(
                                  who.bub_pos(), 1, flag_AUTODOC );
        if( !here.has_flag_furn_or_vpart( flag_AUTODOC_COUCH, who.bub_pos() ) || autodocs.empty() ) {
            who.remove_effect( effect_under_op );
            act.set_to_null();
            if( u_see ) {
                add_msg( m_bad, _( "The autodoc suffers a catastrophic failure." ) );
                who.add_msg_player_or_npc( m_bad,
                                           _( "The Autodoc's failure damages you greatly." ),
                                           _( "The Autodoc's failure damages <npcname> greatly." ) );
            }
            if( !bps.empty() ) {
                for( const bodypart_id &bp : bps ) {
                    who.add_effect( effect_bleed, 1_hours, bp.id(), difficulty );
                    who.apply_damage( nullptr, bp, 20 * difficulty );
                    if( u_see ) {
                        who.add_msg_player_or_npc( m_bad, _( "Your %s is ripped open." ),
                                                   _( "<npcname>'s %s is ripped open." ), body_part_name_accusative( bp->token ) );
                    }
                    if( bp == bodypart_id( "eyes" ) ) {
                        who.add_effect( effect_blind, 1_hours, bodypart_str_id::NULL_ID() );
                    }
                }
            } else {
                who.add_effect( effect_bleed, 1_hours, bodypart_str_id::NULL_ID(), difficulty );
                who.apply_damage( nullptr, bodypart_id( "torso" ), 20 * difficulty );
            }
        }
    }

    if( current_moves_left > half_op_moves ) {
        if( !bps.empty() ) {
            for( const bodypart_id &bp : bps ) {
                if( action_time_scale::once_every_this_tick( message_freq ) && u_see && autodoc ) {
                    who.add_msg_player_or_npc( m_info,
                                               _( "The Autodoc is meticulously cutting your %s open." ),
                                               _( "The Autodoc is meticulously cutting <npcname>'s %s open." ),
                                               body_part_name_accusative( bp->token ) );
                }
            }
        } else {
            if( action_time_scale::once_every_this_tick( message_freq ) && u_see ) {
                who.add_msg_player_or_npc( m_info,
                                           _( "The Autodoc is meticulously cutting you open." ),
                                           _( "The Autodoc is meticulously cutting <npcname> open." ) );
            }
        }
    } else if( operation_attempted == 0 ) {
        operation_attempted = 1;
        if( op_type == "uninstall" ) {
            if( u_see && autodoc ) {
                add_msg( m_info, _( "The Autodoc attempts to carefully extract the bionic." ) );
            }
            if( who.has_bionic( bid ) ) {
                who.perform_uninstall( bid, difficulty, success,
                                       units::from_joule( capacity ), pl_skill );
            } else {
                debugmsg( _( "Tried to uninstall %s, but you don't have this bionic installed." ), bid.c_str() );
                who.remove_effect( effect_under_op );
                act.set_to_null();
            }
        } else {
            if( u_see && autodoc ) {
                add_msg( m_info, _( "The Autodoc attempts to carefully insert the bionic." ) );
            }
            if( bid.is_valid() ) {
                who.perform_install( bid, upbid, difficulty, success, pl_skill,
                                     installer_name, bid->canceled_mutations );
            } else {
                debugmsg( _( "%s is no a valid bionic_id" ), bid.c_str() );
                who.remove_effect( effect_under_op );
                act.set_to_null();
            }
        }
    } else if( success > 0 ) {
        if( !bps.empty() ) {
            for( const bodypart_id &bp : bps ) {
                if( action_time_scale::once_every_this_tick( message_freq ) && u_see && autodoc ) {
                    who.add_msg_player_or_npc( m_info,
                                               _( "The Autodoc is stitching your %s back up." ),
                                               _( "The Autodoc is stitching <npcname>'s %s back up." ),
                                               body_part_name_accusative( bp->token ) );
                }
            }
        } else {
            if( action_time_scale::once_every_this_tick( message_freq ) && u_see && autodoc ) {
                who.add_msg_player_or_npc( m_info,
                                           _( "The Autodoc is stitching you back up." ),
                                           _( "The Autodoc is stitching <npcname> back up." ) );
            }
        }
    } else {
        if( action_time_scale::once_every_this_tick( message_freq ) && u_see && autodoc ) {
            who.add_msg_player_or_npc( m_bad,
                                       _( "The Autodoc is moving erratically through the rest of its program, not actually stitching your wounds." ),
                                       _( "The Autodoc is moving erratically through the rest of its program, not actually stitching <npcname>'s wounds." ) );
        }
    }

    // Makes sure NPC is still under anesthesia
    if( who.has_effect( effect_narcosis ) ) {
        const time_duration remaining_time = who.get_effect_dur( effect_narcosis );
        if( remaining_time <= time_left ) {
            const time_duration top_off_time = time_left - remaining_time;
            who.add_effect( effect_narcosis, top_off_time );
            who.add_effect( effect_sleep, top_off_time );
        }
    } else {
        who.add_effect( effect_narcosis, time_left );
        who.add_effect( effect_sleep, time_left );
    }
}
void operation_actor::finish( player_activity &act, Character &who )
{
    auto &p = static_cast<player &>( who );
    map &here = get_map();
    if( autodoc ) {
        sound_event se;
        const std::string flag_AUTODOC( "AUTODOC" );
        const std::list<tripoint_bub_ms> autodocs = here.find_furnitures_or_vparts_with_flag_in_radius(
                    who.bub_pos(), 1, flag_AUTODOC );
        if( autodocs.empty() ) {
            debugmsg( "Bionic operation lost its autodoc" );
            who.remove_effect( efftype_id( "under_operation" ) );
            act.set_to_null();
            return;
        }
        se.origin = bub_to_abs( autodocs.front() );
        se.volume = 60;
        se.category = sounds::sound_t::music;
        se.id = "Autodoc";
        if( success > 0 ) {
            add_msg( m_good,
                     _( "The Autodoc returns to its resting position after successfully performing the operation." ) );
            se.description = _( "a short upbeat jingle: \"Operation successful\"" );
            se.variant = "success";
        } else {
            se.variant = "failure";
            if( op_type == "install" ) {
                add_msg( m_warning,
                         _( "The Autodoc completes installation and activates bionic but reports about complications during operation." ) );
                se.description =
                    _( "a sad beeping noise: \"Complications detected!  Report to medical personnel immediately!\"" );
            } else {
                add_msg( m_bad,
                         _( "The Autodoc jerks back to its resting position after failing the operation." ) );
                se.description = _( "a sad beeping noise: \"Operation failed\"" );
            }
        }
        sounds::sound( se );
    } else {
        if( success > 0 ) {
            add_msg( m_good,
                     _( "The operation is a success." ) );
        } else {
            if( op_type == "install" ) {
                add_msg( m_warning,
                         _( "Bionic was installed and activated but a complication happened during operation!" ) );
            } else {
                add_msg( m_bad,
                         _( "The operation is a failure." ) );
            }
        }
    }
    who.remove_effect( efftype_id( "under_operation" ) );
    act.set_to_null();
}

// ─── gunmod_add_actor ────────────────────────────────────────────────────────

gunmod_add_actor::gunmod_add_actor( int r, int rk, int q,
                                    const std::string &tool, safe_reference<item> gun_ref, safe_reference<item> mod_ref )
    : roll( r ), risk( rk ), qty( q ), tool_name( tool ),
      gun( std::move( gun_ref ) ), mod( std::move( mod_ref ) ) {}
void gunmod_add_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "progress", activity_actor::progress );
    jsout.member( "roll", roll );
    jsout.member( "risk", risk );
    jsout.member( "qty", qty );
    jsout.member( "tool_name", tool_name );
    jsout.member( "gun", gun );
    jsout.member( "mod", mod );
    jsout.end_object();
}
std::unique_ptr<activity_actor> gunmod_add_actor::deserialize( JsonIn &jsin )
{
    auto actor = std::make_unique<gunmod_add_actor>();
    JsonObject data = jsin.get_object();
    data.read( "progress", actor->activity_actor::progress );
    data.read( "roll", actor->roll );
    data.read( "risk", actor->risk );
    data.read( "qty", actor->qty );
    data.read( "tool_name", actor->tool_name );
    data.read( "gun", actor->gun );
    data.read( "mod", actor->mod );
    return actor;
}
std::unique_ptr<activity_actor> gunmod_add_actor::legacy_deserialize( const JsonObject &data )
{
    auto actor = std::make_unique<gunmod_add_actor>();
    auto values = data.get_int_array( "values" );
    if( values.size() >= 4 ) {
        actor->roll = values[1];
        actor->risk = values[2];
        actor->qty = values[3];
    }
    actor->tool_name = data.get_string( "name" );
    auto targets = std::vector<safe_reference<item>>();
    data.read( "targets", targets );
    if( targets.size() >= 1 ) { actor->gun = std::move( targets[0] ); }
    if( targets.size() >= 2 ) { actor->mod = std::move( targets[1] ); }
    restore_legacy_progress( *actor, data, "installing gunmod" );
    return actor;
}
void gunmod_add_actor::start( player_activity &, Character & ) {}
void gunmod_add_actor::do_turn( player_activity &, Character & ) {}

void gunmod_add_actor::finish( player_activity &act, Character &who )
{

    if( !gun || !mod ) {
        debugmsg( "Lost gun or gunmod during ACT_GUNMOD_ADD" );
        act.set_to_null();
        return;
    }
    act.set_to_null();

    item &gun = *this->gun;
    item &mod = *this->mod;

    if( !gun.is_gunmod_compatible( mod ).success() ) {
        debugmsg( "Invalid arguments in ACT_GUNMOD_ADD" );
        return;
    }

    const itype_id tool( tool_name );
    if( !tool.is_empty() && qty > 0 ) {
        who.use_charges( tool, qty );
    }

    if( rng( 0, 100 ) <= roll ) {
        add_msg( m_good, _( "You successfully attached the %1$s to your %2$s." ), mod.tname(),
                 gun.tname() );
        gun.put_in( mod.detach() );

    } else if( rng( 0, 100 ) <= risk ) {
        if( gun.inc_damage() ) {
            // Remove irremovable mods prior to destroying the gun
            for( item *mod : gun.gunmods() ) {
                if( mod->is_irremovable() ) {
                    who.remove_item( *mod );
                }
            }
            add_msg( m_bad, _( "You failed at installing the %s and destroyed your %s!" ), mod.tname(),
                     gun.tname() );
            gun.detach();
        } else {
            add_msg( m_bad, _( "You failed at installing the %s and damaged your %s!" ), mod.tname(),
                     gun.tname() );
        }

    } else {
        add_msg( m_info, _( "You failed at installing the %s." ), mod.tname() );
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch tables
// ─────────────────────────────────────────────────────────────────────────────

namespace activity_actors
{

// Please keep this alphabetically sorted
const std::unordered_map<activity_id, std::unique_ptr<activity_actor>( * )( JsonIn & )>
deserialize_functions = {
    { activity_id( "ACT_AIM" ), &aim_activity_actor::deserialize },
    { activity_id( "ACT_AUTODRIVE" ), &autodrive_activity_actor::deserialize },
    { activity_id( "ACT_BOLTCUTTING" ), &boltcutting_activity_actor::deserialize },
    { activity_id( "ACT_BUILD" ), &construction_activity_actor::deserialize },
    { activity_id( "ACT_CRAFT" ), &craft_activity_actor::deserialize },
    { activity_id( "ACT_DIG" ), &dig_activity_actor::deserialize },
    { activity_id( "ACT_FIRSTAID" ), &firstaid_actor::deserialize },
    { activity_id( "ACT_FETCH_REQUIRED" ), &fetch_required_actor::deserialize },
    { activity_id( "ACT_FIELD_DRESS" ), &butcher_actor::deserialize },
    { activity_id( "ACT_GUNMOD_ADD" ), &gunmod_add_actor::deserialize },
    { activity_id( "ACT_MOVE_LOOT" ), &move_loot_activity_actor::deserialize },
    { activity_id( "ACT_OPERATION" ), &operation_actor::deserialize },
    { activity_id( "ACT_DIG_CHANNEL" ), &dig_channel_activity_actor::deserialize },
    { activity_id( "ACT_DISASSEMBLE" ), &disassemble_activity_actor::deserialize },
    { activity_id( "ACT_DISMEMBER" ), &butcher_actor::deserialize },
    { activity_id( "ACT_DISSECT" ), &butcher_actor::deserialize },
    { activity_id( "ACT_DROP" ), &drop_activity_actor::deserialize },
    { activity_id( "ACT_HACKING" ), &hacking_activity_actor::deserialize },
    { activity_id( "ACT_HACKSAW" ), &hacksaw_activity_actor::deserialize },
    { activity_id( "ACT_LOCKPICK" ), &lockpick_activity_actor::deserialize },
    { activity_id( "ACT_MIGRATION_CANCEL" ), &migration_cancel_activity_actor::deserialize },
    { activity_id( "ACT_MOVE_ITEMS" ), &move_items_activity_actor::deserialize },
    { activity_id( "ACT_TOGGLE_GATE" ), &toggle_gate_activity_actor::deserialize },
    { activity_id( "ACT_OXYTORCH" ), &oxytorch_activity_actor::deserialize },
    { activity_id( "ACT_PICKUP" ), &pickup_activity_actor::deserialize },
    { activity_id( "ACT_READ" ), &read_activity_actor::deserialize },
    { activity_id( "ACT_SHEAR" ), &shear_actor::deserialize },
    { activity_id( "ACT_SOCIALIZE" ), &socialize_actor::deserialize },
    { activity_id( "ACT_START_ENGINES" ), &start_engines_actor::deserialize },
    { activity_id( "ACT_START_FIRE" ), &start_fire_actor::deserialize },
    { activity_id( "ACT_STASH" ), &stash_activity_actor::deserialize },
    { activity_id( "ACT_STUDY_SPELL" ), &study_spell_actor::deserialize },
    { activity_id( "ACT_THROW" ), &throw_activity_actor::deserialize },
    { activity_id( "ACT_TRAIN_SKILL" ), &train_skill_activity_actor::deserialize },
    { activity_id( "ACT_TRAIN" ), &train_actor::deserialize },
    { activity_id( "ACT_ASSIST" ), &assist_activity_actor::deserialize },
    { activity_id( "ACT_BLEED" ), &butcher_actor::deserialize },
    { activity_id( "ACT_BUTCHER" ), &butcher_actor::deserialize },
    { activity_id( "ACT_BUTCHER_FULL" ), &butcher_actor::deserialize },
    { activity_id( "ACT_CLEAR_RUBBLE" ), &clear_rubble_actor::deserialize },
    { activity_id( "ACT_FILL_LIQUID" ), &liquid_transfer_actor::deserialize },
    { activity_id( "ACT_HAND_CRANK" ), &hand_crank_charge_actor::deserialize },
    { activity_id( "ACT_HOTWIRE_CAR" ), &hotwire_car_actor::deserialize },
    { activity_id( "ACT_LONGSALVAGE" ), &salvage_activity_actor::deserialize },
    { activity_id( "ACT_ENCHANT" ), &enchant_activity_actor::deserialize },
    { activity_id( "ACT_MAKE_ZLAVE" ), &make_zlave_actor::deserialize },
    { activity_id( "ACT_MILK" ), &milk_actor::deserialize },
    { activity_id( "ACT_PLAY_WITH_PET" ), &play_with_pet_actor::deserialize },
    { activity_id( "ACT_PULP" ), &pulp_actor::deserialize },
    { activity_id( "ACT_QUARTER" ), &butcher_actor::deserialize },
    { activity_id( "ACT_REPAIR_ITEM" ), &repair_actor::deserialize },
    { activity_id( "ACT_SKIN" ), &butcher_actor::deserialize },
    { activity_id( "ACT_TRAIN_PET" ), &train_pet_actor::deserialize },
    { activity_id( "ACT_TREE_COMMUNION" ), &tree_communion_actor::deserialize },
    { activity_id( "ACT_VEHICLE" ), &vehicle_work_actor::deserialize },
    { activity_id( "ACT_WAIT_NPC" ), &wait_npc_actor::deserialize },
    { activity_id( "ACT_WAIT_STAMINA" ), &wait_stamina_actor::deserialize },
    { activity_id( "ACT_WEAR" ), &wear_actor::deserialize }
};

const std::unordered_map<activity_id, std::unique_ptr<activity_actor>( * )( const JsonObject & )>
legacy_deserialize_functions = {
    { activity_id( "ACT_CLEAR_RUBBLE" ), &clear_rubble_actor::legacy_deserialize },
    { activity_id( "ACT_CRAFT" ), &craft_activity_actor::legacy_deserialize },
    { activity_id( "ACT_FETCH_REQUIRED" ), &fetch_required_actor::legacy_deserialize },
    { activity_id( "ACT_FIELD_DRESS" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_FILL_LIQUID" ), &liquid_transfer_actor::legacy_deserialize },
    { activity_id( "ACT_HAND_CRANK" ), &hand_crank_charge_actor::legacy_deserialize },
    { activity_id( "ACT_MOVE_LOOT" ), &move_loot_activity_actor::legacy_deserialize },
    { activity_id( "ACT_OPERATION" ), &operation_actor::legacy_deserialize },
    { activity_id( "ACT_READ" ), &read_activity_actor::legacy_deserialize },
    { activity_id( "ACT_REPAIR_ITEM" ), &repair_actor::legacy_deserialize },
    { activity_id( "ACT_VEHICLE" ), &vehicle_work_actor::legacy_deserialize },
    { activity_id( "ACT_WAIT_NPC" ), &wait_npc_actor::legacy_deserialize },
    { activity_id( "ACT_WAIT_STAMINA" ), &wait_stamina_actor::legacy_deserialize },
    { activity_id( "ACT_WEAR" ), &wear_actor::legacy_deserialize },
    { activity_id( "ACT_FIRSTAID" ), &firstaid_actor::legacy_deserialize },
    { activity_id( "ACT_GUNMOD_ADD" ), &gunmod_add_actor::legacy_deserialize },
    { activity_id( "ACT_HACKING" ), &hacking_activity_actor::legacy_deserialize },
    { activity_id( "ACT_HOTWIRE_CAR" ), &hotwire_car_actor::legacy_deserialize },
    { activity_id( "ACT_MAKE_ZLAVE" ), &make_zlave_actor::legacy_deserialize },
    { activity_id( "ACT_MILK" ), &milk_actor::legacy_deserialize },
    { activity_id( "ACT_PLAY_WITH_PET" ), &play_with_pet_actor::legacy_deserialize },
    { activity_id( "ACT_PULP" ), &pulp_actor::legacy_deserialize },
    { activity_id( "ACT_SHEAR" ), &shear_actor::legacy_deserialize },
    { activity_id( "ACT_SOCIALIZE" ), &socialize_actor::legacy_deserialize },
    { activity_id( "ACT_START_ENGINES" ), &start_engines_actor::legacy_deserialize },
    { activity_id( "ACT_START_FIRE" ), &start_fire_actor::legacy_deserialize },
    { activity_id( "ACT_STUDY_SPELL" ), &study_spell_actor::legacy_deserialize },
    { activity_id( "ACT_TRAIN_SKILL" ), &train_skill_activity_actor::legacy_deserialize },
    { activity_id( "ACT_TRAIN" ), &train_actor::legacy_deserialize },
    { activity_id( "ACT_TRAIN_PET" ), &train_pet_actor::legacy_deserialize },
    { activity_id( "ACT_TREE_COMMUNION" ), &tree_communion_actor::legacy_deserialize },
    { activity_id( "ACT_BLEED" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_BUTCHER" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_BUTCHER_FULL" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_DISMEMBER" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_DISSECT" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_FIELD_DRESS" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_QUARTER" ), &butcher_actor::legacy_deserialize },
    { activity_id( "ACT_SKIN" ), &butcher_actor::legacy_deserialize }
};
} // namespace activity_actors

void serialize( const std::unique_ptr<activity_actor> &actor, JsonOut &jsout )
{
    if( !actor ) {
        jsout.write_null();
    } else {
        jsout.start_object();

        jsout.member( "actor_type", actor->get_type() );
        jsout.member( "actor_data", *actor );

        jsout.end_object();
    }
}

void deserialize( std::unique_ptr<activity_actor> &actor, JsonIn &jsin )
{
    if( jsin.test_null() ) {
        actor = nullptr;
    } else {
        JsonObject data = jsin.get_object();
        if( data.has_member( "actor_data" ) ) {
            activity_id actor_type;
            data.read( "actor_type", actor_type );
            auto deserializer = activity_actors::deserialize_functions.find( actor_type );
            if( deserializer != activity_actors::deserialize_functions.end() ) {
                actor = deserializer->second( *data.get_raw( "actor_data" ) );
            } else {
                debugmsg( "Failed to find activity actor deserializer for type \"%s\"", actor_type.c_str() );
                actor = nullptr;
            }
        } else {
            debugmsg( "Failed to load activity actor" );
            actor = nullptr;
        }
    }
}
