#pragma once

#include "bodypart.h"
#include "calendar.h"
#include "cata_dynamic_bitset.h"
#include "coordinates.h"
#include "dimension_info.h"
#include "enums.h"
#include "filter_utils.h"
#include "game_constants.h"
#include "hash_utils.h"
#include "item.h"
#include "item_stack.h"
#include "legacy_pathfinding.h"
#include "lightmap.h"
#include "line.h"
#include "lru_cache.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "mapgen/mapgen_functions.h"
#include "memory_fast.h"
#include "shadowcasting.h"
#include "sounds.h"
#include "submap_load_manager.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/vpart_position.h"

#include <array>
#include <bitset>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>


enum class spawn_disposition;
struct scent_block;
template <typename T> class string_id;

namespace catacurses {
class window;
} // namespace catacurses
class active_tile_data;
class Character;
class Creature;
class character_id;
class computer;
class field;
class field_entry;
class map_cursor;
class mapbuffer;
class mapgendata;
class monster;
class optional_vpart_position;
class player;
class submap;
template <typename Tripoint> class tripoint_range;
class vehicle;
class zone_data;
struct maptile;
struct partial_con;
struct trap;

enum class special_item_type : int;
class npc_template;
class tileray;
class vpart_reference;
struct mongroup;
struct projectile;
struct veh_collision;

template <typename T> class visitable;

struct wrapped_vehicle {
    tripoint_bub_ms pos;
    vehicle* v;
};

using VehicleList = std::vector<wrapped_vehicle>;
class map;

enum ter_bitflags : int;
struct pathfinding_settings;
template <typename T> struct weighted_int_list;
struct rl_vec2d;
struct sound_event;

/** Causes all generated maps to be empty grass and prevents saved maps from being loaded, used by
 * the test suite */
extern bool disable_mapgen;

namespace cata {
template <class T> class poly_serialized;
} // namespace cata

struct map_stack_options {
    location_vector<item>* stack = nullptr;
    tripoint_abs_ms location;
    mapbuffer* origin = nullptr;
    map* local_origin = nullptr;
};

struct map_move_furn_options {
    Character* mover = nullptr;
    bool pulling = false;
    bool move_contents = true;
    bool move_fire = true;
};

class map_stack: public item_stack {
private:
    tripoint_abs_ms location;
    mapbuffer* myorigin = nullptr;
    map* local_origin = nullptr;

    auto local_location() const -> tripoint_bub_ms;

public:
    explicit map_stack(const map_stack_options& options)
        : item_stack(options.stack),
          location(options.location),
          myorigin(options.origin),
          local_origin(options.local_origin) {}
    void insert(detached_ptr<item>&& newitem) override;
    auto erase(const_iterator it, detached_ptr<item>* out = nullptr) -> iterator override;
    auto remove(item* to_remove) -> detached_ptr<item> override;
    auto clear() -> std::vector<detached_ptr<item>> override;
    auto count_limit() const -> int override { return MAX_ITEM_IN_SQUARE; }
    auto max_volume() const -> units::volume override;
};

struct visibility_variables {
    // Is this struct initialized for current z-level
    bool variables_set = false;
    bool u_sight_impaired = false;
    bool u_is_boomered = false;
    // Cached values for map visibility calculations
    int g_light_level = 0;
    int u_clairvoyance = 0;
    int u_unimpaired_range = 0;
    float vision_threshold = 0.0f;
    float visibility_range = 60.0f;
    float detail_range = 60.0f;
    float visibility_scale_factor = 1.0f;
};

struct bash_params {
    // Initial strength
    int strength;
    // Make a sound?
    bool silent;
    // Essentially infinite bash strength + some
    bool destroy;
    // Do we want to bash floor if no furn/wall exists?
    bool bash_floor;
    /**
     * Value from 0.0 to 1.0 that affects interpolation between str_min and str_max
     * At 0.0, the bash is against str_min of targeted objects
     * This is required for proper "piercing" bashing, so that one strong hit
     * can destroy a wall and a floor under it rather than only one at a time.
     */
    float roll;
    /*
     * Are we bashing this location from above?
     * Used in determining what sort of terrain the location will turn into,
     * since if we bashed from above and destroyed it, it probably shouldn't
     * have a roof either.
     */
    bool bashing_from_above;
    /**
     * Hack to prevent infinite recursion.
     * TODO: Remove, properly unwrap the calls instead
     */
    bool do_recurse = true;
    // Was this bash action directly caused by the avatar?
    bool caused_by_player = false;
};

struct bash_results {
    bash_results(bool did_bash, bool success, bool bashed_solid)
        : did_bash(did_bash),
          success(success),
          bashed_solid(bashed_solid) {}
    bash_results() = default;
    // Was anything hit?
    bool did_bash = false;
    // Was anything destroyed?
    bool success = false;
    // Did we bash furniture, terrain or vehicle
    bool bashed_solid = false;
    // If there was recurrent bashing, it will be here
    std::vector<bash_results> subresults;

    auto operator|=(const bash_results& other) -> bash_results&;
};

/** Draw parameters used by map::drawsq() and similar methods. */
struct drawsq_params {
private:
    tripoint_bub_ms view_center = tripoint_bub_ms(tripoint_min);
    bool do_highlight = false;
    bool do_show_items = true;
    bool do_low_light = false;
    bool do_bright_light = false;
    bool do_memorize = false;
    bool do_output = true;

public:
    constexpr drawsq_params() = default;

    /**
     * Highlight the tile. On TILES, draws an overlay; on CURSES, inverts color.
     * Default: false.
     */
    //@{
    constexpr auto highlight(bool v) -> drawsq_params& {
        do_highlight = v;
        return *this;
    }
    constexpr auto highlight() const -> bool { return do_highlight; }
    //@}

    /**
     * Whether to draw items on the tile.
     * Default: true.
     */
    //@{
    constexpr auto show_items(bool v) -> drawsq_params& {
        do_show_items = v;
        return *this;
    }
    constexpr auto show_items() const -> bool { return do_show_items; }
    //@}

    /**
     * Whether tile is low light, and should be drawn with muted color.
     * Default: false.
     */
    //@{
    constexpr auto low_light(bool v) -> drawsq_params& {
        do_low_light = v;
        return *this;
    }
    constexpr auto low_light() const -> bool { return do_low_light; }
    //@}

    /**
     * Whether tile is in bright light. Affects NV overlay, and nothing else.
     * Default: false;
     */
    //@{
    constexpr auto bright_light(bool v) -> drawsq_params& {
        do_bright_light = v;
        return *this;
    }
    constexpr auto bright_light() const -> bool { return do_bright_light; }
    //@}

    /**
     * Whether the tile should be memorized. Used only in map::draw().
     * Default: false.
     */
    //@{
    constexpr auto memorize(bool v) -> drawsq_params& {
        do_memorize = v;
        return *this;
    }
    constexpr auto memorize() const -> bool { return do_memorize; }
    //@}

    /**
     * HACK: Whether the tile should be printed. Used only in map::draw()
     * as a hack for memorizing off-screen tiles.
     * Default: true.
     */
    //@{
    constexpr auto output(bool v) -> drawsq_params& {
        do_output = v;
        return *this;
    }
    constexpr auto output() const -> bool { return do_output; }
    //@}

    /**
     * Set view center.
     * Default: uses avatar's current view center.
     */
    //@{
    constexpr auto center(const tripoint_bub_ms& p) -> drawsq_params& {
        view_center = p;
        return *this;
    }
    constexpr auto center_at_avatar() -> drawsq_params& {
        view_center = tripoint_bub_ms(tripoint_min);
        return *this;
    }
    auto center() const -> tripoint_bub_ms;
    //@}
};

struct level_cache {
    // Zeros all relevant values.
    // Default constructor creates a zero-sized cache used as a null sentinel only.
    level_cache();
    // Normal constructor: mx = SEEX * mapsize, my = SEEY * mapsize.
    explicit level_cache(int mx, int my);
    level_cache(const level_cache& other) = default;
    auto operator=(const level_cache& other) -> level_cache& = default;

    // Runtime dimensions for this cache.
    // cache_x = SEEX * mapsize, cache_y = SEEY * mapsize, cache_mapsize = mapsize.
    int cache_x = 0;
    int cache_y = 0;
    int cache_mapsize = 0;

    /// Flat index for tile-coordinate arrays: vec[x * cache_y + y].
    /// Uses the runtime cache_y stride (= SEEY * mapsize) so that all
    /// vector accesses correctly reflect the actual loaded-area dimensions.
    auto idx(int x, int y) const -> int { return x * cache_y + y; }
    // Flat index for submap-coordinate bitsets: bitset[sx * cache_mapsize + sy]
    auto bidx(int sx, int sy) const -> int { return sx * cache_mapsize + sy; }
    /// True if the tile-coordinate point_bub_ms & is within this cache's rendered area.
    auto inbounds(const point_bub_ms& p) const -> bool {
        return p.x() >= 0 && p.x() < cache_x && p.y() >= 0 && p.y() < cache_y;
    }

    // ---- per-submap dirty bitsets (size: cache_mapsize²) ----
    cata_dynamic_bitset transparency_cache_dirty;
    cata_dynamic_bitset outside_cache_dirty;
    cata_dynamic_bitset floor_cache_dirty;
    // FIX ABSORPTION AND WALL CACHE CALLS
    // absorption_cache_dirty is for tile sound absorption checking/rebuild purposes.
    // Should be set for a tile position if the tile in question changes significantly, or if a tile
    // feature that affects sound propagation is added/removed.
    cata_dynamic_bitset absorption_cache_dirty;
    cata_dynamic_bitset sound_wall_cache_dirty;

    bool seen_cache_dirty = false;
    // Set by map mutations and dynamic light-state changes; cleared after
    // generate_lightmap completes for this level.
    bool lightmap_dirty = true;
    // True when CPU lm contains current lighting for the whole level. SDL GPU
    // lighting may keep resident GPU lm current while leaving this false.
    bool lm_cpu_cache_valid = false;
    // Incremented whenever CPU lm contents are invalidated before a rebuild.
    uint64_t lm_cpu_cache_generation = 0;
    // Per-level visibility dirtiness. The map-level aggregate flag is the source
    // of truth for gameplay consumers that need completed player visibility.
    bool visibility_cache_dirty = true;
    // Set by build_floor_cache; true when at least one tile has a floor.
    bool has_any_floor = true;
    bool has_any_vehicle_floor = false;
    bool suspension_cache_initialized = false;
    bool suspension_cache_dirty = false;
    // Vehicle floor cache dirty: set when vehicle below moves
    bool vehicle_floor_cache_dirty = false;
    // Vehicle cache dirty: set when vehicle here moves
    bool vehicle_caches_dirty = false;
    std::list<point_abs_ms> suspension_cache;

    // ---- 12 tile-coordinate arrays (size: cache_x * cache_y) ----
    // All indexed as: vec[x * cache_y + y]  (X-outer layout, matching old C-array
    // [MAPSIZE_X][MAPSIZE_Y])
    std::vector<float> lm;
    std::vector<float> sm;
    // To prevent redundant ray casting into neighbors: precalculate bulk light source positions.
    // This is only valid for the duration of generate_lightmap
    std::vector<float> light_source_buffer;
    std::vector<float> colored_light_source_buffer;
    std::vector<uint32_t> light_source_color_buffer;
    // Source tiles touched in light_source_buffer.
    std::vector<point_bub_ms> light_source_points;

    // True when the tile has sky access via the 3×3 overhang rule (top-down floor cascade).
    // False means fully enclosed — protected from rain, wind, weather effects.
    std::vector<char> outside_cache;

    // True when at least one tile within 3×3 above has overhead coverage (floor or sheltered
    // tile at z+1).  Distinct from outside_cache: a tile can be outside yet sheltered (overhang).
    std::vector<char> sheltered_cache;

    // true when vehicle below has "ROOF" or "OPAQUE" part, furniture below has "SUN_ROOF_ABOVE"
    //      or terrain doesn't have "NO_FLOOR" flag
    // false otherwise
    // i.e. non-zero == has floor
    // Stored as char (not bool) for contiguous storage; non-zero means true.
    std::vector<char> floor_cache;

    // Subset of floor_cache: entries contributed by vehicle ROOF or OPAQUE parts only
    // (set by vehicle_caching_internal_above).  Used to exclude vehicle-derived floors
    // from floor_crossing_blocked and to stamp a fixed roof-shadow pass.
    // Stored as char for contiguous storage; non-zero means true.
    std::vector<char> vehicle_floor_cache;

    // stores cached transparency of the tiles
    // units: "transparency" (see LIGHT_TRANSPARENCY_OPEN_AIR)
    std::vector<float> transparency_cache;

    // true when light entering a tile diagonally is blocked by the walls of a turned vehicle. The
    // direction is the direction that the light must be travelling. check the nw value of x+1, y+1
    // to find the se value of a tile and the ne of x-1, y+1 for sw
    std::vector<diagonal_blocks> vehicle_obscured_cache;

    // same as above but for obstruction rather than light
    std::vector<diagonal_blocks> vehicle_obstructed_cache;

    // stores "visibility" of the tiles to the player
    // values range from 1 (fully visible to player) to 0 (not visible)
    std::vector<float> seen_cache;

    // same as `seen_cache` (same units) but contains values for cameras and mirrors
    // effective "visibility_cache" is calculated as "max(seen_cache, camera_cache)"
    std::vector<float> camera_cache;

    // stores resulting apparent brightness to player, calculated by map::apparent_light_at
    std::vector<lit_level> visibility_cache;

    std::vector<uint32_t> colored_light_cache;
    bool colored_light_cache_active = false;

    // per-tile map-memory seen bitset (size: cache_x * cache_y), indexed [x + y * cache_x]
    cata_dynamic_bitset map_memory_seen_cache;
    std::vector<tripoint_bub_ms> map_memory_seen_cache_dirty_points;
    bool map_memory_seen_cache_dirty_all = true;

    bool veh_in_active_range = false;
    std::vector<bool> veh_exists_at;
    std::map<tripoint_bub_ms, std::pair<vehicle*, int>> veh_cached_parts;
    std::set<vehicle*> vehicle_list;
    std::set<vehicle*> zone_vehicles;

