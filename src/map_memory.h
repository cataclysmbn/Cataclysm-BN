#pragma once

#include <map>
#include <string>

#include "game_constants.h"
#include "memory_fast.h"
#include "coordinates.h"

class JsonOut;
class JsonIn;

struct memorized_terrain_tile {
    std::string tile;
    int subtile;
    int rotation;

    bool operator==( const memorized_terrain_tile &rhs ) const {
        return ( rotation == rhs.rotation ) && ( subtile == rhs.subtile ) && ( tile == rhs.tile );
    }

    bool operator!=( const memorized_terrain_tile &rhs ) const {
        return !( *this == rhs );
    }
};

/** Represent a submap-sized chunk of tile memory. */
struct mm_submap {
    public:
        friend class map_memory;
        static const memorized_terrain_tile default_tile;
        static const int default_symbol;

        mm_submap();

        /** Whether this mm_submap is empty. Empty submaps are skipped during saving. */
        bool is_empty() const {
            return tiles.empty() && symbols.empty() && terrain_tiles.empty();
        }

        const memorized_terrain_tile &tile( const point_sm_ms &p ) const {
            if( tiles.empty() ) {
                return default_tile;
            } else {
                return tiles[p.y() * SEEX + p.x()];
            }
        }

        void set_tile( const point_sm_ms &p, const memorized_terrain_tile &value ) {
            if( tiles.empty() ) {
                // call 'reserve' first to force allocation of exact size
                tiles.reserve( SEEX * SEEY );
                tiles.resize( SEEX * SEEY, default_tile );
            }
            tiles[p.y() * SEEX + p.x()] = value;
        }

        const memorized_terrain_tile &terrain_tile( const point_sm_ms &p ) const {
            if( terrain_tiles.empty() ) {
                return default_tile;
            } else {
                return terrain_tiles[p.y() * SEEX + p.x()];
            }
        }

        void set_terrain_tile( const point_sm_ms &p, const memorized_terrain_tile &value ) {
            if( terrain_tiles.empty() ) {
                terrain_tiles.reserve( SEEX * SEEY );
                terrain_tiles.resize( SEEX * SEEY, default_tile );
            }
            terrain_tiles[p.y() * SEEX + p.x()] = value;
        }

        int symbol( const point_sm_ms &p ) const {
            if( symbols.empty() ) {
                return default_symbol;
            } else {
                return symbols[p.y() * SEEX + p.x()];
            }
        }

        void set_symbol( const point_sm_ms &p, int value ) {
            if( symbols.empty() ) {
                // call 'reserve' first to force allocation of exact size
                symbols.reserve( SEEX * SEEY );
                symbols.resize( SEEX * SEEY, default_symbol );
            }
            symbols[p.y() * SEEX + p.x()] = value;
        }

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );

    private:
        std::vector<memorized_terrain_tile>
        tiles;         // overlay: furniture, vpart, trap — 0 or SEEX*SEEY
        std::vector<memorized_terrain_tile>
        terrain_tiles; // base terrain layer             — 0 or SEEX*SEEY
        std::vector<int> symbols; // holds either 0 or SEEX*SEEY elements
        bool valid = true;
};

/**
 * Represents a square of mm_submaps.
 * For faster save/load, submaps are collected into regions
 * and each region is saved in its own file.
 */
struct mm_region {
    shared_ptr_fast<mm_submap> submaps[MM_REG_SIZE][MM_REG_SIZE];

    mm_region();

    bool is_empty() const;

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * Manages map tiles memorized by the avatar.
 * Note that there are 2 separate memories in here:
 *   1. memorized graphic tiles (for TILES with a tileset)
 *   2. memorized symbols (for CURSES or TILES in ascii mode)
 * TODO: combine tiles and curses. Also, split map memory into layers (terrain/furn/vpart/...)?
 */
class map_memory
{
    public:
        map_memory();

        /** Load memorized submaps around given global map square pos. */
        void load( const tripoint_abs_ms &pos );

