#include "mapbuffer_registry.h"

#include <vector>

#include "mapbuffer.h"
#include "overmapbuffer_registry.h" // g_active_dimension_id
#include "thread_pool.h"

mapbuffer_registry MAPBUFFER_REGISTRY;

mapbuffer_registry::mapbuffer_registry()
{
    // Eagerly create the primary dimension slot so that code which holds
    // references/pointers to MAPBUFFER.primary() never observes a dangling state.
    auto &primary = *buffers_.emplace( primary_dimension_id(),
                                       std::make_unique<mapbuffer>() ).first->second;
    primary.set_dimension_id( primary_dimension_id() );
}

auto mapbuffer_registry::get( const dimension_id &dim_id ) -> mapbuffer &
{
    auto it = buffers_.find( dim_id );
    if( it == buffers_.end() ) {
        auto result = buffers_.emplace( dim_id, std::make_unique<mapbuffer>() );
        result.first->second->set_dimension_id( dim_id );
        it = result.first;
        ++generation_;
    }
    return *it->second;
}

auto mapbuffer_registry::find( const dimension_id &dim_id ) -> mapbuffer *
{
    const auto it = buffers_.find( dim_id );
    return it == buffers_.end() ? nullptr : it->second.get();
}

auto mapbuffer_registry::find( const dimension_id &dim_id ) const -> const mapbuffer *
{
    const auto it = buffers_.find( dim_id );
    return it == buffers_.end() ? nullptr : it->second.get();
}

auto mapbuffer_registry::is_registered( const dimension_id &dim_id ) const -> bool
{
    return buffers_.count( dim_id ) > 0;
}

auto mapbuffer_registry::has_any_loaded( const dimension_id &dim_id ) const -> bool
{
    const auto it = buffers_.find( dim_id );
    if( it == buffers_.end() ) {
        return false;
    }
    return !it->second->is_empty();
}

auto mapbuffer_registry::unload_dimension( const dimension_id &dim_id ) -> void
{
    if( buffers_.erase( dim_id ) > 0 ) {
        ++generation_;
    }
}

auto mapbuffer_registry::for_each(
    const std::function<void( const dimension_id &, mapbuffer & )> &fn ) -> void
{
    for( auto &kv : buffers_ ) {
        fn( kv.first, *kv.second );
    }
}

auto mapbuffer_registry::primary() -> mapbuffer &
{
    return get( primary_dimension_id() );
}

auto mapbuffer_registry::active() -> mapbuffer &
{
    return get( g_active_dimension_id );
}

auto mapbuffer_registry::active_dimension_ids() const -> std::vector<dimension_id>
{
    std::vector<dimension_id> ids;
    ids.reserve( buffers_.size() );
    for( const auto &kv : buffers_ ) {
        ids.push_back( kv.first );
    }
    return ids;
}

void mapbuffer_registry::save_all( bool delete_after_save )
{
    // Snapshot dimension IDs before entering the parallel phase.
    // We must not iterate buffers_ while it could be mutated.
    const auto dim_ids = active_dimension_ids();

    // Dispatch all dimension saves concurrently. Each buffer's save() is internally
    // parallelised over OMTs, so this gives a second level of parallelism when
    // multiple dimensions are loaded.  show_progress=false suppresses UI popup calls
    // that are not safe off the main thread.
    parallel_for( 0, static_cast<int>( dim_ids.size() ), [&]( int i ) {
        const auto &dim_id = dim_ids[i];
        const auto is_primary = dim_id == primary_dimension_id();
        // notify_tracker only for primary; show_progress=false (worker thread).
        buffers_.at( dim_id )->save( delete_after_save, is_primary, /*show_progress=*/false );
    } );
}
