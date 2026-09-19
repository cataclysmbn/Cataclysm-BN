#pragma once

#include "calendar.h"
#include "cata_variant.h"
#include "coordinates.h"
#include "cube_direction.h"
#include "json.h"
#include "type_id.h"
#include "weighted_list.h"

#include <unordered_map>

struct point;
struct tripoint;
class mission;
struct regional_settings;
class mapgen_constructor;
class overmapbuffer;
namespace om_direction {
enum class type : int;
} // namespace om_direction

struct mapgen_arguments {
    std::unordered_map<std::string, cata_variant> map;

    void merge(const mapgen_arguments&);
    void serialize(JsonOut&) const;
    void deserialize(JsonIn&);
};

namespace mapgendata_detail {

// helper to get a variant value with any variant being extractable as a string
template <typename Result> inline auto extract_variant_value(const cata_variant& v) -> Result {
    return v.get<Result>();
}
template <> inline auto extract_variant_value<std::string>(const cata_variant& v) -> std::string {
    return v.get_string();
}

} // namespace mapgendata_detail

/**
 * Contains various information regarding the individual mapgen instance
 * (generating a specific part of the map), used by the various mapgen
 * functions to do their thing.
 *
 * Contains for example:
 * - the @ref map to generate the map onto,
 * - the overmap terrain of the area to generate and its surroundings,
 * - regional settings to use.
 *
 * An instance of this class is passed through most of the mapgen code.
 * If any of these functions need more information, add them here.
 */
// TODO: documentation
// TODO: encapsulate data member
class mapgendata {
private:
    oter_id terrain_type_;
    float density_;
    time_point when_;
    ::mission* mission_;
    mapgen_arguments mapgen_args_;
    std::set<flag_id> flags;
    // Explicit overmapbuffer for this generation context.
    // Stored as a reference so worker threads use the dimension-specific
    // buffer (get_overmapbuffer(dim)) rather than the active-dimension global.
    overmapbuffer& omapbuf_;

public:
    oter_id t_nesw[8];

    int n_fac = 0;  // dir == 0
    int e_fac = 0;  // dir == 1
    int s_fac = 0;  // dir == 2
    int w_fac = 0;  // dir == 3
    int ne_fac = 0; // dir == 4
    int se_fac = 0; // dir == 5
    int sw_fac = 0; // dir == 6
    int nw_fac = 0; // dir == 7

    oter_id t_above;
    oter_id t_below;

    std::unordered_map<cube_direction, std::string> joins;

    const tripoint_abs_omt pos;
    const regional_settings& region;

    mapgen_constructor& m;

    weighted_int_list<ter_id> default_groundcover;

    struct dummy_settings_t {};
    static constexpr dummy_settings_t dummy_settings = {};

    /** Return the overmapbuffer bound to this generation context. */
    auto get_overmapbuffer() const -> overmapbuffer& { return omapbuf_; }

    mapgendata(mapgen_constructor&, dummy_settings_t);

    mapgendata(
        const tripoint_abs_omt& over, mapgen_constructor& m, float density, const time_point& when,
        ::mission* miss, overmapbuffer& omap);

    /**
     * Creates a copy of this mapgen data, but stores a different @ref terrain_type.
     * Useful when you want to create a base map (e.g. forest/field/river), that gets
     * refined later:
     * @code
     * void generate_foo( mapgendata &dat ) {
     *     mapgendata base_dat( dat, oter_id( "forest" ) );
     *     generate( base_dat );
     *     ... // refine map some more.
     * }
     * @endcode
     */
    mapgendata(const mapgendata& other, const oter_id& other_id);

    /**
     * Creates a copy of this mapgendata, but stores new parameter values.
     */
    mapgendata(const mapgendata& other, const mapgen_arguments&);

    /**
     * Creates a copy of this mapgendata, but stores new parameter values.
     */
    mapgendata(const mapgendata& other, const mapgen_arguments&, const std::set<flag_id>&);

    auto terrain_type() const -> const oter_id& { return terrain_type_; }
    auto monster_density() const -> float { return density_; }
    auto when() const -> const time_point& { return when_; }
    auto mission() const -> ::mission* { return mission_; }
    auto zlevel() const -> int { return pos.z(); }

    void set_dir(int dir_in, int val);
    void fill(int val);
    auto dir(int dir_in) -> int&;
    auto north() const -> const oter_id& { return t_nesw[0]; }
    auto east() const -> const oter_id& { return t_nesw[1]; }
    auto south() const -> const oter_id& { return t_nesw[2]; }
    auto west() const -> const oter_id& { return t_nesw[3]; }
    auto neast() const -> const oter_id& { return t_nesw[4]; }
    auto seast() const -> const oter_id& { return t_nesw[5]; }
    auto swest() const -> const oter_id& { return t_nesw[6]; }
    auto nwest() const -> const oter_id& { return t_nesw[7]; }
    auto above() const -> const oter_id& { return t_above; }
    auto below() const -> const oter_id& { return t_below; }
    auto neighbor_at(om_direction::type dir) const -> const oter_id&;
    auto neighbor_at(direction) const -> const oter_id&;
    void fill_groundcover() const;
    void square_groundcover(const point_omt_ms& p1, const point_omt_ms& p2) const;
    auto groundcover() const -> ter_id;
    auto is_groundcover(const ter_id& iid) const -> bool;

    auto has_join(const cube_direction, const std::string& join_id) const -> bool;

    auto has_flag(const flag_id& id) const -> bool;

    template <typename Result> auto get_arg(const std::string& name) const -> Result {
        auto it = mapgen_args_.map.find(name);
        if (it == mapgen_args_.map.end()) {
            debugmsg("No such parameter \"%s\"", name);
            return Result();
        }
        return mapgendata_detail::extract_variant_value<Result>(it->second);
    }

    template <typename Result>
    auto get_arg_or(const std::string& name, const Result& fallback) const -> Result {
        auto it = mapgen_args_.map.find(name);
        if (it == mapgen_args_.map.end()) { return fallback; }
        return mapgendata_detail::extract_variant_value<Result>(it->second);
    }
};
