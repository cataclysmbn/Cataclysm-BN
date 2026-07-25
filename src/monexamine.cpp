#include "monexamine.h"

#include <climits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "avatar_action.h"
#include "bodypart.h"
#include "calendar.h"
#include "catalua_hooks.h"
#include "catalua_icallback_actor.h"
#include "cata_utility.h"
#include "character.h"
#include "debug.h"
#include "enums.h"
#include "game.h"
#include "game_inventory.h"
#include "item.h"
#include "itype.h"
#include "iuse.h"
#include "map.h"
#include "material.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "output.h"
#include "player_activity.h"
#include "point.h"
#include "rng.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "units.h"
#include "value_ptr.h"


static const quality_id qual_shear( "SHEAR" );
static const quality_id qual_butcher( "BUTCHER" );

static const efftype_id effect_sheared( "sheared" );

static const activity_id ACT_MILK( "ACT_MILK" );
static const activity_id ACT_PLAY_WITH_PET( "ACT_PLAY_WITH_PET" );
static const activity_id ACT_TRAIN_PET( "ACT_TRAIN_PET" );

static const efftype_id effect_ai_waiting( "ai_waiting" );
static const efftype_id effect_docile( "docile" );
static const efftype_id effect_harnessed( "harnessed" );
static const efftype_id effect_has_bag( "has_bag" );
static const efftype_id effect_monster_armor( "monster_armor" );
static const efftype_id effect_paid( "paid" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_saddled( "monster_saddled" );
static const efftype_id effect_leashed( "leashed" );
static const efftype_id effect_led_by_leash( "led_by_leash" );
static const efftype_id effect_tied( "tied" );
static const efftype_id effect_well_fed( "well_fed" );

static const itype_id itype_cash_card( "cash_card" );
static const itype_id itype_id_industrial( "id_industrial" );
static const itype_id itype_id_military( "id_military" );

static const skill_id skill_survival( "survival" );
static const species_id ZOMBIE( "ZOMBIE" );

static const flag_id json_flag_TIE_UP( "TIE_UP" );
static const flag_id json_flag_TACK( "TACK" );
static const flag_id json_flag_MECH_BAT( "MECH_BAT" );

static const int ITEM_RADIUS = 2;

namespace
{

struct monster_ammo_option {
    itype_id ammo_id;
    int amount = 0;
};

struct monster_reload_option {
    itype_id ammo_id;
    int missing_ammo = 0;
    std::vector<monster_ammo_option> compatible_ammo;
};

auto has_reloadable_ammo( const monster &z ) -> bool
{
    return z.has_flag( MF_DROPS_AMMO ) && !z.type->starting_ammo.empty();
}

auto compatible_reload_ammo( const avatar &you, const monster &z,
                             const itype_id &ammo_id ) -> std::vector<monster_ammo_option>
{
    auto ammo_options = std::vector<monster_ammo_option> {};
    for( const auto &compatible_ammo_id : z.ammo_slot_items( ammo_id ) ) {
        const auto available_ammo = you.charges_of( compatible_ammo_id );
        if( available_ammo <= 0 ) {
            continue;
        }
        ammo_options.emplace_back( monster_ammo_option{
            .ammo_id = compatible_ammo_id,
            .amount = available_ammo,
        } );
    }
    return ammo_options;
}

auto loaded_slot_ammo( const monster &z, const itype_id &ammo_id )
-> std::vector<monster_ammo_option>
{
    auto ammo_options = std::vector<monster_ammo_option> {};
    for( const auto &compatible_ammo_id : z.ammo_slot_items( ammo_id ) ) {
        const auto available_ammo = z.ammo.contains( compatible_ammo_id ) ? z.ammo.at(
                                        compatible_ammo_id ) : 0;
        if( available_ammo <= 0 ) {
            continue;
        }
        ammo_options.emplace_back( monster_ammo_option{
            .ammo_id = compatible_ammo_id,
            .amount = available_ammo,
        } );
    }
    return ammo_options;
}

auto monster_reload_options( const avatar &you,
                             const monster &z ) -> std::vector<monster_reload_option>
{
    auto reload_options = std::vector<monster_reload_option> {};
    if( !has_reloadable_ammo( z ) ) {
        return reload_options;
    }

    for( const auto &[ammo_id, max_ammo] : z.type->starting_ammo ) {
        const auto current_ammo = z.ammo_count_for_slot( ammo_id );
        const auto missing_ammo = max_ammo - current_ammo;
        if( missing_ammo <= 0 ) {
            continue;
        }
        auto reload_option = monster_reload_option{
            .ammo_id = ammo_id,
            .missing_ammo = missing_ammo,
            .compatible_ammo = compatible_reload_ammo( you, z, ammo_id ),
        };
        reload_options.emplace_back( std::move( reload_option ) );
    }

    return reload_options;
}

auto needs_ammo_reload( const monster &z ) -> bool
{
    if( !has_reloadable_ammo( z ) ) {
        return false;
    }

    for( const auto &[ammo_id, max_ammo] : z.type->starting_ammo ) {
        const auto current_ammo = z.ammo_count_for_slot( ammo_id );
        if( current_ammo < max_ammo ) {
            return true;
        }
    }

    return false;
}

auto has_compatible_reload_ammo( const avatar &you, const monster &z ) -> bool
{
    for( const auto &reload_option : monster_reload_options( you, z ) ) {
        if( !reload_option.compatible_ammo.empty() ) {
            return true;
        }
    }

    return false;
}

auto has_loaded_ammo( const monster &z ) -> bool
{
    for( const auto &[ammo_id, max_ammo] : z.type->starting_ammo ) {
        static_cast<void>( max_ammo );
        if( z.ammo_count_for_slot( ammo_id ) > 0 ) {
            return true;
        }
    }

    return false;
}

auto ammo_slot_name( const monster &z, const itype_id &ammo_id ) -> std::string
{
    const auto loaded_ammo = z.loaded_ammo_for_slot( ammo_id );
    const auto display_ammo = loaded_ammo.is_empty() ? ammo_id : loaded_ammo;
    return display_ammo->nname( 1 );
}

auto ammo_state_text( const monster &z ) -> std::string
{
    if( !has_reloadable_ammo( z ) ) {
        return {};
    }

    return enumerate_as_string( z.type->starting_ammo.begin(), z.type->starting_ammo.end(),
    [&z]( const auto & ammo_entry ) {
        const auto &[ammo_id, max_ammo] = ammo_entry;
        const auto current_ammo = z.ammo_count_for_slot( ammo_id );
        return string_format( _( "%s %d/%d" ), ammo_slot_name( z, ammo_id ), current_ammo, max_ammo );
    }, enumeration_conjunction::none );
}

auto reload_menu_text( const avatar &you, const monster &z ) -> std::string
{
    const auto ammo_text = ammo_state_text( z );
    if( has_compatible_reload_ammo( you, z ) ) {
        return string_format( _( "Reload weapons (%s)" ), ammo_text );
    }
    if( needs_ammo_reload( z ) ) {
        return string_format( _( "Reload weapons (%s; need compatible ammo)" ), ammo_text );
    }
    return string_format( _( "Reload weapons (%s; already full)" ), ammo_text );
}

auto unload_menu_text( const monster &z ) -> std::string
{
    const auto ammo_text = ammo_state_text( z );
    if( has_loaded_ammo( z ) ) {
        return string_format( _( "Unload weapons (%s)" ), ammo_text );
    }
    return string_format( _( "Unload weapons (%s; already empty)" ), ammo_text );
}

auto select_ammo_slot( const std::string &prompt, const monster &z,
                       const std::vector<itype_id> &ammo_slots ) -> itype_id
{
    if( ammo_slots.empty() ) {
        return itype_id {};
    }
    if( ammo_slots.size() == 1 ) {
        return ammo_slots.front();
    }

    uilist selection_menu;
    selection_menu.text = prompt;
    auto i = 0;
    for( const auto &ammo_slot : ammo_slots ) {
        selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "%1$s (%2$d/%3$d)" ),
                                 ammo_slot_name( z, ammo_slot ), z.ammo_count_for_slot( ammo_slot ),
                                 z.ammo_capacity_for_slot( ammo_slot ) );
    }
    selection_menu.query();
    if( selection_menu.ret < 0 || static_cast<size_t>( selection_menu.ret ) >= ammo_slots.size() ) {
        return itype_id {};
    }
    return ammo_slots[selection_menu.ret];
}

