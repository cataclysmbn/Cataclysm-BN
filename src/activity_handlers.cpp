#include "activity_handlers.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <weighted_list.h>

#include "pathfinding.h"
#include "action.h"
#include "action_time_scale.h"
#include "activity_actor_definitions.h"
#include "advanced_inv.h"
#include "armor_layers.h"
#include "avatar.h"
#include "avatar_action.h"
#include "bionics.h"
#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "character_functions.h"
#include "character_martial_arts.h"
#include "clzones.h"
#include "color.h"
#include "construction.h"
#include "construction_partial.h"
#include "coordinates.h"
#include "craft_command.h"
#include "crafting.h"
#include "crafting_quality.h"
#include "creature.h"
#include "damage.h"
#include "debug.h"
// TODO (https://github.com/cataclysmbn/Cataclysm-BN/issues/1612):
// Remove that include after implementing repair_activity_actor.
#include "distribution_grid.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "fault.h"
#include "field_type.h"
#include "fstream_utils.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "handle_liquid.h"
#include "harvest.h"
#include "iexamine.h"
#include "int_id.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "item_group.h"
#include "itype.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "line.h"
#include "magic/magic.h"
#include "material.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "martialarts.h"
#include "messages.h"
#include "mongroup.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "npc.h"
#include "omdata.h"
#include "output.h"
#include "overmapbuffer.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "ranged.h"
#include "recipe.h"
#include "requirements.h"
#include "ret_val.h"
#include "rng.h"
#include "skill.h"
#include "sounds.h"
#include "units.h"
#include "magic/spell_targeting.h"
#include "string_formatter.h"
#include "string_id.h"
#include "text_snippets.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "veh_interact.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "string_utils.h"

enum creature_size : int;

static const activity_id ACT_ADV_INVENTORY( "ACT_ADV_INVENTORY" );
static const activity_id ACT_ARMOR_LAYERS( "ACT_ARMOR_LAYERS" );
static const activity_id ACT_ATM( "ACT_ATM" );
static const activity_id ACT_BLEED( "ACT_BLEED" );
static const activity_id ACT_BURROW( "ACT_BURROW" );
static const activity_id ACT_BUTCHER( "ACT_BUTCHER" );
static const activity_id ACT_BUTCHER_FULL( "ACT_BUTCHER_FULL" );
static const activity_id ACT_CHOP_LOGS( "ACT_CHOP_LOGS" );
static const activity_id ACT_CHOP_PLANKS( "ACT_CHOP_PLANKS" );
static const activity_id ACT_CHOP_TREE( "ACT_CHOP_TREE" );
static const activity_id ACT_CHURN( "ACT_CHURN" );
static const activity_id ACT_CLEAR_RUBBLE( "ACT_CLEAR_RUBBLE" );
static const activity_id ACT_CONSUME_DRINK_MENU( "ACT_CONSUME_DRINK_MENU" );
static const activity_id ACT_CONSUME_FOOD_MENU( "ACT_CONSUME_FOOD_MENU" );
static const activity_id ACT_CONSUME_MEDS_MENU( "ACT_CONSUME_MEDS_MENU" );
static const activity_id ACT_CRACKING( "ACT_CRACKING" );
static const activity_id ACT_CRAFT( "ACT_CRAFT" );
static constexpr auto craft_is_long_idx = 0;
static constexpr auto craft_bench_type_idx = 1;
static constexpr auto craft_tools_mult_percent_idx = 2;
static constexpr auto craft_tools_mult_next_refresh_idx = 3;
static const activity_id ACT_DISMEMBER( "ACT_DISMEMBER" );
static const activity_id ACT_DISSECT( "ACT_DISSECT" );
static const activity_id ACT_EAT_MENU( "ACT_EAT_MENU" );
static const activity_id ACT_FERTILIZE_PLOT( "ACT_FERTILIZE_PLOT" );
static const activity_id ACT_FETCH_REQUIRED( "ACT_FETCH_REQUIRED" );
static const activity_id ACT_FIELD_DRESS( "ACT_FIELD_DRESS" );
static const activity_id ACT_MILK( "ACT_MILK" );
static const activity_id ACT_FILL_PIT( "ACT_FILL_PIT" );
static const activity_id ACT_FIND_MOUNT( "ACT_FIND_MOUNT" );
static const activity_id ACT_FISH( "ACT_FISH" );
static const activity_id ACT_FORAGE( "ACT_FORAGE" );
static const activity_id ACT_GAME( "ACT_GAME" );
static const activity_id ACT_GENERIC_GAME( "ACT_GENERIC_GAME" );
static const activity_id ACT_HAIRCUT( "ACT_HAIRCUT" );
static const activity_id ACT_HAND_CRANK( "ACT_HAND_CRANK" );
static const activity_id ACT_HOTWIRE_CAR( "ACT_HOTWIRE_CAR" );
static const activity_id ACT_JACKHAMMER( "ACT_JACKHAMMER" );
static const activity_id ACT_MAKE_ZLAVE( "ACT_MAKE_ZLAVE" );
static const activity_id ACT_MEDITATE( "ACT_MEDITATE" );
static const activity_id ACT_MEND_ITEM( "ACT_MEND_ITEM" );
static const activity_id ACT_MIND_SPLICER( "ACT_MIND_SPLICER" );
static const activity_id ACT_MOVE_LOOT( "ACT_MOVE_LOOT" );
static const activity_id ACT_MULTIPLE_BUTCHER( "ACT_MULTIPLE_BUTCHER" );
static const activity_id ACT_MULTIPLE_CHOP_PLANKS( "ACT_MULTIPLE_CHOP_PLANKS" );
static const activity_id ACT_MULTIPLE_CHOP_TREES( "ACT_MULTIPLE_CHOP_TREES" );
static const activity_id ACT_MULTIPLE_CONSTRUCTION( "ACT_MULTIPLE_CONSTRUCTION" );
static const activity_id ACT_MULTIPLE_MINE( "ACT_MULTIPLE_MINE" );
static const activity_id ACT_MULTIPLE_FARM( "ACT_MULTIPLE_FARM" );
static const activity_id ACT_MULTIPLE_FISH( "ACT_MULTIPLE_FISH" );
static const activity_id ACT_OPERATION( "ACT_OPERATION" );
static const activity_id ACT_PICKAXE( "ACT_PICKAXE" );
static const activity_id ACT_PLANT_SEED( "ACT_PLANT_SEED" );
static const activity_id ACT_PLAY_WITH_PET( "ACT_PLAY_WITH_PET" );
static const activity_id ACT_TRAIN_PET( "ACT_TRAIN_PET" );
static const activity_id ACT_PRY_NAILS( "ACT_PRY_NAILS" );
static const activity_id ACT_QUARTER( "ACT_QUARTER" );
static const activity_id ACT_READ( "ACT_READ" );
static const activity_id ACT_RELOAD( "ACT_RELOAD" );
static const activity_id ACT_REPAIR_ITEM( "ACT_REPAIR_ITEM" );
static const activity_id ACT_ROBOT_CONTROL( "ACT_ROBOT_CONTROL" );
static const activity_id ACT_SHAVE( "ACT_SHAVE" );
static const activity_id ACT_SKIN( "ACT_SKIN" );
static const activity_id ACT_SOCIALIZE( "ACT_SOCIALIZE" );
static const activity_id ACT_SPELLCASTING( "ACT_SPELLCASTING" );
static const activity_id ACT_START_ENGINES( "ACT_START_ENGINES" );
static const activity_id ACT_TIDY_UP( "ACT_TIDY_UP" );
static const activity_id ACT_TOOLMOD_ADD( "ACT_TOOLMOD_ADD" );
static const activity_id ACT_TRAIN( "ACT_TRAIN" );
static const activity_id ACT_TRAVELLING( "ACT_TRAVELLING" );
static const activity_id ACT_TRY_SLEEP( "ACT_TRY_SLEEP" );
static const activity_id ACT_VEHICLE( "ACT_VEHICLE" );
static const activity_id ACT_VEHICLE_DECONSTRUCTION( "ACT_VEHICLE_DECONSTRUCTION" );
static const activity_id ACT_VEHICLE_REPAIR( "ACT_VEHICLE_REPAIR" );
static const activity_id ACT_VIBE( "ACT_VIBE" );
static const activity_id ACT_WAIT( "ACT_WAIT" );
static const activity_id ACT_WAIT_NPC( "ACT_WAIT_NPC" );
static const activity_id ACT_WAIT_STAMINA( "ACT_WAIT_STAMINA" );
static const activity_id ACT_WAIT_WEATHER( "ACT_WAIT_WEATHER" );
static const activity_id ACT_WASH_SELF( "ACT_WASH_SELF" );
static const activity_id ACT_WEAR( "ACT_WEAR" );

