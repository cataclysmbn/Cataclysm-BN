#include "handle_liquid.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "action.h"
#include "avatar.h"
#include "cata_utility.h"
#include "debug.h"
#include "enums.h"
#include "fstream_utils.h"
#include "game.h"
#include "game_inventory.h"
#include "iexamine.h"
#include "item.h"
#include "item_contents.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "messages.h"
#include "monster.h"
#include "player_activity.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

static void serialize_liquid_source( player_activity &act, vehicle &veh,
                                     int part_id )
{
    act.values.push_back( LST_VEHICLE );
    act.values.push_back( part_id );
    act.coords.push_back( bub_to_abs( veh.bub_part_location( 0 ) ) );
}

static void serialize_liquid_source( player_activity &act, const tripoint_bub_ms &pos )
{
    act.values.push_back( LST_INFINITE_MAP );
    act.values.push_back( 0 ); //dummy
    act.coords.push_back( bub_to_abs( pos ) );
}

static void serialize_liquid_target( player_activity &act, const vehicle &veh )
{
    act.values.push_back( LTT_VEHICLE );
    act.coords.push_back( bub_to_abs( veh.bub_part_location( 0 ) ) );
}

static void serialize_liquid_target( player_activity &act, item &container )
{
    act.values.push_back( LTT_CONTAINER );
    act.targets.emplace_back( &container );
}

static void serialize_liquid_target( player_activity &act, const tripoint_bub_ms &pos )
{
    act.values.push_back( LTT_MAP );
    act.coords.push_back( bub_to_abs( pos ) );
}

