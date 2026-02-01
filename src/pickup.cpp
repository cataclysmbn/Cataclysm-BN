#include "pickup.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "activity_actor.h"
#include "activity_actor_definitions.h"
#include "auto_pickup.h"
#include "avatar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "color.h"
#include "coordinates.h"
#include "coordinate_conversions.h"
#include "cursesdef.h"
#include "debug.h"
#include "drop_token.h"
#include "enums.h"
#include "faction.h"
#include "game.h"
#include "input.h"
#include "int_id.h"
#include "item.h"
#include "item_contents.h"
#include "item_search.h"
#include "item_stack.h"
#include "json.h"
#include "line.h"
#include "make_static.h"
#include "map.h"
#include "map_selector.h"
#include "mapdata.h"
#include "messages.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "panels.h"
#include "pickup_token.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "popup.h"
#include "ret_val.h"
#include "rot.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "units.h"
#include "units_utility.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "vpart_position.h"

static const trait_id trait_DEBUG_STORAGE( "DEBUG_STORAGE" );

using item_count = std::pair<item *, int>;
using pickup_map = std::map<std::string, item_count>;

static void show_pickup_message( const pickup_map &mapPickup );
static std::optional<tripoint_abs_omt> get_note_pos_from_item( const item &it );
static void maybe_remove_favorite_drop_note( const tripoint_abs_omt &note_pos,
        const std::string &item_name );

struct pickup_count {
    bool pick = false;
    // nullopt if the whole stack is being picked up, nonzero otherwise.
    std::optional<int> count;
    std::optional<size_t> parent;
    std::vector<size_t> children;
    bool all_children_picked = false;
};

static bool select_autopickup_items( const std::vector<std::list<item_stack::iterator>> &here,
                                     std::vector<pickup_count> &getitem )
{
    bool found_something = false;

    //Loop through Items lowest Volume first
    bool do_pickup = false;

    for( size_t rounded_volume = 0, num_checked = 0; num_checked < here.size(); rounded_volume++ ) {
        for( size_t i = 0; i < here.size(); i++ ) {
            do_pickup = false;
            item *begin = *here[i].front();
            if( begin->volume() / units::legacy_volume_factor == static_cast<int>( rounded_volume ) ) {
                num_checked++;
                const std::string item_name = begin->tname( 1, false );

                //Check the Pickup Rules
                if( get_auto_pickup().check_item( item_name ) == RULE_WHITELISTED ) {
                    do_pickup = true;
                } else if( begin->is_container() && !begin->is_container_empty() &&
                           get_auto_pickup().check_item( begin->get_contained().tname( 1, false ) ) == RULE_WHITELISTED ) {
                    do_pickup = true;
                } else if( get_auto_pickup().check_item( item_name ) != RULE_BLACKLISTED ) {
                    //No prematched pickup rule found
                    //check rules in more detail
                    get_auto_pickup().create_rule( begin );

                    if( get_auto_pickup().check_item( item_name ) == RULE_WHITELISTED ) {
                        do_pickup = true;
                    }
                }

                //Auto Pickup all items with Volume <= AUTO_PICKUP_VOL_LIMIT * 50 and Weight <= AUTO_PICKUP_ZERO * 50
                //items will either be in the autopickup list ("true") or unmatched ("")
                if( !do_pickup ) {
                    int weight_limit = get_option<int>( "AUTO_PICKUP_WEIGHT_LIMIT" );
                    int volume_limit = get_option<int>( "AUTO_PICKUP_VOL_LIMIT" );
                    if( weight_limit && volume_limit ) {
                        if( begin->volume() <= units::from_milliliter( volume_limit * 50 ) &&
                            begin->weight() <= weight_limit * 50_gram &&
                            get_auto_pickup().check_item( item_name ) != RULE_BLACKLISTED ) {
                            do_pickup = true;
                        }
                    }
                }
            }

            if( do_pickup ) {
                getitem[i].pick = true;
                found_something = true;
            }
        }
    }
    return found_something;
}

enum pickup_answer : int {
    CANCEL = -1,
    WIELD,
    WEAR,
    SPILL,
    EMPTY,
    STASH,
    NUM_ANSWERS
};

static pickup_answer handle_problematic_pickup( const item &it, bool &offered_swap,
        bool has_children, const std::string &explain )
{
    if( offered_swap ) {
        return CANCEL;
    }

    player &u = g->u;

    uilist amenu;

    amenu.text = explain;

    offered_swap = true;
    // TODO: Gray out if not enough hands
    // TODO: Calculate required hands vs freed hands
    if( it.is_armor() ) {
        amenu.addentry( WEAR, u.can_wear( it ).success(), 'W', _( "Wear %s" ), it.display_name() );
    }
    if( u.is_armed() ) {
        amenu.addentry( WIELD, !u.primary_weapon().has_flag( STATIC( flag_id( "NO_UNWIELD" ) ) ), 'w',
                        _( "Dispose of %s and wield %s" ), u.primary_weapon().display_name(),
                        it.display_name() );
    } else {
        amenu.addentry( WIELD, true, 'w', _( "Wield %s" ), it.display_name() );
    }
    if( has_children ) {
        // TODO: Fix problematic pickup due to child weight when parent alone is also too heavy
        // Maybe-TODO: A short summary of contained items
        amenu.addentry( EMPTY, u.can_pick_volume( it ), 'e', _( "Pick up just %s, without contents" ),
                        it.display_name() );
    }
    if( it.is_bucket_nonempty() ) {
        amenu.addentry( SPILL, u.can_pick_volume( it ), 's', _( "Spill %s, then pick up %s" ),
                        it.contents.front().tname(), it.display_name() );
    }

    amenu.query();
    int choice = amenu.ret;

    if( choice <= CANCEL || choice >= NUM_ANSWERS ) {
        return CANCEL;
    }

    return static_cast<pickup_answer>( choice );
}