static const efftype_id effect_ai_waiting( "ai_waiting" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_sheared( "sheared" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_tied( "tied" );
static const efftype_id effect_under_op( "under_operation" );
static const efftype_id effect_well_fed( "well_fed" );

static const fault_id fault_bionic_nonsterile( "fault_bionic_nonsterile" );

static const itype_id itype_2x4( "2x4" );
static const itype_id itype_animal( "animal" );
static const itype_id itype_battery( "battery" );
static const itype_id itype_burnt_out_bionic( "burnt_out_bionic" );
static const itype_id itype_grapnel( "grapnel" );
static const itype_id itype_hd_tow_cable( "hd_tow_cable" );
static const itype_id itype_log( "log" );
static const itype_id itype_mind_scan_robofac( "mind_scan_robofac" );
static const itype_id itype_muscle( "muscle" );
static const itype_id itype_nail( "nail" );
static const itype_id itype_rope_30( "rope_30" );
static const itype_id itype_rope_makeshift_30( "rope_makeshift_30" );
static const itype_id itype_splinter( "splinter" );
static const itype_id itype_stick_long( "stick_long" );
static const itype_id itype_vine_30( "vine_30" );
static const itype_id itype_wool_staple( "wool_staple" );

static const zone_type_id zone_type_FARM_PLOT( "FARM_PLOT" );

static const skill_id skill_computer( "computer" );
static const skill_id skill_electronics( "electronics" );
static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_firstaid( "firstaid" );
static const skill_id skill_mechanics( "mechanics" );
static const skill_id skill_survival( "survival" );

static const quality_id qual_BUTCHER( "BUTCHER" );
static const quality_id qual_CUT_FINE( "CUT_FINE" );

static const species_id HUMAN( "HUMAN" );
static const species_id ZOMBIE( "ZOMBIE" );

static const trait_flag_str_id trait_flag_CANNIBAL( "CANNIBAL" );
static const trait_flag_str_id trait_flag_PSYCHOPATH( "PSYCHOPATH" );
static const trait_flag_str_id trait_flag_SAPIOVORE( "SAPIOVORE" );

static const bionic_id bio_painkiller( "bio_painkiller" );

static const itype_id itype_UPS( "UPS" );

static const trait_id trait_NOPAIN( "NOPAIN" );
static const trait_id trait_SPIRITUAL( "SPIRITUAL" );
static const trait_id trait_STOCKY_TROGLO( "STOCKY_TROGLO" );

// not to confuse with item flags (json_flag)
static const std::string flag_AUTODOC( "AUTODOC" );
static const std::string flag_AUTODOC_COUCH( "AUTODOC_COUCH" );
static const std::string flag_BUTCHER_EQ( "BUTCHER_EQ" );
static const std::string flag_PLANTABLE( "PLANTABLE" );
static const std::string flag_TREE( "TREE" );

using namespace activity_handlers;

const std::map< activity_id, std::function<void( player_activity *, player * )> >
activity_handlers::do_turn_functions = {
    { ACT_BURROW, burrow_do_turn },
    // craft_do_turn — moved into craft_activity_actor::do_turn()
    { ACT_PICKAXE, pickaxe_do_turn },
    { ACT_GAME, game_do_turn },
    { ACT_GENERIC_GAME, generic_game_do_turn },
    { ACT_VIBE, vibe_do_turn },
    { ACT_MULTIPLE_FISH, multiple_fish_do_turn },
    { ACT_MULTIPLE_CONSTRUCTION, multiple_construction_do_turn },
    { ACT_MULTIPLE_MINE, multiple_mine_do_turn },
    { ACT_MULTIPLE_BUTCHER, multiple_butcher_do_turn },
    { ACT_MULTIPLE_FARM, multiple_farm_do_turn },
    // fetch_do_turn — moved into fetch_required_actor::do_turn()
    { ACT_EAT_MENU, eat_menu_do_turn },
    { ACT_VEHICLE_DECONSTRUCTION, vehicle_deconstruction_do_turn },
    { ACT_VEHICLE_REPAIR, vehicle_repair_do_turn },
    { ACT_MULTIPLE_CHOP_TREES, chop_trees_do_turn },
    { ACT_CONSUME_FOOD_MENU, consume_food_menu_do_turn },
    { ACT_CONSUME_DRINK_MENU, consume_drink_menu_do_turn },
    { ACT_CONSUME_MEDS_MENU, consume_meds_menu_do_turn },
    { ACT_ADV_INVENTORY, adv_inventory_do_turn },
    { ACT_ARMOR_LAYERS, armor_layers_do_turn },
    { ACT_ATM, atm_do_turn },
    { ACT_CRACKING, cracking_do_turn },
    { ACT_FISH, fish_do_turn },
    // repair_item_do_turn — moved into repair_actor::do_turn()
    { ACT_TRAVELLING, travel_do_turn },
    { ACT_PRY_NAILS, pry_nails_do_turn },
    { ACT_CHOP_TREE, chop_tree_do_turn },
    { ACT_CHOP_LOGS, chop_tree_do_turn },
    { ACT_CHOP_PLANKS, chop_tree_do_turn },
    { ACT_TIDY_UP, tidy_up_do_turn },
    { ACT_JACKHAMMER, jackhammer_do_turn },
    { ACT_FIND_MOUNT, find_mount_do_turn },
    { ACT_FILL_PIT, fill_pit_do_turn },
    { ACT_MULTIPLE_CHOP_PLANKS, multiple_chop_planks_do_turn },
    { ACT_FERTILIZE_PLOT, fertilize_plot_do_turn },
    { ACT_TRY_SLEEP, try_sleep_do_turn },
    // operation_do_turn — moved into operation_actor::do_turn()
    { ACT_ROBOT_CONTROL, robot_control_do_turn },
};


const std::map< activity_id, std::function<void( player_activity *, player * )> >
activity_handlers::finish_functions = {
    { ACT_BURROW, burrow_finish },
    { ACT_FISH, fish_finish },
    { ACT_FORAGE, forage_finish },
    { ACT_PICKAXE, pickaxe_finish },
    { ACT_RELOAD, reload_finish },
    // train_finish — moved into train_actor::finish()
    { ACT_CHURN, churn_finish },
    { ACT_PLANT_SEED, plant_seed_finish },
    // vehicle_finish — moved into vehicle_work_actor::finish()
    { ACT_CRACKING, cracking_finish },
    // repair_item_finish — MOVED INTO repair_actor::finish()
    { ACT_MEND_ITEM, mend_item_finish },
    { ACT_TOOLMOD_ADD, toolmod_add_finish },
    { ACT_MEDITATE, meditate_finish },
    { ACT_WAIT, wait_finish },
    { ACT_WAIT_WEATHER, wait_weather_finish },
    { ACT_TRY_SLEEP, try_sleep_finish },
    // operation_finish — moved into operation_actor::finish()
    { ACT_VIBE, vibe_finish },
    { ACT_ATM, atm_finish },
    { ACT_EAT_MENU, eat_menu_finish },
    { ACT_CONSUME_FOOD_MENU, eat_menu_finish },
    { ACT_CONSUME_DRINK_MENU, eat_menu_finish },
    { ACT_CONSUME_MEDS_MENU, eat_menu_finish },
    { ACT_PRY_NAILS, pry_nails_finish },
    { ACT_CHOP_TREE, chop_tree_finish },
    { ACT_CHOP_LOGS, chop_logs_finish },
    { ACT_CHOP_PLANKS, chop_planks_finish },
    { ACT_JACKHAMMER, jackhammer_finish },
    { ACT_FILL_PIT, fill_pit_finish },
    { ACT_SHAVE, shaving_finish },
    { ACT_HAIRCUT, haircut_finish },
    { ACT_ROBOT_CONTROL, robot_control_finish },
    { ACT_MIND_SPLICER, mind_splicer_finish },
    { ACT_SPELLCASTING, spellcasting_finish }
};

bool activity_handlers::resume_for_multi_activities( Character &who )
{
    if( !who.backlog.empty() ) {
        activity_ptr &back_act = who.backlog.front();
        if( back_act->is_multi_type() ) {
            who.assign_activity( who.backlog.front()->id() );
            who.backlog.clear();
            return true;
        }
    }
    return false;
}

void activity_handlers::burrow_do_turn( player_activity *act, player *p )
{
    sfx::play_activity_sound( "activity", "burrow",
                              sfx::get_heard_volume( abs_to_bub( act->placement ), 70 ) );
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        sound_event se;
        se.origin = act->placement;
        se.volume = 65;
        se.category = sounds::sound_t::movement;
        se.description = _( "ScratchCrunchScrabbleScurry." ); //~ Sound of a Rat mutant burrowing!
        se.id = "activity";
        se.variant = "burrow";
        se.from_player = p->is_avatar();
        se.from_npc = !se.from_player;
        se.faction = p->get_faction()->id;
        se.monfaction = p->get_faction()->mon_faction;
        sounds::sound( se );
    }
}

void activity_handlers::burrow_finish( player_activity *act, player *p )
{
    auto pos = act->placement; // make a copy to avoid use-after-free
    map &here = get_map();
    if( p->is_avatar() ) {
        int act_exertion = act->moves_total;
        // Impossible in vanilla since competing thresholds, but allowed in case of mods
        if( p->has_trait( trait_STOCKY_TROGLO ) ) {
            act_exertion /= 2;
        }
        // Base cost of 1 fatigue per 3 minutes, or 20 fatigue at 8 strength since 60 minutes
        // Strength, terrain, and helpers accounted for by time calculation
        p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 45_seconds ) ) );
        p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
        p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 3_minutes ) ) );
        p->mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 5_seconds ) ) );
    }
    act->set_to_null();
    p->add_msg_if_player( m_good, _( "You finish burrowing." ) );
    here.destroy( abs_to_bub( pos ), true );
}

static bool check_butcher_cbm( const int roll )
{
    // Success rate for dissection rolls, simple percentage roll
    // +10% per fine cutting quality, +10% per 2 levels of first aid and electronics
    // Additional, small randomized bonus/penalty if dexterity is above/below average
    // Roll is reduced by corpse damage level (up to -4), minimum of 10% success chance
    add_msg( m_debug, _( "Roll = %i" ), roll );
    add_msg( m_debug, _( "Success chance = %i%%" ), std::min( 100, ( roll * 10 ) ) );
    const bool success = x_in_y( roll, 10 );
    return success;
}

void extract_or_wreck_cbms( std::vector<detached_ptr<item>> &cbms, int roll,
                            Character &who )
{
    if( roll < 0 ) {
        return;
    }
    for( detached_ptr<item> &it : cbms ) {
        // For some stupid reason, zombie pheromones are dropped using bionic type
        // This complicates things
        if( it->is_bionic() ) {
            if( check_butcher_cbm( roll ) || it->typeId() == itype_burnt_out_bionic ) {
                if( it->has_flag( flag_BIONIC_FAULTY ) ) {
                    it->convert( itype_burnt_out_bionic );
                    // We don't need the non-sterile fault on a piece of burnt-out bionic
                    if( it->has_fault( fault_bionic_nonsterile ) ) {
                        it->faults.erase( fault_bionic_nonsterile );
                    }
                }
                add_msg( m_good, _( "You discover: %s!" ), it->tname() );
            } else {
                it->convert( itype_burnt_out_bionic );
                if( it->has_fault( fault_bionic_nonsterile ) ) {
                    it->faults.erase( fault_bionic_nonsterile );
                }
                add_msg( m_bad, _( "Your imprecise surgery damaged a bionic, producing a %s." ), it->tname() );
            }
        } else {
            if( !check_butcher_cbm( roll ) ) {
                add_msg( m_bad, _( "Your imprecise surgery destroyed something." ) );
                continue;
            } else {
                // If we have non-bionic loot in a harvest's bionic_group it doesn't need to be marked non-sterile either.
                if( it->has_fault( fault_bionic_nonsterile ) ) {
                    it->faults.erase( fault_bionic_nonsterile );
                }
                add_msg( m_good, _( "You discover: %s!" ), it->tname() );
            }
        }

        if( it->type->phase == LIQUID ) {
            // TODO: smarter NPC liquid handling
            if( who.is_npc() ) {
                drop_on_map( who, item_drop_reason::deliberate, { std::move( it ) }, who.bub_pos() );
            } else {
                liquid_handler::handle_all_liquid( std::move( it ), PICKUP_RANGE );
            }
        } else {
            get_map().add_item( who.bub_pos(), std::move( it ) );
        }
    }
}

// TODO: Implement
enum class butcherable_rating : int {
    butcherable = 0,
    no_tool,
    no_tree_rope_rack,
    no_table,
    no_saw,
    too_damaged,
    already_dressed,
    already_skinned,
    already_bled,
    already_quartered,
    too_small,
    needs_dressing,
    warn_cannibalism,
    info_tools
};

