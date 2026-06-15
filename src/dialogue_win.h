#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <utility>
#include <optional>

#include "color.h"
#include "cursesdef.h"

/** Represents avatar response */
struct talk_data {
    char letter;
    nc_color col;
    std::string text;
};

class ui_adaptor;

class dialogue_window
{
    public:
        dialogue_window() = default;
        void resize_dialogue( ui_adaptor &ui );
        void print_header( const std::string &name );

        void clear_window_texts();
        std::optional<size_t> handle_scrolling( int ch );
        void display_responses( const std::vector<talk_data> &responses, size_t selected_response );
        void refresh_response_display();
        /** Adds message to history. It must be already translated.
         *  Pass continuation=true for unattributed follow-on lines: suppresses the
         *  blank separator line that normally appears between history entries. */
        void add_to_history( const std::string &msg, bool continuation = false );

        /** Mark the start of a new dialogue turn.  Everything added to history
         *  after this call is considered "current" and rendered in white;
         *  everything before fades to gray.  Call this at the moment a player
         *  choice is selected, before adding any speech or executing the body. */
        void mark_turn_start();

    private:
        catacurses::window d_win;
        /**
         * This contains the exchanged words, it is basically like the global message log.
         * Each responses of the player character and the NPC are added as are information about
         * what each of them does (e.g. the npc drops their weapon).
         */
        std::vector<std::string> history;
        // Parallel to history: false means omit the blank separator before this entry.
        std::vector<bool> history_separator_;
        // Index into history from which entries are considered "current turn" (rendered white).
        // 0 on construction so the initial NPC greeting is always white.
        size_t turn_start_ = 0;
        /**
         * Drawing cache: basically all entries from @ref history, but folded to current
         * window width and with separators between. Used for rendering, recalculated each time window size changes.
         */
        std::vector<std::pair<std::string, size_t>> draw_cache;
        /** Scroll position in response window (page number) */
        size_t curr_page = 0;
        bool can_scroll_up = false;
        bool can_scroll_down = false;
        /* Page start indices for selecting first entry of page when paging up/down */
        size_t next_page_start = 0;
        size_t prev_page_start = 0;

        // Prints history. Automatically highlighting last message.
        void print_history();

        /**
         * Folds given message to current window width and adds it to drawing cache.
         * Also adds a separator (empty line).
         * idx is this message's position within @ref history_raw
         */
        void cache_msg( const std::string &msg, size_t idx, bool with_separator );

        std::string npc_name;
};

