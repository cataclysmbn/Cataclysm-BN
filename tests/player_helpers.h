#pragma once
#ifndef CATA_TESTS_PLAYER_HELPERS_H
#    define CATA_TESTS_PLAYER_HELPERS_H

#    include "coordinates.h"
#    include "type_id.h"

#    include <string>
#    include <vector>

class npc;
class player;

// Absolute map origin used as the default test center.
// Coincides with the traditional tripoint_bub_ms( 60, 60, 0 ) bubble center
// after a fresh clear_all_state() / clear_character().
// Tests should use setpos( test_origin_abs ) + ensure_simulated_islands_for()
// rather than place_player() for map-spawn behavior.
static constexpr tripoint_abs_ms test_origin_abs( 60, 60, 0 );

int get_remaining_charges(const std::string& tool_id);
bool player_has_item_of_type(const std::string&);
void clear_character(player&, bool debug_storage = true);
void clear_avatar();
void process_activity(player& dummy);

npc& spawn_npc(const tripoint_bub_ms&, const std::string& npc_class);
void give_and_activate_bionic(player& p, const bionic_id& bioid);

void arm_character(
    player& shooter, const std::string& gun_type, const std::vector<std::string>& mods = {},
    const std::string& ammo_type = "");

#endif // CATA_TESTS_PLAYER_HELPERS_H
