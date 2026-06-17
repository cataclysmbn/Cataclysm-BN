#pragma once

#include <functional>
#include <climits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "map.h"
#include "mapgen_functions.h"
#include "point.h"
#include "type_id.h"
#include "units_angle.h"

class computer;
class field_entry;
class item;
class mapbuffer;
class mapgendata;
class submap;
class trap;
class vehicle;
struct json_source_location;
struct rl_vec2d;
template<typename T>
class detached_ptr;
template<typename T>
class location_vector;
template<typename T>
struct weighted_int_list;

struct mapgen_generate_options {
    bool defer_postprocess_hooks = false;
    bool worker_safe = false;
    bool use_selected_mapgen = false;
    std::shared_ptr<mapgen_function> selected_mapgen;
};

class mapgen_constructor
{
    public:
        explicit mapgen_constructor( mapbuffer &buffer );
        mapgen_constructor( mapbuffer &buffer, const tripoint_abs_omt &omt_pos );
        ~mapgen_constructor();

        auto generate( const tripoint_abs_omt &omt_pos, const time_point &when,
        const mapgen_generate_options &options = {} ) -> mapgen_result;

        auto load( const tripoint_abs_omt &omt_pos ) -> bool;
        auto bind_omt_for_hook( const tripoint_abs_omt &omt_pos ) -> bool;
        auto reset_scratch_omt( const tripoint_abs_omt &omt_pos, const ter_id &terrain,
                                const furn_id &furniture, const trap_id &trap ) -> void;
        auto clear() -> void;
        auto clear_spawns() -> void;
        auto clear_traps() -> void;

        auto get_abs_omt() const -> tripoint_abs_omt;
        auto get_bound_dimension() const -> const dimension_id &;
        auto get_buffer() const -> mapbuffer &;

        auto points_on_zlevel() const -> point_range<point_omt_ms>;
        auto points_in_rectangle( const point_omt_ms &from,
                                  const point_omt_ms &to ) const -> point_range<point_omt_ms>;
        auto points_in_radius( const point_omt_ms &center,
                               size_t radius ) const -> point_range<point_omt_ms>;

        auto ter( const point_omt_ms &p ) const -> ter_id;
        auto furn( const point_omt_ms &p ) const -> furn_id;
        auto tr_at( const point_omt_ms &p ) const -> const trap &;
        auto has_furn( const point_omt_ms &p ) const -> bool;
        auto has_flag( const std::string &flag, const point_omt_ms &p ) const -> bool;
        auto has_flag( ter_bitflags flag, const point_omt_ms &p ) const -> bool;
        auto has_flag_ter( const std::string &flag, const point_omt_ms &p ) const -> bool;
        auto has_flag_ter( const ter_bitflags flag, const point_omt_ms &p ) const -> bool;
        auto has_flag_furn( const std::string &flag, const point_omt_ms &p ) const -> bool;
        auto has_flag_furn( ter_bitflags flag, const point_omt_ms &p ) const -> bool;
        auto passable( const point_omt_ms &p ) const -> bool;
        auto impassable( const point_omt_ms &p ) const -> bool;
        auto move_cost( const point_omt_ms &p, const vehicle *ignored_vehicle = nullptr ) const -> int;
        auto is_bashable( const point_omt_ms &p, bool allow_floor = false ) const -> bool;
        auto veh_at( const point_omt_ms &p ) const -> optional_vpart_position;
        auto get_vehicles() const -> std::vector<vehicle *>;

