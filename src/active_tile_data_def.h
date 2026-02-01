#pragma once

#include "active_tile_data.h"
#include "point.h"
#include "type_id.h"

namespace active_tiles
{
struct furn_transform {
    furn_str_id id = furn_str_id::NULL_ID();
    std::string msg;

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );
};
} // namespace active_tiles

class battery_tile : public active_tile_data
{
    public:
        /* In kJ */
        int stored;
        int max_stored;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;

        int get_resource() const;
        int mod_resource( int amt );
};

class charge_watcher_tile : public active_tile_data
{
    public:
        /* In J */
        int min_power;
        active_tiles::furn_transform transform;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
};

class charger_tile : public active_tile_data
{
    public:
        /* In W */
        int power;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
};

class solar_tile : public active_tile_data
{
    public:
        /* In W */
        int power;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
        auto get_power_w() const -> int;
};

class steady_consumer_tile : public active_tile_data
{
    public:
        /* In J */
        int power;
        time_duration consume_every = 1_seconds;
        active_tiles::furn_transform transform;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
};

class vehicle_connector_tile : public active_tile_data
{
    public:
        std::vector<tripoint_abs_ms> connected_vehicles;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
};

class countdown_tile : public active_tile_data
{
    public:
        time_duration timer = 5_seconds;
        active_tiles::furn_transform transform;
        int ticks = -1;

        void update_internal( time_point to, const tripoint_abs_ms &p, distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
};
