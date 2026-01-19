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

#include "action.h"
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
#include "craft_command.h"
#include "crafting.h"
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
#include "magic.h"
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
static const activity_id ACT_DISMEMBER( "ACT_DISMEMBER" );
static const activity_id ACT_DISSECT( "ACT_DISSECT" );
static const activity_id ACT_EAT_MENU( "ACT_EAT_MENU" );
static const activity_id ACT_FERTILIZE_PLOT( "ACT_FERTILIZE_PLOT" );
static const activity_id ACT_FETCH_REQUIRED( "ACT_FETCH_REQUIRED" );
static const activity_id ACT_FIELD_DRESS( "ACT_FIELD_DRESS" );
static const activity_id ACT_FILL_LIQUID( "ACT_FILL_LIQUID" );
static const activity_id ACT_MILK( "ACT_MILK" );
static const activity_id ACT_FILL_PIT( "ACT_FILL_PIT" );
static const activity_id ACT_FIND_MOUNT( "ACT_FIND_MOUNT" );
static const activity_id ACT_FIRSTAID( "ACT_FIRSTAID" );
static const activity_id ACT_FISH( "ACT_FISH" );
static const activity_id ACT_FORAGE( "ACT_FORAGE" );
static const activity_id ACT_GAME( "ACT_GAME" );
static const activity_id ACT_GENERIC_GAME( "ACT_GENERIC_GAME" );
static const activity_id ACT_GUNMOD_ADD( "ACT_GUNMOD_ADD" );
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
static const activity_id ACT_PRY_NAILS( "ACT_PRY_NAILS" );
static const activity_id ACT_PULP( "ACT_PULP" );
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
static const activity_id ACT_START_FIRE( "ACT_START_FIRE" );
static const activity_id ACT_STUDY_SPELL( "ACT_STUDY_SPELL" );
static const activity_id ACT_TIDY_UP( "ACT_TIDY_UP" );
static const activity_id ACT_TOOLMOD_ADD( "ACT_TOOLMOD_ADD" );
static const activity_id ACT_TRAIN( "ACT_TRAIN" );
static const activity_id ACT_TRAVELLING( "ACT_TRAVELLING" );
static const activity_id ACT_TREE_COMMUNION( "ACT_TREE_COMMUNION" );
static const activity_id ACT_TRY_SLEEP( "ACT_TRY_SLEEP" );
static const activity_id ACT_VEHICLE( "ACT_VEHICLE" );
static const activity_id ACT_VEHICLE_DECONSTRUCTION( "ACT_VEHICLE_DECONSTRUCTION" );
static const activity_id ACT_VEHICLE_REPAIR( "ACT_VEHICLE_REPAIR" );
static const activity_id ACT_VIBE( "ACT_VIBE" );
static const activity_id ACT_TRAIN_SKILL( "ACT_TRAIN_SKILL" );
static const activity_id ACT_WAIT( "ACT_WAIT" );
static const activity_id ACT_WAIT_NPC( "ACT_WAIT_NPC" );
static const activity_id ACT_WAIT_STAMINA( "ACT_WAIT_STAMINA" );
static const activity_id ACT_WAIT_WEATHER( "ACT_WAIT_WEATHER" );
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
static const itype_id itype_welder( "welder" );
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

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
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
    { ACT_CRAFT, craft_do_turn },
    { ACT_FILL_LIQUID, fill_liquid_do_turn },
    { ACT_PICKAXE, pickaxe_do_turn },
    { ACT_PULP, pulp_do_turn },
    { ACT_GAME, game_do_turn },
    { ACT_GENERIC_GAME, generic_game_do_turn },
    { ACT_START_FIRE, start_fire_do_turn },
    { ACT_VIBE, vibe_do_turn },
    { ACT_TRAIN_SKILL, train_skill_do_turn },
    { ACT_HAND_CRANK, hand_crank_do_turn },
    { ACT_WEAR, wear_do_turn },
    { ACT_MULTIPLE_FISH, multiple_fish_do_turn },
    { ACT_MULTIPLE_CONSTRUCTION, multiple_construction_do_turn },
    { ACT_MULTIPLE_MINE, multiple_mine_do_turn },
    { ACT_MULTIPLE_BUTCHER, multiple_butcher_do_turn },
    { ACT_MULTIPLE_FARM, multiple_farm_do_turn },
    { ACT_FETCH_REQUIRED, fetch_do_turn },
    { ACT_EAT_MENU, eat_menu_do_turn },
    { ACT_VEHICLE_DECONSTRUCTION, vehicle_deconstruction_do_turn },
    { ACT_VEHICLE_REPAIR, vehicle_repair_do_turn },
    { ACT_MULTIPLE_CHOP_TREES, chop_trees_do_turn },
    { ACT_CONSUME_FOOD_MENU, consume_food_menu_do_turn },
    { ACT_CONSUME_DRINK_MENU, consume_drink_menu_do_turn },
    { ACT_CONSUME_MEDS_MENU, consume_meds_menu_do_turn },
    { ACT_MOVE_LOOT, move_loot_do_turn },
    { ACT_ADV_INVENTORY, adv_inventory_do_turn },
    { ACT_ARMOR_LAYERS, armor_layers_do_turn },
    { ACT_ATM, atm_do_turn },
    { ACT_CRACKING, cracking_do_turn },
    { ACT_FISH, fish_do_turn },
    { ACT_REPAIR_ITEM, repair_item_do_turn },
    { ACT_BUTCHER, butcher_do_turn },
    { ACT_BUTCHER_FULL, butcher_do_turn },
    { ACT_TRAVELLING, travel_do_turn },
    { ACT_FIELD_DRESS, butcher_do_turn },
    { ACT_BLEED, butcher_do_turn }, // unsure bout this one lol
    { ACT_SKIN, butcher_do_turn },
    { ACT_QUARTER, butcher_do_turn },
    { ACT_DISMEMBER, butcher_do_turn },
    { ACT_DISSECT, butcher_do_turn },
    { ACT_PRY_NAILS, pry_nails_do_turn },
    { ACT_CHOP_TREE, chop_tree_do_turn },
    { ACT_CHOP_LOGS, chop_tree_do_turn },
    { ACT_TIDY_UP, tidy_up_do_turn },
    { ACT_CHOP_PLANKS, chop_tree_do_turn },
    { ACT_TIDY_UP, tidy_up_do_turn },
    { ACT_JACKHAMMER, jackhammer_do_turn },
    { ACT_FIND_MOUNT, find_mount_do_turn },
    { ACT_FILL_PIT, fill_pit_do_turn },
    { ACT_MULTIPLE_CHOP_PLANKS, multiple_chop_planks_do_turn },
    { ACT_FERTILIZE_PLOT, fertilize_plot_do_turn },
    { ACT_TRY_SLEEP, try_sleep_do_turn },
    { ACT_OPERATION, operation_do_turn },
    { ACT_ROBOT_CONTROL, robot_control_do_turn },
    { ACT_TREE_COMMUNION, tree_communion_do_turn },
    { ACT_STUDY_SPELL, study_spell_do_turn},
    { ACT_READ, read_do_turn},
    { ACT_WAIT_STAMINA, wait_stamina_do_turn }
};

const std::map< activity_id, std::function<void( player_activity *, player * )> >
activity_handlers::finish_functions = {
    { ACT_BURROW, burrow_finish },
    { ACT_BUTCHER, butcher_finish },
    { ACT_BUTCHER_FULL, butcher_finish },
    { ACT_FIELD_DRESS, butcher_finish },
    { ACT_SKIN, butcher_finish },
    { ACT_BLEED, butcher_finish },
    { ACT_QUARTER, butcher_finish },
    { ACT_DISMEMBER, butcher_finish },
    { ACT_DISSECT, butcher_finish },
    { ACT_FIRSTAID, firstaid_finish },
    { ACT_FISH, fish_finish },
    { ACT_FORAGE, forage_finish },
    { ACT_HOTWIRE_CAR, hotwire_finish },
    { ACT_MAKE_ZLAVE, make_zlave_finish },
    { ACT_PICKAXE, pickaxe_finish },
    { ACT_RELOAD, reload_finish },
    { ACT_START_FIRE, start_fire_finish },
    { ACT_TRAIN, train_finish },
    { ACT_CHURN, churn_finish },
    { ACT_PLANT_SEED, plant_seed_finish },
    { ACT_VEHICLE, vehicle_finish },
    { ACT_START_ENGINES, start_engines_finish },
    { ACT_PULP, pulp_finish },
    { ACT_CRACKING, cracking_finish },
    { ACT_REPAIR_ITEM, repair_item_finish },
    { ACT_MEND_ITEM, mend_item_finish },
    { ACT_GUNMOD_ADD, gunmod_add_finish },
    { ACT_TOOLMOD_ADD, toolmod_add_finish },
    { ACT_CLEAR_RUBBLE, clear_rubble_finish },
    { ACT_MEDITATE, meditate_finish },
    { ACT_READ, read_finish },
    { ACT_WAIT, wait_finish },
    { ACT_WAIT_WEATHER, wait_weather_finish },
    { ACT_WAIT_NPC, wait_npc_finish },
    { ACT_WAIT_STAMINA, wait_stamina_finish },
    { ACT_SOCIALIZE, socialize_finish },
    { ACT_TRY_SLEEP, try_sleep_finish },
    { ACT_OPERATION, operation_finish },
    { ACT_VIBE, vibe_finish },
    { ACT_TRAIN_SKILL, train_skill_finish },
    { ACT_ATM, atm_finish },
    { ACT_EAT_MENU, eat_menu_finish },
    { ACT_CONSUME_FOOD_MENU, eat_menu_finish },
    { ACT_CONSUME_DRINK_MENU, eat_menu_finish },
    { ACT_CONSUME_MEDS_MENU, eat_menu_finish },
    { ACT_PRY_NAILS, pry_nails_finish },
    { ACT_CHOP_TREE, chop_tree_finish },
    { ACT_MILK, milk_finish },
    { activity_id( "ACT_SHEAR" ), shear_finish },
    { ACT_CHOP_LOGS, chop_logs_finish },
    { ACT_CHOP_PLANKS, chop_planks_finish },
    { ACT_JACKHAMMER, jackhammer_finish },
    { ACT_FILL_PIT, fill_pit_finish },
    { ACT_PLAY_WITH_PET, play_with_pet_finish },
    { ACT_SHAVE, shaving_finish },
    { ACT_HAIRCUT, haircut_finish },
    { ACT_ROBOT_CONTROL, robot_control_finish },
    { ACT_MIND_SPLICER, mind_splicer_finish },
    { ACT_SPELLCASTING, spellcasting_finish },
    { ACT_STUDY_SPELL, study_spell_finish }
};

bool activity_handlers::resume_for_multi_activities( player &p )
{
    if( !p.backlog.empty() ) {
        activity_ptr &back_act = p.backlog.front();
        if( back_act->is_multi_type() ) {
            p.assign_activity( p.backlog.front()->id() );
            p.backlog.clear();
            return true;
        }
    }
    return false;
}

void activity_handlers::burrow_do_turn( player_activity *act, player * )
{
    sfx::play_activity_sound( "activity", "burrow", sfx::get_heard_volume( act->placement ) );
    if( calendar::once_every( 1_minutes ) ) {
        sounds::sound( act->placement, 10, sounds::sound_t::movement,
                       //~ Sound of a Rat mutant burrowing!
                       _( "ScratchCrunchScrabbleScurry." ) );
    }
}

void activity_handlers::burrow_finish( player_activity *act, player *p )
{
    tripoint pos = act->placement; // make a copy to avoid use-after-free
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
    }
    act->set_to_null();
    p->add_msg_if_player( m_good, _( "You finish burrowing." ) );
    here.destroy( pos, true );
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

