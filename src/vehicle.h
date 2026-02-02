#pragma once

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

class avatar;
class Character;
class Creature;
class JsonIn;
class JsonOut;
class map;
class monster;
class nc_color;
class npc;
class player;
class vehicle;
struct vehicle_part;
class vehicle_cursor;
class vehicle_part_range;
class vpart_info;
struct itype;
struct uilist_entry;
template <typename T> class visitable;
struct rl_vec2d;

enum vpart_bitflags : int;
enum ter_bitflags : int;
template<typename feature_type>
class vehicle_part_with_feature_range;

void handbrake();

namespace catacurses
{
class window;
} // namespace catacurses
namespace vehicles
{
// ratio of constant rolling resistance to the part that varies with velocity
constexpr double rolling_constant_to_variable = 33.33;
constexpr float vmiph_per_tile = 400.0f;
} // namespace vehicles
struct rider_data {
    Creature *psg = nullptr;
    int prt = -1;
    bool moved = false;
};
//collision factor for vehicle-vehicle collision; delta_v in m/s
float get_collision_factor( float delta_v );

//How far to scatter parts from a vehicle when the part is destroyed (+/-)
constexpr int SCATTER_DISTANCE = 3;
//adjust this to balance collision damage
constexpr int k_mvel = 200;

enum class turret_filter_types : int {
    MANUAL = 0,
    AUTOMATIC,
    BOTH
};

enum class part_status_flag : int {
    any = 0,
    working = 1 << 0,
    available = 1 << 1,
    enabled = 1 << 2
};
part_status_flag inline operator|( const part_status_flag &rhs, const part_status_flag &lhs )
{
    return static_cast<part_status_flag>( static_cast<int>( lhs ) | static_cast<int>( rhs ) );
}
int inline operator&( const part_status_flag &rhs, const part_status_flag &lhs )
{
    return static_cast<int>( lhs ) & static_cast<int>( rhs );
}

enum veh_coll_type : int {
    veh_coll_nothing,  // 0 - nothing,
    veh_coll_body,     // 1 - monster/player/npc
    veh_coll_veh,      // 2 - vehicle
    veh_coll_bashable, // 3 - bashable
    veh_coll_other,    // 4 - other
    veh_coll_veh_nocollide, // 5 - vehicle NOCOLLIDE
    num_veh_coll_types
};

struct veh_collision {
    //int veh?
    int part  = 0;
    veh_coll_type type = veh_coll_nothing;
    // Impulse, in Ns. Call impulse_to_damage or damage_to_impulse from vehicle_move.cpp for conversion to damage.
    int  imp = 0;
    //vehicle
    void *target  = nullptr;
    //vehicle partnum
    int target_part = 0;
    std::string target_name;

    veh_collision() = default;
};
//TODO!: location stuffs here
class vehicle_stack : public item_stack
{
    private:
        point location;
        vehicle *myorigin;
        int part_num;
    public:
        vehicle_stack( location_vector<item> *newstack, point newloc, vehicle *neworigin, int part ) :
            item_stack( newstack ), location( newloc ), myorigin( neworigin ), part_num( part ) {}
        iterator erase( const_iterator it, detached_ptr<item> *out = nullptr ) override;
        detached_ptr<item> remove( item *to_remove ) override;
        void insert( detached_ptr<item> &&newitem ) override;
        int count_limit() const override {
            return MAX_ITEM_IN_VEHICLE_STORAGE;
        }
        units::volume max_volume() const override;
};

enum towing_point_side : int {
    TOW_FRONT,
    TOW_SIDE,
    TOW_BACK,
    NUM_TOW_TYPES
};

class towing_data
{
    private:
        vehicle *towing;
        vehicle *towed_by;
    public:
        towing_data( vehicle *towed_veh = nullptr, vehicle *tower_veh = nullptr ) :
            towing( towed_veh ), towed_by( tower_veh ) {}
        vehicle *get_towed_by() const {
            return towed_by;
        }
        bool set_towing( vehicle *tower_veh, vehicle *towed_veh );
        vehicle *get_towed() const {
            return towing;
        }
        void clear_towing() {
            towing = nullptr;
            towed_by = nullptr;
        }
        towing_point_side tow_direction;
        // temp variable used for saving/loading
        tripoint other_towing_point;
};

struct bounding_box {
    point p1;
    point p2;
};

char keybind( const std::string &opt, const std::string &context = "VEHICLE" );

int mps_to_vmiph( double mps );
double vmiph_to_mps( int vmiph );
int cmps_to_vmiph( int cmps );
int vmiph_to_cmps( int vmiph );
float impulse_to_damage( float impulse );
float damage_to_impulse( float damage );

class turret_data
{
        friend vehicle;

    public:
        turret_data() = default;
        turret_data( const turret_data & ) = delete;
        turret_data &operator=( const turret_data & ) = delete;
        turret_data( turret_data && ) = default;
        turret_data &operator=( turret_data && ) = default;

        /** Is this a valid instance? */
        explicit operator bool() const {
            return veh && part;
        }

        std::string name() const;

        /** Get base item location */
        item &base();
        item &base() const;

        const vehicle *get_veh() const {
            return veh;
        }

        /** Quantity of ammunition available for use */
        int ammo_remaining() const;

        /** Maximum quantity of ammunition turret can itself contain */
        int ammo_capacity() const;

        /** Specific ammo data or returns nullptr if no ammo available */
        const itype *ammo_data() const;

        /** Specific ammo type or returns "null" if no ammo available */
        itype_id ammo_current() const;

        /** What ammo is available for this turret (may be multiple if uses tanks) */
        std::set<itype_id> ammo_options() const;

        /** Attempts selecting ammo type and returns true if selection was valid */
        bool ammo_select( const itype_id &ammo );

        /** Effects inclusive of any from ammo loaded from tanks */
        std::set<ammo_effect_str_id> ammo_effects() const;

        /** Maximum range considering current ammo (if any) */
        int range() const;

        /**
         * Check if target is in range of this turret (considers current ammo)
         * Assumes this turret's status is 'ready'
         */
        bool in_range( const tripoint &target ) const;

        /**
         * Prepare the turret for firing, called by firing function.
         * This sets up vehicle tanks, recoil adjustments, vehicle rooftop status,
         * and performs any other actions that must be done before firing a turret.
         * @param p the player that is firing the gun, subject to recoil adjustment.
         */
        void prepare_fire( Character &who );

        /**
         * Reset state after firing a prepared turret, called by the firing function.
         * @param p the player that just fired (or attempted to fire) the turret.
         * @param shots the number of shots fired by the most recent call to turret::fire.
         */
        void post_fire( Character &who, int shots );

        /**
         * Fire the turret's gun at a given target.
         * @param p the player firing the turret, passed to pre_fire and post_fire calls.
         * @param target coordinates that will be fired on.
         * @return the number of shots actually fired (may be zero).
         */
        int fire( Character &who, const tripoint &target );

        bool can_reload() const;
        bool can_unload() const;

        enum class status {
            invalid,
            no_ammo,
            no_power,
            ready
        };

        status query() const;

