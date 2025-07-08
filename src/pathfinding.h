#pragma once

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "game_constants.h"
#include "point.h"
#include "rng.h"


// A struct defining abilities of the actor and how to respond to various terrain features
struct PathfindingSettings {
    // Our approximate bash strength is `bash_strength_val` * `bash_strength_quanta`
    // We quantize bash strength to reduce the amount of d_maps created for different mob types, considering the actual bash strength
    //   does not change g-values much
    int bash_strength_val = 0;
    // Our approximate bash strength is `bash_strength_val` * `bash_strength_quanta`
    // We quantize bash strength to reduce the amount of d_maps created for different mob types, considering the actual bash strength
    //   does not change g-values much.
    int bash_strength_quanta = 1;

    // Mulitplier of just raw move cost. 2.0 would mean just movement alone takes 2x time.
    float move_cost_coeff = 1.0;

    // Even if we can bash, multiply time needed to do it by this
    // >1 to make bashes less attractive, <1 to make them more attractive. Do not use negative values.
    float bash_cost = 2.0;

    // Cost of climbing terrain. INFINITY if we can't
    float climb_cost = INFINITY;

    // Cost of moving through a trap tile, INFINITY to avoid completely. `can_fly == true` overrides this value to be 0.
    float trap_cost = 0.;

    // Cost of opening a door. INFINITY to never open doors, otherwise 2 to open and then move in.
    float door_open_cost = INFINITY;

    // Extra penalty for moving through rough terrain
    float rough_terrain_cost = 0.;

    // Extra penalty for moving through sharp terrain
    float sharp_terrain_cost = 0.;

    // If a mob is in the way currently, add this extra cost. INFINITY to always path around other critters.
    float mob_presence_penalty = 0.;

    // Can we fly? This implies we can climb stairs (`can_climb_stairs = true`),
    //   move over trap tiles freely (`trap_cost = 0`)
    //   and travel over open air and go up and down from there
    bool can_fly = false;

    // Can we climb stairs? `can_fly == true` overrides this value to be true.
    bool can_climb_stairs = false;

    // A map of tiles that have an extra G-cost assigned to them. Used for potential fields, preclosed tiles, etc.
    std::unordered_map<point, float> extra_g_costs;

    bool operator==( const PathfindingSettings &rhs ) const = default;
    int z_move_type() const;
};

// A struct defining various coefficient used when creating/calculating a path from a dijikstra map
//   or determining tiles a valid path can cross where valid means at each point in the path the cost function decreases
struct RouteSettings {
    // How directed the pathfinding is. A value of 1.0 makes pathfinding equivalent to A*, 0.0 is raw Dijikstra;
    //   this adjusts precision, high values will converge quicker, but produce a possibly less than shortest path.
    float h_coeff = 1.0;

    // If multiple tiles are valid for a path, where valid means at each point in the path the cost function decreases,
    //   we may want to select one of them randomly to decongest routes across multiple pathfinds.
    // To do so, we'll rank up all the tiles based on their cost with first one being the most optimal path and last one being a least optimal path.
    // This coefficient determines how weights are distributed among these paths
    // -1 -- always choose the longest path (why would you though...)
    //  0 -- choose any tile with no bias [very "flowy" pathing]
    //  1 -- always choose the shortest path [in open terrain, this is just a straight line]
    // Don't bother setting this too close to 1.0, it will just make the path linear with rare single steps away from the shortest path
    float alpha = 1.0;

    unsigned int rank_weighted_rng( const unsigned int n ) const;

    /*
    ```plain
        -----
       /  |r'\
      /   |   \
     /   ---   \
    |   / |r\   |
    |  |  |  |  |
    |--S--m--E--|
    |  |  |  |  |
    | t \ | /   |
     \   ---   /
      \   |   /
       \  |  /
        -----
     ```
    S -- `start`
    m -- `midpoint` (middle point between `start` and `end`)
    E -- `end`
    t -- candidate `t`ile
    r -- `r`adius (euclidean distance from `S` to `E`)
    r' -- search `r`adius (`r` * `search_radius_coeff`)
    Limit our search area to only tiles `t` that are inside circle of radius r'.

    **WARNING**: This necessiates rebuilding dijikstra map due to being relative domain, though g-values won't be recalculated which are generally the most expensive part.
      Additionally, limiting the search area too much might cause the destination to be inaccessible
      which is the worst case for pathfinding as it forces a complete scan of the whole search area though we have workarounds for that.
    Use only if needed.
    */
    float search_radius_coeff = INFINITY;
    // Test if `pos` is in the circle of radius distance from `start` to `end` by `search_radius_coeff` centered at `end`
    constexpr bool is_in_search_radius( const point start, const point pos,
                                        const point end ) const;