bool pickup::query_thief()
{
    player &u = g->u;
    const bool force_uc = get_option<bool>( "FORCE_CAPITAL_YN" );
    const auto &allow_key = force_uc ? input_context::disallow_lower_case
                            : input_context::allow_all_keys;
    std::string answer = query_popup()
                         .allow_cancel( false )
                         .context( "YES_NO_ALWAYS_NEVER" )
                         .message( "%s", force_uc
                                   ? _( "Picking up this item will be considered stealing, continue?  (Case sensitive)" )
                                   : _( "Picking up this item will be considered stealing, continue?" ) )
                         .option( "YES", allow_key ) // yes, steal all items in this location that is selected
                         .option( "NO", allow_key ) // no, pick up only what is free
                         .option( "ALWAYS", allow_key ) // Yes, steal all items and stop asking me this question
                         .option( "NEVER", allow_key ) // no, only grab free item and never ask me again
                         .cursor( 1 ) // default to the second option `NO`
                         .query()
                         .action; // retrieve the input action
    if( answer == "YES" ) {
        u.set_value( "THIEF_MODE", "THIEF_STEAL" );
        u.set_value( "THIEF_MODE_KEEP", "NO" );
        return true;
    } else if( answer == "NO" ) {
        u.set_value( "THIEF_MODE", "THIEF_HONEST" );
        u.set_value( "THIEF_MODE_KEEP", "NO" );
        return false;
    } else if( answer == "ALWAYS" ) {
        u.set_value( "THIEF_MODE", "THIEF_STEAL" );
        u.set_value( "THIEF_MODE_KEEP", "YES" );
        return true;
    } else if( answer == "NEVER" ) {
        u.set_value( "THIEF_MODE", "THIEF_HONEST" );
        u.set_value( "THIEF_MODE_KEEP", "YES" );
        return false;
    } else {
        // error
        debugmsg( "Not a valid option [ %s ]", answer );
    }
    return false;
}

// Returns false if pickup caused a prompt and the player selected to cancel pickup
static bool pick_one_up( pickup::pick_drop_selection &selection, bool &got_water,
                         bool &offered_swap,
                         pickup_map &map_pickup, bool autopickup )
{
    player &u = get_avatar();
    int moves_taken = 100;
    bool picked_up = false;
    pickup_answer option = CANCEL;

    // We already checked in do_pickup if this was a nullptr
    item *loc = &*selection.target;
    const bool note_item_favorite = loc->is_favorite;
    const std::string note_item_name = loc->display_name();
    const std::optional<tripoint_abs_omt> note_item_pos = note_item_favorite
            ? get_note_pos_from_item( *loc )
            : std::nullopt;

    const std::optional<int> &quantity = selection.quantity;
    // If the faction would murder you on sight, we no longer care about stealing from them since it can't make things worse
    if( !loc->is_owned_by( g->u, true ) && loc->get_owner()->likes_u >= -10 ) {
        // Has the player given input on if stealing is ok?
        if( u.get_value( "THIEF_MODE" ) == "THIEF_ASK" ) {
            pickup::query_thief();
        }
        if( u.get_value( "THIEF_MODE" ) == "THIEF_HONEST" ) {
            return true; // Since we are honest, return no problem before picking up
        }
    }

    if( loc->invlet != '\0' &&
        u.invlet_to_item( loc->invlet ) != nullptr ) {
        // Existing invlet is not re-usable, remove it and let the code in player.cpp/inventory.cpp
        // add a new invlet, otherwise keep the (usable) invlet.
        loc->invlet = '\0';
    }

    std::vector<safe_reference<item>> &children = selection.children;
    units::volume children_volume = std::accumulate( children.begin(), children.end(), 0_ml,
    []( units::volume acc, const safe_reference<item> &c ) {
        return acc + c->volume();
    } );
    units::mass children_weight = std::accumulate( children.begin(), children.end(), 0_gram,
    []( units::mass acc, const safe_reference<item> &c ) {
        return acc + c->weight();
    } );

    bool did_prompt = false;

    auto with_det = [&]( detached_ptr<item> &&newloc ) {

        // Ammo can sometimes be picked up into containers
        newloc = u.i_add_to_container( std::move( newloc ), false );

        if( !newloc || ( newloc->count_by_charges() && newloc->charges == 0 ) ) {
            // We've picked up everything into containers, skip the options part
            picked_up = true;
            option = NUM_ANSWERS;
        } else if( newloc->made_of( LIQUID ) ) {
            got_water = true;
        } else if( !u.can_pick_weight( newloc->weight() + children_weight, false ) ) {
            if( !autopickup ) {
                const std::string &explain = string_format( _( "The %s is too heavy!" ),
                                             newloc->display_name() );
                option = handle_problematic_pickup( *newloc, offered_swap, !children.empty(), explain );
                did_prompt = true;
            } else {
                option = CANCEL;
            }
        } else if( newloc->is_bucket() && !newloc->is_container_empty() ) {
            if( !autopickup ) {
                const std::string &explain = string_format( _( "Can't stash %s while it's not empty" ),
                                             newloc->display_name() );
                option = handle_problematic_pickup( *newloc, offered_swap, !children.empty(), explain );
                did_prompt = true;
            } else {
                option = CANCEL;
            }
        } else if( !u.can_pick_volume( newloc->volume() + children_volume ) ) {
            if( !autopickup ) {
                const std::string &explain = string_format( _( "Not enough capacity to stash %s" ),
                                             newloc->display_name() );
                option = handle_problematic_pickup( *newloc, offered_swap, !children.empty(), explain );
                did_prompt = true;
            } else {
                option = CANCEL;
            }
        } else {
            option = STASH;
        }


        switch( option ) {
            case NUM_ANSWERS:
                // Some other option
                break;
            case CANCEL:
                picked_up = false;
                break;
            case WEAR:
                newloc = u.wear_item( std::move( newloc ) );
                picked_up = !newloc;
                break;
            case WIELD: {
                const ret_val<bool> wield_check = u.can_wield( *newloc );
                if( wield_check.success() ) {

                    newloc = u.wield( std::move( newloc ) );

                    picked_up = !newloc;
                    if( u.primary_weapon().invlet ) {
                        add_msg( m_info, _( "Wielding %c - %s" ), u.primary_weapon().invlet,
                                 u.primary_weapon().display_name() );
                    } else {
                        add_msg( m_info, _( "Wielding - %s" ), u.primary_weapon().display_name() );
                    }
                } else {
                    add_msg( m_neutral, "%s", wield_check.c_str() );
                }
                break;
            }
            case SPILL: {
                if( newloc->is_container_empty() ) {
                    debugmsg( "Tried to spill contents from an empty container" );
                    break;
                }
                //using original item, possibly modifying it
                picked_up = newloc->spill_contents( u );
                if( !picked_up ) {
                    break;
                }
            }
            // Intentional fallthrough
            case EMPTY:
            // Handled later
            case STASH: {
                auto &entry = map_pickup[newloc->tname()];
                entry.second += newloc->count();
                entry.first = &*newloc;
                u.i_add( std::move( newloc ) );
                picked_up = true;
                break;
            }
        }
        // NOLINTNEXTLINE(bugprone-use-after-move)
        return std::move( newloc );
    };

    if( quantity && loc->count_by_charges() ) {
        loc->attempt_split( *quantity, with_det );
    } else {
        loc->attempt_detach( with_det );
    }


    if( picked_up ) {
        // Children have to be picked up first, since removing parent would re-index the stack
        if( option != EMPTY ) {
            for( safe_reference<item> &child_loc : children ) {
                item &added = *child_loc;
                const bool child_favorite = added.is_favorite;
                const std::string child_note_name = added.display_name();
                const std::optional<tripoint_abs_omt> child_note_pos = child_favorite
                        ? get_note_pos_from_item( added )
                        : std::nullopt;
                auto &pickup_entry = map_pickup[added.tname()];
                pickup_entry.first = &added;
                pickup_entry.second += added.count();
                u.i_add( added.detach() );
                if( child_favorite && child_note_pos ) {
                    maybe_remove_favorite_drop_note( *child_note_pos, child_note_name );
                }
            }
        }
        u.moves -= moves_taken;
        if( note_item_favorite && note_item_pos ) {
            maybe_remove_favorite_drop_note( *note_item_pos, note_item_name );
        }
    }

    return picked_up || !did_prompt;
}

