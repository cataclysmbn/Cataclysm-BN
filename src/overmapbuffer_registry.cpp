#include "overmapbuffer_registry.h"

// Include the full definitions so unique_ptr destructors can be instantiated
// in this translation unit only.
#include "overmapbuffer.h"
#include "overmap.h"

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "thread_pool.h"

// ---------------------------------------------------------------------------
// Registry class — defined here, never exposed in a header.
// This prevents MSVC from eagerly validating the destructor chain
// (unique_ptr<overmapbuffer> → ~overmapbuffer → unique_ptr<overmap> → ~overmap)
// in translation units where overmap is an incomplete type.
//
// Design note: overmapbuffer_registry uses a function-local static singleton
// (see registry() below) while mapbuffer_registry uses a named global
// (MAPBUFFER_REGISTRY in mapbuffer_registry.cpp).  Both have well-defined
// static-storage-duration lifetimes and neither references the other at
// static-init time, so there is no static-initialization-order issue.
// The different patterns exist because overmapbuffer_registry must hide its
// class definition to avoid exposing overmap's incomplete destructor chain,
// whereas mapbuffer_registry is safe to expose as a named global in its header.
// ---------------------------------------------------------------------------

namespace
{

static constexpr const char *PRIMARY_DIMENSION_ID = "";

class overmapbuffer_registry
{
    public:
        overmapbuffer_registry() {
            // Eagerly create the primary dimension slot.
            buffers_.emplace( PRIMARY_DIMENSION_ID, std::make_unique<overmapbuffer>() );
        }
        ~overmapbuffer_registry() = default;

        // Non-copyable
        overmapbuffer_registry( const overmapbuffer_registry & ) = delete;
        overmapbuffer_registry &operator=( const overmapbuffer_registry & ) = delete;

        overmapbuffer &get( const std::string &dim_id ) {
            auto it = buffers_.find( dim_id );
            if( it == buffers_.end() ) {
                auto result = buffers_.emplace( dim_id, std::make_unique<overmapbuffer>() );
                it = result.first;
            }
            return *it->second;
        }

        bool has_any_loaded( const std::string &dim_id ) const {
            return buffers_.count( dim_id ) > 0;
        }

        void unload_dimension( const std::string &dim_id ) {
            buffers_.erase( dim_id );
        }

        void for_each( const std::function<void( const std::string &, overmapbuffer & )> &fn ) {
            for( auto &kv : buffers_ ) {
                fn( kv.first, *kv.second );
            }
        }

        overmapbuffer &primary() {
            return get( PRIMARY_DIMENSION_ID );
        }

    private:
        std::map<std::string, std::unique_ptr<overmapbuffer>> buffers_;
};

overmapbuffer_registry &registry()
{
    static overmapbuffer_registry instance;
    return instance;
}

} // namespace

// ---------------------------------------------------------------------------
// Free function implementations
// ---------------------------------------------------------------------------

overmapbuffer &get_overmapbuffer( const std::string &dim_id )
{
    return registry().get( dim_id );
}

bool has_any_overmapbuffer( const std::string &dim_id )
{
    return registry().has_any_loaded( dim_id );
}

void unload_overmapbuffer_dimension( const std::string &dim_id )
{
    registry().unload_dimension( dim_id );
}

void for_each_overmapbuffer(
    const std::function<void( const std::string &, overmapbuffer & )> &fn )
{
    registry().for_each( fn );
}

overmapbuffer &get_primary_overmapbuffer()
{
    return registry().primary();
}

// ---------------------------------------------------------------------------
// Active dimension tracking — main-thread-only global.
// See overmapbuffer_registry.h for threading constraints.
// ---------------------------------------------------------------------------

std::string g_active_dimension_id;  // default "" = overworld (primary)

overmapbuffer &get_active_overmapbuffer()
{
    return registry().get( g_active_dimension_id );
}

auto save_all_overmapbuffers() -> void
{
    // Thread-safety audit (§7.4, task 30):
    // Each dimension's overmapbuffer writes to a distinct subdirectory:
    //   overworld  →  <world>/o_X.Y.ovr  (legacy path, no subdirectory)
    //   other dims →  <world>/dimensions/<dim_id>/o_X.Y.ovr
    // No two dimensions share a file path, so concurrent saves have zero
    // file-level contention.  overmap::save(dim_id) uses the dim-aware
    // world::write_overmap(dim_id, ...) overload — no global state is read
    // at worker-thread execution time.
    //
    // The overmapbuffer::save(dim_id) call holds a read-lock on the buffer's
    // internal shared_mutex while iterating overmaps.  Multiple dimensions
    // each hold their own buffer's lock, so there is no cross-dimension
    // mutex contention.
    std::vector<std::future<void>> futures;
    futures.reserve( 8 );  // most games have at most a handful of dimensions

    for_each_overmapbuffer( [&futures]( const std::string & dim_id, overmapbuffer & buf ) {
        futures.push_back( get_thread_pool().submit_returning(
        [dim_id, &buf]() {
            buf.save( dim_id );
        } ) );
    } );

    std::ranges::for_each( futures, []( auto &f ) {
        f.get();
    } );
}