    private:
        turret_data( vehicle *veh, vehicle_part *part )
            : veh( veh ), part( part ) {}
        double cached_recoil = 0;

    protected:
        vehicle *veh = nullptr;
        vehicle_part *part = nullptr;
};

/**
 * Struct used for storing labels
 * (easier to json opposed to a std::map<point, std::string>)
 */
struct label : public point {
    label() = default;
    explicit label( point p ) : point( p ) {}
    label( point p, std::string text ) : point( p ), text( std::move( text ) ) {}

    std::string text;

    void deserialize( JsonIn &jsin );
    void serialize( JsonOut &json ) const;
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
class vehicle
{
    private:
        bool has_structural_part( point dp ) const;
        bool has_structural_or_extendable_part( point dp ) const;
        bool is_structural_part_removed() const;
        void open_or_close( int part_index, bool opening );
        bool is_connected( const vehicle_part &to, const vehicle_part &from,
                           const vehicle_part &excluded ) const;
        void add_missing_frames();
        void add_steerable_wheels();

        // direct damage to part (armor protection and internals are not counted)
        // returns damage bypassed
        int damage_direct( int p, int dmg, damage_type type = DT_TRUE );
        // Removes the part, breaks it into pieces and possibly removes parts attached to it
        int break_off( int p, int dmg );
        // Returns if it did actually explode
        bool explode_fuel( int p, damage_type type );
        std::pair<int, int> get_controls_and_security() const;
        //damages vehicle controls and security system
        void smash_security_system();
        // get vpart powerinfo for part number, accounting for variable-sized parts and hps.
        int part_vpower_w( int index, bool at_full_hp = false ) const;

        // get vpart epowerinfo for part number.
        int part_epower_w( int index ) const;

        // convert watts over time to battery energy
        int power_to_energy_bat( int power_w, const time_duration &d ) const;

        // convert vhp to watts.
        static int vhp_to_watts( int power );

        //Refresh all caches and re-locate all parts
        void refresh();

        // Do stuff like clean up blood and produce smoke from broken parts. Returns false if nothing needs doing.
        bool do_environmental_effects();

        units::volume total_folded_volume() const;

        // Vehicle fuel indicator (by fuel)
        void print_fuel_indicator( const catacurses::window &w, point p,
                                   const itype_id &fuel_type,
                                   bool verbose = false, bool desc = false );
        void print_fuel_indicator( const catacurses::window &w, point p,
                                   const itype_id &fuel_type,
                                   std::map<itype_id, float> fuel_usages,
                                   bool verbose = false, bool desc = false );

        // Calculate how long it takes to attempt to start an engine
        int engine_start_time( int e ) const;

        // How much does the temperature effect the engine starting (0.0 - 1.0)
        double engine_cold_factor( int e ) const;

        // refresh pivot_cache, clear pivot_dirty
        void refresh_pivot() const;

        void refresh_mass() const;
        void calc_mass_center( bool precalc ) const;

        /** empty the contents of a tank, battery or turret spilling liquids randomly on the ground */
        void leak_fuel( vehicle_part &pt );

        int next_hack_id = 0;

    public:

        vehicle_part &get_part_hack( int );
        int get_part_id_hack( int );
        void refresh_locations_hack();

        int get_next_hack_id() {
            return next_hack_id++;
        }

        /**
         * Find a possibly off-map vehicle. If necessary, loads up its submap through
         * the global MAPBUFFER and pulls it from there. For this reason, you should only
         * give it the coordinates of the origin tile of a target vehicle.
         * @param where Location of the other vehicle's origin tile.
         */
        static vehicle *find_vehicle( const tripoint &where );
        static vehicle *find_vehicle( const tripoint_abs_ms &where ) {
            return find_vehicle( where.raw() );
        }

        vehicle( const vproto_id &type_id, int init_veh_fuel = -1, int init_veh_status = -1,
                 std::optional<bool> locked = std::nullopt, std::optional<bool> has_keys = std::nullopt );
        vehicle();
        ~vehicle();

    private:
        void copy_static_from( const vehicle & );
        vehicle( const vehicle & ) = delete;
        vehicle( vehicle && ) = delete;
        vehicle &operator=( vehicle && ) = delete;
        vehicle &operator=( const vehicle & ) = delete;

    public:
        /** Disable or enable refresh() ; used to speed up performance when creating a vehicle */
        void suspend_refresh();
        void enable_refresh();

        void attach() {
            attached = true;
        }

        void detach() {
            attached = false;
        }

        bool is_loaded() const;

        /**
         * Set stat for part constrained by range [0,durability]
         * @note does not invoke base @ref item::on_damage callback
         */
        void set_hp( vehicle_part &pt, int qty );

        /**
         * Apply damage to part constrained by range [0,durability] possibly destroying it
         * @param pt Part being damaged
         * @param qty maximum amount by which to adjust damage (negative permissible)
         * @param dt type of damage which may be passed to base @ref item::on_damage callback
         * @return whether part was destroyed as a result of the damage
         */
        bool mod_hp( vehicle_part &pt, int qty, damage_type dt = DT_NULL );

        // check if given player controls this vehicle
        bool player_in_control( const Character &who ) const;
        // check if player controls this vehicle remotely
        bool remote_controlled( const Character &who ) const;

        // init parts state for randomly generated vehicle
        void init_state( int init_veh_fuel, int init_veh_status,
                         std::optional<bool> locked,
                         std::optional<bool> has_keys );

        // damages all parts of a vehicle by a random amount
        void smash( map &m, float hp_percent_loss_min = 0.1f, float hp_percent_loss_max = 1.2f,
                    float percent_of_parts_to_affect = 1.0f, point damage_origin = point_zero, float damage_size = 0 );

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
        // Vehicle parts list - all the parts on a single tile
        int print_part_list( const catacurses::window &win, int y1, int max_y, int width, int p,
                             int hl = -1, bool detail = false, int start_at = 0 ) const;

        // Vehicle parts descriptions - descriptions for all the parts on a single tile
        void print_vparts_descs( const catacurses::window &win, int max_y, int width, int p,
                                 int &start_at, int &start_limit ) const;
        // towing functions
        void invalidate_towing( bool first_vehicle = false );
        void do_towing_move();
        bool tow_cable_too_far() const;
        bool no_towing_slack() const;
        bool is_towing() const;
        bool has_tow_attached() const;
        int get_tow_part() const;
        bool is_external_part( const tripoint &part_pt ) const;
        bool is_towed() const;
        void set_tow_directions();
        // owner functions
        bool is_owned_by( const Character &c, bool available_to_take = false ) const;
        bool is_old_owner( const Character &c, bool available_to_take = false ) const;
        std::string get_owner_name() const;
        void set_old_owner( const faction_id &temp_owner );
        void remove_old_owner();
        void set_owner( const faction_id &new_owner );
        void set_owner( const Character &c );
        void remove_owner();
        faction_id get_owner() const;
        faction_id get_old_owner() const;
        bool has_owner() const;
        bool has_old_owner() const;
        /**
         * Handle potential vehicle theft.
         * @param you Avatar to check against
         * @param check_only If true, won't prompt to steal and instead will
         * refure to interact
         * @param prompt Whether to prompt confirmation or proceed with the
         * theft without prompt
         * @return whether the avatar is willing to interact with the vehicle
         */
        bool handle_potential_theft( avatar &you, bool check_only = false, bool prompt = true );
        // project a tileray forward to predict obstacles
        std::set<point> immediate_path( units::angle rotate = 0_degrees );
        std::set<point> collision_check_points;
        void autopilot_patrol();
        units::angle get_angle_from_targ( const tripoint &targ );
        void drive_to_local_target( const tripoint &target, bool follow_protocol );
        tripoint get_autodrive_target();
        // Drive automatically towards some destination for one turn.
        autodrive_result do_autodrive( Character &driver );
        // Stop any kind of automatic vehicle control and apply the brakes.
        void stop_autodriving( bool apply_brakes = true );
        /**
         *  Operate vehicle controls
         *  @param pos location of physical controls to operate (ignored during remote operation)
         */
        void use_controls( const tripoint &pos );

