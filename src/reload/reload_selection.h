#pragma once

#include "reload.h"

#include <vector>

class item;
class player;

namespace reload_selection {
struct selection_options {
    /** Ask the player to choose even when discovery finds only one option. */
    bool prompt = false;
    /** Filters passed to `reload::discover_ammo`. */
    reload::discovery_options discovery;
};

/** What the caller must do next with a `selection_result`. */
enum class selection_outcome {
    /** `selected` holds the chosen option. */
    automatic,
    /** The player must choose from `options`. */
    interaction_required,
    /** The caller supplied no options. */
    empty_supplied,
    /** Discovery found no options, and the base lacks a magazine that it needs. */
    missing_magazine,
    /** Discovery found no options, but some source matches. The target may be full. */
    nothing_to_reload,
    /** Discovery found no options, and no source matches the ammunition type. */
    missing_ammunition
};

struct selection_result {
    selection_outcome outcome;
    /** Chosen option. Valid only when `outcome` is `automatic`. */
    item_reload_option selected{};
    /** Options for the player. Filled only when `outcome` is `interaction_required`. */
    std::vector<item_reload_option> options;
};

/**
 * Sort options into preference order. The sort is stable, and the keys in priority order are:
 *  1. sources that still hold ammunition before empty ones
 *  2. lower `moves()` first
 *  3. more ammunition remaining first
 */
auto order_ammo(std::vector<item_reload_option>& options) -> void;

/**
 * Discovers and sorts reload options. NPCs take the first option; the player gets a choice
 * (unless there is only one option and prompting is disabled).
 *
 * Returns a failure outcome when discovery finds no options, or empty_supplied
 * if the base is a holster.
 *
 * Does not display UI, change items, or update reload history.
 * @param who Character who performs the reload
 * @param base Item to reload
 * @param options Prompt and discovery controls
 */
auto prepare(const player& who, item& base, selection_options options = {}) -> selection_result;

/**
 * Decide on a list the caller built. This overload neither discovers nor sorts.
 *  - An empty list returns `empty_supplied`.
 *  - An NPC takes the first option, as `automatic`.
 *  - A player gets `interaction_required` with the list in its given order.
 * @param who Character who performs the reload
 * @param options Candidate reloads, in the order the caller wants them
 */
auto prepare(const player& who, std::vector<item_reload_option> options) -> selection_result;
} // namespace reload_selection
