#pragma once
#ifndef CATA_SRC_CHARACTER_STATE_PROVIDER_H
#define CATA_SRC_CHARACTER_STATE_PROVIDER_H

#include <optional>
#include <string>
#include <vector>

class Character;

/**
 * @file character_state_provider.h
 * @brief Provides character state queries for the UV modifier rendering system.
 *
 * This module defines the interface for querying character states that can be
 * used by tilesets to apply UV-based sprite modifications. Tileset authors can
 * define modifier groups (e.g., "movement_mode", "downed") and this module
 * provides the mapping from game state to the string identifiers used in JSON.
 */

/**
 * Returns the active state string for a given modifier group ID.
 *
 * @param ch The character to query state from
 * @param group_id The modifier group identifier (e.g., "movement_mode", "downed")
 * @return The current state string (e.g., "walk", "crouch", "downed"), or
 *         std::nullopt if the group_id is not recognized
 *
 * Supported group IDs:
 * - "movement_mode": Returns "walk", "run", or "crouch"
 * - "downed": Returns "normal" or "downed"
 * - "lying_down": Returns "normal" or "lying"
 */
std::optional<std::string> get_character_state_for_group(
    const Character &ch,
    const std::string &group_id
);

/**
 * Returns a list of all supported modifier group IDs.
 *
 * This can be used for validation or documentation purposes to inform
 * tileset authors what groups are available.
 *
 * @return Vector of supported group ID strings
 */
std::vector<std::string> get_supported_modifier_groups();

#endif // CATA_SRC_CHARACTER_STATE_PROVIDER_H
