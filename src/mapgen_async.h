#pragma once

#include <string>

#include "calendar.h"
#include "coordinates.h"

/**
 * Deferred Lua mapgen post-process hook.
 *
 * When map::generate() runs on a worker thread, the Lua
 * on_mapgen_postprocess hook cannot execute in place (Lua is not
 * thread-safe).  Instead, the hook parameters are pushed here and
 * drained on the main thread by map::shift() after drain_completed().
 *
 * The submaps are already in the mapbuffer when the hook runs (saven()
 * is called before the hook), so the main thread reconstructs a
 * temporary tinymap from the mapbuffer to give the hook a live map
 * reference identical in content to what it would have received on the
 * main thread.
 */
struct deferred_mapgen_hook {
    std::string       dim;
    tripoint_abs_omt  omt_pos;
    time_point        when;
};

/** Push a hook entry from a worker thread (thread-safe). */
void push_deferred_mapgen_hook( deferred_mapgen_hook h );

/**
 * Drain all deferred hooks and run each one on the main thread.
 *
 * For each entry, loads the already-saved submaps from the mapbuffer into a
 * temporary tinymap and calls run_on_mapgen_postprocess_hooks().  Modifications
 * from the hook land directly on the mapbuffer-owned submap objects.
 *
 * Must be called on the main thread only (Lua is not thread-safe).
 * Called by map::shift() after drain_completed().
 */
void run_deferred_mapgen_hooks();
