#pragma once

#include <optional>

class Character;
class item;
struct consume_overrides;

/// Values the game applied when a comestible was consumed (Lua overrides and
/// enchantments included). Passed to the post-consume Lua callbacks so they
/// report exactly what was applied.
struct consumed_values {
    int kcal = 0;
    int thirst = 0;
    int fun = 0;
    int fun_max = 0;
};

/// Fun (morale) that eating @p food applies: `Character::fun_for`, then the Lua
/// overrides, then the FOOD_FUN enchantment bonus. `Character::modify_morale`
/// applies this value, so call it before the meal enters the consumption history
/// (the monotony penalty reads the history).
auto applied_fun_for(const Character& who, const item& food, const consume_overrides& overrides)
    -> consumed_values;

/// Values that taking @p med applies, for the post-consume Lua callbacks.
/// Parenteral (NO_INGEST) medication applies no nutrition or thirst.
/// Call it before the medication's effects are applied.
auto consumed_med_values(const Character& who, const item& med, const consume_overrides& overrides)
    -> consumed_values;

/// Runs the `on_character_try_consume` hook and the item's `on_try_consume`
/// callback before a comestible is consumed. Returns the overrides they
/// requested, or std::nullopt when either one denies the consume.
auto run_try_consume_callbacks(Character& who, item& food) -> std::optional<consume_overrides>;

/// Runs the `on_character_consume` hook and the item's `on_consume` callback
/// after @p food was actually consumed. @p applied carries the values the game
/// applied; return values are ignored.
auto run_consume_callbacks(Character& who, item& food, const consumed_values& applied) -> void;
