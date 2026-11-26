---@type Character
local char = test_data.character
local pos = test_data.pos

-- character-side filters
local alcohol_enum = char:get_items_if({ addiction_type = AddictionType.ALCOHOL })
local alcohol_str = char:get_items_if({ addiction_type = "alcohol" })
local drug_and_alcohol = char:get_items_if({ addiction_type = { "crack", AddictionType.ALCOHOL } })
local no_addiction = char:get_items_if({ addiction_type = AddictionType.NONE })
local stone = char:get_items_if({ material = "stone" })
local alcohol_mat = char:get_items_if({ material = MaterialTypeId.new("alcohol") })
local wheat_or_stone = char:get_items_if({ material = { "wheat", "stone" } })
local food_alcohol = char:get_items_if({ is_food = true, addiction_type = "alcohol" })

test_data.alcohol_enum_count = #alcohol_enum
test_data.alcohol_str_count = #alcohol_str
test_data.drug_and_alcohol_count = #drug_and_alcohol
test_data.no_addiction_count = #no_addiction
test_data.stone_count = #stone
test_data.alcohol_mat_count = #alcohol_mat
test_data.wheat_or_stone_count = #wheat_or_stone
test_data.food_alcohol_count = #food_alcohol

-- map-side filters go through the same handlers
local map_beer = gapi.get_items_if({ pos = pos, radius = 1, addiction_type = "alcohol" })
local map_alcohol_mat = gapi.get_items_if({ pos = pos, radius = 1, material = "alcohol" })
local map_crack = gapi.get_items_if({ pos = pos, radius = 1, addiction_type = "crack" })

test_data.map_beer_count = #map_beer
test_data.map_alcohol_mat_count = #map_alcohol_mat
test_data.map_crack_count = #map_crack