auto select_ammo_variant( const std::string &prompt,
                          const std::vector<monster_ammo_option> &ammo_options ) -> itype_id
{
    if( ammo_options.empty() ) {
        return itype_id {};
    }
    if( ammo_options.size() == 1 ) {
        return ammo_options.front().ammo_id;
    }

    uilist selection_menu;
    selection_menu.text = prompt;
    auto i = 0;
    for( const auto &ammo_option : ammo_options ) {
        selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "%1$s (%2$d)" ),
                                 ammo_option.ammo_id->nname( ammo_option.amount ),
                                 ammo_option.amount );
    }
    selection_menu.query();
    if( selection_menu.ret < 0 || static_cast<size_t>( selection_menu.ret ) >= ammo_options.size() ) {
        return itype_id {};
    }
    return ammo_options[selection_menu.ret].ammo_id;
}

auto reload_monster_weapons( avatar &you, monster &z ) -> void
{
    const auto reload_options = monster_reload_options( you, z );
    auto reloadable_slots = std::vector<itype_id> {};
    for( const auto &reload_option : reload_options ) {
        if( !reload_option.compatible_ammo.empty() ) {
            reloadable_slots.push_back( reload_option.ammo_id );
        }
    }

    if( reloadable_slots.empty() ) {
        if( needs_ammo_reload( z ) ) {
            add_msg( m_info, _( "You don't have compatible ammo to reload the %s." ), z.get_name() );
        } else {
            add_msg( m_info, _( "The %s's weapons are already fully loaded." ), z.get_name() );
        }
        return;
    }

    const auto selected_slot = select_ammo_slot( _( "Reload which weapon?" ), z, reloadable_slots );
    if( selected_slot.is_empty() ) {
        return;
    }

    const auto reload_option_iter = std::ranges::find_if( reload_options,
    [&selected_slot]( const monster_reload_option & reload_option ) {
        return reload_option.ammo_id == selected_slot;
    } );
    if( reload_option_iter == reload_options.end() ) {
        return;
    }

    const auto selected_ammo = select_ammo_variant(
                                   string_format( _( "Reload the %s with what?" ),
                                           ammo_slot_name( z, selected_slot ) ),
                                   reload_option_iter->compatible_ammo );
    if( selected_ammo.is_empty() ) {
        return;
    }

    const auto selected_ammo_iter = std::ranges::find_if( reload_option_iter->compatible_ammo,
    [&selected_ammo]( const monster_ammo_option & ammo_option ) {
        return ammo_option.ammo_id == selected_ammo;
    } );
    if( selected_ammo_iter == reload_option_iter->compatible_ammo.end() ) {
        return;
    }

    const auto reload_amount = std::min( reload_option_iter->missing_ammo, selected_ammo_iter->amount );
    z.ammo[selected_ammo] += reload_amount;
    you.use_charges( selected_ammo, reload_amount );
    add_msg( vgettext( "You load %1$d x %2$s round into the %3$s.",
                       "You load %1$d x %2$s rounds into the %3$s.", reload_amount ),
             reload_amount, selected_ammo->nname( reload_amount ), z.get_name() );
    you.moves -= 100;
}

auto unload_monster_weapons( avatar &you, monster &z ) -> void
{
    auto loaded_slots = std::vector<itype_id> {};
    for( const auto &[ammo_id, max_ammo] : z.type->starting_ammo ) {
        static_cast<void>( max_ammo );
        if( z.ammo_count_for_slot( ammo_id ) > 0 ) {
            loaded_slots.push_back( ammo_id );
        }
    }

    if( loaded_slots.empty() ) {
        add_msg( m_info, _( "The %s's weapons are already empty." ), z.get_name() );
        return;
    }

    const auto selected_slot = select_ammo_slot( _( "Unload which weapon?" ), z, loaded_slots );
    if( selected_slot.is_empty() ) {
        return;
    }

    const auto loaded_ammo = loaded_slot_ammo( z, selected_slot );
    const auto selected_ammo = select_ammo_variant(
                                   string_format( _( "Unload which ammo from the %s?" ),
                                           ammo_slot_name( z, selected_slot ) ),
                                   loaded_ammo );
    if( selected_ammo.is_empty() ) {
        return;
    }

    const auto loaded_ammo_iter = std::ranges::find_if( loaded_ammo,
    [&selected_ammo]( const monster_ammo_option & ammo_option ) {
        return ammo_option.ammo_id == selected_ammo;
    } );
    if( loaded_ammo_iter == loaded_ammo.end() ) {
        return;
    }

    auto unloaded_ammo = item::spawn( selected_ammo, calendar::turn, loaded_ammo_iter->amount );
    unloaded_ammo = you.i_add_or_drop( std::move( unloaded_ammo ) );
    if( unloaded_ammo ) {
        add_msg( m_info, _( "You can't unload the %s right now." ), z.get_name() );
        return;
    }

    z.ammo.erase( selected_ammo );
    add_msg( vgettext( "You unload %1$d x %2$s round from the %3$s.",
                       "You unload %1$d x %2$s rounds from the %3$s.", loaded_ammo_iter->amount ),
             loaded_ammo_iter->amount, selected_ammo->nname( loaded_ammo_iter->amount ),
             z.get_name() );
    you.moves -= 100;
}

} // namespace

