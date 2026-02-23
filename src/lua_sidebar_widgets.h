#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalua_sol.h"

namespace cata::lua_sidebar_widgets
{
struct widget_entry {
    std::string id;
    std::string name;
    int height = 1;
    std::optional<int> order;
    bool default_toggle = true;
    bool redraw_every_frame = false;
    std::optional<bool> panel_visible_value;
    std::optional<sol::protected_function> panel_visible_fn;
    sol::protected_function draw;
    std::optional<sol::protected_function> render;
};

struct widget_options {
    std::string id;
    std::string name;
    int height = 1;
    std::optional<int> order;
    bool default_toggle = true;
    bool redraw_every_frame = false;
    std::optional<bool> panel_visible_value;
    std::optional<sol::protected_function> panel_visible_fn;
    sol::protected_function draw;
    std::optional<sol::protected_function> render;
};

auto register_widget( const widget_options &opts ) -> void;
auto clear_widgets() -> void;
auto get_widgets() -> const std::vector<widget_entry> &; // *NOPAD*
auto find_widget( std::string_view id ) -> const widget_entry *; // *NOPAD*
} // namespace cata::lua_sidebar_widgets
