#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "harvest.h"
#include "item.h"
#include "itype.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "map/submap.h"
#include "mapgen/mapgen_constructor.h"
#include "monstergenerator.h"
#include "mtype.h"
#include "options_helpers.h"
#include "rot_test_state.h"
#include "type_id.h"

#include <algorithm>
#include <ranges>
#include <vector>

TEST_CASE(
    "Decayed zombie corpses leave no zombie pheromone", "[rot][open_issue][corpse][issue_5296]") {
    const auto state = rot_test_state(1_hours);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto&& monsters = MonsterGenerator::generator().get_all_mtypes();
    auto dropping_pheromone = std::vector<mtype_id>();
    for (const auto& type : monsters) {
        const auto& entries = type.harvest.obj();
        const auto drops = std::ranges::any_of(entries, [](const auto& entry) {
            return entry.drop == "pheromone";
        });
        if (drops) { dropping_pheromone.push_back(type.id); }
    }
    REQUIRE_FALSE(dropping_pheromone.empty());

    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto& buffer = MAPBUFFER_REGISTRY.get(here.get_bound_dimension());
    const auto sm_pos = project_to<coords::sm>(bub_to_abs(pos));
    auto* sm = buffer.find_resident_submap(sm_pos);
    REQUIRE(sm != nullptr);
    for (const auto& id : dropping_pheromone) {
        CAPTURE(id.str());
        for (const auto ignored : std::views::iota(0, 40)) {
            static_cast<void>(ignored);
            auto corpse = item::make_corpse(id, calendar::turn);
            corpse->set_relative_rot(11.0);
            here.add_item(pos, std::move(corpse));
        }
        sm->last_touched = calendar::turn - 1_turns;
        buffer.actualize_submap(sm_pos);
        CHECK(std::ranges::none_of(here.i_at(pos), [](const item* it) {
            return it->typeId() == itype_id("pheromone");
        }));
        here.i_clear(pos);
    }
}

TEST_CASE(
    "Perishable items placed by mapgen are fresh when the map is generated",
    "[rot][open_issue][mapgen][issue_9834]") {
    const auto state = rot_test_state(1_hours);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto global_spawn_rate = override_option("ITEM_SPAWNRATE", "1.0");
    const auto food_spawn_rate = override_option("SPAWN_RATE_food", "1.0");
    const auto perishable_spawn_rate = override_option("SPAWN_RATE_perishables", "1.0");
    auto& buffer = MAPBUFFER_REGISTRY.get(mapbuffer_registry::primary_dimension_id());
    auto tm = mapgen_constructor(buffer);
    tm.reset_scratch_omt(
        tripoint_abs_omt(11, 13, 0), ter_id("t_floor"), furn_id("f_null"), trap_id("tr_null"));
    const auto group = GENERATE("razorclaw_nest", "razorclaw_roe");
    CAPTURE(group);
    for (const auto ignored : std::views::iota(0, 20)) {
        static_cast<void>(ignored);
        tm.place_items(item_group_id(group), 100, point_omt_ms(2, 2), point_omt_ms(20, 20), true,
                       calendar::turn, 0, 0);
    }
    auto roe = std::vector<item*>();
    for (const auto x : std::views::iota(2, 21)) {
        for (const auto y : std::views::iota(2, 21)) {
            for (auto* const it : tm.i_at(point_omt_ms(x, y))) {
                if (it->typeId() == itype_id("razorclaw_roe")) { roe.push_back(it); }
            }
        }
    }
    REQUIRE_FALSE(roe.empty());
    for (const auto* egg : roe) {
        CHECK_FALSE(egg->rotten());
        CHECK(egg->get_rot() == 0_turns);
    }
}

TEST_CASE(
    "Eating from a container reports no location error and leaves the container usable",
    "[rot][open_issue][container][issue_10683]") {
    const auto state = rot_test_state(1_hours);
    const auto container_food = GENERATE("can_beans", "can_peach", "jar_pickle_glass");
    CAPTURE(container_food);
    if (!itype_id(container_food).is_valid()) { return; }
    auto& you = get_avatar();
    you.set_stored_kcal(you.max_stored_kcal() / 2);
    auto packed = item::in_its_container(item::spawn(itype_id(container_food), calendar::turn));
    auto& container = you.i_add(std::move(packed));
    const auto messages = capture_debugmsg_during([&]() { you.consume(container); });
    CHECK(messages.empty());
}

TEST_CASE(
    "Using up part of a sealed container's food unseals it",
    "[rot][open_issue][container][issue_3588]") {
    const auto state = rot_test_state(1_hours);
    const auto sealed = GENERATE("jar_glass_sealed", "can_medium");
    CAPTURE(sealed);
    auto container = item::spawn(itype_id(sealed), calendar::turn);
    REQUIRE(container->is_non_resealable_container());
    auto food = item::spawn("can_beans", calendar::turn, 2);
    container->put_in(std::move(food));
    const auto unsealed = container->type->container->unseals_into;
    REQUIRE(container->typeId() != unsealed);
    auto& you = get_avatar();
    you.set_stored_kcal(you.max_stored_kcal() / 2);
    auto& held = you.i_add(std::move(container));
    you.consume(held);
    CHECK(held.typeId() == unsealed);
}