bool monexamine::pet_menu( monster &z )
{
    enum choices {
        push_zlave = 0,
        lead,
        stop_lead,
        rename,
        attach_bag,
        remove_bag,
        drop_all,
        give_items,
        take_items,
        mon_armor_add,
        mon_harness_remove,
        mon_armor_remove,
        leash,
        unleash,
        play_with_pet,
        train_combat_pet,
        slaughter,
        milk,
        shear,
        pay,
        attach_saddle,
        remove_saddle,
        mount,
        tie,
        untie,
        remove_bat,
        insert_bat,
        check_bat,
        reload_weapons,
        unload_weapons,
        change_orders,
        disable_pet,
        attack,
        COUNT
    };

    uilist amenu;
    std::string pet_name = z.get_name();
    bool is_zombie = z.type->in_species( ZOMBIE );
    bool can_slaughter = z.type->in_category( "WILDLIFE" );
    const auto mon_item_id = z.type->revert_to_itype;
    avatar &you = get_avatar();
    if( is_zombie ) {
        pet_name = _( "zombie slave" );
    }

    amenu.text = string_format( _( "What to do with your %s?" ), pet_name );

    amenu.addentry( push_zlave, true, 'p', _( "Push %s" ), pet_name );
    if( z.has_effect( effect_leashed ) ) {
        if( z.has_effect( effect_led_by_leash ) ) {
            amenu.addentry( stop_lead, true, 'P', _( "Stop leading %s" ), pet_name );
        } else {
            amenu.addentry( lead, true, 'P', _( "Lead %s by the leash" ), pet_name );
        }
    }
    amenu.addentry( rename, true, 'e', _( "Rename" ) );
    if( z.has_effect( effect_has_bag ) ) {
        amenu.addentry( give_items, true, 'g', _( "Place items into bag" ) );
        amenu.addentry( remove_bag, true, 'b', _( "Remove bag from %s" ), pet_name );

        if( !z.get_items().empty() ) {
            amenu.addentry( take_items, true, 'G', _( "Take items from bag" ) );
            amenu.addentry( drop_all, true, 'd', _( "Remove all items from bag" ) );
        }
    } else if( !z.has_flag( MF_RIDEABLE_MECH ) ) {
        amenu.addentry( attach_bag, true, 'b', _( "Attach bag to %s" ), pet_name );
    }
    if( z.has_effect( effect_harnessed ) ) {
        amenu.addentry( mon_harness_remove, true, 'H', _( "Remove vehicle harness from %s" ), pet_name );
    }
    if( z.has_effect( effect_monster_armor ) ) {
        amenu.addentry( mon_armor_remove, true, 'a', _( "Remove armor from %s" ), pet_name );
    } else if( !z.has_flag( MF_RIDEABLE_MECH ) ) {
        amenu.addentry( mon_armor_add, true, 'a', _( "Equip %s with armor" ), pet_name );
    }
    if( z.has_flag( MF_CANPLAY ) ) {
        amenu.addentry( play_with_pet, true, 'y', _( "Play with %s" ), pet_name );
    }
    if( !z.type->pet_training ) {
        amenu.addentry( train_combat_pet, false, '[', _( "Train %s (cannot be trained)" ), pet_name );
    } else if( z.has_flag( MF_CANT_TRAIN ) ) {
        amenu.addentry( train_combat_pet, false, '[', _( "Train %s (cannot be trained)" ), pet_name );
    } else if( z.training_level >= z.type->pet_training->max_level ) {
        amenu.addentry( train_combat_pet, false, '[', _( "Train %s (level %d/%d - fully trained)" ),
                        pet_name, z.training_level, z.type->pet_training->max_level );
    } else if( get_player_character().get_skill_level( skill_survival ) <
               z.type->pet_training->min_skill ) {
        amenu.addentry( train_combat_pet, false, '[', _( "Train %s (requires survival %d)" ), pet_name,
                        z.type->pet_training->min_skill );
    } else if( !z.has_effect( effect_well_fed ) ) {
        amenu.addentry( train_combat_pet, false, '[', _( "Train %s (requires well-fed)" ), pet_name );
    } else {
        amenu.addentry( train_combat_pet, true, '[', _( "Train %s (level %d/%d)" ), pet_name,
                        z.training_level, z.type->pet_training->max_level );
    }
    if( z.has_effect( effect_tied ) ) {
        amenu.addentry( untie, true, 'u', _( "Untie" ) );
    }
    if( z.has_effect( effect_leashed ) && !z.has_effect( effect_tied ) ) {
        amenu.addentry( tie, true, 't', _( "Tie" ) );
        amenu.addentry( unleash, true, 'T', _( "Remove leash from %s" ), pet_name );
    }
    if( !z.has_effect( effect_leashed ) && !z.has_flag( MF_RIDEABLE_MECH ) ) {
        Character &player_character = get_player_character();
        std::vector<item *> rope_inv = player_character.items_with( []( const item & it ) {
            return it.has_flag( json_flag_TIE_UP );
        } );
        if( !rope_inv.empty() ) {
            amenu.addentry( leash, true, 'l', _( "Attach leash to %s" ), pet_name );
        } else {
            amenu.addentry( leash, false, 'l', _( "You need any type of rope to tie %s in place" ),
                            pet_name );
        }
    }
    if( z.has_flag( MF_MILKABLE ) ) {
        amenu.addentry( milk, true, 'm', _( "Milk %s" ), pet_name );
    }
    if( z.has_flag( MF_SHEARABLE ) ) {
        bool available = true;
        if( season_of_year( calendar::turn ) == WINTER ) {
            amenu.addentry( shear, false, 'S',
                            _( "This animal would freeze if you shear it during winter." ) );
            available = false;
        } else if( z.has_effect( effect_sheared ) ) {
            amenu.addentry( shear, false, 'S', _( "This animal is not ready to be sheared again yet." ) );
            available = false;
        }
        if( available ) {
            if( you.has_quality( qual_shear, 1 ) ) {
                amenu.addentry( shear, true, 'S', _( "Shear %s." ), pet_name );
            } else {
                amenu.addentry( shear, false, 'S', _( "You cannot shear this animal without shears." ) );
            }
        }
    }
    if( z.has_flag( MF_PET_MOUNTABLE ) && !z.has_effect( effect_saddled ) &&
        you.has_item_with_flag( json_flag_TACK ) && you.get_skill_level( skill_survival ) >= 1 ) {
        amenu.addentry( attach_saddle, true, 'h', _( "Tack up %s" ), pet_name );
    } else if( z.has_flag( MF_PET_MOUNTABLE ) && z.has_effect( effect_saddled ) ) {
        amenu.addentry( remove_saddle, true, 'h', _( "Remove tack from %s" ), pet_name );
    } else if( z.has_flag( MF_PET_MOUNTABLE ) && !z.has_effect( effect_saddled ) &&
               you.has_item_with_flag( json_flag_TACK ) && you.get_skill_level( skill_survival ) < 1 ) {
        amenu.addentry( remove_saddle, false, 'h', _( "You don't know how to saddle %s" ), pet_name );
    }
    if( z.has_flag( MF_PAY_BOT ) ) {
        amenu.addentry( pay, true, 'f', _( "Manage your friendship with %s" ), pet_name );
    }

    if( !z.has_flag( MF_RIDEABLE_MECH ) ) {
        if( z.has_flag( MF_PET_MOUNTABLE ) ) {
            auto status = you.get_mountable_status( z );
            if( status.can_mount() ) {
                const auto msg = z.has_effect( effect_tied )
                                 ? _( "Untie and mount %s" )
                                 : _( "Mount %s" );
                amenu.addentry( mount, true, 'r', msg, pet_name );
            } else if( !status.size ) {
                amenu.addentry( mount, false, 'r', _( "%s is too small to carry your weight" ), pet_name );
            } else if( !status.carry_weight ) {
                amenu.addentry( mount, false, 'r', _( "You are too heavy to mount %s" ), pet_name );
            } else if( !status.skills && you.get_skill_level( skill_survival ) < 1 ) {
                amenu.addentry( mount, false, 'r', _( "You have no knowledge of riding at all" ) );
            } else if( !status.skills && you.get_skill_level( skill_survival ) < 4 &&
                       !z.has_effect( effect_saddled ) ) {
                amenu.addentry( mount, false, 'r', _( "You are not skilled enough to ride without a saddle" ) );
            } else {
                amenu.addentry( mount, false, 'r', _( "%s cannot be mounted right now" ), pet_name );
            }
        } else {
            amenu.addentry( mount, false, 'r', _( "%s cannot be mounted" ), pet_name );
        }
    } else {
        const itype &type = *z.type->mech_battery;
        int max_charge = type.magazine->capacity;
        float charge_percent;
        if( z.get_battery_item() ) {
            charge_percent = static_cast<float>( z.get_battery_item()->ammo_remaining() ) / max_charge * 100;
        } else {
            charge_percent = 0.0;
        }
        amenu.addentry( check_bat, false, 'c', _( "%s battery level is %d%%" ), z.get_name(),
                        static_cast<int>( charge_percent ) );
        if( ( you.primary_weapon().is_null() || z.type->mech_weapon.is_empty() ) && z.get_battery_item() ) {
            amenu.addentry( mount, true, 'r', _( "Climb into the mech and take control" ) );
        } else if( !you.primary_weapon().is_null() && !z.type->mech_weapon.is_empty() ) {
            amenu.addentry( mount, false, 'r', _( "You cannot pilot this mech whilst wielding something" ) );
        } else if( !z.get_battery_item() ) {
            amenu.addentry( mount, false, 'r', _( "This mech has a dead battery and won't turn on" ) );
        }
        if( z.get_battery_item() ) {
            amenu.addentry( remove_bat, true, 'x', _( "Remove the mech's battery pack" ) );
        } else if( you.has_amount( z.type->mech_battery, 1 ) ) {
            amenu.addentry( insert_bat, true, 'x', _( "Insert a new battery pack" ) );
        } else {
            amenu.addentry( insert_bat, false, 'x', _( "You need a %s to power this mech" ), type.nname( 1 ) );
        }
    }
    if( has_reloadable_ammo( z ) ) {
        amenu.addentry( reload_weapons, true, 'R', reload_menu_text( you, z ) );
        if( has_loaded_ammo( z ) ) {
            amenu.addentry( unload_weapons, true, 'U', unload_menu_text( z ) );
        }
    }
    if( z.has_flag( MF_CAN_BE_ORDERED ) ) {
        if( z.has_effect( effect_docile ) ) {
            amenu.addentry( change_orders, true, 'O', _( "Order to engage targets" ), pet_name );
        } else {
            amenu.addentry( change_orders, true, 'O', _( "Order to ignore enemies and follow" ), pet_name );
        }
    }
    if( !mon_item_id.is_empty() && !z.has_flag( MF_PAY_BOT ) ) {
        if( z.has_effect( effect_has_bag ) || z.has_effect( effect_monster_armor ) ||
            z.has_effect( effect_leashed ) || z.has_effect( effect_saddled ) || z.get_battery_item() ) {
            amenu.addentry( disable_pet, true, 'D', _( "Remove items and deactivate the %s" ), pet_name );
        } else {
            amenu.addentry( disable_pet, true, 'D', _( "Deactivate the %s" ), pet_name );
        }
    }
    if( ( is_zombie || can_slaughter ) && you.has_quality( qual_butcher, 1 ) ) {
        amenu.addentry( slaughter, true, 'A', _( "Slaughter %s" ), pet_name );
    } else {
        amenu.addentry( attack, true, 'A', _( "Attack" ) );
    }

    std::vector<lua_menu_entry> lua_entries {};
    std::map<int, lua_menu_entry> lua_entries_map;
    const auto cb_actor = z.get_lua_callbacks();
    if( cb_actor ) {
        const auto entries = cb_actor->call_get_examine_menu_entries( you, z );
        for( const auto entry : entries ) {
            if( entry.valid() ) {
                lua_entries.push_back( entry );
            }
        }
    }

    const auto hook_results = cata::run_hooks( "on_monster_get_examine_menu_entries",
    [&]( auto & params ) { params["avatar"] = &you; params["monster"] = &z; } );
    for( const auto results = cata::get_hook_results( hook_results ); const auto result : results ) {
        if( !result.is<sol::table>() ) { continue; }

        const sol::table &entries_table = result.as<sol::table>();
        const int size = entries_table.size();
        for( int j = 1; j <= size; ++j ) {
            sol::optional<sol::table> entry_opt = entries_table[j];
            if( !entry_opt.has_value() ) {
                debugmsg( "Empty entry at index %d", j );
                continue;
            }

            const sol::table &entry = *entry_opt;
            std::string id = entry.get<std::string>( "menu_id" );
            std::string label = entry.get<std::string>( "menu_label" );
            lua_entries.emplace_back( id, label );
        }
    }

    std::ranges::sort( lua_entries, []( const lua_menu_entry & a, const lua_menu_entry & b ) {
        return a.menu_label > b.menu_label;
    } );

    int last_int = COUNT - 1;
    for( const auto entry : lua_entries ) {
        last_int++;
        lua_entries_map.emplace( last_int, entry );
        amenu.addentry( last_int, true, ' ', entry.menu_label );
    }

    amenu.query();
    int choice = amenu.ret;
    std::string entry_str_id;

    switch( choice ) {
        case push_zlave:
            entry_str_id = "push_zlave";
            push( z );
            break;
        case lead:
            entry_str_id = "lead";
            start_leading( z );
            break;
        case stop_lead:
            entry_str_id = "stop_lead";
            stop_leading( z );
            break;
        case rename:
            entry_str_id = "rename";
            rename_pet( z );
            break;
        case attach_bag:
            entry_str_id = "attach_bag";
            attach_bag_to( z );
            break;
        case remove_bag:
            entry_str_id = "remove_bag";
            remove_bag_from( z );
            break;
        case drop_all:
            entry_str_id = "drop_all";
            dump_items( z );
            break;
        case give_items:
            entry_str_id = "give_items";
            return give_items_to( z );
        case take_items:
            entry_str_id = "take_items";
            take_items_from( z );
            break;
        case mon_armor_add:
            entry_str_id = "mon_armor_add";
            return add_armor( z );
        case mon_harness_remove:
            entry_str_id = "mon_harness_remove";
            remove_harness( z );
            break;
        case mon_armor_remove:
            entry_str_id = "mon_armor_remove";
            remove_armor( z );
            break;
        case play_with_pet:
            if( query_yn( _( "Spend a few minutes to play with your %s?" ), pet_name ) ) {
                entry_str_id = "play_with_pet";
                play_with( z );
            }
            break;
        case train_combat_pet:
            entry_str_id = "train_combat_pet";
            train_pet( z );
            break;
        case slaughter:
            if( query_yn( _( "Really kill the %s?" ), pet_name ) ) {
                entry_str_id = "slaughter";
                kill_zslave( z );
            }
            break;
        case leash:
            entry_str_id = "leash";
            add_leash( z );
            break;
        case unleash:
            entry_str_id = "unleash";
            remove_leash( z );
            break;
        case attach_saddle:
            entry_str_id = "attach_saddle";
            attach_or_remove_saddle( z );
            break;
        case remove_saddle:
            entry_str_id = "remove_saddle";
            attach_or_remove_saddle( z );
            break;
        case mount:
            entry_str_id = "mount";
            mount_pet( z );
            break;
        case tie:
            entry_str_id = "tie";
            tie_pet( z );
            break;
        case untie:
            entry_str_id = "untie";
            untie_pet( z );
            break;
        case milk:
            entry_str_id = "milk";
            milk_source( z );
            break;
        case shear:
            entry_str_id = "shear";
            shear_animal( z );
            break;
        case pay:
            entry_str_id = "pay";
            pay_bot( z );
            break;
        case remove_bat:
            entry_str_id = "remove_bat";
            remove_battery( z );
            break;
        case insert_bat:
            entry_str_id = "insert_bat";
            insert_battery( z );
            break;
        case check_bat:
            entry_str_id = "check_bat";
            break;
        case reload_weapons:
            reload_monster_weapons( you, z );
            break;
        case unload_weapons:
            entry_str_id = "unload_weapons";
            unload_monster_weapons( you, z );
            break;
        case change_orders:
            entry_str_id = "change_orders";
            toggle_ignore_targets( z );
            break;
        case disable_pet:
            if( query_yn( _( "Really deactivate your %s?" ), pet_name ) ) {
                entry_str_id = "disable_pet";
                deactivate_pet( z );
            }
            break;
        case attack:
            if( query_yn( _( "You may be attacked!  Proceed?" ) ) ) {
                entry_str_id = "attack";
                avatar_action::melee_attack_while_handling_manual_combat_mode( get_avatar(), z );
            }
            break;
        default:
            if( choice >= COUNT ) {
                entry_str_id = lua_entries_map[choice].menu_id;
            }
            break;
    }
    if( !entry_str_id.empty() ) {
        if( cb_actor ) {
            cb_actor->call_on_examine_menu_entry( you, z, entry_str_id );
        }
        cata::run_hooks( "on_monster_examine_menu_entry", [&](
        auto & params ) { params["avatar"] = &you; params["monster"] = &z; params["entry"] = entry_str_id; } );
    }

    return true;
}

