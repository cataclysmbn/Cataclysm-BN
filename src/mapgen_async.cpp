#include "mapgen_async.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>

#include "auto_note.h"
#include "catalua.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "init.h"
#include "map.h"
#include "map_extras.h"
#include "options.h"
#include "overmapbuffer.h"
#include "overmapbuffer_registry.h"
#include "point.h"
#include "string_formatter.h"
#include "string_id.h"

namespace
{

std::mutex                        g_hook_mutex;
std::vector<deferred_mapgen_hook> g_hooks;

std::mutex                      g_autonote_mutex;
std::vector<deferred_autonote>  g_autonotes;

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

void push_deferred_autonote( deferred_autonote entry )
{
    std::lock_guard<std::mutex> lk( g_autonote_mutex );
    g_autonotes.push_back( std::move( entry ) );
}

void run_deferred_autonotes()
{
    std::vector<deferred_autonote> pending;
    {
        std::lock_guard<std::mutex> lk( g_autonote_mutex );
        pending.swap( g_autonotes );
    }
    if( pending.empty() ) {
        return;
    }

    auto &settings = get_auto_notes_settings();
    const bool notes_active = get_option<bool>( "AUTO_NOTES" ) &&
                              get_option<bool>( "AUTO_NOTES_MAP_EXTRAS" );

    std::ranges::for_each( pending, [&]( const deferred_autonote & entry ) {
        const string_id<map_extra> extra_sid( entry.extra_id );
        settings.set_discovered( extra_sid );

        if( !notes_active || !settings.has_auto_note_enabled( extra_sid ) ) {
            return;
        }

        const map_extra &extra = extra_sid.obj();
        std::string sprite_prefix;
        if( extra.looks_like && !extra.looks_like->empty() ) {
            sprite_prefix = "SPRITE:" + *extra.looks_like + ";";
        }
        const std::string mx_note =
            sprite_prefix + string_format(
                "%s:%s;<color_yellow>%s</color>: <color_white>%s</color>",
                extra.get_symbol(),
                get_note_string_from_color( extra.color ),
                extra.name(),
                extra.description() );
        get_overmapbuffer( entry.dim ).add_note( entry.omt_pos, mx_note );
    } );
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
