#include "activity_handlers.h" // IWYU pragma: associated

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <ranges>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "cata_algo.h"
#include "character.h"
#include "character_functions.h"
#include "clzones.h"
#include "construction.h"
#include "construction_partial.h"
#include "creature.h"
#include "debug.h"
#include "drop_token.h"
#include "enums.h"
#include "field.h"
#include "field_type.h"
#include "fire.h"
#include "flag.h"
#include "flat_set.h"
#include "game.h"
#include "game_constants.h"
#include "iexamine.h"
#include "int_id.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"
#include "iuse.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "map_selector.h"
#include "mapdata.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "output.h"
#include "options.h"
#include "overmapbuffer.h"
#include "pickup.h"
#include "pickup_token.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "requirements.h"
#include "ret_val.h"
#include "rng.h"
#include "stomach.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_utils.h"
#include "translations.h"
#include "trap.h"
#include "units.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "vpart_position.h"
#include "weather.h"
#include "map_utils.h"

namespace views = std::views;

static const activity_id ACT_BUTCHER_FULL( "ACT_BUTCHER_FULL" );
static const activity_id ACT_CHOP_LOGS( "ACT_CHOP_LOGS" );
static const activity_id ACT_CHOP_PLANKS( "ACT_CHOP_PLANKS" );
static const activity_id ACT_CHOP_TREE( "ACT_CHOP_TREE" );
static const activity_id ACT_CHURN( "ACT_CHURN" );
static const activity_id ACT_FETCH_REQUIRED( "ACT_FETCH_REQUIRED" );
static const activity_id ACT_FISH( "ACT_FISH" );
static const activity_id ACT_JACKHAMMER( "ACT_JACKHAMMER" );
static const activity_id ACT_MOVE_LOOT( "ACT_MOVE_LOOT" );
static const activity_id ACT_MULTIPLE_BUTCHER( "ACT_MULTIPLE_BUTCHER" );
static const activity_id ACT_MULTIPLE_CHOP_PLANKS( "ACT_MULTIPLE_CHOP_PLANKS" );
static const activity_id ACT_MULTIPLE_CHOP_TREES( "ACT_MULTIPLE_CHOP_TREES" );
static const activity_id ACT_MULTIPLE_CONSTRUCTION( "ACT_MULTIPLE_CONSTRUCTION" );
static const activity_id ACT_MULTIPLE_FARM( "ACT_MULTIPLE_FARM" );
static const activity_id ACT_MULTIPLE_FISH( "ACT_MULTIPLE_FISH" );
static const activity_id ACT_MULTIPLE_MINE( "ACT_MULTIPLE_MINE" );
static const activity_id ACT_PICKAXE( "ACT_PICKAXE" );
static const activity_id ACT_TIDY_UP( "ACT_TIDY_UP" );
static const activity_id ACT_VEHICLE( "ACT_VEHICLE" );
static const activity_id ACT_VEHICLE_DECONSTRUCTION( "ACT_VEHICLE_DECONSTRUCTION" );
static const activity_id ACT_VEHICLE_REPAIR( "ACT_VEHICLE_REPAIR" );