butchery_setup consider_butchery( const item &corpse_item, player &u, butcher_type action )
{
    map &here = get_map();
    butchery_setup setup;
    setup.can_do = butchery_possibility::yes;
    setup.type = action;
    const auto wont_do = [&setup]( const std::string & msg, butcherable_rating ) {
        setup.problems.emplace_back( msg );
        setup.can_do = butchery_possibility::never;
    };
    const auto not_this_one = [&setup]( const std::string & msg, butcherable_rating ) {
        setup.problems.emplace_back( msg );
        if( setup.can_do != butchery_possibility::never ) {
            setup.can_do = butchery_possibility::not_this;
        }
    };
    const auto info = [&setup]( const std::string & msg, butcherable_rating ) {
        setup.info.emplace_back( msg );
    };
    const auto need_confirm = [&setup]( const std::string & msg, butcherable_rating ) {
        setup.problems.emplace_back( msg );
        if( setup.can_do == butchery_possibility::yes ) {
            setup.can_do = butchery_possibility::need_confirmation;
        }
    };

    const inventory &inv = u.crafting_inventory();
    const int factor = inv.max_quality( action == DISSECT ? qual_CUT_FINE : qual_BUTCHER );

    const mtype &corpse = *corpse_item.get_mtype();

    if( action != DISSECT && factor == INT_MIN ) {
        wont_do( _( "None of your cutting tools are suitable for butchering." ),
                 butcherable_rating::no_tool );
    }

    if( action == DISSECT ) {
        switch( factor ) {
            case INT_MIN:
                wont_do( _( "None of your tools are sharp and precise enough to do that." ),
                         butcherable_rating::no_tool );
                break;
            case 1:
                info( _( "You could use a better tool, but this will do." ), butcherable_rating::info_tools );
                break;
            case 2:
                info( _( "This tool is great, but you still would like a scalpel." ),
                      butcherable_rating::info_tools );
                break;
            case 3:
                info( _( "You dissect the corpse with a trusty scalpel." ), butcherable_rating::info_tools );
                break;
            case 5:
                info( _( "You dissect the corpse with a sophisticated system of surgical grade scalpels." ),
                      butcherable_rating::info_tools );
                break;
        }
    }

    bool has_tree_nearby = false;
    for( const auto &pt : here.points_in_radius( u.bub_pos(), PICKUP_RANGE ) ) {
        if( here.has_flag( flag_TREE, pt ) ) {
            has_tree_nearby = true;
            break;
        }
    }
    bool b_rack_present = false;
    for( const auto &pt : here.points_in_radius( u.bub_pos(), PICKUP_RANGE ) ) {
        if( here.has_flag_furn( flag_BUTCHER_EQ, pt ) ) {
            b_rack_present = true;
            break;
        }
        //vehicle part
        const optional_vpart_position vp = here.veh_at( pt );
        if( !vp ) {
            continue;
        }
        vp->vehicle();
        if( vp.part_with_feature( "BUTCHER_EQ", true ) ) {
            b_rack_present = true;
            break;
        }
    }
    if( !b_rack_present ) {
        b_rack_present = inv.has_item_with( []( const item & it ) {
            return it.has_flag( flag_BUTCHER_RACK );
        } );
    }
    // workshop butchery (full) prequisites
    if( action == BUTCHER_FULL ) {
        const bool has_rope = inv.has_amount( itype_rope_30, 1 ) ||
                              inv.has_amount( itype_rope_makeshift_30, 1 ) ||
                              inv.has_amount( itype_hd_tow_cable, 1 ) ||
                              inv.has_amount( itype_vine_30, 1 ) ||
                              inv.has_amount( itype_grapnel, 1 );
        const bool big_corpse = corpse.size >= creature_size::medium;

        if( big_corpse ) {
            if( has_rope && !has_tree_nearby && !b_rack_present ) {
                not_this_one(
                    _( "You need to suspend this corpse to butcher it.  While you have a rope to lift the corpse, there is no tree nearby to hang it from." ),
                    butcherable_rating::no_tree_rope_rack );
            }
            if( !has_rope && !b_rack_present ) {
                not_this_one(
                    _( "To perform a full butchery on a corpse this big, you need either a butchering rack, a nearby hanging meathook, or both a long rope in your inventory and a nearby tree to hang the corpse from." ),
                    butcherable_rating::no_tree_rope_rack );
            }
            if( !( here.has_nearby_table( u.bub_pos(), PICKUP_RANGE ) ||
            inv.has_item_with( []( const item & it ) {
            return it.has_flag( flag_FLAT_SURFACE );
            } ) ) ) {
                not_this_one(
                    _( "To perform a full butchery on a corpse this big, you need a table nearby or something else with a flat surface.  A leather tarp spread out on the ground could suffice." ),
                    butcherable_rating::no_table );
            }
        }
    }

    if( action == DISSECT && ( corpse_item.has_flag( flag_QUARTERED ) ||
                               corpse_item.has_flag( flag_FIELD_DRESS_FAILED ) ) ) {
        not_this_one( _( "It would be futile to search for implants inside this badly damaged corpse." ),
                      butcherable_rating::too_damaged );
    }

    if( action == F_DRESS && ( corpse_item.has_flag( flag_FIELD_DRESS ) ||
                               corpse_item.has_flag( flag_FIELD_DRESS_FAILED ) ) ) {
        not_this_one( _( "This corpse is already field dressed." ),
                      butcherable_rating::already_dressed );
    }

    if( action == SKIN && corpse_item.has_flag( flag_SKINNED ) ) {
        not_this_one( _( "This corpse is already skinned." ), butcherable_rating::already_skinned );
    }

    if( action == QUARTER ) {
        if( corpse.size == creature_size::tiny ) {
            not_this_one( _( "This corpse is too small to quarter without damaging." ),
                          butcherable_rating::too_small );
        }
        if( corpse_item.has_flag( flag_QUARTERED ) ) {
            not_this_one( _( "This is already quartered." ), butcherable_rating::already_quartered );
        }
        if( !( corpse_item.has_flag( flag_FIELD_DRESS ) ||
               corpse_item.has_flag( flag_FIELD_DRESS_FAILED ) ) &&
            corpse_item.get_mtype()->harvest->has_entry_type( "offal" ) ) {
            not_this_one( _( "You need to perform field dressing before quartering." ),
                          butcherable_rating::needs_dressing );
        }
    }

    if( action == BLEED ) {
        if( corpse_item.has_flag( flag_BLED ) ) {
            not_this_one( _( "This has already been bled." ), butcherable_rating::already_bled );
        }
        if( ( corpse_item.has_flag( flag_FIELD_DRESS ) ||
              corpse_item.has_flag( flag_FIELD_DRESS_FAILED ) ) &&
            corpse_item.get_mtype()->harvest->has_entry_type( "offal" ) ) {
            not_this_one( _( "Field dressed corpses no longer have blood." ),
                          butcherable_rating::already_bled );
        }
        if( corpse_item.has_flag( flag_QUARTERED ) ) {
            not_this_one( _( "Quartered corpses no longer have blood." ), butcherable_rating::already_bled );
        }
    }

    // applies to all butchery actions
    const bool is_human = corpse.id == mtype_id::NULL_ID() || ( corpse.in_species( HUMAN ) &&
                          !corpse.in_species( ZOMBIE ) );
    if( is_human && !( u.has_trait_flag( trait_flag_CANNIBAL ) ||
                       u.has_trait_flag( trait_flag_PSYCHOPATH ) ||
                       u.has_trait_flag( trait_flag_SAPIOVORE ) ) ) {
        need_confirm( _( "Would you dare desecrate the mortal remains of a fellow human being?" ),
                      butcherable_rating::warn_cannibalism );
    }

    setup.move_cost = butcher_time_to_cut( corpse_item, action );

    return setup;
}

static int size_factor_in_time_to_cut( creature_size size )
{
    switch( size ) {
        // Time (roughly) in turns to cut up the corpse
        case creature_size::tiny:
            return 15000;
        case creature_size::small:
            return 30000;
        case creature_size::medium:
            return 45000;
        case creature_size::large:
            return 60000;
        case creature_size::huge:
            return 180000;
        default:
            debugmsg( "Invalid creature_size value for butchering corpse: %d", static_cast<int>( size ) );
            break;
    }
    return 0;
}

int butcher_time_to_cut( const item &corpse_item, const butcher_type action )
{
    const mtype &corpse = *corpse_item.get_mtype();
    int time_to_cut = size_factor_in_time_to_cut( corpse.size );

    switch( action ) {
        case BUTCHER:
        case BLEED:
            break;
        case BUTCHER_FULL:
            if( !corpse_item.has_flag( flag_FIELD_DRESS ) || corpse_item.has_flag( flag_FIELD_DRESS_FAILED ) ) {
                time_to_cut *= 6;
            } else {
                time_to_cut *= 4;
            }
            break;
        case F_DRESS:
        case SKIN:
            break;
        case QUARTER:
            time_to_cut = std::max( 1000, time_to_cut / 4 );
            break;
        case DISMEMBER:
            time_to_cut = std::max( 400, time_to_cut / 10 );
            break;
        case DISSECT:
            time_to_cut *= 4;
            break;
    }

    if( corpse_item.has_flag( flag_QUARTERED ) ) {
        time_to_cut /= 4;
    }
    return time_to_cut;
}

// this function modifies the input weight by its damage level, depending on the bodypart
static int corpse_damage_effect( int weight, const std::string &entry_type, int damage_level )
{
    const float slight_damage = 0.9;
    const float damage = 0.75;
    const float high_damage = 0.5;
    const int destroyed = 0;

    switch( damage_level ) {
        case 2:
            // "damaged"
            if( entry_type == "offal" ) {
                return std::round( weight * damage );
            }
            if( entry_type == "skin" ) {
                return std::round( weight * damage );
            }
            if( entry_type == "flesh" ) {
                return std::round( weight * slight_damage );
            }
            break;
        case 3:
            // "mangled"
            if( entry_type == "offal" ) {
                return destroyed;
            }
            if( entry_type == "skin" ) {
                return std::round( weight * high_damage );
            }
            if( entry_type == "bone" ) {
                return std::round( weight * slight_damage );
            }
            if( entry_type == "flesh" ) {
                return std::round( weight * damage );
            }
            break;
        case 4:
            // "pulped"
            if( entry_type == "offal" ) {
                return destroyed;
            }
            if( entry_type == "skin" ) {
                return destroyed;
            }
            if( entry_type == "bone" ) {
                return std::round( weight * damage );
            }
            if( entry_type == "flesh" ) {
                return std::round( weight * high_damage );
            }
            break;
        default:
            // "bruised" modifier is almost impossible to avoid; also includes no modifier (zero damage)
            break;
    }
    return weight;
}

