#pragma once
#ifndef CATA_TESTS_MAP_HELPERS_H
#    define CATA_TESTS_MAP_HELPERS_H

#    include "coordinates.h"
#    include "type_id.h"

#    include <string>

class monster;
class time_point;

static constexpr int T_BUBBLE_SIZE = 6;
static constexpr int T_MAPSIZE = 2 * T_BUBBLE_SIZE + 3;
static constexpr int T_HALF_MAPSIZE = static_cast<int>(T_MAPSIZE / 2);

static constexpr int T_MAPSIZE_X = SEEX * T_MAPSIZE;
static constexpr int T_MAPSIZE_Y = SEEY * T_MAPSIZE;

static constexpr int T_HALF_MAPSIZE_X = SEEX * T_HALF_MAPSIZE;
static constexpr int T_HALF_MAPSIZE_Y = SEEY * T_HALF_MAPSIZE;

void wipe_map_terrain();
void clear_creatures();
void clear_npcs();
void clear_fields(int zlevel);
void clear_items(int zlevel);
void clear_map();
void clear_overmap();
void put_player_underground();
auto move_player_out_of_the_way() -> void;
monster& spawn_test_monster(const std::string& monster_type, const tripoint_bub_ms& start);
void clear_vehicles();
void build_test_map(const ter_id& terrain);
void build_water_test_map(const ter_id& surface, const ter_id& mid, const ter_id& bottom);
void set_time(const time_point& time);
static constexpr tripoint_abs_ms test_origin = tripoint_abs_ms::zero();
point_abs_sm bub_abs_sub();
// Returns the bubble projected absolute 0,0,0 point
tripoint_bub_ms bub_test_origin();

#endif // CATA_TESTS_MAP_HELPERS_H
