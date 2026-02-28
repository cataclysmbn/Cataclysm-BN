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

/** Return the primary dimension's overmapbuffer. */
overmapbuffer &get_primary_overmapbuffer();

// Backwards-compatibility macro: replaces the old `extern overmapbuffer overmap_buffer;`
// NOLINTNEXTLINE(cata-text-style)
#define overmap_buffer ( get_primary_overmapbuffer() )
