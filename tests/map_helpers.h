#pragma once
#ifndef CATA_TESTS_MAP_HELPERS_H
#    define CATA_TESTS_MAP_HELPERS_H

#    include "coordinates.h"
#    include "type_id.h"

#    include <string>

class monster;
class time_point;

inline constexpr auto test_origin = tripoint_abs_ms::zero();

void wipe_map_terrain();
void clear_creatures();
void clear_npcs();
void clear_fields(int zlevel);
void clear_items(int zlevel);
void clear_map();
void clear_overmap();
void put_player_underground();
auto move_player_out_of_the_way() -> void;
auto spawn_test_monster(const std::string& monster_type, const tripoint_bub_ms& start)
    -> monster&; // *NOPAD*
void clear_vehicles();
void build_test_map(const ter_id& terrain);
void build_water_test_map(const ter_id& surface, const ter_id& mid, const ter_id& bottom);
void set_time(const time_point& time);
auto test_origin_at_z(int z) -> tripoint_abs_ms;
auto test_map_origin() -> point_abs_sm;
auto bub_test_origin() -> tripoint_bub_ms;
auto bub_test_origin(int z) -> tripoint_bub_ms;

#endif // CATA_TESTS_MAP_HELPERS_H
