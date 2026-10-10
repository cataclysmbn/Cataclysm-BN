#if defined(CATA_MCP) && defined(CATA_DEBUG_NATIVE_TEST)

#    include "cached_options.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_interaction_validation.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_presentation.h"
#    include "client_presentation_scope.h"
#    include "cursesdef.h"
#    include "debug.h"
#    include "debug_test_support.h"
#    include "engine_client_contract.h"
#    include "enum_bitset.h"
#    include "get_version.h"
#    include "input.h"
#    include "json.h"
#    include "messages.h"
#    include "output.h"
#    include "path_info.h"
#    include "replay/replay.h"
#    include "rng.h"
#    include "thread_pool.h"
#    include "translations.h"
#    include "ui_manager.h"

#    include <algorithm>
#    include <array>
#    include <cstdint>
#    include <filesystem>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <utility>
#    include <vector>

namespace {
namespace client = game_client;
namespace support = debug_test_support;

struct clipboard_service final: client::render_service {
    bool available = true;
    std::vector<std::string> copies;
    int suspends = 0;
    int restores = 0;
    int invalidations = 0;
    bool* destroyed = nullptr;
    ~clipboard_service() override {
        if (destroyed) { *destroyed = true; }
    }
    auto clipboard_available() const -> bool override { return available; }
    auto set_clipboard_text(const std::string& text) -> bool override {
        copies.push_back(text);
        return available;
    }
    auto suspend_clip() -> std::optional<rectangle<point>> override {
        ++suspends;
        return rectangle<point>(point(1, 2), point(7, 8));
    }
    auto restore_clip(const std::optional<rectangle<point>>& clip) -> void override {
        CHECK(clip.has_value());
        if (clip) {
            CHECK(clip->p_min == point(1, 2));
            CHECK(clip->p_max == point(7, 8));
        }
        ++restores;
    }
    auto invalidate_framebuffer(bool /*force*/) -> void override { ++invalidations; }
};

auto serialized_messages() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    Messages::serialize(json);
    json.end_object();
    return stream.str();
}

/// This target is a dedicated native-message process, not the ordinary fake-message suite.
/// Seed empty message authority once for the run; existing entries are never reset/replaced.
/// The sentinel lives until process teardown, so all fixtures observe nonempty authority.
struct debug_messages_listener: Catch::TestEventListenerBase {
    using TestEventListenerBase::TestEventListenerBase;
    auto testRunStarting(const Catch::TestRunInfo& info) -> void override {
        TestEventListenerBase::testRunStarting(info);
        if (Messages::size() == 0) { Messages::add_msg("Native debug message purity sentinel"); }
    }
};

CATCH_REGISTER_LISTENER(debug_messages_listener)

struct native_fixture {
    client::memory::scoped_state memory;
    support::scoped_state debug;
    restore_on_out_of_scope<int> width{TERMX};
    restore_on_out_of_scope<int> height{TERMY};
    restore_on_out_of_scope<int> full_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> full_height{FULL_SCREEN_HEIGHT};
    int timeout = inp_mngr.get_timeout();
    const std::string messages = serialized_messages();

    explicit native_fixture(const bool fail = false) {
        TERMX = 120;
        TERMY = 50;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        client::memory::initialize(TERMX, TERMY);
        if (fail) { throw std::runtime_error("fixture construction"); }
    }
    ~native_fixture() { inp_mngr.set_timeout(timeout); }
};

auto report(const support::prompt_options& options) -> std::string {
    const auto prefix =
        options.force
            ? "            Excessive error repetition detected.  Please file a bug report at https://github.com/cataclysmbn/Cataclysm-BN/issues\n"
            : "";
    return string_format(
        "%s DEBUG    : %s\n\n FUNCTION : %s\n FILE     : %s\n LINE     : %s\n VERSION  : BN %s\n",
        prefix, options.text, options.function, options.filename, options.line, getVersionString());
}

[[noreturn]] auto failing_provider(const int /*timeout*/) -> input_event {
    throw std::runtime_error("provider failure");
}

/// Records a real error through the existing capture route, avoiding expensive addr2line.
/// The native prompt and threshold routing are exercised separately, with capture disabled.
auto record_error() -> void {
    capture_debugmsg_during([]() {
        DebugLog(DL::Error, DC::DebugMsg) << "unrelated pre-existing error";
    });
}

/// Always bounded, including unrecognized controls. No semantic acknowledgement is guessed.
auto raw_sequence(const std::vector<int>& keys, int& reads) -> void {
    client::memory::set_input_provider([keys, &reads](const int /*timeout*/) {
        if (reads >= static_cast<int>(keys.size())) {
            throw std::runtime_error("raw input budget exhausted");
        }
        return input_event(keys[reads++], input_event_t::keyboard);
    });
}

auto choice_id(const client::interaction_snapshot& snapshot, const std::string& label)
    -> std::string {
    const auto found =
        std::ranges::find(snapshot.choices, label, &client::interaction_choice::label);
    REQUIRE(found != snapshot.choices.end());
    REQUIRE_FALSE(found->id.empty());
    REQUIRE(found->enabled);
    REQUIRE(found->selectable);
    return found->id;
}

auto semantic_choice(const client::interaction_snapshot& snapshot, const std::string& label)
    -> input_event {
    auto command = client::input_command{};
    auto interaction = client::interaction_command{};
    interaction.input_id = snapshot.input_id;
    interaction.operation = client::interaction_operation::choose;
    interaction.target_id = choice_id(snapshot, label);
    command.interaction = std::move(interaction);
    const auto resolved = client::resolve_input_command(command, client::memory::screen_size());
    REQUIRE(resolved.has_value());
    return *resolved;
}

struct debug_replay_file {
    std::filesystem::path directory;
    std::filesystem::path path;
    debug_replay_file() {
        REQUIRE_FALSE(replay::is_enabled());
        static auto next = std::uint64_t{0};
        do {
            directory =
                std::filesystem::path(PATH_INFO::user_dir())
                / ("debug-replay-" + std::to_string(++next));
        } while (!std::filesystem::create_directory(directory));
        path = directory / "input.jsonl";
    }
    ~debug_replay_file() {
        replay::stop();
        std::filesystem::remove_all(directory);
    }
};

