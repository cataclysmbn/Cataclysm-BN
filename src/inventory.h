#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "item.h"
#include "units.h"
#include "visitable.h"

class Character;
class JsonIn;
class JsonOut;
class item_stack;
class map;
class npc;
class player;
struct tripoint;

using invstack = std::list<std::vector<item *> >;
using const_invslice = std::vector<const std::vector<item *> *>;
using indexed_invslice = std::vector< std::pair<std::vector<item *> *, int> >;
using itype_bin = std::unordered_map< itype_id, std::list<const item *> >;
using invlets_bitset = std::bitset<std::numeric_limits<char>::max()>;

/** First element is pointer to item stack (first item), second is amount. */
using excluded_stacks = std::map<item *, int>;

/**
 * Wrapper to handled a set of valid "inventory" letters. "inventory" can be any set of
 * objects that the player can access via a single character (e.g. bionics).
 * The class is (currently) derived from std::string for compatibility and because it's
 * simpler. But it may be changed to derive from `std::set<int>` or similar to get the full
 * range of possible characters.
 */
class invlet_wrapper : private std::string
{
    public:
        invlet_wrapper( const char *chars ) : std::string( chars ) { }

        bool valid( int invlet ) const;
        std::string get_allowed_chars() const {
            return *this;
        }

        using std::string::begin;
        using std::string::end;
        using std::string::rbegin;
        using std::string::rend;
        using std::string::size;
        using std::string::length;
};

const extern invlet_wrapper inv_chars;

class location_inventory;

// For each item id, store a set of "favorite" inventory letters.
// This class maintains a bidirectional mapping between invlet letters and item ids.
// Each invlet has at most one id and each id has any number of invlets.
class invlet_favorites
{
    public:
        invlet_favorites() = default;
        invlet_favorites( const std::unordered_map<itype_id, std::string> & );

        void set( char invlet, const itype_id & );
        void erase( char invlet );
        bool contains( char invlet, const itype_id & ) const;
        std::string invlets_for( const itype_id & ) const;

        // For serialization only
        const std::unordered_map<itype_id, std::string> &get_invlets_by_id() const;
    private:
        std::unordered_map<itype_id, std::string> invlets_by_id;
        std::array<itype_id, 256> ids_by_invlet;
};

class inventory : public temp_visitable<inventory>
{
    private:
        template<bool IsCached>
        item &add_item_internal( item &newit, bool keep_invlet, bool assign_invlet, bool should_stack );

    public:
        const_invslice const_slice() const;
        const std::vector<item *> &const_stack( int i ) const;
        size_t size() const;
        bool locked = false;

        std::map<char, itype_id> assigned_invlet;

        inventory();
        inventory( inventory && ) = default;
        inventory( const inventory & ) = default;
        inventory &operator=( inventory && ) = default;
        inventory &operator=( const inventory & ) = default;

        void unsort(); // flags the inventory as unsorted
        void clear();

        // --- Currently Unused - Kept since there was an add-assign overload before
        inventory &add_items( const inventory &rhs, bool keep_invlet, bool assign_invlet = true,
                              bool should_stack = true );
        inventory &add_items( const item_stack &rhs, bool keep_invlet, bool assign_invlet = true,
                              bool should_stack = true );
        // ---

        inventory &add_items( const location_inventory &rhs, bool keep_invlet, bool assign_invlet = true,
                              bool should_stack = true );
        inventory &add_items( const std::vector<item *> &rhs, bool keep_invlet, bool assign_invlet = true,
                              bool should_stack = true );
        inventory &add_items( const location_vector<item> &rhs, bool keep_invlet, bool assign_invlet = true,
                              bool should_stack = true );

        // returns a reference to the added item
        item &add_item( item &newit, bool keep_invlet, bool assign_invlet = true,
                        bool should_stack = true );
        // use item type cache to speed up, remember to run build_items_type_cache() before using it
        item &add_item_by_items_type_cache( item &newit, bool keep_invlet, bool assign_invlet = true,
                                            bool should_stack = true );

        /* Check all items for proper stacking, rearranging as needed
         * game pointer is not necessary, but if supplied, will ensure no overlap with
         * the player's worn items / weapon
         */
        void restack( player &p );
        void form_from_zone( map &m, std::unordered_set<tripoint> &zone_pts, const Character *pl = nullptr,
                             bool assign_invlet = true );
        void form_from_map( const tripoint &origin, int range, const Character *pl = nullptr,
                            bool assign_invlet = true,
                            bool clear_path = true );
        void form_from_map( map &m, const tripoint &origin, int range, const Character *pl = nullptr,
                            bool assign_invlet = true,
                            bool clear_path = true );
        void form_from_map( map &m, std::vector<tripoint> pts, const Character *pl,
                            bool assign_invlet = true );
        /**
         * Remove a specific item from the inventory. The item is compared
         * by pointer. Contents of the item are removed as well.
         * @param it A pointer to the item to be removed. The item *must* exists
         * in this inventory.
         * @return A copy of the removed item.
         */
        item &remove_item( const item *it );
        item &remove_item( int position );