    // stores cached sound absorption amounts of tiles
    // In 100ths of decibels
    // This is in level_cache instead of sound cache as this is a function of terrain,
    // and we dont want to regenerate this for every single sound.
    std::vector<short> absorption_cache;
    // sound_wall_cache is set alongside the absorption cache, and is used during the process for
    // floodfilling sounds.
    std::vector<bool> sound_wall_cache;
};

// Use the vector sounds_caches for most purposes when working with sounds in
// reference to a specific position or checking multiple sounds. Each
// sound_instance_cache is an originating sound_event, a "volume" short that
// stores the mdB volume of that sound event at that map position, and some
// sorting bools. These are kept until they have been heard by both monsters and
// the player, and are then discarded/removed from the sounds_caches vector.
struct sound_instance_cache {

    // Default constructor creates a zero-sized cache used as a null sentinel only.
    sound_instance_cache();

    explicit sound_instance_cache(
        sound_event& input_sound, const sound_vol_for_flood_dist& d_e, const int& f_r);
    sound_instance_cache(const sound_instance_cache& other) = default;
    auto operator=(const sound_instance_cache& other) -> sound_instance_cache& = default;

    // The originating sound, includes volume @1m, tripoint, description, type,
    // etc.
    sound_event sound;

    sound_vol_for_flood_dist dist_enum;

    // "radius" around the origin to flood a sound through.
    // The flood envelope is 1 + (radius * 2) on a side.
    // Set based on the dist_enum above, and the distance setting for each enum in sound_cache.
    // radius 7 equates to a 15x15 area, radius 3 is a 7x7 area.
    // However as our index point starts at local x/y = 0,
    // our bounds are actually radius * 2, radius * 2
    int flood_radius = 3;

    // Normal tripoint origin of the sound instance.
    tripoint_bub_ms origin;

    // The tripoint that corresponds to index location 0.
    // Calculated off the origin point - flood radius to x and y.
    tripoint_bub_ms envelope_index_point;

    // Offsets are used to get the right envelope index when using bubble tripoints.

    // The numerical offset between index_point.x and 0. Must be calced on sound instance creation.
    int offset_x;
    // the numerical offset between index_point.y and 0. Must be calced on sound instance creation.
    int offset_y;

    // Volume in 100ths of a dB (mdB) of the sound in question
    // Indexed as: vec[x * (flood_radius * 2) + y]
    // The origin point is always the center tile, indexed at (flood_radius * (flood_radius * 2) +
    // flood_radius) This has to be fully initialized when the sound is made, to the desired flood
    // envelope.
    std::vector<short> volume;

    // The base volume level in mdB to use for long distance sound, by direction defaulting to 0.
    // Determined by the highest volume on a tile along the envelope boundary in that respective
    // direction.
    //              0 1 2
    // Indexed as   7 @ 3 where @ is the source, indexes 8 and 9 are Down and Up escapes,
    // respectively.
    //              6 5 4
    // Index to use is determined by general direction from source to requester.
    std::array<short, 10> base_distance_vol_by_dir = {{0}};

    // Flat index for tile-coordinate arrays: vec[x * (flood_radius * 2) + y].
    // Returns index location if provided with x and y corrected to envelope quards.
    // vector accesses DOES NOT REFLECT ACTUAL LOADED-AREA DIMENSIONS.
    // auto idx(int x, int y) const -> int { return x * (2 * flood_radius) + y; }

    // Returns corresponding flood envelope volume index provided a relative point.
    // Use carefully.
    auto env_index(const point_rel_ms& p) const -> int {
        return ((p.x()) * ((2 * flood_radius) + 1) + (p.y()));
    }

    // Returns the corresponding flood envelope volume index provided a bubble point.
    auto p_to_env_index(const point_bub_ms& p) const -> int {
        return ((p.x() - offset_x) * ((2 * flood_radius) + 1) + (p.y() - offset_y));
    }
    // Returns the corresponding flood envelope volume index provided a bubble tripoint.
    auto p_to_env_index(const tripoint_bub_ms& p) const -> int {
        return ((p.x() - offset_x) * ((2 * flood_radius) + 1) + (p.y() - offset_y));
    }

    // Returns true if a given bubble tripoint is inside our envelope.
    // X and Y offsets taken from our index point, the bottom left corner of our envelope.
    auto in_envelope(const tripoint_bub_ms& tp) const -> bool {
        return (tp.x() - offset_x) >= 0 && (tp.y() - offset_y) >= 0
            && (tp.x() - offset_x) < get_flood_envelope_by_enum(dist_enum)
            && (tp.y() - offset_y) < get_flood_envelope_by_enum(dist_enum);
    }
    // Returns true if a given bubble point is inside our envelope.
    // X and Y offsets taken from our index point, the bottom left corner of our envelope.
    auto in_envelope(const point_bub_ms& tp) const -> bool {
        return (tp.x() - offset_x) >= 0 && (tp.y() - offset_y) >= 0
            && (tp.x() - offset_x) < get_flood_envelope_by_enum(dist_enum)
            && (tp.y() - offset_y) < get_flood_envelope_by_enum(dist_enum);
    }

    // Returns true if a given point is on the border of the flood envelope.
    auto on_envelope_border(const point_bub_ms& p) const -> bool {
        return (p.x() - offset_x) == 0 || (p.x() - offset_x) == (flood_radius * 2)
            || (p.y() - offset_y) == 0 || (p.y() - offset_y) == (flood_radius * 2);
    }

    // Checks if a bubble tripoint is within the floodfill envelope. Returns the volume if true, -1
    // if not.
    auto vol_at_tri(const tripoint_bub_ms& tri) const -> short {
        return (in_envelope(tri) ? volume[p_to_env_index(tri.xy())] : -1);
    }

    // NPCs/Monsters/the Player all get a chance to hear a sound.
    // After everyone has heard the sound, it is deleted.
    // This requires a little bit of juggling.

    // Has a sound been heard by the player?
    bool heard_by_player = false;
    // Has a sound been heard by the monsters?
    bool heard_by_monsters = false;
    // Has a sound been heard by the NPCs?
    bool heard_by_npcs = false;

    // Is this noise from movement? For quick filtering, so monsters dont hear
    // their own footsteps and NPCs dont investiage a noise they should know is
    // wandering zombies.
    bool movement_noise = false;
    // Dis the player make this noise? for quick filtering.
    bool from_player = false;
    // Did a monster make this noise? For quick filtering.
    bool from_monster = false;
    // Did an NPC make this noise? For quick filtering.
    bool from_npc = false;
    // If the noise was not made by the player, a monster, or an NPC, it is
    // ambient or enviornmental.

    // Was the source of our sound indoors?
    bool source_indoors = false;
    // If the source of our sound was indoors, did it ever escape to an outside tile?
    // Used for approximating sound reduction of a large sealed room without having to actually
    // floodfill an entire floor of the necropolis or something.
    bool escaped_indoors = false;

    // mdB spl absorption per tile value of the local terrain at the sound source tile.
    short terrain_sound_absorbtion_at_source = 0;
    // The approximated tile distance until a sound reaches a volume of 20dB spl based on the
    // maximum escape vol. Will always be atleast the flood radius, can be bonkers huge. For use
    // with monsters as a easy distance filter. Goodhearing monsters always ignore this and check
    // anyways.
    int approximate_minvol_distance = 3;
};

// These are used to filter against the vector of sound instances
struct sound_filter_key {
    sound_filter_key();
    sound_filter_key(const sound_filter_key& other) = default;
    auto operator=(const sound_filter_key& other) -> sound_filter_key& = default;

    // Ignore sounds of this category or less. Defaults to weather.
    sounds::sound_t category = sounds::sound_t::weather;
    mfaction_str_id monfaction = mfaction_str_id("");
    // Not currently implimented, does this monster belong to an NPC faction? i.e., a robot owned by
    // some survivors, someones dog, etc. faction_id npc_faction = faction_id( "no_faction" ); Is
    // the monster something like a zombie that uses simpler logic and gets angry at everything.
    bool horde_monster = false;
    // For whatever reason, does this monster ignore movement noise?
    bool ignore_movement = false;
    // Is the monster afraid of noise?
    bool noise_fear = false;
    // Does the monster get angry at noises?
    bool noise_angers = false;

    auto operator==(const sound_filter_key& other) const -> bool {
        return (
            category == other.category && monfaction == other.monfaction
            && horde_monster == other.horde_monster && ignore_movement == other.ignore_movement
            && noise_fear == other.noise_fear && noise_angers == other.noise_angers);
    }
};

template <> struct std::hash<sound_filter_key> {
    auto operator()(const sound_filter_key& key) const -> std::size_t {
        using cata::hash_combine;
        using std::size_t;
        using std::string;
        const int cat_int = static_cast<int>(key.category);
        const std::string stringfac = static_cast<std::string>(key.monfaction.str());

        std::size_t seed = 0;
        hash_combine(seed, cat_int);
        hash_combine(seed, stringfac);
        hash_combine(seed, key.horde_monster);
        hash_combine(seed, key.ignore_movement);
        hash_combine(seed, key.noise_fear);
        hash_combine(seed, key.noise_angers);
        return seed;
    }
};

// One sound_cache to rule them all.
// TODO: Make it so that each sound has a pointer or ref? Pointers need to be killed when sounds
// expire
struct sound_cache {

    sound_cache();
    // sound_cache(const sound_cache &) = default;
    // sound_cache(sound_cache &&) = default;
    auto operator=(const sound_cache&) -> sound_cache& = default;
    // sound_cache &operator=(sound_cache &&) = default;

    std::vector<sound_instance_cache> sound_instances;

    // Return the radius to flood a sound out to from the provided enum.
    auto flood_radius_by_enum(const enum sound_vol_for_flood_dist& dist_enum) const -> int {
        return get_flood_radius_by_enum(dist_enum);
    }
    // Return a sorting enum provided a dB volume short.
    auto flood_dist_enum_by_volume(const short& dB_vol) const -> sound_vol_for_flood_dist {
        return get_flood_dist_enum(dB_vol);
    }
    // Generated and checked against while informing monster AI of sounds.
    // MUST be cleared after all monsters are informed of sounds, or if the sound cache gets culled.
    // Wanted this to contain a vector of pointers, but then we would always loose the sounds it
    // pointed to. So instead each is a vector of iterator numbers for the sound_instances vector.
    // If we have more sounds than short can point to as an iterator, something is very wrong and
    // the bad memory access crashing the game is probably doing us a favor.
    std::unordered_map<sound_filter_key, std::vector<short>> sound_list_filtered;
    // Adds a filter kay and pointer vector pair to the filtered sound list.
    // We do want to make copies, as any reference made when informing a monster AI of sounds would
    // go out of scope when we move to the next monster.
    // auto add_filtered_sound_list(const sound_filter_key &key, const std::vector<short> &list ) ->
    // void { sound_list_filtered.insert({key,list}); }
    // True if there is a matching list, false if not.
    // auto matching_filtered_list(const sound_filter_key &key) -> bool { return
    // sound_list_filtered.contains(key); }

    // Clear the filtered list so we dont try to grab an old sound.
    // auto clear_all_filtered_lists() -> void { sound_list_filtered.clear(); }

    // For debug purposes. These are incremented by their respective sound functions, and zero'ed
    // during sounds::clear_floodfill_que() If soundperf during game::do_turn(), these will still be
    // zeroed but the debug diagnostic message will not print.
    short sounds_this_turn = 0;
    short attempted_monster_sounds = 0;
    short attempted_NPC_sounds = 0;
    short attempted_movement_sounds = 0;
    short attempted_potential_deafening_sounds = 0;
    short attempted_non_batch_floodfills = 0;
    short batch_flooded_monster_sounds = 0;
    short batch_flooded_NPC_sounds = 0;
    short invalidated_batch_sounds = 0;
    short filtered_sound_lists_made = 0;
    short filtered_sound_lists_cleared = 0;
    short prior_turn_sound_vector_size = 0;
    short sounds_culled_this_turn = 0;
};

/**
 * Manage and cache data about a part of the map.
 *
 * Despite the name, this class isn't actually responsible for managing the map as a whole. For that
 * function, see \ref mapbuffer. Instead, this class loads a part of the mapbuffer into a cache, and
 * adds certain temporary information such as lighting calculations to it.
 *
 * To understand the following descriptions better, you should also read \ref map_management
 *
 * The map coordinates always start at (0, 0) for the top-left and end at (map_width-1,
 * map_height-1) for the bottom-right.
 *
 * The actual map data is stored in `submap` instances. These instances are managed by `mapbuffer`.
 * Non-owning references to the currently active submaps are cached by `map`:
 *     0 1 2
 *     3 4 5
 *     6 7 8
 * In this example, the top-right submap would be at cache slot 2.
 *
 * When the player moves between submaps, the whole map is shifted, so that if the player moves one
 * submap to the right, (0, 0) now points to a tile one submap to the right from before
 */
class map: public submap_load_listener {
    friend class editmap;
    friend class mapbuffer;
    friend class visitable<map_cursor>;
    friend class location_visitable<map_cursor>;

public:
    using interacting_entity = std::variant<monster*, Character*>;
    using const_interacting_entity = std::variant<const monster*, const Character*>;

    // Constructors & Initialization
    map(int mapsize = MAPSIZE);

    virtual ~map();

    auto operator=(const map&) -> map& = delete;
    auto operator=(map&&) noexcept -> map&;

    /**
     * Resize the map's internal grid and level-caches to @p new_mapsize.
     *
     * The map MUST be unloaded (all grid pointers null) before calling.
     * Called from game::setup() after init_bubble_config() sets g_mapsize
     * so that pimpl<map>'s lightweight initial allocation is replaced with
     * the player-configured bubble size.
     */
    auto resize(int new_mapsize) -> void;

    /**
     * Return the dimension ID this map is currently bound to.
     * An empty string means the primary (default) dimension.
     */
    auto get_bound_dimension() const -> const dimension_id& { return bound_dimension_; }

