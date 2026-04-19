#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <iterator>
#include <map>

#include "active_item_cache.h"
#include "active_tile_data.h"
#include "calendar.h"
#include "computer.h"
#include "construction_partial.h"
#include "field.h"
#include "game_constants.h"
#include "item.h"
#include "legacy_pathfinding.h"
#include "type_id.h"
#include "monster.h"
#include "point.h"
#include "poly_serialized.h"

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
    point pos;
    int count;
    mtype_id type;
    int faction_id;
    int mission_id;
    spawn_disposition disposition;
    std::string name;
    spawn_point( const mtype_id &T = mtype_id::NULL_ID(), int C = 0, point P = point_zero,
                 int FAC = -1, int MIS = -1, spawn_disposition DISP = spawn_disposition::SpawnDisp_Default,
                 const std::string &N = "NONE" ) :
        pos( P ), count( C ), type( T ), faction_id( FAC ),
        mission_id( MIS ), disposition( DISP ), name( N ) {}

    // helper function to convert internal disposition into a binary bool value.
    // This is required to preserve save game compatibility because submaps store/load
    // their spawn_points using a boolean flag.
    bool is_friendly( ) const {
        return disposition != spawn_disposition::SpawnDisp_Default;
    }

    // helper function to convert binary bool friendly value to internal disposition.
    // This is required to preserve save game compatibility because submaps store/load
    // their spawn_points using a boolean flag.
    static spawn_disposition friendly_to_spawn_disposition( bool friendly ) {
        return friendly ? spawn_disposition::SpawnDisp_Friendly
               : spawn_disposition::SpawnDisp_Default;
    }
};

template<int sx, int sy>
struct maptile_soa {
    protected:
        maptile_soa( tripoint offset );
    public:
        ter_id             ter[sx][sy];  // Terrain on each square
        furn_id            frn[sx][sy];  // Furniture on each square
        std::uint8_t       lum[sx][sy];  // Number of items emitting light on each square
        location_vector<item> itm[sx][sy]; // Items on each square
        field              fld[sx][sy];  // Field on each square
        trap_id            trp[sx][sy];  // Trap on each square
        int                rad[sx][sy];  // Irradiation of each square

        void swap_soa_tile( point p1, point p2 );
};

class submap : maptile_soa<SEEX, SEEY>
{
    public:
        submap( tripoint offset );
        ~submap();

        trap_id get_trap( point p ) const {
            return trp[p.x][p.y];
        }

        void set_trap( point p, trap_id trap ) {
            is_uniform = false;
            trp[p.x][p.y] = trap;
            if( trap != tr_null ) {
                trap_cache.push_back( p );
            }
        }

        void set_all_traps( const trap_id &trap ) {
            std::uninitialized_fill_n( &trp[0][0], elements, trap );
            trap_cache.clear();
        }

        furn_id get_furn( point p ) const {
            return frn[p.x][p.y];
        }

        void set_furn( point p, furn_id furn ) {
            is_uniform = false;
            emitter_cache = std::nullopt;
            frn[p.x][p.y] = furn;
            frn_vars[p].merge( furn->default_vars );
            if( furn != f_null ) {
                return;
            }
            frn_vars.erase( p );
        }

        void set_all_furn( const furn_id &furn ) {
            std::uninitialized_fill_n( &frn[0][0], elements, furn );
            emitter_cache = std::nullopt;
            if( furn != f_null ) {
                return;
            }
            // Reset furniture vars on clear
            frn_vars.clear();
        }

        ter_id get_ter( point p ) const {
            return ter[p.x][p.y];
        }

        void set_ter( point p, ter_id terr ) {
            is_uniform = false;
            ter[p.x][p.y] = terr;
        }

        void set_all_ter( const ter_id &terr ) {
            std::uninitialized_fill_n( &ter[0][0], elements, terr );
        }

        int get_radiation( point p ) const {
            return rad[p.x][p.y];
        }

        void set_radiation( point p, const int radiation ) {
            is_uniform = false;
            rad[p.x][p.y] = radiation;
        }

        uint8_t get_lum( point p ) const {
            return lum[p.x][p.y];
        }

        void set_lum( point p, uint8_t luminance ) {
            is_uniform = false;
            lum[p.x][p.y] = luminance;
        }

