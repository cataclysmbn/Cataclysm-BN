#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "fault.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "map/submap.h"
#include "map_helpers.h"
#include "message_helpers.h"
#include "options_helpers.h"
#include "rng.h"
#include "state_helpers.h"
#include "weather/weather.h"

#include <algorithm>
#include <memory>
#include <ranges>

namespace {

struct rot_spawn_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<cata_default_random_engine> random{rng_get_engine()};
    rot_spawn_state() {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + 91_days;
        calendar::turn += 12_hours - time_past_midnight(calendar::turn);
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~rot_spawn_state() { clear_all_state(); }
};

constexpr auto pos = tripoint_bub_ms(65, 65, 0);

auto buffer() -> mapbuffer& // *NOPAD*
{
    return MAPBUFFER_REGISTRY.get(get_map().get_bound_dimension());
}

auto tile_submap() -> submap& // *NOPAD*
{
    auto* sm = buffer().lookup_submap_in_memory(project_to<coords::sm>(bub_to_abs(pos)));
    REQUIRE(sm != nullptr);
    return *sm;
}

auto update_visibility() -> void {
    get_avatar().recalc_sight_limits();
    g->reset_light_level();
    // Match vision_test: the second pass uses the current adaptation threshold.
    for (const auto ignored : std::views::iota(0, 2)) {
        static_cast<void>(ignored);
        get_map().invalidate_map_cache(0);
        get_map().build_map_cache(0, true);
        get_map().update_visibility_cache(0);
    }
}

auto finish_rot(const item& snapshot) -> void {
    buffer().handle_rotten_away_item(
        bub_to_abs(pos), snapshot, {.mode = mapbuffer_lookup_mode::resident_only});
}

} // namespace

TEST_CASE(
    "Active rot callbacks dispatch food, corpses, unrelated items and missing tiles",
    "[rot][spawn]") {
    const auto state = rot_spawn_state();
    const auto rate = override_option("CARRION_SPAWNRATE", "1.0");
    auto& sm = tile_submap();
    sm.spawns.clear();
    finish_rot(*item::spawn("test_rot_remains"));
    finish_rot(*item::spawn("test_rot_food"));
    CHECK(sm.spawns.empty());
    CHECK(get_map().i_at(pos).empty());
    finish_rot(*item::spawn("test_rot_egg"));
    REQUIRE(sm.spawns.size() == 1);
    CHECK(sm.spawns.front().type == mtype_id("mon_test_rot_corpse"));
    auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
    corpse->set_relative_rot(11.0);
    finish_rot(*corpse);
    CHECK(get_map().i_at(pos).size() == 64);
    auto empty = mapbuffer();
    const auto missing = tripoint_abs_ms(1000000, 1000000, 0);
    empty.handle_rotten_away_item(missing, *corpse, {.mode = mapbuffer_lookup_mode::resident_only});
    CHECK(empty.lookup_submap_in_memory(project_to<coords::sm>(missing)) == nullptr);
    sm.spawns.clear();
}

TEST_CASE(
    "Rot spawns outside the active bubble remain queued in their owning submap", "[rot][spawn]") {
    const auto state = rot_spawn_state();
    const auto rate = override_option("CARRION_SPAWNRATE", "1.0");
    auto remote = mapbuffer();
    const auto sm_pos = tripoint_abs_sm(10000, 10000, 0);
    const auto absolute = project_to<coords::ms>(sm_pos);
    auto sm = std::make_unique<submap>(sm_pos, remote.get_dimension_id());
    sm->set_all_ter(t_floor);
    auto* const loaded = sm.get();
    REQUIRE(remote.add_submap(sm_pos, sm));
    const auto messages = capture_messages_during([&]() {
        remote.handle_rotten_away_item(
            absolute, *item::spawn("test_rot_egg"), {.mode = mapbuffer_lookup_mode::resident_only});
    });
    REQUIRE(loaded->spawns.size() == 1);
    CHECK(loaded->spawns.front().type == mtype_id("mon_test_rot_corpse"));
    CHECK(messages.empty());
}

TEST_CASE("Rot does not enqueue null or blacklisted monster results", "[rot][spawn]") {
    const auto state = rot_spawn_state();
    const auto rate = override_option("CARRION_SPAWNRATE", "1.0");
    const auto type = GENERATE("test_rot_egg_empty", "test_rot_egg_blacklisted");
    auto& sm = tile_submap();
    sm.spawns.clear();
    finish_rot(*item::spawn(type));
    CHECK(sm.spawns.empty());
}

TEST_CASE("Visible rot spawns emit a message while unseen spawns do not", "[rot][spawn]") {
    const auto state = rot_spawn_state();
    const auto rate = override_option("CARRION_SPAWNRATE", "1.0");
    const auto blind = GENERATE(false, true);
    auto& who = get_avatar();
    who.setpos(pos + tripoint_west);
    if (blind) { who.add_effect(efftype_id("blind"), 1_days); }
    update_visibility();
    REQUIRE(who.sees(pos) == !blind);
    auto& sm = tile_submap();
    sm.spawns.clear();
    auto egg = item::spawn("test_rot_egg");
    const auto name = egg->tname();
    const auto messages = capture_messages_during([&]() { finish_rot(*egg); });
    CHECK(sm.spawns.size() == 1);
    REQUIRE(messages.size() == (blind ? 0 : 1));
    if (!blind) { CHECK(messages.front().find(name) != std::string::npos); }
    sm.spawns.clear();
}

