#pragma once
#ifndef CATA_TESTS_MAP_HELPERS_H
#define CATA_TESTS_MAP_HELPERS_H

#include <string>

#include "coordinates.h"
#include "type_id.h"

class map;
class monster;
class time_point;

auto map_local_to_abs( const map &m, const tripoint_bub_ms &local ) -> tripoint_abs_ms;
auto map_local_to_abs( const map &m, const tripoint_bub_sm &local ) -> tripoint_abs_sm;
auto abs_to_map_local( const map &m, const tripoint_abs_ms &abs ) -> tripoint_bub_ms;
void wipe_map_terrain();
void clear_creatures();
void clear_npcs();
void clear_fields( int zlevel );
void clear_items( int zlevel );
void clear_map();
void clear_overmap();
void put_player_underground();
monster &spawn_test_monster( const std::string &monster_type, const tripoint_bub_ms &start );
void clear_vehicles();
void build_test_map( const ter_id &terrain );
void build_water_test_map( const ter_id &surface, const ter_id &mid, const ter_id &bottom );
void set_time( const time_point &time );

#endif // CATA_TESTS_MAP_HELPERS_H