    /**
     * Return true if the submap containing local position @p p is actively
     * simulated (i.e. covered by a non-lazy_border load request).
     * Use this instead of inbounds() when the question is "should gameplay
     * logic process this position?" rather than "is this position in the
     * render-area cache?".
     */
    auto is_position_simulated(const tripoint_bub_sm& p) const -> bool;
    auto is_position_simulated(const tripoint_bub_ms& p) const -> bool {
        return is_position_simulated(project_to<coords::sm>(p));
    }

    /**
     * Bind this map to a specific dimension.
     * Should be called when the player transitions to another dimension.
     */
    auto bind_dimension(const dimension_id& dim) -> void;

    /**
     * Return true if the submap at absolute-submap coordinates @p pos
     * falls within the current loaded region of this map.
     */
    auto contains_abs_sm(const tripoint_abs_sm& p) const -> bool;

    // submap_load_listener implementation
    void on_submap_loaded(const tripoint_abs_sm& pos, const dimension_id& dim_id) override;
    void on_submap_unloaded(const tripoint_abs_sm& pos, const dimension_id& dim_id) override;

    /**
     * Sets a dirty flag on the a given cache.
     *
     * If this isn't set, it's just assumed that
     * the cache hasn't changed and
     * doesn't need to be updated.
     */
    /*@{*/

    // This will also set the z_levels sound absorption cache to dirty as is almost certainly
    // invalidated as well.
    void set_transparency_cache_dirty(const int zlev);

    // more granular version of the transparency cache invalidation
    // preferred over map::set_transparency_cache_dirty( const int zlev )
    // p is in local coords ("ms")
    void set_transparency_cache_dirty(const tripoint_bub_ms& p);

    // Invalidates a specific location's (p, in local cords "ms") absorption_cache and marks it for
    // recalculation. Should be called whenever a tile's (or its contents) ability to absorb sound
    // significantly changes. For example if wind blocking furniture is added or removed, the tile
    // is set to a tile type with wind blocking, if a tile is set to a type with very high
    // absorption, etc.
    void set_absorption_cache_dirty(const tripoint_bub_ms& p);
    // Set an entire zlevel's sound absorption cache to dirty.
    void set_absorption_cache_dirty(const int zlev);

    // invalidates seen cache for the whole zlevel unconditionally

    void set_seen_cache_dirty(const tripoint_bub_ms& change_location);

    void set_seen_cache_dirty(const int& zlevel);

    void set_outside_cache_dirty(const int zlev);
    // Point-level: marks only the tile's submap + boundary neighbours (max 4).
    void set_outside_cache_dirty(const tripoint_bub_ms& p);

    void set_floor_cache_dirty(const int zlev);
    void set_vehicle_cache_dirty(const int zlev);
    // Point-level: marks only the tile's own submap (no horizontal neighbour dependency).
    void set_floor_cache_dirty(const tripoint_bub_ms& p);

    void set_suspension_cache_dirty(const int zlev);

    /// Mark the per-submap pf_cache dirty for all submaps on zlev.
    /// Use the tripoint overload for single-tile changes.
    void set_pathfinding_cache_dirty(int zlev);
    /// Mark the per-submap pf_cache dirty for the single submap containing p.
    void set_pathfinding_cache_dirty(const tripoint_bub_ms& p);
    /*@}*/

    void set_memory_seen_cache_dirty(const tripoint_bub_ms& p);
    auto set_memory_seen_cache_dirty(int zlev) -> void;
    auto is_memory_seen_cache_dirty_all(int zlev) const -> bool;
    auto take_memory_seen_cache_dirty_points(int zlev) -> std::vector<tripoint_bub_ms>;
    auto mark_memory_seen_cache_dirty_all_clean(int zlev) -> void;

    auto is_map_cache_valid(const int zlev) -> bool;
    void invalidate_map_cache(const int zlev);

    /// Mark lightmap_dirty for every loaded z-level.
    void invalidate_lightmap_caches();

    auto mark_visibility_cache_dirty(int zlev) -> void;
    auto mark_visibility_caches_clean() -> void;
    auto visibility_caches_dirty() const -> bool;
    /// Mark visibility_cache_dirty for every loaded z-level.
    void invalidate_visibility_caches();

    auto check_seen_cache(const tripoint_bub_ms& p) const -> bool;
    auto check_and_set_seen_cache(const tripoint_bub_ms& p) const -> bool;

    /**
     * Callback invoked when a vehicle has moved.
     * sm_min/sm_max are the bounding submap grid coords of the vehicle footprint
     * (union of old and new positions); smz is the z-level.
     * TODO: migrate to using proper z-levels
     */
    void on_vehicle_moved(
        const tripoint_bub_sm& sm_min, const tripoint_bub_sm& sm_max, const int& smz);

    struct apparent_light_info {
        bool obstructed;
        float apparent_light;
    };
    /** Helper function for light claculation; exposed here for map editor
     */
    static auto apparent_light_helper(
        const level_cache& map_cache, const tripoint_bub_ms& p, float visibility_scale_factor)
        -> apparent_light_info;
    /** Determine the visible light level for a tile, based on light_at
     * for the tile, vision distance, etc
     *
     * @param p The tile on this map to draw.
     * @param cache Currently cached visibility parameters
     */
    auto apparent_light_at(const tripoint_bub_ms& p, const visibility_variables& cache) const
        -> lit_level;
    auto apparent_light_at(
        const tripoint_bub_ms& p, const visibility_variables& cache, int dist) const -> lit_level;
    auto get_visibility(lit_level ll, const visibility_variables& cache) const -> visibility_type;

    // See field.cpp
    auto get_wind_blockers(const int& winddirection, const tripoint_bub_ms& pos)
        -> std::tuple<maptile, maptile, maptile>;

    /** Draw a visible part of the map into `w`.
     *
     * This method uses `g->u.bub_pos().x()/bub_pos().y()` for visibility calculations, so it can
     * not be used for anything but the player's viewport. Likewise, only
     * `g->m` and maps with equivalent coordinates can be used, as other maps
     * would have coordinate systems incompatible with `g->u.bub_pos().x()`
     *
     * @param w Window we are drawing in
     * @param center The coordinate of the center of the viewport, this can
     *               be different from the player coordinate.
     */
    void draw(const catacurses::window& w, const tripoint_bub_ms& center);

    /**
     * Draw the map tile at the given coordinate. Called by `map::draw()`.
     *
     * @param w The window we are drawing in
     * @param p The tile on this map to draw.
     * @param params Draw parameters.
     */
    void drawsq(
        const catacurses::window& w, const tripoint_bub_ms& p, const drawsq_params& params) const;

    /**
     * Add currently loaded submaps to the @ref mapbuffer.
     * They will than be stored by that class and can be loaded from that class.
     * This can be called several times, the mapbuffer takes care of adding
     * the same submap several times. It should only be called after the map has
     * been loaded.
     * Submaps that have been loaded from the mapbuffer (and not generated) are
     * Load submaps into the local non-owning cache. This might create new submaps if
     * the @ref mapbuffer can not deliver the requested submap (as it does
     * not exist on disc).
     * This must be called before the map can be used at all!
     * @param w global coordinates of the submap at local cache slot (0,0). This
     * is in submap coordinates.
     * @param update_vehicles If true, add vehicles to the vehicle cache.
     * @param pump_events If true, handle window events during loading. If
     * you set this to true, do ensure that the map is not accessed before
     * this function returns (for example, UIs that draw the map should be
     * disabled).
     */
    void load(const point_abs_sm& w, bool update_vehicles, bool pump_events = false);
    /**
     * Shift the map along the vector sp.
     * This is like loading the map with coordinates derived from the current
     * position of the map (@ref abs_sub) plus the shift vector.
     * Note: the map must have been loaded before this can be called.
     */
    void shift(const point_rel_sm& sp);
    void clear_spawns();
    void clear_traps();

    auto maptile_at(const tripoint_bub_ms& p) const -> maptile;
    auto maptile_at_internal(const tripoint_bub_ms& p) const -> maptile;

private:
    void create_hot_air(const tripoint_bub_ms& p, int intensity);
    auto burn_body_part(player& u, field_entry& cur, body_part bp, int scale) -> int;

public:
    // Movement and LOS
    /**
     * Calculate the cost to move past the tile at p.
     *
     * The move cost is determined by various obstacles, such
     * as terrain, vehicles and furniture.
     *
     * @note Movement costs for players and zombies both use this function.
     *
     * @return The return value is interpreted as follows:
     * Move Cost | Meaning
     * --------- | -------
     * 0         | Impassable. Use `passable`/`impassable` to check for this.
     * n > 0     | x*n turns to move past this
     */
    auto move_cost(const tripoint_bub_ms& p, const vehicle* ignored_vehicle = nullptr) const -> int;
    /**
     * Internal versions of public functions to avoid checking same variables multiple times.
     * They lack safety checks, because their callers already do those.
     */
    auto move_cost_internal(
        const furn_t& furniture, const ter_t& terrain, const vehicle* veh, int vpart) const -> int;
    auto impassable(const tripoint_bub_ms& p) const -> bool;
    auto passable(const tripoint_bub_ms& p) const -> bool;
    auto is_wall_adjacent(const tripoint_bub_ms& center) const -> bool;

    /**
     * Similar behavior to `move_cost()`, but ignores vehicles.
     */
    auto move_cost_ter_furn(const tripoint_bub_ms& p) const -> int;
    auto impassable_ter_furn(const tripoint_bub_ms& p) const -> bool;
    auto passable_ter_furn(const tripoint_bub_ms& p) const -> bool;

    /**
     * Cost to move out of one tile and into the next.
     *
     * @return The cost in turns to move out of tripoint `from` and into `to`
     */
    auto combined_movecost(
        const tripoint_bub_ms& from, const tripoint_bub_ms& to,
        const vehicle* ignored_vehicle = nullptr, int modifier = 0, bool flying = false,
        bool via_ramp = false) const -> int;

    /**
     * Returns true if a creature could walk from `from` to `to` in one step.
     * That is, if the tiles are adjacent and either on the same z-level or connected
     * by stairs or (in case of flying monsters) open air with no floors.
     */
    auto valid_move(
        const tripoint_bub_ms& from, const tripoint_bub_ms& to, bool bash = false,
        bool flying = false, bool via_ramp = false) const -> bool;

    /**
     * Size of map objects at `p` for purposes of ranged combat.
     * Size is in percentage of tile: if 1.0, all attacks going through tile
     * should hit map objects on it, if 0.0 there is nothing to be hit (air/water).
     */
    auto ranged_target_size(const tripoint_bub_ms& p) const -> double;

    // Sees:
    /**
     * Returns whether `F` sees `T` with a view range of `range`.
     */
    auto sees(const tripoint_bub_ms& F, const tripoint_bub_ms& T, int range) const -> bool;

private:
    /**
     * Don't expose the slope adjust outside map functions.
     *
     * @param F Thing doing the seeing
     * @param T Thing being seen
     * @param range Vision range of F
     * @param bresenham_slope Indicates the start offset of Bresenham line used to connect
     * the two points, and may subsequently be used to form a path between them.
     * Set to zero if the function returns false.
     **/
    auto sees(const tripoint_bub_ms& F, const tripoint_bub_ms& T, int range, int& bresenham_slope)
        const -> bool;

public:
    /**
     * Returns coverage of target in relation to the observer. Target is loc2, observer is loc1.
     * First tile from the target is an obstacle, which has the coverage value.
     * If there's no obstacle adjacent to the target - no coverage.
     */
    auto obstacle_coverage(const tripoint_bub_ms& loc1, const tripoint_bub_ms& loc2) const -> int;
    /**
     * Returns coverage value of the tile.
     */
    auto coverage(const tripoint_bub_ms& p) const -> int;
    /**
     * Check whether there's a direct line of sight between `F` and
     * `T` with the additional movecost restraints.
     *
     * Checks two things:
     * 1. The `sees()` algorithm between `F` and `T`
     * 2. That moving over the line of sight would have a move_cost between
     *    `cost_min` and `cost_max`.
     */
    auto clear_path(
        const tripoint_bub_ms& f, const tripoint_bub_ms& t, int range, int cost_min,
        int cost_max) const -> bool;

    /**
     * Checks if a rotated vehicle is blocking diagonal movement, tripoints must be adjacent
     */
    auto obstructed_by_vehicle_rotation(
        const tripoint_bub_ms& from, const tripoint_bub_ms& to) const -> bool;

    /**
     * Checks if a rotated vehicle is blocking diagonal vision, tripoints must be adjacent
     */
    auto obscured_by_vehicle_rotation(const tripoint_bub_ms& from, const tripoint_bub_ms& to) const
        -> bool;

    /**
     * Populates a vector of points that are reachable within a number of steps from a
     * point. It could be generalized to take advantage of z levels, but would need some
     * additional code to detect whether a valid transition was on a tile.
     *
     * Does the following:
     * 1. Checks if a point_bub_ms & is reachable using a flood fill and if it is, adds it to a
     * vector.
     *
     */
    void reachable_flood_steps(
        std::vector<tripoint_bub_ms>& reachable_pts, const tripoint_bub_ms& f, int range,
        int cost_min, int cost_max) const;

    /**
     * Iteratively tries Bresenham lines with different biases
     * until it finds a clear line or decides there isn't one.
     * returns the line found, which may be the straight line, but blocked.
     */
    auto find_clear_path(const tripoint_bub_ms& source, const tripoint_bub_ms& destination) const
        -> std::vector<tripoint_bub_ms>;

    /**
     * Check whether the player can access the items located @p. Certain furniture/terrain
     * may prevent that (e.g. a locked safe).
     */
    auto accessible_items(const tripoint_bub_ms& t) const -> bool;
    /**
     * Calculate next search points surrounding the current position.
     * Points closer to the target come first.
     * This method leads to straighter lines and prevents weird looking movements away from the
     * target.
     */
    auto get_dir_circle(const tripoint_bub_ms& f, const tripoint_bub_ms& t) const
        -> std::vector<tripoint_bub_ms>;

