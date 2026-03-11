#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "memory_fast.h"
#include "point.h"
#include "submap_load_manager.h"
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
        /// Calculate total power generation (W) and consumption (W) in the grid,
        /// including any grids linked via grid_link_tile portals.
        auto get_power_stat() const -> power_stat;

        /**
         * Apply a net power delta (in watt-turns) to this grid's batteries.
         * Positive @p delta_w charges, negative discharges.  Clamps to int
         * before forwarding to mod_resource() to handle large batch values.
         */
        void apply_net_power( int64_t delta_w );

    private:
        /// Local-only stat (no portal chaining). Used internally by get_power_stat().
        auto get_power_stat_local() const -> power_stat;
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
 * One end of an active power-portal link between two electrical grids.
 *
 * Each end of the link holds a node in its dimension's distribution_grid_tracker.
 * Power equalisation and upkeep are handled by game::tick_portal_links() each turn.
 * The node is registered when the grid_link_tile active tile is loaded and removed
 * when it is unloaded or the link is destroyed.
 */
struct cross_dimension_export_node {
    /// Anchor tile in this (source) dimension.
    tripoint_abs_ms source_pos;
    /// Dimension of the far-end anchor ("" = primary dimension).
    std::string     target_dim_id;
    /// Absolute position of the far-end anchor tile.
    tripoint_abs_ms target_pos;
    /// True when the link is paused (power failure or manual).
    /// No power is transferred while paused; upkeep is not charged.
    bool            paused = false;
    /// submap_load_manager handle keeping the far end's submap resident.
    /// 0 when paused or not yet registered.
    load_request_handle far_load_handle = 0;
    /// submap_load_manager handle keeping the LOCAL source submap resident.
    /// Without this, the source submap would unload when the player leaves,
    /// which triggers on_submap_unloaded → remove_export_node → releases the
    /// far_load_handle → far end unloads → mutual collapse after 1 turn.
    load_request_handle local_load_handle = 0;
    /// Last time upkeep was charged on this node.  Upkeep is charged every
    /// PORTAL_UPKEEP_INTERVAL to match the timescale of grid power production.
    time_point last_upkeep = calendar::turn_zero;
};

/**
 * Contains and manages all the active distribution grids.
 *
 * Implements submap_load_listener to receive per-submap load/unload events.
 * Registered with submap_load_manager so that grids are built incrementally
 * as submaps enter and leave memory rather than on every map shift.
 */
class distribution_grid_tracker : public submap_load_listener
{
    private:
        /**
         * Mapping of sm position to grid it belongs to.
         */
        std::map<tripoint_abs_sm, shared_ptr_fast<distribution_grid>> parent_distribution_grids;

        /**
         * @param sm_pos Absolute submap position of one of the tiles of the grid.
         */
        distribution_grid &make_distribution_grid_at( const tripoint_abs_sm &sm_pos );

        /**
         * Set of submap positions currently tracked by this instance.
         * Populated by on_submap_loaded() / on_submap_unloaded() events.
         * Replaces the old half_open_rectangle<point_abs_sm> bounds member.
         */
        std::unordered_set<tripoint_abs_sm> tracked_submaps_;

        /**
         * OMTs marked dirty by on_changed() during the current tick.
         * Rebuilt in batch by flush_dirty_omts() at the start of update(),
         * so multiple on_changed() calls on the same OMT cluster within one
         * tick only trigger one make_distribution_grid_at() call per OMT.
         */
        std::unordered_set<tripoint_abs_omt> dirty_omts_;

        /**
         * Rebuild the distribution grid for every OMT accumulated in dirty_omts_
         * since the last flush, then clear dirty_omts_.
         * Called at the start of update() so that the power tick always sees
         * an up-to-date grid topology.
         */
        void flush_dirty_omts();

        mapbuffer &mb;

        grid_furn_transform_queue transform_queue;

        /**
         * Most grids are empty or idle, this contains the rest.
         */
        std::unordered_set<shared_ptr_fast<distribution_grid>> grids_requiring_updates;

        /**
         * The dimension this tracker belongs to (matches the key in game::grid_trackers_).
         * "" means the primary/overworld dimension.
         * Set at construction time and never changes.
         */
        std::string dimension_id_;

