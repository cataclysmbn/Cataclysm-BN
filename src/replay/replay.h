#pragma once

#include "input.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace replay {

inline constexpr auto format_version = std::uint32_t{1};

enum class mode {
    none,
    record,
    playback,
};

/// Metadata describing the input boundary at which an event was observed.
struct input_boundary_metadata {
    std::string context;
    std::vector<std::string> actions;
    int timeout_ms = -1;
    /// Unit of mouse coordinates ("pixel" or "cell"); mouse events replay only in the same unit.
    std::string pointer_space;

    auto operator<=>(const input_boundary_metadata&) const = default; // *NOPAD*
};

/// Signals the clean end marker, not a truncated recording or invalid event.
class completed: public std::runtime_error {
public:
    completed(): std::runtime_error("Replay completed") {}
};

/// Replay header metadata.
struct session_metadata {
    std::uint32_t rng_seed = 0;
};

auto configure_recording(const std::string& path, session_metadata metadata) -> void;
auto configure_playback(const std::string& path) -> void;
auto playback_metadata() -> const session_metadata&; // *NOPAD*
auto configured_mode() -> mode;
auto is_enabled() -> bool;
auto is_recording() -> bool;
auto is_playing() -> bool;
auto start() -> void;
/// Flush and close a recording, or end playback. Throws if finalization fails.
auto finish() -> void;
/// Best-effort cleanup for shutdown paths; use finish() when errors matter.
auto stop() -> void;
/// stop(), and delete the recording start() created, for a launch that fails after start().
auto discard() -> void;
auto record_input_event(const input_event& event, const input_boundary_metadata& boundary = {})
    -> void;
auto next_input_event(const input_boundary_metadata& expected_boundary = {})
    -> std::optional<input_event>;
/// Text the game reads from outside the input stream (clipboard, file). Recording stores what
/// `read` returns; playback returns the stored text and never calls `read`.
auto external_text(const std::string& source, const std::function<auto()->std::string>& read)
    -> std::string;
/// `external_text` for game code, which has no way to recover from a broken replay.
auto external_text_or_exit(
    const std::string& source, const std::function<auto()->std::string>& read) -> std::string;
#if defined(TILES)
/// The text on the system clipboard, empty when it holds none.
auto clipboard_text() -> std::string;
#endif
/// Reports a replay failure and ends the game with exit code 1.
[[noreturn]] auto exit_with_failure(const std::runtime_error& error) -> void;

} // namespace replay
