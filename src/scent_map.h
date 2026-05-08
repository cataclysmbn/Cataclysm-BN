#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "calendar.h"
#include "enums.h" // IWYU pragma: keep
#include "point.h"
#include "type_id.h"

static constexpr int SCENT_MAP_Z_REACH = 1;

class game;
class map;
class JsonObject;

namespace catacurses
{
class window;
} // namespace catacurses

class scent_type
{
    public:
        static void load_scent_type( const JsonObject &jo, const std::string &src );
        void load( const JsonObject &jo, const std::string & );
        static const std::vector<scent_type> &get_all();
        static void check_scent_consistency();
        bool was_loaded = false;

        scenttype_id id;
        std::set<species_id> receptive_species;
        static void reset();
};

class scent_map
{
    protected:
        scenttype_id typescent;
        std::optional<tripoint> player_last_position;
        time_point player_last_moved = calendar::before_time_starts;

        const game &gm;
        map &m_;

        // Per-dimension sets of absolute submap positions that currently have at least
        // one non-zero scent value. Keyed by dimension_id to match MAPBUFFER_REGISTRY.
        // Populated by raw_scent_set(); pruned by decay() once all values reach zero.
        // Lets decay() skip the full mapbuffer scan for each dimension.
        std::map<std::string, std::set<tripoint>> scent_submaps_;

    public:
        scent_map( const game &g, map &m ) : gm( g ), m_( m ) { }

        void deserialize( const std::string &data, bool is_type = false );
        std::string serialize( bool is_type = false ) const;

        void draw( const catacurses::window &win, int div, const tripoint &center ) const;

        void update( const tripoint &center, map &m );
        void reset();
        void decay();

        /**
         * Get the scent value at the given position.
         * An invalid position is allowed and will yield a 0 value.
         * The coordinate system is the same as the @ref map (`g->m`) uses.
         */
        /**@{*/
        void set( const tripoint &p, int value, const scenttype_id &type = scenttype_id() );
        int get( const tripoint &p ) const;
        /**@}*/
        void set_unsafe( const tripoint &p, int value, const scenttype_id &type = scenttype_id() );
        int get_unsafe( const tripoint &p ) const;

        scenttype_id get_type( const tripoint &p ) const;

        bool inbounds( const tripoint &p ) const;
        bool inbounds( point p ) const {
            return inbounds( tripoint( p, 0 ) );
        }

    private:
        /// Read the raw scent integer stored at map-local tile (x, y, z).
        /// Returns 0 for any unloaded submap position.
        auto raw_scent_at( int x, int y, int z ) const -> int;

        /// Write a raw scent integer to map-local tile (x, y, z).
        /// Silently no-ops for any unloaded submap position.
        auto raw_scent_set( int x, int y, int z, int value ) -> void;
};


