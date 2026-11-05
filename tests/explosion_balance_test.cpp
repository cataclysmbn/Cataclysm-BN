#include "../src/map/map.h"
#include "../src/vehicle/vehicle_part.h"
#include "../src/vehicle/vpart_position.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "creature.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "iuse_actor.h"
#include "line.h"
#include "map_helpers.h"
#include "monster.h"
#include "options_helpers.h"
#include "state_helpers.h"
#include "string_id.h"
#include "test_statistics.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_range.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

enum class outcome_type { Kill, Casualty };

namespace {

auto test_map_pos(const tripoint_rel_ms& offset) -> tripoint_abs_ms {
    const auto map_origin_abs = tripoint_abs_ms(project_to<coords::ms>(get_map().get_abs_sub()), 0);
    return map_origin_abs + offset;
}

auto set_off_explosion(item& explosive, const tripoint_abs_ms& origin_abs) -> void {
    explosion_handler::get_explosion_queue().clear();
    explosive.charges = 0;
    explosive.type->invoke(g->u, explosive, abs_to_bub(origin_abs));
    explosion_handler::get_explosion_queue().execute();
}

void check_lethality(
    const std::string& explosive_id, const int range, float lethality, float margin,
    outcome_type expected_outcome) {
    const epsilon_threshold target_lethality{lethality, margin};
    int num_survivors = 0;
    int num_subjects = 0;
    int num_wounded = 0;
    statistics<bool> victims;
    std::stringstream survivor_stats;
    int total_hp = 0;
    do {
        // Clear map
        clear_map();
        move_player_out_of_the_way();
        // Spawn some monsters in a circle.
        const auto origin_abs = test_map_pos(tripoint_rel_ms(30, 30, 0));
        int num_subjects_this_time = 0;
        for (const tripoint_abs_ms& monster_position_abs :
             closest_points_first(origin_abs, range)) {
            if (rl_dist(monster_position_abs, origin_abs) != range) { continue; }
            num_subjects++;
            num_subjects_this_time++;
            monster& new_monster =
                spawn_test_monster("mon_test_explosion_target", abs_to_bub(monster_position_abs));
            new_monster.no_extra_death_drops = true;
        }
        item& explosive = *item::spawn_temporary(explosive_id);
        set_off_explosion(explosive, origin_abs);
        // see how many monsters survive
        std::vector<Creature*> survivors = g->get_creatures_if([](const Creature& critter) {
            return critter.is_monster();
        });
        num_survivors += survivors.size();
        for (Creature* survivor : survivors) {
            survivor_stats << survivor->abs_pos() << " " << survivor->get_hp() << ", ";
            bool wounded = survivor->get_hp() < survivor->get_hp_max();
            num_wounded += wounded ? 1 : 0;
            total_hp += survivor->get_hp();
            if (expected_outcome == outcome_type::Casualty && wounded) {
                victims.add(true);
            } else {
                victims.add(false);
            }
        }
        if (!survivors.empty()) { survivor_stats << '\n'; }
        for (int i = survivors.size(); i < num_subjects_this_time; ++i) { victims.add(true); }
    } while (victims.uncertain_about(target_lethality));
    CAPTURE(margin);
    INFO(explosive_id);
    INFO("range " << range);
    INFO(num_survivors << " survivors out of " << num_subjects << " targets.");
    INFO(survivor_stats.str());
    INFO("Wounded survivors: " << num_wounded);
    const int average_hp = num_survivors ? total_hp / num_survivors : 0;
    INFO("average hp of survivors: " << average_hp);
    CHECK(victims.avg() == Approx(lethality).margin(margin));
}

auto get_part_hp(vehicle* veh) -> std::vector<int> {
    std::vector<int> part_hp;
    part_hp.reserve(veh->part_count());
    for (const vpart_reference& vpr : veh->get_all_parts()) { part_hp.push_back(vpr.part().hp()); }
    return part_hp;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void check_vehicle_damage(
    const std::string& explosive_id, const std::string& vehicle_id, const int range) {
    move_player_out_of_the_way();
    const auto origin_abs = test_map_pos(tripoint_rel_ms(30, 30, 0));

    vehicle* target_vehicle =
        get_map().add_vehicle(vproto_id(vehicle_id), abs_to_bub(origin_abs), 0_degrees, -1, 0);
    std::vector<int> before_hp = get_part_hp(target_vehicle);

    auto explosion_origin_abs = origin_abs;
    while (get_map().veh_at(abs_to_bub(explosion_origin_abs))) {
        explosion_origin_abs += tripoint_rel_ms::east();
    }
    explosion_origin_abs += tripoint_rel_ms(range, 0, 0);
    item& explosive = *item::spawn_temporary(explosive_id);
    set_off_explosion(explosive, explosion_origin_abs);

    std::vector<int> after_hp = get_part_hp(target_vehicle);

    // We don't expect any destroyed parts.
    REQUIRE(before_hp.size() == after_hp.size());
    for (size_t i = 0; i < before_hp.size(); ++i) {
        CAPTURE(i);
        INFO(target_vehicle->part(i).name());
        const auto part_id = target_vehicle->part(i).info().get_id();
        if (part_id == vpart_id("test_explosion_fragile_fixture")) {
            CHECK(before_hp[i] >= after_hp[i]);
        } else {
            CHECK(before_hp[i] == after_hp[i]);
        }
    }
}

} // namespace


TEST_CASE("grenade_lethality", "[.][grenade][explosion][balance][slow]") {
    clear_all_state();
    check_lethality("test_grenade_act", 3, 0.95, 0.06, outcome_type::Kill);
    check_lethality("test_grenade_act", 6, 0.95, 0.06, outcome_type::Casualty);
}

TEST_CASE("grenade_vs_vehicle", "[grenade][explosion][balance]") {
    clear_all_state();
    check_vehicle_damage("test_grenade_act", "test_explosion_vehicle", 5);
}

TEST_CASE("shrapnel behind wall", "[grenade][explosion][balance]") {
    clear_all_state();
    move_player_out_of_the_way();
    const auto origin_abs = test_map_pos(tripoint_rel_ms(30, 30, 0));

    item& grenade = *item::spawn_temporary("test_shrapnel_blast");
    REQUIRE(grenade.get_use("explosion") != nullptr);
    const auto* actor = dynamic_cast<const explosion_iuse*>(
        grenade.get_use("explosion")->get_actor_ptr());
    REQUIRE(actor != nullptr);
    REQUIRE(static_cast<bool>(actor->explosion.fragment));
    REQUIRE(actor->explosion.radius <= 0);
    REQUIRE(actor->explosion.fragment->range > 2);

    for (const tripoint_abs_ms& pt_abs : closest_points_first(origin_abs, 2)) {
        if (square_dist(origin_abs, pt_abs) > 1) {
            g->m.ter_set(abs_to_bub(pt_abs), ter_id("test_t_shrapnel_wall"));
        }
    }

    // Not on the bomb because shrapnel always hits that square
    const monster& m_in_range = spawn_test_monster(
        "mon_test_explosion_target", abs_to_bub(origin_abs + tripoint_rel_ms::east()));
    const monster& m_behind_wall = spawn_test_monster(
        "mon_test_explosion_target", abs_to_bub(origin_abs + tripoint_rel_ms(3, 0, 0)));

    set_off_explosion(grenade, origin_abs);

    CHECK(m_in_range.hp_percentage() < 100);
    CHECK(m_behind_wall.hp_percentage() == 100);
}

TEST_CASE("shrapnel at huge range", "[grenade][explosion]") {
    clear_all_state();
    move_player_out_of_the_way();
    const auto origin_abs = test_map_pos(tripoint_rel_ms::zero());

    item& grenade = *item::spawn_temporary("test_long_shrapnel_blast");
    REQUIRE(grenade.get_use("explosion") != nullptr);
    const auto* actor = dynamic_cast<const explosion_iuse*>(
        grenade.get_use("explosion")->get_actor_ptr());
    REQUIRE(actor != nullptr);
    REQUIRE(static_cast<bool>(actor->explosion.fragment));
    REQUIRE(actor->explosion.radius <= 0);
    REQUIRE(actor->explosion.fragment->range >= 300);
    const auto range = static_cast<int>(std::pow(actor->explosion.fragment->range, 1.4) - 1);
    const auto placement = std::min(range, g_mapsize_x - 1);

    const monster& m = spawn_test_monster(
        "mon_test_explosion_target",
        abs_to_bub(test_map_pos(tripoint_rel_ms(placement, placement, 0))));

    set_off_explosion(grenade, origin_abs);

    CHECK(m.is_dead_state());
}

TEST_CASE("shrapnel at max grenade range", "[grenade][explosion]") {
    clear_all_state();
    move_player_out_of_the_way();
    const auto origin_abs = test_map_pos(tripoint_rel_ms(g_half_mapsize_x, g_half_mapsize_y, 0));

    item& grenade = *item::spawn_temporary("test_shrapnel_blast");
    REQUIRE(grenade.get_use("explosion") != nullptr);
    const auto* actor = dynamic_cast<const explosion_iuse*>(
        grenade.get_use("explosion")->get_actor_ptr());
    REQUIRE(actor != nullptr);
    REQUIRE(static_cast<bool>(actor->explosion.fragment));
    REQUIRE(actor->explosion.radius <= 0);
    REQUIRE(actor->explosion.fragment->range > 1);

    const int range = actor->explosion.fragment->range;
    for (const tripoint_abs_ms& pt_abs : closest_points_first(origin_abs, range + 1)) {
        spawn_test_monster("mon_test_explosion_sturdy_target", abs_to_bub(pt_abs));
    }

    set_off_explosion(grenade, origin_abs);

    for (const tripoint_abs_ms& pt_abs : closest_points_first(origin_abs, range + 1)) {
        const monster* m = g->critter_at<monster>(abs_to_bub(pt_abs));
        REQUIRE(m != nullptr);
        CAPTURE(m->abs_pos());
        CAPTURE(rl_dist(origin_abs, m->abs_pos()));
        if (rl_dist(origin_abs, m->abs_pos()) <= range) {
            CHECK(m->hp_percentage() < 100);
        } else {
            CHECK(m->hp_percentage() == 100);
        }
    }
}

TEST_CASE("rotated_vehicle_walls_block_explosions") {
    clear_all_state();
    move_player_out_of_the_way();
    const auto origin_abs = test_map_pos(tripoint_rel_ms(g_half_mapsize_x, g_half_mapsize_y, 0));

    item& grenade = *item::spawn_temporary("test_shrapnel_blast");

    map& here = get_map();

    here.add_vehicle(
        vproto_id("test_explosion_wall_vehicle"), abs_to_bub(origin_abs), -45_degrees, 0, 0);

    here.build_map_cache(0);

    const auto mon_origin_abs = test_map_pos(
        tripoint_rel_ms(g_half_mapsize_x - 2, g_half_mapsize_y + 1, 0));

    monster& s = spawn_test_monster("mon_test_explosion_target", abs_to_bub(mon_origin_abs));

    REQUIRE(veh_pointer_or_null(here.veh_at(abs_to_bub(mon_origin_abs))) != nullptr);

    const auto explode_at_abs = mon_origin_abs + tripoint_rel_ms::north_west();

    REQUIRE(veh_pointer_or_null(here.veh_at(abs_to_bub(explode_at_abs))) == nullptr);

    set_off_explosion(grenade, explode_at_abs);

    const monster* m = g->critter_at<monster>(abs_to_bub(mon_origin_abs));
    REQUIRE(m != nullptr);
    CHECK(m == &s);
    CHECK(m->get_hp() == m->get_hp_max());
}

// Regression tests for issue #9696 ("EMP Bomb Crashes the game"). An active
// explosive being processed is detached (loc == nullptr) but still in the map
// stack; draining the explosion queue in that window re-detonates it forever.
// scoped_drain_deferral defers drains during map::process_items() to the
// turn-loop drain, which runs after the item is removed, so the chain is finite.

TEST_CASE("explosion queue defers drains during item processing", "[explosion][emp]") {
    clear_all_state();
    auto& queue = explosion_handler::get_explosion_queue();
    queue.clear();

    const auto origin_abs = test_map_pos(tripoint_rel_ms(g_half_mapsize_x, g_half_mapsize_y, 0));
    const auto ex = explosion_data{.damage = 10, .radius = 2.0f};

    {
        // Stands in for the map::process_items() window.
        explosion_handler::scoped_drain_deferral defer;
        explosion_handler::explosion(abs_to_bub(origin_abs), ex, nullptr);
        REQUIRE_FALSE(queue.empty());

        // A re-entrant drain must not run here (it would re-detonate forever).
        queue.execute();
        CHECK_FALSE(queue.empty());
    }

    // The suppressed drain is remembered so the turn loop can run it before cleanup_dead().
    CHECK(queue.take_deferred_drain_request());
    CHECK_FALSE(queue.take_deferred_drain_request()); // consumed by the take

    // The turn-loop drain (outside the window) empties the queue.
    queue.execute();
    CHECK(queue.empty());

    {
        // A suppressed request satisfied by any real drain must not linger.
        explosion_handler::scoped_drain_deferral defer;
        explosion_handler::explosion(abs_to_bub(origin_abs), ex, nullptr);
        queue.execute(); // suppressed: sets the request
    }
    queue.execute(); // real drain satisfies it
    CHECK(queue.empty());
    CHECK_FALSE(queue.take_deferred_drain_request()); // no stale flag
}

TEST_CASE("EMP bomb processed next to a searchlight does not run away", "[explosion][emp]") {
    clear_all_state();

    // Sympathetic detonation must be on for the runaway (default; set explicitly).
    const auto explodium = override_option("MADE_OF_EXPLODIUM", "30");

    const auto bomb_pos_abs = test_map_pos(tripoint_rel_ms(g_half_mapsize_x, g_half_mapsize_y, 0));
    const auto sl_pos_abs = test_map_pos(
        tripoint_rel_ms(g_half_mapsize_x + 1, g_half_mapsize_y, 0));
    g->m.i_clear(abs_to_bub(bomb_pos_abs));

    // A searchlight the EMP kills; its FOCUSEDBEAM death drains the queue.
    auto& searchlight = spawn_test_monster("mon_turret_searchlight", abs_to_bub(sl_pos_abs));
    searchlight.set_hp(1);

    // Active EMP bomb on its own tile, charges 0 so it detonates on its tick.
    auto bomb =
        item::spawn("EMPbomb_act", calendar::start_of_cataclysm, item::default_charges_tag());
    bomb->activate();
    bomb->charges = 0;
    g->m.add_item(abs_to_bub(bomb_pos_abs), std::move(bomb));

    auto& queue = explosion_handler::get_explosion_queue();
    queue.clear();

    // Processing detonates the bomb and kills the searchlight; before the fix the
    // re-entrant drain looped forever (cap fires + debugmsg -> test fails). Tick a
    // few times in case the countdown needs more than one pass.
    for (int i = 0; i < 12 && !searchlight.is_dead(); i++) {
        g->m.process_items();
        queue.execute();
    }

    REQUIRE(searchlight.is_dead()); // scenario actually triggered
    CHECK(queue.empty());           // ...and terminated with no runaway
}

TEST_CASE("queued explosions drop their source when the creature is removed", "[explosion]") {
    clear_all_state();

    const auto source_pos_abs = test_map_pos(
        tripoint_rel_ms(g_half_mapsize_x + 1, g_half_mapsize_y, 0));
    auto& source = spawn_test_monster("mon_zombie", abs_to_bub(source_pos_abs));
    const Creature* source_ptr = &source;

    auto& queue = explosion_handler::get_explosion_queue();
    queue.clear();
    const auto ex = explosion_data{.damage = 10, .radius = 2.0f};
    explosion_handler::explosion(abs_to_bub(source_pos_abs), ex, &source);
    REQUIRE(queue.references_source(source_ptr));

    // The source dies and is destroyed while its blast is still queued (e.g. a
    // drain deferred during map::process_items() outliving cleanup_dead()).
    source.set_hp(0);
    g->cleanup_dead();

    // The source object is gone from here on; only the saved address may be used.
    CHECK_FALSE(queue.references_source(source_ptr));
    queue.execute(); // the drain must not touch the destroyed source
    CHECK(queue.empty());
}