static const efftype_id effect_ai_waiting( "ai_waiting" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_nausea( "nausea" );

static const itype_id itype_battery( "battery" );
static const itype_id itype_log( "log" );
static const itype_id itype_soldering_iron( "soldering_iron" );
static const itype_id itype_welder( "welder" );

static const trap_str_id tr_firewood_source( "tr_firewood_source" );
static const trap_str_id tr_unfinished_construction( "tr_unfinished_construction" );

static const zone_type_id zone_type_source_firewood( "SOURCE_FIREWOOD" );

static const zone_type_id zone_type_CHOP_TREES( "CHOP_TREES" );
static const zone_type_id zone_type_CONSTRUCTION_BLUEPRINT( "CONSTRUCTION_BLUEPRINT" );
static const zone_type_id zone_type_FARM_PLOT( "FARM_PLOT" );
static const zone_type_id zone_type_FISHING_SPOT( "FISHING_SPOT" );
static const zone_type_id zone_type_LOOT_CORPSE( "LOOT_CORPSE" );
static const zone_type_id zone_type_LOOT_IGNORE( "LOOT_IGNORE" );
static const zone_type_id zone_type_LOOT_IGNORE_FAVORITES( "LOOT_IGNORE_FAVORITES" );
static const zone_type_id zone_type_MINING( "MINING" );
static const zone_type_id zone_type_LOOT_UNSORTED( "LOOT_UNSORTED" );
static const zone_type_id zone_type_LOOT_WOOD( "LOOT_WOOD" );
static const zone_type_id zone_type_VEHICLE_DECONSTRUCT( "VEHICLE_DECONSTRUCT" );
static const zone_type_id zone_type_VEHICLE_REPAIR( "VEHICLE_REPAIR" );

static const quality_id qual_AXE( "AXE" );
static const quality_id qual_BUTCHER( "BUTCHER" );
static const quality_id qual_DIG( "DIG" );
static const quality_id qual_FISHING( "FISHING" );
static const quality_id qual_SAW_M( "SAW_M" );
static const quality_id qual_SAW_W( "SAW_W" );
static const quality_id qual_WELD( "WELD" );

static const std::string flag_BUTCHER_EQ( "BUTCHER_EQ" );
static const std::string flag_FISHABLE( "FISHABLE" );
static const std::string flag_GROWTH_HARVEST( "GROWTH_HARVEST" );
static const std::string flag_PLANT( "PLANT" );
static const std::string flag_PLOWABLE( "PLOWABLE" );
static const std::string flag_TREE( "TREE" );

void cancel_aim_processing();
//Generic activity: maximum search distance for zones, constructions, etc.
const int ACTIVITY_SEARCH_DISTANCE = 60;

static bool same_type( const std::vector<item *> &items )
{
    return std::ranges::all_of( items, [&items]( const item * const & it ) {
        return it->type == ( *items.begin() )->type;
    } );
}

static bool same_type( const std::vector<detached_ptr<item>> &items )
{
    return std::ranges::all_of( items, [&items]( const detached_ptr<item> &it ) {
        return it->type == ( *items.begin() )->type;
    } );
}

static void put_into_vehicle( Character &c, item_drop_reason reason,
                              std::vector<detached_ptr<item>> &items,
                              vehicle &veh, int part )
{
    if( items.empty() ) {
        return;
    }

    const tripoint where = veh.global_part_pos3( part );
    map &here = get_map();
    const std::string ter_name = here.name( where );
    int fallen_count = 0;
    bool into_vehicle = false;

    std::vector<item *> items_copy;
    items_copy.reserve( items.size() );
    for( detached_ptr<item> &it : items ) {
        items_copy.push_back( &*it );
    }

    // can't use constant reference here because of the spill_contents()
    for( detached_ptr<item> &it : items ) {
        item &obj = *it;
        it = pickup::handle_spillable_contents( c, std::move( it ), here );
        if( !it ) {
            continue;
        }
        it = veh.add_item( part, std::move( it ) );
        if( !it ) {
            into_vehicle = true;
        } else {
            if( obj.count_by_charges() ) {
                // Maybe we can add a few charges in the trunk and the rest on the ground.
                it = veh.add_charges( part, std::move( it ) );
                into_vehicle = true;
            }
            if( it ) {
                fallen_count += it->count();
                here.add_item_or_charges( where, std::move( it ) );
            }
        }
        obj.handle_pickup_ownership( c );
    }

    const std::string part_name = veh.part_info( part ).name();

    if( same_type( items_copy ) ) {
        const item *it = items_copy.front();
        const int dropcount = items.size() * it->count();
        const std::string it_name = it->tname( dropcount );

        switch( reason ) {
            case item_drop_reason::deliberate:
                c.add_msg_player_or_npc(
                    vgettext( "You put your %1$s in the %2$s's %3$s.",
                              "You put your %1$s in the %2$s's %3$s.", dropcount ),
                    vgettext( "<npcname> puts their %1$s in the %2$s's %3$s.",
                              "<npcname> puts their %1$s in the %2$s's %3$s.", dropcount ),
                    it_name, veh.name, part_name
                );
                break;
            case item_drop_reason::too_large:
                c.add_msg_if_player(
                    vgettext(
                        "There's no room in your inventory for the %s, so you drop it into the %s's %s.",
                        "There's no room in your inventory for the %s, so you drop them into the %s's %s.",
                        dropcount ),
                    it_name, veh.name, part_name
                );
                break;
            case item_drop_reason::too_heavy:
                c.add_msg_if_player(
                    vgettext( "The %s is too heavy to carry, so you drop it into the %s's %s.",
                              "The %s are too heavy to carry, so you drop them into the %s's %s.", dropcount ),
                    it_name, veh.name, part_name
                );
                break;
            case item_drop_reason::tumbling:
                c.add_msg_if_player(
                    m_bad,
                    vgettext( "Your %s tumbles into the %s's %s.",
                              "Your %s tumble into the %s's %s.", dropcount ),
                    it_name, veh.name, part_name
                );
                break;
        }
    } else {
        switch( reason ) {
            case item_drop_reason::deliberate:
                c.add_msg_player_or_npc(
                    _( "You put several items in the %1$s's %2$s." ),
                    _( "<npcname> puts several items in the %1$s's %2$s." ),
                    veh.name, part_name
                );
                break;
            case item_drop_reason::too_large:
            case item_drop_reason::too_heavy:
            case item_drop_reason::tumbling:
                c.add_msg_if_player(
                    m_bad, _( "Some items tumble into the %1$s's %2$s." ),
                    veh.name, part_name
                );
                break;
        }
    }

    if( fallen_count > 0 ) {
        if( into_vehicle ) {
            c.add_msg_if_player(
                m_warning,
                vgettext( "The %s is full, so something fell to the %s.",
                          "The %s is full, so some items fell to the %s.", fallen_count ),
                part_name, ter_name
            );
        } else {
            c.add_msg_if_player(
                m_warning,
                vgettext( "The %s is full, so it fell to the %s.",
                          "The %s is full, so they fell to the %s.", fallen_count ),
                part_name, ter_name
            );
        }
    }
}

static void pass_to_ownership_handling( item &obj, Character &c )
{
    obj.handle_pickup_ownership( c );
}

static void stash_on_pet( std::vector<detached_ptr<item>> &items, monster &pet,
                          Character &who )
{
    if( !pet.get_storage_item() ) {
        debugmsg( "Tried to stash items on a pet without a storage item" );
        return;
    }
    units::volume remaining_volume = pet.get_storage_item()->get_storage() - pet.get_carried_volume();
    units::mass remaining_weight = pet.weight_capacity() - pet.get_carried_weight();
    map &here = get_map();

    for( detached_ptr<item> &it : items ) {
        item &obj = *it;
        if( it->volume() > remaining_volume ) {
            add_msg( m_bad, _( "%1$s did not fit and fell to the %2$s." ), it->display_name(),
                     here.name( pet.pos() ) );
            here.add_item_or_charges( pet.pos(), std::move( it ) );
        } else if( it->weight() > remaining_weight ) {
            add_msg( m_bad, _( "%1$s is too heavy and fell to the %2$s." ), it->display_name(),
                     here.name( pet.pos() ) );
            here.add_item_or_charges( pet.pos(), std::move( it ) );
        } else {
            pet.add_item( std::move( it ) );
            remaining_volume -= obj.volume();
            remaining_weight -= obj.weight();
        }
        // TODO: if NPCs can have pets or move items onto pets
        pass_to_ownership_handling( obj, who );
    }
}

// Helper function to collect names of all favorited items (including contents)
static auto collect_favorited_item_names( const item &it ) -> std::vector<std::string>
{
    std::vector<std::string> favorited_names;
    if( it.is_favorite ) {
        favorited_names.push_back( it.display_name() );
    }

    auto favorited_contents = it.contents.all_items_ptr()
    | std::views::filter( []( const item * i ) { return i->is_favorite; } )
    | std::views::transform( []( const item * i ) { return i->display_name(); } );

    std::ranges::copy( favorited_contents, std::back_inserter( favorited_names ) );
    return favorited_names;
}

void drop_on_map( Character &c, item_drop_reason reason,
                  detached_ptr<item> &&it,
                  const tripoint &where )
{
    std::vector<detached_ptr<item>> vec;
    vec.push_back( std::move( it ) );
    drop_on_map( c, reason, vec, where );
}

void drop_on_map( Character &c, item_drop_reason reason,
                  std::vector<detached_ptr<item>> &items,
                  const tripoint &where )
{
    if( items.empty() ) {
        return;
    }
    map &here = get_map();
    const std::string ter_name = here.name( where );
    const bool can_move_there = here.passable( where );

    if( same_type( items ) ) {
        detached_ptr<item> &it = items.front();
        const int dropcount = items.size() * it->count();
        const std::string it_name = it->tname( dropcount );

        switch( reason ) {
            case item_drop_reason::deliberate:
                if( can_move_there ) {
                    c.add_msg_player_or_npc(
                        vgettext( "You drop your %1$s on the %2$s.",
                                  "You drop your %1$s on the %2$s.", dropcount ),
                        vgettext( "<npcname> drops their %1$s on the %2$s.",
                                  "<npcname> drops their %1$s on the %2$s.", dropcount ),
                        it_name, ter_name
                    );
                } else {
                    c.add_msg_player_or_npc(
                        vgettext( "You put your %1$s in the %2$s.",
                                  "You put your %1$s in the %2$s.", dropcount ),
                        vgettext( "<npcname> puts their %1$s in the %2$s.",
                                  "<npcname> puts their %1$s in the %2$s.", dropcount ),
                        it_name, ter_name
                    );
                }
                break;
            case item_drop_reason::too_large:
                c.add_msg_if_player(
                    vgettext( "There's no room in your inventory for the %s, so you drop it.",
                              "There's no room in your inventory for the %s, so you drop them.", dropcount ),
                    it_name
                );
                break;
            case item_drop_reason::too_heavy:
                c.add_msg_if_player(
                    vgettext( "The %s is too heavy to carry, so you drop it.",
                              "The %s is too heavy to carry, so you drop them.", dropcount ),
                    it_name
                );
                break;
            case item_drop_reason::tumbling:
                c.add_msg_if_player(
                    m_bad,
                    vgettext( "Your %1$s tumbles to the %2$s.",
                              "Your %1$s tumble to the %2$s.", dropcount ),
                    it_name, ter_name
                );
                break;
        }


        if( get_option<bool>( "AUTO_NOTES_DROPPED_FAVORITES" ) && it->is_favorite ) {
            const tripoint_abs_omt your_pos = c.global_omt_location();
            const std::string sprite_prefix = "SPRITE:" + it->typeId().str() + ";";
            if( !overmap_buffer.has_note( your_pos ) ) {
                overmap_buffer.add_note( your_pos, sprite_prefix + it->display_name() );
            } else {
                overmap_buffer.add_note( your_pos, overmap_buffer.note( your_pos ) + "; " + it->display_name() );
            }
        }

    } else {
        switch( reason ) {
            case item_drop_reason::deliberate:
                if( can_move_there ) {
                    c.add_msg_player_or_npc(
                        _( "You drop several items on the %s." ),
                        _( "<npcname> drops several items on the %s." ),
                        ter_name
                    );
                } else {
                    c.add_msg_player_or_npc(
                        _( "You put several items in the %s." ),
                        _( "<npcname> puts several items in the %s." ),
                        ter_name
                    );
                }
                break;
            case item_drop_reason::too_large:
            case item_drop_reason::too_heavy:
            case item_drop_reason::tumbling:
                c.add_msg_if_player( m_bad, _( "Some items tumble to the %s." ), ter_name );
                break;
        }
    }
    if( get_option<bool>( "AUTO_NOTES_DROPPED_FAVORITES" ) ) {
        const tripoint_abs_omt your_pos = c.global_omt_location();

        auto all_favorited_names = items
        | cata::ranges::flat_map( []( const auto & it ) { return collect_favorited_item_names( *it ); } )
        | std::ranges::to<std::vector>();

        if( !all_favorited_names.empty() ) {
            const auto note_text = join( all_favorited_names, "; " );

            if( !overmap_buffer.has_note( your_pos ) ) {
                overmap_buffer.add_note( your_pos, note_text );
            } else {
                overmap_buffer.add_note( your_pos, overmap_buffer.note( your_pos ) + "; " + note_text );
            }
        }
    }

    for( auto &it : items ) {
        item &obj = *it;
        here.add_item_or_charges( where, std::move( it ) );
        pass_to_ownership_handling( obj, c );
    }
}

void put_into_vehicle_or_drop( Character &c, item_drop_reason reason,
                               detached_ptr<item> &&it )
{
    std::vector<detached_ptr<item>> vec;
    vec.push_back( std::move( it ) );
    return put_into_vehicle_or_drop( c, reason, vec, c.pos() );
}

void put_into_vehicle_or_drop( Character &c, item_drop_reason reason,
                               std::vector<detached_ptr<item>> &items )
{
    return put_into_vehicle_or_drop( c, reason, items, c.pos() );
}

void put_into_vehicle_or_drop( Character &c, item_drop_reason reason,
                               detached_ptr<item> &&it,
                               const tripoint &where, bool force_ground )
{

    std::vector<detached_ptr<item>> vec;
    vec.push_back( std::move( it ) );
    put_into_vehicle_or_drop( c, reason, vec, where, force_ground );
}

void put_into_vehicle_or_drop( Character &c, item_drop_reason reason,
                               std::vector<detached_ptr<item>> &items,
                               const tripoint &where, bool force_ground )
{
    map &here = get_map();
    const std::optional<vpart_reference> vp = here.veh_at( where ).part_with_feature( "CARGO", false );
    if( vp && !force_ground ) {
        put_into_vehicle( c, reason, items, vp->vehicle(), vp->part_index() );
        return;
    }
    drop_on_map( c, reason, items, where );
}

static std::list<pickup::act_item> convert_to_items( Character &p,
        const drop_locations &drop,
        const std::function<bool( item &loc )> &filter )
{
    std::list<pickup::act_item> res;

    for( const drop_location &rec : drop ) {
        item *loc = &*rec.loc;
        const int count = rec.count;

        if( !filter( *loc ) ) {
            continue;
        } else if( !p.is_worn( *loc ) && !p.is_wielding( *loc ) ) {
            // Special case. After dropping the first few items, the remaining items are already separated.
            // That means: `drop` already contains references to each of the items in
            // `p.inv.const_stack`, and `count` will be 1 for each of them.
            // If we continued without this check, we iterate over `p.inv.const_stack` multiple times,
            // but each time stopping after visiting the first item.
            // In the end, we would add references to the same item (the first one in the stack) multiple times.
            if( count == 1 ) {
                res.emplace_back( *loc, 1, loc->obtain_cost( p, 1 ) );
                continue;
            }
            int obtained = 0;
            for( item * const &it : p.inv_const_stack( p.get_item_position( &*loc ) ) ) {
                if( obtained >= count ) {
                    break;
                }
                const int qty = it->count_by_charges() ? std::min<int>( it->charges, count - obtained ) : 1;
                obtained += qty;
                res.emplace_back( *it, qty, it->obtain_cost( p, qty ) );
            }
        } else {
            res.emplace_back( *loc, count, p.is_wielding( *loc ) ? 0 : loc->obtain_cost( p ) );
        }
    }

    return res;
}

namespace pickup
{

// Prepares items for dropping by reordering them so that the drop
// cost is minimal and "dependent" items get taken off first.
// Implements the "backpack" logic.
std::list<act_item> reorder_for_dropping( Character &p, const drop_locations &drop )
{
    std::list<act_item> res = convert_to_items( p, drop,
    [&p]( item & loc ) {
        return p.is_wielding( loc );
    } );
    std::list<act_item> inv = convert_to_items( p, drop,
    [&p]( item & loc ) {
        return !p.is_wielding( loc ) && !p.is_worn( loc );
    } );
    std::list<act_item> worn = convert_to_items( p, drop,
    [&p]( item & loc ) {
        return p.is_worn( loc );
    } );

    // Sort inventory items by volume in ascending order
    inv.sort( []( const act_item & first, const act_item & second ) {
        return first.loc->volume() < second.loc->volume();
    } );
    // Add missing dependent worn items (if any).
    for( const auto &wait : worn ) {
        for( item *dit : p.get_dependent_worn_items( *wait.loc ) ) {
            const auto iter = std::ranges::find_if( worn,
            [dit]( const act_item & ait ) {
                return &*ait.loc == dit;
            } );

            if( iter == worn.end() ) {
                // TODO: Use a calculated cost
                worn.emplace_front( *dit, dit->count(), dit->obtain_cost( p ) );
            }
        }
    }
    // Sort worn items by storage in descending order, but dependent items always go first.
    worn.sort( []( const act_item & first, const act_item & second ) {
        return first.loc->is_worn_only_with( *second.loc )
               || ( first.loc->get_storage() > second.loc->get_storage()
                    && !second.loc->is_worn_only_with( *first.loc ) );
    } );

    // Avoid tumbling to the ground. Unload cleanly.
    units::volume dropped_inv_contents = std::accumulate( inv.begin(), inv.end(), 0_ml,
    []( units::volume acc, const act_item & ait ) {
        return acc + ait.loc->volume();
    } );
    const units::volume dropped_worn_storage = std::accumulate( worn.begin(), worn.end(), 0_ml,
    []( units::volume acc, const act_item & ait ) {
        return acc + ait.loc->get_storage();
    } );
    std::set<int> inv_indices;
    std::ranges::transform( inv, std::inserter( inv_indices, inv_indices.begin() ),
    [&p]( const act_item & ait ) {
        return p.get_item_position( &*ait.loc );
    } );

    units::volume excessive_volume = p.volume_carried() - dropped_inv_contents
                                     - p.volume_capacity_reduced_by( dropped_worn_storage );
    if( excessive_volume > 0_ml ) {
        const_invslice old_inv = p.inv_const_slice();
        for( size_t i = 0; i < old_inv.size() && excessive_volume > 0_ml; i++ ) {
            // TODO: Reimplement random dropping?
            if( inv_indices.contains( i ) ) {
                continue;
            }
            const std::vector<item *> &inv_stack = *old_inv[i];
            for( item * const &item : inv_stack ) {
                // Note: zero cost, but won't be contained on drop
                act_item to_drop = act_item( *item, item->count(), 0 );
                inv.push_back( to_drop );
                excessive_volume -= to_drop.loc->volume();
                if( excessive_volume <= 0_ml ) {
                    break;
                }
            }
        }
        // Need to re-sort
        inv.sort( []( const act_item & first, const act_item & second ) {
            return first.loc->volume() < second.loc->volume();
        } );
    }

    // Cumulatively decreases
    units::volume remaining_dropped_storage = dropped_worn_storage;

    while( !worn.empty() && !inv.empty() ) {
        units::volume front_storage = worn.front().loc->get_storage();
        // Does not fit
        // TODO: but maybe an item further down the line does
        if( remaining_dropped_storage < inv.front().loc->volume() ) {
            break;
        }

        res.push_back( worn.front() );
        worn.pop_front();
        remaining_dropped_storage -= front_storage;
        while( !inv.empty() ) {
            units::volume inventory_item_volume = inv.front().loc->volume();
            if( front_storage < inventory_item_volume ) {
                break;
            }
            front_storage -= inventory_item_volume;

            res.push_back( inv.front() );
            // Free of charge
            res.back().consumed_moves = 0;

            inv.pop_front();
        }
    }

    // Now insert everything that remains
    std::ranges::copy( inv, std::back_inserter( res ) );
    std::ranges::copy( worn, std::back_inserter( res ) );

    return res;
}

std::vector<detached_ptr<item>> obtain_and_tokenize_items( player &p, std::list<act_item> &items )
{
    std::vector<detached_ptr<item>> res;
    drop_token_provider &token_provider = drop_token::get_provider();
    item_drop_token last_token = token_provider.make_next( calendar::turn );
    if( items.empty() ) {
        return res;
    }
    units::volume last_storage_volume = items.front().loc->get_storage();
    while( !items.empty() && ( p.is_npc() || p.moves > 0 || items.front().consumed_moves == 0 ) ) {
        act_item &ait = items.front();

        p.mod_moves( -ait.consumed_moves );

        if( p.is_worn( *ait.loc ) ) {
            if( !p.takeoff( *ait.loc, &res ) ) {
                // Skip item if failed to take it off
                debugmsg( "Failed to obtain worn target item of ACT_DROP" );
                items.pop_front();
                continue;
            }
        } else if( ait.loc->count_by_charges() ) {
            res.push_back( p.reduce_charges( const_cast<item *>( &*ait.loc ), ait.count ) );
        } else {
            res.push_back( ait.loc->detach() );
        }

        // TODO: Get the item consistently instead of using back()
        item &current_drop = *res.back();

        // Hack: if it consumes zero moves, it must have been contained
        // TODO: Properly mark containment somehow
        *current_drop.drop_token = token_provider.make_next( calendar::turn );;
        if( ait.consumed_moves == 0 && last_storage_volume >= current_drop.volume() ) {
            last_storage_volume -= current_drop.volume();
            current_drop.drop_token->parent_number = last_token.parent_number;
        } else {
            last_token = *current_drop.drop_token;
            last_storage_volume = current_drop.get_storage();
        }

        items.pop_front();
    }

    return res;
}

} // namespace pickup

// TODO: Display costs in the multidrop menu
static void debug_drop_list( const std::list<pickup::act_item> &list )
{
    if( !debug_mode ) {
        return;
    }

    std::string res( "Items ordered to drop:\n" );
    for( const auto &ait : list ) {
        res += string_format( "Drop %d %s for %d moves\n",
                              ait.count, ait.loc->display_name( ait.count ), ait.consumed_moves );
    }
    popup( res, PF_GET_KEY );
}

static void debug_tokens( const std::vector<detached_ptr<item>> &items )
{
    if( !debug_mode ) {
        return;
    }

    std::stringstream ss;
    ss << "Item tokens:\n";
    for( const detached_ptr<item> &it : items ) {
        ss << it->display_name() << ": " << *it->drop_token << '\n';
    }
    popup( ss.str(), PF_GET_KEY );
}

static std::vector<detached_ptr<item>> obtain_activity_items( Character &who,
                                    std::list<pickup::act_item> &targets )
{
    debug_drop_list( targets );

    std::vector<detached_ptr<item>> res = pickup::obtain_and_tokenize_items( *who.as_player(),
                                          targets );

    debug_tokens( res );

    return res;
}

void drop_activity_actor::do_turn( player_activity &, Character &who )
{
    const tripoint pos = who.pos() + relpos;

    std::vector<detached_ptr<item>> dropped = obtain_activity_items( who, items ) ;

    put_into_vehicle_or_drop( who, item_drop_reason::deliberate,
                              dropped, pos, force_ground );

    get_map().process_falling();

    if( items.empty() ) {
        who.cancel_activity();
    }
}

void activity_on_turn_wear( player_activity &act, player &p )
{
    // ACT_WEAR has item_location targets, and int quantities
    while( p.moves > 0 && !act.targets.empty() && !act.values.empty() ) {
        safe_reference<item> target = std::move( act.targets.back() );
        int quantity = act.values.back();
        act.targets.pop_back();
        act.values.pop_back();

        if( !target ) {
            debugmsg( "Lost target item of ACT_WEAR" );
            continue;
        }
        ret_val<bool> ret = p.can_wear( *target );
        if( ret.success() && ret.value() ) {
            detached_ptr<item> newit = target->split( quantity );
            p.wear_item( std::move( newit ) );
        }
    }

    // If there are no items left we are done
    if( act.targets.empty() ) {
        p.cancel_activity();
    }
}


void stash_activity_actor::do_turn( player_activity &, Character &who )
{
    const tripoint pos = who.pos() + relpos;

    monster *pet = g->critter_at<monster>( pos );
    if( pet != nullptr && pet->has_effect( effect_pet ) ) {
        pet->add_effect( effect_ai_waiting, 2_turns );
        std::vector<detached_ptr<item>> stashed = obtain_activity_items( who, items );
        stash_on_pet( stashed, *pet, who );
        if( items.empty() ) {
            who.cancel_activity();
            pet->remove_effect( effect_ai_waiting );
        }
    } else {
        who.add_msg_if_player( _( "The pet has moved somewhere else." ) );
        who.cancel_activity();
    }
}

static double get_capacity_fraction( int capacity, int volume )
{
    // fraction of capacity the item would occupy
    // fr = 1 is for capacity smaller than is size of item
    // in such case, let's assume player does the trip for full cost with item in hands
    double fr = 1;

    if( capacity > volume ) {
        fr = static_cast<double>( volume ) / capacity;
    }

    return fr;
}

static int move_cost_inv( const item &it, const tripoint &src, const tripoint &dest )
{
    // to prevent potentially ridiculous number
    const int MAX_COST = 500;

    // it seems that pickup cost is flat 100
    // in function pick_one_up, variable moves_taken has initial value of 100
    // and never changes until it is finally used in function
    // remove_from_map_or_vehicle
    const int pickup_cost = 100;

    // drop cost for non-tumbling items (from inventory overload) is also flat 100
    // according to convert_to_items (it does contain todo to use calculated costs)
    const int drop_cost = 100;

    // typical flat ground move cost
    const int mc_per_tile = 100;

    // only free inventory capacity
    const int inventory_capacity = units::to_milliliter( g->u.volume_capacity() -
                                   g->u.volume_carried() );

    const int item_volume = units::to_milliliter( it.volume() );

    const double fr = get_capacity_fraction( inventory_capacity, item_volume );

    // approximation of movement cost between source and destination
    const int move_cost = mc_per_tile * rl_dist( src, dest ) * fr;

    return std::min( pickup_cost + drop_cost + move_cost, MAX_COST );
}

static int move_cost_cart( const item &it, const tripoint &src, const tripoint &dest,
                           const units::volume &capacity )
{
    // to prevent potentially ridiculous number
    const int MAX_COST = 500;

    // cost to move item into the cart
    const int pickup_cost = pickup::cost_to_move_item( g->u, it );

    // cost to move item out of the cart
    const int drop_cost = pickup_cost;

    // typical flat ground move cost
    const int mc_per_tile = 100;

    // only free cart capacity
    const int cart_capacity = units::to_milliliter( capacity );

    const int item_volume = units::to_milliliter( it.volume() );

    const double fr = get_capacity_fraction( cart_capacity, item_volume );

    // approximation of movement cost between source and destination
    const int move_cost = mc_per_tile * rl_dist( src, dest ) * fr;

    return std::min( pickup_cost + drop_cost + move_cost, MAX_COST );
}

static int move_cost( const item &it, const tripoint &src, const tripoint &dest )
{
    if( g->u.get_grab_type() == OBJECT_VEHICLE ) {
        tripoint cart_position = g->u.pos() + g->u.grab_point;

        if( const std::optional<vpart_reference> vp = get_map().veh_at(
                    cart_position ).part_with_feature( "CARGO", false ) ) {
            const vehicle &veh = vp->vehicle();
            size_t vstor = vp->part_index();
            units::volume capacity = veh.free_volume( vstor );

            return move_cost_cart( it, src, dest, capacity );
        }
    }

    return move_cost_inv( it, src, dest );
}

// return true if activity was assigned.
// return false if it was not possible.
static bool vehicle_activity( player &p, const tripoint &src_loc, int vpindex, char type )
{
    map &here = get_map();
    vehicle *veh = veh_pointer_or_null( here.veh_at( src_loc ) );
    if( !veh ) {
        return false;
    }
    int time_to_take = 0;
    if( vpindex >= veh->part_count() ) {
        // if parts got removed during our work, we can't just carry on removing, we want to repair parts!
        // so just bail out, as we don't know if the next shifted part is suitable for repair.
        if( type == 'r' ) {
            return false;
        } else if( type == 'o' ) {
            vpindex = veh->get_next_shifted_index( vpindex, p );
            if( vpindex == -1 ) {
                return false;
            }
        }
    }
    const vpart_info &vp = veh->part_info( vpindex );
    if( type == 'r' ) {
        const vehicle_part &part = veh->part( vpindex );
        time_to_take = vp.repair_time( p ) * part.damage() / part.max_damage();
    } else if( type == 'o' ) {
        time_to_take = vp.removal_time( p );
    }
    p.assign_activity( ACT_VEHICLE, time_to_take, static_cast<int>( type ) );
    // so , NPCs can remove the last part on a position, then there is no vehicle there anymore,
    // for someone else who stored that position at the start of their activity.
    // so we may need to go looking a bit further afield to find it , at activities end.
    for( const auto pt : veh->get_points( true ) ) {
        p.activity->coord_set.insert( here.getabs( pt ) );
    }
    // values[0]
    p.activity->values.push_back( here.getabs( src_loc ).x );
    // values[1]
    p.activity->values.push_back( here.getabs( src_loc ).y );
    // values[2]
    p.activity->values.push_back( point_zero.x );
    // values[3]
    p.activity->values.push_back( point_zero.y );
    // values[4]
    p.activity->values.push_back( -point_zero.x );
    // values[5]
    p.activity->values.push_back( -point_zero.y );
    // values[6]
    p.activity->values.push_back( veh->index_of_part( &veh->part( vpindex ) ) );
    p.activity->str_values.push_back( vp.get_id().str() );
    // values[7]
    p.activity->values.push_back( here.getabs( src_loc ).z );
    // this would only be used for refilling tasks
    p.activity->targets.emplace_back( );
    p.activity->placement = here.getabs( src_loc );
    p.activity_vehicle_part_index = -1;
    return true;
}

static void move_item( player &p, item &it, const int quantity, const tripoint &src,
                       const tripoint &dest, const activity_id &activity_to_restore = activity_id::NULL_ID() )
{
    // Check that we can pick it up.
    if( it.made_of( LIQUID ) ) {
        return;
    }
    detached_ptr<item> moved = it.split( quantity ) ;

    p.mod_moves( -move_cost( it, src, dest ) );
    if( activity_to_restore == ACT_TIDY_UP ) {
        moved->erase_var( "activity_var" );
    } else if( activity_to_restore == ACT_FETCH_REQUIRED ) {
        moved->set_var( "activity_var", p.name );
    }
    put_into_vehicle_or_drop( p, item_drop_reason::deliberate, std::move( moved ), dest );
}

std::vector<tripoint> route_adjacent( const player &p, const tripoint &dest )
{
    auto passable_tiles = std::unordered_set<tripoint>();
    map &here = get_map();

    for( const tripoint &tp : here.points_in_radius( dest, 1 ) ) {
        if( tp != p.pos() && here.passable( tp ) && !here.obstructed_by_vehicle_rotation( dest, tp ) ) {
            passable_tiles.emplace( tp );
        }
    }

    const auto &sorted = get_sorted_tiles_by_distance( p.pos(), passable_tiles );

    const auto &avoid = p.get_legacy_path_avoid();
    for( const tripoint &tp : sorted ) {
        auto route = here.route( p.pos(), tp, p.get_legacy_pathfinding_settings(), avoid );

        if( !route.empty() ) {
            return route;
        }
    }

    return std::vector<tripoint>();
}

static std::vector<construction_id> get_group_roots( const std::vector<construction_id> &all,
        const construction_id &base )
{
    // Get group members
    std::vector<construction_id> group_members;
    std::ranges::copy_if( all, std::back_inserter( group_members ), [&]( const auto & id ) {
        return base->group == id->group;
    } );

    if( group_members.empty() ) {
        return std::vector<construction_id>();
    }

    bool same_output = std::ranges::all_of( group_members, [&]( auto & con ) {
        return ( con->post_furniture == group_members[0]->post_furniture ) &&
               ( con->post_terrain == group_members[0]->post_terrain );
    } );
    if( same_output ) {
        return group_members;
    }

    std::vector<construction_id> ret;
    std::ranges::copy_if( group_members,
    std::back_inserter( ret ), [&]( auto & con ) {
        auto it = std::find_if( group_members.begin(), group_members.end(), [&]( auto & next ) {
            return con->post_terrain == next->pre_terrain && con->post_furniture == next->pre_furniture;
        } );
        return it == group_members.end();
    } );

    return ret;
}

static activity_reason_info find_base_construction(
    const std::vector<construction_id> &list_constructions,
    player &p,
    const inventory &inv,
    const tripoint &loc,
    const std::optional<construction_id> &part_con_id,
    const construction_id id,
    bool strict = true )
{
    //already done?
    map &here = get_map();
    const furn_id furn = here.furn( loc );
    const ter_id ter = here.ter( loc );

    // Get roots:
    std::vector<construction_id> roots = get_group_roots( list_constructions, id );
    // Check if any roots are completed, if so we're done here
    for( const auto &rid : roots ) {
        const construction &rid_build = rid.obj();
        if(
            ( !rid_build.post_terrain.is_empty() && rid_build.post_terrain.id() == ter ) ||
            ( !rid_build.post_furniture.is_empty() && rid_build.post_furniture.id() == furn )
        ) {
            return activity_reason_info::build( do_activity_reason::ALREADY_DONE, false, rid );
        }
    }
    // None of the roots are complete, so let's evaluate everything!
    std::map<ter_str_id, std::vector<construction_id>> post_ter;
    std::map<furn_str_id, std::vector<construction_id>> post_furn;
    for( const auto &con : list_constructions ) {
        if( con->group.is_empty() ) {
            continue;
        }
        ( post_furn[con->post_furniture] ).push_back( con );
        ( post_ter[con->post_terrain] ).push_back( con );
    }

    struct time_con {
        int time;
        construction_id id;

        auto operator<( const time_con &b ) const -> bool {
            return time > b.time;
        }
    };
    std::priority_queue<time_con> pq;
    std::ranges::for_each( roots, [&]( const auto & con ) {
        pq.push( { to_turns<int>( con->time ), con } );
    } );
    auto is_disassembly = []( const auto & con ) -> bool {
        auto check_assembly = []( const auto & con ) -> bool {
            return !con->requirements->get_components().empty();
        };
        auto check_disassembly = []( const auto & con ) -> bool {
            return !!con->byproduct_item_group;
        };
        bool dis = check_disassembly( con );
        bool ass = check_assembly( con );

        return !ass && dis;

        // A D Q
        // 1 0 Assembly
        // 1 1 Assembly
        // 0 1 Disassembly
        // 0 0 Assembly
    };
    activity_reason_info reason_result = activity_reason_info::build(
            do_activity_reason::UNKNOWN_ACTIVITY, false, id );
    std::set<construction_id> used;
    while( !pq.empty() ) {
        auto cur = pq.top();
        pq.pop();

        auto con = cur.id;
        if( used.contains( con ) ) {
            continue;    // already evaluated this one
        }
        if( strict && con->group != id->group ) {
            continue;    // evaluating strictly and this item is not in group
        }
        used.insert( con );

        const auto &con_build = con.obj();
        if(
            ( !con_build.post_terrain.is_empty() && con_build.post_terrain.id() == ter ) ||
            ( !con_build.post_furniture.is_empty() && con_build.post_furniture.id() == furn )
        ) {
            return activity_reason_info::build( do_activity_reason::ALREADY_DONE, false, con );
        }

        // Check to see if we can do anything with this item
        bool has_skill = p.meets_skill_requirements( con_build );
        // partial construction here is the same as what we're evaluating
        if( part_con_id && *part_con_id == con ) {
            if( !has_skill ) {
                return activity_reason_info::build( do_activity_reason::DONT_HAVE_SKILL, false, con );
            }
            return activity_reason_info::build( do_activity_reason::CAN_DO_CONSTRUCTION, true, con );
        }

        // we don't have the skill to do this one, but might be able to do one of the others left in PQ
        if( !has_skill ) {
            reason_result = activity_reason_info::build( do_activity_reason::DONT_HAVE_SKILL, false, con );
            continue;
        }
        // we have the skill, but can we build?
        const bool cc = can_construct( con_build, loc ); // local conditions appropriate
        const bool pcb = player_can_build( p, inv, con_build ); // have stuff to build it

        if( cc ) {
            if( pcb ) {
                // can construct and can build, so let's do it
                return activity_reason_info::build( do_activity_reason::CAN_DO_CONSTRUCTION, true, con );
            }
            //can't build with current inventory. Might be able to build other options
            reason_result = activity_reason_info::build( do_activity_reason::NO_COMPONENTS, false, con );
            continue;
        }

        if( !pcb ) {
            // Don't have the components to construct this, but might be able to construct something else
            // so set our reason in case this is the last thing we check.
            reason_result = activity_reason_info::build( do_activity_reason::NO_COMPONENTS, false, con );
        }

        // there are no pre-requisites.
        // so we need to potentially fetch components
        if( con_build.pre_terrain.is_empty() && con_build.pre_furniture.is_empty() &&
            con_build.pre_special( loc ) ) {
            reason_result = activity_reason_info::build( do_activity_reason::NO_COMPONENTS, false, con );
            continue;
        } else if( !con_build.pre_special( loc ) ) {
            reason_result = activity_reason_info::build( do_activity_reason::BLOCKING_TILE, false, con );
            continue;
        }

        // Add pre-requisites to processing
        if( !con->pre_terrain.is_empty() ) {
            const auto it = post_ter.find( con->pre_terrain );
            if( it == post_ter.end() ) {
                continue;
            }

            for( const auto &next : it->second ) {
                if( is_disassembly( con ) && !is_disassembly( next ) ) {
                    continue;
                }
                pq.push( { cur.time + to_turns<int>( next->time ), next } );
            }
        }
        if( !con->pre_furniture.is_empty() ) {
            const auto it = post_furn.find( con->pre_furniture );
            if( it == post_furn.end() ) {
                continue;
            }
            for( const auto &next : it->second ) {
                if( is_disassembly( con ) && !is_disassembly( next ) ) {
                    continue;
                }
                pq.push( { cur.time + to_turns<int>( next->time ), next } );
            }
        }
    }
    // Partial construction here that was not evaluated during prereq resolution
    if( part_con_id ) {
        return activity_reason_info::build( do_activity_reason::BLOCKING_TILE, false, id );
    }
    //pre-req failed?
    if( reason_result.reason != do_activity_reason::UNKNOWN_ACTIVITY ) {
        if( reason_result.reason == do_activity_reason::NO_COMPONENTS ) {
            return activity_reason_info::build( do_activity_reason::NO_COMPONENTS_PREREQ, false,
                                                *reason_result.con_idx );
        }
        return activity_reason_info::build( reason_result.reason, false, *reason_result.con_idx );
    }
    if( reason_result.reason == do_activity_reason::NO_COMPONENTS ) {
        return reason_result;
    }
    //only cc failed, no pre-req
    return activity_reason_info::build( do_activity_reason::BLOCKING_TILE, false, id );
}

static std::string random_string( size_t length )
{
    auto randchar = []() -> char {
        static constexpr char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        static constexpr size_t num_chars = sizeof( charset ) - 1;
        return charset[rng( 0, num_chars - 1 )];
    };
    std::string str( length, 0 );
    std::generate_n( str.begin(), length, randchar );
    return str;
}

static bool are_requirements_nearby( const std::vector<tripoint> &loot_spots,
                                     const requirement_id &needed_things, player &p, const activity_id &activity_to_restore,
                                     const bool in_loot_zones, const tripoint &src_loc )
{
    zone_manager &mgr = zone_manager::get_manager();
    inventory temp_inv;
    units::volume volume_allowed = p.volume_capacity() - p.volume_carried();
    units::mass weight_allowed = p.weight_capacity() - p.weight_carried();
    static const auto check_weight_if = []( const activity_id & id ) {
        return id == ACT_MULTIPLE_FARM ||
               id == ACT_MULTIPLE_CHOP_PLANKS ||
               id == ACT_MULTIPLE_BUTCHER ||
               id == ACT_VEHICLE_DECONSTRUCTION ||
               id == ACT_VEHICLE_REPAIR ||
               id == ACT_MULTIPLE_CHOP_TREES ||
               id == ACT_MULTIPLE_FISH ||
               id == ACT_MULTIPLE_MINE;
    };
    const bool check_weight = check_weight_if( activity_to_restore ) || ( !p.backlog.empty() &&
                              check_weight_if( p.backlog.front()->id() ) );
    bool found_welder = false;
    for( item *elem : p.inv_dump() ) {
        if( elem->has_quality( qual_WELD ) ) {
            found_welder = true;
        }
        temp_inv.add_item( *elem, true );
    }
    map &here = get_map();
    for( const tripoint &elem : loot_spots ) {
        // if we are searching for things to fetch, we can skip certain things.
        // if, however they are already near the work spot, then the crafting / inventory functions will have their own method to use or discount them.
        if( in_loot_zones ) {
            // skip tiles in IGNORE zone and tiles on fire
            // (to prevent taking out wood off the lit brazier)
            // and inaccessible furniture, like filled charcoal kiln
            if( mgr.has( zone_type_LOOT_IGNORE, here.getabs( elem ) ) ||
                here.dangerous_field_at( elem ) ||
                !here.can_put_items_ter_furn( elem ) ) {
                continue;
            }
        }
        for( const auto &elem2 : here.i_at( elem ) ) {
            if( in_loot_zones && elem2->made_of( LIQUID ) ) {
                continue;
            }
            if( check_weight ) {
                // this fetch task will need to pick up an item. so check for its weight/volume before setting off.
                if( in_loot_zones && ( elem2->volume() > volume_allowed ||
                                       elem2->weight() > weight_allowed ) ) {
                    continue;
                }
            }
            temp_inv.add_item( *elem2, true );
        }
        if( !in_loot_zones ) {
            if( const std::optional<vpart_reference> vp = here.veh_at( elem ).part_with_feature( "CARGO",
                    false ) ) {
                vehicle &src_veh = vp->vehicle();
                int src_part = vp->part_index();
                for( auto &it : src_veh.get_items( src_part ) ) {
                    temp_inv.add_item( *it, true );
                }
            }
        }
    }
    // use nearby welding rig without needing to drag it or position yourself on the right side of the vehicle.
    if( !found_welder ) {
        for( const tripoint &elem : here.points_in_radius( src_loc, PICKUP_RANGE - 1 ) ) {
            const optional_vpart_position vp = here.veh_at( elem );
            if( vp ) {
                vehicle &veh = vp->vehicle();
                const std::optional<vpart_reference> weldpart = vp.part_with_feature( "WELDRIG", true );
                if( weldpart ) {
                    item *welder = item::spawn_temporary( itype_welder, calendar::start_of_cataclysm );
                    welder->charges = veh.fuel_left( itype_battery, true );
                    welder->set_flag( flag_PSEUDO );
                    temp_inv.add_item( *welder, false );
                    item *soldering_iron = item::spawn_temporary( itype_soldering_iron,
                                           calendar::start_of_cataclysm );
                    soldering_iron->charges = veh.fuel_left( itype_battery, true );
                    soldering_iron->set_flag( flag_PSEUDO );
                    temp_inv.add_item( *soldering_iron, false );
                }
            }
        }
    }
    return needed_things.obj().can_make_with_inventory( temp_inv, is_crafting_component );
}

static bool has_skill_for_vehicle_work( const std::map<skill_id, int> &required_skills, player &p )
{
    for( const auto &e : required_skills ) {
        bool hasSkill = p.get_skill_level( e.first ) >= e.second;
        if( !hasSkill ) {
            return false;
        }
    }
    return true;
}

static activity_reason_info can_do_activity_there( const activity_id &act, player &p,
        const tripoint &src_loc, const int distance = ACTIVITY_SEARCH_DISTANCE )
{
    // see activity_handlers.h cant_do_activity_reason enums
    p.invalidate_crafting_inventory();
    zone_manager &mgr = zone_manager::get_manager();
    std::vector<zone_data> zones;
    map &here = get_map();
    if( act == ACT_VEHICLE_DECONSTRUCTION ||
        act == ACT_VEHICLE_REPAIR ) {
        std::vector<int> already_working_indexes;
        vehicle *veh = veh_pointer_or_null( here.veh_at( src_loc ) );
        if( !veh ) {
            return activity_reason_info::fail( do_activity_reason::NO_ZONE );
        }
        // if the vehicle is moving or player is controlling it.
        if( std::abs( veh->velocity ) > 100 || veh->player_in_control( g->u ) ) {
            return activity_reason_info::fail( do_activity_reason::NO_ZONE );
        }
        for( const npc &guy : g->all_npcs() ) {
            if( &guy == &p ) {
                continue;
            }
            // If the NPC has an activity - make sure they're not duplicating work.
            tripoint guy_work_spot;
            if( guy.has_player_activity() && guy.activity->placement != tripoint_min ) {
                guy_work_spot = here.getlocal( guy.activity->placement );
            }
            // If their position or intended position or player position/intended position
            // then discount, don't need to move each other out of the way.
            if( here.getlocal( g->u.activity->placement ) == src_loc ||
                guy_work_spot == src_loc || guy.pos() == src_loc || ( p.is_npc() && g->u.pos() == src_loc ) ) {
                return activity_reason_info::fail( do_activity_reason::ALREADY_WORKING );
            }
            if( guy_work_spot != tripoint_zero ) {
                vehicle *other_veh = veh_pointer_or_null( here.veh_at( guy_work_spot ) );
                // working on same vehicle - store the index to check later.
                if( other_veh && other_veh == veh && guy.activity_vehicle_part_index != -1 ) {
                    already_working_indexes.push_back( guy.activity_vehicle_part_index );
                }
            }
            if( g->u.activity_vehicle_part_index != -1 ) {
                already_working_indexes.push_back( g->u.activity_vehicle_part_index );
            }
        }
        if( act == ACT_VEHICLE_DECONSTRUCTION ) {
            // find out if there is a vehicle part here we can remove.
            std::vector<vehicle_part *> parts = veh->get_parts_at( src_loc, "", part_status_flag::any );
            for( vehicle_part *part_elem : parts ) {
                const vpart_info &vpinfo = part_elem->info();
                int vpindex = veh->index_of_part( part_elem, true );
                // if part is not on this vehicle, or if its attached to another part that needs to be removed first.
                if( vpindex == -1 || !veh->can_unmount( vpindex ) ) {
                    continue;
                }
                // this is the same part that somebody else wants to work on, or already is.
                if( std::ranges::find( already_working_indexes,
                                       vpindex ) != already_working_indexes.end() ) {
                    continue;
                }
                // don't have skill to remove it
                if( !has_skill_for_vehicle_work( vpinfo.removal_skills, p ) ) {
                    continue;
                }
                item &base = *item::spawn_temporary( vpinfo.item );
                if( base.is_wheel() ) {
                    // no wheel removal yet
                    continue;
                }
                const int max_lift = p.best_nearby_lifting_assist( src_loc );
                const int lvl = std::ceil( units::quantity<double, units::mass::unit_type>( base.weight() ) /
                                           TOOL_LIFT_FACTOR );
                const bool use_aid = max_lift >= lvl;
                const bool use_str = character_funcs::can_lift_with_helpers( p, base.lift_strength() );
                if( !( use_aid || use_str ) ) {
                    continue;
                }
                const auto &reqs = vpinfo.removal_requirements();
                const inventory &inv = p.crafting_inventory( false );

                const bool can_make = reqs.can_make_with_inventory( inv, is_crafting_component );
                p.set_value( "veh_index_type", vpinfo.name() );
                // temporarily store the intended index, we do this so two NPCs don't try and work on the same part at same time.
                p.activity_vehicle_part_index = vpindex;
                if( !can_make ) {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_VEH_DECONST );
                } else {
                    return activity_reason_info::ok( do_activity_reason::NEEDS_VEH_DECONST );
                }
            }
        } else if( act == ACT_VEHICLE_REPAIR ) {
            // find out if there is a vehicle part here we can repair.
            std::vector<vehicle_part *> parts = veh->get_parts_at( src_loc, "", part_status_flag::any );
            for( vehicle_part *part_elem : parts ) {
                const vpart_info &vpinfo = part_elem->info();
                int vpindex = veh->index_of_part( part_elem, true );
                // if part is undamaged or beyond repair - can skip it.
                if( part_elem->is_broken() || part_elem->damage() == 0 ||
                    part_elem->info().repair_requirements().is_empty() ) {
                    continue;
                }
                if( std::ranges::find( already_working_indexes,
                                       vpindex ) != already_working_indexes.end() ) {
                    continue;
                }
                // don't have skill to repair it
                if( !has_skill_for_vehicle_work( vpinfo.repair_skills, p ) ) {
                    continue;
                }
                const auto &reqs = vpinfo.repair_requirements();
                const inventory &inv = p.crafting_inventory( src_loc, PICKUP_RANGE - 1, false );
                const bool can_make = reqs.can_make_with_inventory( inv, is_crafting_component );
                p.set_value( "veh_index_type", vpinfo.name() );
                // temporarily store the intended index, we do this so two NPCs don't try and work on the same part at same time.
                p.activity_vehicle_part_index = vpindex;
                if( !can_make ) {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_VEH_REPAIR );
                } else {
                    return activity_reason_info::ok( do_activity_reason::NEEDS_VEH_REPAIR );
                }
            }
        }
        p.activity_vehicle_part_index = -1;
        return activity_reason_info::fail( do_activity_reason::NO_ZONE );
    }
    if( act == ACT_MULTIPLE_MINE ) {
        if( !here.has_flag( "MINEABLE", src_loc ) ) {
            return activity_reason_info::fail( do_activity_reason::NO_ZONE );
        }
        std::vector<item *> mining_inv = p.items_with( []( const item & itm ) {
            return ( itm.has_flag( flag_DIG_TOOL ) && !itm.type->can_use( "JACKHAMMER" ) ) ||
                   ( itm.type->can_use( "JACKHAMMER" ) && itm.ammo_sufficient() );
        } );
        if( mining_inv.empty() ) {
            return activity_reason_info::fail( do_activity_reason::NEEDS_MINING );
        } else {
            return activity_reason_info::ok( do_activity_reason::NEEDS_MINING );
        }
    }
    if( act == ACT_MULTIPLE_FISH ) {
        if( !here.has_flag( flag_FISHABLE, src_loc ) ) {
            return activity_reason_info::fail( do_activity_reason::NO_ZONE );
        }
        std::vector<item *> rod_inv = p.items_with( []( const item & itm ) {
            return itm.has_flag( flag_FISH_POOR ) || itm.has_flag( flag_FISH_GOOD );
        } );
        if( rod_inv.empty() ) {
            return activity_reason_info::fail( do_activity_reason::NEEDS_FISHING );
        } else {
            return activity_reason_info::ok( do_activity_reason::NEEDS_FISHING );
        }
    }
    if( act == ACT_MULTIPLE_CHOP_TREES ) {
        if( here.has_flag( flag_TREE, src_loc ) || here.ter( src_loc ) == t_trunk ||
            here.ter( src_loc ) == t_stump ) {
            if( p.has_quality( qual_AXE ) ) {
                return activity_reason_info::ok( do_activity_reason::NEEDS_TREE_CHOPPING );
            } else {
                return activity_reason_info::fail( do_activity_reason::NEEDS_TREE_CHOPPING );
            }
        } else {
            return activity_reason_info::fail( do_activity_reason::NO_ZONE );
        }
    }
    if( act == ACT_MULTIPLE_BUTCHER ) {
        std::vector<item *> corpses;
        int big_count = 0;
        int small_count = 0;
        for( const auto &i : here.i_at( src_loc ) ) {
            // make sure nobody else is working on that corpse right now
            if( i->is_corpse() && !i->has_var( "activity_var" ) ) {
                const mtype corpse = *i->get_mtype();
                if( corpse.size >= creature_size::medium ) {
                    big_count += 1;
                } else {
                    small_count += 1;
                }
                corpses.push_back( i );
            }
        }
        bool b_rack_present = false;
        for( const tripoint &pt : here.points_in_radius( src_loc, 2 ) ) {
            const inventory &inv = p.crafting_inventory();
            if( here.has_flag_furn( flag_BUTCHER_EQ, pt ) || inv.has_item_with( []( const item & it ) {
            return it.has_flag( flag_BUTCHER_RACK );
            } ) ) {
                b_rack_present = true;
            }
        }
        if( !corpses.empty() ) {
            if( big_count > 0 && small_count == 0 ) {
                const inventory &inv = p.crafting_inventory();
                if( !b_rack_present || !( here.has_nearby_table( src_loc, PICKUP_RANGE ) ||
                inv.has_item_with( []( const item & it ) {
                return it.has_flag( flag_FLAT_SURFACE );
                } ) ) ) {
                    return activity_reason_info::fail( do_activity_reason::NO_ZONE );
                }
                if( p.has_quality( quality_id( qual_BUTCHER ), 1 ) ) {
                    return activity_reason_info::ok( do_activity_reason::NEEDS_BIG_BUTCHERING );
                } else {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_BIG_BUTCHERING );
                }
            }
            if( ( big_count > 0 && small_count > 0 ) || ( big_count == 0 ) ) {
                // there are small corpses here, so we can ignore any big corpses here for the moment.
                if( p.has_quality( qual_BUTCHER, 1 ) ) {
                    return activity_reason_info::ok( do_activity_reason::NEEDS_BUTCHERING );
                } else {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_BUTCHERING );
                }
            }
        }
        return activity_reason_info::fail( do_activity_reason::NO_ZONE );
    }
    if( act == ACT_MULTIPLE_CHOP_PLANKS ) {
        //are there even any logs there?
        for( auto &i : here.i_at( src_loc ) ) {
            if( i->typeId() == itype_log ) {
                // do we have an axe?
                if( p.has_quality( qual_AXE, 1 ) ) {
                    return activity_reason_info::ok( do_activity_reason::NEEDS_CHOPPING );
                } else {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_CHOPPING );
                }
            }
        }
        return activity_reason_info::fail( do_activity_reason::NO_ZONE );
    }
    if( act == ACT_TIDY_UP ) {
        if( mgr.has_near( zone_type_LOOT_UNSORTED, here.getabs( src_loc ), distance ) ) {
            return activity_reason_info::ok( do_activity_reason::CAN_DO_FETCH );
        }
        return activity_reason_info::fail( do_activity_reason::NO_ZONE );
    }
    if( act == ACT_MULTIPLE_CONSTRUCTION ) {
        zones = mgr.get_zones( zone_type_CONSTRUCTION_BLUEPRINT,
                               here.getabs( src_loc ) );
        const partial_con *part_con = here.partial_con_at( src_loc );
        std::optional<construction_id> part_con_idx;
        if( part_con ) {
            part_con_idx = part_con->id;
        }
        const map_stack stuff_there = here.i_at( src_loc );

        // PICKUP_RANGE -1 because we will be adjacent to the spot when arriving.
        const inventory &pre_inv = p.crafting_inventory( src_loc, PICKUP_RANGE - 1 );
        for( const zone_data &zone : zones ) {
            const blueprint_options &options = dynamic_cast<const blueprint_options &>( zone.get_options() );
            const construction_id index = options.get_index();
            if( !stuff_there.empty() ) {
                return activity_reason_info::build( do_activity_reason::BLOCKING_TILE, false, index );
            }
            const activity_reason_info act_info = find_base_construction(
                    constructions::get_all_sorted(), p, pre_inv, src_loc, part_con_idx, index, false );
            return act_info;
        }
    } else if( act == ACT_MULTIPLE_FARM ) {
        zones = mgr.get_zones( zone_type_FARM_PLOT,
                               here.getabs( src_loc ) );
        for( const zone_data &zone : zones ) {
            const plot_options &options = dynamic_cast<const plot_options &>( zone.get_options() );
            const itype_id seed = options.get_seed();
            if( here.has_flag_furn( flag_GROWTH_HARVEST, src_loc ) ) {
                // simple work, pulling up plants, nothing else required.
                return activity_reason_info::ok( do_activity_reason::NEEDS_HARVESTING );
            } else if( here.has_flag( flag_PLOWABLE, src_loc ) && !here.has_furn( src_loc ) ) {
                if( p.has_quality( qual_DIG, 1 ) ) {
                    // we have a shovel/hoe already, great
                    return activity_reason_info::ok( do_activity_reason::NEEDS_TILLING );
                } else {
                    // we need a shovel/hoe
                    return activity_reason_info::fail( do_activity_reason::NEEDS_TILLING );
                }
            } else if( seed.is_valid() &&
                       here.has_flag_ter_or_furn( seed->seed->required_terrain_flag, src_loc ) ) {
                if( here.has_items( src_loc ) ) {
                    return activity_reason_info::fail( do_activity_reason::BLOCKING_TILE );
                } else if( !warm_enough_to_plant( src_loc ) ) {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_WARM_WEATHER );
                    // Plants underground need to be either valid to plant underground, or given artificial heating
                } else if( !seed.obj().has_flag( flag_CAN_PLANT_UNDERGROUND ) && src_loc.z < 0 &&
                           get_weather().get_temperature( src_loc ) < 10_c ) {
                    return activity_reason_info::fail( do_activity_reason::NEEDS_ABOVE_GROUND );
                } else {
                    // do we have the required seed on our person?
                    // If its a farm zone with no specified seed, and we've checked for tilling and harvesting.
                    // then it means no further work can be done here
                    if( seed.is_empty() ) {
                        return activity_reason_info::fail( do_activity_reason::ALREADY_DONE );
                    }
                    std::vector<item *> seed_inv = p.items_with( []( const item & itm ) {
                        return itm.is_seed();
                    } );
                    for( const auto elem : seed_inv ) {
                        if( elem->typeId() == itype_id( seed ) ) {
                            return activity_reason_info::ok( do_activity_reason::NEEDS_PLANTING );
                        }
                    }
                    // didn't find the seed, but maybe there are overlapping farm zones
                    // and another of the zones is for a seed that we have
                    // so loop again, and return false once all zones done.
                }

            } else {
                // can't plant, till or harvest
                return activity_reason_info::fail( do_activity_reason::ALREADY_DONE );
            }

        }
        // looped through all zones, and only got here if its plantable, but have no seeds.
        return activity_reason_info::fail( do_activity_reason::NEEDS_PLANTING );
    } else if( act == ACT_FETCH_REQUIRED ) {
        // we check if its possible to get all the requirements for fetching at two other places.
        // 1. before we even assign the fetch activity and;
        // 2. when we form the src_set to loop through at the beginning of the fetch activity.
        return activity_reason_info::ok( do_activity_reason::CAN_DO_FETCH );
    }
    // Shouldn't get here because the zones were checked previously. if it does, set enum reason as "no zone"
    return activity_reason_info::fail( do_activity_reason::NO_ZONE );
}

