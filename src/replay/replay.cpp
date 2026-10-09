#include "replay/replay.h"

#include "client_interaction.h"
#include "json.h"
#include "rng.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace replay {
namespace {

struct replay_event {
    input_event event;
    input_boundary_metadata boundary;
};

struct replay_text {
    std::string source;
    std::string text;
};

struct replay_state {
    mode active_mode = mode::none;
    bool started = false;
    bool ended = false;
    std::string path;
    session_metadata metadata;
    std::optional<session_metadata> expected_metadata;
    std::ofstream recording;
    std::ifstream playback;
    std::optional<replay_event> next;
    std::optional<replay_text> next_text;
    std::size_t line = 0;
    std::size_t events = 0;
};

auto state() -> replay_state& { // *NOPAD*
    static auto value = replay_state{};
    return value;
}

auto fail(const std::string& message) -> void {
    throw std::runtime_error(
        "Replay " + state().path + ": line " + std::to_string(state().line) + ": " + message);
}

constexpr auto presentation_actions = std::array<std::string_view, 9>{
    "zoom_out",
    "zoom_in",
    "toggle_fullscreen",
    "toggle_pixel_minimap",
    "toggle_zone_overlay",
    "reload_tileset",
    "debug_tileset",
    "TEXT.PASTE",
    "TOGGLE_CHARACTER_PREVIEW_CLOTHES",
};

auto is_presentation_action(const std::string& action) -> bool {
    return std::ranges::find(presentation_actions, action) != presentation_actions.end();
}

auto gameplay_actions(const std::vector<std::string>& actions) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};
    std::ranges::copy_if(actions, std::back_inserter(result), std::not_fn(is_presentation_action));
    return result;
}

auto append_action_list(std::ostringstream& output, const std::vector<std::string>& actions)
    -> void {
    auto separator = std::string_view{};
    for (const auto& action : actions) {
        output << separator << '\'' << action << '\'';
        separator = ", ";
    }
}

auto boundary_mismatch_message(
    const input_boundary_metadata& recorded, const input_boundary_metadata& actual,
    const std::size_t event) -> std::string {
    auto output = std::ostringstream{};
    output << "Input boundary mismatch at event " << event << ':';
    if (recorded.context != actual.context) {
        output << " context recorded='" << recorded.context << "', actual='" << actual.context
               << '\'';
    }
    if (recorded.timeout_ms != actual.timeout_ms) {
        output << " timeout_ms recorded=" << recorded.timeout_ms
               << ", actual=" << actual.timeout_ms;
    }
    const auto recorded_actions = gameplay_actions(recorded.actions);
    const auto actual_actions = gameplay_actions(actual.actions);
    if (recorded_actions != actual_actions) {
        auto missing = std::vector<std::string>{};
        auto unexpected = std::vector<std::string>{};
        std::ranges::copy_if(recorded_actions, std::back_inserter(missing), [&](const auto& action) {
            return std::ranges::find(actual_actions, action) == actual_actions.end();
        });
        std::ranges::
            copy_if(actual_actions, std::back_inserter(unexpected), [&](const auto& action) {
                return std::ranges::find(recorded_actions, action) == recorded_actions.end();
            });
        if (!missing.empty()) {
            output << " missing gameplay actions [";
            append_action_list(output, missing);
            output << ']';
        }
        if (!unexpected.empty()) {
            output << " unexpected gameplay actions [";
            append_action_list(output, unexpected);
            output << ']';
        }
        if (missing.empty() && unexpected.empty()) { output << " gameplay action order differs"; }
    }
    return output.str();
}

auto boundaries_match(
    const input_boundary_metadata& recorded, const input_boundary_metadata& actual) -> bool {
    const auto actions_match =
        gameplay_actions(recorded.actions) == gameplay_actions(actual.actions);
    const auto presentation_changed = actions_match && recorded.actions != actual.actions;
    const auto timeout_matches = recorded.timeout_ms == actual.timeout_ms || presentation_changed;
    return recorded.context == actual.context && timeout_matches && actions_match;
}

