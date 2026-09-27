#pragma once

#include "coordinates.h"
#include "dimension_info.h"
#include "enums.h"
#include "json.h"
#include "memory_fast.h"
#include "overmap_types.h"
#include "string_id.h"
#include "type_id.h"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class character_id;
enum class cube_direction : int;
class map_extra;
class mapgendata;
class monster;
class npc;
class overmap;
class overmap_special;
class overmap_special_batch;
class throbber_popup;
class vehicle;
struct mapgen_arguments;
struct mongroup;
struct om_vehicle;
struct radio_tower;
struct regional_settings;

namespace om_direction {
enum class type;
} // namespace om_direction

struct overmap_path_params {
    int road_cost = -1;
    int field_cost = -1;
    int dirt_road_cost = -1;
    int trail_cost = -1;
    int forest_cost = -1;
    int small_building_cost = -1;
    int shore_cost = -1;
    int swamp_cost = -1;
    int water_cost = -1;
    int air_cost = -1;
    int other_cost = -1;
    bool avoid_danger = true;
    bool only_known_by_player = true;

    static constexpr int standard_cost = 10;
    static auto for_player() -> overmap_path_params;
    static auto for_npc() -> overmap_path_params;
    static auto for_land_vehicle(float offroad_coeff, bool tiny, bool amphibious)
        -> overmap_path_params;
    static auto for_watercraft() -> overmap_path_params;
    static auto for_aircraft() -> overmap_path_params;
};

struct radio_tower_reference {
    /** The radio tower itself, points into @ref overmap::radios */
    radio_tower* tower;
    /** The global absolute position of the tower (in submap coordinates) */
    point_abs_sm abs_sm_pos;
    /** Perceived signal strength (tower output strength minus distance) */
    int signal_strength;
    operator bool() const { return tower != nullptr; }
};

struct city_reference {
    static const city_reference invalid;
    /** The city itself, points into @ref overmap::cities */
    const struct city* city;
    /** The global absolute position of the city (in submap coordinates!) */
    tripoint_abs_sm abs_sm_pos;
    /** Distance to center of the search */
    int distance;

    operator bool() const { return city != nullptr; }

    auto get_distance_from_bounds() const -> int;
};

struct overmap_with_local_coords {
    overmap* om;
    tripoint_om_omt local;

    auto operator!() const -> bool { return !om; }

    explicit operator bool() const { return !!om; }
};

/**
 * Standard arguments for finding overmap terrain
 * @param origin Location of search
 * @param types vector of Terrain type/matching rule to use to find the type
 * @param exclude_types vector of Terrain type/matching rule to use to find the type
 * @param search_range The [minimum, maximum] search distance. If maximum is 0, a function specific
 * default is used.
 * @param search_layers If set, overrides the search layers to this range. Layer from search origin
 * otherwise.
 * @param min_distance Matches within min_distance are ignored.
 * @param seen If true, only terrain seen by the player should be searched.
 * If false, only terrain not seen by the player should be searched.
 * @param explored If true, only terrain marked as explored by the player should be searched.
 * If false, only terrain not marked as explored by the player should be searched.
 * @param existing_only If true, will restrict searches to existing overmaps only. This
 * is particularly useful if we want to attempt to add a missing overmap special to an existing
 * overmap rather than creating many overmaps in an attempt to find it.
 * @param om_special If set, the terrain must be part of the specified overmap special.
 * @param popup If set, the popup will be periodically updated to indicate ongoing search.
 * @param max_results, If set, limits the search result to at most n entries
 */
struct omt_find_params {
    ~omt_find_params();

    std::vector<std::pair<std::string, ot_match_type>> types;
    std::vector<std::pair<std::string, ot_match_type>> exclude_types;
    std::pair<int, int> search_range = {0, 0};
    std::optional<std::pair<int, int>> search_layers = std::nullopt;
    std::optional<bool> seen = std::nullopt;
    std::optional<bool> explored = std::nullopt;
    bool existing_only = false;
    std::optional<overmap_special_id> om_special = std::nullopt;
    shared_ptr_fast<throbber_popup> popup = nullptr;
    std::optional<int> max_results = std::nullopt;
    bool force_sync = false;
};