void monexamine::shear_animal( monster &z )
{
    avatar &you = get_avatar();
    const int moves = to_moves<int>( time_duration::from_minutes( 30 / you.max_quality(
                                         qual_shear ) ) );

    you.assign_activity( activity_id( "ACT_SHEAR" ), moves, -1 );
    you.activity->coords.push_back( z.abs_pos() );
    // pin the sheep in place if it isn't already
    if( !z.has_effect( effect_tied ) ) {
        z.add_effect( effect_tied, 1_turns );
        you.activity->str_values.emplace_back( "temp_tie" );
    }
    you.activity->targets.emplace_back( you.best_quality_item( qual_shear ) );
    add_msg( _( "You start shearing the %s." ), z.get_name() );
}

static item *pet_armor_loc( monster &z )
{
    auto filter = [z]( const item & it ) {
        return z.type->bodytype == it.get_pet_armor_bodytype() &&
               z.get_volume() >= it.get_pet_armor_min_vol() &&
               z.get_volume() <= it.get_pet_armor_max_vol();
    };

    return game_menus::inv::titled_filter_menu( filter, get_avatar(), _( "Pet armor" ), "",
            ITEM_RADIUS );
}

static item *tack_loc()
{
    auto filter = []( const item & it ) {
        return it.has_flag( json_flag_TACK );
    };

    return game_menus::inv::titled_filter_menu( filter, get_avatar(), _( "Tack" ), "", ITEM_RADIUS );
}

