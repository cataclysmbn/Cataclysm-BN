#include "visitable.h"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>

#include "active_item_cache.h"
#include "bionics.h"
#include "character.h"
#include "debug.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "make_static.h"
#include "map.h"
#include "map_selector.h"
#include "monster.h"
#include "mtype.h"
#include "mutation.h"
#include "pimpl.h"
#include "player.h"
#include "point.h"
#include "submap.h"
#include "units.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"

static const itype_id itype_apparatus( "apparatus" );
static const itype_id itype_adv_UPS_off( "adv_UPS_off" );
static const itype_id itype_toolset( "toolset" );
static const itype_id itype_voltmeter_bionic( "voltmeter_bionic" );
static const itype_id itype_UPS( "UPS" );
static const itype_id itype_UPS_off( "UPS_off" );
static const itype_id itype_bio_armor( "bio_armor" );

static const quality_id qual_BUTCHER( "BUTCHER" );

static const bionic_id bio_tools( "bio_tools" );
static const bionic_id bio_electrosense_voltmeter( "bio_electrosense_voltmeter" );
static const bionic_id bio_ups( "bio_ups" );

static const flag_id flag_BIONIC_ARMOR_INTERFACE( "BIONIC_ARMOR_INTERFACE" );

/** @relates visitable */
template <typename T>
item *visitable<T>::find_parent( const item &it )
{
    item *res = nullptr;
    if( visit_items( [&]( item * node, item * parent ) {
    if( node == &it ) {
            res = parent;
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } ) != VisitResponse::ABORT ) {
        debugmsg( "Tried to find item parent using an object that doesn't contain it" );
    }
    return res;
}

/** @relates visitable */
template <typename T>
const item *visitable<T>::find_parent( const item &it ) const
{
    return const_cast<visitable<T> *>( this )->find_parent( it );
}

/** @relates visitable */
template <typename T>
std::vector<item *> visitable<T>::parents( const item &it )
{
    std::vector<item *> res;
    for( item *obj = find_parent( it ); obj; obj = find_parent( *obj ) ) {
        res.push_back( obj );
    }
    return res;
}

/** @relates visitable */
template <typename T>
std::vector<const item *> visitable<T>::parents( const item &it ) const
{
    std::vector<const item *> res;
    for( const item *obj = find_parent( it ); obj; obj = find_parent( *obj ) ) {
        res.push_back( obj );
    }
    return res;
}

/** @relates visitable */
template <typename T>
bool visitable<T>::has_item( const item &it ) const
{
    return visit_items( [&it]( const item * node ) {
        return node == &it ? VisitResponse::ABORT : VisitResponse::NEXT;
    } ) == VisitResponse::ABORT;
}

/** @relates visitable */
template <typename T>
bool visitable<T>::has_item_with( const std::function<bool( const item & )> &filter ) const
{
    return visit_items( [&filter]( const item * node ) {
        return filter( *node ) ? VisitResponse::ABORT : VisitResponse::NEXT;
    } ) == VisitResponse::ABORT;
}

/** @relates visitable */
template <typename T>
bool visitable<T>::has_item_directly( const item &it ) const
{
    return visit_items( [&it]( const item * node ) {
        return node == &it ? VisitResponse::ABORT : VisitResponse::SKIP;
    } ) == VisitResponse::ABORT;
}

/** @relates visitable */
template <typename T>
bool visitable<T>::has_item_with_directly( const std::function<bool( const item & )> &filter ) const
{
    return visit_items( [&filter]( const item * node ) {
        return filter( *node ) ? VisitResponse::ABORT : VisitResponse::SKIP;
    } ) == VisitResponse::ABORT;
}

/** Sums the two terms, being careful to not trigger overflow.
 * Doesn't handle underflow.
 *
 * @param a The first addend.
 * @param b The second addend.
 * @return the sum of the addends, but truncated to std::numeric_limits<int>::max().
 */
template <typename T>
static T sum_no_wrap( T a, T b )
{
    if( a > std::numeric_limits<T>::max() - b ||
        b > std::numeric_limits<T>::max() - a ) {
        return std::numeric_limits<T>::max();
    }
    return a + b;
}

template <typename T>
static int has_quality_internal( const T &self, const quality_id &qual, int level, int limit )
{
    int qty = 0;

    self.visit_items( [&qual, level, &limit, &qty]( const item * e ) {
        if( e->get_quality( qual ) >= level ) {
            qty = sum_no_wrap( qty, e->count() );
            if( qty >= limit ) {
                // found sufficient items
                return VisitResponse::ABORT;
            }
        }
        return VisitResponse::NEXT;
    } );
    return std::min( qty, limit );
}

static int has_quality_from_vpart( const vehicle &veh, int part, const quality_id &qual, int level,
                                   int limit )
{
    int qty = 0;

    auto pos = veh.cpart( part ).mount;
    for( const auto &n : veh.parts_at_relative( pos, true ) ) {

        // only unbroken parts can provide tool qualities
        if( !veh.cpart( n ).is_broken() ) {
            auto tq = veh.part_info( n ).qualities;
            auto iter = tq.find( qual );

            // does the part provide this quality?
            if( iter != tq.end() && iter->second >= level ) {
                qty = sum_no_wrap( qty, 1 );
            }
        }
    }
    return std::min( qty, limit );
}

template <typename T>
bool visitable<T>::has_quality( const quality_id &qual, int level, int qty ) const
{
    return has_quality_internal( *this, qual, level, qty ) == qty;
}

/** @relates visitable */
template <>
bool visitable<inventory>::has_quality( const quality_id &qual, int level, int qty ) const
{
    const inventory *inv = static_cast<const inventory *>( this );
    const std::map<quality_id, std::map<int, int>> &inv_qual_cache = inv->get_quality_cache();
    int res = 0;
    if( !inv_qual_cache.empty() ) {
        auto iter = inv_qual_cache.find( qual );
        if( iter != inv_qual_cache.end() ) {
            for( const auto &q : iter->second ) {
                if( q.first >= level ) {
                    res = sum_no_wrap( res, q.second );
                }
            }
        }
        return res >= qty;
    }
    for( const auto &stack : inv->items ) {
        res += stack.size() * has_quality_internal( *stack.front(), qual, level, qty );
        if( res >= qty ) {
            return true;
        }
    }
    return false;
}

template <>
bool visitable<location_inventory>::has_quality( const quality_id &qual, int level, int qty ) const
{

    auto inv = static_cast<const location_inventory *>( this );
    return inv->inv.has_quality( qual, level, qty );
}

/** @relates visitable */
template <>
bool visitable<vehicle_selector>::has_quality( const quality_id &qual, int level, int qty ) const
{
    for( const auto &cursor : static_cast<const vehicle_selector &>( *this ) ) {
        if( cursor.ignore_vpart ) {
            continue;
        }

        qty -= has_quality_from_vpart( cursor.veh, cursor.part, qual, level, qty );
        if( qty <= 0 ) {
            return true;
        }
    }
    return has_quality_internal( *this, qual, level, qty ) == qty;
}

/** @relates visitable */
template <>
bool visitable<vehicle_cursor>::has_quality( const quality_id &qual, int level, int qty ) const
{
    auto self = static_cast<const vehicle_cursor *>( this );

    if( !self->ignore_vpart ) {
        qty -= has_quality_from_vpart( self->veh, self->part, qual, level, qty );
    }
    return qty <= 0 ? true : has_quality_internal( *this, qual, level, qty ) == qty;
}

/** @relates visitable */
template <>
bool visitable<Character>::has_quality( const quality_id &qual, int level, int qty ) const
{
    auto self = static_cast<const Character *>( this );

    for( const auto &bio : *self->my_bionics ) {
        if( bio.get_quality( qual ) >= level ) {
            if( qty <= 1 ) {
                return true;
            }

            qty--;
        }
    }

    return qty <= 0 ? true : has_quality_internal( *this, qual, level, qty ) == qty;
}

template <typename T>
static int max_quality_internal( const T &self, const quality_id &qual )
{
    int res = INT_MIN;
    self.visit_items( [&res, &qual]( const item * e ) {
        res = std::max( res, e->get_quality( qual ) );
        return VisitResponse::NEXT;
    } );
    return res;
}

static int max_quality_from_vpart( const vehicle &veh, int part, const quality_id &qual )
{
    int res = INT_MIN;

    auto pos = veh.cpart( part ).mount;
    for( const auto &n : veh.parts_at_relative( pos, true ) ) {

        // only unbroken parts can provide tool qualities
        if( !veh.cpart( n ).is_broken() ) {
            auto tq = veh.part_info( n ).qualities;
            auto iter = tq.find( qual );

            // does the part provide this quality?
            if( iter != tq.end() ) {
                res = std::max( res, iter->second );
            }
        }
    }
    return res;
}

template <typename T>
int visitable<T>::max_quality( const quality_id &qual ) const
{
    return max_quality_internal( *this, qual );
}

/** @relates visitable */
template<>
int visitable<Character>::max_quality( const quality_id &qual ) const
{
    int res = INT_MIN;

    auto self = static_cast<const Character *>( this );

    for( const auto &bio : *self->my_bionics ) {
        res = std::max( res, bio.get_quality( qual ) );
    }

    if( qual == qual_BUTCHER ) {
        for( const trait_id &mut : self->get_mutations() ) {
            res = std::max( res, mut->butchering_quality );
        }
    }

    return std::max( res, max_quality_internal( *this, qual ) );
}

/** @relates visitable */
template <>
int visitable<vehicle_cursor>::max_quality( const quality_id &qual ) const
{
    auto self = static_cast<const vehicle_cursor *>( this );
    int vpart = self->ignore_vpart ? 0 : max_quality_from_vpart( self->veh, self->part, qual );
    return std::max( vpart, max_quality_internal( *this, qual ) );
}

/** @relates visitable */
template <>
int visitable<vehicle_selector>::max_quality( const quality_id &qual ) const
{
    int res = INT_MIN;
    for( const auto &e : static_cast<const vehicle_selector &>( *this ) ) {
        res = std::max( res, e.max_quality( qual ) );
    }
    return res;
}

/** @relates visitable */
template <typename T>
std::vector<item *> visitable<T>::items_with( const std::function<bool( const item & )> &filter )
const
{
    std::vector<item *> res;
    visit_items( [&res, &filter]( const item * node, const item * ) {
        if( filter( *node ) ) {
            res.push_back( const_cast<item *>( node ) );
        }
        return VisitResponse::NEXT;
    } );
    return res;
}

/** @relates visitable */
template <typename T>
VisitResponse
visitable<T>::visit_items( const std::function<VisitResponse( const item *,
                           const item * )> &func ) const
{
    return const_cast<visitable<T> *>( this )->visit_items(
               static_cast<const std::function<VisitResponse( item *, item * )>&>( func ) );
}

/** @relates visitable */
template <typename T>
VisitResponse visitable<T>::visit_items( const std::function<VisitResponse( const item * )> &func )
const
{
    return visit_items( [&func]( const item * it, const item * ) {
        return func( it );
    } );
}

/** @relates visitable */
template <typename T>
VisitResponse visitable<T>::visit_items( const std::function<VisitResponse( item * )> &func )
{
    return visit_items( [&func]( item * it, item * ) {
        return func( it );
    } );
}



static VisitResponse visit_internal( std::function < VisitResponse( detached_ptr<item> &&e ) >
                                     filter, location_vector<item> &items )
{
    VisitResponse last = VisitResponse::NEXT;
    items.remove_with( [&last, &filter]( detached_ptr<item> &&e ) {
        if( last == VisitResponse::ABORT ) {
            return std::move( e );
        }
        last = filter( std::move( e ) );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        if( last == VisitResponse::NEXT && e ) {
            e->contents.remove_items_with( [&last, &filter]( detached_ptr<item> &&e ) {
                if( last == VisitResponse::ABORT ) {
                    return last;
                }
                last = filter( std::move( e ) );
                return last;
            } );
        }
        return std::move( e );
    } );
    return last == VisitResponse::ABORT ? VisitResponse::ABORT : VisitResponse::NEXT;
}

// Specialize visitable<T>::visit_items() for each class that will implement the visitable interface

static VisitResponse visit_internal( const std::function<VisitResponse( item *, item * )> &func,
                                     item *node, item *parent = nullptr )
{
    switch( func( node, parent ) ) {
        case VisitResponse::ABORT:
            return VisitResponse::ABORT;

        case VisitResponse::NEXT:
            if( node->is_gun() || node->is_magazine() ) {
                // Content of guns and magazines are accessible only via their specific accessors
                return VisitResponse::NEXT;
            }

            if( node->contents.visit_contents( func, node ) == VisitResponse::ABORT ) {
                return VisitResponse::ABORT;
            }
        /* intentional fallthrough */

        case VisitResponse::SKIP:
            return VisitResponse::NEXT;
    }

    /* never reached but suppresses GCC warning */
    return VisitResponse::ABORT;
}

VisitResponse item_contents::visit_contents( const std::function<VisitResponse( item *, item * )>
        &func, item *parent )
{
    for( item *&e : items ) {
        switch( visit_internal( func, e, parent ) ) {
            case VisitResponse::ABORT:
                return VisitResponse::ABORT;
            default:
                break;
        }
    }
    return VisitResponse::NEXT;
}

/** @relates visitable */
template <>
VisitResponse visitable<item>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    auto it = static_cast<item *>( this );
    return visit_internal( func, it );
}