        // Fold up the vehicle
        bool fold_up();

        // Attempt to start an engine
        bool start_engine( int e );
        // stop all engines
        void stop_engines();
        // Attempt to start the vehicle's active engines
        void start_engines( bool take_control = false, bool autodrive = false );

        // Engine backfire, making a loud noise
        void backfire( int e ) const;

        // get vpart type info for part number (part at given vector index)
        const vpart_info &part_info( int index, bool include_removed = false ) const;

        // check if certain part can be mounted at certain position (not accounting frame direction)
        bool can_mount( point dp, const vpart_id &id ) const;

        // check if certain part can be unmounted
        bool can_unmount( int p ) const;
        bool can_unmount( int p, std::string &reason ) const;

        // install a new part to vehicle
        int install_part( point dp, const vpart_id &id, bool force = false );

        // Install a copy of the given part, skips possibility check
        int install_part( point dp, vehicle_part &&part );

        /** install item specified item to vehicle as a vehicle part */
        int install_part( point dp, const vpart_id &id, detached_ptr<item> &&obj, bool force = false );

        // find a single tile wide vehicle adjacent to a list of part indices
        bool try_to_rack_nearby_vehicle( const std::vector<std::vector<int>> &list_of_racks );
        // merge a previously found single tile vehicle into this vehicle
        bool merge_rackable_vehicle( vehicle *carry_veh, const std::vector<int> &rack_parts );

        /**
         * @param handler A class that receives various callbacks, e.g. for placing items.
         * This handler is different when called during mapgen (when items need to be placed
         * on the temporary mapgen map), and when called during normal game play (when items
         * go on the main map g->m).
         */
        bool remove_part( int p, RemovePartHandler &handler );
        bool remove_part( int p );
        void part_removal_cleanup();

        // remove the carried flag from a vehicle after it has been removed from a rack
        void remove_carried_flag();
        // remove the tracked flag from a tracked vehicle after it has been removed from a rack
        void remove_tracked_flag();
        // remove a vehicle specified by a list of part indices
        bool remove_carried_vehicle( const std::vector<int> &carried_parts );
        // split the current vehicle into up to four vehicles if they have no connection other
        // than the structure part at exclude
        bool find_and_split_vehicles( int exclude );
        // relocate passengers to the same part on a new vehicle
        void relocate_passengers( const std::vector<Character *> &passengers );
        // remove a bunch of parts, specified by a vector indices, and move them to a new vehicle at
        // the same global position
        // optionally specify the new vehicle position and the mount points on the new vehicle
        bool split_vehicles( const std::vector<std::vector <int>> &new_vehs,
                             const std::vector<vehicle *> &new_vehicles,
                             const std::vector<std::vector <point>> &new_mounts );
        bool split_vehicles( const std::vector<std::vector <int>> &new_veh );

        /** Get handle for base item of part */
        item &part_base( int p );

        /** Get index of part with matching base item or INT_MIN if not found */
        int find_part( const item &it ) const;

        /**
         * Remove a part from a targeted remote vehicle. Useful for, e.g. power cables that have
         * a vehicle part on both sides.
         */
        void remove_remote_part( int part_num );
        /**
         * Yields a range containing all parts (including broken ones) that can be
         * iterated over.
         */
        // TODO: maybe not include broken ones? Have a separate function for that?
        // TODO: rename to just `parts()` and rename the data member to `parts_`.
        vehicle_part_range get_all_parts() const;
        /**
         * Yields a range of parts of this vehicle that each have the given feature
         * and are available: not broken, removed, or part of a carried vehicle.
         * The enabled status of the part is ignored.
         */
        /**@{*/
        vehicle_part_with_feature_range<std::string> get_avail_parts( std::string feature ) const;
        vehicle_part_with_feature_range<vpart_bitflags> get_avail_parts( vpart_bitflags f ) const;
        /**@}*/
        /**
         * Yields a range of parts of this vehicle that each have the given feature
         * and are not broken or removed.
         * The enabled status of the part is ignored.
         */
        /**@{*/
        vehicle_part_with_feature_range<std::string> get_parts_including_carried(
            std::string feature ) const;
        vehicle_part_with_feature_range<vpart_bitflags> get_parts_including_carried(
            vpart_bitflags f ) const;
        /**@}*/
        /**
         * Yields a range of parts of this vehicle that each have the given feature and not removed.
         * The enabled status of the part is ignored.
         */
        /**@{*/
        vehicle_part_with_feature_range<std::string> get_any_parts( std::string feature ) const;
        vehicle_part_with_feature_range<vpart_bitflags> get_any_parts( vpart_bitflags f ) const;
        /**@}*/
        /**
         * Yields a range of parts of this vehicle that each have the given feature
         * and are enabled and available: not broken, removed, or part of a carried vehicle.
         */
        /**@{*/
        vehicle_part_with_feature_range<std::string> get_enabled_parts( std::string feature ) const;
        vehicle_part_with_feature_range<vpart_bitflags> get_enabled_parts( vpart_bitflags f ) const;
        /**@}*/

        // returns the list of indices of parts at certain position (not accounting frame direction)
        std::vector<int> parts_at_relative( point dp, bool use_cache ) const;

        // returns index of part, inner to given, with certain flag, or -1
        int part_with_feature( int p, const std::string &f, bool unbroken ) const;
        int part_with_feature( point pt, const std::string &f, bool unbroken ) const;
        int part_with_feature( int p, vpart_bitflags f, bool unbroken ) const;

        // returns index of part, inner to given, with certain flag, or -1
        int avail_part_with_feature( int p, const std::string &f, bool unbroken ) const;
        int avail_part_with_feature( point pt, const std::string &f, bool unbroken ) const;
        int avail_part_with_feature( int p, vpart_bitflags f, bool unbroken ) const;

        int obstacle_at_position( point pos ) const;
        int opaque_at_position( point pos ) const;