    /**
     * Calculate the best path using A*
     *
     * @param f The source location from which to path.
     * @param t The destination to which to path.
     * @param settings Structure describing pathfinding parameters.
     * @param pre_closed Never path through those points. They can still be the source or the
     * destination.
     */
    auto route(
        const tripoint_bub_ms& f, const tripoint_bub_ms& t, const pathfinding_settings& settings,
        const std::set<tripoint_bub_ms>& pre_closed = {{}}) const -> std::vector<tripoint_bub_ms>;
    auto route(
        const tripoint_abs_ms& f, const tripoint_abs_ms& t, const pathfinding_settings& settings,
        const std::set<tripoint_abs_ms>& pre_closed = {{}}) const -> std::vector<tripoint_abs_ms>;

    // Vehicles: Common to 2D and 3D
    auto get_vehicles() -> VehicleList;
    void add_vehicle_to_cache(vehicle*);
    void clear_vehicle_point_from_cache(vehicle* veh, const tripoint_bub_ms& pt);
    void reset_vehicle_cache();
    void clear_vehicle_cache();
    void clear_vehicle_list(int zlev);
    void update_vehicle_list(const submap* to, int zlev);
    // Returns true if vehicle zones are dirty and need to be recached
    auto check_vehicle_zones(int zlev) -> bool;
    auto get_vehicle_zones(int zlev) -> std::vector<zone_data*>;
    void register_vehicle_zone(vehicle*, int zlev);
    auto deregister_vehicle_zone(zone_data& zone) -> bool;

    // Removes vehicle from map and returns it in unique_ptr
    auto detach_vehicle(vehicle* veh) -> std::unique_ptr<vehicle>;
    void destroy_vehicle(vehicle* veh);
    // Vehicle movement
    void vehmove();
    // Selects a vehicle to move, returns false if no moving vehicles
    auto vehproceed(VehicleList& vehicle_list) -> bool;

    // Vehicles
    auto get_vehicles(const tripoint_bub_sm& start, const tripoint_bub_sm& end) -> VehicleList;
    /**
     * Checks if tile is occupied by vehicle and by which part.
     *
     * @param p Tile to check for vehicle
     */
    auto veh_at(const tripoint_bub_ms& p) const -> optional_vpart_position;
    auto veh_at(const tripoint_abs_ms& p) const -> optional_vpart_position;
    auto veh_at_internal(const tripoint_bub_ms& p, int& part_num) -> vehicle*;
    auto veh_at_internal(const tripoint_bub_ms& p, int& part_num) const -> const vehicle*;
    // Put player on vehicle at x,y
    void board_vehicle(const tripoint_bub_ms& p, Character* pl);
    // Remove given passenger from given vehicle part.
    // If dead_passenger, then null passenger is acceptable.
    void unboard_vehicle(const vpart_reference&, Character* passenger, bool dead_passenger = false);
    // Remove passenger from vehicle at p.
    void unboard_vehicle(const tripoint_bub_ms& p, bool dead_passenger = false);
    // Change vehicle coordinates and move vehicle's driver along.
    // WARNING: not checking collisions!
    auto displace_vehicle(vehicle& veh, const tripoint_rel_ms& dp) -> bool;

    // Shift the vehicle's z-level without moving any parts
    void shift_vehicle_z(vehicle& veh, int z_shift);
    // move water under wheels. true if moved
    auto displace_water(const tripoint_bub_ms& dp) -> bool;

    // Returns the wheel area of the vehicle multiplied by traction of the surface
    // When ignore_movement_modifiers is set to true, it returns the area of the wheels touching the
    // ground
    // TODO: Remove the ugly sinking vehicle hack
    auto vehicle_wheel_traction(const vehicle& veh, bool ignore_movement_modifiers = false) const
        -> float;

    // Executes vehicle-vehicle collision based on vehicle::collision results
    // Returns impulse of the executed collision
    // If vector contains collisions with vehicles other than veh2, they will be ignored
    auto vehicle_vehicle_collision(
        vehicle& veh, vehicle& veh2, const std::vector<veh_collision>& collisions) -> float;
    // Throws vehicle passengers about the vehicle, possibly out of it
    // Returns change in vehicle orientation due to lost control
    auto shake_vehicle(vehicle& veh, int velocity_before, units::angle direction) -> units::angle;

    // Actually moves the vehicle
    // Unlike displace_vehicle, this one handles collisions
    auto move_vehicle(vehicle& veh, const tripoint_rel_ms& dp, const tileray& facing) -> vehicle*;

    // Furniture
    void set(const tripoint_bub_ms& p, const ter_id& new_terrain, const furn_id& new_furniture);
    auto name(const tripoint_bub_ms& p) -> std::string;
    auto disp_name(const tripoint_bub_ms& p) -> std::string;
    /**
     * Returns the name of the obstacle at p that might be blocking movement/projectiles/etc.
     * Note that this only accounts for vehicles, terrain, and furniture.
     */
    auto obstacle_name(const tripoint_bub_ms& p) -> std::string;
    auto has_furn(const tripoint_bub_ms& p) const -> bool;
    auto furn(const tripoint_bub_ms& p) const -> furn_id;
    void furn_set(
        const tripoint_bub_ms& p, const furn_id& new_furniture,
        const cata::poly_serialized<active_tile_data>& new_active = nullptr);
    auto move_furn(
        const tripoint_bub_ms& from, const tripoint_bub_ms& to,
        const map_move_furn_options& options = {}) -> bool;
    auto furnname(const tripoint_bub_ms& p) -> std::string;
    auto can_move_furniture(const tripoint_bub_ms& pos, player* p = nullptr) -> bool;

    // Terrain
    auto ter(const tripoint_bub_ms& p) const -> ter_id;

    // Data Vars

    // requires inbounds(p), may return nullptr otherwise
    auto ter_vars(const tripoint_bub_ms& p) const -> data_vars::data_set*;
    // requires inbounds(p), may return nullptr otherwise
    auto furn_vars(const tripoint_bub_ms& p) const -> data_vars::data_set*;

    // Return a bitfield of the adjacent tiles which connect to the given
    // connect_group.  From least-significant bit the order is south, east,
    // west, north (because that's what cata_tiles expects).
    // Based on a combination of visibility and memory, not simply the true
    // terrain. Additional overrides can be passed in to override terrain
    // at specific positions. This is used to display terrain overview in
    // the map editor.
    auto get_known_connections(
        const tripoint_bub_ms& p, int connect_group,
        const std::map<tripoint_bub_ms, ter_id>& override = {}) const -> uint8_t;
    /**
     * Returns the full harvest list, for spawning.
     */
    // as above, but for furniture
    auto get_known_connections_f(
        const tripoint_bub_ms& p, int connect_group,
        const std::map<tripoint_bub_ms, furn_id>& override = {}) const -> uint8_t;

    auto get_harvest(const tripoint_bub_ms& p) const -> const harvest_id&;
    /**
     * Returns names of the items that would be dropped.
     */
    auto get_harvest_names(const tripoint_bub_ms& p) const -> const std::set<std::string>&;
    auto get_ter_transforms_into(const tripoint_bub_ms& p) const -> ter_id;
    auto get_furn_transforms_into(const tripoint_bub_ms& p) const -> furn_id;

    auto ter_set(const tripoint_bub_ms& p, const ter_id& new_terrain) -> bool;

    auto tername(const tripoint_bub_ms& p) const -> std::string;

    auto has_nearby(
        const tripoint_bub_ms& p, const std::function<bool(map& m, const tripoint_bub_ms& p)>& pred,
        int radius = 1) -> bool;
    // Check for terrain/furniture/field that provide a
    // "fire" item to be used for example when crafting or when
    // a iuse function needs fire.
    auto has_nearby_fire(const tripoint_bub_ms& p, int radius = 1) -> bool;
    /**
     * Check whether a table/workbench/vehicle kitchen or other flat
     * surface is nearby that could be used for crafting or eating.
     */
    auto has_nearby_table(const tripoint_bub_ms& p, int radius = 1) -> bool;
    /**
     * Check whether a chair or vehicle seat is nearby.
     */
    auto has_nearby_chair(const tripoint_bub_ms& p, int radius = 1) -> bool;
    /**
     * Check if creature can see some items at p. Includes:
     * - check for items at this location (has_items(p))
     * - check for SEALED flag (sealed furniture/terrain makes
     * items not visible under any circumstances).
     * - check for CONTAINER flag (makes items only visible when
     * the creature is at p or at an adjacent square).
     */
    auto sees_some_items(const tripoint_bub_ms& p, const Creature& who) const -> bool;
    auto sees_some_items(const tripoint_bub_ms& p, const tripoint_bub_ms& from) const -> bool;
    /**
     * Check if the creature could see items at p if there were
     * any items. This is similar to @ref sees_some_items, but it
     * does not check that there are actually any items.
     */
    auto could_see_items(const tripoint_bub_ms& p, const Creature& who) const -> bool;
    auto could_see_items(const tripoint_bub_ms& p, const tripoint_bub_ms& from) const -> bool;
    /**
     * Checks for existence of items. Faster than i_at(p).empty
     */
    auto has_items(const tripoint_bub_ms& p) const -> bool;

    /**
     * Calls the examine function of furniture or terrain at given tile, for given character.
     * Will only examine terrain if furniture had @ref iexamine::none as the examine function.
     */
    void examine(Character& who, const tripoint_bub_ms& pos);

    /**
     * Returns true if point_bub_ms & at pos is harvestable right now, with no extra tools.
     */
    auto is_harvestable(const tripoint_bub_ms& pos) const -> bool;

    // Flags
    // Words relevant to terrain (sharp, etc)
    auto features(const tripoint_bub_ms& p) -> std::string;
    // Checks terrain, furniture and vehicles
    auto has_flag(const std::string& flag, const tripoint_bub_ms& p) const -> bool;
    // True if items can be dropped in this tile
    auto can_put_items(const tripoint_bub_ms& p) const -> bool;
    // True if items can be placed in this tile
    auto can_put_items_ter_furn(const tripoint_bub_ms& p) const -> bool;
    // Checks terrain
    auto has_flag_ter(const std::string& flag, const tripoint_bub_ms& p) const -> bool;
    // Checks furniture
    auto has_flag_furn(const std::string& flag, const tripoint_bub_ms& p) const -> bool;
    // Checks vehicle part flag
    auto has_flag_vpart(const std::string& flag, const tripoint_bub_ms& p) const -> bool;
    // Checks vehicle part or furniture
    auto has_flag_furn_or_vpart(const std::string& flag, const tripoint_bub_ms& p) const -> bool;
    // Checks terrain or furniture
    auto has_flag_ter_or_furn(const std::string& flag, const tripoint_bub_ms& p) const -> bool;
    // Fast "oh hai it's update_scent/lightmap/draw/monmove/self/etc again, what about this one"
    // flag checking Checks terrain, furniture and vehicles
    auto has_flag(ter_bitflags flag, const tripoint_bub_ms& p) const -> bool;
    // Checks terrain
    auto has_flag_ter(ter_bitflags flag, const tripoint_bub_ms& p) const -> bool;
    // Checks furniture
    auto has_flag_furn(ter_bitflags flag, const tripoint_bub_ms& p) const -> bool;
    // Checks terrain or furniture
    auto has_flag_ter_or_furn(ter_bitflags flag, const tripoint_bub_ms& p) const -> bool;

    // Bashable
    /** Returns true if there is a bashable vehicle part or the furn/terrain is bashable at p */
    auto is_bashable(const tripoint_bub_ms& p, bool allow_floor = false) const -> bool;
    /** Returns true if the terrain at p is bashable */
    auto is_bashable_ter(const tripoint_bub_ms& p, bool allow_floor = false) const -> bool;
    /** Returns true if the furniture at p is bashable */
    auto is_bashable_furn(const tripoint_bub_ms& p) const -> bool;
    /** Returns true if the furniture or terrain at p is bashable */
    auto is_bashable_ter_furn(const tripoint_bub_ms& p, bool allow_floor = false) const -> bool;
    /** Returns max_str of the furniture or terrain at p */
    auto bash_strength(const tripoint_bub_ms& p, bool allow_floor = false) const -> int;
    /** Returns min_str of the furniture or terrain at p */
    auto bash_resistance(const tripoint_bub_ms& p, bool allow_floor = false) const -> int;
    /** Returns a success rating from -1 to 10 for a given tile based on a set strength, used for AI
     * movement planning Values roughly correspond to 10% increment chances of success on a given
     * bash, rounded down. -1 means the square is not bashable */
    auto bash_rating(int str, const tripoint_bub_ms& p, bool allow_floor = false) const -> int;
    auto bash_rating_internal(
        int str, const furn_t& furniture, const ter_t& terrain, bool allow_floor,
        const vehicle* veh, int part) const -> int;


    // Rubble
    /** Generates rubble at the given location, if overwrite is true it just writes on top of what
     * currently exists floor_type is only used if there is a non-bashable wall at the location or
     * with overwrite = true */
    void make_rubble(
        const tripoint_bub_ms& p, const furn_id& rubble_type, const ter_id& floor_type,
        bool overwrite = false);
    void make_rubble(const tripoint_bub_ms& p, const furn_id& rubble_type) {
        make_rubble(p, rubble_type, t_dirt, false);
    }
    void make_rubble(const tripoint_bub_ms& p) { make_rubble(p, f_rubble, t_dirt, false); }

    auto is_outside(const tripoint_bub_ms& p) const -> bool;
    // True when the tile has some overhead coverage within 3×3 (floor or sheltered tile
    // at z+1).  A tile can be outside yet sheltered (building overhang).
    auto is_sheltered(const tripoint_bub_ms& p) const -> bool;
    /// Per-submap terrain transparency for game logic (works at any loaded position).
    auto get_transparency(const tripoint_bub_ms& p) const -> float;

    /**
     * Returns whether or not the terrain at the given location can be dived into
     * (by monsters that can swim or are aquatic or non-breathing).
     * @param p The coordinate to look at.
     * @return true if the terrain can be dived into; false if not.
     */
    auto is_divable(const tripoint_bub_ms& p) const -> bool;
    auto is_water_shallow_current(const tripoint_bub_ms& p) const -> bool;

