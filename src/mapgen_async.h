#pragma once

#include <string>

#include "calendar.h"
#include "coordinates.h"

namespace cata
{
struct lua_state;
} // namespace cata

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

/**
 * Deferred autonote for a map extra placed on a worker thread.
 *
 * apply_map_extra() skips auto_note_settings (not thread-safe) when called
 * from a pool worker.  The relevant data is pushed here and processed on the
 * main thread by run_deferred_autonotes(), which mirrors the note-placement
 * logic that would have run inline on the main thread.
 */
struct deferred_autonote {
    std::string       dim;
    tripoint_abs_omt  omt_pos;
    std::string       extra_id;   // raw string backing a string_id<map_extra>
};

/** Push a hook entry from a worker thread (thread-safe).
 *  No-op if no on_mapgen_postprocess hooks are registered — avoids queuing
 *  deferred entries that will be discarded anyway. */
void push_deferred_mapgen_hook( deferred_mapgen_hook h );

/**
 * Update the cached flag used by push_deferred_mapgen_hook() to decide
 * whether any on_mapgen_postprocess hooks are registered.
 *
 * Must be called on the main thread after all mod scripts have finished
 * loading (i.e. after init::load_main_lua_scripts).  Thread-safe write via
 * std::atomic — no Android fallback needed (plain bool, not atomic_ref).
 */
void refresh_mapgen_postprocess_hook_presence( cata::lua_state &state );

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

/** Returns true if any on_mapgen_postprocess Lua hooks are registered.
 *  Safe to call from any thread (relaxed atomic load). */
bool mapgen_hooks_registered();

/** Push an autonote entry from a worker thread (thread-safe). */
void push_deferred_autonote( deferred_autonote entry );

/**
 * Drain all deferred autonotes and process each on the main thread.
 *
 * Calls set_discovered() and, when AUTO_NOTES / AUTO_NOTES_MAP_EXTRAS are
 * enabled, places the overmap note — identical to what apply_map_extra()
 * would have done inline on the main thread.
 *
 * Must be called on the main thread only (auto_note_settings is not thread-safe).
 * Called by submap_load_manager::update() and map::shift().
 */
void run_deferred_autonotes();
