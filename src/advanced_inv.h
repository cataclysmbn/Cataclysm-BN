#pragma once

#include "advanced_inv_area.h"
#include "advanced_inv_listitem.h"
#include "advanced_inv_pane.h"
#include "cursesdef.h"

#include <array>
#include <cctype>
#include <functional>
#include <map>
#include <string>
#include <vector>

class input_context;
class item;
struct advanced_inv_save_state;

namespace game_client
{
struct interaction_row_count;
} // namespace game_client

namespace engine_client
{
struct component;
struct invoked_affordance;
struct panel_stat;
struct row;
struct screen_view;
} // namespace engine_client

void create_advanced_inv();

/**
 * Cancels ongoing move all action.
 * TODO: Make this not needed.
 */
void cancel_aim_processing();

class advanced_inventory
{
    public:
        advanced_inventory();
        ~advanced_inventory();

        void display();

        /**
         * Converts from screen relative location to game-space relative location
         * for control rotation in isometric mode.
         */
        aim_location screen_relative_location( aim_location area );
        std::string get_location_key( aim_location area );

        advanced_inv_area &get_one_square( const aim_location &loc ) { return squares[loc]; }

    private:
        /**
         * Refers to the two panes, used as index into @ref panes.
         */
        enum side { left = 0, right = 1, NUM_PANES = 2 };
        static constexpr int head_height = 5;

        // swap the panes and windows via std::swap()
        void swap_panes();

        // minimap that displays things around character
        catacurses::window minimap;
        catacurses::window mm_border;
        const int minimap_width = 3;
        const int minimap_height = 3;
        void draw_minimap();
        void refresh_minimap();
        char get_minimap_sym( side p ) const;

        bool inCategoryMode = false;

        int itemsPerPage = 0;
        int w_height = 0;
        int w_width = 0;

        int headstart = 0;
        int colstart = 0;

        bool recalc = false;
        /**
         * Which panels is active (item moved from there).
         */
        side src;
        /**
         * Which panel is the destination (items want to go to there).
         */
        side dest;
        /**
         * True if (and only if) the filter of the active panel is currently
         * being edited.
         */
        bool filter_edit = false;
        /**
         * Two panels (left and right) showing the items, use a value of @ref side
         * as index.
         */
        std::array<advanced_inventory_pane, NUM_PANES> panes;
        static const advanced_inventory_pane null_pane;
        /// Mutable because reading a container area refreshes the remembered container index.
        mutable std::array<advanced_inv_area, NUM_AIM_LOCATIONS> squares;

        catacurses::window head;

        bool exit = false;

        advanced_inv_save_state *save_state;

        /**
         * registers all the ctxt for display()
         */
        input_context register_ctxt() const;
        /// What the screen publishes to a protocol client: a const read of the panes, never of a window.
        auto aim_view( const input_context &ctxt ) const -> engine_client::screen_view;
        auto pane_component( side which ) const -> engine_client::component;
        /// The areas a pane can show, with the one it shows selected and, on it, whether it lists the vehicle cargo.
        auto area_rows( side which, bool cargo ) const -> std::vector<engine_client::row>;
        auto pane_stats( const advanced_inventory_pane &pane ) const
        -> std::vector<engine_client::panel_stat>;
        /// The id of each entry of `panes[which].items`, from where the item is and what it is; empty for
        /// an entry that is no item. Never from the translated name.
        auto row_ids_of( side which ) const -> std::vector<std::string>;
        /// Runs an affordance a protocol client invoked; true when the screen is left.
        auto run_affordance( const engine_client::invoked_affordance &call ) -> bool;

