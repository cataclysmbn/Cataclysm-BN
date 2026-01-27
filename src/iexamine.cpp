#include "iexamine.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

#include "action.h"
#include "activity_actor.h"
#include "activity_actor_definitions.h"
// TODO (https://github.com/cataclysmbn/Cataclysm-BN/issues/1612):
// Remove that include after repair_activity_actor.
#include "activity_handlers.h"
#include "active_tile_data_def.h"
#include "ammo.h"
#include "avatar.h"
#include "avatar_action.h"
#include "avatar_functions.h"
#include "bionics.h"
#include "bodypart.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_unreachable.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "character_functions.h"
#include "flag.h"
#include "color.h"
#include "construction.h"
#include "construction_group.h"
#include "construction_partial.h"
#include "coordinate_conversions.h"
#include "craft_command.h"
#include "cursesdef.h"
#include "damage.h"
#include "debug.h"
#include "distribution_grid.h"
#include "effect.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "field_type.h"
#include "flat_set.h"
#include "fungal_effects.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "handle_liquid.h"
#include "harvest.h"
#include "hunting_data.h"
#include "input.h"
#include "int_id.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "item_stack.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "line.h"
#include "magic_teleporter_list.h"
#include "map.h"
#include "map_iterator.h"
#include "map_selector.h"
#include "map_functions.h"
#include "mapdata.h"
#include "material.h"
#include "messages.h"
#include "monster.h"
#include "mongroup.h"
#include "mtype.h"
#include "mutation.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "pickup.h"
#include "pimpl.h"
#include "player.h"
#include "player_activity.h"
#include "pldata.h"
#include "point.h"
#include "recipe.h"
#include "relic.h"
#include "requirements.h"
#include "rng.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_input_popup.h"
#include "timed_event.h"
#include "translations.h"
#include "trap.h"
#include "ui.h"
#include "ui_manager.h"
#include "uistate.h"
#include "units.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "weather.h"

static const activity_id ACT_ATM( "ACT_ATM" );
static const activity_id ACT_CLEAR_RUBBLE( "ACT_CLEAR_RUBBLE" );
static const activity_id ACT_CRACKING( "ACT_CRACKING" );
static const activity_id ACT_FORAGE( "ACT_FORAGE" );
static const activity_id ACT_PLANT_SEED( "ACT_PLANT_SEED" );