/** @relates visitable */
template <>
VisitResponse visitable<inventory>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    auto inv = static_cast<const inventory *>( this );
    for( auto &stack : inv->items ) {
        for( auto &it : stack ) {
            if( visit_internal( func, it ) == VisitResponse::ABORT ) {
                return VisitResponse::ABORT;
            }
        }
    }
    return VisitResponse::NEXT;
}

template <>
VisitResponse visitable<location_inventory>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    auto inv = static_cast<location_inventory *>( this );
    return inv->inv.visit_items( func );
}

/** @relates visitable */
template <>
VisitResponse visitable<Character>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    auto ch = static_cast<Character *>( this );

    if( !ch->primary_weapon().is_null() &&
        visit_internal( func, &ch->primary_weapon() ) == VisitResponse::ABORT ) {
        return VisitResponse::ABORT;
    }

    for( auto &e : ch->worn ) {
        if( visit_internal( func, e ) == VisitResponse::ABORT ) {
            return VisitResponse::ABORT;
        }
    }

    return ch->inv.visit_items( func );
}

/** @relates visitable */
template <>
VisitResponse visitable<map_cursor>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    auto cur = static_cast<map_cursor *>( this );
    map &here = get_map();
    // skip inaccessible items
    if( here.has_flag( "SEALED", *cur ) && !here.has_flag( "LIQUIDCONT", *cur ) ) {
        return VisitResponse::NEXT;
    }

    for( item *&e : here.i_at( *cur ) ) {
        if( visit_internal( func, e ) == VisitResponse::ABORT ) {
            return VisitResponse::ABORT;
        }
    }
    return VisitResponse::NEXT;
}

