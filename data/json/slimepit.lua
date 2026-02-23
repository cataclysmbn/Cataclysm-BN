local slimepit = {}

slimepit.draw = function(data, map)
  local ot_match_prefix = 2
  local ter = data:id()
  local z = data:zlevel()
  if z == 0 then
    data:fill_groundcover()
  else
    map:draw_fill_background("t_rock_floor")
  end
  for dir = 0, 3 do
    if not map.is_ot_match("slimepit", data:get_nesw(dir), ot_match_prefix) then
      -- Use SEEX for 12
      data:set_dir(dir, 12)
    end
  end
  for i = 0, 24 do
    for j = 0, 24 do
      if gapi.rng(0, 4) == 1 then map:set_ter_at(Tripoint.new(i, j, z), TerId.new("t_slime"):int_id()) end
    end
  end
  if ter:str_id():str() == "slimepit_down" then
    map:set_ter_at(Tripoint.new(gapi.rng(3, 20), gapi.rng(3, 20), z), TerId.new("t_slope_down"):int_id())
  elseif data:above():str_id():str() == "slimepit_down" then
    local r = gapi.rng(1, 4)
    if r == 1 then
      map:set_ter_at(Tripoint.new(gapi.rng(0, 2), gapi.rng(0, 2), z), TerId.new("t_slope_up"):int_id())
    elseif r == 2 then
      map:set_ter_at(Tripoint.new(gapi.rng(0, 2), gapi.rng(21, 23), z), TerId.new("t_slope_up"):int_id())
    elseif r == 3 then
      map:set_ter_at(Tripoint.new(gapi.rng(21, 23), gapi.rng(0, 2), z), TerId.new("t_slope_up"):int_id())
    else
      map:set_ter_at(Tripoint.new(gapi.rng(21, 23), gapi.rng(21, 23), z), TerId.new("t_slope_up"):int_id())
    end
  end
  map:place_spawns("GROUP_BLOB", 1, Point.new(0, 0), Point.new(23, 23), 0.15, false)
  map:place_items("sewer", 40, Point.new(0, 0), Point.new(23, 23), true)
end

return slimepit
