#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "explosion.h"
#include "map_helpers.h"
#include "monster.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

#include <string>

static const mtype_id mon_dog("mon_dog");
static const mtype_id mon_uafv_xm246e1("mon_uafv_xm246e1");

TEST_CASE("make_friendly_does_not_downgrade_permanent_friendly", "[monster][friendly]") {
    clear_all_state();
    move_player_out_of_the_way();

    auto& mon = spawn_test_monster(mon_dog.str(), tripoint_bub_ms(30, 30, 0));

    SECTION("hostile monster becomes temporarily friendly") {
        mon.friendly = 0;
        mon.make_friendly();
        CHECK(mon.friendly > 0);
    }

    SECTION("permanently friendly monster stays permanent") {
        mon.friendly = -1;
        mon.make_friendly();
        CHECK(mon.friendly == -1);
    }
}

TEST_CASE("emp_does_not_make_permanent_friendly_robot_temporary", "[explosion][emp][monster]") {
    clear_all_state();
    move_player_out_of_the_way();

    const auto pos = tripoint_bub_ms(30, 30, 0);
    auto& bot = spawn_test_monster(mon_uafv_xm246e1.str(), pos);
    REQUIRE(bot.has_flag(MF_ELECTRONIC));

    SECTION("emp_blast keeps permanently friendly robot permanent") {
        bot.friendly = -1;
        for (auto i = 0; i < 100; i++) {
            bot.set_hp(bot.get_hp_max());
            explosion_handler::emp_blast(pos);
            REQUIRE_FALSE(bot.is_dead());
            CHECK(bot.friendly == -1);
        }
    }

    SECTION("emp_blast can still make hostile robot temporarily friendly") {
        bot.friendly = 0;
        for (auto i = 0; i < 100 && bot.friendly == 0; i++) {
            bot.set_hp(bot.get_hp_max());
            explosion_handler::emp_blast(pos);
            REQUIRE_FALSE(bot.is_dead());
        }
        CHECK(bot.friendly > 0);
    }

    SECTION("scrambler_blast keeps permanently friendly robot permanent") {
        bot.friendly = -1;
        explosion_handler::scrambler_blast(pos);
        CHECK(bot.friendly == -1);
    }

    SECTION("scrambler_blast makes hostile robot temporarily friendly") {
        bot.friendly = 0;
        explosion_handler::scrambler_blast(pos);
        CHECK(bot.friendly > 0);
    }
}

TEST_CASE("lua_friendly_bindings_keep_permanent_friendly", "[lua][monster][friendly]") {
    clear_all_state();
    move_player_out_of_the_way();

    auto& mon = spawn_test_monster(mon_dog.str(), tripoint_bub_ms(30, 30, 0));
    auto lua = make_lua_state();
    lua["test_monster"] = &mon;

    const auto run = [&](const std::string& code) {
        const auto res = lua.safe_script(code, sol::script_pass_on_error);
        REQUIRE(res.valid());
    };

    SECTION("make_friendly") {
        mon.friendly = -1;
        run("test_monster:make_friendly()");
        CHECK(mon.friendly == -1);
    }

    SECTION("set_attitude MATT_FRIEND") {
        mon.friendly = -1;
        run("test_monster:set_attitude(MonsterAttitude.MATT_FRIEND)");
        CHECK(mon.friendly == -1);
    }

    SECTION("set_attitude MATT_FPASSIVE") {
        mon.friendly = -1;
        run("test_monster:set_attitude(MonsterAttitude.MATT_FPASSIVE)");
        CHECK(mon.friendly == -1);
    }

    SECTION("set_attitude MATT_FRIEND on hostile monster") {
        mon.friendly = 0;
        run("test_monster:set_attitude(MonsterAttitude.MATT_FRIEND)");
        CHECK(mon.friendly > 0);
    }
}