static std::vector<std::tuple<tripoint, itype_id, int>> requirements_map( player &p,
        const int distance = ACTIVITY_SEARCH_DISTANCE )
{
    std::vector<std::tuple<tripoint, itype_id, int>> requirement_map;
    if( p.backlog.empty() || p.backlog.front()->str_values.empty() ) {
        return requirement_map;
    }
    const requirement_data things_to_fetch = requirement_id( p.backlog.front()->str_values[0] ).obj();
    const activity_id activity_to_restore = p.backlog.front()->id();
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    requirement_id things_to_fetch_id = things_to_fetch.id();
    std::vector<std::vector<item_comp>> req_comps = things_to_fetch.get_components();
    std::vector<std::vector<tool_comp>> tool_comps = things_to_fetch.get_tools();
    std::vector<std::vector<quality_requirement>> quality_comps = things_to_fetch.get_qualities();
    zone_manager &mgr = zone_manager::get_manager();
    const bool pickup_task = p.backlog.front()->id() == ACT_MULTIPLE_FARM ||
                             p.backlog.front()->id() == ACT_MULTIPLE_CHOP_PLANKS ||
                             p.backlog.front()->id() == ACT_MULTIPLE_BUTCHER ||
                             p.backlog.front()->id() == ACT_MULTIPLE_CHOP_TREES ||
                             p.backlog.front()->id() == ACT_VEHICLE_DECONSTRUCTION ||
                             p.backlog.front()->id() == ACT_VEHICLE_REPAIR ||
                             p.backlog.front()->id() == ACT_MULTIPLE_FISH ||
                             p.backlog.front()->id() == ACT_MULTIPLE_MINE;
    // where it is, what it is, how much of it, and how much in total is required of that item.
    std::vector<std::tuple<tripoint, itype_id, int>> final_map;
    std::vector<tripoint> loot_spots;
    std::vector<tripoint> already_there_spots;
    std::vector<tripoint> combined_spots;
    std::map<itype_id, int> total_map;
    map &here = get_map();
    tripoint src_loc = here.getlocal( p.backlog.front()->placement );
    for( const tripoint &elem : here.points_in_radius( src_loc,
            PICKUP_RANGE - 1 ) ) {
        already_there_spots.push_back( elem );
        combined_spots.push_back( elem );
    }
    for( const tripoint &elem : mgr.get_point_set_loot( here.getabs( p.pos() ), distance,
            p.is_npc() ) ) {
        // if there is a loot zone that's already near the work spot, we don't want it to be added twice.
        if( std::ranges::find( already_there_spots,
                               elem ) != already_there_spots.end() ) {
            // construction tasks don't need the loot spot *and* the already_there/combined spots both added.
            // but a farming task will need to go and fetch the tool no matter if its near the work spot.
            // whereas the construction will automatically use what's nearby anyway.
            if( pickup_task ) {
                loot_spots.push_back( elem );
            } else {
                continue;
            }
        } else {
            loot_spots.push_back( elem );
            combined_spots.push_back( elem );
        }
    }
    // if the requirements aren't available, then stop.
    if( !are_requirements_nearby( pickup_task ? loot_spots : combined_spots, things_to_fetch_id, p,
                                  activity_to_restore, pickup_task, src_loc ) ) {
        return requirement_map;
    }
    // if the requirements are already near the work spot and its a construction/crafting task, then no need to fetch anything more.
    if( !pickup_task &&
        are_requirements_nearby( already_there_spots, things_to_fetch_id, p, activity_to_restore,
                                 false, src_loc ) ) {
        return requirement_map;
    }
    // a vector of every item in every tile that matches any part of the requirements.
    // will be filtered for amounts/charges afterwards.
    for( const tripoint &point_elem : pickup_task ? loot_spots : combined_spots ) {
        std::map<itype_id, int> temp_map;
        for( const item * const &stack_elem : here.i_at( point_elem ) ) {
            for( std::vector<item_comp> &elem : req_comps ) {
                for( item_comp &comp_elem : elem ) {
                    if( comp_elem.type == stack_elem->typeId() ) {
                        // if its near the work site, we can remove a count from the requirements.
                        // if two "lines" of the requirement have the same component appearing again
                        // that is fine, we will choose which "line" to fulfill later, and the decrement will count towards that then.
                        if( !pickup_task &&
                            std::ranges::find( already_there_spots,
                                               point_elem ) != already_there_spots.end() ) {
                            comp_elem.count -= stack_elem->count();
                        }
                        temp_map[stack_elem->typeId()] += stack_elem->count();
                    }
                }
            }
            for( std::vector<tool_comp> &elem : tool_comps ) {
                for( tool_comp &comp_elem : elem ) {
                    if( comp_elem.type == stack_elem->typeId() ) {
                        if( !pickup_task &&
                            std::ranges::find( already_there_spots,
                                               point_elem ) != already_there_spots.end() ) {
                            comp_elem.count -= stack_elem->count();
                        }
                        if( comp_elem.by_charges() ) {
                            // we don't care if there are 10 welders with 5 charges each
                            // we only want the one welder that has the required charge.
                            if( stack_elem->ammo_remaining() >= comp_elem.count ) {
                                temp_map[stack_elem->typeId()] += stack_elem->ammo_remaining();
                            }
                        } else {
                            temp_map[stack_elem->typeId()] += stack_elem->count();
                        }
                    }
                }
            }

            for( std::vector<quality_requirement> &elem : quality_comps ) {
                for( quality_requirement &comp_elem : elem ) {
                    const quality_id tool_qual = comp_elem.type;
                    const int qual_level = comp_elem.level;
                    if( stack_elem->has_quality( tool_qual, qual_level ) ) {
                        if( !pickup_task &&
                            std::ranges::find( already_there_spots,
                                               point_elem ) != already_there_spots.end() ) {
                            comp_elem.count -= stack_elem->count();
                        }
                        temp_map[stack_elem->typeId()] += stack_elem->count();
                    }
                }
            }
        }
        for( const auto &map_elem : temp_map ) {
            total_map[map_elem.first] += map_elem.second;
            // if its a construction/crafting task, we can discount any items already near the work spot.
            // we don't need to fetch those, they will be used automatically in the construction.
            // a shovel for tilling, for example, however, needs to be picked up, no matter if its near the spot or not.
            if( !pickup_task ) {
                if( std::ranges::find( already_there_spots,
                                       point_elem ) != already_there_spots.end() ) {
                    continue;
                }
            }
            requirement_map.emplace_back( point_elem, map_elem.first, map_elem.second );
        }
    }
    // Ok we now have a list of all the items that match the requirements, their points, and a quantity for each one.
    // we need to consolidate them, and winnow it down to the minimum required counts, instead of all matching.
    for( const std::vector<item_comp> &elem : req_comps ) {
        bool line_found = false;
        for( const item_comp &comp_elem : elem ) {
            if( line_found || comp_elem.count <= 0 ) {
                break;
            }
            int quantity_required = comp_elem.count;
            int item_quantity = 0;
            auto it = requirement_map.begin();
            int remainder = 0;
            while( it != requirement_map.end() ) {
                tripoint pos_here = std::get<0>( *it );
                itype_id item_here = std::get<1>( *it );
                int quantity_here = std::get<2>( *it );
                if( comp_elem.type == item_here ) {
                    item_quantity += quantity_here;
                }
                if( item_quantity >= quantity_required ) {
                    // it's just this spot that can fulfil the requirement on its own
                    final_map.emplace_back( pos_here, item_here, std::min<int>( quantity_here,
                                            quantity_required ) );
                    if( quantity_here >= quantity_required ) {
                        line_found = true;
                        break;
                    } else {
                        remainder = quantity_required - quantity_here;
                    }
                    break;
                }
                it++;
            }
            if( line_found ) {
                while( true ) {
                    // go back over things
                    if( it == requirement_map.begin() ) {
                        break;
                    }
                    if( remainder <= 0 ) {
                        line_found = true;
                        break;
                    }
                    tripoint pos_here2 = std::get<0>( *it );
                    itype_id item_here2 = std::get<1>( *it );
                    int quantity_here2 = std::get<2>( *it );
                    if( comp_elem.type == item_here2 ) {
                        if( quantity_here2 >= remainder ) {
                            final_map.emplace_back( pos_here2, item_here2, remainder );
                            line_found = true;
                        } else {
                            final_map.emplace_back( pos_here2, item_here2, remainder );
                            remainder -= quantity_here2;
                        }
                    }
                    it--;
                }
            }
        }
    }
    for( const std::vector<tool_comp> &elem : tool_comps ) {
        bool line_found = false;
        for( const tool_comp &comp_elem : elem ) {
            if( line_found || comp_elem.count < -1 ) {
                break;
            }
            int quantity_required = std::max( 1, comp_elem.count );
            int item_quantity = 0;
            auto it = requirement_map.begin();
            int remainder = 0;
            while( it != requirement_map.end() ) {
                tripoint pos_here = std::get<0>( *it );
                itype_id item_here = std::get<1>( *it );
                int quantity_here = std::get<2>( *it );
                if( comp_elem.type == item_here ) {
                    item_quantity += quantity_here;
                }
                if( item_quantity >= quantity_required ) {
                    // it's just this spot that can fulfil the requirement on its own
                    final_map.emplace_back( pos_here, item_here, std::min<int>( quantity_here,
                                            quantity_required ) );
                    if( quantity_here >= quantity_required ) {
                        line_found = true;
                        break;
                    } else {
                        remainder = quantity_required - quantity_here;
                    }
                    break;
                }
                it++;
            }
            if( line_found ) {
                while( true ) {
                    // go back over things
                    if( it == requirement_map.begin() ) {
                        break;
                    }
                    if( remainder <= 0 ) {
                        line_found = true;
                        break;
                    }
                    tripoint pos_here2 = std::get<0>( *it );
                    itype_id item_here2 = std::get<1>( *it );
                    int quantity_here2 = std::get<2>( *it );
                    if( comp_elem.type == item_here2 ) {
                        if( quantity_here2 >= remainder ) {
                            final_map.emplace_back( pos_here2, item_here2, remainder );
                            line_found = true;
                        } else {
                            final_map.emplace_back( pos_here2, item_here2, remainder );
                            remainder -= quantity_here2;
                        }
                    }
                    it--;
                }
            }
        }
    }
    for( const std::vector<quality_requirement> &elem : quality_comps ) {
        bool line_found = false;
        for( const quality_requirement &comp_elem : elem ) {
            if( line_found || comp_elem.count <= 0 ) {
                break;
            }
            const quality_id tool_qual = comp_elem.type;
            const int qual_level = comp_elem.level;
            for( auto it = requirement_map.begin(); it != requirement_map.end(); ) {
                tripoint pos_here = std::get<0>( *it );
                itype_id item_here = std::get<1>( *it );
                //TODO!: Check avoiding this construction, it's a bad one
                item &test_item = *item::spawn_temporary( item_here, calendar::start_of_cataclysm );
                if( test_item.has_quality( tool_qual, qual_level ) ) {
                    // it's just this spot that can fulfil the requirement on its own
                    final_map.emplace_back( pos_here, item_here, 1 );
                    line_found = true;
                    break;
                }
                it++;
            }
        }
    }
    for( const std::tuple<tripoint, itype_id, int> &elem : final_map ) {
        add_msg( m_debug, "%s is fetching %s from x: %d y: %d ", p.disp_name(),
                 std::get<1>( elem ).str(), std::get<0>( elem ).x, std::get<0>( elem ).y );
    }
    return final_map;
}

