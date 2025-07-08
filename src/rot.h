#pragma once

enum class temperature_flag : int;

class map;
class item;

namespace rot
{

// TODO: Move to item_location method?
auto temperature_flag_for_location( const map &m, const item &loc ) -> temperature_flag;

} // namespace rot