void monexamine::remove_battery( monster &z )
{
    get_map().add_item_or_charges( get_player_character().bub_pos(), z.remove_battery_item() );

}

void monexamine::insert_battery( monster &z )
{
    if( z.get_battery_item() ) {
        // already has a battery, shouldn't be called with one, but just incase.
        return;
    }
    avatar &you = get_avatar();
    std::vector<item *> bat_inv = you.items_with( []( const item & itm ) {
        return itm.has_flag( json_flag_MECH_BAT );
    } );
    if( bat_inv.empty() ) {
        return;
    }
    int i = 0;
    uilist selection_menu;
    selection_menu.text = string_format( _( "Select an battery to insert into your %s." ),
                                         z.get_name() );
    selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "Cancel" ) );
    for( auto &iter : bat_inv ) {
        selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "Use %s" ), iter->tname() );
    }
    selection_menu.selected = 1;
    selection_menu.query();
    auto index = selection_menu.ret;
    if( index == 0 || index == UILIST_CANCEL || index < 0 ||
        index > static_cast<int>( bat_inv.size() ) ) {
        return;
    }
    item *bat_item = bat_inv[index - 1];
    int item_pos = you.get_item_position( bat_item );
    if( item_pos != INT_MIN ) {
        z.set_battery_item( you.i_rem( item_pos ) );
    }
}

