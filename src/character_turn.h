#pragma once

class Character;
struct w_point;

namespace character_funcs
{

/** Maintains body wetness and handles the rate at which the player dries */
void update_body_wetness( Character &who, const w_point &weather );

/** Do pause action ('.' key). */
void do_pause( Character &who );

/** Search surrounding squares for traps (and maybe other things in the future). */
void search_surroundings( Character &who );

/** Uses calc_focus_change to update the player's current focus */
void update_mental_focus( Character &who );

} // namespace character_funcs


