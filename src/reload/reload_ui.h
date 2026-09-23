#pragma once

#include "reload_selection.h"
#include "type_id.h"

#include <vector>

class item;
class player;

namespace reload_ui {
/**
 * Select ammunition to reload @p base, with a menu or a message as needed.
 *
 * `select_ammo` runs `reload_selection::prepare` and acts on the outcome:
 *  - `automatic` returns the selected option.
 *  - `interaction_required` shows the reload menu and returns the player's choice.
 *
 * Every other outcome prints the reason to the player and returns an empty option.
 *
 * When the player confirms a menu row, `select_ammo` records that source's type in
 * `uistate.lastreload`. It doesn't schedule an activity or transfer ammunition.
 * @param who Character who performs the reload
 * @param base Item to reload
 * @param options Prompt and discovery controls
 * @return The chosen option, or an empty option if nothing was chosen
 */
auto select_ammo(const player& who, item& base, reload_selection::selection_options options = {})
    -> item_reload_option;

/**
 * Select ammunition from a list the caller built.
 * This overload behaves like the one above, but the menu shows the list in its given order.
 * @param who Character who performs the reload
 * @param base Item being reloaded; determines menu behavior and reload-history lookup.
 * @param options Candidate reloads
 * @return The chosen option, or an empty option if nothing was chosen
 */
auto select_ammo(const player& who, item& base, std::vector<item_reload_option> options)
    -> item_reload_option;

/** Hotkey bookkeeping across the rows of one reload menu. */
struct hotkey_state {
    /** Source type the player last chose for this base's ammunition type. */
    itype_id last_ammo;
    /** Key that opened the menu, such as `r` for reload. */
    int opening_key;
    /** True once `assign_hotkey` has given the opening key a role or ruled one out. */
    bool opening_key_bound;
    /** Row selected by the callback on the opening key, or -1 to disable the fallback */
    int fallback_index;
};

/** One menu row as `assign_hotkey` sees it. */
struct hotkey_row {
    /** Type of the row's ammunition. For an ammo container, the type of its contents. */
    itype_id ammo_type;
    /** Inventory letter of the ammunition or its first lettered parent, or -1 for none. */
    char inherited_hotkey;
    /** Position of the row in the menu. */
    int index;
};

/**
 * Initializes the opening-key fallback to row 0. Hotkey assignment may change or disable it.
 * Enter gets no fallback and can't replace a row's hotkey.
 */
auto prepare_hotkeys(const itype_id& last_ammo, int opening_key) -> hotkey_state;

/**
 * Choose the hotkey for @p row and update @p state. Call it once per row, in menu order.
 *
 * If the opening key is still available, and the row's ammunition type matches 'last_ammo':
 *  - Without an inherited letter, the row takes the opening key as its hotkey.
 *  - With an inherited letter, the row becomes the fallback for the opening key.
 *
 * Any row whose hotkey equals the opening key disables the fallback.
 * The menu then handles that key directly.
 * @return The row's hotkey: the inherited letter, the opening key, or -1
 */
auto assign_hotkey(hotkey_state& state, const hotkey_row& row) -> char;
} // namespace reload_ui