    /** Check if the last terrain is wall in direction NORTH, SOUTH, WEST or EAST
     *  @param no_furn if true, the function will stop and return false
     *  if it encounters a furniture
     *  @param p starting coordinates of check
     *  @param max ending coordinates of check
     *  @param dir Direction of check
     *  @return true if from x to xmax or y to ymax depending on direction
     *  all terrain is floor and the last terrain is a wall */
    auto is_last_ter_wall(
        bool no_furn, const tripoint_bub_ms& p, const tripoint_bub_ms& max, direction dir) const
        -> bool;

    /**
     * Checks if there are any tinder flagged items on the tile.
     * @param p tile to check
     */
    auto tinder_at(const tripoint_bub_ms& p) -> bool;

    /**
     * Checks if there are any flammable items on the tile.
     * @param p tile to check
     * @param threshold Fuel threshold (lower means worse fuels are accepted).
     */
    auto flammable_items_at(const tripoint_bub_ms& p, int threshold = 0) -> bool;
    /** Returns true if there is a flammable item or field or the furn/terrain is flammable at p */
    auto is_flammable(const tripoint_bub_ms& p) -> bool;
    auto random_outdoor_tile() -> tripoint_bub_ms;
    // mapgen

    void draw_line_ter(const ter_id& type, const tripoint_bub_ms& p1, const tripoint_bub_ms& p2);
    void draw_line_furn(const furn_id& type, const tripoint_bub_ms& p1, const tripoint_bub_ms& p2);
    void draw_fill_background(const ter_id& type);
    void draw_fill_background(ter_id (*f)());
    void draw_fill_background(const weighted_int_list<ter_id>& f);

    void draw_square_ter(const ter_id& type, const tripoint_bub_ms& p1, const tripoint_bub_ms& p2);
    void draw_square_furn(
        const furn_id& type, const tripoint_bub_ms& p1, const tripoint_bub_ms& p2);
    void draw_square_ter(ter_id (*f)(), const tripoint_bub_ms& p1, const tripoint_bub_ms& p2);
    void draw_square_ter(
        const weighted_int_list<ter_id>& f, const tripoint_bub_ms& p1, const tripoint_bub_ms& p2);
    void draw_rough_circle_ter(const ter_id& type, const tripoint_bub_ms& p, int rad);
    void draw_rough_circle_furn(const furn_id& type, const tripoint_bub_ms& p, int rad);
    void draw_circle_ter(const ter_id& type, const rl_vec2d& p, int zlev, double rad);
    void draw_circle_ter(const ter_id& type, const tripoint_bub_ms& p, int rad);
    void draw_circle_furn(const furn_id& type, const tripoint_bub_ms& p, int rad);

    void add_corpse(const tripoint_bub_ms& p);

    // Terrain changing functions
    // Change all instances of $from->$to
    void translate(const ter_id& from, const ter_id& to);
    // Change all instances $from->$to within this radius, optionally limited to locations in the
    // same submap. Optionally toggles instances $from->$to & $to->$from
    void translate_radius(
        const ter_id& from, const ter_id& to, float radi, const tripoint_bub_ms& p,
        bool same_submap = false, bool toggle_between = false);
    auto close_door(const tripoint_bub_ms& p, bool inside, bool check_only) -> bool;

    auto can_open_door(const const_interacting_entity&, const tripoint_bub_ms& p, bool inside) const
        -> bool;
    auto open_door(const interacting_entity&, const tripoint_bub_ms& p, bool inside) -> bool;

    auto can_open_door_ter(
        const const_interacting_entity&, const ter_t& ter, const tripoint_bub_ms& p,
        bool inside) const -> bool;
    auto open_door_ter(
        const interacting_entity&, const ter_t& ter, const tripoint_bub_ms& p, bool inside) -> bool;

    auto can_open_door_furn(
        const const_interacting_entity&, const furn_t& furn, const tripoint_bub_ms& p,
        bool inside) const -> bool;
    auto open_door_furn(
        const interacting_entity&, const furn_t& furn, const tripoint_bub_ms& p, bool inside)
        -> bool;

    auto can_open_door_veh(
        const const_interacting_entity&, const optional_vpart_position& vp,
        const tripoint_bub_ms& p, bool inside) const -> bool;
    auto open_door_veh(
        const interacting_entity&, const optional_vpart_position& vp, const tripoint_bub_ms& p,
        bool inside) -> bool;

    // Destruction
    /** bash a square for a set number of times at set power.  Does not destroy */
    void batter(const tripoint_bub_ms& p, int power, int tries = 1, bool silent = false);
    /** Keeps bashing a square until it can't be bashed anymore */
    void destroy(const tripoint_bub_ms& p, bool silent = false);
    /** Keeps bashing a square until there is no more furniture */
    void destroy_furn(const tripoint_bub_ms& p, bool silent = false);
    void crush(const tripoint_bub_ms& p);
    void shoot(
        const tripoint_bub_ms& origin, const tripoint_bub_ms& p, projectile& proj, bool hit_items);
    /** Checks if a square should collapse, returns the X for the one_in(X) collapse chance */
    auto collapse_check(const tripoint_bub_ms& p) -> int;
    /** Causes a collapse at p, such as from destroying a wall */
    void collapse_at(
        const tripoint_bub_ms& p, bool silent, bool was_supporting = false,
        bool destroy_pos = true);
    /** Checks surrounding tiles for suspension, and has them check for collapse. !!Should only be
     * called after the tile at this point_bub_ms & has been destroyed!!*/
    void propagate_suspension_check(const tripoint_bub_ms& point);
    /** Triggers a recursive collapse of suspended tiles based on their support validity*/
    void collapse_invalid_suspension(const tripoint_bub_ms& point);
    /** Checks the four orientations in which a suspended tile could be valid, and returns if the
     * tile is valid*/
    auto is_suspension_valid(const tripoint_bub_ms& point) -> bool;
    /** Tries to smash the trap at the given tripoint. */
    void smash_trap(const tripoint_bub_ms& p, const int power, const std::string& cause_message);
    /** Tries to smash the items at the given tripoint. */
    void smash_items(
        const tripoint_bub_ms& p, int power, const std::string& cause_message, bool do_destroy);
    /**
     * Returns a pair where first is whether anything was smashed and second is if it was destroyed.
     *
     * @param p Where to bash
     * @param str How hard to bash
     * @param silent Don't produce any sound
     * @param destroy Destroys some otherwise unbashable tiles
     * @param bash_floor Allow bashing the floor and the tile that supports it
     * @param bashing_vehicle Vehicle that should NOT be bashed (because it is doing the bashing)
     */
    auto bash(
        const tripoint_bub_ms& p, int str, bool silent = false, bool destroy = false,
        bool bash_floor = false, const vehicle* bashing_vehicle = nullptr) -> bash_results;
    auto bash(
        const tripoint_bub_ms& p, const bash_params& params,
        const vehicle* bashing_vehicle = nullptr) -> bash_results;

    auto bash_vehicle(const tripoint_bub_ms& p, const bash_params& params) -> bash_results;
    auto bash_ter_furn(const tripoint_bub_ms& p, const bash_params& params) -> bash_results;

    // Effects of attacks/items
    auto hit_with_acid(const tripoint_bub_ms& p) -> bool;
    auto hit_with_fire(const tripoint_bub_ms& p) -> bool;

    /**
     * Returns true if there is an item for which filter returns true in the tile at p.
     * Pass return_true<item> to detect all items.
     * @param p the location to check at
     * @param filter what to filter the item by.
     */
    auto has_item_with(const tripoint_bub_ms& p, const std::function<bool(const item&)>& filter)
        -> bool;

    /**
     * Returns true if there is an item for which filter returns true in a 1 tile radius of p.
     * Pass return_true<item> to detect all adjacent items.
     * @param p the location to check at
     * @param filter what to filter the item by.
     */
    auto has_adjacent_item_with(
        const tripoint_bub_ms& p, const std::function<bool(const item&)>& filter) -> bool;

    /**
     * Returns true if there is furniture for which filter returns true in a 1 tile radius of p.
     * Pass return_true<furn_t> to detect all adjacent furniture.
     * @param p the location to check at
     * @param filter what to filter the furniture by.
     */
    auto has_adjacent_furniture_with(
        const tripoint_bub_ms& p, const std::function<bool(const furn_t&)>& filter) -> bool;

    /**
     * Returns true if there is terrain for which filter returns true in a 1 tile radius of p.
     * Pass return_true<ter_t> to detect all adjacent terrain.
     * @param p the location to check at
     * @param filter what to filter the terrain by.
     */
    auto has_adjacent_terrain_with(
        const tripoint_bub_ms& p, const std::function<bool(const ter_t&)>& filter) -> bool;

    /**
     * Remove moppable fields/items at this location
     *  @param p the location
     *  @return true if anything moppable was there, false otherwise.
     */
    auto mop_spills(const tripoint_bub_ms& p) -> bool;
    /**
     * Moved here from weather.cpp for speed. Decays fire, washable fields and scent.
     * Washable fields are decayed only by 1/3 of the amount fire is.
     */
    void decay_fields_and_scent(const time_duration& amount);

    // Signs
    auto get_signage(const tripoint_bub_ms& p) const -> std::string;
    void set_signage(const tripoint_bub_ms& p, const std::string& message) const;
    void delete_signage(const tripoint_bub_ms& p) const;

    // Radiation
    auto get_radiation(const tripoint_bub_ms& p) const -> int;
    void set_radiation(const tripoint_bub_ms& p, int value);

    /** Increment the radiation in the given tile by the given delta
     *  (decrement it if delta is negative)
     */
    void adjust_radiation(const tripoint_bub_ms& p, int delta);

    // Temperature
    // Temperature for submap
    auto get_temperature(const tripoint_bub_ms& p) const -> int;
    // Set temperature for all four submap quadrants
    void set_temperature(const tripoint_bub_ms& p, int temperature);

    // Returns points for all submaps with inconsistent state relative to
    // the list in map.  Used in tests.
    auto check_submap_active_item_consistency() -> std::vector<tripoint_abs_sm>;
    // Accessor that returns a wrapped reference to an item stack for safe modification.
    auto i_at(const tripoint_bub_ms& p) -> map_stack;
    auto water_from(const tripoint_bub_ms& p) -> detached_ptr<item>;
    auto i_clear(const tripoint_bub_ms& p) -> std::vector<detached_ptr<item>>;
    // i_rem() methods that return values act like container::erase(),
    // returning an iterator to the next item after removal.
    auto i_rem(
        const tripoint_bub_ms& p, map_stack::const_iterator it, detached_ptr<item>* out = nullptr)
        -> map_stack::iterator;

    auto i_rem(const tripoint_bub_ms& p, item* it) -> detached_ptr<item>;
    void spawn_artifact(const tripoint_bub_ms& p);
    void spawn_natural_artifact(const tripoint_bub_ms& p, artifact_natural_property prop);
    void spawn_item(
        const tripoint_bub_ms& p, const itype_id& type_id, unsigned quantity = 1, int charges = 0,
        const time_point& birthday = calendar::start_of_cataclysm, int damlevel = 0);

    // FIXME: remove these overloads and require spawn_item to take an
    // itype_id
    void spawn_item(
        const tripoint_bub_ms& p, const std::string& type_id, unsigned quantity = 1,
        int charges = 0, const time_point& birthday = calendar::start_of_cataclysm,
        int damlevel = 0) {
        spawn_item(p, itype_id(type_id), quantity, charges, birthday, damlevel);
    }
    auto max_volume(const tripoint_bub_ms& p) -> units::volume;
    auto free_volume(const tripoint_bub_ms& p) -> units::volume;
    auto stored_volume(const tripoint_bub_ms& p) -> units::volume;

    /**
     *  Adds an item to map tile or stacks charges
     *  @param pos Where to add item
     *  @param obj Item to add
     *  @param overflow if destination is full attempt to drop on adjacent tiles
     *  @return the item if it could not be handled
     *  @warning function is relatively expensive and meant for user initiated actions, not mapgen
     */
    auto add_item_or_charges(
        const tripoint_bub_ms& pos, detached_ptr<item>&& obj, bool overflow = true)
        -> detached_ptr<item>;

    /**
     * Checks for spawn_rate value for item category of 'itm'.
     * If spawn_rate is less than 1.0, it will make a random roll (0.1-1.0) to check if the item
     * will have a chance to spawn. If spawn_rate is more than or equal to 1.0, it will make item
     * spawn that many times (using roll_remainder).
     */
    auto item_category_spawn_rate(const item& itm) -> float;

    /**
     * Place an item on the map, despite the parameter name, this is not necessarily a new item.
     * WARNING: does -not- check volume or stack charges. player functions (drop etc) should use
     * map::add_item_or_charges
     *
     * @returns The item that got added, or nulitem.
     */
    void add_item(const tripoint_bub_ms& p, detached_ptr<item>&& new_item);
    auto spawn_an_item(
        const tripoint_bub_ms& p, detached_ptr<item>&& new_item, int charges, int damlevel)
        -> detached_ptr<item>;


    /**
     * Remove an item from active item processing queue as necessary
     */
    void make_inactive(item& loc);

    /**
     * Update an item's active status, for example when adding
     * hot or perishable liquid to a container.
     * Should be called as part of activate()
     */
    void make_active(item& loc);

    /**
     * Update luminosity before and after item's transformation
     */
    void update_lum(item& loc, bool add);

    /**
     * @name Consume items on the map
     *
     * The functions here consume accessible items / item charges on the map or in vehicles
     * around the player (whose positions is given as origin).
     * They return a list of copies of the consumed items (with the actually consumed charges
     * in it).
     * The quantity / amount parameter will be reduced by the number of items/charges removed.
     * If all required items could be removed from the map, the quantity/amount will be 0,
     * otherwise it will contain a positive value and the remaining items must be gathered from
     * somewhere else.
     */
    /*@{*/
    auto use_amount_square(
        const tripoint_bub_ms& p, const itype_id& type, int& quantity,
        const std::function<bool(const item&)>& filter = return_true<item>)
        -> std::vector<detached_ptr<item>>;
    auto use_amount(
        const tripoint_bub_ms& origin, int range, const itype_id& type, int& quantity,
        const std::function<bool(const item&)>& filter = return_true<item>)
        -> std::vector<detached_ptr<item>>;
    auto use_charges(
        const tripoint_bub_ms& origin, int range, const itype_id& type, int& quantity,
        const std::function<bool(const item&)>& filter = return_true<item>)
        -> std::vector<detached_ptr<item>>;
    /*@}*/

