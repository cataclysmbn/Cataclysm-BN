#pragma once

#include <climits>
#include <functional>
#include <list>
#include <string>
#include <vector>

#include "filter_utils.h"
#include "type_id.h"

class item;
template<typename T>
class detached_ptr;

enum class VisitResponse {
    ABORT, // Stop processing after this node
    NEXT,  // Descend vertically to any child nodes and then horizontally to next sibling
    SKIP   // Skip any child nodes and move directly to the next sibling
};


template <typename T>
class visitable
{
    public:
        /**
         * Traverses this object and any child items contained using a visitor pattern
         *
         * @param func visitor function called for each node which controls whether traversal continues.
         * The first argument is the node and the second is the parent node (if any)
         *
         * The visitor function should return VisitResponse::Next to recursively process child items,
         * VisitResponse::Skip to ignore children of the current node or VisitResponse::Abort to skip all remaining nodes
         *
         * @return This method itself only ever returns VisitResponse::Next or VisitResponse::Abort.
         */
        VisitResponse visit_items( const std::function<VisitResponse( item *, item * )> &func );
        VisitResponse visit_items( const std::function<VisitResponse( const item *, const item * )> &func )
        const;

        /** Lightweight version which provides only the current node */
        VisitResponse visit_items( const std::function<VisitResponse( item * )> &func );
        VisitResponse visit_items( const std::function<VisitResponse( const item * )> &func ) const;

        /**
         * Determine the immediate parent container (if any) for an item.
         * @param it item to search for which must be contained (at any depth) by this object
         * @return parent container or nullptr if the item is not within a container
         */
        item *find_parent( const item &it );
        const item *find_parent( const item &it ) const;

        /**
         * Returns vector of parent containers (if any) starting with the innermost
         * @param it item to search for which must be contained (at any depth) by this object
         */
        std::vector<item *> parents( const item &it );
        std::vector<const item *> parents( const item &it ) const;

        /** Returns true if this visitable instance contains the item */
        bool has_item( const item &it ) const;

        /** Returns true if any item (including those within a container) matches the filter */
        bool has_item_with( const std::function<bool( const item & )> &filter ) const;

        /** Returns true if this visitable instance directly contains the item. */
        bool has_item_directly( const item &it ) const;

        /** Returns true if any item directly within the visitable (so excluding those within a container) matches the filter */
        bool has_item_with_directly( const std::function<bool( const item & )> &filter ) const;

        /** Returns true if instance has amount (or more) items of at least quality level */
        bool has_quality( const quality_id &qual, int level = 1, int qty = 1 ) const;

        /** Return maximum tool quality level provided by instance or INT_MIN if not found */
        int max_quality( const quality_id &qual ) const;

        /**
         * Count maximum available charges from this instance and any contained items
         * @param what ID of item to count charges of
         * @param limit stop searching after this many charges have been found
         * @param filter only count charges of items that match the filter
         * @param visitor is called when UPS charge is used (parameter is the charge itself)
         */
        int charges_of( const itype_id &what, int limit = INT_MAX,
                        const std::function<bool( const item & )> &filter = return_true<item>,
                        std::function<void( int )> visitor = nullptr ) const;

        /**
         * Count items matching id including both this instance and any contained items
         * @param what ID of items to count. "any" will count all items (usually used with a filter)
         * @param pseudo whether pseudo-items (from map/vehicle tiles, bionics etc) are considered
         * @param limit stop searching after this many matches
         * @param filter only count items that match the filter
         * @note items must be empty to be considered a match
         */
        int amount_of( const itype_id &what, bool pseudo = true,
                       int limit = INT_MAX,
                       const std::function<bool( const item & )> &filter = return_true<item> ) const;

        /** Check instance provides at least qty of an item (@see amount_of) */
        bool has_amount( const itype_id &what, int qty, bool pseudo = true,
                         const std::function<bool( const item & )> &filter = return_true<item> ) const;

        /** Returns all items (including those within a container) matching the filter */
        std::vector<item *> items_with( const std::function<bool( const item & )> &filter ) const;

};

template <typename T>
class temp_visitable : public visitable<T>
{
    public:
        /**
         * Removes items contained by this instance which match the filter
         * @note if this instance itself is an item it will not be considered by the filter
         * @param filter a UnaryPredicate which can optionally std::move the detached pointer. If it does the item will be removed
         * @param count maximum number of items to if unspecified unlimited. A count of zero is a no-op
         * @return any items removed (items counted by charges are not guaranteed to be stacked)
         */
        std::vector<item *> remove_items_with( const std::function < bool( const item & ) > &filter,
                                               int count = INT_MAX );

        /** Removes and returns the item which must be contained by this instance */
        void remove_item( item &it );
};

template <typename T>
class location_visitable : public visitable<T>
{
    public:
        /**
         * Removes items contained by this instance which match the filter
         * @note if this instance itself is an item it will not be considered by the filter
         * @param filter a function which can optionally std::move the detached pointer. If it does the item will be removed and the visit response respected
         */
        void remove_items_with( const std::function < VisitResponse( detached_ptr<item> && ) > &filter );
        /**
         * Removes items contained in the top level of this instance which match the filter
         * @note if this instance itself is an item it will not be considered by the filter
         * @param filter a detached_ptr -> detached_ptr filter
         */
        void remove_top_items_with( const std::function < detached_ptr<item>( detached_ptr<item> && ) >
                                    &filter );

        /**
         * Removes items contained recursively within this instance which match the filter
         * @note if this instance itself is an item it will not be considered by the filter
         * @param filter a detached_ptr -> detached_ptr filter
         */
        void remove_all_items_with( const std::function < detached_ptr<item>( detached_ptr<item> && ) >
                                    &filter );

        /** Removes and returns the item which must be contained by this instance */
        detached_ptr<item> remove_item( item &it );
};