/** @relates visitable */
template <>
VisitResponse visitable<map_selector>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    for( auto &cursor : static_cast<map_selector &>( *this ) ) {
        if( cursor.visit_items( func ) == VisitResponse::ABORT ) {
            return VisitResponse::ABORT;
        }
    }
    return VisitResponse::NEXT;
}

/** @relates visitable */
template <>
VisitResponse visitable<vehicle_cursor>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    auto self = static_cast<vehicle_cursor *>( this );

    int idx = self->veh.part_with_feature( self->part, "CARGO", true );
    if( idx >= 0 ) {
        for( auto *&e : self->veh.get_items( idx ) ) {
            if( visit_internal( func, e ) == VisitResponse::ABORT ) {
                return VisitResponse::ABORT;
            }
        }
    }
    return VisitResponse::NEXT;
}

/** @relates visitable */
template <>
VisitResponse visitable<vehicle_selector>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    for( auto &cursor : static_cast<vehicle_selector &>( *this ) ) {
        if( cursor.visit_items( func ) == VisitResponse::ABORT ) {
            return VisitResponse::ABORT;
        }
    }
    return VisitResponse::NEXT;
}

/** @relates visitable */
template <>
VisitResponse visitable<monster>::visit_items(
    const std::function<VisitResponse( item *, item * )> &func )
{
    monster *mon = static_cast<monster *>( this );

    for( item * const &it : mon->get_items() ) {
        if( visit_internal( func, it ) == VisitResponse::ABORT ) {
            return VisitResponse::ABORT;
        }
    }

    if( mon->get_storage_item() &&
        visit_internal( func, mon->get_storage_item() ) == VisitResponse::ABORT ) {
        return VisitResponse::ABORT;
    }
    if( mon->get_armor_item() &&
        visit_internal( func, mon->get_armor_item() ) == VisitResponse::ABORT ) {
        return VisitResponse::ABORT;
    }
    if( mon->get_tack_item() &&
        visit_internal( func, mon->get_tack_item() ) == VisitResponse::ABORT ) {
        return VisitResponse::ABORT;
    }
    if( mon->get_tied_item() &&
        visit_internal( func, mon->get_tied_item() ) == VisitResponse::ABORT ) {
        return VisitResponse::ABORT;
    }

    return VisitResponse::NEXT;
}