namespace pickup
{

bool do_pickup( std::vector<pick_drop_selection> &targets, bool autopickup )
{
    bool got_water = false;
    Character &u = get_avatar();
    bool weight_was_okay = ( u.weight_carried() <= u.weight_capacity() );
    bool volume_was_okay = ( u.volume_carried() <= u.volume_capacity() );
    bool offered_swap = false;

    // Map of items picked up so we can output them all at the end and
    // merge dropping items with the same name.
    pickup_map map_pickup;

    bool problem = false;
    while( !problem && u.get_moves() >= 0 && !targets.empty() ) {
        pick_drop_selection current_target = targets.back();
        // Whether we pick the item up or not, we're done trying to do so,
        // so remove it from the list.
        targets.pop_back();
        if( !current_target.target ) {
            debugmsg( "lost target item of ACT_PICKUP" );
            continue;
        }

        // TODO: This invocation is very ugly, should get a proper structure or something
        problem = !pick_one_up( current_target, got_water, offered_swap, map_pickup, autopickup );
    }

    if( !map_pickup.empty() ) {
        show_pickup_message( map_pickup );
    }

    if( got_water ) {
        add_msg( m_info, _( "You can't pick up a liquid!" ) );
    }
    if( weight_was_okay && u.weight_carried() > u.weight_capacity() ) {
        add_msg( m_bad, _( "You're overburdened!" ) );
    }
    if( volume_was_okay && u.volume_carried() > u.volume_capacity() ) {
        add_msg( m_bad, _( "You struggle to carry such a large volume!" ) );
    }

    return !problem;
}

static std::vector<std::optional<size_t>> calculate_parents(
        const std::vector<std::list<item_stack::iterator>> &stacked_here )
{
    std::vector<std::optional<size_t>> parents( stacked_here.size() );
    if( !stacked_here.empty() ) {
        size_t last_parent_index = 0;
        item_drop_token last_parent_token = *( *stacked_here.front().front() )->drop_token;
        for( size_t i = 1; i < stacked_here.size(); i++ ) {
            auto item_iter = *stacked_here[i].front();
            const item_drop_token &this_token = *item_iter->drop_token;
            if( this_token.is_child_of( last_parent_token ) ) {
                parents[i] = last_parent_index;
            } else {
                last_parent_token = this_token;
                last_parent_index = i;
            }
        }
    }

    return parents;
}

struct parent_child_check_t {
    bool parent_exists = false;
    bool child_exists = false;
};

struct unstacked_items {
    std::optional<item_stack::iterator> parent;
    std::list<item_stack::iterator> unstacked_children;
};

std::vector<stacked_items> stack_for_pickup_ui( const
        std::vector<item_stack::iterator> &unstacked )
{
    const std::pair<time_point, int> no_parent = std::make_pair(
                calendar::before_time_starts, 0 );
    std::map<std::pair<time_point, int>, parent_child_check_t> parent_child_check;
    // First, we need to check which parent-child groups exist
    for( const item_stack::iterator &it : unstacked ) {
        const auto &token = *( *it )->drop_token;
        if( token.drop_number > 0 ) {
            std::pair<time_point, int> turn_and_drop = std::make_pair( token.turn, token.drop_number );
            parent_child_check[turn_and_drop].parent_exists = true;
        }
        if( token.parent_number != token.drop_number && token.parent_number > 0 ) {
            std::pair<time_point, int> turn_and_parent = std::make_pair( token.turn, token.parent_number );
            parent_child_check[turn_and_parent].child_exists = true;
        }
    }

    // Second pass: we group children and parents together, but only if both sides are known to exist
    std::map<std::pair<time_point, int>, unstacked_items> children_by_parent;
    for( const item_stack::iterator &it : unstacked ) {
        const auto &token = *( *it )->drop_token;
        std::pair<time_point, int> turn_and_drop = std::make_pair( token.turn, token.drop_number );
        if( token.drop_number > 0 && parent_child_check[turn_and_drop].child_exists ) {
            children_by_parent[turn_and_drop].parent = it;
            continue;
        }

        std::pair<time_point, int> turn_and_parent = std::make_pair( token.turn, token.parent_number );
        if( token.parent_number > 0 && token.parent_number != token.drop_number &&
            parent_child_check[turn_and_parent].parent_exists ) {
            children_by_parent[turn_and_parent].unstacked_children.push_back( it );
        } else {
            children_by_parent[no_parent].unstacked_children.push_back( it );
        }
    }

    std::vector<stacked_items> restacked_with_parents;
    for( const auto &pr : children_by_parent ) {
        std::vector<std::list<item_stack::iterator>> restacked_children;
        for( const item_stack::iterator &it : pr.second.unstacked_children ) {
            bool found_stack = false;
            for( std::list<item_stack::iterator> &stack : restacked_children ) {
                const item &stack_top = **stack.front();
                if( stack_top.display_stacked_with( **it ) ) {
                    stack.push_back( it );
                    found_stack = true;
                    break;
                }
            }
            if( !found_stack ) {
                restacked_children.emplace_back( std::list<item_stack::iterator>( { it } ) );
            }
        }

        // Each sub-stack has to be sorted separately
        std::ranges::sort( restacked_children,
        []( const std::list<item_stack::iterator> &lhs, const std::list<item_stack::iterator> &rhs ) {
            return **lhs.front() < **rhs.front();
        } );
        restacked_with_parents.emplace_back( stacked_items{ pr.second.parent, restacked_children } );
    }

    // Sorting by parent is a bit arbitrary (parent-less go last) - sort by count?
    std::ranges::sort( restacked_with_parents,
    []( const stacked_items & lhs, stacked_items & rhs ) {
        return lhs.parent.has_value() && ( !rhs.parent.has_value() || **lhs.parent < **rhs.parent );
    } );


    return restacked_with_parents;
}

std::vector<std::list<item_stack::iterator>> flatten( const std::vector<stacked_items> &stacked )
{
    std::vector<std::list<item_stack::iterator>> flat;
    for( const stacked_items &s : stacked ) {
        if( s.parent ) {
            flat.emplace_back( std::list<item_stack::iterator>( { *s.parent } ) );
        }

        flat.insert( flat.end(), s.stacked_children.begin(), s.stacked_children.end() );
    }

    return flat;
}

} // namespace pickup

