#if defined(CATA_NATIVE_PREVIEW_TEST)

#    include "avatar.h"
#    include "cata_libintl.h"
#    include "cata_utility.h"
#    include "catalua_icallback_actor.h"
#    include "catalua_impl.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction_metadata.h"
#    include "client_interaction_prepared.h"
#    include "evaluation_decision.h"
#    include "game.h"
#    include "init.h"
#    include "item.h"
#    include "json.h"
#    include "language.h"
#    include "language_test.h"
#    include "messages.h"
#    include "options.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "player_helpers.h"
#    include "popup.h"
#    include "state_helpers.h"
#    include "translation_copy_test.h"
#    include "ui_manager.h"
#    include "worldfactory.h"

#    include <algorithm>
#    include <iostream>
#    include <limits>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <string_view>
#    include <vector>

#    if defined(CATA_MCP)
#        include "client_memory.h"
#        include "client_memory_scope.h"
#        include "cursesport.h"
#    endif

namespace {
namespace evaluation = game_client::evaluation;

auto evaluation_options() -> evaluation::options {
    return {
        .evaluation = {
            .session_epoch = 7,
            .engine_checkpoint = "test-owned-engine-checkpoint",
            .runtime_checkpoint = "test-owned-runtime-checkpoint",
            .inputs = "test-coat-inputs",
            .operation = "eligibility",
        }};
}

auto choice_reply(const evaluation::request& request, const std::string& action)
    -> evaluation::decision {
    const auto selected = std::ranges::
        find(request.interaction.choices, action, &game_client::interaction_choice::description);
    REQUIRE(selected != request.interaction.choices.end());
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::choose,
        .schema_id = request.interaction.schema_id,
        .target_id = selected->id,
    };
    return {.expected = request, .event = std::move(event)};
}

auto yes_no_once() -> query_popup::result {
    return query_popup()
        .context("YESNO")
        .message("%s", "Same prompt")
        .option("YES")
        .option("NO")
        .query_once();
}

auto messages() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    json.start_object();
    Messages::serialize(json);
    json.end_object();
    return stream.str();
}

/// A small real MO assembled at runtime, not a replacement translation resolver.
/// The native library parses/owns it; only the translated payload is large (32 KiB).
auto label_catalogue(const std::string_view source, const std::string_view translated)
    -> std::string {
    const auto metadata = std::string{
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "Plural-Forms: nplurals=2; plural=(n != 1);\n"};
    auto binary = std::string{};
    const auto word = [&](const std::uint32_t value) {
        for (const auto shift : {0, 8, 16, 24}) {
            binary.push_back(static_cast<char>((value >> shift) & 0xff));
        }
    };
    word(0x950412de); // Native little-endian GNU MO magic.
    word(0);
    word(2);
    word(28);
    word(44);
    word(0);
    word(0);
    word(0);
    word(60);
    word(source.size());
    word(61);
    const auto translated_offset = 62 + source.size();
    word(metadata.size());
    word(translated_offset);
    word(translated.size());
    word(translated_offset + metadata.size() + 1);
    binary.push_back('\0');
    binary += source;
    binary.push_back('\0');
    binary += metadata;
    binary.push_back('\0');
    binary += translated;
    binary.push_back('\0');
    return binary;
}

/// Native publication ownership restored, and eager caches invalidated, on every exit.
struct scoped_label_catalogue {
    l10n_data::testing::scoped_catalogues publication;

    scoped_label_catalogue(const std::string_view source, const std::string_view translated)
        : publication({label_catalogue(source, translated)}) {}
    auto replace(const std::string_view source, const std::string_view translated) -> void {
        publication.replace({label_catalogue(source, translated)});
    }
};

struct hook_fixture {
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    sol::table hooks = lua["game"]["hooks"];
    sol::object previous = hooks["on_character_try_wear"];
    std::string previous_messages = messages();
    on_out_of_scope cleanup{[this]() {
        hooks["on_character_try_wear"] = previous;
        clear_all_state();
        auto stream = std::istringstream(previous_messages);
        auto json = JsonIn(stream);
        Messages::deserialize(json.get_object());
    }};

    explicit hook_fixture(const std::string& mode) {
        clear_all_state();
        clear_character(get_avatar(), false);
        hooks["on_character_try_wear"] = lua.create_table();
        const auto loaded = lua.safe_script_file(
            "data/mods/TEST_DATA/evaluation_decision.lua", sol::script_pass_on_error);
        REQUIRE(loaded.valid());
        auto begin = loaded.get<sol::protected_function>();
        const auto result = begin(mode);
        REQUIRE(result.valid());
    }
};

#    if defined(CATA_MCP)
/// A real initialized compositor, real UI sentinel, and real native input/presentation callbacks.
/// Scope ownership restores borrowed windows and all providers on exceptional exits.
struct ui_probe {
    game_client::memory::scoped_state memory;
    restore_on_out_of_scope<bool> mode{test_mode};
    restore_on_out_of_scope<int> termx{TERMX};
    restore_on_out_of_scope<int> termy{TERMY};
    restore_on_out_of_scope<int> width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> height{FULL_SCREEN_HEIGHT};
    input_context parent_context{"EVALUATION_PARENT"};
    game_client::input_context_scope
        parent_scope{parent_context, parent_context.category_name(), 19};
    ui_adaptor sentinel;
    int reads = 0;
    int presents = 0;
    int redraws = 0;
    int resizes = 0;
    std::uint64_t input_id = game_client::current_input_id();
    game_client::interaction_work_counts work = game_client::interaction_work();
    game_client::screen_snapshot before;
    catacurses::window screen;
    catacurses::window new_screen;

    ui_probe() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::mvwprintw(catacurses::stdscr, point(2, 1), "sentinel");
        game_client::memory::draw_window(catacurses::stdscr);
        game_client::memory::set_cursor(1);
        game_client::memory::set_timeout(17);
        game_client::memory::set_input_provider([this](const int /*timeout*/) {
            ++reads;
            return input_event('n', input_event_t::keyboard);
        });
        game_client::memory::set_present_callback([this](const auto& /*screen*/) { ++presents; });
        sentinel.position(point_zero, point(100, 40));
        sentinel.on_redraw([this](const auto& /*ui*/) { ++redraws; });
        sentinel.on_screen_resize([this](const auto& /*ui*/) { ++resizes; });
        sentinel.mark_resize();
        before = game_client::memory::snapshot();
        screen = catacurses::stdscr;
        new_screen = catacurses::newscr;
    }

    auto check() const -> void {
        CHECK(reads == 0);
        CHECK(presents == 0);
        CHECK(redraws == 0);
        CHECK(resizes == 0);
        CHECK(game_client::current_input_id() == input_id);
        CHECK(game_client::active_input_context().context == &parent_context);
        CHECK(game_client::active_input_context().category == parent_context.category_name());
        CHECK(game_client::active_input_context().timeout_ms == 19);
        const auto after_work = game_client::interaction_work();
        CHECK(after_work.schema_hashes == work.schema_hashes);
        CHECK(after_work.hashed_choices == work.hashed_choices);
        CHECK(after_work.materialized_choices == work.materialized_choices);
        CHECK(after_work.id_comparisons == work.id_comparisons);
        const auto after = game_client::memory::snapshot();
        CHECK(after.width == before.width);
        CHECK(after.height == before.height);
        CHECK(after.text == before.text);
        CHECK(after.cursor == before.cursor);
        CHECK(after.cursor_visible == before.cursor_visible);
        CHECK(std::ranges::equal(after.cells, before.cells, [](const auto& a, const auto& b) {
            return a.text == b.text && a.foreground == b.foreground && a.background == b.background;
        }));
        CHECK(catacurses::stdscr == screen);
        CHECK(catacurses::newscr == new_screen);
    }
};
#    endif
} // namespace