        /**
         *  Check if vehicle has at least one unbroken part with specified flag
         *  @param flag Specified flag to search parts for
         *  @param enabled if set part must also be enabled to be considered
         *  @returns true if part is found
         */
        bool has_part( const std::string &flag, bool enabled = false ) const;
        /**
         *  Check if vehicle has at least one unbroken part with specified flag
         *  @param flag Specified flag to search parts for
         *  @param enabled if set part must also be enabled to be considered
         *  @returns true if part is found
         */
        bool has_part( const vpart_bitflags &flag, bool enabled = false ) const;

        /**
         *  Check if vehicle has at least one unbroken part with specified flag
         *  @param pos limit check for parts to this global position
         *  @param flag The specified flag
         *  @param enabled if set part must also be enabled to be considered
         */
        bool has_part( const tripoint &pos, const std::string &flag, bool enabled = false ) const;

        /**
         *  Get all enabled, available, unbroken vehicle parts at specified position
         *  @param pos position to check
         *  @param flag if set only flags with this part will be considered
         *  @param condition enum to include unabled, unavailable, and broken parts
         */
        std::vector<vehicle_part *> get_parts_at( const tripoint &pos, const std::string &flag,
                part_status_flag condition );
        std::vector<const vehicle_part *> get_parts_at( const tripoint &pos,
                const std::string &flag, part_status_flag condition ) const;

        /** Test if part can be enabled (unbroken, sufficient fuel etc), optionally displaying failures to user */
        bool can_enable( const vehicle_part &pt, bool alert = false ) const;

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
        int next_part_to_open( int p, bool outside = false ) const;

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
        int next_part_to_close( int p, bool outside = false ) const;
        std::vector<int> all_standalone_parts() const;
        // returns indices of all parts in the given location slot
        std::vector<int> all_parts_at_location( const std::string &location ) const;
        // shifts an index to next available of that type for NPC activities
        int get_next_shifted_index( int original_index, Character &who );
        // Given a part and a flag, returns the indices of all contiguously adjacent parts
        // with the same flag on the X and Y Axis
        std::vector<std::vector<int>> find_lines_of_parts( int part, const std::string &flag );

        // returns true if given flag is present for given part index
        bool part_flag( int p, const std::string &f ) const;
        bool part_flag( int p, vpart_bitflags f ) const;

        // Translate mount coordinates "p" using current pivot direction and anchor and return tile coordinates
        point coord_translate( point p ) const;

        // Translate mount coordinates "p" into tile coordinates "q" using given pivot direction and anchor
        void coord_translate( units::angle dir, point pivot, point p,
                              tripoint &q ) const;

        // Translate rotated tile coordinates "p" into mount coordinates "q" using given pivot direction and anchor
        void coord_translate_reverse( units::angle dir, point pivot, const tripoint &p,
                                      point &q ) const;

        tripoint mount_to_tripoint( point mount ) const;
        tripoint mount_to_tripoint( point mount, point offset ) const;

        //Translate tile coordinates into mount coordinates
        point tripoint_to_mount( const tripoint &p ) const;

        // Seek a vehicle part which obstructs tile with given coordinates relative to vehicle position
        int part_at( point dp ) const;
        int part_displayed_at( point dp ) const;
        int roof_at_part( int p ) const;

        // Given a part, finds its index in the vehicle
        int index_of_part( const vehicle_part *part, bool check_removed = false ) const;

        // get symbol for map
        char part_sym( int p, bool exact = false ) const;
        vpart_id part_id_string( int p, bool roof, char &part_mod ) const;

        // get color for map
        nc_color part_color( int p, bool exact = false ) const;

        // Get all printable fuel types
        std::vector<itype_id> get_printable_fuel_types() const;

        // Vehicle fuel indicators (all of them)
        void print_fuel_indicators(
            const catacurses::window &win, point, int start_index = 0,
            bool fullsize = false, bool verbose = false, bool desc = false,
            bool isHorizontal = false );

        //Refresh part locations
        void refresh_position();

        // Pre-calculate mount points for (idir=0) - current direction or (idir=1) - next turn direction
        void precalc_mounts( int idir, units::angle dir, point pivot );

        // get a list of part indices where is a passenger inside
        std::vector<int> boarded_parts() const;

        // get a list of part indices and Creature pointers with a rider
        std::vector<rider_data> get_riders() const;

        // get passenger at part p
        player *get_passenger( int p ) const;
        // get monster on a boardable part at p
        monster *get_pet( int p ) const;

        bool enclosed_at( const tripoint &pos ); // not const because it calls refresh_insides
        // Returns the location of the vehicle in global map square coordinates.
        tripoint_abs_ms global_square_location() const;
        // Returns the location of the vehicle in global overmap terrain coordinates.
        tripoint_abs_omt global_omt_location() const;
        // Returns the coordinates (in map squares) of the vehicle relative to the local map.
        tripoint global_pos3() const;
        /**
         * Get the coordinates of the studied part of the vehicle
         */
        tripoint global_part_pos3( const int &index ) const;
        tripoint global_part_pos3( const vehicle_part &pt ) const;
        /**
         * All the fuels that are in all the tanks in the vehicle, nicely summed up.
         * Note that empty tanks don't count at all. The value is the amount as it would be
         * reported by @ref fuel_left, it is always greater than 0. The key is the fuel item type.
         */
        std::map<itype_id, int> fuels_left() const;

        // Checks how much certain fuel left in tanks.
        int fuel_left( const itype_id &ftype, bool recurse = false ) const;
        // Checks how much of the part p's current fuel is left
        int fuel_left( int p, bool recurse = false ) const;
        // Checks how much of an engine's current fuel is left in the tanks.
        int engine_fuel_left( int e, bool recurse = false ) const;
        int fuel_capacity( const itype_id &ftype ) const;

        // drains a fuel type (e.g. for the kitchen unit)
        // returns amount actually drained, does not engage reactor
        int drain( const itype_id &ftype, int amount );
        int drain( int index, int amount );
        /**
         * Consumes enough fuel by energy content. Does not support cable draining.
         * @param ftype Type of fuel
         * @param energy_j Desired amount of energy of fuel to consume
         * @return Amount of energy actually consumed. May be more or less than energy.
         */
        double drain_energy( const itype_id &ftype, double energy_j );

        // fuel consumption of vehicle engines of given type
        int basic_consumption( const itype_id &ftype ) const;
        int consumption_per_hour( const itype_id &ftype, int fuel_rate ) const;

        void consume_fuel( int load, int t_seconds = 6, bool skip_electric = false );

        /**
         * Maps used fuel to its basic (unscaled by load/strain) consumption.
         */
        std::map<itype_id, int> fuel_usage() const;

        /**
         * Get all vehicle lights (excluding any that are destroyed)
         * @param active if true return only lights which are enabled
         */
        std::vector<vehicle_part *> lights( bool active = false );

        void update_alternator_load();

        // Total drain or production of electrical power from engines.
        int total_engine_epower_w() const;
        // Total production of electrical power from alternators.
        int total_alternator_epower_w() const;
        // Total power currently being produced by all solar panels.
        int total_solar_epower_w() const;
        // Total power currently being produced by all wind turbines.
        int total_wind_epower_w() const;
        // Total power currently being produced by all water wheels.
        int total_water_wheel_epower_w() const;
        // Total power drain across all vehicle accessories.
        int total_accessory_epower_w() const;
        // Net power draw or drain on batteries.
        int net_battery_charge_rate_w() const;
        // Maximum available power available from all reactors. Power from
        // reactors is only drawn when batteries are empty.
        int max_reactor_epower_w() const;
        // Produce and consume electrical power, with excess power stored or
        // taken from batteries.
        void power_parts();