namespace liquid_handler
{
void handle_all_liquid( detached_ptr<item> &&liquid, const int radius )
{
    // NOLINTNEXTLINE(bugprone-use-after-move)
    while( liquid ) {
        // handle_liquid allows to pour onto the ground, which will handle all the liquid and
        // set charges to 0. This allows terminating the loop.
        // The result of handle_liquid is ignored, the player *has* to handle all the liquid.
        handle_liquid( std::move( liquid ), radius );
    }
}

bool consume_liquid( item &liquid, const int radius )
{
    const auto original_charges = liquid.charges;
    while( liquid.charges > 0 && handle_liquid( liquid, radius ) ) {
        // try again with the remaining charges
    }
    return original_charges != liquid.charges;
}

bool consume_liquid( detached_ptr<item> &&liquid, const int radius )
{
    const auto original_charges = liquid->charges;
    // NOLINTNEXTLINE(bugprone-use-after-move)
    while( liquid && handle_liquid( std::move( liquid ), radius ) ) {
        // try again with the remaining charges
    }
    return !liquid || ( original_charges != liquid->charges );
}

static bool get_liquid_target( item &liquid, const int radius, liquid_dest_opt &target )
{
    if( !liquid.made_of( LIQUID ) ) {
        debugmsg( "Tried to handle_liquid a non-liquid!" );
        // "canceled by the user" because we *can* not handle it.
        return false;
    }

    uilist menu;

    map &here = get_map();
    const std::string liquid_name = liquid.display_name( liquid.charges );

    if( liquid.is_loaded() ) {
        menu.text = string_format( pgettext( "liquid", "What to do with the %1$s from %2$s?" ), liquid_name,
                                   liquid.describe_location() );
    } else {
        menu.text = string_format( pgettext( "liquid", "What to do with the %1$s?" ), liquid_name );
    }

    std::vector<std::function<void()>> actions;

    if( g->u.can_consume( liquid ) && ( !liquid.is_loaded() ||
                                        liquid.where() != item_location_type::monster ) ) {
        if( g->u.can_consume_for_bionic( liquid ) ) {
            menu.addentry( -1, true, 'e', _( "Fuel bionic with it" ) );
        } else {
            menu.addentry( -1, true, 'e', _( "Consume it" ) );
        }

        actions.emplace_back( [&]() {
            target.dest_opt = LD_CONSUME;
        } );
    }
    // This handles containers found anywhere near the player, including on the map and in vehicle storage.
    menu.addentry( -1, true, 'c', _( "Pour into a container" ) );
    actions.emplace_back( [&]() {
        target.it = game_menus::inv::container_for( g->u, liquid, radius );
        item *const cont = target.it;

        if( cont == nullptr || cont->is_null() ) {
            add_msg( _( "Never mind." ) );
            return;
        }
        // Sometimes the cont parameter is omitted, but the liquid is still within a container that counts
        // as valid target for the liquid. So check for that.
        if( cont == liquid.parent_item() || ( !cont->contents.empty() &&
                                              &cont->contents.front() == &liquid ) ) {
            add_msg( m_info, _( "That's the same container!" ) );
            return; // The user has intended to do something, but mistyped.
        }
        target.dest_opt = LD_ITEM;
    } );
    // This handles liquids stored in vehicle parts directly (e.g. tanks).
    std::set<vehicle *> opts;
    for( const auto &e : here.points_in_radius( g->u.bub_pos(), 1 ) ) {
        auto veh = veh_pointer_or_null( here.veh_at( e ) );
        if( veh ) {
            vehicle_part_range vpr = veh->get_all_parts();
            if( veh && std::any_of( vpr.begin(), vpr.end(), [&liquid]( const vpart_reference & pt ) {
            return pt.part().can_reload( &liquid );
            } ) ) {
                opts.insert( veh );
            }
        }
    }
    for( auto veh : opts ) {
        vehicle *source_veh = nullptr;
        if( liquid.has_position() && liquid.where() == item_location_type::vehicle ) {
            source_veh = veh_pointer_or_null( here.veh_at( liquid.position() ) );
        }
        if( veh == source_veh && veh->has_part( "FLUIDTANK", false ) ) {
            for( const vpart_reference &vp : veh->get_avail_parts( "FLUIDTANK" ) ) {
                if( vp.part().get_base().is_reloadable_with( liquid.typeId() ) ) {
                    menu.addentry( -1, true, MENU_AUTOASSIGN, _( "Fill avaliable tank" ) );
                    actions.emplace_back( [ &, veh]() {
                        target.veh = veh;
                        target.dest_opt = LD_VEH;
                    } );
                    break;
                }
            }
        } else {
            menu.addentry( -1, true, MENU_AUTOASSIGN, _( "Fill nearby vehicle %s" ), veh->name );
            actions.emplace_back( [ &, veh]() {
                target.veh = veh;
                target.dest_opt = LD_VEH;
            } );
        }
    }

    for( auto &target_pos : here.points_in_radius( g->u.bub_pos(), 1 ) ) {
        if( !iexamine::has_keg( target_pos ) ) {
            continue;
        }
        if( liquid.is_loaded() && liquid.where() == item_location_type::map &&
            liquid.position() == target_pos ) {
            continue;
        }
        const std::string dir = direction_name( direction_from( g->u.bub_pos(), target_pos ) );
        menu.addentry( -1, true, MENU_AUTOASSIGN, _( "Pour into an adjacent keg (%s)" ), dir );
        actions.emplace_back( [ &, target_pos]() {
            target.pos = target_pos;
            target.dest_opt = LD_KEG;
        } );
    }

    menu.addentry( -1, true, 'g', _( "Pour on the ground" ) );
    actions.emplace_back( [&]() {
        // From infinite source to the ground somewhere else. The target has
        // infinite space and the liquid can not be used from there anyway.
        if( liquid.has_infinite_charges() && liquid.is_loaded() &&
            liquid.where() == item_location_type::map ) {
            add_msg( m_info, _( "Clearing out the %s would take forever." ), here.name( liquid.position() ) );
            return;
        }

        const std::string liqstr = string_format( _( "Pour %s where?" ), liquid_name );

        const std::optional<tripoint_bub_ms> target_pos_ = choose_adjacent( liqstr );
        if( !target_pos_ ) {
            return;
        }
        target.pos = *target_pos_;

        if( liquid.is_loaded() && liquid.where() == item_location_type::map &&
            liquid.position() == target.pos ) {
            add_msg( m_info, _( "That's where you took it from!" ) );
            return;
        }
        if( !here.can_put_items_ter_furn( target.pos ) ) {
            add_msg( m_info, _( "You can't pour there!" ) );
            return;
        }
        target.dest_opt = LD_GROUND;
    } );

    if( liquid.rotten() ) {
        // Pre-select this one as it is the most likely one for rotten liquids
        menu.selected = menu.entries.size() - 1;
    }

    if( menu.entries.empty() ) {
        return false;
    }

    menu.query();
    if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= actions.size() ) {
        add_msg( _( "Never mind." ) );
        // Explicitly canceled all options (container, drink, pour).
        return false;
    }