/// Independent byte oracle: diagnostic and metadata strings are data, never color markup.
/// Deliberately does not use string_format, report(), or any color-tag helper.
auto literal_expected_report(const support::prompt_options& options) -> std::string {
    const auto repetition =
        options.force
            ? std::
                  string{"            Excessive error repetition detected.  Please file a bug report at https://github.com/cataclysmbn/Cataclysm-BN/issues\n"}
            : std::string{};
    return repetition + " DEBUG    : " + options.text + "\n\n FUNCTION : " + options.function
         + "\n FILE     : " + options.filename + "\n LINE     : " + options.line
         + "\n VERSION  : BN " + getVersionString() + "\n";
}

/// Fixed trusted envelope budget includes even its original color markup, permitting either
/// trusted-fragment normalization or supported markup without licensing report expansion.
auto report_envelope_byte_budget() -> std::size_t {
    auto envelope =
        std::string{"\n\n "} + _("An error has occurred!  Written below is the error report:")
        + "\n -----------------------------------------------------------\n"
        + " -----------------------------------------------------------\n";
#    if defined(BACKTRACE)
    envelope +=
        " " + string_format(_("See %s for a full stack backtrace"), PATH_INFO::debug()) + "\n";
#    endif
    envelope +=
        std::string{" "} + _("Press <color_white>space bar</color> to continue the game.") + "\n";
    envelope +=
        std::string{" "}
        + _("Press <color_white>I</color> (or <color_white>i</color>) to also ignore this particular message in the future.")
        + "\n";
    envelope +=
        std::string{" "}
        + _("Press <color_white>C</color> (or <color_white>c</color>) to copy this message to the clipboard.")
        + "\n";
    return envelope.size();
}

} // namespace

TEST_CASE(
    "native debug prompt discloses full structured choices before acknowledgement",
    "[client][debug_prompt][debug_prompt_red]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    const auto available = GENERATE(true, false);
    clipboard->available = available;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto options = support::prompt_options{
        .filename = "tutorial_full_source.cpp",
        .line = "1234",
        .function = "tutorial_start",
        .text = "Tutorial GPU failure\nMissing transparency shader\nFULL REPORT TAIL"};
    auto observation = client::interaction_snapshot{};
    auto category = std::string{};
    auto native_text = std::string{};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 1) { throw std::runtime_error("RED raw Space exit exceeded budget"); }
        category = client::active_input_context().category;
        observation = client::current_interaction();
        // Native drawing is a separate control, never the semantic adapter's data source.
        native_text = client::memory::snapshot().text;
        return input_event(' ', input_event_t::keyboard);
    });
    support::prompt(options);
    CAPTURE(available, category, observation.context, observation.input_id);
    INFO("Complete source report expected at the observed boundary:\n" << report(options));
    REQUIRE(reads == 1);
    CHECK(native_text.find("FULL REPORT TAIL") != std::string::npos);
    CHECK((native_text.find("to copy this message to the clipboard") != std::string::npos)
          == available);
    CHECK(category == "DEBUG_MSG");
    CHECK(observation.context == "DEBUG_MSG");
    CHECK(observation.structured);
    CHECK(observation.kind == client::interaction_kind::choices);
    CHECK_FALSE(observation.allow_cancel);
    CHECK_FALSE(observation.field.has_value());
    CHECK_FALSE(observation.allow_set_count);
    CHECK(observation.message.find(report(options)) != std::string::npos);
    // Require complete instruction content without imposing a color-markup representation.
    const auto semantic_text = remove_color_tags(observation.message);
    CHECK(semantic_text.find(
              remove_color_tags(_("Press <color_white>space bar</color> to continue the game.")))
          != std::string::npos);
    CHECK(
        semantic_text.find(remove_color_tags(_(
            "Press <color_white>I</color> (or <color_white>i</color>) to also ignore this particular message in the future.")))
        != std::string::npos);
    CHECK(
        (semantic_text.find(remove_color_tags(_(
             "Press <color_white>C</color> (or <color_white>c</color>) to copy this message to the clipboard.")))
         != std::string::npos)
        == available);
#    if defined(BACKTRACE)
    const auto backtrace_instructions =
        string_format(_("See %s for a full stack backtrace"), PATH_INFO::debug());
    CHECK(observation.message.find(backtrace_instructions) != std::string::npos);
#    endif
    CHECK(observation.choices.size() == (available ? 3 : 2));
    for (const auto& label : std::vector<std::string>{"Continue", "Ignore"}) {
        CHECK(std::ranges::any_of(observation.choices, [&](const auto& choice) {
            return choice.label == label && !choice.id.empty() && choice.enabled
                && choice.selectable;
        }));
    }
    CHECK(std::ranges::
              any_of(observation.choices, [](const auto& choice) { return choice.label == "Copy"; })
          == available);
    if (available) {
        CHECK(std::ranges::any_of(observation.choices, [](const auto& choice) {
            return choice.label == "Copy" && !choice.id.empty() && choice.enabled
                && choice.selectable;
        }));
    }
}

TEST_CASE(
    "native debug disclosure cannot inherit an enclosing interaction",
    "[client][debug_prompt][debug_prompt_red]") {
    auto fixture = native_fixture{};
    auto presentation = client::presentation_scope{std::make_unique<client::render_service>()};
    auto outer = input_context{"DEBUG_OUTER"};
    auto binding = client::interaction_scope{
        outer, []() {
            auto snapshot = client::interaction_snapshot{};
            snapshot.title = "unrelated outer provider";
            snapshot.structured = true;
            return snapshot;
        }};
    auto context = client::input_context_scope{outer, outer.category_name()};
    const auto options = support::prompt_options{.text = "nested full debug report"};
    auto observed = client::interaction_snapshot{};
    auto category = std::string{};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 2) { throw std::runtime_error("nested RED Space budget"); }
        if (reads == 1) {
            support::prompt(options);
        } else {
            category = client::active_input_context().category;
            observed = client::current_interaction();
        }
        return input_event(' ', input_event_t::keyboard);
    });
    inp_mngr.get_input_event();
    REQUIRE(reads == 2);
    CHECK(client::active_input_context().context == &outer);
    CHECK(client::current_interaction().title == "unrelated outer provider");
    CAPTURE(category, observed.context, observed.title);
    INFO("Complete nested source report:\n" << report(options));
    CHECK(category == "DEBUG_MSG");
    CHECK(observed.context == "DEBUG_MSG");
    CHECK(observed.title != "unrelated outer provider");
    CHECK(observed.message.find(report(options)) != std::string::npos);
    CHECK(observed.kind == client::interaction_kind::choices);
    CHECK(observed.choices.size() == 2);
}

