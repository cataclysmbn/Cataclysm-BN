#pragma once

#include <cstddef>

enum class temperature_flag : int;

class map;
class item;
class vehicle;

namespace rot::temp
{

struct tile_flags {
    bool root_cellar = false;
    bool fridge = false;
    bool freezer = false;
};

/** Resolve the rot temperature modifier for map terrain/furniture storage. */
auto for_tile( const tile_flags &flags ) -> temperature_flag;

// TODO: Move to item_location method?
auto for_location( const map &m, const item &loc ) -> temperature_flag;
auto for_part( const vehicle &veh, size_t part,
               bool engine_heater_is_on = false ) -> temperature_flag;

} // namespace rot::temp

