#pragma once

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
        std::unordered_map<int, std::pair<int, std::vector<cache_reference<item>>>> active_items;
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
         * Returns a vector of all cached active item references.
         * Broken references are removed from the cache.
         */
        std::vector<item *> get();

        /**
         * Returns the first size() / processing_speed() elements of each list, rounded up.
         * Items returned are rotated to the back of their respective lists, otherwise only the
         * first n items will ever be processed.
         * Broken references encountered when collecting the items to be processed are removed from
         * the cache.
         * Relies on the fact that item::processing_speed() is a constant.
         */
        std::vector<item *> get_for_processing();

        /**
         * Returns the currently tracked list of special active items.
         */
        std::vector<item *> get_special( special_item_type type );
};