TEST_CASE(
    "native debug raw copy keeps complete report and waits for continue",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    service.available = GENERATE(true, false);
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto copy_key = GENERATE('C', 'c');
    const auto options = support::prompt_options{.text = "first line\nsecond line\nverbatim tail"};
    auto reads = 0;
    raw_sequence({copy_key, '?', ' '}, reads);
    support::prompt(options);
    CHECK(reads == 3);
    REQUIRE(service.copies.size() == 1);
    CHECK(service.copies.front() == report(options));
    CHECK(service.suspends == 1);
    CHECK(service.restores == 1);
    CHECK(service.invalidations >= 1);
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug ignore is source keyed and force retains warning",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    const auto ignore_key = GENERATE('I', 'i');
    const auto options = support::prompt_options{.text = "original"};
    auto reads = 0;
    raw_sequence({ignore_key}, reads);
    support::prompt(options);
    support::prompt({.text = "changed text, same source"});
    CHECK(reads == 1);
    raw_sequence({' '}, reads = 0);
    support::prompt({.line = "43", .text = "different source"});
    CHECK(reads == 1);
    auto drawn = std::string{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 2) { throw std::runtime_error("force input budget"); }
        drawn = client::memory::snapshot().text;
        return input_event(' ', input_event_t::keyboard);
    });
    support::prompt({.text = "original", .force = true});
    CHECK(reads == 2);
    CHECK(drawn.find("Excessive error repetition detected") != std::string::npos);
    CHECK(drawn.find("original") != std::string::npos);
}

TEST_CASE(
    "native debug real repetition threshold routes forced prompt after ignore",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    const auto options = support::prompt_options{.text = "repeat threshold"};
    auto reads = 0;
    raw_sequence({'I'}, reads);
    support::prompt(options);
    record_error();
    auto drawn = std::string{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 2) { throw std::runtime_error("repetition input budget"); }
        drawn = client::memory::snapshot().text;
        return input_event(' ', input_event_t::keyboard);
    });
    support::prime_repetition(options);
    realDebugmsg(options.filename.c_str(), options.line.c_str(), options.function.c_str(),
                 DL::Error, options.text);
    CHECK(reads == 2);
    CHECK(drawn.find("Excessive error repetition detected") != std::string::npos);
    CHECK(debug_has_error_been_observed());
}

TEST_CASE(
    "native debug nested read restores enclosing provider and monotonic boundary",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    auto outer = input_context{"DEBUG_OUTER"};
    auto binding = client::interaction_scope{
        outer, []() {
            auto snapshot = client::interaction_snapshot{};
            snapshot.title = "outer provider";
            snapshot.structured = true;
            return snapshot;
        }};
    auto context = client::input_context_scope{outer, outer.category_name()};
    const auto before = client::current_input_id();
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 3) { throw std::runtime_error("nested input budget"); }
        if (reads == 1) {
            support::prompt({.text = "nested prompt"});
            CHECK(client::active_input_context().context == &outer);
            CHECK(client::current_interaction().title == "outer provider");
        }
        return input_event(' ', input_event_t::keyboard);
    });
    inp_mngr.get_input_event();
    CHECK(reads == 2);
    CHECK(client::current_input_id() > before);
    CHECK(client::active_input_context().context == &outer);
}

TEST_CASE(
    "native debug redraw interruption restarts and resize remains usable",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    auto redraws = 0;
    auto resizes = 0;
    auto reads = 0;
    auto outer = ui_adaptor{};
    outer.position_from_window(catacurses::stdscr);
    outer.on_screen_resize([&](auto& ui) {
        ++resizes;
        ui.position_from_window(catacurses::stdscr);
    });
    outer.on_redraw([&](const auto& /*ui*/) {
        if (++redraws == 1) { support::prompt({.text = "redraw interruption"}); }
        catacurses::mvwprintw(catacurses::stdscr, point_zero, "outer UI usable");
        catacurses::wnoutrefresh(catacurses::stdscr);
    });
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 2) { throw std::runtime_error("resize input budget"); }
        if (reads == 1) {
            TERMX = 110;
            TERMY = 45;
            client::memory::resize(TERMX, TERMY);
            ui_manager::screen_resized();
            return input_event('?', input_event_t::keyboard);
        }
        return input_event(' ', input_event_t::keyboard);
    });
    ui_manager::redraw();
    CHECK(reads == 2);
    CHECK(redraws >= 2);
    CHECK(resizes >= 1);
    CHECK(service.suspends == 1);
    CHECK(service.restores == 1);
    CHECK(client::memory::snapshot().text.find("outer UI usable") != std::string::npos);
}