static bool construction_activity( player &p, const zone_data * /*zone*/, const tripoint &src_loc,
                                   const activity_reason_info &act_info,
                                   const activity_id &activity_to_restore )
{
    // the actual desired construction
    if( !act_info.con_idx ) {
        debugmsg( "no construction selected" );
        return false;
    }
    const construction &built_chosen = act_info.con_idx->obj();
    std::vector<detached_ptr<item>> used;
    // create the partial construction struct
    std::unique_ptr<partial_con> pc = std::make_unique<partial_con>( src_loc );
    pc->id = built_chosen.id;
    pc->counter = 0;
    map &here = get_map();
    // Set the trap that has the examine function
    if( here.tr_at( src_loc ).loadid == tr_null ) {
        here.trap_set( src_loc, tr_unfinished_construction );
    }
    // Use up the components
    for( const std::vector<item_comp> &it : built_chosen.requirements->get_components() ) {
        std::vector<detached_ptr<item>> tmp = p.consume_items( it, 1, is_crafting_component );
        used.insert( used.end(), std::make_move_iterator( tmp.begin() ),
                     std::make_move_iterator( tmp.end() ) );
    }
    for( detached_ptr<item> &it : used ) {
        pc->components.push_back( std::move( it ) );
    }
    here.partial_con_set( src_loc, std::move( pc ) );
    for( const std::vector<tool_comp> &it : built_chosen.requirements->get_tools() ) {
        p.consume_tools( it );
    }
    p.backlog.emplace_front( std::make_unique<player_activity>( activity_to_restore ) );
    p.assign_activity( std::make_unique<player_activity>( std::make_unique<construction_activity_actor>
                       ( here.getglobal( src_loc ) ) ) );
    return true;
}