    /**
     * Place items from item group in the rectangle f - t. Several items may be spawned
     * on different places. Several items may spawn at once (at one place) when the item group says
     * so (uses @ref item_group::items_from which may return several items at once).
     * @param loc Current location of items to be placed
     * @param chance Chance for more items. A chance of 100 creates 1 item all the time, otherwise
     * it's the chance that more items will be created (place items until the random roll with that
     * chance fails). The chance is used for the first item as well, so it may not spawn an item at
     * all. Values <= 0 or > 100 are invalid.
     * @param p1 One corner of rectangle in which to spawn items
     * @param p2 Second corner of rectangle in which to spawn items
     * @param ongrass If false the items won't spawn on flat terrain (grass, floor, ...).
     * @param turn The birthday that the created items shall have.
     * @param magazine percentage chance item will contain the default magazine
     * @param ammo percentage chance item will be filled with default ammo
     * @return vector containing all placed items
     */
    auto place_items(
        const item_group_id& loc, int chance, const tripoint_bub_ms& p1, const tripoint_bub_ms& p2,
        bool ongrass, const time_point& turn, int magazine = 0, int ammo = 0) -> std::vector<item*>;
    /**
     * Place items from an item group at p. Places as much items as the item group says.
     * (Most item groups are distributions and will only create one item.)
     * @param loc Current location of items
     * @param p Destination of items
     * @param turn The birthday that the created items shall have.
     * @return Vector of pointers to placed items (can be empty, but no nulls).
     */
    auto put_items_from_loc(
        const item_group_id& loc, const tripoint_bub_ms& p,
        const time_point& turn = calendar::start_of_cataclysm) -> std::vector<item*>;

    // Similar to spawn_an_item, but spawns a list of items, or nothing if the list is empty.
    auto spawn_items(const tripoint_bub_ms& p, std::vector<detached_ptr<item>> new_items)
        -> std::vector<detached_ptr<item>>;

    void create_anomaly(
        const tripoint_bub_ms& p, artifact_natural_property prop, bool create_rubble = true);

    // Partial construction functions
    void partial_con_set(const tripoint_bub_ms& p, std::unique_ptr<partial_con> con);
    void partial_con_remove(const tripoint_bub_ms& p);
    auto partial_con_at(const tripoint_bub_ms& p) -> partial_con*;
    // Traps
    void trap_set(const tripoint_bub_ms& p, const trap_id& type);

    auto tr_at(const tripoint_bub_ms& p) const -> const trap&;
    /// See @ref trap::can_see, which is called for the trap here.
    auto can_see_trap_at(const tripoint_bub_ms& p, const Character& c) const -> bool;

    void disarm_trap(const tripoint_bub_ms& p);
    void remove_trap(const tripoint_bub_ms& p);

    /**
     * Apply field effects to the creature when it's on a square with fields.
     */
    void creature_in_field(Creature& critter);
    /**
     * Apply trap effects to the creature, similar to @ref creature_in_field.
     * If there is no trap at the creatures location, nothing is done.
     * If the creature can avoid the trap, nothing is done as well.
     * Otherwise the trap is triggered.
     * @param critter Creature that just got trapped
     * @param may_avoid If true, the creature tries to avoid the trap
     * (@ref Creature::avoid_trap). If false, the trap is always triggered.
     */
    void creature_on_trap(Creature& critter, bool may_avoid = true);
    // Field functions
    /**
     * Get the fields that are here. This is for querying and looking at it only,
     * one can not change the fields.
     * @param p The local map coordinates, if out of bounds, returns an empty field.
     */
    auto field_at(const tripoint_bub_ms& p) const -> const field&;
    /**
     * Gets fields that are here. Both for querying and edition.
     */
    auto field_at(const tripoint_bub_ms& p) -> field&;
    /**
     * Get the age of a field entry (@ref field_entry::age), if there is no
     * field of that type, returns `-1_turns`.
     */
    auto get_field_age(const tripoint_bub_ms& p, const field_type_id& type) const -> time_duration;
    /**
     * Get the intensity of a field entry (@ref field_entry::intensity),
     * if there is no field of that type, returns 0.
     */
    auto get_field_intensity(const tripoint_bub_ms& p, const field_type_id& type) const -> int;
    /**
     * Increment/decrement age of field entry at point.
     * @return resulting age or `-1_turns` if not present (does *not* create a new field).
     */
    auto mod_field_age(
        const tripoint_bub_ms& p, const field_type_id& type, const time_duration& offset)
        -> time_duration;
    /**
     * Increment/decrement intensity of field entry at point, creating if not present,
     * removing if intensity becomes 0.
     * @return resulting intensity, or 0 for not present (either removed or not created at all).
     */
    auto mod_field_intensity(const tripoint_bub_ms& p, const field_type_id& type, int offset)
        -> int;
    /**
     * Set age of field entry at point.
     * @param p Location of field
     * @param type ID of field
     * @param age New age of specified field
     * @param isoffset If true, the given age value is added to the existing value,
     * if false, the existing age is ignored and overridden.
     * @return resulting age or `-1_turns` if not present (does *not* create a new field).
     */
    auto set_field_age(
        const tripoint_bub_ms& p, const field_type_id& type, const time_duration& age,
        bool isoffset = false) -> time_duration;
    /**
     * Set intensity of field entry at point, creating if not present,
     * removing if intensity becomes 0.
     * @param p Location of field
     * @param type ID of field
     * @param new_intensity New intensity of field
     * @param isoffset If true, the given new_intensity value is added to the existing value,
     * if false, the existing intensity is ignored and overridden.
     * @return resulting intensity, or 0 for not present (either removed or not created at all).
     */
    auto set_field_intensity(
        const tripoint_bub_ms& p, const field_type_id& type, int new_intensity,
        bool isoffset = false) -> int;
    /**
     * @return true if there **might** be a field at `p`
     * @return false there's no fields at `p`
     */
    auto has_field_at(const tripoint_bub_ms& p, bool check_bounds = true) -> bool;
    /**
     * Get field of specific type at point.
     * @return NULL if there is no such field entry at that place.
     */
    auto get_field(const tripoint_bub_ms& p, const field_type_id& type) -> field_entry*;
    auto dangerous_field_at(const tripoint_bub_ms& p) -> bool;
    /**
     * Add field entry at point, or set intensity if present
     * @return false if the field could not be created (out of bounds), otherwise true.
     */
    auto add_field(
        const tripoint_bub_ms& p, const field_type_id& type_id, int intensity = INT_MAX,
        const time_duration& age = 0_turns, bool hit_player = true) -> bool;
    /**
     * Remove field entry at xy, ignored if the field entry is not present.
     */
    void remove_field(const tripoint_bub_ms& p, const field_type_id& field_to_remove);

    // Splatters of various kind
    void add_splatter(const field_type_id& type, const tripoint_bub_ms& where, int intensity = 1);
    void add_splatter_trail(
        const field_type_id& type, const tripoint_bub_ms& from, const tripoint_bub_ms& to);
    void add_splash(
        const field_type_id& type, const tripoint_bub_ms& center, int radius, int intensity);

    void propagate_field(
        const tripoint_bub_ms& center, const field_type_id& type, int amount,
        int max_intensity = 0);

    /**
     * Runs one cycle of emission @ref src which **may** result in propagation of fields
     * @param pos Location of emission
     * @param src Id of object producing the emission
     * @param mul Multiplies the chance and possibly qty (if `chance*mul > 100`) of the emission
     */
    void emit_field(const tripoint_bub_ms& pos, const emit_id& src, float mul = 1.0f);

    // Scent propagation helpers
    /**
     * Build the map of scent-resistant tiles.
     * Should be way faster than if done in `game.cpp` using public map functions.
     */
    void scent_blockers(
        std::vector<char>& scent_transfer, int st_sy, const tripoint_bub_ms& min,
        const tripoint_bub_ms& max);

    // Computers
    auto computer_at(const tripoint_bub_ms& p) -> computer*;
    auto add_computer(const tripoint_bub_ms& p, const std::string& name, int security) -> computer*;

    // Graffiti
    auto has_graffiti_at(const tripoint_bub_ms& p) const -> bool;
    auto graffiti_at(const tripoint_bub_ms& p) const -> const std::string&;
    void set_graffiti(const tripoint_bub_ms& p, const std::string& contents);
    void delete_graffiti(const tripoint_bub_ms& p);

    // Climbing
    /**
     * Checks 3x3 block centered on p for terrain to climb.
     * @return Difficulty of climbing check from point_bub_ms & p.
     */
    auto climb_difficulty(const tripoint_bub_ms& p) const -> int;

    // Support (of weight, structures etc.)
private:
    // Tiles whose ability to support things was removed in the last turn
    std::set<tripoint_bub_ms> support_cache_dirty;
    // Checks if the tile is supported and adds it to support_cache_dirty if it isn't
    void support_dirty(const tripoint_bub_ms& p);

public:
    // Returns true if there is a physical floor at p (tile has no TFLAG_NO_FLOOR).
    // Returns false for out-of-bounds z or missing submap.
    auto has_floor(const tripoint_bub_ms& p, bool visible_only = false) const -> bool;

    /** Checks if there's a floor between the two tiles. They must be at most 1 tile away from each
     * other in any dimension. If they're not at the same xy coord there must be floor on both of
     * the relevant tiles
     */
    auto floor_between(const tripoint_bub_ms& first, const tripoint_bub_ms& second) const -> bool;

    /** Does this tile support vehicles and furniture above it */
    auto supports_above(const tripoint_bub_ms& p) const -> bool;
    auto has_floor_or_support(const tripoint_bub_ms& p) const -> bool;

    /**
     * Handles map objects of given type (not creatures) falling down.
     * Returns true if anything changed.
     */
    /*@{*/
    void drop_everything(const tripoint_bub_ms& p);
    void drop_furniture(const tripoint_bub_ms& p);
    void drop_items(const tripoint_bub_ms& p);
    void drop_vehicle(const tripoint_bub_ms& p);
    void drop_fields(const tripoint_bub_ms& p);
    /*@}*/

    /**
     * Invoked @ref drop_everything on cached dirty tiles.
     */
    void process_falling();

    auto is_cornerfloor(const tripoint_bub_ms& p) const -> bool;

    void place_spawns(
        const mongroup_id& group, const int chance, const tripoint_bub_ms& p1,
        const tripoint_bub_ms& p2, const float density, const bool individual = false,
        const bool friendly = false, const std::string& name = "NONE", const int mission_id = -1);
    void place_gas_pump(const tripoint_bub_ms& p, int charges, const itype_id& fuel_type);
    void place_gas_pump(const tripoint_bub_ms& p, int charges);
    // 6 liters at 250 ml per charge
    void place_toilet(const tripoint_bub_ms& p, int charges = 6 * 4);
    void place_vending(
        const tripoint_bub_ms& p, const item_group_id& type, bool reinforced = false);
    // places an NPC, if static NPCs are enabled or if force is true
    auto place_npc(
        const tripoint_bub_ms& p, const string_id<npc_template>& type, bool force = false)
        -> character_id;
    void apply_faction_ownership(
        const tripoint_bub_ms& p1, const tripoint_bub_ms& p2, const faction_id& id);
    void add_spawn(
        const mtype_id& type, int count, const tripoint_bub_ms& p, bool friendly = false,
        int faction_id = -1, int mission_id = -1, const std::string& name = "NONE") const;
    void add_spawn(
        const mtype_id& type, int count, const tripoint_bub_ms& p, spawn_disposition disposition,
        int faction_id = -1, int mission_id = -1, const std::string& name = "NONE") const;
    void do_vehicle_caching(int z);
    // Note: in 3D mode, will actually build caches on ALL z-levels
    void build_map_cache(int zlev, bool skip_lightmap = false);
    // Unlike the other caches, this populates a supplied cache instead of an internal cache.
    void build_obstacle_cache(
        const tripoint_bub_ms& start, const tripoint_bub_ms& end, float* obstacle_cache,
        int cache_sy);

    auto add_vehicle(
        const std::variant<vgroup_id, vproto_id>& type_, const tripoint_bub_ms& p, units::angle dir,
        int init_veh_fuel = -1, int init_veh_status = -1, bool merge_wrecks = true,
        std::optional<bool> locked = std::nullopt, std::optional<bool> has_keys = std::nullopt)
        -> vehicle*;

    // Light/transparency
    auto light_transparency(const tripoint_bub_ms& p) const -> float;
    // Assumes 0,0 is light map center
    auto light_at(const tripoint_bub_ms& p) const -> lit_level;
    // Raw values for tilesets
    auto ambient_light_at(
        const tripoint_bub_ms& p,
        std::source_location location = std::source_location::current()) const -> float;
    /**
     * Returns whether the tile at `p` is transparent(you can look past it).
     */
    auto is_transparent(const tripoint_bub_ms& p) const -> bool;
    // End of light/transparency

    /**
     * Whether the player character (g->u) can see the given square (local map coordinates).
     * This only checks the transparency of the path to the target, the light level is not
     * checked.
     * @param t Target point_bub_ms & to look at
     * @param max_range All squares that are further away than this are invisible.
     * Ignored if smaller than 0.
     */
    auto pl_sees(const tripoint_bub_ms& t, int max_range) const -> bool;
    /**
     * Uses the map cache to tell if the player could see the given square.
     * pl_sees implies pl_line_of_sight
     * Used for infrared.
     */
    auto pl_line_of_sight(const tripoint_bub_ms& t, int max_range) const -> bool;
    std::set<vehicle*> dirty_vehicle_list;

    /**
     * Legacy accessor for the loaded-grid origin.
     *
     * This is the absolute submap coordinate backing map-local cache slot
     * (0,0).  For the player map it should match the player-derived reality
     * bubble origin after map transitions settle. Detached loaded maps keep
     * an explicit origin here.
     */
    auto get_abs_sub() const -> point_abs_sm { return abs_sub; }