TEST_CASE(
    "native debug exceptional cleanup preserves initialized ownership and errors",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto service_destroyed = false;
    clipboard->destroyed = &service_destroyed;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    auto sentinel_reads = 0;
    raw_sequence({'I', ' '}, sentinel_reads);
    support::prompt({.text = "ignored sentinel"});
    inp_mngr.set_timeout(19);
    client::memory::set_cursor(1);
    catacurses::mvwprintw(catacurses::stdscr, point(4, 3), "sentinel surface");
    catacurses::wrefresh(catacurses::stdscr);
    const auto screen = catacurses::stdscr;
    const auto new_screen = catacurses::newscr;
    const auto snapshot = client::memory::snapshot();
    const auto dimensions = point(TERMX, TERMY);
    test_mode = true;
    record_error();
    REQUIRE(debug_has_error_been_observed());
    dont_debugmsg = true;
    auto nested_service_destroyed = false;
    const auto capture = capture_debugmsg_during([&]() {
        realDebugmsg("capture.cpp", "1", "capture", DL::Error, "capture before");
        DebugLog(DL::Info, DC::DebugMsg) << std::hex << 42;
        CHECK_THROWS_AS(native_fixture(true), std::runtime_error);
        try {
            auto nested = native_fixture{};
            auto temporary = std::make_unique<clipboard_service>();
            temporary->destroyed = &nested_service_destroyed;
            auto replacement = client::presentation_scope{std::move(temporary)};
            CHECK_FALSE(service_destroyed);
            inp_mngr.set_timeout(37);
            client::memory::set_input_provider(failing_provider);
            support::prompt({.text = "exceptional prompt"});
            FAIL("provider did not throw");
        } catch (const std::runtime_error& error) {
            CHECK(std::string(error.what()) == "provider failure");
        }
        realDebugmsg("capture.cpp", "2", "capture", DL::Error, "capture after");
        DebugLog(DL::Info, DC::DebugMsg) << 42;
    });
    CHECK(capture == "capture beforecapture after2a\n2a\n");
    CHECK(debug_has_error_been_observed());
    CHECK(test_mode);
    CHECK(dont_debugmsg);
    CHECK(&client::presentation() == &service);
    CHECK_FALSE(service_destroyed);
    CHECK(nested_service_destroyed);
    CHECK(catacurses::stdscr == screen);
    CHECK(catacurses::newscr == new_screen);
    CHECK(point(TERMX, TERMY) == dimensions);
    const auto restored = client::memory::snapshot();
    CHECK(restored.text == snapshot.text);
    CHECK(restored.width == snapshot.width);
    CHECK(restored.height == snapshot.height);
    CHECK(restored.cursor == snapshot.cursor);
    CHECK(restored.cursor_visible == snapshot.cursor_visible);
    CHECK(
        std::ranges::equal(restored.cells, snapshot.cells, [](const auto& left, const auto& right) {
            return left.text == right.text && left.foreground == right.foreground
                && left.background == right.background;
        }));
    CHECK(inp_mngr.get_timeout() == 19);
    test_mode = false;
    dont_debugmsg = false;
    support::prompt({.text = "same ignored source survives nested scope"});
    CHECK(sentinel_reads == 1);
    support::prompt({.line = "44", .text = "normal prompt after exception"});
    CHECK(sentinel_reads == 2);
    CHECK(service.suspends == service.restores);
}

TEST_CASE(
    "native debug repetition survives exceptional nested isolation",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    const auto options = support::prompt_options{.text = "restored repetition"};
    auto reads = 0;
    raw_sequence({'I', ' '}, reads);
    support::prompt(options);
    record_error();
    support::prime_repetition(options);
    CHECK_THROWS_AS(
        ([&]() {
            auto nested = native_fixture{};
            client::memory::set_input_provider(failing_provider);
            support::prompt({.text = "nested"});
        }()),
        std::runtime_error);
    realDebugmsg(options.filename.c_str(), options.line.c_str(), options.function.c_str(),
                 DL::Error, options.text);
    CHECK(reads == 2);
    CHECK(client::memory::snapshot().text.find("Excessive error repetition detected")
          != std::string::npos);
    CHECK(debug_has_error_been_observed());
}

TEST_CASE(
    "native debug fixture construction restores cold memory state",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto isolate = client::memory::scoped_state{};
    auto debug = support::scoped_state{};
    client::memory::shutdown();
    const auto dimensions = point(TERMX, TERMY);
    const auto old_mode = test_mode;
    const auto old_suppression = dont_debugmsg;
    const auto old_timeout = inp_mngr.get_timeout();
    auto* const service = &client::presentation();
    CHECK_THROWS_AS(native_fixture(true), std::runtime_error);
    CHECK_FALSE(catacurses::stdscr);
    CHECK_FALSE(catacurses::newscr);
    CHECK(client::memory::snapshot().cells.empty());
    CHECK(client::memory::screen_size() == point_zero);
    CHECK(point(TERMX, TERMY) == dimensions);
    CHECK(test_mode == old_mode);
    CHECK(dont_debugmsg == old_suppression);
    CHECK(inp_mngr.get_timeout() == old_timeout);
    CHECK(&client::presentation() == service);
    auto fixture = native_fixture{};
    auto reads = 0;
    raw_sequence({' ', ' '}, reads);
    support::prompt({.text = "usable after cold failure"});
    support::prompt({.text = "Space does not ignore"});
    CHECK(reads == 2);
}

TEST_CASE(
    "native debug startup and worker queues survive exceptional isolation",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    auto startup = support::scoped_state{{.buffering = true}};
    auto pool = cata_thread_pool{1};
    const auto main_options =
        support::prompt_options{.filename = "startup.cpp", .text = "saved startup report"};
    const auto worker_options =
        support::prompt_options{.filename = "worker.cpp", .text = "saved worker report"};
    const auto queue_main = [](const auto& options) {
        realDebugmsg(options.filename.c_str(), options.line.c_str(), options.function.c_str(),
                     DL::Error, options.text);
    };
    const auto queue_worker = [&](const auto& options) {
        pool.submit_returning([&]() {
                // Do not call Catch or drive drawing on the worker; propagate status via the
                // future.
                if (!is_pool_worker_thread()) { throw std::runtime_error("not a pool worker"); }
                queue_main(options);
            })
            .get();
    };
    // Real producers, with the logging capture route only to avoid symbolizer subprocesses.
    // Capture does not bypass startup/worker routing when test_mode is false.
    capture_debugmsg_during([&]() {
        queue_main(main_options);
        queue_worker(worker_options);
    });
    auto reads = 0;
    auto frames = std::vector<std::string>{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 2) { throw std::runtime_error("restored queue input budget"); }
        frames.push_back(client::memory::snapshot().text);
        return input_event(' ', input_event_t::keyboard);
    });
    CHECK_THROWS_AS(
        ([&]() {
            auto nested = support::scoped_state{{.buffering = true}};
            capture_debugmsg_during([&]() {
                queue_main(support::prompt_options{
                    .filename = "discard.cpp", .text = "discard nested startup"});
                queue_worker(support::prompt_options{
                    .filename = "discard_worker.cpp", .text = "discard nested worker"});
            });
            throw std::runtime_error("queued isolation failure");
        }()),
        std::runtime_error);
    CHECK(reads == 0);
    replay_buffered_debugmsg_prompts();
    REQUIRE(frames.size() == 2);
    CHECK(frames[0].find("saved startup report") != std::string::npos);
    CHECK(frames[1].find("saved worker report") != std::string::npos);
    CHECK(frames[0].find("discard nested") == std::string::npos);
    CHECK(frames[1].find("discard nested") == std::string::npos);
    drain_worker_thread_debugmsgs();
    replay_buffered_debugmsg_prompts();
    CHECK(reads == 2);
}