void butchery_drops_harvest( item *corpse_item, const mtype &mt, Character &who,
                             const std::function<int()> &roll_butchery, butcher_type action,
                             const std::function<double()> &roll_drops )
{
    who.add_msg_if_player( m_neutral, mt.harvest->message() );
    int monster_weight = to_gram( mt.weight );
    monster_weight += std::round( monster_weight * rng_float( -0.1, 0.1 ) );
    if( corpse_item->has_flag( flag_QUARTERED ) ) {
        monster_weight *= 0.95;
    }
    if( corpse_item->has_flag( flag_GIBBED ) ) {
        monster_weight = std::round( 0.85 * monster_weight );
        if( action != F_DRESS ) {
            who.add_msg_if_player( m_bad,
                                   _( "You salvage what you can from the corpse, but it is badly damaged." ) );
        }
    }
    if( corpse_item->has_flag( flag_SKINNED ) ) {
        monster_weight = std::round( 0.85 * monster_weight );
    }
    if( corpse_item->has_flag( flag_BLED ) ) {
        monster_weight = std::round( 0.90 * monster_weight );
    }
    int practice = 4 + roll_butchery();

    if( mt.harvest.is_null() ) {
        debugmsg( "ERROR: %s has no harvest entry.", mt.id.c_str() );
        return;
    }

    map &here = get_map();
    for( const harvest_entry &entry : *mt.harvest ) {
        const int butchery = roll_butchery();
        const float min_num = entry.base_num.first + butchery * entry.scale_num.first;
        const float max_num = entry.base_num.second + butchery * entry.scale_num.second;
        int roll = 0;
        // mass_ratio will override the use of base_num, scale_num, and max
        if( entry.mass_ratio != 0.00f ) {
            roll = static_cast<int>( std::round( entry.mass_ratio * monster_weight ) );
            roll = corpse_damage_effect( roll, entry.type, corpse_item->damage_level( 4 ) );
        } else if( entry.type != "bionic" && entry.type != "bionic_group" ) {
            roll = std::min<int>( entry.max, std::round( rng_float( min_num, max_num ) ) );
            // will not give less than min_num defined in the JSON
            roll = std::max<int>( corpse_damage_effect( roll, entry.type, corpse_item->damage_level( 4 ) ),
                                  entry.base_num.first );
        }
        const itype *drop = nullptr;
        if( entry.type != "bionic_group" ) {
            drop = &*itype_id( entry.drop );
        }

        // BIONIC handling - no code for DISSECT to let the bionic drop fall through
        if( entry.type == "bionic" || entry.type == "bionic_group" ) {
            if( action == F_DRESS ) {
                if( drop != nullptr && !drop->bionic ) {
                    if( one_in( 3 ) ) {
                        who.add_msg_if_player( m_bad,
                                               _( "You notice something embedded in the corpse, perhaps harvestable via careful dissection." ) );
                    }
                    continue;
                }
                who.add_msg_if_player( m_bad,
                                       _( "You notice there are implants in this corpse, that careful dissection might preserve." ) );
                continue;
            }
            if( action == BUTCHER || action == BUTCHER_FULL || action == DISMEMBER ) {
                if( drop != nullptr && !drop->bionic ) {
                    if( one_in( 3 ) ) {
                        who.add_msg_if_player( m_bad,
                                               _( "Your butchering tool destroys something.  Perhaps a more surgical approach would allow harvesting it." ) );
                    }
                    continue;
                }
                switch( rng( 1, 3 ) ) {
                    case 1:
                        who.add_msg_if_player( m_bad,
                                               _( "Your butchering tool encounters something implanted in this corpse, but your rough cuts destroy it." ) );
                        break;
                    case 2:
                        who.add_msg_if_player( m_bad,
                                               _( "You find traces of implants in the body, but you care only for the flesh." ) );
                        break;
                    case 3:
                        who.add_msg_if_player( m_bad,
                                               _( "You found some implants in the body, but harvesting them would require more surgical approach." ) );
                        break;
                }
                continue;
            }
        }

        // Check if monster was gibbed, and handle accordingly
        if( corpse_item->has_flag( flag_GIBBED ) && ( entry.type == "flesh" || entry.type == "bone" ) ) {
            roll /= 2;
        }

        // Corpses that have been skinned, field dressed, or bleed do not yield that item anymore
        // Also ensure message does not mention blood if you're not bleeding the corpse
        const bool has_any_field_dressing = corpse_item->has_flag( flag_FIELD_DRESS ) ||
                                            corpse_item->has_flag( flag_FIELD_DRESS_FAILED ) || corpse_item->has_flag( flag_QUARTERED );
        const bool already_harvested = ( corpse_item->has_flag( flag_SKINNED ) && entry.type == "skin" ) ||
                                       ( has_any_field_dressing && entry.type == "offal" ) || ( ( has_any_field_dressing ||
                                               corpse_item->has_flag( flag_BLED ) || action != BLEED ) && entry.type == "blood" );
        if( already_harvested ) {
            roll = 0;
        }

        // QUICK BUTCHERY
        if( action == BUTCHER ) {
            if( entry.type == "flesh" ) {
                roll = roll / 4;
            } else if( entry.type == "bone" ) {
                roll /= 2;
            } else if( corpse_item->get_mtype()->size >= creature_size::medium && ( entry.type == "skin" ) ) {
                roll /= 2;
            } else if( entry.type == "offal" ) {
                roll /= 5;
            } else {
                continue;
            }
        }
        // RIP AND TEAR
        if( action == DISMEMBER ) {
            if( entry.type == "flesh" ) {
                roll /= 6;
            } else {
                continue;
            }
        }
        // field dressing ignores everything outside below list
        if( action == F_DRESS ) {
            if( entry.type == "bone" ) {
                roll = rng( 0, roll / 2 );
            }
            if( entry.type == "flesh" ) {
                continue;
            }
            if( entry.type == "skin" ) {
                continue;
            }
        }

        // you only get the skin from skinning
        if( action == SKIN ) {
            if( entry.type != "skin" ) {
                continue;
            }
            if( corpse_item->has_flag( flag_FIELD_DRESS_FAILED ) ) {
                roll = rng( 0, roll );
            }
        }

        // you only get the liquids from bleeding
        if( action == BLEED ) {
            if( entry.type != "blood" ) {
                continue;
            }
        }

        // field dressing removed innards and bones from meatless limbs
        if( ( action == BUTCHER_FULL || action == BUTCHER ) && corpse_item->has_flag( flag_FIELD_DRESS ) ) {
            if( entry.type == "offal" ) {
                continue;
            }
            if( entry.type == "bone" ) {
                roll = ( roll / 2 ) + rng( roll / 2, roll );
            }
        }
        // unskillfull field dressing may damage the skin, meat, and other parts
        if( ( action == BUTCHER_FULL || action == BUTCHER ) &&
            corpse_item->has_flag( flag_FIELD_DRESS_FAILED ) ) {
            if( entry.type == "offal" ) {
                continue;
            }
            if( entry.type == "bone" ) {
                roll = ( roll / 2 ) + rng( roll / 2, roll );
            }
            if( entry.type == "flesh" || entry.type == "skin" ) {
                roll = rng( 0, roll );
            }
        }
        // quartering ruins skin
        if( corpse_item->has_flag( flag_QUARTERED ) ) {
            if( entry.type == "skin" ) {
                //not continue to show fail effect
                roll = 0;
            } else {
                roll /= 4;
            }
        }

        if( entry.type != "bionic" && entry.type != "bionic_group" ) {
            // divide total dropped weight by drop's weight to get amount
            if( entry.mass_ratio != 0.00f ) {
                // apply skill before converting to items, but only if mass_ratio is defined
                roll *= roll_drops();
                // cap dropped weight at monster weight * mass ratio of drop
                roll = std::min<float>( roll, to_gram( mt.weight ) * entry.mass_ratio );
                roll = std::ceil( static_cast<double>( roll ) /
                                  to_gram( drop->weight ) );
            }

            if( roll <= 0 ) {
                if( !already_harvested ) {
                    who.add_msg_if_player( m_bad, _( "You fail to harvest: %s" ), drop->nname( 1 ) );
                }
                continue;
            }
            if( drop->phase == LIQUID ) {
                detached_ptr<item> it = item::spawn( drop, calendar::turn, roll );
                item &obj = *it;
                if( obj.goes_bad() ) {
                    obj.set_rot( corpse_item->get_rot() );
                }
                for( const flag_id &flg : entry.flags ) {
                    obj.set_flag( flg );
                }
                for( const fault_id &flt : entry.faults ) {
                    obj.faults.emplace( flt );
                }
                // TODO: smarter NPC liquid handling
                if( who.is_npc() || action != butcher_type::BLEED ) {
                    drop_on_map( who, item_drop_reason::deliberate, std::move( it ), who.bub_pos() );
                } else {
                    liquid_handler::handle_all_liquid( std::move( it ), PICKUP_RANGE );
                }
            } else if( drop->count_by_charges() ) {
                detached_ptr<item> it = item::spawn( drop, calendar::turn, roll );
                item &obj = *it;
                if( obj.goes_bad() ) {
                    obj.set_rot( corpse_item->get_rot() );
                }
                for( const flag_id &flg : entry.flags ) {
                    obj.set_flag( flg );
                }
                for( const fault_id &flt : entry.faults ) {
                    obj.faults.emplace( flt );
                }
                if( !who.backlog.empty() && who.backlog.front()->id() == ACT_MULTIPLE_BUTCHER ) {
                    obj.set_var( "activity_var", who.name );
                }
                here.add_item_or_charges( who.bub_pos(), std::move( it ) );
            } else {
                item &obj = *item::spawn_temporary( drop, calendar::turn );
                obj.set_mtype( &mt );
                if( obj.goes_bad() ) {
                    obj.set_rot( corpse_item->get_rot() );
                }
                for( const flag_id &flg : entry.flags ) {
                    obj.set_flag( flg );
                }
                for( const fault_id &flt : entry.faults ) {
                    obj.faults.emplace( flt );
                }
                if( !who.backlog.empty() && who.backlog.front()->id() == ACT_MULTIPLE_BUTCHER ) {
                    obj.set_var( "activity_var", who.name );
                }
                for( int i = 0; i != roll; ++i ) {
                    here.add_item_or_charges( who.bub_pos(), item::spawn( obj ) );
                }
            }
            who.add_msg_if_player( m_good, _( "You harvest: %s" ), drop->nname( roll ) );
        }
        practice++;
    }
    // 20% of the original corpse weight is not an item, but liquid gore

    if( action != DISSECT ) {
        who.practice( skill_survival, std::max( 0, practice ), std::max( mt.size - creature_size::medium,
                      0 ) + 4 );
    }
}

void butchery_quarter( item *corpse_item, const Character &who )
{
    corpse_item->set_flag( flag_QUARTERED );
    who.add_msg_if_player( m_good,
                           _( "You roughly slice the corpse of %s into four parts and set them aside." ),
                           corpse_item->get_mtype()->nname() );
    map &here = get_map();
    // 4 quarters (one exists, add 3, flag does the rest)
    for( int i = 1; i <= 3; i++ ) {
        here.add_item_or_charges( who.bub_pos(), item::spawn( *corpse_item ), true );
    }
}

void activity_handlers::forage_finish( player_activity *act, player *p )
{
    // Don't forage if we aren't next to the bush - otherwise we get weird bugs
    bool next_to_bush = false;
    map &here = get_map();
    for( const auto &pnt : here.points_in_radius( p->bub_pos(), 1 ) ) {
        if( bub_to_abs( pnt ) == act->placement ) {
            next_to_bush = true;
            break;
        }
    }

    if( !next_to_bush ) {
        act->set_to_null();
        return;
    }

    const int veggy_chance = rng( 1, 100 );
    bool found_something = false;

    item_group_id loc;
    ter_str_id next_ter;

    switch( season_of_year( calendar::turn ) ) {
        case SPRING:
            loc = item_group_id( "forage_spring" );
            next_ter = ter_str_id( "t_underbrush_harvested_spring" );
            break;
        case SUMMER:
            loc = item_group_id( "forage_summer" );
            next_ter = ter_str_id( "t_underbrush_harvested_summer" );
            break;
        case AUTUMN:
            loc = item_group_id( "forage_autumn" );
            next_ter = ter_str_id( "t_underbrush_harvested_autumn" );
            break;
        case WINTER:
            loc = item_group_id( "forage_winter" );
            next_ter = ter_str_id( "t_underbrush_harvested_winter" );
            break;
        default:
            debugmsg( "Invalid season" );
    }

    here.ter_set( abs_to_bub( act->placement ), next_ter );

    // Survival gives a bigger boost, and Perception is leveled a bit.
    // Both survival and perception affect time to forage

    ///\EFFECT_PER slightly increases forage success chance
    ///\EFFECT_SURVIVAL increases forage success chance
    if( veggy_chance < p->get_skill_level( skill_survival ) * 3 + p->per_cur - 2 ) {
        const std::vector<item *> dropped = here.put_items_from_loc( loc, p->bub_pos(), calendar::turn );
        for( item *it : dropped ) {
            add_msg( m_good, _( "You found: %s!" ), it->tname() );
            found_something = true;
            if( it->has_flag( flag_FORAGE_POISON ) && one_in( 10 ) ) {
                it->set_flag( flag_HIDDEN_POISON );
                it->poison = rng( 2, 7 );
            }
            if( it->has_flag( flag_FORAGE_HALLU ) && !it->has_flag( flag_HIDDEN_POISON ) && one_in( 10 ) ) {
                it->set_flag( flag_HIDDEN_HALLU );
            }
        }
    }
    // 10% to drop a item/items from this group.
    if( one_in( 10 ) ) {
        const std::vector<item *> dropped = here.put_items_from_loc( item_group_id( "trash_forest" ),
                                            p->bub_pos(),
                                            calendar::turn );
        for( item * const &it : dropped ) {
            add_msg( m_good, _( "You found: %s!" ), it->tname() );
            found_something = true;
        }
    }

    if( !found_something ) {
        add_msg( _( "You didn't find anything." ) );
    }

    iexamine::practice_survival_while_foraging( p );

    act->set_to_null();
}

void activity_handlers::generic_game_do_turn( player_activity * /*act*/, player *p )
{
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        // So 30 points per play
        p->add_morale( MORALE_GAME, 2, 60, 2_hours, 30_minutes, true );
        return;
    }
}

void activity_handlers::game_do_turn( player_activity *act, player *p )
{
    item &game_item = *act->targets.front();

    // Consume battery charges for every minute spent playing
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        int energy = game_item.ammo_required();
        energy -= game_item.ammo_consume( energy );
        if( energy > 0 && game_item.has_flag( flag_USE_UPS ) ) {
            if( p->use_charges_if_avail( itype_UPS, energy ) ) {
                energy = 0;
            }
        }
        // Morale boost from game is handled in iuse::portable_game
        if( energy ) {
            act->moves_left = 0;
            add_msg( m_info, _( "The %s runs out of batteries." ), game_item.tname() );
        }
    }
}

void activity_handlers::pickaxe_do_turn( player_activity *act, player *p )
{
    sfx::play_activity_sound( "tool", "pickaxe",
                              sfx::get_heard_volume( abs_to_bub(
                                          act->placement ), 80 ) );
    // each turn is too much
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        //~ Sound of a Pickaxe at work!
        sound_event se;
        se.origin = act->placement;
        se.volume = 90;
        se.category = sounds::sound_t::destructive_activity;
        se.description = _( "CHNK!  CHNK!  CHNK!" );
        se.id = "tool";
        se.variant = "pickaxe";
        se.from_player = p->is_avatar();
        se.from_npc = !se.from_player;
        se.faction = p->get_faction()->id;
        se.monfaction = p->get_faction()->mon_faction;
        sounds::sound( se );
    }
}

void activity_handlers::pickaxe_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const auto pos = abs_to_bub( act->placement );
    if( p->is_avatar() ) {
        int act_exertion = act->moves_total;
        // Troglodyte mutants can dig longer before tiring
        if( p->has_trait( trait_STOCKY_TROGLO ) ) {
            act_exertion /= 2;
        }
        // Base cost of 1 fatigue per 3 minutes, or 30 fatigue at 8 strength since 90 minutes
        // Strength, terrain, and helpers accounted for by time calculation
        p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 45_seconds ) ) );
        p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
        p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 3_minutes ) ) );
        p->mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 5_seconds ) ) );
    }
    act->set_to_null();
    p->add_msg_player_or_npc( m_good,
                              _( "You finish digging." ),
                              _( "<npcname> finishes digging." ) );
    if( here.has_flag_furn( TFLAG_MINEABLE, pos ) ) {
        here.destroy_furn( pos, true );
    } else {
        here.destroy( pos, true );
    }
    if( !act->get_tools().empty() ) {
        item &it = *act->get_tools().front();
        p->consume_charges( it, it.ammo_required() );
    } else {
        debugmsg( "pickaxe activity has no tool" );
    }
    if( resume_for_multi_activities( *p ) ) {
        for( item *&elem : here.i_at( pos ) ) {
            elem->set_var( "activity_var", p->name );
        }
    }
}

