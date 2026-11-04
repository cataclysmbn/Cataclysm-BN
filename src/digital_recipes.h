#pragma once

#include "type_id.h"

#include <expected>
#include <string>
#include <vector>

class item;

struct digital_recipe_reference {
    recipe_id recipe;
    int skill_level = 0;
};

namespace data_vars
{
class data_set;
}

/// Validate recipe definitions stored directly in item variables.
auto validate_digital_recipe_data( const data_vars::data_set &vars ) ->
std::expected<void, std::string>;
/// Resolve the file's fixed/random recipe definitions once and persist the selection.
auto initialize_digital_recipe_file( item &file ) -> void;
auto digital_file_recipes( const item &file ) -> std::vector<digital_recipe_reference>;
