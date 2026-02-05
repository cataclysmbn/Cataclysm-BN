#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "point.h"

struct lua_tile_entry {
    std::string tile_id;           // Any tileset sprite ID
    tripoint_abs_ms pos;           // Absolute world position (map squares)
    int priority;                  // Higher = drawn on top
    int duration_turns;            // -1 = permanent
    bool cleanup_outside_bubble;   // true = remove when outside reality bubble
    time_point created_at;         // calendar::turn at creation
    int handle;                    // Unique ID for removal
};

class lua_tile_manager
{
    public:
        static lua_tile_manager &get();

        /**
         * Add a tile to be drawn at an absolute world position.
         * @param tile_id Any tileset sprite ID string
         * @param abs_pos Absolute map-square coordinates
         * @param priority Draw order (higher = on top)
         * @param duration_turns Turns until expiry; -1 = permanent (use with care)
         * @param cleanup_outside_bubble If true, remove when outside reality bubble
         * @return Unique handle for later removal
         */
        int add_tile( const std::string &tile_id, const tripoint &abs_pos,
                      int priority, int duration_turns, bool cleanup_outside_bubble );

        /** Remove a specific tile by its handle. Returns true if found and removed. */
        bool remove_tile( int handle );

        /** Remove all tiles at the given absolute position. Returns count removed. */
        int remove_tiles_at( const tripoint &abs_pos );

        /** Remove all lua tiles. */
        void clear_all();

        /**
         * Get tiles at a local map position for rendering.
         * Converts local->abs internally, populates out sorted by priority ascending.
         */
        void get_tiles_at_local( const tripoint &local_pos,
                                 std::vector<const lua_tile_entry *> &out ) const;

        /** Periodic cleanup: removes expired and out-of-bubble entries. */
        void cleanup();

        bool empty() const {
            return entries_.empty();
        }

    private:
        lua_tile_manager() = default;

        int next_handle_ = 1;
        std::vector<lua_tile_entry> entries_;
};
