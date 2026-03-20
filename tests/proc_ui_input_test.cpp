#include "catch/catch.hpp"

#include "input.h"
#include "proc_ui_input.h"

TEST_CASE( "proc_builder_enters_search_mode_from_builder_panels", "[proc][ui]" )
{
    const auto from_slots = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::slots,
        .action = "",
        .ch = '/',
        .text = "",
        .search_query = "",
    } );
    const auto from_candidates = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::candidates,
        .action = "",
        .ch = '/',
        .text = "",
        .search_query = "",
    } );

    CHECK( from_slots.handled );
    CHECK( from_slots.focus == proc::builder_focus::search );
    CHECK( from_candidates.handled );
    CHECK( from_candidates.focus == proc::builder_focus::search );
}

TEST_CASE( "proc_builder_search_mode_returns_to_expected_panel", "[proc][ui]" )
{
    const auto to_slots = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "LEFT",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto to_candidates = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "RIGHT",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto confirm_to_candidates = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "CONFIRM",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );

    CHECK( to_slots.handled );
    CHECK( to_slots.focus == proc::builder_focus::slots );
    CHECK( to_slots.search_query == "bread" );
    CHECK( to_candidates.handled );
    CHECK( to_candidates.focus == proc::builder_focus::candidates );
    CHECK( confirm_to_candidates.handled );
    CHECK( confirm_to_candidates.focus == proc::builder_focus::candidates );
}

TEST_CASE( "proc_builder_search_mode_updates_and_clears_query", "[proc][ui]" )
{
    const auto typed = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "ANY_INPUT",
        .ch = 0,
        .text = " ham",
        .search_query = "bread",
    } );
    const auto backspaced = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "",
        .ch = KEY_BACKSPACE,
        .text = "",
        .search_query = "bread",
    } );
    const auto deleted_empty = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "",
        .ch = KEY_DC,
        .text = "",
        .search_query = "",
    } );

    CHECK( typed.handled );
    CHECK( typed.search_query == "bread ham" );
    CHECK( typed.focus == proc::builder_focus::search );
    CHECK( backspaced.handled );
    CHECK( backspaced.search_query == "brea" );
    CHECK( backspaced.focus == proc::builder_focus::search );
    CHECK( deleted_empty.handled );
    CHECK( deleted_empty.search_query.empty() );
}

TEST_CASE( "proc_builder_search_quit_returns_to_previous_panel", "[proc][ui]" )
{
    const auto to_slots = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .return_focus = proc::builder_focus::slots,
        .action = "QUIT",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto to_candidates = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .return_focus = proc::builder_focus::candidates,
        .action = "QUIT",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );

    CHECK( to_slots.handled );
    CHECK( to_slots.focus == proc::builder_focus::slots );
    CHECK( to_slots.return_focus == proc::builder_focus::slots );
    CHECK( to_slots.search_query == "bread" );
    CHECK( to_candidates.handled );
    CHECK( to_candidates.focus == proc::builder_focus::candidates );
    CHECK( to_candidates.return_focus == proc::builder_focus::candidates );
    CHECK( to_candidates.search_query == "bread" );
}

TEST_CASE( "proc_builder_search_mode_consumes_navigation_keys", "[proc][ui]" )
{
    const auto up = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "UP",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto down = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "DOWN",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto page_up = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "PAGE_UP",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto page_down = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "PAGE_DOWN",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto home = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "HOME",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );
    const auto end = proc::handle_builder_search_input( {
        .focus = proc::builder_focus::search,
        .action = "END",
        .ch = 0,
        .text = "",
        .search_query = "bread",
    } );

    CHECK( up.handled );
    CHECK( up.focus == proc::builder_focus::search );
    CHECK( up.search_query == "bread" );
    CHECK( down.handled );
    CHECK( down.focus == proc::builder_focus::search );
    CHECK( down.search_query == "bread" );
    CHECK( page_up.handled );
    CHECK( page_up.focus == proc::builder_focus::search );
    CHECK( page_up.search_query == "bread" );
    CHECK( page_down.handled );
    CHECK( page_down.focus == proc::builder_focus::search );
    CHECK( page_down.search_query == "bread" );
    CHECK( home.handled );
    CHECK( home.focus == proc::builder_focus::search );
    CHECK( home.search_query == "bread" );
    CHECK( end.handled );
    CHECK( end.focus == proc::builder_focus::search );
    CHECK( end.search_query == "bread" );
}
