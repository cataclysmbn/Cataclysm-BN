#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "monster.h"
#include "mtype.h"
#include "player.h"
#include "state_helpers.h"
#include "string_id.h"
#include "type_id.h"
#include "units.h"

#include <climits>
#include <memory>

static auto find_adjacent_monster(const tripoint_bub_ms& pos) -> monster* { // *NOPAD*
    tripoint_bub_ms target = pos;
    for (target.x() = pos.x() - 1; target.x() <= pos.x() + 1; target.x()++) {
        for (target.y() = pos.y() - 1; target.y() <= pos.y() + 1; target.y()++) {
            if (target == pos) { continue; }
            if (monster* const candidate = g->critter_at<monster>(target)) { return candidate; }
        }
    }
    return nullptr;
}

TEST_CASE("manhack", "[iuse_actor][manhack]") {
    clear_all_state();
    player& dummy = get_avatar();

    g->clear_zombies();
    detached_ptr<item> det =
        item::spawn("bot_manhack", calendar::start_of_cataclysm, item::default_charges_tag{});
    item& test_item = *det;
    dummy.i_add(std::move(det));

    int test_item_pos = dummy.inv_position_by_item(&test_item);
    REQUIRE(test_item_pos != INT_MIN);

    monster* new_manhack = find_adjacent_monster(dummy.bub_pos());
    REQUIRE(new_manhack == nullptr);

    dummy.invoke_item(&test_item);

    test_item_pos = dummy.inv_position_by_item(&test_item);
    REQUIRE(test_item_pos == INT_MIN);

    new_manhack = find_adjacent_monster(dummy.bub_pos());
    REQUIRE(new_manhack != nullptr);
    REQUIRE(new_manhack->type->id == mtype_id("mon_manhack"));
    g->clear_zombies();
}

TEST_CASE("place_monster_failure_result_breaks_item", "[iuse_actor][place_monster]") {
    clear_all_state();
    player& dummy = get_avatar();
    g->clear_zombies();

    detached_ptr<item> det =
        item::spawn("test_handy_hauler_random", calendar::start_of_cataclysm);
    item& test_item = *det;
    test_item.set_flag(flag_SPAWN_HOSTILE);
    dummy.i_add(std::move(det));

    dummy.invoke_item(&test_item);

    CHECK(test_item.typeId() == itype_id("broken_handy_hauler"));
    CHECK(dummy.inv_position_by_item(&test_item) != INT_MIN);
    CHECK(find_adjacent_monster(dummy.bub_pos()) == nullptr);
    g->clear_zombies();
}

TEST_CASE("handy_hauler_activates_as_pacifist_pet", "[iuse_actor][place_monster]") {
    clear_all_state();
    player& dummy = get_avatar();
    g->clear_zombies();

    detached_ptr<item> det =
        item::spawn("test_handy_hauler_random", calendar::start_of_cataclysm);
    item& test_item = *det;
    test_item.set_flag(flag_SPAWN_FRIENDLY);
    dummy.i_add(std::move(det));

    dummy.invoke_item(&test_item);

    const monster* const hauler = find_adjacent_monster(dummy.bub_pos());
    REQUIRE(hauler != nullptr);
    CHECK(hauler->type->id == mtype_id("mon_handy_hauler"));
    CHECK(hauler->friendly == -1);
    CHECK(hauler->has_flag(MF_PACIFIST));
    CHECK(units::to_gram(hauler->weight_capacity()) == Approx(18000).margin(1));
    CHECK(hauler->type->revert_to_itype == itype_id("bot_handy_hauler"));
    g->clear_zombies();
}