        const item &find_item( int position ) const;
        item &find_item( int position );

        /**
         * Returns the item position of the stack that contains the given item (compared by
         * pointers). Returns INT_MIN if the item is not found.
         * Note that this may lose some information, for example the returned position is the
         * same when the given item points to the container and when it points to the item inside
         * the container. All items that are part of the same stack have the same item position.
         */
        int position_by_item( const item *it ) const;
        int position_by_type( const itype_id &type ) const;

        /** Return the item position of the item with given invlet, return INT_MIN if
         * the inventory does not have such an item with that invlet. Don't use this on npcs inventory. */
        int invlet_to_position( char invlet ) const;

        bool has_tools( const itype_id &it, int quantity,
                        const std::function<bool( const item & )> &filter = return_true<item> ) const;
        bool has_components( const itype_id &it, int quantity,
                             const std::function<bool( const item & )> &filter = return_true<item> ) const;
        bool has_charges( const itype_id &it, int quantity,
                          const std::function<bool( const item & )> &filter = return_true<item> ) const;

        int leak_level( const flag_id &flag ) const; // level of leaked bad stuff from items

        // NPC/AI functions
        int worst_item_value( npc *p ) const;
        bool has_enough_painkiller( int pain ) const;
        item *most_appropriate_painkiller( int pain );

        void rust_iron_items();

        units::mass weight() const;
        units::mass weight_without( const excluded_stacks &without ) const;
        units::volume volume() const;
        units::volume volume_without( const excluded_stacks &without ) const;

        // dumps contents into dest (does not delete contents)
        void dump( std::vector<item *> &dest );

        // vector rather than list because it's NOT an item stack
        // returns all items that need processing
        std::vector<item *> active_items();

        // Assigns an invlet if any remain.  If none do, will assign ` if force is
        // true, empty (invlet = 0) otherwise.
        void assign_empty_invlet( item &it, const Character &p, bool force = false );
        // Assigns the item with the given invlet, and updates the favorite invlet cache. Does not check for uniqueness
        void reassign_item( item &it, char invlet, bool remove_old = true );
        // Removes invalid invlets, and assigns new ones if assign_invlet is true. Does not update the invlet cache.
        void update_invlet( item &it, bool assign_invlet = true );

        void set_stack_favorite( int position, bool favorite );

        invlets_bitset allocated_invlets() const;

        /**
         * Returns visitable items binned by their itype.
         * May not contain items that wouldn't be visited by @ref visitable methods.
         */
        const itype_bin &get_binned_items() const;

        void update_invlet_cache_with_item( item &newit );
        // gets a singular enchantment that is an amalgamation of all items that have active enchantments
        enchantment get_active_enchantment_cache( const Character &owner ) const;

        int count_item( const itype_id &item_type ) const;

        void update_quality_cache();
        const std::map<quality_id, std::map<int, int>> &get_quality_cache() const;

        void build_items_type_cache();

    private:
        friend location_inventory;
        friend visitable<inventory>;
        friend temp_visitable<inventory>;
        friend location_visitable<location_inventory>;
        invlet_favorites invlet_cache;
        char find_usable_cached_invlet( const itype_id &item_type );

        std::list<std::vector<item *>> items;
        std::map<itype_id, std::list<std::vector<item *> *>> items_type_cache;
        std::map<quality_id, std::map<int, int>> quality_cache;

        bool items_type_cached = false;
        mutable bool binned = false;
        /**
         * Items binned by their type.
         * That is, item_bin["carrot"] is a list of pointers to all carrots in inventory.
         * `mutable` because this is a pure cache that doesn't affect the contained items.
         */
        mutable itype_bin binned_items;
};

class location_inventory : public location_visitable<location_inventory>
{
    private:
        std::unique_ptr<item_location> loc;
        inventory inv;

        friend location_visitable<location_inventory>;
        friend visitable<location_inventory>;

        template<bool IsCached>
        item &add_item_internal( detached_ptr<item> &&newit, bool keep_invlet, bool assign_invlet,
                                 bool should_stack );
    public:

        const_invslice const_slice() const;
        const std::vector<item *> &const_stack( int i ) const;
        size_t size() const;

        location_inventory( item_location *location );
        location_inventory( location_inventory && ) = delete;
        location_inventory( const location_inventory & ) = delete;
        location_inventory &operator=( location_inventory && ) noexcept;
        location_inventory &operator=( const location_inventory & ) = delete;

        ~location_inventory();

        std::map<char, itype_id> assigned_invlet;

        void unsort();
        void clear();

