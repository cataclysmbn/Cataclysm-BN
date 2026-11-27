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
        std::make_unique<lua_iconsume_actor>(
            "blood", result.get<sol::protected_function>(), sol::protected_function(sol::lua_nil)));
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

/// Tests for the post-consume `on_character_consume` hook and the per-item
/// `on_consume` callback (`lua_iconsume_actor`). Both are fired from
/// `Character::eat` (`src/consumption.cpp`) after the comestible has actually
/// been consumed, so the test checks that they see the applied (override-aware)
/// values and the already-deducted charges, and that neither runs when the
/// consume is denied beforehand.
TEST_CASE("on_character_consume hook and on_consume callback", "[consume_hook][lua]") {
    clear_all_state();
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    auto hooks = lua["game"]["hooks"].get<sol::table>();
    REQUIRE(hooks.valid());
    const auto previous = hooks["on_character_consume"].get<sol::object>();
    const auto restore = on_out_of_scope([&]() {
        hooks["on_character_consume"] = previous;
        lua.globals()["consume_hook_seen"] = sol::lua_nil;
        lua.globals()["iconsume_gate"] = sol::lua_nil;
        lua.globals()["iconsume_on_consume_seen"] = sol::lua_nil;
        clear_all_state();
    });

    // Registers the hook function returned by `chunk` as the only
    // `on_character_consume` entry.
    const auto register_hook = [&](const std::string& chunk) {
        const auto result = lua.safe_script(chunk, sol::script_pass_on_error);
        REQUIRE(result.valid());
        auto hook_table = lua.create_table();
        hook_table[1] = result.get<sol::function>();
        hooks["on_character_consume"] = hook_table;
    };

    // Replaces the global per-item actor for "blood": the try callback is
    // gated by the `iconsume_gate` global (an unset gate allows consumption),
    // the on_consume callback records its params into a global.
    const auto try_cb = lua.safe_script(
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
    REQUIRE(try_cb.valid());
    const auto consume_cb = lua.safe_script(
        R"(
        iconsume_on_consume_seen = nil
        return function( params )
            iconsume_on_consume_seen = {
                nutrition = params.nutrition,
                thirst = params.thirst,
                fun = params.fun,
                fun_max = params.fun_max,
                item_id = params.item:get_type():str(),
                charges = params.item.charges,
            }
        end
    )",
        sol::script_pass_on_error);
    REQUIRE(consume_cb.valid());
    item_controller->add_iconsume_actor(
        itype_id("blood"),
        std::make_unique<lua_iconsume_actor>(
            "blood", try_cb.get<sol::protected_function>(),
            consume_cb.get<sol::protected_function>()));
    item_controller->resolve_lua_callbacks();

    const auto set_gate = [&](const std::string& chunk) {
        const auto gate = lua.safe_script(chunk, sol::script_pass_on_error);
        REQUIRE(gate.valid());
        lua.globals()["iconsume_gate"] = gate.get<sol::function>();
    };

    avatar dummy;
    item& blood = *item::spawn_temporary("blood", calendar::turn, 10);
    REQUIRE(blood.is_comestible());

    SECTION("hook and callback fire after consumption with the applied values") {
        // The try callback overrides nutrition and thirst; the post-consume
        // hook and callback must report the overridden (applied) values.
        set_gate(R"(
            return function( params )
                params.nutrition = 500
                params.thirst = 100
            end
        )");
        register_hook(R"(
            return function( params )
                consume_hook_seen = {
                    nutrition = params.nutrition,
                    thirst = params.thirst,
                    fun = params.fun,
                    fun_max = params.fun_max,
                    item_id = params.item:get_type():str(),
                    charges = params.item.charges,
                    is_avatar = params.char:is_avatar(),
                }
            end
        )");

        dummy.set_thirst(200);
        REQUIRE(dummy.eat(blood, true));
        CHECK(blood.charges == 9);
        CHECK(dummy.stomach.get_calories() == 500);
        CHECK(dummy.get_thirst() == 100);
        // No fun override, so the base fun of -50 applies.
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == -50);

        // The hook runs after the charge was deducted and the effects applied.
        const auto seen = lua.globals()["consume_hook_seen"].get<sol::table>();
        REQUIRE(seen.valid());
        CHECK(seen["nutrition"].get<int>() == 500);
        CHECK(seen["thirst"].get<int>() == 100);
        CHECK(seen["fun"].get<int>() == -50);
        CHECK(seen["fun_max"].get<int>() == -300);
        CHECK(seen["item_id"].get<std::string>() == "blood");
        CHECK(seen["charges"].get<int>() == 9);
        CHECK(seen["is_avatar"].get<bool>());

        // The per-item callback sees the same applied values.
        const auto cb_seen = lua.globals()["iconsume_on_consume_seen"].get<sol::table>();
        REQUIRE(cb_seen.valid());
        CHECK(cb_seen["nutrition"].get<int>() == 500);
        CHECK(cb_seen["thirst"].get<int>() == 100);
        CHECK(cb_seen["fun"].get<int>() == -50);
        CHECK(cb_seen["fun_max"].get<int>() == -300);
        CHECK(cb_seen["item_id"].get<std::string>() == "blood");
        CHECK(cb_seen["charges"].get<int>() == 9);
    }

    SECTION("neither fires when the consume is denied") {
        set_gate(R"(
            return function( params )
                return false
            end
        )");
        register_hook(R"(
            return function( params )
                consume_hook_seen = true
            end
        )");

        REQUIRE_FALSE(dummy.eat(blood, true));
        CHECK(blood.charges == 10);
        CHECK(dummy.get_morale(MORALE_FOOD_BAD) == 0);
        // Neither the hook nor the on_consume callback ran, so the marker
        // globals stay unset. Check for nil: the callback stores a table, so
        // reading it as a bool would pass even when the callback ran.
        CHECK_FALSE(lua.globals()["consume_hook_seen"].get<sol::object>().valid());
        CHECK_FALSE(lua.globals()["iconsume_on_consume_seen"].get<sol::object>().valid());
    }

    SECTION("hook reports the applied fun when the monotony penalty applies") {
        // Cheese has positive fun and the default monotony penalty, so each
        // repeat meal within two days is less fun. The hook runs after the
        // meal enters the consumption history and must still report the fun
        // that was applied, not one recomputed with the extra history entry.
        register_hook(R"(
            return function( params )
                consume_hook_seen = params.fun
            end
        )");
        item& cheese = *item::spawn_temporary("cheese", calendar::turn, 8);
        REQUIRE(cheese.get_comestible_fun() > 0);
        REQUIRE(cheese.get_comestible()->monotony_penalty > 0);

        const auto first_fun = dummy.fun_for(cheese).first;
        REQUIRE(dummy.eat(cheese, true));
        CHECK(lua.globals()["consume_hook_seen"].get<int>() == first_fun);

        const auto second_fun = dummy.fun_for(cheese).first;
        REQUIRE(second_fun < first_fun);
        REQUIRE(dummy.eat(cheese, true));
        CHECK(lua.globals()["consume_hook_seen"].get<int>() == second_fun);
    }
}

