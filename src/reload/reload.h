#pragma once

#include <climits>
#include <vector>

class Character;
class item;
class player;

/**
 * One candidate reload: pairing a source item paired with a target that receives its ammunition.
 * Does not own the referenced character or items; those must remain valid while the option is used.
 */
class item_reload_option {
public:
    item_reload_option() = default;

    item_reload_option(const item_reload_option&);
    auto operator=(const item_reload_option&) -> item_reload_option&;

    /**
     * Build an option that transfers as much ammunition as the source and target allow.
     * @param who Character who performs the reload
     * @param target Item that receives the ammunition
     * @param parent Base item the reload was requested for; may equal @p target
     * @param ammo Source item
     */
    item_reload_option(const player* who, item* target, const item* parent, item& ammo);

    /** Character who performs the reload. */
    const player* who = nullptr;
    /** Item that receives the ammunition: the base, a gunmod, or an installed magazine. */
    item* target = nullptr;
    /** Source item: loose ammunition, a magazine, a speedloader, or a container. */
    item* ammo;

    /** Amount of ammunition this option transfers. */
    auto qty() const -> int { return qty_; }

    /**
     * Set the amount of ammunition to transfer.
     *
     * The setter clamps @p val to the smallest of these limits:
     *  - the ammunition the source holds
     *  - the capacity left in the target
     *  - `max_qty`
     *
     * `RELOAD_ONE` limits the quantity to one, unless the source is a speedloader.
     * The setter then raises the result to at least 1.
     */
    void qty(int val);

    /**
     * Move cost for this option, including source acquisition and target reload cost.
     * Reloading a separate target adds the parent's reload time for a gun, or 100 moves
     * for a tool.
     */
    auto moves() const -> int;

    /**
     * True when the option names a character, target, and source, with a positive quantity.
     * A default-constructed option is false, and callers use it to mean "nothing selected".
     */
    explicit operator bool() const { return who && target && ammo && qty_ > 0; }

private:
    int qty_ = 0;
    /** Limit beyond the target's capacity. An ammo belt sets it to the linkages carried. */
    int max_qty = INT_MAX;
    /** Base item the reload was requested for. */
    const item* parent = nullptr;
};

namespace reload {
/** Filters for `discover_ammo`. */
struct discovery_options {
    /** Offer empty magazines as sources. */
    bool include_empty_mags = true;
    /**
     * Also offer sources whose type the target accepts but the character cannot load now.
     * An example is a speedloader for a revolver that still holds rounds.
     */
    bool include_potential = false;
};

struct discovery_result {
    /** Candidate reloads in discovery order, unsorted. */
    std::vector<item_reload_option> options;
    /**
     * True when some source matches the ammunition type of some target.
     * The flag can be true while `options` is empty.
     * `reload_selection::prepare` uses the flag to tell "nothing to reload" from missing
     * ammunition.
     * A speedloader counts only against an empty target, unless `include_potential` is set.
     */
    bool ammo_match_found = false;
};

/**
 * Find every source and target pair that @p who could use to reload @p base.
 *
 * Discovery visits targets in this order:
 *
 *     gunmods -> base -> base's current magazine -> each gunmod's current magazine
 *
 * For each target, the search visits sources in this order:
 *
 *     carried items -> ground within one tile -> vehicle cargo within one tile
 *
 * A mounted character searches carried items only.
 *
 * @param who Character who looks for ammunition
 * @param base Item to find ammunition for
 * @param options Filters for empty magazines and potential sources
 * @return Unsorted options and the match flag. `reload_selection::order_ammo` sorts the options.
 */
auto discover_ammo(const Character& who, item& base, discovery_options options = {})
    -> discovery_result;
} // namespace reload
