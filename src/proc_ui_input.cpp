#include "proc_ui_input.h"

#include "input.h"

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
    }

    if( opts.ch == '/' ) {
        result.focus = builder_focus::search;
        result.return_focus = opts.focus;
        result.handled = true;
    }

    return result;
}

} // namespace proc