        void update_lum_add( point p, const item &i ) {
            is_uniform = false;
            if( i.is_emissive() && lum[p.x][p.y] < 255 ) {
                lum[p.x][p.y]++;
            }
        }

        void update_lum_rem( point p, const item &i );

        // TODO: Replace this as it essentially makes itm public
        location_vector<item> &get_items( const point &p ) {
            return itm[p.x][p.y];
        }

        const location_vector<item> &get_items( const point &p ) const {
            return itm[p.x][p.y];
        }

        // TODO: Replace this as it essentially makes fld public
        field &get_field( point p ) {
            return fld[p.x][p.y];
        }

        const field &get_field( point p ) const {
            return fld[p.x][p.y];
        }

        data_vars::data_set &get_ter_vars( point p ) {
            return ter_vars[p];
        };

        data_vars::data_set &get_furn_vars( point p ) {
            return frn_vars[p];
        };

        const data_vars::data_set &get_ter_vars( point p ) const {
            const auto it = ter_vars.find( p );
            if( it == ter_vars.end() ) {
                return EMPTY_VARS;
            }
            return it->second;
        };

        const data_vars::data_set &get_furn_vars( point p ) const {
            const auto it = ter_vars.find( p );
            if( it == ter_vars.end() ) {
                return EMPTY_VARS;
            }
            return it->second;
        };

        struct cosmetic_t {
            point pos;
            std::string type;
            std::string str;
        };

        void insert_cosmetic( point p, const std::string &type, const std::string &str );

        int get_temperature() const {
            return temperature;
        }

        void set_temperature( int new_temperature ) {
            temperature = new_temperature;
        }

        bool has_graffiti( point p ) const;
        const std::string &get_graffiti( point p ) const;
        void set_graffiti( point p, const std::string &new_graffiti );
        void delete_graffiti( point p );

        // Signage is a pretend union between furniture on a square and stored
        // writing on the square. When both are present, we have signage.
        // Its effect is meant to be cosmetic and atmospheric only.
        bool has_signage( point p ) const;
        // Dependent on furniture + cosmetics.
        std::string get_signage( point p ) const;
        // Can be used anytime (prevents code from needing to place sign first.)
        void set_signage( point p, const std::string &s );
        // Can be used anytime (prevents code from needing to place sign first.)
        void delete_signage( point p );

        bool has_computer( point p ) const;
        const computer *get_computer( point p ) const;
        computer *get_computer( point p );
        void set_computer( point p, const computer &c );
        void delete_computer( point p );

        bool contains_vehicle( vehicle * );

        void rotate( int turns );

        void store( JsonOut &jsout ) const;
        void load( JsonIn &jsin, const std::string &member_name, int version, const tripoint offset );

        // If is_uniform is true, this submap is a solid block of terrain
        // Uniform submaps aren't saved/loaded, because regenerating them is faster
        bool is_uniform;

        std::vector<cosmetic_t> cosmetics; // Textual "visuals" for squares

        active_item_cache active_items;

        int field_count = 0;
        // Per-submap flat lists used to avoid full 144-tile scans.
        // Entries may be stale (tile no longer has the relevant data); callers must validate.
        // A stale entry is benign — it just costs a cheap branch on iteration.
        // trap_cache: positions of any non-null trap; rebuilt incrementally via set_trap.
        std::vector<point> trap_cache;
        // field_cache: positions of tiles that have ever had a field added this load cycle.
        // Append-only; cleared when field_count drops to zero after processing.
        std::vector<point> field_cache;
        // TODO: A future improvement is to unify all per-tile dirty state into a 144-bit
        // bitmask (e.g. std::bitset<SEEX * SEEY> or three uint64_t words), one per category.
        // Bitmask iteration with _Find_first()/_Find_next() or ctz on 64-bit words is
        // significantly faster than vector<point> for dense cases and essentially free for
        // sparse ones, and multiple masks can be ANDed cheaply to combine conditions.
        // Deferred because it is a broader refactor touching all cache consumers.

        /** Positions of EMITTER furniture on this submap.
         *  std::nullopt = dirty (needs rebuild by scanning all tiles).
         *  Empty vector = no emitters present.
         *  Rebuilt lazily; invalidated by set_furn / set_all_furn. */
        std::optional<std::vector<point>> emitter_cache;
        // Serialized as "turn_last_touched" (absolute turn number).
        // Initialized to calendar::turn_zero; legacy saves that predate
        // serialization will receive the maximum-capped catchup on first load.
        time_point last_touched = calendar::turn_zero;
        std::vector<spawn_point> spawns;