    auto active_submap_views() const -> std::span<const mapbuffer_abs_submap_view> {
        return active_submaps_.submaps();
    }

    auto active_submap_views(const int zlev) const -> std::span<const mapbuffer_abs_submap_view> {
        return active_submaps_.submaps(zlev);
    }

    auto active_submap_view(const tripoint_abs_sm& pos) const
        -> std::optional<mapbuffer_abs_submap_view> {
        return active_submaps_.get_submap_view(pos);
    }

    auto has_active_load_region() const -> bool { return static_cast<bool>(active_load_region_); }
    auto update_active_load_region(const point_abs_sm& begin, const point_abs_sm& end) -> void;
    auto release_active_load_region() -> void;

    auto inbounds_z(const int z) const -> bool {
        return z >= -OVERMAP_DEPTH && z <= OVERMAP_HEIGHT;
    }
    auto inbounds(const tripoint_bub_sm& p) const -> bool;
    auto inbounds(const tripoint_bub_ms& p) const -> bool {
        return inbounds(project_to<coords::sm>(p));
    }
    virtual auto inbounds(const tripoint_abs_sm& p) const -> bool;
    auto inbounds(const tripoint_abs_ms& p) const -> bool {
        return inbounds(project_to<coords::sm>(p));
    }
    auto inbounds(const point_bub_sm& p) const -> bool;
    auto inbounds(const point_bub_ms& p) const -> bool {
        return inbounds(project_to<coords::sm>(p));
    }

    /** Clips the coordinates of p to fit the map bounds */
    void clip_to_bounds(tripoint_bub_sm& p) const;
    void clip_to_bounds(tripoint_bub_ms& p) const;

    void clip_to_bounds(point_bub_sm& p) const;
    void clip_to_bounds(point_bub_ms& p) const;

    auto getmapsize() const -> int { return my_MAPSIZE; }

    // Not protected/private for mapgen_functions.cpp access
    // Rotates the current map 90*turns degrees clockwise
    // Useful for houses, shops, etc
    // @param turns number of 90 clockwise turns to make
    // @param setpos_safe if true, being used outside of mapgen and can use setpos to
    // set NPC positions.  if false, cannot use setpos
    void rotate(int turns, bool setpos_safe = false);

    // Monster spawning:
public:
    /**
     * Spawn monsters from submap spawn points and from the overmap.
     * @param ignore_sight If true, monsters may spawn in the view of the player
     * character (useful when the whole map has been loaded instead, e.g.
     * when starting a new game, or after teleportation or after moving vertically).
     * If false, monsters are not spawned in view of player character.
     */
    void spawn_monsters(bool ignore_sight);
    /**
     * Like spawn_monsters(), but only processes the strip of submaps that
     * newly entered the reality bubble due to a map shift.  Avoids
     * re-processing already-loaded submaps and spuriously placing stale
     * monster_map entries that correspond to already-active monsters.
     * @param shift_amount The shift that just occurred, in submap units.
     */
    void spawn_monsters_new_submaps(const point_rel_sm& shift_amount);

private:
    // Helper #1 - spawns monsters on one submap
    void spawn_monsters_submap(const tripoint_bub_sm& gp, bool ignore_sight);
    // Helper #2 - spawns monsters on one submap and from one group on this submap
    void spawn_monsters_submap_group(const tripoint_bub_sm& gp, mongroup& group, bool ignore_sight);

protected:
    void loadn(const tripoint_bub_sm& grid, bool update_vehicles, bool incremental = false);
    void loadn(const point_bub_sm& grid, bool update_vehicles) {
        const auto actualize_loaded_grid = [&](const tripoint_bub_sm& grid_pos) {
            const auto abs_pos = tripoint_abs_sm(
                abs_sub.x() + grid_pos.x(), abs_sub.y() + grid_pos.y(), grid_pos.z());
            MAPBUFFER_REGISTRY.get(bound_dimension_).actualize_submap(abs_pos);
        };
        for (const auto gridz : std::views::iota(-OVERMAP_DEPTH, OVERMAP_HEIGHT + 1)) {
            const auto grid_pos = tripoint_bub_sm(grid, gridz);
            loadn(grid_pos, update_vehicles);
            actualize_loaded_grid(grid_pos);
        }
    }
    /**
     * Apply the dimension boundary terrain overlay to the edge tiles of @p sm at
     * absolute submap position @p pos.  Only operates when the map has active
     * dimension bounds (@ref current_bounds_).  This is a runtime-only overlay —
     * the saved submap data is never modified.  Must run before mapbuffer
     * actualization so legacy actualization sees the correct terrain.
     */
    auto apply_boundary_overlay(submap& sm, const tripoint_abs_sm& pos) -> void;
    void player_in_field(player& u);
    void monster_in_field(monster& z);

    // Builds a transparency cache and returns true if the cache was invalidated.
    // Used to determine if seen cache should be rebuilt.
    auto build_transparency_cache(int zlev) -> bool;
    auto build_transparency_caches(int minz, int maxz) -> std::vector<int>;
    // Refreshes the weather-transparency lookup table if the sight penalty
    // has changed.  Must be called once serially before any parallel call to
    // build_transparency_cache() to avoid a data race on the shared table.
    void update_weather_transparency_lookup();
    auto build_vision_transparency_cache(const Character& player) -> bool;
    // fills lm with sunlight. pzlev is current player's zlevel
    void build_sunlight_cache(int pzlev);
    // Recomputes sun direction and scatter factor from the current game time.
    void update_solar_params();
    enum class direct_sunlight_state : int { none, shadow, direct };
    // Distinguishes roofed tiles, angled-sun shadow, and full direct sun.
    auto direct_sunlight_state_at(point_bub_ms p, int zlev) const -> direct_sunlight_state;
    auto has_direct_sunlight_at(point_bub_ms p, int zlev) const -> bool;
    auto current_lightmap_source_signature() -> std::size_t;
    void invalidate_lightmap_caches_if_light_state_changed();

public:
    // Rebuilds outside_caches for zlev top-down:
    // A tile is outside if any neighbour in the 3×3 at z+1
    // is (outside AND has no floor).
    // A tile is sheltered if any neighbour in the 3×3 at z+1
    // has a floor or is itself sheltered.
    // Recursively ensures zlev+1 is current before proceeding.
    void build_outside_cache(int zlev);
    // Builds a floor cache and returns true if the cache was invalidated.
    // Used to determine if seen cache should be rebuilt.
    auto build_floor_cache(int zlev) -> bool;
    // We want this visible in `game`, because we want it built earlier in the turn than the rest
    void build_floor_caches();
    // Checks all suspended tiles on a z level and adds those that are invalid to the
    // support_dirty_cache */
    void update_suspension_cache(const int& z);
    // Builds a sound absorption cache and returns true if the cache was invalidated.
    // If true, update the absorption cache. We want this built after the other caches, but before
    // sounds are calced. Function logic located in sounds.cpp
    auto build_absorption_cache(const int zlev) -> bool;
    // Builds a sound_instance_cache by flood filling from a given sound event.
    // Function logic located in sounds.cpp
    void flood_fill_sound(const sound_event soundevent, int zlev);
    // Batch builds a set of sound caches from std::vector<sound_event> sound_batch_floodfill_que
    // Similar to flood_fill_sound, used for monster sounds for performance.
    void batch_flood_fill_sounds();
    // Checks and culls sound_instance_caches from the sound_instance_caches vector.
    // Sounds that have been heard by monsters and by the player are culled so they are not
    // re-heard.
    void cull_heard_sounds();

protected:
    // When skip_shared_init is true the caller has already: cleared sm/lsb for
    // this level, called build_sunlight_cache() once, and applied character
    // lights.  The function then processes only entities whose position z
    // matches zlev, avoiding cross-level cache writes for parallel safety.
    void generate_lightmap(int zlev);
    void generate_lightmap_worker(int zlev);
    void flush_lightmap_cpu_read_counters() const;
    void build_seen_cache(const tripoint_bub_ms& origin, int target_z);
    auto vision_transparency_block_mask() const -> uint32_t;
    // Applies vehicle mirror/camera FOV from @p origin's vehicle.
    // Separated from build_seen_cache for readability and Tracy granularity.
    void apply_vehicle_optics(const tripoint_bub_ms& origin, int target_z);
    void apply_character_light(Character& who);

    // Adds/removes player specific transparencies
    void apply_vision_transparency_cache(
        const tripoint_bub_ms& center, int target_z, float (&vision_restore_cache)[9],
        bool (&blocked_restore_cache)[8]);
    void restore_vision_transparency_cache(
        const tripoint_bub_ms& center, int target_z, float (&vision_restore_cache)[9],
        bool (&blocked_restore_cache)[8]);

    int my_MAPSIZE;

    inline auto bubble_tiles() const -> point_range<point_bub_ms> {
        return {point_bub_ms::zero(),
                point_bub_ms(coords::map_squares_per(coords::scale::submap) * my_MAPSIZE - 1,
                             coords::map_squares_per(coords::scale::submap) * my_MAPSIZE - 1)};
    }

    inline auto reality_bubble_2D_bounds() const -> inclusive_rectangle<point_bub_sm> {
        return {point_bub_sm::zero(), point_bub_sm(my_MAPSIZE - 1, my_MAPSIZE - 1)};
    }

    inline auto reality_bubble_3D_bounds() const -> inclusive_cuboid<tripoint_bub_sm> {
        return {tripoint_bub_sm(0, 0, -OVERMAP_DEPTH),
                tripoint_bub_sm(my_MAPSIZE - 1, my_MAPSIZE - 1, OVERMAP_HEIGHT)};
    }

    inline auto bubble_submaps() const -> tripoint_range<tripoint_bub_sm> {
        return {tripoint_bub_sm(0, 0, -OVERMAP_DEPTH),
                tripoint_bub_sm(my_MAPSIZE - 1, my_MAPSIZE - 1, OVERMAP_HEIGHT)};
    }

    inline auto flat_bubble_submaps() const -> point_range<point_bub_sm> {
        return {point_bub_sm::zero(), point_bub_sm(my_MAPSIZE - 1, my_MAPSIZE - 1)};
    }

    // stores vision adjustment for the tiles immediately surrounding the player, the order is given
    // by eight_adjacent_offsets in point.h examples of adjustment: crouching
    vision_adjustment vision_transparency_cache[8] = {VISION_ADJUST_NONE};

    // Pre-computed 1/exp(t*i) table for the current weather transparency.
    // Written serially before parallel shadowcasting calls.
    exp_lookup weather_lookup_{LIGHT_TRANSPARENCY_OPEN_AIR * 1.1f};

    // Last player position for which build_seen_cache was run.
    // Initialized to tripoint_min so the first build_map_cache call always rebuilds.
    // Reset to tripoint_min by invalidate_map_cache so any full-cache invalidation
    // forces a seen_cache rebuild regardless of whether the player moved.
    tripoint_bub_ms m_last_seen_cache_origin = tripoint_bub_ms(tripoint_min);
    bool visibility_caches_dirty_ = true;
    std::size_t m_last_lightmap_source_signature = 0;
    bool m_last_lightmap_source_signature_valid = false;

    // State for the directional sunlight system.  Rebuilt by update_solar_params().
    struct solar_params {
        // Horizontal shadow displacement per z-level.  The sky-access ray
        // back toward the sun uses the inverse of this vector.
        // Positive dx_per_z = east (+x); negative = west.
        // SUN_EAST_SIGN in update_solar_params() flips the axis if needed.
        // dy_per_z is always 0 (no latitude tilt modelled).
        float dx_per_z = 0.f;
        float dy_per_z = 0.f;
        // False at night; true for all daylight hours (day/night boundary only).
        bool direct_active = false;
    };
    solar_params m_solar;

    /**
     * Absolute submap coordinate of loaded cache slot (0,0).
     *
     * This is a loaded-grid/cache origin, not the definition of player
     * reality-bubble space.  The player map's value is synchronized with
     * player_reality_bubble_origin() by load, shift, resize, and vertical
     * transition code.  Detached maps use this as their explicit local
     * anchor.
     */
    point_abs_sm abs_sub;
    mapbuffer_bounds_view active_submaps_;
    mapbuffer_load_region active_load_region_;

    auto set_abs_sub(const point_abs_sm& p) -> void {
        abs_sub = p;
        refresh_active_submap_view();
    }

    auto refresh_active_submap_view() -> void;
    auto validate_active_submap_view_complete(const char* context) const -> void;

public:
    auto get_field(const tripoint_bub_ms& p) -> field&;

    /**
     * Compatibility map-local lookup. Absolute data lookup belongs on
     * mapbuffer; simulation membership belongs on submap_load_manager.
     */
    auto get_submap_at(const tripoint_bub_ms& p) const -> submap*;
    /**
     * Compatibility map-local lookup with submap-local offset.
     */
    auto get_submap_at(const tripoint_bub_ms& p, point_sm_ms& offset_p) const -> submap*;

private:
    /** Caclulate the greatest populated zlevel in the loaded submaps and save in the level cache.
     * fills the map::max_populated_zlev and returns it
     * @return max_populated_zlev value
     */
    auto calc_max_populated_zlev() -> int;
    /**
     * Conditionally invalidates max_pupulated_zlev cache if the submap uniformity change occurs
     * above current max_pupulated_zlev value
     * @param zlev zlevel where uniformity change occured
     */
    void invalidate_max_populated_zlev(int zlev);

    /**
     * Internal version of the drawsq. Keeps a cached maptile for less re-getting.
     * Returns false if it has drawn all it should, true if `draw_from_above` should be called
     * after.
     */
    auto draw_maptile(
        const catacurses::window& w, const tripoint_bub_ms& p, const maptile& tile,
        const drawsq_params& params) const -> bool;
    /**
     * Draws the tile as seen from above.
     */
    void draw_from_above(
        const catacurses::window& w, const tripoint_bub_ms& p, const maptile& tile,
        const drawsq_params& params) const;

