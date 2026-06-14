#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "type_id.h"

class mapbuffer;

/**
 * Registry managing one mapbuffer per dimension.
 *
 * Each dimension is identified by a dimension_id.  The primary/default
 * dimension uses an empty dimension_id, which is also accessible through the
 * MAPBUFFER macro for backwards compatibility.
 */
class mapbuffer_registry
{
    public:
        /// The dimension ID used for the primary/default world.
        static auto primary_dimension_id() -> dimension_id {
            return dimension_id();
        }

        mapbuffer_registry();

        // Non-copyable
        mapbuffer_registry( const mapbuffer_registry & ) = delete;
        mapbuffer_registry &operator=( const mapbuffer_registry & ) = delete;

        /**
         * Return the mapbuffer for the given dimension, creating it if it does not
         * already exist.
         */
        auto get( const dimension_id &dim_id ) -> mapbuffer &;

        /**
         * Return the mapbuffer for the given dimension if a registry slot already
         * exists.  Does not create a slot or load any submaps.
         */
        auto find( const dimension_id &dim_id ) -> mapbuffer *;
        auto find( const dimension_id &dim_id ) const -> const mapbuffer *;

        /**
         * Return true if a registry slot exists for the given dimension.
         * The slot may hold an empty mapbuffer; use has_any_loaded() to
         * check whether submaps are actually resident.
         */
        auto is_registered( const dimension_id &dim_id ) const -> bool;

        /**
         * Return true if the given dimension has at least one submap
         * currently resident in memory.
         */
        auto has_any_loaded( const dimension_id &dim_id ) const -> bool;

        /**
         * Remove and destroy the mapbuffer for the given dimension.
         * All submaps held in it are deleted.  Does nothing if the dimension
         * is not registered.
         */
        auto unload_dimension( const dimension_id &dim_id ) -> void;

        /**
         * Invoke @p fn for every registered dimension.
         * Callback signature: void( const dimension_id& dim_id, mapbuffer& buf )
         */
        auto for_each( const std::function<void( const dimension_id &, mapbuffer & )> &fn ) -> void;

        /** Convenience accessor: returns the primary dimension's mapbuffer. */
        auto primary() -> mapbuffer &;

        /**
         * Return the mapbuffer for the currently active dimension
         * (g_active_dimension_id).
         *
         * **Rendering only** — must NOT be used for gameplay logic that needs
         * a specific dimension.  Gameplay code should use
         * MAPBUFFER_REGISTRY.get(dim_id) with an explicit dimension ID.
         */
        auto active() -> mapbuffer &;

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
        auto active_dimension_ids() const -> std::vector<dimension_id>;

        /**
         * Monotonic counter bumped whenever registry slots are created or erased.
         * Cached mapbuffer pointers can use this to detect possible invalidation
         * without doing a registry lookup on every access.
         */
        auto generation() const -> std::size_t {
            return generation_;
        }

    private:
        std::map<dimension_id, std::unique_ptr<mapbuffer>> buffers_;
        std::size_t generation_ = 0;
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