static bool tidy_activity( player &p, const tripoint &src_loc,
                           const activity_id &activity_to_restore, const int distance = ACTIVITY_SEARCH_DISTANCE )
{
    auto &mgr = zone_manager::get_manager();
    map &here = get_map();
    tripoint loot_abspos = here.getabs( src_loc );
    tripoint loot_src_lot;
    const auto &zone_src_set = mgr.get_near( zone_type_LOOT_UNSORTED, loot_abspos, distance );
    if( !zone_src_set.empty() ) {
        const auto &zone_src_sorted = get_sorted_tiles_by_distance( loot_abspos, zone_src_set );
        // Find the nearest unsorted zone to dump objects at
        for( auto &src_elem : zone_src_sorted ) {
            if( !here.can_put_items_ter_furn( here.getlocal( src_elem ) ) ) {
                continue;
            }
            loot_src_lot = here.getlocal( src_elem );
            break;
        }
    }
    if( loot_src_lot == tripoint_zero ) {
        return false;
    }
    auto items_there = here.i_at( src_loc );
    for( auto &it : items_there ) {
        if( it->has_var( "activity_var" ) && it->get_var( "activity_var", "" ) == p.name ) {
            move_item( p, *it, it->count(), src_loc, loot_src_lot,
                       activity_to_restore );
            break;
        }
    }
    // we are adjacent to an unsorted zone, we came here to just drop items we are carrying
    if( mgr.has( zone_type_LOOT_UNSORTED, here.getabs( src_loc ) ) ) {
        for( item *inv_elem : p.inv_dump() ) {
            if( inv_elem->has_var( "activity_var" ) ) {
                inv_elem->erase_var( "activity_var" );
                put_into_vehicle_or_drop( p, item_drop_reason::deliberate, inv_elem->detach(),
                                          src_loc );
            }
        }
    }
    return true;
}

static bool fetch_activity( player &p, const tripoint &src_loc,
                            const activity_id &activity_to_restore, const int distance = ACTIVITY_SEARCH_DISTANCE )
{
    map &here = get_map();
    if( !here.can_put_items_ter_furn( here.getlocal( p.backlog.front()->coords.back() ) ) ) {
        return false;
    }
    const std::vector<std::tuple<tripoint, itype_id, int>> mental_map_2 = requirements_map( p,
            distance );
    int pickup_count = 1;
    auto items_there = here.i_at( src_loc );
    vehicle *src_veh = nullptr;
    int src_part = 0;
    if( const std::optional<vpart_reference> vp = here.veh_at( src_loc ).part_with_feature( "CARGO",
            false ) ) {
        src_veh = &vp->vehicle();
        src_part = vp->part_index();
    }
    const units::volume volume_allowed = p.volume_capacity() - p.volume_carried();
    const units::mass weight_allowed = p.weight_capacity() - p.weight_carried();
    // TODO: vehicle_stack and map_stack into one loop.
    if( src_veh ) {
        for( auto &veh_elem : src_veh->get_items( src_part ) ) {
            for( auto elem : mental_map_2 ) {
                if( std::get<0>( elem ) == src_loc && veh_elem->typeId() == std::get<1>( elem ) ) {
                    if( !p.backlog.empty() && p.backlog.front()->id() == ACT_MULTIPLE_CONSTRUCTION ) {
                        move_item( p, *veh_elem, veh_elem->count_by_charges() ? std::get<2>( elem ) : 1, src_loc,
                                   here.getlocal( p.backlog.front()->coords.back() ), activity_to_restore );
                        return true;
                    }
                }
            }
        }
    }
    for( auto item_iter = items_there.begin(); item_iter != items_there.end(); item_iter++ ) {
        item &it = **item_iter;
        for( auto elem : mental_map_2 ) {
            if( std::get<0>( elem ) == src_loc && it.typeId() == std::get<1>( elem ) ) {
                // construction/crafting tasks want the required item moved near the work spot.
                if( !p.backlog.empty() && p.backlog.front()->id() == ACT_MULTIPLE_CONSTRUCTION ) {
                    move_item( p, it, it.count_by_charges() ? std::get<2>( elem ) : 1, src_loc,
                               here.getlocal( p.backlog.front()->coords.back() ), activity_to_restore );
                    return true;
                    // other tasks want the tool picked up
                } else if( !p.backlog.empty() && ( p.backlog.front()->id() == ACT_MULTIPLE_FARM ||
                                                   p.backlog.front()->id() == ACT_MULTIPLE_CHOP_PLANKS ||
                                                   p.backlog.front()->id() == ACT_VEHICLE_DECONSTRUCTION ||
                                                   p.backlog.front()->id() == ACT_VEHICLE_REPAIR ||
                                                   p.backlog.front()->id() == ACT_MULTIPLE_BUTCHER ||
                                                   p.backlog.front()->id() == ACT_MULTIPLE_CHOP_TREES ||
                                                   p.backlog.front()->id() == ACT_MULTIPLE_FISH ||
                                                   p.backlog.front()->id() == ACT_MULTIPLE_MINE ) ) {
                    if( it.volume() > volume_allowed || it.weight() > weight_allowed ) {
                        continue;
                    }
                    //This invalidates our iterator but we don't care because it isn't used again
                    detached_ptr<item> moved = it.split( pickup_count ) ;
                    moved->set_var( "activity_var", p.name );
                    const std::string item_name = moved->tname();
                    if( p.is_npc() ) {
                        if( pickup_count == 1 ) {
                            add_msg( _( "%1$s picks up a %2$s." ), p.disp_name(), item_name );
                        } else {
                            add_msg( _( "%s picks up several items." ),  p.disp_name() );
                        }
                    }
                    p.i_add( std::move( moved ) );
                    return true;
                }
            }
        }
    }
    // if we got here, then the fetch failed for reasons that werent predicted before setting it.
    // nothing was moved or picked up, and nothing can be moved or picked up
    // so call the whole thing off to stop it looping back to this point ad nauseum.
    p.set_moves( 0 );
    p.activity = std::make_unique<player_activity>();
    p.backlog.clear();
    return false;
}

