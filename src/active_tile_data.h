#pragma once

#include <string>
#include "calendar.h"
#include "coordinates.h"

class JsonObject;
class JsonOut;
struct tripoint;
class distribution_grid;

class active_tile_data
{
    public:
        static active_tile_data *create( const std::string &id );

    private:
        time_point last_updated;

    protected:
        /**
         * @param to the time to update to
         * @param p absolute map coordinates (@ref map::getabs) of the tile being updated
         * @param grid distribution grid being updated, containing the tile being updated
         */
        virtual void update_internal( time_point to, const tripoint_abs_ms &p,
                                      distribution_grid &grid ) = 0;

    public:
        void update( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) {
            update_internal( to, p, grid );
            last_updated = to;
        }

        time_point get_last_updated() {
            return last_updated;
        }
        void set_last_updated( time_point t ) {
            last_updated = t;
        }

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );

        virtual ~active_tile_data();
        virtual active_tile_data *clone() const = 0;
        virtual const std::string &get_type() const = 0;

        virtual void store( JsonOut &jsout ) const = 0;
        virtual void load( JsonObject &jo ) = 0;
};

// TODO: Better place for this
namespace active_tiles
{

// TODO: Don't return a raw pointer
template <typename T = active_tile_data>
T * furn_at( const tripoint_abs_ms &pos );

} // namespace active_tiles

