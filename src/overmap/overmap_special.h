#pragma once

#include "coordinates.h"
#include "cube_direction.h"
#include "flat_set.h"
#include "memory_fast.h"
#include "omdata.h"
#include "type_id.h"

#include <array>
#include <bitset>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <list>
#include <set>
#include <string>
#include <vector>

struct city;

class JsonObject;
class overmap_special_batch;
class overmap_special;
class overmap;

// LINE_**** corresponds to the ACS_**** macros in ncurses, and are patterned
// the same way; LINE_NESW, where X indicates a line and O indicates no line
// (thus, LINE_OXXX looks like 'T'). LINE_ is defined in output.h.  The ACS_
// macros can't be used here, since ncurses hasn't been initialized yet.

// Overmap specials--these are "special encounters," dungeons, nests, etc.
// This specifies how often and where they may be placed.

struct overmap_special_spawns: public overmap_spawns {
    numeric_interval<int> radius;

    auto operator==(const overmap_special_spawns& rhs) const -> bool {
        return overmap_spawns::operator==(rhs) && radius == rhs.radius;
    }

    void deserialize(const JsonObject& jo);
};

// This is the information needed to know whether you can place a particular
// piece of an overmap_special at a particular location
struct overmap_special_locations {
    overmap_special_locations() = default;
    overmap_special_locations(
        const tripoint_rel_omt& p, const cata::flat_set<overmap_location_id>& l)
        : p(p),
          locations(l) {};
    tripoint_rel_omt p;
    cata::flat_set<overmap_location_id> locations;

    /**
     * Returns whether this terrain of the special can be placed on the specified terrain.
     * It's true if oter meets any of locations.
     */
    auto can_be_placed_on(const oter_id& oter) const -> bool;
    void deserialize(JsonIn& jsin);
};

struct overmap_special_terrain: overmap_special_locations {
    overmap_special_terrain() = default;
    overmap_special_terrain(
        const tripoint_rel_omt& p, const oter_str_id& t,
        const cata::flat_set<overmap_location_id>& l)
        : overmap_special_locations{p, l},
          terrain(t) {};
    oter_str_id terrain;

    void deserialize(JsonIn& jsin);
};

struct overmap_special_connection {
    tripoint_rel_omt p;
    std::optional<tripoint_rel_omt> from;
    cube_direction initial_dir = cube_direction::last;
    overmap_connection_id connection;
    bool existing = false;

    void deserialize(const JsonObject& jo);
    void finalize();
};

struct overmap_special_placement_constraints {
    numeric_interval<int> city_size{0, INT_MAX};
    numeric_interval<int> city_distance{0, INT_MAX};
    numeric_interval<int> occurrences;
};

enum class overmap_special_subtype { fixed, mutable_, last };

template <> struct enum_traits<overmap_special_subtype> {
    static constexpr overmap_special_subtype last = overmap_special_subtype::last;
};

struct overmap_special_data;
struct special_placement_result;

class overmap_special {
public:
    overmap_special() = default;
    overmap_special(const overmap_special_id& i, const overmap_special_terrain& ter);
    auto get_subtype() const -> overmap_special_subtype { return subtype_; }

    auto get_constraints() const -> const overmap_special_placement_constraints& {
        return constraints_;
    }
    auto is_rotatable() const -> bool { return rotatable_; }
    auto can_spawn() const -> bool;
    /** Returns terrain at the given point. */
    auto get_terrain_at(const tripoint_rel_omt& p) const -> const oter_str_id&;
    /** @returns true if this special requires a city */
    auto requires_city() const -> bool;
    /** @returns whether the special at specified tripoint can belong to the specified city. */
    auto can_belong_to_city(const tripoint_om_omt& p, const city& cit) const -> bool;

    auto get_params() const -> const mapgen_parameters& { return mapgen_params_; }
    auto get_args(const mapgendata&) const -> mapgen_arguments;

