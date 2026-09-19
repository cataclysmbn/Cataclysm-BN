#pragma once

#include "active_item_cache.h"
#include "active_tile_data.h"
#include "calendar.h"
#include "computer.h"
#include "construction_partial.h"
#include "field.h"
#include "game_constants.h"
#include "item.h"
#include "legacy_pathfinding.h"
#include "monster.h"
#include "point.h"
#include "poly_serialized.h"
#include "sounds.h"
#include "type_id.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class JsonIn;
class JsonOut;
class map;
struct level_cache;
struct trap;
struct ter_t;
// tr_null forward-declared here to keep set_trap inline without pulling in all of trap.h.
extern trap_id tr_null;
struct furn_t;
class vehicle;

// enum defines the initial disposition of the monster that is to be spawned
enum class spawn_disposition {
    SpawnDisp_Default,
    SpawnDisp_Friendly,
    SpawnDisp_Pet,
};

struct spawn_point {
    point_sm_ms pos;
    int count;
    mtype_id type;
    int faction_id;
    int mission_id;
    spawn_disposition disposition;
    std::string name;
    spawn_point(
        const mtype_id& T = mtype_id::NULL_ID(), int C = 0, const point_sm_ms& P = point_sm_ms{},
        int FAC = -1, int MIS = -1, spawn_disposition DISP = spawn_disposition::SpawnDisp_Default,
        const std::string& N = "NONE")
        : pos(P),
          count(C),
          type(T),
          faction_id(FAC),
          mission_id(MIS),
          disposition(DISP),
          name(N) {}

    // helper function to convert internal disposition into a binary bool value.
    // This is required to preserve save game compatibility because submaps store/load
    // their spawn_points using a boolean flag.
    auto is_friendly() const -> bool { return disposition != spawn_disposition::SpawnDisp_Default; }

    // helper function to convert binary bool friendly value to internal disposition.
    // This is required to preserve save game compatibility because submaps store/load
    // their spawn_points using a boolean flag.
    static auto friendly_to_spawn_disposition(bool friendly) -> spawn_disposition {
        return friendly ? spawn_disposition::SpawnDisp_Friendly
                        : spawn_disposition::SpawnDisp_Default;
    }
};

template <int sx, int sy> struct maptile_soa {
protected:
    maptile_soa(const tripoint_abs_sm& position, const dimension_id& dim);

public:
    ter_id ter[sx][sy];                // Terrain on each square
    furn_id frn[sx][sy];               // Furniture on each square
    std::uint8_t lum[sx][sy];          // Number of items emitting light on each square
    location_vector<item> itm[sx][sy]; // Items on each square
    field fld[sx][sy];                 // Field on each square
    trap_id trp[sx][sy];               // Trap on each square
    int rad[sx][sy];                   // Irradiation of each square

    void swap_soa_tile(const point_sm_ms& p1, const point_sm_ms& p2);
};

class submap: maptile_soa<SEEX, SEEY> {
public:
    submap(const tripoint_abs_sm& position, const dimension_id& dim);
    ~submap();

    auto get_dimension() const -> const dimension_id { return dim_; }
    auto position() const -> const tripoint_abs_sm { return pos_; }
    auto set_dimension(const dimension_id& dim) -> void;
    auto set_position(const tripoint_abs_sm& position) -> void;

    auto get_trap(const point_sm_ms& p) const -> trap_id { return trp[p.x()][p.y()]; }

    /// The effective trap at a tile: a terrain-attached trap (ter_t::trap) takes
    /// precedence over a standalone trap in the trp array. Mirrors map::tr_at().
    /// Use this (not get_trap()) when a tile may carry a terrain-attached trap,
    /// e.g. a gutter downspout's funnel.
    auto get_effective_trap(const point_sm_ms& p) const -> trap_id {
        const trap_id ter_trap = get_ter(p).obj().trap;
        if (ter_trap != tr_null) { return ter_trap; }
        return get_trap(p);
    }

    void set_trap(const point_sm_ms& p, trap_id trap) {
        is_uniform = false;
        trp[p.x()][p.y()] = trap;
        if (trap != tr_null) { trap_cache.push_back(p); }
    }

