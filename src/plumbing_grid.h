#pragma once

#include <bitset>
#include <map>
#include <set>
#include <vector>

#include "coordinates.h"
#include "cube_direction.h"
#include "type_id.h"
#include "units.h"

class overmap;

namespace plumbing_grid
{

using connection_bitset = std::bitset<six_cardinal_directions.size()>;
using connection_map = std::map<tripoint_om_omt, connection_bitset>;

struct water_storage_stats {
    units::volume stored = 0_ml;
    units::volume capacity = 0_ml;
};

struct water_storage_state {
    units::volume stored_clean = 0_ml;
    units::volume stored_dirty = 0_ml;
    units::volume capacity = 0_ml;

    auto stored_total() const -> units::volume
    {
        return stored_clean + stored_dirty;
    }
};

auto connections_for( overmap &om ) -> connection_map &;
auto connections_for( const overmap &om ) -> const connection_map &;
auto storage_for( overmap &om ) -> std::map<tripoint_om_omt, water_storage_state> &;
auto storage_for( const overmap &om ) -> const std::map<tripoint_om_omt, water_storage_state> &;

auto grid_at( const tripoint_abs_omt &p ) -> std::set<tripoint_abs_omt>;
auto grid_connectivity_at( const tripoint_abs_omt &p ) -> std::vector<tripoint_rel_omt>;
auto water_storage_at( const tripoint_abs_omt &p ) -> water_storage_stats;
auto liquid_charges_at( const tripoint_abs_omt &p, const itype_id &liquid_type ) -> int;
auto add_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                         int charges ) -> int;
auto drain_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                           int charges ) -> int;
auto on_contents_changed( const tripoint_abs_ms &p ) -> void;
auto on_structure_changed( const tripoint_abs_ms &p ) -> void;
auto disconnect_tank( const tripoint_abs_ms &p ) -> void;
auto add_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool;
auto remove_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool;
auto clear() -> void;

} // namespace plumbing_grid
