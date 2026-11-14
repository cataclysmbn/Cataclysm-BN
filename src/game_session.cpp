#include "game_session.h"
#include "engine_client_session.h"

namespace game_session
{
namespace
{

auto session_running = false;

} // namespace

auto set_running( const bool running ) -> void
{
    if( session_running != running ) {
        engine_client::process_session().replace_world();
    }
    session_running = running;
    set_phase( running ? "waiting_for_input" : "menu" );
}

auto set_phase( const char *phase ) -> void
{
    engine_client::process_session().set_phase( phase );
}

auto running() -> bool
{
    return session_running;
}

} // namespace game_session