TEST_CASE("evaluation decision identities bind actual popup occurrences", "[evaluation_decision]") {
    const auto opts = evaluation_options();
    const auto first = evaluation::evaluate(opts, yes_no_once);
    CHECK(first.evaluation.state == evaluation::status::requires_decision);
    CHECK_FALSE(first.value);
    REQUIRE(first.evaluation.pending);
    const auto request = *first.evaluation.pending;
    CHECK(request.evaluation == opts.evaluation);
    CHECK(request.occurrence == 1);
    CHECK(request.interaction.context == "YESNO");
    REQUIRE(request.interaction.choices.size() == 2);
    CHECK(request.interaction.choices[0].description == "YES");
    CHECK(request.interaction.choices[1].description == "NO");
    CHECK(request.popup_schema.find("bindings") != std::string::npos);
    CHECK_FALSE(evaluation::active());

    for (const auto& action : {std::string{"YES"}, std::string{"NO"}}) {
        auto explicit_input = opts;
        explicit_input.transcript.push_back(choice_reply(request, action));
        const auto completed = evaluation::evaluate(explicit_input, yes_no_once);
        CHECK(completed.evaluation.state == evaluation::status::completed);
        REQUIRE(completed.value);
        CHECK(completed.value->action == action);
        CHECK_FALSE(completed.value->wait_input);
    }

    // Every supplied identity field, callback occurrence, and complete request must match.
    const auto mismatch = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
    auto stale = opts;
    auto reply = choice_reply(request, "YES");
    switch (mismatch) {
        case 0:
            ++reply.expected.evaluation.session_epoch;
            break;
        case 1:
            reply.expected.evaluation.engine_checkpoint += "changed";
            break;
        case 2:
            reply.expected.evaluation.runtime_checkpoint += "changed";
            break;
        case 3:
            reply.expected.evaluation.inputs += "changed";
            break;
        case 4:
            reply.expected.evaluation.operation += "changed";
            break;
        case 5:
            ++reply.expected.occurrence;
            break;
        case 6:
            reply.expected.callbacks.push_back({.callback = "other", .occurrence = 1});
            break;
        case 7:
            reply.expected.interaction.message += "changed";
            break;
        case 8:
            reply.expected.popup_schema += "changed";
            break;
    }
    stale.transcript.push_back(std::move(reply));
    const auto denied = evaluation::evaluate(stale, yes_no_once);
    CHECK(denied.evaluation.state == evaluation::status::stale);
    CHECK_FALSE(denied.value);
    CHECK_FALSE(evaluation::active());
}

TEST_CASE(
    "evaluation decisions cannot escape via invalid native popup input", "[evaluation_decision]") {
    const auto opts = evaluation_options();
    const auto pending = evaluation::evaluate(opts, yes_no_once);
    REQUIRE(pending.evaluation.pending);
    auto reply = choice_reply(*pending.evaluation.pending, "YES");
    const auto invalid = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    switch (invalid) {
        case 0:
            reply.event.interaction->target_id = "not-an-option";
            break;
        case 1:
            reply.event.interaction->schema_id += "stale";
            break;
        case 2:
            reply.event.interaction->operation = game_client::interaction_operation::cancel;
            reply.event.interaction->target_id.clear();
            break;
        case 3:
            reply.event.interaction->value = "extra";
            break;
        case 4:
            reply.event.sequence.push_back('Y');
            break;
        case 5:
            reply.event = input_event(KEY_LEFT, input_event_t::keyboard);
            break;
        case 6:
            reply.event = input_event(KEY_RIGHT, input_event_t::keyboard);
            break;
        case 7:
            reply.event = input_event(MOUSE_MOVE, input_event_t::mouse);
            break;
        case 8:
            reply.event = input_event{};
            break;
        case 9: {
            const auto& bindings = inp_mngr.get_input_for_action("HELP_KEYBINDINGS", "YESNO");
            REQUIRE_FALSE(bindings.empty());
            reply.event = bindings.front();
            break;
        }
    }
    auto input = opts;
    input.transcript.push_back(std::move(reply));
    const auto result = evaluation::evaluate(input, yes_no_once);
    CHECK(result.evaluation.state == evaluation::status::failed);
    CHECK(result.evaluation.reason == evaluation::failure::invalid_response);
    CHECK_FALSE(result.value);
}

TEST_CASE("evaluation reuses native popup filters and confirmation", "[evaluation_decision]") {
    const auto force = override_option("FORCE_CAPITAL_YN", "true");
    const auto opts = evaluation_options();
    const auto query = []() { return query_yn("Allow case-sensitive coat?"); };
    const auto pending = evaluation::evaluate(opts, query);
    REQUIRE(pending.evaluation.pending);
    const auto& request = *pending.evaluation.pending;
    CHECK(request.interaction.message.find("Case Sensitive") != std::string::npos);
    const auto uppercase = GENERATE(false, true);
    auto input = opts;
    auto reply = choice_reply(request, "YES");
    reply.event = input_event(uppercase ? 'Y' : 'y', input_event_t::keyboard);
    input.transcript.push_back(std::move(reply));
    const auto result = evaluation::evaluate(input, query);
    if (uppercase) {
        CHECK(result.evaluation.state == evaluation::status::completed);
        REQUIRE(result.value);
        CHECK(*result.value);
    } else {
        CHECK(result.evaluation.state == evaluation::status::failed);
        CHECK_FALSE(result.value);
    }

    auto filter_calls = 0;
    const auto filtered = [&]() {
        return query_popup()
            .context("YESNO")
            .message("%s", "Filter probe")
            .option("YES",
                    [&](const auto& /*event*/) {
                        ++filter_calls;
                        return false;
                    })
            .query_once();
    };
    const auto filter_pending = evaluation::evaluate(opts, filtered);
    REQUIRE(filter_pending.evaluation.pending);
    CHECK(filter_calls == 0); // No effectful eligibility prediction when constructing a request.
    auto filter_input = opts;
    filter_input.transcript.push_back(choice_reply(*filter_pending.evaluation.pending, "YES"));
    const auto rejected = evaluation::evaluate(filter_input, filtered);
    CHECK(rejected.evaluation.state == evaluation::status::failed);
    CHECK_FALSE(rejected.value);
    CHECK(filter_calls == 1); // Actual native filter exactly once.

    auto confirm = choice_reply(*filter_pending.evaluation.pending, "YES");
    const auto& bindings = inp_mngr.get_input_for_action("CONFIRM", "YESNO");
    REQUIRE_FALSE(bindings.empty());
    confirm.event = bindings.front();
    filter_input.transcript = {confirm};
    const auto confirmed = evaluation::evaluate(filter_input, filtered);
    CHECK(confirmed.evaluation.state == evaluation::status::completed);
    REQUIRE(confirmed.value);
    CHECK(confirmed.value->action == "YES");
    CHECK(filter_calls == 1); // Native CONFIRM accepts the cursor without the key filter.
}

