#pragma once
#ifndef CATA_SRC_CHARACTER_STATE_PROVIDER_H
#define CATA_SRC_CHARACTER_STATE_PROVIDER_H

#include <optional>
#include <string>
#include <vector>

class Character;

/**
 * Returns the active state for a modifier group.
 * Supported: "movement_mode" (walk/run/crouch), "downed", "lying_down".
 */
std::optional<std::string> get_character_state_for_group(
    const Character &ch,
    const std::string &group_id
);

/** Returns list of supported modifier group IDs. */
std::vector<std::string> get_supported_modifier_groups();

#endif // CATA_SRC_CHARACTER_STATE_PROVIDER_H
