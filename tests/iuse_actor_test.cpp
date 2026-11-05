#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "item.h"
#include "monster.h"
#include "mtype.h"
#include "player.h"
#include "state_helpers.h"
#include "string_id.h"
#include "type_id.h"

#include <climits>
#include <memory>

static auto find_adjacent_monster(const tripoint_abs_ms& pos_abs) -> monster* { // *NOPAD*
    tripoint_abs_ms target_abs = pos_abs;
    for (target_abs.x() = pos_abs.x() - 1; target_abs.x() <= pos_abs.x() + 1; target_abs.x()++) {
        for (target_abs.y() = pos_abs.y() - 1; target_abs.y() <= pos_abs.y() + 1;
             target_abs.y()++) {
            if (target_abs == pos_abs) { continue; }
            if (monster* const candidate = g->critter_at<monster>(abs_to_bub(target_abs))) {
                return candidate;
            }
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

    monster* new_manhack = find_adjacent_monster(dummy.abs_pos());
    REQUIRE(new_manhack == nullptr);

    dummy.invoke_item(&test_item);

    test_item_pos = dummy.inv_position_by_item(&test_item);
    REQUIRE(test_item_pos == INT_MIN);

    new_manhack = find_adjacent_monster(dummy.abs_pos());
    REQUIRE(new_manhack != nullptr);
    REQUIRE(new_manhack->type->id == mtype_id("mon_manhack"));
    g->clear_zombies();
}
