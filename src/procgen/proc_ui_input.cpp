#include "procgen/proc_ui_input.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "input.h"
#include "procgen/proc_ui_navigation.h"

namespace
{

auto is_search_navigation_action( const std::string &action ) -> bool
{
    constexpr auto search_navigation_actions = std::array<std::string_view, 6> {
        "UP", "DOWN", "PAGE_UP", "PAGE_DOWN", "HOME", "END"
    };
    return std::ranges::any_of( search_navigation_actions, [&]( const std::string_view candidate ) {
        return action == candidate;
    } );
}

auto is_builder_up_action( const std::string &action ) -> bool
{
    return action == "UP" || action == "SCROLL_UP";
}

auto is_builder_down_action( const std::string &action ) -> bool
{
    return action == "DOWN" || action == "SCROLL_DOWN";
}

} // namespace

namespace proc
{

auto handle_builder_search_input( const builder_input_options &opts ) -> builder_input_result
{
    auto result = builder_input_result{
        .focus = opts.focus,
        .return_focus = opts.return_focus,
        .search_query = opts.search_query,
    };

    if( opts.focus == builder_focus::search ) {
        if( opts.ch == KEY_BACKSPACE || opts.ch == KEY_DC ) {
            if( !result.search_query.empty() ) {
                result.search_query.pop_back();
            }
            result.handled = true;
            return result;
        }
        if( opts.action == "LEFT" ) {
            result.focus = builder_focus::slots;
            result.return_focus = builder_focus::slots;
            result.handled = true;
            return result;
        }
        if( opts.action == "CONFIRM" || opts.action == "RIGHT" ) {
            result.focus = builder_focus::candidates;
            result.return_focus = builder_focus::candidates;
            result.handled = true;
            return result;
        }
        if( opts.action == "QUIT" ) {
            result.focus = opts.return_focus;
            result.handled = true;
            return result;
        }
        if( opts.action == "ANY_INPUT" && !opts.text.empty() ) {
            result.search_query += opts.text;
            result.handled = true;
            return result;
        }
        if( is_search_navigation_action( opts.action ) ) {
            result.handled = true;
            return result;
        }
    }

    if( opts.ch == '/' ) {
        result.focus = builder_focus::search;
        result.return_focus = opts.focus;
        result.handled = true;
    }

    return result;
}

auto handle_builder_slot_navigation( const builder_slot_navigation_options &opts ) ->
builder_slot_navigation_result
{
    auto result = builder_slot_navigation_result{
        .slot_cursor = opts.slot_cursor,
        .search_query = opts.search_query,
    };

    if( opts.focus != builder_focus::slots || opts.slot_count <= 0 ) {
        return result;
    }

    auto next_cursor = opts.slot_cursor;
    if( is_builder_up_action( opts.action ) ) {
        next_cursor = wrap_cursor( opts.slot_cursor, -1, opts.slot_count );
    } else if( is_builder_down_action( opts.action ) ) {
        next_cursor = wrap_cursor( opts.slot_cursor, 1, opts.slot_count );
    } else if( opts.action == "HOME" ) {
        next_cursor = 0;
    } else if( opts.action == "END" ) {
        next_cursor = opts.slot_count - 1;
    } else {
        return result;
    }

    result.handled = true;
    result.slot_cursor = next_cursor;
    if( next_cursor != opts.slot_cursor ) {
        result.search_query.clear();
    }
    return result;
}

auto handle_builder_candidate_navigation( const builder_candidate_navigation_options &opts ) ->
builder_candidate_navigation_result
{
    auto result = builder_candidate_navigation_result{
        .candidate_cursor = opts.candidate_cursor,
    };

    if( opts.focus != builder_focus::candidates || opts.candidate_count <= 0 ) {
        return result;
    }

    if( is_builder_up_action( opts.action ) ) {
        result.handled = true;
        result.candidate_cursor = wrap_cursor( opts.candidate_cursor, -1, opts.candidate_count );
        return result;
    }
    if( is_builder_down_action( opts.action ) ) {
        result.handled = true;
        result.candidate_cursor = wrap_cursor( opts.candidate_cursor, 1, opts.candidate_count );
        return result;
    }
    if( opts.action == "PAGE_UP" ) {
        result.handled = true;
        result.candidate_cursor = std::max( opts.candidate_cursor - opts.page_size, 0 );
        return result;
    }
    if( opts.action == "PAGE_DOWN" ) {
        result.handled = true;
        result.candidate_cursor = std::min( opts.candidate_cursor + opts.page_size,
                                            opts.candidate_count - 1 );
        return result;
    }
    if( opts.action == "HOME" ) {
        result.handled = true;
        result.candidate_cursor = 0;
        return result;
    }
    if( opts.action == "END" ) {
        result.handled = true;
        result.candidate_cursor = opts.candidate_count - 1;
        return result;
    }

    return result;
}

} // namespace proc