    /*
    ```plain
          t
         / .
        /   .
       /     .
      /       .
     /a        .
    S----------E
    ```
    S -- `start`
    E -- `end`
    t -- candidate `t`ile
    a -- `a`ngle of tSE
    Limit our search area to only tiles `t` where `-search_cone_angle` <= `a` <= `search_cone_angle`

    **WARNING**: This necessiates rebuilding dijikstra map due to being relative domain, though g-values won't be recalculated which are generally the most expensive part.
      Additionally, limiting the search area too much might cause the destination to be inaccessible
      which is the worst case for pathfinding as it forces a complete scan of the whole search area though we have workarounds for that.
    Use only if needed.
    */
    float search_cone_angle = 180.0;
    // Test if `pos` is in the cone of `search_cone_angle` projected from `start` to `end`
    constexpr bool is_in_search_cone( const point start, const point pos,
                                      const point end ) const;

    /* Limit our search area such that a path will contain steps only up to this coefficient multiplied by chebyshev distance between start and end.
    In other words, it limits the amount of tiles to step through for any given path.
    Be aware this does **not** limit search domain, which means if nothing else limits it,
      the whole map will be explored and a longer in terms of steps, but less expensive of terms of time path will be rejected even if a valid shorter path exists.
    */
    float max_s_coeff = INFINITY;

    /* Limit our search only to paths whose unbiased f-value is less than
    this coefficient multiplied by the distance between start and end
    if `f_limit_based_on_max_dist` is false
    otherwise we multiply `max_dist` value instead.
    */
    float max_f_coeff = INFINITY;

    // Don't pathfind if target is more than this distance away.
    float max_dist = INFINITY;

    // Do we use distance between start and end or do we use `max_dist`
    //   for f-value limited search domain?
    // Check `max_f_coeff` for more detail.
    bool f_limit_based_on_max_dist = true;

    // Does the search domain depend on start position?
    constexpr bool is_relative_search_domain() const;
};

class Pathfinding
{
    private:
        using val_pair = std::pair<float, point>;

        enum class State {
            UNVISITED, // Tile has not been expanded to yet
            ACCESSIBLE, // Tile is reachable
            IMPASSABLE, // Tile is reachable, but cannot be gone into
            INACCESSIBLE, // Tile is completely unreachable (or outside search area)
            BOUNDS, // Value used to define map edges
        };
        enum class MapDomain {
            RELATIVE_DOMAIN, // Map's search domain limit includes relative limits (that is, depending on start position)
            ABSOLUTE_DOMAIN // Map's search domain limit is centered at the destination
        };
        enum class ExpansionOutcome {
            PATH_FOUND, // Path exists
            TARGET_INACCESSIBLE, // Although pathfinding reached the target, the target is inside some inaccessible location
            PATH_NOT_FOUND, // The map has not been explored fully, but a path may still exist with a wider search area
            NO_PATH_EXISTS, // Map explored fully, no path exists
            UNSET // Internal use
        };

        // Location we can change our Z level with
        struct ZLevelChange {
            enum class Type {
                STAIRS,
                RAMP,
                OPEN_AIR
            };

            tripoint from;
            tripoint to;
            Type type;
        };
        struct ZLevelChangeOpenAirPair {
            std::optional<ZLevelChange> reach_from_below;
            std::optional<ZLevelChange> reach_from_above;
        };

        // Global state: allocated dijikstra d_maps. Pull to `d_maps` from here.
        static std::vector<std::unique_ptr<Pathfinding>> d_maps_store;

        // Global state: memoized dijikstra d_maps. Transfer to `d_maps_store` every game turn.
        static std::vector<std::unique_ptr<Pathfinding>> d_maps;

        // We store the area covered by last Z-scan (in global coords, top left loaded submap)
        // ```
        // -----
        // |1  |
        // | --+---
        // | | |  |
        // --+--  |
        //   |   2|
        //   ------
        // ```
        // If we moved our area from square 1 to square 2, then
        // `ZLevelChange`s that only remain in 1 will be removed
        // `ZLevelChange`s that remain in both 1 and 2 will be shifted so their local coords match square 2
        // and points that are only in 2 will be scanned for new Z-changes.
        static point z_area;

