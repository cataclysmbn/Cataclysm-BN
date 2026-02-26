#include "mapbuffer_registry.h"

#include "mapbuffer.h"

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

void mapbuffer_registry::save_all( bool delete_after_save )
{
    for( auto &kv : buffers_ ) {
        // Only notify the distribution_grid_tracker for the primary dimension.
        // Secondary dimensions use a stub save (Phase 4 TODO) and should never
        // trigger a tracker rebuild based on primary-world global state.
        const bool is_primary = ( kv.first == PRIMARY_DIMENSION_ID );
        kv.second->save( delete_after_save, is_primary );
    }
}