void activity_handlers::reload_finish( player_activity *act, player *p )
{
    act->set_to_null();

    if( act->targets.size() != 2 || act->index <= 0 ) {
        debugmsg( "invalid arguments to ACT_RELOAD" );
        return;
    }

    if( !act->targets[0] ) {
        debugmsg( "reload target is null, failed to reload" );
        return;
    }

    if( !act->targets[1] ) {
        debugmsg( "ammo target is null, failed to reload" );
        return;
    }

    item &reloadable = *act->targets[ 0 ];
    item &ammo = *act->targets[1];
    std::string ammo_name = ammo.tname();
    const int qty = act->index;
    const bool is_speedloader = ammo.has_flag( flag_SPEEDLOADER );

    if( !reloadable.reload( *p, ammo, qty ) ) {
        add_msg( m_info, _( "Can't reload the %s." ), reloadable.tname() );
        return;
    }

    std::string msg = _( "You reload the %s." );


    if( reloadable.get_var( "dirt", 0 ) > 7800 ) {
        msg =
            _( "You manage to loosen some debris and make your %s somewhat operational." );
        reloadable.set_var( "dirt", ( reloadable.get_var( "dirt", 0 ) - rng( 790, 2750 ) ) );
    }

    if( reloadable.is_gun() ) {
        p->recoil = MAX_RECOIL;

        if( reloadable.has_flag( flag_RELOAD_ONE ) && !is_speedloader ) {
            for( int i = 0; i != qty; ++i ) {
                msg = _( "You insert one %2$s into the %1$s." );
            }
        }
        if( reloadable.type->gun->reload_noise_volume > 0_dB ) {
            sound_event se;
            se.origin = p->abs_pos();
            se.volume = units::to_decibel( reloadable.type->gun->reload_noise_volume );
            se.category = sounds::sound_t::activity;
            se.description = reloadable.type->gun->reload_noise;
            se.id = "reload";
            se.variant = reloadable.typeId().str();

            sounds::sound( se );
            sfx::play_variant_sound( "reload", reloadable.typeId().str(),
                                     sfx::get_heard_volume( p->bub_pos(), se.volume ) );
        }
    } else if( reloadable.is_container() ) {
        msg = _( "You refill the %s." );
    }
    add_msg( m_neutral, msg, reloadable.tname(), ammo_name );
}

void activity_handlers::vibe_do_turn( player_activity *act, player *p )
{
    //Using a vibrator takes time (10 minutes), not speed
    //Linear increase in morale during action with a small boost at end
    //Deduct 1 battery charge for every minute in use, or vibrator is much less effective
    item &vibrator_item = *act->get_tools().front();

    if( p->encumb( body_part_mouth ) >= 30 ) {
        act->moves_left = 0;
        add_msg( m_bad, _( "You have trouble breathing, and stop." ) );
    }

    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        p->mod_fatigue( 1 );
        if( vibrator_item.ammo_remaining() > 0 ) {
            vibrator_item.ammo_consume( 1 );
            p->add_morale( MORALE_FEELING_GOOD, 3, 40 );
            if( vibrator_item.ammo_remaining() == 0 ) {
                add_msg( m_info, _( "The %s runs out of batteries." ), vibrator_item.tname() );
            }
        } else {
            //twenty minutes to fill
            p->add_morale( MORALE_FEELING_GOOD, 1, 40 );
        }
    }
    // Dead Tired: different kind of relaxation needed
    if( p->get_fatigue() >= fatigue_levels::dead_tired ) {
        act->moves_left = 0;
        add_msg( m_info, _( "You're too tired to continue." ) );
    }

    // Vibrator requires that you be able to move around, stretch, etc, so doesn't play
    // well with roots.  Sorry.  :-(
}



void activity_handlers::cracking_finish( player_activity *act, player *p )
{
    auto &here = get_map();
    p->add_msg_if_player( m_good, _( "With a satisfying click, the lock on the safe opens!" ) );
    here.furn_set( abs_to_bub( act->placement ), f_safe_c );
    act->set_to_null();
}

// TODO (https://github.com/cataclysmbn/Cataclysm-BN/issues/1612):
// Remove that repair code after repair_activity_actor.

// repeat_type enum and repeat_menu — MOVED INTO activity_actor.cpp

namespace activity_handlers
{
namespace repair_activity_hack
{

// Total idea is that:
// 1. Modify activity to make sure that repair action wouldn't search item in inventory.
// 2. Put coords of interesting vehicle part or furniture.
// 3. Before applying each stage of repair, search for possible fake item.
//
// This relies on fact that repairing with real tools
// never use `player_activity::coords`
// and use `player::activity::values` with only one item.

void patch_activity_for_vehicle(
    player_activity &activity,
    const tripoint_bub_ms &veh_part_position,
    const vehicle &veh,
    int interact_part_idx,
    const itype_id &it )
{
    // Player may start another activity on welder/soldering iron
    // Check it here instead of vehicle interaction code
    // because we want to encapsulate hack here.
    if( activity.id() != ACT_REPAIR_ITEM ) {
        return;
    }

    const int crafter_index = veh.part_with_feature( interact_part_idx, "CRAFTER", true );
    // This tells activity, that real item doesn't exists in inventory.
    activity.index = INT_MIN;
    // Data for lookup vehicle part
    activity = player_activity(
                   std::make_unique<repair_actor>(
                       hack_type_t::vehicle, bub_to_abs( veh_part_position ), it, crafter_index
                   )
               );
}

void patch_activity_for_furniture( player_activity &activity,
                                   const tripoint_bub_ms &furniture_position,
                                   const itype_id &itt )
{
    // Player may start another activity on welder/soldering iron
    // Check it here instead of furniture interaction code
    // because we want to encapsulate hack here.
    if( activity.id() != ACT_REPAIR_ITEM ) {
        return;
    }

    // This tells activity, that real item doesn't exists in inventory.
    activity.index = INT_MIN;
    // Data for lookup furniture
    activity = player_activity(
                   std::make_unique<repair_actor>(
                       hack_type_t::furniture, bub_to_abs( furniture_position ), itt, -1
                   )
               );
}

} // namespace repair_activity_hack
} // namespace activity_handlers

// repair_item_finish — MOVED INTO repair_actor::finish()

void activity_handlers::mend_item_finish( player_activity *act, player *p )
{
    act->set_to_null();
    if( act->targets.size() != 1 ) {
        debugmsg( "invalid arguments to ACT_MEND_ITEM" );
        return;
    }

    item *target = &*act->targets[ 0 ];

    const auto f = target->faults.find( fault_id( act->name ) );
    if( f == target->faults.end() ) {
        debugmsg( "item %s does not have fault %s", target->tname(), act->name );
        return;
    }

    if( act->str_values.empty() ) {
        debugmsg( "missing mending_method id for ACT_MEND_ITEM." );
        return;
    }

    const mending_method *method = fault_id( act->name )->find_mending_method( act->str_values[0] );
    if( !method ) {
        debugmsg( "invalid mending_method id for ACT_MEND_ITEM." );
        return;
    }

    const inventory &inv = p->crafting_inventory();
    const requirement_data &reqs = method->requirements.obj();
    if( !reqs.can_make_with_inventory( inv, is_crafting_component ) ) {
        add_msg( m_info, _( "You are currently unable to mend the %s." ), target->tname() );
    }
    for( const auto &e : reqs.get_components() ) {
        p->consume_items( e );
    }
    for( const auto &e : reqs.get_tools() ) {
        p->consume_tools( e );
    }
    p->invalidate_crafting_inventory();

    const auto mend = [&]( item * target ) -> void {
        target->faults.erase( *f );
        if( method->turns_into )
        {
            target->faults.emplace( *method->turns_into );
        }
        // also_mends removes not just the fault picked to be mended, but this as well.
        if( method->also_mends )
        {
            target->faults.erase( *method->also_mends );
        }
        if( act->name == "fault_gun_blackpowder" || act->name == "fault_gun_dirt" )
        {
            target->set_var( "dirt", 0 );
        }
        add_msg( m_good, method->success_msg.translated(), target->tname() );
    };

    mend( target );

    // iterate over attachments and apply the same changes if they have the same fault
    for( const auto &mod : target->gunmods() ) {
        if( !mod->faults.contains( fault_id( act->name ) ) ) {
            continue;
        }
        mend( mod );
    }
}

void activity_handlers::toolmod_add_finish( player_activity *act, player *p )
{
    act->set_to_null();
    if( act->targets.size() != 1 || !act->get_tools()[0] || !act->targets[0] ) {
        debugmsg( "Incompatible arguments to ACT_TOOLMOD_ADD" );
        return;
    }
    item &tool = *act->get_tools()[0];
    item &mod = *act->targets[0];
    p->add_msg_if_player( m_good, _( "You successfully attached the %1$s to your %2$s." ),
                          mod.tname(), tool.tname() );

    mod.set_flag( flag_IRREMOVABLE );
    tool.put_in( mod.detach() );
}

void activity_handlers::meditate_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( m_good, _( "You pause to engage in spiritual contemplation." ) );
    p->add_morale( MORALE_FEELING_GOOD, 5, 10 );
    act->set_to_null();
}



// This activity opens the menu (it's not meant to queue consumption of items)
void activity_handlers::eat_menu_do_turn( player_activity *, player * )
{
    avatar_action::eat( g->u );
}

void activity_handlers::consume_food_menu_do_turn( player_activity *, player * )
{
    avatar_action::eat( g->u, game_menus::inv::consume_food( g->u ) );
}

void activity_handlers::consume_drink_menu_do_turn( player_activity *, player * )
{
    avatar_action::eat( g->u, game_menus::inv::consume_drink( g->u ) );
}

void activity_handlers::consume_meds_menu_do_turn( player_activity *, player * )
{
    avatar_action::eat( g->u, game_menus::inv::consume_meds( g->u ) );
}


void activity_handlers::adv_inventory_do_turn( player_activity *, player *p )
{
    p->cancel_activity();
    create_advanced_inv();
}

void activity_handlers::travel_do_turn( player_activity *act, player *p )
{
    if( !p->omt_path.empty() ) {
        p->omt_path.pop_back();
        if( p->omt_path.empty() ) {
            p->add_msg_if_player( m_info, _( "You have reached your destination." ) );
            act->set_to_null();
            return;
        }
        const tripoint_abs_omt next_omt = p->omt_path.back();
        tripoint_abs_ms waypoint;
        if( p->omt_path.size() == 1 ) {
            // if next omt is the final one, target its midpoint
            waypoint = midpoint( project_bounds<coords::ms>( next_omt ) );
        } else {
            // otherwise target the middle of the edge nearest to our current location
            const auto cur_omt_mid = midpoint( project_bounds<coords::ms>( p->abs_omt_pos() ) );
            waypoint = clamp( cur_omt_mid, project_bounds<coords::ms>( next_omt ) );
        }
        map &here = get_map();
        // TODO: fix point types
        auto centre_sub = abs_to_bub( waypoint );
        if( !here.passable( centre_sub ) ) {
            tripoint_range<tripoint_bub_ms> candidates = here.points_in_radius( centre_sub, 2 );
            for( const auto &elem : candidates ) {
                if( here.passable( elem ) ) {
                    centre_sub = elem;
                    break;
                }
            }
        }
        auto &pf_buffer = MAPBUFFER_REGISTRY.get( p->get_dimension() );
        const auto pair = p->get_pathfinding_pair();
        auto route = Pathfinding::route( pf_buffer, p->abs_pos(), bub_to_abs( centre_sub ),
                                         pair.first, pair.second );
        if( !route.empty() ) {
            const activity_id act_travel = ACT_TRAVELLING;
            p->set_destination( route, std::make_unique<player_activity>( act_travel ) );
        } else {
            p->add_msg_if_player( _( "You cannot reach that destination" ) );
        }
    } else {
        p->add_msg_if_player( m_info, _( "You have reached your destination." ) );
    }
    act->set_to_null();
}

void activity_handlers::armor_layers_do_turn( player_activity *, player *p )
{
    p->cancel_activity();
    show_armor_layers_ui( *p );
}

void activity_handlers::atm_do_turn( player_activity *, player *p )
{
    iexamine::atm( *p, p->bub_pos() );
}