// Specialize visitable<T>::remove_items_with() for each class that will implement the visitable interface

/** @relates visitable */
template <typename T>
void temp_visitable<T>::remove_item( item &it )
{
    auto obj = remove_items_with( [&it]( const item & e ) {
        return &e == &it;
    } );
    if( obj.empty() ) {
        debugmsg( "Tried removing item from object which did not contain it" );
    }
}

template <typename T>
detached_ptr<item> location_visitable<T>::remove_item( item &it )
{
    detached_ptr<item> ret;
    remove_items_with( [&it, &ret]( detached_ptr<item> &&e ) {
        if( &*e == &it ) {
            ret = std::move( e );
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );
    if( !ret ) {
        debugmsg( "Tried removing item from object which did not contain it" );
    }
    return ret;
}

void item_contents::remove_items_with( const std::function < VisitResponse(
        detached_ptr<item> && ) > &filter )
{
    visit_internal( filter, items );
}

/** @relates visitable */
template <>
std::vector<item *> temp_visitable<inventory>::remove_items_with( const
        std::function<bool( const item &e )> &filter, int count )
{
    auto inv = static_cast<inventory *>( this );
    std::vector<item *> res;
    if( inv->locked ) {
        debugmsg( "Attempted recursive item removal" );
        return res;
    }

    if( count <= 0 ) {
        // nothing to do
        return res;
    }
    inv->locked = true;
    for( auto stack = inv->items.begin(); stack != inv->items.end() && count > 0; ) {
        std::vector<item *> &istack = *stack;
        for( auto istack_iter = istack.begin(); istack_iter != istack.end() && count > 0; ) {
            if( filter( **istack_iter ) ) {
                count--;
                res.push_back( *istack_iter );
                istack_iter = istack.erase( istack_iter );
            } else {
                ++istack_iter;
            }
        }

        if( istack.empty() ) {
            stack = inv->items.erase( stack );
        } else {
            ++stack;
        }
    }

    // Invalidate binning cache
    inv->binned = false;
    inv->items_type_cached = false;
    inv->locked = false;
    return res;
}

template <>
void location_visitable<item>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    auto it = static_cast<item *>( this );
    it->contents.remove_items_with( filter );
}

template <>
void location_visitable<location_inventory>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    auto inv = static_cast<location_inventory *>( this );
    if( inv->inv.locked ) {
        debugmsg( "Attempted recursive item removal" );
        return;
    }
    inv->inv.locked = true;


    VisitResponse last = VisitResponse::NEXT;
    for( auto stack = inv->inv.items.begin(); stack != inv->inv.items.end() &&
         last != VisitResponse::ABORT; ) {
        std::vector<item *> &istack = *stack;
        const auto original_invlet = istack.front()->invlet;

        for( auto istack_iter = istack.begin(); istack_iter != istack.end() &&
             last != VisitResponse::ABORT; ) {
            location<item> *saved_loc = ( *istack_iter )->loc;
            ( *istack_iter )->remove_location();
            detached_ptr<item> t( *istack_iter );

            last = filter( std::move( t ) );

            // NOLINTNEXTLINE(bugprone-use-after-move)
            if( !t ) {
                istack_iter = istack.erase( istack_iter );
                if( istack_iter == istack.begin() && istack_iter != istack.end() ) {
                    ( *istack_iter )->invlet = original_invlet;
                }
            } else if( last == VisitResponse::NEXT ) {
                t->contents.remove_items_with( [&filter, &last]( detached_ptr<item> &&e ) {
                    last = filter( std::move( e ) );
                    return last;
                } );
            }

            if( t ) {
                t.release();
                ( *istack_iter )->set_location( saved_loc );
                istack_iter++;
            }
        }

        if( istack.empty() ) {
            stack = inv->inv.items.erase( stack );
        } else {
            ++stack;
        }
    }

    // Invalidate binning cache
    inv->inv.binned = false;
    inv->inv.items_type_cached = false;
    inv->inv.locked = false;
}

