local pos = test_data.pos

local food = gapi.get_items_if({ pos = pos, radius = 1, is_food = true })
local food_radius2 = gapi.get_items_if({ pos = pos, radius = 2, is_food = true })
local rock = gapi.get_items_if({ pos = pos, radius = 1, type = "rock" })
local all = gapi.get_items_if({ pos = pos, radius = 2 })
local contained_food = gapi.get_items_if({ pos = pos, radius = 2, type = "banana" })

test_data.food_count = #food
test_data.food_radius2_count = #food_radius2
test_data.rock_count = #rock
test_data.all_count = #all
test_data.contained_food_count = #contained_food

-- sacrifice flow: find food within 1 tile of pos and delete it from the map
local map = gapi.get_map()
local sacrificed = nil
for _, pt in pairs(map:points_in_radius(pos, 1)) do
  for _, itm in pairs(map:get_items_at(pt)) do
    if itm:is_food() and sacrificed == nil then
      map:remove_item_at(pt, itm)
      sacrificed = itm:display_name(1)
    end
  end
end
test_data.sacrificed = sacrificed
test_data.food_after_sacrifice = #gapi.get_items_if({ pos = pos, radius = 1, is_food = true })