auto event_type_name(const input_event_t type) -> std::string {
    switch (type) {
        case input_event_t::keyboard:
            return "keyboard";
        case input_event_t::gamepad:
            return "gamepad";
        case input_event_t::mouse:
            return "mouse";
        case input_event_t::timeout:
            return "timeout";
        case input_event_t::interaction:
            return "interaction";
        case input_event_t::error:
            return "idle";
    }
    throw std::runtime_error("Error input events cannot be recorded");
}

auto read_type(const std::string& type) -> input_event_t {
    if (type == "keyboard") { return input_event_t::keyboard; }
    if (type == "gamepad") { return input_event_t::gamepad; }
    if (type == "mouse") { return input_event_t::mouse; }
    if (type == "timeout") { return input_event_t::timeout; }
    if (type == "interaction") { return input_event_t::interaction; }
    if (type == "idle") { return input_event_t::error; }
    throw std::runtime_error("Unknown input event type: " + type);
}

auto strict_int64(const JsonObject& object, const std::string& name) -> std::int64_t {
    auto* const value = object.get_raw(name);
    if (!value->test_number()) { object.throw_error(name + " must be an integer"); }
    const auto start = value->tell();
    value->skip_number();
    const auto spelling = value->substr(start, value->tell() - start);
    if (spelling.find_first_of(".eE") != std::string::npos) {
        object.throw_error(name + " must be an integer");
    }
    return object.get_member(name).get_int64();
}

auto strict_int(const JsonObject& object, const std::string& name) -> int {
    const auto value = strict_int64(object, name);
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        object.throw_error(name + " is outside the supported integer range");
    }
    return static_cast<int>(value);
}

auto strict_int(const JsonObject& object, const std::string& name, const int fallback) -> int {
    return object.has_member(name) ? strict_int(object, name) : fallback;
}

auto strict_integer_array_spelling(const std::string& text) -> bool {
    auto index = std::size_t{0};
    const auto skip_whitespace = [&]() {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
    };
    skip_whitespace();
    if (index == text.size() || text[index++] != '[') { return false; }
    skip_whitespace();
    if (index < text.size() && text[index] == ']') {
        ++index;
        skip_whitespace();
        return index == text.size();
    }
    while (index < text.size()) {
        if (text[index] == '-') { ++index; }
        if (index == text.size() || !std::isdigit(static_cast<unsigned char>(text[index]))) {
            return false;
        }
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        skip_whitespace();
        if (index < text.size() && text[index] == ',') {
            ++index;
            skip_whitespace();
            continue;
        }
        if (index < text.size() && text[index] == ']') {
            ++index;
            skip_whitespace();
            return index == text.size();
        }
        return false;
    }
    return false;
}

auto strict_int_array(const JsonObject& object, const std::string& name) -> std::vector<int> {
    auto array = object.get_array(name);
    auto spelling = array.str();
    const auto end = spelling.find(']');
    if (end == std::string::npos) { object.throw_error(name + " must be an array"); }
    spelling.resize(end + 1);
    if (!strict_integer_array_spelling(spelling)) {
        object.throw_error(name + " must contain only integers");
    }
    return object.get_int_array(name);
}

