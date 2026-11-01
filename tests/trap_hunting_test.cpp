#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "item.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "mtype.h"
#include "overmap/overmapbuffer.h"
#include "state_helpers.h"
#include "trap.h"
#include "trap_hunting.h"
#include "type_id.h"

#include <cmath>
#include <vector>

namespace {

struct omt_override {
    tripoint_abs_omt pos;
    oter_id original;
};

/// Turns the 3x3 overmap tiles around @p center into forest, returning what to restore.
auto surround_with_forest(overmapbuffer& omb, const tripoint_abs_omt& center)
    -> std::vector<omt_override> {
    auto overrides = std::vector<omt_override>{};
    for (auto dy = -1; dy <= 1; dy++) {
        for (auto dx = -1; dx <= 1; dx++) {
            const auto pos = center + tripoint_rel_omt(dx, dy, 0);
            overrides.push_back({.pos = pos, .original = omb.ter(pos)});
            omb.ter_set(pos, oter_str_id("forest").id());
        }
    }
    return overrides;
}

struct baited_trap_options {
    tripoint_bub_ms pos;
    trap_str_id trap;
    itype_id bait;
};

auto set_baited_trap(map& here, const baited_trap_options& opts) -> void {
    here.ter_set(opts.pos, ter_id("t_grass"));
    here.trap_set(opts.pos, opts.trap);
    auto bait = item::spawn(opts.bait);
    trap_hunting::mark_as_bait(
        *bait,
        {
            .pos = map_local_to_abs(here, opts.pos),
            .placed_at = calendar::turn,
            .trapper_skill = 0,
            .cover = 1.0,
        });
    here.add_item(opts.pos, std::move(bait));
}

auto set_baited_snare(map& here, const tripoint_bub_ms& pos, const itype_id& bait_type) -> void {
    set_baited_trap(here, {.pos = pos, .trap = trap_str_id("tr_hunting_snare"), .bait = bait_type});
}

auto resolve_at(map& here, const tripoint_bub_ms& pos) -> std::optional<mtype_id> {
    return trap_hunting::resolve({
        .buffer = here.get_mapbuffer(),
        .pos = map_local_to_abs(here, pos),
        .now = calendar::turn,
        .lookup_mode = mapbuffer_lookup_mode::resident_only,
    });
}

} // namespace

TEST_CASE("hunting catch chance does not depend on how often it is checked", "[trap][hunting]") {
    const auto rate = 0.4;
    const auto one_day = trap_hunting::catch_chance(rate, 1_days);
    const auto two_days = trap_hunting::catch_chance(rate, 2_days);

    CHECK(one_day == Approx(1.0 - std::exp(-rate)));
    CHECK(two_days == Approx(1.0 - (1.0 - one_day) * (1.0 - one_day)));
    CHECK(trap_hunting::catch_chance(0.0, 1_days) == 0.0);
    CHECK(trap_hunting::catch_chance(rate, 0_turns) == 0.0);
}

TEST_CASE("hunting catch delay inverts the catch chance", "[trap][hunting]") {
    const auto rate = 0.4;
    const auto exposure = 3_days;

    SECTION("a roll at or above the chance catches nothing") {
        const auto chance = trap_hunting::catch_chance(rate, exposure);
        CHECK_FALSE(trap_hunting::catch_delay(
            {.rate_per_day = rate, .exposure = exposure, .roll = chance}));
    }
    SECTION("a roll below the chance catches at the matching time") {
        const auto roll = trap_hunting::catch_chance(rate, 1_days);
        const auto delay = trap_hunting::catch_delay(
            {.rate_per_day = rate, .exposure = exposure, .roll = roll});
        REQUIRE(delay);
        CHECK(to_hours<double>(*delay) == Approx(24.0).margin(0.01));
    }
}

TEST_CASE("prey keeps away from nearby observers", "[trap][hunting]") {
    CHECK(trap_hunting::presence_factor(0) == 0.0);
    CHECK(trap_hunting::presence_factor(6) == 0.0);
    CHECK(trap_hunting::presence_factor(18) == Approx(0.5));
    CHECK(trap_hunting::presence_factor(30) == 1.0);
    CHECK(trap_hunting::presence_factor(100) == 1.0);
}

