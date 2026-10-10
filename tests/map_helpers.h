#pragma once
#ifndef CATA_TESTS_MAP_HELPERS_H
#    define CATA_TESTS_MAP_HELPERS_H

#    include "coordinates.h"
#    include "type_id.h"

#    include <string>

class monster;
class time_point;

void wipe_map_terrain();
void clear_creatures();
void clear_npcs();
void clear_fields(int zlevel);
void clear_items(int zlevel);
void clear_map();
void clear_overmap();
void put_player_underground();
auto move_player_out_of_the_way() -> void;
/// Binds the real map to `dim` and releases every load request, as a test that loads its map
/// directly holds none.
auto rebind_map_dimension(const dimension_id& dim) -> void;
auto spawn_test_monster(const std::string& monster_type, const tripoint_bub_ms& start)
    -> monster&; // *NOPAD*
void clear_vehicles();
void build_test_map(const ter_id& terrain);
void build_water_test_map(const ter_id& surface, const ter_id& mid, const ter_id& bottom);
void set_time(const time_point& time);

#endif // CATA_TESTS_MAP_HELPERS_H
