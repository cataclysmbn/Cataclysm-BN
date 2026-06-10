#pragma once

#include <optional>
#include <string>

#include "input.h"

namespace replay
{

enum class mode {
    none,
    record,
    playback,
};

auto configure_recording( const std::string &path ) -> void;
auto configure_playback( const std::string &path ) -> void;
auto configured_mode() -> mode;
auto is_enabled() -> bool;
auto is_recording() -> bool;
auto is_playing() -> bool;
auto start() -> void;
auto stop() -> void;
auto record_input_event( const input_event &event ) -> void;
auto next_input_event() -> std::optional<input_event>;

} // namespace replay
