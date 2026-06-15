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

// Registry class — defined here, never exposed in a header.
// Prevents MSVC from eagerly validating the destructor chain
// (unique_ptr<overmapbuffer> → ~overmapbuffer → unique_ptr<overmap> → ~overmap)
// in translation units where overmap is an incomplete type.

namespace
{

auto primary_dimension_id() -> dimension_id
{
    return dimension_id();
}

class overmapbuffer_registry
{
    public:
        overmapbuffer_registry() {
            // Eagerly create the primary dimension slot.
            auto primary = std::make_unique<overmapbuffer>();
            primary->set_dimension_id( primary_dimension_id() );
            buffers_.emplace( primary_dimension_id(), std::move( primary ) );
        }
        ~overmapbuffer_registry() = default;

        // Non-copyable
        overmapbuffer_registry( const overmapbuffer_registry & ) = delete;
        overmapbuffer_registry &operator=( const overmapbuffer_registry & ) = delete;

        auto get( const dimension_id &dim_id ) -> overmapbuffer & { // *NOPAD*
            auto it = buffers_.find( dim_id );
            if( it == buffers_.end() ) {
                auto buf = std::make_unique<overmapbuffer>();
                buf->set_dimension_id( dim_id );
                auto result = buffers_.emplace( dim_id, std::move( buf ) );
                it = result.first;
            }
            return *it->second;
        }

        auto has_any_loaded( const dimension_id &dim_id ) const -> bool {
            return buffers_.count( dim_id ) > 0;
        }

        auto unload_dimension( const dimension_id &dim_id ) -> void {
            buffers_.erase( dim_id );
        }

        auto for_each( const std::function<void( const dimension_id &, overmapbuffer & )> &fn )
        -> void {
            for( auto &kv : buffers_ ) {
                fn( kv.first, *kv.second );
            }
        }

        auto primary() -> overmapbuffer & { // *NOPAD*
            return get( primary_dimension_id() );
        }

    private:
        std::map<dimension_id, std::unique_ptr<overmapbuffer>> buffers_;
};

overmapbuffer_registry &registry()
{
    static overmapbuffer_registry instance;
    return instance;
}

} // namespace

auto get_overmapbuffer( const dimension_id &dim_id ) -> overmapbuffer &
{
    return registry().get( dim_id );
}

auto has_any_overmapbuffer( const dimension_id &dim_id ) -> bool
{
    return registry().has_any_loaded( dim_id );
}

auto unload_overmapbuffer_dimension( const dimension_id &dim_id ) -> void
{
    registry().unload_dimension( dim_id );
}

void for_each_overmapbuffer(
    const std::function<void( const dimension_id &, overmapbuffer & )> &fn )
{
    registry().for_each( fn );
}

auto get_primary_overmapbuffer() -> overmapbuffer &
{
    return registry().primary();
}

dimension_id g_active_dimension_id;  // default "" = overworld (primary)

auto get_active_overmapbuffer() -> overmapbuffer &
{
    return registry().get( g_active_dimension_id );
}

auto save_all_overmapbuffers() -> void
{
    // Each dimension writes to a distinct subdirectory, so concurrent saves
    // have no file-level contention.  Each overmapbuffer::save() holds its own
    // read-lock; no cross-dimension mutex contention.  No global state is read
    // at worker-thread execution time.
    std::vector<std::future<void>> futures;
    futures.reserve( 8 );  // most games have at most a handful of dimensions

    for_each_overmapbuffer( [&futures]( const dimension_id & dim_id, overmapbuffer & buf ) {
        futures.push_back( get_thread_pool().submit_returning(
        [dim_id, &buf]() {
            buf.save( dim_id );
        } ) );
    } );

    std::ranges::for_each( futures, []( auto & f ) {
        f.get();
    } );
}
