local pos = test_data.pos

local food = gapi.get_items_if({ pos = pos, radius = 1, is_food = true })
local food_radius2 = gapi.get_items_if({ pos = pos, radius = 2, is_food = true })
local rock = gapi.get_items_if({ pos = pos, radius = 1, type = "rock" })
local all = gapi.get_items_if({ pos = pos, radius = 2 })

test_data.food_count = #food
test_data.food_radius2_count = #food_radius2
test_data.rock_count = #rock
test_data.all_count = #all