        /** Load legacy memory file. TODO: remove after 0.F (or whatever BN will have instead). */
        void load_legacy( JsonIn &jsin );

        /** Save memorized submaps to disk, drop ones far from given global map square pos. */
        bool save( const tripoint_abs_ms &pos );

        /** Clear all memorized data. */
        void clear();


        /**
         * Prepares map memory for optimized rendering and/or memorization of given region.
         * @param p1 top-left corner of the region, in global ms coords
         * @param p2 bottom-right corner of the region, in global ms coords
         * Both coords are inclusive and should be on the same Z level.
         * @return whether the region was re-cached
         */
        bool prepare_region( const tripoint_abs_ms &p1, const tripoint_abs_ms &p2 );

        /**
         * Memorizes given overlay tile (furniture, vehicle part, trap), overwriting old value.
         * @param pos tile position, in global ms coords.
         */
        void memorize_tile( const tripoint_abs_ms &pos, const std::string &ter,
                            int subtile, int rotation );
        /**
         * Returns memorized overlay tile (furniture, vehicle part, trap).
         * @param pos tile position, in global ms coords.
         */
        const memorized_terrain_tile &get_tile( const tripoint_abs_ms &pos );

        /**
         * Memorizes base terrain tile, overwriting old value.
         * Stored separately from the overlay slot so furniture/vparts don't obscure terrain.
         * @param pos tile position, in global ms coords.
         */
        void memorize_terrain_tile( const tripoint_abs_ms &pos, const std::string &ter,
                                    int subtile, int rotation );
        /**
         * Returns memorized base terrain tile.
         * @param pos tile position, in global ms coords.
         */
        memorized_terrain_tile get_terrain_tile( const tripoint_abs_ms &pos );

        /**
         * For autodrive use only.
         * Checks whether tile at given pos was memorized.
         * @param pos tile position, in global ms coords.
         */
        bool has_memory_for_autodrive( const tripoint_abs_ms &pos );

        /**
         * Memorizes given symbol, overwriting old value.
         * @param pos tile position, in global ms coords.
        */
        void memorize_symbol( const tripoint_abs_ms &pos, int symbol );

        /**
         * Returns memorized symbol.
         * @param pos tile position, in global ms coords.
         */
        int get_symbol( const tripoint_abs_ms &pos );

        /**
         * Clears memorized overlay tile (furniture/vpart/trap) and symbol, leaving terrain memory intact.
         * Use this when a vehicle or furniture leaves a tile so the ground beneath is preserved.
         * @param pos tile position, in global ms coords.
         */
        void clear_memorized_overlay( const tripoint_abs_ms &pos );

        /**
         * Clears memorized tile, symbol, and terrain.
         * @param pos tile position, in global ms coords.
         */
        void clear_memorized_tile( const tripoint_abs_ms &pos );

    private:
        std::map<tripoint_abs_sm, shared_ptr_fast<mm_submap>> submaps;

        std::vector<shared_ptr_fast<mm_submap>> cached;
        tripoint_abs_sm cache_pos;
        point_rel_sm cache_size;

        /** Find, load or allocate a submap. May be slow. @returns the submap. */
        shared_ptr_fast<mm_submap> fetch_submap( const tripoint_abs_sm &sm_pos );
        /** Find submap amongst the loaded submaps. @returns nullptr if failed. */
        shared_ptr_fast<mm_submap> find_submap( const tripoint_abs_sm &sm_pos );
        /** Load submap from disk. @returns nullptr if failed. */
        shared_ptr_fast<mm_submap> load_submap( const tripoint_abs_sm &sm_pos );
        /** Allocate empty submap. @returns the submap. */
        shared_ptr_fast<mm_submap> allocate_submap( const tripoint_abs_sm &sm_pos );

        /**
         * Find, load or allocate a submap.
         * Uses cache made by @ref prepare_region to speed up the lookup.
         * @returns the submap.
         */
        mm_submap &get_submap( const tripoint_abs_sm &sm_pos );

        void clear_cache();
};


