#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "engine_client_component.h"
#    include "engine_client_schema.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "game.h"
#    include "game_constants.h"
#    include "game_session.h"
#    include "input.h"
#    include "language.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "map_perception.h"
#    include "mcp_server.h"
#    include "messages.h"
#    include "mod_manager.h"
#    include "options_helpers.h"
#    include "path_info.h"
#    include "state_helpers.h"
#    include "translations.h"
#    include "weather/weather.h"
#    include "worldfactory.h"

#    include <algorithm>
#    include <chrono>
#    include <filesystem>
#    include <fstream>
#    include <iostream>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <utility>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;
using namespace locale_fixture;

auto strength = 8;
/// An avatar and an environment with words of their own.
auto test_text() -> ec::world_state {
    auto result = ec::world_state{};
    result.avatar = ec::avatar_value{
        .id = "e:avatar",
        .name = "Ada",
        .stats = {{"strength", _("Strength"), std::to_string(strength)}},
        .compass = {{.toward = {}, .label = _("North:")}}};
    result.avatar->sidebar.focus_label = _("Focus");
    result.environment = ec::environment_value{.turn = "1", .time = _("Dawn")};
    return result;
}

auto events_of(ec::push_reader& reader) -> std::vector<ec::public_event> {
    auto result = std::vector<ec::public_event>{};
    for (const auto& item : reader.take()) {
        if (const auto* event = std::get_if<ec::event_push>(&item)) {
            result.push_back(event->event);
        }
    }
    return result;
}

auto rpc(const std::string& method, const std::string& params, const std::string& id)
    -> std::string {
    return R"({"jsonrpc":"2.0","method":")" + method + R"(","params":)" + params + R"(,"id":)" + id
         + "}\n";
}
auto hello(const std::string& locale = {}) -> std::string {
    return rpc(
        "bn.hello",
        R"({"versions":["0.2"],"client":{"name":"t","version":"1"},"schema":")"
            + std::string{ec::engine_schema_hash} + '"'
            + (locale.empty() ? "" : R"(,"locale":")" + locale + '"') + "}",
        "1");
}
const auto subscribe = rpc("bn.subscribe", "{}", "2");

/// One connection to a session, which reads whatever it is sent and whatever the session publishes.
struct connection {
    std::ostringstream output;
    std::ostringstream errors;
    std::vector<bn::mcp::key_event> queued;
    bn::mcp::server server;
    explicit connection(ec::session& authority, const bool spectator)
        : server{
              {
                  .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
                  .submit =
                      [this](const auto& input) {
                          queued = input;
                          return true;
                      },
                  .has_input = [this] { return !queued.empty(); },
                  .contract_session = &authority,
              },
              {.spectator = spectator}} {}
    auto send(const std::string& text) -> void {
        auto input = std::istringstream{text};
        for (auto lines = std::ranges::count(text, '\n'); lines > 0; --lines) {
            REQUIRE(server.poll_input(input, output, errors));
        }
    }
    auto flush() -> void { REQUIRE(server.flush_pushed(output)); }
    /// Every frame written so far.
    auto frames() const -> std::vector<json> {
        auto result = std::vector<json>{};
        auto lines = std::istringstream{output.str()};
        for (auto line = std::string{}; std::getline(lines, line);) {
            if (!line.empty()) { result.push_back(json::parse(line)); }
        }
        return result;
    }
    /// The result of the request with this id.
    auto result(const int id) const -> json {
        for (const auto& frame : frames()) {
            if (frame.contains("id") && frame["id"] == id) { return frame.at("result"); }
        }
        FAIL("no response with id " << id << " in " << output.str());
        return {};
    }
    /// The events of every `bn.events` frame, in order.
    auto events() const -> std::vector<json> {
        auto result = std::vector<json>{};
        for (const auto& frame : frames()) {
            if (frame.value("method", "") != "bn.events") { continue; }
            for (const auto& event : frame["params"]["events"]) { result.push_back(event); }
        }
        return result;
    }
};
} // namespace

