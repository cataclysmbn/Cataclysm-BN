#include "mapgen_async.h"

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

} // namespace

void push_deferred_mapgen_hook( deferred_mapgen_hook h )
{
    std::lock_guard<std::mutex> lk( g_hook_mutex );
    g_hooks.push_back( std::move( h ) );
}

void run_deferred_mapgen_hooks()
{
    // Drain under the lock, then process without holding it.
    std::vector<deferred_mapgen_hook> pending;
    {
        std::lock_guard<std::mutex> lk( g_hook_mutex );
        pending.swap( g_hooks );
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
