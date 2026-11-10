#pragma once

namespace game_session
{

/// Marks whether the game has completed world and avatar setup and is entering its turn loop.
auto set_running( bool running ) -> void;

/// Explicit process readiness transitions outside a stable input read.
auto set_phase( const char *phase ) -> void;

/// Returns true while a loaded world and initialized avatar are active.
auto running() -> bool;

} // namespace game_session