// Pick up items at (pos).
void pickup::pick_up( const tripoint &p, int min, from_where get_items_from )
{
    int cargo_part = -1;

    const optional_vpart_position vp = g->m.veh_at( p );
    vehicle *const veh = veh_pointer_or_null( vp );
    bool from_vehicle = false;

    if( min != -1 ) {
        if( veh != nullptr && get_items_from == prompt ) {
            const std::optional<vpart_reference> carg = vp.part_with_feature( "CARGO", false );
            const bool veh_has_items = carg && !veh->get_items( carg->part_index() ).empty();
            const bool map_has_items = g->m.has_items( p );
            if( veh_has_items && map_has_items ) {
                uilist amenu( _( "Get items from where?" ), { _( "Get items from vehicle cargo" ), _( "Get items on the ground" ) } );
                if( amenu.ret == UILIST_CANCEL ) {
                    return;
                }
                get_items_from = static_cast<from_where>( amenu.ret );
            } else if( veh_has_items ) {
                get_items_from = from_cargo;
            }
        }
        if( get_items_from == from_cargo ) {
            const std::optional<vpart_reference> carg = vp.part_with_feature( "CARGO", false );
            cargo_part = carg ? carg->part_index() : -1;
            from_vehicle = cargo_part >= 0;
        } else {
            // Nothing to change, default is to pick from ground anyway.
            if( g->m.has_flag( "SEALED", p ) ) {
                return;
            }
        }
    }

    if( !from_vehicle ) {
        bool isEmpty = ( g->m.i_at( p ).empty() );

        // Hide the pickup window if this is a toilet and there's nothing here
        // but non-frozen water.
        if( ( !isEmpty ) && g->m.furn( p ) == f_toilet ) {
            isEmpty = true;
            for( const item * const &maybe_water : g->m.i_at( p ) ) {
                if( maybe_water->typeId() != itype_id( "water" ) ) {
                    isEmpty = false;
                    break;
                }
            }
        }

        if( isEmpty && ( min != -1 || !get_option<bool>( "AUTO_PICKUP_ADJACENT" ) ) ) {
            return;
        }
    }

    // which items are we grabbing?
    std::vector<item_stack::iterator> here;
    if( from_vehicle ) {
        vehicle_stack vehitems = veh->get_items( cargo_part );
        for( item_stack::iterator it = vehitems.begin(); it != vehitems.end(); ++it ) {
            here.push_back( it );
        }
    } else {
        map_stack mapitems = g->m.i_at( p );
        for( item_stack::iterator it = mapitems.begin(); it != mapitems.end(); ++it ) {
            here.push_back( it );
        }
    }

    if( min == -1 ) {
        // Recursively pick up adjacent items if that option is on.
        if( get_option<bool>( "AUTO_PICKUP_ADJACENT" ) && g->u.pos() == p ) {
            //Autopickup adjacent
            direction adjacentDir[8] = {direction::NORTH, direction::NORTHEAST, direction::EAST, direction::SOUTHEAST, direction::SOUTH, direction::SOUTHWEST, direction::WEST, direction::NORTHWEST};
            for( auto &elem : adjacentDir ) {

                tripoint apos = tripoint( displace_XY( elem ), 0 );
                apos += p;

                pick_up( apos, min );
            }
        }

        // Bail out if this square cannot be auto-picked-up
        if( g->check_zone( zone_type_id( "NO_AUTO_PICKUP" ), p ) ) {
            return;
        } else if( g->m.has_flag( "SEALED", p ) ) {
            return;
        }
    }

    // Not many items, just grab them
    if( static_cast<int>( here.size() ) <= min && min != -1 ) {
        if( from_vehicle ) {
            g->u.assign_activity( std::make_unique<player_activity>( std::make_unique<pickup_activity_actor>(
            std::vector<pickup::pick_drop_selection> { { *here.front(), std::nullopt, {} } },
            std::nullopt ) ) );
        } else {
            g->u.assign_activity( std::make_unique<player_activity>( std::make_unique<pickup_activity_actor>(
            std::vector<pickup::pick_drop_selection> { { *here.front(), std::nullopt, {} } },
            g->u.pos() ) ) );
        }
        return;
    }

    const std::vector<stacked_items> &stacked_here_new = stack_for_pickup_ui( here );
    // To avoid having to rewrite things.
    // TODO: Remove flattening
    const std::vector<std::list<item_stack::iterator>> &stacked_here = flatten( stacked_here_new );
    std::vector<pickup_count> getitem( stacked_here.size() );
    std::vector<std::optional<size_t>> parents = calculate_parents( stacked_here );
    for( size_t i = 0; i < getitem.size(); i++ ) {
        getitem[i].parent = parents[i];
        if( parents[i] ) {
            getitem[*parents[i]].children.push_back( i );
        }
    }

    if( min == -1 ) { //Auto Pickup, select matching items
        if( !select_autopickup_items( stacked_here, getitem ) ) {
            // If we didn't find anything, bail out now.
            return;
        }
    } else {
        g->temp_exit_fullscreen();

        int start = 0;
        int selected = 0;
        int maxitems = 0;
        int pickupH = 0;
        int pickupW = 44;
        int pickupX = 0;
        catacurses::window w_pickup;
        catacurses::window w_item_info;

        ui_adaptor ui;
        ui.on_screen_resize( [&]( ui_adaptor & ui ) {
            const int itemsH = std::min( 25, TERMY / 2 );
            const int pickupBorderRows = 3;

            // The pickup list may consume the entire terminal, minus space needed for its
            // header/footer and the item info window.
            const int minleftover = itemsH + pickupBorderRows;
            const int maxmaxitems = TERMY - minleftover;
            const int minmaxitems = 9;
            maxitems = clamp<int>( stacked_here.size(), minmaxitems, maxmaxitems );

            start = selected - selected % maxitems;

            pickupH = maxitems + pickupBorderRows;

            //find max length of item name and resize pickup window width
            for( const std::list<item_stack::iterator> &cur_list : stacked_here ) {
                const item &this_item = **cur_list.front();
                const int item_len = utf8_width( remove_color_tags( this_item.display_name() ) ) + 10;
                if( item_len > pickupW && item_len < TERMX ) {
                    pickupW = item_len;
                }
            }

            pickupX = 0;
            std::string position = get_option<std::string>( "PICKUP_POSITION" );
            if( position == "left" ) {
                pickupX = panel_manager::get_manager().get_width_left();
            } else if( position == "right" ) {
                pickupX = TERMX - panel_manager::get_manager().get_width_right() - pickupW;
            } else if( position == "overlapping" ) {
                if( get_option<std::string>( "SIDEBAR_POSITION" ) == "right" ) {
                    pickupX = TERMX - pickupW;
                }
            }

            w_pickup = catacurses::newwin( pickupH, pickupW, point( pickupX, 0 ) );
            w_item_info = catacurses::newwin( TERMY - pickupH, pickupW,
                                              point( pickupX, pickupH ) );

            ui.position( point( pickupX, 0 ), point( pickupW, TERMY ) );
        } );
        ui.mark_resize();

        std::optional<int> itemcount;

        std::string action;
        int raw_input_char = ' ';
        input_context ctxt( "PICKUP" );
        ctxt.register_action( "UP" );
        ctxt.register_action( "DOWN" );
        ctxt.register_action( "RIGHT" );
        ctxt.register_action( "LEFT" );
        ctxt.register_action( "NEXT_TAB", to_translation( "Next page" ) );
        ctxt.register_action( "PREV_TAB", to_translation( "Previous page" ) );
        ctxt.register_action( "SCROLL_UP" );
        ctxt.register_action( "SCROLL_DOWN" );
        ctxt.register_action( "CONFIRM" );
        ctxt.register_action( "SELECT_ALL" );
        ctxt.register_action( "QUIT", to_translation( "Cancel" ) );
        ctxt.register_action( "ANY_INPUT" );
        ctxt.register_action( "HELP_KEYBINDINGS" );
        ctxt.register_action( "FILTER" );
#if defined(__ANDROID__)
        ctxt.allow_text_entry = true; // allow user to specify pickup amount
#endif

        bool update = true;
        int iScrollPos = 0;

        std::string filter;
        std::string new_filter;
        // Indexes of items that match the filter
        std::vector<int> matches;
        bool filter_changed = true;

        units::mass weight_predict = 0_gram;
        units::volume volume_predict = 0_ml;

        const std::string all_pickup_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ:;";

        ui.on_redraw( [&]( const ui_adaptor & ) {
            const item &selected_item = **stacked_here[matches[selected]].front();

            if( selected >= 0 && selected <= static_cast<int>( stacked_here.size() ) - 1 ) {
                item *loc = *stacked_here[matches[selected]].front();
                temperature_flag temperature = rot::temperature_flag_for_location( get_map(), *loc );

                std::vector<iteminfo> this_item = selected_item.info( temperature );

                item_info_data dummy( {}, {}, this_item, {}, iScrollPos );
                dummy.without_getch = true;
                dummy.without_border = true;

                draw_item_info( w_item_info, dummy );
            } else {
                werase( w_item_info );
                wnoutrefresh( w_item_info );
            }
            draw_custom_border( w_item_info, 0 );

            // print info window title: < item name >
            mvwprintw( w_item_info, point( 2, 0 ), "< " );
            trim_and_print( w_item_info, point( 4, 0 ), pickupW - 8, selected_item.color_in_inventory(),
                            selected_item.display_name() );
            wprintw( w_item_info, " >" );
            wnoutrefresh( w_item_info );

            const std::string pickup_chars = ctxt.get_available_single_char_hotkeys( all_pickup_chars );

            werase( w_pickup );
            for( int cur_it = start; cur_it < start + maxitems; cur_it++ ) {
                if( cur_it < static_cast<int>( matches.size() ) ) {
                    int true_it = matches[cur_it];
                    const item &this_item = **stacked_here[true_it].front();
                    nc_color icolor = this_item.color_in_inventory();
                    if( cur_it == selected ) {
                        icolor = hilite( c_white );
                    }

                    if( cur_it < static_cast<int>( pickup_chars.size() ) ) {
                        mvwputch( w_pickup, point( 0, 1 + ( cur_it % maxitems ) ), icolor,
                                  static_cast<char>( pickup_chars[cur_it] ) );
                    } else if( cur_it < static_cast<int>( pickup_chars.size() ) + static_cast<int>
                               ( pickup_chars.size() ) *
                               static_cast<int>( pickup_chars.size() ) ) {
                        int p = cur_it - pickup_chars.size();
                        int p1 = p / pickup_chars.size();
                        int p2 = p % pickup_chars.size();
                        mvwprintz( w_pickup, point( 0, 1 + ( cur_it % maxitems ) ), icolor, "`%c%c",
                                   static_cast<char>( pickup_chars[p1] ), static_cast<char>( pickup_chars[p2] ) );
                    } else {
                        mvwputch( w_pickup, point( 0, 1 + ( cur_it % maxitems ) ), icolor, ' ' );
                    }
                    if( getitem[true_it].parent ) {
                        const pickup_count &parent = getitem[*getitem[true_it].parent];
                        nc_color color = parent.pick ?
                                         ( parent.all_children_picked ? c_light_blue : c_yellow ) :
                                         c_dark_gray;
                        // TODO: Cute symbol here
                        wprintz( w_pickup, color, "\\" );
                    } else {
                        wprintw( w_pickup, " " );
                    }
                    if( getitem[true_it].pick ) {
                        if( getitem[true_it].count ) {
                            wprintz( w_pickup, c_light_blue, "# " );
                        } else {
                            wprintz( w_pickup, c_light_blue, "+ " );
                        }
                    } else {
                        wprintw( w_pickup, "- " );
                    }
                    std::string item_name;
                    if( ( *stacked_here[true_it].front() )->is_money() ) {
                        //Count charges
                        // TODO: transition to the item_location system used for the inventory
                        unsigned int charges_total = 0;
                        for( const item_stack::iterator &it : stacked_here[true_it] ) {
                            charges_total += ( *it )->charges;
                        }
                        //Picking up none or all the cards in a stack
                        if( !getitem[true_it].pick || !getitem[true_it].count ) {
                            item_name = ( *stacked_here[true_it].front() )->display_money( stacked_here[true_it].size(),
                                        charges_total );
                        } else {
                            unsigned int charges = 0;
                            int item_count = getitem[true_it].count ? *getitem[true_it].count : 0;
                            int c = item_count;
                            for( std::list<item_stack::iterator>::const_iterator it = stacked_here[true_it].begin();
                                 it != stacked_here[true_it].end() && c > 0; ++it, --c ) {
                                charges += ( **it )->charges;
                            }

                            item_name = ( *stacked_here[true_it].front() )->display_money( item_count, charges_total, charges );
                        }
                    } else {
                        item_name = this_item.display_name( stacked_here[true_it].size() );
                    }
                    if( stacked_here[true_it].size() > 1 ) {
                        item_name = string_format( "%d %s", stacked_here[true_it].size(), item_name );
                    }
                    if( get_option<bool>( "ITEM_SYMBOLS" ) ) {
                        item_name = string_format( "%s %s", this_item.symbol().c_str(),
                                                   item_name );
                    }

                    // if the item does not belong to your fraction then add the stolen symbol
                    if( !this_item.is_owned_by( g->u, true ) ) {
                        item_name = string_format( "<color_light_red>!</color> %s", item_name );
                    }

                    trim_and_print( w_pickup, point( 6, 1 + ( cur_it % maxitems ) ), pickupW - 4, icolor,
                                    item_name );
                }
            }

            mvwprintw( w_pickup, point( 0, maxitems + 1 ), _( "[%s] Unmark" ),
                       ctxt.get_desc( "LEFT", 1 ) );

            center_print( w_pickup, maxitems + 1, c_light_gray, string_format( _( "[%s] Help" ),
                          ctxt.get_desc( "HELP_KEYBINDINGS", 1 ) ) );

            right_print( w_pickup, maxitems + 1, 0, c_light_gray, string_format( _( "[%s] Mark" ),
                         ctxt.get_desc( "RIGHT", 1 ) ) );

            mvwprintw( w_pickup, point( 0, maxitems + 2 ), _( "[%s] Prev" ),
                       ctxt.get_desc( "PREV_TAB", 1 ) );

            center_print( w_pickup, maxitems + 2, c_light_gray, string_format( _( "[%s] All" ),
                          ctxt.get_desc( "SELECT_ALL", 1 ) ) );

            right_print( w_pickup, maxitems + 2, 0, c_light_gray, string_format( _( "[%s] Next" ),
                         ctxt.get_desc( "NEXT_TAB", 1 ) ) );

            const std::string fmted_weight_predict = colorize(
                        string_format( "%.1f", round_up( convert_weight( weight_predict ), 1 ) ),
                        weight_predict > g->u.weight_capacity() ? c_red : c_white );
            const std::string fmted_weight_capacity = string_format(
                        "%.1f", round_up( convert_weight( g->u.weight_capacity() ), 1 ) );
            const std::string fmted_volume_predict = colorize(
                        format_volume( volume_predict ),
                        volume_predict > g->u.volume_capacity() ? c_red : c_white );
            const std::string fmted_volume_capacity = format_volume( g->u.volume_capacity() );

            trim_and_print( w_pickup, point_zero, pickupW, c_white,
                            string_format( _( "PICK Wgt %1$s/%2$s  Vol %3$s/%4$s" ),
                                           fmted_weight_predict, fmted_weight_capacity,
                                           fmted_volume_predict, fmted_volume_capacity ) );

            wnoutrefresh( w_pickup );
        } );

        // Now print the two lists; those on the ground and about to be added to inv
        // Continue until we hit return or space
        do {
            const std::string pickup_chars = ctxt.get_available_single_char_hotkeys( all_pickup_chars );
            int idx = -1;

            if( action == "ANY_INPUT" &&
                raw_input_char >= '0' && raw_input_char <= '9' ) {
                int raw_input_char_value = static_cast<char>( raw_input_char ) - '0';
                if( !itemcount ) {
                    itemcount.emplace( 0 );
                }
                *itemcount *= 10;
                *itemcount += raw_input_char_value;
                if( *itemcount < 0 ) {
                    *itemcount = 0;
                }
                if( *itemcount == 0 ) {
                    itemcount.reset();
                }
            } else if( action == "SCROLL_UP" ) {
                iScrollPos--;
            } else if( action == "SCROLL_DOWN" ) {
                iScrollPos++;
            } else if( action == "PREV_TAB" ) {
                if( start > 0 ) {
                    start -= maxitems;
                } else {
                    start = static_cast<int>( ( matches.size() - 1 ) / maxitems ) * maxitems;
                }
                selected = start;
            } else if( action == "NEXT_TAB" ) {
                if( start + maxitems < static_cast<int>( matches.size() ) ) {
                    start += maxitems;
                } else {
                    start = 0;
                }
                iScrollPos = 0;
                selected = start;
            } else if( action == "UP" ) {
                selected--;
                iScrollPos = 0;
                if( selected < 0 ) {
                    selected = matches.size() - 1;
                    start = static_cast<int>( matches.size() / maxitems ) * maxitems;
                    if( start >= static_cast<int>( matches.size() ) ) {
                        start -= maxitems;
                    }
                } else if( selected < start ) {
                    start -= maxitems;
                }
            } else if( action == "DOWN" ) {
                selected++;
                iScrollPos = 0;
                if( selected >= static_cast<int>( matches.size() ) ) {
                    selected = 0;
                    start = 0;
                } else if( selected >= start + maxitems ) {
                    start += maxitems;
                }
            } else if( selected >= 0 && selected < static_cast<int>( matches.size() ) &&
                       ( ( action == "RIGHT" && !getitem[matches[selected]].pick ) ||
                         ( action == "LEFT" && getitem[matches[selected]].pick ) ) ) {
                idx = selected;
            } else if( action == "FILTER" ) {
                new_filter = filter;
                string_input_popup popup;
                popup
                .title( _( "Set filter" ) )
                .width( 30 )
                .edit( new_filter );
                if( !popup.canceled() ) {
                    filter_changed = true;
                }
            } else if( action == "ANY_INPUT" && raw_input_char == '`' ) {
                std::string ext = string_input_popup()
                                  .title( _( "Enter 2 letters (case sensitive):" ) )
                                  .width( 3 )
                                  .max_length( 2 )
                                  .query_string();
                if( ext.size() == 2 ) {
                    int p1 = pickup_chars.find( ext.at( 0 ) );
                    int p2 = pickup_chars.find( ext.at( 1 ) );
                    if( p1 != -1 && p2 != -1 ) {
                        idx = pickup_chars.size() + ( p1 * pickup_chars.size() ) + p2;
                    }
                }
            } else if( action == "ANY_INPUT" ) {
                idx = ( raw_input_char <= 127 ) ? pickup_chars.find( raw_input_char ) : -1;
                iScrollPos = 0;
            } else if( action == "SELECT_ALL" ) {
                int count = 0;
                for( auto i : matches ) {
                    if( getitem[i].pick ) {
                        count++;
                    }
                    getitem[i].pick = true;
                    getitem[i].count.reset();
                    // TODO: What about containers with children?
                    // TODO: Recalc all_children_picked
                }
                if( count == static_cast<int>( stacked_here.size() ) ) {
                    for( size_t i = 0; i < stacked_here.size(); i++ ) {
                        getitem[i].pick = false;
                        getitem[i].all_children_picked = false;
                    }
                }
                update = true;
            }

            if( idx >= 0 && idx < static_cast<int>( matches.size() ) ) {
                size_t true_idx = matches[idx];
                pickup_count &selected_stack = getitem[true_idx];
                if( itemcount || selected_stack.count ) {
                    const item &temp = **stacked_here[true_idx].front();
                    int amount_available = temp.count_by_charges() ? temp.charges : stacked_here[true_idx].size();
                    if( itemcount && *itemcount >= amount_available ) {
                        itemcount.reset();
                    }
                    selected_stack.count = itemcount;
                    itemcount.reset();
                }

                // Note: this might not change the value of getitem[idx] at all!
                selected_stack.pick = ( action == "RIGHT" ? true :
                                        ( action == "LEFT" ? false :
                                          !selected_stack.pick ) );
                if( action != "RIGHT" && action != "LEFT" ) {
                    selected = idx;
                    start = ( idx / maxitems ) * maxitems;
                }

                if( !selected_stack.pick ) {
                    selected_stack.count.reset();
                }
                selected_stack.all_children_picked = selected_stack.pick;
                for( size_t child_index : selected_stack.children ) {
                    pickup_count &child_stack = getitem[child_index];
                    child_stack.pick = selected_stack.pick;
                    child_stack.count.reset();
                }
                if( selected_stack.parent ) {
                    pickup_count &parent_stack = getitem[*selected_stack.parent];
                    if( selected_stack.pick ) {
                        parent_stack.all_children_picked = std::ranges::all_of(
                                                               parent_stack.children,
                        [&]( size_t child_index ) {
                            return getitem[child_index].pick;
                        } );
                    } else {
                        parent_stack.all_children_picked = false;
                    }
                }

                update = true;
            }
            if( filter_changed ) {
                matches.clear();
                while( matches.empty() ) {
                    auto filter_func = item_filter_from_string( new_filter );
                    for( size_t index = 0; index < stacked_here.size(); index++ ) {
                        if( filter_func( **stacked_here[index].front() ) ) {
                            matches.push_back( index );
                        }
                    }
                    if( matches.empty() ) {
                        popup( _( "Your filter returned no results" ) );
                        // The filter must have results, or simply be emptied or canceled,
                        // as this screen can't be reached without there being
                        // items available
                        string_input_popup popup;
                        popup
                        .title( _( "Set filter" ) )
                        .width( 30 )
                        .edit( new_filter );
                        if( popup.canceled() ) {
                            new_filter = filter;
                            filter_changed = false;
                        }
                    }
                }
                if( filter_changed ) {
                    filter = new_filter;
                    filter_changed = false;
                    selected = 0;
                    start = 0;
                    iScrollPos = 0;
                }
            }

            if( update ) { // Update weight & volume information
                update = false;
                units::mass weight_picked_up = 0_gram;
                units::volume volume_picked_up = 0_ml;
                for( size_t i = 0; i < getitem.size(); i++ ) {
                    if( getitem[i].pick ) {
                        // Make a copy for calculating weight/volume
                        item &temp = *item::spawn_temporary( **stacked_here[i].front() );
                        if( temp.count_by_charges() && getitem[i].count && *getitem[i].count < temp.charges ) {
                            temp.charges = *getitem[i].count;
                        }
                        int num_picked = std::min( stacked_here[i].size(),
                                                   getitem[i].count ? *getitem[i].count : stacked_here[i].size() );
                        weight_picked_up += temp.weight() * num_picked;
                        volume_picked_up += temp.volume() * num_picked;
                    }
                }

                weight_predict = g->u.weight_carried() + weight_picked_up;
                volume_predict = g->u.volume_carried() + volume_picked_up;
            }

            ui_manager::redraw();
            action = ctxt.handle_input();
            raw_input_char = ctxt.get_raw_input().get_first_input();

        } while( action != "QUIT" && action != "CONFIRM" );

        bool item_selected = false;
        // Check if we have selected an item.
        for( const pickup_count &selection : getitem ) {
            if( selection.pick ) {
                item_selected = true;
            }
        }
        if( action != "CONFIRM" || !item_selected ) {
            add_msg( _( "Never mind." ) );
            g->reenter_fullscreen();
            return;
        }
    }

    // At this point we've selected our items, register an activity to pick them up.
    std::vector<std::pair<item_stack::iterator, int>> pick_values;
    for( size_t i = 0; i < stacked_here.size(); i++ ) {
        const pickup_count &selection = getitem[i];
        if( !selection.pick ) {
            continue;
        }

        const std::list<item_stack::iterator> &stack = stacked_here[i];
        // Note: items can be both charged and stacked
        // For robustness, let's assume they can be both in the same stack
        int count = selection.count ? *selection.count : 0;
        for( const item_stack::iterator &it : stack ) {
            if( selection.count && count == 0 ) {
                break;
            }

            if( ( *it )->count_by_charges() ) {
                int num_picked = std::min( ( *it )->charges, count );
                pick_values.emplace_back( it, num_picked );
                count -= num_picked;
            } else {
                pick_values.emplace_back( it, 0 );
                --count;
            }
        }
    }

    std::vector<item *> locations;
    std::vector<int> quantities;

    for( std::pair<item_stack::iterator, int> &iter_qty : pick_values ) {
        item *loc = *iter_qty.first;
        locations.push_back( loc );
        quantities.push_back( iter_qty.second );
    }

    std::vector<pickup::pick_drop_selection> targets = pickup::optimize_pickup( locations, quantities );
    g->u.assign_activity( std::make_unique<player_activity>( std::make_unique<pickup_activity_actor>
                          ( targets,
                            g->u.pos() ) ) );
    if( min == -1 ) {
        // Auto pickup will need to auto resume since there can be several of them on the stack.
        g->u.activity->auto_resume = true;
    }

    g->reenter_fullscreen();
}