TEST_CASE("evaluation preserves native cancel and acknowledgement rules", "[evaluation_decision]") {
    const auto anykey = GENERATE(false, true);
    const auto once = GENERATE(false, true);
    const auto query = [&]() {
        auto popup = query_popup{};
        popup.context("YESNO")
            .message("%s", "Acknowledge")
            .allow_anykey(anykey)
            .allow_cancel(!anykey);
        return once ? popup.query_once() : popup.query();
    };
    const auto opts = evaluation_options();
    const auto pending = evaluation::evaluate(opts, query);
    REQUIRE(pending.evaluation.pending);
    const auto& request = *pending.evaluation.pending;
    REQUIRE(request.interaction.choices.size() == 1);
    CHECK(request.interaction.choices[0].id == "acknowledge");
    auto input = opts;
    input.transcript.push_back(choice_reply(request, anykey ? "ANY_INPUT" : "QUIT"));
    const auto result = evaluation::evaluate(input, query);
    CHECK(result.evaluation.state == evaluation::status::completed);
    REQUIRE(result.value);
    CHECK(result.value->action == (anykey ? "ANY_INPUT" : "QUIT"));
    if (!anykey) {
        input.transcript[0].event.interaction->operation =
            game_client::interaction_operation::cancel;
        input.transcript[0].event.interaction->target_id.clear();
        const auto canceled = evaluation::evaluate(input, query);
        CHECK(canceled.evaluation.state == evaluation::status::completed);
        REQUIRE(canceled.value);
        CHECK(canceled.value->action == "QUIT");
    } else {
        input.transcript[0].event = input_event('~', input_event_t::keyboard);
        const auto acknowledged = evaluation::evaluate(input, query);
        CHECK(acknowledged.evaluation.state == evaluation::status::completed);
        REQUIRE(acknowledged.value);
        CHECK(acknowledged.value->action == "ANY_INPUT");
    }
}

TEST_CASE(
    "evaluation completion is sticky across nesting exceptions and limits",
    "[evaluation_decision]") {
    const auto opts = evaluation_options();
    const auto nested = evaluation::evaluate(opts, [&]() {
        const auto inner = evaluation::evaluate(opts, yes_no_once);
        CHECK(inner.evaluation.state == evaluation::status::requires_decision);
        return true; // A permissive outer consumer cannot erase pending inner state.
    });
    CHECK(nested.evaluation.state == evaluation::status::requires_decision);
    CHECK_FALSE(nested.value);
    CHECK_FALSE(evaluation::active());
    const auto caught = evaluation::evaluate(opts, []() {
        try {
            yes_no_once();
        } catch (const std::exception&) {}
        return true;
    });
    CHECK(caught.evaluation.state == evaluation::status::requires_decision);
    CHECK_FALSE(caught.value);

    const auto thrown = evaluation::evaluate(opts, []() -> bool { throw std::bad_alloc{}; });
    CHECK(thrown.evaluation.state == evaluation::status::failed);
    CHECK_FALSE(thrown.value);
    CHECK_FALSE(evaluation::active());
    const auto ordinary = evaluation::evaluate(opts, []() -> bool {
        throw std::runtime_error("body");
    });
    CHECK(ordinary.evaluation.state == evaluation::status::failed);
    CHECK_FALSE(ordinary.value);

    auto bounded = opts;
    bounded.budget.requests = 0;
    CHECK(evaluation::evaluate(bounded, []() { return true; }).evaluation.reason
          == evaluation::failure::resource_limit);
    bounded = opts;
    bounded.budget.retained_bytes = 1;
    CHECK(evaluation::evaluate(bounded, yes_no_once).evaluation.reason
          == evaluation::failure::resource_limit);
    bounded = opts;
    bounded.budget.nesting = 1;
    const auto too_deep = evaluation::evaluate(bounded, [&]() {
        return evaluation::evaluate(bounded, []() { return true; }).value.has_value();
    });
    CHECK(too_deep.evaluation.state == evaluation::status::failed);
    CHECK_FALSE(too_deep.value);
    bounded = opts;
    bounded.evaluation.runtime_checkpoint.clear();
    CHECK(evaluation::evaluate(bounded, yes_no_once).evaluation.reason
          == evaluation::failure::invalid_identity);

    const auto pending = evaluation::evaluate(opts, yes_no_once);
    REQUIRE(pending.evaluation.pending);
    bounded = opts;
    bounded.transcript.push_back(choice_reply(*pending.evaluation.pending, "YES"));
    CHECK(evaluation::evaluate(bounded, []() { return true; }).evaluation.reason
          == evaluation::failure::unused_decisions);
    bounded.transcript.resize(129, bounded.transcript.front());
    CHECK(evaluation::evaluate(bounded, yes_no_once).evaluation.reason
          == evaluation::failure::resource_limit);
    CHECK_FALSE(evaluation::active());
}

TEST_CASE(
    "real bound evaluation eligibility preserves YES and NO without UI effects",
    "[evaluation_decision][evaluation_bound]") {
    const auto fixture = hook_fixture("normal");
    const auto coat = item::spawn(itype_id("test_preview_callback_coat"));
#    if defined(CATA_MCP)
    auto probe = ui_probe{};
#    endif
    const auto before_messages = messages();
    const auto operation = [&]() { return get_avatar().can_wear(*coat); };
    const auto opts = evaluation_options();
    const auto pending = evaluation::evaluate(opts, operation);
    CHECK(pending.evaluation.state == evaluation::status::requires_decision);
    CHECK_FALSE(pending.value);
    REQUIRE(pending.evaluation.pending);
    const auto& request = *pending.evaluation.pending;
    REQUIRE(request.callbacks.size() == 1);
    CHECK(request.callbacks[0].callback.starts_with("hook:on_character_try_wear:"));
    CHECK(request.callbacks[0].occurrence == 1);
    CHECK(request.interaction.message == "Allow this fixture coat?");
    for (const auto& answer : {std::string{"YES"}, std::string{"NO"}}) {
        auto input = opts;
        input.transcript.push_back(choice_reply(request, answer));
        const auto result = evaluation::evaluate(input, operation);
        CHECK(result.evaluation.state == evaluation::status::completed);
        REQUIRE(result.value);
        CHECK(result.value->success() == (answer == "YES"));
        if (answer == "NO") { CHECK(result.value->str() == "Fixture coat refused."); }
    }
    CHECK(messages() == before_messages);
#    if defined(CATA_MCP)
    probe.check();
#    endif
}

TEST_CASE(
    "actual Lua pcall and nested bound callbacks cannot complete pending decisions",
    "[evaluation_decision][evaluation_bound]") {
    const auto mode = GENERATE(std::string{"pcall"}, std::string{"nested"}, std::string{"error"});
    const auto fixture = hook_fixture(mode);
    const auto coat = item::spawn(itype_id("test_preview_callback_coat"));
#    if defined(CATA_MCP)
    auto probe = ui_probe{};
#    endif
    const auto before_messages = messages();
    const auto result = evaluation::evaluate(evaluation_options(), [&]() {
        return get_avatar().can_wear(*coat);
    });
    CHECK_FALSE(result.value);
    if (mode == "error") {
        CHECK(result.evaluation.state == evaluation::status::failed);
    } else {
        CHECK(result.evaluation.state == evaluation::status::requires_decision);
        REQUIRE(result.evaluation.pending);
        CHECK(result.evaluation.pending->callbacks.size() == (mode == "nested" ? 2 : 1));
    }
    CHECK(messages() == before_messages);
    CHECK_FALSE(evaluation::active());
#    if defined(CATA_MCP)
    probe.check();
#    endif
}

