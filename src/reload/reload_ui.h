#pragma once

#include "reload_selection.h"
#include "type_id.h"

#include <vector>

class item;
class player;

namespace reload_ui {
/**
 * Select suitable ammo with which to reload the item.
 * @param who Character who looks for ammo
 * @param base Item to select ammo for
 * @param options Selection controls for prompting, empty magazines, and potential ammo
 */
auto select_ammo(const player& who, item& base, reload_selection::selection_options options = {})
    -> item_reload_option;
/** Select ammo from the provided options. */
auto select_ammo(const player& who, item& base, std::vector<item_reload_option> options)
    -> item_reload_option;

struct hotkey_state {
    itype_id last_ammo;
    int opening_key;
    bool opening_key_bound;
    int fallback_index;
};

struct hotkey_row {
    itype_id ammo_type;
    char inherited_hotkey;
    int index;
};

auto prepare_hotkeys(const itype_id& last_ammo, int opening_key) -> hotkey_state;
auto assign_hotkey(hotkey_state& state, const hotkey_row& row) -> char;
} // namespace reload_ui