TEST_CASE("Plant rot spawns use seed metadata and ignore noncomestible seeds", "[rot][spawn]") {
    const auto state = rot_spawn_state();
    const auto rate = override_option("CARRION_SPAWNRATE", "1.0");
    const auto type = GENERATE("test_rot_seed", "test_rot_inedible_seed");
    auto& who = get_avatar();
    who.setpos(pos + tripoint_west);
    update_visibility();
    REQUIRE(who.sees(pos));
    auto& sm = tile_submap();
    sm.spawns.clear();
    auto seed = item::spawn(type);
    REQUIRE(seed->is_seed());
    const auto name = seed->get_plant_name();
    const auto edible = seed->is_comestible();
    seed->set_birthday(calendar::turn - seed->get_plant_epoch());
    seed->set_relative_rot(0.0);
    get_map().furn_set(pos, furn_str_id("f_plant_seed"));
    get_map().add_item(pos, std::move(seed));
    sm.last_touched = calendar::turn - 1_turns;
    const auto messages = capture_messages_during([&]() {
        buffer().actualize_submap(project_to<coords::sm>(bub_to_abs(pos)));
    });
    CHECK(sm.spawns.size() == (edible ? 1 : 0));
    REQUIRE(messages.size() == (edible ? 1 : 0));
    if (edible) { CHECK(messages.front().find(name) != std::string::npos); }
    sm.spawns.clear();
}

TEST_CASE(
    "Corpse harvest skips blood and implant rolls and normalizes food charges", "[rot][harvest]") {
    const auto state = rot_spawn_state();
    const auto old = GENERATE(false, true);
    auto corpse = item::make_corpse(
        mtype_id("mon_test_rot_mixed_harvest"), calendar::turn - (old ? 60_days : 0_days));
    corpse->set_relative_rot(11.0);
    finish_rot(*corpse);
    const auto items = get_map().i_at(pos);
    const auto remains = std::ranges::count_if(items, [](const auto* it) {
        return it->typeId() == itype_id("test_rot_remains");
    });
    CHECK(remains >= 10);
    CHECK(remains <= 18);
    const auto food = std::ranges::find_if(items, [](const auto* it) {
        return it->typeId() == itype_id("test_rot_harvest_food");
    });
    CHECK((food == items.end()) == old);
    if (food != items.end()) { CHECK((*food)->charges == 1); }
    CHECK(items.size() == static_cast<size_t>(remains + (old ? 0 : 1)));
}

TEST_CASE(
    "Corpse component recovery handles both implant outcomes and sterilization", "[rot][harvest]") {
    const auto state = rot_spawn_state();
    const auto dirty = GENERATE(false, true);
    const auto fault = fault_id("fault_bionic_nonsterile");
    auto burnt = false;
    auto intact = false;
    auto recovered_component = false;
    auto lost_component = false;
    // Fixed engine seeds provide witnesses for both random recovery outcomes.
    // Assert that both occurred, as well as each outcome's ownership and faults.
    for (const auto seed : std::views::iota(1, 129)) {
        get_map().i_clear(pos);
        auto corpse = item::make_corpse(mtype_id("mon_test_rot_components"), calendar::turn);
        corpse->set_relative_rot(11.0);
        auto implant = item::spawn("test_rot_bionic");
        auto component = item::spawn("test_rot_remains");
        if (dirty) {
            implant->faults.insert(fault);
            component->faults.insert(fault);
        }
        corpse->get_components().push_back(std::move(implant));
        corpse->get_components().push_back(std::move(component));
        rng_set_engine_seed(seed);
        finish_rot(*corpse);
        const auto items = get_map().i_at(pos);
        REQUIRE(items.size() >= 1);
        REQUIRE(items.size() <= 2);
        const auto* bionic = *items.begin();
        if (bionic->typeId() == itype_id("burnt_out_bionic")) {
            burnt = true;
            CHECK_FALSE(bionic->has_fault(fault));
        } else {
            intact = true;
            CHECK(bionic->typeId() == itype_id("test_rot_bionic"));
            CHECK(bionic->has_fault(fault) == dirty);
        }
        if (items.size() == 2) {
            recovered_component = true;
            const auto* recovered = *std::next(items.begin());
            CHECK(recovered->typeId() == itype_id("test_rot_remains"));
            CHECK_FALSE(recovered->has_fault(fault));
        } else {
            lost_component = true;
        }
        for (const auto* it : items) { CHECK(it->abs_pos() == bub_to_abs(pos)); }
    }
    CHECK(burnt);
    CHECK(intact);
    CHECK(recovered_component);
    CHECK(lost_component);
}
