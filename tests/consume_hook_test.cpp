#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catalua.h"
#include "catalua_icallback_actor.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "init.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "morale_types.h"
#include "state_helpers.h"
#include "type_id.h"

#include <string>

// Tests for the `on_character_try_consume` Lua hook fired from `Character::eat`
// (`src/consumption.cpp`).

TEST_CASE("on_character_try_consume hook", "[consume_hook][lua]") {
    clear_all_state();
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    auto hooks = lua["game"]["hooks"].get<sol::table>();
    REQUIRE(hooks.valid());
    const auto previous = hooks["on_character_try_consume"].get<sol::object>();
    const auto restore = on_out_of_scope([&]() {
        hooks["on_character_try_consume"] = previous;
        lua.globals()["consume_hook_seen"] = sol::lua_nil;
        clear_all_state();
    });

    // Registers the hook function returned by `chunk` as the only
    // `on_character_try_consume` entry.
    const auto register_hook = [&](const std::string& chunk) {
        const auto result = lua.safe_script(chunk, sol::script_pass_on_error);
        REQUIRE(result.valid());
        auto hook_table = lua.create_table();
        hook_table[1] = result.get<sol::function>();
        hooks["on_character_try_consume"] = hook_table;
    };

    avatar dummy;
    item& blood = *item::spawn_temporary("blood", calendar::turn, 10);
    REQUIRE(blood.is_comestible());

    SECTION("hook sees nutrition values and allowing proceeds normally") {
        register_hook(R"(
            return function( params )
                consume_hook_seen = {
                    nutrition = params.nutrition,
                    thirst = params.thirst,
                    fun = params.fun,
                    fun_max = params.fun_max,
                    item_id = params.item:get_type():str(),
                    is_avatar = params.char:is_avatar(),
                }
            end
        )");

        REQUIRE(dummy.eat(blood, true));
        CHECK(blood.charges == 9);
        // Normal consumption happened: blood has fun -50.
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == -50);
        // Without a skip the human-flesh guilt applies after eating.
        CHECK(dummy.get_morale(MORALE_CANNIBAL) == -60);

        const auto seen = lua.globals()["consume_hook_seen"].get<sol::table>();
        REQUIRE(seen.valid());
        CHECK(seen["nutrition"].get<int>() == 43);
        CHECK(seen["thirst"].get<int>() == 5);
        CHECK(seen["fun"].get<int>() == -50);
        CHECK(seen["fun_max"].get<int>() == -300);
        CHECK(seen["item_id"].get<std::string>() == "blood");
        CHECK(seen["is_avatar"].get<bool>());
    }

    SECTION("returning false denies consumption") {
        register_hook(R"(
            return function( params )
                consume_hook_seen = true
                return false
            end
        )");

        REQUIRE_FALSE(dummy.eat(blood, true));
        // The item is untouched and no morale was applied.
        CHECK(blood.charges == 10);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
        CHECK(lua.globals()["consume_hook_seen"].get<bool>());
    }

    SECTION("hook can consume the item itself and deny the normal eat") {
        register_hook(R"(
            return function( params )
                params.item:mod_charges( -1 )
                return false
            end
        )");

        REQUIRE_FALSE(dummy.eat(blood, true));
        // Only the hook's charge change happened, no normal effects.
        CHECK(blood.charges == 9);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
    }

    SECTION("hook result table overrides nutrition, thirst and fun") {
        register_hook(R"(
            return function( params )
                return { nutrition = 500, thirst = 100, fun = 40, fun_max = 200 }
            end
        )");

        dummy.set_thirst(200);
        REQUIRE(dummy.eat(blood, true));
        CHECK(blood.charges == 9);
        CHECK(dummy.stomach.get_calories() == 500);
        CHECK(dummy.get_thirst() == 100);
        // Blood normally has fun -50; the override turns it into a bonus.
        CHECK(dummy.get_morale(MORALE_FOOD_GOOD) == 40);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
    }

    SECTION("hook result table can skip the cannibalism check") {
        register_hook(R"(
            return function( params )
                return { skip_cannibalism = true }
            end
        )");

        // A fresh character is not thirsty; blood has quench 5, which would
        // add the "excess food will be wasted" consequence.
        dummy.set_thirst(50);
        // Blood carries human_flesh_vitamin; without the skip the
        // cannibalism consequence makes will_eat (and eat) fail.
        REQUIRE(dummy.eat(blood, false));
        CHECK(blood.charges == 9);
        // No values were overridden, so the base fun of -50 applies.
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == -50);
        // The skip also suppresses the post-eat human-flesh guilt.
        CHECK(dummy.get_morale(MORALE_CANNIBAL) == 0);
    }
}

// Tests for the per-item `on_try_consume` callback (`lua_iconsume_actor`), the
// item-specific equivalent of the `on_character_try_consume` hook, fired from
// `Character::eat` for items registered via `game.iconsume_functions`.

