#include "dialogue_win.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "input.h"
#include "output.h"
#include "point.h"
#include "translations.h"
#include "ui_manager.h"

void dialogue_window::resize_dialogue( ui_adaptor &ui )
{
    int win_beginy = TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 4 : 0;
    int win_beginx = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 4 : 0;
    int maxy = win_beginy ? TERMY - 2 * win_beginy : FULL_SCREEN_HEIGHT;
    int maxx = win_beginx ? TERMX - 2 * win_beginx : FULL_SCREEN_WIDTH;
    d_win = catacurses::newwin( maxy, maxx, point( win_beginx, win_beginy ) );
    ui.position_from_window( d_win );
    curr_page = 0;
    draw_cache.clear();
    for( size_t idx = 0; idx < history.size(); idx++ ) {
        cache_msg( history[idx], idx );
    }
}

namespace
{
constexpr auto header_height = 3;
auto dialogue_divider_x( const catacurses::window &w ) -> int { const auto winx = getmaxx( w ); const auto inner_w = winx - 2; return 1 + inner_w * 3 / 5; }
}

void dialogue_window::print_header( const std::string &name )
{
    draw_border( d_win );
    const auto winy = getmaxy( d_win );
    const auto winx = getmaxx( d_win );
    const auto divider_x = dialogue_divider_x( d_win );

    // Header separator (full width, inside border)
    mvwhline( d_win, point( 1, header_height ), LINE_OXOX, winx - 2 );

    // Left/right divider starts below header
    mvwvline( d_win, point( divider_x, header_height + 1 ), LINE_XOXO, winy - header_height - 2 );

    // Restore border tees for the divider
    mvwputch( d_win, point( divider_x, header_height ), BORDER_COLOR, LINE_OXXX );
    mvwputch( d_win, point( divider_x, winy - 1 ), BORDER_COLOR, LINE_XXOX );

    // Header text in top-left of header panel
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    mvwprintz( d_win, point( 1, 1 ), c_white, _( "Dialogue:" ) );
    mvwprintz( d_win, point( 11, 1 ), c_light_green, name );

    // Right panel label just below header
    mvwprintz( d_win, point( divider_x + 2, header_height + 1 ), c_white, _( "Your response:" ) );
    npc_name = name;
}

void dialogue_window::clear_window_texts()
{
    werase( d_win );
    print_header( npc_name );
}

void dialogue_window::add_to_history( const std::string &msg )
{
    size_t idx = history.size();
    history.push_back( msg );
    cache_msg( msg, idx );
}

void dialogue_window::print_history()
{
    if( history.empty() ) {
        return;
    }
    int curline = getmaxy( d_win ) - 2;
    int curindex = draw_cache.size() - 1;
    // Highligh last two messages: indicates the most recent exchange betwen player and NPC
    size_t first_msg_to_highlight = history.size() - 2;
    // Print at line 2 and below, line 1 contains the header, line 0 the border
    while( curindex >= 0 && curline >= header_height + 1 ) {
        const std::pair<std::string, size_t> &msg = draw_cache[curindex];
        const nc_color col = ( msg.second >= first_msg_to_highlight ) ? c_white : c_light_gray;
        auto cur_col = col;
        print_colored_text( d_win, point( 1, curline ), cur_col, col, draw_cache[curindex].first );
        curline--;
        curindex--;
    }
}

struct page_entry {
    nc_color col;
    std::vector<std::string> lines;
    size_t response_index = 0;
    char letter = '\0';
};

struct page {
    std::vector<page_entry> entries;
};

static std::vector<page> split_to_pages( const std::vector<talk_data> &responses, int page_w,
        int page_h )
{
    page this_page;
    std::vector<page> ret;
    int fold_width = page_w - 3;
    int this_h = 0;
    size_t response_index = 0;
    for( const talk_data &resp : responses ) {
        // Assemble single entry for printing
        const std::vector<std::string> folded = foldstring( resp.text, fold_width );
        page_entry this_entry;
        this_entry.col = resp.col;
        this_entry.response_index = response_index;
        response_index++;
        if( !folded.empty() ) {
            this_entry.lines.push_back( string_format( "%s", folded[0] ) );
            this_entry.letter = resp.letter;
            for( size_t i = 1; i < folded.size(); i++ ) {
                this_entry.lines.push_back( string_format( "   %s", folded[i] ) );
            }
        }

        // Add entry to current / new page
        if( this_h + static_cast<int>( folded.size() ) > page_h ) {
            ret.push_back( this_page );
            this_page = page();
            this_h = 0;
        }
        this_h += this_entry.lines.size();
        this_page.entries.push_back( this_entry );
    }
    if( !this_page.entries.empty() ) {
        ret.push_back( this_page );
    }
    return ret;
}

