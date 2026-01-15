#pragma once

#include <optional>
#include <string>
#include <vector>

#include "catalua_sol.h"

namespace cata::lua_action_menu
{
struct entry {
    std::string id;
    std::string name;
    std::string category_id;
    int hotkey = -1;
    sol::protected_function callback;
};

struct entry_options {
    std::string id;
    std::string name;
    std::string category_id = "misc";
    std::optional<std::string> hotkey;
    sol::protected_function callback;
};

auto register_entry( const entry_options &opts ) -> void;
auto clear_entries() -> void;
auto get_entries() -> const std::vector<entry> &; // *NOPAD*
auto run_entry( const std::string &id ) -> bool;
} // namespace cata::lua_action_menu