        /**
         * Try to charge our (and, optionally, connected vehicles') batteries by the given amount.
         * @return amount of charge left over.
         */
        int charge_battery( int amount, bool include_other_vehicles = true );

        /**
         * Try to discharge our (and, optionally, connected vehicles') batteries by the given amount.
         * @return amount of request unfulfilled (0 if totally successful).
         */
        int discharge_battery( int amount, bool recurse = true );

        /**
         * Mark mass caches and pivot cache as dirty
         */
        void invalidate_mass();

        //Converts angles into turning increments
        static int angle_to_increment( units::angle dir );

        // get the total mass of vehicle, including cargo and passengers
        units::mass total_mass() const;

        // Gets the center of mass calculated for precalc[0] coordinates
        point rotated_center_of_mass() const;
        // Gets the center of mass calculated for mount point coordinates
        point local_center_of_mass() const;

        // Get the pivot point of vehicle; coordinates are unrotated mount coordinates.
        // This may result in refreshing the pivot point if it is currently stale.
        point pivot_point() const;

        // Get the (artificial) displacement of the vehicle due to the pivot point changing
        // between precalc[0] and precalc[1]. This needs to be subtracted from any actual
        // vehicle motion after precalc[1] is prepared.
        point pivot_displacement() const;

        // Get combined power of all engines, the ideal amount of power, not the current power
        int ideal_engine_power( bool safe = false ) const;
        // Get combined power of all engines. If fueled == true, then only engines which
        // vehicle have fuel for are accounted.  If safe == true, then limit engine power to
        // their safe power.
        int total_power_w( bool fueled = true, bool safe = false ) const;

        // Get ground acceleration gained by combined power of all engines. If fueled == true,
        // then only engines which the vehicle has fuel for are included
        int ground_acceleration( bool fueled = true, int at_vel_in_vmi = -1, bool ideal = false ) const;
        // Get water acceleration gained by combined power of all engines. If fueled == true,
        // then only engines which the vehicle has fuel for are included
        int water_acceleration( bool fueled = true, int at_vel_in_vmi = -1, bool ideal = false ) const;
        // get air acceleration gained by combined power of all engines. If fueled == true,
        // then only engines which the vehicle hs fuel for are included
        int aircraft_acceleration( bool fueled = true, int at_vel_in_vmi = -1, bool ideal = false ) const;

        // Get acceleration for the current movement mode
        int acceleration( bool fueled = true, int at_vel_in_vmi = -1 ) const;

        // Get the vehicle's actual current acceleration
        int current_acceleration( bool fueled = true ) const;

        // is the vehicle currently moving?
        bool is_moving() const;

        // can the vehicle use rails?
        bool can_use_rails() const;

        // Get maximum ground velocity gained by combined power of all engines.
        // If fueled == true, then only the engines which the vehicle has fuel for are included
        int max_ground_velocity( bool fueled = true, bool ideal = false ) const;
        // Get maximum water velocity gained by combined power of all engines.
        // If fueled == true, then only the engines which the vehicle has fuel for are included
        int max_water_velocity( bool fueled = true, bool ideal = false ) const;
        // get maximum air velocity based on rotor physics
        int max_air_velocity( bool fueled = true, bool ideal = false ) const;
        // Get maximum velocity for the current movement mode
        int max_velocity( bool fueled = true, bool ideal = false ) const;
        // Get maximum reverse velocity for the current movement mode
        int max_reverse_velocity( bool fueled = true, bool ideal = false ) const;

        // Get safe ground velocity gained by combined power of all engines.
        // If fueled == true, then only the engines which the vehicle has fuel for are included
        int safe_ground_velocity( bool fueled = true, bool ideal = false ) const;
        // get safe air velocity gained by combined power of all engines.
        // if fueled == true, then only the engines which the vehicle hs fuel for are included
        int safe_aircraft_velocity( bool fueled = true, bool ideal = false ) const;
        // Get safe water velocity gained by combined power of all engines.
        // If fueled == true, then only the engines which the vehicle has fuel for are included
        int safe_water_velocity( bool fueled = true, bool ideal = false ) const;
        // Get maximum velocity for the current movement mode
        int safe_velocity( bool fueled = true ) const;

        // Generate field from a part, either at front or back of vehicle depending on velocity.
        void spew_field( double joules, int part, field_type_id type, int intensity = 1 );

        // Loop through engines and generate noise and smoke for each one
        void noise_and_smoke( int load, time_duration time = 1_turns );

        /**
         * Calculates the sum of the area under the wheels of the vehicle.
         */
        int wheel_area() const;
        // average off-road rating for displaying off-road performance
        float average_or_rating() const;

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
        double coeff_air_drag() const;

        /**
         * coefficient of rolling resistance
         * multiplied by velocity to get the variable part of rolling resistance drag in N
         * multiplied by a constant to get the constant part of rolling resistance drag in N
         * depends on wheel design, wheel number, and vehicle weight
         */
        double coeff_rolling_drag() const;

        /**
         * coefficient of water drag in kg/m
         * multiplied by the square of speed to calculate water drag force in N
         * proportional to cross sectional area of the vehicle, times the density of water,
         * times a dimensional constant based on the vehicle's shape
         */
        double coeff_water_drag() const;

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
        double max_buoyancy() const;

        /**
         * watertight hull height in meters measures distance from bottom of vehicle
         * to the point where the vehicle will start taking on water
         */
        double water_hull_height() const;

        /**
         * water draft in meters - how much of the vehicle's body is under water
         * must be less than the hull height or the boat will sink
         * at some point, also add boats with deep draft running around
         */
        double water_draft() const;

        /**
         * can_float
         * does the vehicle have freeboard or does it overflow with water?
         */
        bool can_float() const;
        /**
         * is the vehicle mostly in water or mostly on fairly dry land?
         */
        bool is_in_water( bool deep_water = false ) const;
        bool is_watercraft() const;
        /**
         * is the vehicle flying? is it an aircraft?
         */
        bool is_aircraft() const;
        /**
         * does the vehicle use rotors?
         */
        bool is_rotorcraft() const;
        /**
         * does the vehicle have lift-generating parts?
         */
        bool has_lift() const;
        /**
         * total area of every rotors in m^2
         */
        double total_rotor_area() const;
        /**
         * lift of balloons in newtons
         */
        double total_balloon_lift() const;
        /**
         * lift from wings in newtons
         */
        double total_wing_lift() const;
        /**
         * speed needed for aircraft takeoff
         */
        int get_takeoff_speed() const;
        /**
         * total area of every propeller in m^2
         */
        double total_propeller_area() const;
        /**
         * lift of rotorcraft in newton
         */
        double thrust_of_rotorcraft( bool fuelled, bool safe = false, bool ideal = false ) const;
        /**
         * foward thrust of propellers in newtons
         */
        double foward_thrust_of_propellers( bool fuelled, bool safe = false, bool ideal = false ) const;
        /**
         * total foward thrust of all airborn pushers
         */
        double total_thrust( bool fuelled, bool safe = false, bool ideal = false ) const;
        /**
         * total lift of all lifters
         */
        double total_lift( bool fuelled, bool safe = false, bool ideal = false ) const;
        bool has_sufficient_lift() const;
        int get_z_change() const;
        bool is_flying_in_air() const;
        void set_flying( bool new_flying_value );
        /**
         * Traction coefficient of the vehicle.
         * 1.0 on road. Outside roads, depends on mass divided by wheel area
         * and the surface beneath wheels.
         *
         * Affects safe velocity, acceleration and handling difficulty.
         */
        float k_traction( float wheel_traction_area ) const;
        /*@}*/