TEST_CASE(
    "native debug fixture teardown leaves nonempty message authority untouched",
    "[client][debug_prompt][debug_prompt_controls]") {
    REQUIRE(Messages::size() > 0);
    const auto count = Messages::size();
    const auto messages = serialized_messages();
    const auto recent = Messages::recent_messages(count);
    {
        auto fixture = native_fixture{};
        auto reads = 0;
        raw_sequence({' '}, reads);
        support::prompt({.text = "message purity on normal teardown"});
        CHECK(reads == 1);
    }
    CHECK(Messages::size() == count);
    CHECK(Messages::recent_messages(count) == recent);
    CHECK(serialized_messages() == messages);
    CHECK_THROWS_AS(native_fixture(true), std::runtime_error);
    CHECK(Messages::size() == count);
    CHECK(Messages::recent_messages(count) == recent);
    CHECK(serialized_messages() == messages);
    CHECK_THROWS_AS(
        ([&]() {
            auto fixture = native_fixture{};
            client::memory::set_input_provider(failing_provider);
            support::prompt({.text = "message purity on exceptional teardown"});
        }()),
        std::runtime_error);
    CHECK(Messages::size() == count);
    CHECK(Messages::recent_messages(count) == recent);
    CHECK(serialized_messages() == messages);
}

TEST_CASE(
    "native debug isolation restores a nondefault log level mask",
    "[client][debug_prompt][debug_prompt_controls]") {
    auto fixture = native_fixture{};
    auto original_mask = enum_bitset<DL>{};
    original_mask.set(DL::Warn);
    setDebugLogLevels(original_mask, true);
    CHECK_THROWS_AS(
        ([&]() {
            auto nested = support::scoped_state{};
            auto different_mask = enum_bitset<DL>{};
            different_mask.set(DL::Error);
            setDebugLogLevels(different_mask, true);
            throw std::runtime_error("log mask isolation failure");
        }()),
        std::runtime_error);
    auto reads = 0;
    raw_sequence({' '}, reads);
    const auto captured = capture_debugmsg_during([&]() {
        realDebugmsg("mask.cpp", "1", "mask", DL::Warn, "restored warning route");
        CHECK(reads == 1);
        realDebugmsg("mask.cpp", "2", "mask", DL::Error, "restored disabled route");
        CHECK(reads == 1);
    });
    CHECK(captured == "restored warning routerestored disabled route");
}

