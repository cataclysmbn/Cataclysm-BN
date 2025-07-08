#include "inventory.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>

#include "avatar.h"
#include "debug.h"
#include "diary.h"
#include "distribution_grid.h"
#include "game.h"
#include "iexamine.h"
#include "locations.h"
#include "magic_enchantment.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "messages.h" //for rust message
#include "npc.h"
#include "options.h"
#include "translations.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "calendar.h"
#include "character.h"
#include "damage.h"
#include "enums.h"
#include "flag.h"
#include "player.h"
#include "rng.h"
#include "material.h"
#include "type_id.h"
#include "flat_set.h"
#include "point.h"
#include "inventory_ui.h" // auto inventory blocking

static const itype_id itype_aspirin( "aspirin" );
static const itype_id itype_battery( "battery" );
static const itype_id itype_codeine( "codeine" );
static const itype_id itype_heroin( "heroin" );
static const itype_id itype_salt_water( "salt_water" );
static const itype_id itype_tramadol( "tramadol" );
static const itype_id itype_oxycodone( "oxycodone" );
static const itype_id itype_water( "water" );

struct itype;

const invlet_wrapper
inv_chars( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#&()+.:;=@[\\]^_{|}" );

bool invlet_wrapper::valid( const int invlet ) const
{
    if( invlet > std::numeric_limits<char>::max() || invlet < std::numeric_limits<char>::min() ) {
        return false;
    }
    return find( static_cast<char>( invlet ) ) != std::string::npos;
}

invlet_favorites::invlet_favorites( const std::unordered_map<itype_id, std::string> &map )
{
    for( const auto &p : map ) {
        if( p.second.empty() ) {
            // The map gradually accumulates empty lists; remove those here
            continue;
        }
        invlets_by_id.insert( p );
        for( char invlet : p.second ) {
            uint8_t invlet_u = invlet;
            if( !ids_by_invlet[invlet_u].is_empty() ) {
                debugmsg( "Duplicate invlet: %s and %s both mapped to %c",
                          ids_by_invlet[invlet_u].str(), p.first.str(), invlet );
            }
            ids_by_invlet[invlet_u] = p.first;
        }
    }
}

void invlet_favorites::set( char invlet, const itype_id &id )
{
    if( contains( invlet, id ) ) {
        return;
    }
    erase( invlet );
    uint8_t invlet_u = invlet;
    ids_by_invlet[invlet_u] = id;
    invlets_by_id[id].push_back( invlet );
}

void invlet_favorites::erase( char invlet )
{
    uint8_t invlet_u = invlet;
    const itype_id &id = ids_by_invlet[invlet_u];
    if( id.is_empty() ) {
        return;
    }
    std::string &invlets = invlets_by_id[id];
    std::string::iterator it = std::ranges::find( invlets, invlet );
    invlets.erase( it );
    ids_by_invlet[invlet_u] = itype_id();
}

bool invlet_favorites::contains( char invlet, const itype_id &id ) const
{
    uint8_t invlet_u = invlet;
    return ids_by_invlet[invlet_u] == id;
}

std::string invlet_favorites::invlets_for( const itype_id &id ) const
{
    auto map_iterator = invlets_by_id.find( id );
    if( map_iterator == invlets_by_id.end() ) {
        return {};
    }
    return map_iterator->second;
}

const std::unordered_map<itype_id, std::string> &
invlet_favorites::get_invlets_by_id() const
{
    return invlets_by_id;
}

inventory::inventory() = default;

const_invslice inventory::const_slice() const
{
    const_invslice stacks;
    for( const auto &item : items ) {
        stacks.push_back( &item );
    }
    return stacks;
}

const std::vector<item *> &inventory::const_stack( int i ) const
{
    if( i < 0 || i >= static_cast<int>( items.size() ) ) {
        debugmsg( "Attempted to access stack %d in an inventory (size %d)", i, items.size() );
        static const std::vector<item *> nullstack{};
        return nullstack;
    }

    invstack::const_iterator iter = items.begin();
    for( int j = 0; j < i; ++j ) {
        ++iter;
    }
    return *iter;
}

size_t inventory::size() const
{
    return items.size();
}

void inventory::unsort()
{
    binned = false;
    items_type_cached = false;
}

static bool stack_compare( const std::vector<item *> &lhs, const std::vector<item *> &rhs )
{
    return *lhs.front() < *rhs.front();
}

void inventory::clear()
{
    items.clear();
    binned = false;
    items_type_cached = false;
}

inventory &inventory::add_items( const inventory &rhs, bool keep_invlet, bool assign_invlet,
                                 bool should_stack )
{
    for( size_t i = 0; i < rhs.size(); i++ ) {
        add_items( rhs.const_stack( i ), keep_invlet, assign_invlet, should_stack );
    }
    return *this;
}

inventory &inventory::add_items( const item_stack &rhs, bool keep_invlet, bool assign_invlet,
                                 bool should_stack )
{
    for( const auto &it : rhs ) {
        if( !it->made_of( LIQUID ) ) {
            add_item( *it, keep_invlet, assign_invlet, should_stack );
        }
    }
    return *this;
}

inventory &inventory::add_items( const std::vector<item *> &rhs, bool keep_invlet,
                                 bool assign_invlet, bool should_stack )
{
    for( const auto &it : rhs ) {
        add_item( *it, keep_invlet, assign_invlet, should_stack );
    }
    return *this;
}

inventory &inventory::add_items( const location_inventory &rhs, bool keep_invlet,
                                 bool assign_invlet, bool should_stack )
{
    for( size_t i = 0; i < rhs.size(); i++ ) {
        add_items( rhs.const_stack( i ), keep_invlet, assign_invlet, should_stack );
    }
    return *this;
}

inventory &inventory::add_items( const location_vector<item> &rhs, bool keep_invlet,
                                 bool assign_invlet, bool should_stack )
{
    for( item * const &it : rhs ) {
        add_item( *it, keep_invlet, assign_invlet, should_stack );
    }
    return *this;
}

// This function keeps the invlet cache updated when a new item is added.
void inventory::update_invlet_cache_with_item( item &newit )
{
    // This function does two things:
    // 1. It adds newit's invlet to the list of favorite letters for newit's item type.
    // 2. It removes newit's invlet from the list of favorite letters for all other item types.

    // no invlet item, just return.
    // TODO: Should we instead remember that the invlet was cleared?
    if( newit.invlet == 0 ) {
        return;
    }

    invlet_cache.set( newit.invlet, newit.typeId() );
}

char inventory::find_usable_cached_invlet( const itype_id &item_type )
{
    // Some of our preferred letters might already be used.
    for( auto invlet : invlet_cache.invlets_for( item_type ) ) {
        // Don't overwrite user assignments.
        if( assigned_invlet.contains( invlet ) ) {
            continue;
        }
        // Check if anything is using this invlet.
        if( g->u.invlet_to_item( invlet ) != nullptr ) {
            continue;
        }
        return invlet;
    }

    return 0;
}

void inventory::build_items_type_cache()
{
    items_type_cache.clear();
    for( auto &elem : items ) {
        itype_id type = elem.front()->typeId();
        items_type_cache[type].push_back( &elem );
    }
    items_type_cached = true;
}

namespace
{
inline static bool add_item_stack_helper( inventory &inv, item &newit, std::vector<item *> &elem,
        bool keep_invlet, bool assign_invlet, item *&existing )
{
    item *&it = *elem.begin();
    if( it->stacks_with( newit ) ) {
        if( it->invlet == '\0' ) {
            if( !keep_invlet ) {
                inv.update_invlet( newit, assign_invlet );
            }
            inv.update_invlet_cache_with_item( newit );
            it->invlet = newit.invlet;
        } else {
            newit.invlet = it->invlet;
        }
        elem.push_back( &newit );
        existing = elem.back();
        return true;
    } else if( keep_invlet && assign_invlet && it->invlet == newit.invlet &&
               it->invlet != '\0' ) {
        // If keep_invlet is true, we'll be forcing other items out of their current invlet.
        inv.assign_empty_invlet( *it, g->u );
    }
    return false;
}
}

template<bool IsCached>
item &inventory::add_item_internal( item &newit, bool keep_invlet, bool assign_invlet,
                                    bool should_stack )
{
    binned = false;

    itype_id type = newit.typeId();
    if constexpr( IsCached ) {
        if( !items_type_cached ) {
            debugmsg( "Tried to add item to inventory using cache without building the items_type_cache." );
            build_items_type_cache();
        }
        if( should_stack ) {
            // See if we can't stack this item.
            for( auto &elem : items_type_cache[type] ) {
                item *ex{};
                if( ::add_item_stack_helper( *this, newit, *elem, keep_invlet, assign_invlet, ex ) ) {
                    return *ex;
                }
            }
        }
    } else {
        items_type_cached = false;
        if( should_stack ) {
            // See if we can't stack this item.
            for( auto &elem : items ) {
                item *ex{};
                if( ::add_item_stack_helper( *this, newit, elem, keep_invlet, assign_invlet, ex ) ) {
                    return *ex;
                }
            }
        }
    }

    // Couldn't stack the item, proceed.
    if( !keep_invlet ) {
        update_invlet( newit, assign_invlet );
    }
    update_invlet_cache_with_item( newit );

    items.push_back( {&newit} );

    if constexpr( IsCached ) {
        items_type_cache[type].push_back( &items.back() );
    }

    return *items.back().back();
}

item &inventory::add_item( item &newit, bool keep_invlet, bool assign_invlet, bool should_stack )
{
    return add_item_internal<false>( newit, keep_invlet, assign_invlet, should_stack );
}

item &inventory::add_item_by_items_type_cache( item &newit, bool keep_invlet, bool assign_invlet,
        bool should_stack )
{
    return add_item_internal<true>( newit, keep_invlet, assign_invlet, should_stack );
}

#if defined(__ANDROID__)
extern void remove_stale_inventory_quick_shortcuts();
#endif

void inventory::restack( player &p )
{
    // tasks that the old restack seemed to do:
    // 1. reassign inventory letters
    // 2. remove items from non-matching stacks
    // 3. combine matching stacks

    binned = false;
    items_type_cached = false;
    std::vector<item *> to_restack;
    int idx = 0;
    for( invstack::iterator iter = items.begin(); iter != items.end(); ++iter, ++idx ) {
        std::vector<item *> &stack = *iter;
        // Sort the inner stack itself, so the most recently used item is at front and keeps the invlet
        std::ranges::sort( stack, []( auto * lhs, auto * rhs ) {
            return *lhs < *rhs;
        } );
        item &topmost = *stack.front();

        const item *invlet_item = p.invlet_to_item( topmost.invlet );
        if( !inv_chars.valid( topmost.invlet ) || ( invlet_item != nullptr &&
                position_by_item( invlet_item ) != idx ) ) {
            assign_empty_invlet( topmost, p );
            for( auto &stack_iter : stack ) {
                stack_iter->invlet = topmost.invlet;
            }
        }
        // remove non-matching items, stripping off end of stack so the first item keeps the invlet.
        while( stack.size() > 1 && !topmost.stacks_with( *stack.back() ) ) {
            to_restack.push_back( iter->back() );
            iter->pop_back();
        }
    }

    // combine matching stacks
    // separate loop to ensure that ALL stacks are homogeneous
    for( invstack::iterator iter = items.begin(); iter != items.end(); ++iter ) {
        for( invstack::iterator other = iter; other != items.end(); ++other ) {
            if( iter != other && iter->front()->stacks_with( *other->front() ) ) {
                if( other->front()->count_by_charges() ) {
                    iter->front()->charges += other->front()->charges;
                } else {
                    for( auto &elem : *other ) {
                        iter->push_back( elem );
                    }
                }
                other = items.erase( other );
                --other;
            }
        }
    }

    //re-add non-matching items
    for( auto &elem : to_restack ) {
        add_item( *elem, false );
    }

    //Ensure that all items in the same stack have the same invlet.
    for( std::vector< item * > &outer : items ) {
        for( item *&inner : outer ) {
            inner->invlet = outer.front()->invlet;
        }
    }
    items.sort( stack_compare );

#if defined(__ANDROID__)
    remove_stale_inventory_quick_shortcuts();
#endif
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

void inventory::form_from_map( const tripoint &origin, int range, const Character *pl,
                               bool assign_invlet,
                               bool clear_path )
{
    form_from_map( g->m, origin, range, pl, assign_invlet, clear_path );
}

void inventory::form_from_zone( map &m, std::unordered_set<tripoint> &zone_pts, const Character *pl,
                                bool assign_invlet )
{
    std::vector<tripoint> pts;
    pts.reserve( zone_pts.size() );
    for( const tripoint &elem : zone_pts ) {
        pts.push_back( m.getlocal( elem ) );
    }
    form_from_map( m, pts, pl, assign_invlet );
}

void inventory::form_from_map( map &m, const tripoint &origin, int range, const Character *pl,
                               bool assign_invlet,
                               bool clear_path )
{
    // populate a grid of spots that can be reached
    std::vector<tripoint> reachable_pts = {};
    // If we need a clear path we care about the reachability of points
    if( clear_path ) {
        m.reachable_flood_steps( reachable_pts, origin, range, 1, 100 );
    } else {
        // Fill reachable points with points_in_radius
        tripoint_range<tripoint> in_radius = m.points_in_radius( origin, range );
        for( const tripoint &p : in_radius ) {
            reachable_pts.emplace_back( p );
        }
    }
    form_from_map( m, reachable_pts, pl, assign_invlet );
}

//TODO!: check that not stacking the crafting inventory works ok
void inventory::form_from_map( map &m, std::vector<tripoint> pts, const Character *pl,
                               bool assign_invlet )
{
    const time_point bday = calendar::start_of_cataclysm;
    std::unordered_map<const vehicle *, std::unordered_set<const vpart_reference *>> checked_vehi;
    items.clear();
    build_items_type_cache();
    for( const tripoint &p : pts ) {
        if( m.has_furn( p ) ) {
            const furn_t &f = m.furn( p ).obj();
            const std::vector<itype> tool_list = f.crafting_pseudo_item_types();
            if( !tool_list.empty() ) {
                for( const itype &type : tool_list ) {
                    item &furn_item = *item::spawn_temporary( type.get_id(), calendar::turn, 0 );
                    furn_item.set_flag( flag_PSEUDO );
                    const itype_id &ammo = furn_item.ammo_default();
                    if( furn_item.has_flag( flag_USES_GRID_POWER ) ) {
                        // TODO: The grid tracker should correspond to map!
                        auto &grid = get_distribution_grid_tracker().grid_at( tripoint_abs_ms( m.getabs( p ) ) );
                        furn_item.charges = grid.get_resource();
                    } else {
                        furn_item.charges = ammo ? count_charges_in_list( &*ammo, m.i_at( p ) ) : 0;
                    }
                    add_item_by_items_type_cache( furn_item, false, true, false );
                }
            }
        }
        if( m.has_items( p ) && m.accessible_items( p ) ) {
            bool allow_liquids = m.has_flag_ter_or_furn( "LIQUIDCONT", p );
            for( auto &i : m.i_at( p ) ) {
                // if it's *the* player requesting this from from map inventory
                // then don't allow items owned by another faction to be factored into recipe components etc.
                if( pl && !i->is_owned_by( *pl, true ) && i->get_owner()->likes_u >= -10 ) {
                    continue;
                }
                if( allow_liquids || !i->made_of( LIQUID ) ) {
                    add_item_by_items_type_cache( *i, false, assign_invlet, false );
                }
            }
        }
        // Kludges for now!
        if( m.has_nearby_fire( p, 0 ) ) {
            item &fire = *item::spawn_temporary( "fire", bday );
            fire.charges = 1;
            add_item_by_items_type_cache( fire, false, true, false );
        }
        // Handle any water from infinite map sources.
        detached_ptr<item> water = m.water_from( p );
        if( water ) {
            add_item_by_items_type_cache( *water, false, true, false );
        }
        // kludge that can probably be done better to check specifically for toilet water to use in
        // crafting
        if( m.furn( p ).obj().examine == &iexamine::toilet ) {
            // get water charges at location
            auto toilet = m.i_at( p );
            item *waterp = nullptr;
            for( auto candidate = toilet.begin(); candidate != toilet.end(); ++candidate ) {
                if( ( *candidate )->typeId() == itype_water ) {
                    waterp = *candidate;
                    break;
                }
            }
            if( waterp != nullptr && waterp->charges > 0 ) {
                add_item_by_items_type_cache( *waterp, false, true, false );
            }
        }

        // WARNING: The part below has a bug that's currently quite minor
        // When a vehicle has multiple faucets in range, available water is
        //  multiplied by the number of faucets.
        // Same thing happens for all other tools and resources, but not cargo
        const optional_vpart_position vp = m.veh_at( p );
        if( !vp ) {
            continue;
        }
        vehicle *const veh = &vp->vehicle();
        if( !checked_vehi.contains( veh ) ) {
            // We haven't worked with this vehicle yet.
            checked_vehi[veh] = std::unordered_set<const vpart_reference *>();
        }
        // Make sure we're ready to record
        std::unordered_set<const vpart_reference *> &found_parts = checked_vehi[veh];

        //Adds faucet to kitchen stuff; may be horribly wrong to do such....
        //ShouldBreak into own variable
        const std::optional<vpart_reference> kpart = vp.part_with_feature( "KITCHEN", true );
        const std::optional<vpart_reference> butcherpart = vp.part_with_feature( "BUTCHER_EQ", true );
        const std::optional<vpart_reference> faupart = vp.part_with_feature( "FAUCET", true );
        const std::optional<vpart_reference> weldpart = vp.part_with_feature( "WELDRIG", true );
        const std::optional<vpart_reference> craftpart = vp.part_with_feature( "CRAFTRIG", true );
        const std::optional<vpart_reference> forgepart = vp.part_with_feature( "FORGE", true );
        const std::optional<vpart_reference> kilnpart = vp.part_with_feature( "KILN", true );
        const std::optional<vpart_reference> chempart = vp.part_with_feature( "CHEMLAB", true );
        const std::optional<vpart_reference> autoclavepart = vp.part_with_feature( "AUTOCLAVE", true );
        const std::optional<vpart_reference> cargo = vp.part_with_feature( "CARGO", true );

        if( cargo ) {
            const auto items = veh->get_items( cargo->part_index() );
            for( const auto &it : items ) {
                add_item_by_items_type_cache( *it, false, false, false );
            }
        }

        if( faupart && !found_parts.contains( &*faupart ) ) {
            for( const auto &it : veh->fuels_left() ) {
                item &fuel = *item::spawn_temporary( it.first, bday );
                if( fuel.made_of( LIQUID ) ) {
                    fuel.charges = it.second;
                    add_item_by_items_type_cache( fuel, false, true, false );
                }
            }
            found_parts.insert( &*faupart );
        }

        static const flag_id flag_PSEUDO( "PSEUDO" );
        static const flag_id flag_HEATS_FOOD( "HEATS_FOOD" );
        static const flag_id flag_FLATSURF( "FLAT_SURFACE" );

        if( kpart && !found_parts.contains( &*kpart ) ) {
            item &hotplate = *item::spawn_temporary( "hotplate", bday );
            hotplate.charges = veh->fuel_left( itype_battery, true );
            hotplate.item_tags.insert( flag_PSEUDO );
            // TODO: Allow disabling
            hotplate.item_tags.insert( flag_HEATS_FOOD );
            add_item_by_items_type_cache( hotplate, false );

            item &pot = *item::spawn_temporary( "pot", bday );
            pot.set_flag( flag_PSEUDO );
            add_item_by_items_type_cache( pot, false );
            item &pan = *item::spawn_temporary( "pan", bday );
            pan.set_flag( flag_PSEUDO );
            add_item_by_items_type_cache( pan, false );
            found_parts.insert( &*kpart );
        }
        if( butcherpart &&
            !found_parts.contains( &*butcherpart ) ) { //copy n paste code moment(this will go wrong)
            item &butchery = *item::spawn_temporary( "fake_adv_butchery", bday );
            butchery.charges = veh->fuel_left( itype_battery, true );
            butchery.item_tags.insert( flag_PSEUDO );
            //A very hacky way to make game take vehicle part into the account for flat surface
            butchery.item_tags.insert( flag_FLATSURF );
            add_item_by_items_type_cache( butchery, false );
            found_parts.insert( &*butcherpart );
        }
        if( weldpart && !found_parts.contains( &*weldpart ) ) {
            item &welder = *item::spawn_temporary( "welder", bday );
            welder.charges = veh->fuel_left( itype_battery, true );
            welder.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( welder, false );

            item &soldering_iron = *item::spawn_temporary( "soldering_iron", bday );
            soldering_iron.charges = veh->fuel_left( itype_battery, true );
            soldering_iron.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( soldering_iron, false );
            found_parts.insert( &*weldpart );
        }
        if( craftpart && !found_parts.contains( &*craftpart ) ) {
            item &vac_sealer = *item::spawn_temporary( "vac_sealer", bday );
            vac_sealer.charges = veh->fuel_left( itype_battery, true );
            vac_sealer.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( vac_sealer, false );

            item &dehydrator = *item::spawn_temporary( "dehydrator", bday );
            dehydrator.charges = veh->fuel_left( itype_battery, true );
            dehydrator.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( dehydrator, false );

            item &food_processor = *item::spawn_temporary( "food_processor", bday );
            food_processor.charges = veh->fuel_left( itype_battery, true );
            food_processor.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( food_processor, false );

            item &press = *item::spawn_temporary( "press", bday );
            press.charges = veh->fuel_left( itype_battery, true );
            press.set_flag( flag_PSEUDO );
            add_item_by_items_type_cache( press, false );
            found_parts.insert( &*craftpart );
        }
        if( forgepart && !found_parts.contains( &*forgepart ) ) {
            item &forge = *item::spawn_temporary( "forge", bday );
            forge.charges = veh->fuel_left( itype_battery, true );
            forge.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( forge, false );
            found_parts.insert( &*forgepart );
        }
        if( kilnpart && !found_parts.contains( &*kilnpart ) ) {
            item &kiln = *item::spawn_temporary( "kiln", bday );
            kiln.charges = veh->fuel_left( itype_battery, true );
            kiln.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( kiln, false );
            found_parts.insert( &*kilnpart );
        }
        if( chempart && !found_parts.contains( &*chempart ) ) {
            item &chemistry_set = *item::spawn_temporary( "chemistry_set", bday );
            chemistry_set.charges = veh->fuel_left( itype_battery, true );
            chemistry_set.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( chemistry_set, false );

            item &electrolysis_kit = *item::spawn_temporary( "electrolysis_kit", bday );
            electrolysis_kit.charges = veh->fuel_left( itype_battery, true );
            electrolysis_kit.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( electrolysis_kit, false );
            found_parts.insert( &*chempart );
        }
        if( autoclavepart && !found_parts.contains( &*autoclavepart ) ) {
            item &autoclave = *item::spawn_temporary( "autoclave", bday );
            autoclave.charges = veh->fuel_left( itype_battery, true );
            autoclave.item_tags.insert( flag_PSEUDO );
            add_item_by_items_type_cache( autoclave, false );
            found_parts.insert( &*autoclavepart );
        }
    }
    pts.clear();
}

std::vector<detached_ptr<item>> location_inventory::reduce_stack( const int position,
                             const int quantity )
{
    int pos = 0;
    std::vector<detached_ptr<item>> ret;
    for( invstack::iterator iter = inv.items.begin(); iter != inv.items.end(); ++iter ) {
        if( position == pos ) {
            inv.binned = false;
            inv.items_type_cached = false;
            if( quantity >= static_cast<int>( iter->size() ) || quantity < 0 ) {
                std::vector<item *> stack = *iter;
                inv.items.erase( iter );
                for( item *&it : stack ) {
                    it->remove_location();
                    ret.push_back( detached_ptr<item>( it ) );
                }
            } else {
                for( int i = 0 ; i < quantity ; i++ ) {
                    item *it = &inv.remove_item( iter->front() );
                    it->remove_location();
                    ret.push_back( detached_ptr<item>( it ) );
                }
            }
            break;
        }
        ++pos;
    }
    return ret;
}

item &inventory::remove_item( const item *it )
{
    auto tmp = remove_items_with( [&it]( const item & i ) {
        return &i == it;
    }, 1 );
    if( !tmp.empty() ) {
        binned = false;
        items_type_cached = false;
        return *tmp.front();
    }
    debugmsg( "Tried to remove a item not in inventory." );
    return null_item_reference();
}

item &inventory::remove_item( const int position )
{
    int pos = 0;
    for( invstack::iterator iter = items.begin(); iter != items.end(); ++iter ) {
        if( position == pos ) {
            binned = false;
            items_type_cached = false;
            if( iter->size() > 1 ) {
                std::vector<item *>::iterator stack_member = iter->begin();
                char invlet = ( *stack_member )->invlet;
                ++stack_member;
                ( *stack_member )->invlet = invlet;
            }
            item &ret = *iter->front();
            iter->erase( iter->begin() );
            if( iter->empty() ) {
                items.erase( iter );
            }
            return ret;
        }
        ++pos;
    }

    return null_item_reference();
}

std::vector<detached_ptr<item>> location_inventory::remove_randomly_by_volume(
                                 const units::volume &volume )
{
    std::vector<detached_ptr<item>> result;
    units::volume volume_dropped = 0_ml;
    while( volume_dropped < volume ) {
        units::volume cumulative_volume = 0_ml;
        auto chosen_stack = inv.items.begin();
        auto chosen_item = chosen_stack->begin();
        for( auto stack = inv.items.begin(); stack != inv.items.end(); ++stack ) {
            for( auto stack_it = stack->begin(); stack_it != stack->end(); ++stack_it ) {
                cumulative_volume += ( *stack_it )->volume();
                if( x_in_y( ( *stack_it )->volume().value(), cumulative_volume.value() ) ) {
                    chosen_item = stack_it;
                    chosen_stack = stack;
                }
            }
        }
        volume_dropped += ( *chosen_item )->volume();
        //TODO!: check
        item *chosen = *chosen_item;
        chosen->remove_location();
        chosen_item = chosen_stack->erase( chosen_item );

        result.push_back( detached_ptr<item>( chosen ) );
        if( chosen_item == chosen_stack->begin() && !chosen_stack->empty() ) {
            // preserve the invlet when removing the first item of a stack
            ( *chosen_item )->invlet = result.back()->invlet;
        }
        if( chosen_stack->empty() ) {
            inv.binned = false;
            inv.items_type_cached = false;
            inv.items.erase( chosen_stack );
        }
    }
    return result;
}

void inventory::dump( std::vector<item *> &dest )
{
    for( auto &elem : items ) {
        for( auto &elem_stack_iter : elem ) {
            dest.push_back( elem_stack_iter );
        }
    }
}

void location_inventory::dump_remove( std::vector<detached_ptr<item>> &dest )
{

    for( auto &elem : inv.items ) {
        for( auto &elem_stack_iter : elem ) {
            elem_stack_iter->remove_location();
            dest.push_back( detached_ptr<item>( elem_stack_iter ) );
        }
    }
    inv.clear();
}

std::vector<detached_ptr<item>> location_inventory::dump_remove( )
{
    std::vector<detached_ptr<item>> dest;
    for( auto &elem : inv.items ) {
        for( auto &elem_stack_iter : elem ) {
            elem_stack_iter->remove_location();
            dest.push_back( detached_ptr<item>( elem_stack_iter ) );
        }
    }
    inv.clear();
    return dest;
}

const item &inventory::find_item( int position ) const
{
    if( position < 0 || position >= static_cast<int>( items.size() ) ) {
        return null_item_reference();
    }
    invstack::const_iterator iter = items.begin();
    for( int j = 0; j < position; ++j ) {
        ++iter;
    }
    return *iter->front();
}

item &inventory::find_item( int position )
{
    return const_cast<item &>( const_cast<const inventory *>( this )->find_item( position ) );
}

int inventory::invlet_to_position( char invlet ) const
{
    int i = 0;
    for( const auto &elem : items ) {
        if( ( *elem.begin() )->invlet == invlet ) {
            return i;
        }
        ++i;
    }
    return INT_MIN;
}

int inventory::position_by_item( const item *it ) const
{
    int p = 0;
    for( const auto &stack : items ) {
        for( const auto &e : stack ) {
            if( e->has_item( *it ) ) {
                return p;
            }
        }
        p++;
    }
    return INT_MIN;
}

int inventory::position_by_type( const itype_id &type ) const
{
    int i = 0;
    for( auto &elem : items ) {
        if( elem.front()->typeId() == type ) {
            return i;
        }
        ++i;
    }
    return INT_MIN;
}

std::vector<detached_ptr<item>> location_inventory::use_amount( itype_id it, int quantity,
                             const std::function<bool( const item & )> &filter )
{
    inv.items.sort( stack_compare );
    std::vector<detached_ptr<item>> ret;

    remove_items_with( [&]( detached_ptr<item> &&a ) {
        a = item::use_amount( std::move( a ), it, quantity, ret, filter );
        return quantity > 0 ? VisitResponse::SKIP : VisitResponse::ABORT;
    } );

    inv.binned = false;
    inv.items_type_cached = false;
    return ret;
}

bool inventory::has_tools( const itype_id &it, int quantity,
                           const std::function<bool( const item & )> &filter ) const
{
    return has_amount( it, quantity, true, filter );
}

bool inventory::has_components( const itype_id &it, int quantity,
                                const std::function<bool( const item & )> &filter ) const
{
    return has_amount( it, quantity, false, filter );
}

bool inventory::has_charges( const itype_id &it, int quantity,
                             const std::function<bool( const item & )> &filter ) const
{
    return ( charges_of( it, INT_MAX, filter ) >= quantity );
}

int inventory::leak_level( const flag_id &flag ) const
{
    int ret = 0;

    for( const auto &elem : items ) {
        for( const auto &elem_stack_iter : elem ) {
            if( elem_stack_iter->has_flag( flag ) ) {
                if( elem_stack_iter->has_flag( flag_LEAK_ALWAYS ) ) {
                    ret += elem_stack_iter->volume() / units::legacy_volume_factor;
                } else if( elem_stack_iter->has_flag( flag_LEAK_DAM ) && elem_stack_iter->damage() > 0 ) {
                    ret += elem_stack_iter->damage_level( 4 );
                }
            }
        }
    }
    return ret;
}

int inventory::worst_item_value( npc *p ) const
{
    int worst = 99999;
    for( const auto &elem : items ) {
        const item &it = *elem.front();
        int val = p->value( it );
        if( val < worst ) {
            worst = val;
        }
    }
    return worst;
}

bool inventory::has_enough_painkiller( int pain ) const
{
    for( const auto &elem : items ) {
        const item &it = *elem.front();
        if( ( pain <= 35 && it.typeId() == itype_aspirin ) ||
            ( pain >= 50 && it.typeId() == itype_oxycodone ) ||
            it.typeId() == itype_tramadol || it.typeId() == itype_codeine ) {
            return true;
        }
    }
    return false;
}

item *inventory::most_appropriate_painkiller( int pain )
{
    int difference = 9999;
    item *ret = &null_item_reference();
    for( auto &elem : items ) {
        int diff = 9999;
        itype_id type = elem.front()->typeId();
        if( type == itype_aspirin ) {
            diff = std::abs( pain - 15 );
        } else if( type == itype_codeine ) {
            diff = std::abs( pain - 30 );
        } else if( type == itype_oxycodone ) {
            diff = std::abs( pain - 60 );
        } else if( type == itype_heroin ) {
            diff = std::abs( pain - 100 );
        } else if( type == itype_tramadol ) {
            diff = std::abs( pain - 40 ) / 2; // Bonus since it's long-acting
        }

        if( diff < difference ) {
            difference = diff;
            ret = elem.front();
        }
    }
    return ret;
}

void inventory::rust_iron_items()
{
    for( auto &elem : items ) {
        for( auto &elem_stack_iter : elem ) {
            if( elem_stack_iter->made_of( material_id( "iron" ) ) &&
                !elem_stack_iter->has_flag( flag_WATERPROOF_GUN ) &&
                !elem_stack_iter->has_flag( flag_WATERPROOF ) &&
                elem_stack_iter->damage() < elem_stack_iter->max_damage() / 2 &&
                //Passivation layer prevents further rusting
                one_in( 500 ) &&
                //Scale with volume, bigger = slower (see #24204)
                one_in(
                    static_cast<int>(
                        14 * std::cbrt(
                            0.5 * std::max(
                                0.05, static_cast<double>(
                                    elem_stack_iter->base_volume().value() ) / 250 ) ) ) ) &&
                //                       ^season length   ^14/5*0.75/pi (from volume of sphere)
                //Freshwater without oxygen rusts slower than air
                g->m.water_from( g->u.pos() )->typeId() == itype_salt_water ) {
                elem_stack_iter->inc_damage( DT_ACID ); // rusting never completely destroys an item
                add_msg( m_bad, _( "Your %s is damaged by rust." ), elem_stack_iter->tname() );
            }
        }
    }
}

units::mass inventory::weight() const
{
    units::mass ret = 0_gram;
    for( const auto &elem : items ) {
        for( const auto &elem_stack_iter : elem ) {
            ret += elem_stack_iter->weight();
        }
    }
    return ret;
}

// Helper function to iterate over the intersection of the inventory and a list
// of items given
template<typename F>
void for_each_item_in_both(
    const invstack &items, const std::map<item *, int> &other, const F &f )
{
    // Shortcut the logic in the common case where other is empty
    if( other.empty() ) {
        return;
    }

    for( const auto &elem : items ) {
        item &representative = *elem.front();
        auto other_it = other.find( &representative );
        if( other_it == other.end() ) {
            continue;
        }

        int num_to_count = other_it->second;
        if( representative.count_by_charges() ) {
            //TODO!: what the shit is this used for
            item &copy = *item::spawn_temporary( representative );
            copy.charges = std::min( copy.charges, num_to_count );
            f( copy );
        } else {
            for( const auto &elem_stack_iter : elem ) {
                f( *elem_stack_iter );
                if( --num_to_count <= 0 ) {
                    break;
                }
            }
        }
    }
}

units::mass inventory::weight_without( const excluded_stacks &without ) const
{
    units::mass ret = weight();

    for_each_item_in_both( items, without,
    [&]( const item & i ) {
        ret -= i.weight();
    }
                         );

    if( ret < 0_gram ) {
        debugmsg( "Negative mass after removing some of inventory" );
        ret = {};
    }

    return ret;
}

units::volume inventory::volume() const
{
    units::volume ret = 0_ml;
    for( const auto &elem : items ) {
        for( const auto &elem_stack_iter : elem ) {
            ret += elem_stack_iter->volume();
        }
    }
    return ret;
}

units::volume inventory::volume_without( const excluded_stacks &without ) const
{
    units::volume ret = volume();

    for_each_item_in_both( items, without,
    [&]( const item & i ) {
        ret -= i.volume();
    }
                         );

    if( ret < 0_ml ) {
        debugmsg( "Negative volume after removing some of inventory" );
        ret = 0_ml;
    }

    return ret;
}

std::vector<item *> inventory::active_items()
{
    std::vector<item *> ret;
    for( std::vector<item *> &elem : items ) {
        for( item * const &elem_stack_iter : elem ) {
            if( elem_stack_iter->needs_processing() ) {
                ret.push_back( elem_stack_iter );
            }
        }
    }
    return ret;
}

enchantment inventory::get_active_enchantment_cache( const Character &owner ) const
{
    enchantment temp_cache;
    for( const std::vector<item *> &elem : items ) {
        for( const item * const &check_item : elem ) {
            for( const enchantment &ench : check_item->get_enchantments() ) {
                if( ench.is_active( owner, *check_item ) ) {
                    temp_cache.force_add( ench );
                }
            }
        }
    }
    return temp_cache;
}

void inventory::update_quality_cache()
{
    quality_cache.clear();
    visit_items( [ this ]( const item * e ) {
        const std::map<quality_id, int> &item_qualities = e->get_qualities();
        for( const std::pair<const quality_id, int> &quality : item_qualities ) {
            const int item_count = e->count_by_charges() ? e->charges : 1;
            // quality.first is the id of the quality, quality.second is the quality level
            // the value is the number of items with that quality level
            quality_cache[quality.first][quality.second] += item_count;
        }
        return VisitResponse::NEXT;
    } );
}

const std::map<quality_id, std::map<int, int>> &inventory::get_quality_cache() const
{
    return quality_cache;
}

int inventory::count_item( const itype_id &item_type ) const
{
    int num = 0;
    const itype_bin bin = get_binned_items();
    if( bin.find( item_type ) == bin.end() ) {
        return num;
    }
    const std::list<const item *> items = get_binned_items().find( item_type )->second;
    for( const item *it : items ) {
        num += it->count();
    }
    return num;
}

void inventory::assign_empty_invlet( item &it, const Character &p, const bool force )
{
    const std::string auto_setting = get_option<std::string>( "AUTO_INV_ASSIGN" );
    if( auto_setting == "disabled" || ( ( auto_setting == "favorites" ) && !it.is_favorite ) ) {
        return;
    }

    invlets_bitset cur_inv = p.allocated_invlets();
    itype_id target_type = it.typeId();
    for( auto iter : assigned_invlet ) {
        if( iter.second == target_type && !cur_inv[iter.first] ) {
            it.invlet = iter.first;
            return;
        }
    }
    if( cur_inv.count() < inv_chars.size() ) {
        // XXX YUCK I don't know how else to get the keybindings
        // FIXME: Find a better way to get bound keys
        avatar &u = g->u;
        inventory_selector selector( u );

        std::vector<char> binds = selector.all_bound_keys();

        for( const auto &inv_char : inv_chars ) {
            if( assigned_invlet.contains( inv_char ) ) {
                // don't overwrite assigned keys
                continue;
            }
            if( std::ranges::find( binds, inv_char ) != binds.end() ) {
                // don't auto-assign bound keys
                continue;
            }
            if( !cur_inv[inv_char] ) {
                it.invlet = inv_char;
                return;
            }
        }
    }
    if( !force ) {
        it.invlet = 0;
        return;
    }
    // No free hotkey exist, re-use some of the existing ones
    for( auto &elem : items ) {
        item &o = *elem.front();
        if( o.invlet != 0 ) {
            it.invlet = o.invlet;
            o.invlet = 0;
            return;
        }
    }
    debugmsg( "could not find a hotkey for %s", it.tname() );
}

void inventory::reassign_item( item &it, char invlet, bool remove_old )
{
    if( it.invlet == invlet ) { // no change needed
        return;
    }
    if( remove_old && it.invlet ) {
        invlet_cache.erase( it.invlet );
    }

    // Remove invlet from other items beforehand, since restack may assign it to other items in the same stack.
    auto fn = [ = ]( item * ii ) {
        if( ii->invlet == invlet ) {
            ii->invlet = 0;
        }
        return VisitResponse::SKIP;
    };
    visit_items( fn );

    it.invlet = invlet;
    update_invlet_cache_with_item( it );
}

void inventory::update_invlet( item &newit, bool assign_invlet )
{
    // Avoid letters that have been manually assigned to other things.
    if( newit.invlet && assigned_invlet.find( newit.invlet ) != assigned_invlet.end() &&
        assigned_invlet[newit.invlet] != newit.typeId() ) {
        newit.invlet = '\0';
    }

    // Remove letters that are not in the favorites cache
    if( newit.invlet ) {
        if( !invlet_cache.contains( newit.invlet, newit.typeId() ) ) {
            newit.invlet = '\0';
        }
    }

    // Remove letters that have been assigned to other items in the inventory
    if( newit.invlet ) {
        char tmp_invlet = newit.invlet;
        newit.invlet = '\0';
        if( g->u.invlet_to_item( tmp_invlet ) == nullptr ) {
            newit.invlet = tmp_invlet;
        }
    }

    if( assign_invlet ) {
        // Assign a cached letter to the item
        if( !newit.invlet ) {
            newit.invlet = find_usable_cached_invlet( newit.typeId() );
        }

        // Give the item an invlet if it has none
        if( !newit.invlet ) {
            assign_empty_invlet( newit, g->u );
        }
    }
}

void inventory::set_stack_favorite( const int position, const bool favorite )
{
    for( auto &e : *std::next( items.begin(), position ) ) {
        e->set_favorite( favorite );
    }
}

invlets_bitset inventory::allocated_invlets() const
{
    invlets_bitset invlets;

    for( const auto &stack : items ) {
        const char invlet = stack.front()->invlet;
        invlets.set( invlet );
    }
    invlets[0] = false;
    return invlets;
}

const itype_bin &inventory::get_binned_items() const
{
    if( binned ) {
        return binned_items;
    }

    binned_items.clear();

    // HACK: Hack warning
    inventory *this_nonconst = const_cast<inventory *>( this );
    this_nonconst->visit_items( [ this ]( item * e ) {
        binned_items[ e->typeId() ].push_back( e );
        return VisitResponse::NEXT;
    } );

    binned = true;
    return binned_items;
}

const_invslice location_inventory::const_slice() const
{
    return inv.const_slice();
}

const std::vector<item *> &location_inventory::const_stack( int i ) const
{
    return inv.const_stack( i );
}

size_t location_inventory::size() const
{
    return inv.size();
}

location_inventory::~location_inventory()
{
    for( const auto &stack : inv.items ) {
        for( auto it : stack ) {
            it->destroy_in_place();
        }
    }
}

location_inventory::location_inventory( item_location *location ) : loc( location ) {}

location_inventory &location_inventory::operator=( location_inventory &&source )
noexcept
{
    for( auto &stack : source.inv.items ) {
        for( item * const &it : stack ) {
            it->remove_location();
            it->set_location( &*loc );
        }
    }
    inv = std::move( source.inv );
    return *this;
}

void location_inventory::unsort()
{
    inv.unsort();
}

void location_inventory::clear()
{
    for( const auto &stack : inv.items ) {
        for( auto it : stack ) {
            it->remove_location();
            it->destroy();
        }
    }
    return inv.clear();
}

void location_inventory::add_items( std::vector<detached_ptr<item>> &newits, bool keep_invlet,
                                    bool assign_invlet, bool should_stack )
{
    for( detached_ptr<item> &it : newits ) {
        add_item( std::move( it ), keep_invlet, assign_invlet, should_stack );
    }
}

template<bool IsCached>
item &location_inventory::add_item_internal( detached_ptr<item> &&newit, bool keep_invlet,
        bool assign_invlet, bool should_stack )
{
    if( !newit ) {
        return null_item_reference();
    }
    item *as_p = &*newit;
    if( &*loc == as_p->saved_loc ) {
        newit.release();
        as_p->saved_loc = nullptr;
        as_p->set_location( &*loc );
        return *as_p;
    }

    as_p->resolve_saved_loc();

    if( should_stack ) {
        for( auto &elem : inv.items ) {
            item *&it = *elem.begin();
            // NOLINTNEXTLINE(bugprone-use-after-move)
            if( it->stacks_with( *newit ) ) {
                // NOLINTNEXTLINE(bugprone-use-after-move)
                if( it->merge_charges( std::move( newit ) ) ) {
                    return *it;
                }
            }
        }
    }

    newit.release();
    as_p->set_location( &*loc );

    if constexpr( IsCached ) {
        return inv.add_item_by_items_type_cache( *as_p, keep_invlet, assign_invlet, should_stack );
    } else {
        return inv.add_item( *as_p, keep_invlet, assign_invlet, should_stack );
    }
}

item &location_inventory::add_item( detached_ptr<item> &&newit, bool keep_invlet,
                                    bool assign_invlet, bool should_stack )
{
    return add_item_internal<false>( std::move( newit ), keep_invlet, assign_invlet, should_stack );
}

item &location_inventory::add_item_by_items_type_cache( detached_ptr<item> &&newit,
        bool keep_invlet, bool assign_invlet, bool should_stack )
{
    return add_item_internal<true>( std::move( newit ), keep_invlet, assign_invlet, should_stack );
}

void location_inventory::restack( player &p )
{
    return inv.restack( p );
}
detached_ptr<item> location_inventory::remove_item( item *it )
{
    if( it ) {
        it->remove_location();
    }
    return detached_ptr<item>( &inv.remove_item( it ) );
}
detached_ptr<item> location_inventory::remove_item( int position )
{
    item *obj = &inv.remove_item( position );
    if( obj ) {
        obj->remove_location();
    }
    return detached_ptr<item>( obj );
}

const item &location_inventory::find_item( int position ) const
{
    return inv.find_item( position );
}
item &location_inventory::find_item( int position )
{
    return inv.find_item( position );
}

int location_inventory::position_by_item( const item *it ) const
{
    return inv.position_by_item( it );
}
int location_inventory::position_by_type( const itype_id &type ) const
{
    return inv.position_by_type( type );
}
int location_inventory::invlet_to_position( char invlet ) const
{
    return inv.invlet_to_position( invlet );
}

bool location_inventory::has_tools( const itype_id &it, int quantity,
                                    const std::function<bool( const item & )> &filter ) const
{
    return inv.has_tools( it, quantity, filter );
}
bool location_inventory::has_components( const itype_id &it, int quantity,
        const std::function<bool( const item & )> &filter ) const
{
    return inv.has_components( it, quantity, filter );
}
bool location_inventory::has_charges( const itype_id &it, int quantity,
                                      const std::function<bool( const item & )> &filter ) const
{
    return inv.has_charges( it, quantity, filter );
}

int location_inventory::leak_level( const flag_id &flag ) const
{
    return inv.leak_level( flag );
}

int location_inventory::worst_item_value( npc *p ) const
{
    return inv.worst_item_value( p );
}
bool location_inventory::has_enough_painkiller( int pain ) const
{
    return inv.has_enough_painkiller( pain );
}
item *location_inventory::most_appropriate_painkiller( int pain )
{
    return inv.most_appropriate_painkiller( pain );
}

void location_inventory::rust_iron_items()
{
    return inv.rust_iron_items();
}

units::mass location_inventory::weight() const
{
    return inv.weight();
}

units::mass location_inventory::weight_without( const excluded_stacks &without ) const
{
    return inv.weight_without( without );
}

units::volume location_inventory::volume() const
{
    return inv.volume();
}

units::volume location_inventory::volume_without( const excluded_stacks &without ) const
{
    return inv.volume_without( without );
}

void location_inventory::dump( std::vector<item *> &dest )
{
    return inv.dump( dest );
}

std::vector<item *> location_inventory::active_items()
{
    return inv.active_items();
}

void location_inventory::assign_empty_invlet( item &it, const Character &p, bool force )
{
    return inv.assign_empty_invlet( it, p, force );
}

void location_inventory::reassign_item( item &it, char invlet, bool remove_old )
{
    return inv.reassign_item( it, invlet, remove_old );
}

void location_inventory::update_invlet( item &it, bool assign_invlet )
{
    return inv.update_invlet( it, assign_invlet );
}

void location_inventory::set_stack_favorite( int position, bool favorite )
{
    return inv.set_stack_favorite( position, favorite );
}

invlets_bitset location_inventory::allocated_invlets() const
{
    return inv.allocated_invlets();
}

const itype_bin &location_inventory::get_binned_items() const
{
    return inv.get_binned_items();
}

void location_inventory::update_invlet_cache_with_item( item &newit )
{
    return inv.update_invlet_cache_with_item( newit );
}

enchantment location_inventory::get_active_enchantment_cache( const Character &owner ) const
{
    return inv.get_active_enchantment_cache( owner );
}

int location_inventory::count_item( const itype_id &item_type ) const
{
    return inv.count_item( item_type );
}

void location_inventory::update_quality_cache()
{
    return inv.update_quality_cache();
}

const std::map<quality_id, std::map<int, int>> &location_inventory::get_quality_cache() const
{
    return inv.get_quality_cache();
}

void location_inventory::build_items_type_cache()
{
    return inv.build_items_type_cache();
}

const inventory &location_inventory::as_inventory() const
{
    return inv;
}