        // Extra drag on the vehicle from components other than wheels.
        // @param actual is current drag if true or nominal drag otherwise
        int static_drag( bool actual = true ) const;

        // strain of engine(s) if it works higher that safe speed (0-1.0)
        float strain() const;

        // Calculate if it can move using its wheels
        bool sufficient_wheel_config() const;
        bool balanced_wheel_config() const;
        bool valid_wheel_config() const;

        // return the relative effectiveness of the steering (1.0 is normal)
        // <0 means there is no steering installed at all.
        float steering_effectiveness() const;

        /** Returns roughly driving skill level at which there is no chance of fumbling. */
        float handling_difficulty() const;

        /**
         * Use grid traversal to enumerate all connected vehicles.
         * @param connected_vehicles is an output map from vehicle pointers to
         * a bool that is true if the vehicle is in the reality bubble.
         * @param vehicle_list is a set of pointers to vehicles present in the reality bubble.
         */
        static void enumerate_vehicles( std::map<vehicle *, bool> &connected_vehicles,
                                        const std::set<vehicle *> &vehicle_list );
        // idle fuel consumption
        void idle( bool on_map = true );
        // continuous processing for running vehicle alarms
        void alarm();
        // leak from broken tanks
        void slow_leak();

        //checks if we are, or will be after movement, on a ramp
        bool check_on_ramp( int idir = 0, const tripoint &offset = tripoint_zero ) const;

        //calculates the precalc zlevels wrt ramps
        void adjust_zlevel( int idir = 0, const tripoint &offset = tripoint_zero );

        // thrust (1) or brake (-1) vehicle
        // @param z = z thrust for helicopters etc
        void thrust( int thd, int z = 0 );

        //deceleration due to ground friction and air resistance
        int slowdown( int velocity ) const;

        // depending on skid vectors, chance to recover.
        void possibly_recover_from_skid();

        //forward component of velocity.
        float forward_velocity() const;

        // cruise control
        void cruise_thrust( int amount );

        // turn vehicle left (negative) or right (positive), degrees
        void turn( units::angle deg );

        void set_facing( units::angle deg, bool refresh = true ) {
            turn_dir = deg;
            face.init( deg );
            pivot_rotation[0] = deg;
            if( refresh ) {
                refresh_position();
            }
        }

        void set_pivot( point pivot, bool refresh = true ) {
            pivot_cache = pivot;
            pivot_anchor[0] = pivot;
            if( refresh ) {
                refresh_position();
            }
        }

        void set_facing_and_pivot( units::angle deg, point pivot, bool refresh = true ) {
            set_facing( deg, false );
            set_pivot( pivot, refresh );
        }

        // Returns if any collision occurred
        bool collision( std::vector<veh_collision> &colls,
                        const tripoint &dp,
                        bool just_detect, bool bash_floor = false );

        // Handle given part collision with vehicle, monster/NPC/player or terrain obstacle
        // Returns collision, which has type, impulse, part, & target.
        veh_collision part_collision( int part, const tripoint &p,
                                      bool just_detect, bool bash_floor );

        // Process the trap beneath
        void handle_trap( const tripoint &p, int part );
        void activate_magical_follow();
        void activate_animal_follow();
        /**
         * vehicle is driving itself
         */
        void selfdrive( point );
        /**
         * can the helicopter descend/ascend here?
         */
        bool check_heli_descend( Character &who );
        bool check_heli_ascend( Character &who );
        bool check_is_heli_landed();
        /**
         * Player is driving the vehicle
         * @param p direction player is steering
         * @param z for vertical movement - e.g helicopters
         */
        void pldrive( Character &driver, point p, int z = 0 );

        // stub for per-vpart limit
        units::volume max_volume( int part ) const;
        units::volume free_volume( int part ) const;
        units::volume stored_volume( int part ) const;

        /**
         * Remove an item from active item processing queue as necessary
         */
        void make_inactive( item &target );
        /**
         * Update an item's active status, for example when adding
         * hot or perishable liquid to a container.
         */
        void make_active( item &target );
        /**
         * Try to add an item to part's cargo.
         */
        detached_ptr<item> add_item( int part, detached_ptr<item> &&itm );
        /** Like the above */
        detached_ptr<item> add_item( vehicle_part &pt, detached_ptr<item> &&obj );

        /**
         * Add an item counted by charges to the part's cargo.
         *
         * @returns Any remaining charges that couldn't be added.
         */
        detached_ptr<item> add_charges( int part, detached_ptr<item> &&itm );

        // remove item from part's cargo
        detached_ptr<item> remove_item( int part, item *it );
        vehicle_stack::iterator remove_item( int part, vehicle_stack::const_iterator it,
                                             detached_ptr<item>  *ret = nullptr );

        vehicle_stack get_items( int part ) const;
        vehicle_stack get_items( int part );
        void dump_items_from_part( size_t index );

        // Generates starting items in the car, should only be called when placed on the map
        void place_spawn_items();

        void gain_moves();

        // if its a summoned vehicle - its gotta dissappear at some point, return true if destroyed
        bool decrement_summon_timer();

        // reduces velocity to 0
        void stop( bool update_cache = true );

        void refresh_insides();

        void unboard_all();

        // Damage individual part. bash means damage
        // must exceed certain threshold to be subtracted from hp
        // (a lot light collisions will not destroy parts)
        // Returns damage bypassed
        int damage( int p, int dmg, damage_type type = DT_BASH, bool aimed = true );

        // damage all parts (like shake from strong collision), range from dmg1 to dmg2
        void damage_all( int dmg1, int dmg2, damage_type type, point impact );

        //Shifts the coordinates of all parts and moves the vehicle in the opposite direction.
        void shift_parts( point delta );
        bool shift_if_needed();

        void shed_loose_parts();

        /**
         * @name Vehicle turrets
         *
         *@{*/

        /** Get all vehicle turrets (excluding any that are destroyed) */
        std::vector<vehicle_part *> turrets();

        /** Get all vehicle turrets loaded and ready to fire at target */
        std::vector<vehicle_part *> turrets( const tripoint &target );

        /** Get firing data for a turret */
        turret_data turret_query( vehicle_part &pt );
        turret_data turret_query( const vehicle_part &pt ) const;