//helper function for Pickup::pick_up
void show_pickup_message( const pickup_map &mapPickup )
{
    for( auto &entry : mapPickup ) {
        if( entry.second.first->invlet != 0 ) {
            add_msg( _( "You pick up: %d %s [%c]" ), entry.second.second,
                     entry.second.first->display_name( entry.second.second ), entry.second.first->invlet );
        } else {
            add_msg( _( "You pick up: %d %s" ), entry.second.second,
                     entry.second.first->display_name( entry.second.second ) );
        }
    }
}

static std::optional<tripoint_abs_omt> get_note_pos_from_item( const item &it )
{
    if( !it.has_position() ) {
        return std::nullopt;
    }
    const map &here = get_map();
    const tripoint abs_ms = here.getabs( it.position() );
    return tripoint_abs_omt( ms_to_omt_copy( abs_ms ) );
}

static void maybe_remove_favorite_drop_note( const tripoint_abs_omt &note_pos,
        const std::string &item_name )
{
    if( !get_option<bool>( "AUTO_NOTES_DROPPED_FAVORITES" ) ) {
        return;
    }
    if( !overmap_buffer.has_note( note_pos ) ) {
        return;
    }
    const std::string note_text = overmap_buffer.note( note_pos );
    std::vector<std::string> tokens = string_split( note_text, ';' );
    std::vector<std::string> kept;
    kept.reserve( tokens.size() );
    bool removed = false;
    for( std::string &token : tokens ) {
        std::string trimmed = trim_whitespaces( token );
        if( trimmed.empty() ) {
            continue;
        }
        if( trimmed == item_name ) {
            removed = true;
            continue;
        }
        kept.push_back( std::move( trimmed ) );
    }
    if( !removed ) {
        return;
    }
    if( kept.empty() || ( kept.size() == 1 && kept.front().starts_with( "SPRITE:" ) ) ) {
        overmap_buffer.delete_note( note_pos );
        return;
    }
    std::string updated = kept.front();
    for( size_t i = 1; i < kept.size(); ++i ) {
        updated += "; ";
        updated += kept[i];
    }
    overmap_buffer.add_note( note_pos, updated );
}