        // ---- Per-submap simulation caches ----
        // Source of truth for game-logic queries on any loaded submap.
        // terrain-derived caches carry a dirty flag.
        // scent_values is serialized; the other caches are rebuilt on load.

        float  transparency_cache[SEEX][SEEY] = {};
        bool   outside_cache[SEEX][SEEY]      = {};
        bool   sheltered_cache[SEEX][SEEY]    = {};
        char   floor_cache[SEEX][SEEY]         = {};
        pf_special pf_special_cache[SEEX][SEEY]  = {};
        int    scent_values[SEEX][SEEY]        = {};

        bool transparency_dirty = true;
        bool outside_dirty      = true;
        bool sheltered_dirty    = true;
        bool floor_dirty        = true;
        bool pf_dirty           = true;

        // Rebuild per-submap caches from terrain/furniture/field data.
        // grid_pos = submap grid coordinates within map m (x,y = submap index, z = z-level).
        // above: the level_cache for z+1 (nullptr at OVERMAP_HEIGHT — base case).
        // outside_cache: true when the tile has sky access via the 3×3 overhang rule.
        // sheltered_cache: true when some overhead cover exists within 3×3 of the tile.
        auto rebuild_outside_cache( const level_cache *above, tripoint grid_pos ) -> void;
        auto rebuild_sheltered_cache( const level_cache *above, tripoint grid_pos ) -> void;
        auto rebuild_floor_cache( const map &m, tripoint grid_pos ) -> void;
        auto rebuild_pf_cache( const map &m, tripoint grid_pos ) -> void;
        // rebuild_transparency_cache calls rebuild_outside_cache first if outside_dirty.
        auto rebuild_transparency_cache( const map &m, tripoint grid_pos ) -> void;
        /**
         * Vehicles on this submap (their (0,0) point is on this submap).
         * This vehicle objects are deleted by this submap when it gets
         * deleted.
         */
        std::vector<std::unique_ptr<vehicle>> vehicles;
        std::map<tripoint, std::unique_ptr<partial_con>> partial_constructions;
        std::map<point_sm_ms, cata::poly_serialized<active_tile_data>> active_furniture;
        std::map<point_sm_ms, time_point> transformer_last_run;

        static void swap( submap &first, submap &second );

    private:
        static const data_vars::data_set EMPTY_VARS;
        std::unordered_map<point, data_vars::data_set> ter_vars;
        std::unordered_map<point, data_vars::data_set> frn_vars;

        std::map<point, computer> computers;
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
        submap *const sm;
        point pos_;

        point pos() const {
            return pos_;
        }

        maptile( submap *sub, point p ) :
            sm( sub ), pos_( p ) { }
    public:
        trap_id get_trap() const {
            return sm->get_trap( pos() );
        }

        furn_id get_furn() const {
            return sm->get_furn( pos() );
        }

        ter_id get_ter() const {
            return sm->get_ter( pos() );
        }

        const trap &get_trap_t() const {
            return sm->get_trap( pos() ).obj();
        }

        const furn_t &get_furn_t() const {
            return sm->get_furn( pos() ).obj();
        }
        const ter_t &get_ter_t() const {
            return sm->get_ter( pos() ).obj();
        }

        const field &get_field() const {
            return sm->get_field( pos() );
        }

        field_entry *find_field( const field_type_id &field_to_find ) {
            return sm->get_field( pos() ).find_field( field_to_find );
        }

        int get_radiation() const {
            return sm->get_radiation( pos() );
        }

        bool has_graffiti() const {
            return sm->has_graffiti( pos() );
        }

        const std::string &get_graffiti() const {
            return sm->get_graffiti( pos() );
        }

        bool has_signage() const {
            return sm->has_signage( pos() );
        }

        std::string get_signage() const {
            return sm->get_signage( pos() );
        }

        // For map::draw_maptile
        size_t get_item_count() const {
            return sm->get_items( pos() ).size();
        }

        // Assumes there is at least one item
        const item &get_uppermost_item() const {
            return **std::prev( sm->get_items( pos() ).cend() );
        }
};