constexpr const std::pair<int, int> omt_find_all_layers = {-OVERMAP_DEPTH, OVERMAP_HEIGHT};
constexpr const std::pair<int, int> omt_find_above_ground_layer = {0, OVERMAP_HEIGHT};

/**
 * Standard arguments for finding overmap route
 * @param radius Radius of revealed terrain around the route
 * @param road_only If set, restricts route to only use roads
 * @param popup If set, the popup will be periodically updated to indicate ongoing pathfinding.
 */
struct omt_route_params {
    ~omt_route_params();

    int radius = 0;
    bool road_only = false;
    shared_ptr_fast<throbber_popup> popup = nullptr;
};

class overmapbuffer {
public:
    overmapbuffer();

    /**
     * The dimension this buffer belongs to.  Set by the overmapbuffer
     * registry when creating the slot; used to route I/O to the correct
     * dimension subdirectory without reading global state.
     */
    auto get_dimension_id() const -> const dimension_id& { // *NOPAD*
        return dimension_id_;
    }
    auto set_dimension_id(const dimension_id& id) -> void { dimension_id_ = id; }

    /**
     * Uses overmap coordinates, that means x and y are directly
     * compared with the position of the overmap.
     */
    auto get(const point_abs_om&) -> overmap&;

    /**
     * Save every loaded overmap to disk using @p dim_id for path resolution.
     * Thread-safe when different dimensions save concurrently: distinct paths
     * guarantee no file-level contention.
     */
    auto save(const dimension_id& dim_id) -> void;

    void clear();
    void create_custom_overmap(const point_abs_om&, overmap_special_batch& specials);

    /**
     * Set dimension bounds for pocket dimension overmap rendering.
     * When set, tiles outside the bounds return boundary overmap terrain.
     */
    void set_pocket_info(const pocket_dimension_data& info);
    /**
     * Clear dimension bounds (e.g. when exiting a pocket dimension).
     */
    void clear_pocket_info();

    /**
     * Generates overmap tiles, if missing
     */
    void generate(const std::vector<point_abs_om>& locs);

    /**
     * Returns the overmap terrain at the given OMT coordinates.
     * Creates a new overmap if necessary.
     */
    auto ter(const tripoint_abs_omt& p) -> const oter_id&;
    /**
     * Returns the overmap terrain at the given OMT coordinates.
     * Returns ot_null if the point is not in any existing overmap.
     */
    auto ter_existing(const tripoint_abs_omt& p) -> const oter_id&;
    void ter_set(const tripoint_abs_omt& p, const oter_id& id);
    auto join_used_at(const std::pair<tripoint_abs_omt, cube_direction>&) -> std::string*;
    auto mapgen_args(const tripoint_abs_omt&) -> std::optional<mapgen_arguments>*;
    /**
     * Thread-safe lazy initializer for overmap_special mapgen arguments.
     * Returns the arguments for @p p, initializing them from the special's
     * parameter definitions (using @p md as context) if not yet set.
     * Returns std::nullopt if no args are defined for this position.
     */
    auto get_or_init_mapgen_args(
        const tripoint_abs_omt& p, const mapgendata& md, const std::string& terrain_type_id)
        -> std::optional<mapgen_arguments>;
    /**
     * Uses global overmap terrain coordinates.
     */
    auto has_note(const tripoint_abs_omt& p) -> bool;
    /**
     * Check whether the tile has a note that's marked as dangerous.
     * If such note exists, returns danger radius (may be 0).
     */
    auto has_note_with_danger_radius(const tripoint_abs_omt& p) -> std::optional<int>;
    auto is_marked_dangerous(const tripoint_abs_omt& p) -> bool;
    auto note(const tripoint_abs_omt& p) -> const std::string&;
    void add_note(const tripoint_abs_omt&, const std::string& message);
    void delete_note(const tripoint_abs_omt& p);
    void mark_note_dangerous(const tripoint_abs_omt& p, int radius, bool is_dangerous);
    auto has_extra(const tripoint_abs_omt& p) -> bool;
    auto extra(const tripoint_abs_omt& p) -> const string_id<map_extra>&;
    void add_extra(const tripoint_abs_omt& p, const string_id<map_extra>& id);
    void delete_extra(const tripoint_abs_omt& p);
    auto is_explored(const tripoint_abs_omt& p) -> bool;
    void toggle_explored(const tripoint_abs_omt& p);
    auto is_path(const tripoint_abs_omt& p) -> bool;
    void toggle_path(const tripoint_abs_omt& p);
    auto seen(const tripoint_abs_omt& p) -> bool;
    void set_seen(const tripoint_abs_omt& p, bool seen = true);
    auto has_vehicle(const tripoint_abs_omt& p) -> bool;
    auto has_horde(const tripoint_abs_omt& p) -> bool;
    auto get_horde_size(const tripoint_abs_omt& p) -> int;
    auto get_vehicle(const tripoint_abs_omt& p) -> std::vector<om_vehicle>;
    auto get_settings(const tripoint_abs_omt& p) -> const regional_settings&;
    std::string current_region_type;
    /**
     * Accessors for horde introspection into overmaps.
     * Probably also useful for NPC overmap-scale navigation.
     */
    /**
     * Returns the 3x3 array of scent values surrounding the origin point.
     * @param origin is in world-global omt coordinates.
     */
    auto scents_near(const tripoint_abs_omt& origin) -> std::array<std::array<scent_trace, 3>, 3>;
    /**
     * Method to retrieve the scent at a given location.
     **/
    auto scent_at(const tripoint_abs_omt& pos) -> scent_trace;
    /**
     * Method to set a scent trace.
     * @param loc is in world-global omt coordinates.
     * @param strength sets the intensity of the scent trace,
     *     used for determining if a monster can detect the scent.
     */
    void set_scent(const tripoint_abs_omt& loc, int strength);
    /**
     * Check for any dangerous monster groups at the global overmap terrain coordinates.
     * If there are any, it's not safe.
     */
    auto is_safe(const tripoint_abs_omt& p) -> bool;

