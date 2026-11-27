#include "catalua/consume_callbacks.h"

#include "catalua_hooks.h"
#include "catalua_icallback_actor.h"
#include "catalua_sol.h"
#include "character.h"
#include "flag.h"
#include "item.h"
#include "itype.h"
#include "type_id.h"

namespace {

const auto ench_val_FOOD_FUN = enchantment_value_id("FOOD_FUN");

} // namespace

auto applied_fun_for(const Character& who, const item& food, const consume_overrides& overrides)
    -> consumed_values {
    const auto base = who.fun_for(food);
    auto fun = overrides.fun ? *overrides.fun : base.first;
    const auto fun_max = overrides.fun_max ? *overrides.fun_max : base.second;
    // Same expression as Character::modify_morale, so rounding matches.
    fun += who.bonus_from_enchantments(fun, ench_val_FOOD_FUN);
    return {.fun = fun, .fun_max = fun_max};
}

auto consumed_med_values(const Character& who, const item& med, const consume_overrides& overrides)
    -> consumed_values {
    auto values = applied_fun_for(who, med, overrides);
    if (med.has_flag(flag_NO_INGEST)) { return values; }
    values.kcal = overrides.kcal ? *overrides.kcal : who.compute_effective_nutrients(med).kcal;
    values.thirst = overrides.thirst ? *overrides.thirst : med.get_comestible()->quench;
    return values;
}

auto run_try_consume_callbacks(Character& who, item& food) -> std::optional<consume_overrides> {
    auto overrides = consume_overrides{};
    if (cata::has_hooks("on_character_try_consume")) {
        const auto& hook_results = cata::run_hooks(
            "on_character_try_consume",
            [&](sol::table& params) {
                params["char"] = &who;
                params["item"] = &food;
                params["nutrition"] = who.compute_effective_nutrients(food).kcal;
                params["thirst"] = food.get_comestible()->quench;
                const auto fun = who.fun_for(food);
                params["fun"] = fun.first;
                params["fun_max"] = fun.second;
            },
            {.exit_early = true});
        if (!hook_results.get_or("allowed", true)) { return std::nullopt; }
        if (const auto kcal = hook_results.get<sol::optional<int>>("nutrition")) {
            overrides.kcal = kcal.value();
        }
        if (const auto quench = hook_results.get<sol::optional<int>>("thirst")) {
            overrides.thirst = quench.value();
        }
        if (const auto fun = hook_results.get<sol::optional<int>>("fun")) {
            overrides.fun = fun.value();
        }
        if (const auto fun_max = hook_results.get<sol::optional<int>>("fun_max")) {
            overrides.fun_max = fun_max.value();
        }
        if (const auto skip = hook_results.get<sol::optional<bool>>("skip_cannibalism")) {
            overrides.skip_cannibalism = skip.value();
        }
    }

    if (const lua_iconsume_actor* iconsume_cb = food.type->iconsume_callbacks) {
        if (!iconsume_cb->call_on_try_consume(who, food, overrides)) { return std::nullopt; }
    }
    return overrides;
}

auto run_consume_callbacks(Character& who, item& food, const consumed_values& applied) -> void {
    if (cata::has_hooks("on_character_consume")) {
        cata::run_hooks("on_character_consume", [&](sol::table& params) {
            params["char"] = &who;
            params["item"] = &food;
            params["nutrition"] = applied.kcal;
            params["thirst"] = applied.thirst;
            params["fun"] = applied.fun;
            params["fun_max"] = applied.fun_max;
        });
    }
    if (const lua_iconsume_actor* iconsume_cb = food.type->iconsume_callbacks) {
        iconsume_cb->call_on_consume(who, food, applied);
    }
}