TEST_CASE(
    "a reader in another language is posted its own wording of what changed",
    "[engine_client_locale][engine_client_session]") {
    const auto fixture = korean_catalogue{};
    const auto boundary = yes_no_boundary{};
    auto authority = ec::session{};
    authority.set_world_capture(&test_text, nullptr, &test_text);
    auto english = authority.open_push();
    auto reader = authority.open_push();
    reader.use_locale(korean);
    strength = 8;
    REQUIRE(authority.publish_boundary());

    SECTION("a snapshot has the ids and the stack of the engine's, in the language asked") {
        const auto engine = authority.current();
        const auto other = authority.current(korean);
        REQUIRE(engine);
        REQUIRE(other);
        const auto& base = engine->value.interaction;
        const auto& worded = other->value.interaction;
        CHECK(worded.id == base.id);
        REQUIRE(worded.interaction);
        CHECK(worded.interaction->schema_id == base.interaction->schema_id);
        CHECK(worded.interaction->choices.front().id == base.interaction->choices.front().id);
        CHECK(base.interaction->choices.front().label == "Yes");
        CHECK(worded.interaction->choices.front().label == in_korean("Yes"));
        REQUIRE(worded.stack);
        REQUIRE(base.stack);
        CHECK(worded.stack->size() == base.stack->size());
        CHECK(other->at == engine->at);
        CHECK(engine->value.world.avatar->stats.front().label == "Strength");
        CHECK(other->value.world.avatar->stats.front().label == in_korean("Strength"));
        CHECK(other->value.world.environment->time == in_korean("Dawn"));
        // The sidebar and the compass panel are worded in the language too.
        CHECK(engine->value.world.avatar->compass.front().label == "North:");
        CHECK(other->value.world.avatar->compass.front().label == in_korean("North:"));
        CHECK(engine->value.world.avatar->sidebar.focus_label == "Focus");
        CHECK(other->value.world.avatar->sidebar.focus_label == in_korean("Focus"));
        // Only the words differ.
        CHECK(other->value.world.avatar->stats.front().value == "8");
        CHECK(other->value.world.avatar->id == engine->value.world.avatar->id);
        CHECK(authority.current(get_language().id)->value.world.avatar->stats.front().label
              == "Strength");
    }
    SECTION("a snapshot starts from the last publication, not from the live game") {
        strength = 50;
        const auto engine = authority.current();
        const auto other = authority.current(korean);
        REQUIRE(other);
        CHECK(engine->value.world.avatar->stats.front().value == "8");
        CHECK(other->value.world.avatar->stats.front().value == "8");
    }
    SECTION("a language no publication has captured is not ready") {
        auto late = authority.open_push();
        late.use_locale("de_DE");
        const auto none = authority.current("de_DE");
        REQUIRE_FALSE(none);
        CHECK(none.error() == ec::error::not_ready);
        authority.prepare_locale("de_DE");
        CHECK(authority.current("de_DE"));
    }
    SECTION("an event has the same sequence, revision and ids, with the words of its reader") {
        REQUIRE(authority.current(korean));
        static_cast<void>(events_of(english));
        static_cast<void>(events_of(reader));
        strength = 9;
        // A new native boundary: the screen is captured again, and so are the avatar and the
        // environment.
        game_client::begin_input_boundary();
        REQUIRE(authority.publish_boundary());
        // Other tests leave a message or a presentation fact in the game; only the screen change is
        // compared.
        const auto screen_changes = [](auto events) {
            std::erase_if(events, [](const auto& event) {
                return !event.value().delta.interaction;
            });
            return events;
        };
        const auto base = screen_changes(events_of(english));
        const auto worded = screen_changes(events_of(reader));
        REQUIRE(base.size() == 1);
        REQUIRE(worded.size() == base.size());
        const auto& left = base.front().value();
        const auto& right = worded.front().value();
        CHECK(right.sequence == left.sequence);
        CHECK(right.revision == left.revision);
        CHECK(right.type == left.type);
        REQUIRE(left.delta.interaction);
        REQUIRE(right.delta.interaction);
        CHECK(right.delta.interaction->id == left.delta.interaction->id);
        CHECK(right.delta.interaction->interaction->choices.front().id
              == left.delta.interaction->interaction->choices.front().id);
        CHECK(left.delta.interaction->interaction->choices.front().label == "Yes");
        CHECK(right.delta.interaction->interaction->choices.front().label == in_korean("Yes"));
        CHECK(right.delta.actions_unchanged == left.delta.actions_unchanged);
        REQUIRE(left.delta.avatar);
        REQUIRE(right.delta.avatar);
        CHECK(left.delta.avatar->stats.front().label == "Strength");
        CHECK(right.delta.avatar->stats.front().label == in_korean("Strength"));
        CHECK(right.delta.avatar->stats.front().value == "9");
        // The members that did not change are kept, as they are for the reader of the engine's
        // language.
        CHECK(right.delta.avatar_kept == left.delta.avatar_kept);
        // The wire the reader's event goes out as is sealed in the language too.
        CHECK(worded.back().wire().find("Yes-ko") != std::string::npos);
        CHECK(base.back().wire().find("Yes-ko") == std::string::npos);
    }
    SECTION("an event with no text in it is the engine's own for every reader") {
        REQUIRE(authority.current(korean));
        static_cast<void>(events_of(english));
        static_cast<void>(events_of(reader));
        Messages::clear_messages();
        Messages::add_msg("A line.");
        REQUIRE(authority.publish_boundary());
        const auto base = events_of(english);
        const auto worded = events_of(reader);
        REQUIRE(base.size() == 1);
        REQUIRE(worded.size() == 1);
        CHECK(worded.front().wire() == base.front().wire());
    }
    SECTION("a reader of the engine's own language has no variant of its own") {
        REQUIRE(authority.current(korean));
        auto same = authority.open_push();
        same.use_locale(get_language().id);
        strength = 9;
        game_client::begin_input_boundary();
        REQUIRE(authority.publish_boundary());
        const auto base = events_of(english);
        const auto own = events_of(same);
        REQUIRE(base.size() == 1);
        REQUIRE(own.size() == 1);
        CHECK(own.front().wire() == base.front().wire());
    }
}