bool monexamine::mech_hack( monster &z )
{
    itype_id card_type = ( z.has_flag( MF_MILITARY_MECH ) ? itype_id_military : itype_id_industrial );
    avatar &you = get_avatar();
    if( you.has_amount( card_type, 1 ) ) {
        if( query_yn( _( "Swipe your %s into the %s's security port?" ), item::nname( card_type ),
                      z.get_name() ) ) {
            you.mod_moves( -100 );
            z.add_effect( effect_pet, 1_turns );
            z.friendly = -1;
            if( z.has_flag( MF_RIDEABLE_MECH ) ) {
                add_msg( m_good, _( "The %s whirs into life and opens its restraints to accept a pilot." ),
                         z.get_name() );
            } else {
                add_msg( m_good, _( "The %s begins to follow you." ),
                         z.get_name() );
            }
            you.use_amount( card_type, 1 );
            return true;
        }
    } else {
        add_msg( m_info, _( "You do not have the required %s to activate this." ),
                 item::nname( card_type ) );
    }
    return false;
}

static int prompt_for_amount( const char *const msg, const int max )
{
    const std::string formatted = string_format( msg, max );
    const int amount = string_input_popup()
                       .title( formatted )
                       .width( 20 )
                       .text( std::to_string( max ) )
                       .only_digits( true )
                       .query_int();

    return clamp( amount, 0, max );
}

bool monexamine::pay_bot( monster &z )
{
    avatar &you = get_avatar();
    time_duration friend_time = z.get_effect_dur( effect_pet );
    const int charge_count = you.charges_of( itype_cash_card );

    int amount = 0;
    uilist bot_menu;
    bot_menu.text = string_format(
                        _( "Welcome to the %s Friendship Interface.  What would you like to do?\n"
                           "Your current friendship will last: %s" ), z.get_name(), to_string( friend_time ) );
    if( charge_count > 0 ) {
        bot_menu.addentry( 1, true, 'b', _( "Get more friendship.  10 cents/min" ) );
    } else {
        bot_menu.addentry( 2, true, 'q',
                           _( "Sadly you're not currently able to extend your friendship.  - Quit menu" ) );
    }
    bot_menu.query();
    switch( bot_menu.ret ) {
        case 1:
            amount = prompt_for_amount(
                         vgettext( "How much friendship do you get?  Max: %d minute.  (0 to cancel)",
                                   "How much friendship do you get?  Max: %d minutes.", charge_count / 10 ), charge_count / 10 );
            if( amount > 0 ) {
                time_duration time_bought = time_duration::from_minutes( amount );
                you.use_charges( itype_cash_card, amount * 10 );
                z.add_effect( effect_pet, time_bought );
                z.add_effect( effect_paid, time_bought );
                z.friendly = -1;
                popup( _( "Your friendship grows stronger!\n This %s will follow you for %s." ), z.get_name(),
                       to_string( z.get_effect_dur( effect_pet ) ) );
                return true;
            }
            break;
        case 2:
            break;
    }

    return false;
}

bool monexamine::mfriend_menu( monster &z )
{
    enum choices {
        push_monster = 0,
        rename,
        reload_weapons,
        unload_weapons,
        change_orders,
        disable_pet,
        attack
    };

    uilist amenu;
    const std::string pet_name = z.get_name();
    const auto mon_item_id = z.type->revert_to_itype;

    amenu.text = string_format( _( "What to do with your %s?" ), pet_name );

    amenu.addentry( push_monster, true, 'p', _( "Push %s" ), pet_name );
    amenu.addentry( rename, true, 'e', _( "Rename" ) );
    if( has_reloadable_ammo( z ) ) {
        amenu.addentry( reload_weapons, true, 'R', reload_menu_text( get_avatar(), z ) );
        if( has_loaded_ammo( z ) ) {
            amenu.addentry( unload_weapons, true, 'U', unload_menu_text( z ) );
        }
    }
    if( z.has_flag( MF_CAN_BE_ORDERED ) ) {
        if( z.has_effect( effect_docile ) ) {
            amenu.addentry( change_orders, true, 'O', _( "Order to engage targets" ), pet_name );
        } else {
            amenu.addentry( change_orders, true, 'O', _( "Order to ignore enemies and follow" ), pet_name );
        }
    }
    if( !mon_item_id.is_empty() && !z.has_flag( MF_PAY_BOT ) ) {
        amenu.addentry( disable_pet, true, 'D', _( "Deactivate the %s" ), pet_name );
    }
    amenu.addentry( attack, true, 'a', _( "Attack" ) );

    amenu.query();
    const int choice = amenu.ret;

    switch( choice ) {
        case push_monster:
            push( z );
            break;
        case rename:
            rename_pet( z );
            break;
        case reload_weapons:
            reload_monster_weapons( get_avatar(), z );
            break;
        case unload_weapons:
            unload_monster_weapons( get_avatar(), z );
            break;
        case change_orders:
            toggle_ignore_targets( z );
            break;
        case disable_pet:
            if( query_yn( _( "Really deactivate your %s?" ), pet_name ) ) {
                deactivate_pet( z );
            }
            break;
        case attack:
            if( query_yn( _( "You may be attacked!  Proceed?" ) ) ) {
                avatar_action::melee_attack_while_handling_manual_combat_mode( get_avatar(), z );
            }
            break;
        default:
            break;
    }

    return true;
}

void monexamine::attach_or_remove_saddle( monster &z )
{
    if( z.has_effect( effect_saddled ) ) {
        get_avatar().i_add( z.remove_tack_item() );
    } else {
        item *loc = tack_loc();

        if( !loc ) {
            add_msg( _( "Never mind." ) );
            return;
        }
        z.set_tack_item( loc->detach() );
    }
}

bool Character::can_mount( const monster &critter ) const
{
    auto status = get_mountable_status( critter );
    return status.can_mount();
}

mountable_status Character::get_mountable_status( const monster &critter ) const
{
    const auto &avoid = get_legacy_path_avoid();
    auto route = get_map().route( bub_pos(), critter.bub_pos(), get_legacy_pathfinding_settings(),
                                  avoid );

    if( route.empty() ) {
        return {};
    }

    mountable_status status{};
    status.mountable = critter.has_flag( MF_PET_MOUNTABLE )
                       && critter.friendly == -1
                       && !critter.has_effect( effect_ridden );
    status.skills = (
                        critter.has_effect( effect_saddled )
                        && get_skill_level( skill_survival ) >= 1
                    )
                    || get_skill_level( skill_survival ) >= 4;
    status.size = critter.get_size() >= ( get_size() + 1 );
    status.carry_weight = ( get_weight() + critter.get_carried_weight() ) <=
                          ( 4 * critter.weight_capacity() );

    return status;
}

void monexamine::mount_pet( monster &z )
{
    if( z.has_effect( effect_tied ) ) {
        untie_pet( z );
    }
    get_avatar().mount_creature( z );
}

