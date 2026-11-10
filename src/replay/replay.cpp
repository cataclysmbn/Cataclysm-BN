#include "replay/replay.h"

#include "debug.h"
#include "json.h"
#if defined(CATA_MCP)
#    include "client_interaction_data.h"
#endif
#include "runtime_handlers.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(TILES)
#    include <SDL3/SDL.h>
#endif

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
        output << " gameplay actions recorded=[";
        append_action_list(output, recorded_actions);
        output << "], actual=[";
        append_action_list(output, actual_actions);
        output << ']';
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

struct event_type_name {
    input_event_t type;
    std::string_view name;
};

constexpr auto event_type_names = std::to_array<event_type_name>({
    {input_event_t::keyboard, "keyboard"},
    {input_event_t::gamepad, "gamepad"},
    {input_event_t::mouse, "mouse"},
    {input_event_t::timeout, "timeout"},
#if defined(CATA_MCP)
    {input_event_t::interaction, "interaction"},
#endif
    {input_event_t::error, "idle"},
});

auto event_type_text(const input_event_t type) -> std::string {
    return std::string{std::ranges::find(event_type_names, type, &event_type_name::type)->name};
}

auto read_type(const std::string& name) -> input_event_t {
    const auto found = std::ranges::find(event_type_names, name, &event_type_name::name);
    if (found == event_type_names.end()) {
        throw std::runtime_error("Unknown input event type: " + name);
    }
    return found->type;
}

#if defined(CATA_MCP)
auto read_position(JsonObject position) -> game_client::interaction_position {
    const auto result = game_client::interaction_position{
        .x = position.get_int("x"),
        .y = position.get_int("y"),
        .z = position.get_int("z"),
    };
    position.finish();
    return result;
}

auto write_position(JsonOut& json, const game_client::interaction_position& position) -> void {
    json.member("position");
    json.start_object();
    json.member("x", position.x);
    json.member("y", position.y);
    json.member("z", position.z);
    json.end_object();
}

auto read_invoke_arguments(JsonObject object) -> game_client::interaction_arguments {
    auto result = game_client::interaction_arguments{};
    if (object.has_member("row")) { result.row = object.get_string("row"); }
    if (object.has_member("count")) { result.count = object.get_member("count").get_int64(); }
    if (object.has_member("text")) { result.text = object.get_string("text"); }
    if (object.has_member("position")) {
        result.position = read_position(object.get_object("position"));
    }
    if (object.has_member("pane")) { result.pane = object.get_string("pane"); }
    object.finish();
    return result;
}

auto write_invoke_arguments(JsonOut& json, const game_client::interaction_arguments& arguments)
    -> void {
    json.member("arguments");
    json.start_object();
    if (arguments.row) { json.member("row", *arguments.row); }
    if (arguments.count) { json.member("count", *arguments.count); }
    if (arguments.text) { json.member("text", *arguments.text); }
    if (arguments.position) { write_position(json, *arguments.position); }
    if (arguments.pane) { json.member("pane", *arguments.pane); }
    json.end_object();
}

auto read_interaction(JsonObject semantic) -> game_client::interaction_event {
    const auto operation = game_client::parse_interaction_operation(
        semantic.get_string("operation"));
    if (!operation) { semantic.throw_error("Unknown semantic interaction operation"); }
    using game_client::interaction_operation;
    auto payload = game_client::interaction_event{
        .operation = *operation,
        .schema_id = semantic.get_string("schema_id", ""),
        .target_id = semantic.get_string("target_id", ""),
        .value = semantic.get_string("value", ""),
    };
    if (*operation != interaction_operation::invoke && payload.schema_id.empty()) {
        semantic.throw_error("schema_id must not be empty");
    }
    if (*operation == interaction_operation::fill) {
        if (!semantic.has_member("submit")) {
            semantic.throw_error("fill requires explicit submit");
        }
        payload.submit = semantic.get_bool("submit");
    } else if (*operation == interaction_operation::set_count) {
        const auto count = semantic.get_member("count").get_int64();
        if (count < 0) { semantic.throw_error("count must not be negative"); }
        payload.count = static_cast<std::uint64_t>(count);
    } else if (*operation == interaction_operation::set_target) {
        payload.position = read_position(semantic.get_object("position"));
    } else if (*operation == interaction_operation::invoke) {
        if (!semantic.has_member("arguments")) {
            semantic.throw_error("invoke requires arguments");
        }
        payload.arguments = read_invoke_arguments(semantic.get_object("arguments"));
    }
    semantic.finish();
    const auto needs_target =
        *operation == interaction_operation::choose || *operation == interaction_operation::fill
        || *operation == interaction_operation::set_count
        || *operation == interaction_operation::invoke;
    const auto takes_value = *operation == interaction_operation::fill;
    const auto invalid_shape =
        (needs_target && payload.target_id.empty()) || (!takes_value && !payload.value.empty())
        || (*operation == interaction_operation::cancel && !payload.target_id.empty());
    if (invalid_shape) { throw std::runtime_error("Invalid semantic interaction payload"); }
    return payload;
}

