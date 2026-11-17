#pragma once

#include <string>

namespace replay {

/// Open a new state log and write its header; refuses to overwrite an existing file.
auto configure_state_hash_log(const std::string& path) -> void;
/// Append the hashes of the current game state to the log, if one is configured: one record
/// `{"kind":"state","step":N,"turn":T,"parts":{...}}`. It hashes what gameplay owns: the clock, the
/// RNG, the avatar, monsters, NPCs and the map around the avatar. Nothing a client can see without
/// owning it (the sidebar, animations) is hashed.
auto log_state_hash() -> void;
/// Close the log; a later configure starts a new one.
auto stop_state_hash_log() -> void;

} // namespace replay