/** @relates visitable */
template <>
void location_visitable<Character>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    auto ch = static_cast<Character *>( this );

    VisitResponse last = VisitResponse::NEXT;

    auto filter_and_collect = [&last, &filter]( detached_ptr<item> &&e ) {
        if( last == VisitResponse::ABORT ) {
            return last;
        }
        last = filter( std::move( e ) );
        return last;
    };

    // first try and remove items from the inventory
    ch->inv.remove_items_with( filter_and_collect );

    if( last == VisitResponse::ABORT ) {
        return;
    }

    visit_internal( filter_and_collect, ch->worn );

    if( last == VisitResponse::ABORT ) {
        return;
    }

    if( !ch->primary_weapon().attempt_detach( [&last, &filter]( detached_ptr<item> &&e ) {
    last = filter( std::move( e ) );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        return std::move( e );
    } ) ) {
        if( last == VisitResponse::NEXT ) {
            ch->primary_weapon().contents.remove_items_with( filter );
        }
    }
}

/** @relates visitable */
template <>
void location_visitable<map_cursor>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    auto cur = static_cast<map_cursor *>( this );

    map &here = get_map();
    if( !here.inbounds( *cur ) ) {
        debugmsg( "cannot remove items from map: cursor out-of-bounds" );
        return;
    }

    // fetch the appropriate item stack
    point offset;
    submap *sub = here.get_submap_at( *cur, offset );

    visit_internal( [&filter, sub, offset]( detached_ptr<item> &&e ) {
        item &obj = *e;
        VisitResponse res = filter( std::move( e ) );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        if( !e ) {
            // remove from the active items cache (if it isn't there does nothing)
            sub->active_items.remove( &obj );

            // if necessary remove item from the luminosity map
            sub->update_lum_rem( offset, obj );
        }
        return res;
    }, sub->get_items( offset ) );

    here.update_submap_active_item_status( *cur );
}

