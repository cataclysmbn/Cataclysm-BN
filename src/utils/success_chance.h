#pragma once

class Character;

namespace success_chance {

struct roller {
    bool best = false;
    /// Higher is better for the player.
    auto up(int lo, int hi) const -> int;
    /// Lower is better for the player.
    auto down(int lo, int hi) const -> int;
    auto dice_up(int n, int sides) const -> int;
    auto dice_down(int n, int sides) const -> int;
    auto one_in(int chance) const -> bool;
};

struct hacker {
    int computer = 0;
    int intelligence = 0;
    roller roll;
    /// Same skills, best roll for the player.
    auto best() const -> hacker;
};

auto hacker_of(const Character& who) -> hacker;

/// True on success. Add alerts to security first.
auto computer_hack(const hacker& h, int security) -> bool;
/// Success at >= 0. Friendly robot possible at >= -2.
auto robot_override(const hacker& h, int difficulty, bool mech) -> float;
/// Success at >= 0.
auto vehicle_hack(const hacker& h) -> int;
/// Success at > 0.
auto decrypt_memory_card(const hacker& h) -> int;
/// Success at >= 0.
auto fish_trap(int spot, int bait, int survival) -> int;

} // namespace success_chance
