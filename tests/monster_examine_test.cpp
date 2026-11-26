#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "map_helpers.h"
#include "monster.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

#include <string>

TEST_CASE("monster examine describes action readiness without raw move points", "[monster][ui]") {
    clear_all_state();
    clear_map();

    auto& mon = spawn_test_monster("mon_zombie", get_avatar().bub_pos() + tripoint_east);
    REQUIRE(mon.get_speed() > 0);

    mon.set_moves(50);
    auto desc = mon.extended_description();
    CHECK(desc.find("It can act right now.") != std::string::npos);
    CHECK(desc.find("Moves:") == std::string::npos);

    mon.set_moves(0);
    mon.process_turn();
    const int credit = mon.get_moves();
    REQUIRE(credit > 0);

    mon.set_moves(-1);
    desc = mon.extended_description();
    CHECK(desc.find("It will be ready next turn.") != std::string::npos);

    mon.set_moves(-credit - 1);
    desc = mon.extended_description();
    CHECK(desc.find("It will be ready in 2 turns.") != std::string::npos);

    get_avatar().set_mutation(trait_id("INATTENTIVE"));
    mon.set_moves(50);
    desc = mon.extended_description();
    CHECK(desc.find("It can act right now.") == std::string::npos);
    CHECK(desc.find("It will be ready") == std::string::npos);
}