static bool butcher_corpse_activity( player &p, const tripoint &src_loc,
                                     const do_activity_reason &reason )
{
    map &here = get_map();
    map_stack items = here.i_at( src_loc );
    for( auto &elem : items ) {
        if( elem->is_corpse() && !elem->has_var( "activity_var" ) ) {
            const mtype corpse = *elem->get_mtype();
            if( corpse.size >= creature_size::medium && reason != do_activity_reason::NEEDS_BIG_BUTCHERING ) {
                continue;
            }
            elem->set_var( "activity_var", p.name );
            p.assign_activity( ACT_BUTCHER_FULL, 0, true );
            p.activity->targets.emplace_back( elem );
            p.activity->placement = here.getabs( src_loc );
            return true;
        }
    }
    return false;
}

static bool chop_plank_activity( player &p, const tripoint &src_loc )
{
    item *best_qual = p.best_quality_item( qual_AXE );
    if( !best_qual ) {
        return false;
    }
    if( best_qual->type->can_have_charges() ) {
        p.consume_charges( *best_qual, best_qual->type->charges_to_use() );
    }
    map &here = get_map();
    for( auto &i : here.i_at( src_loc ) ) {
        if( i->typeId() == itype_log ) {
            here.i_rem( src_loc, i );
            int moves = to_moves<int>( 20_minutes );
            p.add_msg_if_player( _( "You cut the log into planks." ) );
            p.assign_activity( ACT_CHOP_PLANKS, moves, -1 );
            p.activity->placement = here.getabs( src_loc );
            return true;
        }
    }
    return false;
}

void activity_on_turn_move_loot( player_activity &act, player &p )
{
    enum activity_stage : int {
        //Initial stage
        INIT = 0,
        //Think about what to do first: choose destination
        THINK,
        //Do activity
        DO,
    };

    int &stage = act.index;
    //Prepare activity stage
    if( stage < 0 ) {
        stage = INIT;
        //num_processed
        act.values.push_back( 0 );
    }
    int &num_processed = act.values[ 0 ];

    map &here = get_map();
    const auto abspos = here.getabs( p.pos() );
    auto &mgr = zone_manager::get_manager();
    if( here.check_vehicle_zones( g->get_levz() ) ) {
        mgr.cache_vzones();
    }

    if( stage == INIT ) {
        act.coord_set = mgr.get_near( zone_type_LOOT_UNSORTED, abspos, ACTIVITY_SEARCH_DISTANCE );
        stage = THINK;
    }

    if( stage == THINK ) {
        //initialize num_processed
        num_processed = 0;
        const auto &src_set = act.coord_set;
        // sort source tiles by distance
        const auto &src_sorted = get_sorted_tiles_by_distance( abspos, src_set );

        for( auto &src : src_sorted ) {
            act.placement = src;
            act.coord_set.erase( src );

            const auto &src_loc = here.getlocal( src );
            if( !here.inbounds( src_loc ) ) {
                if( !here.inbounds( p.pos() ) ) {
                    // p is implicitly an NPC that has been moved off the map, so reset the activity
                    // and unload them
                    p.cancel_activity();
                    p.assign_activity( ACT_MOVE_LOOT );
                    p.set_moves( 0 );
                    g->reload_npcs();
                    return;
                }
                std::vector<tripoint> route;
                route = here.route( p.pos(), src_loc, p.get_legacy_pathfinding_settings(),
                                    p.get_legacy_path_avoid() );
                if( route.empty() ) {
                    // can't get there, can't do anything, skip it
                    continue;
                }
                stage = DO;
                p.set_destination( route, p.remove_activity() );
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

            bool is_adjacent_or_closer = square_dist( p.pos(), src_loc ) <= 1;
            // before we move any item, check if player is at or
            // adjacent to the loot source tile
            if( !is_adjacent_or_closer ) {
                std::vector<tripoint> route;
                bool adjacent = false;

                // get either direct route or route to nearest adjacent tile if
                // source tile is impassable
                if( here.passable( src_loc ) ) {
                    route = here.route( p.pos(), src_loc, p.get_legacy_pathfinding_settings(),
                                        p.get_legacy_path_avoid() );
                } else {
                    // impassable source tile (locker etc.),
                    // get route to nearest adjacent tile instead
                    route = route_adjacent( p, src_loc );
                    adjacent = true;
                }

                // check if we found path to source / adjacent tile
                if( route.empty() ) {
                    add_msg( m_info, _( "%s can't reach the source tile.  Try to sort out loot without a cart." ),
                             p.disp_name() );
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
                p.set_destination( route, p.remove_activity() );
                return;
            }
            stage = DO;
            break;
        }
    }
    if( stage == DO ) {
        const tripoint &src = act.placement;
        const tripoint &src_loc = here.getlocal( src );

        bool is_adjacent_or_closer = square_dist( p.pos(), src_loc ) <= 1;
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
                if( !it->is_owned_by( p, true ) && it->get_owner()->likes_u >= -10 ) {
                    continue;
                }
                it->set_owner( p );
                items.emplace_back( it, true );
            }
        } else {
            src_veh = nullptr;
            src_part = -1;
        }
        for( auto &it : here.i_at( src_loc ) ) {
            if( !it->is_owned_by( p, true ) && it->get_owner()->likes_u >= -10 ) {
                continue;
            }
            it->set_owner( p );
            items.emplace_back( it, false );
        }

        //Skip items that have already been processed
        for( auto it = items.begin() + num_processed; it < items.end(); ++it ) {
            ++num_processed;
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

            const std::unordered_set<tripoint> &dest_set = mgr.get_near( id, abspos, ACTIVITY_SEARCH_DISTANCE,
                    &thisitem );
            for( const tripoint &dest : dest_set ) {
                const tripoint &dest_loc = here.getlocal( dest );

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
                    move_item( p, thisitem, thisitem.count(), src_loc, dest_loc );

                    // moved item away from source so decrement
                    if( num_processed > 0 ) {
                        --num_processed;
                    }
                    break;
                }
            }
            if( p.moves <= 0 ) {
                return;
            }
        }

        //this location is sorted
        stage = THINK;
        return;
    }

    // If we got here without restarting the activity, it means we're done
    add_msg( m_info, _( "%s sorted out every item possible." ), p.disp_name( false, true ) );
    if( p.is_npc() ) {
        npc *guy = dynamic_cast<npc *>( &p );
        guy->revert_after_activity();
    }
    p.activity->set_to_null();
}

static bool mine_activity( player &p, const tripoint &src_loc )
{
    map &here = get_map();
    std::vector<item *> mining_inv = p.items_with( []( const item & itm ) {
        return ( itm.has_flag( flag_DIG_TOOL ) && !itm.type->can_use( "JACKHAMMER" ) ) ||
               ( itm.type->can_use( "JACKHAMMER" ) && itm.ammo_sufficient() );
    } );
    if( mining_inv.empty() || p.is_mounted() || p.is_underwater() || here.veh_at( src_loc ) ||
        !here.has_flag( "MINEABLE", src_loc ) ) {
        return false;
    }
    item *chosen_item = nullptr;
    bool powered = false;
    // is it a pickaxe or jackhammer?
    for( item *elem : mining_inv ) {
        if( chosen_item == nullptr ) {
            chosen_item = elem;
            if( elem->type->can_use( "JACKHAMMER" ) ) {
                powered = true;
            }
        } else {
            // prioritise powered tools
            if( chosen_item->type->can_use( "PICKAXE" ) && elem->type->can_use( "JACKHAMMER" ) ) {
                chosen_item = elem;
                powered = true;
                break;
            }
        }
    }
    if( chosen_item == nullptr ) {
        return false;
    }
    int moves = to_moves<int>( powered ? 10_minutes : 30_minutes );
    if( powered ) {
        moves += ( ( MAX_STAT + 4 ) - std::min( p.str_cur, MAX_STAT ) ) * to_moves<int>( 75_seconds );
    } else {
        moves += ( ( MAX_STAT + 4 ) - std::min( p.str_cur, MAX_STAT ) ) * to_moves<int>( 225_seconds );
    }
    if( here.move_cost( src_loc ) == 2 ) {
        // We're breaking up some flat surface like pavement, which is much easier
        moves /= 2;
    }
    p.assign_activity( powered ? ACT_JACKHAMMER : ACT_PICKAXE, moves );
    p.activity->tools.emplace_back( chosen_item );
    p.activity->placement = here.getabs( src_loc );
    return true;

}

static bool chop_tree_activity( player &p, const tripoint &src_loc )
{
    item *best_qual = p.best_quality_item( qual_AXE );
    if( !best_qual ) {
        return false;
    }
    int moves = iuse::chop_moves( p, *best_qual );
    if( best_qual->type->can_have_charges() ) {
        p.consume_charges( *best_qual, best_qual->type->charges_to_use() );
    }
    map &here = get_map();
    const ter_id ter = here.ter( src_loc );
    if( here.has_flag( flag_TREE, src_loc ) ) {
        p.assign_activity( ACT_CHOP_TREE, moves, -1, p.get_item_position( best_qual ) );
        p.activity->tools.emplace_back( best_qual );
        p.activity->placement = here.getabs( src_loc );
        return true;
    } else if( ter == t_trunk || ter == t_stump ) {
        p.assign_activity( ACT_CHOP_LOGS, moves, -1, p.get_item_position( best_qual ) );
        p.activity->tools.emplace_back( best_qual );
        p.activity->placement = here.getabs( src_loc );
        return true;
    }
    return false;
}

static void check_npc_revert( player &p )
{
    if( p.is_npc() ) {
        npc *guy = dynamic_cast<npc *>( &p );
        if( guy ) {
            guy->revert_after_activity();
        }
    }
}

static zone_type_id get_zone_for_act( const tripoint &src_loc, const zone_manager &mgr,
                                      const activity_id &act_id )
{
    zone_type_id ret = zone_type_id( "" );
    if( act_id == ACT_VEHICLE_DECONSTRUCTION ) {
        ret = zone_type_VEHICLE_DECONSTRUCT;
    }
    if( act_id == ACT_VEHICLE_REPAIR ) {
        ret = zone_type_VEHICLE_REPAIR;
    }
    if( act_id == ACT_MULTIPLE_CHOP_TREES ) {
        ret = zone_type_CHOP_TREES;
    }
    if( act_id == ACT_MULTIPLE_CONSTRUCTION ) {
        ret = zone_type_CONSTRUCTION_BLUEPRINT;
    }
    if( act_id == ACT_MULTIPLE_FARM ) {
        ret = zone_type_FARM_PLOT;
    }
    if( act_id == ACT_MULTIPLE_BUTCHER ) {
        ret = zone_type_LOOT_CORPSE;
    }
    if( act_id == ACT_MULTIPLE_CHOP_PLANKS ) {
        ret = zone_type_LOOT_WOOD;
    }
    if( act_id == ACT_MULTIPLE_FISH ) {
        ret = zone_type_FISHING_SPOT;
    }
    if( act_id == ACT_MULTIPLE_MINE ) {
        ret = zone_type_MINING;
    }
    if( src_loc != tripoint_zero && act_id == ACT_FETCH_REQUIRED ) {
        const zone_data *zd = mgr.get_zone_at( get_map().getabs( src_loc ) );
        if( zd ) {
            ret = zd->get_type();
        }
    }
    return ret;
}

/** Determine all locations for this generic activity */
/** Returns locations */
static std::unordered_set<tripoint> generic_multi_activity_locations( player &p,
        const activity_id &act_id )
{
    bool dark_capable = false;
    std::unordered_set<tripoint> src_set;

    zone_manager &mgr = zone_manager::get_manager();
    const tripoint localpos = p.pos();
    map &here = get_map();
    const tripoint abspos = here.getabs( localpos );
    if( act_id == ACT_TIDY_UP ) {
        dark_capable = true;
        tripoint unsorted_spot;
        std::unordered_set<tripoint> unsorted_set = mgr.get_near( zone_type_LOOT_UNSORTED, abspos,
                ACTIVITY_SEARCH_DISTANCE );
        if( !unsorted_set.empty() ) {
            unsorted_spot = here.getlocal( random_entry( unsorted_set ) );
        }
        bool found_one_point = false;
        bool found_route = true;
        for( const tripoint &elem : here.points_in_radius( localpos,
                ACTIVITY_SEARCH_DISTANCE ) ) {
            // There's no point getting the entire list of all items to tidy up now.
            // the activity will run again after pathing to the first tile anyway.
            // tidy up activity has no requirements that will discount a square and
            // have the requirement to skip and scan the next one, ( other than checking path )
            // shortcircuiting the need to scan the entire map continuously can improve performance
            // especially if NPCs have a backlog of moves or there is a lot of them
            if( !found_route ) {
                found_route = true;
                continue;
            }
            if( found_one_point ) {
                break;
            }
            for( const item * const &stack_elem : here.i_at( elem ) ) {
                if( stack_elem->has_var( "activity_var" ) && stack_elem->get_var( "activity_var", "" ) == p.name ) {
                    const furn_t &f = here.furn( elem ).obj();
                    if( !f.has_flag( flag_PLANT ) ) {
                        src_set.insert( here.getabs( elem ) );
                        found_one_point = true;
                        // only check for a valid path, as that is all that is needed to tidy something up.
                        if( square_dist( p.pos(), elem ) > 1 ) {
                            std::vector<tripoint> route = route_adjacent( p, elem );
                            if( route.empty() ) {
                                found_route = false;
                            }
                        }
                        break;
                    }
                }
            }
        }
        if( src_set.empty() && unsorted_spot != tripoint_zero ) {
            for( const item *inv_elem : p.inv_dump() ) {
                if( inv_elem->has_var( "activity_var" ) ) {
                    // we've gone to tidy up all the things lying around, now tidy up the things we picked up.
                    src_set.insert( here.getabs( unsorted_spot ) );
                    break;
                }
            }
        }
    } else if( act_id != ACT_FETCH_REQUIRED ) {
        zone_type_id zone_type = get_zone_for_act( tripoint_zero, mgr, act_id );
        src_set = mgr.get_near( zone_type_id( zone_type ), abspos, ACTIVITY_SEARCH_DISTANCE );
        // multiple construction will form a list of targets based on blueprint zones and unfinished constructions
        if( act_id == ACT_MULTIPLE_CONSTRUCTION ) {
            for( const tripoint &elem : here.points_in_radius( localpos, ACTIVITY_SEARCH_DISTANCE ) ) {
                partial_con *pc = here.partial_con_at( elem );
                if( pc ) {
                    src_set.insert( here.getabs( elem ) );
                }
            }
            // farming activities encompass tilling, planting, harvesting.
        } else if( act_id == ACT_MULTIPLE_FARM ) {
            dark_capable = true;
        }
    } else {
        dark_capable = true;
        // get the right zones for the items in the requirements.
        // we previously checked if the items are nearby before we set the fetch task
        // but we will check again later, to be sure nothings changed.
        std::vector<std::tuple<tripoint, itype_id, int>> mental_map = requirements_map( p,
                ACTIVITY_SEARCH_DISTANCE );
        for( const auto &elem : mental_map ) {
            const tripoint &elem_point = std::get<0>( elem );
            src_set.insert( here.getabs( elem_point ) );
        }
    }
    // prune the set to remove tiles that are never gonna work out.
    const bool pre_dark_check = src_set.empty();
    for( auto it2 = src_set.begin(); it2 != src_set.end(); ) {
        // remove dangerous tiles
        const tripoint set_pt = here.getlocal( *it2 );
        if( here.dangerous_field_at( set_pt ) ) {
            it2 = src_set.erase( it2 );
            // remove tiles in darkness, if we aren't lit-up ourselves
        } else if( !dark_capable && !character_funcs::can_see_fine_details( p, set_pt ) ) {
            it2 = src_set.erase( it2 );
        } else if( act_id == ACT_MULTIPLE_FISH ) {
            const ter_id terrain_id = here.ter( set_pt );
            if( !terrain_id.obj().has_flag( TFLAG_DEEP_WATER ) ) {
                it2 = src_set.erase( it2 );
            } else {
                ++it2;
            }
        } else {
            ++it2;
        }
    }
    const bool post_dark_check = src_set.empty();
    if( !pre_dark_check && post_dark_check ) {
        p.add_msg_if_player( m_info, _( "It is too dark to do this activity." ) );
    }
    return src_set;
}

