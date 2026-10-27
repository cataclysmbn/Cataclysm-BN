#pragma once

#include "type_id.h"

#include <string>
#include <vector>

class JsonObject;
class item;

struct digital_recipe_reference {
    recipe_id recipe;
    int skill_level = 0;
};

auto load_digital_recipe_group( const JsonObject &jo, const std::string &src ) -> void;
auto reset_digital_recipe_groups() -> void;
auto check_digital_recipe_groups() -> void;
auto digital_recipe_group_exists( const std::string &id ) -> bool;
/// Resolve a file's JSON recipe group once and persist the selected recipes.
auto initialize_digital_recipe_file( item &file ) -> void;
auto digital_file_recipes( const item &file ) -> std::vector<digital_recipe_reference>;
