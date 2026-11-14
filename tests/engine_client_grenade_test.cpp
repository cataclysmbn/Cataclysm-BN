#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "engine_client_event.h"
#include "engine_client_presentation.h"
#include "engine_client_world.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "map/field.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "monster.h"
#include "options.h"
#include "options_helpers.h"
#include "ranged.h"
#include "rng.h"
#include "sounds.h"
#include "state_helpers.h"
#include "weather/weather.h"

#include <algorithm>
#include <fstream>
#include <locale>
#include <nlohmann/json.hpp>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

using namespace engine_client;

namespace {

constexpr auto you_at = tripoint_bub_ms(62, 62, 0);
constexpr auto landing = tripoint_bub_ms(67, 62, 0);

enum class presentation_mode { detached, animated, silenced };

struct grenade_run {
    std::vector<presentation_value> facts;
    std::string rng;
    std::string state;
};

auto avatar_save() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    get_avatar().serialize(json);
    return stream.str();
}

/// Every run starts from the same avatar: the helpers' reset keeps skill practice and focus.
auto restore_avatar(const std::string& save) -> void {
    auto stream = std::istringstream{save};
    auto json = JsonIn{stream};
    get_avatar().deserialize(json);
}

/// Everything gameplay owns around the landing square, the monsters and the avatar's save.
auto authority() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    auto& here = get_map();
    json.start_object();
    json.member("cells");
    json.start_array();
    for (const auto& pos : here.points_in_radius(landing, 6)) {
        json.start_array();
        json.write(pos);
        json.write(here.ter(pos).id().str());
        json.write(here.furn(pos).id().str());
        for (const auto& [id, entry] : here.field_at(pos)) {
            json.write(id.id().str());
            json.write(entry.get_field_intensity());
        }
        for (const auto& it : here.i_at(pos)) { it->serialize(json); }
        json.end_array();
    }
    json.end_array();
    json.member("monsters");
    json.start_array();
    for (const auto& mon : g->all_monsters()) { mon.serialize(json); }
    json.end_array();
    json.member("avatar");
    get_avatar().serialize(json);
    json.end_object();
    return stream.str();
}

/// Pulls the pin, throws a grenade at `landing` past a zombie and a door, and lets the fuse burn.
auto throw_grenade(const presentation_mode mode, const std::string& start) -> grenade_run {
    rng_set_engine_seed(424242);
    clear_all_state();
    restore_avatar(start);
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    sounds::reset_sounds();
    get_avatar().setpos(you_at);
    auto& here = get_map();
    here.ter_set(landing + tripoint_south, ter_id("t_door_c"));
    here.build_map_cache(0);
    map_perception::acquire();
    spawn_test_monster("mon_zombie", landing + tripoint_east);
    const auto live = restore_on_out_of_scope(test_mode);
    test_mode = mode != presentation_mode::silenced;
    const override_option
        animations("ANIMATIONS", mode == presentation_mode::silenced ? "false" : "true");
    const override_option projectiles("ANIMATION_PROJECTILES", "true");
    const override_option stepwise("BULLETS_AS_LASERS", "false");
    const override_option instant("ANIMATION_DELAY", "0");
    presentation::collect(mode != presentation_mode::detached);
    presentation::take();
    auto& you = get_avatar();
    you.set_moves(1000);
    auto grenade = item::spawn("grenade");
    REQUIRE(grenade->type->invoke(you, *grenade, you_at) == 0);
    REQUIRE(grenade->is_active());
    you.mod_stamina(0);
    ranged::throw_item(you, landing, std::move(grenade), std::nullopt);
    for (auto turn = 0; turn < 7; ++turn) {
        calendar::turn += 1_turns;
        here.process_items();
        explosion_handler::get_explosion_queue().execute();
        here.batch_flood_fill_sounds();
        sounds::process_sound_markers(&you);
    }
    auto result = grenade_run{.facts = presentation::take()};
    presentation::collect(false);
    auto engine = std::ostringstream{};
    engine.imbue(std::locale::classic());
    engine << rng_get_engine();
    result.rng = engine.str();
    result.state = authority();
    return result;
}

/// The avatar every run starts as.
auto baseline() -> std::string {
    clear_all_state();
    return avatar_save();
}

