#pragma once

#include "active_item_cache.h"
#include "calendar.h"
#include "clzones.h"
#include "coordinates.h"
#include "damage.h"
#include "game_constants.h"
#include "item.h"
#include "item_stack.h"
#include "point.h"
#include "tileray.h"
#include "type_id.h"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class avatar;
class Character;
class Creature;
class JsonIn;
class JsonOut;
class map;
class mapgen_constructor;
class monster;
class nc_color;
class npc;
class player;
class vehicle;
struct vehicle_part;
class mapbuffer;
class vehicle_cursor;
class vehicle_part_range;
class vpart_info;
struct itype;
struct uilist_entry;
template <typename T> class visitable;
struct rl_vec2d;

enum vpart_bitflags : int;
enum ter_bitflags : int;
template <typename feature_type> class vehicle_part_with_feature_range;

void handbrake();

namespace catacurses {
class window;
} // namespace catacurses
namespace vehicles {
// ratio of constant rolling resistance to the part that varies with velocity
constexpr double rolling_constant_to_variable = 33.33;
// 1 tile/s is approximately 1.78816 m/s == 178.816 cm/s.
constexpr float cmps_per_tile = 178.816f;
} // namespace vehicles
struct rider_data {
    Creature* psg = nullptr;
    int prt = -1;
    bool moved = false;
};

struct cargo_recharge_target {
    safe_reference<item> target;
    int cargo_part = -1;
};

// collision factor for vehicle-vehicle collision; delta_v in m/s
auto get_collision_factor(float delta_v) -> float;

// How far to scatter parts from a vehicle when the part is destroyed (+/-)
constexpr int SCATTER_DISTANCE = 3;
// adjust this to balance collision damage
constexpr int k_mvel = 200;

enum class turret_filter_types : int { MANUAL = 0, AUTOMATIC, BOTH };

