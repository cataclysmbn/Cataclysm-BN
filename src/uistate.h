#pragma once

#include <list>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <vector>

#include "calendar.h"
#include "enums.h"
#include "flat_set.h"
#include "om_direction.h"
#include "type_id.h"

class item;
class JsonObject;
class JsonOut;
struct oter_t;
class overmap_connection;
class overmap_special;

struct advanced_inv_pane_save_state {
    public:
        int sort_idx = 1;
        std::string filter;
        int area_idx = 11;
        int selected_idx = 0;

        bool in_vehicle = false;

        void serialize( JsonOut &json, const std::string &prefix ) const;
        void deserialize( const JsonObject &jo, const std::string &prefix );
};

struct advanced_inv_save_state {
    public:
        int exit_code = 0;
        int re_enter_move_all = 0;
        int aim_all_location = 1;

        bool active_left = true;
        int last_popup_dest = 0;

        int saved_area = 11;
        int saved_area_right = 0;
        advanced_inv_pane_save_state pane;
        advanced_inv_pane_save_state pane_right;

        void serialize( JsonOut &json, const std::string &prefix ) const;
        void deserialize( const JsonObject &jo, const std::string &prefix );
};

struct wisheffect_state {
    public:
        int last_type_selected_index = 0;
        bodypart_str_id bodypart = bodypart_str_id::NULL_ID();
        time_duration duration = 0_seconds;
        int intensity = -1;
        bool force = false;

        void serialize( JsonOut &json ) const;
        void deserialize( const JsonObject &jo );
};

struct debug_menu_state {
    public:
        wisheffect_state effect;

        void serialize( JsonOut &json ) const;
        void deserialize( const JsonObject &jo );
};
/*
  centralized depot for trivial ui data such as sorting, string_input_popup history, etc.
  To use this, see the ****notes**** below
*/
// There is only one game instance, so losing a few bytes of memory
// due to padding is not much of a concern.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class uistatedata
{
        /**** this will set a default value on startup, however to save, see below ****/
    private:
        enum side { left = 0, right = 1, NUM_PANES = 2 };
    public:
        int ags_pay_gas_selected_pump = 0;

        int wishitem_selected = 0;
        int wishmutate_selected = 0;
        int wishmonster_selected = 0;
        int iexamine_atm_selected = 0;

        debug_menu_state debug_menu;

        int adv_inv_container_location = -1;
        int adv_inv_container_index = 0;
        itype_id adv_inv_container_type = itype_id::NULL_ID();
        itype_id adv_inv_container_content_type = itype_id::NULL_ID();
        bool adv_inv_container_in_vehicle = false;

        advanced_inv_save_state transfer_save;

        bool editmap_nsa_viewmode = false;      // true: ignore LOS and lighting
        bool overmap_blinking = true;           // toggles active blinking of overlays.
        bool overmap_show_overlays = false;     // whether overlays are shown or not.
        bool overmap_show_map_notes = true;
        bool overmap_show_land_use_codes = false; // toggle land use code sym/color for terrain
        bool overmap_show_city_labels = true;
        bool overmap_show_hordes = true;
        bool overmap_show_forest_trails = true;
        bool overmap_default_0 = false;
        bool overmap_visible_weather = false;
        bool overmap_debug_weather = false;
        // draw monster groups on the overmap.
        bool overmap_debug_mongroup = false;
        std::set<tripoint_abs_omt> overmap_highlighted_omts;

        // V Menu Stuff
        int list_item_sort = 0;
        std::set<itype_id> read_items;
        std::string list_item_filter;
        std::string list_item_downvote;
        std::string list_item_priority;
        bool vmenu_show_items = true; // false implies show monsters
        bool list_item_filter_active = false;
        bool list_item_downvote_active = false;
        bool list_item_priority_active = false;
        bool list_item_init = false;

        // construction menu selections
        std::string construction_filter;
        construction_group_str_id last_construction = construction_group_str_id::NULL_ID();
        construction_category_id construction_tab = construction_category_id::NULL_ID();

        // overmap editor selections
        const oter_t *place_terrain = nullptr;
        const overmap_special *place_special = nullptr;
        om_direction::type omedit_rotation = om_direction::type::none;

        std::set<recipe_id> hidden_recipes;
        std::set<recipe_id> favorite_recipes;
        cata::flat_set<recipe_id> read_recipes;
        std::vector<recipe_id> recent_recipes;

        std::set<construction_group_str_id> favorite_construct_recipes;

        bionic_ui_sort_mode bionic_sort_mode = bionic_ui_sort_mode::POWER;

        /* to save input history and make accessible via 'up', you don't need to edit this file, just run:
           output = string_input_popup(str, int, str, str, std::string("set_a_unique_identifier_here") );
        */

        std::map<std::string, std::vector<std::string>> input_history;

        std::map<ammotype, itype_id> lastreload; // id of ammo last used when reloading ammotype

        // Wide and tall display for messages window
        bool msg_window_wide_display = false;
        bool msg_window_full_height_display = false;

        // internal stuff
        bool _testing_save = true; // internal: whine on json errors. set false if no complaints in 2 weeks.
        bool _really_testing_save = false; // internal: spammy

        std::vector<std::string> &gethistory( const std::string &id ) {
            return input_history[id];
        }

        void serialize( JsonOut &json ) const;
        void deserialize( const JsonObject &jo );
};
extern uistatedata uistate;
