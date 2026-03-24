#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class mapbuffer;

/**
 * Registry managing one mapbuffer per dimension.
 *
 * Each dimension is identified by a string key.  The primary (default) dimension
 * uses PRIMARY_DIMENSION_ID (""), which is also accessible through the MAPBUFFER
 * macro for backwards compatibility.
 */
class mapbuffer_registry
{
    public:
        /// The dimension ID used for the primary/default world.
        static constexpr const char *PRIMARY_DIMENSION_ID = "";

        mapbuffer_registry();

        // Non-copyable
        mapbuffer_registry( const mapbuffer_registry & ) = delete;
        mapbuffer_registry &operator=( const mapbuffer_registry & ) = delete;

        /**
         * Return the mapbuffer for the given dimension, creating it if it does not
         * already exist.
         */
        mapbuffer &get( const std::string &dim_id );

        /**
         * Return true if a registry slot exists for the given dimension.
         * The slot may hold an empty mapbuffer; use has_any_loaded() to
         * check whether submaps are actually resident.
         */
        bool is_registered( const std::string &dim_id ) const;

        /**
         * Return true if the given dimension has at least one submap
         * currently resident in memory.
         */
        bool has_any_loaded( const std::string &dim_id ) const;

        /**
         * Remove and destroy the mapbuffer for the given dimension.
         * All submaps held in it are deleted.  Does nothing if the dimension
         * is not registered.
         */
        void unload_dimension( const std::string &dim_id );

        /**
         * Invoke @p fn for every registered dimension.
         * Callback signature: void( const std::string& dim_id, mapbuffer& buf )
         */
        void for_each( const std::function<void( const std::string &, mapbuffer & )> &fn );

        /** Convenience accessor: returns the primary dimension's mapbuffer. */
        mapbuffer &primary();

        /**
         * Return the mapbuffer for the currently active dimension
         * (g_active_dimension_id).
         *
         * **Rendering only** — must NOT be used for gameplay logic that needs
         * a specific dimension.  Gameplay code should use
         * MAPBUFFER_REGISTRY.get(dim_id) with an explicit dimension ID.
         */
        mapbuffer &active();

        /**
         * Save all registered dimensions in parallel.
         * All dimension saves are dispatched concurrently via parallel_for so that
         * independent file I/O (or SQLite writes) for different dimensions overlap.
         * on_submap_unloaded() is fired for evicted submaps only for the primary
         * dimension's tracker to avoid spurious updates from secondary-dimension
         * buffers.  Progress popups are suppressed in the parallel dispatch path
         * (UI calls are main-thread-only).
         */
        void save_all( bool delete_after_save = false );

        /**
         * Return a snapshot of all currently registered dimension IDs.
         * Used by save_all() to enumerate dimensions before the parallel phase
         * (iterating buffers_ while modifying it would be unsafe).
         */
        std::vector<std::string> active_dimension_ids() const;

    private:
        std::map<std::string, std::unique_ptr<mapbuffer>> buffers_;
};

extern mapbuffer_registry MAPBUFFER_REGISTRY;

// Backwards-compatibility macro — resolves to the primary dimension's mapbuffer.
// NOLINTNEXTLINE(cata-text-style)
#define MAPBUFFER ( MAPBUFFER_REGISTRY.primary() )

// Active-dimension macro — resolves to the currently active dimension's mapbuffer.
// *** RENDERING ONLY *** — must NOT be used for gameplay logic.
// Gameplay code should use MAPBUFFER_REGISTRY.get(dim_id) with an explicit dimension.
// NOLINTNEXTLINE(cata-text-style)
#define ACTIVE_MAPBUFFER ( MAPBUFFER_REGISTRY.active() )
