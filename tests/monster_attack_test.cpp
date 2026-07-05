#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monattack.h"
#include "monster.h"
#include "state_helpers.h"
#include "type_id.h"

namespace {
const auto effect_dazed = efftype_id("dazed");
const auto effect_shrieking = efftype_id("shrieking");

struct shriek_stun_setup {
    monster& screecher;
    avatar& target;
};

auto setup_shriek_stun_test() -> shriek_stun_setup {
    clear_all_state();

    auto& target = get_avatar();
    const auto screecher_pos = tripoint_bub_ms(60, 60, 0);
    const auto target_pos = tripoint_bub_ms(62, 60, 0);
    target.setpos(map_local_to_abs(get_map(), target_pos));

    auto& screecher = spawn_test_monster("mon_zombie_screecher", screecher_pos);
    screecher.set_dest(target.bub_pos());
    screecher.add_effect(effect_shrieking, 1_minutes);

    return {.screecher = screecher, .target = target};
}

} // namespace

TEST_CASE("hearing protection blocks screecher daze", "[monster][sound]") {
    const auto protected_item = GENERATE("ear_plugs", "army_powered_earmuffs_on");
    CAPTURE(protected_item);

    auto setup = setup_shriek_stun_test();
    REQUIRE(!setup.target.wear_item(item::spawn(protected_item), false));

    REQUIRE(mattack::shriek_stun(&setup.screecher));

    CHECK_FALSE(setup.target.has_effect(effect_dazed));
}

TEST_CASE("screecher dazes unprotected targets", "[monster][sound]") {
    auto setup = setup_shriek_stun_test();

    REQUIRE(mattack::shriek_stun(&setup.screecher));

    CHECK(setup.target.has_effect(effect_dazed));
}