    auto determine_wall_corner(const tripoint_bub_ms& p) const -> int;
    struct apply_directional_light_options {
        tripoint_bub_ms p;
        int direction = 0;
        float luminance = 0.0f;
        uint32_t color_rgb = 0u;
    };
    struct apply_light_arc_options {
        tripoint_bub_ms p;
        units::angle angle = 0_degrees;
        float luminance = 0.0f;
        units::angle wideangle = 30_degrees;
        uint32_t color_rgb = 0u;
    };
    struct apply_light_ray_options {
        std::vector<bool>& lit;
        tripoint_bub_ms s;
        tripoint_bub_ms e;
        float luminance = 0.0f;
        uint32_t color_rgb = 0u;
    };
    // apply a circular light pattern immediately, however it's best to use...
    void apply_light_source(const tripoint_bub_ms& p, float luminance, uint32_t color_rgb = 0u);
    // ...this, which will apply the light after at the end of generate_lightmap, and prevent
    // redundant light rays from causing massive slowdowns, if there's a huge amount of light.
    void add_light_source(const tripoint_bub_ms& p, float luminance, uint32_t color_rgb = 0u);
    // Handle just cardinal directions and 45 deg angles.
    void apply_directional_light(const tripoint_bub_ms& p, int direction, float luminance);
    auto apply_directional_light(const apply_directional_light_options& opt) -> void;
    void apply_light_arc(
        const tripoint_bub_ms& p, units::angle, float luminance,
        units::angle wideangle = 30_degrees);
    auto apply_light_arc(const apply_light_arc_options& opt) -> void;
    void apply_light_ray(
        std::vector<bool>& lit, const tripoint_bub_ms& s, const tripoint_bub_ms& e,
        float luminance);
    auto apply_light_ray(const apply_light_ray_options& opt) -> void;
    void add_light_from_items(
        const tripoint_bub_ms& p, const item_stack::iterator& begin,
        const item_stack::iterator& end);
    auto add_vehicle_to_map(std::unique_ptr<vehicle> veh, bool merge_wrecks)
        -> std::unique_ptr<vehicle>;

    // Internal methods used to bash just the selected features
    // "Externaled" for testing, because the interface to bashing is rng dependent
public:
    // Information on what to bash/what was bashed is read from/written to the
    // bash_params/bash_results struct
    auto bash_items(const tripoint_bub_ms& p, const bash_params& params) -> bash_results;
    auto bash_field(const tripoint_bub_ms& p, const bash_params& params) -> bash_results;

    // Successfully bashing things down
    auto bash_ter_success(const tripoint_bub_ms& p, const bash_params& params) -> bash_results;
    auto bash_furn_success(const tripoint_bub_ms& p, const bash_params& params) -> bash_results;

    // Gets the roof type of the tile at p
    // Second argument refers to whether we have to get a roof (we're over an unpassable tile)
    // or can just return air because we bashed down an entire floor tile
    auto get_roof(const tripoint_bub_ms& p, bool allow_air) const -> ter_id;

    void process_items(int turns = 1);

private:
    // Iterates over every item on the map, passing each item to the provided function.
    auto process_items_in_submap(
        submap& current_submap, const tripoint_bub_sm& gridp, std::vector<item*>& active_items,
        int turns = 1) -> void;
    void process_items_in_vehicles(submap& current_submap, int turns = 1);
    void process_items_in_vehicle(vehicle& cur_veh, submap& current_submap, int turns = 1);

    /** Enum used by functors in `function_over` to control execution. */
    enum iteration_state {
        ITER_CONTINUE = 0, // Keep iterating
        ITER_SKIP_SUBMAP,  // Skip the rest of this submap
        ITER_SKIP_ZLEVEL,  // Skip the rest of this z-level
        ITER_FINISH        // End iteration
    };
    /**
     * Runs a functor over given submaps
     * over submaps in the area, getting next submap only when the current one "runs out" rather
     * than every time. gp in the functor is the map-local submap coordinate, Will silently clip the
     * area to map bounds.
     * @param start Starting point_bub_ms & for function
     * @param end End point_bub_ms & for function
     * @param fun Function to run
     */
    /*@{*/
    template <typename Functor>
    auto function_over(const tripoint_bub_ms& start, const tripoint_bub_ms& end, Functor fun) const
        -> void;
    /**
     * Holds caches for visibility, light, transparency and vehicles
     */
    std::array<std::unique_ptr<level_cache>, OVERMAP_LAYERS> caches;

    /**
     * Flat list of all funnel trap locations in this dimension's loaded submaps.
     * Each entry is (abs_sm position, local tile point_bub_ms & within that submap).
     * Populated by on_submap_loaded() and trap_set(); pruned by on_submap_unloaded()
     * and remove_trap(). Lets fill_water_collectors() skip the mapbuffer scan entirely.
     */
    std::vector<std::pair<tripoint_abs_sm, point_sm_ms>> funnel_locations_;

    /**
     * Direct-mapped cache of coordinate pairs recently checked for visibility.
     * Each slot stores a packed (key, value) entry.  Hash collisions silently
     * evict the old entry — no linked list, no heap allocation.
     *
     * Protected by skew_vision_cache_mutex so that compute_plan() can be
     * called in parallel across monsters (P-6).
     */
    struct vision_cache_slot {
        // 64-bit key: two tripoints packed as 29 bits each (12x + 12y + 5z).
        // Handles coordinates up to 4095 per axis — safe for g_mapsize up to ~340.
        int64_t key = 0;
        char value = -1; // -1 = empty/miss
    };
    static constexpr std::size_t vision_cache_slots = 1 << 17; // 131072 entries (~1.5 MB)
    mutable std::vector<vision_cache_slot> skew_vision_cache;
    // shared_mutex allows concurrent cache reads (common case)
    // while still serialising inserts.  Use shared_lock for reads and
    // unique_lock for writes in map::sees().
    mutable std::unique_ptr<std::shared_mutex> skew_vision_cache_mutex;

    /**
     * Vehicle list doesn't change often, but is pretty expensive.
     */
    VehicleList last_full_vehicle_list;
    bool last_full_vehicle_list_dirty = true;
    std::map<tripoint_bub_ms, std::pair<vehicle*, int>> cached_veh_rope;

    // Note: no bounds check
    auto get_cache(int zlev) const -> level_cache& { return *caches[zlev + OVERMAP_DEPTH]; }

    visibility_variables visibility_variables_cache;

    // caches the highest zlevel above which all zlevels are uniform
    // !value || value->first != the loaded-grid origin means cache is invalid
    std::optional<std::pair<tripoint_abs_sm, int>> max_populated_zlev = std::nullopt;

    // The dimension ID this map is bound to (empty = primary dimension)
    dimension_id bound_dimension_;

public:
    auto get_mapbuffer() -> mapbuffer& { return MAPBUFFER_REGISTRY.get(bound_dimension_); }
    auto get_mapbuffer() const -> mapbuffer& { return MAPBUFFER_REGISTRY.get(bound_dimension_); }
    auto has_rope_at(tripoint_bub_ms pt) const -> bool;
    auto get_rope_at(const tripoint_bub_ms& pt) const -> std::pair<vehicle*, int>;

    auto get_cache_ref(int zlev) const -> const level_cache& {
        return *caches[zlev + OVERMAP_DEPTH];
    }

    /**
     * Holds the individual caches for sounds. Each individual cache has a std::vector<short> that
     * stores mdB spl volumes for the flooded area, a sound_event, and some filtering bools. Each
     * sound is only flooded out to a certain distance for performance and memory reasons.
     * Individual sounds are not referenced directly for volume. See the strut definition for
     * information on the various getters.
     * TODO: total rms volume for a tile getter and a ref to the loudest relevant sound event which
     * could be used to speed up flood filling, informing monsters/npcs of sounds, etc.
     */
    sound_cache m_sound_cache;
    // std::vector< sound_instance_cache > sound_instance_caches;

    /// Return the pathfinding flags for a single tile, rebuilding the per-submap
    /// pf_cache if it has been marked dirty.  Works for any loaded position.
    auto get_pf_special(const tripoint_bub_ms& p) const -> pf_special;

    auto update_visibility_cache(int zlev, const std::function<void()>& while_gpu_pending = {})
        -> void;
    auto make_visibility_variables(int zlev) const -> visibility_variables;
    auto get_visibility_variables_cache() const -> const visibility_variables&;

    void update_submap_active_item_status(const tripoint_bub_ms& p);

    auto get_funnel_locations() const
        -> const std::vector<std::pair<tripoint_abs_sm, point_sm_ms>>& {
        return funnel_locations_;
    }

    // Just exposed for unit test introspection.
    auto get_submaps_with_active_items() const -> const std::set<tripoint_abs_sm>& {
        return get_mapbuffer().get_submaps_with_active_items();
    }
    // Clips the area to map bounds
    auto points_in_rectangle(const tripoint_bub_ms& from, const tripoint_bub_ms& to) const
        -> tripoint_range<tripoint_bub_ms>;
    auto points_in_radius(const tripoint_bub_ms& center, size_t radius, size_t radiusz = 0) const
        -> tripoint_range<tripoint_bub_ms>;
    /**
     * Yields a range of all points that are contained in the map and have the z-level of
     * this map (@ref abs_sub).
     */
    auto points_on_zlevel() const -> tripoint_range<tripoint_bub_ms>;
    /// Same as above, but uses the specific z-level. If the given z-level is invalid, it
    /// returns an empty range.
    auto points_on_zlevel(int z) const -> tripoint_range<tripoint_bub_ms>;

    auto get_active_items_in_radius(const tripoint_bub_ms& center, int radius) const
        -> std::vector<item*>;
    auto get_active_items_in_radius(
        const tripoint_bub_ms& center, int radius, special_item_type type) const
        -> std::vector<item*>;

    /** returns positions of furnitures with matching flag in the overmap terrain*/
    auto find_furnitures_with_flag_in_omt(const tripoint_bub_ms& p, const std::string& flag)
        -> std::vector<tripoint_bub_ms>;

    /**returns positions of furnitures with matching flag in the specified radius*/
    auto find_furnitures_with_flag_in_radius(
        const tripoint_bub_ms& center, size_t radius, const std::string& flag, size_t radiusz = 0)
        -> std::list<tripoint_bub_ms>;
    /**returns positions of furnitures or vehicle parts with matching flag in the specified radius*/
    auto find_furnitures_or_vparts_with_flag_in_radius(
        const tripoint_bub_ms& center, size_t radius, const std::string& flag, size_t radiusz = 0)
        -> std::list<tripoint_bub_ms>;
    /**returns creatures in specified radius*/
    auto get_creatures_in_radius(const tripoint_bub_ms& center, size_t radius, size_t radiusz = 0)
        -> std::list<Creature*>;

    auto access_cache(int zlev) -> level_cache&;
    auto access_cache(int zlev) const -> const level_cache&;
    auto dont_draw_lower_floor(const tripoint_bub_ms& p) -> bool;
};

auto get_map() -> map&;

auto player_reality_bubble_origin() -> tripoint_abs_sm;
auto reality_bubble_origin_from_player(const tripoint_abs_ms& player_pos, int reality_bubble_size)
    -> tripoint_abs_sm;
auto reality_bubble_center_from_origin(const tripoint_abs_sm& origin, int reality_bubble_size)
    -> tripoint_abs_sm;

auto bub_to_abs(const tripoint_bub_ms& p) -> tripoint_abs_ms;
auto abs_to_bub(const tripoint_abs_ms& p) -> tripoint_bub_ms;
auto bub_to_abs(const tripoint_bub_sm& p) -> tripoint_abs_sm;
auto abs_to_bub(const tripoint_abs_sm& p) -> tripoint_bub_sm;
auto bub_to_abs(const point_bub_ms& p) -> point_abs_ms;
auto abs_to_bub(const point_abs_ms& p) -> point_bub_ms;
auto bub_to_abs(const point_bub_sm& p) -> point_abs_sm;
auto abs_to_bub(const point_abs_sm& p) -> point_bub_sm;

auto is_in_reality_bubble_bounds(const tripoint_bub_sm& p) -> bool;
auto is_in_reality_bubble_bounds(const tripoint_bub_ms& p) -> bool;

// Convert against a specific map object's loaded-grid origin, not the player bubble origin.
auto map_local_to_abs(const map& m, const tripoint_bub_ms& local) -> tripoint_abs_ms;
auto abs_to_map_local(const map& m, const tripoint_abs_ms& abs) -> tripoint_bub_ms;
auto map_local_to_abs(const map& m, const tripoint_bub_sm& local) -> tripoint_abs_sm;
auto abs_to_map_local(const map& m, const tripoint_abs_sm& abs) -> tripoint_bub_sm;
auto map_local_to_abs(const map& m, const point_bub_ms& local) -> point_abs_ms;
auto abs_to_map_local(const map& m, const point_abs_ms& abs) -> point_bub_ms;
auto map_local_to_abs(const map& m, const point_bub_sm& local) -> point_abs_sm;
auto abs_to_map_local(const map& m, const point_abs_sm& abs) -> point_bub_sm;

/**
 * RAII guard that temporarily redirects get_map() to a different map object
 * for the duration of its lifetime on the calling thread.
 *
 * Intended use: bind a detached map to an out-of-bubble loaded region, push this
 * guard, then process entities in that region.  All entity AI calls to
 * get_map() transparently receive the bound map rather than the global
 * reality bubble.  On destruction, the previous context is restored.
 *
 * Thread-safe: each thread maintains an independent context stack via
 * thread_local storage, so worker threads (which never push a context)
 * are unaffected.
 *
 * Supports nesting: pushing a second guard while one is active works
 * correctly; the outer context is restored when the inner guard is destroyed.
 */
class scoped_map_context {
public:
    explicit scoped_map_context(map& m) noexcept;
    ~scoped_map_context() noexcept;

    scoped_map_context(const scoped_map_context&) = delete;
    auto operator=(const scoped_map_context&) -> scoped_map_context& = delete;

private:
    map* prev_;
};

// Shift a square grid bitset (side length `size`, submap stride `multiplier`) by `s` submaps.
void shift_bitset_cache(
    cata_dynamic_bitset& cache, int size, int multiplier, const point_rel_sm& s);

auto ter_furn_has_flag(const ter_t& ter, const furn_t& furn, ter_bitflags flag) -> bool;