static void print_responses( const catacurses::window &w, const page &responses,
                             size_t selected_response )
{
    // Responses go on the right side of the window, add 2 for border + space
    const auto divider_x = dialogue_divider_x( w );
    const auto x_start = divider_x + 1;
    // First line we can print on, +1 for border, +2 for your name + newline
    const auto y_start = 2 + 1 + header_height;

    int curr_y = y_start;
    for( const page_entry &entry : responses.entries ) {
        const auto selected = entry.response_index == selected_response;
        const auto col = selected ? hilite( entry.col ) : entry.col;
        const auto letter_col = selected ? hilite( entry.col ) : c_light_green;
        bool first_line = true;
        for( const std::string &line : entry.lines ) {
            // add letter and space to only first line
            if( first_line && entry.letter != '\0' ) {
                mvwprintz( w, point( x_start, curr_y ), letter_col, string_format( " %c ", entry.letter ) );
                mvwprintz( w, point( x_start + 3, curr_y ), col, line );
                first_line = false;
            } else {
                mvwprintz( w, point( x_start, curr_y ), col, line );
            }
            curr_y += 1;
        }
    }
}

static void print_keybindings( const catacurses::window &w )
{
    const int winx = getmaxx( w );

    const std::string col0 = _( "L: Look at" );
    const std::string col1 = _( "S: Size up stats" );
    const std::string col2 = _( "Y: Yell" );
    const std::string col3 = _( "O: Check opinion" );

    const int col0_width = std::max( static_cast<int>( col0.size() ),
                                     static_cast<int>( col2.size() ) );
    const int col1_width = std::max( static_cast<int>( col1.size() ),
                                     static_cast<int>( col3.size() ) );

    const int grid_width = col0_width + 2 + col1_width;
    const int x = std::max( 1, winx - 1 - grid_width );
    const int y = 1;

    mvwprintz( w, point( x, y ), c_magenta, col0 );
    mvwprintz( w, point( x + col0_width + 2, y ), c_magenta, col1 );
    mvwprintz( w, point( x, y + 1 ), c_magenta, col2 );
    mvwprintz( w, point( x + col0_width + 2, y + 1 ), c_magenta, col3 );
}

void dialogue_window::cache_msg( const std::string &msg, size_t idx )
{
    const auto divider_x = dialogue_divider_x( d_win );
    const auto folded = foldstring( msg, divider_x - 1 );
    draw_cache.emplace_back( "", idx );
    for( const std::string &fs : folded ) {
        draw_cache.emplace_back( fs, idx );
    }
}

void dialogue_window::refresh_response_display()
{
    curr_page = 0;
    can_scroll_down = false;
    can_scroll_up = false;
}

std::optional<size_t> dialogue_window::handle_scrolling( const int ch )
{
    if( ch == KEY_NPAGE && can_scroll_down ) {
        return next_page_start;
    }
    if( ch == KEY_PPAGE && can_scroll_up ) {
        return prev_page_start;
    }
    return std::nullopt;
}

void dialogue_window::display_responses( const std::vector<talk_data> &responses,
        size_t selected_response )
{
    const int win_maxy = getmaxy( d_win );
    clear_window_texts();
    print_history();

    // TODO: cache paged entries
    // -2 for borders, -2 for your name + newline, -4 for keybindings
    const auto page_h = getmaxy( d_win ) - 2 - 2 - 4;
    const auto divider_x = dialogue_divider_x( d_win );
    const auto page_w = getmaxx( d_win ) - divider_x - 2; // -2 for borders
    const std::vector<page> pages = split_to_pages( responses, page_w, page_h );
    if( !pages.empty() ) {
        auto selected_page = pages.size();
        size_t page_index = 0;
        for( const page &page : pages ) {
            for( const page_entry &entry : page.entries ) {
                if( entry.response_index == selected_response ) {
                    selected_page = page_index;
                    break;
                }
            }
            if( selected_page != pages.size() ) {
                break;
            }
            page_index++;
        }
        if( selected_page != pages.size() ) {
            curr_page = selected_page;
        }
    }
    if( !pages.empty() ) {
        if( curr_page >= pages.size() ) {
            curr_page = pages.size() - 1;
        }
        print_responses( d_win, pages[curr_page], selected_response );
    }
    print_keybindings( d_win );
    can_scroll_up = curr_page > 0;
    can_scroll_down = curr_page + 1 < pages.size();

    if( can_scroll_up ) {
        mvwprintz( d_win, point( getmaxx( d_win ) - 2 - 2, header_height + 2 ), c_green, "^^" );
        prev_page_start = pages[curr_page - 1].entries.front().response_index;
    }
    if( can_scroll_down ) {
        mvwprintz( d_win, point( FULL_SCREEN_WIDTH - 2 - 2, win_maxy - 2 ), c_green, "vv" );
        next_page_start = pages[curr_page + 1].entries.front().response_index;
    }
    wnoutrefresh( d_win );
}
