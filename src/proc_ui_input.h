#pragma once

#include <string>

namespace proc
{

enum class builder_focus : int {
    slots,
    candidates,
    search,
};

struct builder_input_options {
    builder_focus focus = builder_focus::slots;
    builder_focus return_focus = builder_focus::candidates;
    std::string action;
    int ch = 0;
    std::string text;
    std::string search_query;
};

struct builder_input_result {
    builder_focus focus = builder_focus::slots;
    builder_focus return_focus = builder_focus::candidates;
    std::string search_query;
    bool handled = false;
};

auto handle_builder_search_input( const builder_input_options &opts ) -> builder_input_result;

} // namespace proc