TEST_CASE(
    "identical bound prompts require separate explicit interaction occurrences",
    "[evaluation_decision][evaluation_bound]") {
    const auto fixture = hook_fixture("repeat");
    const auto coat = item::spawn(itype_id("test_preview_callback_coat"));
    const auto operation = [&]() { return get_avatar().can_wear(*coat); };
    const auto opts = evaluation_options();
    const auto first = evaluation::evaluate(opts, operation);
    REQUIRE(first.evaluation.pending);
    auto input = opts;
    input.transcript.push_back(choice_reply(*first.evaluation.pending, "YES"));
    const auto second = evaluation::evaluate(input, operation);
    CHECK(second.evaluation.state == evaluation::status::requires_decision);
    CHECK_FALSE(second.value);
    REQUIRE(second.evaluation.pending);
    CHECK(second.evaluation.pending->occurrence == 2);
    CHECK(second.evaluation.pending->interaction.message
          == first.evaluation.pending->interaction.message);
    CHECK(second.evaluation.pending->callbacks == first.evaluation.pending->callbacks);
    input.transcript.push_back(input.transcript.front());
    const auto wrong_occurrence = evaluation::evaluate(input, operation);
    CHECK(wrong_occurrence.evaluation.state == evaluation::status::stale);
    CHECK_FALSE(wrong_occurrence.value);
    input.transcript.back() = choice_reply(*second.evaluation.pending, "NO");
    const auto completed = evaluation::evaluate(input, operation);
    CHECK(completed.evaluation.state == evaluation::status::completed);
    REQUIRE(completed.value);
    CHECK_FALSE(completed.value->success());
    CHECK(completed.value->str() == "Fixture coat refused.");
}

TEST_CASE(
    "real item callback defaults and protected conversion retain evaluation failure",
    "[evaluation_decision][evaluation_bound]") {
    auto& lua = DynamicDataLoader::get_instance().lua->lua;
    const auto loaded = lua.safe_script_file(
        "data/mods/TEST_DATA/evaluation_decision.lua", sol::script_pass_on_error);
    REQUIRE(loaded.valid());
    auto begin = loaded.get<sol::protected_function>();
    const auto item_callback = begin("item_pcall");
    REQUIRE(item_callback.valid());
    auto function = item_callback.get<sol::protected_function>();
    const auto actor = lua_iwearable_actor(
        "test_preview_callback_coat", sol::protected_function{sol::lua_nil},
        sol::protected_function{sol::lua_nil}, std::move(function),
        sol::protected_function{sol::lua_nil});
    const auto coat = item::spawn(itype_id("test_preview_callback_coat"));
    const auto result = evaluation::evaluate(evaluation_options(), [&]() {
        return actor.call_can_wear(get_avatar(), *coat);
    });
    CHECK(result.evaluation.state == evaluation::status::requires_decision);
    CHECK_FALSE(result.value);
    REQUIRE(result.evaluation.pending);
    CHECK(result.evaluation.pending->callbacks[0].callback
          == "iwearable:test_preview_callback_coat:can_wear");

    const auto bad = begin("item_bad");
    REQUIRE(bad.valid());
    auto bad_function = bad.get<sol::protected_function>();
    const auto malformed = lua_iwearable_actor(
        "test_preview_callback_coat", sol::protected_function{sol::lua_nil},
        sol::protected_function{sol::lua_nil}, std::move(bad_function),
        sol::protected_function{sol::lua_nil});
    const auto failed = evaluation::evaluate(evaluation_options(), [&]() {
        return malformed.call_can_wear(get_avatar(), *coat);
    });
    CHECK(failed.evaluation.state == evaluation::status::failed);
    CHECK_FALSE(failed.value);
}