    void set_all_traps(const trap_id& trap) {
        std::fill_n(&trp[0][0], elements, trap);
        trap_cache.clear();
    }

    auto get_furn(const point_sm_ms& p) const -> furn_id { return frn[p.x()][p.y()]; }

    void set_furn(const point_sm_ms& p, furn_id furn) {
        is_uniform = false;
        emitter_cache = std::nullopt;
        frn[p.x()][p.y()] = furn;
        frn_vars[p].merge(furn->default_vars);
        if (furn != f_null) { return; }
        frn_vars.erase(p);
    }

    void set_all_furn(const furn_id& furn) {
        std::fill_n(&frn[0][0], elements, furn);
        emitter_cache = std::nullopt;
        if (furn != f_null) { return; }
        // Reset furniture vars on clear
        frn_vars.clear();
    }

    auto get_ter(const point_sm_ms& p) const -> ter_id { return ter[p.x()][p.y()]; }

    void set_ter(const point_sm_ms& p, ter_id terr) {
        is_uniform = false;
        emitter_cache = std::nullopt;
        ter[p.x()][p.y()] = terr;
        if (terr->trap != tr_null) { trap_cache.push_back(p); }
    }

    void set_all_ter(const ter_id& terr) {
        std::fill_n(&ter[0][0], elements, terr);
        emitter_cache = std::nullopt;
    }

    auto get_radiation(const point_sm_ms& p) const -> int { return rad[p.x()][p.y()]; }

    void set_radiation(const point_sm_ms& p, const int radiation) {
        is_uniform = false;
        rad[p.x()][p.y()] = radiation;
    }

    auto get_lum(const point_sm_ms& p) const -> uint8_t { return lum[p.x()][p.y()]; }

    auto static_emitter_tiles() const -> const std::vector<point_sm_ms>&;

    void set_lum(const point_sm_ms& p, uint8_t luminance) {
        is_uniform = false;
        lum[p.x()][p.y()] = luminance;
    }

    void update_lum_add(const point_sm_ms& p, const item& i) {
        is_uniform = false;
        if (i.is_emissive() && lum[p.x()][p.y()] < 255) { lum[p.x()][p.y()]++; }
    }

    void update_lum_rem(const point_sm_ms& p, const item& i);

    // TODO: Replace this as it essentially makes itm public
    auto get_items(const point_sm_ms& p) -> location_vector<item>& { return itm[p.x()][p.y()]; }

    auto get_items(const point_sm_ms& p) const -> const location_vector<item>& {
        return itm[p.x()][p.y()];
    }

    // TODO: Replace this as it essentially makes fld public
    auto get_field(const point_sm_ms& p) -> field& { return fld[p.x()][p.y()]; }

    auto get_field(const point_sm_ms& p) const -> const field& { return fld[p.x()][p.y()]; }

    auto get_ter_vars(const point_sm_ms& p) -> data_vars::data_set& { return ter_vars[p]; };

    auto get_furn_vars(const point_sm_ms& p) -> data_vars::data_set& { return frn_vars[p]; };

    auto get_ter_vars(const point_sm_ms& p) const -> const data_vars::data_set& {
        const auto it = ter_vars.find(p);
        if (it == ter_vars.end()) { return EMPTY_VARS; }
        return it->second;
    };

    auto get_furn_vars(const point_sm_ms& p) const -> const data_vars::data_set& {
        const auto it = ter_vars.find(p);
        if (it == ter_vars.end()) { return EMPTY_VARS; }
        return it->second;
    };

    struct cosmetic_t {
        point_sm_ms pos;
        std::string type;
        std::string str;
    };

    void insert_cosmetic(const point_sm_ms& p, const std::string& type, const std::string& str);

    auto get_temperature() const -> int { return temperature; }

    void set_temperature(int new_temperature) { temperature = new_temperature; }

    auto has_graffiti(const point_sm_ms& p) const -> bool;
    auto get_graffiti(const point_sm_ms& p) const -> const std::string&;
    void set_graffiti(const point_sm_ms& p, const std::string& new_graffiti);
    void delete_graffiti(const point_sm_ms& p);

