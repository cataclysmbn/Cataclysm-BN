#include "overmapbuffer_registry.h"

// Include the full definitions so unique_ptr destructors can be instantiated
// in this translation unit only.
#include "overmapbuffer.h"
#include "overmap.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

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
