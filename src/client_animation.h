#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "animation.h"
#include "coordinates.h"
#include "enums.h"
#include "type_id.h"
#include "units_angle.h"
#include "weather/weather.h"
#include "zone_draw_options.h"

class Character;
class game;
class monster;

namespace game_client
{

struct explosion_animation_options {
    tripoint_bub_ms position;
    int radius;
    nc_color color;
    std::string name;
};

struct custom_explosion_animation_options {
    tripoint_bub_ms position;
    const std::map<tripoint_bub_ms, nc_color> *area;
    std::string name;
    const std::list<std::map<tripoint_bub_ms, explosion_tile>> *layers;
};

struct bullet_animation_options {
    tripoint_bub_ms position;
    int index;
    const std::vector<tripoint_bub_ms> *trajectory;
    char bullet;
    std::string custom_sprite;
};

struct bullet_trajectories_animation_options {
    const draw_bullet_trajectories_options *trajectories;
};

struct hit_mon_animation_options {
    tripoint_bub_ms position;
    const monster *target;
    bool dead;
};

struct hit_player_animation_options {
    tripoint_bub_ms position;
    const Character *target;
    int damage;
};

struct line_animation_options {
    tripoint_bub_ms position;
    tripoint_bub_ms center;
    const std::vector<tripoint_bub_ms> *points;
    bool no_reveal;
};

struct cursor_animation_options {
    tripoint_bub_ms position;
};

struct weather_animation_options {
    const weather_printable *weather;
};

struct zones_animation_options {
    const zone_draw_options *zones;
};

struct radiation_override_options {
    tripoint_bub_ms position;
    int radiation;
};

struct terrain_override_options {
    tripoint_bub_ms position;
    ter_id terrain;
};

struct furniture_override_options {
    tripoint_bub_ms position;
    furn_id furniture;
};

struct graffiti_override_options {
    tripoint_bub_ms position;
    bool has_graffiti;
};

struct trap_override_options {
    tripoint_bub_ms position;
    trap_id trap;
};

struct field_override_options {
    tripoint_bub_ms position;
    field_type_id field;
};

struct item_override_options {
    tripoint_bub_ms position;
    itype_id item;
    mtype_id monster;
    bool highlight;
};

struct vehicle_part_override_options {
    tripoint_bub_ms position;
    vpart_id part;
    int part_mod;
    units::angle direction;
    bool highlight;
    tripoint_mnt_veh mount;
};

struct below_override_options {
    tripoint_bub_ms position;
    bool draw;
};

struct monster_override_options {
    tripoint_bub_ms position;
    mtype_id monster;
    int count;
    bool more;
    Attitude attitude;
};

struct cone_aoe_animation_options {
    tripoint_bub_ms origin;
    const std::map<tripoint_bub_ms, double> *coverage;
};

struct animation_progress_options {
    int multiplier = 1;
    bool draw_popup = true;
};

class animation_service
{
    public:
        virtual ~animation_service() = default;
        virtual auto draw_explosion( const explosion_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_custom_explosion( const custom_explosion_animation_options &/*options*/ ) ->
        bool { return false; }
        virtual auto draw_bullet( const bullet_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_bullet_trajectories( const bullet_trajectories_animation_options &/*options*/ ) ->
        bool { return false; }
        virtual auto draw_hit_mon( const hit_mon_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_hit_player( const hit_player_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_line( const line_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_trail_line( const line_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_line_of( const draw_sprite_line_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_cursor( const cursor_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_highlight( const cursor_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_weather( const weather_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_sct() -> bool { return false; }
        virtual auto draw_zones( const zones_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_radiation_override( const radiation_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_terrain_override( const terrain_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_furniture_override( const furniture_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_graffiti_override( const graffiti_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_trap_override( const trap_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_field_override( const field_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_item_override( const item_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_vehicle_part_override( const vehicle_part_override_options &/*options*/ ) ->
        bool { return false; }
        virtual auto draw_below_override( const below_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_monster_override( const monster_override_options &/*options*/ ) -> bool { return false; }
        virtual auto draw_cone_aoe( const cone_aoe_animation_options &/*options*/ ) -> bool { return false; }
        virtual auto minimap_requires_animation() const -> bool { return false; }
        virtual auto terrain_requires_animation() const -> bool { return false; }
};

auto animation() -> animation_service &; // *NOPAD*
auto set_animation( std::unique_ptr<animation_service> service ) -> void;
auto progress_animation( const animation_progress_options &options = {} ) -> void;
auto install_tiles_animation() -> void;

} // namespace game_client