auto types_of(const std::vector<presentation_value>& facts) -> std::vector<std::string> {
    return facts | std::views::transform(&presentation_value::type)
         | std::ranges::to<std::vector>();
}
/// The index of the first fact of `type` at or after `from`, or the size when there is none.
auto first_of(
    const std::vector<std::string>& types, const std::string& type, const std::size_t from = 0)
    -> std::size_t {
    return std::ranges::find(types.begin() + from, types.end(), type) - types.begin();
}

} // namespace

TEST_CASE("a thrown grenade publishes its events in causal order", "[engine_client_grenade]") {
    const auto run = throw_grenade(presentation_mode::animated, baseline());
    const auto types = types_of(run.facts);
    CAPTURE(types);
    const auto pin = first_of(types, "item.activated");
    const auto thrown = first_of(types, "item.thrown", pin);
    const auto flight = first_of(types, "projectile.moved", thrown);
    const auto landed = first_of(types, "item.landed", flight);
    const auto tick = first_of(types, "item.activated", landed);
    const auto bang = first_of(types, "explosion.started", tick);
    const auto hurt = first_of(types, "entity.damaged", bang);
    const auto smoke = first_of(types, "field.changed", bang);
    const auto door = first_of(types, "terrain.changed", bang);
    const auto done = first_of(types, "explosion.ended", bang);
    REQUIRE(done < types.size());
    CHECK(pin == 0);
    CHECK(thrown == pin + 1);
    CHECK(flight == thrown + 1);
    CHECK(landed > flight);
    CHECK(tick > landed);
    CHECK(bang > tick);
    CHECK(hurt > bang);
    CHECK(smoke > bang);
    CHECK(door > bang);
    CHECK(hurt < done);
    CHECK(smoke < done);
    CHECK(door < done);
    CHECK(std::ranges::count(types, "item.thrown") == 1);
    CHECK(std::ranges::count(types, "item.landed") == 1);
    // The projectile id ties the throw, its flight and its landing together.
    CHECK(run.facts[thrown].id == run.facts[flight].id);
    CHECK(run.facts[thrown].id == run.facts[landed].id);
    CHECK(run.facts[thrown].origin == run.facts[pin].at);
    CHECK(run.facts[landed].at == run.facts[thrown].target);
    // The fuse counts down in whole turns.
    auto fuse = std::vector<std::uint64_t>{};
    for (const auto& fact : run.facts | std::views::take(bang)) {
        if (fact.type == "item.activated") { fuse.push_back(fact.turns.value()); }
    }
    CHECK(fuse.front() == 5);
    CHECK(fuse.back() == 0);
    // It never skips a turn: each report is the last one or one less.
    CHECK(std::ranges::
              adjacent_find(fuse, [](const auto a, const auto b) { return b != a && b + 1 != a; })
          == fuse.end());
    // The zombie next to the landing square was hurt by an id the client can follow.
    CHECK(run.facts[hurt].entity.starts_with("e:"));
    CHECK(run.facts[hurt].amount > 0);
    // The door beside it was smashed.
    CHECK(run.facts[door].previous->id == "t_door_c");
    CHECK(run.facts[door].appearance->id != run.facts[door].previous->id);
    CHECK(run.facts[door].at == engine_client::world::position_at(landing + tripoint_south));
    CHECK(run.facts[smoke].field == "fd_smoke");
    // What the blast makes is heard as the avatar hears it; a fuse of 20 dB is under the ambient
    // noise.
    auto heard = std::vector<std::string>{};
    for (const auto& fact : run.facts) {
        if (fact.type == "sound.heard") { heard.push_back(fact.text); }
    }
    CAPTURE(heard);
    CHECK(first_of(types, "sound.heard", bang) < types.size());
}