/** @relates visitable */
template <>
void location_visitable<map_selector>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    VisitResponse last = VisitResponse::NEXT;
    for( auto &cursor : static_cast<map_selector &>( *this ) ) {
        cursor.remove_items_with( [&last, &filter]( detached_ptr<item> &&e ) {
            if( last == VisitResponse::ABORT ) {
                return last;
            }
            last = filter( std::move( e ) );
            return last;
        } );
        if( last == VisitResponse::ABORT ) {
            return;
        }
    }
}

/** @relates visitable */
template <>
void location_visitable<vehicle_cursor>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    auto cur = static_cast<vehicle_cursor *>( this );

    int idx = cur->veh.part_with_feature( cur->part, "CARGO", false );
    if( idx < 0 ) {
        return;
    }

    vehicle_part &part = cur->veh.part( idx );
    bool removed = false;

    visit_internal( [&filter, &removed]( detached_ptr<item> &&e ) {
        VisitResponse ret = filter( std::move( e ) );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        if( !e ) {
            removed = true;
        }
        return ret;
    }, part.items );

    if( removed ) {
        // if we removed any items then invalidate the cached mass
        cur->veh.invalidate_mass();
    }

}

/** @relates visitable */
template <>
void location_visitable<vehicle_selector>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{

    VisitResponse last = VisitResponse::NEXT;
    for( auto &cursor : static_cast<vehicle_selector &>( *this ) ) {
        cursor.remove_items_with( [&last, &filter]( detached_ptr<item> &&e ) {
            if( last == VisitResponse::ABORT ) {
                return last;
            }
            last = filter( std::move( e ) );
            return last;
        } );
        if( last == VisitResponse::ABORT ) {
            return;
        }
    }
}

/** @relates visitable */
template <>
void location_visitable<monster>::remove_items_with( const
        std::function < VisitResponse( detached_ptr<item> &&e ) > &filter )
{
    monster *mon = static_cast<monster *>( this );

    VisitResponse last = VisitResponse::NEXT;
    auto filter_and_collect = [&last, &filter]( detached_ptr<item> &&e ) {
        if( last == VisitResponse::ABORT ) {
            return last;
        }
        last = filter( std::move( e ) );
        return last;
    };

    visit_internal( filter_and_collect, mon->inv );

    auto check_item = [&filter_and_collect]( detached_ptr<item> &&it ) {
        filter_and_collect( std::move( it ) );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        return std::move( it );
    };

    if( mon->get_storage_item() ) {
        mon->get_storage_item()->attempt_detach( check_item );
    }
    if( mon->get_armor_item() ) {
        mon->get_armor_item()->attempt_detach( check_item );
    }
    if( mon->get_tack_item() ) {
        mon->get_tack_item()->attempt_detach( check_item );
    }
    if( mon->get_tied_item() ) {
        mon->get_tied_item()->attempt_detach( check_item );
    }
}

template <typename T, typename M>
static int charges_of_internal( const T &self, const M &main, const itype_id &id, int limit,
                                const std::function<bool( const item & )> &filter,
                                std::function<void( int )> visitor )
{
    int qty = 0;

    bool found_tool_with_UPS = false;
    self.visit_items( [&]( const item * e ) {
        if( filter( *e ) ) {
            if( e->is_tool() ) {
                if( e->typeId() == id ) {
                    // includes charges from any included magazine.
                    qty = sum_no_wrap( qty, e->ammo_remaining() );
                    if( e->has_flag( STATIC( flag_id( "USE_UPS" ) ) ) ) {
                        found_tool_with_UPS = true;
                    }
                }
                return qty < limit ? VisitResponse::SKIP : VisitResponse::ABORT;

            } else if( e->count_by_charges() ) {
                if( e->typeId() == id ) {
                    qty = sum_no_wrap( qty, e->charges );
                }
                // items counted by charges are not themselves expected to be containers
                return qty < limit ? VisitResponse::SKIP : VisitResponse::ABORT;
            }
        }
        // recurse through any nested containers
        return qty < limit ? VisitResponse::NEXT : VisitResponse::ABORT;
    } );

    if( qty < limit && found_tool_with_UPS ) {
        qty += main.charges_of( itype_UPS, limit - qty );
        if( visitor ) {
            visitor( qty );
        }
    }

    return std::min( qty, limit );
}