void monexamine::push( monster &z )
{
    std::string pet_name = z.get_name();
    avatar &you = get_avatar();
    you.moves -= 30;

    add_msg( _( "You pushed the %s." ), pet_name );

    auto delta = z.bub_pos().xy() - you.bub_pos().xy();
    z.move_to( z.bub_pos() + delta );
}

void monexamine::rename_pet( monster &z )
{
    std::string unique_name = string_input_popup()
                              .title( _( "Enter new pet name:" ) )
                              .width( 20 )
                              .query_string();
    if( !unique_name.empty() ) {
        z.unique_name = unique_name;
    }
}

void monexamine::attach_bag_to( monster &z )
{
    std::string pet_name = z.get_name();

    auto filter = []( const item & it ) {
        return it.is_armor() && it.get_storage() > 0_ml;
    };

    avatar &you = get_avatar();
    item *loc = game_menus::inv::titled_filter_menu( filter, you, _( "Bag item" ), "", ITEM_RADIUS );

    if( !loc ) {
        add_msg( _( "Never mind." ) );
        return;
    }

    item &it = *loc;
    z.set_storage_item( it.detach( ) );
    add_msg( _( "You mount the %1$s on your %2$s." ), it.display_name(), pet_name );
    // Update encumbrance in case we were wearing it
    you.flag_encumbrance();
    you.moves -= 200;
}

void monexamine::remove_bag_from( monster &z )
{
    std::string pet_name = z.get_name();
    if( z.get_storage_item() ) {
        if( !z.get_items().empty() ) {
            dump_items( z );
        }
        avatar &you = get_avatar();
        add_msg( _( "You remove the %1$s from %2$s." ), z.get_storage_item()->display_name(), pet_name );
        get_map().add_item_or_charges( you.bub_pos(), z.remove_storage_item() );
        you.moves -= 200;
    } else {
        add_msg( m_bad, _( "Your %1$s doesn't have a bag!" ), pet_name );
    }
}

void monexamine::dump_items( monster &z )
{
    std::string pet_name = z.get_name();
    avatar &you = get_avatar();
    z.drop_items( you.bub_pos() );
    add_msg( _( "You dump the contents of the %s's bag on the ground." ), pet_name );
    you.moves -= 200;
}

bool monexamine::give_items_to( monster &z )
{
    std::string pet_name = z.get_name();
    if( !z.get_storage_item() ) {
        add_msg( _( "There is no container on your %s to put things in!" ), pet_name );
        return true;
    }

    item &storage = *z.get_storage_item();
    units::mass max_weight = z.weight_capacity() - z.get_carried_weight();
    units::volume max_volume = storage.get_storage() - z.get_carried_volume();
    avatar &you = get_avatar();
    drop_locations items = game_menus::inv::multidrop( you );
    drop_locations to_move;
    for( const drop_location &itq : items ) {
        item *it_copy = &*itq.loc;
        if( it_copy->count_by_charges() ) {
            it_copy = item::spawn_temporary( *it_copy );
            it_copy->charges = itq.count;
        }

        units::volume item_volume = it_copy->volume();
        units::mass item_weight = it_copy->weight();
        if( max_weight < item_weight ) {
            add_msg( _( "The %1$s is too heavy for the %2$s to carry." ), it_copy->tname(), pet_name );
            continue;
        } else if( max_volume < item_volume ) {
            add_msg( _( "The %1$s is too big to fit in the %2$s." ), it_copy->tname(), storage.tname() );
            continue;
        } else {
            max_weight -= item_weight;
            max_volume -= item_volume;
            to_move.insert( to_move.end(), itq );
        }
    }
    z.add_effect( effect_ai_waiting, 2_turns );
    you.drop( to_move, z.bub_pos(), true );

    return false;
}

void monexamine::take_items_from( monster &z )
{
    const std::string pet_name = z.get_name();
    const std::vector<item *> &monster_inv = z.get_items();
    if( monster_inv.empty() ) {
        return;
    }

    int i = 0;
    uilist selection_menu;
    selection_menu.text = string_format( _( "Select an item to remove from the %s." ), pet_name );
    selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "Cancel" ) );
    for( auto iter : monster_inv ) {
        selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "Retrieve %s" ), iter->tname() );
    }
    selection_menu.selected = 1;
    selection_menu.query();
    const int index = selection_menu.ret;
    if( index == 0 || index == UILIST_CANCEL || index < 0 ||
        index > static_cast<int>( monster_inv.size() ) ) {
        return;
    }

    // because the first entry is the cancel option
    const int selection = index - 1;
    item *retrieved_item = monster_inv[selection];
    detached_ptr<item> detached = z.remove_item( retrieved_item );

    add_msg( _( "You remove the %1$s from the %2$s's bag." ), retrieved_item->tname(), pet_name );

    avatar &you = get_avatar();
    you.i_add( std::move( detached ) );
}

bool monexamine::add_armor( monster &z )
{
    std::string pet_name = z.get_name();
    item *loc = pet_armor_loc( z );

    if( !loc ) {
        add_msg( _( "Never mind." ) );
        return true;
    }

    item &armor = *loc;
    units::mass max_weight = z.weight_capacity() - z.get_carried_weight();
    if( max_weight <= armor.weight() ) {
        add_msg( pgettext( "pet armor", "Your %1$s is too heavy for your %2$s." ), armor.tname( 1 ),
                 pet_name );
        return true;
    }

    armor.set_var( "pet_armor", "true" );
    z.set_armor_item( loc->detach() );
    add_msg( pgettext( "pet armor", "You put the %1$s on your %2$s." ), armor.display_name(),
             pet_name );
    // TODO: armoring a horse takes a lot longer than 2 seconds. This should be a long action.
    get_avatar().moves -= 200;
    return true;
}

void monexamine::remove_harness( monster &z )
{
    z.remove_effect( effect_harnessed );
    add_msg( m_info, _( "You unhitch %s from the vehicle." ), z.get_name() );
}

void monexamine::remove_armor( monster &z )
{
    std::string pet_name = z.get_name();
    if( item *armor = z.get_armor_item() ) {
        get_map().add_item_or_charges( z.bub_pos(), z.remove_armor_item() );
        add_msg( pgettext( "pet armor", "You remove the %1$s from %2$s." ), armor->display_name(),
                 pet_name );
        // TODO: removing armor from a horse takes a lot longer than 2 seconds. This should be a long action.
        get_avatar().moves -= 200;
    } else {
        add_msg( m_bad, _( "Your %1$s isn't wearing armor!" ), pet_name );
    }
}

