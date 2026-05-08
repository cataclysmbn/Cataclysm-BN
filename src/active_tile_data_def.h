#pragma once

#include <optional>
#include <string>

#include "active_tile_data.h"
#include "coordinates.h"
#include "point.h"
#include "submap_load_manager.h"
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

/**
 * Active tile for a generic dimensional portal.
 *
 * Transports the player to a fixed target position in a target dimension when
 * examined.  Optionally pre-loads an area around the target, supports item-
 * linkability and bionic/vehicle tapping, and can generate a destination special
 * on first use (dynamic portals).
 */
class portal_tile : public active_tile_data
{
    public:
        /// Dimension ID of the destination ("" = primary).
        std::string target_dim_id;
        /// Absolute position of the landing spot in the target dimension.
        tripoint_abs_ms target_pos;
        /// Number of submaps to keep resident around target_pos (0 = no preload).
        int load_radius = 0;
        /// itype flag: items with this flag can link to this portal.
        std::string linkable_item_flag;
        /// If true, bio_portal_tap and vp_portal_tap can link to this portal.
        bool allow_bionic_tap = false;
        /// If true player can enter but not use the far end to return.
        bool one_way = false;
        /// On first use: generate this overmap_special in target_dim_id then set target_pos.
        overmap_special_id dynamic_special;
        /// True once the target has been configured.
        bool linked = false;

        void update_internal( time_point to, const tripoint_abs_ms &p,
                              distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;

    private:
        /// Load handle keeping the target area resident (0 = not active).
        load_request_handle preload_handle_ = 0;
};

/**
 * Active tile for a power-portal node (interdimensional/long-range electrical bridge).
 *
 * Stores the link configuration persistently so that the link survives save/load.
 * Actual power equalisation and upkeep are handled by game::tick_portal_links()
 * each turn; this tile's update_internal() is intentionally a no-op.
 *
 * Named grid_link_tile to avoid collision with dimensional portal terminology.
 */
class grid_link_tile : public active_tile_data
{
    public:
        /// Total upkeep cost (kJ) for one linked portal pair, charged every
        /// 300 s.  Expressed in the same kJ-per-300-s unit used by grid
        /// steady_consumer tiles, so the voltmeter shows it as this many "W".
        static constexpr int upkeep_kj = 60;

        /// True once a link target has been configured.
        bool linked = false;
        /// True when the link is paused (power failure or manual player pause).
        /// Ignored when linked == false.
        bool paused = false;
        /// Dimension ID of the far-end portal ("" = primary dimension).
        std::string target_dim_id;
        /// Absolute map-square position of the far-end portal.
        tripoint_abs_ms target_pos;

        void update_internal( time_point to, const tripoint_abs_ms &p,
                              distribution_grid &grid ) override;
        active_tile_data *clone() const override;
        const std::string &get_type() const override;
        void store( JsonOut &jsout ) const override;
        void load( JsonObject &jo ) override;
};