TEST_CASE(
    "native debug semantic continue and ignore preserve raw behavior",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto semantic = GENERATE(false, true);
    const auto label = GENERATE(std::string("Continue"), std::string("Ignore"));
    CAPTURE(semantic, label);
    auto outer = input_context{"DEBUG_PARITY_OUTER"};
    auto binding = client::interaction_scope{
        outer, []() {
            auto snapshot = client::interaction_snapshot{};
            snapshot.title = "enclosing parity provider";
            return snapshot;
        }};
    auto context = client::input_context_scope{outer, outer.category_name()};
    const auto options = support::prompt_options{.text = "semantic parity full report"};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 4) { throw std::runtime_error("parity input budget"); }
        const auto snapshot = client::current_interaction();
        CHECK(snapshot.context == "DEBUG_MSG");
        CHECK(snapshot.structured);
        if (reads == 1) { CHECK(snapshot.message.find(report(options)) != std::string::npos); }
        const auto selected = reads == 1 ? label : "Continue";
        return semantic ? semantic_choice(snapshot, selected)
                        : input_event(selected == "Ignore" ? 'I' : ' ', input_event_t::keyboard);
    });
    support::prompt(options);
    CHECK(reads == 1);
    support::prompt({.text = "same source with changed text"});
    CHECK(reads == (label == "Ignore" ? 1 : 2));
    support::prompt({.line = "43", .text = "different source"});
    CHECK(reads == (label == "Ignore" ? 2 : 3));
    support::prompt({.text = options.text, .force = true});
    CHECK(reads == (label == "Ignore" ? 3 : 4));
    CHECK(service.copies.empty());
    CHECK(client::active_input_context().context == &outer);
    CHECK(client::current_interaction().title == "enclosing parity provider");
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug repeated copy waits for continue and restores enclosing read",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto semantic = GENERATE(false, true);
    auto outer = input_context{"DEBUG_COPY_OUTER"};
    auto binding = client::interaction_scope{
        outer, []() {
            auto snapshot = client::interaction_snapshot{};
            snapshot.title = "enclosing copy provider";
            return snapshot;
        }};
    auto context = client::input_context_scope{outer, outer.category_name()};
    const auto options = support::prompt_options{.text = "copy first line\ncopy full tail"};
    auto reads = 0;
    auto previous_boundary = client::current_input_id();
    auto schema = std::string{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 5) { throw std::runtime_error("copy input budget"); }
        if (reads == 1) {
            CHECK(client::active_input_context().context == &outer);
            support::prompt(options);
            CHECK(client::active_input_context().context == &outer);
            CHECK(client::current_interaction().title == "enclosing copy provider");
            return input_event(' ', input_event_t::keyboard);
        }
        const auto snapshot = client::current_interaction();
        CHECK(snapshot.context == "DEBUG_MSG");
        CHECK(snapshot.input_id > previous_boundary);
        previous_boundary = snapshot.input_id;
        if (schema.empty()) { schema = snapshot.schema_id; }
        CHECK(snapshot.schema_id == schema);
        CHECK(service.copies.size() == static_cast<std::size_t>(std::min(reads - 2, 2)));
        const auto copy = reads < 4;
        return semantic ? semantic_choice(snapshot, copy ? "Copy" : "Continue")
                        : input_event(copy ? 'C' : ' ', input_event_t::keyboard);
    });
    inp_mngr.get_input_event();
    CHECK(reads == 4);
    REQUIRE(service.copies.size() == 2);
    CHECK(std::ranges::all_of(service.copies, [&](const auto& text) {
        return text == report(options);
    }));
    support::prompt(options);
    CHECK(reads == 5);
    CHECK(service.copies.size() == 2);
    CHECK(client::active_input_context().context == &outer);
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug rejects unauthorized semantic events before raw or semantic effects",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    const auto reason = GENERATE(
        std::string("schema"), std::string("unknown"), std::string("absent-copy"),
        std::string("cancel"), std::string("fill"), std::string("count"),
        std::string("malformed-choose"), std::string("target"));
    CAPTURE(reason);
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto options = support::prompt_options{.text = "semantic rejection report"};
    auto absent_copy_id = std::string{};
    if (reason == "absent-copy") {
        auto discovery_reads = 0;
        client::memory::set_input_provider([&](const int /*timeout*/) {
            if (++discovery_reads > 1) { throw std::runtime_error("copy discovery budget"); }
            absent_copy_id = choice_id(client::current_interaction(), "Copy");
            return input_event(' ', input_event_t::keyboard);
        });
        support::prompt(options);
        service.available = false;
    }
    auto outer = input_context{"DEBUG_REJECTION_OUTER"};
    auto binding = client::interaction_scope{
        outer, []() {
            auto snapshot = client::interaction_snapshot{};
            snapshot.title = "enclosing rejection provider";
            return snapshot;
        }};
    auto context = client::input_context_scope{outer, outer.category_name()};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 1) { throw std::runtime_error("rejection input budget"); }
        const auto snapshot = client::current_interaction();
        auto payload = client::interaction_event{};
        payload.operation = client::interaction_operation::choose;
        payload.schema_id = snapshot.schema_id;
        payload.target_id = choice_id(snapshot, "Ignore");
        if (reason == "schema") {
            payload.schema_id = "wrong-schema";
            payload.target_id = choice_id(snapshot, "Copy");
        }
        if (reason == "unknown") { payload.target_id = "not-offered"; }
        if (reason == "absent-copy") { payload.target_id = absent_copy_id; }
        if (reason == "cancel") {
            payload.operation = client::interaction_operation::cancel;
            payload.target_id.clear();
        }
        if (reason == "fill") {
            payload.operation = client::interaction_operation::fill;
            payload.value = "value";
            payload.submit = true;
        }
        if (reason == "count") {
            payload.operation = client::interaction_operation::set_count;
            payload.count = 1;
        }
        if (reason == "malformed-choose") { payload.value = "unauthorized value"; }
        if (reason == "target") {
            payload.operation = client::interaction_operation::set_target;
            payload.target_id.clear();
            payload.position = client::interaction_position{};
        }
        const auto command = client::interaction_command{
            .input_id = snapshot.input_id,
            .operation = payload.operation,
            .target_id = payload.target_id,
            .value = payload.value,
            .submit = payload.submit,
            .count = payload.count,
            .position = payload.position};
        const auto resolved = client::resolve_checked_interaction(
            {.command = command, .expected_schema = payload.schema_id});
        CHECK_FALSE(resolved.has_value());
        // A hostile event must not fall through to raw I, nor execute a semantic side effect.
        auto event = input_event('I', input_event_t::keyboard);
        event.interaction = payload;
        return event;
    });
    CHECK_THROWS_WITH(
        support::prompt(options), Catch::Matchers::Contains("Semantic interaction rejected:"));
    CHECK(reads == 1);
    CHECK(service.copies.empty());
    CHECK(client::active_input_context().context == &outer);
    CHECK(client::current_interaction().title == "enclosing rejection provider");
    auto retry_reads = 0;
    raw_sequence({' '}, retry_reads);
    support::prompt(options);
    CHECK(retry_reads == 1);
    CHECK(service.copies.empty());
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug rejects stale boundary commands across real raw reads",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto options = support::prompt_options{.text = "stale boundary report"};
    auto saved = client::interaction_command{};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 3) { throw std::runtime_error("stale input budget"); }
        const auto snapshot = client::current_interaction();
        if (reads == 1) {
            saved.input_id = snapshot.input_id - 1;
            saved.operation = client::interaction_operation::choose;
            saved.target_id = choice_id(snapshot, "Ignore");
            const auto stale = client::resolve_checked_interaction({.command = saved});
            REQUIRE_FALSE(stale.has_value());
            CHECK(stale.error().kind == client::interaction_rejection::stale_boundary);
            saved.input_id = snapshot.input_id;
            return input_event('?', input_event_t::keyboard);
        }
        if (reads == 2) {
            CHECK(snapshot.input_id > saved.input_id);
            const auto stale = client::resolve_checked_interaction({.command = saved});
            REQUIRE_FALSE(stale.has_value());
            CHECK(stale.error().kind == client::interaction_rejection::stale_boundary);
        }
        return semantic_choice(snapshot, "Continue");
    });
    support::prompt(options);
    CHECK(reads == 2);
    support::prompt(options);
    CHECK(reads == 3);
    CHECK(service.copies.empty());
}

