#pragma once

#include <functional>
#include <expected>

class Creature;

namespace creature_functions
{
struct auto_find_hostile_target_option {
    /// maximum range to look for monsters, anything outside of that range is ignored
    int range = 0;
    /// whether projectile leaves dangerous trail (e.g., lasers)
    bool trail = false;
    /// The area of effect of the projectile aimed.
    int area = 0;
};

/// For fake-players (turrets, mounted turrets) this functions
/// chooses a target. This is for creatures that are friendly towards
/// the player and therefor choose a target that is hostile
/// to the player.
///
/// Meant to deprecate Creature::auto_find_hostile_target
/// HACK: internally moves creature temporarily to vehicle boundary positions, need to fix before adding on_moved hooks
///
/// @returns either a reference to the found target creature, or
///          the number of targets that have been skipped
///          because the player is in the way.
auto auto_find_hostile_target(
    const Creature &creature,
    const auto_find_hostile_target_option &option
) -> std::expected<std::reference_wrapper<Creature>, int>;

} // namespace creature_functions
