#pragma once
#ifndef CATA_TESTS_PLAYER_HELPERS_H
#    define CATA_TESTS_PLAYER_HELPERS_H

#    include "coordinates.h"
#    include "type_id.h"

#    include <string>
#    include <vector>

class npc;
class player;

auto get_remaining_charges(const std::string& tool_id) -> int;
auto player_has_item_of_type(const std::string&) -> bool;
void clear_character(player&, bool debug_storage = true);
void clear_avatar();
void process_activity(player& dummy);

auto spawn_npc(const tripoint_bub_ms&, const std::string& npc_class) -> npc&; // *NOPAD*
void give_and_activate_bionic(player& p, const bionic_id& bioid);

void arm_character(
    player& shooter, const std::string& gun_type, const std::vector<std::string>& mods = {},
    const std::string& ammo_type = "");

#endif // CATA_TESTS_PLAYER_HELPERS_H
