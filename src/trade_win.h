#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "cursesdef.h"
#include "npctrade.h"

class item;
class npc;
class string_input_popup;
class ui_adaptor;

class trading_window
{
    public:
        explicit trading_window( npc_trading::trade_state &state );
        ~trading_window();

        auto perform_trade( npc &np, const std::string &deal ) -> bool;

    private:
        auto setup_win( ui_adaptor &ui ) -> void;
        auto update_win( npc &np, const std::string &deal ) -> void;
        enum class info_popup_result {
            none,
            move_up,
            move_down
        };
        auto show_item_data( size_t index, bool target_is_theirs ) -> info_popup_result;
        auto build_filtered_indices( const std::vector<item_pricing> &list,
                                     const std::string &filter ) const -> std::vector<size_t>;
        auto get_var_trade( const item &it, int total_count, int amount_hint ) -> int;

        npc_trading::trade_state &state;
        catacurses::window w_head;
        catacurses::window w_them;
        catacurses::window w_you;
        catacurses::window w_info;
        size_t entries_per_page = 0;
        bool focus_them = true; // Is the focus on them?
        size_t them_off = 0; // Offset from the start of the list
        size_t you_off = 0;
        size_t them_cursor = 0;
        size_t you_cursor = 0;
        bool category_mode = false;
        size_t them_category_cursor = 0;
        size_t you_category_cursor = 0;
        std::vector<size_t> them_filtered;
        std::vector<size_t> you_filtered;
        std::string them_filter;
        std::string you_filter;
        bool filter_edit = false;
        bool filter_edit_theirs = false;
        bool show_item_info = false;
        std::unique_ptr<string_input_popup> filter_popup;
};
