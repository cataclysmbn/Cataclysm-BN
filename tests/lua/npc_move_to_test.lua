---@type NPC
local moving_npc = test_data.npc
---@type TripointBubMs
local destination = test_data.destination

moving_npc:move_to(destination)
test_data.moved = moving_npc:get_pos_ms() == destination