    actions[menu.ret]();
    return true;
}

static bool perform_liquid_transfer( item &liquid, liquid_dest_opt &target )
{
    map &here = get_map();
    switch( target.dest_opt ) {
        case LD_CONSUME:
            liquid.attempt_split( 0, []( detached_ptr<item> &&it ) {
                return get_player_character().consume_item( std::move( it ) );
            } );
            return true;
        case LD_ITEM:
            liquid.attempt_split( 0, [&target]( detached_ptr<item> &&it ) {
                return get_player_character().pour_into( *target.it, std::move( it ) );
            } );
            g->u.mod_moves( -100 );
            return false;
        case LD_VEH:
            liquid.attempt_split( 0, [&target]( detached_ptr<item> &&det ) {
                return g->u.pour_into( *target.veh, std::move( det ) );
            } );

            g->u.mod_moves( -1000 ); // consistent with veh_interact::do_refill activity

            return false;
        case LD_KEG:
        case LD_GROUND:
            if( target.dest_opt == LD_KEG ) {
                liquid.attempt_split( 0, [&target]( detached_ptr<item> &&det ) {
                    return iexamine::pour_into_keg( target.pos, std::move( det ) );
                } );
            } else {
                here.add_item_or_charges( target.pos, liquid.detach() );
            }
            g->u.mod_moves( -100 );
            return false;
        case LD_NULL:
        default:
            return false;
    }
}

static bool perform_liquid_transfer( detached_ptr<item> &&liquid, liquid_dest_opt &target )
{
    map &here = get_map();
    Character &you = get_player_character();
    switch( target.dest_opt ) {
        case LD_CONSUME:
            liquid = you.consume_item( std::move( liquid ) );
            return true;
        case LD_ITEM:
            liquid = you.pour_into( *target.it, std::move( liquid ) );
            you.mod_moves( -100 );
            return true;
        case LD_VEH:
            liquid = you.pour_into( *target.veh, std::move( liquid ) );
            you.mod_moves( -1000 ); // consistent with veh_interact::do_refill activity
            return true;
        case LD_KEG:
        case LD_GROUND:
            if( target.dest_opt == LD_KEG ) {
                liquid = iexamine::pour_into_keg( target.pos, std::move( liquid ) );
            } else {
                here.add_item_or_charges( target.pos, std::move( liquid ) );
            }

            you.mod_moves( -100 );
            return true;
        case LD_NULL:
        default:
            return false;
    }
}
static bool perform_liquid_transfer( const tripoint_bub_ms &pos, liquid_dest_opt &target )
{
    map &here = get_map();
    switch( target.dest_opt ) {
        case LD_CONSUME:
            g->u.consume_item( here.water_from( pos ) );
            return true;
        case LD_ITEM:
            g->u.assign_activity( activity_id( "ACT_FILL_LIQUID" ) );
            serialize_liquid_source( *g->u.activity, pos );
            serialize_liquid_target( *g->u.activity, *target.it );
            return true;
        case LD_VEH:
            g->u.assign_activity( activity_id( "ACT_FILL_LIQUID" ) );
            serialize_liquid_source( *g->u.activity, pos );
            serialize_liquid_target( *g->u.activity, *target.veh );
            return true;
        case LD_KEG:
        case LD_GROUND:
            g->u.assign_activity( activity_id( "ACT_FILL_LIQUID" ) );
            serialize_liquid_source( *g->u.activity, pos );
            serialize_liquid_target( *g->u.activity, target.pos );
            return true;
        case LD_NULL:
        default:
            return false;
    }
}
static bool perform_liquid_transfer( vehicle *veh, int part_id, liquid_dest_opt &target )
{
    item &liquid = veh->part( part_id ).get_base().contents.back();
    switch( target.dest_opt ) {
        case LD_CONSUME:
            liquid.attempt_split( 0, []( detached_ptr<item> &&it ) {
                return g->u.consume_item( std::move( it ) );
            } );
            return true;
        case LD_ITEM:
            g->u.assign_activity( activity_id( "ACT_FILL_LIQUID" ) );
            serialize_liquid_source( *g->u.activity, *veh, part_id );
            serialize_liquid_target( *g->u.activity, *target.it );
            return true;
        case LD_VEH:
            g->u.assign_activity( activity_id( "ACT_FILL_LIQUID" ) );
            serialize_liquid_source( *g->u.activity, *veh, part_id );
            serialize_liquid_target( *g->u.activity, *target.veh );
            return true;
        case LD_KEG:
        case LD_GROUND:
            g->u.assign_activity( activity_id( "ACT_FILL_LIQUID" ) );
            serialize_liquid_source( *g->u.activity, *veh, part_id );
            serialize_liquid_target( *g->u.activity, target.pos );
            return true;
        case LD_NULL:
        default:
            return false;
    }
}

