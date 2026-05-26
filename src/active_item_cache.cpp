#include "active_item_cache.h"

#include <algorithm>
#include <utility>

#include "item.h"
#include "safe_reference.h"

void active_item_cache::remove( const item *it )
{
    for( auto &list : active_items ) {
        int count = 0;
        list.second.second.erase( std::remove_if( list.second.second.begin(),
        list.second.second.end(), [it, &count, &list]( const cache_reference<item> &active_item ) {
            if( !active_item ) {
                count++;
                return true;
            }
            item *const target = &*active_item;
            if( !target || target == it ) {
                if( count >= list.second.first ) {
                    list.second.first = std::max( 0, list.second.first - 1 );
                }
                count++;
                return true;
            }
            count++;
            return false;
        } ), list.second.second.end() );
    }
    if( it->can_revive() ) {
        std::vector<cache_reference<item>> &corpse = special_items[ special_item_type::corpse ];
        corpse.erase( std::remove( corpse.begin(), corpse.end(), it ), corpse.end() );
    }
    if( it->get_use( "explosion" ) ) {
        std::vector<cache_reference<item>> &explosive = special_items[ special_item_type::explosive ];
        explosive.erase( std::remove( explosive.begin(), explosive.end(), it ), explosive.end() );
    }
}

void active_item_cache::add( item &it )
{
    // If the item is alread in the cache for some reason, don't add a second reference
    std::vector<cache_reference<item>> &target_list = active_items[it.processing_speed()].second;
    if( std::find( target_list.begin(), target_list.end(), it ) != target_list.end() ) {
        return;
    }
    if( it.can_revive() ) {
        special_items[ special_item_type::corpse ].emplace_back( it );
    }
    if( it.get_use( "explosion" ) ) {
        special_items[ special_item_type::explosive ].emplace_back( it );
    }
    target_list.emplace_back( it );
}

bool active_item_cache::empty() const
{
    return std::ranges::all_of( active_items, []( const auto & active_queue ) {
        return std::ranges::none_of( active_queue.second.second, []( const cache_reference<item>
        &active_item ) {
            return static_cast<bool>( active_item );
        } );
    } );
}

std::vector<item *> active_item_cache::get()
{
    std::vector<item *> all_cached_items;
    for( std::pair<const int, std::pair<int, std::vector<cache_reference<item>>>> &kv : active_items ) {
        for( std::vector<cache_reference<item>>::iterator it = kv.second.second.begin();
             it != kv.second.second.end(); ) {
            if( *it ) {
                all_cached_items.push_back( & **it );
                ++it;
            } else {
                it = kv.second.second.erase( it );
            }
        }
    }
    return all_cached_items;
}

std::vector<item *> active_item_cache::get_for_processing()
{
    auto items_to_process = std::vector<item *> {};
    get_for_processing( items_to_process );
    return items_to_process;
}

auto active_item_cache::get_for_processing( std::vector<item *> &items_to_process ) -> void
{
    items_to_process.clear();
    for( std::pair < const int, std::pair<int, std::vector<cache_reference<item>>>> &kv :
         active_items ) {
        //The algorithm here is a bit weird. We're going to process a fraction of the list at a time, keeping track of where we are in the list with a simple int.
        //But, the list could change between each run. As such the number will be reduced when items are removed from it (in ::remove) to prevent skips.

        if( kv.second.second.empty() ) { //Prevents a div by 0 in the modulo operations
            kv.second.first = 0; //May as well reset the position
            continue;
        }

        // Rely on iteration logic to make sure the number is sane.
        int num_to_process = std::max( 1, static_cast<int>( kv.second.second.size() / kv.first ) );
        std::vector<cache_reference<item>>::iterator it = kv.second.second.begin();


        kv.second.first = kv.second.first %
                          kv.second.second.size(); //Make sure the key isn't larger than the array
        std::advance( it, kv.second.first );

        kv.second.first += num_to_process + 1;
        while( num_to_process > 0 ) {
            if( *it ) {
                items_to_process.push_back( & **it );
                --num_to_process;
                ++it;
            } else {
                // The item has been destroyed, so remove the reference from the cache
                it = kv.second.second.erase( it );
                if( kv.second.second.empty() ) {
                    break;
                }
            }
            if( it == kv.second.second.end() ) {
                it = kv.second.second.begin();
            }
        }
    }
}

std::vector<item *> active_item_cache::get_special( special_item_type type )
{
    std::vector<item *> matching_items;
    for( const cache_reference<item> &it : special_items[type] ) {
        if( it ) {
            matching_items.push_back( &*it );
        }
    }
    return matching_items;
}
