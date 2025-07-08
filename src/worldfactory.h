#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <vector>
#include <string>

#include "options.h"
#include "pimpl.h"
#include "type_id.h"
#include "world.h"

enum class special_game_type;

class JsonIn;
class JsonObject;

namespace catacurses
{
class window;
} // namespace catacurses


class mod_manager;
class mod_ui;
class input_context;

class worldfactory
{
    public:
        worldfactory();
        ~worldfactory();

        // Generate a world
        WORLDINFO *make_new_world( bool show_prompt = true, const std::string &world_to_copy = "" );
        WORLDINFO *make_new_world( special_game_type special_type );
        // Used for unit tests - does NOT verify if the mods can be loaded
        WORLDINFO *make_new_world( const std::vector<mod_id> &mods );
        // Returns the *existing* world of given name.
        WORLDINFO *get_world( const std::string &name );
        // Returns index for world name, 0 if world cannot be found.
        size_t get_world_index( const std::string &name );
        bool has_world( const std::string &name ) const;

        void set_active_world( WORLDINFO *world );

        void init();

        WORLDINFO *pick_world( bool show_prompt = true, bool empty_only = false );

        std::unique_ptr<world> active_world;

        std::vector<std::string> all_worldnames() const;

        std::string last_world_name;
        std::string last_character_name;

        void save_last_world_info();

        mod_manager &get_mod_manager();

        void remove_world( const std::string &worldname );
        bool valid_worldname( const std::string &name, bool automated = false );
        std::string get_next_valid_worldname();

        /**
         * @param delete_folder If true: delete all the files and directories  of the given
         * world folder. Else just avoid deleting the config files and the directory
         * itself.
         */
        void delete_world( const std::string &worldname, bool delete_folder );

        static void draw_worldgen_tabs( const catacurses::window &w, size_t current );
        void show_active_world_mods( const std::vector<mod_id> &world_mods );
        void edit_active_world_mods( WORLDINFO *world );

        void convert_to_v2( const std::string &worldname );

    private:
        std::map<std::string, std::unique_ptr<WORLDINFO>> all_worlds;

        void load_last_world_info();

        std::string pick_random_name();
        int show_worldgen_tab_options( const catacurses::window &win, WORLDINFO *world,
                                       const std::function<bool()> &on_quit );
        int show_worldgen_tab_modselection( const catacurses::window &win, WORLDINFO *world,
                                            const std::function<bool()> &on_quit );
        int show_worldgen_tab_confirm( const catacurses::window &win, WORLDINFO *world,
                                       const std::function<bool()> &on_quit );

        int show_modselection_window( const catacurses::window &win, std::vector<mod_id> &active_mod_order,
                                      const std::function<bool()> &on_quit,
                                      const std::function<bool()> &on_backtab,
                                      bool standalone );
        void draw_modselection_borders( const catacurses::window &win, const input_context &ctxtp,
                                        bool standalone );
        static void draw_empty_worldgen_tabs( const catacurses::window &w );
        void draw_mod_list( const catacurses::window &w, int &start, size_t cursor,
                            const std::vector<mod_id> &mods, bool is_active_list, const std::string &text_if_empty,
                            const catacurses::window &w_shift );

        WORLDINFO *add_world( std::unique_ptr<WORLDINFO> retworld );

        pimpl<mod_manager> mman;
        pimpl<mod_ui> mman_ui;

        using worldgen_display = std::function<int ( const catacurses::window &, WORLDINFO *,
                                 const std::function<bool()> )>;

        std::vector<worldgen_display> tabs;
};

void load_world_option( const JsonObject &jo );

//load external option from json
void load_external_option( const JsonObject &jo );

extern std::unique_ptr<worldfactory> world_generator;