    auto get_flags() const -> const cata::flat_set<std::string>& { return flags_; }
    auto has_flag(const std::string& flag) const -> bool { return flags_.count(flag); }
    void set_flag(const std::string& flag) { flags_.insert(flag); }
    auto use_absolute_spawn_loc() const -> bool { return use_absolute_spawn_loc_; }
    auto at_absolute_spawn_loc(point_abs_om point) const -> bool {
        return point == absolute_spawn_loc_;
    }

    auto longest_side() const -> int;
    auto all_terrains() const -> std::vector<oter_str_id>;
    auto preview_terrains() const -> std::vector<overmap_special_terrain>;
    auto required_locations() const -> std::vector<overmap_special_locations>;

    auto place(overmap& om, const tripoint_om_omt& origin, om_direction::type dir) const
        -> special_placement_result;

    auto get_monster_spawns() const -> const overmap_special_spawns& { return monster_spawns_; }
    auto get_nested_specials() const
        -> const std::unordered_map<tripoint_rel_omt, overmap_special_id>& {
        return nested_;
    }

    overmap_special_id id;

    // Used by generic_factory
    bool was_loaded = false;
    void load(const JsonObject& jo, const std::string& src);
    void finalize();
    void finalize_mapgen_parameters();
    void check() const;
    std::vector<overmap_special_connection> connections;

    /**
     * Returns true if this special is allowed to spawn in the given dimension.
     * @param dim_id           The dimension ID (empty = primary/overworld).
     * @param dim_inherits_base True if the dimension's world_type has inherit_base_mapgen=true.
     */
    auto can_spawn_in_dimension(const dimension_id& dim_id, bool dim_inherits_base) const -> bool;

private:
    /// Dimension IDs this special is restricted to.  Empty = primary dimension only.
    std::vector<std::string> dimensions_;
    overmap_special_subtype subtype_;
    overmap_special_placement_constraints constraints_;
    shared_ptr_fast<const overmap_special_data> data_;

    bool rotatable_ = true;
    overmap_special_spawns monster_spawns_;
    cata::flat_set<std::string> flags_;

    // These locations are the default values if ones are not specified for the individual OMTs.
    cata::flat_set<overmap_location_id> default_locations_;
    mapgen_parameters mapgen_params_;
    std::unordered_map<tripoint_rel_omt, overmap_special_id> nested_;
    bool use_absolute_spawn_loc_ = false;
    point_abs_om absolute_spawn_loc_ = point_abs_om(0, 0);
};

namespace overmap_specials {

void load(const JsonObject& jo, const std::string& src);
void finalize();
void finalize_mapgen_parameters();
void check_consistency();
void reset();

auto get_all() -> const std::vector<overmap_special>&;

auto get_default_batch(const point_abs_om& origin) -> overmap_special_batch;
/**
 * Generates a simple special from a building id.
 */
auto create_building_from(const oter_type_str_id& base) -> overmap_special_id;

} // namespace overmap_specials

namespace city_buildings {

void load(const JsonObject& jo, const std::string& src);

} // namespace city_buildings

// Wrapper around an overmap special to track progress of placing specials.
struct overmap_special_placement {
    int instances_placed;
    const overmap_special* special_details;
};

// A batch of overmap specials to place.
class overmap_special_batch {
public:
    overmap_special_batch(const point_abs_om& origin): origin_overmap(origin) {}
    overmap_special_batch(
        const point_abs_om& origin, const std::vector<const overmap_special*>& specials)
        : origin_overmap(origin) {
        std::transform(
            specials.begin(), specials.end(), std::back_inserter(placements),
            [](const overmap_special* elem) { return overmap_special_placement{0, elem}; });
    }

    // Wrapper methods that make overmap_special_batch act like
    // the underlying vector of overmap placements.
    auto begin() -> std::vector<overmap_special_placement>::iterator { return placements.begin(); }
    auto end() -> std::vector<overmap_special_placement>::iterator { return placements.end(); }
    auto erase(std::vector<overmap_special_placement>::iterator pos)
        -> std::vector<overmap_special_placement>::iterator {
        return placements.erase(pos);
    }
    auto empty() -> bool { return placements.empty(); }

    auto get_origin() const -> point_abs_om { return origin_overmap; }

private:
    std::vector<overmap_special_placement> placements;
    point_abs_om origin_overmap;
};
