#pragma once

#include <cstdint>
#include <iosfwd>
#include <list>
#include <unordered_map>
#include <vector>

#include "point.h"
#include "safe_reference.h"

class item;

enum class special_item_type : int {
    none,
    corpse,
    explosive
};

struct active_item_count {
    int64_t total = 0;
    int64_t rottable = 0;
};

struct active_item_queue {
    int cursor = 0;
    int64_t processing_credit = 0;
    int last_processed_turn = 0;
    std::vector<cache_reference<item>> items;
};

namespace std
{
template <>
struct hash<special_item_type> {
    std::size_t operator()( const special_item_type &k ) const noexcept {
        return static_cast<size_t>( k );
    }
};
} // namespace std

class active_item_cache
{
    private:
        std::unordered_map<int, active_item_queue> active_items;
        std::unordered_map<special_item_type, std::vector<cache_reference<item>>> special_items;

    public:
        /**
         * Removes the item if it is in the cache. Does nothing if the item is not in the cache.
         * Relies on the fact that item::processing_speed() is a constant.
         * Also removes any items that have been destroyed in the list containing it
         */
        void remove( const item *it );

        /**
         * Adds the reference to the cache. Does nothing if the reference is already in the cache.
         * Relies on the fact that item::processing_speed() is a constant.
         */
        void add( item &it );

        /**
         * Returns true if the cache is empty
         */
        bool empty() const;

        /**
         * Counts currently valid cached active item references.
         */
        auto count() const -> active_item_count;

        /**
         * Returns a vector of all cached active item references.
         * Broken references are removed from the cache.
         */
        std::vector<item *> get();

        /**
         * Returns items from each list at the average rate requested by item::processing_speed().
         * Items returned are rotated through their respective lists, otherwise only the first
         * n items will ever be processed.
         * Broken references encountered when collecting the items to be processed are removed from
         * the cache.
         * Relies on the fact that item::processing_speed() is a constant.
         */
        std::vector<item *> get_for_processing();
        /**
         * Clears and fills @p items_to_process with the same items as get_for_processing().
         * Reuses caller-owned vector capacity across submaps.
         */
        auto get_for_processing( std::vector<item *> &items_to_process ) -> void;

        /**
         * Returns the currently tracked list of special active items.
         */
        std::vector<item *> get_special( special_item_type type );
};