static void extract_or_wreck_cbms( std::vector<detached_ptr<item>> &cbms, int roll,
                                   player &p )
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
            if( p.is_npc() ) {
                drop_on_map( p, item_drop_reason::deliberate, { std::move( it ) }, p.pos() );
            } else {
                liquid_handler::handle_all_liquid( std::move( it ), PICKUP_RANGE );
            }
        } else {
            get_map().add_item( p.pos(), std::move( it ) );
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
    for( const tripoint &pt : here.points_in_radius( u.pos(), PICKUP_RANGE ) ) {
        if( here.has_flag( flag_TREE, pt ) ) {
            has_tree_nearby = true;
            break;
        }
    }
    bool b_rack_present = false;
    for( const tripoint &pt : here.points_in_radius( u.pos(), PICKUP_RANGE ) ) {
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
            if( !( here.has_nearby_table( u.pos(), PICKUP_RANGE ) || inv.has_item_with( []( const item & it ) {
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

static void set_up_butchery_activity( player_activity &act, player &u, const butchery_setup &setup )
{
    const auto print_reasons = [&u, &setup]() {
        for( const std::string &prob : setup.problems ) {
            u.add_msg_if_player( m_bad, prob );
        }
        if( setup.problems.empty() ) {
            for( const std::string &info : setup.info ) {
                u.add_msg_if_player( m_info, info );
            }
        }
    };

    if( setup.can_do == butchery_possibility::never ) {
        act.set_to_null();
        print_reasons();
        return;
    }
    if( setup.can_do == butchery_possibility::not_this ) {
        act.targets.pop_back();
        print_reasons();
        return;
    }
    if( setup.can_do == butchery_possibility::need_confirmation ) {
        if( u.is_player() ) {
            if( query_yn( _( "Would you dare desecrate the mortal remains of a fellow human being?" ) ) ) {
                switch( rng( 1, 3 ) ) {
                    case 1:
                        u.add_msg_if_player( m_bad, _( "You clench your teeth at the prospect of this gruesome job." ) );
                        break;
                    case 2:
                        u.add_msg_if_player( m_bad, _( "This will haunt you in your dreams." ) );
                        break;
                    case 3:
                        u.add_msg_if_player( m_bad,
                                             _( "You try to look away, but this gruesome image will stay on your mind for some time." ) );
                        break;
                }
                g->u.add_morale( MORALE_BUTCHER, -50, 0, 2_days, 3_hours );
            } else {
                u.add_msg_if_player( m_good, _( "It needs a coffin, not a knife." ) );
                act.targets.pop_back();
                return;
            }
        } else {
            u.add_morale( MORALE_BUTCHER, -50, 0, 2_days, 3_hours );
        }
    }

    print_reasons();
    act.tools.clear();
    act.speed.calc_all_moves( u );
    act.moves_left = setup.move_cost;
    act.moves_total = setup.move_cost;
    // We have a valid target, so preform the full finish function
    // instead of just selecting the next valid target
    act.index = false;
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

static void butchery_drops_harvest( item *corpse_item, const mtype &mt, player &p,
                                    const std::function<int()> &roll_butchery, butcher_type action,
                                    const std::function<double()> &roll_drops )
{
    p.add_msg_if_player( m_neutral, mt.harvest->message() );
    int monster_weight = to_gram( mt.weight );
    monster_weight += std::round( monster_weight * rng_float( -0.1, 0.1 ) );
    if( corpse_item->has_flag( flag_QUARTERED ) ) {
        monster_weight *= 0.95;
    }
    if( corpse_item->has_flag( flag_GIBBED ) ) {
        monster_weight = std::round( 0.85 * monster_weight );
        if( action != F_DRESS ) {
            p.add_msg_if_player( m_bad,
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
                        p.add_msg_if_player( m_bad,
                                             _( "You notice something embedded in the corpse, perhaps harvestable via careful dissection." ) );
                    }
                    continue;
                }
                p.add_msg_if_player( m_bad,
                                     _( "You notice there are implants in this corpse, that careful dissection might preserve." ) );
                continue;
            }
            if( action == BUTCHER || action == BUTCHER_FULL || action == DISMEMBER ) {
                if( drop != nullptr && !drop->bionic ) {
                    if( one_in( 3 ) ) {
                        p.add_msg_if_player( m_bad,
                                             _( "Your butchering tool destroys something.  Perhaps a more surgical approach would allow harvesting it." ) );
                    }
                    continue;
                }
                switch( rng( 1, 3 ) ) {
                    case 1:
                        p.add_msg_if_player( m_bad,
                                             _( "Your butchering tool encounters something implanted in this corpse, but your rough cuts destroy it." ) );
                        break;
                    case 2:
                        p.add_msg_if_player( m_bad,
                                             _( "You find traces of implants in the body, but you care only for the flesh." ) );
                        break;
                    case 3:
                        p.add_msg_if_player( m_bad,
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
                    p.add_msg_if_player( m_bad, _( "You fail to harvest: %s" ), drop->nname( 1 ) );
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
                if( p.is_npc() || action != butcher_type::BLEED ) {
                    drop_on_map( p, item_drop_reason::deliberate, std::move( it ), p.pos() );
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
                if( !p.backlog.empty() && p.backlog.front()->id() == ACT_MULTIPLE_BUTCHER ) {
                    obj.set_var( "activity_var", p.name );
                }
                here.add_item_or_charges( p.pos(), std::move( it ) );
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
                if( !p.backlog.empty() && p.backlog.front()->id() == ACT_MULTIPLE_BUTCHER ) {
                    obj.set_var( "activity_var", p.name );
                }
                for( int i = 0; i != roll; ++i ) {
                    here.add_item_or_charges( p.pos(), item::spawn( obj ) );
                }
            }
            p.add_msg_if_player( m_good, _( "You harvest: %s" ), drop->nname( roll ) );
        }
        practice++;
    }
    // 20% of the original corpse weight is not an item, but liquid gore

    if( action != DISSECT ) {
        p.practice( skill_survival, std::max( 0, practice ), std::max( mt.size - creature_size::medium,
                    0 ) + 4 );
    }
}

static void butchery_quarter( item *corpse_item, const player &p )
{
    corpse_item->set_flag( flag_QUARTERED );
    p.add_msg_if_player( m_good,
                         _( "You roughly slice the corpse of %s into four parts and set them aside." ),
                         corpse_item->get_mtype()->nname() );
    map &here = get_map();
    // 4 quarters (one exists, add 3, flag does the rest)
    for( int i = 1; i <= 3; i++ ) {
        here.add_item_or_charges( p.pos(), item::spawn( *corpse_item ), true );
    }
}

void activity_handlers::butcher_finish( player_activity *act, player *p )
{
    // No targets means we are done
    if( act->targets.empty() ) {
        act->set_to_null();
        resume_for_multi_activities( *p );
        return;
    }

    map &here = get_map();
    safe_reference<item> &target = act->targets.back();
    const inventory &inv = p->crafting_inventory();

    // Corpses can disappear (rezzing!), so check for that
    if( !target || !target->is_corpse() ) {
        p->add_msg_if_player( m_info, _( "There's no corpse to butcher!" ) );
        act->set_to_null();
        return;
    }

    butcher_type action = BUTCHER;
    if( act->id() == ACT_BUTCHER ) {
        action = BUTCHER;
    } else if( act->id() == ACT_BUTCHER_FULL ) {
        action = BUTCHER_FULL;
    } else if( act->id() == ACT_FIELD_DRESS ) {
        action = F_DRESS;
    } else if( act->id() == ACT_QUARTER ) {
        action = QUARTER;
    } else if( act->id() == ACT_BLEED ) {
        action = BLEED;
    } else if( act->id() == ACT_DISSECT ) {
        action = DISSECT;
    } else if( act->id() == ACT_SKIN ) {
        action = SKIN;
    } else if( act->id() == ACT_DISMEMBER ) {
        action = DISMEMBER;
    }

    // index is a bool that determines if we are ready to start the next target
    if( act->index ) {
        const butchery_setup setup = consider_butchery( *target, *p, action );
        set_up_butchery_activity( *act, *p, setup );
        return;
    }

    item &corpse_item = *target;
    const mtype *corpse = corpse_item.get_mtype();
    const field_type_id type_blood = corpse->bloodType();
    const field_type_id type_gib = corpse->gibType();

    if( action == QUARTER ) {
        butchery_quarter( &corpse_item, *p );
        act->index = true;
        return;
    }

    int skill_level = p->get_skill_level( skill_survival );
    int factor = inv.max_quality( action == DISSECT ? qual_CUT_FINE :
                                  qual_BUTCHER );

    // DISSECT has special case factor calculation and results.
    if( action == DISSECT ) {
        skill_level = p->get_skill_level( skill_firstaid ) / 2;
        skill_level += p->get_skill_level( skill_electronics ) / 2;
        skill_level += inv.max_quality( qual_CUT_FINE );
        add_msg( m_debug, _( "Skill: %s" ), skill_level );
    }

    const auto roll_butchery = [&]() {
        double skill_shift = 0.0;
        ///\Relevant skill(s), plus tool quality if dissecting, consistently increases butcher rolls
        skill_shift += skill_level;
        ///\EFFECT_DEX >8 randomly increases butcher rolls, slightly, <8 decreases
        skill_shift += rng_float( 0, p->get_dex() - 8 ) / 4.0;
        if( factor < 0 ) {
            skill_shift -= rng_float( 0, -factor / 5.0 );
        }
        return static_cast<int>( std::round( skill_shift ) );
    };

    if( action == DISMEMBER ) {
        here.add_splatter( type_gib, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
    }

    //all BUTCHERY types - FATAL FAILURE
    if( action != DISSECT && roll_butchery() <= ( -15 ) && one_in( 2 ) ) {
        switch( rng( 1, 3 ) ) {
            case 1:
                p->add_msg_if_player( m_warning,
                                      _( "You hack up the corpse so unskillfully, that there is nothing left to salvage from this bloody mess." ) );
                break;
            case 2:
                p->add_msg_if_player( m_warning,
                                      _( "You wanted to cut the corpse, but instead you hacked the meat, spilled the guts all over it, and made a bloody mess." ) );
                break;
            case 3:
                p->add_msg_if_player( m_warning,
                                      _( "You made so many mistakes during the process that you doubt even vultures will be interested in what's left of it." ) );
                break;
        }

        // Remove the target from the map
        target->detach();

        act->targets.pop_back();

        here.add_splatter( type_gib, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
        here.add_splatter( type_blood, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
        for( int i = 1; i <= corpse->size; i++ ) {
            here.add_splatter_trail( type_gib, p->pos(), random_entry( here.points_in_radius( p->pos(),
                                     corpse->size + 1 ) ) );
            here.add_splatter_trail( type_blood, p->pos(), random_entry( here.points_in_radius( p->pos(),
                                     corpse->size + 1 ) ) );
        }

        // Ready to move on to the next item, if there is one
        act->index = true;
        return;
    }
    // function just for drop yields
    const auto roll_drops = [&]() {
        factor = std::max( factor, -50 );
        return 0.5 * skill_level / 10 + 0.3 * ( factor + 50 ) / 100 + 0.2 * p->dex_cur / 20;
    };
    // all action types - yields
    butchery_drops_harvest( &corpse_item, *corpse, *p, roll_butchery, action, roll_drops );
    // after this point, if there was a liquid handling from the harvest,
    // and the liquid handling was interrupted, then the activity was canceled,
    // therefore operations on this activities targets and values may be invalidated.
    // reveal hidden items / hidden content
    if( action == DISSECT ) {
        int roll = roll_butchery() - corpse_item.damage_level( 4 );
        roll = roll < 1 ? 1 : roll;
        add_msg( m_debug, _( "Roll penalty for corpse damage = %s" ), 0 - corpse_item.damage_level( 4 ) );
        std::vector<detached_ptr<item>> cbms = corpse_item.remove_components();
        std::vector<detached_ptr<item>> contents = corpse_item.contents.clear_items();
        for( detached_ptr<item> &it : contents ) {
            cbms.push_back( std::move( it ) );
        }
        extract_or_wreck_cbms( cbms, roll, *p );
        // those lines are for XP gain with dissecting. It depends on the size of the corpse, time to dissect the corpse and the amount of bionics you would gather.
        int time_to_cut = size_factor_in_time_to_cut( corpse->size ) / 100;
        int level_cap = std::min<int>( MAX_SKILL,
                                       ( static_cast<int>( corpse->size ) + ( cbms.size() * 2 + 1 ) ) );
        int size_mult = corpse->size > creature_size::medium ? ( corpse->size * corpse->size ) : 8;
        int practice_amt = ( size_mult + 1 ) * ( ( time_to_cut / 150 ) + 1 ) *
                           ( cbms.size() * cbms.size() / 2 + 1 );
        p->practice( skill_firstaid, practice_amt, level_cap );
        add_msg( m_debug, "Experience: %d, Level cap: %d, Time to cut: %d", practice_amt, level_cap,
                 time_to_cut );
    }

    //end messages and effects
    switch( action ) {
        case QUARTER:
            break;
        case BUTCHER:
            p->add_msg_if_player( m_good,
                                  _( "You apply few quick cuts to the %s and leave what's left of it for scavengers." ),
                                  corpse_item.tname() );

            // Remove the target from the map
            target->detach();
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;
        case BUTCHER_FULL:
            p->add_msg_if_player( m_good, _( "You finish butchering the %s." ), corpse_item.tname() );

            // Remove the target from the map
            target->detach();
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;
        case F_DRESS:
            // partial failure
            if( roll_butchery() < 0 ) {
                switch( rng( 1, 3 ) ) {
                    case 1:
                        p->add_msg_if_player( m_warning,
                                              _( "You unskillfully hack up the corpse and chop off some excess body parts.  You're left wondering how you did so poorly." ) );
                        break;
                    case 2:
                        p->add_msg_if_player( m_warning,
                                              _( "Your unskilled hands slip and damage the corpse.  You still hope it's not a total waste though." ) );
                        break;
                    case 3:
                        p->add_msg_if_player( m_warning,
                                              _( "You did something wrong and hacked the corpse badly.  Maybe it's still recoverable." ) );
                        break;
                }
                corpse_item.set_flag( flag_FIELD_DRESS_FAILED );

                here.add_splatter( type_gib, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
                here.add_splatter( type_blood, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
                for( int i = 1; i <= corpse->size; i++ ) {
                    here.add_splatter_trail( type_gib, p->pos(), random_entry( here.points_in_radius( p->pos(),
                                             corpse->size + 1 ) ) );
                    here.add_splatter_trail( type_blood, p->pos(), random_entry( here.points_in_radius( p->pos(),
                                             corpse->size + 1 ) ) );
                }

            } else {
                // success
                switch( rng( 1, 3 ) ) {
                    case 1:
                        p->add_msg_if_player( m_good, _( "You field dress the %s." ), corpse->nname() );
                        break;
                    case 2:
                        p->add_msg_if_player( m_good,
                                              _( "You slice the corpse's belly and remove intestines and organs, until you're confident that it will not rot from inside." ) );
                        break;
                    case 3:
                        p->add_msg_if_player( m_good,
                                              _( "You remove guts and excess parts, preparing the corpse for later use." ) );
                        break;
                }
                corpse_item.set_flag( flag_FIELD_DRESS );

                here.add_splatter( type_gib, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
                here.add_splatter( type_blood, p->pos(), rng( corpse->size + 2, ( corpse->size + 1 ) * 2 ) );
                for( int i = 1; i <= corpse->size; i++ ) {
                    here.add_splatter_trail( type_gib, p->pos(), random_entry( here.points_in_radius( p->pos(),
                                             corpse->size + 1 ) ) );
                    here.add_splatter_trail( type_blood, p->pos(), random_entry( here.points_in_radius( p->pos(),
                                             corpse->size + 1 ) ) );
                }

            }
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;

        case BLEED:
            // Only one message variant for now
            p->add_msg_if_player( m_good, _( "You bleed the %s." ), corpse->nname() );
            corpse_item.set_flag( flag_BLED );
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;
        case SKIN:
            switch( rng( 1, 4 ) ) {
                case 1:
                    p->add_msg_if_player( m_good, _( "You skin the %s." ), corpse->nname() );
                    break;
                case 2:
                    p->add_msg_if_player( m_good, _( "You carefully remove the hide from the %s" ),
                                          corpse->nname() );
                    break;
                case 3:
                    p->add_msg_if_player( m_good,
                                          _( "The %s is challenging to skin, but you get a good hide from it." ),
                                          corpse->nname() );
                    break;
                case 4:
                    p->add_msg_if_player( m_good, _( "With a few deft slices you take the skin from the %s" ),
                                          corpse->nname() );
                    break;
            }
            corpse_item.set_flag( flag_SKINNED );
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;
        case DISMEMBER:
            switch( rng( 1, 3 ) ) {
                case 1:
                    p->add_msg_if_player( m_good, _( "You hack the %s apart." ), corpse_item.tname() );
                    break;
                case 2:
                    p->add_msg_if_player( m_good, _( "You lop the limbs off the %s." ), corpse_item.tname() );
                    break;
                case 3:
                    p->add_msg_if_player( m_good, _( "You cleave the %s into pieces." ), corpse_item.tname() );
            }

            // Remove the target from the map
            target->detach();
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;
        case DISSECT:
            p->add_msg_if_player( m_good, _( "You finish dissecting the %s." ), corpse_item.tname() );

            // Remove the target from the map
            target->detach();
            if( !act->targets.empty() ) {
                act->targets.pop_back();
            }
            break;
    }

    // Ready to move on to the next item, if there is one (for example if multibutchering)
    act->index = true;
    // if its mutli-tile butchering,then restart the backlog.
    resume_for_multi_activities( *p );
}

void activity_handlers::shear_finish( player_activity *act, player *p )
{
    if( act->coords.empty() ) {
        debugmsg( "shearing activity with no position of monster stored" );
        return;
    }
    safe_reference<item> &loc = act->targets[ 0 ];

    if( !loc ) {
        debugmsg( "shearing item location lost" );
        return;
    }
    item *shears = &*loc;
    map &here = get_map();
    const tripoint source_pos = here.getlocal( act->coords.at( 0 ) );
    monster *source_mon = g->critter_at<monster>( source_pos );
    if( source_mon == nullptr ) {
        debugmsg( "could not find source creature for shearing" );
        return;
    }
    // 22 wool staples corresponds to an average wool-producing sheep yield of 10 lbs or so
    for( int i = 0; i != 22; ++i ) {
        detached_ptr<item> wool_staple = item::spawn( itype_wool_staple, calendar::turn );
        here.add_item_or_charges( p->pos(), std::move( wool_staple ) );
    }
    source_mon->add_effect( effect_sheared, calendar::season_length() );
    if( !act->str_values.empty() && act->str_values[0] == "temp_tie" ) {
        source_mon->remove_effect( effect_tied );
    }
    act->set_to_null();
    if( shears->type->can_have_charges() ) {
        p->consume_charges( *shears, shears->type->charges_to_use() );
    }
}

void activity_handlers::milk_finish( player_activity *act, player *p )
{
    if( act->coords.empty() ) {
        debugmsg( "milking activity with no position of monster stored" );
        return;
    }
    map &here = get_map();
    const tripoint source_pos = here.getlocal( act->coords.at( 0 ) );
    monster *source_mon = g->critter_at<monster>( source_pos );
    if( source_mon == nullptr ) {
        debugmsg( "could not find source creature for liquid transfer" );
        return;
    }
    auto milked_item = source_mon->ammo.find( source_mon->type->starting_ammo.begin()->first );
    if( milked_item == source_mon->ammo.end() ) {
        debugmsg( "animal has no milkable ammo type" );
        return;
    }
    if( milked_item->second <= 0 ) {
        debugmsg( "started milking but udders are now empty before milking finishes" );
        return;
    }
    detached_ptr<item> milk = item::spawn( milked_item->first, calendar::turn, milked_item->second );
    liquid_handler::handle_liquid( std::move( milk ) );
    // NOLINTNEXTLINE(bugprone-use-after-move)
    if( !milk ) {
        milked_item->second = 0;
        p->add_msg_if_player( _( "The %s's udders run dry." ), source_mon->get_name() );
    } else {
        milked_item->second = milk->charges;
    }
    // if the monster was not manually tied up, but needed to be fixed in place temporarily then
    // remove that now.
    if( !act->str_values.empty() && act->str_values[0] == "temp_tie" ) {
        source_mon->remove_effect( effect_tied );
    }
    act->set_to_null();
}

void activity_handlers::fill_liquid_do_turn( player_activity *act, player *p )
{
    player_activity &act_ref = *act;
    map &here = get_map();
    try {

        // 1. Prepare source lambda
        liquid_source_type source_type = static_cast<liquid_source_type>( act_ref.values.at( 0 ) );
        auto transfer = [source_type, &here,
                     &act_ref]( const std::function < detached_ptr<item>( detached_ptr<item> &&it ) > & cb ) {
            tripoint pos = act_ref.coords.at( 0 );
            static const units::volume volume_per_second = units::from_liter( 4.0F / 6.0F );
            int charges;
            detached_ptr<item> source;
            switch( source_type ) {
                case LST_INFINITE_MAP:
                    source = here.water_from( pos );
                    charges = std::max( 1, source->charges_per_volume( volume_per_second ) );
                    source->charges = charges;
                    source = cb( std::move( source ) );
                    return source && source->charges == charges;
                case LST_VEHICLE:
                    auto vp = here.veh_at( pos );
                    if( !vp ) {
                        debugmsg( "Lost track of vehicle source for fill_liquid activity" );
                    }
                    item &base = vp->vehicle().part( act_ref.values.at( 1 ) ).get_base();
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
        // 2. Transfer charges.
        switch( static_cast<liquid_target_type>( act_ref.values.at( 2 ) ) ) {
            case LTT_VEHICLE:
                if( const optional_vpart_position vp = here.veh_at( act_ref.coords.at( 1 ) ) ) {
                    finished = transfer( [&p, &vp]( detached_ptr<item> &&it ) {
                        return p->pour_into( vp->vehicle(), std::move( it ) );
                    } );
                } else {
                    throw std::runtime_error( "could not find target vehicle for liquid transfer" );
                }
                break;
            case LTT_MAP:
                if( iexamine::has_keg( act_ref.coords.at( 1 ) ) ) {
                    finished = transfer( [&act_ref]( detached_ptr<item> &&it ) {
                        return iexamine::pour_into_keg( act_ref.coords.at( 1 ), std::move( it ) );
                    } );
                } else {
                    finished = transfer( [&p, &act_ref, &here]( detached_ptr<item> &&it ) {
                        p->add_msg_if_player( _( "You pour %1$s onto the ground." ), it->tname() );
                        here.add_item_or_charges( act_ref.coords.at( 1 ), std::move( it ) );
                        return detached_ptr<item>();
                    } );
                }
                break;
            case LTT_MONSTER:
                //Do nothing here
                break;
            case LTT_CONTAINER:
                safe_reference<item> &container = act_ref.targets.at( 0 );
                if( !container ) {
                    throw std::runtime_error( "could not find target container for liquid transfer" );
                }

                finished = transfer( [&p, &container]( detached_ptr<item> &&it ) {
                    return p->pour_into( *container,  std::move( it ) );
                } );

                break;
        }
        if( finished ) {
            act_ref.set_to_null();
        }

    } catch( const std::runtime_error &err ) {
        debugmsg( "error in activity data: \"%s\"", err.what() );
        act_ref.set_to_null();
        return;
    }
}

void activity_handlers::firstaid_finish( player_activity *act, player *p )
{
    static const std::string iuse_name_string( "heal" );

    item &it = *act->targets.front();
    item *used_tool = it.get_usable_item( iuse_name_string );
    if( used_tool == nullptr ) {
        debugmsg( "Lost tool used for healing" );
        act->set_to_null();
        return;
    }

    const use_function *use_fun = used_tool->get_use( iuse_name_string );
    const heal_actor *actor = dynamic_cast<const heal_actor *>( use_fun->get_actor_ptr() );
    if( actor == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act->set_to_null();
        return;
    }

    // TODO: Store the patient somehow, retrieve here
    player &patient = *p;
    const bodypart_str_id healed = bodypart_str_id( act->str_values[0] );
    const int charges_consumed = actor->finish_using( *p, patient, *used_tool, healed );
    p->consume_charges( it, charges_consumed );

    // Erase activity and values.
    act->set_to_null();
    act->values.clear();
}

void activity_handlers::forage_finish( player_activity *act, player *p )
{
    // Don't forage if we aren't next to the bush - otherwise we get weird bugs
    bool next_to_bush = false;
    map &here = get_map();
    for( const tripoint &pnt : here.points_in_radius( p->pos(), 1 ) ) {
        if( here.getabs( pnt ) == act->placement ) {
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

    here.ter_set( here.getlocal( act->placement ), next_ter );

    // Survival gives a bigger boost, and Perception is leveled a bit.
    // Both survival and perception affect time to forage

    ///\EFFECT_PER slightly increases forage success chance
    ///\EFFECT_SURVIVAL increases forage success chance
    if( veggy_chance < p->get_skill_level( skill_survival ) * 3 + p->per_cur - 2 ) {
        const std::vector<item *> dropped = here.put_items_from_loc( loc, p->pos(), calendar::turn );
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
                                            p->pos(),
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
    if( calendar::once_every( 1_minutes ) ) {
        // So 30 points per play
        p->add_morale( MORALE_GAME, 2, 60, 2_hours, 30_minutes, true );
        return;
    }
}

void activity_handlers::game_do_turn( player_activity *act, player *p )
{
    item &game_item = *act->targets.front();

    // Consume battery charges for every minute spent playing
    if( calendar::once_every( 1_minutes ) ) {
        int energy = game_item.ammo_required();
        energy -= game_item.ammo_consume( energy, p->pos() );
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

void activity_handlers::hotwire_finish( player_activity *act, player *p )
{
    //Grab this now, in case the vehicle gets shifted
    if( const optional_vpart_position vp = g->m.veh_at( g->m.getlocal( tripoint( act->values[0],
                                           act->values[1],
                                           p->posz() ) ) ) ) {
        vehicle *const veh = &vp->vehicle();
        const int mech_skill = act->values[2];
        if( mech_skill > rng( 1, 6 ) ) {
            //success
            veh->is_locked = false;
            add_msg( _( "This wire will start the engine." ) );
        } else if( mech_skill > rng( 0, 4 ) ) {
            //soft fail
            veh->is_locked = false;
            veh->is_alarm_on = veh->has_security_working();
            add_msg( _( "This wire will probably start the engine." ) );
        } else if( veh->is_alarm_on ) {
            veh->is_locked = false;
            add_msg( _( "By process of elimination, this wire will start the engine." ) );
        } else {
            //hard fail
            veh->is_alarm_on = veh->has_security_working();
            add_msg( _( "The red wire always starts the engine, doesn't it?" ) );
        }
    } else {
        debugmsg( "process_activity ACT_HOTWIRE_CAR: vehicle not found" );
    }
    act->set_to_null();
}

void activity_handlers::make_zlave_finish( player_activity *act, player *p )
{
    act->set_to_null();
    map_stack items = g->m.i_at( p->pos() );
    const std::string corpse_name = act->str_values[0];
    item *body = nullptr;

    for( item *&it : items ) {
        if( it->display_name() == corpse_name ) {
            body = it;
        }
    }

    if( body == nullptr ) {
        add_msg( m_info, _( "There's no corpse to make into a zombie slave!" ) );
        return;
    }

    int success = act->values[0];

    if( success > 0 ) {

        p->practice( skill_firstaid, rng( 2, 5 ) );
        p->practice( skill_survival, rng( 2, 5 ) );

        p->add_msg_if_player( m_good,
                              _( "You slice muscles and tendons, and remove body parts until you're confident the zombie won't be able to attack you when it reanimates." ) );

        body->set_var( "zlave", "zlave" );
        //take into account the chance that the body yet can regenerate not as we need.
        if( one_in( 10 ) ) {
            body->set_var( "zlave", "mutilated" );
        }

    } else if( success > -20 ) {

        p->practice( skill_firstaid, rng( 3, 6 ) );
        p->practice( skill_survival, rng( 3, 6 ) );

        p->add_msg_if_player( m_warning,
                              _( "You hack into the corpse and chop off some body parts.  You think the zombie won't be able to attack when it reanimates." ) );

        success += rng( 1, 20 );

        if( success > 0 && !one_in( 5 ) ) {
            body->set_var( "zlave", "zlave" );
        } else {
            body->set_var( "zlave", "mutilated" );
        }

    } else {

        p->practice( skill_firstaid, rng( 1, 8 ) );
        p->practice( skill_survival, rng( 1, 8 ) );

        body->mod_damage( rng( 0, body->max_damage() - body->damage() ), DT_STAB );
        if( body->damage() == body->max_damage() ) {
            body->deactivate();
            p->add_msg_if_player( m_warning, _( "You cut up the corpse too much, it is thoroughly pulped." ) );
        } else {
            p->add_msg_if_player( m_warning,
                                  _( "You cut into the corpse trying to make it unable to attack, but you don't think you have it right." ) );
        }
    }
}

void activity_handlers::pickaxe_do_turn( player_activity *act, player * )
{
    const tripoint &pos = get_map().getlocal( act->placement );
    sfx::play_activity_sound( "tool", "pickaxe", sfx::get_heard_volume( pos ) );
    // each turn is too much
    if( calendar::once_every( 1_minutes ) ) {
        //~ Sound of a Pickaxe at work!
        sounds::sound( pos, 30, sounds::sound_t::destructive_activity, _( "CHNK!  CHNK!  CHNK!" ) );
    }
}

void activity_handlers::pickaxe_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const tripoint pos( here.getlocal( act->placement ) );
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
    if( !act->tools.empty() ) {
        item &it = *act->tools.front();
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

void activity_handlers::pulp_do_turn( player_activity *act, player *p )
{
    map &here = get_map();
    const tripoint &pos = here.getlocal( act->placement );

    // Stabbing weapons are a lot less effective at pulping
    const int cut_power = std::max( p->primary_weapon().damage_melee( DT_CUT ),
                                    p->primary_weapon().damage_melee( DT_STAB ) / 2 );

    ///\EFFECT_STR increases pulping power, with diminishing returns
    float pulp_power = std::sqrt( ( p->str_cur + p->primary_weapon().damage_melee( DT_BASH ) ) *
                                  ( cut_power + 1.0f ) );
    float pulp_effort = p->str_cur + p->primary_weapon().damage_melee( DT_BASH );
    // Multiplier to get the chance right + some bonus for survival skill
    pulp_power *= 40 + p->get_skill_level( skill_survival ) * 5;

    const int mess_radius = p->primary_weapon().has_flag( flag_MESSY ) ? 2 : 1;

    int moves = 0;
    // use this to collect how many corpse are pulped
    int &num_corpses = act->index;
    map_stack corpse_pile = here.i_at( pos );
    for( item *&corpse : corpse_pile ) {
        const mtype *corpse_mtype = corpse->get_mtype();
        if( !corpse->is_corpse() || ( !corpse_mtype->has_flag( MF_REVIVES ) &&
                                      !corpse_mtype->zombify_into ) ||
            ( std::ranges::find( act->str_values, "auto_pulp_no_acid" ) !=
              act->str_values.end() && corpse_mtype->bloodType().obj().has_acid ) ) {
            // Don't smash non-rezing corpses //don't smash acid zombies when auto pulping
            continue;
        }

        while( corpse->damage() < corpse->max_damage() ) {
            // Increase damage as we keep smashing ensuring we eventually smash the target.
            if( x_in_y( pulp_power, corpse->volume() / units::legacy_volume_factor ) ) {
                corpse->inc_damage( DT_BASH );
                if( corpse->damage() == corpse->max_damage() ) {
                    num_corpses++;
                }
            }

            if( x_in_y( pulp_power, corpse->volume() / units::legacy_volume_factor ) ) {
                // Splatter some blood around
                // Splatter a bit more randomly, so that it looks cooler
                const int radius = mess_radius + x_in_y( pulp_power, 500 ) + x_in_y( pulp_power, 1000 );
                const tripoint dest( pos + point( rng( -radius, radius ), rng( -radius, radius ) ) );
                const field_type_id type_blood = ( mess_radius > 1 && x_in_y( pulp_power, 10000 ) ) ?
                                                 corpse->get_mtype()->gibType() :
                                                 corpse->get_mtype()->bloodType();
                here.add_splatter_trail( type_blood, pos, dest );
            }

            p->mod_stamina( -pulp_effort );

            if( one_in( 4 ) ) {
                // Smashing may not be butchery, but it involves some zombie anatomy
                p->practice( skill_survival, 2, 2 );
            }

            float stamina_ratio = static_cast<float>( p->get_stamina() ) / p->get_stamina_max();
            moves += 100 / std::max( 0.25f, stamina_ratio );
            if( stamina_ratio < 0.33 || p->is_npc() ) {
                p->moves = std::min( 0, p->moves - moves );
                return;
            }
            if( moves >= p->moves ) {
                // Enough for this turn;
                p->moves -= moves;
                return;
            }
        }
        corpse->set_flag( flag_PULPED );
    }
    // If we reach this, all corpses have been pulped, finish the activity
    act->moves_left = 0;
    if( num_corpses == 0 ) {
        p->add_msg_if_player( m_bad, _( "The corpse moved before you could finish smashing it!" ) );
        return;
    }
    // TODO: Factor in how long it took to do the smashing.
    p->add_msg_player_or_npc( vgettext( "The corpse is thoroughly pulped.",
                                        "The corpses are thoroughly pulped.", num_corpses ),
                              vgettext( "<npcname> finished pulping the corpse.",
                                        "<npcname> finished pulping the corpses.", num_corpses ) );
}

void activity_handlers::pulp_finish( player_activity *act, player *p )
{
    if( p->is_npc() ) {
        npc *guy = dynamic_cast<npc *>( p );
        guy->revert_after_activity();
    } else {
        act->set_to_null();
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
        if( reloadable.type->gun->reload_noise_volume > 0 ) {
            sfx::play_variant_sound( "reload", reloadable.typeId().str(),
                                     sfx::get_heard_volume( p->pos() ) );
            sounds::ambient_sound( p->pos(), reloadable.type->gun->reload_noise_volume,
                                   sounds::sound_t::activity, reloadable.type->gun->reload_noise );
        }
    } else if( reloadable.is_watertight_container() ) {
        msg = _( "You refill the %s." );
    }
    add_msg( m_neutral, msg, reloadable.tname(), ammo_name );
}

void activity_handlers::start_fire_finish( player_activity *act, player *p )
{
    static const std::string iuse_name_string( "firestarter" );

    item &it = *act->tools.front();
    item *used_tool = it.get_usable_item( iuse_name_string );
    if( used_tool == nullptr ) {
        debugmsg( "Lost tool used for starting fire" );
        act->set_to_null();
        return;
    }

    const use_function *use_fun = used_tool->get_use( iuse_name_string );
    const firestarter_actor *actor = dynamic_cast<const firestarter_actor *>
                                     ( use_fun->get_actor_ptr() );
    if( actor == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act->set_to_null();
        return;
    }

    if( it.type->can_have_charges() ) {
        if( it.has_flag( flag_USE_UPS ) ) {
            p->use_charges( itype_UPS, it.type->charges_to_use() );
        }
        p->consume_charges( it, it.type->charges_to_use() );
    }
    p->practice( skill_survival, act->index, 5 );

    firestarter_actor::resolve_firestarter_use( *p, act->placement );
    act->set_to_null();
}

void activity_handlers::start_fire_do_turn( player_activity *act, player *p )
{
    map &here = get_map();
    item &firestarter = *act->tools.front();
    // Try fueling the fire if we don't already have fuel, OR if the tool needs to look for tinder to work
    if( !here.is_flammable( act->placement ) || ( firestarter.has_flag( flag_REQUIRES_TINDER ) &&
            !here.tinder_at( act->placement ) ) ) {
        try_fuel_fire( *act, *p, true );
        if( !here.is_flammable( act->placement ) ) {
            p->add_msg_if_player( m_info, _( "There's nothing to light there." ) );
            p->cancel_activity();
            return;
        }
    }

    if( firestarter.has_flag( flag_REQUIRES_TINDER ) ) {
        if( !here.tinder_at( act->placement ) ) {
            p->add_msg_if_player( m_info, _( "This item requires tinder to light." ) );
            p->cancel_activity();
            return;
        }
    }

    const use_function *usef = firestarter.type->get_use( "firestarter" );
    if( usef == nullptr || usef->get_actor_ptr() == nullptr ) {
        add_msg( m_bad, _( "You have lost the item you were using to start the fire." ) );
        p->cancel_activity();
        return;
    }

    p->mod_moves( -p->moves );
    const firestarter_actor *actor = dynamic_cast<const firestarter_actor *>( usef->get_actor_ptr() );
    const float light = actor->light_mod( p->pos() );
    act->moves_left -= light * 100;
    if( light < 0.1 ) {
        add_msg( m_bad, _( "There is not enough sunlight to start a fire now.  You stop trying." ) );
        p->cancel_activity();
    }
}

static bool magic_train( player_activity *act, player *p )
{
    if( !p ) {
        return false;
    }
    const spell_id &sp_id = spell_id( act->name );
    if( sp_id.is_valid() ) {
        const bool knows = g->u.magic->knows_spell( sp_id );
        if( knows ) {
            spell &studying = p->magic->get_spell( sp_id );
            const int expert_multiplier = act->values.empty() ? 0 : act->values[0];
            const int xp = roll_remainder( studying.exp_modifier( *p ) * expert_multiplier );
            studying.gain_exp( xp );
            p->add_msg_if_player( m_good, _( "You learn a little about the spell: %s" ),
                                  sp_id->name );
        } else {
            p->magic->learn_spell( act->name, *p );
            // you can decline to learn this spell , as it may lock you out of other magic.
            if( p->magic->knows_spell( sp_id ) ) {
                add_msg( m_good, _( "You learn %s." ), sp_id->name.translated() );
            } else {
                act->set_to_null();
            }
        }
        return true;
    }
    return false;
}

void activity_handlers::train_finish( player_activity *act, player *p )
{
    const skill_id sk( act->name );
    if( sk.is_valid() ) {
        const Skill &skill = sk.obj();
        std::string skill_name = skill.name();
        int old_skill_level = p->get_skill_level( sk );
        p->get_skill_level_object( sk ).train( 100 * ( old_skill_level + 1 ), true );
        int new_skill_level = p->get_skill_level( sk );
        if( old_skill_level != new_skill_level ) {
            add_msg( m_good, _( "You finish training %s to level %d." ),
                     skill_name, new_skill_level );
            g->events().send<event_type::gains_skill_level>( p->getID(), sk, new_skill_level );
        } else {
            add_msg( m_good, _( "You get some training in %s." ), skill_name );
        }
        act->set_to_null();
        return;
    }

    const matype_id &ma_id = matype_id( act->name );
    if( ma_id.is_valid() ) {
        const martialart &mastyle = ma_id.obj();
        // Trained martial arts,
        g->events().send<event_type::learns_martial_art>( p->getID(), ma_id );
        p->martial_arts_data->learn_style( mastyle.id, p->is_avatar() );
    } else if( !magic_train( act, p ) ) {
        debugmsg( "train_finish without a valid skill or style or spell name" );
    }

    act->set_to_null();
    return;
}

void activity_handlers::vehicle_finish( player_activity *act, player *p )
{
    map &here = get_map();
    //Grab this now, in case the vehicle gets shifted
    const optional_vpart_position vp = here.veh_at( here.getlocal( tripoint( act->values[0],
                                       act->values[1],
                                       act->values[7] ) ) );
    veh_interact::complete_vehicle( *p );
    // complete_vehicle set activity type to NULL if the vehicle
    // was completely dismantled, otherwise the vehicle still exist and
    // is to be examined again.
    if( act->is_null() ) {
        if( npc *guy = dynamic_cast<npc *>( p ) ) {
            guy->revert_after_activity();
            guy->set_moves( 0 );
        }
        return;
    }
    act->set_to_null();
    if( !p->is_npc() ) {
        if( act->values.size() < 8 ) {
            debugmsg( "process_activity invalid ACT_VEHICLE values:%d",
                      act->values.size() );
        } else {
            if( vp ) {
                here.invalidate_map_cache( g->get_levz() );
                // TODO: Z (and also where the activity is queued)
                // Or not, because the vehicle coordinates are dropped anyway
                if( !resume_for_multi_activities( *p ) ) {
                    g->exam_vehicle( vp->vehicle(), point( act->values[ 2 ], act->values[ 3 ] ) );
                }
                return;
            } else {
                debugmsg( "process_activity ACT_VEHICLE: vehicle not found" );
            }
        }
    }
}

void activity_handlers::hand_crank_do_turn( player_activity *act, player *p )
{
    // Hand-crank chargers seem to range from 2 watt (very common easily verified)
    // to 10 watt (suspicious claims from some manufacturers) sustained output.
    // It takes 2.4 minutes to produce 1kj at just slightly under 7 watts (25 kj per hour)
    // time-based instead of speed based because it's a sustained activity
    item &hand_crank_item = *act->tools.front();

    if( calendar::once_every( 144_seconds ) ) {
        p->mod_fatigue( 1 );
        if( hand_crank_item.ammo_capacity() > hand_crank_item.ammo_remaining() ) {
            hand_crank_item.ammo_set( itype_battery, hand_crank_item.ammo_remaining() + 1 );
        } else {
            act->moves_left = 0;
            add_msg( m_info, _( "You've charged the battery completely." ) );
        }
    }
    if( p->get_fatigue() >= fatigue_levels::dead_tired ) {
        act->moves_left = 0;
        add_msg( m_info, _( "You're too exhausted to keep cranking." ) );
    }

}

void activity_handlers::vibe_do_turn( player_activity *act, player *p )
{
    //Using a vibrator takes time (10 minutes), not speed
    //Linear increase in morale during action with a small boost at end
    //Deduct 1 battery charge for every minute in use, or vibrator is much less effective
    item &vibrator_item = *act->tools.front();

    if( p->encumb( body_part_mouth ) >= 30 ) {
        act->moves_left = 0;
        add_msg( m_bad, _( "You have trouble breathing, and stop." ) );
    }

    if( calendar::once_every( 1_minutes ) ) {
        p->mod_fatigue( 1 );
        if( vibrator_item.ammo_remaining() > 0 ) {
            vibrator_item.ammo_consume( 1, p->pos() );
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



void activity_handlers::start_engines_finish( player_activity *act, player *p )
{
    act->set_to_null();
    // Find the vehicle by looking for a remote vehicle first, then by player relative coordinates
    vehicle *veh = g->remoteveh();
    map &here = get_map();
    if( !veh ) {
        const tripoint pos = act->placement + g->u.pos();
        veh = veh_pointer_or_null( here.veh_at( pos ) );
        if( !veh ) {
            return;
        }
    }

    int attempted = 0;
    int non_muscle_attempted = 0;
    int started = 0;
    int non_muscle_started = 0;
    int non_combustion_started = 0;
    const bool take_control = act->values[0];

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

    //Did any engines start?
    veh->engine_on = started;
    //init working engine noise
    sfx::do_vehicle_engine_sfx();

    if( attempted == 0 ) {
        add_msg( m_info, _( "The %s doesn't have an engine!" ), veh->name );
    } else if( non_muscle_attempted > 0 ) {
        //Some non-muscle engines tried to start
        if( non_muscle_attempted == non_muscle_started ) {
            //All of the non-muscle engines started
            add_msg( vgettext( "The %s's engine starts up.",
                               "The %s's engines start up.", non_muscle_started ), veh->name );
        } else if( non_muscle_started > 0 ) {
            //Only some of the non-muscle engines started
            add_msg( vgettext( "One of the %s's engines start up.",
                               "Some of the %s's engines start up.", non_muscle_started ), veh->name );
        } else if( non_combustion_started > 0 ) {
            //Non-combustions "engines" started
            add_msg( _( "The %s is ready for movement." ), veh->name );
        } else {
            //All of the non-muscle engines failed
            add_msg( m_bad, vgettext( "The %s's engine fails to start.",
                                      "The %s's engines fail to start.", non_muscle_attempted ), veh->name );
        }
    }

    if( take_control && !veh->engine_on && !veh->velocity ) {
        p->controlling_vehicle = false;
        add_msg( _( "You let go of the controls." ) );
    }
}

void activity_handlers::cracking_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( m_good, _( "With a satisfying click, the lock on the safe opens!" ) );
    get_map().furn_set( act->placement, f_safe_c );
    act->set_to_null();
}

// TODO (https://github.com/cataclysmbn/Cataclysm-BN/issues/1612):
// Remove that repair code after repair_activity_actor.

enum repeat_type : int {
    // REPEAT_INIT should be zero. In some scenarios (veh welder), activity value default to zero.
    REPEAT_INIT = 0,    // Haven't found repeat value yet.
    REPEAT_ONCE,        // Repeat just once
    REPEAT_FOREVER,     // Repeat for as long as possible
    REPEAT_FULL,        // Repeat until damage==0
    REPEAT_EVENT,       // Repeat until something interesting happens
    REPEAT_CANCEL,      // Stop repeating
};

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

namespace
{
enum class hack_type_t : int {
    vehicle = 0,
    furniture = 1
};

std::optional<hack_type_t> get_hack_type( const player_activity &activity )
{
    // Uses real tool
    if( activity.values.size() < 2 ) {
        return std::nullopt;
    }
    assert( !activity.coords.empty() );
    // Old save data, probably
    if( activity.values.size() == 2 ) {
        return hack_type_t::vehicle;
    }
    return static_cast<hack_type_t>( activity.values[2] );
}

tripoint get_position( const player_activity &activity )
{
    return activity.coords.at( 0 );
}

item *get_fake_tool( hack_type_t hack_type, const player_activity &activity )
{
    const tripoint position = get_position( activity );
    const map &m = get_map();
    //TODO!: chhhecks of big
    item *fake_item = &null_item_reference();

    switch( hack_type ) {
        case hack_type_t::vehicle: {
            const optional_vpart_position pos = m.veh_at( position );
            if( !pos ) {
                debugmsg( "Failed to find vehicle while using it for repair at %s", position.to_string() );
                return fake_item;
            }
            const vehicle &veh = pos->vehicle();

            fake_item = item::spawn_temporary( activity.str_values[1], calendar::turn, 0 );
            fake_item->charges = veh.fuel_left( itype_battery );

            break;
        }
        case hack_type_t::furniture: {
            if( !m.has_furn( position ) ) {
                debugmsg( "Failed to find furniture while using it for repair at %s", position.to_string() );
                // Return nullitem in that case
                return fake_item;
            }
            const furn_t &furniture = m.furn( position ).obj();
            const std::vector<itype> item_type_list = furniture.crafting_pseudo_item_types();

            if( item_type_list.empty() ) {
                return fake_item;
            }

            for( const itype &item_type : item_type_list ) {
                if( item_type.get_id() == static_cast<itype_id>( activity.str_values[1] ) ) {
                    const tripoint_abs_ms abspos( m.getabs( position ) );
                    const distribution_grid &grid = get_distribution_grid_tracker().grid_at( abspos );
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

void discharge_real_power_source(
    hack_type_t hack_type,
    const tripoint &position,
    item &tool,
    const int original_charges
)
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
            const tripoint_abs_ms abspos( m.getabs( position ) );
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

} // namespace

void patch_activity_for_vehicle(
    player_activity &activity,
    const tripoint &veh_part_position,
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
    activity.coords = { veh_part_position };
    activity.values = {
        // Because we called only on start of repair
        static_cast<int>( repeat_type::REPEAT_INIT ),
        crafter_index,
        static_cast<int>( hack_type_t::vehicle )
    };
    activity.str_values.emplace_back( static_cast<std::string>( it ) );
}

void patch_activity_for_furniture( player_activity &activity,
                                   const tripoint &furniture_position,
                                   const itype_id &itt )
{
    // Player may start another activity on welder/soldering iron
    // Check it here instead of furniture interaction code
    // because we want to encapsulate hack here.
    if( activity.id() != ACT_REPAIR_ITEM && activity.id() != ACT_TRAIN_SKILL ) {
        return;
    }

    // This tells activity, that real item doesn't exists in inventory.
    activity.index = INT_MIN;
    // Data for lookup furniture
    activity.coords = { furniture_position };
    activity.values = {
        // Because we called only on start of repair
        static_cast<int>( repeat_type::REPEAT_INIT ),
        0, // Useless for us, set only to be compatible with vehicle
        static_cast<int>( hack_type_t::furniture )
    };
    activity.str_values.emplace_back( static_cast<std::string>( itt ) );
}

} // namespace repair_activity_hack
} // namespace activity_handlers

void activity_handlers::train_skill_do_turn( player_activity *act, player *p )
{
    namespace hack = activity_handlers::repair_activity_hack;

    std::optional<hack::hack_type_t> hack_type = hack::get_hack_type( *act );
    const tripoint hack_pos = hack_type ? hack::get_position( * act ) : tripoint{};
    int hack_original_charges = 0;
    item *main_tool = nullptr;
    if( hack_type ) {
        main_tool = hack::get_fake_tool( hack_type.value(), *act );
        if( main_tool != nullptr ) {
            hack_original_charges = main_tool ? main_tool->charges : 0;
        }
    } else {
        main_tool = &*act->tools.front();
    }
    if( main_tool == nullptr ) {
        debugmsg( "train skill tools array and hack values are empty. this would have caused invalid safe reference error" );
        act->moves_left = 0;
        return;
    }
    item &skill_training_item = *main_tool;
    int training_skill_interval = atoi( p->get_value( "training_iuse_skill_interval" ).c_str() );

    if( training_skill_interval <= 0 ) {
        debugmsg( "training_iuse_skill_interval is invalid ( %d )", training_skill_interval );
        act->moves_left = 0;
        return;
    }

    if( calendar::once_every( 1_minutes * training_skill_interval ) ) {
        // pull metadata. this is probably the easiest way to get this data from the JSON definition
        std::string training_skill = p->get_value( "training_iuse_skill" );
        if( training_skill.empty() ) {
            debugmsg( "training_iuse_skill is empty" );
            act->moves_left = 0;
            return;
        }
        int training_skill_xp = atoi( p->get_value( "training_iuse_skill_xp" ).c_str() );
        int training_skill_max_level = atoi( p->get_value( "training_iuse_skill_xp_max_level" ).c_str() );
        int training_skill_xp_chance = atoi( p->get_value( "training_iuse_skill_xp_chance" ).c_str() );
        int training_skill_fatigue = atoi( p->get_value( "training_iuse_skill_fatigue" ).c_str() );

        p->mod_fatigue( training_skill_fatigue );
        if( skill_training_item.ammo_remaining() > 0 ) {
            skill_training_item.ammo_consume( 1, p->pos() );
            if( hack_type.has_value() ) {
                hack::discharge_real_power_source(
                    hack_type.value(),
                    hack_pos,
                    skill_training_item,
                    hack_original_charges
                );
            }
        } else if( skill_training_item.ammo_required() > 0 ) {
            act->moves_left = 0;
            add_msg( m_info, _( "The %s runs out of power." ), skill_training_item.tname() );
            return;
        }
        if( p->get_skill_level( skill_id( training_skill ) ) >= training_skill_max_level ) {
            act->moves_left = 0;
            add_msg( m_info, _( "You can no longer learn anything from this." ) );
            return;
        }
        if( rng( 1, 100 ) < training_skill_xp_chance ) {
            p->practice( skill_id( training_skill ), training_skill_xp,
                         training_skill_max_level );
        }
    }

    // needs rest
    if( p->get_fatigue() >= fatigue_levels::dead_tired ) {
        if( hack_type.has_value() ) {
            hack::discharge_real_power_source(
                hack_type.value(),
                hack_pos,
                skill_training_item,
                hack_original_charges
            );
        }
        act->moves_left = 0;
        add_msg( m_info, _( "You're too tired to continue." ) );
    }
}

void activity_handlers::repair_item_finish( player_activity *act, player *p )
{
    namespace hack = activity_handlers::repair_activity_hack;

    const std::string iuse_name_string = act->get_str_value( 0, "repair_item" );
    repeat_type repeat = static_cast<repeat_type>( act->get_value( 0, REPEAT_INIT ) );

    // nullopt if used real tool
    std::optional<hack::hack_type_t> hack_type = hack::get_hack_type( *act );
    item *fake_tool = nullptr;
    // real tool if used.
    item *ploc = nullptr;

    if( hack_type ) {
        fake_tool = hack::get_fake_tool( hack_type.value(), *act );
    } else {
        ploc = &*act->targets[0];
    }
    const tripoint hack_position = hack_type ? hack::get_position( *act ) : tripoint{};
    const int hack_original_charges = fake_tool ? fake_tool->charges : 0;

    item *main_tool = nullptr;
    if( hack_type.has_value() ) {
        main_tool = fake_tool;
    }
    if( main_tool == nullptr && ploc ) {
        main_tool = ploc;
    }
    if( main_tool == nullptr ) {
        main_tool = &p->i_at( act->index );
    }
    if( main_tool == nullptr ) {
        debugmsg( "Failed to get main_tool for long repair" );
        act->set_to_null();
        return;
    }

    item *used_tool = main_tool->get_usable_item( iuse_name_string );
    if( used_tool == nullptr ) {
        debugmsg( "Lost tool used for long repair" );
        act->set_to_null();
        return;
    }

    const use_function *use_fun = used_tool->get_use( iuse_name_string );
    // TODO: De-uglify this block. Something like get_use<iuse_actor_type>() maybe?
    const repair_item_actor *actor = dynamic_cast<const repair_item_actor *>
                                     ( use_fun->get_actor_ptr() );
    if( actor == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act->set_to_null();
        return;
    }

    // Valid Repeat choice and target, attempt repair.
    if( repeat != REPEAT_INIT && act->targets.size() >= 2 ) {
        safe_reference<item> &fix_location = act->targets[1];

        // Remember our level: we want to stop retrying on level up
        const int old_level = p->get_skill_level( actor->used_skill );
        const repair_item_actor::attempt_hint attempt = actor->repair( *p, *used_tool, *fix_location );
        if( attempt != repair_item_actor::AS_CANT ) {
            if( ploc && ploc->where() == item_location_type::map ) {
                used_tool->ammo_consume( used_tool->ammo_required(), ploc->position() );
            } else {
                p->consume_charges( *used_tool, used_tool->ammo_required() );
            }
            if( hack_type.has_value() ) {
                hack::discharge_real_power_source(
                    hack_type.value(),
                    hack_position,
                    *used_tool,
                    hack_original_charges
                );
            }
        }

        // TODO: Allow setting this in the actor
        // TODO: Don't use charges_to_use: welder has 50 charges per use, soldering iron has 1
        if( !used_tool->units_sufficient( *p ) ) {
            p->add_msg_if_player( _( "Your %s ran out of charges" ), used_tool->tname() );
            act->set_to_null();
            return;
        }

        // Print message explaining why we stopped
        // But only if we didn't destroy the item (because then it's obvious)
        const bool destroyed = attempt == repair_item_actor::AS_DESTROYED;
        const bool cannot_continue_repair = attempt == repair_item_actor::AS_CANT ||
                                            destroyed || !actor->can_repair_target( *p, *fix_location, !destroyed );
        if( cannot_continue_repair ) {
            // Cannot continue to repair target, select another target.
            // **Warning**: as soon as the item is popped back, it is destroyed and can't be used anymore!
            act->targets.pop_back();
        }

        const bool event_happened = attempt == repair_item_actor::AS_FAILURE ||
                                    attempt == repair_item_actor::AS_SUCCESS ||
                                    old_level != p->get_skill_level( actor->used_skill );

        const bool need_input =
            ( repeat == REPEAT_ONCE ) ||
            ( repeat == REPEAT_EVENT && event_happened ) ||
            ( repeat == REPEAT_FULL && ( cannot_continue_repair || fix_location->damage() <= 0 ) );
        if( need_input ) {
            repeat = REPEAT_INIT;
        }
    }

    // Check tool is valid before we query target and Repeat choice.
    if( !actor->can_use_tool( *p, *used_tool, true ) ) {
        act->set_to_null();
        return;
    }

    // target selection and validation.
    while( act->targets.size() < 2 ) {
        item *item_loc = game_menus::inv::repair( *p, actor, main_tool );

        if( item_loc == nullptr ) {
            p->add_msg_if_player( m_info, _( "Never mind." ) );
            act->set_to_null();
            return;
        }
        if( actor->can_repair_target( *p, *item_loc, true ) ) {
            act->targets.emplace_back( item_loc );
            repeat = REPEAT_INIT;
        }
    }

    const item &fix = *act->targets[1];

    if( repeat == REPEAT_INIT ) {
        const int level = p->get_skill_level( actor->used_skill );
        repair_item_actor::repair_type action_type = actor->default_action( fix, level );
        if( action_type == repair_item_actor::RT_NOTHING ) {
            p->add_msg_if_player( _( "You won't learn anything more by doing that." ) );
        }

        const std::pair<float, float> chance = actor->repair_chance( *p, fix, action_type );
        if( chance.first <= 0.0f ) {
            action_type = repair_item_actor::RT_PRACTICE;
        }

        std::string title = string_format( _( "%s %s\n" ),
                                           repair_item_actor::action_description( action_type ),
                                           fix.tname() );
        title += string_format( _( "Charges: <color_light_blue>%s/%s</color> %s (%s per use)\n" ),
                                used_tool->ammo_remaining(), used_tool->ammo_capacity(),
                                item::nname( used_tool->ammo_current() ),
                                used_tool->ammo_required() );
        title += string_format( _( "Skill used: <color_light_blue>%s (%s)</color>\n" ),
                                actor->used_skill->name(), level );
        title += string_format( _( "Success chance: <color_light_blue>%.1f</color>%%\n" ),
                                100.0f * chance.first );
        title += string_format( _( "Damage chance: <color_light_blue>%.1f</color>%%" ),
                                100.0f * chance.second );

        if( act->values.empty() ) {
            act->values.resize( 1 );
        }
        do {
            repeat = repeat_menu( title, repeat );

            if( repeat == REPEAT_CANCEL ) {
                act->set_to_null();
                return;
            }
            act->values[0] = static_cast<int>( repeat );
            // BACK selected, redo target selection next.
            if( repeat == REPEAT_INIT ) {
                p->activity->targets.pop_back();
                return;
            }
            if( repeat == REPEAT_FULL && fix.damage() <= 0 ) {
                p->add_msg_if_player( m_info, _( "Your %s is already fully repaired." ), fix.tname() );
                repeat = REPEAT_INIT;
            }
        } while( repeat == REPEAT_INIT );
    }

    // Otherwise keep retrying
    act->moves_left = actor->move_cost;
}

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

void activity_handlers::gunmod_add_finish( player_activity *act, player *p )
{
    act->set_to_null();
    // first unpack all of our arguments
    if( act->values.size() != 4 ) {
        debugmsg( "Insufficient arguments to ACT_GUNMOD_ADD" );
        return;
    }

    item &gun = *act->targets.at( 0 );
    item &mod = *act->targets.at( 1 );

    // chance of success (%)
    const int roll = act->values[1];
    // chance of damage (%)
    const int risk = act->values[2];

    // any tool charges used during installation
    const itype_id tool( act->name );
    const int qty = act->values[3];

    if( !gun.is_gunmod_compatible( mod ).success() ) {
        debugmsg( "Invalid arguments in ACT_GUNMOD_ADD" );
        return;
    }

    if( !tool.is_empty() && qty > 0 ) {
        p->use_charges( tool, qty );
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
                    p->remove_item( *mod );
                }
            }
            add_msg( m_bad, _( "You failed at installing the %s and destroyed your %s!" ), mod.tname(),
                     gun.tname() );
            gun.detach( );
        } else {
            add_msg( m_bad, _( "You failed at installing the %s and damaged your %s!" ), mod.tname(),
                     gun.tname() );
        }

    } else {
        add_msg( m_info, _( "You failed at installing the %s." ), mod.tname() );
    }
}

void activity_handlers::toolmod_add_finish( player_activity *act, player *p )
{
    act->set_to_null();
    if( act->targets.size() != 1 || !act->tools[0] || !act->targets[0] ) {
        debugmsg( "Incompatible arguments to ACT_TOOLMOD_ADD" );
        return;
    }
    item &tool = *act->tools[0];
    item &mod = *act->targets[0];
    p->add_msg_if_player( m_good, _( "You successfully attached the %1$s to your %2$s." ),
                          mod.tname(), tool.tname() );

    mod.set_flag( flag_IRREMOVABLE );
    tool.put_in( mod.detach() );
}

void activity_handlers::clear_rubble_finish( player_activity *act, player *p )
{
    const tripoint &pos = act->placement;
    map &here = get_map();
    const map_bash_info &bash = here.furn( pos ).obj().bash;
    p->add_msg_if_player( m_info, _( "You clear up the %s." ),
                          here.furnname( pos ) );
    here.spawn_items( pos, item_group::items_from( bash.drop_group, calendar::turn ) );
    here.furn_set( pos, f_null );

    act->set_to_null();
}

void activity_handlers::meditate_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( m_good, _( "You pause to engage in spiritual contemplation." ) );
    p->add_morale( MORALE_FEELING_GOOD, 5, 10 );
    act->set_to_null();
}

void activity_handlers::wear_do_turn( player_activity *act, player *p )
{
    activity_on_turn_wear( *act, *p );
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

void activity_handlers::move_loot_do_turn( player_activity *act, player *p )
{
    activity_on_turn_move_loot( *act, *p );
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
            const tripoint_abs_ms cur_omt_mid = midpoint( project_bounds<coords::ms>
                                                ( p->global_omt_location() ) );
            waypoint = clamp( cur_omt_mid, project_bounds<coords::ms>( next_omt ) );
        }
        map &here = get_map();
        // TODO: fix point types
        tripoint centre_sub = here.getlocal( waypoint.raw() );
        if( !here.passable( centre_sub ) ) {
            tripoint_range<tripoint> candidates = here.points_in_radius( centre_sub, 2 );
            for( const tripoint &elem : candidates ) {
                if( here.passable( elem ) ) {
                    centre_sub = elem;
                    break;
                }
            }
        }
        const std::vector<tripoint> route_to = here.route( p->pos(), centre_sub,
                                               p->get_legacy_pathfinding_settings(),
                                               p->get_legacy_path_avoid() );
        if( !route_to.empty() ) {
            const activity_id act_travel = ACT_TRAVELLING;
            p->set_destination( route_to, std::make_unique<player_activity>( act_travel ) );
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
    iexamine::atm( *p, p->pos() );
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
            p->pos(), item::make_corpse( fish_mon, calendar::turn +
                                         rng( 0_turns, 3_hours ) ) );

        p->add_msg_if_player( m_good, _( "You caught a %s." ), fish_mon.obj().nname() );
    } else {
        itype_id possible( caught->first );
        if( possible.is_valid() ) {
            here.add_item_or_charges( p->pos(), item::spawn( caught->first, calendar::turn, caught->second ),
                                      true );
            p->add_msg_if_player( m_good, _( "You reeled in %s." ) );
        }
    }

    for( item *&elem : here.i_at( p->pos() ) ) {
        if( elem->is_corpse() && !elem->has_var( "activity_var" ) ) {
            elem->set_var( "activity_var", p->name );
        }
    }
}

void activity_handlers::fish_do_turn( player_activity *act, player *p )
{
    int fishing_mult = iuse::good_fishing_spot( act->placement );
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
    item &rod = *act->tools.front();
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
    if( calendar::once_every( 60_minutes ) ) {
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
    const int effective_moves = p->moves / vision_mod;
    if( effective_moves <= act->moves_left ) {
        act->moves_left -= effective_moves;
        p->moves = 0;
    } else {
        p->moves -= act->moves_left * vision_mod;
        act->moves_left = 0;
    }
}

void activity_handlers::butcher_do_turn( player_activity * /*act*/, player *p )
{
    p->mod_stamina( -20 );
}

void activity_handlers::read_do_turn( player_activity *act, player *p )
{
    if( p->is_player() ) {
        if( !act->str_values.empty() && act->str_values[0] == "martial_art" && one_in( 3 ) ) {
            if( act->values.empty() ) {
                act->values.push_back( p->get_stamina() );
            }
            p->set_stamina( act->values[0] - 1 );
            act->values[0] = p->get_stamina();
        }
    } else {
        p->moves = 0;
    }

    if( calendar::once_every( 1_minutes ) ) {
        safe_reference<item> &loc = act->targets[0];
        if( !loc || !loc->is_book() ) {
            p->add_msg_if_player( m_bad, _( "You lost your book!  You stop reading." ) );
            act->set_to_null();
        }
    }
}

void activity_handlers::read_finish( player_activity *act, player *p )
{
    if( !act || !act->targets.front() ) {
        debugmsg( "Lost target of ACT_READ" );
        return;
    }
    if( p->is_npc() ) {
        npc *guy = dynamic_cast<npc *>( p );
        guy->finish_read( &*act->targets.front() );
    } else {
        if( avatar *u = dynamic_cast<avatar *>( p ) ) {
            u->do_read( &*act->targets.front() );
        } else {
            act->set_to_null();
        }
        if( !act ) {
            p->add_msg_if_player( m_info, _( "You finish reading." ) );
        }
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
    if( rl_dist( guy.pos(), mon->pos() ) <= 1 ) {
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
        const std::vector<tripoint> route = route_adjacent( *p, guy.chosen_mount.lock()->pos() );
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

void activity_handlers::wait_npc_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( _( "%s finishes with you…" ), act->str_values[0] );
    act->set_to_null();
}

void activity_handlers::wait_stamina_do_turn( player_activity *act, player *p )
{
    int stamina_threshold = p->get_stamina_max();
    if( !act->values.empty() ) {
        stamina_threshold = act->values[0];
        // remember initial stamina, only for waiting-with-threshold
        if( act->values.size() == 1 ) {
            act->values.push_back( p->get_stamina() );
        }
    }
    if( p->get_stamina() >= stamina_threshold ) {
        wait_stamina_finish( act, p );
    }
}

void activity_handlers::wait_stamina_finish( player_activity *act, player *p )
{
    if( !act->values.empty() ) {
        const int stamina_threshold = act->values[0];
        const int stamina_initial = ( act->values.size() > 1 ) ? act->values[1] : p->get_stamina();
        if( p->get_stamina() < stamina_threshold && p->get_stamina() <= stamina_initial ) {
            debugmsg( "Failed to wait until stamina threshold %d reached, only at %d. You may not be regaining stamina.",
                      act->values.front(), p->get_stamina() );
        }
    } else if( p->get_stamina() < p->get_stamina_max() ) {
        p->add_msg_if_player( _( "You are bored of waiting, so you stop." ) );
    } else {
        p->add_msg_if_player( _( "You finish waiting and feel refreshed." ) );
    }
    act->set_to_null();
}

void activity_handlers::socialize_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( _( "%s finishes chatting with you." ), act->str_values[0] );
    act->set_to_null();
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
        if( calendar::once_every( 30_minutes ) ) {
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

void activity_handlers::operation_do_turn( player_activity *act, player *p )
{
    /**
    - values[0]: Difficulty
    - values[1]: success
    - values[2]: max_power_level
    - values[3]: pl_skill
    - str_values[0]: install/uninstall
    - str_values[1]: bionic_id
    - str_values[2]: installer_name
    - str_values[3]: bool autodoc
    */
    enum operation_values_ids {
        operation_type = 0,
        cbm_id = 1,
        installer_name = 2,
        is_autodoc = 3
    };
    const bionic_id bid( act->str_values[cbm_id] );
    const bionic_id upbid = bid->upgraded_bionic;
    const bool autodoc = act->str_values[is_autodoc] == "true";
    const bool u_see = g->u.sees( p->pos() ) && ( !g->u.has_effect( effect_narcosis ) ||
                       g->u.has_bionic( bio_painkiller ) || g->u.has_trait( trait_NOPAIN ) );

    const int difficulty = act->values.front();

    const std::vector<bodypart_id> bps = get_occupied_bodyparts( bid );

    const time_duration half_op_duration = difficulty * 10_minutes;
    const time_duration message_freq = difficulty * 2_minutes;
    time_duration time_left = time_duration::from_turns( act->moves_left / 100 );

    map &here = get_map();

    // check if player is on an autodoc couch
    if( autodoc && here.inbounds( p->pos() ) ) {
        // this checks if there's an autodoc in a 3D radius around the player (during the operation), excluding just above/below him
        const std::list<tripoint> autodocs = here.find_furnitures_or_vparts_with_flag_in_radius( p->pos(),
                                             1,
                                             flag_AUTODOC );
        if( !here.has_flag_furn_or_vpart( flag_AUTODOC_COUCH, p->pos() ) || autodocs.empty() ) {
            p->remove_effect( effect_under_op );
            act->set_to_null();

            if( u_see ) {
                add_msg( m_bad, _( "The autodoc suffers a catastrophic failure." ) );

                p->add_msg_player_or_npc( m_bad,
                                          _( "The Autodoc's failure damages you greatly." ),
                                          _( "The Autodoc's failure damages <npcname> greatly." ) );
            }
            if( !bps.empty() ) {
                for( const bodypart_id &bp : bps ) {
                    p->add_effect( effect_bleed, 1_hours, bp.id(), difficulty );
                    p->apply_damage( nullptr, bp, 20 * difficulty );

                    if( u_see ) {
                        p->add_msg_player_or_npc( m_bad, _( "Your %s is ripped open." ),
                                                  _( "<npcname>'s %s is ripped open." ), body_part_name_accusative( bp->token ) );
                    }

                    if( bp == bodypart_id( "eyes" ) ) {
                        p->add_effect( effect_blind, 1_hours, bodypart_str_id::NULL_ID() );
                    }
                }
            } else {
                p->add_effect( effect_bleed, 1_hours, bodypart_str_id::NULL_ID(), difficulty );
                p->apply_damage( nullptr, bodypart_id( "torso" ), 20 * difficulty );
            }
        }
    }

    if( time_left > half_op_duration ) {
        if( !bps.empty() ) {
            for( const bodypart_id &bp : bps ) {
                if( calendar::once_every( message_freq ) && u_see && autodoc ) {
                    p->add_msg_player_or_npc( m_info,
                                              _( "The Autodoc is meticulously cutting your %s open." ),
                                              _( "The Autodoc is meticulously cutting <npcname>'s %s open." ),
                                              body_part_name_accusative( bp->token ) );
                }
            }
        } else {
            if( calendar::once_every( message_freq ) && u_see ) {
                p->add_msg_player_or_npc( m_info,
                                          _( "The Autodoc is meticulously cutting you open." ),
                                          _( "The Autodoc is meticulously cutting <npcname> open." ) );
            }
        }
    } else if( time_left == half_op_duration ) {
        if( act->str_values[operation_type] == "uninstall" ) {
            if( u_see && autodoc ) {
                add_msg( m_info, _( "The Autodoc attempts to carefully extract the bionic." ) );
            }

            if( p->has_bionic( bid ) ) {
                p->perform_uninstall( bid, act->values[0], act->values[1],
                                      units::from_joule( act->values[2] ), act->values[3] );
            } else {
                debugmsg( _( "Tried to uninstall %s, but you don't have this bionic installed." ), bid.c_str() );
                p->remove_effect( effect_under_op );
                act->set_to_null();
            }
        } else {
            if( u_see && autodoc ) {
                add_msg( m_info, _( "The Autodoc attempts to carefully insert the bionic." ) );
            }

            if( bid.is_valid() ) {
                p->perform_install( bid, upbid, act->values[0], act->values[1], act->values[3],
                                    act->str_values[installer_name], bid->canceled_mutations );
            } else {
                debugmsg( _( "%s is no a valid bionic_id" ), bid.c_str() );
                p->remove_effect( effect_under_op );
                act->set_to_null();
            }
        }
    } else if( act->values[1] > 0 ) {
        if( !bps.empty() ) {
            for( const bodypart_id &bp : bps ) {
                if( calendar::once_every( message_freq ) && u_see && autodoc ) {
                    p->add_msg_player_or_npc( m_info,
                                              _( "The Autodoc is stitching your %s back up." ),
                                              _( "The Autodoc is stitching <npcname>'s %s back up." ),
                                              body_part_name_accusative( bp->token ) );
                }
            }
        } else {
            if( calendar::once_every( message_freq ) && u_see && autodoc ) {
                p->add_msg_player_or_npc( m_info,
                                          _( "The Autodoc is stitching you back up." ),
                                          _( "The Autodoc is stitching <npcname> back up." ) );
            }
        }
    } else {
        if( calendar::once_every( message_freq ) && u_see && autodoc ) {
            p->add_msg_player_or_npc( m_bad,
                                      _( "The Autodoc is moving erratically through the rest of its program, not actually stitching your wounds." ),
                                      _( "The Autodoc is moving erratically through the rest of its program, not actually stitching <npcname>'s wounds." ) );
        }
    }

    // Makes sure NPC is still under anesthesia
    if( p->has_effect( effect_narcosis ) ) {
        const time_duration remaining_time = p->get_effect_dur( effect_narcosis );
        if( remaining_time <= time_left ) {
            const time_duration top_off_time = time_left - remaining_time;
            p->add_effect( effect_narcosis, top_off_time );
            p->add_effect( effect_sleep, top_off_time );
        }
    } else {
        p->add_effect( effect_narcosis, time_left );
        p->add_effect( effect_sleep, time_left );
    }
}

void activity_handlers::try_sleep_finish( player_activity *act, player *p )
{
    if( !p->has_effect( effect_sleep ) ) {
        p->add_msg_if_player( _( "You try to sleep, but can't…" ) );
    }
    act->set_to_null();
}

void activity_handlers::operation_finish( player_activity *act, player *p )
{
    map &here = get_map();
    if( act->str_values[3] == "true" ) {
        if( act->values[1] > 0 ) {
            add_msg( m_good,
                     _( "The Autodoc returns to its resting position after successfully performing the operation." ) );
            const std::list<tripoint> autodocs = here.find_furnitures_or_vparts_with_flag_in_radius( p->pos(),
                                                 1,
                                                 flag_AUTODOC );
            sounds::sound( autodocs.front(), 10, sounds::sound_t::music,
                           _( "a short upbeat jingle: \"Operation successful\"" ), true,
                           "Autodoc",
                           "success" );
        } else {
            if( act->str_values[0] == "install" ) {
                add_msg( m_warning,
                         _( "The Autodoc completes installation and activates bionic but reports about complications during operation." ) );
                const std::list<tripoint> autodocs = here.find_furnitures_or_vparts_with_flag_in_radius( p->pos(),
                                                     1,
                                                     flag_AUTODOC );
                sounds::sound( autodocs.front(), 10, sounds::sound_t::music,
                               _( "a sad beeping noise: \"Complications detected!  Report to medical personnel immediately!\"" ),
                               true,
                               "Autodoc",
                               "failure" );
            } else {
                add_msg( m_bad,
                         _( "The Autodoc jerks back to its resting position after failing the operation." ) );
                const std::list<tripoint> autodocs = here.find_furnitures_or_vparts_with_flag_in_radius( p->pos(),
                                                     1,
                                                     flag_AUTODOC );
                sounds::sound( autodocs.front(), 10, sounds::sound_t::music,
                               _( "a sad beeping noise: \"Operation failed\"" ), true,
                               "Autodoc",
                               "failure" );
            }

        }
    } else {
        if( act->values[1] > 0 ) {
            add_msg( m_good,
                     _( "The operation is a success." ) );
        } else {
            if( act->str_values[0] == "install" ) {
                add_msg( m_warning,
                         _( "Bionic was installed and activated but a complication happened during operation!" ) );
            } else {
                add_msg( m_bad,
                         _( "The operation is a failure." ) );
            }

        }
    }
    p->remove_effect( effect_under_op );
    act->set_to_null();
}

void activity_handlers::churn_finish( player_activity *act, player *p )
{
    map &here = get_map();
    p->add_msg_if_player( _( "You finish churning up the earth here." ) );
    here.ter_set( here.getlocal( act->placement ), t_dirtmound );
    // Go back to what we were doing before
    // could be player zone activity, or could be NPC multi-farming
    act->set_to_null();
    resume_for_multi_activities( *p );
}

void activity_handlers::plant_seed_finish( player_activity *act, player *p )
{
    map &here = get_map();
    tripoint examp = here.getlocal( act->placement );
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

void activity_handlers::fetch_do_turn( player_activity *act, player *p )
{
    generic_multi_activity_handler( *act, *p );
}

void activity_handlers::craft_do_turn( player_activity *act, player *p )
{
    item *craft = &*act->targets.front();

    // item_location::get_item() will return nullptr if the item is lost
    if( !craft ) {
        p->add_msg_player_or_npc(
            _( "You no longer have the in progress craft in your possession.  "
               "You stop crafting.  "
               "Reactivate the in progress craft to continue crafting." ),
            _( "<npcname> no longer has the in progress craft in their possession.  "
               "<npcname> stops crafting." ) );
        p->cancel_activity();
        return;
    }

    if( !craft->is_craft() ) {
        debugmsg( "ACT_CRAFT target '%s' is not a craft.  Aborting ACT_CRAFT.", craft->tname() );
        p->cancel_activity();
        return;
    }

    if( !p->can_continue_craft( *craft ) ) {
        p->cancel_activity();
        return;
    }

    const recipe &rec = craft->get_making();
    const tripoint bench_pos = act->coords.front();
    // Ugly
    bench_type bench_t = bench_type( act->values[1] );
    const float crafting_speed = crafting_speed_multiplier( *p, *craft, bench_location{bench_t, bench_pos} );
    const int assistants = p->available_assistant_count( craft->get_making() );
    const bool is_long = act->values[0];

    if( crafting_speed <= 0.0f ) {
        p->cancel_activity();
        return;
    }

    // item_counter represents the percent progress relative to the base batch time
    // stored precise to 5 decimal places ( e.g. 67.32 percent would be stored as 6'732'000 )
    const int old_counter = craft->get_counter();

    // Base moves for batch size with no speed modifier or assistants
    // Must ensure >= 1 so we don't divide by 0;
    const double base_total_moves = std::max( 1, rec.batch_time( craft->charges, 1.0f, 0 ) );
    // Current expected total moves, includes crafting speed modifiers and assistants
    const double cur_total_moves = std::max( 1, rec.batch_time( craft->charges, crafting_speed,
                                   assistants ) );
    // Delta progress in moves adjusted for current crafting speed
    const double delta_progress = p->get_moves() > 0
                                  ? p->get_moves() * base_total_moves / cur_total_moves
                                  : 0;
    // Current progress in moves
    const double current_progress = old_counter * base_total_moves / 10'000'000.0 + delta_progress;
    // Current progress as a percent of base_total_moves to 2 decimal places
    const auto new_counter_f = current_progress / base_total_moves * 10'000'000.0;
    // This is to ensure we don't over count skill steps
    const auto new_counter = std::min( static_cast<int>( std::round( new_counter_f ) ), 10'000'000 );
    craft->set_counter( new_counter );

    p->set_moves( 0 );

    // Skill and tools are gained/consumed after every 5% progress
    int five_percent_steps = craft->get_counter() / 500'000 - old_counter / 500'000;
    if( five_percent_steps > 0 ) {
        p->craft_skill_gain( *craft, five_percent_steps );
    }

    // Unlike skill, tools are consumed once at the start and should not be consumed at the end
    if( craft->get_counter() >= 10'000'000 ) {
        --five_percent_steps;
    }

    if( five_percent_steps > 0 ) {
        if( !p->craft_consume_tools( *craft, five_percent_steps, false ) ) {
            // So we don't skip over any tool comsuption
            craft->set_counter( craft->get_counter() - ( craft->get_counter() % 500'000 + 1 ) );
            p->cancel_activity();
            return;
        }
    }

    // if item_counter has reached 100% or more
    if( craft->get_counter() >= 10'000'000 ) {
        //TODO!: CHEEKY check
        item *craft_copy = craft;
        p->cancel_activity();
        complete_craft( *p, *craft_copy );
        act->targets.front()->detach();
        if( is_long ) {
            if( p->making_would_work( p->lastrecipe, craft_copy->charges ) ) {
                p->last_craft->execute( bench_pos );
            }
        }
    } else if( craft->get_counter() >= craft->get_next_failure_point() ) {
        bool destroy = craft->handle_craft_failure( *p );
        // If the craft needs to be destroyed, do it and stop crafting.
        if( destroy ) {
            p->add_msg_player_or_npc( _( "There is nothing left of the %s to craft from." ),
                                      _( "There is nothing left of the %s <npcname> was crafting." ), craft->tname() );
            act->targets.front()->detach();
            p->cancel_activity();
        }
    }
}

void activity_handlers::vibe_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( m_good, _( "You feel much better." ) );
    p->add_morale( MORALE_FEELING_GOOD, 10, 40 );
    act->set_to_null();
}

void activity_handlers::train_skill_finish( player_activity *act, player *p )
{
    p->add_msg_if_player( m_good, _( "You feel like you've learned a little bit." ) );
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
    sfx::play_activity_sound( "tool", "hammer", sfx::get_heard_volume( act->placement ) );
}

void activity_handlers::pry_nails_finish( player_activity *act, player *p )
{
    const tripoint &pnt = act->placement;
    map &here = get_map();
    const ter_id type = here.ter( pnt );

    p->add_msg_if_player( _( "You pry out the nails from the terrain." ) );

    p->practice( skill_fabrication, 1, 1 );
    here.spawn_item( p->pos(), itype_nail, 1, type->nail_pull_items[0] );
    here.spawn_item( p->pos(), itype_2x4, type->nail_pull_items[1] );
    here.ter_set( pnt, type->nail_pull_result );
    act->set_to_null();
}

void activity_handlers::chop_tree_do_turn( player_activity *act, player * )
{
    map &here = get_map();
    sfx::play_activity_sound( "tool", "axe", sfx::get_heard_volume( here.getlocal( act->placement ) ) );
    if( calendar::once_every( 1_minutes ) ) {
        //~ Sound of a wood chopping tool at work!
        sounds::sound( here.getlocal( act->placement ), 15, sounds::sound_t::activity, _( "CHK!" ) );
    }
}

void activity_handlers::chop_tree_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const tripoint &pos = here.getlocal( act->placement );

    tripoint direction;
    if( !p->is_npc() ) {
        if( p->backlog.empty() || p->backlog.front()->id() != ACT_MULTIPLE_CHOP_TREES ) {
            while( true ) {
                if( const std::optional<tripoint> dir = choose_direction(
                        _( "Select a direction for the tree to fall in." ) ) ) {
                    direction = *dir;
                    break;
                }
                // try again
            }
        }
    } else {
        // Try to safely fell tree
        std::vector<tripoint> valid_directions;

        for( const tripoint &elem : here.points_in_radius( pos, 1 ) ) {
            bool cantuse = false;
            tripoint direc = elem - pos;
            tripoint proposed_to = pos + point( 3 * direction.x, 3 * direction.y );
            std::vector<tripoint> rough_tree_line = line_to( pos, proposed_to );
            for( const tripoint &elem : rough_tree_line ) {
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

    const tripoint to = pos + 3 * direction.xy() + point( rng( -1, 1 ), rng( -1, 1 ) );
    std::vector<tripoint> tree = line_to( pos, to, rng( 1, 8 ) );
    for( const tripoint &elem : tree ) {
        here.batter( elem, 300, 5 );
        here.ter_set( elem, t_trunk );
    }

    here.ter_set( pos, t_stump );
    p->add_msg_if_player( m_good, _( "You finish chopping down a tree." ) );
    here.collapse_at( pos, false, true, false );
    // sound of falling tree
    sfx::play_variant_sound( "misc", "timber",
                             sfx::get_heard_volume( here.getlocal( act->placement ) ) );
    act->set_to_null();

    // Quality of tool used and assistants can together both reduce intensity of work.
    if( act->tools.empty() ) {
        debugmsg( "woodcutting item location not set" );
        resume_for_multi_activities( *p );
        return;
    }

    safe_reference<item> &loc = act->tools[ 0 ];
    if( !loc ) {
        debugmsg( "woodcutting item location lost" );
        resume_for_multi_activities( *p );
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

    resume_for_multi_activities( *p );
}

void activity_handlers::chop_logs_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const tripoint &pos = here.getlocal( act->placement );
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

    safe_reference<item> &loc = act->tools[ 0 ];
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
        here.spawn_item( here.getlocal( act->placement ), itype_2x4, planks, 0, calendar::turn );
        p->add_msg_if_player( m_good, _( "You produce %d planks." ), planks );
    }
    if( scraps > 0 ) {
        here.spawn_item( here.getlocal( act->placement ), itype_splinter, scraps, 0, calendar::turn );
        p->add_msg_if_player( m_good, _( "You produce %d splinters." ), scraps );
    }
    if( planks < max_planks / 2 ) {
        p->add_msg_if_player( m_bad, _( "You waste a lot of the wood." ) );
    }
    act->set_to_null();
    resume_for_multi_activities( *p );
}

void activity_handlers::jackhammer_do_turn( player_activity *act, player * )
{
    map &here = get_map();
    sfx::play_activity_sound( "tool", "jackhammer",
                              sfx::get_heard_volume( here.getlocal( act->placement ) ) );
    if( calendar::once_every( 1_minutes ) ) {
        sounds::sound( here.getlocal( act->placement ), 15, sounds::sound_t::destructive_activity,
                       //~ Sound of a jackhammer at work!
                       _( "TATATATATATATAT!" ) );
    }
}

void activity_handlers::jackhammer_finish( player_activity *act, player *p )
{
    map &here = get_map();
    const tripoint &pos = here.getlocal( act->placement );

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
    }
    p->add_msg_player_or_npc( m_good,
                              _( "You finish drilling." ),
                              _( "<npcname> finishes drilling." ) );
    act->set_to_null();
    if( !act->tools.empty() ) {
        item &it = *act->tools.front();
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

void activity_handlers::fill_pit_do_turn( player_activity *act, player * )
{
    sfx::play_activity_sound( "tool", "shovel", 100 );
    if( calendar::once_every( 1_minutes ) ) {
        //~ Sound of a shovel filling a pit or mound at work!
        sounds::sound( act->placement, 10, sounds::sound_t::activity, _( "hsh!" ) );
    }
}

void activity_handlers::fill_pit_finish( player_activity *act, player *p )
{
    const tripoint &pos = act->placement;
    map &here = get_map();
    const ter_id ter = here.ter( pos );
    const ter_id old_ter = ter;

    here.ter_set( pos, old_ter->fill_result );
    int act_exertion = to_moves<int>( time_duration::from_minutes( old_ter->fill_minutes ) );
    const int helpersize = character_funcs::get_crafting_helpers( *p, 3 ).size();
    act_exertion = act_exertion * ( 10 - helpersize ) / 10;
    p->mod_stored_kcal( std::min( -1, -act_exertion / to_moves<int>( 20_seconds ) ) );
    p->mod_thirst( std::max( 1, act_exertion / to_moves<int>( 3_minutes ) ) );
    p->mod_fatigue( std::max( 1, act_exertion / to_moves<int>( 90_seconds ) ) );
    p->add_msg_if_player( m_good, _( "You finish filling up %s." ), old_ter->name() );
    act->set_to_null();
}

void activity_handlers::play_with_pet_finish( player_activity *act, player *p )
{
    p->add_morale( MORALE_PLAY_WITH_PET, rng( 3, 10 ), 10, 5_hours, 25_minutes );
    p->add_msg_if_player( m_good, _( "Playing with your %s has lifted your spirits a bit." ),
                          act->str_values[0] );
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

std::vector<tripoint> get_sorted_tiles_by_distance( const tripoint &abspos,
        const std::unordered_set<tripoint> &tiles )
{
    const auto cmp = [abspos]( tripoint a, tripoint b ) {
        const int da = rl_dist( abspos, a );
        const int db = rl_dist( abspos, b );

        return da < db;
    };

    std::vector<tripoint> sorted( tiles.begin(), tiles.end() );
    std::ranges::sort( sorted, cmp );

    return sorted;
}

template<typename fn>
static void cleanup_tiles( std::unordered_set<tripoint> &tiles, fn &cleanup )
{
    auto it = tiles.begin();
    map &here = get_map();
    while( it != tiles.end() ) {
        auto current = it++;

        const tripoint &tile_loc = here.getlocal( *current );

        if( cleanup( tile_loc ) ) {
            tiles.erase( current );
        }
    }
}

static void perform_zone_activity_turn( player *p,
                                        const zone_type_id &ztype,
                                        const std::function<bool( const tripoint & )> &tile_filter,
                                        const std::function<void ( player &p, const tripoint & )> &tile_action,
                                        const std::string &finished_msg )
{
    const zone_manager &mgr = zone_manager::get_manager();
    map &here = get_map();
    const tripoint abspos = here.getabs( p->pos() );
    std::unordered_set<tripoint> unsorted_tiles = mgr.get_near( ztype, abspos );

    cleanup_tiles( unsorted_tiles, tile_filter );

    // sort remaining tiles by distance
    const std::vector<tripoint> &tiles = get_sorted_tiles_by_distance( abspos, unsorted_tiles );

    for( const tripoint &tile : tiles ) {
        const tripoint &tile_loc = here.getlocal( tile );

        std::vector<tripoint> route = here.route( p->pos(), tile_loc, p->get_legacy_pathfinding_settings(),
                                      p->get_legacy_path_avoid() );
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

    const auto reject_tile = [&]( const tripoint & tile ) {
        check_fertilizer();
        ret_val<bool> can_fert = iexamine::can_fertilize( *p, tile, fertilizer );
        return !can_fert.success();
    };

    const auto fertilize = [&]( player & p, const tripoint & tile ) {
        check_fertilizer();
        if( have_fertilizer() ) {
            iexamine::fertilize_plant( p, tile, fertilizer );
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

void activity_handlers::tree_communion_do_turn( player_activity *act, player *p )
{
    // There's an initial rooting process.
    if( act->values.front() > 0 ) {
        act->values.front() -= 1;
        if( act->values.front() == 0 ) {
            if( p->has_trait( trait_id( trait_SPIRITUAL ) ) ) {
                p->add_msg_if_player( m_good, _( "The ancient tree spirits answer your call." ) );
            } else {
                p->add_msg_if_player( m_good, _( "Your communion with the trees has begun." ) );
            }
        }
        return;
    }
    // Information is received every minute.
    if( !calendar::once_every( 1_minutes ) ) {
        return;
    }
    // Breadth-first search forest tiles until one reveals new overmap tiles.
    std::queue<tripoint_abs_omt> q;
    std::unordered_set<tripoint_abs_omt> seen;
    tripoint_abs_omt loc = p->global_omt_location();
    q.push( loc );
    seen.insert( loc );
    const std::function<bool( const oter_id & )> filter = []( const oter_id & ter ) {
        return ter.obj().is_wooded() || ter.obj().get_name() == "field";
    };
    while( !q.empty() ) {
        tripoint_abs_omt tpt = q.front();
        if( overmap_buffer.reveal( tpt, 3, filter ) ) {
            if( p->has_trait( trait_SPIRITUAL ) ) {
                p->add_morale( MORALE_TREE_COMMUNION, 2, 30, 8_hours, 6_hours );
            } else {
                p->add_morale( MORALE_TREE_COMMUNION, 1, 15, 2_hours, 1_hours );
            }
            if( one_in( 128 ) ) {
                p->add_msg_if_player( "%s", SNIPPET.random_from_category( "tree_communion" ).value_or(
                                          translation() ) );
            }
            return;
        }
        for( const tripoint_abs_omt &neighbor : points_in_radius( tpt, 1 ) ) {
            if( seen.contains( neighbor ) ) {
                continue;
            }
            seen.insert( neighbor );
            if( !overmap_buffer.ter( neighbor ).obj().is_wooded() ) {
                continue;
            }
            q.push( neighbor );
        }
        q.pop();
    }
    p->add_msg_if_player( m_info, _( "The trees have shown you what they will." ) );
    act->set_to_null();
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

    tripoint target = p->pos();
    bool target_is_valid = false;
    if( spell_being_cast.range() > 0 && !spell_being_cast.is_valid_target( target_none ) &&
        !spell_being_cast.has_flag( RANDOM_TARGET ) ) {
        do {
            avatar &you = *p->as_avatar();
            std::vector<tripoint> trajectory = target_handler::mode_spell( you, spell_being_cast, no_fail,
                                               no_mana );

            if( !trajectory.empty() ) {
                target = trajectory.back();
                target_is_valid = spell_being_cast.is_valid_target( *p, target );
                if( !( spell_being_cast.is_valid_target( target_ground ) || p->sees( target ) ) ) {
                    target_is_valid = false;
                }
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
        const std::optional<tripoint> target_ = spell_being_cast.random_valid_target( *p, p->pos() );
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
        sounds::sound( p->pos(), p->get_shout_volume() / 2, sounds::sound_t::speech, _( "cast a spell" ),
                       false );
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

void activity_handlers::study_spell_do_turn( player_activity *act, player *p )
{
    // moves_left decreases by player speed each turn and thus is a pain to work with
    // But we want a persistent value
    if( act->values.size() < 4 ) {
        act->values.push_back( 0 );
    }
    if( !character_funcs::can_see_fine_details( *p ) ) {
        act->values[2] = -1;
        act->moves_left = 0;
        return;
    }
    if( act->get_str_value( 1 ) == "study" ) {
        spell &studying = p->magic->get_spell( spell_id( act->name ) );
        const int old_level = studying.get_level();
        const int xp = roll_remainder( studying.exp_modifier( *p ) / to_turns<float>( 6_seconds ) );

        act->values[0] += xp;
        studying.gain_exp( xp );

        // This should trigger infrequently
        if( act->values[3] % 600 == 599 ) {
            // if we are at the first run through, we need to set spot 3 as 0.
            if( act->values.size() < 5 ) {
                act->values.push_back( 0 );
            }
            p->add_msg_if_player( m_good, _( "You gained %i experience in %s" ),
                                  act->values[0] - act->values[4], studying.name() );
            // This way we only display the difference
            act->values[4] = act->values[0];
        }

        // Every time we use get_level the level is recalculated, this is suboptimal, so we remember it here.
        const int new_level = studying.get_level();

        if( new_level > old_level ) {
            act->values[1] += new_level - old_level;
            g->events().send<event_type::player_levels_spell>( studying.id(), new_level );
            if( act->get_str_value( 0 ) == "gain_level" ) {
                act->moves_left = 0;
            }
        } else if( act->get_str_value( 0 ) == "gain_level" ) {
            act->moves_left = 1000000;
        }
    }
    // increment
    act->values[3] += 1;
}

void activity_handlers::study_spell_finish( player_activity *act, player *p )
{
    act->set_to_null();
    const int total_exp_gained = act->get_value( 0 );
    const int total_levels_gained = act->get_value( 1 );

    if( act->get_str_value( 1 ) == "study" ) {
        std::string level_string;
        if( total_levels_gained > 0 ) {
            level_string = string_format( vgettext( " and %d level", " and %d levels", total_levels_gained ),
                                          total_levels_gained );
        }
        p->add_msg_if_player( m_good, _( "You gained %i experience%s from your study session." ),
                              total_exp_gained, level_string );
        const spell &sp = p->magic->get_spell( spell_id( act->name ) );
        p->practice( sp.skill(), total_exp_gained, sp.get_difficulty() );
    } else if( act->get_str_value( 1 ) == "learn" && act->values[2] == 0 ) {
        p->magic->learn_spell( act->name, *p );
    }
    if( act->values[2] == -1 ) {
        p->add_msg_if_player( m_bad, _( "It's too dark to read." ) );
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
