---@type TripointAbsOmt
local center = test_data.center
---@type NPC
local expected_npc = test_data.expected_npc
---@type Monster
local expected_monster = test_data.expected_monster

local npcs = gapi.get_npcs_near_omt({ center = center, radius = 0, ignore_z = true })
local monsters = gapi.get_monsters_near_omt({ center = center, radius = 0, ignore_z = true })

test_data.npc_count = #npcs
test_data.monster_count = #monsters
test_data.found_expected_npc = npcs[1] == expected_npc
test_data.found_expected_monster = monsters[1] == expected_monster
