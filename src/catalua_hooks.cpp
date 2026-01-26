#include "catalua_hooks.h"
#include "catalua_impl.h"

namespace cata
{

constexpr auto hook_names = std::array
{
    "on_game_load",
    "on_game_save",
    "on_game_started",
    "on_weather_changed",
    "on_weather_updated",
    "on_character_reset_stats",
    "on_character_effect_added",
    "on_character_effect",
    "on_character_effect_removed",
    "on_mon_effect_added",
    "on_mon_effect",
    "on_mon_effect_removed",
    "on_mon_death",
    "on_character_death",
    "on_shoot",
    "on_throw",
    "on_creature_dodged",
    "on_creature_blocked",
    "on_creature_performed_technique",
    "on_creature_melee_attacked",
    "on_character_try_move",
    "on_mapgen_postprocess",
    "on_explosion_start",
    "on_creature_spawn",
    "on_monster_spawn",
    "on_npc_spawn",
    "on_creature_loaded",
    "on_monster_loaded",
    "on_npc_loaded",
};

void define_hooks( lua_state &state )
{
    sol::state &lua = state.lua;
    sol::table hooks = lua.create_table();

    // Main game data table
    sol::table gt = lua.globals()["game"];
    gt["hooks"] = hooks;

    for( const auto &hook : hook_names ) {
        hooks[hook] = lua.create_table();
    }
}

} // namespace cata