auto write_interaction(JsonOut& json, const game_client::interaction_event& interaction) -> void {
    json.member("interaction");
    json.start_object();
    json.member("operation", game_client::interaction_operation_name(interaction.operation));
    json.member("schema_id", interaction.schema_id);
    json.member("target_id", interaction.target_id);
    json.member("value", interaction.value);
    if (interaction.submit) { json.member("submit", *interaction.submit); }
    if (interaction.count) { json.member("count", *interaction.count); }
    if (interaction.position) { write_position(json, *interaction.position); }
    if (interaction.arguments) { write_invoke_arguments(json, *interaction.arguments); }
    json.end_object();
}
#endif

auto read_event(const JsonObject& object) -> replay_event {
    auto result = replay_event{};
    auto& event = result.event;
    event.type = read_type(object.get_string("type"));
    event.sequence = object.get_int_array("sequence");
    event.modifiers = object.get_int_array("modifiers");
    const auto position = object.get_int_array("mouse_pos");
    if (position.size() != 2) { throw std::runtime_error("mouse_pos requires two coordinates"); }
    event.mouse_pos = point(position[0], position[1]);
    event.text = object.get_string("text");
    event.edit = object.get_string("edit");
    event.edit_refresh = object.get_bool("edit_refresh");
#if defined(CATA_MCP)
    event.action = object.get_string("action", "");
    if (event.type == input_event_t::interaction) {
        event.interaction = read_interaction(object.get_object("interaction"));
    }
#endif
    auto boundary = object.get_object("boundary");
    boundary.allow_omitted_members();
    result.boundary.context = boundary.get_string("context");
    result.boundary.timeout_ms = boundary.get_int("timeout_ms", -1);
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
        object.allow_omitted_members();
        const auto kind = object.get_string("kind");
        if (header) {
            if (kind != "header" || object.get_string("format") != "cataclysm-bn-replay") {
                throw std::runtime_error("First record must be a cataclysm-bn-replay header");
            }
            if (object.get_member("version").get_int64() != format_version) {
                throw std::runtime_error("Unsupported replay version");
            }
            const auto seed = object.get_member("rng_seed").get_int64();
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
            const auto count = object.get_member("events").get_int64();
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

auto flush_record() -> void {
    auto& current = state();
    current.recording << '\n' << std::flush;
    if (!current.recording) { throw std::runtime_error("Could not write replay: " + current.path); }
}

/// Write one `{"kind":...}` record line; `members` adds the rest.
auto write_record(const std::string& kind, const auto& members) -> void {
    auto& current = state();
    auto json = JsonOut(current.recording);
    json.start_object();
    json.member("kind", kind);
    members(json);
    json.end_object();
    flush_record();
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
    if (metadata.rng_seed == 0) { throw std::runtime_error("RNG seed must be nonzero"); }
    state().active_mode = mode::record;
    state().path = path;
    state().metadata = metadata;
}

auto configure_playback(const std::string& path) -> void {
    require_unconfigured();
    state().active_mode = mode::playback;
    state().path = path;
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
            current.recording
                .open(current.path, std::ios::out | std::ios::binary | std::ios::noreplace);
            if (!current.recording) {
                throw std::runtime_error("Could not create new replay file: " + current.path);
            }
            write_record("header", [&](JsonOut& json) {
                json.member("format", "cataclysm-bn-replay");
                json.member("version", format_version);
                json.member("rng_seed", current.metadata.rng_seed);
            });
            if (!current.recording) { throw std::runtime_error("Could not write replay header"); }
        } else {
            current.playback.open(current.path, std::ios::in | std::ios::binary);
            if (!current.playback) {
                throw std::runtime_error("Could not open replay file: " + current.path);
            }
            read_record(true);
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
            write_record("end", [&](JsonOut& json) { json.member("events", current.events); });
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

auto discard() -> void {
    const auto created = is_recording() && state().started ? state().path : std::string{};
    stop();
    auto ignored = std::error_code{};
    if (!created.empty()) { std::filesystem::remove(created, ignored); }
}

auto record_input_event(const input_event& event, const input_boundary_metadata& boundary) -> void {
    auto& current = state();
    if (!is_recording()) { return; }
    if (!current.started) { throw std::runtime_error("Replay recording has not been started"); }
    write_record("input", [&](JsonOut& json) {
        json.member("type", event_type_text(event.type));
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
#if defined(CATA_MCP)
        if (!event.action.empty()) { json.member("action", event.action); }
        if (event.interaction) { write_interaction(json, *event.interaction); }
#endif
        json.member("boundary");
        json.start_object();
        json.member("context", boundary.context);
        json.member("actions", boundary.actions);
        json.member("timeout_ms", boundary.timeout_ms);
        json.member("pointer_space", boundary.pointer_space);
        json.end_object();
    });
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
        write_record("text", [&](JsonOut& json) {
            json.member("source", source);
            json.member("text", text);
        });
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

auto external_text_or_exit(
    const std::string& source, const std::function<auto()->std::string>& read) -> std::string {
    try {
        return external_text(source, read);
    } catch (const std::runtime_error& error) { exit_with_failure(error); }
}

#if defined(TILES)
auto clipboard_text() -> std::string {
    auto text = std::string{};
    char* const clip = SDL_GetClipboardText();
    if (clip) {
        text = clip;
        SDL_free(clip);
    }
    return text;
}
#endif

auto exit_with_failure(const std::runtime_error& error) -> void {
    DebugLog(DL::Error, DC::Main) << error.what();
    std::cerr << "Cataclysm BN: " << error.what() << '\n';
    stop();
    exit_handler(1);
}

} // namespace replay
