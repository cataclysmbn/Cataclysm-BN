#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "flag.h"
#include "item.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "map/submap.h"
#include "map_helpers.h"
#include "options_helpers.h"
#include "state_helpers.h"

#include <algorithm>
#include <ranges>
#include <vector>

namespace {

/// Force the next insertion to reallocate, regardless of prior tests' tile capacity.
auto fill_spare_item_capacity(const tripoint_bub_ms& pos) -> size_t {
    auto& here = get_map();
    auto local = point_sm_ms();
    auto* sm = here.get_submap_at(pos, local);
    REQUIRE(sm != nullptr);
    auto& storage = sm->get_items(local);
    auto existing = storage.clear();
    storage = location_vector<item>();
    for (auto& it : existing) { storage.push_back(std::move(it)); }
    const auto& items = storage.as_vector();
    const auto padding = items.capacity() - items.size();
    for (const auto ignored : std::views::iota(size_t{0}, padding)) {
        static_cast<void>(ignored);
        here.add_item(pos, item::spawn("test_rot_remains"));
    }
    REQUIRE(items.size() == items.capacity());
    return padding;
}

} // namespace

TEST_CASE(
    "Corpse decay preserves neighbouring items when actualizing a submap",
    "[item][rot][regression]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto& buffer = MAPBUFFER_REGISTRY.get(here.get_bound_dimension());
    const auto sm_pos = project_to<coords::sm>(bub_to_abs(pos));
    auto* sm = buffer.lookup_submap_in_memory(sm_pos);
    REQUIRE(sm != nullptr);

    auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
    REQUIRE(corpse->goes_bad());
    REQUIRE_FALSE(corpse->can_revive());
    corpse->set_relative_rot(11.0);
    here.add_item(pos, std::move(corpse));
    auto neighbour = item::spawn("test_rot_remains");
    const auto* neighbour_ptr = neighbour.get();
    here.add_item(pos, std::move(neighbour));
    REQUIRE(here.i_at(pos).size() == 2);
    const auto padding = fill_spare_item_capacity(pos);
    sm->last_touched = calendar::turn - 1_turns;

    buffer.actualize_submap(sm_pos);

    const auto items = here.i_at(pos);
    CHECK(items.size() == 65 + padding);
    CHECK(std::ranges::count(items, neighbour_ptr) == 1);
    CHECK(std::ranges::all_of(items, [](const auto* it) {
        return it->typeId() == itype_id("test_rot_remains");
    }));
    CHECK(std::ranges::all_of(items, [&](const auto* it) {
        return it->has_position() && it->abs_pos() == bub_to_abs(pos);
    }));
    buffer.actualize_submap(sm_pos);
    CHECK(here.i_at(pos).size() == 65 + padding);
    // Also traverse again, rather than only checking the same-turn early return.
    sm->last_touched -= 1_turns;
    buffer.actualize_submap(sm_pos);
    CHECK(here.i_at(pos).size() == 65 + padding);
}

TEST_CASE(
    "Corpse decay handles every ordering of corpses and surviving items",
    "[item][rot][regression][property]") {
    const auto mask = GENERATE(Catch::Generators::range(0, 16));
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto& buffer = MAPBUFFER_REGISTRY.get(here.get_bound_dimension());
    const auto sm_pos = project_to<coords::sm>(bub_to_abs(pos));
    auto* sm = buffer.lookup_submap_in_memory(sm_pos);
    REQUIRE(sm != nullptr);
    auto survivors = std::vector<const item*>();
    auto corpses = 0;
    for (const auto slot : std::views::iota(0, 4)) {
        if (mask & (1 << slot)) {
            auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
            corpse->set_relative_rot(11.0);
            here.add_item(pos, std::move(corpse));
            ++corpses;
        } else {
            auto survivor = item::spawn("test_rot_remains");
            survivors.push_back(survivor.get());
            here.add_item(pos, std::move(survivor));
        }
    }
    const auto padding = fill_spare_item_capacity(pos);
    sm->last_touched = calendar::turn - 1_turns;
    CAPTURE(mask, padding);
    buffer.actualize_submap(sm_pos);
    const auto items = here.i_at(pos);
    CHECK(items.size() == survivors.size() + corpses * 64 + padding);
    for (const auto* survivor : survivors) { CHECK(std::ranges::count(items, survivor) == 1); }
    CHECK(std::ranges::all_of(items, [&](const auto* it) {
        return it->has_position() && it->abs_pos() == bub_to_abs(pos);
    }));
}