TEST_CASE(
    "native debug rejects an earlier report schema without ignoring the new report",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    const auto before = support::prompt_options{.text = "earlier full report"};
    const auto after = support::prompt_options{.text = "changed full report"};
    auto stale = input_event{};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 3) { throw std::runtime_error("changed schema input budget"); }
        const auto snapshot = client::current_interaction();
        if (reads == 1) { stale = semantic_choice(snapshot, "Ignore"); }
        if (reads == 2) {
            REQUIRE(stale.interaction.has_value());
            CHECK(snapshot.schema_id != stale.interaction->schema_id);
            CHECK(snapshot.message.find(report(after)) != std::string::npos);
            return stale;
        }
        return semantic_choice(snapshot, "Continue");
    });
    support::prompt(before);
    CHECK_THROWS_WITH(
        support::prompt(after), Catch::Matchers::Contains("Semantic interaction rejected:"));
    support::prompt(after);
    CHECK(reads == 3);
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug semantic replay copies full reports and continues without live input",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    auto replay_file = debug_replay_file{};
    auto rng = restore_on_out_of_scope<cata_default_random_engine>{rng_get_engine()};
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto options = support::prompt_options{.text = "replayed full report\nreplay tail"};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 3) { throw std::runtime_error("replay record input budget"); }
        return semantic_choice(client::current_interaction(), reads < 3 ? "Copy" : "Continue");
    });
    replay::configure_recording(replay_file.path.string(), {.rng_seed = 424242});
    replay::start();
    support::prompt(options);
    replay::finish();
    REQUIRE(service.copies.size() == 2);
    client::memory::set_input_provider(failing_provider);
    replay::configure_playback(replay_file.path.string(), {.rng_seed = 424242});
    replay::start();
    support::prompt(options);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    CHECK(reads == 3);
    REQUIRE(service.copies.size() == 4);
    CHECK(std::ranges::all_of(service.copies, [&](const auto& text) {
        return text == report(options);
    }));
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug rejects hostile replay records before clipboard or ignored state changes",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    auto replay_file = debug_replay_file{};
    auto rng = restore_on_out_of_scope<cata_default_random_engine>{rng_get_engine()};
    const auto reason = GENERATE(
        std::string("schema"), std::string("unknown"), std::string("cancel"), std::string("fill"),
        std::string("count"), std::string("malformed"), std::string("boundary"));
    CAPTURE(reason);
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    const auto options = support::prompt_options{.text = "hostile replay report"};
    auto recorded = input_event{};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 1) { throw std::runtime_error("replay discovery input budget"); }
        recorded = semantic_choice(client::current_interaction(), "Ignore");
        return semantic_choice(client::current_interaction(), "Continue");
    });
    support::prompt(options);
    REQUIRE(recorded.interaction.has_value());
    auto& payload = *recorded.interaction;
    if (reason == "schema") { payload.schema_id = "wrong-schema"; }
    if (reason == "unknown") { payload.target_id = "not-offered"; }
    if (reason == "cancel") {
        payload.operation = client::interaction_operation::cancel;
        payload.target_id.clear();
    }
    if (reason == "fill") {
        payload.operation = client::interaction_operation::fill;
        payload.value = "value";
        payload.submit = true;
    }
    if (reason == "count") {
        payload.operation = client::interaction_operation::set_count;
        payload.count = 1;
    }
    if (reason == "malformed") { payload.value = "unauthorized value"; }
    replay::configure_recording(replay_file.path.string(), {.rng_seed = 424242});
    replay::start();
    replay::record_input_event(
        recorded,
        {.context = reason == "boundary" ? "WRONG_CONTEXT" : "DEBUG_MSG",
         .actions = {},
         .timeout_ms = inp_mngr.get_timeout()});
    replay::finish();
    client::memory::set_input_provider(failing_provider);
    replay::configure_playback(replay_file.path.string(), {.rng_seed = 424242});
    if (reason == "malformed") {
        CHECK_THROWS(replay::start());
    } else {
        replay::start();
        CHECK_THROWS(support::prompt(options));
    }
    replay::stop();
    CHECK(service.copies.empty());
    auto retry_reads = 0;
    raw_sequence({' '}, retry_reads);
    support::prompt(options);
    CHECK(retry_reads == 1);
    CHECK(service.copies.empty());
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug keeps full oversized reports and refuses bounded contract admission",
    "[client][debug_prompt][debug_prompt_semantic]") {
    auto fixture = native_fixture{};
    const auto oversized = GENERATE(false, true);
    const auto text =
        oversized
            ? std::string(engine_client::maximum_inline_bytes, 'x') + "\nOVERSIZED REPORT TAIL"
            : "ordinary admission report";
    const auto options = support::prompt_options{.text = text};
    const auto messages = fixture.messages;
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 1) { throw std::runtime_error("admission raw exit budget"); }
        const auto snapshot = client::current_interaction();
        REQUIRE(snapshot.structured);
        CHECK(snapshot.context == "DEBUG_MSG");
        CHECK(snapshot.message.find(report(options)) != std::string::npos);
        const auto admitted = engine_client::capture_boundary(
            {.epoch = "debug-admission-epoch",
             .ready = {.phase = "waiting_for_input",
                       .game_ready = false,
                       .accepts_interaction_commands = true}});
        if (oversized) {
            CHECK(snapshot.message.find("OVERSIZED REPORT TAIL") != std::string::npos);
            CHECK(snapshot.message.size() > engine_client::maximum_inline_bytes);
            REQUIRE_FALSE(admitted.has_value());
            CHECK(admitted.error() == engine_client::error::resource_limit);
        } else {
            REQUIRE(admitted.has_value());
            REQUIRE(admitted->interaction.has_value());
            CHECK(admitted->interaction->message == snapshot.message);
            CHECK_FALSE(admitted->ready.game_ready);
        }
        // A test-only native Space exits after observing admission; no command success is invented.
        return input_event(' ', input_event_t::keyboard);
    });
    support::prompt(options);
    CHECK(reads == 1);
    CHECK(serialized_messages() == messages);
}