    /**
     * Move the tracking mark of the given vehicle.
     * @param veh The vehicle whose tracking device is active and
     * that has been moved.
     * @param old_msp The previous position (before the movement) of the
     * vehicle. In map square coordinates (see vehicle::real_global_pos), it's
     * used to remove the vehicle from the old overmap if the new position is
     * on another overmap.
     */
    void move_vehicle(vehicle* veh, const point_abs_omt& old_omt);
    /**
     * Add the vehicle to be tracked in the overmap.
     */
    void add_vehicle(vehicle* veh);
    /**
     * Remove the vehicle from being tracked in the overmap.
     */
    void remove_vehicle(const vehicle* veh);

    /**
     * Get all npcs in a area with given radius around given central point.
     * Only npcs on the given z-level are considered.
     * Uses square_dist for distance calculation.
     * @param p Central point in submap coordinates.
     * @param radius Maximal distance of npc from (x,y). If the npc
     * is at most this far away from (x,y) it will be returned.
     * A radius of 0 returns only those npcs that are on the
     * specific submap.
     */

    auto get_npcs_near(const tripoint_abs_sm& p, int radius) -> std::vector<shared_ptr_fast<npc>>;
    /**
     * Get all (currently loaded!) npcs that have a companion
     * mission set.
     */
    auto get_companion_mission_npcs(int range = 100) -> std::vector<shared_ptr_fast<npc>>;
    /**
     * Uses overmap terrain coordinates, this also means radius is
     * in overmap terrain.
     * A radius of 0 returns all npcs that are on that specific
     * overmap terrain tile.
     */
    auto get_npcs_near_omt(const tripoint_abs_omt& p, int radius)
        -> std::vector<shared_ptr_fast<npc>>;
    /**
     * Same as @ref get_npcs_near(int,int,int,int) but uses
     * player position as center.
     */
    auto get_npcs_near_player(int radius) -> std::vector<shared_ptr_fast<npc>>;
    /**
     * Find the npc with the given ID.
     * Returns NULL if the npc could not be found.
     * Searches all loaded overmaps.
     */
    auto find_npc(character_id id) -> shared_ptr_fast<npc>;
    /**
     * Get all NPCs active on the overmap
     */
    auto get_overmap_npcs() -> std::vector<shared_ptr_fast<npc>>;
    /**
     * Find npc by id and if found, erase it from the npc list
     * and return it ( or return nullptr if not found ).
     */
    auto remove_npc(const character_id& id) -> shared_ptr_fast<npc>;
    /**
     * Adds the npc to an overmap ( based on the npcs current location )
     * and stores it there. The overmap takes ownership of the pointer.
     */
    void insert_npc(const shared_ptr_fast<npc>& who);