static const efftype_id effect_antibiotic( "antibiotic" );
static const efftype_id effect_bite( "bite" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_disinfected( "disinfected" );
static const efftype_id effect_earphones( "earphones" );
static const efftype_id effect_infected( "infected" );
static const efftype_id effect_pblue( "pblue" );
static const efftype_id effect_pkill2( "pkill2" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_strong_antibiotic( "strong_antibiotic" );
static const efftype_id effect_teleglow( "teleglow" );
static const efftype_id effect_weak_antibiotic( "weak_antibiotic" );

static const itype_id itype_2x4( "2x4" );
static const itype_id itype_arm_splint( "arm_splint" );
static const itype_id itype_battery( "battery" );
static const itype_id itype_bot_broken_cyborg( "bot_broken_cyborg" );
static const itype_id itype_bot_prototype_cyborg( "bot_prototype_cyborg" );
static const itype_id itype_cash_card( "cash_card" );
static const itype_id itype_money_bundle( "money_bundle" );
static const itype_id itype_charcoal( "charcoal" );
static const itype_id itype_chem_carbide( "chem_carbide" );
static const itype_id itype_corpse( "corpse" );
static const itype_id itype_electrohack( "electrohack" );
static const itype_id itype_fake_milling_item( "fake_milling_item" );
static const itype_id itype_fake_cloning_vat( "fake_cloning_vat_item" );
static const itype_id itype_fake_smoke_plume( "fake_smoke_plume" );
static const itype_id itype_fertilizer( "fertilizer" );
static const itype_id itype_fire( "fire" );
static const itype_id itype_fungal_seeds( "fungal_seeds" );
static const itype_id itype_grapnel( "grapnel" );
static const itype_id itype_hickory_root( "hickory_root" );
static const itype_id itype_id_industrial( "id_industrial" );
static const itype_id itype_id_military( "id_military" );
static const itype_id itype_id_science( "id_science" );
static const itype_id itype_leg_splint( "leg_splint" );
static const itype_id itype_maple_sap( "maple_sap" );
static const itype_id itype_marloss_berry( "marloss_berry" );
static const itype_id itype_marloss_seed( "marloss_seed" );
static const itype_id itype_mycus_fruit( "mycus_fruit" );
static const itype_id itype_nail( "nail" );
static const itype_id itype_petrified_eye( "petrified_eye" );
static const itype_id itype_plut_generator_item( "plut_generator_item" );
static const itype_id itype_sheet( "sheet" );
static const itype_id itype_stick( "stick" );
static const itype_id itype_string_36( "string_36" );
static const itype_id itype_tree_spile( "tree_spile" );
static const itype_id itype_unfinished_cac2( "unfinished_cac2" );
static const itype_id itype_unfinished_charcoal( "unfinished_charcoal" );
static const itype_id itype_UPS( "UPS" );
static const itype_id itype_water( "water" );
static const itype_id itype_dna( "dna" );
static const itype_id itype_embryo( "embryo" );
static const itype_id itype_embryo_empty( "embryo_empty" );

static const trap_str_id tr_unfinished_construction( "tr_unfinished_construction" );

static const skill_id skill_computer( "computer" );
static const skill_id skill_cooking( "cooking" );
static const skill_id skill_electronics( "electronics" );
static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_firstaid( "firstaid" );
static const skill_id skill_mechanics( "mechanics" );
static const skill_id skill_survival( "survival" );
static const skill_id skill_trapping( "trapping" );

static const ter_str_id t_dimensional_portal( "t_dimensional_portal" );
static const ter_str_id t_web_bridge( "t_web_bridge" );

static const trait_id trait_AMORPHOUS( "AMORPHOUS" );
static const trait_id trait_ARACHNID_ARMS_OK( "ARACHNID_ARMS_OK" );
static const trait_id trait_BADKNEES( "BADKNEES" );
static const trait_id trait_BEAK_HUM( "BEAK_HUM" );
static const trait_id trait_BURROW( "BURROW" );
static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
static const trait_id trait_ILLITERATE( "ILLITERATE" );
static const trait_id trait_INSECT_ARMS_OK( "INSECT_ARMS_OK" );
static const trait_id trait_M_DEFENDER( "M_DEFENDER" );
static const trait_id trait_M_DEPENDENT( "M_DEPENDENT" );
static const trait_id trait_M_FERTILE( "M_FERTILE" );
static const trait_id trait_M_SPORES( "M_SPORES" );
static const trait_id trait_NOPAIN( "NOPAIN" );
static const trait_id trait_PROBOSCIS( "PROBOSCIS" );
static const trait_id trait_THRESH_MARLOSS( "THRESH_MARLOSS" );
static const trait_id trait_THRESH_MYCUS( "THRESH_MYCUS" );
static const trait_id trait_WEB_BRIDGE( "WEB_BRIDGE" );
static const trait_id trait_DEBUG_NOCLIP( "DEBUG_NOCLIP" );

static const quality_id qual_ANESTHESIA( "ANESTHESIA" );
static const quality_id qual_DIG( "DIG" );
static const quality_id qual_LOCKPICK( "LOCKPICK" );

static const mtype_id mon_broken_cyborg( "mon_broken_cyborg" );
static const mtype_id mon_dark_wyrm( "mon_dark_wyrm" );
static const mtype_id mon_fungal_blossom( "mon_fungal_blossom" );
static const mtype_id mon_prototype_cyborg( "mon_prototype_cyborg" );
static const mtype_id mon_spider_cellar_giant_s( "mon_spider_cellar_giant_s" );
static const mtype_id mon_spider_web_s( "mon_spider_web_s" );
static const mtype_id mon_spider_widow_giant_s( "mon_spider_widow_giant_s" );

static const bionic_id bio_fingerhack( "bio_fingerhack" );
static const bionic_id bio_lighter( "bio_lighter" );
static const bionic_id bio_lockpick( "bio_lockpick" );
static const bionic_id bio_painkiller( "bio_painkiller" );

static const std::string flag_AUTODOC( "AUTODOC" );
static const std::string flag_AUTODOC_COUCH( "AUTODOC_COUCH" );
static const std::string flag_BARRICADABLE_WINDOW_CURTAINS( "BARRICADABLE_WINDOW_CURTAINS" );
static const std::string flag_CLIMBABLE( "CLIMBABLE" );
static const std::string flag_CLIMB_SIMPLE( "CLIMB_SIMPLE" );
static const std::string flag_GROWTH_HARVEST( "GROWTH_HARVEST" );
static const std::string flag_OPENCLOSE_INSIDE( "OPENCLOSE_INSIDE" );
static const std::string flag_WALL( "WALL" );

// @TODO maybe make this a property of the item (depend on volume/type)
static const time_duration milling_time = 6_hours;

/**
 * Nothing player can interact with here.
 */
void iexamine::none( player &/*p*/, const tripoint &examp )
{
    add_msg( _( "That is a %s." ), get_map().name( examp ) );
}

/**
 * Pick an appropriate item and apply diamond coating if possible.
 */
void iexamine::cvdmachine( player &p, const tripoint & )
{
    // Select an item to which it is possible to apply a diamond coating
    auto loc = g->inv_map_splice( []( const item & e ) {
        return ( e.is_melee( DT_CUT ) || e.is_melee( DT_STAB ) ) && e.made_of( material_id( "steel" ) ) &&
               !e.has_flag( flag_DIAMOND ) && !e.has_flag( flag_NO_CVD );
    }, _( "Apply diamond coating" ), 1, _( "You don't have a suitable item to coat with diamond" ) );

    if( !loc ) {
        return;
    }

    // Require materials proportional to selected item volume
    auto qty = loc->volume() / units::legacy_volume_factor;
    qty = std::max( 1, qty );
    auto reqs = *requirement_id( "cvd_diamond" ) * qty;

    if( !reqs.can_make_with_inventory( p.crafting_inventory(), is_crafting_component ) ) {
        popup( "%s", reqs.list_missing() );
        return;
    }

    // Consume materials
    for( const auto &e : reqs.get_components() ) {
        p.consume_items( e, 1, is_crafting_component );
    }
    for( const auto &e : reqs.get_tools() ) {
        p.consume_tools( e );
    }
    p.invalidate_crafting_inventory();

    // Apply flag to item
    loc->set_flag( flag_DIAMOND );
    add_msg( m_good, _( "You apply a diamond coating to your %s" ), loc->type_name() );
    p.mod_moves( -to_turns<int>( 10_seconds ) );
}

/**
 * UI FOR LAB_FINALE NANO FABRICATOR.
 */
void iexamine::nanofab( player &p, const tripoint &examp )
{
    bool table_exists = false;
    tripoint spawn_point;
    map &here = get_map();
    for( const auto &valid_location : here.points_in_radius( examp, 1 ) ) {
        if( here.ter( valid_location ) == ter_str_id( "t_nanofab_body" ) ) {
            spawn_point = valid_location;
            table_exists = true;
            break;
        }
    }
    if( !table_exists ) {
        return;
    }

    auto nanofab_template = g->inv_map_splice( []( const item & e ) {
        return e.has_var( "NANOFAB_GROUP_ID" ) || e.has_var( "NANOFAB_ITEM_ID" );
    }, _( "Introduce nanofabricator template:" ), PICKUP_RANGE,
    _( "You don't have any usable templates." ) );

    if( !nanofab_template ) {
        return;
    }

    std::vector<std::string> recipe_ids;

    if( nanofab_template->has_var( "NANOFAB_GROUP_ID" ) ) {
        // Preferred behavior: build from group
        item_group_id group_id( nanofab_template->get_var( "NANOFAB_GROUP_ID" ) );
        std::set<const itype *> all_items = item_group::every_possible_item_from( group_id );
        for( const itype *it : all_items ) {
            recipe_ids.push_back( it->get_id().str() );
        }
    } else if( nanofab_template->has_var( "NANOFAB_ITEM_ID" ) ) {
        // Fallback for old templates: use single stored recipe
        recipe_ids.push_back( nanofab_template->get_var( "NANOFAB_ITEM_ID" ) );
    }

    if( recipe_ids.empty() ) {
        return;
    }

    std::string chosen_recipe;
    if( recipe_ids.size() > 1 ) {
        uilist menu;
        menu.text = _( "Choose a recipe:" );
        for( size_t i = 0; i < recipe_ids.size(); ++i ) {
            itype_id item = itype_id( recipe_ids[i] );
            auto button_text = string_format( "%s [%d]", item->nname( 1 ),
                                              std::max( 1, item->volume / 250_ml ) * 5 );
            menu.addentry( i, true, -1, button_text );
        }
        menu.query();

        if( menu.ret >= 0 && static_cast<size_t>( menu.ret ) < recipe_ids.size() ) {
            chosen_recipe = recipe_ids[ menu.ret ];
        }
    } else {
        chosen_recipe = recipe_ids.front();
    }

    if( chosen_recipe.empty() ) {
        return;
    }

    int item_count = 1;

    detached_ptr<item> new_item = item::spawn( itype_id( chosen_recipe ), calendar::turn );

    if( new_item->made_of( LIQUID ) ) {
        const int amount = string_input_popup()
                           .title( "Dispense how many units?" )
                           .width( 5 )
                           .text( std::to_string( 1 ) )
                           .only_digits( true )
                           .query_int();
        item_count = amount;

        new_item = item::spawn( itype_id( chosen_recipe ), calendar::turn, item_count );
    }

    auto qty = std::max( 1, new_item->volume() / 250_ml );
    auto reqs = *requirement_id( "nanofabricator" ) * qty;

    if( !reqs.can_make_with_inventory( p.crafting_inventory(), is_crafting_component ) ) {
        popup( "%s", reqs.list_missing() );
        return;
    }

    for( const auto &e : reqs.get_components() ) {
        p.consume_items( e, 1, is_crafting_component );
    }
    for( const auto &e : reqs.get_tools() ) {
        p.consume_tools( e );
    }
    p.invalidate_crafting_inventory();

    if( new_item->is_armor() && new_item->has_flag( flag_VARSIZE ) ) {
        new_item->set_flag( flag_FIT );
    }

    // we're sticking an item from our inventory under the nanofabrication dispenser
    if( new_item->made_of( LIQUID ) ) {
        liquid_handler::handle_all_liquid( std::move( new_item ), PICKUP_RANGE );  // let it own the pointer
        return;
    }

    here.add_item_or_charges( spawn_point, std::move( new_item ) );
}

/**
 * Use "gas pump."  Will pump any liquids on tile.
 */
void iexamine::gaspump( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( !query_yn( _( "Use the %s?" ), here.tername( examp ) ) ) {
        none( p, examp );
        return;
    }

    auto items = here.i_at( examp );
    for( auto item_it = items.begin(); item_it != items.end(); ++item_it ) {
        item *content = *item_it;
        if( content->made_of( LIQUID ) ) {
            item_it = location_vector<item>::iterator();
            ///\EFFECT_DEX decreases chance of spilling gas from a pump
            if( one_in( 10 + p.get_dex() ) ) {
                add_msg( m_bad, _( "You accidentally spill the %s." ), content->type_name() );
                static const auto max_spill_volume = units::from_liter( 1 );
                const int max_spill_charges = std::max( 1, content->charges_per_volume( max_spill_volume ) );
                ///\EFFECT_DEX decreases amount of gas spilled from a pump
                const int qty = rng( 1, max_spill_charges * 8.0 / std::max( 1, p.get_dex() ) );

                //This could invalidate the iterator, but it's not used again as we return right after
                detached_ptr<item> spill = content->split( qty );
                here.add_item_or_charges( p.pos(), std::move( spill ) );
                return;
            } else {
                liquid_handler::handle_liquid( *content, 1 );
            }
            return;
        }
    }
    add_msg( m_info, _( "Out of order." ) );
}

void iexamine::translocator( player &, const tripoint &examp )
{
    // TODO: fix point types
    const tripoint_abs_omt omt_loc( ms_to_omt_copy( get_map().getabs( examp ) ) );
    avatar &player_character = get_avatar();
    const bool activated = player_character.translocators->knows_translocator( omt_loc );
    if( !activated ) {
        g->u.translocators->activate_teleporter( omt_loc, examp );
        add_msg( m_info, _( "Translocator gate active." ) );
    } else {
        const int choice = uilist( _( "Do what with Translocator?" ), {
            _( "Visit another gate." ),
            _( "Deactivate." )
        } );
        if( choice == 0 ) {
            item *vtm = item::spawn_temporary( "translocation_caster", calendar::start_of_cataclysm );
            player_character.invoke_item( vtm );
        } else if( choice == 1 ) {
            if( query_yn( _( "Deactivate this Translocator?" ) ) ) {
                g->u.translocators->deactivate_teleporter( omt_loc, examp );
            }
        } else {
            add_msg( _( "Never mind." ) );
        }
    }
}

namespace
{
//--------------------------------------------------------------------------------------------------
//! Implements iexamine::atm(...)
//--------------------------------------------------------------------------------------------------
class atm_menu
{
    public:
        // menu choices
        enum options : int {
            cancel, purchase_card, deposit_money, withdraw_money, deposit_cash, withdraw_cash, transfer_all_money
        };

        atm_menu()                           = delete;
        atm_menu( atm_menu const & )            = delete;
        atm_menu( atm_menu && )                 = delete;
        atm_menu &operator=( atm_menu const & ) = delete;
        atm_menu &operator=( atm_menu && )      = delete;

        explicit atm_menu( player &p ) : u( p ) {
            reset( false );
        }

        void start() {
            for( bool result = false; !result; ) {
                switch( choose_option() ) {
                    case purchase_card:
                        result = do_purchase_card();
                        break;
                    case deposit_money:
                        result = do_deposit_money();
                        break;
                    case withdraw_money:
                        result = do_withdraw_money();
                        break;
                    case deposit_cash:
                        result = do_deposit_cash();
                        break;
                    case withdraw_cash:
                        result = do_withdraw_cash();
                        break;
                    case transfer_all_money:
                        result = do_transfer_all_money();
                        break;
                    default:
                        return;
                }
                if( !u.activity->is_null() ) {
                    break;
                }
            }
        }
    private:
        int value_of_money_bundle = item::spawn_temporary( itype_money_bundle,
                                    calendar::start_of_cataclysm )->price( false );

        void add_choice( const int i, const char *const title ) {
            amenu.addentry( i, true, -1, title );
        }
        void add_info( const int i, const char *const title ) {
            amenu.addentry( i, false, -1, title );
        }

        options choose_option() {
            if( u.activity->id() == ACT_ATM ) {
                return static_cast<options>( u.activity->index );
            }
            amenu.query();
            uistate.iexamine_atm_selected = amenu.selected;
            return amenu.ret < 0 ? cancel : static_cast<options>( amenu.ret );
        }

        //! Reset and repopulate the menu; with a fair bit of work this could be more efficient.
        void reset( const bool clear = true ) {
            const int cash_amount   = u.amount_of( itype_money_bundle );
            const int card_count   = u.amount_of( itype_cash_card );
            const int charge_count = card_count ? u.charges_of( itype_cash_card ) : 0;

            if( clear ) {
                amenu.reset();
            }

            amenu.selected = uistate.iexamine_atm_selected;
            amenu.text = string_format( _( "Welcome to the C.C.B.o.t.T. ATM.  What would you like to do?\n"
                                           "Your current balance is: %s" ),
                                        format_money( u.cash ) );

            if( u.cash >= 1000 ) {
                add_choice( purchase_card, _( "Purchase cash card" ) );
            } else {
                add_info( purchase_card, _( "You need $10.00 in your account to purchase a card." ) );
            }

            if( u.cash > value_of_money_bundle ) {
                add_choice( withdraw_cash, _( "Withdraw cash" ) );
            } else if( u.cash < 0 ) {
                add_info( withdraw_cash,
                          _( "You need to pay down your debt before withdrawing cash!" ) );
            } else {
                add_info( withdraw_cash,
                          _( "You don't have enough to withdraw a money bundle!" ) );
            }

            if( cash_amount > 0 ) {
                add_choice( deposit_cash, _( "Deposit cash" ) );
            } else {
                add_info( deposit_cash,
                          _( "You need cash to deposit!" ) );
            }

            if( card_count && u.cash > 0 ) {
                add_choice( withdraw_money, _( "Withdraw onto cash card" ) );
            } else if( u.cash > 0 ) {
                add_info( withdraw_money, _( "You need a cash card before you can withdraw money!" ) );
            } else if( u.cash < 0 ) {
                add_info( withdraw_money,
                          _( "You need to pay down your debt before withdrawing money onto a card!" ) );
            } else {
                add_info( withdraw_money,
                          _( "You need money in your account before you can withdraw money!" ) );
            }

            if( charge_count ) {
                add_choice( deposit_money, _( "Deposit from cash card" ) );
            } else {
                add_info( deposit_money,
                          _( "You need a charged cash card before you can deposit money!" ) );
            }

            if( card_count >= 2 && charge_count ) {
                add_choice( transfer_all_money, _( "Combine cash cards" ) );
            }
        }

        //! print a bank statement for @p print = true;
        void finish_interaction( const bool print = true ) {
            if( print ) {
                if( u.cash < 0 ) {
                    add_msg( m_info, _( "Your debt is now %s." ), format_money( u.cash ) );
                } else {
                    add_msg( m_info, _( "Your account now holds %s." ), format_money( u.cash ) );
                }
            }

            u.moves -= to_turns<int>( 5_seconds );
        }

        //! Prompt for an integral value clamped to [0, max].
        static int prompt_for_amount( const char *const msg, const int max ) {
            const std::string formatted = string_format( msg, max );
            const int amount = string_input_popup()
                               .title( formatted )
                               .width( 20 )
                               .text( std::to_string( max ) )
                               .only_digits( true )
                               .query_int();

            return clamp( amount, 0, max );
        }

        //!Get a new cash card. $10.00 fine.
        bool do_purchase_card() {
            const char *prompt =
                _( "This will automatically deduct $10.00 from your bank account.  Continue?" );

            if( !query_yn( prompt ) ) {
                return false;
            }

            detached_ptr<item> card = item::spawn( "cash_card", calendar::turn );
            card->charges = 0;
            u.i_add( std::move( card ) );
            u.cash -= 1000;
            u.moves -= to_turns<int>( 5_seconds );
            finish_interaction();

            return true;
        }

        //!Deposit money from cash card into bank account.
        bool do_deposit_cash() {
            int money = u.charges_of( itype_money_bundle );

            if( !money ) {
                popup( _( "You can only deposit money from charged cash cards!" ) );
                return false;
            }

            const int amount = prompt_for_amount( vgettext(
                    "Deposit how many bundles?  Max: %d bundles.  (0 to cancel) ",
                    "Deposit how many bundles?  Max: %d bundles.  (0 to cancel) ", money ),
                                                  money );

            if( !amount ) {
                return false;
            }

            u.use_charges( itype_money_bundle, amount );
            u.cash += amount * value_of_money_bundle;
            u.moves -= to_turns<int>( 10_seconds );
            finish_interaction();

            return true;
        }

        //!Deposit money from cash card into bank account.
        bool do_deposit_money() {
            int money = u.charges_of( itype_cash_card );

            if( !money ) {
                popup( _( "You can only deposit money from charged cash cards!" ) );
                return false;
            }

            const int amount = prompt_for_amount( vgettext(
                    "Deposit how much?  Max: %d cent.  (0 to cancel) ",
                    "Deposit how much?  Max: %d cents.  (0 to cancel) ", money ), money );

            if( !amount ) {
                return false;
            }

            add_msg( m_info, "amount: %d", amount );
            u.use_charges( itype_cash_card, amount );
            u.cash += amount;
            u.moves -= to_turns<int>( 10_seconds );
            finish_interaction();

            return true;
        }

        //!Move money from bank account onto cash card.
        bool do_withdraw_money() {
            //We may want to use visit_items here but that's fairly heavy.
            //For now, just check weapon if we didn't find it in the inventory.
            int pos = u.inv_position_by_type( itype_cash_card );
            item *dst;
            if( pos == INT_MIN ) {
                dst = &u.primary_weapon();
            } else {
                dst = &u.i_at( pos );
            }

            if( dst == nullptr || dst->is_null() ) {
                //Just in case we run into an edge case
                popup( _( "You do not have a cash card to withdraw money!" ) );
                return false;
            }

            const int amount = prompt_for_amount( vgettext(
                    "Withdraw how much?  Max: %d cent.  (0 to cancel) ",
                    "Withdraw how much?  Max: %d cents.  (0 to cancel) ", u.cash ), u.cash );

            if( !amount ) {
                return false;
            }

            dst->charges += amount;
            u.cash -= amount;
            u.moves -= to_turns<int>( 10_seconds );
            finish_interaction();

            return true;
        }

        //!Move money from bank account onto cash card.
        bool do_withdraw_cash() {
            const int amount = prompt_for_amount( vgettext(
                    "Withdraw how much?  Max: %d bundles.  (0 to cancel) ",
                    "Withdraw how much?  Max: %d bundles.  (0 to cancel) ", u.cash / value_of_money_bundle ),
                                                  u.cash / value_of_money_bundle );

            if( !amount ) {
                return false;
            }

            for( int i = 0; i < amount; i++ ) {
                detached_ptr<item> card = item::spawn( "money_bundle", calendar::turn );

                u.i_add( std::move( card ) );
                u.cash -= value_of_money_bundle;
            }

            u.moves -= to_turns<int>( 5_seconds );

            finish_interaction();

            return true;
        }

        //!Move the money from all the cash cards in inventory to a single card.
        bool do_transfer_all_money() {
            item *dst;
            if( u.activity->id() == ACT_ATM ) {
                u.activity->set_to_null(); // stop for now, if required, it will be created again.
                //TODO!: safety check?
                dst = &*u.activity->targets.front();
                if( dst == nullptr || dst->is_null() || dst->typeId() != itype_cash_card ) {
                    return false;
                }
            } else {
                const int pos = u.inv_position_by_type( itype_cash_card );

                if( pos == INT_MIN ) {
                    return false;
                }
                dst = &u.i_at( pos );
            }

            for( auto &i : u.inv_dump() ) {
                if( i == dst || i->charges <= 0 || i->typeId() != itype_cash_card ) {
                    continue;
                }
                if( u.moves < 0 ) {
                    // Money from `*i` could be transferred, but we're out of moves, schedule it for
                    // the next turn. Putting this here makes sure there will be something to be
                    // done next turn.
                    u.assign_activity( ACT_ATM, 0, transfer_all_money );
                    u.activity->targets.emplace_back( dst );
                    break;
                }

                dst->charges += i->charges;
                i->charges = 0;
                u.moves -= 10;
            }

            return true;
        }

        player &u;
        uilist amenu;
};
} //namespace

/**
 * launches the atm menu class which then handles all the atm interactions.
 */
void iexamine::atm( player &p, const tripoint & )
{
    atm_menu {p} .start();
}

/**
 * Generates vending machine UI and allows players to purchase contained items with a cash card.
 */
void iexamine::vending( player &p, const tripoint &examp )
{
    constexpr int moves_cost = to_turns<int>( 5_seconds );
    int money = p.charges_of( itype_cash_card );
    auto vend_items = get_map().i_at( examp );

    if( vend_items.empty() ) {
        add_msg( m_info, _( "The vending machine is empty." ) );
        return;
    }

    if( !money ) {
        popup( _( "You need some money on a cash card to buy things." ) );
    }

    int w_items_w = 0;
    int w_info_w = 0;
    int list_lines = 0;
    int lines_above = 0;
    int lines_below = 0;
    constexpr int first_item_offset = 3; // header size

    catacurses::window w;
    catacurses::window w_item_info;

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const point padding( std::max( 0, TERMX - FULL_SCREEN_WIDTH ) / 4, std::max( 0,
                             TERMY - FULL_SCREEN_HEIGHT ) / 6 );
        const int window_h   = FULL_SCREEN_HEIGHT + std::max( 0, TERMY - FULL_SCREEN_HEIGHT ) * 2 / 3;
        const int window_w   = FULL_SCREEN_WIDTH + std::max( 0, TERMX - FULL_SCREEN_WIDTH ) / 2;
        w_items_w  = window_w / 2;
        w_info_w   = window_w - w_items_w;
        list_lines = window_h - 4; // minus for header and footer

        lines_above = list_lines / 2;                  // lines above the selector
        lines_below = list_lines / 2 + list_lines % 2; // lines below the selector

        w = catacurses::newwin( window_h, w_items_w,
                                padding );
        w_item_info = catacurses::newwin( window_h, w_info_w,
                                          padding + point( w_items_w, 0 ) );

        ui.position( padding, point( window_w, window_h ) );
    } );
    ui.mark_resize();

    bool used_machine = false;
    input_context ctxt( "VENDING_MACHINE" );
    ctxt.register_updown();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    std::vector<std::vector<item *>> item_map;
    for( item *&it : vend_items ) {
        // |# {name}|
        // 123      4
        std::string name = it->tname();
        bool found = false;
        for( auto item_list : item_map ) {
            if( item_list.front()->tname() == name ) {
                item_list.push_back( it );
                found = true;
                break;
            }
        }
        if( !found ) {
            item_map.push_back( {it} );
        }
    }

    int cur_pos = 0;
    ui.on_redraw( [&]( const ui_adaptor & ) {
        const int num_items = item_map.size();
        const int page_size = std::min( num_items, list_lines );

        werase( w );
        wborder( w, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                 LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
        mvwhline( w, point( 1, first_item_offset - 1 ), LINE_OXOX, w_items_w - 2 );
        mvwaddch( w, point( 0, first_item_offset - 1 ), LINE_XXXO ); // |-
        mvwaddch( w, point( w_items_w - 1, first_item_offset - 1 ), LINE_XOXX ); // -|

        trim_and_print( w, point( 2, 1 ), w_items_w - 3, c_light_gray,
                        _( "Money left: %s" ), format_money( money ) );

        // Keep the item selector centered in the page.
        int page_beg;
        if( cur_pos < num_items - cur_pos ) {
            page_beg = std::max( 0, cur_pos - lines_above );
        } else {
            int page_end = std::min( num_items, cur_pos + lines_below );
            page_beg = std::max( 0, page_end - list_lines );
        }

        for( int line = 0; line < page_size; ++line ) {
            const int i = page_beg + line;
            const auto color = ( i == cur_pos ) ? h_light_gray : c_light_gray;
            const auto &elem = item_map[i];
            const int count = elem.size();
            const char c = ( count < 10 ) ? ( '0' + count ) : '*';
            trim_and_print( w, point( 1, first_item_offset + line ), w_items_w - 3, color, "%c %s", c,
                            elem.front()->tname().c_str() );
        }

        draw_scrollbar( w, cur_pos, list_lines, num_items, point( 0, first_item_offset ) );
        wnoutrefresh( w );

        // Item info
        auto &cur_items = item_map[static_cast<size_t>( cur_pos )];
        auto &cur_item  = cur_items.back();

        werase( w_item_info );
        // | {line}|
        // 12      3
        fold_and_print( w_item_info, point( 2, 1 ), w_info_w - 3, c_light_gray,
                        cur_item->info_string( ) );
        wborder( w_item_info, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                 LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );

        //+<{name}>+
        //12      34
        const std::string name = utf8_truncate( cur_item->display_name(),
                                                static_cast<size_t>( w_info_w - 4 ) );

        const auto cost = format_money( cur_item->price( false ) );
        mvwprintw( w_item_info, point_east, "<%s> %s", name, cost );
        wnoutrefresh( w_item_info );
    } );

    for( ;; ) {
        ui_manager::redraw();

        const int num_items = item_map.size();

        // Item info
        auto &cur_items = item_map[static_cast<size_t>( cur_pos )];
        auto &cur_item  = cur_items.back();

        const std::string &action = ctxt.handle_input();
        if( action == "DOWN" ) {
            cur_pos = ( cur_pos + 1 ) % num_items;
        } else if( action == "UP" ) {
            cur_pos = ( cur_pos + num_items - 1 ) % num_items;
        } else if( action == "CONFIRM" ) {
            const int iprice = cur_item->price( false );

            if( iprice > money ) {
                popup( _( "You can't afford that item." ) );
                continue;
            }

            if( !used_machine ) {
                used_machine = true;
                p.moves -= moves_cost;
            }

            money -= iprice;
            p.use_charges( itype_cash_card, iprice );
            p.i_add_or_drop( vend_items.remove( cur_item ) );

            cur_items.pop_back();
            if( !cur_items.empty() ) {
                continue;
            }

            item_map.erase( std::begin( item_map ) + cur_pos );
            if( item_map.empty() ) {
                add_msg( _( "With a beep, the empty vending machine shuts down." ) );
                return;
            } else if( cur_pos == num_items - 1 ) {
                cur_pos--;
            }
        } else if( action == "QUIT" ) {
            break;
        }
    }
}

/**
 * If there's water, allow its usage but add chance of poison.
 */
void iexamine::toilet( player &p, const tripoint &examp )
{
    auto items = get_map().i_at( examp );
    auto water = items.begin();
    for( ; water != items.end(); ++water ) {
        if( ( *water )->typeId() == itype_water ) {
            break;
        }
    }

    if( water == items.end() ) {
        add_msg( m_info, _( "This toilet is empty." ) );
    } else if( !( *water )->made_of( LIQUID ) ) {
        add_msg( m_info, _( "The toilet water is frozen solid!" ) );
    } else {
        // Use a different poison value each time water is drawn from the toilet.
        ( *water )->poison = one_in( 3 ) ? 0 : rng( 1, 3 );

        // TODO: use me
        ( void ) p;
        liquid_handler::handle_liquid( **water );
    }
}

/** Toggle the lights in a overmap terrain*/
void iexamine::toggle_lights( player &/*p*/, const tripoint &examp )
{
    map &here = get_map();
    const auto flag = here.has_flag_furn( "L_OFF", examp ) ? "L_OFF" : "L_ON";

    add_msg( _( here.furn( examp ).obj().message ) );

    for( const auto &light_loc : here.find_furnitures_with_flag_in_omt( examp, flag ) ) {
        here.furn_set( light_loc, here.get_furn_transforms_into( light_loc ) );
    };
}

/**
 * Open or close gate.
 */
void iexamine::controls_gate( player &p, const tripoint &examp )
{
    if( !query_yn( _( "Use the %s?" ), get_map().tername( examp ) ) ) {
        none( p, examp );
        return;
    }
    g->toggle_gate( examp );
}

static bool try_start_hacking( player &p, const tripoint &examp )
{
    if( p.has_trait( trait_ILLITERATE ) ) {
        add_msg( _( "You cannot read!" ) );
        return false;
    }
    const bool has_item = p.has_charges( itype_electrohack, 25 );
    const bool has_bionic = p.has_bionic( bio_fingerhack ) && p.get_power_level() >= 25_kJ;
    if( !has_item && !has_bionic ) {
        add_msg( _( "You don't have a hacking tool with enough charges!" ) );
        return false;
    }
    bool use_bionic = has_bionic;
    if( has_item && has_bionic ) {
        uilist menu;
        menu.settext( _( "Use which hacking tool?" ) );
        menu.addentry( 0, true, MENU_AUTOASSIGN, "%s", itype_electrohack->nname( 1 ) );
        menu.addentry( 1, true, MENU_AUTOASSIGN, "%s", bio_fingerhack->name );
        menu.query();
        switch( menu.ret ) {
            case 0:
                use_bionic = false;
                break;
            case 1:
                use_bionic = true;
                break;
            default:
                return false;
        }
    }
    if( use_bionic ) {
        p.mod_power_level( -25_kJ );
        p.assign_activity( std::make_unique<player_activity>( std::make_unique<hacking_activity_actor>(
                               hacking_activity_actor::use_bionic {} ) ) );
    } else {
        p.use_charges( itype_electrohack, 25 );
        p.assign_activity( std::make_unique<player_activity>
                           ( std::make_unique<hacking_activity_actor>() ) );
    }
    p.activity->placement = examp;
    return true;
}

/**
 * Use id/hack reader. Using an id despawns turrets.
 */
void iexamine::cardreader( player &p, const tripoint &examp )
{
    bool open = false;
    map &here = get_map();
    itype_id card_type = ( here.ter( examp ) == t_card_science ? itype_id_science :
                           here.ter( examp ) == t_card_military ? itype_id_military :
                           itype_id_industrial );
    if( p.has_amount( card_type, 1 ) && query_yn( _( "Swipe your ID card?" ) ) ) {
        p.mod_moves( -to_turns<int>( 1_seconds ) );
        for( const tripoint &tmp : here.points_in_radius( examp, 3 ) ) {
            if( here.ter( tmp ) == t_door_metal_locked ) {
                here.ter_set( tmp, t_door_metal_c );
                open = true;
            }
        }
        for( monster &critter : g->all_monsters() ) {
            // Check 1) same overmap coords, 2) turret, 3) hostile
            if( ms_to_omt_copy( here.getabs( critter.pos() ) ) == ms_to_omt_copy( here.getabs( examp ) ) &&
                critter.has_flag( MF_ID_CARD_DESPAWN ) &&
                critter.attitude_to( p ) == Attitude::A_HOSTILE ) {
                g->remove_zombie( critter );
            }
        }
        if( open ) {
            add_msg( _( "You insert your ID card." ) );
            add_msg( m_good, _( "The nearby doors unlock." ) );
            p.use_amount( card_type, 1 );
        } else {
            add_msg( _( "The nearby doors are already opened." ) );
        }
    } else if( query_yn( _( "Attempt to hack this card-reader?" ) ) ) {
        try_start_hacking( p, examp );
    }
}

void iexamine::cardreader_robofac( player &p, const tripoint &examp )
{
    itype_id card_type = itype_id_science;
    if( p.has_amount( card_type, 1 ) && query_yn( _( "Swipe your ID card?" ) ) ) {
        p.mod_moves( -100 );
        p.use_amount( card_type, 1 );
        add_msg( m_bad, _( "The card reader short circuits!" ) );
        get_map().ter_set( examp, t_card_reader_broken );
        intercom( p, examp );
    } else {
        add_msg( _( "You have never seen this card reader model before.  Hacking it seems impossible." ) );
    }
}

void iexamine::cardreader_foodplace( player &p, const tripoint &examp )
{
    bool open = false;
    if( ( p.is_wearing( itype_id( "foodperson_mask" ) ) ||
          p.is_wearing( itype_id( "foodperson_mask_on" ) ) ) &&
        query_yn( _( "Press mask on the reader?" ) ) ) {
        p.mod_moves( -100 );
        map &here = get_map();
        for( const tripoint &tmp : here.points_in_radius( examp, 3 ) ) {
            if( here.ter( tmp ) == t_door_metal_locked ) {
                here.ter_set( tmp, t_door_metal_c );
                open = true;
            }
        }
        if( open ) {
            add_msg( _( "You press your face on the reader." ) );
            add_msg( m_good, _( "The nearby doors are unlocked." ) );
            sounds::sound( examp, 6, sounds::sound_t::electronic_speech,
                           _( "\"Hello Foodperson.  Welcome home.\"" ), true, "speech", "welcome" );
        } else {
            add_msg( _( "The nearby doors are already unlocked." ) );
            if( query_yn( _( "Lock doors?" ) ) ) {
                for( const tripoint &tmp : here.points_in_radius( examp, 3 ) ) {
                    if( here.ter( tmp ) == t_door_metal_o || here.ter( tmp ) == t_door_metal_c ) {
                        if( p.pos() == tmp ) {
                            p.add_msg_if_player( m_bad, _( "You are in the way of the door, move before trying again." ) );
                        } else {
                            here.ter_set( tmp, t_door_metal_locked );
                        }
                    }
                }
            }
        }
    } else if( p.has_amount( itype_id( "foodperson_mask" ), 1 ) ||
               p.has_amount( itype_id( "foodperson_mask_on" ), 1 ) ) {
        sounds::sound( examp, 6, sounds::sound_t::electronic_speech,
                       _( "\"FOODPERSON DETECTED.  Please make yourself presentable.\"" ), true,
                       "speech", "welcome" );
    } else {
        sounds::sound( examp, 6, sounds::sound_t::electronic_speech,
                       _( "\"Your face is inadequate.  Please go away.\"" ), true,
                       "speech", "welcome" );
        if( query_yn( _( "Attempt to hack this card-reader?" ) ) ) {
            try_start_hacking( p, examp );
        }
    }
}

void iexamine::intercom( player &p, const tripoint &examp )
{
    const std::vector<npc *> intercom_npcs = g->get_npcs_if( [examp]( const npc & guy ) {
        return guy.myclass == npc_class_id( "NC_ROBOFAC_INTERCOM" ) && rl_dist( guy.pos(), examp ) < 10;
    } );
    if( intercom_npcs.empty() ) {
        p.add_msg_if_player( m_info, _( "No one responds." ) );
    } else {
        intercom_npcs.front()->talk_to_u( false );
    }
}

/**
 * Prompt removal of rubble. Select best shovel and invoke "CLEAR_RUBBLE" on tile.
 */
void iexamine::rubble( player &p, const tripoint &examp )
{
    int moves;
    if( p.has_quality( qual_DIG, 3 ) || p.has_trait( trait_BURROW ) ) {
        moves = to_moves<int>( 1_minutes );
    } else if( p.has_quality( qual_DIG, 2 ) ) {
        moves = to_moves<int>( 2_minutes );
    } else {
        add_msg( m_info, _( "If only you had a shovel…" ) );
        return;
    }
    map &here = get_map();
    if( ( here.veh_at( examp ) || !here.tr_at( examp ).is_null() ||
          g->critter_at( examp ) != nullptr ) &&
        !query_yn( _( "Clear up that %s?" ), here.furnname( examp ) ) ) {
        return;
    }
    p.assign_activity( ACT_CLEAR_RUBBLE, moves, -1, 0 );
    p.activity->placement = examp;
    return;
}

/**
 * Prompt climbing over fence. Calculates move cost, applies it to player and, moves them.
 */
void iexamine::chainfence( player &p, const tripoint &examp )
{
    // We're not going to do anything if we're already on that point.
    // Also prompt the player before taking an action.
    if( p.pos() == examp || !query_yn( _( "Climb obstacle?" ) ) ) {
        none( p, examp );
        return;
    }

    map &here = get_map();
    if( here.has_flag( flag_CLIMB_SIMPLE, examp ) &&
        p.mutation_value( "movecost_obstacle_modifier" ) <= 0.5f ) {
        add_msg( _( "You vault over the obstacle with ease." ) );
        p.moves -= 100; // Not tall enough to warrant spider-climbing, so only relevant trait.
    } else if( here.has_flag( flag_CLIMB_SIMPLE, examp ) ) {
        add_msg( _( "You vault over the obstacle." ) );
        p.moves -= 300; // Most common move cost for barricades pre-change.
    } else if( p.has_trait( trait_ARACHNID_ARMS_OK ) &&
               !p.wearing_something_on( bodypart_id( "torso" ) ) ) {
        add_msg( _( "Climbing this obstacle is trivial for one such as you." ) );
        p.moves -= 75; // Yes, faster than walking.  6-8 limbs are impressive.
    } else if( p.has_trait( trait_INSECT_ARMS_OK ) &&
               !p.wearing_something_on( bodypart_id( "torso" ) ) ) {
        add_msg( _( "You quickly scale the fence." ) );
        p.moves -= 90;
    } else if( p.mutation_value( "movecost_obstacle_modifier" ) <= 0.5f ) {
        add_msg( _( "This obstacle is no match for your freerunning abilities." ) );
        p.moves -= 100;
    } else {
        p.moves -= 400;
        ///\EFFECT_DEX decreases chances of slipping while climbing
        int climb = p.dex_cur;
        if( p.has_trait( trait_BADKNEES ) ) {
            climb = climb / 2;
        }
        if( g->slip_down() ) {
            return;
        }
        p.moves += climb * 10;
        sfx::play_variant_sound( "plmove", "clear_obstacle", sfx::get_heard_volume( g->u.pos() ) );
    }
    if( p.in_vehicle ) {
        here.unboard_vehicle( p.pos() );
    }
    p.setpos( examp );
    if( examp.x < HALF_MAPSIZE_X || examp.y < HALF_MAPSIZE_Y ||
        examp.x >= HALF_MAPSIZE_X + SEEX || examp.y >= HALF_MAPSIZE_Y + SEEY ) {
        if( p.is_player() ) {
            g->update_map( p );
        }
    }
}

/**
 * If player has amorphous trait, slip through the bars.
 */
void iexamine::bars( player &p, const tripoint &examp )
{
    if( !( p.has_trait( trait_AMORPHOUS ) ) ) {
        none( p, examp );
        return;
    }
    map &here = get_map();
    if( ( ( p.encumb( body_part_torso ) ) >= 10 ) && ( ( p.encumb( body_part_head ) ) >= 10 ) &&
        ( p.encumb( body_part_foot_l ) >= 10 ||
          p.encumb( body_part_foot_r ) >=
          10 ) ) { // Most likely places for rigid gear that would catch on the bars.
        add_msg( m_info,
                 _( "Your amorphous body could slip though the %s, but your cumbersome gear can't." ),
                 here.tername( examp ) );
        return;
    }
    if( !query_yn( _( "Slip through the %s?" ), here.tername( examp ) ) ) {
        none( p, examp );
        return;
    }
    p.moves -= to_turns<int>( 2_seconds );
    add_msg( _( "You slide right between the bars." ) );
    p.setpos( examp );
}

void iexamine::deployed_furniture( player &p, const tripoint &pos )
{
    map &here = get_map();
    if( !query_yn( _( "Take down the %s?" ), here.furn( pos ).obj().name() ) ) {
        return;
    }
    p.add_msg_if_player( m_info, _( "You take down the %s." ),
                         here.furn( pos ).obj().name() );
    const auto furn_item = here.furn( pos ).obj().deployed_item;
    here.add_item_or_charges( pos, item::spawn( furn_item, calendar::turn ) );
    here.furn_set( pos, f_null );
}

static std::pair<itype_id, const deploy_tent_actor *> find_tent_itype( const furn_str_id &id )
{
    const itype_id &iid = id->deployed_item;
    if( iid.is_valid() ) {
        const itype &type = *iid;
        for( const auto &pair : type.use_methods ) {
            const auto actor = dynamic_cast<const deploy_tent_actor *>( pair.second.get_actor_ptr() );
            if( !actor ) {
                continue;
            }
            if( ( actor->floor_center && *actor->floor_center == id ) || ( !actor->floor_center &&
                    actor->floor == id ) ) {
                return std::make_pair( iid, actor );
            }
        }
    }
    return std::make_pair( iid, nullptr );
}

/**
 * Determine structure's type and prompts its removal.
 */
void iexamine::portable_structure( player &p, const tripoint &examp )
{
    map &here = get_map();
    const furn_str_id fid = here.furn( examp ).id();
    const std::pair<itype_id, const deploy_tent_actor *> tent_item_type = find_tent_itype( fid );
    if( tent_item_type.first.is_null() ) {
        debugmsg( "unknown furniture %s: don't know how to transform it into an item", fid.str() );
        return;
    }

    itype_id dropped = tent_item_type.first;
    std::string name = item::nname( dropped );
    int radius;

    if( tent_item_type.second ) {
        const deploy_tent_actor &actor = *tent_item_type.second;
        if( !actor.check_intact( examp ) ) {
            if( !actor.broken_type ) {
                add_msg( _( "The %s is broken and can not be picked up." ), name );
                none( p, examp );
                return;
            }
            dropped = *actor.broken_type;
            name = string_format( _( "damaged %s" ), name );
        }
        radius = actor.radius;

    } else {
        radius = std::max( 1, fid->bash.collapse_radius );
    }

    if( !query_yn( _( "Take down the %s?" ), name ) ) {
        none( p, examp );
        return;
    }

    p.moves -= to_turns<int>( 2_seconds );
    for( const tripoint &pt : here.points_in_radius( examp, radius ) ) {
        here.furn_set( pt, f_null );
    }

    here.add_item_or_charges( examp, item::spawn( dropped, calendar::turn ) );
}

/**
 * If there is a 2x4 around, prompt placing it across pit.
 */
void iexamine::pit( player &p, const tripoint &examp )
{
    const inventory &crafting_inv = p.crafting_inventory();
    if( !crafting_inv.has_amount( itype_2x4, 1 ) ) {
        none( p, examp );
        return;
    }
    std::vector<item_comp> planks;
    planks.emplace_back( itype_2x4, 1 );

    map &here = get_map();
    if( query_yn( _( "Place a plank over the pit?" ) ) ) {
        p.consume_items( planks, 1, is_crafting_component );
        if( here.ter( examp ) == t_pit ) {
            here.ter_set( examp, t_pit_covered );
        } else if( here.ter( examp ) == t_pit_spiked ) {
            here.ter_set( examp, t_pit_spiked_covered );
        } else if( here.ter( examp ) == t_pit_glass ) {
            here.ter_set( examp, t_pit_glass_covered );
        }
        add_msg( _( "You place a plank of wood over the pit." ) );
        p.mod_moves( -to_turns<int>( 1_seconds ) );
    }
}

/**
 * Prompt removing the 2x4 placed across the pit
 */
void iexamine::pit_covered( player &p, const tripoint &examp )
{
    if( !query_yn( _( "Remove cover?" ) ) ) {
        none( p, examp );
        return;
    }

    map &here = get_map();
    add_msg( _( "You remove the plank." ) );
    here.add_item_or_charges( p.pos(), item::spawn( "2x4", calendar::turn ) );

    if( here.ter( examp ) == t_pit_covered ) {
        here.ter_set( examp, t_pit );
    } else if( here.ter( examp ) == t_pit_spiked_covered ) {
        here.ter_set( examp, t_pit_spiked );
    } else if( here.ter( examp ) == t_pit_glass_covered ) {
        here.ter_set( examp, t_pit_glass );
    }
    p.mod_moves( -to_turns<int>( 1_seconds ) );
}

/**
 * Loop prompt to bet $10.
 */
void iexamine::slot_machine( player &p, const tripoint & )
{
    const int price = 10;
    auto cents = []( int x ) {
        return x * 100;
    };
    bool played = false;
    while( true ) {
        if( p.cash < cents( price ) ) {
            add_msg( m_info, _( "You need $%d to play." ), price );
            break;
        }
        if( !query_yn( played ? _( "Play again for $%d?" ) : _( "Insert $%d?" ), price ) ) {
            break;
        }
        p.cash -= cents( price );
        played = true;
        int won;
        if( one_in( 5 ) ) {
            won = price;
            popup( _( "Three cherries… you get your money back!" ) );
        } else if( one_in( 20 ) ) {
            won = 50;
            popup( _( "Three bells… you win $%d!" ), won );
        } else if( one_in( 50 ) ) {
            won = 200;
            popup( _( "Three stars… you win $%d!" ), won );
        } else if( one_in( 1000 ) ) {
            won = 3000;
            popup( _( "JACKPOT!  You win $%d!" ), won );
        } else {
            won = 0;
            popup( _( "No win." ) );
        }
        p.cash += cents( won );
    }
}

static item *find_best_prying_tool( player &p )
{
    std::vector<item *> prying_items = p.items_with( []( const item & it ) {
        // we want to get worn items (eg crowbar in toolbelt), so no check on item position
        return it.has_quality( quality_id( "PRY" ), 1 );
    } );

    // Sort by their quality level.
    std::ranges::sort( prying_items, []( const item * a, const item * b ) -> bool {
        return a->get_quality( quality_id( "PRY" ) ) > b->get_quality( quality_id( "PRY" ) );
    } );

    // if crowbar() ever eats charges or otherwise alters the passed item, rewrite this to reflect
    // changes to the original item.
    if( prying_items.empty() ) {
        return nullptr;
    }
    return prying_items[0];
}

static void apply_prying_tool( player &p, item *it, const tripoint &examp )
{
    map &here = get_map();
    //~ %1$s: terrain/furniture name, %2$s: prying tool name
    p.add_msg_if_player( _( "You attempt to pry open the %1$s using your %2$s…" ),
                         here.has_furn( examp ) ? here.furnname( examp ) : here.tername( examp ), it->tname() );

    // if crowbar() ever eats charges or otherwise alters the passed item, rewrite this to reflect
    // changes to the original item.
    iuse::crowbar( &p, it, false, examp );
}

static time_duration safecracking_time( const player &p )
{
    time_duration time = 120_minutes;
    time -= 10_minutes * p.get_skill_level( skill_mechanics );
    if( p.get_per() > 10 ) {
        time -= 5_minutes * ( p.get_per() - 10 );
    }
    // Count Safecracking tools (stethoscopes) as 2 perception
    if( p.has_item_with_flag( flag_SAFECRACK ) ) {
        time -= 10_minutes;
    }
    // Should take longer with poor hearing, hence the negative possibility
    time -= ( 5_minutes * ( p.hearing_ability() - 1 ) );
    return std::max( time, 5_minutes );
}

/**
 * Attempt to crack safe through audio-feedback manual lock manipulation.
 *
 * Try to unlock the safe by moving the dial and listening for the mechanism to "click into place."
 *
 * Time per attempt affected by perception and mechanics. 5 minutes per attempt minimum.
 * Small chance of just guessing the combo without listening device.
 */
void iexamine::safe( player &p, const tripoint &examp )
{

    map &here = get_map();
    safe_reference<item> prying_tool = find_best_prying_tool( p );
    const int target_diff = here.has_furn( examp ) ? here.furn( examp )->pry.pry_quality : here.ter(
                                examp )->pry.pry_quality;
    if( target_diff > 0 && prying_tool && !p.movement_mode_is( CMM_CROUCH ) ) {
        // keep going in case we have a prying tool that can't be used against the target, so we can try lockpicking
        if( prying_tool->get_quality( quality_id( "PRY" ) ) >= target_diff ) {
            apply_prying_tool( p, prying_tool.get(), examp );
            return;
        }
    }

    // Requires mutant hearing, Enhanced Hearing CBM, or a stethoscope.
    bool can_safecrack = p.hearing_ability() > 1.5f || p.has_item_with_flag( flag_SAFECRACK );
    bool can_decode = p.get_skill_level( skill_mechanics ) >= 5;

    // We can skip worrying about a stethoscope if we're skilled enough.
    if( !can_decode ) {
        // Lack both the tools and the skills so fiddle with the dial a bit.
        if( !can_safecrack ) {
            p.mod_moves( -to_moves<int>( 10_seconds ) );
            // one_in(30^3) chance of guessing
            if( one_in( 27000 ) ) {
                p.add_msg_if_player( m_good,
                                     _( "Lacking the skill to crack this without tools, you mess with the dial for a little bit… and it opens!" ) );
                get_map().furn_set( examp, f_safe_o );
                return;
            } else {
                p.add_msg_if_player( m_info,
                                     _( "Lacking the skill to crack this without tools, you mess with the dial for a little bit." ) );
                return;
            }
        }
        // We both need and have hearing enhancement, so here we rule out states that prevent us from using it.
        if( p.is_deaf() ) {
            add_msg( m_info, _( "You can't crack a safe while deaf!" ) );
            return;
        } else if( p.has_effect( effect_earphones ) ) {
            add_msg( m_info, _( "You can't crack a safe while listening to music!" ) );
            return;
        }
    }

    if( query_yn(
            _( "Attempt to crack the safe?\n\nUses a stethoscope, augmented hearing, or mechanics skill of 5 or higher." ) ) ) {
        std::string safecracking_message = can_decode ?
                                           _( "You begin to expertly decode the safe." ) :
                                           _( "You start cracking the safe." );
        add_msg( m_info, safecracking_message );
        // 120 minutes - 10 minutes per mechanics point, - 5 per perception point above 10;
        // capped at 5 minutes minimum.
        const time_duration time = safecracking_time( p );

        p.assign_activity( ACT_CRACKING, to_moves<int>( time ) );
        p.activity->placement = examp;
    }
}

/**
 * Attempt to "hack" the gunsafe's electronic lock and open it.
 * Also allow for trying to pry it open as an alternative.
 */
void iexamine::gunsafe_el( player &p, const tripoint &examp )
{
    map &here = get_map();
    safe_reference<item> prying_tool = find_best_prying_tool( p );
    const int target_diff = here.has_furn( examp ) ? here.furn( examp )->pry.pry_quality : here.ter(
                                examp )->pry.pry_quality;
    if( target_diff > 0 && prying_tool && !p.movement_mode_is( CMM_CROUCH ) ) {
        // keep going in case we have a prying tool that can't be used against the target, so we can try lockpicking
        if( prying_tool->get_quality( quality_id( "PRY" ) ) >= target_diff ) {
            apply_prying_tool( p, prying_tool.get(), examp );
            return;
        }
    }
    if( query_yn( _( "Attempt to hack this safe?" ) ) ) {
        try_start_hacking( p, examp );
    }
}

static item *find_best_lock_picking_tool( player &p )
{
    std::vector<item *> picklocks = p.items_with( []( const item & it ) {
        // we want to get worn items (eg hairpin), so no check on item position
        return it.type->get_use( "PICK_LOCK" ) != nullptr;
    } );

    // Sort by their picklock level.
    std::ranges::sort( picklocks, [&]( const item * a, const item * b ) {
        return a->get_quality( qual_LOCKPICK ) > b->get_quality( qual_LOCKPICK );
    } );

    if( picklocks.empty() ) {
        return nullptr;
    }

    return picklocks[0];
}

static void apply_lock_picking_tool( player &p, item *it, const tripoint &examp )
{
    map &here = get_map();

    const use_function *iuse_fn = it->type->get_use( "PICK_LOCK" );
    std::string target;
    if( here.has_furn( examp ) ) {
        target = here.furnname( examp );
    } else if( here.veh_at( examp ) ) {
        target = here.veh_at( examp )->vehicle().name;
    } else {
        target = here.tername( examp );
    }
    p.add_msg_if_player(
        _( "You attempt to pick lock of %1$s using your %2$s…" ), target, it->tname() );
    const ret_val<bool> can_use = iuse_fn->can_call( p, *it, false, examp );
    if( can_use.success() ) {
        p.invoke_item( it, "PICK_LOCK", examp );
        return;
    } else {
        p.add_msg_if_player( m_bad, can_use.str() );
    }
}

static bool pick_lock( player &p, const tripoint &examp )
{
    map &here = get_map();

    if( p.has_bionic( bio_lockpick ) ) {
        if( p.get_power_level() >= bio_lockpick->power_activate ) {
            p.mod_power_level( -bio_lockpick->power_activate );
            p.add_msg_if_player( m_info, _( "You activate your %s." ), bio_lockpick->name );
            p.assign_activity( std::make_unique<player_activity>( lockpick_activity_actor::use_bionic(
                                   item::spawn( bio_lockpick->fake_item ), here.getabs( examp ) ) ) );
            return true;
        } else {
            p.add_msg_if_player( m_info, _( "You don't have enough power to activate your %s." ),
                                 bio_lockpick->name );
            return false;
        }
    }

    safe_reference<item> lock_picking_tool = find_best_lock_picking_tool( p );
    if( lock_picking_tool ) {
        apply_lock_picking_tool( p, lock_picking_tool.get(), examp );
        return true;
    }

    return false;
}

/**
 * Checks whether PC has a crowbar then calls iuse.crowbar.
 */
void iexamine::locked_object( player &p, const tripoint &examp )
{
    map &here = get_map();

    // if the furniture/terrain is also lockpickable
    // try lockpicking first if we're crouched
    if( lockpick_activity_actor::is_pickable( examp ) && p.movement_mode_is( CMM_CROUCH ) ) {
        if( pick_lock( p, examp ) ) {
            return;
        }
    }

    safe_reference<item> prying_tool = find_best_prying_tool( p );
    if( prying_tool ) {
        const int target_diff = here.has_furn( examp ) ? here.furn( examp )->pry.pry_quality : here.ter(
                                    examp )->pry.pry_quality;
        // keep going in case we have a prying tool that can't be used against the target, so we can try lockpicking
        if( prying_tool->get_quality( quality_id( "PRY" ) ) >= target_diff ) {
            apply_prying_tool( p, prying_tool.get(), examp );
            return;
        }
    }

    std::string target;
    if( here.has_furn( examp ) ) {
        target = here.furnname( examp );
    } else if( here.veh_at( examp ) ) {
        target = here.veh_at( examp )->vehicle().name;
    } else {
        target = here.tername( examp );
    }

    if( lockpick_activity_actor::is_pickable( examp ) ) {
        if( !pick_lock( p, examp ) ) {
            if( prying_tool ) {
                add_msg( m_info,
                         _( "The %s is locked.  If only you had something to pick its lock, or a stronger prying tool…" ),
                         target );
            } else {
                add_msg( m_info, _( "The %s is locked.  If only you had something to pry it or pick its lock…" ),
                         target );
            }
        }
        return;
    }

    if( prying_tool ) {
        add_msg( m_info, _( "The %s is locked, and your tools aren't strong enough to pry it open…" ),
                 target );
    } else {
        add_msg( m_info, _( "The %s is locked.  If only you had something to pry it…" ),
                 target );
    }
}

/**
* Checks whether PC has picklocks then calls pick_lock iuse function OR assigns ACT_LOCKPICK
*/
void iexamine::locked_object_pickable( player &p, const tripoint &examp )
{
    map &here = get_map();

    std::string target;
    if( here.has_furn( examp ) ) {
        target = here.furnname( examp );
    } else if( here.veh_at( examp ) ) {
        target = here.veh_at( examp )->vehicle().name;
    } else {
        target = here.tername( examp );
    }

    if( !pick_lock( p, examp ) ) {
        add_msg( m_info, _( "The %s is locked.  If only you had something to pick its lock…" ),
                 target );
    }
}

/**
 * Display popup with reference to "The Enigma of Amigara Fault."
 */
void iexamine::fault( player &, const tripoint & )
{
    popup( _( "This wall is perfectly vertical.  Odd, twisted holes are set in it, leading\n"
              "as far back into the solid rock as you can see.  The holes are humanoid in\n"
              "shape, but with long, twisted, distended limbs." ) );
}

/**
 * Display popup message pulled from the object's message property
 */
void iexamine::notify( player &, const tripoint &pos )
{
    std::string message = g->m.has_furn( pos ) ?
                          g->m.furn( pos ).obj().message :
                          g->m.ter( pos ).obj().message;
    if( !message.empty() ) {
        popup( _( message ) );
    }
}

/**
* Transform the examined object into the object specified by its transforms_into property. If the new object has a message property,
* it is displayed as if the notify examine_action was used.
*/
void iexamine::transform( player &p, const tripoint &pos )
{
    std::string message;
    std::string prompt;
    const bool furn_is_deployed = !g->m.furn( pos ).obj().deployed_item.is_empty();
    const bool can_climb = g->m.has_flag( flag_CLIMBABLE, pos ) ||
                           g->m.has_flag( flag_CLIMB_SIMPLE, pos );

    if( g->m.has_furn( pos ) ) {
        message = g->m.furn( pos ).obj().message;
        prompt = g->m.furn( pos ).obj().prompt;
    } else {
        message = g->m.ter( pos ).obj().message;
        prompt = g->m.ter( pos ).obj().prompt;
    }

    uilist selection_menu;
    selection_menu.text = _( "Select an action" );
    selection_menu.addentry( 0, true, 'g', _( "Get items" ) );
    selection_menu.addentry( 1, true, 't', !prompt.empty() ? _( prompt ) : _( "Transform furniture" ) );
    if( furn_is_deployed ) {
        selection_menu.addentry( 2, true, 'T', _( "Take down the %s" ), g->m.furnname( pos ) );
    }
    if( can_climb ) {
        selection_menu.addentry( 3, true, 'c', _( "Climb %s" ), g->m.furnname( pos ) );
    }
    selection_menu.query();

    switch( selection_menu.ret ) {
        case 0:
            none( p, pos );
            pickup::pick_up( pos, 0 );
            return;
        case 1: {
            if( g->m.has_furn( pos ) ) {
                if( !message.empty() ) {
                    add_msg( _( message ) );
                }
                g->m.furn_set( pos, g->m.get_furn_transforms_into( pos ) );
            } else {
                if( !message.empty() ) {
                    add_msg( _( message ) );
                }
                g->m.ter_set( pos, g->m.get_ter_transforms_into( pos ) );
            }
            p.moves -= to_moves<int>( 2_seconds );
            return;
        }
        case 2: {
            add_msg( m_info, _( "You take down the %s." ),
                     g->m.furnname( pos ) );
            const auto furn_item = g->m.furn( pos ).obj().deployed_item;
            g->m.add_item_or_charges( pos, item::spawn( furn_item, calendar::turn ) );
            g->m.furn_set( pos, f_null );
            return;
        }
        case 3: {
            iexamine::chainfence( p, pos );
            return;
        }
        default:
            none( p, pos );
            return;
    }
}

/**
 * Spawn 1d4 wyrms and sink pedestal into ground.
 */
void iexamine::pedestal_wyrm( player &p, const tripoint &examp )
{
    map &here = get_map();
    map_stack items = here.i_at( examp );
    if( !items.empty() ) {
        if( items.only_item().typeId() == itype_petrified_eye &&
            query_yn( _( "Remove the petrified eye from the pedestal?" ) ) ) {
            here.i_clear( examp );

            p.i_add_or_drop( item::spawn( itype_petrified_eye ) );

            // Send in a few wyrms to start things off.
            get_event_bus().send<event_type::awakes_dark_wyrms>();
            for( const tripoint &p : here.points_on_zlevel() ) {
                if( here.ter( p ) == ter_id( "t_orifice" ) ) {
                    g->place_critter_around( mon_dark_wyrm, p, 1 );
                }
            }

            sounds::sound( examp, 80, sounds::sound_t::combat, _( "an ominous grinding noise…" ), true,
                           "misc", "stones_grinding" );
            add_msg( _( "The pedestal sinks into the ground…" ) );
            here.ter_set( examp, t_rock_floor );
            g->timed_events.add( TIMED_EVENT_SPAWN_WYRMS, calendar::turn + rng( 30_seconds, 60_seconds ) );
        } else {
            none( p, examp );
            add_msg( _( "You decided to leave the petrified eye on the pedestal…" ) );
            return;
        }
    }
}

/**
 * Put petrified eye on pedestal causing it to sink into ground and open temple.
 */
void iexamine::pedestal_temple( player &p, const tripoint &examp )
{
    map &here = get_map();
    map_stack items = here.i_at( examp );
    if( !items.empty() && items.only_item().typeId() == itype_petrified_eye ) {
        add_msg( _( "The pedestal sinks into the ground…" ) );
        here.ter_set( examp, t_dirt );
        here.i_clear( examp );
        g->timed_events.add( TIMED_EVENT_TEMPLE_OPEN, calendar::turn + 10_seconds );
    } else if( p.has_amount( itype_petrified_eye, 1 ) &&
               query_yn( _( "Place your petrified eye on the pedestal?" ) ) ) {
        p.use_amount( itype_petrified_eye, 1 );
        add_msg( _( "The pedestal sinks into the ground…" ) );
        here.ter_set( examp, t_dirt );
        g->timed_events.add( TIMED_EVENT_TEMPLE_OPEN, calendar::turn + 10_seconds );
    } else {
        add_msg( _( "This pedestal is engraved in eye-shaped diagrams, and has a "
                    "large semi-spherical indentation at the top." ) );
    }
}

/**
 * Unlock/open door or attempt to peek through peephole.
 */
void iexamine::door_peephole( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( here.is_outside( p.pos() ) ) {
        // if door is a locked type attempt to open
        if( here.has_flag( flag_OPENCLOSE_INSIDE, examp ) ) {
            locked_object( p, examp );
        } else {
            p.add_msg_if_player( _( "You cannot look through the peephole from the outside." ) );
        }

        return;
    }

    if( here.can_open_door( &p, examp, true ) ) {
        g->peek( examp );
        p.add_msg_if_player( _( "You peek through the peephole." ) );
    } else {
        // Peek through the peephole, or open the door.
        const int choice = uilist( _( "Do what with the door?" ), {
            _( "Peek through peephole." ),
            _( "Open door." )
        } );
        if( choice == 0 ) {
            // Peek
            g->peek( examp );
            p.add_msg_if_player( _( "You peek through the peephole." ) );
        } else if( choice == 1 ) {
            here.open_door( &p, examp, true );
            p.add_msg_if_player( _( "You open the door." ) );
        } else {
            p.add_msg_if_player( _( "Never mind." ) );
        }
    }
}

void iexamine::fswitch( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( !query_yn( _( "Flip the %s?" ), here.tername( examp ) ) ) {
        none( p, examp );
        return;
    }
    ter_id terid = here.ter( examp );
    p.moves -= to_moves<int>( 1_seconds );
    tripoint tmp;
    tmp.z = examp.z;
    for( tmp.y = examp.y; tmp.y <= examp.y + 5; tmp.y++ ) {
        for( tmp.x = 0; tmp.x < MAPSIZE_X; tmp.x++ ) {
            if( terid == t_switch_rg ) {
                if( here.ter( tmp ) == t_rock_red ) {
                    here.ter_set( tmp, t_floor_red );
                } else if( here.ter( tmp ) == t_floor_red ) {
                    here.ter_set( tmp, t_rock_red );
                } else if( here.ter( tmp ) == t_rock_green ) {
                    here.ter_set( tmp, t_floor_green );
                } else if( here.ter( tmp ) == t_floor_green ) {
                    here.ter_set( tmp, t_rock_green );
                }
            } else if( terid == t_switch_gb ) {
                if( here.ter( tmp ) == t_rock_blue ) {
                    here.ter_set( tmp, t_floor_blue );
                } else if( here.ter( tmp ) == t_floor_blue ) {
                    here.ter_set( tmp, t_rock_blue );
                } else if( here.ter( tmp ) == t_rock_green ) {
                    here.ter_set( tmp, t_floor_green );
                } else if( here.ter( tmp ) == t_floor_green ) {
                    here.ter_set( tmp, t_rock_green );
                }
            } else if( terid == t_switch_rb ) {
                if( here.ter( tmp ) == t_rock_blue ) {
                    here.ter_set( tmp, t_floor_blue );
                } else if( here.ter( tmp ) == t_floor_blue ) {
                    here.ter_set( tmp, t_rock_blue );
                } else if( here.ter( tmp ) == t_rock_red ) {
                    here.ter_set( tmp, t_floor_red );
                } else if( here.ter( tmp ) == t_floor_red ) {
                    here.ter_set( tmp, t_rock_red );
                }
            } else if( terid == t_switch_even ) {
                if( ( tmp.y - examp.y ) % 2 == 1 ) {
                    if( here.ter( tmp ) == t_rock_red ) {
                        here.ter_set( tmp, t_floor_red );
                    } else if( here.ter( tmp ) == t_floor_red ) {
                        here.ter_set( tmp, t_rock_red );
                    } else if( here.ter( tmp ) == t_rock_green ) {
                        here.ter_set( tmp, t_floor_green );
                    } else if( here.ter( tmp ) == t_floor_green ) {
                        here.ter_set( tmp, t_rock_green );
                    } else if( here.ter( tmp ) == t_rock_blue ) {
                        here.ter_set( tmp, t_floor_blue );
                    } else if( here.ter( tmp ) == t_floor_blue ) {
                        here.ter_set( tmp, t_rock_blue );
                    }
                }
            }
        }
    }
    add_msg( m_warning, _( "You hear the rumble of rock shifting." ) );
    g->timed_events.add( TIMED_EVENT_TEMPLE_SPAWN, calendar::turn + 3_turns );
}

/**
 * If it's winter: show msg and return true. Otherwise return false
 */
static bool dead_plant( bool flower, player &p, const tripoint &examp )
{
    if( season_of_year( calendar::turn ) == WINTER ) {
        if( flower ) {
            add_msg( m_info, _( "This flower is dead.  You can't get it." ) );
        } else {
            add_msg( m_info, _( "This plant is dead.  You can't get it." ) );
        }

        iexamine::none( p, examp );
        return true;
    }

    return false;
}

/**
 * Helper method to see if player has traits, hunger and mouthwear for drinking nectar.
 */
static bool can_drink_nectar( const player &p, const item &nectar )
{
    return ( p.has_active_mutation( trait_PROBOSCIS )  ||
             p.has_active_mutation( trait_BEAK_HUM ) ) &&
           ( ( p.max_stored_kcal() - p.get_stored_kcal() ) >
             nectar.get_comestible()->default_nutrition.kcal ) &&
           ( !( p.wearing_something_on( bodypart_id( "mouth" ) ) ) );
}

/**
 * Consume Nectar. -15 hunger.
 */
static bool drink_nectar( player &p )
{
    item &nectar = *item::spawn_temporary( "nectar", calendar::turn, 1 );
    if( can_drink_nectar( p, nectar ) ) {
        p.moves -= to_moves<int>( 30_seconds );
        add_msg( _( "You drink some nectar." ) );
        p.eat( nectar );
        return true;
    }

    return false;
}

/**
 * Spawn an item after harvesting the plant
 */
static void handle_harvest( player &p, const std::string &itemid, bool force_drop )
{
    detached_ptr<item> harvest = item::spawn( itemid );
    if( harvest->has_flag( flag_FORAGE_POISON ) && one_in( 10 ) ) {
        harvest->set_flag( flag_HIDDEN_POISON );
        harvest->poison = rng( 2, 7 );
    }
    if( harvest->has_flag( flag_FORAGE_HALLU ) && !harvest->has_flag( flag_HIDDEN_POISON ) &&
        one_in( 10 ) ) {
        harvest->set_flag( flag_HIDDEN_HALLU );
    }
    // Drop items that're exceed available space and things that aren't comestibles
    if( !force_drop && p.can_pick_volume( *harvest ) &&
        p.can_pick_weight( *harvest, !get_option<bool>( "DANGEROUS_PICKUPS" ) ) ) {

        p.add_msg_if_player( _( "You harvest: %s." ), harvest->tname() );
        p.i_add( std::move( harvest ) );
    } else {
        p.add_msg_if_player( _( "You harvest and drop: %s." ), harvest->tname() );
        get_map().add_item_or_charges( p.pos(), std::move( harvest ) );
    }
}

/**
 * Prompt pick (or drink nectar if able) poppy bud. Not safe for player.
 *
 * Drinking causes: -25 hunger, +20 fatigue, pkill2-70 effect and, 1 in 20 pkiller-1 addiction.
 * Picking w/ env_resist < 5 causes 1 in 3  sleep for 12 min and 4 dmg to each leg
 */
void iexamine::flower_poppy( player &p, const tripoint &examp )
{
    if( dead_plant( true, p, examp ) ) {
        return;
    }
    map &here = get_map();
    // TODO: Get rid of this section and move it to eating
    // Two y/n prompts is just too much
    item &poppy = *item::spawn_temporary( "nectar", calendar::turn, 1 );
    if( can_drink_nectar( p, poppy ) ) {
        if( !query_yn( _( "You feel woozy as you explore the %s. Drink?" ),
                       here.furnname( examp ) ) ) {
            return;
        }
        p.moves -= to_moves<int>( 30_seconds ); // You take your time...
        add_msg( _( "You slowly suck up the nectar." ) );
        p.eat( poppy );
        p.mod_fatigue( 20 );
        p.add_effect( effect_pkill2, 7_minutes );
        // Please drink poppy nectar responsibly.
        if( one_in( 20 ) ) {
            p.add_addiction( add_type::PKILLER, 1 );
        }
    }
    if( !query_yn( _( "Pick %s?" ), here.furnname( examp ) ) ) {
        none( p, examp );
        return;
    }

    int resist = p.get_env_resist( bodypart_id( "mouth" ) );

    if( resist < 10 ) {
        // Can't smell the flowers with a gas mask on!
        add_msg( m_warning, _( "This flower has a heady aroma." ) );
    }

    auto recentWeather = sum_conditions( calendar::turn - 10_minutes, calendar::turn, p.pos() );

    // If it has been raining recently, then this event is twice less likely.
    if( ( ( recentWeather.rain_amount > 1 ) ? one_in( 6 ) : one_in( 3 ) ) && resist < 5 ) {
        // Should user player::infect, but can't!
        // player::infect needs to be restructured to return a bool indicating success.
        add_msg( m_bad, _( "You fall asleep…" ) );
        p.fall_asleep( 2_hours );
        add_msg( m_bad, _( "Your legs are covered in the poppy's roots!" ) );
        p.apply_damage( nullptr, bodypart_id( "leg_l" ), 4 );
        p.apply_damage( nullptr, bodypart_id( "leg_r" ), 4 );
        p.moves -= 50;
    }

    here.furn_set( examp, f_null );

    handle_harvest( p, "poppy_bud", false );
    handle_harvest( p, "withered", false );
}

/**
 * Prompt pick cactus pad. Not safe for player.
 */
void iexamine::flower_cactus( player &p, const tripoint &examp )
{
    if( dead_plant( true, p, examp ) ) {
        return;
    }

    map &here = get_map();
    if( !query_yn( _( "Pick %s?" ), here.furnname( examp ) ) ) {
        none( p, examp );
        return;
    }

    if( one_in( 6 ) ) {
        add_msg( m_bad, _( "The cactus' nettles sting you!" ) );
        p.apply_damage( nullptr, bodypart_id( "arm_l" ), 4 );
        p.apply_damage( nullptr, bodypart_id( "arm_r" ), 4 );
    }

    here.furn_set( examp, f_null );

    handle_harvest( p, "cactus_pad", false );
    handle_harvest( p, "seed_cactus", false );
}

/**
 * Dig up its roots or drink its nectar if you can.
 */
void iexamine::flower_dahlia( player &p, const tripoint &examp )
{
    if( dead_plant( true, p, examp ) ) {
        return;
    }

    if( drink_nectar( p ) ) {
        return;
    }

    map &here = get_map();
    bool can_get_root = p.has_quality( qual_DIG ) || p.has_trait( trait_BURROW );
    if( can_get_root ) {
        if( !query_yn( _( "Pick %s?" ), here.furnname( examp ) ) ) {
            none( p, examp );
            return;
        }
    } else {
        if( !query_yn( _( "You don't have a digging tool to dig up roots.  Pick %s anyway?" ),
                       here.furnname( examp ) ) ) {
            none( p, examp );
            return;
        }
    }

    here.furn_set( examp, f_null );

    if( can_get_root ) {
        handle_harvest( p, "dahlia_root", false );
    }
    handle_harvest( p, "seed_dahlia", false );
    handle_harvest( p, "withered", false );
    // There was a bud and flower spawn here
    // But those were useless, don't re-add until they get useful
}

static bool harvest_common( player &p, const tripoint &examp, bool furn, bool nectar,
                            bool auto_forage = false )
{
    map &here = get_map();
    const auto hid = here.get_harvest( examp );
    if( hid.is_null() || hid->empty() ) {
        if( !auto_forage ) {
            p.add_msg_if_player( m_info, _( "Nothing can be harvested from this currently." ) );
        }
        if( p.manual_examine ) {
            iexamine::none( p, examp );
        }
        return false;
    }

    const auto &harvest = hid.obj();

    // If nothing can be harvested, neither can nectar
    // Incredibly low priority TODO: Allow separating nectar seasons
    if( nectar && drink_nectar( p ) ) {
        return false;
    }

    if( p.is_player() && !auto_forage &&
        !query_yn( _( "Pick %s?" ), furn ? here.furnname( examp ) : here.tername(
                       examp ) ) ) {
        iexamine::none( p, examp );
        return false;
    }

    int lev = p.get_skill_level( skill_survival );
    bool got_anything = false;
    for( const auto &entry : harvest ) {
        float min_num = entry.base_num.first + lev * entry.scale_num.first;
        float max_num = entry.base_num.second + lev * entry.scale_num.second;
        int roll = std::min<int>( entry.max, std::round( rng_float( min_num, max_num ) ) );
        if( roll >= 1 ) {
            got_anything = true;
            for( int i = 0; i < roll; i++ ) {
                handle_harvest( p, entry.drop, entry.no_auto_pickup );
            }
        }
    }

    if( !got_anything ) {
        p.add_msg_if_player( m_bad, _( "You couldn't harvest anything." ) );
    }

    iexamine::practice_survival_while_foraging( &p );

    p.mod_moves( -to_moves<int>( rng( 5_seconds, 15_seconds ) ) );
    return true;
}

void iexamine::harvest_furn_nectar( player &p, const tripoint &examp )
{
    bool auto_forage = get_option<bool>( "AUTO_FEATURES" ) &&
                       ( get_option<std::string>( "AUTO_FORAGING" ) == "flowers" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "both" );
    if( harvest_common( p, examp, true, true, auto_forage ) ) {
        map &here = get_map();
        get_map().furn_set( examp, here.get_furn_transforms_into( examp ) );
    }
}

void iexamine::harvest_furn( player &p, const tripoint &examp )
{
    bool auto_forage = get_option<bool>( "AUTO_FEATURES" ) &&
                       ( get_option<std::string>( "AUTO_FORAGING" ) == "flowers" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "both" );
    if( harvest_common( p, examp, true, false, auto_forage ) ) {
        map &here = get_map();
        get_map().furn_set( examp, here.get_furn_transforms_into( examp ) );
    }
}

void iexamine::harvest_ter_nectar( player &p, const tripoint &examp )
{
    bool auto_forage = get_option<bool>( "AUTO_FEATURES" ) &&
                       ( get_option<std::string>( "AUTO_FORAGING" ) == "both" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "bushes" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "flowers" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "trees" );
    if( harvest_common( p, examp, false, true, auto_forage ) ) {
        map &here = get_map();
        here.ter_set( examp, here.get_ter_transforms_into( examp ) );
    }
}

void iexamine::harvest_ter( player &p, const tripoint &examp )
{
    bool auto_forage = get_option<bool>( "AUTO_FEATURES" ) &&
                       ( get_option<std::string>( "AUTO_FORAGING" ) == "both" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "flowers" ||
                         get_option<std::string>( "AUTO_FORAGING" ) == "trees" );
    if( harvest_common( p, examp, false, false, auto_forage ) ) {
        map &here = get_map();
        here.ter_set( examp, here.get_ter_transforms_into( examp ) );
    }
}

/**
 * Only harvest a plant once per season.  Display message and call iexamine::none.
 */
void iexamine::harvested_plant( player &p, const tripoint &examp )
{
    p.add_msg_if_player( m_info, _( "Nothing can be harvested from this currently." ) );
    iexamine::none( p, examp );
}

void iexamine::flower_marloss( player &p, const tripoint &examp )
{
    if( season_of_year( calendar::turn ) == WINTER ) {
        add_msg( m_info, _( "This flower is still alive, despite the harsh conditions…" ) );
    }
    map &here = get_map();
    item &nectar = *item::spawn_temporary( "nectar" );
    if( can_drink_nectar( p, nectar ) ) {
        if( !query_yn( _( "You feel out of place as you explore the %s. Drink?" ),
                       here.furnname( examp ) ) ) {
            return;
        }
        p.moves -= to_moves<int>( 30_seconds ); // Takes 30 seconds
        add_msg( m_bad, _( "This flower tastes very wrong…" ) );
        // If you can drink flowers, you're post-thresh and the Mycus does not want you.
        p.add_effect( effect_teleglow, 10_minutes );
    }
    if( !query_yn( _( "Pick %s?" ), here.furnname( examp ) ) ) {
        none( p, examp );
        return;
    }
    here.furn_set( examp, f_null );
    here.spawn_item( p.pos(), itype_marloss_seed, 1, 3, calendar::turn );
    handle_harvest( p, "withered", false );
}

/**
 * Spawn spiders from a spider egg sack in radius 1 around the egg sack.
 * Transforms the egg sack furniture into a ruptured egg sack (f_egg_sacke).
 * Also spawns eggs.
 * @param p The player
 * @param examp Location of egg sack
 * @param montype The monster type of the created spiders.
 */
void iexamine::egg_sack_generic( player &p, const tripoint &examp,
                                 const mtype_id &montype )
{
    map &here = get_map();
    const std::string old_furn_name = here.furnname( examp );
    if( !query_yn( _( "Harvest the %s?" ), old_furn_name ) ) {
        none( p, examp );
        return;
    }
    here.furn_set( examp, f_egg_sacke );
    int monster_count = 0;
    if( one_in( 2 ) ) {
        for( const tripoint &nearby_pos : closest_points_first( examp, 1 ) ) {
            if( !one_in( 3 ) ) {
                continue;
            } else if( g->place_critter_at( montype, nearby_pos ) ) {
                monster_count++;
            }
        }
    }
    int roll = rng( 4, 20 );
    bool drop_eggs = monster_count >= 1;
    for( int i = 0; i < roll; i++ ) {
        handle_harvest( p, "spider_egg", drop_eggs );
    }
    if( monster_count == 1 ) {
        add_msg( m_warning, _( "A spiderling bursts from the %s!" ), old_furn_name );
    } else if( monster_count >= 1 ) {
        add_msg( m_warning, _( "Spiderlings burst from the %s!" ), old_furn_name );
    }
}

void iexamine::egg_sackbw( player &p, const tripoint &examp )
{
    egg_sack_generic( p, examp, mon_spider_widow_giant_s );
}

void iexamine::egg_sackcs( player &p, const tripoint &examp )
{
    egg_sack_generic( p, examp, mon_spider_cellar_giant_s );
}

void iexamine::egg_sackws( player &p, const tripoint &examp )
{
    egg_sack_generic( p, examp, mon_spider_web_s );
}

/**
 * Remove furniture. Add spore effect.
 */
void iexamine::fungus( player &p, const tripoint &examp )
{
    map &here = get_map();
    add_msg( _( "The %s crumbles into spores!" ), here.furnname( examp ) );
    fungal_effects( *g, here ).create_spores( examp, &p );
    here.furn_set( examp, f_null );
    p.moves -= 50;
}

/**
 *  Make lists of unique seed types and names for the menu(no multiple hemp seeds etc)
 */
std::vector<seed_tuple> iexamine::get_seed_entries( const std::vector<item *> &seed_inv )
{
    std::map<itype_id, int> seed_map;
    for( auto &seed : seed_inv ) {
        seed_map[seed->typeId()] += ( seed->charges > 0 ? seed->charges : 1 );
    }

    std::vector<seed_tuple> seed_entries;
    seed_entries.reserve( seed_map.size() );
    for( const auto &pr : seed_map ) {
        seed_entries.emplace_back(
            pr.first, item::nname( pr.first, pr.second ), pr.second );
    }

    // Sort by name
    std::ranges::sort( seed_entries,
    []( const seed_tuple & l, const seed_tuple & r ) {
        return std::get<1>( l ).compare( std::get<1>( r ) ) < 0;
    } );

    return seed_entries;
}

/**
 *  Choose seed for planting
 */
int iexamine::query_seed( const std::vector<seed_tuple> &seed_entries, int min_req )
{
    uilist smenu;

    smenu.text = _( "Use which seed?" );
    int count = 0;
    for( const auto &entry : seed_entries ) {
        const std::string &seed_name = std::get<1>( entry );
        int seed_count = std::get<2>( entry );

        std::string format = seed_count > 0 ? "%s (%d)" : "%s";

        smenu.addentry( count++, seed_count >= min_req, MENU_AUTOASSIGN, format.c_str(),
                        seed_name, seed_count );
    }

    smenu.query();

    if( smenu.ret >= 0 ) {
        return smenu.ret;
    } else {
        return seed_entries.size();
    }
}

/**
 *  Actual planting of selected seed
 */
void iexamine::plant_seed( player &p, const tripoint &examp, const itype_id &seed_id )
{
    std::unique_ptr<player_activity> act = std::make_unique<player_activity>( ACT_PLANT_SEED,
                                           to_moves<int>( 30_seconds ) );
    act->placement = get_map().getabs( examp );
    act->str_values.emplace_back( seed_id );
    p.assign_activity( std::move( act ) );
}

/**
 * If it's warm enough, pick one of the player's seeds and plant it.
 */
void iexamine::dirtmound( player &p, const tripoint &examp )
{

    if( !warm_enough_to_plant( g->u.pos() ) ) {
        add_msg( m_info, _( "It is too cold to plant anything now." ) );
        return;
    }
    map &here = get_map();
    /* ambient_light_at() not working?
    if (here.ambient_light_at(examp) < LIGHT_AMBIENT_LOW) {
        add_msg(m_info, _("It is too dark to plant anything now."));
        return;
    }*/
    std::vector<item *> seed_inv = p.items_with( []( const item & itm ) {
        return itm.is_seed();
    } );
    if( seed_inv.empty() ) {
        add_msg( m_info, _( "You have no seeds to plant." ) );
        return;
    }
    if( !here.i_at( examp ).empty() ) {
        add_msg( _( "Something's lying there…" ) );
        return;
    }

    auto seed_entries = get_seed_entries( seed_inv );

    int seed_index = query_seed( seed_entries );

    // Did we cancel?
    if( seed_index < 0 || seed_index >= static_cast<int>( seed_entries.size() ) ) {
        add_msg( _( "You saved your seeds for later." ) );
        return;
    }
    const auto &seed_id = std::get<0>( seed_entries[seed_index] );

    // Separate temp check because for now we permit growing regular plants underground with artificial heating
    if( !seed_id.obj().has_flag( flag_CAN_PLANT_UNDERGROUND ) && examp.z < 0 &&
        get_weather().get_temperature( examp ) < 10_c ) {
        add_msg( _( "It's too cold down here to plant this type of seed underground." ) );
        return;
    }

    if( !here.has_flag_ter_or_furn( seed_id->seed->required_terrain_flag, examp ) ) {
        add_msg( _( "This type of seed can not be planted in this location." ) );
        return;
    }

    plant_seed( p, examp, seed_id );
}

/**
 * Items that appear when a generic plant is harvested. Seed @ref islot_seed.
 * @param type The seed type, must have a @ref itype::seed slot.
 * @param plant_count Number of fruits to generate. For charge-based items, this
 *     specifies multiples of the default charge.
 * @param seed_count Number of seeds to generate.
 * @param byproducts If true, byproducts (like straw, withered plants, see
 * @ref islot_seed::byproducts) are included.
 */
std::vector<detached_ptr<item>> iexamine::get_harvest_items( const itype &type,
                             const int plant_count,
                             const int seed_count, const bool byproducts )
{
    std::vector<detached_ptr<item>> result;
    if( !type.seed ) {
        return result;
    }
    const islot_seed &seed_data = *type.seed;
    // This is a temporary measure, itype should instead provide appropriate accessors
    // to expose data about the seed item to allow harvesting to function.
    const itype_id &seed_type = type.get_id();

    const auto add = [&]( const itype_id & id, int count ) {

        //TODO!:check
        detached_ptr<item> new_item = item::spawn( id, calendar::turn );
        if( new_item->count_by_charges() && count > 0 ) {
            new_item->charges *= count;
            new_item->charges /= seed_data.fruit_div;
            if( new_item->charges <= 0 ) {
                new_item->charges = 1;
            }
            result.push_back( std::move( new_item ) );
        } else {
            while( count > 0 ) {
                result.push_back( item::spawn( id, calendar::turn ) );
                count--;
            }
        }
    };

    if( seed_data.spawn_seeds ) {
        add( seed_type, seed_count );
    }

    add( seed_data.fruit_id, plant_count );

    if( byproducts ) {
        for( auto &b : seed_data.byproducts ) {
            add( b, 1 );
        }
    }

    return result;
}

/**
 * Actual harvesting of selected plant
 */
void iexamine::harvest_plant( player &p, const tripoint &examp, bool from_activity )
{
    map &here = get_map();
    // Can't use item_stack::only_item() since there might be fertilizer
    map_stack items = here.i_at( examp );
    map_stack::iterator seed_it = std::ranges::find_if( items,
    []( const item * const & it ) {
        return it->is_seed();
    } );

    if( seed_it == items.end() ) {
        debugmsg( "Missing seed for plant at (%d, %d, %d)", examp.x, examp.y, examp.z );
        here.i_clear( examp );
        here.furn_set( examp, f_null );
        return;
    }
    item *&seed = *seed_it;
    seed_it = map_stack::iterator();
    const itype_id &seedType = seed->typeId();
    if( seedType == itype_fungal_seeds ) {
        fungus( p, examp );
        here.i_clear( examp );
    } else if( seedType == itype_marloss_seed ) {
        fungus( p, examp );
        here.i_clear( examp );
        if( p.has_trait( trait_M_DEPENDENT ) && ( p.get_kcal_percent() < 0.8f ||
                p.get_thirst() > thirst_levels::very_thirsty ) ) {
            here.ter_set( examp, t_marloss );
            add_msg( m_info,
                     _( "We have altered this unit's configuration to extract and provide local nutriment.  The Mycus provides." ) );
        } else if( ( p.has_trait( trait_M_DEFENDER ) ) || ( ( p.has_trait( trait_M_SPORES ) ||
                   p.has_trait( trait_M_FERTILE ) ) &&
                   one_in( 2 ) ) ) {
            g->place_critter_at( mon_fungal_blossom, examp );
            add_msg( m_info, _( "The seed blooms forth!  We have brought true beauty to this world." ) );
        } else if( ( p.has_trait( trait_THRESH_MYCUS ) ) || one_in( 4 ) ) {
            here.furn_set( examp, f_flower_marloss );
            add_msg( m_info, _( "The seed blossoms rather rapidly…" ) );
        } else {
            here.furn_set( examp, f_flower_fungal );
            add_msg( m_info, _( "The seed blossoms into a flower-looking fungus." ) );
        }
    } else { // Generic seed, use the seed item data
        const itype &type = *seed->type;
        here.i_clear( examp );

        int skillLevel = p.get_skill_level( skill_survival );
        ///\EFFECT_SURVIVAL increases number of plants harvested from a seed
        int plant_count = rng( skillLevel / 2, skillLevel );
        plant_count *= here.furn( examp )->plant->harvest_multiplier;
        const int max_harvest_count = get_option<int>( "MAX_HARVEST_COUNT" );
        if( plant_count >= max_harvest_count ) {
            plant_count = max_harvest_count;
        } else if( plant_count <= 0 ) {
            plant_count = 1;
        }
        const int seedCount = std::max( 1, rng( plant_count / 4, plant_count / 2 ) );
        for( auto &i : get_harvest_items( type, plant_count, seedCount, true ) ) {
            if( from_activity ) {
                i->set_var( "activity_var", p.name );
            }
            here.add_item_or_charges( examp, std::move( i ) );
        }
        here.furn_set( examp, furn_str_id( here.furn( examp )->plant->transform ) );
        p.moves -= to_moves<int>( 10_seconds );
    }
}

ret_val<bool> iexamine::can_fertilize( player &p, const tripoint &tile,
                                       const itype_id &fertilizer )
{
    map &here = get_map();
    if( !here.has_flag_furn( "PLANT", tile ) ) {
        return ret_val<bool>::make_failure( _( "Tile isn't a plant" ) );
    }
    if( here.i_at( tile ).size() > 1 ) {
        return ret_val<bool>::make_failure( _( "Tile is already fertilized" ) );
    }
    if( !p.has_charges( fertilizer, 1 ) ) {
        return ret_val<bool>::make_failure(
                   _( "Tried to fertilize with %s, but player doesn't have any." ),
                   fertilizer.c_str() );
    }

    return ret_val<bool>::make_success();
}

void iexamine::fertilize_plant( player &p, const tripoint &tile, const itype_id &fertilizer )
{
    ret_val<bool> can_fert = can_fertilize( p, tile, fertilizer );
    if( !can_fert.success() ) {
        debugmsg( can_fert.str() );
        return;
    }

    map &here = get_map();
    // The plant furniture has the NOITEM token which prevents adding items on that square,
    // spawned items are moved to an adjacent field instead, but the fertilizer token
    // must be on the square of the plant, therefore this hack:
    const auto old_furn = here.furn( tile );
    here.furn_set( tile, f_null );
    here.spawn_item( tile, itype_fertilizer, 1, 1, calendar::turn );
    here.furn_set( tile, old_furn );
    std::vector<detached_ptr<item>> planted = p.use_charges( fertilizer, 1 );

    // Can't use item_stack::only_item() since there might be fertilizer
    map_stack items = here.i_at( tile );
    map_stack::iterator seed_it = std::ranges::find_if( items,
    []( const item * const & it ) {
        return it->is_seed();
    } );
    if( seed_it == items.end() ) {
        debugmsg( "Missing seed for plant at (%d, %d, %d)", tile.x, tile.y, tile.z );
        here.i_clear( tile );
        here.furn_set( tile, f_null );
        return;
    }

    item *&seed = *seed_it;
    seed_it = map_stack::iterator();
    // Reduce the amount of time it takes until the next stage of growth
    // by 60% of the seed's stage duration, or 20% of its overall growth time
    const time_duration fertilized_boost = seed->get_plant_epoch() * 0.6;
    seed->set_birthday( seed->birthday() - fertilized_boost );
    p.mod_moves( -to_moves<int>( 10_seconds ) );
    //~ %1$s: plant name, %2$s: fertilizer name
    add_msg( m_info, _( "You fertilize the %1$s with the %2$s." ), seed->get_plant_name(),
             planted.front()->tname() );
}

itype_id iexamine::choose_fertilizer( player &p, const std::string &pname, bool ask_player )
{
    std::vector<item *> f_inv = p.all_items_with_flag( flag_FERTILIZER );
    if( f_inv.empty() ) {
        add_msg( m_info, _( "You have no fertilizer for the %s." ), pname );
        return itype_id();
    }

    std::vector<itype_id> f_types;
    std::vector<std::string> f_names;
    for( auto &f : f_inv ) {
        if( std::ranges::find( f_types, f->typeId() ) == f_types.end() ) {
            f_types.push_back( f->typeId() );
            f_names.push_back( f->tname() );
        }
    }

    if( ask_player && !query_yn( _( "Fertilize the %s?" ), pname ) ) {
        return itype_id();
    }

    // Choose fertilizer from list
    int f_index = 0;
    if( f_types.size() > 1 ) {
        f_index = uilist( _( "Use which fertilizer?" ), f_names );
    }
    if( f_index < 0 ) {
        return itype_id();
    }

    return f_types[f_index];

}
void iexamine::aggie_plant( player &p, const tripoint &examp )
{
    map &here = get_map();
    // Can't use item_stack::only_item() since there might be fertilizer
    map_stack items = here.i_at( examp );
    map_stack::iterator seed_it = std::ranges::find_if( items,
    []( const item * const & it ) {
        return it->is_seed();
    } );

    if( seed_it == items.end() ) {
        debugmsg( "Missing seed for plant at (%d, %d, %d)", examp.x, examp.y, examp.z );
        here.i_clear( examp );
        here.furn_set( examp, f_null );
        return;
    }

    const item *const &seed = *seed_it;
    seed_it = map_stack::iterator();

    const std::string pname = seed->get_plant_name();

    if( here.has_flag_furn( flag_GROWTH_HARVEST, examp ) &&
        query_yn( _( "Harvest the %s?" ), pname ) ) {
        harvest_plant( p, examp );
    } else if( !here.has_flag_furn( flag_GROWTH_HARVEST, examp ) ) {
        if( here.i_at( examp ).size() > 1 ) {
            add_msg( m_info, _( "This %s has already been fertilized." ), pname );
            return;
        }
        itype_id fertilizer = choose_fertilizer( p, pname, true /*ask player for confirmation */ );

        if( !fertilizer.is_empty() ) {
            fertilize_plant( p, examp, fertilizer );
        }
    }
}

// Highly modified fermenting vat functions
void iexamine::kiln_empty( player &p, const tripoint &examp )
{
    map &here = get_map();
    furn_id cur_kiln_type = here.furn( examp );
    furn_id next_kiln_type = f_null;
    if( cur_kiln_type == f_kiln_empty ) {
        next_kiln_type = f_kiln_full;
    } else if( cur_kiln_type == f_kiln_metal_empty ) {
        next_kiln_type = f_kiln_metal_full;
    } else {
        debugmsg( "Examined furniture has action kiln_empty, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    static const std::set<material_id> kilnable{ material_id( "wood" ), material_id( "bone" ), material_id( "bone_heavy" ) };
    bool fuel_present = false;
    auto items = here.i_at( examp );
    for( const item * const &i : items ) {
        if( i->typeId() == itype_charcoal ) {
            add_msg( _( "This kiln already contains charcoal." ) );
            add_msg( _( "Remove it before firing the kiln again." ) );
            return;
        } else if( i->made_of_any( kilnable ) ) {
            fuel_present = true;
        } else {
            add_msg( m_bad, _( "This kiln contains %s, which can't be made into charcoal!" ), i->tname( 1,
                     false ) );
            return;
        }
    }

    if( !fuel_present ) {
        add_msg( _( "This kiln is empty.  Fill it with wood or bone and try again." ) );
        return;
    }

    ///\EFFECT_FABRICATION decreases loss when firing a kiln
    const int skill = p.get_skill_level( skill_fabrication );
    int loss = 60 - 2 *
               skill; // We can afford to be inefficient - logs and skeletons are cheap, charcoal isn't

    // Burn stuff that should get charred, leave out the rest
    units::volume total_volume = 0_ml;
    for( const item * const &i : items ) {
        total_volume += i->volume();
    }

    units::volume char_volume = ( 100 - loss ) * total_volume / 100;
    int char_charges = itype_unfinished_charcoal->charges_per_volume( char_volume );
    if( char_charges < 1 ) {
        add_msg( _( "The batch in this kiln is too small to yield any charcoal." ) );
        return;
    }

    if( !p.has_charges( itype_fire, 1 ) ) {
        add_msg( _( "This kiln is ready to be fired, but you have no fire source." ) );
        return;
    } else {
        add_msg( _( "This kiln contains %s %s of material, and is ready to be fired." ),
                 format_volume( total_volume ), volume_units_abbr() );
        if( !query_yn( _( "Fire the kiln?" ) ) ) {
            return;
        }
    }

    p.use_charges( itype_fire, 1 );
    here.i_clear( examp );
    here.furn_set( examp, next_kiln_type );
    detached_ptr<item> result = item::spawn( itype_unfinished_charcoal, calendar::turn );
    result->charges = char_charges;
    here.add_item( examp, std::move( result ) );
    add_msg( _( "You fire the charcoal kiln." ) );
}

void iexamine::kiln_full( player &, const tripoint &examp )
{
    map &here = get_map();
    furn_id cur_kiln_type = here.furn( examp );
    furn_id next_kiln_type = f_null;
    if( cur_kiln_type == f_kiln_full ) {
        next_kiln_type = f_kiln_empty;
    } else if( cur_kiln_type == f_kiln_metal_full ) {
        next_kiln_type = f_kiln_metal_empty;
    } else {
        debugmsg( "Examined furniture has action kiln_full, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    map_stack items = here.i_at( examp );
    if( items.empty() ) {
        add_msg( _( "This kiln is empty…" ) );
        here.furn_set( examp, next_kiln_type );
        return;
    }
    add_msg( _( "There's a charcoal kiln there." ) );
    const time_duration firing_time = 6_hours; // 5 days in real life
    const time_duration time_left = firing_time - items.only_item().age();
    if( time_left > 0_turns ) {
        int hours = to_hours<int>( time_left );
        int minutes = to_minutes<int>( time_left ) + 1;
        if( minutes > 60 ) {
            add_msg( vgettext( "It will finish burning in about %d hour.",
                               "It will finish burning in about %d hours.",
                               hours ), hours );
        } else if( minutes > 30 ) {
            add_msg( _( "It will finish burning in less than an hour." ) );
        } else {
            add_msg( _( "It should take about %d minutes to finish burning." ), minutes );
        }
        return;
    }

    units::volume total_volume = 0_ml;
    // Burn stuff that should get charred, leave out the rest
    for( auto item_it = items.begin(); item_it != items.end(); ) {
        if( ( *item_it )->typeId() == itype_unfinished_charcoal ||
            ( *item_it )->typeId() == itype_charcoal ) {
            total_volume += ( *item_it )->volume();
            item_it = items.erase( item_it );
        } else {
            item_it++;
        }
    }
    detached_ptr<item> result = item::spawn( itype_charcoal, calendar::turn );
    result->charges = itype_charcoal->charges_per_volume( total_volume );
    add_msg( _( "It has finished burning, yielding %d charcoal." ), result->charges );
    here.add_item( examp, std::move( result ) );
    here.furn_set( examp, next_kiln_type );
}
//arc furnance start
void iexamine::arcfurnace_empty( player &p, const tripoint &examp )
{
    map &here = get_map();
    furn_id cur_arcfurnace_type = here.furn( examp );
    furn_id next_arcfurnace_type = f_null;
    if( cur_arcfurnace_type == f_arcfurnace_empty ) {
        next_arcfurnace_type = f_arcfurnace_full;
    } else {
        debugmsg( "Examined furniture has action arcfurnace_empty, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    static const std::set<material_id> arcfurnaceable{ material_id( "cac2powder" ) };
    bool fuel_present = false;
    auto items = here.i_at( examp );
    for( const item * const &i : items ) {
        if( i->typeId() == itype_chem_carbide ) {
            add_msg( _( "This furnace already contains calcium carbide." ) );
            add_msg( _( "Remove it before activating the arc furnace again." ) );
            return;
        } else if( i->made_of_any( arcfurnaceable ) ) {
            fuel_present = true;
        } else {
            add_msg( m_bad, _( "This furnace contains %s, which can't be made into calcium carbide!" ),
                     i->tname( 1, false ) );
            return;
        }
    }

    if( !fuel_present ) {
        add_msg( _( "This furance is empty.  Fill it with powdered coke and lime mix, and try again." ) );
        return;
    }

    ///\EFFECT_FABRICATION decreases loss when firing a furnace
    const int skill = p.get_skill_level( skill_fabrication );
    int loss = 60 - 2 *
               skill; // Inefficency is still fine, coal and limestone is abundant

    // Burn stuff that should get charred, leave out the rest
    units::volume total_volume = 0_ml;
    for( const item * const &i : items ) {
        total_volume += i->volume();
    }

    units::volume char_volume = ( 100 - loss ) * total_volume / 100;
    int char_charges = itype_unfinished_cac2->charges_per_volume( char_volume );
    if( char_charges < 1 ) {
        add_msg( _( "The batch in this furance is too small to yield usable calcium carbide." ) );
        return;
    }
    //arc furnaces require a huge amount of current, so 1 full storage battery would work as a stand in
    if( !p.has_charges( itype_UPS, 1250 ) ) {
        add_msg( _( "This furnace is ready to be turned on, but you lack a UPS with sufficient power." ) );
        return;
    } else {
        add_msg( _( "This furnace contains %s %s of material, and is ready to be turned on." ),
                 format_volume( total_volume ), volume_units_abbr() );
        if( !query_yn( _( "Turn on the furnace?" ) ) ) {
            return;
        }
    }

    p.use_charges( itype_UPS, 1250 );
    here.i_clear( examp );
    here.furn_set( examp, next_arcfurnace_type );
    detached_ptr<item> result = item::spawn( itype_unfinished_cac2, calendar::turn );
    result->charges = char_charges;
    here.add_item( examp, std::move( result ) );
    add_msg( _( "You turn on the furnace." ) );
}

void iexamine::arcfurnace_full( player &, const tripoint &examp )
{
    map &here = get_map();
    furn_id cur_arcfurnace_type = here.furn( examp );
    furn_id next_arcfurnace_type = f_null;
    if( cur_arcfurnace_type == f_arcfurnace_full ) {
        next_arcfurnace_type = f_arcfurnace_empty;
    } else {
        debugmsg( "Examined furniture has action arcfurnace_full, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    map_stack items = here.i_at( examp );
    if( items.empty() ) {
        add_msg( _( "This furnace is empty…" ) );
        here.furn_set( examp, next_arcfurnace_type );
        return;
    }
    add_msg( _( "There's an arc furnace there." ) );
    const time_duration firing_time = 2_hours; // Arc furnaces work really fast in reality
    const time_duration time_left = firing_time - items.only_item().age();
    if( time_left > 0_turns ) {
        int hours = to_hours<int>( time_left );
        int minutes = to_minutes<int>( time_left ) + 1;
        if( minutes > 60 ) {
            add_msg( vgettext( "It will finish burning in about %d hour.",
                               "It will finish burning in about %d hours.",
                               hours ), hours );
        } else if( minutes > 30 ) {
            add_msg( _( "It will finish burning in less than an hour." ) );
        } else {
            add_msg( _( "It should take about %d minutes to finish burning." ), minutes );
        }
        return;
    }

    units::volume total_volume = 0_ml;
    // Burn stuff that should get charred, leave out the rest
    for( auto item_it = items.begin(); item_it != items.end(); ) {
        if( ( *item_it )->typeId() == itype_unfinished_cac2 ||
            ( *item_it )->typeId() == itype_chem_carbide ) {
            total_volume += ( *item_it )->volume();
            item_it = items.erase( item_it );
        } else {
            item_it++;
        }
    }

    detached_ptr<item> result = item::spawn( itype_chem_carbide, calendar::turn );
    result->charges = itype_chem_carbide->charges_per_volume( total_volume );

    add_msg( _( "It has finished burning, yielding %d calcium carbide." ), result->charges );
    here.add_item( examp, std::move( result ) );
    here.furn_set( examp, next_arcfurnace_type );
}
//arc furnace end

void iexamine::autoclave_empty( player &p, const tripoint & )
{
    item *bionic = game_menus::inv::sterilize_cbm( p );
    if( bionic ) {
        avatar_funcs::mend_item( *p.as_avatar(), *bionic );
    } else {
        add_msg( _( "Never mind." ) );
    }
}

void iexamine::autoclave_full( player &, const tripoint &examp )
{
    map &here = get_map();
    furn_id cur_autoclave_type = here.furn( examp );
    furn_id next_autoclave_type = f_null;
    if( cur_autoclave_type == furn_id( "f_autoclave_full" ) ) {
        next_autoclave_type = furn_id( "f_autoclave" );
    } else {
        debugmsg( "Examined furniture has action autoclave_full, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    map_stack items = here.i_at( examp );
    bool cbms = std::ranges::all_of( items, []( const item * const & i ) {
        return i->is_bionic();
    } );

    bool cbms_not_packed = std::ranges::all_of( items, []( const item * const & i ) {
        return i->is_bionic() && i->has_flag( flag_NO_PACKED );
    } );

    if( items.empty() ) {
        add_msg( _( "This autoclave is empty…" ) );
        here.furn_set( examp, next_autoclave_type );
        return;
    }
    if( !cbms ) {
        add_msg( m_bad,
                 _( "ERROR Autoclave can't process non CBM items." ) );
        return;
    }
    add_msg( _( "The autoclave is running." ) );

    const item &clock = **items.begin();
    const time_duration Cycle_time = 90_minutes;
    const time_duration time_left = Cycle_time - clock.age();

    if( time_left > 0_turns ) {
        add_msg( _( "The cycle will be complete in %s." ), to_string( time_left ) );
        return;
    }

    here.furn_set( examp, next_autoclave_type );
    for( item * const &it : items ) {
        if( !it->has_flag( flag_NO_PACKED ) ) {
            it->unset_flag( flag_NO_STERILE );
        }
    }
    add_msg( m_good, _( "The cycle is complete, the CBMs are now sterile." ) );

    if( cbms_not_packed ) {
        add_msg( m_info,
                 _( "CBMs in direct contact with the environment will almost immediately become contaminated." ) );
    }
    here.furn_set( examp, next_autoclave_type );
}

void iexamine::fireplace( player &p, const tripoint &examp )
{
    map &here = get_map();
    const bool already_on_fire = here.has_nearby_fire( examp, 0 );
    const bool furn_is_deployed = !here.furn( examp ).obj().deployed_item.is_empty();

    std::multimap<int, item *> firestarters;
    for( item *it : p.items_with( []( const item & it ) {
    return it.has_flag( flag_FIRESTARTER ) || it.has_flag( flag_FIRE );
    } ) ) {
        const auto usef = it->type->get_use( "firestarter" );
        if( usef != nullptr && usef->get_actor_ptr() != nullptr ) {
            const auto actor = dynamic_cast<const firestarter_actor *>( usef->get_actor_ptr() );
            if( actor->can_use( p, *it, false, examp ).success() ) {
                firestarters.insert( std::pair<int, item *>( actor->moves_cost_fast, it ) );
            }
        }
    }

    const bool has_firestarter = !firestarters.empty();
    const bool has_bionic_firestarter = p.has_bionic( bio_lighter ) &&
                                        p.enough_power_for( bio_lighter );

    uilist selection_menu;
    selection_menu.text = _( "Select an action" );
    selection_menu.addentry( 0, true, 'g', _( "Get items" ) );
    if( !already_on_fire ) {
        selection_menu.addentry( 1, has_firestarter, 'f',
                                 has_firestarter ? _( "Start a fire" ) : _( "Start a fire… you'll need a fire source." ) );
        if( has_bionic_firestarter ) {
            selection_menu.addentry( 2, true, 'b', _( "Use a CBM to start a fire" ) );
        }
    } else {
        selection_menu.addentry( 4, true, 'e', _( "Extinguish fire" ) );
    }
    if( furn_is_deployed ) {
        selection_menu.addentry( 3, true, 't', _( "Take down the %s" ), here.furnname( examp ) );
    }
    selection_menu.query();

    switch( selection_menu.ret ) {
        case 0:
            none( p, examp );
            pickup::pick_up( examp, 0 );
            return;
        case 1: {
            for( auto &firestarter : firestarters ) {
                item *it = firestarter.second;
                const auto usef = it->type->get_use( "firestarter" );
                const auto actor = dynamic_cast<const firestarter_actor *>( usef->get_actor_ptr() );
                p.add_msg_if_player( _( "You attempt to start a fire with your %s…" ), it->tname() );
                const ret_val<bool> can_use = actor->can_use( p, *it, false, examp );
                if( can_use.success() ) {
                    const int charges = actor->use( p, *it, false, examp );
                    p.use_charges( it->typeId(), charges );
                    return;
                } else {
                    p.add_msg_if_player( m_bad, can_use.str() );
                }
            }
            p.add_msg_if_player( _( "You weren't able to start a fire." ) );
            return;
        }
        case 2: {
            if( !here.get_field( examp, fd_fire ) && here.add_field( examp, fd_fire, 1 ) ) {
                p.mod_power_level( -bio_lighter->power_activate );
                p.mod_moves( -to_moves<int>( 1_seconds ) );
            } else {
                p.add_msg_if_player( m_info, _( "You can't light a fire there." ) );
            }
            return;
        }
        case 3: {
            if( already_on_fire ) {
                if( !query_yn( _( "Really take down the %s while it's on fire?" ), here.furnname( examp ) ) ) {
                    return;
                }
            }
            p.add_msg_if_player( m_info, _( "You take down the %s." ),
                                 here.furnname( examp ) );
            const auto furn_item = here.furn( examp ).obj().deployed_item;
            here.add_item_or_charges( examp, item::spawn( furn_item, calendar::turn ) );
            here.furn_set( examp, f_null );
            return;
        }
        case 4: {
            here.remove_field( examp, fd_fire );
            p.mod_moves( -200 );
            p.add_msg_if_player( m_info, _( "With a few determined moves you put out the fire in the %s." ),
                                 here.furnname( examp ) );
            return;
        }
        default:
            none( p, examp );
            return;
    }
}

void iexamine::fvat_empty( player &p, const tripoint &examp )
{
    itype_id brew_type;
    std::string brew_nname;
    bool to_deposit = false;
    static const auto vat_volume = units::from_liter( 50 );
    bool vat_full = false;
    bool ferment = false;
    bool brew_present = false;
    int charges_on_ground = 0;
    map &here = get_map();
    auto items = here.i_at( examp );
    for( auto item_it = items.begin(); item_it != items.end(); ) {
        //TODO!: check
        if( !( *item_it )->is_brewable() || brew_present ) {
            // This isn't a brew or there was already another kind of brew inside,
            // so this has to be moved.
            // This will add items to a space near the vat, because it's flagged as NOITEM.
            detached_ptr<item> det;
            item_it = items.erase( item_it, &det );
            items.insert( std::move( det ) );
        } else {
            item_it++;
            brew_present = true;
        }
    }
    if( !brew_present ) {
        add_msg( _( "This keg is empty." ) );
        // TODO: Allow using brews from crafting inventory
        const auto b_inv = p.items_with( []( const item & it ) {
            return it.is_brewable();
        } );
        if( b_inv.empty() ) {
            add_msg( m_info, _( "You have no brew to ferment." ) );
            return;
        }
        // Make lists of unique typeids and names for the menu
        // Code shamelessly stolen from the crop planting function!
        std::vector<itype_id> b_types;
        std::vector<std::string> b_names;
        for( auto &b : b_inv ) {
            if( std::ranges::find( b_types, b->typeId() ) == b_types.end() ) {
                b_types.push_back( b->typeId() );
                b_names.push_back( item::nname( b->typeId() ) );
            }
        }
        // Choose brew from list
        int b_index = 0;
        if( b_types.size() > 1 ) {
            b_index = uilist( _( "Use which brew?" ), b_names );
        } else { //Only one brew type was in inventory, so it's automatically used
            if( !query_yn( _( "Set %s in the vat?" ), b_names[0] ) ) {
                b_index = -1;
            }
        }
        if( b_index < 0 ) {
            return;
        }
        to_deposit = true;
        brew_type = b_types[b_index];
        brew_nname = item::nname( brew_type );
    } else {
        item &brew = here.i_at( examp ).only_item();
        brew_type = brew.typeId();
        brew_nname = item::nname( brew_type );
        charges_on_ground = brew.charges;
        add_msg( _( "This keg contains %s (%d), %0.f%% full." ),
                 brew.tname(), brew.charges, brew.volume() * 100.0 / vat_volume );
        enum options { ADD_BREW, REMOVE_BREW, START_FERMENT };
        uilist selectmenu;
        selectmenu.text = _( "Select an action" );
        selectmenu.addentry( ADD_BREW, ( p.charges_of( brew_type ) > 0 ), MENU_AUTOASSIGN,
                             _( "Add more %s to the vat" ), brew_nname );
        selectmenu.addentry( REMOVE_BREW, brew.made_of( LIQUID ), MENU_AUTOASSIGN,
                             _( "Remove %s from the vat" ), brew.tname() );
        selectmenu.addentry( START_FERMENT, true, MENU_AUTOASSIGN, _( "Start fermenting cycle" ) );
        selectmenu.query();
        switch( selectmenu.ret ) {
            case ADD_BREW: {
                to_deposit = true;
                break;
            }
            case REMOVE_BREW: {
                liquid_handler::handle_liquid( **here.i_at( examp ).begin() );
                return;
            }
            case START_FERMENT: {
                ferment = true;
                break;
            }
            default:
                add_msg( _( "Never mind." ) );
                return;
        }
    }
    if( to_deposit ) {
        detached_ptr<item> brew = item::spawn( brew_type, calendar::start_of_cataclysm );
        int charges_held = p.charges_of( brew_type );
        brew->charges = charges_on_ground;
        for( int i = 0; i < charges_held && !vat_full; i++ ) {
            p.use_charges( brew_type, 1 );
            brew->charges++;
            if( brew->volume() >= vat_volume ) {
                vat_full = true;
            }
        }
        add_msg( _( "Set %s in the vat." ), brew_nname );
        add_msg( _( "The keg now contains %s (%d), %0.f%% full." ),
                 brew->tname(), brew->charges, brew->volume() * 100.0 / vat_volume );
        here.i_clear( examp );
        //This is needed to bypass NOITEM
        here.add_item( examp, std::move( brew ) );
        p.moves -= to_moves<int>( 20_seconds );
        if( !vat_full ) {
            ferment = query_yn( _( "Start fermenting cycle?" ) );
        }
    }
    if( vat_full || ferment ) {
        here.i_at( examp ).only_item().set_age( 0_turns );
        here.furn_set( examp, f_fvat_full );
        if( vat_full ) {
            add_msg( _( "The vat is full, so you close the lid and start the fermenting cycle." ) );
        } else {
            add_msg( _( "You close the lid and start the fermenting cycle." ) );
        }
    }
}

void iexamine::fvat_full( player &p, const tripoint &examp )
{
    map &here = get_map();
    map_stack items_here = here.i_at( examp );
    if( items_here.empty() ) {
        debugmsg( "fvat_full was empty!" );
        here.furn_set( examp, f_fvat_empty );
        return;
    }

    for( auto it = items_here.begin(); it != items_here.end(); ) {
        if( !( *it )->made_of( LIQUID ) ) {
            add_msg( _( "You remove %s from the vat." ), ( *it )->tname() );
            detached_ptr<item> det;
            it = here.i_rem( examp, it, &det );
            here.add_item_or_charges( p.pos(), std::move( det ) );
        } else {
            it++;
        }
    }

    if( items_here.empty() ) {
        here.furn_set( examp, f_fvat_empty );
        return;
    }

    item &brew_i = **items_here.begin();
    // Does the vat contain unfermented brew, or already fermented booze?
    // TODO: Allow "recursive brewing" to continue without player having to check on it
    if( brew_i.is_brewable() ) {
        add_msg( _( "There's a vat of %s set to ferment there." ), brew_i.tname() );

        // TODO: change brew_time to return time_duration
        const time_duration brew_time = brew_i.brewing_time();
        const time_duration progress = brew_i.age();
        if( progress < brew_time ) {
            int hours = to_hours<int>( brew_time - progress );
            if( hours < 1 ) {
                add_msg( _( "It will finish brewing in less than an hour." ) );
            } else {
                add_msg( vgettext( "It will finish brewing in about %d hour.",
                                   "It will finish brewing in about %d hours.",
                                   hours ), hours );
            }
            return;
        }

        if( query_yn( _( "Finish brewing?" ) ) ) {
            const auto results = brew_i.brewing_results();

            here.i_clear( examp );
            for( const auto &result : results ) {
                // TODO: Different age based on settings
                detached_ptr<item> booze = item::spawn( result, brew_i.birthday(), brew_i.charges );
                if( booze->made_of( LIQUID ) ) {
                    add_msg( _( "The %s is now ready for bottling." ), booze->tname() );
                }
                here.add_item( examp, std::move( booze ) );
            }

            p.moves -= to_moves<int>( 5_seconds );
            p.practice( skill_cooking, std::min( to_minutes<int>( brew_time ) / 10, 100 ) );
        }

        return;
    } else {
        add_msg( _( "There's a vat of fermented %s there." ), brew_i.tname() );
    }

    const std::string booze_name = brew_i.tname();
    liquid_handler::handle_liquid( **items_here.begin() );
    if( items_here.empty() ) {
        add_msg( _( "You squeeze the last drops of %s from the vat." ), booze_name );
        here.furn_set( examp, f_fvat_empty );
        return;
    }
}

static units::volume get_keg_capacity( const tripoint &pos )
{
    const furn_t &furn = get_map().furn( pos ).obj();
    return furn.keg_capacity;
}

/**
 * Check whether there is a keg on the map that can be filled via @ref pour_into_keg.
 */
bool iexamine::has_keg( const tripoint &pos )
{
    return get_keg_capacity( pos ) > 0_ml;
}

static void displace_items_except_one_liquid( const tripoint &examp )
{
    map &here = get_map();
    // Temporarily replace the real furniture with a fake furniture with NOITEM
    const furn_id previous_furn = here.furn( examp );
    here.furn_set( examp, furn_id( "f_no_item" ) );

    bool liquid_present = false;
    map_stack items = here.i_at( examp );
    for( map_stack::iterator it = items.begin(); it != items.end(); ) {
        //TODO!:check
        if( !( *it )->made_of( LIQUID ) || liquid_present ) {
            // This isn't a liquid or there was already another kind of liquid inside,
            // so this has to be moved.
            // This will add items to a space near the vat, because it's flagged as NOITEM.
            detached_ptr<item> det;
            it = items.erase( it, &det );
            items.insert( std::move( det ) );
        } else {
            it++;
            liquid_present = true;
        }
    }

    // Replace the real furniture
    here.furn_set( examp, previous_furn );
}

void iexamine::keg( player &p, const tripoint &examp )
{
    none( p, examp );
    map &here = get_map();
    const auto keg_name = here.name( examp );
    units::volume keg_cap = get_keg_capacity( examp );

    const bool has_container_with_liquid = map_cursor( examp ).has_item_with( []( const item & it ) {
        return !it.is_container_empty() && it.can_unload_liquid();
    } );
    const bool liquid_present = map_cursor( examp ).has_item_with( []( const item & it ) {
        return it.made_of( LIQUID );
    } );

    if( !liquid_present || has_container_with_liquid ) {
        add_msg( m_info, _( "It is empty." ) );
        // Get list of all drinks
        auto drinks_inv = p.items_with( []( const item & it ) {
            return it.made_of( LIQUID );
        } );
        if( drinks_inv.empty() ) {
            add_msg( m_info, _( "You don't have any drinks to fill the %s with." ), keg_name );
            return;
        }
        // Make lists of unique drinks... about third time we do this, maybe we ought to make a function next time
        std::vector<itype_id> drink_types;
        std::vector<std::string> drink_names;
        std::vector<double> drink_rot;
        for( auto &drink : drinks_inv ) {
            auto found_drink = std::ranges::find( drink_types, drink->typeId() );
            if( found_drink == drink_types.end() ) {
                drink_types.push_back( drink->typeId() );
                drink_names.push_back( item::nname( drink->typeId() ) );
                drink_rot.push_back( drink->get_relative_rot() );
            } else {
                auto rot_iter = std::next( drink_rot.begin(), std::distance( drink_types.begin(), found_drink ) );
                // Yep, worst rot wins.
                *rot_iter = std::max( *rot_iter, drink->get_relative_rot() );
            }
        }
        // Choose drink to store in keg from list
        int drink_index = 0;
        if( drink_types.size() > 1 ) {
            drink_index = uilist( _( "Store which drink?" ), drink_names );
            if( drink_index < 0 || static_cast<size_t>( drink_index ) >= drink_types.size() ) {
                drink_index = -1;
            }
        } else { //Only one drink type was in inventory, so it's automatically used
            if( !query_yn( _( "Fill the %1$s with %2$s?" ),
                           keg_name, drink_names[0].c_str() ) ) {
                drink_index = -1;
            }
        }
        if( drink_index < 0 ) {
            return;
        }

        // First empty the keg of foreign objects
        displace_items_except_one_liquid( examp );

        //Store liquid chosen in the keg
        itype_id drink_type = drink_types[ drink_index ];
        int charges_held = p.charges_of( drink_type );
        detached_ptr<item> drink = item::spawn( drink_type, calendar::start_of_cataclysm );
        drink->set_relative_rot( drink_rot[ drink_index ] );
        drink->charges = 0;
        bool keg_full = false;
        for( int i = 0; i < charges_held && !keg_full; i++ ) {
            g->u.use_charges( drink->typeId(), 1 );
            drink->charges++;
            keg_full = drink->volume() >= keg_cap;
        }
        if( keg_full ) {
            add_msg( _( "You completely fill the %1$s with %2$s." ),
                     keg_name, item::nname( drink_type ) );
        } else {
            add_msg( _( "You fill the %1$s with %2$s." ),
                     keg_name, item::nname( drink_type ) );
        }
        p.moves -= to_moves<int>( 10_seconds );
        here.i_clear( examp );
        here.add_item( examp, std::move( drink ) );
        return;
    } else {
        // First empty the keg of foreign objects
        displace_items_except_one_liquid( examp );

        map_stack items = here.i_at( examp );
        item &drink = items.only_item();
        const std::string drink_tname = drink.tname();
        const std::string drink_nname = item::nname( drink.typeId() );
        enum options {
            DISPENSE,
            HAVE_A_DRINK,
            REFILL,
            EXAMINE,
        };
        uilist selectmenu;
        selectmenu.addentry( DISPENSE, drink.made_of( LIQUID ), MENU_AUTOASSIGN,
                             _( "Dispense or dump %s" ), drink_tname );
        selectmenu.addentry( HAVE_A_DRINK, drink.is_food() && drink.made_of( LIQUID ),
                             MENU_AUTOASSIGN, _( "Have a drink" ) );
        selectmenu.addentry( REFILL, true, MENU_AUTOASSIGN, _( "Refill" ) );
        selectmenu.addentry( EXAMINE, true, MENU_AUTOASSIGN, _( "Examine" ) );

        selectmenu.text = _( "Select an action" );
        selectmenu.query();

        switch( selectmenu.ret ) {
            case DISPENSE:
                if( liquid_handler::handle_liquid( **items.begin() ) ) {
                    add_msg( _( "You squeeze the last drops of %1$s from the %2$s." ),
                             drink_tname, keg_name );
                }
                return;

            case HAVE_A_DRINK:
                if( !p.eat( drink ) ) {
                    return; // They didn't actually drink
                }

                if( drink.charges == 0 ) {
                    add_msg( _( "You squeeze the last drops of %1$s from the %2$s." ),
                             drink_tname, keg_name );
                    here.i_clear( examp );
                }
                p.moves -= to_moves<int>( 5_seconds );
                return;

            case REFILL: {
                if( drink.volume() >= keg_cap ) {
                    add_msg( _( "The %s is completely full." ), keg_name );
                    return;
                }
                int charges_held = p.charges_of( drink.typeId() );
                if( charges_held < 1 ) {
                    add_msg( m_info, _( "You don't have any %1$s to fill the %2$s with." ),
                             drink_nname, keg_name );
                    return;
                }
                detached_ptr<item> tmp = item::spawn( drink.typeId(), calendar::turn, charges_held );
                tmp = pour_into_keg( examp, std::move( tmp ) );
                p.use_charges( drink.typeId(), charges_held - tmp->charges );
                add_msg( _( "You fill the %1$s with %2$s." ), keg_name, drink_nname );
                p.moves -= to_moves<int>( 10_seconds );
                return;
            }

            case EXAMINE: {
                add_msg( m_info, _( "It contains %s (%d), %0.f%% full." ),
                         drink_tname, drink.charges, drink.volume() * 100.0 / keg_cap );
                return;
            }

            default:
                return;
        }
    }
}

/**
 * Pour liquid into a keg (furniture) on the map. The transferred charges (if any)
 * will be removed from the liquid item.
 * @return Any remaining liquid.
 */
detached_ptr<item> iexamine::pour_into_keg( const tripoint &pos, detached_ptr<item> &&liquid )
{
    const units::volume keg_cap = get_keg_capacity( pos );
    if( keg_cap <= 0_ml ) {
        return std::move( liquid );
    }
    map &here = get_map();
    const auto keg_name = here.name( pos );
    item &obj = *liquid;

    map_stack stack = here.i_at( pos );
    if( stack.empty() ) {
        int charges = liquid->charges;
        here.add_item( pos, std::move( liquid ) );
        obj.charges = 0;
        while( charges > 0 && obj.volume() < keg_cap ) {
            obj.charges++;
            charges--;
        }
        add_msg( _( "You pour %1$s into the %2$s." ), obj.tname(), keg_name );
        if( charges > 0 ) {
            detached_ptr<item> ret = item::spawn( obj );
            ret->charges = charges;
            return ret;
        }
        return detached_ptr<item>();
    } else if( stack.only_item().typeId() != liquid->typeId() ) {
        add_msg( _( "The %s already contains some %s, you can't add a different liquid to it." ),
                 keg_name, item::nname( stack.only_item().typeId() ) );
        return std::move( liquid );
    } else {
        item &drink = stack.only_item();
        if( drink.volume() >= keg_cap ) {
            add_msg( _( "The %s is full." ), keg_name );
            return std::move( liquid );
        }
        while( liquid->charges > 0 && drink.volume() < keg_cap ) {
            drink.charges++;
            liquid->charges--;
        }
        add_msg( _( "You pour %1$s into the %2$s." ), obj.tname(), keg_name );
        if( liquid->charges == 0 ) {
            return detached_ptr<item>();
        }
    }

    return std::move( liquid );
}

static void pick_plant( player &p, const tripoint &examp,
                        const itype_id &itemType, ter_id new_ter, bool seeds = false )
{
    map &here = get_map();
    bool auto_forage = get_option<bool>( "AUTO_FEATURES" ) &&
                       get_option<std::string>( "AUTO_FORAGING" ) != "off";
    if( p.is_player() && !auto_forage &&
        !query_yn( _( "Harvest the %s?" ), here.tername( examp ) ) ) {
        iexamine::none( p, examp );
        return;
    }

    const int survival = p.get_skill_level( skill_survival );
    p.practice( skill_survival, 6 );

    int plantBase = rng( 2, 5 );
    ///\EFFECT_SURVIVAL increases number of plants harvested
    int plantCount = rng( plantBase, plantBase + survival / 2 );
    plantCount = std::min( plantCount, 12 );

    here.spawn_item( p.pos(), itemType, plantCount, 0, calendar::turn );

    if( seeds ) {
        // FIXME: shouldn't derive seed type by string manipulation
        here.spawn_item( p.pos(), itype_id( "seed_" + itemType.str() ), 1,
                         rng( plantCount / 4, plantCount / 2 ), calendar::turn );
    }

    here.ter_set( examp, new_ter );
}

void iexamine::tree_hickory( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( harvest_common( p, examp, false, false ) ) {
        here.ter_set( examp, here.get_ter_transforms_into( examp ) );
    }
    if( !p.has_quality( qual_DIG ) ) {
        p.add_msg_if_player( m_info, _( "You have no tool to dig with…" ) );
        return;
    }
    if( p.is_player() &&
        !query_yn( _( "Dig up %s?  This kills the tree!" ), here.tername( examp ) ) ) {
        return;
    }

    ///\EFFECT_SURVIVAL increases hickory root number per tree
    here.spawn_item( p.pos(), itype_hickory_root, rng( 1, 3 + p.get_skill_level( skill_survival ) ), 0,
                     calendar::turn );
    here.ter_set( examp, t_tree_hickory_dead );
    ///\EFFECT_SURVIVAL speeds up hickory root digging
    p.moves -= to_moves<int>( 20_seconds ) / ( p.get_skill_level( skill_survival ) + 1 ) + 100;
}

static item *maple_tree_sap_container()
{
    const item &maple_sap = *item::spawn_temporary( itype_maple_sap, calendar::start_of_cataclysm );
    return g->inv_map_splice( [&]( const item & it ) {
        return it.get_remaining_capacity_for_liquid( maple_sap, true ) > 0;
    }, _( "Which container?" ), PICKUP_RANGE );
}

void iexamine::tree_maple( player &p, const tripoint &examp )
{
    if( !p.has_quality( quality_id( "DRILL" ) ) ) {
        add_msg( m_info, _( "You need a tool to drill the crust to tap this maple tree." ) );
        return;
    }

    if( !p.has_quality( quality_id( "HAMMER" ) ) ) {
        add_msg( m_info,
                 _( "You need a tool to hammer the spile into the crust to tap this maple tree." ) );
        return;
    }

    const inventory &crafting_inv = p.crafting_inventory();

    if( !crafting_inv.has_amount( itype_tree_spile, 1 ) ) {
        add_msg( m_info, _( "You need a %s to tap this maple tree." ),
                 item::nname( itype_tree_spile ) );
        return;
    }

    std::vector<item_comp> comps;
    comps.emplace_back( itype_tree_spile, 1 );
    p.consume_items( comps, 1, is_crafting_component );

    p.mod_moves( -to_moves<int>( 20_seconds ) );
    map &here = get_map();
    here.ter_set( examp, t_tree_maple_tapped );

    item *container = maple_tree_sap_container();
    if( container ) {
        here.add_item_or_charges( examp, container->detach(), false );
    } else {
        add_msg( m_info, _( "No container added.  The sap will just spill on the ground." ) );
    }
}

void iexamine::tree_maple_tapped( player &p, const tripoint &examp )
{
    bool has_sap = false;
    item *container = nullptr;
    int charges = 0;

    const std::string maple_sap_name = item::nname( itype_maple_sap );

    map &here = get_map();
    map_stack items = here.i_at( examp );
    if( !items.empty() ) {
        item &it = items.only_item();
        if( it.is_bucket() || it.is_watertight_container() ) {
            container = &it;

            if( !it.is_container_empty() && it.contents.front().typeId() == itype_maple_sap ) {
                has_sap = true;
                charges = it.contents.front().charges;
            }
        }
    }

    enum options {
        REMOVE_TAP,
        ADD_CONTAINER,
        HARVEST_SAP,
        REMOVE_CONTAINER,
    };
    uilist selectmenu;
    selectmenu.addentry( REMOVE_TAP, true, MENU_AUTOASSIGN, _( "Remove tap" ) );
    selectmenu.addentry( ADD_CONTAINER, !container, MENU_AUTOASSIGN,
                         _( "Add a container to receive the %s" ), maple_sap_name );
    selectmenu.addentry( HARVEST_SAP, has_sap, MENU_AUTOASSIGN, _( "Harvest current %s (%d)" ),
                         maple_sap_name, charges );
    selectmenu.addentry( REMOVE_CONTAINER, container, MENU_AUTOASSIGN, _( "Remove container" ) );

    selectmenu.text = _( "Select an action" );
    selectmenu.query();

    switch( selectmenu.ret ) {
        case REMOVE_TAP: {
            if( !p.has_quality( quality_id( "HAMMER" ) ) ) {
                add_msg( m_info, _( "You need a hammering tool to remove the spile from the crust." ) );
                return;
            }

            detached_ptr<item> tree_spile = item::spawn( "tree_spile" );
            add_msg( _( "You remove the %s." ), tree_spile->tname( 1 ) );
            here.add_item_or_charges( p.pos(), std::move( tree_spile ) );

            if( container ) {
                here.add_item_or_charges( p.pos(), container->detach() );
            }
            here.i_clear( examp );

            p.mod_moves( -to_moves<int>( 20_seconds ) );
            here.ter_set( examp, t_tree_maple );

            return;
        }

        case ADD_CONTAINER: {
            container = maple_tree_sap_container();
            if( container ) {
                here.add_item_or_charges( examp, container->detach(), false );

            } else {
                add_msg( m_info, _( "No container added.  The sap will just spill on the ground." ) );
            }

            return;
        }

        case HARVEST_SAP: {
            liquid_handler::handle_liquid( container->contents.front(), PICKUP_RANGE );
            return;
        }

        case REMOVE_CONTAINER: {
            g->u.assign_activity( std::make_unique<player_activity>( std::make_unique<pickup_activity_actor>(
            std::vector<pickup::pick_drop_selection> { { container, std::nullopt, {} } }, g->u.pos() ) ) );
            return;
        }

        default:
            return;
    }
}

void iexamine::shrub_marloss( player &p, const tripoint &examp )
{
    if( p.has_trait( trait_THRESH_MYCUS ) ) {
        pick_plant( p, examp, itype_mycus_fruit, t_shrub_fungal );
    } else if( p.has_trait( trait_THRESH_MARLOSS ) ) {
        map &here = get_map();
        here.spawn_item( p.pos(), itype_mycus_fruit, 1, 0, calendar::turn );
        here.ter_set( examp, t_fungus );
        add_msg( m_info, _( "The shrub offers up a fruit, then crumbles into a fungal bed." ) );
    } else {
        pick_plant( p, examp, itype_marloss_berry, t_shrub_fungal );
    }
}

void iexamine::tree_marloss( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( p.has_trait( trait_THRESH_MYCUS ) ) {
        pick_plant( p, examp, itype_mycus_fruit, t_tree_fungal );
        if( p.has_trait( trait_M_DEPENDENT ) && one_in( 3 ) ) {
            // Folks have a better shot at keeping fed.
            add_msg( m_info,
                     _( "We have located a particularly vital nutrient deposit underneath this location." ) );
            add_msg( m_good, _( "Additional nourishment is available." ) );
            here.ter_set( examp, t_marloss_tree );
        }
    } else if( p.has_trait( trait_THRESH_MARLOSS ) ) {
        here.spawn_item( p.pos(), itype_mycus_fruit, 1, 0, calendar::turn );
        here.ter_set( examp, t_tree_fungal );
        add_msg( m_info, _( "The tree offers up a fruit, then shrivels into a fungal tree." ) );
    } else {
        pick_plant( p, examp, itype_marloss_berry, t_tree_fungal );
    }
}

void iexamine::shrub_wildveggies( player &p, const tripoint &examp )
{
    map &here = get_map();
    // Ask if there's something possibly more interesting than this shrub here
    if( ( !here.i_at( examp ).empty() ||
          here.veh_at( examp ) ||
          !here.tr_at( examp ).is_null() ||
          g->critter_at( examp ) != nullptr ) &&
        !query_yn( _( "Forage through %s?" ), here.tername( examp ) ) ) {
        none( p, examp );
        return;
    }

    add_msg( _( "You forage through the %s." ), here.tername( examp ) );
    ///\EFFECT_SURVIVAL speeds up foraging
    int move_cost = 100000 / ( 2 * p.get_skill_level( skill_survival ) + 5 );
    ///\EFFECT_PER randomly speeds up foraging
    move_cost /= rng( std::max( 4, p.per_cur ), 4 + p.per_cur * 2 );
    p.assign_activity( ACT_FORAGE, move_cost, 0 );
    p.activity->placement = here.getabs( examp );
    p.activity->auto_resume = true;
    return;
}

void iexamine::recycle_compactor( player &, const tripoint &examp )
{
    // choose what metal to recycle
    auto metals = materials::get_compactable();
    uilist choose_metal;
    choose_metal.text = _( "Recycle what metal?" );
    for( auto &m : metals ) {
        choose_metal.addentry( m.name() );
    }
    choose_metal.query();
    int m_idx = choose_metal.ret;
    if( m_idx < 0 || m_idx >= static_cast<int>( metals.size() ) ) {
        add_msg( _( "Never mind." ) );
        return;
    }
    material_type m = metals.at( m_idx );

    map &here = get_map();
    // check inputs and tally total mass
    auto inputs = here.i_at( examp );
    units::mass sum_weight = 0_gram;
    auto ca = m.compact_accepts();
    std::set<material_id> accepts( ca.begin(), ca.end() );
    accepts.insert( m.id );
    for( auto &input : inputs ) {
        if( !input->only_made_of( accepts ) ) {
            //~ %1$s: an item in the compactor , %2$s: desired compactor output material
            add_msg( _( "You realize this isn't going to work because %1$s is not made purely of %2$s." ),
                     input->tname(), m.name() );
            return;
        }
        if( input->is_container() && !input->is_container_empty() ) {
            //~ %1$s: an item in the compactor
            add_msg( _( "You realize this isn't going to work because %1$s has not been emptied of its contents." ),
                     input->tname() );
            return;
        }
        sum_weight += input->weight();
    }
    if( sum_weight <= 0_gram ) {
        //~ %1$s: desired compactor output material
        add_msg( _( "There is no %1$s in the compactor.  Drop some metal items onto it and try again." ),
                 m.name() );
        return;
    }

    // See below for recover_factor (rng(6,9)/10), this
    // is the normal value of that recover factor.
    static const double norm_recover_factor = 8.0 / 10.0;
    const units::mass norm_recover_weight = sum_weight * norm_recover_factor;

    // choose output
    uilist choose_output;
    //~ %1$.3f: total mass of material in compactor, %2$s: weight units , %3$s: compactor output material
    choose_output.text = string_format( _( "Compact %1$.3f %2$s of %3$s into:" ),
                                        convert_weight( sum_weight ), weight_units(), m.name() );
    for( auto &ci : m.compacts_into() ) {
        //TODO!: check
        auto it = item::spawn_temporary( ci, calendar::start_of_cataclysm, item::solitary_tag{} );
        const int amount = norm_recover_weight / it->weight();
        //~ %1$d: number of, %2$s: output item
        choose_output.addentry( string_format( _( "about %1$d %2$s" ), amount,
                                               it->tname( amount ) ) );
    }
    choose_output.query();
    int o_idx = choose_output.ret;
    if( o_idx < 0 || o_idx >= static_cast<int>( m.compacts_into().size() ) ) {
        add_msg( _( "Never mind." ) );
        return;
    }

    // remove items
    for( auto it = inputs.begin(); it != inputs.end(); ) {
        it = inputs.erase( it );
    }

    // produce outputs
    double recover_factor = rng( 6, 9 ) / 10.0;
    sum_weight = sum_weight * recover_factor;
    sounds::sound( examp, 80, sounds::sound_t::combat, _( "Ka-klunk!" ), true, "tool", "compactor" );
    bool out_desired = false;
    bool out_any = false;
    for( auto it = m.compacts_into().begin() + o_idx; it != m.compacts_into().end(); ++it ) {
        const units::mass ow = item::spawn_temporary( *it, calendar::start_of_cataclysm, item::solitary_tag{} )->weight();
        int count = sum_weight / ow;
        sum_weight -= count * ow;
        if( count > 0 ) {
            here.spawn_item( examp, *it, count, 1, calendar::turn );
            if( !out_any ) {
                out_any = true;
                if( it == m.compacts_into().begin() + o_idx ) {
                    out_desired = true;
                }
            }
        }
    }

    // feedback to user
    if( !out_any ) {
        add_msg( _( "The compactor chews up all the items in its hopper." ) );
        //~ %1$s: compactor output material
        add_msg( _( "The compactor beeps: \"No %1$s to process!\"" ), m.name() );
        return;
    }
    if( !out_desired ) {
        //~ %1$s: compactor output material
        add_msg( _( "The compactor beeps: \"Insufficient %1$s!\"" ), m.name() );
        add_msg( _( "It spits out an assortment of smaller pieces instead." ) );
    }
}

void iexamine::trap( player &p, const tripoint &examp )
{
    map &here = get_map();
    const auto &tr = here.tr_at( examp );
    if( !p.is_player() || tr.is_null() ) {
        return;
    }
    const int possible = tr.get_difficulty();
    bool seen = tr.can_see( examp, p );
    if( tr.loadid == tr_unfinished_construction || here.partial_con_at( examp ) ) {
        partial_con *pc = here.partial_con_at( examp );
        if( pc ) {
            const construction &built = pc->id.obj();
            if( !character_funcs::can_see_fine_details( p ) && !built.dark_craftable &&
                !p.has_trait( trait_DEBUG_HS ) ) {
                add_msg( m_info, _( "It is too dark to construct right now." ) );
                return;
            }
            if( !query_yn( _( "Unfinished task: %s, %d%% complete here, continue construction?" ),
                           built.group->name(), pc->counter / 100000 ) ) {
                if( query_yn( _( "Cancel construction?" ) ) ) {
                    here.disarm_trap( examp );
                    for( detached_ptr<item> &it : pc->components.clear() ) {
                        here.add_item_or_charges( p.pos(), std::move( it ) );
                    }
                    here.partial_con_remove( examp );
                    return;
                } else {
                    return;
                }
            } else {
                p.assign_activity( std::make_unique<player_activity>( std::make_unique<construction_activity_actor>
                                   ( here.getglobal( examp ) ) ) );
                return;
            }
        } else {
            return;
        }
    }
    if( seen && possible >= 99 ) {
        add_msg( m_info, _( "That %s looks too dangerous to mess with.  Best leave it alone." ),
                 tr.name() );
        return;
    }
    // Some traps are not actual traps. Those should get a different query.
    if( seen && possible == 0 &&
        tr.get_avoidance() == 0 ) { // Separated so saying no doesn't trigger the other query.
        if( query_yn( _( "There is a %s there.  Take down?" ), tr.name() ) ) {
            here.disarm_trap( examp );
        }
    } else if( seen && query_yn( _( "There is a %s there.  Disarm?" ), tr.name() ) ) {
        here.disarm_trap( examp );
    }
}
//Note that these three functions are checked by pointer in map::water_from. Yes it's awful.
void iexamine::water_source( player &, const tripoint &examp )
{
    // Routed to map::water_from to handle poison
    liquid_handler::handle_liquid( examp );
}

//Note that these three functions are checked by pointer in map::water_from. Yes it's awful.
void iexamine::clean_water_source( player &, const tripoint & )
{
    liquid_handler::handle_liquid( item::spawn( "water_clean", calendar::start_of_cataclysm,
                                   item::INFINITE_CHARGES ) );
}

//Note that these three functions are checked by pointer in map::water_from. Yes it's awful.
void iexamine::liquid_source( player &, const tripoint &examp )
{
    liquid_handler::handle_liquid( item::spawn( get_map().furn( examp ).obj().provides_liquids,
                                   calendar::turn, item::INFINITE_CHARGES ) );
}

std::vector<itype> furn_t::crafting_pseudo_item_types() const
{
    std::vector<itype> conversion;
    conversion.reserve( crafting_pseudo_items.size() );
    for( const itype_id &itid : crafting_pseudo_items ) {
        conversion.push_back( *itid );
    }
    return conversion;
}

std::vector<itype> furn_t::crafting_ammo_item_types() const
{
    const std::vector<itype> pseudo_list = crafting_pseudo_item_types();

    std::vector<itype> pseudo_ammo_list;
    if( !pseudo_list.empty() ) {
        for( const itype &pseudo : pseudo_list ) {
            if( pseudo.tool && !pseudo.tool->ammo_id.empty() ) {
                const itype_id &iid = ammotype( *pseudo.tool->ammo_id.begin() )->default_ammotype();
                pseudo_ammo_list.push_back( *iid );
            }
        }
    }
    return pseudo_ammo_list;
}

static int count_charges_in_list( const itype *type, const map_stack &items )
{
    for( const auto &candidate : items ) {
        if( candidate->type == type ) {
            return candidate->charges;
        }
    }
    return 0;
}

void iexamine::reload_furniture( player &p, const tripoint &examp )
{
    map &here = get_map();
    const furn_t &f = here.furn( examp ).obj();
    const std::vector<itype> ammo_types = f.crafting_ammo_item_types();

    // No fake item, no ammo, something is wrong here.
    if( ammo_types.empty() ) {
        debugmsg( ( "The %s has no crafting_pseudo_items or no defined ammo!" ), f.name() );
        return;
    }

    map_stack items_here = here.i_at( examp );
    std::vector<std::string> ammo_names;
    std::vector<itype> ammo_filtered;
    int ammo_index = 0;

    for( const itype &at : ammo_types ) {
        if( at.get_id() != itype_battery ) {
            ammo_names.emplace_back( at.nname( 1 ) );
            ammo_filtered.emplace_back( at );
        }
    }

    if( ammo_filtered.empty() ) {
        debugmsg( "Furniture %s has no valid ammo to reload!", f.name() );
        return;
    } else if( ammo_names.size() > 1 ) {
        ammo_index = uilist( _( "What would you like to change?" ), ammo_names );
        if( ammo_index < 0 || static_cast<size_t>( ammo_index ) >= ammo_names.size() ) {
            ammo_index = -1;
        }
    }
    if( ammo_index < 0 ) {
        return;
    }

    // HACK: Yes I know I'm converting from itype to itype_id and back again into a pointer.
    // It seems to be needed to make the appropriate connections?
    // Otherwise the part where it queries for the amount doesn't seem to work right.
    const itype *cur_ammo = &*ammo_types.at( ammo_index ).get_id();
    const int amount_in_furn = count_charges_in_list( cur_ammo, items_here );
    const int amount_in_inv = p.charges_of( cur_ammo->get_id() );
    if( amount_in_furn > 0 ) {
        if( p.query_yn( _( "The %1$s contains %2$d %3$s.  Unload?" ), f.name(), amount_in_furn,
                        cur_ammo->nname( amount_in_furn ) ) ) {
            auto items = here.i_at( examp );
            for( auto &itm : items ) {
                if( itm->type == cur_ammo ) {
                    g->u.assign_activity( std::make_unique<player_activity>( std::make_unique<pickup_activity_actor>(
                    std::vector<pickup::pick_drop_selection> { { itm, std::nullopt, {} } }, g->u.pos() ) ) );
                    return;
                }
            }
        }
    }

    // Note that this can lead to hammerspace as each ammo type doesn't take other items on the tile into account.
    const int max_amount_in_furn = cur_ammo->charges_per_volume( f.max_volume );
    const int max_reload_amount = max_amount_in_furn - amount_in_furn;
    if( max_reload_amount <= 0 ) {
        return;
    }
    if( amount_in_inv == 0 ) {
        //~ Reloading or restocking a piece of furniture, for example a forge.
        add_msg( m_info, _( "You need some %1$s to reload this %2$s." ),
                 cur_ammo->nname( 2 ),
                 f.name() );
        return;
    }
    const int max_amount = std::min( amount_in_inv, max_reload_amount );
    //~ Loading fuel or other items into a piece of furniture.
    const std::string popupmsg = string_format( _( "Put how many of the %1$s into the %2$s?" ),
                                 cur_ammo->nname( max_amount ), f.name() );
    int amount = string_input_popup()
                 .title( popupmsg )
                 .width( 20 )
                 .text( std::to_string( max_amount ) )
                 .only_digits( true )
                 .query_int();
    if( amount <= 0 || amount > max_amount ) {
        return;
    }
    p.use_charges( cur_ammo->get_id(), amount );
    auto items = here.i_at( examp );
    for( auto &itm : items ) {
        if( itm->type == cur_ammo ) {
            itm->charges += amount;
            amount = 0;
            break;
        }
    }
    if( amount != 0 ) {
        here.add_item( examp, item::spawn( cur_ammo->get_id(), calendar::turn, amount ) );
    }

    const int amount_in_furn_after_placing = count_charges_in_list( &ammo_types.at( ammo_index ),
            items );
    //~ %1$s - furniture, %2$d - number, %3$s items.
    add_msg( _( "The %1$s contains %2$d %3$s." ), f.name(), amount_in_furn_after_placing,
             cur_ammo->nname( amount_in_furn_after_placing ) );

    add_msg( _( "You reload the %s." ), here.furnname( examp ) );
    p.moves -= to_moves<int>( 5_seconds );
}


void iexamine::use_furn_fake_item( player &p, const tripoint &examp )
{
    map &m = get_map();
    const tripoint_abs_ms abspos( m.getabs( examp ) );

    if( !m.has_furn( examp ) ) {
        debugmsg( "lost furniture at %s", examp.to_string() );
        return;
    }
    const furn_t &furniture = m.furn( examp ).obj();
    const std::vector<itype> item_type_list = furniture.crafting_pseudo_item_types();
    std::vector<itype> usable_item_types;
    std::vector<std::string> usable_item_names;
    if( item_type_list.empty() ) {
        debugmsg(
            "Furniture ( %s ) with use_furn_fake_item does not have fake item to use.",
            furniture.id.c_str()
        );
        return;
    }
    for( const itype &itt : item_type_list ) {
        if( !itt.has_use() ) {
            continue;
        }
        usable_item_types.push_back( itt );
        usable_item_names.push_back( itt.nname( 1 ) );
    }
    if( usable_item_types.empty() ) {
        debugmsg(
            "Furniture ( %s ) with use_furn_fake_item has no fake item with uses",
            furniture.id.c_str() );
        return;
    }

    int tool_index = 0;
    if( usable_item_types.size() > 1 ) {
        tool_index = uilist( _( "Which tool do you want to use?" ), usable_item_names );
        if( tool_index < 0 || static_cast<size_t>( tool_index ) >= usable_item_types.size() ) {
            tool_index = -1;
        }
    }
    if( tool_index < 0 ) {
        return;
    }

    const itype &cur_tool = usable_item_types.at( tool_index );
    item &fake_item = *item::spawn_temporary( cur_tool.get_id(), calendar::turn, 0 );
    const itype_id ammo = fake_item.ammo_default();
    fake_item.set_flag( flag_PSEUDO );

    enum class charges_type {
        grid, ammo_from_map, none
    };

    charges_type charge_type = charges_type::none;
    fake_item.charges = 0;

    if( fake_item.has_flag( flag_USES_GRID_POWER ) ) {
        const distribution_grid &grid = get_distribution_grid_tracker().grid_at( abspos );
        fake_item.charges = grid.get_resource();
        charge_type = charges_type::grid;
    } else if( ammo != itype_id::NULL_ID() ) {
        fake_item.charges = count_charges_in_list( &*ammo, m.i_at( examp ) );
        charge_type = charges_type::ammo_from_map;
    }

    const int original_charges = fake_item.charges;
    p.invoke_item( &fake_item, examp );

    // HACK: Evil hack incoming
    activity_handlers::repair_activity_hack::patch_activity_for_furniture( *g->u.activity, examp,
            cur_tool.get_id() );

    const int discharged_ammo = original_charges - fake_item.charges;

    if( discharged_ammo == 0 ) {
        return;
    }

    switch( charge_type ) {
        case charges_type::grid: {
            distribution_grid &grid = get_distribution_grid_tracker().grid_at( abspos );
            const int remainder = grid.mod_resource( -discharged_ammo );
            if( remainder != 0 ) {
                debugmsg( "Fake item %s discharged more charges than have in grid at %s.",
                          cur_tool.get_id().c_str(), abspos.to_string() );
            }
            return;
        }
        case charges_type::ammo_from_map: {
            int by_ref = discharged_ammo;
            m.use_charges( examp, 0, ammo->get_id(), by_ref );
            if( by_ref != 0 ) {
                debugmsg( "Discharged fake item %s more ammo than %s has at %s.",
                          cur_tool.get_id().c_str(),
                          furniture.id.c_str(),
                          examp.to_string()
                        );
            }
            return;
        }
        case charges_type::none:
            debugmsg( "Somehow changed charges of fake item %s without ammo type to %d.",
                      cur_tool.get_id().c_str(),
                      fake_item.charges
                    );
            return;
    }
}


void iexamine::curtains( player &p, const tripoint &examp )
{
    map &here = get_map();
    const bool closed_window_with_curtains = here.has_flag( flag_BARRICADABLE_WINDOW_CURTAINS, examp );
    if( here.is_outside( p.pos() ) && ( here.has_flag( flag_WALL, examp ) ||
                                        closed_window_with_curtains ) ) {
        locked_object( p, examp );
        return;
    }

    const ter_id ter = here.ter( examp );

    // Peek through the curtains, or tear them down.
    uilist window_menu;
    window_menu.text = _( "Do what with the curtains?" );
    window_menu.addentry( 0, ( !ter.obj().close &&
                               closed_window_with_curtains ), 'p', _( "Peek through the closed curtains." ) );
    window_menu.addentry( 1, true, 't', _( "Tear down the curtains." ) );
    window_menu.query();
    const int choice = window_menu.ret;
    if( choice == 0 ) {
        // Peek
        g->peek( examp );
        p.add_msg_if_player( _( "You carefully peek through the curtains." ) );
    } else if( choice == 1 ) {
        // Mr. Gorbachev, tear down those curtains!
        if( here.ter( examp )->has_curtains() ) {
            here.ter_set( examp, here.ter( examp )->curtain_transform );
        }

        here.spawn_item( p.pos(), itype_nail, 1, 4, calendar::turn );
        here.spawn_item( p.pos(), itype_sheet, 2, 0, calendar::turn );
        here.spawn_item( p.pos(), itype_stick, 1, 0, calendar::turn );
        here.spawn_item( p.pos(), itype_string_36, 1, 0, calendar::turn );
        p.moves -= to_moves<int>( 10_seconds );
        p.add_msg_if_player( _( "You tear the curtains and curtain rod off the windowframe." ) );
    } else {
        p.add_msg_if_player( _( "Never mind." ) );
    }
}

void iexamine::sign( player &p, const tripoint &examp )
{
    map &here = get_map();
    std::string existing_signage = here.get_signage( examp );
    bool previous_signage_exists = !existing_signage.empty();

    // Display existing message, or lack thereof.
    if( p.has_trait( trait_ILLITERATE ) ) {
        popup( _( "You're illiterate, and can't read the message on the sign." ) );
    } else if( previous_signage_exists ) {
        popup( existing_signage.c_str() );
    } else {
        p.add_msg_if_player( m_neutral, _( "Nothing legible on the sign." ) );
    }

    // Allow chance to modify message.
    std::vector<tool_comp> tools;
    std::vector<item *> filter = p.crafting_inventory().items_with( []( const item & it ) {
        return it.has_flag( flag_WRITE_MESSAGE ) && it.charges > 0;
    } );
    tools.reserve( filter.size() );
    for( const item *writing_item : filter ) {
        tools.emplace_back( writing_item->typeId(), 1 );
    }

    if( !tools.empty() ) {
        // Different messages if the sign already has writing associated with it.
        std::string query_message = previous_signage_exists ?
                                    _( "Overwrite the existing message on the sign?" ) :
                                    _( "Add a message to the sign?" );
        std::string ignore_message = _( "You leave the sign alone." );
        if( query_yn( query_message ) ) {
            std::string signage = string_input_popup()
                                  .title( _( "Write what?" ) )
                                  .identifier( "signage" )
                                  .query_string();
            if( signage.empty() ) {
                p.add_msg_if_player( m_neutral, ignore_message );
            } else {
                std::string spray_painted_message = previous_signage_exists ?
                                                    _( "You overwrite the previous message on the sign with your graffiti." ) :
                                                    _( "You graffiti a message onto the sign." );
                here.set_signage( examp, signage );
                p.add_msg_if_player( m_info, spray_painted_message );
                p.mod_moves( - 20 * signage.length() );
                p.consume_tools( tools, 1 );
            }
        } else {
            p.add_msg_if_player( m_neutral, ignore_message );
        }
    }
}

static int getNearPumpCount( const tripoint &p )
{
    int result = 0;
    map &here = get_map();
    for( const tripoint &tmp : here.points_in_radius( p, 12 ) ) {
        const auto t = here.ter( tmp );
        if( t == ter_str_id( "t_gas_pump" ) || t == ter_str_id( "t_gas_pump_a" ) ) {
            result++;
        }
    }
    return result;
}

std::optional<tripoint> iexamine::getNearFilledGasTank( const tripoint &center, int &gas_units )
{
    map &here = get_map();
    std::optional<tripoint> tank_loc;
    int distance = INT_MAX;
    gas_units = 0;

    for( const tripoint &tmp : here.points_in_radius( center, SEEX * 2 ) ) {
        if( here.ter( tmp ) != ter_str_id( "t_gas_tank" ) ) {
            continue;
        }

        const int new_distance = rl_dist( center, tmp );

        if( new_distance >= distance ) {
            continue;
        }
        if( !tank_loc ) {
            // Return a potentially empty tank, but only if we don't find a closer full one.
            tank_loc.emplace( tmp );
        }
        for( auto &k : here.i_at( tmp ) ) {
            if( k->made_of( LIQUID ) ) {
                distance = new_distance;
                tank_loc.emplace( tmp );
                gas_units = k->charges;
                break;
            }
        }
    }
    return tank_loc;
}

static int getGasDiscountCardQuality( const item &it )
{
    for( const flag_id &tag : it.type->get_flags() ) {
        int discount_value;
        if( sscanf( tag->id.c_str(), "DISCOUNT_VALUE_%i", &discount_value ) == 1 ) {
            return discount_value;
        }
    }
    return 0;
}

static int findBestGasDiscount( player &p )
{
    int discount = 0;

    for( size_t i = 0; i < p.inv_size(); i++ ) {
        item &it = p.inv_find_item( i );

        if( it.has_flag( flag_GAS_DISCOUNT ) ) {

            int q = getGasDiscountCardQuality( it );
            if( q > discount ) {
                discount = q;
            }
        }
    }

    return discount;
}

static std::string str_to_illiterate_str( std::string s )
{
    if( !g->u.has_trait( trait_ILLITERATE ) ) {
        return s;
    } else {
        for( auto &i : s ) {
            i = i + rng( 0, 5 ) - rng( 0, 5 );
            if( i < ' ' ) {
                // some control character, most likely not handled correctly be the print functions
                i = ' ';
            } else if( i == '%' ) {
                // avoid characters that trigger formatting in the various print functions
                i++;
            }
        }
        return s;
    }
}

static std::string getGasDiscountName( int discount )
{
    if( discount == 3 ) {
        return str_to_illiterate_str( _( "Platinum member" ) );
    } else if( discount == 2 ) {
        return str_to_illiterate_str( _( "Gold member" ) );
    } else if( discount == 1 ) {
        return str_to_illiterate_str( _( "Silver member" ) );
    } else {
        return str_to_illiterate_str( _( "Beloved customer" ) );
    }
}

static int getGasPricePerLiter( int discount )
{
    // Those prices are in cents
    static const int prices[4] = { 1400, 1320, 1200, 1000 };
    if( discount < 0 || discount > 3 ) {
        return prices[0];
    } else {
        return prices[discount];
    }
}

std::optional<tripoint> iexamine::getGasPumpByNumber( const tripoint &p, int number )
{
    map &here = get_map();
    int k = 0;
    for( const tripoint &tmp : here.points_in_radius( p, 12 ) ) {
        const auto t = here.ter( tmp );
        if( ( t == ter_str_id( "t_gas_pump" ) || t == ter_str_id( "t_gas_pump_a" ) ) && number == k++ ) {
            return tmp;
        }
    }
    return std::nullopt;
}

bool iexamine::toPumpFuel( const tripoint &src, const tripoint &dst, int units )
{
    map &here = get_map();
    auto items = here.i_at( src );
    for( auto item_it = items.begin(); item_it != items.end(); ++item_it ) {
        item *content = *item_it;
        if( content->made_of( LIQUID ) ) {
            if( content->charges < units ) {
                return false;
            }

            content->charges -= units;

            const auto backup_pump = here.ter( dst );
            here.ter_set( dst, ter_str_id::NULL_ID() );
            here.add_item_or_charges( dst, item::spawn( content->type, calendar::turn, units ) );
            here.ter_set( dst, backup_pump );

            if( content->charges < 1 ) {
                items.erase( item_it );
            }

            return true;
        }
    }

    return false;
}

static int fromPumpFuel( const tripoint &dst, const tripoint &src )
{
    map &here = get_map();
    auto items = here.i_at( src );
    for( auto item_it = items.begin(); item_it != items.end(); ++item_it ) {
        item *content = *item_it;
        if( content->made_of( LIQUID ) ) {
            // how much do we have in the pump?

            // add the charges to the destination
            const auto backup_tank = here.ter( dst );
            here.ter_set( dst, ter_str_id::NULL_ID() );
            here.add_item_or_charges( dst, item::spawn( content->type, calendar::turn, content->charges ) );
            here.ter_set( dst, backup_tank );

            // remove the liquid from the pump
            int amount = content->charges;
            items.erase( item_it );
            return amount;
        }
    }
    return -1;
}

static void turnOnSelectedPump( const tripoint &p, int number )
{
    map &here = get_map();
    int k = 0;
    for( const tripoint &tmp : here.points_in_radius( p, 12 ) ) {
        const auto t = here.ter( tmp );
        if( t == ter_str_id( "t_gas_pump" ) || t == ter_str_id( "t_gas_pump_a" ) ) {
            if( number == k++ ) {
                here.ter_set( tmp, ter_str_id( "t_gas_pump_a" ) );
            } else {
                here.ter_set( tmp, ter_str_id( "t_gas_pump" ) );
            }
        }
    }
}

void iexamine::pay_gas( player &p, const tripoint &examp )
{

    int choice = -1;
    const int buy_gas = 1;
    const int choose_pump = 2;
    const int hack = 3;
    const int refund = 4;

    if( p.has_trait( trait_ILLITERATE ) ) {
        popup( _( "You're illiterate, and can't read the screen." ) );
    }

    int pumpCount = getNearPumpCount( examp );
    if( pumpCount == 0 ) {
        popup( str_to_illiterate_str( _( "Failure!  No gas pumps found!" ) ) );
        return;
    }

    int tankGasUnits;
    const std::optional<tripoint> pTank_ = getNearFilledGasTank( examp, tankGasUnits );
    if( !pTank_ ) {
        popup( str_to_illiterate_str( _( "Failure!  No gas tank found!" ) ) );
        return;
    }
    const tripoint pTank = *pTank_;

    if( tankGasUnits == 0 ) {
        popup( str_to_illiterate_str(
                   _( "This station is out of fuel.  We apologize for the inconvenience." ) ) );
        return;
    }

    if( uistate.ags_pay_gas_selected_pump + 1 > pumpCount ) {
        uistate.ags_pay_gas_selected_pump = 0;
    }

    int discount = findBestGasDiscount( p );
    std::string discountName = getGasDiscountName( discount );

    int pricePerUnit = getGasPricePerLiter( discount );

    bool can_hack = ( !p.has_trait( trait_ILLITERATE ) &&
                      ( ( p.has_charges( itype_electrohack, 25 ) ) ||
                        ( p.has_bionic( bio_fingerhack ) && p.get_power_level() > 24_kJ ) ) );

    uilist amenu;
    amenu.selected = 1;
    amenu.text = str_to_illiterate_str( _( "Welcome to AutoGas!" ) );
    amenu.addentry( 0, false, -1, str_to_illiterate_str( _( "What would you like to do?" ) ) );

    amenu.addentry( buy_gas, true, 'b', str_to_illiterate_str( _( "Buy gas." ) ) );
    amenu.addentry( refund, true, 'r', str_to_illiterate_str( _( "Refund cash." ) ) );

    std::string gaspumpselected = str_to_illiterate_str( _( "Current gas pump: " ) ) +
                                  std::to_string( uistate.ags_pay_gas_selected_pump + 1 );
    amenu.addentry( 0, false, -1, gaspumpselected );
    amenu.addentry( choose_pump, true, 'p', str_to_illiterate_str( _( "Choose a gas pump." ) ) );

    amenu.addentry( 0, false, -1, str_to_illiterate_str( _( "Your discount: " ) ) + discountName );
    amenu.addentry( 0, false, -1, str_to_illiterate_str( _( "Your price per gasoline unit: " ) ) +
                    format_money( pricePerUnit ) );

    if( can_hack ) {
        amenu.addentry( hack, true, 'h', _( "Hack console." ) );
    }

    amenu.query();
    choice = amenu.ret;

    if( choose_pump == choice ) {
        uilist amenu;
        amenu.selected = uistate.ags_pay_gas_selected_pump;
        amenu.text = str_to_illiterate_str( _( "Please choose gas pump:" ) );

        for( int i = 0; i < pumpCount; i++ ) {
            amenu.addentry( i, true, -1,
                            str_to_illiterate_str( _( "Pump " ) ) + std::to_string( i + 1 ) );
        }
        amenu.query();
        choice = amenu.ret;

        if( choice < 0 ) {
            return;
        }

        uistate.ags_pay_gas_selected_pump = choice;

        turnOnSelectedPump( examp, uistate.ags_pay_gas_selected_pump );

        return;

    }

    if( buy_gas == choice ) {
        int money = p.charges_of( itype_cash_card );

        if( money < pricePerUnit ) {
            popup( str_to_illiterate_str(
                       _( "Not enough money, please refill your cash card." ) ) ); //or ride on a solar car, ha ha ha
            return;
        }

        int maximum_liters = std::min( money / pricePerUnit, tankGasUnits / 1000 );

        std::string popupmsg = string_format(
                                   _( "How many liters of gasoline to buy?  Max: %d L.  (0 to cancel)" ), maximum_liters );
        int liters = string_input_popup()
                     .title( popupmsg )
                     .width( 20 )
                     .text( std::to_string( maximum_liters ) )
                     .only_digits( true )
                     .query_int();
        if( liters <= 0 ) {
            return;
        }
        if( liters > maximum_liters ) {
            liters = maximum_liters;
        }

        const std::optional<tripoint> pGasPump = getGasPumpByNumber( examp,
                uistate.ags_pay_gas_selected_pump );
        if( !pGasPump || !toPumpFuel( pTank, *pGasPump, liters * 1000 ) ) {
            return;
        }

        sounds::sound( p.pos(), 6, sounds::sound_t::activity, _( "Glug Glug Glug" ), true, "tool",
                       "gaspump" );

        int cost = liters * pricePerUnit;
        money -= cost;
        p.use_charges( itype_cash_card, cost );

        add_msg( m_info, _( "Your cash cards now hold %s." ), format_money( money ) );
        p.moves -= to_moves<int>( 5_seconds );
        return;
    }

    if( hack == choice ) {
        try_start_hacking( p, examp );
    }

    if( refund == choice ) {
        const int pos = p.inv_position_by_type( itype_id( "cash_card" ) );

        if( pos == INT_MIN ) {
            add_msg( _( "Never mind." ) );
            return;
        }

        item *cashcard = &( p.i_at( pos ) );
        // Okay, we have a cash card. Now we need to know what's left in the pump.
        const std::optional<tripoint> pGasPump = getGasPumpByNumber( examp,
                uistate.ags_pay_gas_selected_pump );
        int amount = pGasPump ? fromPumpFuel( pTank, *pGasPump ) : 0;
        if( amount >= 0 ) {
            sounds::sound( p.pos(), 6, sounds::sound_t::activity, _( "Glug Glug Glug" ), true, "tool",
                           "gaspump" );
            cashcard->charges += amount * pricePerUnit / 1000.0f;
            add_msg( m_info, _( "Your cash cards now hold %s." ),
                     format_money( p.charges_of( itype_cash_card ) ) );
            p.moves -= to_moves<int>( 5_seconds );
            return;
        } else {
            popup( _( "Unable to refund, no fuel in pump." ) );
            return;
        }
    }
}

void iexamine::ledge( player &p, const tripoint &examp )
{
    enum ledge_action : int { jump_over, climb_down, spin_web_bridge };
    if( p.in_vehicle ) {
        if( !character_funcs::can_fly( p ) &&
            !query_yn( _( "Do you really want to jump off the vehicle?" ) ) ) {
            return;
        }
        get_map().unboard_vehicle( p.pos() );
    }
    if( get_map().ter( p.pos() ).id().str() == "t_open_air" && !character_funcs::can_fly( p ) ) {
        tripoint where = p.pos();
        tripoint below = where;
        below.z--;

        // Keep going down until we find a tile that is NOT open air
        while( get_map().ter( below ).id().str() == "t_open_air" &&
               get_map().valid_move( where, below, false, true ) ) {
            where.z--;
            below.z--;
        }
        // where now represents the first NON-open-air tile or the last valid move before hitting one
        const int height = p.pos().z - below.z;

        if( height > 0 ) {
            g->vertical_move( -height, true );  // fall onto the solid tile
            return;
        }
    }

    uilist cmenu;
    cmenu.text = _( "There is a ledge here.  What do you want to do?" );
    cmenu.addentry( ledge_action::jump_over, true, 'j', _( "Jump over." ) );
    cmenu.addentry( ledge_action::climb_down, true, 'c', _( "Climb down." ) );
    if( p.has_trait( trait_WEB_BRIDGE ) ) {
        cmenu.addentry( ledge_action::spin_web_bridge, true, 'w', _( "Spin Web Bridge." ) );
    }

    cmenu.query();

    map &here = get_map();
    switch( cmenu.ret ) {
        case ledge_action::jump_over: {
            tripoint dest( p.posx() + 2 * sgn( examp.x - p.posx() ), p.posy() + 2 * sgn( examp.y - p.posy() ),
                           p.posz() );
            if( p.get_str() < 4 ) {
                add_msg( m_warning, _( "You are too weak to jump over an obstacle." ) );
            } else if( 100 * p.weight_carried() / p.weight_capacity() > 25 ) {
                add_msg( m_warning, _( "You are too burdened to jump over an obstacle." ) );
            } else if( !here.valid_move( examp, dest, false, true ) ) {
                add_msg( m_warning, _( "You cannot jump over an obstacle - something is blocking the way." ) );
            } else if( g->critter_at( dest ) ) {
                add_msg( m_warning, _( "You cannot jump over an obstacle - there is %s blocking the way." ),
                         g->critter_at( dest )->disp_name() );
            } else if( here.ter( dest ).obj().trap == tr_ledge ) {
                add_msg( m_warning, _( "You are not going to jump over an obstacle only to fall down." ) );
            } else {
                add_msg( m_info, _( "You jump over an obstacle." ) );
                p.setpos( dest );
            }
            break;
        }
        case ledge_action::climb_down: {
            tripoint where = examp;
            tripoint below = examp;
            below.z--;
            while( here.valid_move( where, below, false, true ) ) {
                where.z--;
                below.z--;
            }

            const int height = examp.z - where.z;
            if( height == 0 ) {
                p.add_msg_if_player( _( "You can't climb down there." ) );
                return;
            }

            const bool has_grapnel = p.has_amount( itype_grapnel, 1 );
            const auto climb_cost = map_funcs::climbing_cost( here, where, examp );
            const auto fall_mod = p.fall_damage_mod();
            const std::string query_str = vgettext( "Looks like %d story.  Jump down?",
                                                    "Looks like %d stories.  Jump down?",
                                                    height );

            if( height > 1 && !query_yn( query_str.c_str(), height ) ) {
                return;
            } else if( height == 1 ) {
                enum class climb_result {
                    one_way_dangerous, one_way_unclimbable,
                    both_way_safe, both_way_grapnel, both_way_hard_to_climb,
                };
                const auto get_climb_result = [&]() {
                    if( has_grapnel ) {
                        return climb_result::both_way_grapnel;
                    }
                    if( climb_cost.has_value() ) {
                        return climb_cost.value() < 200
                               ? climb_result::both_way_safe
                               : climb_result::both_way_hard_to_climb;
                    } else {
                        return fall_mod > 0.8
                               ? climb_result::one_way_dangerous
                               : climb_result::one_way_unclimbable;
                    }
                };
                const auto get_message = []( climb_result res ) {
                    switch( res ) {
                        case climb_result::both_way_safe:
                            return _( "You climb down." );
                        case climb_result::both_way_grapnel:
                            return _( "You tie the rope around your waist and begin to climb down." );
                        case climb_result::both_way_hard_to_climb:
                            return _( "You climb down but feel that it won't be easy to climb back up." );
                        case climb_result::one_way_dangerous:
                            return _( "You probably won't be able to get up and jumping down may hurt.  Jump?" );
                        case climb_result::one_way_unclimbable:
                            return _( "You probably won't be able to get back up.  Climb down?" );
                    }
                    cata::unreachable();
                };
                add_msg( m_debug, "climb_cost: %d", climb_cost.value_or( -1 ) );
                const auto result = get_climb_result();
                const auto message = get_message( result );
                switch( result ) {
                    case climb_result::both_way_safe:
                    case climb_result::both_way_grapnel:
                    case climb_result::both_way_hard_to_climb:
                        p.add_msg_if_player( message );
                        break;
                    case climb_result::one_way_dangerous:
                    case climb_result::one_way_unclimbable:
                    default:
                        if( !query_yn( message ) ) {
                            return;
                        }
                }
            }

            p.moves -= to_moves<int>( 1_seconds + 1_seconds * fall_mod );
            p.setpos( examp );

            if( climb_cost > 0 || rng_float( 0.8, 1.0 ) > fall_mod ) {
                // One tile of falling less (possibly zero)
                g->vertical_move( -1, true );
            }
            here.creature_on_trap( p );
            break;
        }
        case ledge_action::spin_web_bridge: {

            if( !can_use_mutation_warn( trait_WEB_BRIDGE, p ) ) {
                break;
            }
            const int range = 6; //this means we could web across a gap of 5.
            int success_range = 0;
            bool success = false;
            for( int i = 2; i <= range; i++ ) {
                //break at the first non empty space encountered
                if( g->m.ter( tripoint( p.posx() + i * sgn( examp.x - p.posx() ),
                                        p.posy() + i * sgn( examp.y - p.posy() ), p.posz() ) ) != t_open_air ) {
                    success_range = i;
                    success = true;
                    break;
                }
            }
            if( !success ) {
                p.add_msg_if_player( _( "There is nothing for your to attach your web to!" ) );
            } else {
                for( int i = 1; i < success_range; i++ ) {
                    tripoint dest( p.posx() + i * sgn( examp.x - p.posx() ), p.posy() + i * sgn( examp.y - p.posy() ),
                                   p.posz() );

                    g->m.ter_set( dest, t_web_bridge );
                }
                p.mutation_spend_resources( trait_WEB_BRIDGE );
            }
            break;
        }
        default:
            p.add_msg_if_player( _( "You decided to step back from the ledge." ) );
            break;
    }
}

static player &player_on_couch( player &p, const tripoint &autodoc_loc, player &null_patient,
                                bool &adjacent_couch, tripoint &couch_pos )
{
    map &here = get_map();
    for( const auto &couch_loc : here.find_furnitures_or_vparts_with_flag_in_radius( autodoc_loc, 1,
            flag_AUTODOC_COUCH ) ) {
        adjacent_couch = true;
        couch_pos = couch_loc;
        if( p.pos() == couch_loc ) {
            return p;
        }
        for( const npc *e : g->allies() ) {
            if( e->pos() == couch_loc ) {
                return  *g->critter_by_id<player>( e->getID() );
            }
        }
    }
    return null_patient;
}

static Character &operator_present( Character &p, const tripoint &autodoc_loc,
                                    Character &null_patient )
{
    map &here = get_map();
    for( const auto &loc : here.points_in_radius( autodoc_loc, 1 ) ) {
        if( !here.has_flag_furn_or_vpart( flag_AUTODOC_COUCH, loc ) ) {
            if( p.pos() == loc ) {
                return p;
            }
            for( const npc *e : g->allies() ) {
                if( e->pos() == loc ) {
                    return  *g->critter_by_id<player>( e->getID() );
                }
            }
        }
    }
    return null_patient;
}

static item *cyborg_on_couch( const tripoint &couch_pos )
{
    for( item * const &it : get_map().i_at( couch_pos ) ) {
        if( it->typeId() == itype_bot_broken_cyborg || it->typeId() == itype_bot_prototype_cyborg ) {
            return it;
        }
        if( it->typeId() == itype_corpse ) {
            if( it->get_mtype()->id == mon_broken_cyborg || it->get_mtype()->id == mon_prototype_cyborg ) {
                return it;
            }
        }
    }
    // if we're in a autodoc couch on a vehicle, go through the items in it, and return the item if's a cyborg
    if( const std::optional<vpart_reference> vp = get_map().veh_at( couch_pos ).part_with_feature(
                flag_AUTODOC_COUCH, false ) ) {
        auto dest_veh = &vp->vehicle();
        int dest_part = vp->part_index();
        for( item * const &it : dest_veh->get_items( dest_part ) ) {
            if( it->typeId() == itype_bot_broken_cyborg || it->typeId() == itype_bot_prototype_cyborg ) {
                return it;
            }
            if( it->typeId() == itype_corpse ) {
                if( it->get_mtype()->id == mon_broken_cyborg || it->get_mtype()->id == mon_prototype_cyborg ) {
                    return it;
                }
            }
        }
    }
    return nullptr;
}

static player &best_installer( player &p, player &null_player, int difficulty )
{
    float player_skill = p.bionics_adjusted_skill( skill_firstaid,
                         skill_computer,
                         skill_electronics );

    std::vector< std::pair<float, int>> ally_skills;
    ally_skills.reserve( g->allies().size() );
    for( size_t i = 0; i < g->allies().size() ; i ++ ) {
        std::pair<float, int> ally_skill;
        const npc *e = g->allies()[ i ];

        player &ally = *g->critter_by_id<player>( e->getID() );
        ally_skill.second = i;
        ally_skill.first = ally.bionics_adjusted_skill( skill_firstaid,
                           skill_computer,
                           skill_electronics );
        ally_skills.push_back( ally_skill );
    }
    std::ranges::sort( ally_skills, [&]( const std::pair<float, int> &lhs,
    const std::pair<float, int> &rhs ) {
        return rhs.first < lhs.first;
    } );
    int player_cos = bionic_manip_cos( player_skill, difficulty );
    for( size_t i = 0; i < g->allies().size() ; i ++ ) {
        if( ally_skills[ i ].first > player_skill ) {
            const npc *e = g->allies()[ ally_skills[ i ].second ];
            player &ally = *g->critter_by_id<player>( e->getID() );
            int ally_cos = bionic_manip_cos( ally_skills[ i ].first, difficulty );
            if( e->has_effect( effect_sleep ) ) {
                if( !g->u.query_yn(
                        //~ %1$s is the name of the ally
                        _( "<color_white>%1$s is asleep, but has a <color_green>%2$d<color_white> chance of success compared to your <color_red>%3$d<color_white> chance of success.  Continue with a higher risk of failure?</color>" ),
                        ally.disp_name(), ally_cos, player_cos ) ) {
                    return null_player;
                } else {
                    continue;
                }
            }
            //~ %1$s is the name of the ally
            add_msg( _( "%1$s will perform the operation with a %2$d chance of success." ), ally.disp_name(),
                     ally_cos );
            return ally;
        } else {
            break;
        }
    }

    return p;
}

template<typename ...Args>
inline void popup_player_or_npc( player &p, const char *player_mes, const char *npc_mes,
                                 Args &&... args )
{
    if( p.is_player() ) {
        popup( player_mes, std::forward<Args>( args )... );
    } else {
        popup( p.replace_with_npc_name( string_format( npc_mes, std::forward<Args>( args )... ) ) );
    }
}

void iexamine::autodoc( player &p, const tripoint &examp )
{
    enum options {
        INSTALL_CBM,
        UNINSTALL_CBM,
        BONESETTING,
        TREAT_WOUNDS,
        RAD_AWAY,
        BLOOD_ANALYSIS,
    };

    bool adjacent_couch = false;
    static avatar null_player;
    tripoint couch_pos;
    player &patient = player_on_couch( p, examp, null_player, adjacent_couch, couch_pos );
    Character &Operator = operator_present( p, examp, null_player );

    //static item null_cyborg;
    item *cyborg = cyborg_on_couch( couch_pos );

    if( !adjacent_couch ) {
        popup( _( "No connected couches found.  Operation impossible.  Exiting." ) );
        return;
    }
    if( &patient == &null_player ) {
        if( cyborg != nullptr ) {
            if( cyborg->typeId() == itype_corpse && !cyborg->is_active() ) {
                popup( _( "Patient is dead.  Please remove corpse to proceed.  Exiting." ) );
                return;
            } else if( cyborg->typeId() == itype_bot_broken_cyborg || cyborg->typeId() == itype_corpse ) {
                popup( _( "ERROR Bionic Level Assessement: FULL CYBORG.  Autodoc Mk. XI can't opperate.  Please move patient to appropriate facility.  Exiting." ) );
                return;
            }

            uilist cmenu;
            cmenu.text = _( "Autodoc Mk. XI.  Status: Online.  Please choose operation." );
            cmenu.addentry( 1, true, 'i', _( "Choose Compact Bionic Module to install." ) );
            cmenu.addentry( 2, true, 'u', _( "Choose installed bionic to uninstall." ) );
            cmenu.query();

            switch( cmenu.ret ) {
                case 1: {
                    popup( _( "ERROR NO SPACE AVAILABLE.  Operation impossible.  Exiting." ) );
                    break;
                }
                case 2: {
                    std::vector<std::string> choice_names;
                    choice_names.emplace_back( _( "Personality_Override" ) );
                    for( size_t i = 0; i < 6; i++ ) {
                        choice_names.emplace_back( _( "C0RR#PTED?D#TA" ) );
                    }
                    int choice_index = uilist( _( "Choose bionic to uninstall" ), choice_names );
                    if( choice_index == 0 ) {
                        g->save_cyborg( cyborg, couch_pos, p );
                    } else {
                        popup( _( "UNKNOWN COMMAND.  Autodoc Mk. XI. Crashed." ) );
                        return;
                    }
                    break;
                }
                default:
                    return;
            }
            return;
        } else {
            popup( _( "No patient found located on the connected couches.  Operation impossible.  Exiting." ) );
            return;
        }
    } else if( patient.activity->id() == activity_id( "ACT_OPERATION" ) ) {
        popup( _( "Operation underway.  Please wait until the end of the current procedure.  Estimated time remaining: %s." ),
               to_string( time_duration::from_turns( patient.activity->moves_left / 100 ) ) );
        p.add_msg_if_player( m_info, _( "The autodoc is working on %s." ), patient.disp_name() );
        return;
    }

    const bool unsafe_usage = &Operator == &null_player || ( &Operator == &p && &patient == &p );
    std::string autodoc_header = _( "Autodoc Mk. XI.  Status: Online.  Please choose operation" );
    if( unsafe_usage ) {
        const std::string &warning_sign = colorize( " /", c_yellow ) + colorize( "!",
                                          c_red ) + colorize( "\\", c_yellow );
        const std::string &warning = warning_sign + colorize( _( " WARNING: Operator missing" ),
                                     c_red ) + warning_sign;
        autodoc_header = warning +
                         _( "\n Using the Autodoc without an operator can lead to <color_light_cyan>serious injuries</color> and <color_light_cyan>various internal bionic malfunctions</color>.\n Manufacturer <color_light_green>guarantees automated bionic installation in functional condition</color>.\n Manufacturer <color_light_cyan>does not guarantee automated bionic uninstallation</color>.\n By continuing with the operation you accept the risks and acknowledge that you will not take any legal actions against this facility in case of an accident.\n\nThe following skills affect autodoc installation: Computers, First Aid, and Electronics." );
    }

    std::vector<item *> arm_splints;
    std::vector<item *> leg_splints;

    // find splints on the ground
    for( item *&supplies : get_map().i_at( examp ) ) {
        if( supplies->typeId() == itype_arm_splint ) {
            arm_splints.push_back( supplies );
        }
        if( supplies->typeId() == itype_leg_splint ) {
            leg_splints.push_back( supplies );
        }
    }
    // find splints in vehicle
    if( const std::optional<vpart_reference> vp = get_map().veh_at( examp ).part_with_feature(
                flag_AUTODOC, false ) ) {
        auto dest_veh = &vp->vehicle();
        int dest_part = vp->part_index();
        for( item *&it : dest_veh->get_items( dest_part ) ) {
            if( it->typeId() == itype_arm_splint ) {
                arm_splints.push_back( it );
            }
            if( it->typeId() == itype_leg_splint ) {
                leg_splints.push_back( it );
            }
        }
    }

    autodoc_header +=
        string_format(
            _( "\n\n<color_white>Internal supplies:</color>\n Arm splints: %d\n Leg splints: %d" ),
            arm_splints.size(), leg_splints.size() );

    uilist amenu;
    amenu.text = autodoc_header;
    amenu.addentry( INSTALL_CBM, true, 'i', _( "Choose Compact Bionic Module to install" ) );
    amenu.addentry( UNINSTALL_CBM, true, 'u', _( "Choose installed bionic to uninstall" ) );
    amenu.addentry( BONESETTING, true, 's', _( "Splint broken limbs" ) );
    amenu.addentry( TREAT_WOUNDS, true, 'w', _( "Treat wounds" ) );
    amenu.addentry( RAD_AWAY, true, 'r', _( "Check radiation level" ) );
    amenu.addentry( BLOOD_ANALYSIS, true, 'b', _( "Conduct blood analysis" ) );

    amenu.query();

    bool needs_anesthesia = cbm_needs_anesthesia( patient );
    std::vector<tool_comp> anesth_kit;

    if( needs_anesthesia && amenu.ret < 2 ) {
        const inventory &crafting_inv = p.crafting_inventory();
        std::vector<item *> a_filter = crafting_inv.items_with( []( const item & it ) {
            return it.has_quality( qual_ANESTHESIA );
        } );
        for( const item *anesthesia_item : a_filter ) {
            if( anesthesia_item->ammo_remaining() >= 1 ) {
                anesth_kit.emplace_back( anesthesia_item->typeId(), 1 );
            }
        }
    }

    switch( amenu.ret ) {
        case INSTALL_CBM: {
            item *bionic = game_menus::inv::install_bionic( p, patient );

            if( !bionic ) {
                return;
            }

            const itype *itemtype = bionic->type;

            player &installer = best_installer( p, null_player, itemtype->bionic->difficulty );
            if( &installer == &null_player ) {
                return;
            }

            std::vector<item_comp> progs;
            bool has_install_program = false;

            std::vector<item *> install_programs = p.crafting_inventory().items_with( [itemtype](
                    const item & it ) -> bool { return it.typeId() == itemtype->bionic->installation_data; } );

            if( !install_programs.empty() ) {
                has_install_program = true;
                progs.emplace_back( install_programs[0]->typeId(), 1 );
            }

            const int weight = 7;
            const int surgery_duration = itemtype->bionic->difficulty * 2;
            const requirement_data req_anesth = *requirement_id( "anesthetic" ) *
                                                surgery_duration * weight;

            if( patient.can_install_bionics( ( *itemtype ), installer, true, has_install_program ? 10 : -1 ) ) {
                const time_duration duration = itemtype->bionic->difficulty * 20_minutes;
                patient.introduce_into_anesthesia( duration, installer, needs_anesthesia );
                bionic->detach();
                if( needs_anesthesia ) {
                    for( const auto &e : req_anesth.get_components() ) {
                        p.consume_items( e, 1, is_crafting_component );
                    }
                    for( const auto &e : req_anesth.get_tools() ) {
                        p.consume_tools( e );
                    }
                    p.invalidate_crafting_inventory();
                }
                installer.mod_moves( -to_moves<int>( 1_minutes ) );
                patient.install_bionics( ( *itemtype ), installer, true, has_install_program ? 10 : -1 );

                if( has_install_program ) {
                    patient.consume_items( progs );
                }
            }
            break;
        }

        case UNINSTALL_CBM: {
            const bionic_collection &installed_bionics = *patient.my_bionics;
            if( installed_bionics.empty() ) {
                popup_player_or_npc( patient, _( "You don't have any bionics installed." ),
                                     _( "<npcname> doesn't have any bionics installed." ) );
                return;
            }

            for( const bionic &bio : installed_bionics ) {
                if( bio.info().itype().is_valid() ) {
                    // put cbm items in your inventory
                    detached_ptr<item> bionic_to_uninstall = item::spawn( bio.id.str(), calendar::turn );
                    bionic_to_uninstall->set_flag( flag_IN_CBM );
                    bionic_to_uninstall->set_flag( flag_NO_STERILE );
                    bionic_to_uninstall->set_flag( flag_NO_PACKED );
                    g->u.i_add( std::move( bionic_to_uninstall ) );
                }
            }

            const item *bionic = game_menus::inv::uninstall_bionic( p, patient );
            if( !bionic ) {
                g->u.remove_items_with( []( detached_ptr<item> &&it ) { // remove cbm items from inventory
                    if( it->has_flag( flag_IN_CBM ) ) {
                        detached_ptr<item> del = std::move( it ); //This acts as a delete
                    }
                    return VisitResponse::SKIP;
                } );
                return;
            }
            const item *it = bionic;
            const itype *itemtype = it->type;
            const bionic_id &bid = itemtype->bionic->id;

            g->u.remove_items_with( []( detached_ptr<item> &&it ) { // remove cbm items from inventory
                if( it->has_flag( flag_IN_CBM ) ) {
                    detached_ptr<item> del = std::move( it ); //This acts as a delete
                }
                return VisitResponse::SKIP;
            } );

            // Malfunctioning bionics that don't have associated items and get a difficulty of 12
            const int difficulty = itemtype->bionic ? itemtype->bionic->difficulty : 12;
            const float volume_anesth = difficulty * 20 * 2; // 2ml/min

            player &installer = best_installer( p, null_player, difficulty );
            if( &installer == &null_player ) {
                return;
            }

            if( patient.can_uninstall_bionic( bid, installer, true ) ) {
                const time_duration duration = difficulty * 20_minutes;
                patient.introduce_into_anesthesia( duration, installer, needs_anesthesia );
                if( needs_anesthesia ) {
                    p.consume_tools( anesth_kit, volume_anesth );
                }
                installer.mod_moves( -to_moves<int>( 1_minutes ) );
                patient.uninstall_bionic( bid, installer, true );
            }
            break;
        }

        case BONESETTING: {
            if( arm_splints.empty() && leg_splints.empty() ) {
                popup( _( "Internal supply of splints exhausted.  Operation impossible.  Exiting." ) );
                return;
            }

            int broken_limbs_count = 0;
            for( const bodypart_id &part : patient.get_all_body_parts( true ) ) {
                const bool broken = patient.is_limb_broken( part );
                if( !broken ) {
                    continue;
                }
                broken_limbs_count++;
                patient.moves -= 500;
                // TODO: fail here if unable to perform the action, i.e. can't wear more, trait mismatch.
                int quantity = 1;
                if( part == bodypart_id( "arm_l" ) || part == bodypart_id( "arm_r" ) ) {
                    if( !arm_splints.empty() ) {
                        for( detached_ptr<item> &it : get_map().use_amount( examp, 1, itype_arm_splint, quantity ) ) {
                            patient.wear_item( std::move( it ), false );
                        }
                    } else {
                        popup( _( "Internal supply of arm splints exhausted.  Splinting broken arms impossible.  Exiting." ) );
                        continue;
                    }
                } else if( part == bodypart_id( "leg_l" ) || part == bodypart_id( "leg_r" ) ) {
                    if( !leg_splints.empty() ) {
                        for( detached_ptr<item> &it : get_map().use_amount( examp, 1, itype_leg_splint, quantity ) ) {
                            patient.wear_item( std::move( it ), false );
                        }
                    } else {
                        popup( _( "Internal supply of leg splints exhausted.  Splinting broken legs impossible.  Exiting." ) );
                        continue;
                    }
                }

                if( patient.worn_with_flag( flag_SPLINT, part ) ) {
                    patient.add_msg_player_or_npc( m_good, _( "The machine rapidly sets and splints your broken %s." ),
                                                   _( "The machine rapidly sets and splints <npcname>'s broken %s." ),
                                                   body_part_name( part ) );
                    // TODO: Prevent exploits with hp draining stuff?
                    int heal_amt = patient.get_part_hp_max( part ) / 2 - patient.get_part_hp_cur( part );
                    if( heal_amt > 0 ) {
                        patient.heal( part, heal_amt );
                    }
                }
            }
            if( broken_limbs_count == 0 ) {
                popup_player_or_npc( patient, _( "You have no limbs that require splinting." ),
                                     _( "<npcname> doesn't have limbs that require splinting." ) );
            }
            break;
        }

        case TREAT_WOUNDS: {
            if( !patient.has_effect( effect_bleed ) && !patient.has_effect( effect_infected ) &&
                !patient.has_effect( effect_bite ) ) {
                p.add_msg_player_or_npc( m_info, _( "You don't have any wounds that need treatment." ),
                                         _( "<npcname> doesn't have any wounds that need treatment." ) );
                return;
            }

            if( patient.has_effect( effect_infected ) ) {
                if( patient.has_effect( effect_strong_antibiotic ) ||
                    patient.has_effect( effect_antibiotic ) ||
                    patient.has_effect( effect_weak_antibiotic ) ) {
                    patient.add_msg_player_or_npc( m_info,
                                                   _( "The autodoc detected a bacterial infection in your body, but as it also detected you've already taken antibiotics, it decided not to apply another dose right now." ),
                                                   _( "The autodoc detected a bacterial infection in <npcname>'s body, but as it also detected they've already taken antibiotics, it decided not to apply another dose right now." ) );
                } else {
                    patient.add_effect( effect_strong_antibiotic, 12_hours );
                    patient.mod_pain( 3 );
                    patient.add_msg_player_or_npc( m_good,
                                                   _( "The autodoc detected a bacterial infection in your body and injected antibiotics to treat it." ),
                                                   _( "The autodoc detected a bacterial infection in <npcname>'s body and injected antibiotics to treat it." ) );

                }
            }

            for( const bodypart_id &bp : patient.get_all_body_parts( true ) ) {
                const bodypart_str_id &bp_healed = bp.id();
                if( patient.has_effect( effect_bleed, bp_healed ) ) {
                    patient.remove_effect( effect_bleed, bp_healed );
                    patient.add_msg_player_or_npc( m_good,
                                                   _( "The autodoc detected a bleeding on your %s and applied a hemostatic drug to stop it." ),
                                                   _( "The autodoc detected a bleeding on <npcname>'s %s and applied a hemostatic drug to stop it." ),
                                                   body_part_name( bp_healed ) );
                }

                if( patient.has_effect( effect_bite, bp_healed ) ) {
                    patient.remove_effect( effect_bite, bp_healed );
                    patient.add_msg_player_or_npc( m_good,
                                                   _( "The autodoc detected an open wound on your %s and applied a disinfectant to clean it." ),
                                                   _( "The autodoc detected an open wound on <npcname>'s %s and applied a disinfectant to clean it." ),
                                                   body_part_name( bp_healed ) );

                    // Fixed disinfectant intensity of 4 disinfectant_power + 10 first aid skill level of autodoc.
                    const int disinfectant_intensity = 14;
                    patient.add_effect( effect_disinfected, 1_turns, bp_healed );
                    effect &e = patient.get_effect( effect_disinfected, bp_healed );
                    e.set_duration( e.get_int_dur_factor() * disinfectant_intensity );
                    bodypart_str_id target_part = player::bp_to_hp( bp_healed );
                    bodypart &part = patient.get_part( target_part );
                    part.set_damage_disinfected( part.get_hp_max() - part.get_hp_cur() );

                }
            }
            patient.moves -= 500;
            break;
        }

        case RAD_AWAY: {
            patient.moves -= 500;
            patient.add_msg_player_or_npc( m_info,
                                           _( "The autodoc scanned you and detected a radiation level of %d mSv." ),
                                           _( "The autodoc scanned <npcname> and detected a radiation level of %d mSv." ),
                                           patient.get_rad() );
            if( patient.get_rad() ) {
                if( patient.has_effect( effect_pblue ) ) {
                    patient.add_msg_player_or_npc( m_info,
                                                   _( "The autodoc detected an anti-radiation drug in your bloodstream, so it decided not to administer another dose right now." ),
                                                   _( "The autodoc detected an anti-radiation drug in <npcname>'s bloodstream, so it decided not to administer another dose right now." ) );
                } else {
                    add_msg( m_good,
                             _( "The autodoc administered an anti-radiation drug to treat radiation poisoning." ) );
                    patient.mod_pain( 3 );
                    patient.add_effect( effect_pblue, 1_hours );
                }
            }
            if( patient.leak_level( flag_RADIOACTIVE ) ) {
                popup( _( "Warning!  Autodoc detected a radiation leak of %d mSv from items in patient's possession.  Urgent decontamination procedures highly recommended." ),
                       patient.leak_level( flag_RADIOACTIVE ) );
            }
            break;
        }

        case BLOOD_ANALYSIS: {
            patient.moves -= 500;
            patient.conduct_blood_analysis();
            patient.add_msg_player_or_npc( m_info,
                                           _( "The autodoc analyzed your blood." ),
                                           _( "The autodoc analyzed <npcname>'s blood." ) );
            break;
        }

        default:
            return;
    }
}

namespace sm_rack
{
const int MIN_CHARCOAL = 100;
const int CHARCOAL_PER_LITER = 25;
const units::volume MAX_FOOD_VOLUME_MILLING = units::from_liter( 100 );
const units::volume MAX_FOOD_VOLUME = units::from_liter( 20 );
const units::volume MAX_FOOD_VOLUME_PORTABLE = units::from_liter( 15 );
} // namespace sm_rack

static int get_charcoal_charges( units::volume food )
{
    const int charcoal = to_liter( food ) * sm_rack::CHARCOAL_PER_LITER;

    return  std::max( charcoal, sm_rack::MIN_CHARCOAL );
}

static bool is_non_rotten_crafting_component( const item &it )
{
    return is_crafting_component( it ) && !it.rotten();
}

static void mill_activate( player &p, const tripoint &examp )
{
    map &here = get_map();
    const furn_id cur_mill_type = here.furn( examp );
    furn_id next_mill_type = f_null;
    if( cur_mill_type == f_wind_mill ) {
        next_mill_type = f_wind_mill_active;
    } else if( cur_mill_type == f_water_mill ) {
        next_mill_type = f_water_mill_active;
    } else {
        debugmsg( "Examined furniture has action mill_activate, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }
    bool food_present = false;
    map_stack items = here.i_at( examp );
    units::volume food_volume = 0_ml;

    for( item *&it : items ) {
        if( it->type->milling_data ) {
            food_present = true;
            food_volume += it->volume();
            continue;
        } else {
            add_msg( m_bad, _( "This mill contains %s, which can't be milled!" ), it->tname( 1, false ) );
            add_msg( _( "You remove the %s from the mill." ), it->tname() );
            p.mod_moves( -p.item_handling_cost( *it ) );
            here.add_item_or_charges( p.pos(), here.i_rem( examp, it ) );
            return;
        }
    }
    if( !food_present ) {
        add_msg( _( "This mill is empty.  Fill it with starchy products such as wheat, barley or oats and try again." ) );
        return;
    }
    if( food_volume > sm_rack::MAX_FOOD_VOLUME_MILLING ) {
        add_msg( _( "This mill is overloaded with products, and the millstone can't turn.  Remove some and try again." ) );
        add_msg( pgettext( "volume units", "You think that you can load about %s %s in it." ),
                 format_volume( sm_rack::MAX_FOOD_VOLUME_MILLING ), volume_units_long() );
        return;
    }

    for( auto &it : here.i_at( examp ) ) {
        if( it->type->milling_data ) {
            it->set_flag( flag_PROCESSING );
        }
    }
    here.furn_set( examp, next_mill_type );
    detached_ptr<item> result = item::spawn( "fake_milling_item", calendar::turn );
    result->activate();
    result->set_counter( to_turns<int>( milling_time ) );
    here.add_item( examp, std::move( result ) );
    add_msg( _( "You remove the brake on the millstone and it slowly starts to turn." ) );
}

static void cloning_vat_activate( player &p, const tripoint &examp )
{
    map &here = get_map();
    // 86400 = 1 day, so this is 12 hrs per size increment
    const int turns_to_clone = 43200;

    // filter out faulty carriers
    auto carriers = p.wielded_items();
    for( size_t i = 0; i < carriers.size(); ) {
        item *const carrier = carriers[i];  // already a pointer
        if( carrier->has_fault( fault_id( "fault_bionic_nonsterile" ) ) ||
            carrier->typeId() != itype_embryo_empty ) {
            carriers.erase( carriers.begin() + i ); // erase by iterator
            // do NOT increment i here
        } else {
            i++; // increment only if we didn't erase
        }
    }
    if( carriers.empty() ) {
        popup( "You need a sterilized artificial womb and DNA to begin incubation." );
        return;
    }
    if( ( *carriers.begin() )->has_flag( flag_RADIO_MOD ) ) {
        popup( "You need to remove the radio mod first." );
        return;
    }

    // choose specimen sample
    auto syringes = p.all_items_with_id( itype_dna );
    if( syringes.size() == 0 ) {
        popup( "You have no valid specimen samples." );
        return;
    }
    uilist specimen_menu;
    specimen_menu.text = _( "Select specimen sample:" );
    for( size_t z = 0; z < syringes.size(); z++ ) {
        const shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>
                ( mtype_id( syringes[z]->get_var( "specimen_sample" ) ) );
        specimen_menu.addentry( z, true, MENU_AUTOASSIGN, string_format( "%s [%s]",
                                syringes[z]->display_name(),
                                to_string( time_duration::from_turns( turns_to_clone * ( syringes[z]->get_var( "specimen_size",
                                           1 ) + 1 ) ) ) ) );
    }
    specimen_menu.query();
    const int choice = specimen_menu.ret;
    if( choice < 0 ) {
        return;
    }

    // reference to the original detached_ptr
    auto &selected_syringe = syringes[choice];
    p.mod_moves( -250 );

    // Only try to remove if the pointer is valid
    if( !selected_syringe ) {
        return;
    }
    // remove the clean carrier
    detached_ptr<item> weapon = p.remove_primary_weapon();

    // search for DNA and begin process
    std::vector<item *> items = p.all_items_with_id( itype_dna );
    for( size_t x = 0; x < items.size(); x++ ) {
        if( selected_syringe->get_var( "specimen_sample" ) == items[x]->get_var( "specimen_sample" ) ) {
            if( items[x]->units_remaining( p ) <= 1 ) {
                // this consumes the container. need to figure that out
                detached_ptr<item> garbage = p.i_rem( p.inv_position_by_item( items[x] ) );
            } else {
                items[x]->mod_charges( -1 );
            }

            add_msg( m_info, _( "The cloning vat begins its rapid incubation process." ) );
            here.furn_set( examp, furn_str_id( "f_cloning_vat_active" ) );
            detached_ptr<item> result = item::spawn( "fake_cloning_vat_item", calendar::turn );

            // 100 turns = 1 second, so 180000 = 30 min per size increment
            result->set_var( "specimen_name", selected_syringe->get_var( "specimen_name" ) );
            result->set_var( "specimen_sample", selected_syringe->get_var( "specimen_sample" ) );

            // cloning vat random upgrade logic
            if( rng( 1, 100 ) < 90 ) {
                const mtype_id id( selected_syringe->get_var( "specimen_sample" ) );
                const mtype &type = id.obj();

                mongroup_id upgrade_group = mongroup_id::NULL_ID();
                upgrade_group = type.upgrade_group;
                const auto mons = upgrade_group.obj().monsters;

                if( !mons.empty() ) {
                    // calculate total weight (sum of frequencies), pick random, then iterate until we find it
                    int total_freq = 0;
                    for( const MonsterGroupEntry &entry : mons ) {
                        total_freq += entry.frequency;
                    }
                    int roll = rng( 1, total_freq );
                    const MonsterGroupEntry *chosen = nullptr;
                    for( const MonsterGroupEntry &entry : mons ) {
                        roll -= entry.frequency;
                        if( roll <= 0 ) {
                            chosen = &entry;
                            break;
                        }
                    }
                    const shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>
                            ( mtype_id( chosen->name.str() ) );
                    monster &newmon = *newmon_ptr;

                    if( chosen ) {
                        result->set_var( "specimen_sample", chosen->name.str() );
                        result->set_var( "specimen_name", newmon.name() );
                    }
                }
            }

            result->set_counter( turns_to_clone * ( selected_syringe->get_var( "specimen_size", 1 ) + 1 ) );
            result->activate();
            here.add_item( examp, std::move( result ) );

            return;
        }
    }

    return;
}

static void smoker_activate( player &p, const tripoint &examp )
{
    map &here = get_map();
    furn_id cur_smoker_type = here.furn( examp );
    furn_id next_smoker_type = f_null;
    const bool portable = here.furn( examp ) == furn_str_id( "f_metal_smoking_rack" ) ||
                          here.furn( examp ) == furn_str_id( "f_metal_smoking_rack_active" );
    if( cur_smoker_type == f_smoking_rack ) {
        next_smoker_type = f_smoking_rack_active;
    } else if( cur_smoker_type == f_metal_smoking_rack ) {
        next_smoker_type = f_metal_smoking_rack_active;
    } else {
        debugmsg( "Examined furniture has action smoker_activate, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }
    bool food_present = false;
    bool charcoal_present = false;
    map_stack items = here.i_at( examp );
    units::volume food_volume = 0_ml;
    item *charcoal = nullptr;

    for( item * const &it : items ) {
        if( it->has_flag( flag_SMOKED ) && !it->has_flag( flag_SMOKABLE ) ) {
            add_msg( _( "This rack already contains smoked food." ) );
            add_msg( _( "Remove it before firing the smoking rack again." ) );
            return;
        }
        if( it->has_flag( flag_SMOKABLE ) ) {
            food_present = true;
            food_volume += it->volume();
            continue;
        }
        if( it->typeId() == itype_charcoal ) {
            charcoal_present = true;
            charcoal = it;
        }
        if( it->typeId() != itype_charcoal && !it->has_flag( flag_SMOKABLE ) ) {
            add_msg( m_bad, _( "This rack contains %s, which can't be smoked!" ), it->tname( 1,
                     false ) );
            add_msg( _( "You remove %s from the rack." ), it->tname() );
            here.add_item_or_charges( p.pos(), here.i_rem( examp, it ) );
            p.mod_moves( -p.item_handling_cost( *it ) );
            return;
        }
        if( it->has_flag( flag_SMOKED ) && it->has_flag( flag_SMOKABLE ) ) {
            add_msg( _( "This rack has some smoked food that might be dehydrated by smoking it again." ) );
        }
    }
    if( !food_present ) {
        add_msg( _( "This rack is empty.  Fill it with raw meat, fish or sausages and try again." ) );
        return;
    }
    if( !charcoal_present ) {
        add_msg( _( "There is no charcoal in the rack." ) );
        return;
    }
    if( portable && food_volume > sm_rack::MAX_FOOD_VOLUME_PORTABLE ) {
        add_msg( _( "This rack is overloaded with food, and it blocks the flow of smoke.  Remove some and try again." ) );
        add_msg( _( "You think that you can load about %s %s in it." ),
                 format_volume( sm_rack::MAX_FOOD_VOLUME_PORTABLE ), volume_units_long() );
        return;
    } else if( food_volume > sm_rack::MAX_FOOD_VOLUME ) {
        add_msg( _( "This rack is overloaded with food, and it blocks the flow of smoke.  Remove some and try again." ) );
        add_msg( _( "You think that you can load about %s %s in it." ),
                 format_volume( sm_rack::MAX_FOOD_VOLUME ), volume_units_long() );
        return;
    }

    int char_charges = get_charcoal_charges( food_volume );

    if( count_charges_in_list( charcoal->type, here.i_at( examp ) ) < char_charges ) {
        add_msg( _( "There is not enough charcoal in the rack to smoke this much food." ) );
        add_msg( _( "You need at least %1$s pieces of charcoal, and the smoking rack has %2$s inside." ),
                 char_charges, count_charges_in_list( charcoal->type, here.i_at( examp ) ) );
        return;
    }

    if( !p.has_charges( itype_fire, 1 ) ) {
        add_msg( _( "This smoking rack is ready to be fired, but you have no fire source." ) );
        return;
    } else if( !query_yn( _( "Fire the smoking rack?" ) ) ) {
        return;
    }

    p.use_charges( itype_fire, 1 );
    for( auto &it : here.i_at( examp ) ) {
        if( it->has_flag( flag_SMOKABLE ) ) {
            it->set_flag( flag_PROCESSING );
        }
    }
    here.furn_set( examp, next_smoker_type );
    if( charcoal->charges == char_charges ) {
        //TODO!: check
        here.i_rem( examp, charcoal );
    } else {
        charcoal->charges -= char_charges;
    }
    detached_ptr<item> result = item::spawn( "fake_smoke_plume", calendar::turn );
    result->activate();
    result->set_counter( to_turns<int>( 6_hours ) );
    here.add_item( examp, std::move( result ) );
    add_msg( _( "You light a small fire under the rack and it starts to smoke." ) );
}

void iexamine::mill_finalize( player &, const tripoint &examp, const time_point &start_time )
{
    map &here = get_map();
    const furn_id cur_mill_type = here.furn( examp );
    furn_id next_mill_type = f_null;
    if( cur_mill_type == f_wind_mill_active ) {
        next_mill_type = f_wind_mill;
    } else if( cur_mill_type == f_water_mill_active ) {
        next_mill_type = f_water_mill;
    } else {
        debugmsg( "Furniture executed action mill_finalize, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    map_stack items = here.i_at( examp );
    if( items.empty() ) {
        here.furn_set( examp, next_mill_type );
        return;
    }
    std::vector<detached_ptr<item>> results;
    for( auto iter = items.begin(); iter != items.end(); ) {
        item *it = *iter;
        if( it->type->milling_data ) {
            it->mod_last_rot_check( milling_time );
            const islot_milling &mdata = *it->type->milling_data;
            detached_ptr<item> result = item::spawn( mdata.into_, start_time + milling_time,
                                        it->count() * mdata.conversion_rate_ );
            detached_ptr<item> det;
            iter = items.erase( iter, &det );
            result->add_component( std::move( det ) );
            // copied from item::inherit_flags, which can not be called here because it requires a recipe.
            for( const flag_id &f : it->type->get_flags() ) {
                if( f->craft_inherit() ) {
                    result->set_flag( f );
                }
            }
            result->recipe_charges = result->charges;
            // Set flag to tell set_relative_rot() to calc from bday not now
            result->set_flag( flag_PROCESSING_RESULT );
            result->set_relative_rot( it->get_relative_rot() );
            result->unset_flag( flag_PROCESSING_RESULT );

            results.push_back( std::move( result ) );
        } else {
            iter++;
        }
    }

    for( detached_ptr<item> &it : results ) {
        items.insert( std::move( it ) );
    }
    here.furn_set( examp, next_mill_type );
}

void iexamine::cloning_vat_finalize( const tripoint &examp, const time_point & )
{
    // grab items in the vat
    map &here = get_map();
    map_stack items_here = here.i_at( examp );
    item developing_embryo;
    if( items_here.size() == 1 &&
        ( *items_here.begin() )->typeId() == itype_id( "fake_cloning_vat_item" ) ) {
        if( here.furn( examp ) == furn_str_id( "f_cloning_vat_active" ) ) {
            here.furn_set( examp, furn_str_id( "f_cloning_vat" ) );
        }
        developing_embryo = **items_here.begin();
    }

    here.furn_set( examp, furn_str_id( "f_cloning_vat" ) );

    // cloning vat failure: choose random garbage item and faulty womb
    if( rng( 1, 100 ) < 10 ) {
        const std::vector<itype_id> item_results{ itype_id( "arm" ), itype_id( "leg" ), itype_id( "fetus" ) };
        const itype_id &chosen_id = random_entry( item_results );
        detached_ptr<item> spawned_womb = item::spawn( itype_embryo_empty, calendar::turn );
        spawned_womb->faults.emplace( fault_id( "fault_bionic_nonsterile" ) );
        here.add_item( examp, std::move( spawned_womb ) );
        detached_ptr<item> spawned_item = item::spawn( chosen_id, calendar::turn );
        here.add_item( examp, std::move( spawned_item ) );

        sounds::sound( examp, 8, sounds::sound_t::alarm, _( "beep!" ), true, "misc", "beep" );

        return;
    }

    // success: spawn the completed artificial womb
    sounds::sound( examp, 8, sounds::sound_t::alarm, _( "ding!" ), true, "misc", "ding" );
    detached_ptr<item> spawned_embryo = item::spawn( itype_embryo, calendar::turn );
    spawned_embryo->set_var( "place_monster_override", developing_embryo.get_var( "specimen_sample" ) );
    spawned_embryo->set_var( "place_monster_override_name",
                             developing_embryo.get_var( "specimen_name" ) );
    here.add_item( examp, std::move( spawned_embryo ) );

    return;
}

static void smoker_finalize( player &, const tripoint &examp, const time_point &start_time )
{
    map &here = get_map();
    furn_id cur_smoker_type = here.furn( examp );
    furn_id next_smoker_type = f_null;
    if( cur_smoker_type == f_smoking_rack_active ) {
        next_smoker_type = f_smoking_rack;
    } else if( cur_smoker_type == f_metal_smoking_rack_active ) {
        next_smoker_type = f_metal_smoking_rack;
    } else {
        debugmsg( "Furniture executed action smoker_finalize, but is of type %s",
                  here.furn( examp ).id().c_str() );
        return;
    }

    map_stack items = here.i_at( examp );
    if( items.empty() ) {
        here.furn_set( examp, next_smoker_type );
        return;
    }

    std::vector<detached_ptr<item>> results;
    for( auto iter = items.begin(); iter != items.end(); ) {
        item *it = *iter;
        if( it->has_flag( flag_SMOKABLE ) && it->get_comestible() ) {
            if( it->get_comestible()->smoking_result.is_empty() ) {
                it->unset_flag( flag_PROCESSING );
                iter++;
            } else {
                it->mod_last_rot_check( 6_hours );

                detached_ptr<item> result = item::spawn( it->get_comestible()->smoking_result, start_time + 6_hours,
                                            it->charges );

                // Set flag to tell set_relative_rot() to calc from bday not now
                result->set_flag( flag_PROCESSING_RESULT );
                result->set_relative_rot( it->get_relative_rot() );
                result->unset_flag( flag_PROCESSING_RESULT );

                recipe rec;
                result->inherit_flags( *it, rec );

                detached_ptr<item> det;
                iter = items.erase( iter, &det );

                if( !result->has_flag( flag_NUTRIENT_OVERRIDE ) ) {
                    // If the item has "cooks_like" it will be replaced by that item as a component.
                    if( !it->get_comestible()->cooks_like.is_empty() ) {
                        // Set charges to 1 for stacking purposes.
                        det = item::spawn( it->get_comestible()->cooks_like, it->birthday(), 1 );
                    }
                    // Smoking is always 1:1, so these must be equal for correct kcal/vitamin calculation.
                    result->recipe_charges = det->charges;
                    result->add_component( std::move( det ) );
                    result->set_flag_recursive( flag_COOKED );
                }
                results.push_back( std::move( result ) );
            }
        } else {
            iter++;
        }
    }

    for( detached_ptr<item> &it : results ) {
        items.insert( std::move( it ) );
    }

    here.furn_set( examp, next_smoker_type );
}

static void smoker_load_food( player &p, const tripoint &examp,
                              const units::volume &remaining_capacity )
{
    std::vector<item_comp> comps;

    map &here = get_map();
    if( here.furn( examp ) == furn_str_id( "f_smoking_rack_active" ) ||
        here.furn( examp ) == furn_str_id( "f_metal_smoking_rack_active" ) ) {
        p.add_msg_if_player( _( "You can't place more food while it's smoking." ) );
        return;
    }
    // filter SMOKABLE food
    inventory inv = p.crafting_inventory();
    inv.remove_items_with( []( const item & it ) {
        return it.rotten();
    } );
    std::vector<item *> filtered = p.crafting_inventory().items_with( []( const item & it ) {
        return it.has_flag( flag_SMOKABLE );
    } );

    uilist smenu;
    smenu.text = _( "Load smoking rack with what kind of food?" );
    // count and ask for item to be placed ...
    std::list<std::string> names;
    std::vector<const item *> entries;
    for( const item *smokable_item : filtered ) {
        int count;
        if( smokable_item->count_by_charges() ) {
            count = inv.charges_of( smokable_item->typeId() );
        } else {
            count = inv.amount_of( smokable_item->typeId() );
        }
        if( count != 0 ) {
            auto on_list = std::ranges::find( names, item::nname( smokable_item->typeId(), 1 ) );
            if( on_list == names.end() ) {
                smenu.addentry( item::nname( smokable_item->typeId(), 1 ) );
                entries.push_back( smokable_item );
            }
            names.push_back( item::nname( smokable_item->typeId(), 1 ) );
            comps.emplace_back( smokable_item->typeId(), count );
        }
    }

    if( comps.empty() ) {
        p.add_msg_if_player( _( "You don't have any food that can be smoked." ) );
        return;
    }

    smenu.query();

    if( smenu.ret < 0 || static_cast<size_t>( smenu.ret ) >= entries.size() ) {
        add_msg( m_info, _( "Never mind." ) );
        return;
    }
    int count = 0;
    auto what = entries[smenu.ret];
    for( const auto &c : comps ) {
        if( c.type == what->typeId() ) {
            count = c.count;
        }
    }

    const int max_count_for_capacity = remaining_capacity / what->base_volume();
    const int max_count = std::min( count, max_count_for_capacity );

    // ... then ask how many to put it
    const std::string popupmsg = string_format( _( "Insert how many %s into the rack?" ),
                                 item::nname( what->typeId(), count ) );
    int amount = string_input_popup()
                 .title( popupmsg )
                 .width( 20 )
                 .text( std::to_string( max_count ) )
                 .only_digits( true )
                 .query_int();

    if( amount == 0 ) {
        add_msg( m_info, _( "Never mind." ) );
        return;
    } else if( amount > count ) {
        add_msg( m_info, _( "You don't have that many." ) );
        return;
    } else if( amount > max_count_for_capacity ) {
        add_msg( m_info, _( "You can't place that many." ) );
        return;
    }

    // reload comps with chosen items and quantity
    comps.clear();
    comps.emplace_back( what->typeId(), amount );

    // select from where to get the items from and place them
    inv.form_from_map( g->u.pos(), PICKUP_RANGE, &g->u );
    inv.remove_items_with( []( const item & it ) {
        return it.rotten();
    } );

    comp_selection<item_comp> selected = p.select_item_component( comps, 1, inv, true,
                                         is_non_rotten_crafting_component );
    std::vector<detached_ptr<item>> moved = p.consume_items( selected, 1,
                                            is_non_rotten_crafting_component );

    for( detached_ptr<item> &m : moved ) {
        p.mod_moves( -p.item_handling_cost( *m ) );
        add_msg( m_info, _( "You carefully place %s %s in the rack." ), amount,
                 item::nname( m->typeId(), amount ) );
        here.add_item( examp, std::move( m ) );
    }
    p.invalidate_crafting_inventory();
}

static void mill_load_food( player &p, const tripoint &examp,
                            const units::volume &remaining_capacity )
{
    std::vector<item_comp> comps;
    map &here = get_map();
    if( here.furn( examp ) == furn_str_id( "f_wind_mill_active" ) ||
        here.furn( examp ) == furn_str_id( "f_water_mill_active" ) ) {
        p.add_msg_if_player( _( "You can't place more food while it's milling." ) );
        return;
    }
    // filter millable food
    inventory inv = p.crafting_inventory();
    inv.remove_items_with( []( const item & it ) {
        return it.rotten();
    } );
    std::vector<item *> filtered = p.crafting_inventory().items_with( []( const item & it ) {
        return static_cast<bool>( it.type->milling_data );
    } );

    uilist smenu;
    smenu.text = _( "Load mill with what kind of product?" );
    // count and ask for item to be placed ...
    std::list<std::string> names;
    std::vector<const item *> entries;
    for( const item *millable_item : filtered ) {
        int count;
        if( millable_item->count_by_charges() ) {
            count = inv.charges_of( millable_item->typeId() );
        } else {
            count = inv.amount_of( millable_item->typeId() );
        }
        if( count != 0 ) {
            auto on_list = std::ranges::find( names, item::nname( millable_item->typeId(), 1 ) );
            if( on_list == names.end() ) {
                smenu.addentry( item::nname( millable_item->typeId(), 1 ) );
                entries.push_back( millable_item );
            }
            names.push_back( item::nname( millable_item->typeId(), 1 ) );
            comps.emplace_back( millable_item->typeId(), count );
        }
    }

    if( comps.empty() ) {
        p.add_msg_if_player( _( "You don't have any materials that can be milled." ) );
        return;
    }

    smenu.query();

    if( smenu.ret < 0 || static_cast<size_t>( smenu.ret ) >= entries.size() ) {
        add_msg( m_info, _( "Never mind." ) );
        return;
    }
    int count = 0;
    auto what = entries[smenu.ret];
    for( const auto &c : comps ) {
        if( c.type == what->typeId() ) {
            count = c.count;
        }
    }

    const int max_count_for_capacity = remaining_capacity / what->base_volume();
    const int max_count = std::min( count, max_count_for_capacity );

    // ... then ask how many to put it
    const std::string popupmsg = string_format( _( "Insert how many %s into the mill?" ),
                                 item::nname( what->typeId(), count ) );
    int amount = string_input_popup()
                 .title( popupmsg )
                 .width( 20 )
                 .text( std::to_string( max_count ) )
                 .only_digits( true )
                 .query_int();

    if( amount == 0 ) {
        add_msg( m_info, _( "Never mind." ) );
        return;
    } else if( amount > count ) {
        add_msg( m_info, _( "You don't have that many." ) );
        return;
    } else if( amount > max_count_for_capacity ) {
        add_msg( m_info, _( "You can't place that many." ) );
        return;
    }

    // reload comps with chosen items and quantity
    comps.clear();
    comps.emplace_back( what->typeId(), amount );

    // select from where to get the items from and place them
    inv.form_from_map( g->u.pos(), PICKUP_RANGE, &g->u );
    inv.remove_items_with( []( const item & it ) {
        return it.rotten();
    } );

    comp_selection<item_comp> selected = p.select_item_component( comps, 1, inv, true,
                                         is_non_rotten_crafting_component );
    std::vector<detached_ptr<item>> moved = p.consume_items( selected, 1,
                                            is_non_rotten_crafting_component );
    for( detached_ptr<item> &m : moved ) {
        p.mod_moves( -p.item_handling_cost( *m ) );
        add_msg( m_info, pgettext( "item amount and name", "You carefully place %s %s in the mill." ),
                 amount, item::nname( m->typeId(), amount ) );

        here.add_item( examp, std::move( m ) );
    }
    p.invalidate_crafting_inventory();
}

void iexamine::on_smoke_out( const tripoint &examp, const time_point &start_time )
{
    map &here = get_map();
    if( here.furn( examp ) == furn_str_id( "f_smoking_rack_active" ) ||
        here.furn( examp ) == furn_str_id( "f_metal_smoking_rack_active" ) ) {
        smoker_finalize( g->u, examp, start_time );
    }
}

void iexamine::cloning_vat_examine( player &p, const tripoint &examp )
{
    map &here = get_map();
    const bool active = here.furn( examp ) == furn_str_id( "f_cloning_vat_active" );
    map_stack items_here = here.i_at( examp );

    if( !active ) {
        // handle inactive vat: load or unload
        uilist menu;
        menu.text = "What to do with the cloning vat?";
        if( items_here.size() > 0 ) {
            menu.addentry( "Get contents" );
            menu.query();
            if( menu.ret != 0 ) {
                return;
            }

            // get pointer to first item, ask user if they want to wield
            item *it = *items_here.begin();
            if( !query_yn( string_format( "Take %s from the cloning vat?", it->tname().c_str() ) ) ) {
                return;
            }
            // remove from map, store in det
            detached_ptr<item> det;
            items_here.erase( items_here.begin(), &det );
            p.wield( std::move( det ) );

            return;
        }

        menu.addentry( "Begin incubation" );
        menu.query();

        if( menu.ret != 0 ) {
            return;
        }

        cloning_vat_activate( p, examp );
    } else {
        // handle active vat: cancel process
        if( items_here.size() == 0 ) {
            return;
        }

        const std::string prompt = string_format( _( "Cancel incubation (%s left)" ),
                                   to_string( time_duration::from_turns( ( *items_here.begin() )->get_counter() ) ) );

        uilist menu;
        menu.text = "What to do with the active cloning vat?";
        menu.addentry( prompt );
        menu.query();
        if( menu.ret != 0 ) {
            return;
        }
        // Ask using the item's name
        if( !query_yn( _( "Cancel incubation process?  This will kill the specimen inside." ) ) ) {
            return;
        }

        sounds::sound( examp, 8, sounds::sound_t::alarm, _( "beep!" ), true, "misc", "beep" );

        if( items_here.size() > 0 ) {
            items_here.erase( items_here.begin() );  // delete all items here
        }

        // cloning vat failure
        const std::vector<itype_id> item_results{ itype_id( "arm" ), itype_id( "leg" ), itype_id( "fetus" ) };
        const itype_id &chosen_id = random_entry( item_results );
        detached_ptr<item> spawned_remains = item::spawn( chosen_id, calendar::turn, 1 );
        detached_ptr<item> spawned_womb = item::spawn( itype_embryo_empty, calendar::turn, 1 );
        spawned_womb->faults.emplace( fault_id( "fault_bionic_nonsterile" ) );
        here.add_item( examp, std::move( spawned_womb ) );
        here.add_item( examp, std::move( spawned_remains ) );

        here.furn_set( examp, furn_str_id( "f_cloning_vat" ) );
    }
}

void iexamine::quern_examine( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( here.furn( examp ) == furn_str_id( "f_water_mill" ) ) {
        if( !here.is_water_shallow_current( examp ) ) {
            add_msg( _( "The water mill needs to be over shallow flowing water to work." ) );
            return;
        }
    }
    if( here.furn( examp ) == furn_str_id( "f_wind_mill" ) ) {
        if( g->is_sheltered( examp ) ) {
            add_msg( _( "The wind mill needs to be outside in the wind to work." ) );
            return;
        }
    }

    const bool active = here.furn( examp ) == furn_str_id( "f_water_mill_active" ) ||
                        here.furn( examp ) == furn_str_id( "f_wind_mill_active" );
    map_stack items_here = here.i_at( examp );

    if( items_here.empty() && active ) {
        debugmsg( "active mill was empty!" );
        if( here.furn( examp ) == furn_str_id( "f_water_mill_active" ) ) {
            here.furn_set( examp, f_water_mill );
        } else if( here.furn( examp ) == furn_str_id( "f_wind_mill_active" ) ) {
            here.furn_set( examp, f_wind_mill );
        }
        return;
    }

    if( items_here.size() == 1 && ( *items_here.begin() )->typeId() == itype_fake_milling_item ) {
        debugmsg( "f_mill_active was empty, and had fake_milling_item!" );
        if( here.furn( examp ) == furn_str_id( "f_water_mill_active" ) ) {
            here.furn_set( examp, f_water_mill );
        } else if( here.furn( examp ) == furn_str_id( "f_wind_mill_active" ) ) {
            here.furn_set( examp, f_wind_mill );
        }
        items_here.erase( items_here.begin() );
        return;
    }

    time_duration time_left = 0_turns;
    units::volume f_volume = 0_ml;
    bool f_check = false;

    for( const item * const &it : items_here ) {
        if( it->typeId() != itype_fake_milling_item ) {
            f_check = true;
            f_volume += it->volume();
        }
        if( active && it->typeId() == itype_fake_milling_item ) {
            time_left = time_duration::from_turns( it->get_counter() );
        }
    }

    const bool empty = f_volume == 0_ml;
    const bool full = f_volume >= sm_rack::MAX_FOOD_VOLUME_MILLING;
    const auto remaining_capacity = sm_rack::MAX_FOOD_VOLUME_MILLING - f_volume;

    uilist smenu;
    smenu.text = _( "What to do with the mill?" );
    smenu.desc_enabled = true;

    smenu.addentry( 0, true, 'i', _( "Inspect mill" ) );

    if( !active ) {
        smenu.addentry_desc( 1, !empty, 'r',
                             empty ?  _( "Remove brake and start milling… insert some products for milling first" ) :
                             _( "Remove brake and start milling" ),
                             string_format( _( "Remove brake and start milling, milling will take about %s." ),
                                            to_string( milling_time ) ) );

        smenu.addentry_desc( 2, !full, 'p',
                             full ? _( "Insert products for milling… mill is full" ) :
                             string_format( _( "Insert products for milling… remaining capacity is %s %s" ),
                                            format_volume( remaining_capacity ), volume_units_abbr() ),
                             _( "Fill the mill with starchy products such as wheat, barley or oats." ) );

        if( f_check ) {
            smenu.addentry( 3, f_check, 'e', _( "Remove products from mill" ) );
        }

    } else {
        smenu.addentry_desc( 4, true, 'x',
                             _( "Apply brake to mill" ),
                             _( "Applying the brake will stop milling process." ) );
    }

    smenu.query();

    switch( smenu.ret ) {
        case 0: { //inspect mill
            std::string pop;
            if( active ) {
                pop = colorize( _( "There's a mill here.  It is turning and milling." ), c_green ) + "\n";
                if( time_left > 0_turns ) {
                    pop += string_format( _( "It should take about %s to finish milling." ),
                                          to_string_clipped( time_left ) ) + "\n";
                }
            } else {
                pop += colorize( _( "There's a mill here." ), c_green ) + "\n";
            }
            pop += colorize( _( "You inspect its contents and find: " ), c_green ) + "\n\n";
            if( items_here.empty() ) {
                pop += _( "…that it is empty." );
            } else {
                std::map<const item *, int> mill_list;
                for( const item * const &it : items_here ) {
                    if( it->typeId() == itype_fake_milling_item ) {
                        pop += "\n" + colorize( _( "You see that the milling process is not yet complete." ),
                                                c_red ) + "\n";
                        continue;
                    }
                    mill_list[it] += it->count();
                }
                for( auto it_mill : mill_list ) {
                    pop += "-> " + item::nname( it_mill.first->typeId(),
                                                it_mill.first->count() ) + ( ( it_mill.second > 1 ) ? " (" + std::to_string(
                                                            it_mill.second ) + ")\n" : "\n" );
                }
            }
            popup( pop, PF_NONE );
            break;
        }
        case 1:
            //activate
            if( active ) {
                add_msg( _( "It is already milling." ) );
            } else {
                mill_activate( p, examp );
            }
            break;
        case 2:
            // load food
            mill_load_food( p, examp, remaining_capacity );
            break;
        case 3:
            // remove food
            for( map_stack::iterator it = items_here.begin(); it != items_here.end(); ) {
                if( ( *it )->typeId() != itype_fake_milling_item ) {
                    // get handling cost before the item reference is invalidated
                    const int handling_cost = -p.item_handling_cost( **it );

                    add_msg( _( "You remove %s from the mill." ), ( *it )->tname() );
                    detached_ptr<item> det;
                    it = items_here.erase( it, &det );
                    here.add_item_or_charges( p.pos(), std::move( det ) );
                    p.mod_moves( handling_cost );
                } else {
                    ++it;
                }
            }
            if( active ) {
                if( here.furn( examp ) == furn_str_id( "f_water_mill_active" ) ) {
                    here.furn_set( examp, f_water_mill );
                } else if( here.furn( examp ) == furn_str_id( "f_wind_mill_active" ) ) {
                    here.furn_set( examp, f_wind_mill );
                }
                add_msg( m_info, _( "You stop the milling process." ) );
            }
            break;
        default:
            add_msg( m_info, _( "Never mind." ) );
            break;
        case 4:
            if( here.furn( examp ) == furn_str_id( "f_water_mill_active" ) ) {
                here.furn_set( examp, f_water_mill );
            } else if( here.furn( examp ) == furn_str_id( "f_wind_mill_active" ) ) {
                here.furn_set( examp, f_wind_mill );
            }
            add_msg( m_info, _( "You stop the milling process." ) );
            break;
    }
}

void iexamine::smoker_options( player &p, const tripoint &examp )
{
    map &here = get_map();
    const bool active = here.furn( examp ) == furn_str_id( "f_smoking_rack_active" ) ||
                        here.furn( examp ) == furn_str_id( "f_metal_smoking_rack_active" );
    const bool portable = here.furn( examp ) == furn_str_id( "f_metal_smoking_rack" ) ||
                          here.furn( examp ) == furn_str_id( "f_metal_smoking_rack_active" );
    map_stack items_here = here.i_at( examp );

    if( portable && items_here.empty() && active ) {
        debugmsg( "f_metal_smoking_rack_active was empty!" );
        here.furn_set( examp, f_metal_smoking_rack );
        return;
    } else if( items_here.empty() && active ) {
        debugmsg( "f_smoking_rack_active was empty!" );
        here.furn_set( examp, f_smoking_rack );
        return;
    }
    if( portable && items_here.size() == 1 &&
        ( *items_here.begin() )->typeId() == itype_fake_smoke_plume ) {
        debugmsg( "f_metal_smoking_rack_active was empty, and had fake_smoke_plume!" );
        here.furn_set( examp, f_metal_smoking_rack );
        items_here.erase( items_here.begin() );
        return;
    } else if( items_here.size() == 1 && ( *items_here.begin() )->typeId() == itype_fake_smoke_plume ) {
        debugmsg( "f_smoking_rack_active was empty, and had fake_smoke_plume!" );
        here.furn_set( examp, f_smoking_rack );
        items_here.erase( items_here.begin() );
        return;
    }

    bool rem_f_opt = false;
    time_duration time_left = 0_turns;
    int hours_left = 0;
    int minutes_left = 0;
    units::volume f_volume = 0_ml;
    bool f_check = false;

    for( const item * const &it : items_here ) {
        if( it->is_food() ) {
            f_check = true;
            f_volume += it->volume();
        }
        if( active && it->typeId() == itype_fake_smoke_plume ) {
            time_left = time_duration::from_turns( it->get_counter() );
            hours_left = to_hours<int>( time_left );
            minutes_left = to_minutes<int>( time_left ) + 1;
        }
    }

    const bool empty = f_volume == 0_ml;
    const bool full = f_volume >= sm_rack::MAX_FOOD_VOLUME;
    const bool full_portable = f_volume >= sm_rack::MAX_FOOD_VOLUME_PORTABLE;
    const auto remaining_capacity = sm_rack::MAX_FOOD_VOLUME - f_volume;
    const auto remaining_capacity_portable = sm_rack::MAX_FOOD_VOLUME_PORTABLE - f_volume;
    const auto has_coal_in_inventory = p.charges_of( itype_charcoal ) > 0;
    const auto coal_charges = count_charges_in_list( &*itype_charcoal, items_here );
    const auto need_charges = get_charcoal_charges( f_volume );
    const bool has_coal = coal_charges > 0;
    const bool has_enough_coal = coal_charges >= need_charges;

    uilist smenu;
    smenu.text = _( "What to do with the smoking rack:" );
    smenu.desc_enabled = true;

    smenu.addentry( 0, true, 'i', _( "Inspect smoking rack" ) );

    if( !active ) {
        smenu.addentry_desc( 1, !empty && has_enough_coal, 'l',
                             empty ?  _( "Light up and smoke food… insert some food for smoking first" ) :
                             !has_enough_coal ? string_format(
                                 _( "Light up and smoke food… need extra %d charges of charcoal" ),
                                 need_charges - coal_charges ) :
                             _( "Light up and smoke food" ),
                             _( "Light up the smoking rack and start smoking.  Smoking will take about 6 hours." ) );
        if( portable ) {
            smenu.addentry_desc( 2, !full_portable, 'f',
                                 full_portable ? _( "Insert food for smoking… smoking rack is full" ) :
                                 string_format( _( "Insert food for smoking… remaining capacity is %s %s" ),
                                                format_volume( remaining_capacity_portable ), volume_units_abbr() ),
                                 _( "Fill the smoking rack with raw meat, fish or sausages for smoking or fruit or vegetable or smoked meat for drying." ) );

            smenu.addentry_desc( 8, !active, 'z',
                                 active ? _( "You cannot disassemble this smoking rack while it is active!" ) :
                                 _( "Disassemble the smoking rack" ), "" );

        } else {
            smenu.addentry_desc( 2, !full, 'f',
                                 full ? _( "Insert food for smoking… smoking rack is full" ) :
                                 string_format( _( "Insert food for smoking… remaining capacity is %s %s" ),
                                                format_volume( remaining_capacity ), volume_units_abbr() ),
                                 _( "Fill the smoking rack with raw meat, fish or sausages for smoking or fruit or vegetable or smoked meat for drying." ) );
        }

        if( f_check ) {
            smenu.addentry( 4, f_check, 'e', _( "Remove food from smoking rack" ) );
        }

        smenu.addentry_desc( 3, has_coal_in_inventory, 'r',
                             !has_coal_in_inventory ? _( "Reload with charcoal… you don't have any" ) :
                             _( "Reload with charcoal" ),
                             string_format(
                                 _( "You need %d charges of charcoal for %s %s of food.  Minimal amount of charcoal is %d charges." ),
                                 sm_rack::CHARCOAL_PER_LITER, format_volume( 1_liter ), volume_units_long(),
                                 sm_rack::MIN_CHARCOAL ) );

    } else {
        smenu.addentry_desc( 7, true, 'x',
                             _( "Quench burning charcoal" ),
                             _( "Quenching will stop smoking process, but also destroy all used charcoal." ) );
    }

    if( has_coal ) {
        smenu.addentry( 5, true, 'c',
                        active ? string_format( _( "Rake out %d excess charges of charcoal from smoking rack" ),
                                                coal_charges ) :
                        string_format( _( "Remove %d charges of charcoal from smoking rack" ), coal_charges ) );
    }

    smenu.query();

    switch( smenu.ret ) {
        case 0: { //inspect smoking rack
            std::string pop;
            if( active ) {
                pop += colorize( _( "There's a smoking rack here.  It is lit and smoking." ), c_green ) + "\n";
                if( time_left > 0_turns ) {
                    if( minutes_left > 60 ) {
                        pop += string_format( vgettext( "It will finish smoking in about %d hour.",
                                                        "It will finish smoking in about %d hours.",
                                                        hours_left ), hours_left ) + "\n\n";
                    } else if( minutes_left > 30 ) {
                        pop += _( "It will finish smoking in less than an hour." ) + std::string( "\n" );
                    } else {
                        pop += string_format( _( "It should take about %d minutes to finish smoking." ),
                                              minutes_left ) + "\n ";
                    }
                }
            } else {
                pop += colorize( _( "There's a smoking rack here." ), c_green ) + "\n";
            }
            pop += colorize( _( "You inspect its contents and find: " ), c_green ) + "\n";
            if( items_here.empty() ) {
                pop += _( "…that it is empty." );
            } else {
                for( const item * const &it : items_here ) {
                    if( it->typeId() == itype_fake_smoke_plume ) {
                        pop += "\n" + colorize( _( "You see some smoldering embers there." ), c_red ) + "\n";
                        continue;
                    }
                    pop += "-> " + item::nname( it->typeId(),
                                                it->charges ) + " (" + std::to_string( it->charges ) + ")\n";
                }
            }
            popup( pop, PF_NONE );
            break;
        }
        case 1:
            //activate
            if( active ) {
                add_msg( _( "It is already lit and smoking." ) );
            } else {
                smoker_activate( p, examp );
            }
            break;
        case 2:
            // load food
            if( portable ) {
                smoker_load_food( p, examp, remaining_capacity_portable );
            } else {
                smoker_load_food( p, examp, remaining_capacity );
            }
            break;
        case 3:
            // load charcoal
            reload_furniture( p, examp );
            break;
        case 4:
            // remove food
            rem_f_opt = true;
        /* fallthrough */
        case 5: {
            //remove charcoal
            for( map_stack::iterator it = items_here.begin(); it != items_here.end(); ) {
                if( ( rem_f_opt && ( *it )->is_food() ) || ( !rem_f_opt &&
                        ( ( *it )->typeId() == itype_charcoal ) ) ) {
                    // get handling cost before the item reference is invalidated
                    const int handling_cost = -p.item_handling_cost( **it );

                    add_msg( _( "You remove %s from the rack." ), ( *it )->tname() );
                    detached_ptr<item> det;
                    it = items_here.erase( it, &det );
                    here.add_item_or_charges( p.pos(), std::move( det ) );
                    p.mod_moves( handling_cost );
                } else {
                    ++it;
                }
            }
            if( portable && active && rem_f_opt ) {
                here.furn_set( examp, f_metal_smoking_rack );
                add_msg( m_info, _( "You stop the smoking process." ) );
            } else if( active && rem_f_opt ) {
                here.furn_set( examp, f_smoking_rack );
                add_msg( m_info, _( "You stop the smoking process." ) );
            }
        }
        break;
        default:
            add_msg( m_info, _( "Never mind." ) );
            break;
        case 7:
            if( portable ) {
                here.furn_set( examp, f_metal_smoking_rack );
                add_msg( m_info, _( "You stop the smoking process." ) );
            } else {
                here.furn_set( examp, f_smoking_rack );
                add_msg( m_info, _( "You stop the smoking process." ) );
            }
            break;
        case 8:
            here.furn_set( examp, f_metal_smoking_rack );
            deployed_furniture( p, examp );
            break;
    }
}

void iexamine::hunting_snare( player &p, const tripoint &examp )
{
    map &here = get_map();
    const furn_id furn_at_pos = here.furn( examp );
    const std::string furn_str = furn_at_pos.id().str();

    if( furn_str.ends_with( "_empty" ) ) {
        bait_snare( p, examp );
    } else if( furn_str.ends_with( "_set" ) ) {
        // Menu for baited trap
        uilist amenu;
        amenu.text = _( "Baited trap options:" );
        amenu.addentry( 0, true, 'r', _( "Remove bait and reset trap" ) );
        amenu.addentry( 1, true, 'c', _( "Cancel" ) );
        amenu.query();

        if( amenu.ret == 0 ) {
            // Remove bait and convert to _empty
            std::string base_name = furn_str.substr( 0, furn_str.length() - 4 ); // Remove "_set"

            // Return bait to player
            map_stack items = here.i_at( examp );
            for( auto it = items.begin(); it != items.end(); ) {
                item &current_item = **it;

                // Skip the fake tracker item
                if( current_item.typeId().str() == "fake_snare_bait" ) {
                    ++it;
                    continue;
                }

                // Check if this is actual bait (has BAIT_ flag)
                bool is_bait = false;
                for( const flag_id &flag : current_item.type->get_flags() ) {
                    if( flag.str().starts_with( "BAIT_" ) ) {
                        is_bait = true;
                        break;
                    }
                }

                if( is_bait ) {
                    add_msg( m_info, _( "You retrieve the %s." ), current_item.tname() );
                    detached_ptr<item> det;
                    it = here.i_rem( examp, it, &det );
                    p.i_add( std::move( det ) );
                } else {
                    ++it;
                }
            }

            // Remove all items from trap (including fake tracker)
            here.i_clear( examp );

            // Convert to empty trap
            here.furn_set( examp, furn_str_id( base_name + "_empty" ) );
            add_msg( m_good, _( "You reset the trap." ) );
            p.mod_moves( -to_moves<int>( 5_seconds ) );
        }
    } else if( furn_str.ends_with( "_closed" ) ) {
        harvest_snare( p, examp );
    } else {
        debugmsg( "hunting_snare called on unexpected furniture: %s", furn_str );
    }
}

void iexamine::bait_snare( player &p, const tripoint &examp )
{
    map &here = get_map();
    const furn_id furn_at = here.furn( examp );
    std::string furn_str = furn_at.id().str();

    // Verify this is an empty snare
    if( furn_str.length() < 6 || furn_str.substr( furn_str.length() - 6 ) != "_empty" ) {
        add_msg( m_info, _( "There's no empty trap here." ) );
        return;
    }

    // Menu for options
    uilist amenu;
    amenu.text = _( "Snare options:" );
    amenu.addentry( 0, true, 'b', _( "Add bait" ) );
    amenu.addentry( 1, true, 't', _( "Take down the snare" ) );
    amenu.query();

    if( amenu.ret == 1 ) {
        // Take down option - deconstruct the furniture
        add_msg( m_info, _( "You dismantle the snare." ) );
        here.furn_set( examp, f_null );
        // Drop components
        if( !furn_at->deconstruct.drop_group.is_empty() ) {
            here.spawn_items( examp, item_group::items_from( furn_at->deconstruct.drop_group,
                              calendar::turn ) );
        }
        p.mod_moves( -to_moves<int>( 10_seconds ) );
        return;
    }

    if( amenu.ret != 0 ) {
        return;
    }

    // Find bait in inventory (any item with BAIT_ prefix flag)
    std::vector<item *> bait_items;

    for( item *it : p.inv_dump() ) {
        bool has_bait = false;
        for( const flag_id &flag : it->type->get_flags() ) {
            if( flag.str().starts_with( "BAIT_" ) ) {
                has_bait = true;
                break;
            }
        }
        if( has_bait ) {
            bait_items.push_back( it );
        }
    }

    if( bait_items.empty() ) {
        add_msg( m_info, _( "You don't have any bait." ) );
        return;
    }
    // Simple selection - just use first available bait
    item *selected_bait = bait_items[0];
    std::string bait_name = selected_bait->tname();
    uilist bmenu;
    bmenu.text = _( "Select bait to use:" );
    for( auto bait_item : bait_items ) {
        // TODO: invlet is 0 ALWAYS. Should use another one.
        bmenu.addentry( -1, true, -1, bait_item->tname() );
    }
    bmenu.query();
    if( bmenu.ret < 0 || static_cast<size_t>( bmenu.ret ) >= bait_items.size() ) {
        add_msg( m_info, _( "Never mind." ) );
        return;
    } else {
        selected_bait = bait_items[bmenu.ret];
        bait_name = selected_bait->tname();
    }

    // Get base furniture name
    std::string base_name = furn_str.substr( 0, furn_str.length() - 6 );

    // Check for setup requirements (besides bait)
    const hunting::snaring_hunting_data *hd = hunting::find_hunting_data_for_furniture( examp );
    if( hd && hd->req_id_for_setup.has_value() ) {
        const requirement_data &reqs = hd->req_id_for_setup.value().obj();

        // Check if player can fulfill requirements
        if( !reqs.can_make_with_inventory( p.crafting_inventory(), is_crafting_component ) ) {
            std::string missing = reqs.list_missing();
            add_msg( m_info, _( "You need the following to set this trap:\n%s" ), missing );
            return;
        }

        // Consume requirements
        for( const auto &comp_list : reqs.get_components() ) {
            p.consume_items( comp_list, 1, is_crafting_component );
        }
        for( const auto &tool_list : reqs.get_tools() ) {
            p.consume_tools( tool_list, 1 );
        }
    }

    // Create fake tracker item (for time tracking only)
    detached_ptr<item> fake = item::spawn( "fake_snare_bait", calendar::turn );
    const int bait_counter = to_turns<int>( time_duration::from_hours( rng( 4, 8 ) ) );
    fake->activate();
    fake->set_counter( bait_counter );
    fake->set_var( "base_furn", base_name );
    fake->set_var( "bait_counter", bait_counter );

    std::vector<detached_ptr<item>> bait_copy;
    // Consume one unit of bait
    if( selected_bait -> count_by_charges() ) {
        bait_copy = p.use_charges( selected_bait->typeId(), 1 );
    } else {
        bait_copy = p.use_amount( selected_bait->typeId(), 1 );
    }
    // Move actual bait to trap location (don't delete it)
    // Add both items to trap location
    here.add_item( examp, std::move( fake ) );
    here.add_item( examp, std::move( bait_copy.front() ) );
    here.furn_set( examp, furn_str_id( base_name + "_set" ) );

    add_msg( m_good, _( "You bait the snare with %s." ), bait_name );
    p.mod_moves( -to_moves<int>( 5_seconds ) );
}

void iexamine::harvest_snare( player &p, const tripoint &examp )
{
    map &here = get_map();
    const furn_id furn_at = here.furn( examp );

    // Check if this is a triggered trap (must end with "_closed")
    std::string furn_str = furn_at.id().str();
    if( furn_str.length() < 7 || furn_str.substr( furn_str.length() - 7 ) != "_closed" ) {
        add_msg( m_info, _( "This trap hasn't been triggered." ) );
        return;
    }

    map_stack items_here = here.i_at( examp );

    // Find item with stored hunting data (bait with vars set by process_fake_snare)
    item *data_item = nullptr;
    for( item * const &it : items_here ) {
        if( !hunting::extract_bait_types( it ).empty() && it->has_var( "base_furn" ) ) {
            data_item = it;
            break;
        }
    }

    // If no data found, trap triggered but process_fake_snare didn't run
    // (shouldn't happen, but handle gracefully)
    if( !data_item ) {
        add_msg( m_neutral, _( "The snare is empty. Nothing was caught." ) );
        std::string base = furn_str.substr( 0, furn_str.length() - 7 );
        here.furn_set( examp, furn_str_id( base + "_empty" ) );
        items_here.clear();
        p.mod_moves( -to_moves<int>( 5_seconds ) );
        return;
    }

    // Extract stored data
    std::string base_furn = data_item->get_var( "base_furn", "f_wire_snare" );
    int proximity_penalty = data_item->get_var( "proximity_penalty", 0 );
    time_point snared_time = time_point::from_turn( data_item->get_var( "snared_time",
                             to_turn<int>( calendar::turn ) ) );
    std::vector<std::string> bait_flags_str = hunting::extract_bait_types( data_item );

    // Get hunting_data for after_trigger handling
    const hunting::snaring_hunting_data *hd = hunting::find_hunting_data_for_furniture( examp );

    // Process snare catch (check + generate corpse)
    hunting::snare_catch_result catch_result = hunting::process_snare_catch( examp, bait_flags_str,
            p, proximity_penalty, snared_time );

    // Remove all items (bait consumed)
    items_here.clear();

    // Check skill to determine if trap gets damaged - applies to ALL traps
    int const skill_level = p.get_skill_level( skill_trapping );
    int const damage_chance = rng( 0, 5 );
    bool trap_destroyed = false;

    if( damage_chance > skill_level + 2 ) {
        // Try to bash the trap (may or may not succeed in destroying it)
        bash_params params{
            p.get_str(), false, false, false,
            static_cast<float>( rng_float( 0, 1.0f ) ), false
        };
        bash_results results = here.bash_ter_furn( examp, params );

        if( results.success ) {
            // Trap was actually destroyed
            add_msg( m_bad, _( "You damaged the snare while harvesting it, and it breaks." ) );
            trap_destroyed = true;
        } else {
            // Bash failed, trap survives but player nearly broke it
            add_msg( m_warning, _( "You nearly damaged the snare, but managed to recover it." ) );
        }
    }

    // Reset furniture and spawn items - DO THIS FIRST to remove NOITEM flag
    // Only if trap wasn't destroyed by skill check
    if( !trap_destroyed ) {
        hunting::apply_after_trigger( examp, hd, base_furn );
    }

    // NOW process results and spawn corpse after furniture has been changed
    if( catch_result.hunt_result.success && catch_result.corpse ) {
        add_msg( m_good, catch_result.hunt_result.message );
        here.add_item_or_charges( examp, std::move( catch_result.corpse ) );
        p.practice( skill_trapping, 25 );
        p.practice( skill_survival, 12 );
    } else {
        add_msg( m_neutral, catch_result.hunt_result.message );
        p.practice( skill_trapping, 5 );
        p.practice( skill_survival, 2 );
    }
    p.mod_moves( -to_moves<int>( 5_seconds ) );
}

void iexamine::open_safe( player &, const tripoint &examp )
{
    add_msg( m_info, _( "You open the unlocked safe." ) );
    get_map().furn_set( examp, f_safe_o );
}

void iexamine::workbench( player &, const tripoint & )
{
    // Dummied out and only used for function equality check
}

void iexamine::dimensional_portal( player &p, const tripoint &examp )
{
    uilist menu;
    menu.text = _( "What to do with the portal:" );
    menu.desc_enabled = true;

    std::vector<item *> nukes = p.all_items_with_flag( flag_CLOSES_PORTAL );
    menu.addentry_desc( 0, !nukes.empty(), 'e', _( "Close from here" ),
                        _( "Requires a nuclear explosive" ) );
    menu.addentry_desc( 1, true, 'Q', _( "Sacrifice yourself" ),
                        _( "This will kill you, but close the portal" ) );
    menu.query();

    switch( menu.ret ) {
        case 0: {
            item *the_nuke = game_menus::inv::titled_filter_menu( []( const item & it ) {
                return it.has_flag( flag_CLOSES_PORTAL );
            }, static_cast<avatar &>( p ), _( "What to use to close the portal?" ) );
            if( !the_nuke ) {
                add_msg( m_info, _( "Never mind." ) );
                break;
            }

            add_msg( m_good, _( "You throw the armed %s into the portal!" ), the_nuke->tname() );
            the_nuke->detach();
            g->m.translate_radius( t_dimensional_portal, t_thconc_floor, 5, examp, true );
            g->win();
            break;
        }

        case 1:
            p.set_all_parts_hp_cur( 0 );
            g->m.translate_radius( t_dimensional_portal, t_thconc_floor, 5, examp, true );
            g->win();
            break;
        default:
            add_msg( m_info, _( "Never mind." ) );
    }
}

void iexamine::check_power( player &, const tripoint &examp )
{
    tripoint_abs_ms abspos( g->m.getabs( examp ) );
    battery_tile *battery = active_tiles::furn_at<battery_tile>( abspos );
    if( battery != nullptr ) {
        add_msg( m_info, _( "This battery stores %d kJ of electric power." ), battery->get_resource() );
    }
    int amt = get_distribution_grid_tracker().grid_at( abspos ).get_resource();
    add_msg( m_info, _( "This electric grid stores %d kJ of electric power." ), amt );
}

void iexamine::migo_nerve_cluster( player &p, const tripoint &examp )
{
    map &here = get_map();
    if( query_yn( _( "This looks important.  Tear open nerve cluster?" ) ) ) {
        p.mod_moves( -200 );
        add_msg( _( "You grab hold of a sinewy tendril and wrench it loose!" ) );
        map_funcs::migo_nerve_cage_removal( here, examp, false );
        here.furn_set( examp, furn_id( "f_alien_scar" ) );
    }
}

void iexamine::cardreader_plutgen( player &p, const tripoint &examp )
{
    map &here = get_map();
    itype_id card_type = itype_id_military;
    if( p.has_amount( card_type, 1 ) && query_yn( _( "Swipe your ID card?" ) ) ) {
        // The duration taken may need modification.
        p.mod_moves( -100 );
        p.use_amount( card_type, 1 );
        add_msg( _( "You insert your ID card." ) );
        add_msg( m_good,
                 _( "The plutonium generator beeps twice, then disengages from the surrounding conduits with a series of mechanical clunks." ) );
        here.ter_set( examp, t_concrete );
        here.add_item_or_charges( examp, item::spawn( itype_plut_generator_item, calendar::turn ) );
    } else {
        add_msg( _( "The plutonium generator has significant security measures in place. Without the necessary ID, you'll have to remove it by hand." ) );
    }
}

/**
 * Given then name of one of the above functions, returns the matching function
 * pointer. If no match is found, defaults to iexamine::none but prints out a
 * debug message as a warning.
 * @param function_name The name of the function to get.
 * @return A function pointer to the specified function.
 */
iexamine_function iexamine_function_from_string( const std::string &function_name )
{
    static const std::map<std::string, iexamine_function> function_map = {{
            { "none", &iexamine::none },
            { "deployed_furniture", &iexamine::deployed_furniture },
            { "cvdmachine", &iexamine::cvdmachine },
            { "nanofab", &iexamine::nanofab },
            { "gaspump", &iexamine::gaspump },
            { "atm", &iexamine::atm },
            { "vending", &iexamine::vending },
            { "toilet", &iexamine::toilet },
            { "elevator", &iexamine::elevator },
            { "toggle_lights", &iexamine::toggle_lights},
            { "controls_gate", &iexamine::controls_gate },
            { "cardreader", &iexamine::cardreader },
            { "cardreader_robofac", &iexamine::cardreader_robofac },
            { "cardreader_fp", &iexamine::cardreader_foodplace },
            { "intercom", &iexamine::intercom },
            { "rubble", &iexamine::rubble },
            { "chainfence", &iexamine::chainfence },
            { "bars", &iexamine::bars },
            { "portable_structure", &iexamine::portable_structure },
            { "pit", &iexamine::pit },
            { "pit_covered", &iexamine::pit_covered },
            { "slot_machine", &iexamine::slot_machine },
            { "safe", &iexamine::safe },
            { "fault", &iexamine::fault },
            { "notify", &iexamine::notify },
            { "transform", &iexamine::transform },
            { "pedestal_wyrm", &iexamine::pedestal_wyrm },
            { "pedestal_temple", &iexamine::pedestal_temple },
            { "door_peephole", &iexamine::door_peephole },
            { "fswitch", &iexamine::fswitch },
            { "flower_poppy", &iexamine::flower_poppy },
            { "flower_cactus", &iexamine::flower_cactus },
            { "fungus", &iexamine::fungus },
            { "flower_dahlia", &iexamine::flower_dahlia },
            { "flower_marloss", &iexamine::flower_marloss },
            { "egg_sackbw", &iexamine::egg_sackbw },
            { "egg_sackcs", &iexamine::egg_sackcs },
            { "egg_sackws", &iexamine::egg_sackws },
            { "dirtmound", &iexamine::dirtmound },
            { "aggie_plant", &iexamine::aggie_plant },
            { "fvat_empty", &iexamine::fvat_empty },
            { "fvat_full", &iexamine::fvat_full },
            { "keg", &iexamine::keg },
            { "harvest_furn_nectar", &iexamine::harvest_furn_nectar },
            { "harvest_furn", &iexamine::harvest_furn },
            { "harvest_ter_nectar", &iexamine::harvest_ter_nectar },
            { "harvest_ter", &iexamine::harvest_ter },
            { "harvested_plant", &iexamine::harvested_plant },
            { "shrub_marloss", &iexamine::shrub_marloss },
            { "translocator", &iexamine::translocator },
            { "tree_marloss", &iexamine::tree_marloss },
            { "tree_hickory", &iexamine::tree_hickory },
            { "tree_maple", &iexamine::tree_maple },
            { "tree_maple_tapped", &iexamine::tree_maple_tapped },
            { "shrub_wildveggies", &iexamine::shrub_wildveggies },
            { "recycle_compactor", &iexamine::recycle_compactor },
            { "trap", &iexamine::trap },
            { "water_source", &iexamine::water_source },
            { "clean_water_source", &iexamine::clean_water_source },
            { "liquid_source", &iexamine::liquid_source },
            { "reload_furniture", &iexamine::reload_furniture },
            { "use_furn_fake_item", &iexamine::use_furn_fake_item },
            { "curtains", &iexamine::curtains },
            { "sign", &iexamine::sign },
            { "pay_gas", &iexamine::pay_gas },
            { "gunsafe_el", &iexamine::gunsafe_el },
            { "locked_object", &iexamine::locked_object },
            { "locked_object_pickable", &iexamine::locked_object_pickable },
            { "kiln_empty", &iexamine::kiln_empty },
            { "kiln_full", &iexamine::kiln_full },
            { "arcfurnace_empty", &iexamine::arcfurnace_empty },
            { "arcfurnace_full", &iexamine::arcfurnace_full },
            { "autoclave_empty", &iexamine::autoclave_empty },
            { "autoclave_full", &iexamine::autoclave_full },
            { "fireplace", &iexamine::fireplace },
            { "ledge", &iexamine::ledge },
            { "autodoc", &iexamine::autodoc },
            { "quern_examine", &iexamine::quern_examine },
            { "cloning_vat_examine", &iexamine::cloning_vat_examine },
            { "smoker_options", &iexamine::smoker_options },
            { "hunting_snare", &iexamine::hunting_snare },
            { "open_safe", &iexamine::open_safe },
            { "workbench", &iexamine::workbench },
            { "dimensional_portal", &iexamine::dimensional_portal },
            { "check_power", &iexamine::check_power },
            { "migo_nerve_cluster", &iexamine::migo_nerve_cluster },
            { "cardreader_plutgen", &iexamine::cardreader_plutgen },
        }
    };

    auto iter = function_map.find( function_name );
    if( iter != function_map.end() ) {
        return iter->second;
    }

    //No match found
    debugmsg( "Could not find an iexamine function matching '%s'!", function_name );
    return &iexamine::none;
}

void iexamine::practice_survival_while_foraging( player *p )
{
    ///\EFFECT_INT Intelligence caps survival skill gains from foraging
    const int max_forage_skill = p->int_cur / 2 + 1;
    const int max_exp = 2 * max_forage_skill;
    // Award experience for foraging attempt regardless of success
    p->practice( skill_survival, rng( 1, max_exp ), max_forage_skill );
}