enum class part_status_flag : int {
    any = 0,
    working = 1 << 0,
    available = 1 << 1,
    enabled = 1 << 2
};
auto inline operator|(const part_status_flag& rhs, const part_status_flag& lhs)
    -> part_status_flag {
    return static_cast<part_status_flag>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
auto inline operator&(const part_status_flag& rhs, const part_status_flag& lhs) -> int {
    return static_cast<int>(lhs) & static_cast<int>(rhs);
}

enum veh_coll_type : int {
    veh_coll_nothing,       // 0 - nothing,
    veh_coll_body,          // 1 - monster/player/npc
    veh_coll_veh,           // 2 - vehicle
    veh_coll_bashable,      // 3 - bashable
    veh_coll_other,         // 4 - other
    veh_coll_veh_nocollide, // 5 - vehicle NOCOLLIDE
    num_veh_coll_types
};

struct veh_collision {
    // int veh?
    int part = 0;
    veh_coll_type type = veh_coll_nothing;
    // Impulse, in Ns. Call impulse_to_damage or damage_to_impulse from vehicle_move.cpp for
    // conversion to damage.
    int imp = 0;
    // vehicle
    void* target = nullptr;
    // vehicle partnum
    int target_part = 0;
    std::string target_name;

    veh_collision() = default;
};

struct vehicle_collision_options {
    std::vector<veh_collision>& colls;
    tripoint_rel_ms dp;
    bool just_detect = false;
    bool bash_floor = false;
    const Creature* ignored_critter = nullptr;
};

struct vehicle_part_collision_options {
    int part = 0;
    tripoint_bub_ms pos;
    bool just_detect = false;
    bool bash_floor = false;
    bool vertical = false;
    const Creature* ignored_critter = nullptr;
};
// TODO!: location stuffs here
class vehicle_stack: public item_stack {
private:
    tripoint_bub_ms location;
    vehicle* myorigin;
    int part_num;

public:
    vehicle_stack(
        location_vector<item>* newstack, tripoint_bub_ms newloc, vehicle* neworigin, int part)
        : item_stack(newstack),
          location(newloc),
          myorigin(neworigin),
          part_num(part) {}
    auto erase(const_iterator it, detached_ptr<item>* out = nullptr) -> iterator override;
    auto remove(item* to_remove) -> detached_ptr<item> override;
    void insert(detached_ptr<item>&& newitem) override;
    auto count_limit() const -> int override { return MAX_ITEM_IN_VEHICLE_STORAGE; }
    auto max_volume() const -> units::volume override;
};

enum towing_point_side : int { TOW_FRONT, TOW_SIDE, TOW_BACK, NUM_TOW_TYPES };

class towing_data {
private:
    vehicle* towing;
    vehicle* towed_by;

public:
    towing_data(vehicle* towed_veh = nullptr, vehicle* tower_veh = nullptr)
        : towing(towed_veh),
          towed_by(tower_veh) {}
    auto get_towed_by() const -> vehicle* { return towed_by; }
    auto set_towing(vehicle* tower_veh, vehicle* towed_veh) -> bool;
    auto get_towed() const -> vehicle* { return towing; }
    void clear_towing() {
        towing = nullptr;
        towed_by = nullptr;
    }
    towing_point_side tow_direction;
    // temp variable used for saving/loading
    tripoint_bub_ms other_towing_point;
};

struct bounding_box {
    point p1;
    point p2;
};

auto keybind(const std::string& opt, const std::string& context = "VEHICLE") -> char;

auto mps_to_cmps(double mps) -> int;
auto cmps_to_mps(int cmps) -> double;
auto impulse_to_damage(float impulse) -> float;
auto damage_to_impulse(float damage) -> float;

class turret_data {
    friend vehicle;

public:
    turret_data() = default;
    turret_data(const turret_data&) = delete;
    auto operator=(const turret_data&) -> turret_data& = delete;
    turret_data(turret_data&&) = default;
    auto operator=(turret_data&&) -> turret_data& = default;

    /** Is this a valid instance? */
    explicit operator bool() const { return veh && part; }

    auto name() const -> std::string;

    /** Get base item location */
    auto base() -> item&;
    auto base() const -> item&;

    auto get_veh() const -> const vehicle* { return veh; }

    /** Quantity of ammunition available for use */
    auto ammo_remaining() const -> int;

    /** Maximum quantity of ammunition turret can itself contain */
    auto ammo_capacity() const -> int;

    /** Specific ammo data or returns nullptr if no ammo available */
    auto ammo_data() const -> const itype*;

    /** Specific ammo type or returns "null" if no ammo available */
    auto ammo_current() const -> itype_id;

    /** What ammo is available for this turret (may be multiple if uses tanks) */
    auto ammo_options() const -> std::set<itype_id>;

    /** Attempts selecting ammo type and returns true if selection was valid */
    auto ammo_select(const itype_id& ammo) -> bool;

    /** Effects inclusive of any from ammo loaded from tanks */
    auto ammo_effects() const -> std::set<ammo_effect_str_id>;

    /** Maximum range considering current ammo (if any) */
    auto range() const -> int;

    /**
     * Check if target is in range of this turret (considers current ammo)
     * Assumes this turret's status is 'ready'
     */
    auto in_range(const tripoint_abs_ms& target) const -> bool;

    /**
     * Prepare the turret for firing, called by firing function.
     * This sets up vehicle tanks, recoil adjustments, vehicle rooftop status,
     * and performs any other actions that must be done before firing a turret.
     * @param p the player that is firing the gun, subject to recoil adjustment.
     */
    void prepare_fire(Character& who);

    /**
     * Reset state after firing a prepared turret, called by the firing function.
     * @param p the player that just fired (or attempted to fire) the turret.
     * @param shots the number of shots fired by the most recent call to turret::fire.
     */
    void post_fire(Character& who, int shots);

    /**
     * Fire the turret's gun at a given target.
     * @param p the player firing the turret, passed to pre_fire and post_fire calls.
     * @param target coordinates that will be fired on.
     * @return the number of shots actually fired (may be zero).
     */
    auto fire(Character& who, const tripoint_abs_ms& target) -> int;

    auto can_reload() const -> bool;
    auto can_unload() const -> bool;

    enum class status { invalid, no_ammo, no_power, ready };

    auto query() const -> status;

private:
    turret_data(vehicle* veh, vehicle_part* part): veh(veh), part(part) {}
    double cached_recoil = 0;

protected:
    vehicle* veh = nullptr;
    vehicle_part* part = nullptr;
};

/**
 * Struct used for storing labels
 * (easier to json opposed to a std::map<point, std::string>)
 */
struct label: public tripoint_mnt_veh {
    label() = default;
    explicit label(tripoint_mnt_veh p): tripoint_mnt_veh(p) {}
    label(tripoint_mnt_veh p, std::string text): tripoint_mnt_veh(p), text(std::move(text)) {}

    std::string text;

    void deserialize(JsonIn& jsin);
    void serialize(JsonOut& json) const;
};

enum class autodrive_result : int {
    // the driver successfully performed course correction or simply did nothing
    // in order to keep going forward
    ok,
    // something bad happened (navigation error, crash, loss of visibility, or just
    // couldn't find a way around obstacles) and autodrive cannot continue
    abort,
    // arrived at the destination
    finished
};

class RemovePartHandler;

/**
 * A vehicle as a whole with all its components.
 *
 * This object can occupy multiple tiles, the objects actually visible
 * on the map are of type `vehicle_part`.
 *
 * Facts you need to know about implementation:
 * - Vehicles belong to map. There's `std::vector<vehicle>`
 *   for each submap in grid. When requesting a reference
 *   to vehicle, keep in mind it can be invalidated
 *   by functions such as `map::displace_vehicle()`.
 * - To check if there's any vehicle at given map tile,
 *   call `map::veh_at()`, and check vehicle type (`veh_null`
 *   means there's no vehicle there).
 * - Vehicle consists of parts (represented by vector). Parts have some
 *   constant info: see veh_type.h, `vpart_info` structure and
 *   vpart_list array -- that is accessible through `part_info()` method.
 *   The second part is variable info, see `vehicle_part` structure.
 * - Parts are mounted at some point relative to vehicle position (or starting part)
 *   (`0, 0` in mount coordinates). There can be more than one part at
 *   given mount coordinates, and they are mounted in different slots.
 *   Check tileray.h file to see a picture of coordinate axes.
 * - Vehicle can be rotated to arbitrary degree. This means that
 *   mount coordinates are rotated to match vehicle's face direction before
 *   their actual positions are known. For optimization purposes
 *   mount coordinates are precalculated for current vehicle face direction
 *   and stored in `precalc[0]`. `precalc[1]` stores mount coordinates for
 *   next move (vehicle can move and turn). Method `map::displace_vehicle()`
 *   assigns `precalc[1]` to `precalc[0]`. At any time (except
 *   `map::vehmove()` innermost cycle) you can get actual part coordinates
 *   relative to vehicle's position by reading `precalc[0]`.
 *   Vehicles rotate around a (possibly changing) pivot point, and
 *   the precalculated coordinates always put the pivot point at (0,0).
 * - Vehicle keeps track of 3 directions:
 *     Direction | Meaning
 *     --------- | -------
 *     face      | where it's facing currently
 *     move      | where it's moving, it's different from face if it's skidding
 *     turn_dir  | where it will turn at next move, if it won't stop due to collision
 * - Some methods take `part` or `p` parameter. This is the index of a part in
 *   the parts list.
 * - Driver doesn't know what vehicle he drives.
 *   There's only player::in_vehicle flag which
 *   indicates that he is inside vehicle. To figure
 *   out what, you need to ask a map if there's a vehicle
 *   at driver/passenger position.
 * - To keep info consistent, always use
 *   `map::board_vehicle()` and `map::unboard_vehicle()` for
 *   boarding/unboarding player.
 * - To add new predesigned vehicle, add an entry to data/raw/vehicles.json
 *   similar to the existing ones. Keep in mind, that positive x coordinate points
 *   forwards, negative x is back, positive y is to the right, and
 *   negative y to the left:
 *
 *       orthogonal dir left (-Y)
 *            ^
 *       -X ------->  +X (forward)
 *            v
 *       orthogonal dir right (+Y)
 *
 *   When adding parts, function checks possibility to install part at given
 *   coordinates. If it shows debug messages that it can't add parts, when you start
 *   the game, you did something wrong.
 *   There are a few rules:
 *   1. Every mount point (tile) must begin with a part in the 'structure'
 *      location, usually a frame.
 *   2. No part can stack with itself.
 *   3. No part can stack with another part in the same location, unless that
 *      part is so small as to have no particular location (such as headlights).
 *   If you can't understand why installation fails, try to assemble your
 *   vehicle in game first.
 */
class vehicle {
private:
    auto has_structural_part(const tripoint_mnt_veh& dp) const -> bool;
    auto has_structural_or_extendable_part(const tripoint_mnt_veh& dp) const -> bool;
    auto is_structural_part_removed() const -> bool;
    void open_or_close(int part_index, bool opening);
    auto is_connected(
        const vehicle_part& to, const vehicle_part& from, const vehicle_part& excluded) const
        -> bool;
    void add_missing_frames();
    void add_steerable_wheels();

    // direct damage to part (armor protection and internals are not counted)
    // returns damage bypassed
    auto damage_direct(int p, int dmg, damage_type type = DT_TRUE) -> int;
    // Removes the part, breaks it into pieces and possibly removes parts attached to it
    auto break_off(int p, int dmg) -> int;
    // Returns if it did actually explode
    auto explode_fuel(int p, damage_type type) -> bool;
    auto get_controls_and_security() const -> std::pair<int, int>;
    // damages vehicle controls and security system
    void smash_security_system();
    // get vpart powerinfo for part number, accounting for variable-sized parts and hps.
    auto part_vpower_w(int index, bool at_full_hp = false) const -> int;

    // get vpart epowerinfo for part number.
    auto part_epower_w(int index) const -> int;

    // convert watts over time to battery energy
    auto power_to_energy_bat(int power_w, const time_duration& d) const -> int;

    // convert vhp to watts.
    static auto vhp_to_watts(int power) -> int;

    // Refresh all caches and re-locate all parts
    void refresh();

    // Do stuff like clean up blood and produce smoke from broken parts. Returns false if nothing
    // needs doing.
    auto do_environmental_effects(const int turns = 1) -> bool;

    auto total_folded_volume() const -> units::volume;

    // Vehicle fuel indicator (by fuel)
    void print_fuel_indicator(
        const catacurses::window& w, point p, const itype_id& fuel_type, bool verbose = false,
        bool desc = false);
    void print_fuel_indicator(
        const catacurses::window& w, point p, const itype_id& fuel_type,
        std::map<itype_id, float> fuel_usages, bool verbose = false, bool desc = false);

    // Calculate how long it takes to attempt to start an engine
    auto engine_start_time(int e) const -> int;

    // How much does the temperature effect the engine starting (0.0 - 1.0)
    auto engine_cold_factor(int e) const -> double;

    // refresh pivot_cache, clear pivot_dirty
    void refresh_pivot() const;

    void refresh_mass() const;
    void calc_mass_center(bool precalc) const;

    /** empty the contents of a tank, battery or turret spilling liquids randomly on the ground */
    void leak_fuel(vehicle_part& pt);

    int next_hack_id = 0;

public:
    auto find_part_hack(int id) -> vehicle_part*;
    auto find_part_hack(int id) const -> const vehicle_part*;
    auto get_part_id_hack(int id) const -> int;
    void refresh_locations_hack();

    auto get_next_hack_id() -> int { return next_hack_id++; }

    /**
     * Find a possibly off-map vehicle. If necessary, loads up its submap and pulls
     * it from there. For this reason, you should only give it the coordinates of the
     * origin tile of a target vehicle.
     *
     * The overload without @p mbuf uses the currently bound map's dimension and is
     * only correct when that dimension matches the target vehicle's dimension.
     * Prefer the mapbuffer overload when the caller has explicit dimension context.
     *
     * @param where  Location of the other vehicle's origin tile (absolute ms coords).
     * @param mbuf   Mapbuffer for the dimension that owns the target vehicle.
     */
    static auto find_vehicle(const tripoint_abs_ms& where) -> vehicle*;
    static auto find_vehicle(const tripoint_abs_ms& where, mapbuffer& mbuf) -> vehicle*;

    vehicle(const vproto_id& type_id, int init_veh_fuel = -1, int init_veh_status = -1,
            std::optional<bool> locked = std::nullopt, std::optional<bool> has_keys = std::nullopt);
    vehicle();
    ~vehicle();

private:
    void copy_static_from(const vehicle&);
    vehicle(const vehicle&) = delete;
    vehicle(vehicle&&) = delete;
    auto operator=(vehicle&&) -> vehicle& = delete;
    auto operator=(const vehicle&) -> vehicle& = delete;

public:
    /** Disable or enable refresh() ; used to speed up performance when creating a vehicle */
    void suspend_refresh();
    void enable_refresh();

    void attach() { attached = true; }

    void detach() { attached = false; }

    auto is_loaded() const -> bool;

    /**
     * Set stat for part constrained by range [0,durability]
     * @note does not invoke base @ref item::on_damage callback
     */
    void set_hp(vehicle_part& pt, int qty);

    /**
     * Apply damage to part constrained by range [0,durability] possibly destroying it
     * @param pt Part being damaged
     * @param qty maximum amount by which to adjust damage (negative permissible)
     * @param dt type of damage which may be passed to base @ref item::on_damage callback
     * @return whether part was destroyed as a result of the damage
     */
    auto mod_hp(vehicle_part& pt, int qty, damage_type dt = DT_NULL) -> bool;

    // check if given player controls this vehicle
    auto player_in_control(const Character& who) const -> bool;
    // check if player controls this vehicle remotely
    auto remote_controlled(const Character& who) const -> bool;

    // init parts state for randomly generated vehicle
    void init_state(
        int init_veh_fuel, int init_veh_status, std::optional<bool> locked,
        std::optional<bool> has_keys);

    // damages all parts of a vehicle by a random amount
    void smash(
        map& m, float hp_percent_loss_min = 0.1f, float hp_percent_loss_max = 1.2f,
        float percent_of_parts_to_affect = 1.0f,
        tripoint_rel_ms damage_origin = tripoint_rel_ms::zero(), float damage_size = 0);
    auto smash(
        mapgen_constructor& m, float hp_percent_loss_min = 0.1f, float hp_percent_loss_max = 1.2f,
        float percent_of_parts_to_affect = 1.0f,
        tripoint_rel_ms damage_origin = tripoint_rel_ms::zero(), float damage_size = 0) -> void;

    void serialize(JsonOut& json) const;
    void deserialize(JsonIn& jsin);
    // Vehicle parts list - all the parts on a single tile
    auto print_part_list(
        const catacurses::window& win, int y1, int max_y, int width, int p, int hl = -1,
        bool detail = false, int start_at = 0) const -> int;

    // Vehicle parts descriptions - descriptions for all the parts on a single tile
    void print_vparts_descs(
        const catacurses::window& win, int max_y, int width, int p, int& start_at,
        int& start_limit) const;
    // towing functions
    void invalidate_towing(bool first_vehicle = false);
    void do_towing_move();
    auto tow_cable_too_far() const -> bool;
    auto no_towing_slack() const -> bool;
    auto is_towing() const -> bool;
    auto has_tow_attached() const -> bool;
    auto get_tow_part() const -> int;
    auto is_external_part(const tripoint_bub_ms& part_pt) const -> bool;
    auto is_towed() const -> bool;
    void set_tow_directions();
    // owner functions
    auto is_owned_by(const Character& c, bool available_to_take = false) const -> bool;
    auto is_old_owner(const Character& c, bool available_to_take = false) const -> bool;
    auto get_owner_name() const -> std::string;
    void set_old_owner(const faction_id& temp_owner);
    void remove_old_owner();
    void set_owner(const faction_id& new_owner);
    void set_owner(const Character& c);
    void remove_owner();
    auto get_owner() const -> faction_id;
    auto get_old_owner() const -> faction_id;
    auto has_owner() const -> bool;
    auto has_old_owner() const -> bool;
    /**
     * Handle potential vehicle theft.
     * @param you Avatar to check against
     * @param check_only If true, won't prompt to steal and instead will
     * refure to interact
     * @param prompt Whether to prompt confirmation or proceed with the
     * theft without prompt
     * @return whether the avatar is willing to interact with the vehicle
     */
    auto handle_potential_theft(avatar& you, bool check_only = false, bool prompt = true) -> bool;
    // project a tileray forward to predict obstacles
    auto immediate_path(units::angle rotate = 0_degrees) -> std::set<point_abs_ms>;
    // This would require a proper rework to make 3D. Stays 2D for now.
    std::set<point_abs_ms> collision_check_points;
    void autopilot_patrol();
    auto get_angle_from_targ(const tripoint_abs_ms& targ) -> units::angle;
    void drive_to_local_target(const tripoint_abs_ms& target, bool follow_protocol);
    auto get_autodrive_target() -> tripoint_abs_ms;
    // Drive automatically towards some destination for one turn.
    auto do_autodrive(Character& driver) -> autodrive_result;
    // Stop any kind of automatic vehicle control and apply the brakes.
    void stop_autodriving(bool apply_brakes = true);
    /**
     *  Operate vehicle controls
     *  @param pos location of physical controls to operate (ignored during remote operation)
     */
    void use_controls(const tripoint_bub_ms& pos);

    // Fold up the vehicle
    auto fold_up() -> bool;

    // Attempt to start an engine
    auto start_engine(int e) -> bool;
    // stop all engines
    void stop_engines();
    // Attempt to start the vehicle's active engines
    void start_engines(bool take_control = false, bool autodrive = false);

    // Engine backfire, making a loud noise
    void backfire(int e) const;

    // get vpart type info for part number (part at given vector index)
    auto part_info(int index, bool include_removed = false) const -> const vpart_info&;

    // check if certain part can be mounted at certain position (not accounting frame direction)
    auto can_mount(const tripoint_mnt_veh& dp, const vpart_id& id) const -> bool;

    // check if certain part can be unmounted
    auto can_unmount(int p) const -> bool;
    auto can_unmount(int p, std::string& reason) const -> bool;

    // install a new part to vehicle
    auto install_part(const tripoint_mnt_veh& dp, const vpart_id& id, bool force = false) -> int;

    // Install a copy of the given part, skips possibility check
    auto install_part(const tripoint_mnt_veh& dp, vehicle_part&& part) -> int;

    /** install item specified item to vehicle as a vehicle part */
    auto install_part(
        const tripoint_mnt_veh& dp, const vpart_id& id, detached_ptr<item>&& obj,
        bool force = false) -> int;

    // find a single tile wide vehicle adjacent to a list of part indices
    auto try_to_rack_nearby_vehicle(const std::vector<std::vector<int>>& list_of_racks) -> bool;
    // merge a previously found single tile vehicle into this vehicle
    auto merge_rackable_vehicle(vehicle* carry_veh, const std::vector<int>& rack_parts) -> bool;

    /**
     * @param handler A class that receives various callbacks, e.g. for placing items.
     * This handler is different when called during mapgen (when items need to be placed
     * on the temporary mapgen map), and when called during normal game play (when items
     * go on the main map g->m).
     */
    auto remove_part(int p, RemovePartHandler& handler) -> bool;
    auto remove_part(int p) -> bool;
    void part_removal_cleanup();

    // remove the carried flag from a vehicle after it has been removed from a rack
    void remove_carried_flag();
    // remove the tracked flag from a tracked vehicle after it has been removed from a rack
    void remove_tracked_flag();
    // remove a vehicle specified by a list of part indices
    auto remove_carried_vehicle(const std::vector<int>& carried_parts) -> bool;
    // split the current vehicle into up to four vehicles if they have no connection other
    // than the structure part at exclude
    auto find_and_split_vehicles(int exclude) -> bool;
    // relocate passengers to the same part on a new vehicle
    void relocate_passengers(const std::vector<Character*>& passengers);
    // remove a bunch of parts, specified by a vector indices, and move them to a new vehicle at
    // the same global position
    // optionally specify the new vehicle position and the mount points on the new vehicle
    auto split_vehicles(
        const std::vector<std::vector<int>>& new_vehs, const std::vector<vehicle*>& new_vehicles,
        const std::vector<std::vector<tripoint_mnt_veh>>& new_mounts) -> bool;
    auto split_vehicles(const std::vector<std::vector<int>>& new_veh) -> bool;

    /** Get handle for base item of part */
    auto part_base(int p) -> item&;

    /** Get index of part with matching base item or INT_MIN if not found */
    auto find_part(const item& it) const -> int;

    /**
     * Remove a part from a targeted remote vehicle. Useful for, e.g. power cables that have
     * a vehicle part on both sides.
     */
    void remove_remote_part(int part_num);
    /**
     * Yields a range containing all parts (including broken ones) that can be
     * iterated over.
     */
    // TODO: maybe not include broken ones? Have a separate function for that?
    // TODO: rename to just `parts()` and rename the data member to `parts_`.
    auto get_all_parts() const -> vehicle_part_range;
    /**
     * Yields a range of parts of this vehicle that each have the given feature
     * and are available: not broken, removed, or part of a carried vehicle.
     * The enabled status of the part is ignored.
     */
    /**@{*/
    auto get_avail_parts(std::string feature) const -> vehicle_part_with_feature_range<std::string>;
    auto get_avail_parts(vpart_bitflags f) const -> vehicle_part_with_feature_range<vpart_bitflags>;
    /**@}*/
    /**
     * Yields a range of parts of this vehicle that each have the given feature
     * and are not broken or removed.
     * The enabled status of the part is ignored.
     */
    /**@{*/
    auto get_parts_including_carried(std::string feature) const
        -> vehicle_part_with_feature_range<std::string>;
    auto get_parts_including_carried(vpart_bitflags f) const
        -> vehicle_part_with_feature_range<vpart_bitflags>;
    /**@}*/
    /**
     * Yields a range of parts of this vehicle that each have the given feature and not removed.
     * The enabled status of the part is ignored.
     */
    /**@{*/
    auto get_any_parts(std::string feature) const -> vehicle_part_with_feature_range<std::string>;
    auto get_any_parts(vpart_bitflags f) const -> vehicle_part_with_feature_range<vpart_bitflags>;
    /**@}*/
    /**
     * Yields a range of parts of this vehicle that each have the given feature
     * and are enabled and available: not broken, removed, or part of a carried vehicle.
     */
    /**@{*/
    auto get_enabled_parts(std::string feature) const
        -> vehicle_part_with_feature_range<std::string>;
    auto get_enabled_parts(vpart_bitflags f) const
        -> vehicle_part_with_feature_range<vpart_bitflags>;
    /**@}*/

    // returns the list of indices of parts at certain position (not accounting frame direction)
    auto parts_at_relative(const tripoint_mnt_veh& dp, bool use_cache) const -> std::vector<int>;

    // returns index of part, inner to given, with certain flag, or -1
    auto part_with_feature(int p, const std::string& f, bool unbroken) const -> int;
    auto part_with_feature(const tripoint_mnt_veh& pt, const std::string& f, bool unbroken) const
        -> int;
    auto part_with_feature(int p, vpart_bitflags f, bool unbroken) const -> int;

    // returns index of part, inner to given, with certain flag, or -1
    auto avail_part_with_feature(int p, const std::string& f, bool unbroken) const -> int;
    auto avail_part_with_feature(
        const tripoint_mnt_veh& pt, const std::string& f, bool unbroken) const -> int;
    auto avail_part_with_feature(int p, vpart_bitflags f, bool unbroken) const -> int;

    auto obstacle_at_position(const tripoint_mnt_veh& pos) const -> int;
    auto opaque_at_position(const tripoint_mnt_veh& pos) const -> int;

    /**
     *  Check if vehicle has at least one unbroken part with specified flag
     *  @param flag Specified flag to search parts for
     *  @param enabled if set part must also be enabled to be considered
     *  @returns true if part is found
     */
    auto has_part(const std::string& flag, bool enabled = false) const -> bool;
    /**
     *  Check if vehicle has at least one unbroken part with specified flag
     *  @param flag Specified flag to search parts for
     *  @param enabled if set part must also be enabled to be considered
     *  @returns true if part is found
     */
    auto has_part(const vpart_bitflags& flag, bool enabled = false) const -> bool;

    /**
     *  Check if vehicle has at least one unbroken part with specified flag
     *  @param pos limit check for parts to this global position
     *  @param flag The specified flag
     *  @param enabled if set part must also be enabled to be considered
     */
    auto has_part(const tripoint_bub_ms& pos, const std::string& flag, bool enabled = false) const
        -> bool;

    /**
     *  Get all enabled, available, unbroken vehicle parts at specified position
     *  @param pos position to check
     *  @param flag if set only flags with this part will be considered
     *  @param condition enum to include unabled, unavailable, and broken parts
     */
    auto get_parts_at(
        const tripoint_bub_ms& pos, const std::string& flag, part_status_flag condition)
        -> std::vector<vehicle_part*>;
    auto get_parts_at(
        const tripoint_bub_ms& pos, const std::string& flag, part_status_flag condition) const
        -> std::vector<const vehicle_part*>;

    /** Test if part can be enabled (unbroken, sufficient fuel etc), optionally displaying failures
     * to user */
    auto can_enable(const vehicle_part& pt, bool alert = false) const -> bool;

    /**
     *  Return the index of the next part to open at `p`'s location
     *
     *  The next part to open is the first unopened part in the reversed list of
     *  parts at part `p`'s coordinates.
     *
     *  @param p Part who's coordinates provide the location to check
     *  @param outside If true, give parts that can be opened from outside only
     *  @return part index or -1 if no part
     */
    auto next_part_to_open(int p, bool outside = false) const -> int;

    /**
     *  Return the index of the next part to close at `p`
     *
     *  The next part to open is the first opened part in the list of
     *  parts at part `p`'s coordinates. Returns -1 for no more to close.
     *
     *  @param p Part who's coordinates provide the location to check
     *  @param outside If true, give parts that can be closed from outside only
     *  @return part index or -1 if no part
     */
    auto next_part_to_close(int p, bool outside = false) const -> int;
    auto all_standalone_parts() const -> std::vector<int>;
    // returns indices of all parts in the given location slot
    auto all_parts_at_location(const std::string& location) const -> std::vector<int>;
    // shifts an index to next available of that type for NPC activities
    auto get_next_shifted_index(int original_index, Character& who) -> int;
    // Given a part and a flag, returns the indices of all contiguously adjacent parts
    // with the same flag on the X and Y Axis
    auto find_lines_of_parts(int part, const std::string& flag) -> std::vector<std::vector<int>>;

    // returns true if given flag is present for given part index
    auto part_flag(int p, const std::string& f) const -> bool;
    auto part_flag(int p, vpart_bitflags f) const -> bool;

    // Translate mount coordinates "p" using current pivot direction and anchor and return tile
    // coordinates
    auto coord_translate(const tripoint_mnt_veh& p) const -> tripoint_rel_ms;

    // Translate mount coordinates "p" into tile coordinates "q" using given pivot direction and
    // anchor
    void coord_translate(
        units::angle dir, const tripoint_mnt_veh& pivot, const tripoint_mnt_veh& p,
        point_rel_ms& q) const;

    // Translate rotated tile coordinates "p" into mount coordinates "q" using given pivot direction
    // and anchor
    void coord_translate_reverse(
        units::angle dir, const tripoint_mnt_veh& pivot, const tripoint_rel_ms& p,
        tripoint_mnt_veh& q) const;

    auto mount_to_bubble(const tripoint_mnt_veh& mount) const -> tripoint_bub_ms;
    auto mount_to_bubble(const tripoint_mnt_veh& mount, const tripoint_rel_veh& offset) const
        -> tripoint_bub_ms;

    // Translate tile coordinates into mount coordinates
    auto bubble_to_mount(const tripoint_bub_ms& p) const -> tripoint_mnt_veh;

    auto mount_to_abs(const tripoint_mnt_veh& mount) const -> tripoint_abs_ms;
    auto mount_to_abs(const tripoint_mnt_veh& mount, const tripoint_rel_veh& offset) const
        -> tripoint_abs_ms {
        return mount_to_abs(mount + offset);
    }
    auto abs_to_mount(const tripoint_abs_ms& abs) const -> tripoint_mnt_veh;

    // Seek a vehicle part which obstructs tile with given coordinates relative to vehicle position
    auto part_at(const tripoint_rel_ms& dp) const -> int;
    auto part_displayed_at(const tripoint_mnt_veh& dp) const -> int;
    auto roof_at_part(int p) const -> int;

    // Given a part, finds its index in the vehicle
    auto index_of_part(const vehicle_part* part, bool check_removed = false) const -> int;

    // get symbol for map
    auto part_sym(int p, bool exact = false) const -> char;
    auto part_id_string(int p, bool roof, char& part_mod) const -> vpart_id;
    auto part_display_direction(int p, bool roof = false) const -> units::angle;

    // get color for map
    auto part_color(int p, bool exact = false) const -> nc_color;

    // get text and color of damage summary (e.g. "like new" or "battered")
    auto vehicle_damage_summary() const -> std::pair<std::string, nc_color>;

    // Get all printable fuel types
    auto get_printable_fuel_types() const -> std::vector<itype_id>;

    // Vehicle fuel indicators (all of them)
    void print_fuel_indicators(
        const catacurses::window& win, point, int start_index = 0, bool fullsize = false,
        bool verbose = false, bool desc = false, bool isHorizontal = false);

    // Refresh part locations
    void refresh_position();

    // Pre-calculate mount points for (idir=0) - current direction or (idir=1) - next turn direction
    void precalc_mounts(int idir, units::angle dir, const tripoint_mnt_veh& pivot);

    // get a list of part indices where is a passenger inside
    auto boarded_parts() const -> std::vector<int>;

    // get a list of part indices and Creature pointers with a rider
    auto get_riders() const -> std::vector<rider_data>;

    // get passenger at part p
    auto get_passenger(int p) const -> player*;
    // get monster on a boardable part at p
    auto get_pet(int p) const -> monster*;

    auto enclosed_at(const tripoint_bub_ms& pos) -> bool; // not const because it calls
                                                          // refresh_insides
    // Returns the location of the vehicle in global map square coordinates.
    auto abs_ms_location() const -> tripoint_abs_ms;
    // Returns the coordinates (in map squares) of the vehicle relative to the local map.
    auto bub_ms_location() const -> tripoint_bub_ms;
    /**
     * Get the coordinates of the studied part of the vehicle
     */
    auto bub_part_location(const int& index) const -> tripoint_bub_ms;
    auto bub_part_location(const vehicle_part& pt) const -> tripoint_bub_ms;
    auto abs_part_location(const int& index) const -> tripoint_abs_ms;
    auto abs_part_location(const vehicle_part& pt) const -> tripoint_abs_ms;
    /**
     * All the fuels that are in all the tanks in the vehicle, nicely summed up.
     * Note that empty tanks don't count at all. The value is the amount as it would be
     * reported by @ref fuel_left, it is always greater than 0. The key is the fuel item type.
     */
    auto fuels_left() const -> std::map<itype_id, int>;

    // Checks how much certain fuel left in tanks.
    auto fuel_left(const itype_id& ftype, bool recurse = false) const -> int;
    // Checks how much of the part p's current fuel is left
    auto fuel_left(int p, bool recurse = false) const -> int;
    // Checks how much of an engine's current fuel is left in the tanks.
    auto engine_fuel_left(int e, bool recurse = false) const -> int;
    auto fuel_capacity(const itype_id& ftype) const -> int;

    // drains a fuel type (e.g. for the kitchen unit)
    // returns amount actually drained, does not engage reactor
    auto drain(const itype_id& ftype, int amount) -> int;
    auto drain(int index, int amount) -> int;
    /**
     * Consumes enough fuel by energy content. Does not support cable draining.
     * @param ftype Type of fuel
     * @param energy_j Desired amount of energy of fuel to consume
     * @return Amount of energy actually consumed. May be more or less than energy.
     */
    auto drain_energy(const itype_id& ftype, double energy_j) -> double;

    // fuel consumption of vehicle engines of given type
    auto basic_consumption(const itype_id& ftype) const -> int;
    auto consumption_per_hour(const itype_id& ftype, int fuel_rate) const -> int;

    void consume_fuel(int load, int t_seconds = 6, bool skip_electric = false);

    /**
     * Maps used fuel to its basic (unscaled by load/strain) consumption.
     */
    auto fuel_usage() const -> std::map<itype_id, int>;

    /**
     * Get all vehicle lights (excluding any that are destroyed)
     * @param active if true return only lights which are enabled
     */
    auto lights(bool active = false) -> std::vector<vehicle_part*>;

    void update_alternator_load();

    // Total drain or production of electrical power from engines.
    auto total_engine_epower_w() const -> int;
    // Total production of electrical power from alternators.
    auto total_alternator_epower_w() const -> int;
    // Total power currently being produced by all solar panels.
    auto total_solar_epower_w() const -> int;
    // Total power currently being produced by all wind turbines.
    auto total_wind_epower_w() const -> int;
    // Total power currently being produced by all water wheels.
    auto total_water_wheel_epower_w() const -> int;
    // Total power drain across all vehicle accessories.
    auto total_accessory_epower_w() const -> int;
    // Net power draw or drain on batteries.
    auto net_battery_charge_rate_w() const -> int;
    // Maximum available power available from all reactors. Power from
    // reactors is only drawn when batteries are empty.
    auto max_reactor_epower_w() const -> int;
    // Produce and consume electrical power, with excess power stored or
    // taken from batteries.
    void power_parts(const int turns = 1);

    /**
     * Try to charge our (and, optionally, connected vehicles') batteries by the given amount.
     * @return amount of charge left over.
     */
    auto charge_battery(int amount, bool include_other_vehicles = true) -> int;

    /**
     * Try to discharge our (and, optionally, connected vehicles') batteries by the given amount.
     * @return amount of request unfulfilled (0 if totally successful).
     */
    auto discharge_battery(int amount, bool recurse = true) -> int;

    /**
     * Mark mass caches and pivot cache as dirty
     */
    void invalidate_mass();

    // Converts angles into turning increments
    static auto angle_to_increment(units::angle dir) -> int;

    // get the total mass of vehicle, including cargo and passengers
    auto total_mass() const -> units::mass;

    // Gets the center of mass calculated for precalc[0] coordinates
    auto rotated_center_of_mass() const -> tripoint_mnt_veh;
    // Gets the center of mass calculated for mount point coordinates
    auto local_center_of_mass() const -> tripoint_mnt_veh;

    // Get the pivot point of vehicle; coordinates are unrotated mount coordinates.
    // This may result in refreshing the pivot point if it is currently stale.
    auto pivot_point() const -> tripoint_mnt_veh;

    // Get the (artificial) displacement of the vehicle due to the pivot point changing
    // between precalc[0] and precalc[1]. This needs to be subtracted from any actual
    // vehicle motion after precalc[1] is prepared.
    auto pivot_displacement() const -> tripoint_rel_ms;

    // Get combined power of all engines, the ideal amount of power, not the current power
    auto ideal_engine_power(bool safe = false) const -> int;
    // Get combined power of all engines. If fueled == true, then only engines which
    // vehicle have fuel for are accounted.  If safe == true, then limit engine power to
    // their safe power.
    auto total_power_w(bool fueled = true, bool safe = false) const -> int;

    // Get ground acceleration gained by combined power of all engines. If fueled == true,
    // then only engines which the vehicle has fuel for are included
    auto ground_acceleration(bool fueled = true, int at_vel_in_vmi = -1, bool ideal = false) const
        -> int;
    // Get water acceleration gained by combined power of all engines. If fueled == true,
    // then only engines which the vehicle has fuel for are included
    auto water_acceleration(bool fueled = true, int at_vel_in_vmi = -1, bool ideal = false) const
        -> int;
    // get air acceleration gained by combined power of all engines. If fueled == true,
    // then only engines which the vehicle hs fuel for are included
    auto aircraft_acceleration(bool fueled = true, int at_vel_in_vmi = -1, bool ideal = false) const
        -> int;

    // Get acceleration for the current movement mode
    auto acceleration(bool fueled = true, int at_vel_in_vmi = -1) const -> int;

    // Get the vehicle's actual current acceleration
    auto current_acceleration(bool fueled = true) const -> int;

    // is the vehicle currently moving?
    auto is_moving() const -> bool;

    // can the vehicle use rails?
    auto can_use_rails() const -> bool;

    // Get maximum ground velocity gained by combined power of all engines.
    // If fueled == true, then only the engines which the vehicle has fuel for are included
    auto max_ground_velocity(bool fueled = true, bool ideal = false) const -> int;
    // Get maximum water velocity gained by combined power of all engines.
    // If fueled == true, then only the engines which the vehicle has fuel for are included
    auto max_water_velocity(bool fueled = true, bool ideal = false) const -> int;
    // get maximum air velocity based on rotor physics
    auto max_air_velocity(bool fueled = true, bool ideal = false) const -> int;
    // Get maximum velocity for the current movement mode
    auto max_velocity(bool fueled = true, bool ideal = false) const -> int;
    // Get maximum reverse velocity for the current movement mode
    auto max_reverse_velocity(bool fueled = true, bool ideal = false) const -> int;

    // Get safe ground velocity gained by combined power of all engines.
    // If fueled == true, then only the engines which the vehicle has fuel for are included
    auto safe_ground_velocity(bool fueled = true, bool ideal = false) const -> int;
    // get safe air velocity gained by combined power of all engines.
    // if fueled == true, then only the engines which the vehicle hs fuel for are included
    auto safe_aircraft_velocity(bool fueled = true, bool ideal = false) const -> int;
    // Get safe water velocity gained by combined power of all engines.
    // If fueled == true, then only the engines which the vehicle has fuel for are included
    auto safe_water_velocity(bool fueled = true, bool ideal = false) const -> int;
    // Get maximum velocity for the current movement mode
    auto safe_velocity(bool fueled = true) const -> int;

    // Generate field from a part, either at front or back of vehicle depending on velocity.
    void spew_field(double joules, int part, field_type_id type, int intensity = 1);

    // Loop through engines and generate noise and smoke for each one
    void noise_and_smoke(int load, time_duration time = 1_turns);

    /**
     * Calculates the sum of the area under the wheels of the vehicle.
     */
    auto wheel_area() const -> int;
    // average off-road rating for displaying off-road performance
    auto average_or_rating() const -> float;

    /**
     * Physical coefficients used for vehicle calculations.
     */
    /*@{*/
    /**
     * coefficient of air drag in kg/m
     * multiplied by the square of speed to calculate air drag force in N
     * proportional to cross sectional area of the vehicle, times the density of air,
     * times a dimensional constant based on the vehicle's shape
     */
    auto coeff_air_drag() const -> double;

    /**
     * coefficient of airship balloon drag
     */
    auto coeff_balloon_drag() const -> double;

    /**
     * coefficient of rolling resistance
     * multiplied by velocity to get the variable part of rolling resistance drag in N
     * multiplied by a constant to get the constant part of rolling resistance drag in N
     * depends on wheel design, wheel number, and vehicle weight
     */
    auto coeff_rolling_drag() const -> double;

    /**
     * coefficient of water drag in kg/m
     * multiplied by the square of speed to calculate water drag force in N
     * proportional to cross sectional area of the vehicle, times the density of water,
     * times a dimensional constant based on the vehicle's shape
     */
    auto coeff_water_drag() const -> double;

    /**
     * maximum possible buoyancy in Newtons.
     *
     * buoyancy force = V * D * g
     *
     * V: total volume of the vehicle (because it's maximally submerged)
     * D: density of submerged fluid (in our case, water)
     * g: force of gravity
     *
     * @return The max buoyancy in Newtons.
     */
    auto max_buoyancy() const -> double;

    /**
     * watertight hull height in meters measures distance from bottom of vehicle
     * to the point where the vehicle will start taking on water
     */
    auto water_hull_height() const -> double;

    /**
     * water draft in meters - how much of the vehicle's body is under water
     * must be less than the hull height or the boat will sink
     * at some point, also add boats with deep draft running around
     */
    auto water_draft() const -> double;

    /**
     * can_float
     * does the vehicle have freeboard or does it overflow with water?
     */
    auto can_float() const -> bool;
    /**
     * is the vehicle mostly in water or mostly on fairly dry land?
     */
    auto is_in_water(bool deep_water = false) const -> bool;
    auto is_watercraft() const -> bool;
    /**
     * is the vehicle flying? is it an aircraft?
     */
    auto is_aircraft() const -> bool;
    /**
     * does the vehicle use rotors?
     */
    auto is_rotorcraft() const -> bool;
    /**
     * does the vehicle have lift-generating parts?
     */
    auto has_lift() const -> bool;
    /**
     * total area of every rotors in m^2
     */
    auto total_rotor_area() const -> double;
    /**
     * lift of balloons in newtons
     */
    auto total_balloon_lift() const -> double;
    /**
     * lift from wings in newtons
     */
    auto total_wing_lift() const -> double;
    /**
     * speed needed for aircraft takeoff
     */
    auto get_takeoff_speed(std::string speed_type = "default") const -> int;
    /**
     * total area of every propeller in m^2
     */
    auto total_propeller_area() const -> double;
    /**
     * lift of rotorcraft in newton
     */
    auto thrust_of_rotorcraft(bool fuelled, bool safe = false, bool ideal = false) const -> double;
    /**
     * foward thrust of propellers in newtons
     */
    auto foward_thrust_of_propellers(bool fuelled, bool safe = false, bool ideal = false) const
        -> double;
    /**
     * total foward thrust of all airborn pushers
     */
    auto total_thrust(bool fuelled, bool safe = false, bool ideal = false) const -> double;
    /**
     * total lift of all lifters
     */
    auto total_lift(
        bool fuelled, bool safe = false, bool ideal = false, bool unpowered = false,
        bool idle = false) const -> double;
    auto has_sufficient_lift(bool unpowered = false, bool idle = false) const -> bool;
    auto get_lift_percent(bool unpowered = false) const -> double;
    auto get_z_change() const -> int;
    auto is_flying_in_air() const -> bool;
    void set_flying(bool new_flying_value);
    /**
     * Traction coefficient of the vehicle.
     * 1.0 on road. Outside roads, depends on mass divided by wheel area
     * and the surface beneath wheels.
     *
     * Affects safe velocity, acceleration and handling difficulty.
     */
    auto k_traction(float wheel_traction_area) const -> float;
    /*@}*/

    // Extra drag on the vehicle from components other than wheels.
    // @param actual is current drag if true or nominal drag otherwise
    auto static_drag(bool actual = true) const -> int;

    // strain of engine(s) if it works higher that safe speed (0-1.0)
    auto strain() const -> float;

    // Calculate if it can move using its wheels
    auto sufficient_wheel_config() const -> bool;
    auto balanced_wheel_config() const -> bool;
    auto valid_wheel_config() const -> bool;

    // return the relative effectiveness of the steering (1.0 is normal)
    // <0 means there is no steering installed at all.
    auto steering_effectiveness() const -> float;

    /** Returns roughly driving skill level at which there is no chance of fumbling. */
    auto handling_difficulty() const -> float;

    /**
     * Use grid traversal to enumerate all connected vehicles.
     * @param connected_vehicles is an output map from vehicle pointers to
     * a bool that is true if the vehicle is in the reality bubble.
     * @param vehicle_list is a set of pointers to vehicles present in the reality bubble.
     */
    static void enumerate_vehicles(
        std::map<vehicle*, bool>& connected_vehicles, const std::set<vehicle*>& vehicle_list);
    // idle fuel consumption
    void idle(bool on_map = true);
    // idle fuel consumption in bulk
    // Called by update_time given the batched parameter
    void idle_turns(const int turns);
    // continuous processing for running vehicle alarms
    void alarm();
    // leak from broken tanks
    void slow_leak();

    // checks if we are, or will be after movement, on a ramp
    auto check_on_ramp(int idir = 0, const tripoint_rel_ms& offset = tripoint_rel_ms::zero()) const
        -> bool;

    // calculates the precalc zlevels wrt ramps
    void adjust_zlevel(int idir = 0, const tripoint_rel_ms& offset = tripoint_rel_ms::zero());

    // thrust (1) or brake (-1) vehicle
    // @param z = z thrust for helicopters etc
    void thrust(int thd, int z = 0);

    // deceleration due to ground friction and air resistance
    auto slowdown(int velocity) const -> int;

    // depending on skid vectors, chance to recover.
    void possibly_recover_from_skid();

    // forward component of velocity.
    auto forward_velocity() const -> float;

    // cruise control
    void cruise_thrust(int amount);

    // turn vehicle left (negative) or right (positive), degrees
    void turn(units::angle deg);

    void set_facing(units::angle deg, bool refresh = true) {
        turn_dir = deg;
        face.init(deg);
        pivot_rotation[0] = deg;
        if (refresh) { refresh_position(); }
    }

    void set_pivot(const tripoint_mnt_veh& pivot, bool refresh = true) {
        pivot_cache = pivot;
        pivot_anchor[0] = pivot;
        if (refresh) { refresh_position(); }
    }

    void set_facing_and_pivot(units::angle deg, tripoint_mnt_veh pivot, bool refresh = true) {
        set_facing(deg, false);
        set_pivot(pivot, refresh);
    }

    // Returns if any collision occurred
    auto collision(const vehicle_collision_options& options) -> bool;

    // Handle given part collision with vehicle, monster/NPC/player or terrain obstacle
    // Returns collision, which has type, impulse, part, & target.
    auto part_collision(const vehicle_part_collision_options& options) -> veh_collision;

    // Process the trap beneath
    void handle_trap(const tripoint_bub_ms& p, int part);
    void activate_magical_follow();
    void activate_animal_follow();
    /**
     * vehicle is driving itself
     */
    void selfdrive(point);
    /**
     * can the helicopter descend/ascend here?
     */
    auto check_heli_descend(Character& who) -> bool;
    auto check_heli_ascend(Character& who) -> bool;
    auto check_is_heli_landed() -> bool;
    /**
     * Player is driving the vehicle
     * @param p direction player is steering
     * @param z for vertical movement - e.g helicopters
     */
    void pldrive(Character& driver, tripoint_rel_veh p);

    // stub for per-vpart limit
    auto max_volume(int part) const -> units::volume;
    auto free_volume(int part) const -> units::volume;
    auto stored_volume(int part) const -> units::volume;

    /**
     * Remove an item from active item processing queue as necessary
     */
    void make_inactive(item& target);
    /**
     * Update an item's active status, for example when adding
     * hot or perishable liquid to a container.
     */
    void make_active(item& target);
    /// Rebuild-on-demand cache for cargo recharge candidates.
    auto get_cargo_recharge_targets() -> std::vector<cargo_recharge_target>;
    auto invalidate_cargo_recharge_cache() -> void;
    /**
     * Try to add an item to part's cargo.
     */
    auto add_item(int part, detached_ptr<item>&& itm) -> detached_ptr<item>;
    /** Like the above */
    auto add_item(vehicle_part& pt, detached_ptr<item>&& obj) -> detached_ptr<item>;

    /**
     * Add an item counted by charges to the part's cargo.
     *
     * @returns Any remaining charges that couldn't be added.
     */
    auto add_charges(int part, detached_ptr<item>&& itm) -> detached_ptr<item>;

    // remove item from part's cargo
    auto remove_item(int part, item* it) -> detached_ptr<item>;
    auto remove_item(int part, vehicle_stack::const_iterator it, detached_ptr<item>* ret = nullptr)
        -> vehicle_stack::iterator;

    auto get_items(int part) const -> vehicle_stack;
    auto get_items(int part) -> vehicle_stack;
    void dump_items_from_part(size_t index);

    // Generates starting items in the car, should only be called when placed on the map
    void place_spawn_items();

    void gain_moves();

    // if its a summoned vehicle - its gotta dissappear at some point, return true if destroyed
    auto decrement_summon_timer() -> bool;

    // reduces velocity to 0
    void stop(bool update_cache = true);

    void refresh_insides();

    void unboard_all();

    // Damage individual part. bash means damage
    // must exceed certain threshold to be subtracted from hp
    // (a lot light collisions will not destroy parts)
    // Returns damage bypassed
    auto damage(
        int p, int dmg, damage_type type = DT_BASH, bool aimed = true, bool random_part = true)
        -> int;

    // damage all parts (like shake from strong collision), range from dmg1 to dmg2
    void damage_all(int dmg1, int dmg2, damage_type type, const tripoint_mnt_veh& impact);

    // Shifts the coordinates of all parts and moves the vehicle in the opposite direction.
    void shift_parts(const tripoint_rel_veh& delta);
    auto shift_if_needed() -> bool;

    void shed_loose_parts();

    /**
     * @name Vehicle turrets
     *
     *@{*/

    /** Get all vehicle turrets (excluding any that are destroyed) */
    auto turrets() -> std::vector<vehicle_part*>;

    /** Get all vehicle turrets loaded and ready to fire at target */
    auto turrets(const tripoint_bub_ms& target) -> std::vector<vehicle_part*>;

    /** Get firing data for a turret */
    auto turret_query(vehicle_part& pt) -> turret_data;
    auto turret_query(const vehicle_part& pt) const -> turret_data;

    auto turret_query(const tripoint_abs_ms& pos) -> turret_data;
    auto turret_query(const tripoint_abs_ms& pos) const -> turret_data;

    /** Returns true if any part on the tile the turret is installed on has the MANUAL flag. */
    auto is_manual_turret(const vehicle_part& pt) const -> bool;

    /** Set targeting mode for specific turrets */
    void turrets_set_targeting();

    /** Set firing mode for specific turrets */
    void turrets_set_mode();

    /** Select a single ready turret, aim it using the aiming UI and fire. */
    void turrets_aim_and_fire_single(avatar& you);

    /*
     * Find all ready turrets, aim them using aiming UI and fire.
     * @param turret_filter Decide which filter to use (manual, automatic, both)
     * @param show_msg Show 'no such turrets found' message. Does not affect returned value.
     * @return False if there are no such turrets
     */

    auto turrets_aim_and_fire_mult(
        avatar& you, const turret_filter_types turret_filter, const bool show_msg = false) -> bool;

    /*
     * Fire turret at automatically acquired target
     * @return number of shots actually fired (which may be zero)
     */
    auto automatic_fire_turret(vehicle_part& pt) -> int;

    // How many hits of damage `dmg` and damage type `type` to part with ID `p` to destroy it is
    // needed? 0 if it will never destroy. Be aware this will not consider damage to more outside
    // parts such as inner parts protected by an outer wall, only armor effects are considered
    auto hits_to_destroy(int p, int dmg, damage_type type) const -> unsigned int;

    /**
     * @name Vehicle Droppers
     *
     *@{*/

    auto get_cargo_part(vehicle_part* part) -> vehicle_part*;

    auto has_item_stored(vehicle_part* part) -> bool;

    void item_dropper_drop(std::vector<vehicle_part*> droppers, bool single);

    void item_dropper_drop_all();

    void item_dropper_drop_single(bool single);

private:
    /*
     * Find all turrets that are ready to fire.
     * @param manual Include turrets set to 'manual' targeting mode
     * @param automatic Include turrets set to 'automatic' targeting mode
     */
    auto find_all_ready_turrets(turret_filter_types filter) -> std::vector<vehicle_part*>;

    /*
     * Select target using the aiming UI and set turrets to aim at it.
     * Assumes all turrets are ready to fire.
     * @return False if target selection was aborted / no target was found
     */
    auto turrets_aim(std::vector<vehicle_part*>& turrets) -> bool;

    /*
     * Select target using the aiming UI, set turrets to aim at it and fire them.
     * Assumes all turrets are ready to fire.
     * @return Number of shots fired by all turrets (which may be zero)
     */
    auto turrets_aim_and_fire(std::vector<vehicle_part*>& turrets) -> int;

    /*
     * @param pt the vehicle part containing the turret we're trying to target.
     * @return npc object with suitable attributes for targeting a vehicle turret.
     */
    auto get_targeting_npc(const vehicle_part& pt) -> std::unique_ptr<npc>;
    /*@}*/

public:
    /**
     *  Try to assign a crew member (who must be a player ally) to a specific seat
     *  @note enforces NPC's being assigned to only one seat (per-vehicle) at once
     */
    auto assign_seat(vehicle_part& pt, const npc& who) -> bool;

    // Update the set of occupied points and return a reference to it
    auto get_points(bool force_refresh = false) -> std::set<tripoint_abs_ms>&;

    // opens/closes doors or multipart doors
    void open(int part_index);
    void close(int part_index);
    // returns whether the door is open or not
    auto is_open(int part_index) const -> bool;

    auto can_close(int part_index, Character& who) -> bool;

    // Consists only of parts with the FOLDABLE tag.
    auto is_foldable() const -> bool;
    // Restore parts of a folded vehicle.
    auto restore(const std::string& data) -> bool;
    // handles locked vehicles interaction
    auto interact_vehicle_locked() -> bool;
    // true if an alarm part is installed on the vehicle
    auto has_security_working() const -> bool;
    /**
     *  Opens everything that can be opened on the same tile as `p`
     */
    void open_all_at(int p);

    // Honk the vehicle's horn, if there are any
    void honk_horn();
    void reload_seeds(const tripoint_bub_ms& pos);
    void beeper_sound();
    void play_music();
    void play_chimes();
    void operate_planter();
    auto brake_hold_toggle_string() const -> std::string;
    auto tracking_toggle_string() -> std::string;
    void autopilot_patrol_check();
    void toggle_autopilot();
    void toggle_brake_hold();
    void enable_patrol();
    void toggle_tracking();
    // scoop operation,pickups, battery drain, etc.
    void operate_scoop();
    void operate_reaper();
    // for destroying any terrain around vehicle part. Automated mining tool.
    void crash_terrain_around();
    void transform_terrain();
    void add_toggle_to_opts(
        std::vector<uilist_entry>& options, std::vector<std::function<void()>>& actions,
        const std::string& name, char key, const std::string& flag);
    void set_electronics_menu_options(
        std::vector<uilist_entry>& options, std::vector<std::function<void()>>& actions);
    // main method for the control of multiple electronics
    void control_electronics();
    // main method for the control of individual engines
    void control_engines();
    // shows ui menu to select an engine
    auto select_engine() -> int;
    // returns whether the engine is enabled or not, and has fueltype
    auto is_engine_type_on(int e, const itype_id& ft) const -> bool;
    // returns whether the engine is enabled or not
    auto is_engine_on(int e) const -> bool;
    // returns whether the part is enabled or not
    auto is_part_on(int p) const -> bool;
    // returns whether the engine uses specified fuel type
    auto is_engine_type(int e, const itype_id& ft) const -> bool;
    // returns whether the alternator is operational
    auto is_alternator_on(int a) const -> bool;
    // mark engine as on or off
    void toggle_specific_engine(int e, bool on);
    void toggle_specific_part(int p, bool on);
    // muscle engine validation
    auto can_enable_muscle_engine(int e, std::string& failure_reason) const -> bool;
    auto has_muscle_engine_operator(int e) const -> bool;
    void validate_muscle_engines();
    // true if an engine exists with specified type
    // If enabled true, this engine must be enabled to return true
    auto has_engine_type(const itype_id& ft, bool enabled) const -> bool;
    auto has_harnessed_animal() const -> bool;
    // true if an engine exists without the specified type
    // If enabled true, this engine must be enabled to return true
    auto has_engine_type_not(const itype_id& ft, bool enabled) const -> bool;
    // returns true if there's another engine with the same exclusion list; conflict_type holds
    // the exclusion
    auto has_engine_conflict(const vpart_info* possible_conflict, std::string& conflict_type) const
        -> bool;
    // returns true if the engine doesn't consume fuel
    auto is_perpetual_type(int e) const -> bool;
    // if necessary, damage this engine
    void do_engine_damage(size_t e, int strain);
    // remotely open/close doors
    void control_doors();
    // return a vector w/ 'direction' & 'magnitude', in its own sense of the words.
    auto velo_vec() const -> rl_vec2d;
    // normalized vectors, from tilerays face & move
    auto face_vec() const -> rl_vec2d;
    auto move_vec() const -> rl_vec2d;
    // As above, but calculated for the actually used variable `dir`
    auto dir_vec() const -> rl_vec2d;
    // update vehicle parts as the vehicle moves
    void on_move();
    // move the vehicle on the map. Returns updated pointer to self.
    auto act_on_map() -> vehicle*;
    // check if the vehicle should be falling or is in water
    void check_falling_or_floating();

    /**
     * Update the tracker info in the overmap (if enabled).
     * This should be called only when the vehicle has actually been moved.
     */
    void update_overmap(const tripoint_abs_sm& prev_sm);
    void use_monster_capture(int part, const tripoint_bub_ms& pos);
    void use_bike_rack(int part);
    void use_harness(int part, const tripoint_bub_ms& pos);

    void interact_with(const tripoint_bub_ms& pos, int interact_part);

    // Check if a movement is blocked, must be adjacent points
    auto allowed_move(const tripoint_mnt_veh& from, const tripoint_mnt_veh& to) const -> bool;

    // Check if light is blocked, must be adjacent points
    auto allowed_light(const tripoint_mnt_veh& from, const tripoint_mnt_veh& to) const -> bool;

    // Checks if the conditional holds for tiles that can be skipped due to rotation
    auto check_rotated_intervening(
        const tripoint_mnt_veh& from, const tripoint_mnt_veh& to,
        bool (*check)(const vehicle*, const tripoint_mnt_veh&)) const -> bool;

    auto disp_name() const -> std::string;

    /** Required strength to be able to successfully lift the vehicle unaided by equipment */
    auto lift_strength() const -> int;

    // Called by map.cpp to make sure the real position of each zone_data is accurate
    auto refresh_zones() -> bool;

    // Gets the vehicle space xy bounding box for a vehicle in its current rotation.
    auto get_bounding_box() -> bounding_box;
    // Retroactively pass time spent outside bubble
    // Funnels, solar panels
    // If batched is used, it will also drain engines and batteries and use plutonium generators
    void update_time(const time_point& update_to, const bool batched);
    // Process vehicle emitters
    void process_emitters();

private:
    // The faction that owns this vehicle.
    faction_id owner = faction_id::NULL_ID();
    // The faction that previously owned this vehicle
    faction_id old_owner = faction_id::NULL_ID();

    mutable double coefficient_air_resistance = 1;
    mutable double coefficient_rolling_resistance = 1;
    mutable double coefficient_water_resistance = 1;
    mutable double draft_m = 1;
    mutable double hull_height = 0.3;
    mutable double hull_area = 0; // total area of hull in m^2

    // Cached points occupied by the vehicle
    std::set<tripoint_abs_ms> occupied_points;

    std::vector<vehicle_part> parts; // Parts which occupy different tiles
public:
    // Number of parts contained in this vehicle
    auto part_count() const -> int;
    // Returns the vehicle_part with the given part number
    auto part(int part_num) -> vehicle_part&;
    // Same as vehicle::part() except with const binding
    auto cpart(int part_num) const -> const vehicle_part&;
    // Determines whether the given part_num is valid for this vehicle
    auto valid_part(int part_num) const -> bool;
    // Updates the internal precalculated mount offsets after the vehicle has been displaced
    // used in map::displace_vehicle()
    auto advance_precalc_mounts(const tripoint_abs_ms& src) -> std::set<int>;
    // Adjust the vehicle's global z-level to match its center
    void shift_zlevel();

    std::vector<int> alternators;     // List of alternator indices
    std::vector<int> battery_parts;   // List of battery indices
    std::vector<int> engines;         // List of engine indices
    std::vector<int> reactors;        // List of reactor indices
    std::vector<int> solar_panels;    // List of solar panel indices
    std::vector<int> wind_turbines;   // List of wind turbine indices
    std::vector<int> water_wheels;    // List of water wheel indices
    std::vector<int> sails;           // List of sail indices
    std::vector<int> funnels;         // List of funnel indices
    std::vector<int> emitters;        // List of emitter parts
    std::vector<int> loose_parts;     // List of UNMOUNT_ON_MOVE parts
    std::vector<int> wheelcache;      // List of wheels
    std::vector<int> rotors;          // List of rotors
    std::vector<int> balloons;        // List of balloons
    std::vector<int> wings;           // List of wings
    std::vector<int> propellers;      // List of propellerrs
    std::vector<int> rail_wheelcache; // List of rail wheels
    std::vector<int> steering;        // List of STEERABLE parts
    std::vector<int> droppers;        // List of droppers
    std::vector<int> tanks;           // List of FLUIDTANKs
    std::vector<int> converters;      // List of coverters
    // List of parts that will not be on a vehicle very often, or which only one will be present
    std::vector<int> speciality;
    std::vector<int> floating; // List of parts that provide buoyancy to boats

    /**
     * Rail profile of the vehicle.
     *
     * Describes where the vehicle would expect rails to be on its y axis relative to
     * its pivot point in its own coordinate space.
     *
     * For example, for 2-seated draisine (drawn as facing north):
     *
     *       +x
     *    ..:...:..
     *    ..:...:..      - and |  draisine frame
     *    ..0---0..         0     railwheels
     * -y ..|---|.. +y      p     pivot point
     *    ..0-p-0..         .     ground
     *    ..:...:..         :     expected rail
     *    ..:...:..
     *       -x
     *
     * The rail profile would take the value of { -2, 2 }.
     */
    std::vector<int> rail_profile;

    // config values
    std::string name; // vehicle name
    /**
     * Type of the vehicle as it was spawned. This will never change, but it can be an invalid
     * type (e.g. if the definition of the prototype has been removed from json or if it has been
     * spawned with the default constructor).
     */
    vproto_id type;
    // parts_at_relative(dp) is used a lot (to put it mildly)
    std::map<tripoint_mnt_veh, std::vector<int>> relative_parts;
    std::set<label> labels;     // stores labels
    std::set<std::string> tags; // Properties of the vehicle
    // After fuel consumption, this tracks the remainder of fuel < 1, and applies it the next time.
    std::map<itype_id, float> fuel_remainder;
    std::map<itype_id, float> fuel_used_last_turn;
    std::unordered_multimap<tripoint_mnt_veh, zone_data> loot_zones;
    active_item_cache active_items;
    std::vector<cargo_recharge_target> cargo_recharge_targets_;
    bool cargo_recharge_targets_dirty = true;
    // a magic vehicle, powered by magic.gif
    bool magic = false;
    // when does the magic vehicle disappear?
    std::optional<time_duration> summon_time_limit = std::nullopt;

private:
    mutable units::mass mass_cache;
    // cached pivot point
    mutable tripoint_mnt_veh pivot_cache;
    /*
     * The co-ordinates of the bounding box of the vehicle's mount points
     */
    mutable tripoint_mnt_veh mount_max;
    mutable tripoint_mnt_veh mount_min;
    mutable tripoint_mnt_veh mass_center_precalc;
    mutable tripoint_mnt_veh mass_center_no_precalc;
    tripoint_abs_ms autodrive_local_target = tripoint_abs_ms::zero(); // current node the autopilot
                                                                      // is aiming for
    class autodrive_controller;
    std::shared_ptr<autodrive_controller> active_autodrive_controller;

public:
    // Subtract from parts.size() to get the real part count.
    int removed_part_count = 0;

    // Not serialized. When a submap is serialized, it maintains it's own vector
    // of stored vehicles.
    tripoint_abs_sm abs_sm_pos;
    // This *is* serialized. It's the position of the vehicle within the submap.
    point_sm_ms sm_ms_pos;

    // alternator load as a percentage of engine power, in units of 0.1% so 1000 is 100.0%
    int alternator_load = 0;
    /// Time occupied points were calculated.
    time_point occupied_cache_time = calendar::before_time_starts;
    // Turn the vehicle was last processed
    time_point last_update = calendar::before_time_starts;
    // vehicle current velocity, cm/s
    int velocity = 0;
    // velocity vehicle's cruise control trying to achieve
    int cruise_velocity = 0;
    // Only used for collisions, vehicle falls instantly
    int vertical_velocity = 0;
    // id of the om_vehicle struct corresponding to this vehicle
    int om_id = -1;

    auto get_dimension() const -> const dimension_id& { return dimension_id_; }
    auto set_dimension(const dimension_id& dim_id) -> void { dimension_id_ = dim_id; }
    // direction, to which vehicle is turning (player control). will rotate frame on next move
    // must be a multiple of 15 degrees
    units::angle turn_dir = 0_degrees;
    // amount of last turning (for calculate skidding due to handbrake)
    units::angle last_turn = 0_degrees;
    // goes from ~1 to ~0 while proceeding every turn
    float of_turn = 0.0f;
    // leftover from previous turn
    float of_turn_carry = 0.0f;
    int extra_drag = 0;
    // last time point the fluid was inside tanks was checked for processing
    time_point last_fluid_check = calendar::turn_zero;
    // the time point when it was successfully stolen
    std::optional<time_point> theft_time;
    // rotation used for mount precalc values
    std::array<units::angle, 2> pivot_rotation = {{0_degrees, 0_degrees}};

    tripoint_mnt_veh front_left;
    tripoint_mnt_veh front_right;
    towing_data tow_data;
    // points used for rotation of mount precalc values
    std::array<tripoint_mnt_veh, 2> pivot_anchor;
    // frame direction
    tileray face;
    // direction we are moving
    tileray move;


private:
    auto rotate_to_world(units::angle dir, const tripoint_mnt_veh& pivot, const tripoint_mnt_veh& p)
        const -> tripoint_rel_ms;
    auto rotate_to_local(units::angle dir, const tripoint_mnt_veh& pivot, const tripoint_rel_ms& p)
        const -> tripoint_mnt_veh;

    bool no_refresh = false;

    // if true, pivot_cache needs to be recalculated
    mutable bool pivot_dirty = true;
    mutable bool mass_dirty = true;
    mutable bool mass_center_precalc_dirty = true;
    mutable bool mass_center_no_precalc_dirty = true;
    // cached values for air, water, and  rolling resistance;
    mutable bool coeff_rolling_dirty = true;
    mutable bool coeff_air_dirty = true;
    mutable bool coeff_water_dirty = true;
    // air uses a two stage dirty check: one dirty bit gets set on part install,
    // removal, or breakage. The other dirty bit only gets set during part_removal_cleanup,
    // and that's the bit that controls recalculation.  The intent is to only recalculate
    // the coeffs once per turn, even if multiple parts are destroyed in a collision
    mutable bool coeff_air_changed = true;
    // is the vehicle currently mostly in deep water
    mutable bool is_floating = false;
    // is the vehicle currently mostly in water
    mutable bool in_water = false;
    // is the vehicle currently flying
    mutable bool is_flying = false;
    int requested_z_change = 0;
    // is the vehicle currently placed on the map
    bool attached = false;

public:
    // vehicle being driven by player/npc automatically
    bool is_autodriving = false;
    bool is_following = false;
    int follow_distance = 0;
    bool is_patrolling = false;
    // Autodrive speed
    int min_autodrive_speed = 1;
    int max_autodrive_speed = 9;

    // TODO: change these to a bitset + enum?
    // cruise control on/off
    bool cruise_on = true;
    // at least one engine is on, of any type
    bool engine_on = false;
    // parked braking drag on/off
    bool brake_hold = true;
    // vehicle tracking on/off
    bool tracking_on = false;
    // vehicle has no key
    bool is_locked = false;
    // vehicle has alarm on
    bool is_alarm_on = false;
    bool camera_on = false;
    bool autopilot_on = false;
    // true if any non-broken part has the AUTOLOADER flag; maintained by refresh()
    bool has_autoloaders = false;
    // MISSED-4: true if any non-broken part has the RECHARGE flag; maintained by
    // refresh().  Gates the cargo recharge loop in process_items_in_vehicle()
    // (analogous to has_autoloaders gating process_autoloaders() in idle()).
    bool has_cargo_recharge = false;
    // skidding mode
    bool skidding = false;
    // has bloody or smoking parts
    bool check_environmental_effects = false;
    // "inside" flags are outdated and need refreshing
    bool insides_dirty = true;
    // Is the vehicle hanging in the air and expected to fall down in the next turn?
    bool is_falling = false;
    // zone_data positions are outdated and need refreshing
    bool zones_dirty = true;

    // current noise of vehicle (engine working, etc.)
    unsigned char vehicle_noise = 0;

    // Returns debug data to overlay on the screen, a vector of {map tile position
    // relative to vehicle pos, color and text}.
    auto get_debug_overlay_data() const -> std::vector<std::tuple<point_rel_ms, int, std::string>>;

    // Set cruise control
    void set_cruise_control_speed();

private:
    // ID of the dimension this vehicle belongs to.  Empty = primary dimension.
    // Persisted across saves so cross-dimension processing survives reload.
    dimension_id dimension_id_;
};
