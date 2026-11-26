---@type Character
local char = test_data.character

local all = char:get_items_if({})
local food = char:get_items_if({ is_food = true })
local food_category = char:get_items_if({ category = "food" })
local wielded_rock = char:get_items_if({ type = "rock" })
local worn_tshirt = char:get_items_if({ type = "tshirt" })
local flagged = char:get_items_if({ flags = { "TRADER_AVOID" } })
local combined = char:get_items_if({ category = "food", is_food = true })
local worn_only = char:get_items_if({ worn = true })
local wielded_only = char:get_items_if({ wielded = true })
local not_worn = char:get_items_if({ worn = false })
local charged_tools = char:get_items_if({ need_charges = true })
local empty_char_forge = char:get_items_if({ has_charges = { id = "char_forge", amount = 1 } })
local any_char_forge = char:get_items_if({ has_charges = { id = "char_forge", amount = 0 } })
local removed_food = char:remove_items_if({ is_food = true })
local after_remove = char:get_items_if({})

-- reproduce the mod flow: remove by the ItemTypeId object from get_type()
local rock_item = char:get_items_if({ type = "rock" })[1]
local by_object = char:get_items_if({ type = rock_item:get_type() })
local by_table = char:get_items_if({ type = { rock_item:get_type() } })
local removed_by_object = char:remove_items_if({ type = rock_item:get_type() })
test_data.by_object_count = #by_object
test_data.by_table = #by_table
test_data.removed_by_object_count = #removed_by_object
test_data.type_str = tostring(rock_item:get_type():str())
test_data.id_type = type(rock_item:get_type())
test_data.id_eq = (rock_item:get_type() == ItypeId.new("rock"))

test_data.all_count = #all
test_data.food_count = #food
test_data.food_category_count = #food_category
test_data.wielded_rock_count = #wielded_rock
test_data.worn_tshirt_count = #worn_tshirt
test_data.flag_count = #flagged
test_data.combined_count = #combined
test_data.worn_count = #worn_only
test_data.wielded_count = #wielded_only
test_data.not_worn_count = #not_worn
test_data.charged_tools_count = #charged_tools
test_data.empty_char_forge_count = #empty_char_forge
test_data.any_char_forge_count = #any_char_forge
test_data.removed_food_count = #removed_food
test_data.after_remove_count = #after_remove
