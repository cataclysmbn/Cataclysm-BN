#include "utils/success_chance.h"

#include "character.h"
#include "rng.h"

#include <algorithm>

namespace success_chance {

auto roller::up(const int lo, const int hi) const -> int {
    return best ? std::max(lo, hi) : rng(lo, hi);
}

auto roller::down(const int lo, const int hi) const -> int {
    return best ? std::min(lo, hi) : rng(lo, hi);
}

auto roller::dice_up(const int n, const int sides) const -> int {
    return best ? std::max(0, n) * std::max(1, sides) : dice(n, sides);
}

auto roller::dice_down(const int n, const int sides) const -> int {
    return best ? std::max(0, n) * std::min(1, sides) : dice(n, sides);
}

auto roller::one_in(const int chance) const -> bool { return best || ::one_in(chance); }

auto hacker::best() const -> hacker {
    return {.computer = computer, .intelligence = intelligence, .roll = {.best = true}};
}

auto hacker_of(const Character& who) -> hacker {
    return {.computer = who.get_skill_level(skill_id("computer")), .intelligence = who.int_cur};
}

auto computer_hack(const hacker& h, const int security) -> bool {
    const auto& [computer, intelligence, r] = h;
    auto player_roll = computer;
    ///\EFFECT_INT <8 randomly penalizes hack attempts, 50% of the time
    if (intelligence < 8 && r.one_in(2)) {
        player_roll -= r.down(0, 8 - intelligence);
        ///\EFFECT_INT >8 randomly benefits hack attempts, 33% of the time
    } else if (intelligence > 8 && r.one_in(3)) {
        player_roll += r.up(0, intelligence - 8);
    }
    ///\EFFECT_COMPUTER increases chance of successful hack attempt, vs Security level
    return r.dice_up(player_roll, 6) >= r.dice_down(security, 6);
}

auto robot_override(const hacker& h, const int difficulty, const bool mech) -> float {
    const auto& [computer, intelligence, r] = h;
    /// @EFFECT_INT increases chance of successful robot reprogramming, vs difficulty
    /// @EFFECT_COMPUTER increases chance of successful robot reprogramming, vs difficulty
    const auto randomized_skill = static_cast<float>(r.up(2, intelligence) + computer);
    // rideable mechs are not hostile, they have no AI, they do not resist control as much.
    return mech ? randomized_skill - r.down(1, 11) : computer - 3 * difficulty / randomized_skill;
}

auto vehicle_hack(const hacker& h) -> int {
    /// @EFFECT_INT increases chance of bypassing vehicle security system
    /// @EFFECT_COMPUTER increases chance of bypassing vehicle security system
    return h.roll.dice_up(h.computer + 2, h.intelligence) - 50;
}

auto decrypt_memory_card(const hacker& h) -> int {
    const auto& [computer, intelligence, r] = h;
    /// @EFFECT_INT increases chance of safely decrypting memory card
    /// @EFFECT_COMPUTER increases chance of safely decrypting memory card
    return computer * r.up(1, computer) * r.up(1, intelligence) - r.down(30, 80);
}

auto fish_trap(const int spot, const int bait, const int survival) -> int {
    return -250 + 15 * spot + bait * (survival + 2 * spot);
}

} // namespace success_chance