    /**
     * Find all places with the specific overmap terrain type.
     * This function may create a new overmap if needed.
     * @param origin Location of search
     * see omt_find_params for definitions of the terms
     */
    auto find_all(const tripoint_abs_omt& origin, const omt_find_params& params)
        -> std::vector<tripoint_abs_omt>;

private:
    auto find_all_async(const tripoint_abs_omt& origin, const omt_find_params& params)
        -> std::vector<tripoint_abs_omt>;
    auto find_all_sync(const tripoint_abs_omt& origin, const omt_find_params& params)
        -> std::vector<tripoint_abs_omt>;

public:
    /**
     * Returns a random point of specific terrain type among those found in certain search
     * radius.
     * This function may create new overmaps if needed.
     * @param origin Location of search
     * see omt_find_params for definitions of the terms
     */
    auto find_random(const tripoint_abs_omt& origin, const omt_find_params& params)
        -> tripoint_abs_omt;

    /**
     * Returns the closest point of terrain type.
     * @param origin Location of search
     * see omt_find_params for definitions of the terms
     */
    auto find_closest(const tripoint_abs_omt& origin, const omt_find_params& params)
        -> tripoint_abs_omt;


    /**
     * Mark a square area around center on Z-level z
     * as seen.
     * @param center is in absolute overmap terrain coordinates.
     * @param radius The half size of the square to make visible.
     * A value of 0 makes only center visible, radius 1 makes a
     * square 3x3 visible.
     * @param z Z level to make area on
     * @return true if something has actually been revealed.
     */
    auto reveal(const point_abs_omt& center, int radius, int z) -> bool;
    auto reveal(const tripoint_abs_omt& center, int radius) -> bool;
    auto reveal(
        const tripoint_abs_omt& center, int radius,
        const std::function<bool(const oter_id&)>& filter) -> bool;
    auto get_travel_path(
        const tripoint_abs_omt& src, const tripoint_abs_omt& dest, overmap_path_params params)
        -> std::vector<tripoint_abs_omt>;
    auto reveal_route(
        const tripoint_abs_omt& source, const tripoint_abs_omt& dest,
        const omt_route_params& params) -> bool;

    /* These functions return the overmap that contains the given
     * overmap terrain coordinate, and the local coordinates of that point
     * within the overmap (for use with overmap APIs).
     * get_existing_om_global will not create a new overmap and
     * if the requested overmap does not yet exist it returns
     * { nullptr, tripoint_zero }.
     * get_om_global creates a new overmap if needed.
     */
    auto get_existing_om_global(const point_abs_omt& p) -> overmap_with_local_coords;
    auto get_existing_om_global(const tripoint_abs_omt& p) -> overmap_with_local_coords;
    auto get_om_global(const point_abs_omt& p) -> overmap_with_local_coords;
    auto get_om_global(const tripoint_abs_omt& p) -> overmap_with_local_coords;

    /**
     * Pass global overmap coordinates (same as @ref get).
     * @returns true if the buffer has a overmap with
     * the given coordinates.
     */
    auto has(const point_abs_om& p) -> bool;
    /**
     * Get an existing overmap, does not create a new one
     * and may return NULL if the requested overmap does not
     * exist.
     * (x,y) are global overmap coordinates (same as @ref get).
     */
    auto get_existing(const point_abs_om& p) -> overmap*;
    /**
     * Returns whether or not the location has been generated (e.g. mapgen has run).
     * @param loc is in world-global omt coordinates.
     * @returns True if the location has been generated.
     */
    auto is_omt_generated(const tripoint_abs_omt& loc) -> bool;

