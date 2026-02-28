#include "mapbuffer_registry.h"

#include <vector>

#include "mapbuffer.h"
#include "thread_pool.h"

mapbuffer_registry MAPBUFFER_REGISTRY;

mapbuffer_registry::mapbuffer_registry()
{
    // Eagerly create the primary dimension slot so that code which holds
    // references/pointers to MAPBUFFER.primary() never observes a dangling state.
    buffers_.emplace( PRIMARY_DIMENSION_ID, std::make_unique<mapbuffer>() );
}

mapbuffer &mapbuffer_registry::get( const std::string &dim_id )
{
    auto it = buffers_.find( dim_id );
    if( it == buffers_.end() ) {
        auto result = buffers_.emplace( dim_id, std::make_unique<mapbuffer>() );
        it = result.first;
    }
    return *it->second;
}

bool mapbuffer_registry::is_registered( const std::string &dim_id ) const
{
    return buffers_.count( dim_id ) > 0;
}

bool mapbuffer_registry::has_any_loaded( const std::string &dim_id ) const
{
    const auto it = buffers_.find( dim_id );
    if( it == buffers_.end() ) {
        return false;
    }
    return !it->second->is_empty();
}

void mapbuffer_registry::unload_dimension( const std::string &dim_id )
{
    buffers_.erase( dim_id );
}

void mapbuffer_registry::for_each(
    const std::function<void( const std::string &, mapbuffer & )> &fn )
{
    for( auto &kv : buffers_ ) {
        fn( kv.first, *kv.second );
    }
}

mapbuffer &mapbuffer_registry::primary()
{
    return get( PRIMARY_DIMENSION_ID );
}

std::vector<std::string> mapbuffer_registry::active_dimension_ids() const
{
    std::vector<std::string> ids;
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
    const std::vector<std::string> dim_ids = active_dimension_ids();

    // Dispatch all dimension saves concurrently. Each buffer's save() is internally
    // parallelised over OMT quads, so this gives a second level of parallelism when
    // multiple dimensions are loaded.  show_progress=false suppresses UI popup calls
    // that are not safe off the main thread.
    parallel_for( 0, static_cast<int>( dim_ids.size() ), [&]( int i ) {
        const std::string &dim_id = dim_ids[i];
        const bool is_primary = ( dim_id == PRIMARY_DIMENSION_ID );
        // notify_tracker only for primary; show_progress=false (worker thread).
        buffers_.at( dim_id )->save( delete_after_save, is_primary, /*show_progress=*/false );
    } );
}