TEST_CASE(
    "evaluation admits before snapshot schema and transcript allocation",
    "[evaluation_decision][evaluation_resource]") {
    const auto opts = evaluation_options();
#    if defined(CATA_MCP)
    auto probe = ui_probe{};
#    endif
    const auto large = std::string(1048577, 'x');
    auto popup = query_popup{};
    popup.context("YESNO").message("%s", large).option("YES").option("NO");
    const auto description = evaluation::evaluate(opts, [&]() { return popup.query_once(); });
    CHECK(description.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(description.value);
    CHECK(description.evaluation.work.constructed_requests == 0);
    CHECK(description.evaluation.work.peak_construction_bytes <= opts.budget.retained_bytes);
    CHECK(description.evaluation.work.retained_bytes <= opts.budget.retained_bytes);

    auto oversized = opts;
    const auto pending = evaluation::evaluate(opts, yes_no_once);
    REQUIRE(pending.evaluation.pending);
    oversized.transcript.push_back(choice_reply(*pending.evaluation.pending, "YES"));
    oversized.transcript[0].expected.interaction.choices[0].description = large;
    auto invoked = false;
    const auto transcript = evaluation::evaluate(oversized, [&]() {
        invoked = true;
        return true;
    });
    CHECK(transcript.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(invoked);
    CHECK(transcript.evaluation.work.constructed_requests == 0);
    CHECK(transcript.evaluation.work.retained_bytes == 0);

    auto source = *pending.evaluation.pending;
    source.interaction.message = large;
    const auto direct = evaluation::evaluate(opts, [&]() { return evaluation::consume(source); });
    CHECK(direct.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(direct.value);
    CHECK(direct.evaluation.work.retained_bytes <= opts.budget.retained_bytes);

    oversized = opts;
    oversized.transcript.push_back(choice_reply(*pending.evaluation.pending, "YES"));
    oversized.transcript[0].event.text.assign(200000, 'x');
    const auto response = evaluation::evaluate(oversized, yes_no_once);
    CHECK(response.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(response.value);
    CHECK(response.evaluation.work.retained_bytes <= opts.budget.retained_bytes);

    oversized = opts;
    oversized.transcript.push_back(choice_reply(*pending.evaluation.pending, "YES"));
    oversized.transcript[0].expected.interaction.choices[0].columns.resize(10000);
    CHECK(evaluation::evaluate(oversized, yes_no_once).evaluation.reason
          == evaluation::failure::resource_limit);

    // The registry is non-const; the public view is const. Mutate only under RAII restoration.
    auto& events = const_cast<std::vector<input_event>&>(
        inp_mngr.get_input_for_action("YES", "YESNO"));
    auto saved = events;
    const auto cleanup = on_out_of_scope([&]() { events = std::move(saved); });
    events = {input_event('Y', input_event_t::keyboard)};
    events[0].sequence.resize(100000, 'Y');
    const auto bindings = evaluation::evaluate(opts, yes_no_once);
    CHECK(bindings.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(bindings.value);
    CHECK(bindings.evaluation.work.constructed_requests == 0);
    CHECK(bindings.evaluation.work.retained_bytes <= opts.budget.retained_bytes);
    const auto arithmetic = evaluation::evaluate(opts, []() {
        auto budget = evaluation::construction_budget{};
        budget.add_array(std::numeric_limits<std::size_t>::max(), 2);
        return true;
    });
    CHECK(arithmetic.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(arithmetic.value);
    const auto callback = evaluation::evaluate(opts, [&]() {
        auto invocation = evaluation::callback_scope({large});
        return true;
    });
    CHECK(callback.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(callback.value);
#    if defined(CATA_MCP)
    probe.check();
#    endif
}

TEST_CASE(
    "evaluation owns response values across legal caller and returned aliases",
    "[evaluation_decision]") {
    auto opts = evaluation_options();
    const auto pending = evaluation::evaluate(opts, yes_no_once);
    REQUIRE(pending.evaluation.pending);
    opts.transcript.push_back(choice_reply(*pending.evaluation.pending, "YES"));
    auto& caller_event = *opts.transcript[0].event.interaction;
    const auto result = evaluation::evaluate(opts, [&]() {
        caller_event.target_id = "caller-mutated";
        auto response = yes_no_once();
        CHECK(response.action == "YES");
        REQUIRE(response.evt.interaction);
        response.evt.interaction->target_id = "returned-mutated";
        CHECK(caller_event.target_id == "caller-mutated");
        return response;
    });
    REQUIRE(result.value);
    CHECK(result.evaluation.state == evaluation::status::completed);
    CHECK(result.value->evt.interaction->target_id == "returned-mutated");
    CHECK(opts.transcript[0].event.interaction->target_id == "caller-mutated");
    opts.transcript[0].expected.interaction.message = "changed-after-result";
    CHECK(pending.evaluation.pending->interaction.message == "Same prompt");
}

TEST_CASE(
    "evaluation terminal extraction cannot revive moved pending state", "[evaluation_decision]") {
    {
        auto boundary = evaluation::scope(evaluation_options());
        CHECK_THROWS_AS(yes_no_once(), evaluation::interrupted);
        const auto first = boundary.finish();
        REQUIRE(first.pending);
        CHECK(first.state == evaluation::status::requires_decision);
        const auto second = boundary.finish();
        CHECK(second.state == evaluation::status::failed);
        CHECK(second.reason == evaluation::failure::already_finished);
        CHECK_THROWS_AS(yes_no_once(), evaluation::interrupted);
        CHECK(first.pending->interaction.message == "Same prompt");
    }
    CHECK_FALSE(evaluation::active());
    const auto opts = evaluation_options();
    for (const auto fault :
         {evaluation::construction_fault::allocation,
          evaluation::construction_fault::interrupted}) {
        evaluation::inject_construction_failure(fault);
        const auto failed = evaluation::evaluate(opts, []() { return true; });
        CHECK(failed.evaluation.state == evaluation::status::failed);
        CHECK(failed.evaluation.reason == evaluation::failure::exception);
        CHECK_FALSE(failed.value);
        CHECK_FALSE(evaluation::active());
        const auto ancestor = evaluation::evaluate(opts, [&]() {
            evaluation::inject_construction_failure(fault);
            const auto child = evaluation::evaluate(opts, []() { return true; });
            CHECK_FALSE(child.value);
            CHECK(evaluation::active());
            return true;
        });
        CHECK(ancestor.evaluation.state == evaluation::status::failed);
        CHECK_FALSE(ancestor.value);
        CHECK_FALSE(evaluation::active());
    }
}

TEST_CASE(
    "nested decision retention stays bounded and unused scopes stay sticky",
    "[evaluation_decision]") {
    auto opts = evaluation_options();
    const auto pending = evaluation::evaluate(opts, yes_no_once);
    REQUIRE(pending.evaluation.pending);
    auto child_opts = opts;
    child_opts.transcript.push_back(choice_reply(*pending.evaluation.pending, "YES"));
    const auto forgotten = evaluation::evaluate(opts, [&]() {
        { auto child = evaluation::scope(child_opts); }
        return true;
    });
    CHECK(forgotten.evaluation.reason == evaluation::failure::unused_decisions);
    CHECK_FALSE(forgotten.value);
    opts.budget.retained_bytes = 4096;
    auto popup = query_popup{};
    popup.context("YESNO").message("%s", std::string(2000, 'x')).option("YES").option("NO");
    const auto bounded = evaluation::evaluate(opts, [&]() {
        const auto child = evaluation::evaluate(evaluation_options(), [&]() {
            return popup.query_once();
        });
        CHECK(child.evaluation.state == evaluation::status::requires_decision);
        REQUIRE(child.evaluation.pending);
        return true;
    });
    CHECK(bounded.evaluation.state == evaluation::status::failed);
    CHECK(bounded.evaluation.reason == evaluation::failure::resource_limit);
    CHECK(bounded.evaluation.work.retained_bytes <= opts.budget.retained_bytes);
    CHECK_FALSE(bounded.value);
    CHECK_FALSE(evaluation::active());
}

TEST_CASE(
    "translated native labels must be admitted before their owned copies",
    "[evaluation_decision][translated_label_red]") {
    const auto warm = GENERATE(false, true);
    const auto once = GENERATE(false, true);
    auto ctxt = input_context({.category = "YESNO", .mode = input_context_mode::metadata});
    const auto source = ctxt.action_name_source("YES");
    REQUIRE(source);
    const auto& native_translation = source->get();
    REQUIRE(native_translation.debug_get_raw() == "Yes");
    auto opts = evaluation_options();
    opts.budget.retained_bytes = 24576;
    const auto query = [&]() {
        auto popup = query_popup{};
        popup.context("YESNO").message("%s", "Translated label budget").option("YES");
        return once ? popup.query_once() : popup.query();
    };
    // Guard against a meaningless green from rejecting unrelated bindings before the label.
    auto positive_observe = translation_testing::scoped_copy_observer(native_translation);
    const auto normal = evaluation::evaluate(opts, query);
    CHECK(positive_observe.work().owned_returns == 1);
    CHECK(positive_observe.work().owned_return_bytes == 3);
    CHECK(positive_observe.work().cache_fills <= 1);
    REQUIRE(normal.evaluation.state == evaluation::status::requires_decision);
    REQUIRE(normal.evaluation.pending);
    REQUIRE(normal.evaluation.pending->interaction.choices.front().label.size() <= 16);
    const auto translated = std::string(32768, 'x');
    const auto catalogue = scoped_label_catalogue(native_translation.debug_get_raw(), translated);
    REQUIRE(std::string(detail::_translate_internal("Yes")) == translated);
    // A successful native accessor control proves actual translation and owned-copy work.
    {
        auto observe = translation_testing::scoped_copy_observer(native_translation);
        CHECK(ctxt.get_action_name("YES") == translated);
        CHECK(observe.work().cache_fills == 1);
        CHECK(observe.work().owned_returns == 1);
        CHECK(observe.work().owned_return_bytes == translated.size());
    }
    if (!warm) { invalidate_translations(); }
    auto observe = translation_testing::scoped_copy_observer(native_translation);
    const auto result = evaluation::evaluate(opts, query);
    const auto work = observe.work();
    std::cout << "TRANSLATED_LABEL warm=" << warm << " once=" << once
              << " raw_bytes=" << native_translation.debug_get_raw().size() << " translated_bytes="
              << translated.size() << " budget=" << opts.budget.retained_bytes
              << " normal_construction_bytes=" << normal.evaluation.work.peak_construction_bytes
              << " cache_fills=" << work.cache_fills << " cache_fill_bytes="
              << work.cache_fill_bytes << " owned_returns=" << work.owned_returns
              << " owned_return_bytes=" << work.owned_return_bytes
              << " constructed_requests=" << result.evaluation.work.constructed_requests << '\n';
    CAPTURE(warm, once, work.cache_fills, work.cache_fill_bytes, work.owned_return_bytes);
    CHECK(result.evaluation.state == evaluation::status::failed);
    CHECK(result.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(result.value);
    CHECK(result.evaluation.work.constructed_requests == 0);
    // RED: the seam's owned return copy has already completed before rejection.
    // These expectations must become green via pre-copy resolution, not counter suppression.
    CHECK(work.owned_returns == 0);
    CHECK(work.owned_return_bytes == 0);
    CHECK(work.cache_fills == 0);
    CHECK(work.cache_fill_bytes == 0);
    CHECK_FALSE(evaluation::active());
}

#    if defined(CATA_MCP)
TEST_CASE(
    "evaluation characterizes native popup response precedence combinations",
    "[evaluation_decision][popup_response_matrix]") {
    const auto anykey = GENERATE(false, true);
    const auto cancel = GENERATE(false, true);
    const auto accept = GENERATE(false, true);
    const auto once = GENERATE(false, true);
    const auto kind = GENERATE(
        std::string{"raw_choice"}, std::string{"semantic_choice"}, std::string{"raw_cancel"},
        std::string{"semantic_cancel"}, std::string{"raw_confirm"}, std::string{"raw_unbound"});
    CAPTURE(anykey, cancel, accept, once, kind);
    auto reads = 0;
    auto native_phase = true;
    auto display_filter_calls = 0;
    auto filter_calls = 0;
    const auto query = [&]() {
        auto popup = query_popup{};
        popup.context("YESNO")
            .message("%s", "Native response matrix")
            .allow_anykey(anykey)
            .allow_cancel(cancel)
            .option("YES", [&](const auto& /*event*/) {
                // Native fold_query/get_desc filters labels before the first input read.
                // Evaluation must not predict by invoking these display-time filters.
                if (native_phase && reads == 0) {
                    ++display_filter_calls;
                } else {
                    ++filter_calls;
                }
                return accept;
            });
        return once ? popup.query_once() : popup.query();
    };
    const auto binding = [](const auto& action) {
        const auto& events = inp_mngr.get_input_for_action(action, "YESNO");
        REQUIRE_FALSE(events.empty());
        return events.front();
    };
    const auto make_event = [&](const auto& snapshot) {
        if (kind == "raw_choice") { return binding("YES"); }
        if (kind == "raw_cancel") { return binding("QUIT"); }
        if (kind == "raw_confirm") { return binding("CONFIRM"); }
        if (kind == "raw_unbound") { return input_event('~', input_event_t::keyboard); }
        auto event = input_event{};
        event.type = input_event_t::interaction;
        event.interaction = game_client::interaction_event{
            .operation =
                kind == "semantic_cancel"
                    ? game_client::interaction_operation::cancel
                    : game_client::interaction_operation::choose,
            .schema_id = snapshot.schema_id,
            .target_id = kind == "semantic_cancel" ? "" : "option:0",
        };
        return event;
    };
    auto native = std::optional<query_popup::result>{};
    auto native_error = std::string{};
    auto expected_display_filter_calls = 0;
    {
        auto probe = ui_probe{};
        // Compare display work with the actual native label utility, not a guessed
        // binding count: get_desc can stop at the first inline label match.
        auto label_context = input_context(
            {.category = "YESNO", .mode = input_context_mode::metadata});
        static_cast<void>(label_context.get_desc(
            "YES", label_context.get_action_name("YES"), [&](const auto& /*event*/) {
                ++expected_display_filter_calls;
                return accept;
            }));
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            ++reads;
            REQUIRE(reads <= 2); // Fail rather than hang if native precedence changes.
            return reads == 1 ? make_event(game_client::current_interaction()) : binding("CONFIRM");
        });
        try {
            native = query();
        } catch (const std::runtime_error& error) { native_error = error.what(); }
    }
    const auto native_filter_calls = filter_calls;
    native_phase = false;
    filter_calls = 0;
    auto probe = ui_probe{};
    const auto opts = evaluation_options();
    const auto pending = evaluation::evaluate(opts, query);
    REQUIRE(pending.evaluation.pending);
    CHECK(filter_calls == 0);
    auto input = opts;
    input.transcript.push_back({
        .expected = *pending.evaluation.pending,
        .event = make_event(pending.evaluation.pending->interaction),
    });
    const auto result = evaluation::evaluate(input, query);
    probe.check();
    std::cout << "POPUP_MATRIX anykey=" << anykey << " cancel=" << cancel << " accept=" << accept
              << " once=" << once << " reply=" << kind
              << " native_action=" << (native ? native->action : "exception")
              << " native_wait=" << (native ? native->wait_input : false)
              << " native_reads=" << reads << " native_filters=" << native_filter_calls
              << " native_display_filters=" << display_filter_calls << " evaluation_completed="
              << (result.evaluation.state == evaluation::status::completed)
              << " evaluation_filters=" << filter_calls << '\n';
    const auto rejected = !accept && (kind == "raw_choice" || kind == "semantic_choice");
    const auto nonterminal = rejected && (kind == "semantic_choice" || !anykey);
    const auto ignored_raw =
        !anykey && (kind == "raw_unbound" || (kind == "raw_cancel" && !cancel));
    const auto invalid_semantic = kind == "semantic_cancel" && !cancel;
    if (invalid_semantic) {
        CHECK_FALSE(native);
        CHECK(native_error.find("Semantic interaction rejected") != std::string::npos);
        CHECK(reads == 1);
    } else {
        REQUIRE(native);
        CHECK(native_error.empty());
        CHECK(native->wait_input == (nonterminal && once));
        CHECK(reads == (ignored_raw || (nonterminal && !once) ? 2 : 1));
        const auto expected_action =
            cancel && (kind == "raw_cancel" || kind == "semantic_cancel") ? "QUIT"
            : anykey && (kind == "raw_unbound" || (kind == "raw_cancel" && !cancel))
                ? "ANY_INPUT"
                : "YES";
        CHECK(native->action == expected_action);
    }
    CHECK(display_filter_calls == expected_display_filter_calls);
    CHECK(native_filter_calls == (kind == "raw_choice" || kind == "semantic_choice" ? 1 : 0));
    CHECK(filter_calls == (kind == "raw_choice" || kind == "semantic_choice" ? 1 : 0));
    if (nonterminal || ignored_raw || invalid_semantic) {
        CHECK(result.evaluation.state == evaluation::status::failed);
        CHECK(result.evaluation.reason == evaluation::failure::invalid_response);
        CHECK_FALSE(result.value);
    } else {
        CHECK(result.evaluation.state == evaluation::status::completed);
        REQUIRE(result.value);
        REQUIRE(native);
        CHECK(result.value->action == native->action);
        CHECK_FALSE(result.value->wait_input);
    }
}
#    endif

TEST_CASE(
    "catalogue fixtures restore owned publications across replacement and exceptional exits",
    "[evaluation_decision][catalogue_publication_scope]") {
    REQUIRE(world_generator);
    REQUIRE(world_generator->active_world);
    const auto initialized = GENERATE(false, true);
    const auto loaded = GENERATE(false, true);
    const auto exceptional = GENERATE(false, true);
    CAPTURE(initialized, loaded, exceptional);
    const auto ambient = l10n_data::pin_library();
    const auto ambient_loaded = l10n_data::mod_catalogues_are_loaded();
    const auto language = get_language().id;
    const auto option = get_option<std::string>("USE_LANG");
    const auto base_path = PATH_INFO::base_path();
    const auto mods = world_generator->active_world->info->active_mod_order;
    const auto source = to_translation("__catalogue_scope_label__");
    const auto frozen = source.snapshot();
    const auto ambient_text = source.translated();
    auto first = localization::locale_snapshot{};
    auto second = localization::locale_snapshot{};
    {
        // Also isolate setup's native mod-loaded transitions from the ambient test session.
        auto restore_ambient = l10n_data::testing::scoped_catalogues{{}};
        l10n_data::unload_catalogues();
        if (loaded) { l10n_data::load_mod_catalogues(); }
        REQUIRE(l10n_data::mod_catalogues_are_loaded() == loaded);
        restore_ambient.replace(
            initialized ? std::vector<std::string>{label_catalogue(source.debug_get_raw(), "prior")}
                        : std::vector<std::string>{});
        const auto prior = l10n_data::pin_library();
        const auto prior_text = source.translated();
        CHECK(prior_text == (initialized ? "prior" : "__catalogue_scope_label__"));
        const auto prior_version = detail::get_current_language_version();
        const auto bad_constructor = []() {
            const auto rejected = l10n_data::testing::scoped_catalogues{{"not a catalogue"}};
        };
        CHECK_THROWS_AS(bad_constructor(), std::runtime_error);
        CHECK(&l10n_data::get_library() == &prior.library());
        CHECK(l10n_data::pin_library().fingerprint() == prior.fingerprint());
        CHECK(l10n_data::mod_catalogues_are_loaded() == loaded);
        CHECK(detail::get_current_language_version() == prior_version);
        auto exit_version = prior_version;
        const auto exercise = [&]() {
            auto catalogue = scoped_label_catalogue(source.debug_get_raw(), "first");
            first = l10n_data::pin_library();
            CHECK(&first.library() != &prior.library());
            CHECK(first.fingerprint() != prior.fingerprint());
            CHECK(detail::get_current_language_version() != prior_version);
            CHECK(source.translated() == "first");
            CHECK(frozen.translated(prior) == prior_text);
            const auto first_version = detail::get_current_language_version();
            CHECK_THROWS_AS(catalogue.publication.replace({"not a catalogue"}), std::runtime_error);
            CHECK(&l10n_data::get_library() == &first.library());
            CHECK(l10n_data::pin_library().fingerprint() == first.fingerprint());
            CHECK(detail::get_current_language_version() == first_version);
            CHECK(l10n_data::mod_catalogues_are_loaded() == loaded);
            CHECK(source.translated() == "first");
            catalogue.replace(source.debug_get_raw(), "second");
            second = l10n_data::pin_library();
            CHECK(&second.library() != &first.library());
            CHECK(second.fingerprint() != first.fingerprint());
            CHECK(detail::get_current_language_version() != first_version);
            CHECK(source.translated() == "second");
            CHECK(frozen.translated(first) == "first");
            const auto second_version = detail::get_current_language_version();
            {
                const auto nested = scoped_label_catalogue(source.debug_get_raw(), "nested");
                CHECK(source.translated() == "nested");
                CHECK(frozen.translated(second) == "second");
                CHECK(detail::get_current_language_version() != second_version);
            }
            CHECK(&l10n_data::get_library() == &second.library());
            CHECK(l10n_data::pin_library().fingerprint() == second.fingerprint());
            CHECK(l10n_data::mod_catalogues_are_loaded() == loaded);
            CHECK(source.translated() == "second");
            CHECK(detail::get_current_language_version() != second_version);
            // Native publication/flag changes inside the fixture must also be unwound.
            l10n_data::unload_catalogues();
            CHECK_FALSE(l10n_data::mod_catalogues_are_loaded());
            CHECK(source.translated() == "__catalogue_scope_label__");
            exit_version = detail::get_current_language_version();
            if (exceptional) { throw std::runtime_error("fixture body"); }
        };
        if (exceptional) {
            CHECK_THROWS_AS(exercise(), std::runtime_error);
        } else {
            exercise();
        }
        CHECK(&l10n_data::get_library() == &prior.library());
        CHECK(l10n_data::pin_library().fingerprint() == prior.fingerprint());
        CHECK(l10n_data::mod_catalogues_are_loaded() == loaded);
        CHECK(detail::get_current_language_version() != exit_version);
        CHECK(source.translated() == prior_text);
        CHECK(frozen.translated(first) == "first");
        CHECK(frozen.translated(second) == "second");
    }
    CHECK(&l10n_data::get_library() == &ambient.library());
    CHECK(l10n_data::pin_library().fingerprint() == ambient.fingerprint());
    CHECK(l10n_data::mod_catalogues_are_loaded() == ambient_loaded);
    CHECK(source.translated() == ambient_text);
    CHECK(frozen.translated(ambient) == ambient_text);
    CHECK(get_language().id == language);
    CHECK(get_option<std::string>("USE_LANG") == option);
    CHECK(PATH_INFO::base_path() == base_path);
    CHECK(world_generator->active_world->info->active_mod_order == mods);
}

TEST_CASE(
    "bounded native translation preserves fallback and exact byte limits",
    "[evaluation_decision][bounded_translation]") {
    const auto max = std::numeric_limits<std::size_t>::max();
    auto catalogue = scoped_label_catalogue("one", "longer");
    const auto source = translation::to_translation("one");
    auto observe = translation_testing::scoped_copy_observer(source);
    CHECK_FALSE(source.translated_bounded(5));
    CHECK(observe.work().cache_fills == 0);
    CHECK(observe.work().owned_returns == 0);
    CHECK(source.translated_bounded(6) == "longer");
    CHECK(observe.work().cache_fills == 1);
    CHECK(observe.work().owned_returns == 1);
    CHECK(source.translated_bounded(max, 200) == "longer"); // Count ignored without plural.
    CHECK(observe.work().cache_fills == 1);
    CHECK(observe.work().owned_returns == 2);
    CHECK_FALSE(source.translated_bounded(5));
    CHECK(observe.work().owned_returns == 2);
    CHECK(source.translated() == "longer");
    CHECK(observe.work().owned_returns == 3);
    // Exercise maximum arithmetic in the cold C-string probe, not just the warm check.
    const auto cold_maximum = translation::to_translation("one");
    auto maximum_observe = translation_testing::scoped_copy_observer(cold_maximum);
    CHECK(cold_maximum.translated_bounded(max) == "longer");
    CHECK(maximum_observe.work().cache_fills == 1);
    CHECK(maximum_observe.work().owned_return_bytes == 6);
    const auto missing = translation::to_translation("missing");
    auto missing_observe = translation_testing::scoped_copy_observer(missing);
    CHECK_FALSE(missing.translated_bounded(6));
    CHECK(missing_observe.work().cache_fills == 0);
    CHECK(missing.translated_bounded(7) == "missing");
    CHECK(missing_observe.work().cache_fills == 1);
    CHECK(missing_observe.work().owned_returns == 1);
    const auto raw = translation::no_translation(std::string{"a\0b", 3});
    auto raw_observe = translation_testing::scoped_copy_observer(raw);
    CHECK_FALSE(raw.translated_bounded(2));
    CHECK(raw_observe.work().owned_returns == 0);
    CHECK(raw.translated_bounded(3) == std::string("a\0b", 3));
    CHECK(raw_observe.work().cache_fills == 0);
    CHECK(raw_observe.work().owned_returns == 1);
    CHECK(raw.translated_bounded(max) == raw.translated());
    const auto empty = translation{};
    auto empty_observe = translation_testing::scoped_copy_observer(empty);
    CHECK(empty.translated_bounded(0) == "");
    CHECK(empty_observe.work().cache_fills == 0);
    CHECK(empty_observe.work().owned_returns == 1);
}

TEST_CASE(
    "bounded translation keeps context plural language and allocation keys coherent",
    "[evaluation_decision][bounded_translation]") {
    const auto key = std::string{"ctx\4one"} + '\0' + "many";
    const auto forms = std::string{"un"} + '\0' + "plusieurs";
    auto catalogue = scoped_label_catalogue(key, forms);
    const auto source = translation::pl_translation("ctx", "one", "many");
    auto observe = translation_testing::scoped_copy_observer(source);
    CHECK(source.translated_bounded(2, 1) == "un");
    CHECK(observe.work().cache_fills == 1);
    CHECK_FALSE(source.translated_bounded(2, 2));
    CHECK(observe.work().cache_fills == 1);
    CHECK(observe.work().owned_returns == 1);
    CHECK(source.translated_bounded(2, 1) == "un"); // Rejected count did not alter key.
    CHECK(observe.work().cache_fills == 1);
    CHECK(source.translated_bounded(9, 2) == "plusieurs");
    CHECK(observe.work().cache_fills == 2);
    CHECK_FALSE(source.translated_bounded(1, 1));
    CHECK(source.translated(2) == "plusieurs");
    CHECK(observe.work().cache_fills == 2);
    catalogue.replace(key, std::string{"fresh"} + '\0' + "fresh_plural");
    CHECK_FALSE(source.translated_bounded(9, 2)); // Old language fits; fresh_plural must not reuse
                                                  // it.
    CHECK(observe.work().cache_fills == 2);
    CHECK(source.translated_bounded(5, 1) == "fresh");
    CHECK(observe.work().cache_fills == 3);
    auto fault_cleanup = on_out_of_scope([]() {
        translation_testing::inject_copy_failure(translation_testing::copy_fault::none);
    });
    translation_testing::inject_copy_failure(translation_testing::copy_fault::cache_fill);
    CHECK_THROWS_AS(source.translated_bounded(12, 2), std::bad_alloc);
    CHECK(observe.work().cache_fills == 3);
    CHECK(source.translated_bounded(5, 1) == "fresh"); // Failed fill left old count/content
                                                       // coherent.
    CHECK(observe.work().cache_fills == 3);
    const auto returns = observe.work().owned_returns;
    translation_testing::inject_copy_failure(translation_testing::copy_fault::owned_return);
    CHECK_THROWS_AS(source.translated_bounded(12, 2), std::bad_alloc);
    CHECK(observe.work().cache_fills == 4); // Successful fill remains coherent if return fails.
    CHECK(observe.work().owned_returns == returns);
    CHECK(source.translated_bounded(12, 2) == "fresh_plural");
    CHECK(observe.work().cache_fills == 4);
    catalogue.replace(key, std::string{"next"} + '\0' + "next_plural");
    translation_testing::inject_copy_failure(translation_testing::copy_fault::cache_fill);
    CHECK_THROWS_AS(source.translated_bounded(11, 2), std::bad_alloc);
    CHECK(observe.work().cache_fills == 4);
    CHECK(source.translated_bounded(11, 2) == "next_plural");
    CHECK(observe.work().cache_fills == 5);
    const auto absent_context = translation::to_translation("other", "one");
    CHECK(absent_context.translated_bounded(3) == "one");
    const auto absent_plural = translation::pl_translation("absent", "several");
    CHECK(absent_plural.translated_bounded(6, 1) == "absent");
    CHECK_FALSE(absent_plural.translated_bounded(6, 2));
    CHECK(absent_plural.translated_bounded(7, 2) == "several");
    const auto contextual = translation::to_translation("ctx", "one");
    CHECK(contextual.translated_bounded(4) == "next");
}

TEST_CASE(
    "bounded input names retain overrides local masks and native fallback",
    "[evaluation_decision][bounded_translation]") {
    auto catalogue = scoped_label_catalogue("override", "Expanded override");
    auto ctxt = input_context({.category = "YESNO", .mode = input_context_mode::metadata});
    ctxt.register_action("YES", translation::to_translation("override"));
    const auto source = ctxt.action_name_source("YES");
    REQUIRE(source);
    auto observe = translation_testing::scoped_copy_observer(source->get());
    CHECK_FALSE(ctxt.get_action_name_bounded("YES", 16));
    CHECK(observe.work().cache_fills == 0);
    CHECK(observe.work().owned_returns == 0);
    CHECK(ctxt.get_action_name_bounded("YES", 17) == "Expanded override");
    CHECK(observe.work().cache_fills == 1);
    CHECK(observe.work().owned_returns == 1);
    CHECK(ctxt.get_action_name("YES") == "Expanded override");
    CHECK(observe.work().owned_returns == 2);
    auto masked = input_context({.category = "PICKUP", .mode = input_context_mode::metadata});
    auto global = input_context({.category = "default", .mode = input_context_mode::metadata});
    const auto local_source = masked.action_name_source("LEFT");
    const auto default_source = global.action_name_source("LEFT");
    REQUIRE(local_source);
    REQUIRE(default_source);
    REQUIRE(&local_source->get() != &default_source->get());
    // These views refer to genuinely mutable native registry translations. Restore them,
    // rather than adding a test-only API exposing the input manager's private map.
    auto& local = const_cast<translation&>(local_source->get());
    auto& defaults = const_cast<translation&>(default_source->get());
    auto old_local = local;
    auto old_default = defaults;
    auto cleanup = on_out_of_scope([&]() {
        local = std::move(old_local);
        defaults = std::move(old_default);
    });
    local = translation{};
    const auto expected = masked.get_action_name("LEFT");
    CHECK(expected == defaults.translated());
    CHECK(masked.get_action_name_bounded("LEFT", expected.size()) == expected);
    REQUIRE_FALSE(expected.empty());
    CHECK_FALSE(masked.get_action_name_bounded("LEFT", expected.size() - 1));
    defaults = translation{};
    CHECK_FALSE(masked.action_name_source("LEFT"));
    CHECK(masked.get_action_name("LEFT") == "LEFT");
    CHECK_FALSE(masked.get_action_name_bounded("LEFT", 3));
    CHECK(masked.get_action_name_bounded("LEFT", 4) == "LEFT");
}

TEST_CASE(
    "text construction capacity uses checked maximum size arithmetic",
    "[evaluation_decision][bounded_translation]") {
    auto opts = evaluation_options();
    opts.budget.retained_bytes = std::numeric_limits<std::size_t>::max();
    auto invoked = false;
    const auto excessive = evaluation::evaluate(opts, [&]() {
        invoked = true;
        return true;
    });
    CHECK(excessive.evaluation.reason == evaluation::failure::resource_limit);
    CHECK_FALSE(invoked); // Preserve the broker's hard 1 MiB budget ceiling.
    opts.budget.retained_bytes = 1048576;
    auto boundary = evaluation::scope(opts);
    auto budget = evaluation::construction_budget{};
    const auto capacity = budget.remaining_text_capacity();
    CHECK(capacity <= opts.budget.retained_bytes / 32);
    REQUIRE(capacity > 0);
    budget.add_array(capacity, 32);
    CHECK(budget.remaining_text_capacity() == 0);
    budget.add_text("");
    CHECK_THROWS_AS(
        budget.add_bytes(std::numeric_limits<std::size_t>::max()), evaluation::interrupted);
    CHECK_THROWS_AS(budget.remaining_text_capacity(), evaluation::interrupted);
    const auto result = boundary.finish();
    CHECK(result.state == evaluation::status::failed);
    CHECK(result.reason == evaluation::failure::resource_limit);
}

#endif // CATA_NATIVE_PREVIEW_TEST
