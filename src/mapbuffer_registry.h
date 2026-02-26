#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

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
         * Save all registered dimensions.
         * The distribution_grid_tracker is notified (on_saved) only for the
         * primary dimension to avoid spurious rebuilds from secondary buffers.
         */
        void save_all( bool delete_after_save = false );

    private:
        std::map<std::string, std::unique_ptr<mapbuffer>> buffers_;
};

extern mapbuffer_registry MAPBUFFER_REGISTRY;

// Backwards-compatibility macro — all existing MAPBUFFER.foo() calls remain valid.
// Take care with &MAPBUFFER: it expands to &(MAPBUFFER_REGISTRY.primary()) which is
// legal since primary() returns an lvalue reference, but prefer
// MAPBUFFER_REGISTRY.primary() directly where the address is really needed.
// NOLINTNEXTLINE(cata-text-style)
#define MAPBUFFER ( MAPBUFFER_REGISTRY.primary() )