detached_ptr<item> pickup::handle_spillable_contents( Character &c, detached_ptr<item> &&it,
        map &m )
{
    if( it->is_bucket_nonempty() ) {
        const item &it_cont = it->contents.front();
        int num_charges = it_cont.charges;
        while( !it->spill_contents( c ) ) {
            if( num_charges > it_cont.charges ) {
                num_charges = it_cont.charges;
            } else {
                break;
            }
        }

        // If bucket is still not empty then player opted not to handle the
        // rest of the contents
        if( it->is_bucket_nonempty() ) {
            c.add_msg_player_or_npc(
                _( "To avoid spilling its contents, you set your %1$s on the %2$s." ),
                _( "To avoid spilling its contents, <npcname> sets their %1$s on the %2$s." ),
                it->display_name(), m.name( c.pos() )
            );
            m.add_item_or_charges( c.pos(), std::move( it ) );
            return detached_ptr<item>();
        }
    }

    return std::move( it );
}

int pickup::cost_to_move_item( const Character &who, const item &it )
{
    // Do not involve inventory capacity, it's not like you put it in backpack
    int ret = 50;
    if( who.is_armed() ) {
        // No free hand? That will cost you extra
        ret += 20;
    }
    if( who.has_trait( trait_DEBUG_STORAGE ) ) {
        return ret;
    }

    const int delta_weight = units::to_gram( it.weight() - who.weight_capacity() );
    // Is it too heavy? It'll take 10 moves per kg over limit
    ret += std::max( 0, delta_weight / 100 );

    // Keep it sane - it's not a long activity
    return std::min( 400, ret );
}

