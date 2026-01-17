#include "catalua_hooks.h"
#include "catalua_impl.h"
#include "catalua_sol.h"

#include <set>
#include <string>

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
};

// Set of valid hook names for validation
static const std::set<std::string> valid_hook_names( hook_names.begin(), hook_names.end() );

void define_hooks( lua_state &state )
{
    sol::state &lua = state.lua;
    sol::table hooks = lua.create_table();

    // Main game data table
    sol::table gt = lua.globals()["game"];
    gt["hooks"] = hooks;

    // Create empty tables for each hook
    for( const auto &hook : hook_names ) {
        hooks[hook] = lua.create_table();
    }

    // Register the subscribe function
    // Usage: game.hooks.subscribe("hook_name", callback, {priority = 0})
    hooks["subscribe"] = [&lua]( sol::this_state L,
                                 std::string hook_name,
                                 sol::protected_function callback,
    sol::optional<sol::table> options ) {
        sol::state_view lua_view( L );

        // Validate hook name
        if( valid_hook_names.find( hook_name ) == valid_hook_names.end() ) {
            luaL_error( L.lua_state(), "Unknown hook name: %s", hook_name.c_str() );
            return;
        }

        // Capture current_mod at subscription time
        std::string mod_id;
        sol::object current_mod = lua_view.globals()["game"]["current_mod"];

        if( current_mod.is<std::string>() ) {
            mod_id = current_mod.as<std::string>();
        }

        // Get priority from options (default 0)
        int priority = 0;
        if( options ) {
            priority = options->get_or( "priority", 0 );
        }

        // Get the hooks table for this hook name
        sol::table hooks_table = lua_view.globals()["game"]["hooks"];
        sol::table hook_table = hooks_table[hook_name];

        // Create entry with metadata
        sol::table entry = lua_view.create_table();
        entry["func"] = callback;
        entry["mod_id"] = mod_id;
        entry["priority"] = priority;

        // Append to hook table using table.insert equivalent
        // Find the next available index
        size_t next_idx = 1;
        while( hook_table[next_idx].valid() && hook_table[next_idx].get_type() != sol::type::nil ) {
            ++next_idx;
        }
        hook_table[next_idx] = entry;
    };

    // Register the unsubscribe function
    // Usage: game.hooks.unsubscribe("hook_name", callback)
    // Returns true if the callback was found and removed, false otherwise
    hooks["unsubscribe"] = [&lua]( sol::this_state L,
                                   std::string hook_name,
    sol::protected_function callback ) -> bool {
        sol::state_view lua_view( L );

        // Validate hook name
        if( valid_hook_names.find( hook_name ) == valid_hook_names.end() )
        {
            return false;
        }

        // Get the hooks table for this hook name
        sol::table hooks_table = lua_view.globals()["game"]["hooks"];
        sol::optional<sol::table> maybe_hook_table = hooks_table[hook_name].get<sol::optional<sol::table>>();
        if( !maybe_hook_table ) {
            return false;
        }

        sol::table hook_table = *maybe_hook_table;

        // Find and remove the matching callback
        for( size_t i = 1; ; ++i ) {
            sol::object entry_obj = hook_table[i];
            if( entry_obj.get_type() == sol::type::nil ) {
                break;
            }

            sol::protected_function func;
            if( entry_obj.is<sol::table>() ) {
                // New format: {func, mod_id, priority}
                sol::table entry = entry_obj.as<sol::table>();
                sol::optional<sol::protected_function> maybe_func =
                    entry["func"].get<sol::optional<sol::protected_function>>();
                if( maybe_func ) {
                    func = *maybe_func;
                }
            } else if( entry_obj.get_type() == sol::type::function ) {
                // Old format: raw function
                func = entry_obj.as<sol::protected_function>();
            }

            // Compare function references
            if( func.valid() && func == callback ) {
                // Remove this entry by shifting subsequent entries down
                for( size_t j = i; ; ++j ) {
                    sol::object next_entry = hook_table[j + 1];
                    if( next_entry.get_type() == sol::type::nil ) {
                        hook_table[j] = sol::nil;
                        break;
                    }
                    hook_table[j] = next_entry;
                }
                return true;
            }
        }
        return false;
    };
}

} // namespace cata