bool handle_liquid( detached_ptr<item> &&liquid, int radius )
{
    if( !liquid ) {
        return false;
    }
    if( liquid->made_of( SOLID ) ) {
        debugmsg( "Tried to handle_liquid a non-liquid!" );
        // "canceled by the user" because we *can* not handle it.
        return false;
    }
    if( !liquid->made_of( LIQUID ) ) {
        add_msg( _( "The %s froze solid before you could finish." ), liquid->tname() );
        return false;
    }
    struct liquid_dest_opt liquid_target;
    if( get_liquid_target( *liquid, radius, liquid_target ) ) {
        return perform_liquid_transfer( std::move( liquid ), liquid_target );
    }
    return false;
}

bool handle_liquid( item &liquid, const int radius )
{
    if( liquid.made_of( SOLID ) ) {
        debugmsg( "Tried to handle_liquid a non-liquid!" );
        // "canceled by the user" because we *can* not handle it.
        return false;
    }
    if( !liquid.made_of( LIQUID ) ) {
        add_msg( _( "The %s froze solid before you could finish." ), liquid.tname() );
        return false;
    }
    struct liquid_dest_opt liquid_target;
    if( get_liquid_target( liquid, radius, liquid_target ) ) {
        return perform_liquid_transfer( liquid, liquid_target );
    }
    return false;
}

bool handle_liquid( const tripoint_bub_ms &pos, int radius )
{
    map &here = get_map();
    detached_ptr<item> liquid = here.water_from( pos );
    if( !liquid || liquid->is_null() || liquid->made_of( SOLID ) ) {
        debugmsg( "Tried to handle_liquid a non-liquid!" );
        // "canceled by the user" because we *can* not handle it.
        return false;
    }

    if( !liquid->made_of( LIQUID ) ) {
        add_msg( _( "The %s froze solid before you could finish." ), liquid->tname() );
        return false;
    }
    struct liquid_dest_opt liquid_target;
    if( get_liquid_target( *liquid, radius, liquid_target ) ) {
        return perform_liquid_transfer( pos, liquid_target );
    }
    return false;
}
bool handle_liquid( vehicle *veh, int part_id, int radius )
{

    item &liquid = veh->part( part_id ).get_base().contents.back();

    if( liquid.is_null() || liquid.made_of( SOLID ) ) {
        debugmsg( "Tried to handle_liquid a non-liquid!" );
        // "canceled by the user" because we *can* not handle it.
        return false;
    }

    if( !liquid.made_of( LIQUID ) ) {
        add_msg( _( "The %s froze solid before you could finish." ), liquid.tname() );
        return false;
    }
    struct liquid_dest_opt liquid_target;
    if( get_liquid_target( liquid, radius, liquid_target ) ) {
        return perform_liquid_transfer( veh, part_id, liquid_target );
    }
    return false;
}

} // namespace liquid_handler