/** Check if this activity can not be done immediately because it has some requirements */
static requirement_check_result generic_multi_activity_check_requirement( player &p,
        const activity_id &act_id, activity_reason_info &act_info,
        const tripoint &src, const tripoint &src_loc, const std::unordered_set<tripoint> &src_set,
        const bool check_only = false )
{
    map &here = get_map();
    const tripoint abspos = here.getabs( p.pos() );
    zone_manager &mgr = zone_manager::get_manager();

    bool &can_do_it = act_info.can_do;
    const do_activity_reason &reason = act_info.reason;
    const zone_data *zone = mgr.get_zone_at( src, get_zone_for_act( src_loc, mgr, act_id ) );

    const bool needs_to_be_in_zone = act_id == ACT_FETCH_REQUIRED ||
                                     act_id == ACT_MULTIPLE_FARM ||
                                     act_id == ACT_MULTIPLE_BUTCHER ||
                                     act_id == ACT_MULTIPLE_CHOP_PLANKS ||
                                     act_id == ACT_MULTIPLE_CHOP_TREES ||
                                     act_id == ACT_VEHICLE_DECONSTRUCTION ||
                                     act_id == ACT_VEHICLE_REPAIR ||
                                     act_id == ACT_MULTIPLE_FISH ||
                                     act_id == ACT_MULTIPLE_MINE ||
                                     ( act_id == ACT_MULTIPLE_CONSTRUCTION &&
                                       !here.partial_con_at( src_loc ) );
    // some activities require the target tile to be part of a zone.
    // tidy up activity doesn't - it wants things that may not be in a zone already - things that may have been left lying around.
    if( needs_to_be_in_zone && !zone ) {
        can_do_it = false;
        return SKIP_LOCATION;
    }
    if( can_do_it ) {
        return CAN_DO_LOCATION;
    }
    if( reason == do_activity_reason::DONT_HAVE_SKILL ||
        reason == do_activity_reason::NO_ZONE ||
        reason == do_activity_reason::ALREADY_DONE ||
        reason == do_activity_reason::BLOCKING_TILE ||
        reason == do_activity_reason::NEEDS_WARM_WEATHER ||
        reason == do_activity_reason::NEEDS_ABOVE_GROUND ||
        reason == do_activity_reason::UNKNOWN_ACTIVITY ) {
        // we can discount this tile, the work can't be done.
        if( reason == do_activity_reason::DONT_HAVE_SKILL ) {
            p.add_msg_if_player( m_info, _( "You don't have the skill for this task." ) );
        } else if( reason == do_activity_reason::BLOCKING_TILE ) {
            p.add_msg_if_player( m_info, _( "There is something blocking the location for this task." ) );
        } else if( reason == do_activity_reason::NEEDS_WARM_WEATHER ) {
            p.add_msg_if_player( m_info, _( "It is too cold to plant anything now." ) );
        } else if( reason == do_activity_reason::NEEDS_ABOVE_GROUND ) {
            p.add_msg_if_player( m_info,
                                 _( "It's too cold down here to plant this type of seed underground." ) );
        }
        return SKIP_LOCATION;
    } else if( reason == do_activity_reason::NO_COMPONENTS ||
               reason == do_activity_reason::NO_COMPONENTS_PREREQ ||
               reason == do_activity_reason::NO_COMPONENTS_PREREQ_2 ||
               reason == do_activity_reason::NEEDS_PLANTING ||
               reason == do_activity_reason::NEEDS_TILLING ||
               reason == do_activity_reason::NEEDS_CHOPPING ||
               reason == do_activity_reason::NEEDS_BUTCHERING ||
               reason == do_activity_reason::NEEDS_BIG_BUTCHERING ||
               reason == do_activity_reason::NEEDS_VEH_DECONST ||
               reason == do_activity_reason::NEEDS_VEH_REPAIR ||
               reason == do_activity_reason::NEEDS_TREE_CHOPPING ||
               reason == do_activity_reason::NEEDS_FISHING || reason == do_activity_reason::NEEDS_MINING ) {
        // we can do it, but we need to fetch some stuff first
        // before we set the task to fetch components - is it even worth it? are the components anywhere?
        requirement_id what_we_need;
        std::vector<tripoint> loot_zone_spots;
        std::vector<tripoint> combined_spots;
        for( const tripoint &elem : mgr.get_point_set_loot( abspos, ACTIVITY_SEARCH_DISTANCE,
                p.is_npc() ) ) {
            loot_zone_spots.push_back( elem );
            combined_spots.push_back( elem );
        }
        for( const tripoint &elem : here.points_in_radius( src_loc, PICKUP_RANGE - 1 ) ) {
            combined_spots.push_back( elem );
        }

        if( ( reason == do_activity_reason::NO_COMPONENTS ||
              reason == do_activity_reason::NO_COMPONENTS_PREREQ ||
              reason == do_activity_reason::NO_COMPONENTS_PREREQ_2 ) &&
            act_id == ACT_MULTIPLE_CONSTRUCTION ) {
            if( !act_info.con_idx ) {
                debugmsg( "no construction selected" );
                return SKIP_LOCATION;
            }
            // its a construction and we need the components.
            const construction &built_chosen = act_info.con_idx->obj();
            what_we_need = built_chosen.requirements;
        } else if( reason == do_activity_reason::NEEDS_VEH_DECONST ||
                   reason == do_activity_reason::NEEDS_VEH_REPAIR ) {
            const vehicle *veh = veh_pointer_or_null( here.veh_at( src_loc ) );
            // we already checked this in can_do_activity() but check again just incase.
            if( !veh ) {
                p.activity_vehicle_part_index = 1;
                return SKIP_LOCATION;
            }
            const vpart_info &vpinfo = veh->part_info( p.activity_vehicle_part_index );
            requirement_data reqs;
            if( reason == do_activity_reason::NEEDS_VEH_DECONST ) {
                reqs = vpinfo.removal_requirements();
            } else if( reason == do_activity_reason::NEEDS_VEH_REPAIR ) {
                reqs = vpinfo.repair_requirements();
            }
            const std::string ran_str = random_string( 10 );
            const requirement_id req_id( ran_str );
            requirement_data::save_requirement( reqs, req_id );
            what_we_need = req_id;
        } else if( reason == do_activity_reason::NEEDS_MINING ) {
            what_we_need = requirement_id( "mining_standard" );
        } else if( reason == do_activity_reason::NEEDS_TILLING ||
                   reason == do_activity_reason::NEEDS_PLANTING ||
                   reason == do_activity_reason::NEEDS_CHOPPING ||
                   reason == do_activity_reason::NEEDS_BUTCHERING ||
                   reason == do_activity_reason::NEEDS_BIG_BUTCHERING ||
                   reason == do_activity_reason::NEEDS_TREE_CHOPPING ||
                   reason == do_activity_reason::NEEDS_FISHING ) {
            std::vector<std::vector<item_comp>> requirement_comp_vector;
            std::vector<std::vector<quality_requirement>> quality_comp_vector;
            std::vector<std::vector<tool_comp>> tool_comp_vector;
            if( reason == do_activity_reason::NEEDS_TILLING ) {
                quality_comp_vector.push_back( std::vector<quality_requirement> { quality_requirement( qual_DIG, 1, 1 ) } );
            } else if( reason == do_activity_reason::NEEDS_CHOPPING ||
                       reason == do_activity_reason::NEEDS_TREE_CHOPPING ) {
                quality_comp_vector.push_back( std::vector<quality_requirement> { quality_requirement( qual_AXE, 1, 1 ) } );
            } else if( reason == do_activity_reason::NEEDS_PLANTING ) {
                requirement_comp_vector.push_back( std::vector<item_comp> { item_comp( itype_id( dynamic_cast<const plot_options &>
                                                   ( zone->get_options() ).get_seed() ), 1 )
                                                                          } );
            } else if( reason == do_activity_reason::NEEDS_BUTCHERING ||
                       reason == do_activity_reason::NEEDS_BIG_BUTCHERING ) {
                quality_comp_vector.push_back( std::vector<quality_requirement> { quality_requirement( qual_BUTCHER, 1, 1 ) } );
                if( reason == do_activity_reason::NEEDS_BIG_BUTCHERING ) {
                    quality_comp_vector.push_back( std::vector<quality_requirement> { quality_requirement( qual_SAW_M, 1, 1 ), quality_requirement( qual_SAW_W, 1, 1 ) } );
                }

            } else if( reason == do_activity_reason::NEEDS_FISHING ) {
                quality_comp_vector.push_back( std::vector<quality_requirement> {quality_requirement( qual_FISHING, 1, 1 )} );
            }
            // ok, we need a shovel/hoe/axe/etc.
            // this is an activity that only requires this one tool, so we will fetch and wield it.
            requirement_data reqs_data = requirement_data( tool_comp_vector, quality_comp_vector,
                                         requirement_comp_vector );
            const std::string ran_str = random_string( 10 );
            const requirement_id req_id( ran_str );
            requirement_data::save_requirement( reqs_data, req_id );
            what_we_need = req_id;
        }
        bool tool_pickup = reason == do_activity_reason::NEEDS_TILLING ||
                           reason == do_activity_reason::NEEDS_PLANTING ||
                           reason == do_activity_reason::NEEDS_CHOPPING ||
                           reason == do_activity_reason::NEEDS_BUTCHERING ||
                           reason == do_activity_reason::NEEDS_BIG_BUTCHERING ||
                           reason == do_activity_reason::NEEDS_TREE_CHOPPING ||
                           reason == do_activity_reason::NEEDS_VEH_DECONST ||
                           reason == do_activity_reason::NEEDS_VEH_REPAIR ||
                           reason == do_activity_reason::NEEDS_MINING;
        // is it even worth fetching anything if there isn't enough nearby?
        if( !are_requirements_nearby( tool_pickup ? loot_zone_spots : combined_spots, what_we_need, p,
                                      act_id, tool_pickup, src_loc ) ) {
            p.add_msg_if_player( m_info, _( "The required items are not available to complete this task." ) );
            if( reason == do_activity_reason::NEEDS_VEH_DECONST ||
                reason == do_activity_reason::NEEDS_VEH_REPAIR ) {
                p.activity_vehicle_part_index = -1;
            }
            return SKIP_LOCATION;
        } else {
            if( !check_only ) {
                p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
                p.assign_activity( ACT_FETCH_REQUIRED );
                player_activity &act_prev = *p.backlog.front();
                act_prev.str_values.push_back( what_we_need.str() );
                act_prev.values.push_back( static_cast<int>( reason ) );
                // come back here after successfully fetching your stuff
                if( act_prev.coords.empty() ) {
                    std::vector<tripoint> local_src_set;
                    local_src_set.reserve( src_set.size() );
                    for( const tripoint &elem : src_set ) {
                        local_src_set.push_back( here.getlocal( elem ) );
                    }
                    std::vector<tripoint> candidates;
                    for( const tripoint &point_elem : here.points_in_radius( src_loc, PICKUP_RANGE - 1 ) ) {
                        // we don't want to place the components where they could interfere with our ( or someone else's ) construction spots
                        if( !p.sees( point_elem ) || ( std::ranges::find( local_src_set,
                                                       point_elem ) != local_src_set.end() ) || !here.can_put_items_ter_furn( point_elem ) ) {
                            continue;
                        }
                        candidates.push_back( point_elem );
                    }
                    if( candidates.empty() ) {
                        p.activity = std::make_unique<player_activity>();
                        p.backlog.clear();
                        check_npc_revert( p );
                        return SKIP_LOCATION;
                    }
                    act_prev.coords.push_back( here.getabs( candidates[std::max( 0,
                                                                      static_cast<int>( candidates.size() / 2 ) )] ) );
                }
                act_prev.placement = src;
            }
            return RETURN_EARLY;
        }
    }
    return SKIP_LOCATION;
}

/** Do activity at this location */
/** Returns true if this multi activity may be processed further */
static bool generic_multi_activity_do( player &p, const activity_id &act_id,
                                       const activity_reason_info &act_info,
                                       const tripoint &src, const tripoint &src_loc )
{
    // If any of the following activities return without processing
    // then they MUST return true here, to stop infinite loops.
    zone_manager &mgr = zone_manager::get_manager();

    const do_activity_reason &reason = act_info.reason;
    const zone_data *zone = mgr.get_zone_at( src, get_zone_for_act( src_loc, mgr, act_id ) );
    map &here = get_map();
    // something needs to be done, now we are there.
    // it was here earlier, in the space of one turn, maybe it got harvested by someone else.
    if( reason == do_activity_reason::NEEDS_HARVESTING &&
        here.has_flag_furn( flag_GROWTH_HARVEST, src_loc ) ) {
        iexamine::harvest_plant( p, src_loc, true );
    } else if( reason == do_activity_reason::NEEDS_TILLING && here.has_flag( flag_PLOWABLE, src_loc ) &&
               p.has_quality( qual_DIG, 1 ) && !here.has_furn( src_loc ) ) {
        p.assign_activity( ACT_CHURN, 18000, -1 );
        p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
        p.activity->placement = src;
        return false;
    } else if( reason == do_activity_reason::NEEDS_PLANTING ) {
        std::vector<zone_data> zones = mgr.get_zones( zone_type_FARM_PLOT,
                                       here.getabs( src_loc ) );
        for( const zone_data &zone : zones ) {
            const itype_id seed =
                dynamic_cast<const plot_options &>( zone.get_options() ).get_seed();
            std::vector<item *> seed_inv = p.items_with( [seed]( const item & itm ) {
                return itm.typeId() == itype_id( seed );
            } );
            // we don't have the required seed, even though we should at this point.
            // move onto the next tile, and if need be that will prompt a fetch seeds activity.
            if( seed_inv.empty() ) {
                continue;
            }
            if( !here.has_flag_ter_or_furn( seed->seed->required_terrain_flag, src_loc ) ) {
                continue;
            }
            iexamine::plant_seed( p, src_loc, itype_id( seed ) );
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            return false;
        }
    } else if( reason == do_activity_reason::NEEDS_CHOPPING && p.has_quality( qual_AXE, 1 ) ) {
        if( chop_plank_activity( p, src_loc ) ) {
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            return false;
        }
    } else if( reason == do_activity_reason::NEEDS_BUTCHERING ||
               reason == do_activity_reason::NEEDS_BIG_BUTCHERING ) {
        if( butcher_corpse_activity( p, src_loc, reason ) ) {
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            return false;
        }
    } else if( reason == do_activity_reason::CAN_DO_CONSTRUCTION ||
               reason == do_activity_reason::CAN_DO_PREREQ ) {
        if( here.partial_con_at( src_loc ) ) {
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            p.assign_activity( std::make_unique<player_activity>( std::make_unique<construction_activity_actor>
                               ( tripoint_abs_ms( src ) ) ) );
            return false;
        }
        if( construction_activity( p, zone, src_loc, act_info, act_id ) ) {
            return false;
        }
    } else if( reason == do_activity_reason::CAN_DO_FETCH && act_id == ACT_TIDY_UP ) {
        if( !tidy_activity( p, src_loc, act_id, ACTIVITY_SEARCH_DISTANCE ) ) {
            return false;
        }
    } else if( reason == do_activity_reason::CAN_DO_FETCH && act_id == ACT_FETCH_REQUIRED ) {
        if( fetch_activity( p, src_loc, act_id, ACTIVITY_SEARCH_DISTANCE ) ) {
            if( !p.is_npc() ) {
                // Npcs will automatically start the next thing in the backlog, players need to be manually prompted
                // Because some player activities are necessarily not marked as auto-resume.
                activity_handlers::resume_for_multi_activities( p );
            }
            return false;
        }
    } else if( reason == do_activity_reason::NEEDS_TREE_CHOPPING && p.has_quality( qual_AXE, 1 ) ) {
        if( chop_tree_activity( p, src_loc ) ) {
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            return false;
        }
    } else if( reason == do_activity_reason::NEEDS_FISHING && p.has_quality( qual_FISHING, 1 ) ) {
        p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
        // we don't want to keep repeating the fishing activity, just piggybacking on this functions structure to find requirements.
        p.activity = std::make_unique<player_activity>();
        item *best_rod = p.best_quality_item( qual_FISHING );
        p.assign_activity( std::make_unique<player_activity>( ACT_FISH, to_moves<int>( 5_hours ), 0,
                           0, best_rod->tname() ) );
        p.activity->tools.emplace_back( best_rod );
        p.activity->coord_set = g->get_fishable_locations( ACTIVITY_SEARCH_DISTANCE, src_loc );
        return false;
    } else if( reason == do_activity_reason::NEEDS_MINING ) {
        // if have enough batteries to continue etc.
        p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
        if( mine_activity( p, src_loc ) ) {
            return false;
        }
    } else if( reason == do_activity_reason::NEEDS_VEH_DECONST ) {
        if( vehicle_activity( p, src_loc, p.activity_vehicle_part_index, 'o' ) ) {
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            return false;
        }
        p.activity_vehicle_part_index = -1;
    } else if( reason == do_activity_reason::NEEDS_VEH_REPAIR ) {
        if( vehicle_activity( p, src_loc, p.activity_vehicle_part_index, 'r' ) ) {
            p.backlog.emplace_front( std::make_unique<player_activity>( act_id ) );
            return false;
        }
        p.activity_vehicle_part_index = -1;
    }
    return true;
}