        void add_items( std::vector<detached_ptr<item>> &newits, bool keep_invlet,
                        bool assign_invlet = true, bool should_stack = true );
        // returns a reference to the added item
        item &add_item( detached_ptr<item> &&newit, bool keep_invlet, bool assign_invlet = true,
                        bool should_stack = true );
        // use item type cache to speed up, remember to run build_items_type_cache() before using it
        item &add_item_by_items_type_cache( detached_ptr<item> &&newit, bool keep_invlet,
                                            bool assign_invlet = true, bool should_stack = true );

        /* Check all items for proper stacking, rearranging as needed
         * game pointer is not necessary, but if supplied, will ensure no overlap with
         * the player's worn items / weapon
         */
        void restack( player &p );
        /**
         * Remove a specific item from the inventory. The item is compared
         * by pointer. Contents of the item are removed as well.
         * @param it A pointer to the item to be removed. The item *must* exists
         * in this inventory.
         * @return A copy of the removed item.
         */
        detached_ptr<item> remove_item( item *it );
        detached_ptr<item> remove_item( int position );
        /**
         * Randomly select items until the volume quota is filled.
         */
        std::vector<detached_ptr<item>> remove_randomly_by_volume( const units::volume &volume );
        std::vector<detached_ptr<item>> reduce_stack( int position, int quantity );

        const item &find_item( int position ) const;
        item &find_item( int position );

        /**
         * Returns the item position of the stack that contains the given item (compared by
         * pointers). Returns INT_MIN if the item is not found.
         * Note that this may lose some information, for example the returned position is the
         * same when the given item points to the container and when it points to the item inside
         * the container. All items that are part of the same stack have the same item position.
         */
        int position_by_item( const item *it ) const;
        int position_by_type( const itype_id &type ) const;

        /** Return the item position of the item with given invlet, return INT_MIN if
         * the inventory does not have such an item with that invlet. Don't use this on npcs inventory. */
        int invlet_to_position( char invlet ) const;

        // Below, "amount" refers to quantity
        //        "charges" refers to charges
        std::vector<detached_ptr<item>> use_amount( itype_id it, int quantity,
                                     const std::function<bool( const item & )> &filter = return_true<item> );

        bool has_tools( const itype_id &it, int quantity,
                        const std::function<bool( const item & )> &filter = return_true<item> ) const;
        bool has_components( const itype_id &it, int quantity,
                             const std::function<bool( const item & )> &filter = return_true<item> ) const;
        bool has_charges( const itype_id &it, int quantity,
                          const std::function<bool( const item & )> &filter = return_true<item> ) const;

        int leak_level( const flag_id &flag ) const; // level of leaked bad stuff from items

        // NPC/AI functions
        int worst_item_value( npc *p ) const;
        bool has_enough_painkiller( int pain ) const;
        item *most_appropriate_painkiller( int pain );

        void rust_iron_items();

        units::mass weight() const;
        units::mass weight_without( const excluded_stacks &without ) const;
        units::volume volume() const;
        units::volume volume_without( const excluded_stacks &without ) const;

        // dumps contents into dest (does not delete contents)
        void dump( std::vector<item *> &dest );

        // dumps contents into dest (delete contents)
        void dump_remove( std::vector<detached_ptr<item>> &dest );
        std::vector<detached_ptr<item>> dump_remove();

        // vector rather than list because it's NOT an item stack
        // returns all items that need processing
        std::vector<item *> active_items();

        void json_load_invcache( JsonIn &jsin );
        void json_load_items( JsonIn &jsin );

        void json_save_invcache( JsonOut &json ) const;
        void json_save_items( JsonOut &json ) const;

        // Assigns an invlet if any remain.  If none do, will assign ` if force is
        // true, empty (invlet = 0) otherwise.
        void assign_empty_invlet( item &it, const Character &p, bool force = false );
        // Assigns the item with the given invlet, and updates the favorite invlet cache. Does not check for uniqueness
        void reassign_item( item &it, char invlet, bool remove_old = true );
        // Removes invalid invlets, and assigns new ones if assign_invlet is true. Does not update the invlet cache.
        void update_invlet( item &it, bool assign_invlet = true );

        void set_stack_favorite( int position, bool favorite );

        invlets_bitset allocated_invlets() const;

        /**
         * Returns visitable items binned by their itype.
         * May not contain items that wouldn't be visited by @ref visitable methods.
         */
        const itype_bin &get_binned_items() const;

        void update_invlet_cache_with_item( item &newit );

        // gets a singular enchantment that is an amalgamation of all items that have active enchantments
        enchantment get_active_enchantment_cache( const Character &owner ) const;

        int count_item( const itype_id &item_type ) const;

        void update_quality_cache();
        const std::map<quality_id, std::map<int, int>> &get_quality_cache() const;

        void build_items_type_cache();

        const inventory &as_inventory() const;
};


