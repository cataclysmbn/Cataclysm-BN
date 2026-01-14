#pragma once

#include "catalua_sol_fwd.h"

#include <string>

namespace cata
{
namespace detail
{
std::string fmt_lua_va( sol::variadic_args va );
void override_default_print( sol::state &lua );
void forbid_unsafe_functions( sol::state &lua );

void reg_avatar( sol::state &lua );
void reg_character( sol::state &lua );
void reg_colors( sol::state &lua );
void reg_constants( sol::state &lua );
void reg_coords_library( sol::state &lua );
void reg_creature_family( sol::state &lua );
void reg_creature( sol::state &lua );
void reg_damage_instance( sol::state &lua );
void reg_debug_api( sol::state &lua );
void reg_distribution_grid( sol::state &lua );
void reg_effect( sol::state &lua );
void reg_enums( sol::state &lua );
void reg_game_api( sol::state &lua );
void reg_game_ids( sol::state &lua );
void reg_hooks_examples( sol::state &lua );
void reg_inventory( sol::state &lua );
void reg_item( sol::state &lua );
void reg_locale_api( sol::state &lua );
void reg_map( sol::state &lua );
void reg_mission( sol::state &lua );
void reg_mission_type( sol::state &lua );
void reg_monster( sol::state &lua );
void mod_mutation_branch( sol::state &lua );
void reg_magic( sol::state &lua );
void reg_npc( sol::state &lua );
void reg_overmap( sol::state &lua );
void reg_player( sol::state &lua );
void reg_point_tripoint( sol::state &lua );
void reg_recipe( sol::state &lua );
void reg_requirement( sol::state &lua );
void reg_skill_level_map( sol::state &lua );
void reg_spell_type( sol::state &lua );
void reg_spell_fake( sol::state &lua );
void reg_spell( sol::state &lua );
void reg_known_magic( sol::state &lua );
void reg_testing_library( sol::state &lua );
void reg_technique( sol::state &lua );
void reg_time_types( sol::state &lua );
void reg_types( sol::state &lua );
void reg_ui_elements( sol::state &lua );
void reg_units( sol::state &lua );

} // namespace detail

void reg_all_bindings( sol::state &lua );

} // namespace cata