/// Tests that medication fires the same Lua consume callbacks as food:
/// `Character::consume_med` runs the `on_character_try_consume` hook before
/// taking the medication (it can deny it) and the `on_character_consume` hook
/// after the charge was used. Aspirin is an oral (ingested) medication.
TEST_CASE("consume hooks fire for medication", "[consume_hook][lua]") {
    clear_all_state();
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    auto hooks = lua["game"]["hooks"].get<sol::table>();
    REQUIRE(hooks.valid());
    const auto previous_try = hooks["on_character_try_consume"].get<sol::object>();
    const auto previous_post = hooks["on_character_consume"].get<sol::object>();
    const auto restore = on_out_of_scope([&]() {
        hooks["on_character_try_consume"] = previous_try;
        hooks["on_character_consume"] = previous_post;
        lua.globals()["med_try_seen"] = sol::lua_nil;
        lua.globals()["med_post_seen"] = sol::lua_nil;
        clear_all_state();
    });

    // Registers the function returned by `chunk` as the only entry of `hook`.
    const auto register_hook = [&](const std::string& hook, const std::string& chunk) {
        const auto result = lua.safe_script(chunk, sol::script_pass_on_error);
        REQUIRE(result.valid());
        auto hook_table = lua.create_table();
        hook_table[1] = result.get<sol::function>();
        hooks[hook] = hook_table;
    };
    register_hook("on_character_consume", R"(
        return function( params )
            med_post_seen = {
                item_id = params.item:get_type():str(),
                charges = params.item.charges,
            }
        end
    )");

    avatar dummy;
    item& aspirin = *item::spawn_temporary("aspirin", calendar::turn, 20);
    REQUIRE(aspirin.is_medication());

    SECTION("both hooks fire when the medication is taken") {
        register_hook("on_character_try_consume", R"(
            return function( params )
                med_try_seen = params.item:get_type():str()
            end
        )");
        dummy.consume_med(aspirin);
        CHECK(aspirin.charges < 20);
        CHECK(lua.globals()["med_try_seen"].get<std::string>() == "aspirin");
        const auto seen = lua.globals()["med_post_seen"].get<sol::table>();
        REQUIRE(seen.valid());
        CHECK(seen["item_id"].get<std::string>() == "aspirin");
        CHECK(seen["charges"].get<int>() == aspirin.charges);
    }

    SECTION("the try hook can deny the medication") {
        register_hook("on_character_try_consume", R"(
            return function( params )
                return false
            end
        )");
        CHECK_FALSE(dummy.consume_med(aspirin));
        CHECK(aspirin.charges == 20);
        CHECK_FALSE(lua.globals()["med_post_seen"].get<sol::object>().valid());
    }
}