// fish-with-rod fish catching function.
static void rod_fish( player *p,
                      const weighted_int_list<std::pair<std::string, int>> &fishables )
{
    map &here = get_map();
    const std::pair<std::string, int> *caught = fishables.pick();
    if( caught->first.contains( "fish" ) ) {
        const std::vector<mtype_id> fish_group = MonsterGroupManager::GetMonstersFromGroup(
                    mongroup_id( "GROUP_FISH" ) );
        const mtype_id fish_mon = random_entry_ref( fish_group );
        here.add_item_or_charges(
            p->bub_pos(), item::make_corpse( fish_mon, calendar::turn +
                                             rng( 0_turns, 3_hours ) ) );

        p->add_msg_if_player( m_good, _( "You caught a %s." ), fish_mon.obj().nname() );
    } else {
        itype_id possible( caught->first );
        if( possible.is_valid() ) {
            here.add_item_or_charges( p->bub_pos(), item::spawn( caught->first, calendar::turn,
                                      caught->second ),
                                      true );
            p->add_msg_if_player( m_good, _( "You reeled in %s." ) );
        }
    }

    for( item *&elem : here.i_at( p->bub_pos() ) ) {
        if( elem->is_corpse() && !elem->has_var( "activity_var" ) ) {
            elem->set_var( "activity_var", p->name );
        }
    }
}

void activity_handlers::fish_do_turn( player_activity *act, player *p )
{
    int fishing_mult = good_fishing_spot( p->get_mapbuffer(), act->placement );
    if( fishing_mult == 0 || p->is_blind() ) {
        act->set_to_null();
        p->add_msg_if_player( m_info,
                              _( "You realize fishing here at the moment is pointless, and stop." ) );
        if( !p->backlog.empty() && p->backlog.front()->id() == ACT_MULTIPLE_FISH ) {
            p->backlog.clear();
            p->assign_activity( ACT_TIDY_UP );
            return;
        }
        return;
    }
    item &rod = *act->get_tools().front();
    int fish_chance = 1;
    int survival_mod = p->get_skill_level( skill_survival );
    if( rod.has_flag( flag_FISH_POOR ) ) {
        survival_mod += dice( 1, 8 ); // avg of 4
    } else if( rod.has_flag( flag_FISH_GOOD ) ) {
        // Much better chances with a good fishing implement.
        survival_mod += dice( 3, 6 ); //avg of 10-11
    }
    fish_chance += ( survival_mod *  fishing_mult );
    // no matter the population of fish, your skill and tool limits the ease of catching.
    fish_chance = std::min( survival_mod * 20, fish_chance );
    if( x_in_y( fish_chance, 600000 ) ) {//Roughly 1/1000 per turn avg.
        p->add_msg_if_player( m_good, _( "You feel a tug on your line!" ) );
        weighted_int_list<std::pair<std::string, int>> caught;
        caught.add( {"fish", 1}, 1 ); //Hardcoded for now, but can be expanded for magnet fishing or smthn
        rod_fish( p, caught );
    }
    if( action_time_scale::once_every_this_tick( 60_minutes ) ) {
        p->practice( skill_survival, rng( 1, 3 ) );
    }

}

void activity_handlers::fish_finish( player_activity *act, player *p )
{
    act->set_to_null();
    p->add_msg_if_player( m_info, _( "You finish fishing" ) );
    if( !p->backlog.empty() && p->backlog.front()->id() == ACT_MULTIPLE_FISH ) {
        p->backlog.clear();
        p->assign_activity( ACT_TIDY_UP );
    }
}

void activity_handlers::cracking_do_turn( player_activity *act, player *p )
{
    // We got deafened in the middle of it and can't decode by touch, so bail out
    if( p->is_deaf() && p->get_skill_level( skill_mechanics ) < 5 ) {
        add_msg( m_bad, _( "You can't hear the tumblers anymore, so you stop." ) );
        act->set_to_null();
        return;
    }
}

void activity_handlers::repair_item_do_turn( player_activity *act, player *p )
{
    // Moves are decremented based on a combination of speed and good vision (not in the dark, farsighted, etc)
    const float vision_mod = character_funcs::fine_detail_vision_mod( *p );
    const auto effective_moves = static_cast<int>(
                                     action_time_scale::activity_progress_from_actor_moves( *p ) / vision_mod );
    if( effective_moves <= act->moves_left ) {
        act->moves_left -= effective_moves;
        p->moves = 0;
    } else {
        p->moves -= action_time_scale::actor_moves_for_activity_progress( *p,
                    act->moves_left * vision_mod );
        act->moves_left = 0;
    }
}



void activity_handlers::wait_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( _( "You finish waiting." ) );
    act->set_to_null();
}

void activity_handlers::wait_weather_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( _( "You finish waiting." ) );
    act->set_to_null();
}

void activity_handlers::find_mount_do_turn( player_activity *act, player *p )
{
    //npc only activity
    if( p->is_player() ) {
        act->set_to_null();
        return;
    }
    npc &guy = dynamic_cast<npc &>( *p );
    monster *mon = guy.chosen_mount.lock().get();
    if( !mon ) {
        act->set_to_null();
        guy.revert_after_activity();
        return;
    }
    if( rl_dist( guy.abs_pos(), mon->abs_pos() ) <= 1 ) {
        if( mon->has_effect( effect_ai_waiting ) ) {
            mon->remove_effect( effect_ai_waiting );
        }
        if( p->can_mount( *mon ) ) {
            act->set_to_null();
            guy.revert_after_activity();
            guy.chosen_mount = weak_ptr_fast<monster>();
            p->mount_creature( *mon );
        } else {
            act->set_to_null();
            guy.revert_after_activity();
            return;
        }
    } else {
        const auto route = route_adjacent( *p, guy.chosen_mount.lock()->abs_pos() );
        if( route.empty() ) {
            act->set_to_null();
            guy.revert_after_activity();
            mon->remove_effect( effect_ai_waiting );
            return;
        } else {
            p->activity->set_to_null();// = player_activity();
            mon->add_effect( effect_ai_waiting, 40_turns );
            p->set_destination( route, std::make_unique<player_activity>( ACT_FIND_MOUNT ) );
        }
    }
}

void activity_handlers::try_sleep_do_turn( player_activity *act, player *p )
{
    if( !p->has_effect( effect_sleep ) ) {
        if( character_funcs::roll_can_sleep( *p ) ) {
            act->set_to_null();
            p->fall_asleep();
            p->remove_value( "sleep_query" );
        } else if( one_in( 1000 ) ) {
            p->add_msg_if_player( _( "You toss and turn…" ) );
        }
        if( action_time_scale::once_every_this_tick( 30_minutes ) ) {
            try_sleep_query( act, p );
        }
    }
}

void activity_handlers::try_sleep_query( player_activity *act, player *p )
{
    if( p->get_value( "sleep_query" ) == "false" ) {
        return;
    }
    uilist sleep_query;
    sleep_query.text = _( "You have trouble sleeping, keep trying?" );
    sleep_query.addentry( 1, true, 'S', _( "Stop trying to fall asleep and get up." ) );
    sleep_query.addentry( 2, true, 'c', _( "Continue trying to fall asleep." ) );
    sleep_query.addentry( 3, true, 'C',
                          _( "Continue trying to fall asleep and don't ask again." ) );
    sleep_query.query();
    switch( sleep_query.ret ) {
        case UILIST_CANCEL:
        case 1:
            act->set_to_null();
            break;
        case 3:
            p->set_value( "sleep_query", "false" );
            break;
        case 2:
        default:
            break;
    }
}

void activity_handlers::try_sleep_finish( player_activity *act, player *p )
{
    if( !p->has_effect( effect_sleep ) ) {
        p->add_msg_if_player( _( "You try to sleep, but can't…" ) );
    }
    act->set_to_null();
}

// operation_do_turn — moved into operation_actor::do_turn()
// operation_finish — moved into operation_actor::finish()

void activity_handlers::churn_finish( player_activity *act, player *p )
{
    map &here = get_map();
    p->add_msg_if_player( _( "You finish churning up the earth here." ) );
    here.ter_set( abs_to_bub( act->placement ), t_dirtmound );
    // Go back to what we were doing before
    // could be player zone activity, or could be NPC multi-farming
    act->set_to_null();
    resume_for_multi_activities( *p );
}

void activity_handlers::plant_seed_finish( player_activity *act, player *p )
{
    map &here = get_map();
    auto examp = abs_to_bub( act->placement );
    const itype_id seed_id( act->str_values[0] );
    std::vector<detached_ptr<item>> used_seed;
    if( item::count_by_charges( seed_id ) ) {
        used_seed = p->use_charges( seed_id, 1 );
    } else {
        used_seed = p->use_amount( seed_id, 1 );
    }
    if( !used_seed.empty() ) {
        used_seed.front()->set_age( 0_turns );
        if( used_seed.front()->has_var( "activity_var" ) ) {
            used_seed.front()->erase_var( "activity_var" );
        }
        used_seed.front()->set_flag( flag_HIDDEN_ITEM );
        here.add_item_or_charges( examp, std::move( used_seed.front() ) );
        if( here.has_flag_furn( seed_id->seed->required_terrain_flag, examp ) ) {
            here.furn_set( examp, furn_str_id( here.furn( examp )->plant->transform ) );
        } else if( seed_id->seed->required_terrain_flag == flag_PLANTABLE ) {
            here.set( examp, t_dirt, f_plant_seed );
        } else {
            here.furn_set( examp, f_plant_seed );
        }
        p->add_msg_player_or_npc( _( "You plant some %s." ), _( "<npcname> plants some %s." ),
                                  item::nname( seed_id ) );
    }
    // Go back to what we were doing before
    // could be player zone activity, or could be NPC multi-farming
    act->set_to_null();
    resume_for_multi_activities( *p );
}

void activity_handlers::tidy_up_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::multiple_fish_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::multiple_construction_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::multiple_mine_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::multiple_chop_planks_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::multiple_butcher_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::vehicle_deconstruction_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::vehicle_repair_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::chop_trees_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::multiple_farm_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

// fetch_do_turn — moved into fetch_required_actor::do_turn()

// craft_do_turn — moved into craft_activity_actor::do_turn()

void activity_handlers::vibe_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( m_good, _( "You feel much better." ) );
    p->add_morale( MORALE_FEELING_GOOD, 10, 40 );
    act->set_to_null();
}

void activity_handlers::atm_finish( player_activity *act, player * )
{
    // ATM sets index to 0 to indicate it's finished.
    if( !act->index ) {
        act->set_to_null();
    }
}

void activity_handlers::eat_menu_finish( player_activity *, player * )
{
    // Only exists to keep the eat activity alive between turns
    return;
}

void activity_handlers::pry_nails_do_turn( player_activity *act, player * )
{
    const auto bub_loc = abs_to_bub( act->placement );
    sfx::play_activity_sound( "tool", "hammer", sfx::get_heard_volume( bub_loc, 70 ) );
}

void activity_handlers::pry_nails_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const auto bub_loc = abs_to_bub( act->placement );
    const ter_id type = here.ter( bub_loc );

    p->add_msg_if_player( _( "You pry out the nails from the terrain." ) );

    p->practice( skill_fabrication, 1, 1 );
    here.spawn_item( p->bub_pos(), itype_nail, 1, type->nail_pull_items[0] );
    here.spawn_item( p->bub_pos(), itype_2x4, type->nail_pull_items[1] );
    here.ter_set( bub_loc, type->nail_pull_result );
    act->set_to_null();
}

void activity_handlers::chop_tree_do_turn( player_activity *act, player *p )
{
    sfx::play_activity_sound( "tool", "axe",
                              sfx::get_heard_volume( abs_to_bub( act->placement ), 85 ) );
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        //~ Sound of a wood chopping tool at work!
        sound_event se;
        se.origin = act->placement;
        se.volume = 85;
        se.category = sounds::sound_t::activity;
        se.description = _( "CHK!" );
        se.id = "tool";
        se.variant = "axe";
        se.from_player = p->is_avatar();
        se.from_npc = !se.from_player;
        se.faction = p->get_faction()->id;
        se.monfaction = p->get_faction()->mon_faction;
        sounds::sound( se );
    }
}

