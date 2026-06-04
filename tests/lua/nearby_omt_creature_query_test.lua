---@type TripointAbsOmt
local center = test_data.center
---@type NPC
local expected_npc = test_data.expected_npc
---@type Monster
local expected_monster = test_data.expected_monster

local npcs = gapi.get_npcs_near_omt(center, 0, true)
local monsters = gapi.get_monsters_near_omt(center, 0, true)
local active_npcs = gapi.get_active_npcs()
local active_monsters = gapi.get_active_monsters()

test_data.npc_count = #npcs
test_data.monster_count = #monsters
test_data.found_expected_npc = npcs[1] == expected_npc
test_data.found_expected_monster = monsters[1] == expected_monster
test_data.active_npc_count = #active_npcs
test_data.active_monster_count = #active_monsters
test_data.found_expected_active_npc = active_npcs[1] == expected_npc
test_data.found_expected_active_monster = active_monsters[1] == expected_monster
