#include "catalua_hooks.h"
#include "catalua_impl.h"

namespace cata
{

constexpr auto hook_names = std::array
{
    "on_game_load",
    "on_game_save",
    "on_game_started",
    "on_character_reset_stats",
    "on_mon_death",
    "on_char_death",
    "on_creature_dodged",
    "on_creature_blocked",
    "on_creature_performed_technique",
    "on_creature_melee_attacked",
    "on_mapgen_postprocess",
    "on_character_death"
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