    // Signage is a pretend union between furniture on a square and stored
    // writing on the square. When both are present, we have signage.
    // Its effect is meant to be cosmetic and atmospheric only.
    auto has_signage(const point_sm_ms& p) const -> bool;
    // Dependent on furniture + cosmetics.
    auto get_signage(const point_sm_ms& p) const -> std::string;
    // Can be used anytime (prevents code from needing to place sign first.)
    void set_signage(const point_sm_ms& p, const std::string& s);
    // Can be used anytime (prevents code from needing to place sign first.)
    void delete_signage(const point_sm_ms& p);

    auto has_computer(const point_sm_ms& p) const -> bool;
    auto get_computer(const point_sm_ms& p) const -> const computer*;
    auto get_computer(const point_sm_ms& p) -> computer*;
    void set_computer(const point_sm_ms& p, const computer& c);
    void delete_computer(const point_sm_ms& p);

    auto contains_vehicle(vehicle*) -> bool;

    void rotate(int turns);

    void store(JsonOut& jsout) const;
    void load(
        JsonIn& jsin, const std::string& member_name, int version, const tripoint_abs_ms offset,
        const dimension_id& dim);

    // If is_uniform is true, this submap is a solid block of terrain
    // Uniform submaps aren't saved/loaded, because regenerating them is faster
    bool is_uniform;

    std::vector<cosmetic_t> cosmetics; // Textual "visuals" for squares

    active_item_cache active_items;

    int field_count = 0;
    // Per-submap flat lists used to avoid full 144-tile scans.
    // Entries may be stale (tile no longer has the relevant data); callers must validate.
    // A stale entry is benign — it just costs a cheap branch on iteration.
    // trap_cache: positions of any non-null trap — standalone (trp) or
    // terrain-attached (ter_t::trap). Maintained by set_trap/set_ter, the
    // rotate() rebuild, and load(); entries may be stale, so callers re-validate.
    std::vector<point_sm_ms> trap_cache;
    // field_cache: positions of tiles with active fields; compacted after each
    // processing pass to remove positions whose fields have fully decayed.
    std::vector<point_sm_ms> field_cache;
    // TODO: A future improvement is to unify all per-tile dirty state into a 144-bit
    // bitmask (e.g. std::bitset<SEEX * SEEY> or three uint64_t words), one per category.
    // Bitmask iteration with _Find_first()/_Find_next() or ctz on 64-bit words is
    // significantly faster than vector<point> for dense cases and essentially free for
    // sparse ones, and multiple masks can be ANDed cheaply to combine conditions.
    // Deferred because it is a broader refactor touching all cache consumers.

    /** Positions of terrain/furniture with emitted light on this submap.
     *  std::nullopt = dirty (needs rebuild by scanning all tiles).
     *  Empty vector = no emitters present.
     *  Rebuilt lazily; invalidated by set_ter/set_all_ter/set_furn/set_all_furn. */
    mutable std::optional<std::vector<point_sm_ms>> emitter_cache;
    // Serialized as "turn_last_touched" (absolute turn number).
    // Initialized to calendar::turn_zero; legacy saves that predate
    // serialization will receive the maximum-capped catchup on first load.
    time_point last_touched = calendar::turn_zero;
    std::vector<spawn_point> spawns;

    // ---- Per-submap simulation caches ----
    // Source of truth for game-logic queries on any loaded submap.
    // terrain-derived caches carry a dirty flag.
    // scent_values is serialized; the other caches are rebuilt on load.

    float transparency_cache[SEEX][SEEY] = {};
    bool outside_cache[SEEX][SEEY] = {};
    bool sheltered_cache[SEEX][SEEY] = {};
    char floor_cache[SEEX][SEEY] = {};
    pf_special pf_special_cache[SEEX][SEEY] = {};
    int scent_values[SEEX][SEEY] = {};
    short absorption_cache[SEEX][SEEY] = {};
    bool sound_wall_cache[SEEX][SEEY] = {};
    // True if any scent_values cell is non-zero. Set in raw_scent_set; cleared by
    // scent_map::decay once all values reach zero. Lets decay() skip unvisited submaps.
    bool has_scent = false;

    bool transparency_dirty = true;
    bool outside_dirty = true;
    bool floor_dirty = true;
    bool pf_dirty = true;
    bool absorption_dirty = true;

    // Since we rebuild the sound_wall_cache at the same time as the absorption cache, we dont need
    // this. bool sound_wall_dirty   = true;

