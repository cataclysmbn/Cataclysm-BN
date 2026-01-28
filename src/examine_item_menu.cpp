#include "examine_item_menu.h"

#include <vector>
#include <string>

#include "auto_pickup.h"
#include "avatar_action.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "crafting.h"
#include "game_inventory.h"
#include "map.h"
#include "input.h"
#include "item.h"
#include "item_functions.h"
#include "itype.h"
#include "messages.h"
#include "output.h"
#include "salvage.h"
#include "recipe_dictionary.h"
#include "rot.h"
#include "ui_manager.h"
//#include "handle_action.cpp"
#include "url_utility.h"
#include "options.h"
#include "ui.h"

struct action_entry {
    std::string action;
    std::function<bool()> on_select;
};

namespace examine_item_menu
{

bool run(
    item &loc,
    const std::function<int()> &func_pos_x,
    const std::function<int()> &func_width,
    menu_pos_t menu_pos
)
{
    avatar &you = get_avatar();
    item &itm = loc;

    // Sanity check
    if( !you.has_item( itm ) ) {
        return true;
    }

    catacurses::window w_info;

    input_context ctxt( "INVENTORY_ITEM" );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    int info_area_scroll_pos = 0;
    constexpr int info_area_scroll_step = 3;
    temperature_flag temperature = rot::temperature_flag_for_location( get_map(), itm );
    std::vector<iteminfo> item_info_vals = itm.info( temperature );
    std::vector<iteminfo> dummy_compare;
    item_info_data data( itm.tname(), itm.type_name(), item_info_vals, dummy_compare,
                         info_area_scroll_pos );

    // The following line stops the game from crashing. Why..?
    data.without_getch = true;

    const bool has_pickup_rule = get_auto_pickup().has_rule( &itm );
    const bool is_wielded = you.is_wielding( itm );
    const bool cant_unwield_if_weapon = is_wielded && !you.can_unwield( itm ).success();
    const bool cant_unwield_existing_weapon = you.is_armed() &&
            !you.can_unwield( you.primary_weapon() ).success();
    const bool cant_takeoff_if_worn = you.is_wearing( itm ) &&
                                      !you.can_takeoff( itm ).success();
    const bool cant_dispose_of = cant_unwield_if_weapon || cant_takeoff_if_worn;
    const bool cant_acquare = cant_unwield_existing_weapon || cant_takeoff_if_worn;
    const hint_rating rate_unwield_item = cant_dispose_of ? hint_rating::cant :
                                          hint_rating::good;
    const hint_rating rate_wield_item = cant_acquare ? hint_rating::cant :
                                        hint_rating::good;
    const hint_rating rate_drop_item = cant_dispose_of ? hint_rating::cant :
                                       hint_rating::good;

    std::vector<action_entry> actions;
    uilist action_list;

    /**
    Add a menu entry
    @param act the action name, corresponding to an action in data/raw/keybindings.json
    @param hint determines what color the text will be
    @param on_select lambda function for what the menu entry actually does
    @param number will be printed after the menu entry in parentheses, optional
                  INT_MIN is treated as a null value
    */
    const auto add_entry = [&]( const char *act, hint_rating hint, std::function<bool()> &&on_select,
    int number = INT_MIN ) {
        action_entry ae;
        ae.action = act;
        ae.on_select = std::move( on_select );
        actions.push_back( std::move( ae ) );

        ctxt.register_action( act );

        std::string bound_key = ctxt.key_bound_to( act );
        int bound_key_i = bound_key.size() == 1 ? bound_key[0] : '?';
        std::string act_name = ctxt.get_action_name( act );
        if( number == INT_MIN ) {
            action_list.addentry( actions.size(), true, bound_key_i, act_name );
        } else {
            action_list.addentry( actions.size(), true, bound_key_i,
                                  act_name + " (" + std::to_string( number ) + ")" );
        }

        auto &list_entry = action_list.entries.back();
        switch( hint ) {
            case hint_rating::cant:
                list_entry.text_color = c_light_gray;
                break;
            case hint_rating::iffy:
                list_entry.text_color = c_light_red;
                break;
            case hint_rating::good:
                list_entry.text_color = c_light_green;
                break;
            case hint_rating::blood:
                list_entry.text_color = c_red;
                break;
        }
    };

    add_entry( "ACTIVATE", rate_action_use( you, itm ), [&]() {
        avatar_action::use_item( you, &itm );
        return true;
    } );

    add_entry( "READ", rate_action_read( you, itm ), [&]() {
        you.read( &itm );
        return true;
    } );

    add_entry( "EAT", rate_action_eat( you, itm ), [&]() {
        avatar_action::eat( you, &itm );
        return true;
    } );

    add_entry( "WEAR", rate_action_wear( you, itm ), [&]() {
        you.wear_possessed( itm );
        return true;
    } );

    if( !is_wielded ) {
        add_entry( "WIELD", rate_wield_item, [&]() {
            avatar_action::wield( itm );
            return true;
        } );
    } else {
        add_entry( "UNWIELD", rate_unwield_item, [&]() {
            avatar_action::wield( itm );
            return true;
        } );
    }

    add_entry( "THROW", rate_drop_item, [&]() {
        avatar_action::plthrow( you, &itm );
        return true;
    } );

    add_entry( "CHANGE_SIDE", rate_action_change_side( you, itm ), [&]() {
        you.change_side( itm );
        return true;
    } );

    add_entry( "TAKE_OFF", rate_action_takeoff( you, itm ), [&]() {
        you.takeoff( itm );
        return true;
    } );

    add_entry( "DROP", rate_drop_item, [&]() {
        you.drop( itm, you.pos() );
        return true;
    } );

    add_entry( "UNLOAD", rate_action_unload( you, itm ), [&]() {
        avatar_funcs::unload_item( you, itm );
        return true;
    } );

    add_entry( "RELOAD", rate_action_reload( you, itm ), [&]() {
        avatar_action::reload( itm );
        return true;
    } );

    add_entry( "PART_RELOAD", rate_action_reload( you, itm ), [&]() {
        avatar_action::reload( itm, true );
        return true;
    } );

    add_entry( "MEND", rate_action_mend( you, itm ), [&]() {
        avatar_action::mend( you, &itm );
        return true;
    } );

    add_entry( "SALVAGE", rate_action_salvage( you, itm ), [&]() {
        salvage::prompt_salvage_single( you, itm );
        return true;
    } );

    add_entry( "DISASSEMBLE", rate_action_disassemble( you, itm ), [&]() {
        crafting::disassemble( you, itm );
        return true;
    } );

    if( itm.kill_count() > 0 ) {
        add_entry( "SHOW_KILL_LIST", hint_rating::blood, [&]() {
            itm.show_kill_list();
            return true;
        }, itm.kill_count() );
    }

    if( !itm.is_favorite ) {
        add_entry( "FAVORITE_ADD",
        hint_rating::good, [&]() {
            itm.is_favorite = true;
            return false;
        } );
    } else {
        add_entry( "FAVORITE_REMOVE",
        hint_rating::good, [&]() {
            itm.is_favorite = false;
            return false;
        } );
    }

    if( !get_option<std::string>( "HHG_URL" ).empty() ) {
        add_entry( "OPEN_ITEM_IN_HHG", hint_rating::good, [&]() {
            open_url( get_option<std::string>( "HHG_URL" ) + std::string( "/item/" ) + itm.typeId().c_str() +
                      std::string( "?t=UNDEAD_PEOPLE" ) );
            return false;
        } );
    }

    add_entry( "REASSIGN", hint_rating::good, [&]() {
        game_menus::inv::prompt_reassign_letter( you, itm );
        return false;
    } );

    if( !has_pickup_rule ) {
        add_entry( "AUTOPICKUP_ADD", hint_rating::good, [&]() {
            get_auto_pickup().add_rule( &itm );
            add_msg( m_info, _( "'%s' added to character pickup rules." ), itm.tname( 1, false ) );
            return false;
        } );
    } else {
        add_entry( "AUTOPICKUP_REMOVE", hint_rating::iffy, [&]() {
            get_auto_pickup().remove_rule( &itm );
            add_msg( m_info, _( "'%s' removed from character pickup rules." ), itm.tname( 1, false ) );
            return false;
        } );
    }

    int selected_action = 0;
    int num_actions = static_cast<int>( actions.size() );

    std::unique_ptr<ui_adaptor> ui = std::make_unique<ui_adaptor>();
    ui->on_screen_resize( [&]( ui_adaptor & ui ) {
        int width = func_width();
        int pos_x = func_pos_x();
        w_info = catacurses::newwin( TERMY, width, point( pos_x, 0 ) );
        ui.position_from_window( w_info );
    } );
    action_list.w_y_setup = 0;
    action_list.w_x_setup = [&]( const int popup_width ) -> int {
        switch( menu_pos )
        {
            default:
            case menu_pos_t::left:
                return func_pos_x() - popup_width;
            case menu_pos_t::right:
                return func_pos_x() + func_width();
        }
    };
    ui->mark_resize();
    ui->on_redraw( [&]( ui_adaptor & ui ) {
        draw_item_info( w_info, data );
        action_list.show( ui );
    } );

    bool exit = false;
    bool ret_val = true;
    while( !exit ) {
        ui->invalidate_ui();
        ui_manager::redraw();

        const std::string &action = ctxt.handle_input();

        if( action == "QUIT" || action == "LEFT" ) {
            ret_val = false;
            exit = true;
            continue;
        } else if( action == "CONFIRM" || action == "RIGHT" ) {
            ret_val = actions[selected_action].on_select();
            exit = true;
        } else if( action == "PAGE_UP" ) {
            info_area_scroll_pos -= info_area_scroll_step;
        } else if( action == "PAGE_DOWN" ) {
            info_area_scroll_pos += info_area_scroll_step;
        } else if( action == "SCROLL_UP" || action == "UP" ) {
            selected_action = ( selected_action - 1 + num_actions ) % num_actions;
            action_list.set_selected( selected_action );
        } else if( action == "SCROLL_DOWN" || action == "DOWN" ) {
            selected_action = ( selected_action + 1 + num_actions ) % num_actions;
            action_list.set_selected( selected_action );
        } else {
            for( action_entry &entry : actions ) {
                if( entry.action == action ) {
                    ret_val = entry.on_select();
                    exit = true;
                    break;
                }
            }
        }
    }
    return ret_val;
}

hint_rating rate_action_use( const avatar &you, const item &it )
{
    if( it.is_tool() ) {
        return it.ammo_sufficient() ? hint_rating::good : hint_rating::iffy;
    } else if( it.is_gunmod() ) {
        /** @EFFECT_GUN >0 allows rating estimates for gun modifications */
        if( you.get_skill_level( skill_id( "gun" ) ) == 0 ) {
            return hint_rating::iffy;
        } else {
            return hint_rating::good;
        }
    } else if( it.is_food() || it.is_medication() || it.is_book() || it.is_armor() ) {
        // The rating is subjective, could be argued as hint_rating::cant or hint_rating::good as well
        return hint_rating::iffy;
    } else if( it.type->has_use() ) {
        return hint_rating::good;
    } else if( !it.is_container_empty() ) {
        return rate_action_use( you, it.get_contained() );
    }

    return hint_rating::cant;
}

hint_rating rate_action_read( const avatar &you, const item &it )
{
    if( !it.is_book() ) {
        return hint_rating::cant;
    }

    if( !you.has_identified( it.typeId() ) ) {
        return hint_rating::good;
    }

    std::vector<std::string> dummy;
    return you.get_book_reader( it, dummy ) == nullptr ? hint_rating::iffy : hint_rating::good;
}

hint_rating rate_action_eat( const avatar &you, const item &it )
{
    if( !you.can_consume( it ) ) {
        return hint_rating::cant;
    }

    const ret_val<edible_rating> rating = you.will_eat( it );
    if( rating.success() ) {
        return hint_rating::good;
    } else if( rating.value() == edible_rating::inedible ||
               rating.value() == edible_rating::inedible_mutation ) {

        return hint_rating::cant;
    }

    return hint_rating::iffy;
}

hint_rating rate_action_wear( const avatar &you, const item &it )
{
    if( !it.is_armor() ) {
        return hint_rating::cant;
    }

    if( you.is_wearing( it ) ) {
        return hint_rating::iffy;
    }

    return you.can_wear( it ).success() ? hint_rating::good : hint_rating::iffy;
}

hint_rating rate_action_change_side( const avatar &you, const item &it )
{
    if( !you.is_worn( it ) ) {
        return hint_rating::iffy;
    }

    if( !it.is_sided() ) {
        return hint_rating::cant;
    }

    return hint_rating::good;
}

hint_rating rate_action_takeoff( const avatar &you, const item &it )
{
    if( !it.is_armor() ) {
        return hint_rating::cant;
    }

    if( you.is_worn( it ) && you.can_takeoff( it ).success() ) {
        return hint_rating::good;
    }

    return hint_rating::iffy;
}

hint_rating rate_action_reload( const avatar &you, const item &it )
{
    hint_rating res = hint_rating::cant;

    // Guns may contain additional reloadable mods so check these first
    for( const auto mod : it.gunmods() ) {
        switch( rate_action_reload( you, *mod ) ) {
            case hint_rating::good:
                return hint_rating::good;

            case hint_rating::blood:
                // This doesn't happen, but if it did, just pass the color hint along
                return hint_rating::blood;

            case hint_rating::cant:
                continue;

            case hint_rating::iffy:
                res = hint_rating::iffy;
        }
    }

    if( !it.is_reloadable() ) {
        return res;
    }

    return you.can_reload( it ) ? hint_rating::good : hint_rating::iffy;
}

hint_rating rate_action_unload( const avatar &/*you*/, const item &it )
{
    return item_funcs::can_be_unloaded( it ) ? hint_rating::good : hint_rating::cant;
}

hint_rating rate_action_mend( const avatar &/*you*/, const item &it )
{
    // TODO: check also if item damage could be repaired via a tool
    if( !it.faults.empty() ) {
        return hint_rating::good;
    }
    return it.faults_potential().empty() ? hint_rating::cant : hint_rating::iffy;
}

hint_rating rate_action_disassemble( avatar &you, const item &it )
{
    if( crafting::can_disassemble( you, it, you.crafting_inventory() ).success() ) {
        return hint_rating::good; // possible
    } else if( recipe_dictionary::get_uncraft( it.typeId() ) ) {
        return hint_rating::iffy; // potentially possible but we currently lack requirements
    } else {
        return hint_rating::cant; // never possible
    }
}

hint_rating rate_action_salvage( avatar &you, const item &it )
{
    //is_salvageable is much cheaper so we do it first
    if( !it.is_salvageable() ) {
        return hint_rating::cant; // never possible
    } else if( auto cache = you.crafting_inventory().get_quality_cache();
               salvage::try_salvage( it, cache ).success() ) {
        return hint_rating::good; // possible
    } else {
        return it.weight() < salvage::minimal_weight_to_cut( it )
               ? hint_rating::cant
               // potentially possible but we currently lack requirements
               : hint_rating::iffy;

    }
}

} // namespace examine_item_menu
