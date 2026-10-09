#include "client_backend.h"
#include "client_display.h"

#include <stdexcept>
#include <utility>

namespace game_client
{
namespace
{

auto backend_factories() -> std::unordered_map<client_kind, backend_factory> &
{
    static auto result = std::unordered_map<client_kind, backend_factory> {};
    return result;
}

auto selected_backend() -> backend_ptr &
{
    static auto result = backend_ptr{};
    return result;
}

} // namespace

auto register_backend( const client_kind kind, backend_factory factory ) -> void
{
    backend_factories().insert_or_assign( kind, std::move( factory ) );
}

auto set_active_backend( backend_ptr client ) -> void
{
    if( !client ) { throw std::invalid_argument( "cannot select an empty client backend" ); }
    if( selected_backend() ) { throw std::logic_error( "client backend has already been selected" ); }
    selected_backend() = std::move( client );
}

auto active_backend() -> backend& // *NOPAD*
{
    if( !selected_backend() ) { throw std::logic_error( "client backend has not been selected" ); }
    return *selected_backend();
}

auto backend_selected() -> bool { return static_cast<bool>( selected_backend() ); }

auto has_tiles() -> bool { return backend_selected() && active_backend().capabilities().tiles; }

auto create_backend( const client_kind kind ) -> backend_ptr
{
    const auto factory = backend_factories().find( kind );
    if( factory == backend_factories().end() || !factory->second ) {
        throw std::invalid_argument( "requested client backend is not registered" );
    }
    return factory->second();
}

auto parse_client_kind( const std::string &name ) -> client_kind
{
    if( name == "tiles" ) { return client_kind::tiles; }
    if( name == "curses" ) { return client_kind::curses; }
    if( name == "mcp" ) { return client_kind::mcp; }
    if( name == "imgui" ) { return client_kind::imgui; }
    throw std::invalid_argument( "unknown client backend: " + name );
}

} // namespace game_client
