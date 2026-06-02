#include "active_item_cache.h"

#include <algorithm>
#include <utility>

#include "calendar.h"
#include "item.h"
#include "safe_reference.h"

static auto reset_queue_state( active_item_queue &queue ) -> void
{
    queue.cursor = 0;
    queue.processing_credit = 0;
    queue.last_processed_turn = 0;
}

void active_item_cache::remove( const item *it )
{
    for( auto &list : active_items ) {
        active_item_queue &queue = list.second;
        int count = 0;
        queue.items.erase( std::remove_if( queue.items.begin(),
        queue.items.end(), [it, &count, &queue]( const cache_reference<item> &active_item ) {
            if( !active_item ) {
                count++;
                return true;
            }
            item *const target = &*active_item;
            if( !target || target == it ) {
                if( count >= queue.cursor ) {
                    queue.cursor = std::max( 0, queue.cursor - 1 );
                }
                count++;
                return true;
            }
            count++;
            return false;
        } ), queue.items.end() );
        if( queue.items.empty() ) {
            reset_queue_state( queue );
        }
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
    // If the item is already in the cache for some reason, don't add a second reference
    active_item_queue &queue = active_items[it.processing_speed()];
    std::vector<cache_reference<item>> &target_list = queue.items;
    if( std::find( target_list.begin(), target_list.end(), it ) != target_list.end() ) {
        return;
    }
    if( target_list.empty() ) {
        queue.last_processed_turn = to_turn<int>( calendar::turn );
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
        return std::ranges::none_of( active_queue.second.items, []( const cache_reference<item>
        &active_item ) {
            return static_cast<bool>( active_item );
        } );
    } );
}

auto active_item_cache::count() const -> active_item_count
{
    auto result = active_item_count {};
    for( const auto &active_entry : active_items ) {
        const auto &active_queue = active_entry.second;
        for( const cache_reference<item> &active_item : active_queue.items ) {
            if( !active_item ) {
                continue;
            }
            ++result.total;
            if( active_item->goes_bad() ) {
                ++result.rottable;
            }
        }
    }
    return result;
}

std::vector<item *> active_item_cache::get()
{
    std::vector<item *> all_cached_items;
    for( auto &kv : active_items ) {
        active_item_queue &queue = kv.second;
        for( auto it = queue.items.begin(); it != queue.items.end(); ) {
            if( *it ) {
                all_cached_items.push_back( & **it );
                ++it;
            } else {
                it = queue.items.erase( it );
            }
        }
        if( queue.items.empty() ) {
            reset_queue_state( queue );
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
    for( auto &kv : active_items ) {
        //The algorithm here is a bit weird. We're going to process a fraction of the list at a time, keeping track of where we are in the list with a simple int.
        //But, the list could change between each run. As such the number will be reduced when items are removed from it (in ::remove) to prevent skips.
        active_item_queue &queue = kv.second;

        if( queue.items.empty() ) { //Prevents a div by 0 in the modulo operations
            reset_queue_state( queue );
            continue;
        }

        const int processing_speed = std::max( 1, kv.first );
        const int queue_size = static_cast<int>( queue.items.size() );
        int num_to_process = queue_size;
        const int current_turn = to_turn<int>( calendar::turn );
        if( processing_speed > 1 ) {
            // For slow queues, preserve the average processing interval instead of forcing one
            // item per active submap every turn.
            const int64_t elapsed_turns = std::max( 0, current_turn - queue.last_processed_turn );
            queue.last_processed_turn = current_turn;
            const int64_t processing_period = static_cast<int64_t>( processing_speed ) + 1;
            queue.processing_credit += elapsed_turns * queue_size;
            num_to_process = static_cast<int>(
                                 std::min<int64_t>( queue_size, queue.processing_credit / processing_period ) );
            queue.processing_credit %= processing_period;
            if( num_to_process <= 0 ) {
                continue;
            }
        } else {
            queue.processing_credit = 0;
            queue.last_processed_turn = current_turn;
        }
        num_to_process = std::min( num_to_process, queue_size );

        auto it = queue.items.begin();

        queue.cursor = queue.cursor %
                       queue.items.size(); //Make sure the key isn't larger than the array
        std::advance( it, queue.cursor );

        queue.cursor += num_to_process;
        int remaining_slots = static_cast<int>( queue.items.size() );
        while( num_to_process > 0 && remaining_slots > 0 ) {
            --remaining_slots;
            if( *it ) {
                items_to_process.push_back( & **it );
                --num_to_process;
                ++it;
            } else {
                // The item has been destroyed, so remove the reference from the cache
                it = queue.items.erase( it );
                if( queue.items.empty() ) {
                    reset_queue_state( queue );
                    break;
                }
            }
            if( it == queue.items.end() ) {
                it = queue.items.begin();
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
