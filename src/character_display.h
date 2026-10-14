#pragma once

#include "character_stat.h"
#include "color.h"
#include "type_id.h"

#include <vector>

class Character;
class item;
class avatar;
class ui_adaptor;

namespace catacurses
{
class window;
} // namespace catacurses

namespace character_display
{

/// One line of the encumbrance table: a body part, or both halves of a pair that are alike.
struct encumbrance_line {
    bodypart_str_id part;
    bool combined = false;
    /// The encumbrance of the worn items, without the layering penalty.
    int encumbrance = 0;
    int layering = 0;
    /// The warmth the player is shown.
    int warmth = 0;
    nc_color encumbrance_color = c_light_gray;
    nc_color warmth_color = c_light_gray;
    /// `selected_clothing` covers the part.
    bool covered = false;
};
/// The lines of the encumbrance table, with the parts `selected_clothing` covers marked.
auto encumbrance_lines( const Character &ch,
                        const item *selected_clothing = nullptr ) -> std::vector<encumbrance_line>;

/**
 * Formats and prints encumbrance info to specified window
 */
void print_encumbrance( ui_adaptor &ui, const catacurses::window &win, const Character &ch,
                        int line = -1,
                        const item *selected_clothing = nullptr );

/**
 * @brief Handles and displays detailed character info for the '@' screen.
 *
 * @param ch Character to display info for. Has to be non-const reference
 * to allow toggling skills and upgrading stats for stats-through-x mods.
 */
void disp_info( Character &ch );

/**
 * Handles upgrade of avatar stats.
 */
void upgrade_stat_prompt( avatar &you, const character_stat &stat );

/** Gets the minimum combined bare-handed damage from skill, bionics, and mutations for display functions */
int display_empty_handed_base_damage( const Character &you );

} // namespace character_display