    using t_point_with_note = std::pair<point_abs_omt, std::string>;
    using t_notes_vector = std::vector<t_point_with_note>;
    auto get_all_notes(int z) -> t_notes_vector {
        return get_notes(z, nullptr); // NULL => don't filter notes
    }
    auto find_notes(int z, const std::string& pattern) -> t_notes_vector {
        return get_notes(z, &pattern); // filter with pattern
    }
    using t_point_with_extra = std::pair<point_abs_omt, string_id<map_extra>>;
    using t_extras_vector = std::vector<t_point_with_extra>;
    auto get_all_extras(int z) -> t_extras_vector {
        return get_extras(z, nullptr); // NULL => don't filter extras
    }
    auto find_extras(int z, const std::string& pattern) -> t_extras_vector {
        return get_extras(z, &pattern); // filter with pattern
    }
    /**
     * Signal nearby hordes to move to given location.
     * @param center The origin of the signal, hordes (that recognize the signal) want to go
     * to there. In global submap coordinates.
     * @param sig_power The signal strength, higher values means it visible farther away.
     */
    void signal_hordes(const tripoint_abs_sm& center, int sig_power);
    /**
     * Signal the nemesis horde to the player's location.
     * @param p The player's location in absolute submap coordinates.
     */
    void signal_nemesis(tripoint_abs_sm p);
    /// Create a new monster group (useful for hordes) and return a pointer to it, or nullptr on
    /// failure.
    auto create_horde(const mongroup& group) -> mongroup*;
    /**
     * Process nearby monstergroups (dying mostly).
     */
    void process_mongroups();
    /**
     * Adds a nemesis horde into the overmap where the kill_nemesis mission is targeted.
     */
    void add_nemesis(const tripoint_abs_omt& p);
    /**
     * Let hordes move a step. Note that this may move monster groups inside the reality bubble,
     * therefore you should probably call @ref map::spawn_monsters to spawn them.
     */
    void move_hordes();
    /**
     * Moves the nemesis horde spawned by the hunted trait across every overmap.
     */
    void move_nemesis();
    /**
     * Removes the nemesis horde on player death.
     */
    void remove_nemesis();
    // hordes -- this uses overmap terrain coordinates!
    auto monsters_at(const tripoint_abs_omt& p) -> std::vector<mongroup*>;
    /**
     * Monster groups at p - absolute submap coordinates.
     * Groups with no population are not included.
     */
    auto groups_at(const tripoint_abs_sm& p) -> std::vector<mongroup*>;

    /**
     * Spawn monsters from the overmap onto the main map (game::m).
     * p is an absolute *submap* coordinate.
     */
    void spawn_monster(const tripoint_abs_sm& p);
    /**
     * Discard all monster_map entries at submap p without placing them.
     * Used at load time to purge stale entries for in-bubble submaps that
     * accumulated across sessions because spawn_monsters() was not called
     * before saving.  The corresponding monsters are already present in
     * critter_tracker; placing the stale entries would create duplicates.
     */
    void discard_monster_map(const tripoint_abs_sm& p);
    /**
     * Despawn the monster back onto the overmap. The monsters position
     * (monster::pos()) is interpreted as relative to the main map.
     */
    void despawn_monster(const monster& critter);
    /**
     * Find radio station with given frequency, search an unspecified area around
     * the current player location.
     * If no matching tower has been found, it returns an object with the tower pointer set
     * to null.
     */
    auto find_radio_station(int frequency) -> radio_tower_reference;
    /**
     * Find all radio stations that can be received around the current player location.
     * All entries in the returned vector are valid (have a valid tower pointer).
     */
    auto find_all_radio_stations() -> std::vector<radio_tower_reference>;
    /**
     * Find all cities within the specified @ref radius.
     * Result is sorted by proximity to @ref location in ascending order.
     */
    auto get_cities_near(const tripoint_abs_sm& location, int radius)
        -> std::vector<city_reference>;
    /**
     * Find the closest city. If no city is close, returns an object with city set to nullptr.
     * @param center The center of the search, the distance for determining the closest city is
     * calculated as distance to this point. In global submap coordinates!
     */
    auto closest_city(const tripoint_abs_sm& center) -> city_reference;

    auto closest_known_city(const tripoint_abs_sm& center) -> city_reference;

    auto get_description_at(const tripoint_abs_sm& where) -> std::string;

