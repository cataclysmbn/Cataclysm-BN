#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <bitset>
#include <list>
#include <set>
#include <vector>
#include <array>
#include <string>

#include "cube_direction.h"
#include "flat_set.h"
#include "memory_fast.h"
#include "omdata.h"
#include "point.h"
#include "type_id.h"

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

struct overmap_special_spawns : public overmap_spawns {
    numeric_interval<int> radius;

    bool operator==( const overmap_special_spawns &rhs ) const {
        return overmap_spawns::operator==( rhs ) && radius == rhs.radius;
    }

    void deserialize( const JsonObject &jo );
};

// This is the information needed to know whether you can place a particular
// piece of an overmap_special at a particular location
struct overmap_special_locations {
    overmap_special_locations() = default;
    overmap_special_locations( const tripoint &p,
                               const cata::flat_set<overmap_location_id> &l )
        : p( p )
        , locations( l )
    {};
    tripoint p;
    cata::flat_set<overmap_location_id> locations;

    /**
     * Returns whether this terrain of the special can be placed on the specified terrain.
     * It's true if oter meets any of locations.
     */
    bool can_be_placed_on( const oter_id &oter ) const;
    void deserialize( JsonIn &jsin );
};

struct overmap_special_terrain : overmap_special_locations {
    overmap_special_terrain() = default;
    overmap_special_terrain( const tripoint &p, const oter_str_id &t,
                             const cata::flat_set<overmap_location_id> &l )
        : overmap_special_locations{ p, l }
        , terrain( t )
    {};
    oter_str_id terrain;

    void deserialize( JsonIn &jsin );
};

struct overmap_special_connection {
    tripoint p;
    std::optional<tripoint> from;
    cube_direction initial_dir = cube_direction::last;
    overmap_connection_id connection;
    bool existing = false;

    void deserialize( const JsonObject &jo );
    void finalize();
};

struct overmap_special_placement_constraints {
    numeric_interval<int> city_size{ 0, INT_MAX };
    numeric_interval<int> city_distance{ 0, INT_MAX };
    numeric_interval<int> occurrences;
};

enum class overmap_special_subtype {
    fixed,
    mutable_,
    last
};

template<>
struct enum_traits<overmap_special_subtype> {
    static constexpr overmap_special_subtype last = overmap_special_subtype::last;
};

struct overmap_special_data;
struct special_placement_result;

class overmap_special
{
    public:
        overmap_special() = default;
        overmap_special( const overmap_special_id &i, const overmap_special_terrain &ter );
        overmap_special_subtype get_subtype() const {
            return subtype_;
        }

        const overmap_special_placement_constraints &get_constraints() const {
            return constraints_;
        }
        bool is_rotatable() const {
            return rotatable_;
        }
        bool can_spawn() const;
        /** Returns terrain at the given point. */
        const oter_str_id &get_terrain_at( const tripoint &p ) const;
        /** @returns true if this special requires a city */
        bool requires_city() const;
        /** @returns whether the special at specified tripoint can belong to the specified city. */
        bool can_belong_to_city( const tripoint_om_omt &p, const city &cit ) const;

        const mapgen_parameters &get_params() const {
            return mapgen_params_;
        }
        mapgen_arguments get_args( const mapgendata & ) const;

        const cata::flat_set<std::string> &get_flags() const {
            return flags_;
        }
        bool has_flag( const std::string &flag ) const {
            return flags_.count( flag );
        }
        void set_flag( const std::string &flag ) {
            flags_.insert( flag );
        }
        int longest_side() const;
        std::vector<oter_str_id> all_terrains() const;
        std::vector<overmap_special_terrain> preview_terrains() const;
        std::vector<overmap_special_locations> required_locations() const;

        special_placement_result place(
            overmap &om, const tripoint_om_omt &origin, om_direction::type dir ) const;

        const overmap_special_spawns &get_monster_spawns() const {
            return monster_spawns_;
        }
        const std::unordered_map<tripoint_rel_omt, overmap_special_id> &get_nested_specials() const {
            return nested_;
        }

        overmap_special_id id;

        // Used by generic_factory
        bool was_loaded = false;
        void load( const JsonObject &jo, const std::string &src );
        void finalize();
        void finalize_mapgen_parameters();
        void check() const;
        std::vector<overmap_special_connection> connections;
    private:
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
};

namespace overmap_specials
{

void load( const JsonObject &jo, const std::string &src );
void finalize();
void finalize_mapgen_parameters();
void check_consistency();
void reset();

const std::vector<overmap_special> &get_all();

overmap_special_batch get_default_batch( const point_abs_om &origin );
/**
 * Generates a simple special from a building id.
 */
overmap_special_id create_building_from( const oter_type_str_id &base );

} // namespace overmap_specials

namespace city_buildings
{

void load( const JsonObject &jo, const std::string &src );

} // namespace city_buildings

// Wrapper around an overmap special to track progress of placing specials.
struct overmap_special_placement {
    int instances_placed;
    const overmap_special *special_details;
};

// A batch of overmap specials to place.
class overmap_special_batch
{
    public:
        overmap_special_batch( const point_abs_om &origin ) : origin_overmap( origin ) {}
        overmap_special_batch( const point_abs_om &origin,
                               const std::vector<const overmap_special *> &specials ) :
            origin_overmap( origin ) {
            std::transform( specials.begin(), specials.end(),
            std::back_inserter( placements ), []( const overmap_special * elem ) {
                return overmap_special_placement{ 0, elem };
            } );
        }

        // Wrapper methods that make overmap_special_batch act like
        // the underlying vector of overmap placements.
        std::vector<overmap_special_placement>::iterator begin() {
            return placements.begin();
        }
        std::vector<overmap_special_placement>::iterator end() {
            return placements.end();
        }
        std::vector<overmap_special_placement>::iterator erase(
            std::vector<overmap_special_placement>::iterator pos ) {
            return placements.erase( pos );
        }
        bool empty() {
            return placements.empty();
        }

        point_abs_om get_origin() const {
            return origin_overmap;
        }

    private:
        std::vector<overmap_special_placement> placements;
        point_abs_om origin_overmap;
};