TEST_CASE(
    "Item removal callbacks may append items while keeping the current item",
    "[item][rot][regression]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto first = item::spawn("test_rot_remains");
    const auto* identity = first.get();
    here.add_item(pos, std::move(first));
    auto local = point_sm_ms();
    auto* sm = here.get_submap_at(pos, local);
    REQUIRE(sm != nullptr);
    auto& items = sm->get_items(local);
    const auto padding = fill_spare_item_capacity(pos);
    auto visits = 0;
    items.remove_with([&](detached_ptr<item>&& it) {
        if (it.get() == identity) {
            ++visits;
            for (const auto ignored : std::views::iota(0, 64)) {
                static_cast<void>(ignored);
                here.add_item(pos, item::spawn("test_rot_remains"));
            }
        }
        return std::move(it);
    });
    CHECK(visits == 1);
    CHECK(items.size() == 65 + padding);
    CHECK(std::ranges::count(items, identity) == 1);
    CHECK(std::ranges::all_of(items, [&](const auto* it) {
        return it->has_position() && it->abs_pos() == bub_to_abs(pos);
    }));
}

TEST_CASE(
    "Item rot actualization leaves map side effects to its caller",
    "[item][rot][corpse][property]") {
    const auto age = GENERATE(10.0, 11.0);
    const auto container_type = GENERATE("test_rot_open", "test_rot_sealed", "test_rot_preserving");
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto& here = get_map();
    auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
    corpse->set_relative_rot(age);
    auto container = item::spawn(container_type);
    container->put_in(std::move(corpse));
    container = item::actualize_rot(std::move(container), {.position = bub_to_abs(pos)});
    REQUIRE(container);
    const auto decayed = age > 10.0 && std::string_view(container_type) == "test_rot_open";
    CAPTURE(age, container_type);
    CHECK(container->contents.empty() == decayed);
    CHECK(here.i_at(pos).empty());
    container = item::actualize_rot(std::move(container), {.position = bub_to_abs(pos)});
    CHECK(here.i_at(pos).empty());
}

TEST_CASE(
    "Rot spawns roll once per stack and respect occupancy and friendly disposition",
    "[item][rot][spawn][property]") {
    const auto count = GENERATE(1, 2, 8);
    const auto friendly = GENERATE(false, true);
    const auto occupied = GENERATE(false, true);
    const auto enabled = GENERATE(false, true);
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    const auto spawn_rate = override_option("CARRION_SPAWNRATE", enabled ? "1.0" : "0.0");
    clear_all_state();
    move_player_out_of_the_way();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto& here = get_map();
    auto& buffer = MAPBUFFER_REGISTRY.get(here.get_bound_dimension());
    const auto sm_pos = project_to<coords::sm>(bub_to_abs(pos));
    auto* sm = buffer.lookup_submap_in_memory(sm_pos);
    REQUIRE(sm != nullptr);
    sm->spawns.clear();
    if (occupied) { spawn_test_monster("debug_mon", pos); }
    auto egg = item::spawn("test_rot_egg", calendar::turn);
    egg->charges = count;
    egg->set_relative_rot(3.0);
    if (friendly) { egg->set_flag(flag_id("SPAWN_FRIENDLY")); }
    here.add_item(pos, std::move(egg));
    sm->last_touched = calendar::turn - 1_turns;
    buffer.actualize_submap(sm_pos);
    CAPTURE(count, friendly, occupied, enabled);
    CHECK(here.i_at(pos).empty());
    CHECK(sm->spawns.size() == (enabled && !occupied ? 1 : 0));
    for (const auto& spawn : sm->spawns) {
        CHECK(spawn.type == mtype_id("mon_test_rot_corpse"));
        CHECK(spawn.count == 1);
        CHECK(
            spawn.disposition
            == (friendly ? spawn_disposition::SpawnDisp_Pet : spawn_disposition::SpawnDisp_Default));
    }
    sm->last_touched -= 1_turns;
    buffer.actualize_submap(sm_pos);
    CHECK(sm->spawns.size() == (enabled && !occupied ? 1 : 0));
    sm->spawns.clear();
}