        /// A row of a move and how much of it.
        struct moved_row {
            advanced_inv_listitem *item;
            int amount;
        };
        /// A move of rows to one destination.
        struct carry_options {
            std::vector<moved_row> rows;
            aim_location destarea;
            bool to_vehicle;
        };
        /// What is already on its way to the destination when the next row is checked against it.
        struct pending_load {
            units::volume volume = 0_ml;
            units::mass weight = 0_gram;
            int items = 0;
            std::map<itype_id, int> worn;
        };
        struct limit_options {
            aim_location destarea;
            const advanced_inv_listitem &sitem;
            int requested;
            bool to_vehicle;
            pending_load pending = {};
        };
        /// How much of a row the destination takes, or why it takes none.
        struct limited_amount {
            int amount = 0;
            std::string denial;
        };
        auto limit_amount( const limit_options &opts ) const -> limited_amount;
        /// Moves rows of the pane opposite `to` to the pane `to` as one move: all of them or none. True
        /// when the screen is left for the activity that carries them out.
        auto move_rows( const std::vector<game_client::interaction_row_count> &requested,
                        side to ) -> bool;
        /// Starts the activity that carries rows out of the inventory, the worn items or the map; true when
        /// the screen is left for it. Nothing is changed when it refuses to
        /// take several worn items off at once. A worn item that cannot be taken off leaves the screen with
        /// nothing carried, as the native move does.
        auto carry_out( const carry_options &opts ) -> bool;
        auto toggle_vehicle( advanced_inventory_pane &spane, advanced_inventory_pane &dpane ) -> void;
        /// Starts the pickup or move activity that carries rows out of the map or a vehicle.
        auto start_activity( const carry_options &opts ) const -> void;

        /**
         * returns whether the display loop exits or not
         */
        bool action_move_item(
            advanced_inv_listitem *sitem, advanced_inventory_pane &dpane,
            const advanced_inventory_pane &spane, const std::string &action );

        void action_examine( advanced_inv_listitem *sitem, advanced_inventory_pane &spane );

        // store/load settings (such as index, filter, etc)
        void save_settings( bool only_panes );
        void load_settings();
        // used to return back to AIM when other activities queued are finished
        void do_return_entry();
        // returns true if currently processing a routine
        // (such as `MOVE_ALL_ITEMS' with `AIM_ALL' source)
        bool is_processing() const;

        static std::string get_sortname( advanced_inv_sortby sortby );
        bool move_all_items( bool nested_call = false );
        void print_items( const advanced_inventory_pane &pane, bool active );
        /// The most volume the area of `pane` can hold, as the native header prints after the slash.
        auto max_volume_of( const advanced_inventory_pane &pane ) const -> units::volume;
        void recalc_pane( side p );
        void redraw_pane( side p );
        void redraw_sidebar();
        // Returns the x coordinate where the header started. The header is
        // displayed right of it, everything left of it is till free.
        int print_header( advanced_inventory_pane &pane, aim_location sel );
        void init();
        /**
         * Translate an action ident from the input context to an aim_location.
         * @param action Action ident to translate
         * @param ret If the action ident referred to a location, its id is stored
         * here. Only valid when the function returns true.
         * @return true if the action did refer to an location (which has been
         * stored in ret), false otherwise.
         */
        bool get_square( const std::string &action, aim_location &ret );
        void change_square(
            aim_location changeSquare, advanced_inventory_pane &dpane, advanced_inventory_pane &spane );
        /**
         * Show the sort-by menu and change the sorting of this pane accordingly.
         * @return whether the sort order was actually changed.
         */
        bool show_sort_menu( advanced_inventory_pane &pane );
        /**
         * Checks whether one can put items into the supplied location.
         * If the supplied location is AIM_ALL, query for the actual location
         * (stores the result in def) and check that destination.
         * @return false if one can not put items in the destination, true otherwise.
         * The result true also indicates the def is not AIM_ALL (because the
         * actual location has been queried).
         */
        bool query_destination( aim_location &def );
        /**
         * Move content of source container into destination container (destination pane =
         * AIM_CONTAINER)
         * @param src_container Source container
         * @param dest_container Destination container
         */
        bool move_content( item &src_container, item &dest_container );
        /**
         * Setup how many items/charges (if counted by charges) should be moved.
         * @param destarea Where to move to. This must not be AIM_ALL.
         * @param sitem The source item, it must contain a valid reference to an item!
         * @param action The action we are querying
         * @param amount The input value is ignored, contains the amount that should
         *      be moved. Only valid if this returns true.
         * @return false if nothing should/can be moved. True only if there can and
         *      should be moved. A return value of true indicates that amount now contains
         *      a valid item count to be moved.
         */
        bool query_charges(
            aim_location destarea, const advanced_inv_listitem &sitem, const std::string &action,
            int &amount );
};