        auto ter_set( const point_omt_ms &p, const ter_id &terrain ) -> bool;
        auto furn_set( const point_omt_ms &p, const furn_id &furniture ) -> bool;
        auto trap_set( const point_omt_ms &p, const trap_id &trap ) -> void;
        auto remove_trap( const point_omt_ms &p ) -> void;
        auto set_radiation( const point_omt_ms &p, int value ) -> void;
        auto adjust_radiation( const point_omt_ms &p, int delta ) -> void;
        auto get_temperature( const point_omt_ms &p ) const -> int;
        auto set_temperature( const point_omt_ms &p, int temperature ) -> void;
        auto add_field( const point_omt_ms &p, const field_type_id &type_id,
                        int intensity = INT_MAX, const time_duration &age = 0_turns,
                        bool hit_player = true ) -> bool;
        auto remove_field( const point_omt_ms &p, const field_type_id &field_to_remove ) -> void;
        auto add_splatter_trail( const field_type_id &type, const point_omt_ms &from,
                                 const point_omt_ms &to ) -> void;
        auto add_computer( const point_omt_ms &p, const std::string &name,
                           int security ) -> computer *;
        auto set_signage( const point_omt_ms &p, const std::string &message ) const -> void;
        auto set_graffiti( const point_omt_ms &p, const std::string &contents ) -> void;
        auto i_at( const point_omt_ms &p ) -> map_stack;
        auto i_clear( const point_omt_ms &p ) -> std::vector<detached_ptr<item>>;
        auto add_item( const point_omt_ms &p, detached_ptr<item> &&new_item ) -> void;
        auto add_item_or_charges( const point_omt_ms &pos, detached_ptr<item> &&obj,
                                  bool overflow = true ) -> detached_ptr<item>;
        auto spawn_an_item( const point_omt_ms &p, detached_ptr<item> &&new_item,
                            int charges, int damlevel ) -> detached_ptr<item>;
        auto spawn_items( const point_omt_ms &p,
                          std::vector<detached_ptr<item>> new_items ) -> std::vector<detached_ptr<item>>;
        auto spawn_item( const point_omt_ms &p, const itype_id &type_id,
                         unsigned quantity = 1, int charges = 0,
                         const time_point &birthday = calendar::start_of_cataclysm,
                         int damlevel = 0 ) -> void;
        auto put_items_from_loc( const item_group_id &loc, const point_omt_ms &p,
                                 const time_point &turn = calendar::start_of_cataclysm ) -> std::vector<item *>;
        auto place_items( const item_group_id &loc, int chance, const point_omt_ms &p1,
                          const point_omt_ms &p2, bool ongrass, const time_point &turn,
                          int magazine = 0, int ammo = 0 ) -> std::vector<item *>;
        auto item_category_spawn_rate( const item &itm ) -> float;
        auto flammable_items_at( const point_omt_ms &p, int threshold = 0 ) -> bool;

        auto draw_line_ter( const ter_id &type, const point_omt_ms &p1,
                            const point_omt_ms &p2 ) -> void;
        auto draw_line_furn( const furn_id &type, const point_omt_ms &p1,
                             const point_omt_ms &p2 ) -> void;
        auto draw_fill_background( const ter_id &type ) -> void;
        auto draw_fill_background( ter_id( *f )() ) -> void;
        auto draw_fill_background( const weighted_int_list<ter_id> &f ) -> void;
        auto draw_square_ter( const ter_id &type, const point_omt_ms &p1,
                              const point_omt_ms &p2 ) -> void;
        auto draw_square_ter( ter_id( *f )(), const point_omt_ms &p1,
                              const point_omt_ms &p2 ) -> void;
        auto draw_square_ter( const weighted_int_list<ter_id> &f, const point_omt_ms &p1,
                              const point_omt_ms &p2 ) -> void;
        auto draw_square_furn( const furn_id &type, const point_omt_ms &p1,
                               const point_omt_ms &p2 ) -> void;
        auto draw_rough_circle_ter( const ter_id &type, const point_omt_ms &p,
                                    int rad ) -> void;
        auto draw_rough_circle_furn( const furn_id &type, const point_omt_ms &p,
                                     int rad ) -> void;
        auto draw_circle_ter( const ter_id &type, const rl_vec2d &p, double rad ) -> void;
        auto draw_circle_ter( const ter_id &type, const point_omt_ms &p, int rad ) -> void;
        auto draw_circle_furn( const furn_id &type, const point_omt_ms &p, int rad ) -> void;
        auto translate( const ter_id &from, const ter_id &to ) -> void;
        auto rotate( int turns, bool setpos_safe = false ) -> void;

