#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <set>
#include <vector>
#include <array>
#include <string>

#include "cuboid_rectangle.h"
#include "catacharset.h"
#include "color.h"
#include "numeric_interval.h"
#include "coordinates.h"
#include "enum_bitset.h"
#include "int_id.h"
#include "om_direction.h"
#include "mapgen_parameter.h"
#include "point.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"

struct city;
class overmap_land_use_code;
struct MonsterGroup;

using overmap_land_use_code_id = string_id<overmap_land_use_code>;
class JsonObject;
struct mapgen_arguments;

static const overmap_land_use_code_id land_use_code_forest( "forest" );
static const overmap_land_use_code_id land_use_code_wetland( "wetland" );
static const overmap_land_use_code_id land_use_code_wetland_forest( "wetland_forest" );
static const overmap_land_use_code_id land_use_code_wetland_saltwater( "wetland_saltwater" );

class overmap_land_use_code
{
    public:
        overmap_land_use_code_id id = overmap_land_use_code_id::NULL_ID();

        int land_use_code = 0;
        std::string name;
        std::string detailed_definition;
        uint32_t symbol = 0;
        nc_color color = c_black;

        std::string get_symbol() const;

        // Used by generic_factory
        bool was_loaded = false;
        void load( const JsonObject &jo, const std::string &src );
        void finalize();
        void check() const;
};

struct overmap_spawns {
        overmap_spawns() : group( mongroup_id::NULL_ID() ) {}

        string_id<MonsterGroup> group;
        numeric_interval<int> population;

        bool operator==( const overmap_spawns &rhs ) const {
            return group == rhs.group && population == rhs.population;
        }

    protected:
        void deserialize( const JsonObject &jo );
};

struct overmap_static_spawns : public overmap_spawns {
    int chance = 0;

    bool operator==( const overmap_static_spawns &rhs ) const {
        return overmap_spawns::operator==( rhs ) && chance == rhs.chance;
    }

    void deserialize( const JsonObject &jo );
};

//terrain flags enum! this is for tracking the indices of each flag.
enum class oter_flags : int {
    known_down = 0,
    known_up,
    no_rotate,    // this tile doesn't have four rotated versions (north, east, south, west)
    ignore_rotation_for_adjacency,
    river_tile,
    has_sidewalk,
    line_drawing, // does this tile have 8 versions, including straights, bends, tees, and a fourway?
    subway_connection,
    lake,
    lake_shore,
    generic_loot,
    risk_high,
    risk_low,
    source_ammo,
    source_animals,
    source_books,
    source_chemistry,
    source_clothing,
    source_construction,
    source_cooking,
    source_drink,
    source_electronics,
    source_fabrication,
    source_farming,
    source_food,
    source_forage,
    source_fuel,
    source_gun,
    source_luxury,
    source_medicine,
    source_people,
    source_safety,
    source_tailoring,
    source_vehicles,
    source_weapon,
    is_bridge,
    num_oter_flags
};

template<>
struct enum_traits<oter_flags> {
    static constexpr auto last = oter_flags::num_oter_flags;
};

struct oter_type_t {
    public:
        static const oter_type_t null_type;

    public:
        oter_type_str_id id;
        std::string name;               // Untranslated name
        uint32_t symbol = 0;
        nc_color color = c_black;
        overmap_land_use_code_id land_use_code = overmap_land_use_code_id::NULL_ID();
        std::vector<std::string> looks_like;
        unsigned char see_cost = 0;     // Affects how far the player can see in the overmap
        unsigned char travel_cost = 5;  // Affects the pathfinding and travel times
        std::string extras = "none";
        int mondensity = 0;
        // Spawns are added to the submaps *once* upon mapgen of the submaps
        overmap_static_spawns static_spawns;
        bool was_loaded = false;

        std::string get_symbol() const;

        oter_type_t() = default;

        oter_id get_first() const;
        oter_id get_rotated( om_direction::type dir ) const;
        oter_id get_linear( size_t n ) const;

        bool has_flag( oter_flags flag ) const {
            return flags[flag];
        }

        void set_flag( oter_flags flag, bool value = true ) {
            flags.set( flag, value );
        }