namespace pickup
{

void pick_drop_selection::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "target", target );
    jsout.member( "quantity", quantity );
    jsout.member( "children", children );

    jsout.end_object();
}

void pick_drop_selection::deserialize( JsonIn &jin )
{
    JsonObject jo = jin.get_object();
    jo.read( "target", target );
    jo.read( "quantity", quantity );
    jo.read( "children", children );
}

std::vector<pick_drop_selection> optimize_pickup( const std::vector<item *> &targets,
        const std::vector<int> &quantities )
{
    // This is essentially legacy code handling, so checks are good design
    if( targets.size() != quantities.size() ) {
        debugmsg( "Sizes of targets and quantity vectors don't match: %x != %x",
                  targets.size(), quantities.size() );
        return {};
    }
    item_drop_token last_token;
    std::vector<pick_drop_selection> optimized;
    for( size_t i = 0; i < targets.size(); i++ ) {
        item *loc = targets[i];
        // If it was possible, the two locations should be required to be consecutive
        if( loc->drop_token->is_child_of( last_token ) ) {
            optimized.back().children.emplace_back( loc );
        } else {
            last_token = *loc->drop_token;
            std::optional<int> q = quantities[i] != 0 ? quantities[i] : std::optional<int>();
            optimized.push_back( {loc, q, {}} );
        }
    }

    return optimized;
}

} // namespace pickup