TEST_CASE(
    "a spectator in another language and a controller read the same ids over the wire",
    "[engine_client_locale][engine_client_server]") {
    const auto fixture = korean_catalogue{};
    const auto boundary = yes_no_boundary{};
    Messages::clear_messages();
    Messages::add_msg("First line.");
    auto authority = ec::session{};
    authority.set_world_capture(&test_text, nullptr, &test_text);
    strength = 8;
    REQUIRE(authority.publish_boundary());
    auto controller = connection{authority, false};
    auto spectator = connection{authority, true};
    controller.send(hello());
    spectator.send(hello(korean));
    // No publication has worded the language yet; the subscribe makes one.
    controller.send(subscribe);
    spectator.send(subscribe);

    SECTION("the hello names the language granted and the engine's") {
        const auto answer = spectator.result(1);
        CHECK(answer["version"] == "0.2");
        CHECK(answer["locale"] == korean);
        CHECK(answer["engine_locale"] == json(get_language().id));
        CHECK(answer["role"] == "spectator");
        CHECK(controller.result(1)["locale"] == json(get_language().id));
        CHECK(controller.result(1)["engine_locale"] == json(get_language().id));
    }
    SECTION("the snapshot has the same ids and stack shape, with the words of the language") {
        const auto base = controller.result(2);
        const auto worded = spectator.result(2);
        const auto& left = base["interaction"];
        const auto& right = worded["interaction"];
        CHECK(right["boundary_id"] == left["boundary_id"]);
        CHECK(right["interaction"]["schema_id"] == left["interaction"]["schema_id"]);
        CHECK(right["interaction"]["choices"][0]["id"] == left["interaction"]["choices"][0]["id"]);
        CHECK(left["interaction"]["choices"][0]["label"] == "Yes");
        CHECK(right["interaction"]["choices"][0]["label"] == json(in_korean("Yes")));
        REQUIRE(left["stack"].size() == right["stack"].size());
        for (const auto index : std::views::iota(std::size_t{0}, left["stack"].size())) {
            CHECK(right["stack"][index]["screen_id"] == left["stack"][index]["screen_id"]);
        }
        CHECK(left["stack"].dump().find("Yes-ko") == std::string::npos);
        CHECK(right["stack"].dump().find("Yes-ko") != std::string::npos);
        CHECK(base["avatar"]["stats"][0]["label"] == "Strength");
        CHECK(worded["avatar"]["stats"][0]["label"] == json(in_korean("Strength")));
        CHECK(worded["environment"]["time"] == json(in_korean("Dawn")));
    }
    SECTION("a message line says it is in the engine's language, to a reader of another") {
        CHECK_FALSE(controller.result(2)["messages"][0].contains("lang"));
        CHECK(spectator.result(2)["messages"][0]["lang"] == json(get_language().id));
        CHECK(spectator.result(2)["messages"][0]["text"] == "First line.");
        controller.output.str("");
        spectator.output.str("");
        Messages::add_msg("Second line.");
        REQUIRE(authority.publish_boundary());
        controller.flush();
        spectator.flush();
        const auto base = controller.events();
        const auto worded = spectator.events();
        REQUIRE(base.size() == 1);
        REQUIRE(worded.size() == 1);
        CHECK(base.front()["type"] == "message.logged");
        CHECK_FALSE(base.front()["data"].contains("lang"));
        CHECK(worded.front()["data"]["lang"] == json(get_language().id));
        CHECK(worded.front()["data"]["text"] == "Second line.");
    }
    SECTION(
        "the snapshot names the catalogues of the connection's language, the game's then the mods'") {
        REQUIRE(std::ranges::contains(
            world_generator->active_world->info->active_mod_order, mod_id{"test_data"}));
        auto& mod = const_cast<MOD_INFORMATION&>(mod_id{"test_data"}.obj());
        const auto old_path = mod.path;
        const auto restore = on_out_of_scope([&]() { mod.path = old_path; });
        mod.path = (fixture.directory / "mod").string();
        // The game runs from its own directory with a relative base path; a reader runs elsewhere.
        PATH_INFO::init_base_path(std::filesystem::relative(fixture.directory).string());
        const auto base = on_out_of_scope([&]() {
            PATH_INFO::init_base_path(fixture.directory.string());
        });
        // The language without its dialect, as the game looks a mod's catalogue up.
        const auto mod_catalogue = fixture.directory / "mod/lang/ko.mo";
        std::filesystem::create_directories(mod_catalogue.parent_path());
        auto output = std::ofstream{mod_catalogue, std::ios::binary};
        const auto bytes = catalogue({{"Yes", "Yes-mod"}});
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        controller.send(rpc("bn.subscribe", "{}", "3"));
        spectator.send(rpc("bn.subscribe", "{}", "3"));
        CHECK_FALSE(controller.result(3).contains("catalogues"));
        const auto catalogues = spectator.result(3)["catalogues"];
        REQUIRE(catalogues.size() == 2);
        for (const auto& path : catalogues) {
            CHECK(std::filesystem::path{path.get<std::string>()}.is_absolute());
        }
        CHECK(std::filesystem::equivalent(
            catalogues[0].get<std::string>(),
            fixture.directory / "lang/mo" / korean / "LC_MESSAGES/cataclysm-bn.mo"));
        CHECK(std::filesystem::equivalent(catalogues[1].get<std::string>(), mod_catalogue));
    }
    SECTION("a converted line is the same LocalisedString for both readers and has no lang") {
        controller.output.str("");
        spectator.output.str("");
        add_msg(to_translation("The %s dies!"), std::string{"zombie"});
        REQUIRE(authority.publish_boundary());
        controller.flush();
        spectator.flush();
        const auto expected = json::parse(R"({"key":"The %s dies!","args":["zombie"]})");
        for (const auto* reader : {&controller, &spectator}) {
            const auto events = reader->events();
            REQUIRE(events.size() == 1);
            CHECK(events.front()["data"]["text"] == expected);
            CHECK(events.front()["data"]["label"] == expected);
            CHECK_FALSE(events.front()["data"].contains("lang"));
        }
    }
    SECTION("an event carries the screen and avatar of its connection's language") {
        controller.output.str("");
        spectator.output.str("");
        strength = 9;
        game_client::begin_input_boundary();
        REQUIRE(authority.publish_boundary());
        controller.flush();
        spectator.flush();
        const auto base = controller.events();
        const auto worded = spectator.events();
        REQUIRE(base.size() == 1);
        REQUIRE(worded.size() == 1);
        CHECK(worded.front()["sequence"] == base.front()["sequence"]);
        CHECK(worded.front()["revision"] == base.front()["revision"]);
        const auto& left = base.front()["changes"];
        const auto& right = worded.front()["changes"];
        CHECK(right["interaction"]["boundary_id"] == left["interaction"]["boundary_id"]);
        CHECK(left["interaction"]["interaction"]["choices"][0]["label"] == "Yes");
        CHECK(right["interaction"]["interaction"]["choices"][0]["label"] == json(in_korean("Yes")));
        CHECK(left["avatar"]["stats"][0]["label"] == "Strength");
        CHECK(right["avatar"]["stats"][0]["label"] == json(in_korean("Strength")));
        CHECK(right["avatar"].at("keep") == left["avatar"].at("keep"));
    }
}

