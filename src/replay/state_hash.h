#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace replay {

/// One hashed part of the game state.
struct state_part {
    std::string name;
    /// 16 lowercase hexadecimal digits.
    std::string digest;
};

/// The hashes of the game state after one `game::do_turn`, one per part.
struct state_hash {
    /// Calendar turns since the game began.
    int turn = 0;
    std::vector<state_part> parts;
};

/// Hash what gameplay owns: the clock, the RNG, the avatar, monsters, NPCs and the map around the
/// avatar. Nothing a client can see without owning it (the sidebar, animations) is hashed.
auto hash_game_state() -> state_hash;
/// One JSONL record of the state log: `{"kind":"state","step":N,"turn":T,"parts":{...}}`.
auto format_state_hash(std::size_t step, const state_hash& hash) -> std::string;
/// Open a new state log and write its header; refuses to overwrite an existing file.
auto configure_state_hash_log(const std::string& path) -> void;
/// Append the hash of the current game state to the log, if one is configured.
auto log_state_hash() -> void;
/// Close the log; a later configure starts a new one.
auto stop_state_hash_log() -> void;

} // namespace replay