        // Global state: Z-level transitions for each z-level (does not include OPEN_AIR due to being numerous, requiring a different approach)
        static std::array<std::vector<ZLevelChange>, OVERMAP_LAYERS> z_caches;
        // Global state: OPEN_AIR type z-level transitions for each z-level
        static std::array<std::unordered_map<point, ZLevelChangeOpenAirPair>, OVERMAP_LAYERS>
        z_caches_open_air;
        // Global state: We cache `z_path` information taken to prevent multiple iterations for the same target
        static std::map<std::tuple<bool, int, tripoint>, ZLevelChange> cached_closest_z_changes;

        // Smallest adjacent f
        std::array<std::array<float, MAPSIZE_X>, MAPSIZE_Y> p_map;
        // Associated tile's g cost [movement, bashing down...]
        std::array<std::array<float, MAPSIZE_X>, MAPSIZE_Y> g_map;
        // Tile overall state [padded on all sides by 1 tile for bounds checking]
        std::array < std::array < State, MAPSIZE_X + 2 >, MAPSIZE_Y + 2 > tile_state;

        // Which points in maps have we modified thus far? Used for resetting.
        std::vector<point> map_modify_set;
        // Which points in tile state have we modified thus far? Used for resetting.
        std::vector<point> tile_state_modify_set;

        // `dest`ination of this map [2D]
        point dest;
        // `z` level of this map
        int z;
        // `settings` which were used to spawn this map
        PathfindingSettings settings;

        MapDomain domain = MapDomain::RELATIVE_DOMAIN;

        // Is the map already fully explored? UNVISITED tiles become INACCESSIBLE in that case.
        bool is_explored = false;

        // We don't want to calculate dijikstra of the whole map every time,
        //   so we store wave `frontier` to proceed from later if needed
        std::vector<point> unbiased_frontier;

        // Moves we don't allow to happen
        std::set<std::pair<point, point>> forbidden_moves;

        // Possibly shift or move all Z-changes if our `z_area` moved
        //   and scan for new changes.
        // Only process OPEN_AIR changes if `update_open_air` is true. OPEN_AIR tiles are numerous on higher Z levels
        //   so they're expensive to go over and update. Do only for fliers.
        static void update_z_caches( bool update_open_air );

        // Get a reference to ZCache for this level
        static std::vector<ZLevelChange> &get_z_cache( const int z );
        static std::unordered_map<point, ZLevelChangeOpenAirPair> &get_z_cache_open_air( const int z );

        static void produce_d_map( point dest, int z, PathfindingSettings settings );

        // Get `p`-value at `p`
        float &p_at( const point &p );
        // Get `g`-value at `p`
        float &g_at( const point &p );
        // f0 = p + g
        float get_f_unbiased( const point &p );
        // f1 = p + g + `h_coeff` * [distance between `start` and `p`]
        float get_f_biased( const point &p, const point &start, float h_coeff );

        void reset_maps();
        void reset_tile_state();
        State &tile_state_at( const point &p );
        bool in_bounds( const point &p );

        // Determine if `start` is surrounded by already visited tiles in `d_map` or tiles allowed by `route_settings`
        //   and if so, clear and fill `out` with all unexplored tiles left.
        void detect_culled_frontier( const point &start,
                                     const RouteSettings &route_settings,
                                     std::unordered_set<point> &out );

        // Test if `p` is in our limited domain defined by `route_settings` relative to `start`
        bool is_in_limited_domain( const point &start, const point &p,
                                   const RouteSettings &route_settings );

        // See `Pathfinding::route`
        static std::vector<tripoint> get_route_2d(
            const point from, const point to, const int z,
            const PathfindingSettings path_settings,
            const RouteSettings route_settings );
        // See `Pathfinding::route`
        static std::vector<tripoint> get_route_3d(
            const tripoint from, const tripoint to,
            const PathfindingSettings path_settings,
            const RouteSettings route_settings
        );

        // Continue expanding the dijikstra map until we reach `origin` or nothing remains of the frontier. Returns whether a route is present.
        ExpansionOutcome expand_2d_up_to( const point &start, const RouteSettings &route_settings );
    public:
        // get `route` from `from` to `to` if available in accordance to `route_settings` while `path_settings` defines our capabilities, otherwise empty vector.
        // Found route will include `from` and `to`.
        static std::vector<tripoint> route( tripoint from, tripoint to,
                                            const std::optional<PathfindingSettings> path_settings = std::nullopt,
                                            const std::optional<RouteSettings> route_settings = std::nullopt );

        // Reset whole pathfinding pretty much
        static void clear_d_maps();

        // Reset Z-level information. Should only be done when new Z-level changes could have appeared
        //   such as change in terrain
        static void mark_dirty_z_cache();
};