void activity_handlers::chop_tree_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const auto pos = abs_to_bub( act->placement );

    tripoint_rel_ms direction;
    if( !p->is_npc() ) {
        if( p->backlog.empty() || p->backlog.front()->id() != ACT_MULTIPLE_CHOP_TREES ) {
            while( true ) {
                if( const auto dir = choose_direction(
                                         _( "Select a direction for the tree to fall in." ) ) ) {
                    direction = *dir;
                    break;
                }
                // try again
            }
        }
    } else {
        // Try to safely fell tree
        std::vector<tripoint_rel_ms> valid_directions;

        for( const auto &elem : here.points_in_radius( pos, 1 ) ) {
            bool cantuse = false;
            auto direc = elem - pos;
            auto proposed_to = pos + point_rel_ms( 3 * direction.x(), 3 * direction.y() );
            std::vector<tripoint_bub_ms> rough_tree_line = line_to( pos, proposed_to );
            for( const auto &elem : rough_tree_line ) {
                // Try not to drop onto a critter
                if( g->critter_at( elem ) ) {
                    cantuse = true;
                    break;
                }

                ter_t ter = here.ter( elem ).obj();
                furn_t furn = here.furn( elem ).obj();
                // Furniture / Terrain test
                if( elem != pos && ( ter.bash.str_max != -1 || ( furn.id && furn.bash.str_max != -1 ) ) ) {
                    cantuse = true;
                    break;
                }
                // Vehicle check
                if( veh_pointer_or_null( here.veh_at( elem ) ) ) {
                    cantuse = true;
                    break;
                }
            }
            if( !cantuse ) {
                // Passed all tests for safe direction, add to the possible routes
                valid_directions.push_back( direc );
            }
        }
        // Select a random valid direction, or none if empty
        direction = random_entry( valid_directions, direction );
    }

    const auto to = pos + 3 * direction.xy() + point( rng( -1, 1 ), rng( -1, 1 ) );
    std::vector<tripoint_bub_ms> tree = line_to( pos, to, rng( 1, 8 ) );
    for( const auto &elem : tree ) {
        here.batter( elem, 300, 5 );
        here.ter_set( elem, t_trunk );
    }

    here.ter_set( pos, t_stump );
    p->add_msg_if_player( m_good, _( "You finish chopping down a tree." ) );
    here.collapse_at( pos, false, true, false );
    // sound of falling tree
    sfx::play_variant_sound( "misc", "timber",
                             sfx::get_heard_volume( pos, 95 ), false );
    act->set_to_null();

    // Quality of tool used and assistants can together both reduce intensity of work.
    if( act->get_tools().empty() ) {
        debugmsg( "woodcutting item location not set" );
        return;
    }

    safe_reference<item> &loc = act->get_tools_mut()[ 0 ];
    if( !loc ) {
        debugmsg( "woodcutting item location lost" );
        return;
    }

    item *it = &*loc;

    int act_exertion = iuse::chop_moves( *p, *it );
    p->add_msg_if_player( m_good, _( "You finish chopping down a tree." ) );
    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    act_exertion = act_exertion * ( 10 - helpers.size() ) / 10;

    p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 80_seconds ) ) );
    p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 12_minutes ) ) );
    p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
    p->mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 10_seconds ) ) );

    resume_for_multi_activities( *p );
}

void activity_handlers::chop_logs_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const auto pos = abs_to_bub( act->placement );
    int log_quan;
    int stick_quan;
    int splint_quan;
    if( here.ter( pos ) == t_trunk ) {
        log_quan = rng( 2, 3 );
        stick_quan = rng( 0, 1 );
        splint_quan = 0;
    } else if( here.ter( pos ) == t_stump ) {
        log_quan = rng( 0, 2 );
        stick_quan = 0;
        splint_quan = rng( 5, 15 );
    } else {
        log_quan = 0;
        stick_quan = 0;
        splint_quan = 0;
    }
    for( int i = 0; i != log_quan; ++i ) {
        detached_ptr<item> obj = item::spawn( itype_log, calendar::turn );
        obj->set_var( "activity_var", p->name );
        here.add_item_or_charges( pos, std::move( obj ) );
    }
    for( int i = 0; i != stick_quan; ++i ) {
        detached_ptr<item> obj = item::spawn( itype_stick_long, calendar::turn );
        obj->set_var( "activity_var", p->name );
        here.add_item_or_charges( pos, std::move( obj ) );
    }
    for( int i = 0; i != splint_quan; ++i ) {
        detached_ptr<item> obj = item::spawn( itype_splinter, calendar::turn );
        obj->set_var( "activity_var", p->name );
        here.add_item_or_charges( pos, std::move( obj ) );
    }
    here.ter_set( pos, t_dirt );
    p->add_msg_if_player( m_good, _( "You finish chopping wood." ) );

    act->set_to_null();

    // Quality of tool used and assistants can together both reduce intensity of work.

    safe_reference<item> &loc = act->get_tools_mut()[ 0 ];
    if( !loc ) {
        debugmsg( "woodcutting item location lost" );
        return;
    }

    item *it = &*loc;
    int act_exertion = iuse::chop_moves( *p, *it );
    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    act_exertion = act_exertion * ( 10 - helpers.size() ) / 10;

    p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 80_seconds ) ) );
    p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 12_minutes ) ) );
    p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
    p->mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 10_seconds ) ) );

    resume_for_multi_activities( *p );
}

void activity_handlers::chop_planks_finish( player_activity *act, player *p )
{
    const int max_planks = 10;
    /** @EFFECT_FABRICATION increases number of planks cut from a log */
    int planks = normal_roll( 2 + p->get_skill_level( skill_id( "fabrication" ) ), 1 );
    int wasted_planks = max_planks - planks;
    int scraps = rng( wasted_planks, wasted_planks * 3 );
    planks = std::min( planks, max_planks );

    map &here = get_map();
    if( planks > 0 ) {
        here.spawn_item( abs_to_bub( act->placement ), itype_2x4, planks, 0, calendar::turn );
        p->add_msg_if_player( m_good, _( "You produce %d planks." ), planks );
    }
    if( scraps > 0 ) {
        here.spawn_item( abs_to_bub( act->placement ), itype_splinter, scraps, 0, calendar::turn );
        p->add_msg_if_player( m_good, _( "You produce %d splinters." ), scraps );
    }
    if( planks < max_planks / 2 ) {
        p->add_msg_if_player( m_bad, _( "You waste a lot of the wood." ) );
    }
    act->set_to_null();
    resume_for_multi_activities( *p );
}

void activity_handlers::jackhammer_do_turn( player_activity *act, player *p )
{
    sfx::play_activity_sound( "tool", "jackhammer",
                              sfx::get_heard_volume( abs_to_bub( act->placement ), 130 ) );
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        sound_event se;
        se.origin = act->placement;
        se.volume = 130;
        se.category = sounds::sound_t::destructive_activity;
        se.description = _( "TATATATATATATAT!" );//~ Sound of a jackhammer at work!
        se.id = "tool";
        se.variant = "jackhammer";
        se.from_player = p->is_avatar();
        se.from_npc = !se.from_player;
        se.faction = p->get_faction()->id;
        se.monfaction = p->get_faction()->mon_faction;
        sounds::sound( se );

    }
}

void activity_handlers::jackhammer_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const auto pos = abs_to_bub( act->placement );

    if( here.has_flag_furn( TFLAG_MINEABLE, pos ) ) {
        here.destroy_furn( pos, true );
    } else {
        here.destroy( pos, true );
    }

    if( p->is_avatar() ) {
        int act_exertion = act->moves_total;
        // Troglodyte mutants can dig longer before tiring
        if( p->has_trait( trait_STOCKY_TROGLO ) ) {
            act_exertion /= 2;
        }
        // Base cost of 1 fatigue per 3 minutes, or 10 fatigue at 8 strength since 30 minutes
        // Strength, terrain, and helpers accounted for by time calculation
        p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 45_seconds ) ) );
        p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 6_minutes ) ) );
        p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 3_minutes ) ) );
        p->mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 5_seconds ) ) );
    }
    p->add_msg_player_or_npc( m_good,
                              _( "You finish drilling." ),
                              _( "<npcname> finishes drilling." ) );
    act->set_to_null();
    if( !act->get_tools().empty() ) {
        item &it = *act->get_tools().front();
        p->consume_charges( it, it.ammo_required() );
    } else {
        debugmsg( "unable to find tool" );
    }
    if( resume_for_multi_activities( *p ) ) {
        for( item *&elem : here.i_at( pos ) ) {
            elem->set_var( "activity_var", p->name );
        }
    }
}

void activity_handlers::fill_pit_do_turn( player_activity *act, player *p )
{
    sfx::play_activity_sound( "tool", "shovel", 100 );
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        //~ Sound of a shovel filling a pit or mound at work!
        sound_event se;
        se.origin = act->placement;
        se.volume = 60;
        se.category = sounds::sound_t::activity;
        se.description = _( "hsh!" );
        se.id = "tool";
        se.variant = "shovel";
        se.from_player = p->is_avatar();
        se.from_npc = !se.from_player;
        se.faction = p->get_faction()->id;
        se.monfaction = p->get_faction()->mon_faction;
        sounds::sound( se );
    }
}

void activity_handlers::fill_pit_finish( player_activity *act, player *p )
{
    const auto &pos = act->placement;
    map &here = get_map();
    const auto bub_pos = abs_to_bub( pos );
    const ter_id ter = here.ter( bub_pos );
    const ter_id old_ter = ter;

    here.ter_set( bub_pos, old_ter->fill_result );
    int act_exertion = to_moves<int>( time_duration::from_minutes( old_ter->fill_minutes ) );
    const int helpersize = character_funcs::get_crafting_helpers( *p, 3 ).size();
    act_exertion = act_exertion * ( 10 - helpersize ) / 10;
    p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 20_seconds ) ) );
    p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 3_minutes ) ) );
    p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 90_seconds ) ) );
    p->mod_stamina( std::min( -1, -act_exertion / to_moves<int>( 15_seconds ) ) );
    p->add_msg_if_player( m_good, _( "You finish filling up %s." ), old_ter->name() );
    act->set_to_null();
}

void activity_handlers::shaving_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( _( "You open up your kit and shave." ) );
    p->add_morale( MORALE_SHAVE, 8, 8, 240_minutes, 3_minutes );
    act->set_to_null();
}

void activity_handlers::haircut_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( _( "You give your hair a trim." ) );
    p->add_morale( MORALE_HAIRCUT, 3, 3, 480_minutes, 3_minutes );
    act->set_to_null();
}

std::vector<tripoint_bub_ms> get_sorted_tiles_by_distance( const tripoint_bub_ms &coord,
        const std::unordered_set<tripoint_bub_ms> &tiles )
{
    const auto cmp = [coord]( tripoint_bub_ms a, tripoint_bub_ms b ) {
        const int da = rl_dist( coord, a );
        const int db = rl_dist( coord, b );

        return da < db;
    };

    std::vector<tripoint_bub_ms> sorted( tiles.begin(), tiles.end() );
    std::ranges::sort( sorted, cmp );

    return sorted;
}

std::vector<tripoint_abs_ms> get_sorted_tiles_by_distance( const tripoint_abs_ms &coord,
        const std::unordered_set<tripoint_abs_ms> &tiles )
{
    const auto cmp = [coord]( tripoint_abs_ms a, tripoint_abs_ms b ) {
        const int da = rl_dist( coord, a );
        const int db = rl_dist( coord, b );

        return da < db;
    };

    std::vector<tripoint_abs_ms> sorted( tiles.begin(), tiles.end() );
    std::ranges::sort( sorted, cmp );

    return sorted;
}

template<typename fn>
static void cleanup_tiles( std::unordered_set<tripoint_abs_ms> &tiles, fn &cleanup )
{
    auto it = tiles.begin();
    while( it != tiles.end() ) {
        auto current = it++;

        if( cleanup( abs_to_bub( *current ) ) ) {
            tiles.erase( current );
        }
    }
}