        turret_data turret_query( const tripoint &pos );
        turret_data turret_query( const tripoint &pos ) const;

        /** Returns true if any part on the tile the turret is installed on has the MANUAL flag. */
        bool is_manual_turret( const vehicle_part &pt ) const;

        /** Set targeting mode for specific turrets */
        void turrets_set_targeting();

        /** Set firing mode for specific turrets */
        void turrets_set_mode();

        /** Select a single ready turret, aim it using the aiming UI and fire. */
        void turrets_aim_and_fire_single( avatar &you );

        /*
         * Find all ready turrets, aim them using aiming UI and fire.
         * @param turret_filter Decide which filter to use (manual, automatic, both)
         * @param show_msg Show 'no such turrets found' message. Does not affect returned value.
         * @return False if there are no such turrets
         */

        bool turrets_aim_and_fire_mult( avatar &you, const turret_filter_types turret_filter,
                                        const bool show_msg = false );

        /*
         * Fire turret at automatically acquired target
         * @return number of shots actually fired (which may be zero)
         */
        int automatic_fire_turret( vehicle_part &pt );

        // How many hits of damage `dmg` and damage type `type` to part with ID `p` to destroy it is needed?
        // 0 if it will never destroy.
        // Be aware this will not consider damage to more outside parts such as inner parts protected by an outer wall, only armor effects are considered
        unsigned int hits_to_destroy( int p, int dmg, damage_type type ) const;

        /**
         * @name Vehicle Droppers
         *
         *@{*/

        vehicle_part *get_cargo_part( vehicle_part *part );

        bool has_item_stored( vehicle_part *part );

        void item_dropper_drop( std::vector<vehicle_part *> droppers, bool single );

        void item_dropper_drop_all();

        void item_dropper_drop_single( bool single );
    private:
        /*
         * Find all turrets that are ready to fire.
         * @param manual Include turrets set to 'manual' targeting mode
         * @param automatic Include turrets set to 'automatic' targeting mode
         */
        std::vector<vehicle_part *> find_all_ready_turrets( turret_filter_types filter );

        /*
         * Select target using the aiming UI and set turrets to aim at it.
         * Assumes all turrets are ready to fire.
         * @return False if target selection was aborted / no target was found
         */
        bool turrets_aim( std::vector<vehicle_part *> &turrets );

        /*
         * Select target using the aiming UI, set turrets to aim at it and fire them.
         * Assumes all turrets are ready to fire.
         * @return Number of shots fired by all turrets (which may be zero)
         */
        int turrets_aim_and_fire( std::vector<vehicle_part *> &turrets );

        /*
         * @param pt the vehicle part containing the turret we're trying to target.
         * @return npc object with suitable attributes for targeting a vehicle turret.
         */
        std::unique_ptr<npc> get_targeting_npc( const vehicle_part &pt );
        /*@}*/

    public:
        /**
         *  Try to assign a crew member (who must be a player ally) to a specific seat
         *  @note enforces NPC's being assigned to only one seat (per-vehicle) at once
         */
        bool assign_seat( vehicle_part &pt, const npc &who );

        // Update the set of occupied points and return a reference to it
        std::set<tripoint> &get_points( bool force_refresh = false );

        // opens/closes doors or multipart doors
        void open( int part_index );
        void close( int part_index );
        // returns whether the door is open or not
        bool is_open( int part_index ) const;

        bool can_close( int part_index, Character &who );

        // Consists only of parts with the FOLDABLE tag.
        bool is_foldable() const;
        // Restore parts of a folded vehicle.
        bool restore( const std::string &data );
        //handles locked vehicles interaction
        bool interact_vehicle_locked();
        //true if an alarm part is installed on the vehicle
        bool has_security_working() const;
        /**
         *  Opens everything that can be opened on the same tile as `p`
         */
        void open_all_at( int p );

        // Honk the vehicle's horn, if there are any
        void honk_horn();
        void reload_seeds( const tripoint &pos );
        void beeper_sound();
        void play_music();
        void play_chimes();
        void operate_planter();
        std::string tracking_toggle_string();
        void autopilot_patrol_check();
        void toggle_autopilot();
        void enable_patrol();
        void toggle_tracking();
        //scoop operation,pickups, battery drain, etc.
        void operate_scoop();
        void operate_reaper();
        // for destroying any terrain around vehicle part. Automated mining tool.
        void crash_terrain_around();
        void transform_terrain();
        void add_toggle_to_opts( std::vector<uilist_entry> &options,
                                 std::vector<std::function<void()>> &actions, const std::string &name, char key,
                                 const std::string &flag );
        void set_electronics_menu_options( std::vector<uilist_entry> &options,
                                           std::vector<std::function<void()>> &actions );
        //main method for the control of multiple electronics
        void control_electronics();
        //main method for the control of individual engines
        void control_engines();
        // shows ui menu to select an engine
        int select_engine();
        //returns whether the engine is enabled or not, and has fueltype
        bool is_engine_type_on( int e, const itype_id &ft ) const;
        //returns whether the engine is enabled or not
        bool is_engine_on( int e ) const;
        //returns whether the part is enabled or not
        bool is_part_on( int p ) const;
        //returns whether the engine uses specified fuel type
        bool is_engine_type( int e, const itype_id &ft ) const;
        //returns whether the alternator is operational
        bool is_alternator_on( int a ) const;
        //mark engine as on or off
        void toggle_specific_engine( int e, bool on );
        void toggle_specific_part( int p, bool on );
        //true if an engine exists with specified type
        //If enabled true, this engine must be enabled to return true
        bool has_engine_type( const itype_id &ft, bool enabled ) const;
        bool has_harnessed_animal() const;
        //true if an engine exists without the specified type
        //If enabled true, this engine must be enabled to return true
        bool has_engine_type_not( const itype_id &ft, bool enabled ) const;
        //returns true if there's another engine with the same exclusion list; conflict_type holds
        //the exclusion
        bool has_engine_conflict( const vpart_info *possible_conflict, std::string &conflict_type ) const;
        //returns true if the engine doesn't consume fuel
        bool is_perpetual_type( int e ) const;
        //if necessary, damage this engine
        void do_engine_damage( size_t e, int strain );
        //remotely open/close doors
        void control_doors();
        // return a vector w/ 'direction' & 'magnitude', in its own sense of the words.
        rl_vec2d velo_vec() const;
        //normalized vectors, from tilerays face & move
        rl_vec2d face_vec() const;
        rl_vec2d move_vec() const;
        // As above, but calculated for the actually used variable `dir`
        rl_vec2d dir_vec() const;
        // update vehicle parts as the vehicle moves
        void on_move();
        // move the vehicle on the map. Returns updated pointer to self.
        vehicle *act_on_map();
        // check if the vehicle should be falling or is in water
        void check_falling_or_floating();

        /**
         * Update the submap coordinates and update the tracker info in the overmap
         * (if enabled).
         * This should be called only when the vehicle has actually been moved, not when
         * the map is just shifted (in the later case simply set smx/smy directly).
         */
        void set_submap_moved( const tripoint &p );
        void use_monster_capture( int part, const tripoint &pos );
        void use_bike_rack( int part );
        void use_harness( int part, const tripoint &pos );