bool generic_multi_activity_handler( player_activity &act, player &p, bool check_only )
{
    map &here = get_map();
    const tripoint abspos = here.getabs( p.pos() );
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    activity_id activity_to_restore = act.id();
    // Nuke the current activity, leaving the backlog alone
    if( !check_only ) {
        p.activity = std::make_unique<player_activity>();
    }
    // now we setup the target spots based on which activity is occurring
    // the set of target work spots - potentially after we have fetched required tools.
    std::unordered_set<tripoint> src_set = generic_multi_activity_locations( p, activity_to_restore );
    // now we have our final set of points
    std::vector<tripoint> src_sorted = get_sorted_tiles_by_distance( abspos, src_set );
    // now loop through the work-spot tiles and judge whether its worth traveling to it yet
    // or if we need to fetch something first.
    for( const tripoint &src : src_sorted ) {
        const tripoint &src_loc = here.getlocal( src );
        if( !here.inbounds( src_loc ) && !check_only ) {
            if( !here.inbounds( p.pos() ) ) {
                // p is implicitly an NPC that has been moved off the map, so reset the activity
                // and unload them
                p.assign_activity( std::make_unique<player_activity>( activity_to_restore ) );
                p.set_moves( 0 );
                g->reload_npcs();
                return false;
            }
            const std::vector<tripoint> route = route_adjacent( p, src_loc );
            if( route.empty() ) {
                // can't get there, can't do anything, skip it
                continue;
            }
            p.set_moves( 0 );
            p.set_destination( route, std::make_unique<player_activity>( activity_to_restore ) );
            return false;
        }
        activity_reason_info act_info = can_do_activity_there( activity_to_restore, p,
                                        src_loc, ACTIVITY_SEARCH_DISTANCE );
        // see activity_handlers.h enum for requirement_check_result
        const requirement_check_result req_res = generic_multi_activity_check_requirement( p,
                activity_to_restore, act_info, src, src_loc, src_set, check_only );
        if( req_res == SKIP_LOCATION ) {
            continue;
        } else if( req_res == RETURN_EARLY ) {
            return true;
        }

        if( square_dist( p.pos(), src_loc ) > 1 ) {
            std::vector<tripoint> route = route_adjacent( p, src_loc );

            // check if we found path to source / adjacent tile
            if( route.empty() ) {
                check_npc_revert( p );
                continue;
            }
            if( !check_only ) {
                if( p.moves <= 0 ) {
                    // Restart activity and break from cycle.
                    p.assign_activity( activity_to_restore );
                    return true;
                }
                // set the destination and restart activity after player arrives there
                // we don't need to check for safe mode,
                // activity will be restarted only if
                // player arrives on destination tile
                p.set_destination( route, std::make_unique<player_activity>( activity_to_restore ) );
                return true;
            }
        }
        // we checked if the work spot was in darkness earlier
        // but there is a niche case where the player is in darkness but the work spot is not
        // this can create infinite loops
        // and we can't check player.pos() for darkness before they've traveled to where they are going to be.
        // but now we are here, we check
        if( activity_to_restore != ACT_TIDY_UP &&
            activity_to_restore != ACT_MOVE_LOOT &&
            activity_to_restore != ACT_FETCH_REQUIRED &&
            !character_funcs::can_see_fine_details( p ) ) {
            p.add_msg_if_player( m_info, _( "It is too dark to work here." ) );
            return false;
        }
        if( !check_only ) {
            if( !generic_multi_activity_do( p, activity_to_restore, act_info, src, src_loc ) ) {
                // if the activity was succesful
                // then a new activity was assigned
                // and the backlog was given the multi-act
                return false;
            }
        } else {
            return true;
        }
    }
    if( !check_only ) {
        if( p.moves <= 0 ) {
            // Restart activity and break from cycle.
            p.assign_activity( activity_to_restore );
            p.activity_vehicle_part_index = -1;
            return false;
        }
        // if we got here, we need to revert otherwise NPC will be stuck in AI Limbo and have a head explosion.
        if( p.backlog.empty() || src_set.empty() ) {
            check_npc_revert( p );
            // tidy up leftover moved parts and tools left lying near the work spots.
            if( player_activity( activity_to_restore ).is_multi_type() ) {
                p.assign_activity( ACT_TIDY_UP );
            }
        }
        p.activity_vehicle_part_index = -1;
    }
    // scanned every location, tried every path.
    return false;
}

static std::optional<tripoint> find_best_fire( const std::vector<tripoint> &from,
        const tripoint &center )
{
    std::optional<tripoint> best_fire;
    time_duration best_fire_age = 1_days;
    map &here = get_map();
    for( const tripoint &pt : from ) {
        field_entry *fire = here.get_field( pt, fd_fire );
        if( fire == nullptr || fire->get_field_intensity() > 1 ||
            !here.clear_path( center, pt, PICKUP_RANGE, 1, 100 ) ) {
            continue;
        }
        time_duration fire_age = fire->get_field_age();
        // Refuel only the best fueled fire (if it needs it)
        if( fire_age < best_fire_age ) {
            best_fire = pt;
            best_fire_age = fire_age;
        }
        // If a contained fire exists, ignore any other fires
        if( here.has_flag_furn( TFLAG_FIRE_CONTAINER, pt ) ) {
            return pt;
        }
    }

    return best_fire;
}

static inline bool has_clear_path_to_pickup_items( const tripoint &from, const tripoint &to )
{
    map &here = get_map();
    return here.has_items( to ) &&
           here.accessible_items( to ) &&
           here.clear_path( from, to, PICKUP_RANGE, 1, 100 );
}

static std::optional<tripoint> find_refuel_spot_zone( const tripoint &center )
{
    const zone_manager &mgr = zone_manager::get_manager();
    map &here = get_map();
    const tripoint center_abs = here.getabs( center );

    const std::unordered_set<tripoint> &tiles_abs_unordered =
        mgr.get_near( zone_type_source_firewood, center_abs, PICKUP_RANGE );
    const std::vector<tripoint> &tiles_abs =
        get_sorted_tiles_by_distance( center_abs, tiles_abs_unordered );

    for( const tripoint &tile_abs : tiles_abs ) {
        const tripoint tile = here.getlocal( tile_abs );
        if( has_clear_path_to_pickup_items( center, tile ) ) {
            return tile;
        }
    }

    return {};
}

static std::optional<tripoint> find_refuel_spot_trap( const std::vector<tripoint> &from,
        const tripoint &center )
{
    const auto tile = std::ranges::find_if( from, [center]( const tripoint & pt ) {
        // Hacky - firewood spot is a trap and it's ID-checked
        return get_map().tr_at( pt ).id == tr_firewood_source
               && has_clear_path_to_pickup_items( center, pt );
    } );

    if( tile != from.end() ) {
        return *tile;
    }

    return {};
}

bool find_auto_consume( player &p, const consume_type type )
{
    // return false if there is no point searching again while the activity is still happening.
    if( p.is_npc() ) {
        return false;
    }
    if( p.has_effect( effect_nausea ) ) {
        return true;
    }
    const tripoint pos = p.pos();
    map &here = get_map();
    zone_manager &mgr = zone_manager::get_manager();
    const zone_type_id consume_type_zone( type == consume_type::FOOD ? "AUTO_EAT" : "AUTO_DRINK" );
    if( here.check_vehicle_zones( g->get_levz() ) ) {
        mgr.cache_vzones();
    }

    const auto ok_to_consume = [&p, type]( const item * it ) -> bool {
        const item &comest = p.get_consumable_from( const_cast<item &>( *it ) );
        /* not food.              */
        if( comest.is_null() || comest.is_craft() || !comest.is_food() )
        {
            return false;
        }
        /* not enjoyable.         */
        if( p.fun_for( comest ).first < -5 )
        {
            return false;
        }
        /* cannot consume.        */
        if( !p.can_consume( comest ) )
        {
            return false;
        }
        /* wont eat, e.g cannibal */
        if( !p.will_eat( comest, false ).success() )
        {
            return false;
        }
        /* not ours, freely steal from hostiles however  */
        if( !it->is_owned_by( p, true ) && it->get_owner()->likes_u >= -10 )
        {
            return false;
        }
        /* not quenching enough or won't sate auto-eat   */
        if( ( type == consume_type::DRINK && comest.get_comestible()->quench < 15 ) || ( type == consume_type::FOOD && p.compute_effective_nutrients( comest ).kcal < 1 ) )
        {
            return false;
        }
        /* Unsafe to drink or eat */
        if( comest.has_flag( flag_UNSAFE_CONSUME ) || comest.get_comestible()->parasites > 0 )
        {
            return false;
        }
        /* Avoid items that may softlock with a Y/N query */
        if( comest.has_use() )
        {
            return false;
        }
        return true;
    };

    using namespace cata::ranges;

    auto get_spoil = []( const item * a ) { return a->spoilage_sort_order(); };

    std::optional<item *> stalest = mgr.get_near( consume_type_zone, here.getabs( pos ),
                                    ACTIVITY_SEARCH_DISTANCE )
                                    | views::filter( [&]( const auto & loc ) -> bool { return loc.z == p.pos().z; } )
                                    | flat_map( get_items_at )
                                    | views::filter( ok_to_consume )
                                    | min_by( get_spoil );
    if( !stalest ) {
        return false;
    }

    // actually eat
    const auto cost = pickup::cost_to_move_item( p, **stalest );
    const auto dist = std::max( rl_dist( p.pos(), ( *stalest )->position() ), 1 );
    p.mod_moves( -cost * dist );

    item *item_loc = &p.get_consumable_from( **stalest );
    avatar_action::eat( get_avatar(), item_loc );
    // eat() may have removed the item, so check its still there.
    if( item_loc && item_loc->is_container() ) {
        item_loc->on_contents_changed();
    }
    return true;
}

void try_fuel_fire( player_activity &act, player &p, const bool starting_fire )
{
    const tripoint pos = p.pos();
    std::vector<tripoint> adjacent = closest_points_first( pos, PICKUP_RANGE );
    adjacent.erase( adjacent.begin() );

    std::optional<tripoint> best_fire = starting_fire ? act.placement : find_best_fire( adjacent,
                                        pos );

    map &here = get_map();
    if( !best_fire || !here.accessible_items( *best_fire ) ) {
        return;
    }

    std::optional<tripoint> refuel_spot = find_refuel_spot_zone( pos );
    if( !refuel_spot ) {
        refuel_spot = find_refuel_spot_trap( adjacent, pos );
        if( !refuel_spot ) {
            return;
        }
    }

    // Special case: fire containers allow burning logs, so use them as fuel if fire is contained
    bool contained = here.has_flag_furn( TFLAG_FIRE_CONTAINER, *best_fire );
    fire_data fd( 1, contained );
    time_duration fire_age = here.get_field_age( *best_fire, fd_fire );

    // Maybe TODO: - refueling in the rain could use more fuel
    // First, simulate expected burn per turn, to see if we need more fuel
    map_stack fuel_on_fire = here.i_at( *best_fire );
    for( item *&it : fuel_on_fire ) {
        it->simulate_burn( fd );
        // Unconstrained fires grow below -50_minutes age
        if( !contained && fire_age < -40_minutes && fd.fuel_produced > 1.0f && !it->made_of( LIQUID ) ) {
            // Too much - we don't want a firestorm!
            // Move item back to refueling pile
            // Note: move_item() handles messages (they're the generic "you drop x")
            move_item( p, *it, 0, *best_fire, *refuel_spot );
            return;
        }
    }

    // Enough to sustain the fire
    // TODO: It's not enough in the rain
    if( !starting_fire && ( fd.fuel_produced >= 1.0f || fire_age < 10_minutes ) ) {
        return;
    }

    // We need to move fuel from stash to fire
    map_stack potential_fuel = here.i_at( *refuel_spot );
    item *found = nullptr;
    item *found_tinder = nullptr;
    for( item *&it : potential_fuel ) {
        if( it->made_of( LIQUID ) ) {
            continue;
        }
        // If we specifically need tinder to start this fire, grab it the instant it's found and ignore any other fuel
        if( starting_fire ) {
            // Only track firestarter if we have an activity assigned to light a new fire, or it will implode.
            item &firestarter = *act.tools.front();
            if( firestarter.has_flag( flag_REQUIRES_TINDER ) ) {
                if( it->has_flag( flag_TINDER ) ) {
                    move_item( p, *it, 1, *refuel_spot, *best_fire );
                    return;
                } else {
                    continue;
                }
            }
        } else {
            // Keep track of any tinder we don't need, but don't use it if we have other options
            if( it->has_flag( flag_TINDER ) ) {
                found_tinder = it;
                continue;
            }
        }

        float last_fuel = fd.fuel_produced;
        it->simulate_burn( fd );
        if( fd.fuel_produced > last_fuel ) {
            found = it;
            break;
        }
    }
    // Only use tinder if we didn't find any other valid fuel to use
    if( found ) {
        const int quantity = std::max( 1, std::min( found->charges, found->charges_per_volume( 250_ml ) ) );
        // Note: move_item() handles messages (they're the generic "you drop x")
        move_item( p, *found, quantity, *refuel_spot, *best_fire );
    } else if( found_tinder ) {
        const int quantity = std::max( 1, std::min( found_tinder->charges,
                                       found_tinder->charges_per_volume( 250_ml ) ) );
        move_item( p, *found_tinder, quantity, *refuel_spot, *best_fire );
    }
}