TEST_CASE(
    "native debug preserves literal and color-like reports verbatim",
    "[client][debug_prompt][debug_prompt_color_red]") {
    auto fixture = native_fixture{};
    const auto diagnostic = GENERATE(
        std::string{"plain diagnostic control"},
        std::string{"literal <color_red>embedded token</color> remains data"},
        std::string{"literal </color> closing token remains data"},
        std::string{"literal unclosed prefix:\n<color_"},
        std::string{"literal overlapping prefixes: <color_<color_>REPORT TAIL"});
    const auto options = support::prompt_options{
        .filename = "literal_report.cpp",
        .line = "117",
        .function = "literal_producer",
        .text = diagnostic};
    const auto expected = literal_expected_report(options);
    auto clipboard = std::make_unique<clipboard_service>();
    auto& service = *clipboard;
    auto presentation = client::presentation_scope{std::move(clipboard)};
    auto observation = client::interaction_snapshot{};
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (++reads > 2) { throw std::runtime_error("literal report Copy/Space budget"); }
        if (reads == 1) { observation = client::current_interaction(); }
        return input_event(reads == 1 ? 'C' : ' ', input_event_t::keyboard);
    });
    support::prompt(options);
    CAPTURE(diagnostic, observation.message);
    INFO("Independent expected literal report:\n" << expected);
    CHECK(reads == 2);
    CHECK(observation.context == "DEBUG_MSG");
    CHECK(observation.structured);
    // Exact contiguous bytes include the diagnostic, function, filename, line and version.
    CHECK(observation.message.find(expected) != std::string::npos);
    REQUIRE(service.copies.size() == 1);
    CHECK(service.copies.front() == expected);
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug malformed prefixes obey a linear construction byte budget before admission",
    "[client][debug_prompt][debug_prompt_color_red]") {
    auto fixture = native_fixture{};
    auto previous_source_bytes = std::size_t{0};
    auto previous_message_bytes = std::size_t{0};
    const auto envelope_bytes = report_envelope_byte_budget();
    for (const auto count : std::array{4, 8, 16, 32}) {
        auto diagnostic = std::string{};
        std::ranges::for_each(std::views::iota(0, count), [&](const auto /*index*/) {
            diagnostic += "<color_";
        });
        diagnostic += ">" + std::string(count * 8, 't') + "UNIQUE CONSTRUCTION TAIL";
        const auto options = support::prompt_options{
            .filename = "bounded_literal_report.cpp",
            .line = "118",
            .function = "literal_construction_producer",
            .text = diagnostic};
        const auto expected = literal_expected_report(options);
        auto observation = client::interaction_snapshot{};
        auto reads = 0;
        auto admitted = false;
        client::memory::set_input_provider([&](const int /*timeout*/) {
            if (++reads > 1) { throw std::runtime_error("construction raw Space budget"); }
            observation = client::current_interaction();
            const auto state = engine_client::capture_boundary(
                {.epoch = "literal-report-budget-epoch",
                 .ready = {.phase = "waiting_for_input",
                           .game_ready = false,
                           .accepts_interaction_commands = true}});
            admitted = state.has_value();
            if (state) {
                REQUIRE(state->interaction.has_value());
                CHECK(state->interaction->message == observation.message);
            }
            // Exit only after observation; neither admission nor error content is hidden.
            return input_event(' ', input_event_t::keyboard);
        });
        support::prompt(options);
        CAPTURE(count, diagnostic.size(), expected.size(), envelope_bytes,
                observation.message.size());
        CHECK(reads == 1);
        CHECK(observation.context == "DEBUG_MSG");
        CHECK(observation.structured);
        CHECK(admitted);
        CHECK(expected.size() < 1024);
        CHECK(observation.message.find(expected) != std::string::npos);
        // Materialized bytes bound the minimum construction/hash/serialization work; no
        // clocks, allocator hooks, huge inputs or helper-under-test expected values are used.
        CHECK(observation.message.size() <= expected.size() + envelope_bytes);
        const auto first_tail = observation.message.find("UNIQUE CONSTRUCTION TAIL");
        REQUIRE(first_tail != std::string::npos);
        CHECK(observation.message.find("UNIQUE CONSTRUCTION TAIL", first_tail + 1)
              == std::string::npos);
        if (previous_source_bytes != 0) {
            const auto source_growth = expected.size() - previous_source_bytes;
            CHECK(observation.message.size() <= previous_message_bytes + source_growth);
        }
        previous_source_bytes = expected.size();
        previous_message_bytes = observation.message.size();
    }
    CHECK(serialized_messages() == fixture.messages);
}

TEST_CASE(
    "native debug preserves literal metadata and runtime backtrace paths",
    "[client][debug_prompt][debug_prompt_literal_metadata]") {
    auto fixture = native_fixture{};
    const auto saved_config = PATH_INFO::config_dir();
    const auto saved_options = PATH_INFO::options();
    const auto saved_autopickup = PATH_INFO::autopickup();
    const auto token =
        GENERATE(std::string{"<color_red>literal</color>"}, std::string{"<color_<color_>"});
    {
        const auto restore_paths = on_out_of_scope{[&]() {
            PATH_INFO::set_config_dir(saved_config);
            PATH_INFO::set_options(saved_options);
            PATH_INFO::set_autopickup(saved_autopickup);
        }};
        // Only logical path strings change; no files/directories or personal config are touched.
        PATH_INFO::set_config_dir(PATH_INFO::user_dir() + "literal-" + token + "/");
        const auto options = support::prompt_options{
            .filename = "source_" + token + ".cpp",
            .line = "119",
            .function = "producer_" + token,
            .text = "literal metadata diagnostic"};
        const auto expected = literal_expected_report(options);
        auto clipboard = std::make_unique<clipboard_service>();
        auto& service = *clipboard;
        auto presentation = client::presentation_scope{std::move(clipboard)};
        auto observation = client::interaction_snapshot{};
        auto reads = 0;
        client::memory::set_input_provider([&](const int /*timeout*/) {
            if (++reads > 2) { throw std::runtime_error("metadata Copy/Space budget"); }
            if (reads == 1) { observation = client::current_interaction(); }
            return input_event(reads == 1 ? 'C' : ' ', input_event_t::keyboard);
        });
        support::prompt(options);
        CAPTURE(token, observation.message);
        CHECK(reads == 2);
        CHECK(observation.context == "DEBUG_MSG");
        CHECK(observation.structured);
        CHECK(observation.message.find(expected) != std::string::npos);
#    if defined(BACKTRACE)
        // Independent oracle: normalization must precede inserting this runtime value.
        const auto instruction = "See " + PATH_INFO::debug() + " for a full stack backtrace";
        CHECK(observation.message.find(instruction) != std::string::npos);
#    endif
        REQUIRE(service.copies.size() == 1);
        CHECK(service.copies.front() == expected);
        CHECK(serialized_messages() == fixture.messages);
    }
    CHECK(PATH_INFO::config_dir() == saved_config);
    CHECK(PATH_INFO::options() == saved_options);
    CHECK(PATH_INFO::autopickup() == saved_autopickup);
}

#endif