TEST_CASE(
    "a grenade out of sight publishes only what the avatar hears", "[engine_client_grenade]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    auto& here = get_map();
    for (const auto& wall : here.points_in_radius(you_at, 2)) {
        const auto reach =
            std::max(std::abs(wall.x() - you_at.x()), std::abs(wall.y() - you_at.y()));
        if (reach == 2) { here.ter_set(wall, ter_id("t_wall")); }
    }
    const auto far_away = tripoint_bub_ms(70, 70, 0);
    get_avatar().setpos(you_at);
    here.invalidate_visibility_caches();
    here.build_map_cache(0);
    map_perception::acquire();
    REQUIRE_FALSE(get_avatar().sees(far_away));
    spawn_test_monster("mon_zombie", far_away);
    presentation::collect(true);
    presentation::take();
    auto grenade = item::spawn("grenade");
    REQUIRE(grenade->type->invoke(get_avatar(), *grenade, far_away) == 0);
    // Pulling the pin happens in the avatar's hand; the fuse burns out of sight.
    presentation::take();
    here.add_item(far_away, std::move(grenade));
    for (auto turn = 0; turn < 7; ++turn) {
        calendar::turn += 1_turns;
        here.process_items();
        explosion_handler::get_explosion_queue().execute();
    }
    const auto facts = presentation::take();
    presentation::collect(false);
    const auto types = types_of(facts);
    CAPTURE(types);
    // The zombie dies in the blast; the native player plays its death at the volume the avatar
    // hears it from there, with no direction. Nothing else of the grenade reaches the avatar.
    const auto heard =
        sfx::get_heard_volume(far_away, 75) * get_option<int>("SOUND_EFFECT_VOLUME") / 100;
    CHECK(heard > 0);
    const auto death = presentation_value{
        .type = "sound.played",
        .sound = sound_cue{.id = "mon_death", .variant = "zombie_death", .volume = heard}};
    CHECK(facts == std::vector{death});
}

TEST_CASE(
    "presentation leaves a thrown grenade's authority and RNG untouched",
    "[engine_client_grenade]") {
    const auto start = baseline();
    const auto bare = throw_grenade(presentation_mode::detached, start);
    const auto again = throw_grenade(presentation_mode::detached, start);
    CHECK(again.state == bare.state);
    const auto animated = throw_grenade(presentation_mode::animated, start);
    const auto silenced = throw_grenade(presentation_mode::silenced, start);
    if (const auto* dir = std::getenv("BN_GRENADE_DUMP")) {
        std::ofstream(std::string(dir) + "/bare.json") << bare.state;
        std::ofstream(std::string(dir) + "/animated.json") << animated.state;
        std::ofstream(std::string(dir) + "/again.json") << again.state;
    }
    CHECK(bare.facts.empty());
    CHECK_FALSE(animated.facts.empty());
    // The ANIMATIONS option is the player's to draw with; the facts of the grenade do not depend on
    // it.
    CHECK_FALSE(silenced.facts.empty());
    CHECK(types_of(silenced.facts) == types_of(animated.facts));
    for (const auto* drawn : {"projectile.moved", "explosion.started", "explosion.ended"}) {
        CAPTURE(drawn);
        CHECK(first_of(types_of(silenced.facts), drawn) < silenced.facts.size());
    }
    CHECK(animated.rng == bare.rng);
    CHECK(animated.state == bare.state);
    CHECK(silenced.rng == bare.rng);
    CHECK(silenced.state == bare.state);
}

TEST_CASE("the facts of a thrown grenade survive the wire in order", "[engine_client_grenade]") {
    const auto run = throw_grenade(presentation_mode::animated, baseline());
    auto initial = state_value{.interaction = {.id = "boundary:1", .ready = {.phase = "menu"}}};
    auto created = event_stream::create("epoch:grenade", initial);
    REQUIRE(created);
    auto stream = std::move(*created);
    auto batch = event_batch{.epoch = "epoch:grenade"};
    for (const auto& fact : run.facts) {
        auto published = stream.publish_presentation({.fact = fact});
        REQUIRE(published);
        batch.events.push_back(*published);
    }
    const auto wire = nlohmann::json::parse(serialize_events(batch));
    if (const auto* dir = std::getenv("BN_GRENADE_DUMP")) {
        std::ofstream(std::string(dir) + "/wire.json") << wire.dump(1);
    }
    REQUIRE(wire["events"].size() == run.facts.size());
    for (auto index = std::size_t{}; index < run.facts.size(); ++index) {
        CHECK(wire["events"][index]["type"].get<std::string>() == run.facts[index].type);
        CHECK(wire["events"][index]["sequence"].get<std::string>() == std::to_string(index + 1));
    }
}
