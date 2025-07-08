#include "item_contents.h"

#include <algorithm>
#include <limits>
#include <algorithm>
#include <memory>

#include "character.h"
#include "enums.h"
#include "handle_liquid.h"
#include "item.h"
#include "itype.h"
#include "locations.h"
#include "map.h"

struct tripoint;

item_contents::item_contents( item *container ) : items( new contents_item_location(
                container ) ) {}
/** used to aid migration */
item_contents::item_contents( item *container,
                              std::vector<detached_ptr<item>> &items ) : items( new contents_item_location( container ),
                                          items ) {}

item_contents::~item_contents() = default;

bool item_contents::empty() const
{
    return items.empty();
}

ret_val<bool> item_contents::insert_item( detached_ptr<item> &&it )
{
    bool stacked = false;
    if( it->count_by_charges() ) {
        for( item *check : items ) {
            // NOLINTNEXTLINE(bugprone-use-after-move)
            if( check->merge_charges( std::move( it ) ) ) {
                stacked = true;
                break;
            }
        }
    }

    if( !stacked ) {
        // NOLINTNEXTLINE(bugprone-use-after-move)
        items.push_back( std::move( it ) );
    }

    return ret_val<bool>::make_success();
}

size_t item_contents::num_item_stacks() const
{
    return items.size();
}

bool item_contents::spill_contents( const tripoint &pos )
{
    for( detached_ptr<item> &it : items.clear() ) {
        get_map().add_item_or_charges( pos, std::move( it ) );
    }
    return true;
}

void item_contents::handle_liquid_or_spill( Character &guy )
{
    for( auto iter = items.begin(); iter != items.end(); ) {
        if( ( *iter )->made_of( LIQUID ) ) {
            detached_ptr<item> det;
            iter = items.erase( iter, &det );
            liquid_handler::handle_all_liquid( std::move( det ), 1 );
        } else {
            detached_ptr<item> det;
            iter = items.erase( iter, &det );
            guy.i_add_or_drop( std::move( det ) );
        }
    }
}

void item_contents::casings_handle( const std::function < detached_ptr<item>
                                    ( detached_ptr<item> && ) > &func )
{
    static const flag_id json_flag_CASING( "CASING" );
    items.remove_with( [&func]( detached_ptr<item> &&it ) {
        if( it->has_flag( json_flag_CASING ) ) {
            it->unset_flag( json_flag_CASING );
            it = func( std::move( it ) );
            if( it ) {
                it->set_flag( json_flag_CASING );
            }
        }
        return std::move( it );
    } );
}

std::vector<detached_ptr<item>> item_contents::clear_items()
{
    return items.clear();
}

void item_contents::on_destroy()
{
    items.on_destroy();
}

void item_contents::set_item_defaults()
{
    /* For Items with a magazine or battery in its contents */
    for( item * const &contained_item : items ) {
        /* for guns and other items defined to have a magazine but don't use "ammo" */
        if( contained_item->is_magazine() ) {
            contained_item->ammo_set(
                contained_item->ammo_default(), contained_item->ammo_capacity() / 2
            );
        } else { //Contents are batteries or food
            contained_item->charges = contained_item->typeId()->charges_default();
        }
    }
}

void item_contents::migrate_item( item &obj, const std::set<itype_id> &migrations )
{
    for( const itype_id &c : migrations ) {
        if( std::ranges::none_of( items, [&]( const item * const & e ) {
        return e->typeId() == c;
        } ) ) {
            obj.put_in( item::spawn( c, obj.birthday() ) );
        }
    }
}

bool item_contents::has_any_with( const std::function<bool( const item &it )> &filter ) const
{
    return std::ranges::any_of( items, [&filter]( const item * const & it ) -> bool{ return filter( *it );} );
}

bool item_contents::stacks_with( const item_contents &rhs ) const
{
    return std::equal( items.begin(), items.end(), rhs.items.begin(), []( const item * const & a,
    const item * const & b ) {
        return a->charges == b->charges && a->stacks_with( *b );
    } );
}

item *item_contents::get_item_with( const std::function<bool( const item &it )> &filter )
{
    auto bomb_it = std::ranges::find_if( items,
                                         [&filter]( const item * const & it ) -> bool{ return filter( *it );} );
    if( bomb_it == items.end() ) {
        return nullptr;
    } else {
        return *bomb_it;
    }
}

const std::vector<item *> &item_contents::all_items_top() const
{
    return items.as_vector();
}

detached_ptr<item> item_contents::remove_top( item *it )
{
    auto iter = std::ranges::find_if( items,
    [&it]( item *&against ) {
        return against == it;
    } );
    detached_ptr<item> ret;
    items.erase( iter, &ret );
    return ret;
}

location_vector<item>::iterator item_contents::remove_top( location_vector<item>::iterator &it,
        detached_ptr<item> *removed )
{
    return items.erase( it, removed );
}

std::vector<item *> item_contents::all_items_ptr()
{
    std::vector<item *> ret;
    for( item * const &it : items ) {
        ret.push_back( it );
        std::vector<item *> inside = it->contents.all_items_ptr();
        //TODO!:check
        ret.insert( ret.end(), inside.begin(), inside.end() );
    }
    return ret;
}

std::vector<const item *> item_contents::all_items_ptr() const
{
    std::vector<const item *> ret;
    for( const item * const &it : items ) {
        ret.push_back( it );
        std::vector<const item *> inside = it->contents.all_items_ptr();
        ret.insert( ret.end(), inside.begin(), inside.end() );
    }
    return ret;
}

std::vector<item *> item_contents::gunmods()
{
    std::vector<item *> res;
    for( item *&e : items ) {
        if( e->is_gunmod() ) {
            res.push_back( e );
        }
    }
    return res;
}

std::vector<const item *> item_contents::gunmods() const
{
    std::vector<const item *> res;
    for( const item * const &e : items ) {
        if( e->is_gunmod() ) {
            res.push_back( e );
        }
    }
    return res;
}

item &item_contents::front()
{
    return *items.front();
}

const item &item_contents::front() const
{
    return *items.front();
}

item &item_contents::back()
{
    return *items.back();
}

const item &item_contents::back() const
{
    return *items.back();
}

units::volume item_contents::item_size_modifier() const
{
    units::volume ret = 0_ml;
    for( const item * const &it : items ) {
        ret += it->volume();
    }
    return ret;
}

units::mass item_contents::item_weight_modifier() const
{
    units::mass ret = 0_gram;
    for( const item * const &it : items ) {
        ret += it->weight();
    }
    return ret;
}

int item_contents::best_quality( const quality_id &id ) const
{
    int ret = INT_MIN;
    for( const item * const &it : items ) {
        ret = std::max( ret, it->get_quality( id ) );
    }
    return ret;
}

void item_contents::remove_top_items_with( const std::function < detached_ptr<item>
        ( detached_ptr<item> && ) >
        &filter )
{
    remove_items_with( [&filter]( detached_ptr<item> &&e ) {
        e = filter( std::move( e ) );
        return VisitResponse::SKIP;
    } );
}