/** @relates visitable */
template <typename T>
int visitable<T>::charges_of( const itype_id &what, int limit,
                              const std::function<bool( const item & )> &filter,
                              std::function<void( int )> visitor ) const
{
    return charges_of_internal( *this, *this, what, limit, filter, visitor );
}

/** @relates visitable */
template <>
int visitable<inventory>::charges_of( const itype_id &what, int limit,
                                      const std::function<bool( const item & )> &filter,
                                      std::function<void( int )> visitor ) const
{
    if( what == itype_UPS ) {
        int qty = 0;
        qty = sum_no_wrap( qty, charges_of( itype_UPS_off ) );
        qty = sum_no_wrap( qty, static_cast<int>( charges_of( itype_adv_UPS_off ) / 0.5 ) );
        return std::min( qty, limit );
    }
    const auto &binned = static_cast<const inventory *>( this )->get_binned_items();
    const auto iter = binned.find( what );
    if( iter == binned.end() ) {
        return 0;
    }

    int res = 0;
    for( const item *it : iter->second ) {
        res = sum_no_wrap( res, charges_of_internal( *it, *this, what, limit, filter, visitor ) );
        if( res >= limit ) {
            break;
        }
    }
    return std::min( limit, res );
}

template <>
int visitable<location_inventory>::charges_of( const itype_id &what, int limit,
        const std::function<bool( const item & )> &filter,
        std::function<void( int )> visitor ) const
{

    auto inv = static_cast<const location_inventory *>( this );
    return inv->inv.charges_of( what, limit, filter, std::move( visitor ) );
}

/** @relates visitable */
template <>
int visitable<Character>::charges_of( const itype_id &what, int limit,
                                      const std::function<bool( const item & )> &filter,
                                      std::function<void( int )> visitor ) const
{
    auto self = static_cast<const Character *>( this );
    auto p = dynamic_cast<const player *>( self );

    if( what == itype_toolset ) {
        if( p && p->has_active_bionic( bio_tools ) ) {
            return std::min( units::to_kilojoule( p->get_power_level() ), limit );
        } else {
            return 0;
        }
    }

    if( what == itype_voltmeter_bionic ) {
        if( p && p->has_bionic( bio_electrosense_voltmeter ) ) {
            return std::min( units::to_kilojoule( p->get_power_level() ), limit );
        } else {
            return 0;
        }
    }

    if( what == itype_bio_armor ) {
        float efficiency = 1;
        int power_charges = 0;

        for( const bionic &bio : *self->my_bionics ) {
            if( bio.powered && bio.info().has_flag( flag_BIONIC_ARMOR_INTERFACE ) ) {
                efficiency = std::max( efficiency, bio.info().fuel_efficiency );
            }
        }
        if( efficiency == 1 ) {
            debugmsg( "Character lacks a bionic armor interface with fuel efficiency field." );
        }
        power_charges = units::to_kilojoule( self->as_player()->get_power_level() ) * efficiency;

        return std::min( power_charges, limit );
    }

    if( what == itype_UPS ) {
        int qty = 0;
        qty = sum_no_wrap( qty, charges_of( itype_UPS_off ) );
        qty = sum_no_wrap( qty, static_cast<int>( charges_of( itype_adv_UPS_off ) / 0.5 ) );
        if( p && p->has_active_bionic( bio_ups ) ) {
            qty = sum_no_wrap( qty, units::to_kilojoule( p->get_power_level() ) );
        }
        if( p && p->is_mounted() ) {
            auto mons = p->mounted_creature.get();
            if( mons->has_flag( MF_RIDEABLE_MECH ) && mons->get_battery_item() ) {
                qty = sum_no_wrap( qty, mons->get_battery_item()->ammo_remaining() );
            }
        }
        return std::min( qty, limit );
    }

    return charges_of_internal( *this, *this, what, limit, filter, std::move( visitor ) );
}