    // Rebuild per-submap caches from terrain/furniture/field data.
    // grid_pos = submap grid coordinates within map m (x,y = submap index, z = z-level).
    // above: the level_cache for z+1 (nullptr at OVERMAP_HEIGHT — base case).
    // outside_cache: true when the tile has sky access via the 3×3 overhang rule.
    // sheltered_cache: true when some overhead cover exists within 3×3 of the tile.
    auto rebuild_outside_cache(const level_cache* above, const tripoint_bub_sm& grid_pos) -> void;
    auto rebuild_floor_cache(const map& m, const tripoint_bub_sm& grid_pos) -> void;
    auto rebuild_pf_cache(const map& m, const tripoint_bub_sm& grid_pos) -> void;
    // rebuild_transparency_cache calls rebuild_outside_cache first if outside_dirty.
    auto rebuild_transparency_cache(const map& m, const tripoint_bub_sm& grid_pos) -> void;

    // Rebuilds the per-submap sound absorption cache from terrain and furniture data.
    // This will also rebuild the sound wall cache for the submap.
    // Check sounds.cpp for implimentation.
    auto rebuild_absorption_cache(const map& m, const tripoint_bub_sm& grid_pos) -> void;
    /**
     * Vehicles on this submap (their (0,0) point is on this submap).
     * This vehicle objects are deleted by this submap when it gets
     * deleted.
     */
    std::vector<std::unique_ptr<vehicle>> vehicles;
    std::map<tripoint_sm_ms, std::unique_ptr<partial_con>> partial_constructions;
    std::map<point_sm_ms, cata::poly_serialized<active_tile_data>> active_furniture;
    std::map<point_sm_ms, time_point> transformer_last_run;

    static void swap(submap& first, submap& second);

private:
    static const data_vars::data_set EMPTY_VARS;
    dimension_id dim_;
    tripoint_abs_sm pos_;
    std::unordered_map<point_sm_ms, data_vars::data_set> ter_vars;
    std::unordered_map<point_sm_ms, data_vars::data_set> frn_vars;

    std::map<point_sm_ms, computer> computers;
    std::unique_ptr<computer> legacy_computer;
    int temperature = 0;

    void update_legacy_computer();

    static constexpr size_t elements = SEEX * SEEY;
};

/**
 * A wrapper for a submap point. Allows getting multiple map features
 * (terrain, furniture etc.) without directly accessing submaps or
 * doing multiple bounds checks and submap gets.
 */
struct maptile {
private:
    friend map; // To allow "sliding" the tile in x/y without bounds checks
    friend submap;
    submap* const sm;
    point_sm_ms pos_;

    auto pos() const -> point_sm_ms { return pos_; }

    maptile(submap* sub, const point_sm_ms& p): sm(sub), pos_(p) {}

public:
    auto get_trap() const -> trap_id { return sm->get_trap(pos()); }

    auto get_furn() const -> furn_id { return sm->get_furn(pos()); }

    auto get_ter() const -> ter_id { return sm->get_ter(pos()); }

    auto get_trap_t() const -> const trap& { return sm->get_trap(pos()).obj(); }

    auto get_furn_t() const -> const furn_t& { return sm->get_furn(pos()).obj(); }
    auto get_ter_t() const -> const ter_t& { return sm->get_ter(pos()).obj(); }

    auto get_field() const -> const field& { return sm->get_field(pos()); }

    auto find_field(const field_type_id& field_to_find) -> field_entry* {
        return sm->get_field(pos()).find_field(field_to_find);
    }

    auto get_radiation() const -> int { return sm->get_radiation(pos()); }

    auto has_graffiti() const -> bool { return sm->has_graffiti(pos()); }

    auto get_graffiti() const -> const std::string& { return sm->get_graffiti(pos()); }

    auto has_signage() const -> bool { return sm->has_signage(pos()); }

    auto get_signage() const -> std::string { return sm->get_signage(pos()); }

    // For map::draw_maptile
    auto get_item_count() const -> size_t { return sm->get_items(pos()).size(); }

    // Assumes there is at least one item
    auto get_uppermost_item() const -> const item& {
        return **std::prev(sm->get_items(pos()).cend());
    }
};