        void load( const JsonObject &jo, const std::string &src );
        void check() const;
        void finalize();

        bool is_rotatable() const {
            return !has_flag( oter_flags::no_rotate ) && !has_flag( oter_flags::line_drawing );
        }

        bool is_linear() const {
            return has_flag( oter_flags::line_drawing );
        }

        bool has_connections() const {
            return !connect_group.empty();
        }

        bool connects_to( const oter_type_id &other ) const {
            return has_connections() && connect_group == other->connect_group;
        }

    private:
        enum_bitset<oter_flags> flags;
        std::vector<oter_id> directional_peers;
        std::string connect_group; // Group for connection when rendering overmap tiles

        void register_terrain( const oter_t &peer, size_t n, size_t max_n );
};

struct oter_t {
    private:
        const oter_type_t *type;

    public:
        oter_str_id id;         // definitive identifier.

        oter_t();
        oter_t( const oter_type_t &type );
        oter_t( const oter_type_t &type, om_direction::type dir );
        oter_t( const oter_type_t &type, size_t line );

        const oter_type_str_id &get_type_id() const {
            return type->id;
        }

        std::string get_mapgen_id() const;
        oter_id get_rotated( om_direction::type dir ) const;

        std::string get_name() const {
            return _( type->name );
        }

        std::string get_symbol( const bool from_land_use_code = false ) const {
            return utf32_to_utf8( from_land_use_code ? symbol_alt : symbol );
        }

        uint32_t get_uint32_symbol() const {
            return symbol;
        }

        nc_color get_color( const bool from_land_use_code = false ) const {
            return from_land_use_code ? type->land_use_code->color : type->color;
        }

        // dir is only meaningful for rotatable, non-linear terrain.  If you
        // need an answer that also works for linear terrain, call
        // get_rotation() instead.
        om_direction::type get_dir() const {
            return dir;
        }

        size_t get_line() const {
            return line;
        }
        void get_rotation_and_subtile( int &rotation, int &subtile ) const;
        int get_rotation() const;

        unsigned char get_see_cost() const {
            return type->see_cost;
        }
        unsigned char get_travel_cost() const {
            return type->travel_cost;
        }

        const std::string &get_extras() const {
            return type->extras;
        }

        int get_mondensity() const {
            return type->mondensity;
        }

        const overmap_static_spawns &get_static_spawns() const {
            return type->static_spawns;
        }

        overmap_land_use_code_id get_land_use_code() const {
            return type->land_use_code;
        }

        bool type_is( const oter_type_id &type_id ) const;
        bool type_is( const oter_type_t &type ) const;

        bool has_connection( om_direction::type dir ) const;

        bool has_flag( oter_flags flag ) const {
            return type->has_flag( flag );
        }

        bool is_hardcoded() const;

        bool is_rotatable() const {
            return type->is_rotatable();
        }

        bool is_linear() const {
            return type->is_linear();
        }

        bool is_river() const {
            return type->has_flag( oter_flags::river_tile );
        }

        bool is_wooded() const {
            return type->land_use_code == land_use_code_forest ||
                   type->land_use_code == land_use_code_wetland ||
                   type->land_use_code == land_use_code_wetland_forest ||
                   type->land_use_code == land_use_code_wetland_saltwater;
        }

        bool is_lake() const {
            return type->has_flag( oter_flags::lake );
        }

        bool is_lake_shore() const {
            return type->has_flag( oter_flags::lake_shore );
        }

    private:
        om_direction::type dir = om_direction::type::none;
        uint32_t symbol;
        uint32_t symbol_alt;
        size_t line = 0;         // Index of line. Only valid in case of line drawing.
};

// TODO: Deprecate these operators
bool operator==( const oter_id &lhs, const char *rhs );
bool operator!=( const oter_id &lhs, const char *rhs );

namespace overmap_terrains
{

void load( const JsonObject &jo, const std::string &src );
void check_consistency();
void finalize();
void reset();

const std::vector<oter_t> &get_all();

} // namespace overmap_terrains

namespace overmap_land_use_codes
{

void load( const JsonObject &jo, const std::string &src );
void finalize();
void check_consistency();
void reset();

const std::vector<overmap_land_use_code> &get_all();

} // namespace overmap_land_use_codes


