local crt_lab_veh_house = {}

research = function(data, point)
  data:nest("crt_lab_veh_house_living", Point.new(point.x, point.y))
  data:nest("crt_lab_veh_house_frame", Point.new(14 + point.x, point.y))
  data:nest("crt_lab_veh_house_research_bed", Point.new(15 + point.x, point.y))
  data:nest("crt_lab_veh_house_research_office", Point.new(15 + point.x, point.y + 5))
end

barracks = function(data, point)
  data:nest("crt_lab_veh_barracks_dorm", Point.new(point.x, point.y))
  data:nest("crt_lab_veh_barracks_dorm", Point.new(14 + point.x, point.y))
end

local houses = { barracks, research }

draw_house_half = function(data, map, norm, flip)
  local do_rot = gapi.rng(1, 2) == 1
  if do_rot then
    map:rotate(2)
    houses[gapi.rng(1, #houses)](data, flip)
  else
    houses[gapi.rng(1, #houses)](data, norm)
  end
  if do_rot then map:rotate(2) end
end

crt_lab_veh_house.draw = function(data, map)
  data:generate("crt_lab_veh_house_backdrop")
  -- Generate rotates the map but we need unrotated maps for nested generation
  -- Rotation has 4 states, you rotate 3 need 1 more to get back to normal and so on
  map:rotate(4 - data:get_rotation())
  local do_rot = gapi.rng(1, 2) == 1
  draw_house_half(data, map, Point.new(1, 1), Point.new(1, 16))
  draw_house_half(data, map, Point.new(1, 16), Point.new(1, 1))
  -- Now that all the nested is dealt with rerotate the map
  map:rotate(data:get_rotation())
end

return crt_lab_veh_house