TEST_CASE("hunting data links items, land use and traps", "[trap][hunting]") {
    CHECK(hunting_bait::for_item(itype_id("meat")) == hunting_bait_id("meat"));
    CHECK(hunting_bait::for_item(itype_id("birdfood")) == hunting_bait_id("seeds"));
    CHECK_FALSE(hunting_bait::for_item(itype_id("rock")));
    CHECK(hunting_habitat::for_land_use(overmap_land_use_code_id("forest"))
          == hunting_habitat_id("forest"));
    CHECK(trap_str_id("tr_hunting_snare")->hunting.has_value());
    CHECK_FALSE(trap_str_id("tr_caltrops")->hunting.has_value());
}

TEST_CASE("bait set on a hunting trap", "[trap][hunting]") {
    clear_all_state();
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(30, 30, 0);
    set_baited_snare(here, pos, itype_id("birdfood"));

    auto* const bait = trap_hunting::find_bait(here, pos);
    REQUIRE(bait != nullptr);

    SECTION("catches nothing without elapsed time") {
        CHECK_FALSE(resolve_at(here, pos));
        CHECK(trap_hunting::find_bait(here, pos) != nullptr);
    }
    SECTION("becomes an ordinary item when the trap is removed") {
        here.remove_trap(pos);
        CHECK(trap_hunting::find_bait(here, pos) == nullptr);
        REQUIRE(here.i_at(pos).size() == 1);
        const auto* const released = *here.i_at(pos).begin();
        CHECK_FALSE(trap_hunting::has_bait_tags(*released));
    }
    SECTION("leaves an empty trap and an ordinary item when picked up") {
        auto& held = get_avatar().i_add(here.i_rem(pos, bait));
        CHECK_FALSE(trap_hunting::has_bait_tags(held));
        CHECK(trap_hunting::find_bait(here, pos) == nullptr);
    }
    SECTION("is released by the trap, not kept for a new one") {
        here.remove_trap(pos);
        here.trap_set(pos, trap_str_id("tr_hunting_snare"));
        CHECK(trap_hunting::find_bait(here, pos) == nullptr);
    }
}

TEST_CASE("baited hunting trap catches prey while left alone", "[trap][hunting]") {
    clear_all_state();
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(30, 30, 0);
    auto& omb = get_overmapbuffer(here.get_bound_dimension());
    const auto overrides =
        surround_with_forest(omb, project_to<coords::omt>(map_local_to_abs(here, pos)));

    set_baited_snare(here, pos, itype_id("birdfood"));
    const auto placed_at = calendar::turn;
    // A year leaves effectively no chance of missing every suitable animal.
    calendar::turn += 365_days;

    const auto caught = resolve_at(here, pos);
    REQUIRE(caught);
    CHECK(trap_hunting::find_bait(here, pos) == nullptr);

    const auto& prey = caught->obj();
    CHECK(prey.size <= creature_size::small);
    CHECK(hunting_bait_id("seeds")->lures(prey));

    auto corpse = static_cast<const item*>(nullptr);
    for (const auto* const it : here.i_at(pos)) {
        if (it->is_corpse() && it->get_mtype()->id == *caught) { corpse = it; }
    }
    REQUIRE(corpse != nullptr);
    CHECK(corpse->birthday() >= placed_at);
    CHECK(corpse->birthday() <= calendar::turn);
    // Snares survive being triggered, so the catch leaves them set.
    CHECK(here.tr_at(pos).id == trap_str_id("tr_hunting_snare"));

    for (const auto& entry : overrides) { omb.ter_set(entry.pos, entry.original); }
}

TEST_CASE("bear trap springs when it catches prey", "[trap][hunting]") {
    clear_all_state();
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(30, 30, 0);
    auto& omb = get_overmapbuffer(here.get_bound_dimension());
    const auto overrides =
        surround_with_forest(omb, project_to<coords::omt>(map_local_to_abs(here, pos)));

    set_baited_trap(
        here, {.pos = pos, .trap = trap_str_id("tr_beartrap"), .bait = itype_id("birdfood")});
    // Few forest animals are both big enough for a bear trap and lured by seeds.
    calendar::turn += 3 * 365_days;

    const auto caught = resolve_at(here, pos);
    REQUIRE(caught);
    CHECK(caught->obj().size >= creature_size::small);
    CHECK(here.tr_at(pos).is_null());

    auto dropped_traps = 0;
    for (const auto* const it : here.i_at(pos)) {
        if (it->typeId() == itype_id("beartrap")) { dropped_traps++; }
    }
    CHECK(dropped_traps == 1);

    for (const auto& entry : overrides) { omb.ter_set(entry.pos, entry.original); }
}
