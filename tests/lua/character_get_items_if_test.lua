---@type Character
local char = test_data.character

local all = char:get_items_if({})
local food = char:get_items_if({ is_food = true })
local food_category = char:get_items_if({ category = "food" })
local wielded_rock = char:get_items_if({ type = "rock" })
local worn_tshirt = char:get_items_if({ type = "tshirt" })
local flagged = char:get_items_if({ flags = { "TRADER_AVOID" } })
local combined = char:get_items_if({ category = "food", is_food = true })

test_data.all_count = #all
test_data.food_count = #food
test_data.food_category_count = #food_category
test_data.wielded_rock_count = #wielded_rock
test_data.worn_tshirt_count = #worn_tshirt
test_data.flag_count = #flagged
test_data.combined_count = #combined