auto read_event(const JsonObject& object) -> replay_event {
    auto result = replay_event{};
    auto& event = result.event;
    event.type = read_type(object.get_string("type"));
    event.sequence = strict_int_array(object, "sequence");
    event.modifiers = strict_int_array(object, "modifiers");
    const auto position = strict_int_array(object, "mouse_pos");
    if (position.size() != 2) { throw std::runtime_error("mouse_pos requires two coordinates"); }
    event.mouse_pos = point(position[0], position[1]);
    event.text = object.get_string("text");
    event.edit = object.get_string("edit");
    event.edit_refresh = object.get_bool("edit_refresh");
    if (event.type == input_event_t::interaction) {
        auto semantic = object.get_object("interaction");
        const auto operation = game_client::parse_interaction_operation(
            semantic.get_string("operation"));
        if (!operation) { semantic.throw_error("Unknown semantic interaction operation"); }
        auto payload = game_client::interaction_event{
            .operation = *operation,
            .schema_id = semantic.get_string("schema_id"),
            .target_id = semantic.get_string("target_id", ""),
            .value = semantic.get_string("value", ""),
        };
        if (payload.schema_id.empty()) { semantic.throw_error("schema_id must not be empty"); }
        if (*operation == game_client::interaction_operation::fill) {
            if (!semantic.has_member("submit")) {
                semantic.throw_error("fill requires explicit submit");
            }
            payload.submit = semantic.get_bool("submit");
        } else if (*operation == game_client::interaction_operation::set_count) {
            const auto count = strict_int64(semantic, "count");
            if (count < 0) { semantic.throw_error("count must not be negative"); }
            payload.count = static_cast<std::uint64_t>(count);
        } else if (*operation == game_client::interaction_operation::set_target) {
            auto position = semantic.get_object("position");
            payload.position = game_client::interaction_position{
                .x = strict_int(position, "x"),
                .y = strict_int(position, "y"),
                .z = strict_int(position, "z"),
            };
            position.finish();
        }
        semantic.finish();
        const auto invalid_shape =
            (*operation == game_client::interaction_operation::choose
             && (payload.target_id.empty() || !payload.value.empty()))
            || (*operation == game_client::interaction_operation::fill && payload.target_id.empty())
            || (*operation == game_client::interaction_operation::set_count
                && (payload.target_id.empty() || !payload.value.empty()))
            || (*operation == game_client::interaction_operation::set_target
                && !payload.value.empty())
            || (*operation == game_client::interaction_operation::cancel
                && (!payload.target_id.empty() || !payload.value.empty()));
        if (invalid_shape) { throw std::runtime_error("Invalid semantic interaction payload"); }
        event.interaction = std::move(payload);
    }
#if defined(__ANDROID__)
    event.shortcut_last_used_action_counter =
        strict_int(object, "shortcut_last_used_action_counter", 0);
#endif
    auto boundary = object.get_object("boundary");
    result.boundary.context = boundary.get_string("context");
    result.boundary.timeout_ms = strict_int(boundary, "timeout_ms", -1);
    result.boundary.pointer_space = boundary.get_string("pointer_space", "");
    result.boundary.actions = boundary.get_string_array("actions");
    boundary.finish();
    return result;
}

/// Read a single record. No game state or RNG is touched by parsing.
auto read_record(const bool header) -> void {
    auto& current = state();
    auto line = std::string{};
    ++current.line;
    if (!std::getline(current.playback, line)) {
        fail(current.playback.bad() ? "Read failed" : "Unexpected EOF (missing end record)");
    }
    try {
        auto stream = std::istringstream(line);
        auto input = JsonIn(stream);
        auto object = input.get_object();
        const auto kind = object.get_string("kind");
        if (strict_int64(object, "version") != format_version) {
            throw std::runtime_error("Unsupported replay version");
        }
        if (header) {
            if (kind != "header" || object.get_string("format") != "cataclysm-bn-replay") {
                throw std::runtime_error("First record must be a cataclysm-bn-replay header");
            }
            const auto seed = strict_int64(object, "rng_seed");
            if (seed <= 0 || seed > std::numeric_limits<unsigned int>::max()) {
                throw std::runtime_error("RNG seed must be a nonzero 32-bit unsigned integer");
            }
            current.metadata = {.rng_seed = static_cast<unsigned int>(seed)};
        } else if (kind == "input") {
            current.next = read_event(object);
        } else if (kind == "text") {
            current.next_text = replay_text{
                .source = object.get_string("source"), .text = object.get_string("text")};
        } else if (kind == "end") {
            const auto count = strict_int64(object, "events");
            if (count < 0 || static_cast<std::uint64_t>(count) != current.events) {
                throw std::runtime_error("End record input count does not match playback");
            }
            if (current.playback.peek() != std::char_traits<char>::eof()) {
                throw std::runtime_error("Unexpected data after end record");
            }
            current.ended = true;
        } else {
            throw std::runtime_error("Expected input, text or end record");
        }
        object.finish();
        input.eat_whitespace();
        if (stream.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("Trailing data after JSON record");
        }
    } catch (const std::exception& error) { fail(error.what()); }
}