static void perform_zone_activity_turn( player *p,
                                        const zone_type_id &ztype,
                                        const std::function<bool( const tripoint_bub_ms & )> &tile_filter,
                                        const std::function<void ( player &p, const tripoint_bub_ms & )> &tile_action,
                                        const std::string &finished_msg )
{
    const zone_manager &mgr = zone_manager::get_manager();
    map &here = get_map();
    const auto abspos = p->abs_pos();
    std::unordered_set<tripoint_abs_ms> unsorted_tiles = mgr.get_near( ztype, abspos );

    cleanup_tiles( unsorted_tiles, tile_filter );

    // sort remaining tiles by distance
    const std::vector<tripoint_abs_ms> &tiles = get_sorted_tiles_by_distance( abspos, unsorted_tiles );

    for( const auto &tile : tiles ) {
        const auto tile_loc = abs_to_bub( tile );

        auto &pf_buffer = MAPBUFFER_REGISTRY.get( p->get_dimension() );
        const auto pair = p->get_pathfinding_pair();
        auto route = Pathfinding::route( pf_buffer, p->abs_pos(), bub_to_abs( tile_loc ),
                                         pair.first, pair.second );
        if( route.size() > 1 ) {
            route.pop_back();
            p->set_destination( route, p->remove_activity() );
            p->activity = std::make_unique<player_activity>( );
            return;
        } else {
            // we are at destination already
            /* Perform action */
            tile_action( *p, tile_loc );
            if( p->moves <= 0 ) {
                return;
            }
        }
    }
    add_msg( m_info, finished_msg );
    p->activity->set_to_null();
}

void activity_handlers::fertilize_plot_do_turn( player_activity *act, player *p )
{
    itype_id fertilizer;
    auto check_fertilizer = [&]( bool ask_user = true ) -> void {
        if( act->str_values.empty() )
        {
            act->str_values.emplace_back( "" );
        }
        fertilizer = itype_id( act->str_values[0] );

        /* If unspecified, or if we're out of what we used before, ask */
        if( ask_user && ( fertilizer.is_empty() || !p->has_charges( fertilizer, 1 ) ) )
        {
            fertilizer = iexamine::choose_fertilizer( *p, "plant",
                    false /* Don't confirm action with player */ );
            act->str_values[0] = fertilizer.str();
        }
    };

    auto have_fertilizer = [&]() {
        return !fertilizer.is_empty() && p->has_charges( fertilizer, 1 );
    };

    const auto reject_tile = [&]( const tripoint_bub_ms & tile ) {
        check_fertilizer();
        ret_val<bool> can_fert = iexamine::can_fertilize( *p, tile, fertilizer );
        return !can_fert.success();
    };

    const auto fertilize = [&]( player & who, const tripoint_bub_ms & tile ) {
        check_fertilizer();
        if( have_fertilizer() ) {
            iexamine::fertilize_plant( who, tile, fertilizer );
            if( !have_fertilizer() ) {
                add_msg( m_info, _( "You have run out of %s." ), item::nname( fertilizer ) );
            }
        }
    };

    check_fertilizer();
    if( !have_fertilizer() ) {
        act->set_to_null();
        return;
    }

    perform_zone_activity_turn( p,
                                zone_type_FARM_PLOT,
                                reject_tile,
                                fertilize,
                                _( "You fertilized every plot you could." ) );
}

void activity_handlers::robot_control_do_turn( player_activity *act, player *p )
{
    if( act->monsters.empty() ) {
        debugmsg( "No monster assigned in ACT_ROBOT_CONTROL" );
        act->set_to_null();
        return;
    }
    const shared_ptr_fast<monster> z = act->monsters[0].lock();

    if( !z || !iuse::robotcontrol_can_target( p, *z ) ) {
        p->add_msg_if_player( _( "Target lost.  IFF override failed." ) );
        act->set_to_null();
        return;
    }

    // TODO: Add some kind of chance of getting the target's attention
}

void activity_handlers::robot_control_finish( player_activity *act, player *p )
{
    act->set_to_null();

    if( act->monsters.empty() ) {
        debugmsg( "No monster assigned in ACT_ROBOT_CONTROL" );
        return;
    }

    shared_ptr_fast<monster> z = act->monsters[0].lock();
    act->monsters.clear();

    if( !z || !iuse::robotcontrol_can_target( p, *z ) ) {
        p->add_msg_if_player( _( "Target lost.  IFF override failed." ) );
        return;
    }

    p->add_msg_if_player( _( "You unleash your override attack on the %s." ), z->name() );

    /** @EFFECT_INT increases chance of successful robot reprogramming, vs difficulty */
    /** @EFFECT_COMPUTER increases chance of successful robot reprogramming, vs difficulty */
    const int computer_skill = p->get_skill_level( skill_id( "computer" ) );
    const float randomized_skill = rng( 2, p->int_cur ) + computer_skill;
    float success = computer_skill - 3 * z->type->difficulty / randomized_skill;
    if( z->has_flag( MF_RIDEABLE_MECH ) ) {
        success = randomized_skill - rng( 1, 11 );
    }
    // rideable mechs are not hostile, they have no AI, they do not resist control as much.
    if( success >= 0 ) {
        p->add_msg_if_player( _( "You successfully override the %s's IFF protocols!" ),
                              z->name() );
        z->friendly = -1;
        if( z->has_flag( MF_RIDEABLE_MECH ) ) {
            z->add_effect( effect_pet, 1_turns );
        }
    } else if( success >= -2 ) {
        //A near success
        p->add_msg_if_player( _( "The %s short circuits as you attempt to reprogram it!" ), z->name() );
        //damage it a little
        z->apply_damage( p, bodypart_id( "torso" ), rng( 1, 10 ) );
        if( z->is_dead() ) {
            p->practice( skill_id( "computer" ), 10 );
            // Do not do the other effects if the robot died
            return;
        }
        if( one_in( 3 ) ) {
            p->add_msg_if_player( _( "…and turns friendly!" ) );
            //did the robot became friendly permanently?
            if( one_in( 3 ) ) {
                //it did
                z->friendly = -1;
            } else {
                // it didn't
                z->friendly = rng( 5, 40 );
            }
        }
    } else {
        p->add_msg_if_player( _( "…but the robot refuses to acknowledge you as an ally!" ) );
    }
    p->practice( skill_computer, 10 );
}

static void blood_magic( player *p, int cost )
{
    std::vector<uilist_entry> uile;
    std::vector<bodypart_id> parts;
    int i = 0;
    for( const bodypart_id &bp : p->get_all_body_parts( true ) ) {
        const int hp_cur = p->get_part_hp_cur( bp );
        uilist_entry entry( i, hp_cur > cost, i + 49, body_part_hp_bar_ui_text( bp ) );

        const std::pair<std::string, nc_color> &hp = get_hp_bar( hp_cur, p->get_part_hp_max( bp ) );
        entry.ctxt = colorize( hp.first, hp.second );
        uile.emplace_back( entry );
        parts.push_back( bp );
        i++;
    }
    int action = -1;
    while( action < 0 ) {
        action = uilist( _( "Choose part\nto draw blood from." ), uile );
    }
    p->mod_part_hp_cur( parts[action], - cost );
    p->mod_pain( std::max( 1, cost / 3 ) );
}

void activity_handlers::spellcasting_finish( player_activity *act, player *p )
{
    act->set_to_null();
    const int level_override = act->get_value( 0 );
    spell_id sp( act->name );

    // if level is -1 then we know it's a player spell, otherwise we build it from the ground up
    spell temp_spell( sp );
    spell &spell_being_cast = ( level_override == -1 ) ? p->magic->get_spell( sp ) : temp_spell;

    // if level != 1 then we need to set the spell's level
    if( level_override != -1 ) {
        while( spell_being_cast.get_level() < level_override && !spell_being_cast.is_max_level() ) {
            spell_being_cast.gain_level();
        }
    }

    const bool no_fail = act->get_value( 1 ) == 1;
    const bool no_mana = act->get_value( 2 ) == 0;

    // choose target for spell (if the spell has a range > 0)

    auto target = p->abs_pos();
    bool target_is_valid = false;
    if( spell_being_cast.range() > 0 && !spell_being_cast.is_valid_target( target_none ) &&
        !spell_being_cast.has_flag( RANDOM_TARGET ) ) {
        g->refresh_player_visibility_cache_if_needed();
        do {
            avatar &you = *p->as_avatar();
            std::vector<tripoint_abs_ms> trajectory = target_handler::mode_spell( you, spell_being_cast,
                    no_fail,
                    no_mana );
            g->refresh_player_visibility_cache_if_needed();

            if( !trajectory.empty() ) {
                target = trajectory.back();
                target_is_valid = spell_target_can_be_resolved( spell_being_cast, *p, target );
            } else {
                target_is_valid = false;
            }
            if( !target_is_valid ) {
                if( query_yn( _( "Stop casting spell?  Time spent will be lost." ) ) ) {
                    return;
                }
            }
        } while( !target_is_valid );
    } else if( spell_being_cast.has_flag( RANDOM_TARGET ) ) {
        const std::optional<tripoint_abs_ms> target_ = spell_being_cast.random_valid_target( *p,
                p->abs_pos() );
        if( !target_ ) {
            p->add_msg_if_player( game_message_params{ m_bad, gmf_bypass_cooldown },
                                  _( "Your spell can't find a suitable target." ) );
            return;
        }
        target = *target_;
    }

    // no turning back now. it's all said and done.
    bool success = no_fail || rng_float( 0.0f, 1.0f ) >= spell_being_cast.spell_fail( *p );
    int exp_gained = spell_being_cast.casting_exp( *p );
    if( !success ) {
        p->add_msg_if_player( game_message_params{ m_bad, gmf_bypass_cooldown },
                              _( "You lose your concentration!" ) );
        if( !spell_being_cast.is_max_level() && level_override == -1 ) {
            // still get some experience for trying
            spell_being_cast.gain_exp( exp_gained / 5 );
            p->add_msg_if_player( m_good, _( "You gain %i experience.  New total %i." ), exp_gained / 5,
                                  spell_being_cast.xp() );
        }
        return;
    }

    if( spell_being_cast.has_flag( spell_flag::VERBAL ) ) {
        sound_event se;
        se.origin = p->abs_pos();
        se.volume = p->get_shout_volume() - 15;
        se.category = sounds::sound_t::speech;
        se.description = _( "cast a spell" );
        se.from_player = p->is_avatar();
        se.from_npc = !se.from_player;
        se.faction = p->get_faction()->id;
        se.monfaction = p->get_faction()->mon_faction;
        sounds::sound( se );
    }

    p->add_msg_if_player( spell_being_cast.message(), spell_being_cast.name() );

    spell_being_cast.cast_all_effects( *p, target );

    if( !no_mana ) {
        // pay the cost
        int cost = spell_being_cast.energy_cost( *p );
        switch( spell_being_cast.energy_source() ) {
            case mana_energy:
                p->magic->mod_mana( *p, -cost );
                break;
            case stamina_energy:
                p->mod_stamina( -cost, spell_being_cast.has_flag( spell_flag::PHYSICAL ) );
                break;
            case bionic_energy:
                p->mod_power_level( -units::from_kilojoule( cost ) );
                break;
            case hp_energy:
                blood_magic( p, cost );
                break;
            case fatigue_energy:
                p->mod_fatigue( cost );
                break;
            case none_energy:
            default:
                break;
        }
        spell_being_cast.use_components( *p );
    }
    if( level_override == -1 ) {
        if( !spell_being_cast.is_max_level() ) {
            // reap the reward
            int old_level = spell_being_cast.get_level();
            if( old_level == 0 ) {
                spell_being_cast.gain_level();
                p->add_msg_if_player( m_good,
                                      _( "Something about how this spell works just clicked!  You gained a level!" ) );
            } else {
                spell_being_cast.gain_exp( exp_gained );
                p->add_msg_if_player( m_good, _( "You gain %i experience.  New total %i." ), exp_gained,
                                      spell_being_cast.xp() );
            }
            if( spell_being_cast.get_level() != old_level ) {
                g->events().send<event_type::player_levels_spell>( spell_being_cast.id(),
                        spell_being_cast.get_level() );
            }
        }
    }
    if( !act->targets.empty() && act->targets.front() ) {
        item &it = *act->targets.front();
        if( !it.has_flag( flag_USE_PLAYER_ENERGY ) ) {
            p->consume_charges( it, it.type->charges_to_use() );
        }
    }
}

//This is just used for robofac_intercom_mission_2
void activity_handlers::mind_splicer_finish( player_activity *act, player *p )
{
    act->set_to_null();

    if( act->targets.size() != 1 || !act->targets[0] ) {
        debugmsg( "Incompatible arguments to: activity_handlers::mind_splicer_finish" );
        return;
    }
    item &data_card = *act->targets[0];
    p->add_msg_if_player( m_info, _( "…you finally find the memory banks." ) );
    p->add_msg_if_player( m_info, _( "The kit makes a copy of the data inside the bionic." ) );
    data_card.contents.clear_items();
    data_card.put_in( item::spawn( itype_mind_scan_robofac ) );
}