    /**
     * Place the specified overmap special directly on the map using the provided location and
     * rotation. Intended to be used when you have a special in hand, the desired location and
     * rotation are known and the special should be directly placed rather than using the overmap's
     * placement algorithm.
     * @param special The overmap special to place.
     * @param p The location to place the overmap special. Absolute overmap terrain coordinates.
     * @param dir The direction to rotate the overmap special before placement.
     * @param must_be_unexplored If true, will require that all of the terrains where the special
     * would be placed are unexplored.
     * @param force If true, placement will bypass the checks for valid placement.
     * @returns If the special was placed, a vector of the points used, else nullopt.
     */
    auto place_special(
        const overmap_special& special, const tripoint_abs_omt& origin, om_direction::type dir,
        bool must_be_unexplored, bool force) -> std::optional<std::vector<tripoint_abs_omt>>;
    /**
     * Place the specified overmap special using the overmap's placement algorithm. Intended to be
     * used when you have a special that you want placed but it should be placed similarly to as if
     * it were created during overmap generation.
     * @param special_id The id of overmap special to place.
     * @param center Used in conjunction with radius to search the specified and adjacent overmaps
     * for a valid placement location. Absolute overmap terrain coordinates.
     * @param min_radius Used in conjunction with center. Absolute overmap terrain units.
     * @param max_radius Used in conjunction with center. Absolute overmap terrain units.
     * @returns True if the special was placed, else false.
     */
    auto place_special(
        const overmap_special_id& special_id, const tripoint_abs_omt& center, int min_radius,
        int max_radius) -> bool;

private:
    dimension_id dimension_id_;
    std::shared_mutex mutex;
    /**
     * Protects all NPC container reads and writes across every overmap in
     * this buffer.  Must always be acquired AFTER @ref mutex (if both are
     * needed).  Held exclusively for insert_npc() / remove_npc() and
     * shared-equivalently for get_npcs_near() family reads.
     *
     * Separate from @ref mutex so that generation workers can call
     * insert_npc() concurrently without blocking overmapbuffer reads.
     */
    mutable std::mutex npc_mutex_;
    /**
     * Protects overmap layer[z].extras and layer[z].notes writes across every
     * overmap in this buffer.  Must be acquired AFTER any @ref mutex operation
     * completes (same ordering rule as npc_mutex_) — in practice both
     * get_om_global() and get_existing_om_global() acquire+release @ref mutex
     * internally, so extras_mutex_ is acquired only after those return.
     *
     * Separate from @ref mutex so that generation workers can call add_extra()
     * and add_note() concurrently without blocking unrelated overmapbuffer reads.
     */
    mutable std::mutex extras_mutex_;
    /**
     * Protects lazy initialization of overmap_special mapgen arguments stored in
     * overmap::mapgen_arg_storage.  Must be acquired AFTER any @ref mutex operation
     * completes — in practice get_om_global() acquires+releases @ref mutex internally,
     * so mapgen_args_mutex_ is acquired only after that returns.
     *
     * Separate from @ref mutex so that generation workers can initialize args
     * concurrently without blocking unrelated overmapbuffer reads.
     */
    mutable std::mutex mapgen_args_mutex_;
    /**
     * Common function used by the find_closest/all/random to determine if the location is
     * findable based on the specified criteria.
     * @param location Location of search
     * see omt_find_params for definitions of the terms
     */
    auto is_findable_location(const tripoint_abs_omt& location, const omt_find_params& params)
        -> bool;
    auto is_findable_location(
        const overmap_with_local_coords& map_loc, const omt_find_params& params) -> bool;

    std::unordered_map<point_abs_om, std::unique_ptr<overmap>> overmaps;
    /**
     * Set of overmap coordinates of overmaps that are known
     * to not exist on disk. See @ref get_existing for usage.
     */
    std::set<point_abs_om> known_non_existing;

    // Optional dimension bounds for pocket dimension rendering
    std::optional<pocket_dimension_data> pocket_info_;
    // Cached resolved overmap terrain id for out-of-bounds tiles
    oter_id bounds_oter_id_;

    // Set of globally unique overmap specials that have already been placed
    std::unordered_set<overmap_special_id> placed_unique_specials;

