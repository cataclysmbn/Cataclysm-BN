#pragma once

#include <string>
#include <vector>

class item;

namespace proc
{

auto food_tags( const item &it ) -> std::vector<std::string>;

} // namespace proc