TEST_CASE("iconsume per-item callback", "[consume_hook][lua]") {
    clear_all_state();
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    const auto restore = on_out_of_scope([&]() {
        lua.globals()["iconsume_gate"] = sol::lua_nil;
        clear_all_state();
    });

    // The actor is registered onto the global item factory, which outlives
    // this test case, so the callback is gated by the `iconsume_gate` global.
    // An unset gate makes the registered callback a no-op that allows
    // consumption, keeping the remaining test suite unaffected.
    const auto result = lua.safe_script(
        R"(
        iconsume_gate = nil
        return function( params )
            local gate = iconsume_gate
            if gate then
                return gate( params )
            end
            return true
        end
    )",
        sol::script_pass_on_error);
    REQUIRE(result.valid());
    item_controller->add_iconsume_actor(
        itype_id("blood"),
        std::make_unique<lua_iconsume_actor>("blood", result.get<sol::protected_function>()));
    item_controller->resolve_lua_callbacks();

    avatar dummy;
    item& blood = *item::spawn_temporary("blood", calendar::turn, 10);
    REQUIRE(blood.is_comestible());

    const auto set_gate = [&](const std::string& chunk) {
        const auto gate = lua.safe_script(chunk, sol::script_pass_on_error);
        REQUIRE(gate.valid());
        lua.globals()["iconsume_gate"] = gate.get<sol::function>();
    };

    SECTION("callback can override nutrition, thirst and fun") {
        set_gate(R"(
            return function( params )
                params.nutrition = 500
                params.thirst = 100
                params.fun = 40
                params.fun_max = 200
            end
        )");

        dummy.set_thirst(200);
        REQUIRE(dummy.eat(blood, true));
        CHECK(blood.charges == 9);
        CHECK(dummy.stomach.get_calories() == 500);
        CHECK(dummy.get_thirst() == 100);
        // Blood normally has fun -50; the override turns it into a bonus.
        CHECK(dummy.get_morale(MORALE_FOOD_GOOD) == 40);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
    }

    SECTION("callback can override the fun_max cap") {
        set_gate(R"(
            return function( params )
                params.fun_max = -50
            end
        )");

        // Blood has fun -50 / fun_max -300: eating twice normally stacks to -100.
        REQUIRE(dummy.eat(blood, true));
        REQUIRE(dummy.eat(blood, true));
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == -50);
    }

    SECTION("callback overrides affect the pre-eat consequences") {
        // Blood carries human_flesh_vitamin; without the CANNIBAL trait
        // will_eat always fails with the cannibalism consequence.
        dummy.toggle_trait(trait_id("CANNIBAL"));
        const efftype_id bloated_effect("bloated");
        dummy.add_effect(bloated_effect, 5_minutes);

        // Without overrides a bloated character refuses to eat (non-interactive).
        CHECK_FALSE(dummy.will_eat(blood, false).success());

        // Explicit overrides passed to will_eat suppress the bloated consequence.
        consume_overrides overrides;
        overrides.kcal = 0;
        overrides.thirst = 0;
        CHECK(dummy.will_eat(blood, false, overrides).success());

        // The same overrides coming from the callback let eat() proceed
        // without the "eat anyway?" prompt (there are no consequences left).
        set_gate(R"(
            return function( params )
                params.nutrition = 0
                params.thirst = 0
            end
        )");

        REQUIRE(dummy.eat(blood, false));
        CHECK(blood.charges == 9);
    }

    SECTION("callback can skip the cannibalism check") {
        set_gate(R"(
            return function( params )
                params.skip_cannibalism = true
            end
        )");

        dummy.set_thirst(50);
        // Blood carries human_flesh_vitamin; the skip removes the
        // cannibalism consequence so eat() needs no confirmation.
        REQUIRE(dummy.eat(blood, false));
        CHECK(blood.charges == 9);
        // No values were overridden, so the base fun of -50 applies.
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == -50);
        // The skip also suppresses the post-eat human-flesh guilt.
        CHECK(dummy.get_morale(MORALE_CANNIBAL) == 0);
    }

    SECTION("returning false interrupts the rest of the consume") {
        set_gate(R"(
            return function( params )
                return false
            end
        )");

        REQUIRE_FALSE(dummy.eat(blood, true));
        CHECK(blood.charges == 10);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
    }

    SECTION("returning a table with allowed = false interrupts the consume") {
        set_gate(R"(
            return function( params )
                return { allowed = false }
            end
        )");

        REQUIRE_FALSE(dummy.eat(blood, true));
        CHECK(blood.charges == 10);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
    }

    SECTION("callback can consume the item itself and interrupt the normal eat") {
        set_gate(R"(
            return function( params )
                params.item:mod_charges( -1 )
                return false
            end
        )");

        REQUIRE_FALSE(dummy.eat(blood, true));
        CHECK(blood.charges == 9);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
    }
}