    /**
     * Get a list of notes in the (loaded) overmaps.
     * @param z only this specific z-level is search for notes.
     * @param pattern only notes that contain this pattern are returned.
     * If the pattern is NULL, every note matches.
     */
    auto get_notes(int z, const std::string* pattern) -> t_notes_vector;
    /**
     * Get a list of map extras in the (loaded) overmaps.
     * @param z only this specific z-level is search for map extras.
     * @param pattern only map extras that contain this pattern are returned.
     * If the pattern is NULL, every map extra matches.
     */
    auto get_extras(int z, const std::string* pattern) -> t_extras_vector;

public:
    /**
     * See overmap::check_ot, this uses global
     * overmap terrain coordinates.
     * This function may create a new overmap if needed.
     */
    auto check_ot(const std::string& otype, ot_match_type match_type, const tripoint_abs_omt& p)
        -> bool;
    auto check_overmap_special_type(const overmap_special_id& id, const tripoint_abs_omt& loc)
        -> bool;
    auto overmap_special_at(const tripoint_abs_omt&) -> std::optional<overmap_special_id>;

    /**
     * These versions of the check_* methods will only check existing overmaps, and
     * return false if the overmap doesn't exist. They do not create new overmaps.
     */
    auto check_ot_existing(
        const std::string& otype, ot_match_type match_type, const tripoint_abs_omt& loc) -> bool;
    auto check_overmap_special_type_existing(
        const overmap_special_id& id, const tripoint_abs_omt& loc) -> bool;

    /**
     * Adds the given globally unique overmap special to the list of placed specials.
     */
    void add_unique_special(const overmap_special_id& id);
    /**
     * Returns true if the given globally unique overmap special has already been placed.
     */
    auto contains_unique_special(const overmap_special_id& id) const -> bool;
    /**
     * Writes the placed unique specials as a JSON value.
     */
    void serialize_placed_unique_specials(JsonOut& json) const;
    /**
     * Reads placed unique specials from JSON and overwrites the global value.
     */
    void deserialize_placed_unique_specials(JsonIn& jsin);

private:
    /**
     * Go thorough the monster groups of the overmap and move out-of-bounds
     * groups to the correct overmap (if it exists), also removes empty groups.
     */
    void fix_mongroups(overmap& new_overmap);
    /**
     * Move the nemesis horde into the overmap matching its absolute position.
     */
    void fix_nemesis(overmap& new_overmap);
    /**
     * Moves out-of-bounds NPCs to the overmaps they should be in.
     */
    void fix_npcs(overmap& new_overmap);
    /**
     * Retrieve overmaps that overlap the bounding box defined by the location and radius.
     * The location is in absolute submap coordinates, the radius is in the same system.
     * The overmaps are returned sorted by distance from the provided location (closest first).
     */
    auto get_overmaps_near(const point_abs_sm& p, int radius) -> std::vector<overmap*>;
    auto get_overmaps_near(const tripoint_abs_sm& location, int radius) -> std::vector<overmap*>;

public:
    /**
     * Gets the electric grid to which a point belongs by walking along the connected tiles.
     * Will always return a non-empty set.
     * @param p Overmap coordinates of the point in the grid
     */
    auto electric_grid_at(const tripoint_abs_omt& p) -> std::set<tripoint_abs_omt>;

    /**
     * Retrieve electric grid connections from given point.
     * Returned vector may be empty.
     */
    auto electric_grid_connectivity_at(const tripoint_abs_omt& p) -> std::vector<tripoint_rel_omt>;

    /**
     * Adds a grid connection between two points. The points must be adjacent.
     */
    auto add_grid_connection(const tripoint_abs_omt& lhs, const tripoint_abs_omt& rhs) -> bool;

    /**
     * Removes a grid connection between two points. The points must be adjacent.
     * Does not fully isolate the points, they can still be in the same grid
     * after this operation.
     */
    auto remove_grid_connection(const tripoint_abs_omt& lhs, const tripoint_abs_omt& rhs) -> bool;
};

// Provides ACTIVE_OVERMAP_BUFFER macro and get_overmapbuffer(dim_id) API.
#include "overmapbuffer_registry.h"
