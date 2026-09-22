#pragma once

#include <climits>
#include <vector>

class Character;
class item;
class player;

class item_reload_option {
public:
    item_reload_option() = default;

    item_reload_option(const item_reload_option&);
    item_reload_option& operator=(const item_reload_option&);

    item_reload_option(const player* who, item* target, const item* parent, item& ammo);

    const player* who = nullptr;
    item* target = nullptr;
    item* ammo;

    int qty() const { return qty_; }
    void qty(int val);

    int moves() const;

    explicit operator bool() const { return who && target && ammo && qty_ > 0; }

private:
    int qty_ = 0;
    int max_qty = INT_MAX;
    const item* parent = nullptr;
};

namespace reload {
struct discovery_options {
    bool include_empty_mags = true;
    bool include_potential = false;
};

struct discovery_result {
    std::vector<item_reload_option> options;
    bool ammo_match_found = false;
};

/**
 * List ammo suitable for the given item.
 * @param who Character who looks for ammo
 * @param base Item to select ammo for
 * @param options Whether to include empty magazines and ammo that can potentially be used
 */
auto discover_ammo(const Character& who, item& base, discovery_options options = {})
    -> discovery_result;
} // namespace reload