auto write_header() -> void {
    auto& current = state();
    auto json = JsonOut(current.recording);
    json.start_object();
    json.member("kind", "header");
    json.member("format", "cataclysm-bn-replay");
    json.member("version", format_version);
    json.member("rng_seed", current.metadata.rng_seed);
    json.end_object();
    current.recording << '\n' << std::flush;
}

auto flush_record() -> void {
    auto& current = state();
    current.recording << '\n' << std::flush;
    if (!current.recording) { throw std::runtime_error("Could not write replay: " + current.path); }
}

auto ensure_next() -> void {
    auto& current = state();
    if (!current.next && !current.next_text && !current.ended) { read_record(false); }
}

auto require_unconfigured() -> void {
    if (state().active_mode != mode::none) {
        throw std::runtime_error("Only one replay mode can be configured");
    }
}

} // namespace

auto configure_recording(const std::string& path, session_metadata metadata) -> void {
    require_unconfigured();
    if (metadata.version != format_version) {
        throw std::runtime_error("Unsupported replay format version");
    }
    state().active_mode = mode::record;
    state().path = path;
    state().metadata = metadata;
}

auto configure_playback(const std::string& path) -> void {
    require_unconfigured();
    state().active_mode = mode::playback;
    state().path = path;
}

auto configure_playback(const std::string& path, session_metadata expected) -> void {
    configure_playback(path);
    state().expected_metadata = expected;
}

auto playback_metadata() -> const session_metadata& { return state().metadata; } // *NOPAD*
auto configured_mode() -> mode { return state().active_mode; }
auto is_enabled() -> bool { return configured_mode() != mode::none; }
auto is_recording() -> bool { return configured_mode() == mode::record; }
auto is_playing() -> bool { return configured_mode() == mode::playback; }

auto start() -> void {
    auto& current = state();
    if (!is_enabled()) { return; }
    if (current.started) { throw std::runtime_error("Replay is already started"); }
    if (current.path.empty()) { throw std::runtime_error("Replay path must not be empty"); }
    try {
        if (is_recording()) {
            if (current.metadata.rng_seed == 0) {
                current.metadata.rng_seed = rng_deterministic_seed_value().value_or(1u);
            }
            current.recording
                .open(current.path, std::ios::out | std::ios::binary | std::ios::noreplace);
            if (!current.recording) {
                throw std::runtime_error("Could not create new replay file: " + current.path);
            }
            write_header();
            if (!current.recording) { throw std::runtime_error("Could not write replay header"); }
        } else {
            current.playback.open(current.path, std::ios::in | std::ios::binary);
            if (!current.playback) {
                throw std::runtime_error("Could not open replay file: " + current.path);
            }
            read_record(true);
            if (current.expected_metadata) {
                const auto expected = *current.expected_metadata;
                if (expected.version != current.metadata.version
                    || (expected.rng_seed != 0 && expected.rng_seed != current.metadata.rng_seed)) {
                    throw std::runtime_error("Replay header does not match expected metadata");
                }
            }
            ensure_next();
        }
        current.started = true;
    } catch (...) {
        stop();
        throw;
    }
}

auto finish() -> void {
    auto& current = state();
    if (!current.started) {
        stop();
        return;
    }
    try {
        if (is_recording()) {
            auto json = JsonOut(current.recording);
            json.start_object();
            json.member("kind", "end");
            json.member("version", format_version);
            json.member("events", current.events);
            json.end_object();
            flush_record();
            current.recording.close();
            if (current.recording.fail()) {
                throw std::runtime_error("Could not close replay recording");
            }
        } else {
            ensure_next();
            if (!current.ended) {
                throw std::runtime_error("Game exited with unconsumed replay input");
            }
        }
    } catch (...) {
        stop();
        throw;
    }
    stop();
}

