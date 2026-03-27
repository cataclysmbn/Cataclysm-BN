#pragma once

#include <functional>
#include <string>

class overmapbuffer;

/**
 * Free-function API for the per-dimension overmapbuffer registry.
 *
 * The registry class itself is defined only in overmapbuffer_registry.cpp;
 * no header exposes std::unique_ptr<overmapbuffer>, which prevents MSVC from
 * eagerly instantiating the full destructor chain (overmapbuffer → overmap)
 * in translation units where overmap is an incomplete type.
 */

/** Return (or create) the overmapbuffer for the given dimension. */
overmapbuffer &get_overmapbuffer( const std::string &dim_id );

/** Return true if a registered overmapbuffer exists for the given dimension. */
bool has_any_overmapbuffer( const std::string &dim_id );

/** Remove and destroy the overmapbuffer for the given dimension. */
void unload_overmapbuffer_dimension( const std::string &dim_id );

/** Invoke @p fn for every registered dimension. */
void for_each_overmapbuffer(
    const std::function<void( const std::string &, overmapbuffer & )> &fn );

/** Save every registered overmapbuffer to disk. */
auto save_all_overmapbuffers() -> void;

/** Return the primary dimension's overmapbuffer. */
overmapbuffer &get_primary_overmapbuffer();

/**
 * The dimension ID of the dimension the player is currently in.
 * Updated by game::travel_to_dimension() on the main thread.
 *
 * THREADING: This is a main-thread-only global.  Background thread-pool tasks
 * must capture the dimension ID **by value** at submission time rather than
 * reading this variable at execution time — the active dimension can change
 * between task submission and task execution.  Use get_overmapbuffer(captured_id)
 * directly inside worker lambdas; never call get_active_overmapbuffer() from a
 * background thread.
 */
extern std::string g_active_dimension_id;

/**
 * Return the overmapbuffer for the player's current dimension.
 * Equivalent to get_overmapbuffer(g_active_dimension_id).
 *
 * Do NOT call this from background threads; see g_active_dimension_id comment above.
 */
overmapbuffer &get_active_overmapbuffer();

// Active-dimension macro — resolves to the currently active dimension's overmapbuffer.
// *** RENDERING / UI ONLY *** — must NOT be used for gameplay logic.
// Gameplay code should use get_overmapbuffer(dim_id) with an explicit dimension.
// NOLINTNEXTLINE(cata-text-style)
#define ACTIVE_OVERMAP_BUFFER ( get_active_overmapbuffer() )