        auto place_spawns( const mongroup_id &group, int chance,
                           const point_omt_ms &p1, const point_omt_ms &p2, float density,
                           bool individual = false, bool friendly = false,
                           const std::string &name = "NONE", int mission_id = -1 ) -> void;
        auto place_gas_pump( const point_omt_ms &p, int charges,
                             const itype_id &fuel_type ) -> void;
        auto place_gas_pump( const point_omt_ms &p, int charges ) -> void;
        auto place_toilet( const point_omt_ms &p, int charges = 6 * 4 ) -> void;
        auto place_vending( const point_omt_ms &p, const item_group_id &type,
                            bool reinforced = false ) -> void;
        auto place_npc( const point_omt_ms &p, const string_id<npc_template> &type,
                        bool force = false ) -> character_id;
        auto apply_faction_ownership( const point_omt_ms &p1, const point_omt_ms &p2,
                                      const faction_id &id ) -> void;
        auto add_spawn( const mtype_id &type, int count, const point_omt_ms &p,
                        bool friendly = false, int faction_id = -1, int mission_id = -1,
                        const std::string &name = "NONE" ) const -> void;
        auto add_spawn( const mtype_id &type, int count, const point_omt_ms &p,
                        spawn_disposition disposition, int faction_id = -1, int mission_id = -1,
                        const std::string &name = "NONE" ) const -> void;
        auto add_vehicle( const std::variant<vgroup_id, vproto_id> &type_,
                          const point_omt_ms &p,
                          units::angle dir, int init_veh_fuel = -1,
                          int init_veh_status = -1, bool merge_wrecks = true,
                          std::optional<bool> locked = std::nullopt,
                          std::optional<bool> has_keys = std::nullopt ) -> vehicle *;
        auto destroy_vehicle( vehicle *veh ) -> void;
        auto add_corpse( const point_omt_ms &p ) -> void;
        auto spawn_artifact( const point_omt_ms &p ) -> void;
        auto spawn_natural_artifact( const point_omt_ms &p,
                                     artifact_natural_property prop ) -> void;
        auto make_rubble( const point_omt_ms &p, const furn_id &rubble_type,
                          const ter_id &floor_type, bool overwrite = false ) -> void;
        auto make_rubble( const point_omt_ms &p, const furn_id &rubble_type ) -> void;
        auto bash( const point_omt_ms &p, int str,
                   bool destroy = false, bool bash_floor = false,
                   const vehicle *bashing_vehicle = nullptr ) -> void;
        auto destroy( const point_omt_ms &p ) -> void;
        auto create_anomaly( const point_omt_ms &p, artifact_natural_property prop,
                             bool create_rubble = true ) -> void;
        auto draw_map( mapgendata &dat, const mapgen_generate_options &options ) -> mapgen_result;
        auto draw_office_tower( const mapgendata &dat ) -> void;
        auto draw_temple( const mapgendata &dat ) -> void;
        auto draw_mine( mapgendata &dat ) -> void;
        auto draw_connections( const mapgendata &dat ) -> void;

    private:
        auto submap_at( const point_omt_ms &p ) const -> submap *;
        auto submap_at_grid( const point_omt_sm &gridp ) const -> submap *;
        auto tile_at( const point_omt_ms &p ) const -> std::pair<submap *, point_sm_ms>;
        auto all_submap_offsets() const -> point_range<point_omt_sm>;
        auto all_tile_bounds() const -> inclusive_rectangle<point_omt_ms>;
        auto for_each_submap( const std::function<void( submap & )> &func ) const -> void;
        auto ensure_omt_submaps( const time_point &when ) -> bool;
        auto commit_generated_submaps() -> bool;
        auto discard_generated_submaps() -> void;

        mapbuffer &buffer_;
        tripoint_abs_omt abs_offset_ = tripoint_abs_omt::zero();
        std::vector<std::unique_ptr<submap>> owned_submaps_;
};

auto points_in_range( const mapgen_constructor &m ) -> point_range<point_omt_ms>;
auto random_point( const point_range<point_omt_ms> &range,
                   const std::function<bool( const point_omt_ms & )> &predicate )
-> std::optional<point_omt_ms>; // *NOPAD*
auto random_point( const mapgen_constructor &m,
                   const std::function<bool( const point_omt_ms & )> &predicate )
-> std::optional<point_omt_ms>; // *NOPAD*