TEST_CASE(
    "a hello asking for a language the engine cannot word is refused",
    "[engine_client_locale][engine_client_server]") {
    const auto fixture = korean_catalogue{};
    const auto boundary = yes_no_boundary{};
    auto authority = ec::session{};
    REQUIRE(authority.publish_boundary());
    auto spectator = connection{authority, true};
    for (const auto* locale : {"xx_XX", "de_DE"}) {
        spectator.output.str("");
        spectator.send(hello(locale));
        const auto out = spectator.output.str();
        INFO(out);
        CHECK(out.find(R"("kind":"unavailable_locale","message":")") != std::string::npos);
        CHECK(out.find(R"("action":"hello")") != std::string::npos);
        CHECK(out.find(R"("version":"0.2")") == std::string::npos);
    }
    // The refusal negotiates nothing, and the same connection may say hello again.
    spectator.output.str("");
    spectator.send(hello(korean));
    CHECK(spectator.result(1)["locale"] == korean);
}

TEST_CASE(
    "a session whose connections all read the engine's language is unchanged in bytes",
    "[engine_client_locale][engine_client_server]") {
    const auto fixture = korean_catalogue{};
    const auto boundary = yes_no_boundary{};
    Messages::clear_messages();
    Messages::add_msg("First line.");
    auto authority = ec::session{};
    authority.set_world_capture(&test_text, nullptr, &test_text);
    strength = 8;
    REQUIRE(authority.publish_boundary());
    auto implicit = connection{authority, true};
    auto named = connection{authority, true};
    implicit.send(hello() + subscribe);
    named.send(hello(get_language().id) + subscribe);
    strength = 9;
    Messages::add_msg("Second line.");
    game_client::begin_input_boundary();
    REQUIRE(authority.publish_boundary());
    implicit.flush();
    named.flush();
    CHECK_FALSE(implicit.output.str().empty());
    CHECK(named.output.str() == implicit.output.str());
    CHECK(implicit.output.str().find(R"("lang")") == std::string::npos);
}

/// Milliseconds a publication costs with no extra language and with one, for an avatar who steps on
/// open ground at noon in a reality bubble of 3: `cata_test-mcp "[.engine_client_locale_cost]"`
/// prints the median of each.
TEST_CASE("the cost of a publication by reader languages", "[.engine_client_locale_cost]") {
    const auto fixture = korean_catalogue{};
    const auto map_size = get_map().getmapsize();
    const auto bubble =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const auto restore = on_out_of_scope([&] {
        g->on_options_changed();
        get_map().resize(map_size);
        clear_all_state();
    });
    clear_all_state();
    const auto sized = override_option("REALITY_BUBBLE_SIZE", "3");
    g->on_options_changed();
    get_avatar().clear_map_memory();
    clear_map();
    make_open_air_uniform();
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    const auto start = tripoint_bub_ms(g_half_mapsize_x, g_half_mapsize_y, 0);
    get_map().invalidate_map_cache(start.z());
    const auto boundary = yes_no_boundary{};
    for (const auto languages : {0, 1}) {
        get_avatar().setpos(start);
        auto authority = ec::session{};
        authority.set_world_capture(
            &ec::world::capture_world, &ec::world::capture_patch, &ec::world::capture_worded);
        auto reader = authority.open_push();
        if (languages > 0) { reader.use_locale(korean); }
        map_perception::acquire();
        REQUIRE(authority.publish_boundary());
        if (languages > 0) { REQUIRE(authority.current(korean)); }
        auto elapsed = std::vector<double>{};
        for (const auto step : std::views::iota(0, 40)) {
            get_avatar().setpos(start + tripoint(step % 2 == 0 ? 1 : 0, 0, 0));
            calendar::turn += 1_turns;
            map_perception::acquire();
            const auto began = std::chrono::steady_clock::now();
            REQUIRE(authority.publish_boundary());
            elapsed.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                    .count());
            static_cast<void>(reader.take());
        }
        std::ranges::sort(elapsed);
        std::cerr << "bubble " << g_reality_bubble_size << ", extra languages " << languages << ": "
                  << elapsed[elapsed.size() / 2] << " ms/publication (median)\n";
    }
    // The part an extra language adds: the avatar and environment worded once more, with the
    // scope that swaps the catalogue and refills every translation cache.
    for (const auto scoped : {false, true}) {
        auto elapsed = std::vector<double>{};
        for (const auto step : std::views::iota(0, 40)) {
            static_cast<void>(step);
            const auto began = std::chrono::steady_clock::now();
            if (scoped) {
                const auto scope = localization::locale_scope{korean};
                static_cast<void>(ec::world::capture_worded());
            } else {
                static_cast<void>(ec::world::capture_worded());
            }
            elapsed.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                    .count());
        }
        std::ranges::sort(elapsed);
        std::cerr << "worded capture " << (scoped ? "in a locale scope: " : "as is: ")
                  << elapsed[elapsed.size() / 2] << " ms (median)\n";
    }
}

#endif
