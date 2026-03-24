#include "mapgen_async.h"

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

#include "catalua.h"
#include "coordinate_conversions.h"
#include "init.h"
#include "map.h"
#include "point.h"

namespace
{

std::mutex                        g_hook_mutex;
std::vector<deferred_mapgen_hook> g_hooks;

// Cached flag: are any on_mapgen_postprocess hooks registered?
// Written on the main thread after mod load; read from worker threads.
// Plain std::atomic<bool> — no Android-specific fallback needed
// (the atomic_ref<float> workaround in shadowcasting.cpp does not apply here).
std::atomic<bool> g_has_mapgen_hooks{ false };

} // namespace

void push_deferred_mapgen_hook( deferred_mapgen_hook h )
{
    // Fast path: skip the lock and the queue entirely when no hooks exist.
    if( !g_has_mapgen_hooks.load( std::memory_order_relaxed ) ) {
        return;
    }
    std::lock_guard<std::mutex> lk( g_hook_mutex );
    g_hooks.push_back( std::move( h ) );
}

void refresh_mapgen_postprocess_hook_presence( cata::lua_state &state )
{
    g_has_mapgen_hooks.store(
        cata::has_mapgen_postprocess_hooks( state ),
        std::memory_order_relaxed );
}

void run_deferred_mapgen_hooks()
{
    // Drain under the lock, then process without holding it.
    std::vector<deferred_mapgen_hook> pending;
    {
        std::lock_guard<std::mutex> lk( g_hook_mutex );
        pending.swap( g_hooks );
    }

    // Skip the expensive tinymap construction when no hooks are registered.
    // Worker threads always push a deferred entry when generating on a worker
    // thread, regardless of hook registration.  Checking here (on the main
    // thread, where Lua access is safe) avoids building O(n) tinymaps per turn
    // just to call a no-op hook loop.
    if( !cata::has_mapgen_postprocess_hooks( *DynamicDataLoader::get_instance().lua ) ) {
        return;
    }

    for( auto &h : pending ) {
        // The submaps are already in the mapbuffer (saven() was called before
        // the hook was deferred).  Load them into a temporary tinymap so the
        // Lua hook receives a live map reference identical in content to what
        // it would have seen on the main thread.  Modifications to the tinymap
        // go directly to the mapbuffer-owned submap objects.
        const tripoint sm_base = omt_to_sm_copy( h.omt_pos.raw() );
        tinymap tmp;
        tmp.bind_dimension( h.dim );
        tmp.load_from_mapbuffer( sm_base );
        cata::run_on_mapgen_postprocess_hooks(
            *DynamicDataLoader::get_instance().lua,
            tmp,
            h.omt_pos.raw(),
            h.when
        );
    }
}