        /**
         * Stub cross-dimension export nodes installed on this tracker.
         * Each node represents one end of a dimensional electrical cable.
         */
        std::vector<cross_dimension_export_node> export_nodes_;

        /**
         * Returns the 4 submap positions that make up the given OMT.
         * An OMT at omt_pos contains submaps at:
         *   project_to<coords::sm>(omt_pos) + (0,0), (1,0), (0,1), (1,1)
         */
        static std::array<tripoint_abs_sm, 4> get_submaps_for_omt( tripoint_abs_omt omt_pos );

        /**
         * Returns the OMT itself and its 4 cardinal neighbors (5 total).
         * Diagonal neighbors are excluded: electrical connections run along cardinal axes only.
         */
        static std::array<tripoint_abs_omt, 5> get_omt_and_cardinal_neighbors( tripoint_abs_omt omt_pos );

    public:
        distribution_grid_tracker();
        distribution_grid_tracker( mapbuffer &buffer, std::string dim_id = {} );
        distribution_grid_tracker( distribution_grid_tracker && ) = default;
        ~distribution_grid_tracker();

        /** The dimension this tracker serves ("" = overworld). */
        auto get_dimension_id() const -> const std::string & { // *NOPAD*
            return dimension_id_;
        }

        /**
         * Register a cross-dimension cable endpoint on this tracker.
         * Registers a submap_load_manager request to keep the far end resident.
         * When @p register_reverse is true (default), also ensures the remote
         * tracker exists and has the matching reverse node.
         */
        void add_export_node( cross_dimension_export_node node, bool register_reverse = true );

        /**
         * Remove the cross-dimension cable whose source tile is @p source_pos.
         * Releases the associated load request.
         */
        void remove_export_node( const tripoint_abs_ms &source_pos );

        /**
         * Pause the export node at @p source_pos (e.g. power failure).
         * Releases the far-end load request; the remote submap may unload.
         */
        void pause_export_node( const tripoint_abs_ms &source_pos );

        /**
         * Resume a previously-paused export node.
         * Re-registers the far-end load request.
         */
        void resume_export_node( const tripoint_abs_ms &source_pos );

        /**
         * Return the export nodes on this tracker (read-only).
         * Used for save/load and for cross-dimension grid resolution.
         */
        auto get_export_nodes() const -> const std::vector<cross_dimension_export_node> & { // *NOPAD*
            return export_nodes_;
        }
        auto get_export_nodes_mut() -> std::vector<cross_dimension_export_node> & { // *NOPAD*
            return export_nodes_;
        }
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
         * submap_load_listener overrides.
         * Called when a submap at @p pos in dimension @p dim_id becomes resident.
         * Inserts the position into tracked_submaps_ and builds the grid cluster.
         */
        void on_submap_loaded( const tripoint_abs_sm &pos,
                               const std::string &dim_id ) override;

        /**
         * Called just before the submap at @p pos in dimension @p dim_id is evicted.
         * Removes from tracked_submaps_ and invalidates all 4 submaps of the affected OMT.
         */
        void on_submap_unloaded( const tripoint_abs_sm &pos,
                                 const std::string &dim_id ) override;

        /**
         * Updates grid at given global map square coordinate.
         * Only rebuilds grids in the 5-OMT cluster affected by the change.
         */
        void on_changed( const tripoint_abs_ms &p );
        void on_options_changed();

        /**
         * Clears all grids and tracked submaps. Used when changing dimensions.
         */
        void clear();

        /**
         * Returns true if this tracker has at least one tracked submap.
         * Used by game to decide when a non-primary dimension's tracker can be destroyed.
         */
        auto has_tracked_submaps() const -> bool {
            return !tracked_submaps_.empty() || !export_nodes_.empty();
        }
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

/**
 * Returns the distribution grid tracker for the given dimension, or nullptr if
 * no tracker exists for @p dim_id.  Used by portal-link code that needs to access
 * a tracker for a dimension other than the player's current one.
 */
distribution_grid_tracker *get_distribution_grid_tracker_for( const std::string &dim_id );

/**
 * Returns the distribution grid tracker for @p dim_id, creating it and
 * registering it with the submap_load_manager if it doesn't already exist.
 * Used by add_export_node() to guarantee the remote tracker is available.
 */
distribution_grid_tracker &ensure_distribution_grid_tracker_for( const std::string &dim_id );