        void interact_with( const tripoint &pos, int interact_part );

        //Check if a movement is blocked, must be adjacent points
        bool allowed_move( point from, point to ) const;

        //Check if light is blocked, must be adjacent points
        bool allowed_light( point from, point to ) const;

        //Checks if the conditional holds for tiles that can be skipped due to rotation
        bool check_rotated_intervening( point from, point to, bool( *check )( const vehicle *,
                                        point ) ) const;

        std::string disp_name() const;

        /** Required strength to be able to successfully lift the vehicle unaided by equipment */
        int lift_strength() const;

        // Called by map.cpp to make sure the real position of each zone_data is accurate
        bool refresh_zones();

        //Gets the vehicle space xy bounding box for a vehicle in its current rotation.
        bounding_box get_bounding_box();
        // Retroactively pass time spent outside bubble
        // Funnels, solar panels
        void update_time( const time_point &update_to );
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
        std::set<tripoint> occupied_points;

        std::vector<vehicle_part> parts;   // Parts which occupy different tiles
    public:
        // Number of parts contained in this vehicle
        int part_count() const;
        // Returns the vehicle_part with the given part number
        vehicle_part &part( int part_num );
        // Same as vehicle::part() except with const binding
        const vehicle_part &cpart( int part_num ) const;
        // Determines whether the given part_num is valid for this vehicle
        bool valid_part( int part_num ) const;
        // Updates the internal precalculated mount offsets after the vehicle has been displaced
        // used in map::displace_vehicle()
        std::set<int> advance_precalc_mounts( point new_pos, const tripoint &src );
        // Adjust the vehicle's global z-level to match its center
        void shift_zlevel();

        std::vector<int> alternators;      // List of alternator indices
        std::vector<int> engines;          // List of engine indices
        std::vector<int> reactors;         // List of reactor indices
        std::vector<int> solar_panels;     // List of solar panel indices
        std::vector<int> wind_turbines;    // List of wind turbine indices
        std::vector<int> water_wheels;     // List of water wheel indices
        std::vector<int> sails;            // List of sail indices
        std::vector<int> funnels;          // List of funnel indices
        std::vector<int> emitters;         // List of emitter parts
        std::vector<int> loose_parts;      // List of UNMOUNT_ON_MOVE parts
        std::vector<int> wheelcache;       // List of wheels
        std::vector<int> rotors;           // List of rotors
        std::vector<int> balloons;         // List of balloons
        std::vector<int> wings;            // List of wings
        std::vector<int> propellers;       // List of propellerrs
        std::vector<int> rail_wheelcache;  // List of rail wheels
        std::vector<int> steering;         // List of STEERABLE parts
        std::vector<int> droppers;         // List of droppers
        // List of parts that will not be on a vehicle very often, or which only one will be present
        std::vector<int> speciality;
        std::vector<int> floating;         // List of parts that provide buoyancy to boats

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
        std::string name;   // vehicle name
        /**
         * Type of the vehicle as it was spawned. This will never change, but it can be an invalid
         * type (e.g. if the definition of the prototype has been removed from json or if it has been
         * spawned with the default constructor).
         */
        vproto_id type;
        // parts_at_relative(dp) is used a lot (to put it mildly)
        std::map<point, std::vector<int>> relative_parts;
        std::set<label> labels;            // stores labels
        std::set<std::string> tags;        // Properties of the vehicle
        // After fuel consumption, this tracks the remainder of fuel < 1, and applies it the next time.
        std::map<itype_id, float> fuel_remainder;
        std::map<itype_id, float> fuel_used_last_turn;
        std::unordered_multimap<point, zone_data> loot_zones;
        active_item_cache active_items;
        // a magic vehicle, powered by magic.gif
        bool magic = false;
        // when does the magic vehicle disappear?
        std::optional<time_duration> summon_time_limit = std::nullopt;

    private:
        mutable units::mass mass_cache;
        // cached pivot point
        mutable point pivot_cache;
        /*
         * The co-ordinates of the bounding box of the vehicle's mount points
         */
        mutable point mount_max;
        mutable point mount_min;
        mutable point mass_center_precalc;
        mutable point mass_center_no_precalc;
        tripoint autodrive_local_target = tripoint_zero; // current node the autopilot is aiming for
        class autodrive_controller;
        std::shared_ptr<autodrive_controller> active_autodrive_controller;

    public:
        // Subtract from parts.size() to get the real part count.
        int removed_part_count = 0;

        /**
         * Submap coordinates of the currently loaded submap (see game::m)
         * that contains this vehicle. These values are changed when the map
         * shifts (but the vehicle is not actually moved than, it also stays on
         * the same submap, only the relative coordinates in map::grid have changed).
         * These coordinates must always refer to the submap in map::grid that contains
         * this vehicle.
         * When the vehicle is really moved (by map::displace_vehicle), set_submap_moved
         * is called and updates these values, when the map is only shifted or when a submap
         * is loaded into the map the values are directly set. The vehicles position does
         * not change therefor no call to set_submap_moved is required.
         */
        tripoint sm_pos;

        // alternator load as a percentage of engine power, in units of 0.1% so 1000 is 100.0%
        int alternator_load = 0;
        /// Time occupied points were calculated.
        time_point occupied_cache_time = calendar::before_time_starts;
        // Turn the vehicle was last processed
        time_point last_update = calendar::before_time_starts;
        // save values
        /**
         * Position of the vehicle *inside* the submap that contains the vehicle.
         * This will (nearly) always be in the range (0...SEEX-1).
         * Note that vehicles are "moved" by map::displace_vehicle. You should not
         * set them directly, except when initializing the vehicle or during mapgen.
         */
        point pos;
        // vehicle current velocity, mph * 100
        int velocity = 0;
        // velocity vehicle's cruise control trying to achieve
        int cruise_velocity = 0;
        // Only used for collisions, vehicle falls instantly
        int vertical_velocity = 0;
        // id of the om_vehicle struct corresponding to this vehicle
        int om_id = -1;
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
        std::array<units::angle, 2> pivot_rotation = { { 0_degrees, 0_degrees } };

        point front_left;
        point front_right;
        towing_data tow_data;
        // points used for rotation of mount precalc values
        std::array<point, 2> pivot_anchor;
        // frame direction
        tileray face;
        // direction we are moving
        tileray move;


    private:
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
        bool is_patrolling = false;
        // TODO: change these to a bitset + enum?
        // cruise control on/off
        bool cruise_on = true;
        // at least one engine is on, of any type
        bool engine_on = false;
        // vehicle tracking on/off
        bool tracking_on = false;
        // vehicle has no key
        bool is_locked = false;
        // vehicle has alarm on
        bool is_alarm_on = false;
        bool camera_on = false;
        bool autopilot_on = false;
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
        std::vector<std::tuple<point, int, std::string>> get_debug_overlay_data() const;
};

namespace rot
{
temperature_flag temperature_flag_for_part( const vehicle &veh, size_t part );
} // namespace rot