template <typename T>
static int amount_of_internal( const T &self, const itype_id &id, bool pseudo, int limit,
                               const std::function<bool( const item & )> &filter )
{
    int qty = 0;
    self.visit_items( [&qty, &id, &pseudo, &limit, &filter]( const item * e ) {
        if( ( id.str() == "any" || e->typeId() == id ) && filter( *e ) && ( pseudo ||
                !e->has_flag( STATIC( flag_id( "PSEUDO" ) ) ) ) ) {
            qty = sum_no_wrap( qty, 1 );
        }
        return qty != limit ? VisitResponse::NEXT : VisitResponse::ABORT;
    } );
    return qty;
}

/** @relates visitable */
template <typename T>
int visitable<T>::amount_of( const itype_id &what, bool pseudo, int limit,
                             const std::function<bool( const item & )> &filter ) const
{
    return amount_of_internal( *this, what, pseudo, limit, filter );
}

/** @relates visitable */
template <>
int visitable<inventory>::amount_of( const itype_id &what, bool pseudo, int limit,
                                     const std::function<bool( const item & )> &filter ) const
{
    const auto &binned = static_cast<const inventory *>( this )->get_binned_items();
    const auto iter = binned.find( what );
    if( iter == binned.end() && what != itype_id( "any" ) ) {
        return 0;
    }

    int res = 0;
    if( what.str() == "any" ) {
        for( const auto &kv : binned ) {
            for( const item *it : kv.second ) {
                res = sum_no_wrap( res, it->amount_of( what, pseudo, limit, filter ) );
                if( res >= limit ) {
                    break;
                }
            }
        }
    } else {
        for( const item *it : iter->second ) {
            res = sum_no_wrap( res, it->amount_of( what, pseudo, limit, filter ) );
            if( res >= limit ) {
                break;
            }
        }
    }

    return std::min( limit, res );
}

template <>
int visitable<location_inventory>::amount_of( const itype_id &what, bool pseudo, int limit,
        const std::function<bool( const item & )> &filter ) const
{

    auto inv = static_cast<const location_inventory *>( this );
    return inv->inv.amount_of( what, pseudo, limit, filter );
}


/** @relates visitable */
template <>
int visitable<Character>::amount_of( const itype_id &what, bool pseudo, int limit,
                                     const std::function<bool( const item & )> &filter ) const
{
    auto self = static_cast<const Character *>( this );

    if( what == itype_toolset && pseudo && self->has_active_bionic( bio_tools ) ) {
        return 1;
    }

    if( what == itype_voltmeter_bionic && pseudo && self->has_bionic( bio_electrosense_voltmeter ) ) {
        return 1;
    }

    if( what == itype_apparatus && pseudo ) {
        int qty = 0;
        visit_items( [&qty, &limit, &filter]( const item * e ) {
            if( e->get_quality( quality_id( "SMOKE_PIPE" ) ) >= 1 && filter( *e ) ) {
                qty = sum_no_wrap( qty, 1 );
            }
            return qty < limit ? VisitResponse::SKIP : VisitResponse::ABORT;
        } );
        return std::min( qty, limit );
    }

    return amount_of_internal( *this, what, pseudo, limit, filter );
}

/** @relates visitable */
template <typename T>
bool visitable<T>::has_amount( const itype_id &what, int qty, bool pseudo,
                               const std::function<bool( const item & )> &filter ) const
{
    return amount_of( what, pseudo, qty, filter ) == qty;
}

template <typename T>
void location_visitable<T>::remove_top_items_with( const std::function < detached_ptr<item>
        ( detached_ptr<item> && ) > &filter )
{
    remove_items_with( [&filter]( detached_ptr<item> &&e ) {
        e = filter( std::move( e ) );
        return VisitResponse::SKIP;
    } );
}

template <typename T>
void location_visitable<T>::remove_all_items_with( const std::function < detached_ptr<item>
        ( detached_ptr<item> && ) > &filter )
{
    remove_items_with( [&filter]( detached_ptr<item> &&e ) {
        e = filter( std::move( e ) );
        return VisitResponse::NEXT;
    } );
}

// explicit template initialization for all classes implementing the visitable interface
template class visitable<item>;
template class visitable<inventory>;
template class visitable<location_inventory>;
template class visitable<Character>;
template class visitable<map_selector>;
template class visitable<map_cursor>;
template class visitable<vehicle_selector>;
template class visitable<vehicle_cursor>;
template class visitable<monster>;

template class temp_visitable<inventory>;
template class location_visitable<item>;
template class location_visitable<location_inventory>;
template class location_visitable<Character>;
template class location_visitable<map_selector>;
template class location_visitable<map_cursor>;
template class location_visitable<vehicle_selector>;
template class location_visitable<vehicle_cursor>;
template class location_visitable<monster>;
