#pragma once

#include "type_id.h"
#include "character.h"
#include <memory>
#include <filesystem>

class Item_factory;
class map;
class time_point;
struct tripoint;
class world;

namespace cata
{
struct lua_state;
struct lua_state_deleter {
    void operator()( lua_state *state ) const;
};

bool has_lua();
int get_lua_api_version();
std::string get_lapi_version_string();
void startup_lua_test();
auto generate_lua_docs( const std::filesystem::path &script_path,
                        const std::filesystem::path &to ) -> bool;
void show_lua_console();
void reload_lua_code();
void debug_write_lua_backtrace( std::ostream &out );

bool save_world_lua_state( const world *world, const std::string &path );
bool load_world_lua_state( const world *world, const std::string &path );

std::unique_ptr<lua_state, lua_state_deleter> make_wrapped_state();

void init_global_state_tables( lua_state &state, const std::vector<mod_id> &modlist );
void set_mod_being_loaded( lua_state &state, const mod_id &mod );
void clear_mod_being_loaded( lua_state &state );
void run_mod_preload_script( lua_state &state, const mod_id &mod );
void run_mod_finalize_script( lua_state &state, const mod_id &mod );
void run_mod_main_script( lua_state &state, const mod_id &mod );
void run_on_game_load_hooks( lua_state &state );
void run_on_game_save_hooks( lua_state &state );
void run_on_every_x_hooks( lua_state &state );
void run_on_mapgen_postprocess_hooks( lua_state &state, map &m, const tripoint &p,
                                      const time_point &when );
void reg_lua_icallback_actors( lua_state &state, Item_factory &ifactory );
void resolve_lua_bionic_and_mutation_callbacks();

} // namespace cata


