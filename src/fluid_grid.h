#pragma once

#include <bitset>
#include <map>
#include <ranges>
#include <set>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "cube_direction.h"
#include "type_id.h"
#include "units.h"

class overmap;

namespace fluid_grid
{

using connection_bitset = std::bitset<six_cardinal_directions.size()>;
using connection_map = std::map<tripoint_om_omt, connection_bitset>;

struct liquid_storage_stats {
    units::volume stored = 0_ml;
    units::volume capacity = 0_ml;
    std::map<itype_id, units::volume> stored_by_type;

    auto stored_for( const itype_id &liquid_type ) const -> units::volume {
        const auto iter = stored_by_type.find( liquid_type );
        if( iter == stored_by_type.end() ) {
            return 0_ml;
        }
        return iter->second;
    }
};

struct liquid_storage_state {
    std::map<itype_id, units::volume> stored_by_type;
    units::volume capacity = 0_ml;

    auto stored_total() const -> units::volume {
        auto total = 0_ml;
        std::ranges::for_each( stored_by_type, [&]( const auto & entry ) {
            total += entry.second;
        } );
        return total;
    }

    auto stored_for( const itype_id &liquid_type ) const -> units::volume {
        const auto iter = stored_by_type.find( liquid_type );
        if( iter == stored_by_type.end() ) {
            return 0_ml;
        }
        return iter->second;
    }
};

auto connections_for( overmap &om ) -> connection_map &; // *NOPAD*
auto connections_for( const overmap &om ) -> const connection_map &; // *NOPAD*
auto storage_for( overmap &om ) -> std::map<tripoint_om_omt, liquid_storage_state> &; // *NOPAD*
auto storage_for( const overmap &om ) -> const std::map<tripoint_om_omt, liquid_storage_state>
&; // *NOPAD*

auto grid_at( const tripoint_abs_omt &p ) -> std::set<tripoint_abs_omt>;
auto grid_connectivity_at( const tripoint_abs_omt &p ) -> std::vector<tripoint_rel_omt>;
auto storage_stats_at( const tripoint_abs_omt &p ) -> liquid_storage_stats;
auto liquid_charges_at( const tripoint_abs_omt &p, const itype_id &liquid_type ) -> int;
auto would_contaminate( const tripoint_abs_omt &p, const itype_id &liquid_type ) -> bool;
auto add_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                         int charges ) -> int;
auto drain_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                           int charges ) -> int;
auto purify_water( const tripoint_abs_omt &p ) -> units::volume;
auto process_transformers_at( const tripoint_abs_omt &p, time_point to ) -> void;
auto update( time_point to ) -> void;
auto on_contents_changed( const tripoint_abs_ms &p ) -> void;
auto on_structure_changed( const tripoint_abs_ms &p ) -> void;
auto on_tank_removed( const tripoint_abs_ms &p ) -> void;
auto disconnect_tank( const tripoint_abs_ms &p ) -> void;
auto add_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool;
auto remove_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool;
auto clear() -> void;

} // namespace fluid_grid
