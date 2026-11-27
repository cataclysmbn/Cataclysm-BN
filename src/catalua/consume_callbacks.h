#pragma once

#include <optional>

class Character;
class item;
struct consume_overrides;

/// Values the game applied when a comestible was consumed, Lua overrides and
/// enchantments included.
struct consumed_values {
    int kcal = 0;
    int thirst = 0;
    int fun = 0;
    int fun_max = 0;
};

/// Must be called before the meal enters the consumption history: the
/// monotony penalty reads the history, and `Character::modify_morale` applies
/// this value.
auto applied_fun_for(const Character& who, const item& food, const consume_overrides& overrides)
    -> consumed_values;

/// Parenteral (NO_INGEST) medication applies no nutrition or thirst; call it
/// before the medication's effects are applied.
auto consumed_med_values(const Character& who, const item& med, const consume_overrides& overrides)
    -> consumed_values;

/// Returns std::nullopt when either one denies the consume.
auto run_try_consume_callbacks(Character& who, item& food) -> std::optional<consume_overrides>;

/// @p applied carries the values the game applied; return values are ignored.
auto run_consume_callbacks(Character& who, item& food, const consumed_values& applied) -> void;