void monexamine::play_with( monster &z )
{
    std::string pet_name = z.get_name();
    avatar &you = get_avatar();
    const int turns = rng( 50, 125 ) * 100;
    you.assign_activity( ACT_PLAY_WITH_PET, turns );
    you.activity->monsters.push_back( g->shared_from( z ) );
    you.activity->str_values.push_back( pet_name );
    z.add_effect( effect_ai_waiting, time_duration::from_turns( turns ) );
    z.on_pet_bonding( you.as_character() );
}

void monexamine::train_pet( monster &z )
{
    avatar &you = get_avatar();
    std::string pet_name = z.get_name();
    you.assign_activity( ACT_TRAIN_PET, to_moves<int>( 60_minutes ) );
    you.activity->monsters.push_back( g->shared_from( z ) );
    you.activity->str_values.push_back( pet_name );
    z.add_effect( effect_ai_waiting, 60_minutes );
}

void monexamine::kill_zslave( monster &z )
{
    avatar &you = get_avatar();
    you.add_msg_if_player( _( "With a clean cut you put your %s down." ), z.get_name() );
    z.die( &you ); // execute it cleanly without damaging the corpse

    you.moves -= 150;
}

void monexamine::add_leash( monster &z )
{
    if( z.has_effect( effect_leashed ) ) {
        return;
    }
    Character &player = get_player_character();
    std::vector<item *> rope_inv = player.items_with( []( const item & it ) {
        return it.has_flag( json_flag_TIE_UP );
    } );

    if( rope_inv.empty() ) {
        return;
    }
    int i = 0;
    uilist selection_menu;
    selection_menu.text = string_format( _( "Select an item to leash your %s with." ), z.get_name() );
    selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "Cancel" ) );
    for( const item *iter : rope_inv ) {
        selection_menu.addentry( i++, true, MENU_AUTOASSIGN, _( "Use %s" ), iter->tname() );
    }
    selection_menu.selected = 1;
    selection_menu.query();
    int index = selection_menu.ret;
    if( index == 0 || index == UILIST_CANCEL || index < 0 ||
        index > static_cast<int>( rope_inv.size() ) ) {
        return;
    }
    item *rope_item = rope_inv[index - 1];
    z.set_tied_item( rope_item->split( 1 ) );
    z.add_effect( effect_leashed, 1_turns );
    z.get_effect( effect_leashed ).set_permanent();
    add_msg( _( "You add a leash to your %s." ), z.get_name() );
}

void monexamine::remove_leash( monster &z )
{
    if( !z.has_effect( effect_leashed ) ) {
        return;
    }
    z.remove_effect( effect_led_by_leash );
    z.remove_effect( effect_leashed );

    if( z.get_tied_item() ) {
        item *it = z.get_tied_item();
        get_player_character().i_add( it->detach() );

    }
    add_msg( _( "You remove the leash from your %s." ), z.get_name() );
}

void monexamine::tie_pet( monster &z )
{
    if( z.has_effect( effect_tied ) ) {
        return;
    }
    z.add_effect( effect_tied, 1_turns );
    z.get_effect( effect_tied ).set_permanent();
    add_msg( _( "You tie your %s." ), z.get_name() );
}

void monexamine::untie_pet( monster &z )
{
    if( !z.has_effect( effect_tied ) ) {
        return;
    }
    z.remove_effect( effect_tied );
    if( !z.has_effect( effect_leashed ) ) {
        // migration code dealing with animals tied before leashing was introduced
        z.add_effect( effect_leashed, 1_turns );
        z.get_effect( effect_leashed ).set_permanent();
    }
    add_msg( _( "You untie your %s." ), z.get_name() );
}

void monexamine::start_leading( monster &z )
{
    if( z.has_effect( effect_led_by_leash ) ) {
        return;
    }
    if( z.has_effect( effect_tied ) ) {
        monexamine::untie_pet( z );
    }
    z.add_effect( effect_led_by_leash, 1_turns );
    z.get_effect( effect_led_by_leash ).set_permanent();

    add_msg( _( "You take hold of the %s's leash to make it follow you." ), z.get_name() );
}

void monexamine::toggle_ignore_targets( monster &z )
{
    if( z.has_effect( effect_docile ) ) {
        z.remove_effect( effect_docile );
        add_msg( _( "You order the %s to engage targets." ), z.get_name() );
        return;
    } else {
        z.add_effect( effect_docile, 1_turns );
        add_msg( _( "You order the %s to focus on following you." ), z.get_name() );
        return;
    }
}

void monexamine::stop_leading( monster &z )
{
    if( !z.has_effect( effect_led_by_leash ) ) {
        return;
    }
    z.remove_effect( effect_led_by_leash );
    // The pet may or may not stop following so don't print that here
    add_msg( _( "You release the %s's leash." ), z.get_name() );
}

void monexamine::deactivate_pet( monster &z )
{
    if( z.has_effect( effect_has_bag ) ) {
        remove_bag_from( z );
    }
    if( z.has_effect( effect_monster_armor ) ) {
        remove_armor( z );
    }
    if( z.has_effect( effect_tied ) ) {
        untie_pet( z );
    }
    if( z.has_effect( effect_leashed ) ) {
        remove_leash( z );
    }
    if( z.has_effect( effect_saddled ) ) {
        attach_or_remove_saddle( z );
    }
    if( z.get_battery_item() ) {
        remove_battery( z );
    }
    map &here = get_map();
    here.add_item_or_charges( z.bub_pos(), z.to_item() );
    if( !z.has_flag( MF_INTERIOR_AMMO ) ) {
        for( auto &ammodef : z.ammo ) {
            if( ammodef.second > 0 ) {
                here.spawn_item( z.bub_pos(), ammodef.first, 1, ammodef.second, calendar::turn );
            }
        }
    }
    get_avatar().moves -= 100;
    g->remove_zombie( z );
}

void monexamine::milk_source( monster &source_mon )
{
    itype_id milked_item = source_mon.type->starting_ammo.begin()->first;
    auto milkable_ammo = source_mon.ammo.find( milked_item );
    if( milkable_ammo == source_mon.ammo.end() ) {
        debugmsg( "The %s has no milkable %s.", source_mon.get_name(), milked_item.str() );
        return;
    }
    avatar &you = get_avatar();
    if( milkable_ammo->second > 0 ) {
        const int moves = to_moves<int>( time_duration::from_minutes( milkable_ammo->second / 2 ) );
        you.assign_activity( ACT_MILK, moves, -1 );
        you.activity->coords.push_back( source_mon.abs_pos() );
        // pin the cow in place if it isn't already
        bool temp_tie = !source_mon.has_effect( effect_tied );
        if( temp_tie ) {
            source_mon.add_effect( effect_tied, 1_turns );
            you.activity->str_values.emplace_back( "temp_tie" );
        }
        add_msg( _( "You milk the %s." ), source_mon.get_name() );
    } else {
        add_msg( _( "The %s has no more milk." ), source_mon.get_name() );
    }
}
