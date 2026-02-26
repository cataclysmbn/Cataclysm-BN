#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <unordered_set>

#include "calendar.h"
#include "coordinates.h"
#include "cuboid_rectangle.h"
#include "memory_fast.h"
#include "point.h"
#include "type_id.h"

class Character;
class map;
class mapbuffer;

struct tile_location {
    point_sm_ms on_submap;
    tripoint_abs_ms absolute;

    tile_location( point_sm_ms on_submap, tripoint_abs_ms absolute )
        : on_submap( on_submap )
        , absolute( absolute )
    {}
};

struct power_stat {
    int64_t gen_w = 0; //< generation in watts
    int64_t use_w = 0; //< comsumption in watts
    auto operator+( const power_stat &other ) const -> power_stat { return { .gen_w = gen_w + other.gen_w, .use_w = use_w + other.use_w }; }
    auto net_w() const { return gen_w - use_w; }
};

/**
 * A cache that organizes producers, storage and consumers
 * of some resource, like electricity.
 * WARNING: Shouldn't be stored, as out of date grids are not updated.
 */
class distribution_grid
{
    private:
        friend class distribution_grid_tracker;

        /**
         * Map of submap coords to points on this submap
         * that contain an active tile.
         */
        std::map<tripoint_abs_sm, std::vector<tile_location>> contents;
        std::vector<tripoint_abs_ms> flat_contents;
        std::vector<tripoint_abs_sm> submap_coords;

        mutable std::optional<int> cached_amount_here;

        mapbuffer &mb;

    public:
        distribution_grid( const std::vector<tripoint_abs_sm> &global_submap_coords, mapbuffer &buffer );
        bool empty() const;
        explicit operator bool() const;
        void update( time_point to );
        int mod_resource( int amt, bool recurse = true );
        int get_resource( bool recurse = true ) const;
        const std::vector<tripoint_abs_ms> &get_contents() const {
            return flat_contents;
        }
        /// Calculate total power generation (W) and consumption (W) in the grid
        auto get_power_stat() const -> power_stat;
};

class distribution_grid_tracker;

struct transform_queue_entry {
    tripoint_abs_ms p;
    furn_str_id id;
    std::string msg;

    bool operator==( const transform_queue_entry &l ) const {
        return p == l.p && id == l.id && msg == l.msg;
    }
};

/**
 * Represents queued active tile / furniture transformations.
 *
 * Some active tiles can turn into other active tiles, or even inactive tiles, as a result
 * of an update. If such transformation is applied immediately, it could trigger recalculation of
 * the grid that's being updated, which would require additional code to handle.
 *
 * As a simpler alternative, we queue active tile transformations and apply them only after
 * all grids have been updated. The transformations are applied according to FIFO method,
 * so if some tile has multiple competing transforms queued, the last one will win out.
 */
class grid_furn_transform_queue
{
    private:
        std::vector<transform_queue_entry> queue;

    public:
        void add( const tripoint_abs_ms &p, const furn_str_id &id, const std::string &msg ) {
            queue.emplace_back( transform_queue_entry{ p, id, msg } );
        }

        void apply( mapbuffer &mb, distribution_grid_tracker &grid_tracker, Character &u, map &m );

        void clear() {
            queue.clear();
        }

        bool operator==( const grid_furn_transform_queue &l ) const {
            return queue == l.queue;
        }

        std::string to_string() const;
};

/**
 * Contains and manages all the active distribution grids.
 */
class distribution_grid_tracker
{
    private:
        /**
         * Mapping of sm position to grid it belongs to.
         */
        std::map<tripoint_abs_sm, shared_ptr_fast<distribution_grid>> parent_distribution_grids;

        /**
         * @param omt_pos Absolute submap position of one of the tiles of the grid.
         */
        distribution_grid &make_distribution_grid_at( const tripoint_abs_sm &sm_pos );

        /**
         * In submap coords, to mirror @ref map
         */
        half_open_rectangle<point_abs_sm> bounds;

        mapbuffer &mb;

        grid_furn_transform_queue transform_queue;

        /**
         * Most grids are empty or idle, this contains the rest.
         */
        std::unordered_set<shared_ptr_fast<distribution_grid>> grids_requiring_updates;

    public:
        distribution_grid_tracker();
        distribution_grid_tracker( mapbuffer &buffer );
        distribution_grid_tracker( distribution_grid_tracker && ) = default;
        /**
         * Gets grid at given global map square coordinate. @ref map::getabs
         */
        /**@{*/
        distribution_grid &grid_at( const tripoint_abs_ms &p );
        const distribution_grid &grid_at( const tripoint_abs_ms &p ) const;
        /*@}*/

        /**
         * Identify grid at given overmap tile (for debug purposes).
         * @returns 0 if there's no grid.
         */
        std::uintptr_t debug_grid_id( const tripoint_abs_omt &omp ) const;

        void update( time_point to );

        grid_furn_transform_queue &get_transform_queue() {
            return transform_queue;
        }

        /**
         * Loads grids in an area given by submap coords.
         */
        void load( half_open_rectangle<point_abs_sm> area );
        /**
         * Loads grids in the same area as a given map.
         */
        void load( const map &m );

        /**
         * Updates grid at given global map square coordinate.
         */
        void on_changed( const tripoint_abs_ms &p );
        void on_saved();
        void on_options_changed();
};

class vehicle;

namespace distribution_graph
{
enum class traverse_visitor_result {
    stop,
    continue_further,
};

/** Traverses the graph of connected vehicles and grids.
*
* Runs Breadth-First over all vehicles and grids calling passed actions on each of them
* until any visitor action return traverse_visitor_result::stop.
* Connected vehicles checked by all POWER_TRANSFER part and grids by vehicle connectors.
*
* @param start       Reference to the start node of the graph. Assumed to be already visited.
* @param veh_action  Visitor function which accepts vehicle& or const & then returns traverse_visitor_result.
* @param grid_action Visitor function which accepts distribution_grid& or const & then returns traverse_visitor_result.
*/
template <typename VehFunc, typename GridFunc, typename StartPoint>
void traverse( StartPoint &start,
               VehFunc veh_action, GridFunc grid_action );

/* Useful if we want to act only in one type. */
constexpr traverse_visitor_result noop_visitor_grid( const distribution_grid & )
{
    return traverse_visitor_result::continue_further;
}

/* Useful if we want to act only in one type. */
constexpr traverse_visitor_result noop_visitor_veh( const vehicle & )
{
    return traverse_visitor_result::continue_further;
}

} // namespace distribution_graph

/**
 * Returns distribution grid tracker that is a part of the global game *g. @ref game
 * TODO: This wouldn't be required in an ideal world
 */
distribution_grid_tracker &get_distribution_grid_tracker();