auto stop() -> void { state() = replay_state{}; }

auto record_input_event(const input_event& event, const input_boundary_metadata& boundary) -> void {
    auto& current = state();
    if (!is_recording()) { return; }
    if (!current.started) { throw std::runtime_error("Replay recording has not been started"); }
    const auto type = event_type_name(event.type);
    auto json = JsonOut(current.recording);
    json.start_object();
    json.member("kind", "input");
    json.member("version", format_version);
    json.member("type", type);
    json.member("sequence", event.sequence);
    json.member("modifiers", event.modifiers);
    json.member("mouse_pos");
    json.start_array();
    json.write(event.mouse_pos.x);
    json.write(event.mouse_pos.y);
    json.end_array();
    json.member("text", event.text);
    json.member("edit", event.edit);
    json.member("edit_refresh", event.edit_refresh);
    if (event.interaction) {
        json.member("interaction");
        json.start_object();
        json.member("operation",
                    game_client::interaction_operation_name(event.interaction->operation));
        json.member("schema_id", event.interaction->schema_id);
        json.member("target_id", event.interaction->target_id);
        json.member("value", event.interaction->value);
        if (event.interaction->submit) { json.member("submit", *event.interaction->submit); }
        if (event.interaction->count) { json.member("count", *event.interaction->count); }
        if (event.interaction->position) {
            json.member("position");
            json.start_object();
            json.member("x", event.interaction->position->x);
            json.member("y", event.interaction->position->y);
            json.member("z", event.interaction->position->z);
            json.end_object();
        }
        json.end_object();
    }
#if defined(__ANDROID__)
    json.member("shortcut_last_used_action_counter", event.shortcut_last_used_action_counter);
#endif
    json.member("boundary");
    json.start_object();
    json.member("context", boundary.context);
    json.member("actions", boundary.actions);
    json.member("timeout_ms", boundary.timeout_ms);
    json.member("pointer_space", boundary.pointer_space);
    json.end_object();
    json.end_object();
    flush_record();
    ++current.events;
}

auto next_input_event(const input_boundary_metadata& expected_boundary)
    -> std::optional<input_event> {
    auto& current = state();
    if (!is_playing()) { return std::nullopt; }
    if (!current.started) { throw std::runtime_error("Replay playback has not been started"); }
    ensure_next();
    if (current.ended) { throw completed{}; }
    if (current.next_text) { fail("expected external text '" + current.next_text->source + "'"); }
    if (current.next->event.type == input_event_t::mouse
        && current.next->boundary.pointer_space != expected_boundary.pointer_space) {
        fail("mouse pointer_space recorded='" + current.next->boundary.pointer_space + "', actual='"
             + expected_boundary.pointer_space + "'");
    }
    if (!boundaries_match(current.next->boundary, expected_boundary)) {
        fail(boundary_mismatch_message(
            current.next->boundary, expected_boundary, current.events + 1));
    }
    auto result = std::move(current.next->event);
    current.next.reset();
    ++current.events;
    return result;
}

auto external_text(const std::string& source, const std::function<auto()->std::string>& read)
    -> std::string {
    auto& current = state();
    if (!is_enabled()) { return read(); }
    if (!current.started) { throw std::runtime_error("Replay has not been started"); }
    if (is_recording()) {
        auto text = read();
        auto json = JsonOut(current.recording);
        json.start_object();
        json.member("kind", "text");
        json.member("version", format_version);
        json.member("source", source);
        json.member("text", text);
        json.end_object();
        flush_record();
        return text;
    }
    ensure_next();
    if (!current.next_text || current.next_text->source != source) {
        fail("expected external text '" + source + "'");
    }
    auto text = std::move(current.next_text->text);
    current.next_text.reset();
    return text;
}

auto playback_exhausted() -> bool {
    if (!is_playing() || !state().started) { return false; }
    ensure_next();
    return state().ended;
}

} // namespace replay
