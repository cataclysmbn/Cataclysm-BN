#include "hanging_rope.h"

#include "map.h"
#include "mapdata.h"
#include "vehicle/vehicle.h"

#include <algorithm>
#include <ranges>

auto is_hanging_rope_visible(const map& here, const tripoint_bub_ms& p) -> bool {
    if (!here.has_rope_at(p)) { return false; }
    const auto veh_z = here.get_rope_at(p).first->bub_ms_location().z();
    return veh_z > p.z()
        && std::ranges::all_of(std::views::iota(p.z() + 1, veh_z + 1), [&](const int z) {
               return here.ter(tripoint_bub_ms(p.xy(), z)) == t_open_air;
           });
}
