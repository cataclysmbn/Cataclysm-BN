#include "replay/replay.h"

#include "json.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace replay {
namespace {

struct replay_state {
    mode active_mode = mode::none;
    std::string path;
    std::ofstream record_stream;
    std::vector<input_event> playback_events;
    std::size_t playback_index = 0;
};

auto state() -> replay_state&; // *NOPAD*

auto state() -> replay_state& // *NOPAD*
{
    static auto value = replay_state{};
    return value;
}

auto event_type_to_string(const input_event_t type) -> std::string {
    switch (type) {
        case input_event_t::keyboard:
            return "keyboard";
        case input_event_t::gamepad:
            return "gamepad";
        case input_event_t::mouse:
            return "mouse";
        case input_event_t::timeout:
            return "timeout";
        case input_event_t::error:
            return "error";
    }
    return "error";
}

auto event_type_from_string(const std::string& type) -> input_event_t {
    if (type == "keyboard") { return input_event_t::keyboard; }
    if (type == "gamepad") { return input_event_t::gamepad; }
    if (type == "mouse") { return input_event_t::mouse; }
    if (type == "timeout") { return input_event_t::timeout; }
    return input_event_t::error;
}

auto should_store_event(const input_event& event) -> bool {
    return event.type == input_event_t::keyboard || event.type == input_event_t::gamepad
        || event.type == input_event_t::mouse || event.type == input_event_t::timeout;
}

auto write_event(std::ostream& stream, const input_event& event) -> void {
    auto jsout = JsonOut(stream);
    jsout.start_object();
    jsout.member("type", event_type_to_string(event.type));
    jsout.member("sequence", event.sequence);
    jsout.member("modifiers", event.modifiers);
    jsout.member("mouse_pos");
    jsout.start_array();
    jsout.write(event.mouse_pos.x);
    jsout.write(event.mouse_pos.y);
    jsout.end_array();
    jsout.member("text", event.text);
    jsout.member("edit", event.edit);
    jsout.member("edit_refresh", event.edit_refresh);
    jsout.end_object();
}

auto read_event(const std::string& line) -> input_event {
    auto input = std::istringstream(line);
    auto jsin = JsonIn(input);
    const auto object = jsin.get_object();
    auto event = input_event{};
    event.type = event_type_from_string(object.get_string("type"));
    event.sequence = object.get_int_array("sequence");
    event.modifiers = object.get_int_array("modifiers");
    const auto mouse_pos = object.get_int_array("mouse_pos");
    if (mouse_pos.size() == 2) { event.mouse_pos = point(mouse_pos[0], mouse_pos[1]); }
    event.text = object.get_string("text", "");
    event.edit = object.get_string("edit", "");
    event.edit_refresh = object.get_bool("edit_refresh", false);
    return event;
}

} // namespace

auto configure_recording(const std::string& path) -> void {
    auto& current = state();
    if (current.active_mode != mode::none) {
        throw std::runtime_error("Only one replay mode can be configured");
    }
    current.active_mode = mode::record;
    current.path = path;
}

auto configure_playback(const std::string& path) -> void {
    auto& current = state();
    if (current.active_mode != mode::none) {
        throw std::runtime_error("Only one replay mode can be configured");
    }
    current.active_mode = mode::playback;
    current.path = path;
}

auto configured_mode() -> mode { return state().active_mode; }

auto is_enabled() -> bool { return configured_mode() != mode::none; }

auto is_recording() -> bool { return configured_mode() == mode::record; }

auto is_playing() -> bool { return configured_mode() == mode::playback; }

auto start() -> void {
    auto& current = state();
    if (current.active_mode == mode::none) { return; }
    if (current.path.empty()) { throw std::runtime_error("Replay path must not be empty"); }
    if (current.active_mode == mode::record) {
        current.record_stream
            .open(current.path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!current.record_stream.good()) {
            throw std::runtime_error("Could not open replay record file: " + current.path);
        }
        return;
    }

    auto input = std::ifstream(current.path, std::ios::in | std::ios::binary);
    if (!input.good()) {
        throw std::runtime_error("Could not open replay playback file: " + current.path);
    }
    current.playback_events.clear();
    current.playback_index = 0;
    auto line = std::string{};
    while (std::getline(input, line)) {
        if (line.empty()) { continue; }
        current.playback_events.push_back(read_event(line));
    }
}

auto stop() -> void {
    auto& current = state();
    if (current.record_stream.is_open()) { current.record_stream.close(); }
    current.active_mode = mode::none;
    current.path.clear();
    current.playback_events.clear();
    current.playback_index = 0;
}

auto record_input_event(const input_event& event) -> void {
    auto& current = state();
    if (current.active_mode != mode::record || !should_store_event(event)) { return; }
    write_event(current.record_stream, event);
    current.record_stream << '\n';
    current.record_stream.flush();
}

auto next_input_event() -> std::optional<input_event> {
    auto& current = state();
    if (current.active_mode != mode::playback) { return std::nullopt; }
    if (current.playback_index >= current.playback_events.size()) {
        throw std::runtime_error("Replay playback exhausted input events");
    }
    return current.playback_events[current.playback_index++];
}

} // namespace replay
