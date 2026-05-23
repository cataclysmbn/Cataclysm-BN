local lab = {}

local ot_match_type = 1
local ot_match_prefix = 2
local ot_match_contains = 3

local south_edge = 23
local east_edge = 23
local see = 12

local t_floor = TerId.new("t_floor"):int_id()
local t_thconc_floor = TerId.new("t_thconc_floor"):int_id()
local t_thconc_floor_olight = TerId.new("t_thconc_floor_olight"):int_id()
local t_strconc_floor = TerId.new("t_strconc_floor"):int_id()
local t_thconc_olight_floor = TerId.new("t_strconc_floor"):int_id()
local t_sewage = TerId.new("t_sewage"):int_id()
local t_bars = TerId.new("t_bars"):int_id()
local t_door_metal_locked = TerId.new("t_door_metal_locked"):int_id()
local t_door_metal_c = TerId.new("t_door_metal_c"):int_id()
local t_concrete_wall = TerId.new("t_concrete_wall"):int_id()
local t_reinforced_glass = TerId.new("t_reinforced_glass"):int_id()
local t_stairs_up = TerId.new("t_stairs_up"):int_id()
local t_stairs_down = TerId.new("t_stairs_down"):int_id()
local t_rock_floor = TerId.new("t_rock_floor"):int_id()
local t_slime = TerId.new("t_slime"):int_id()
local t_water_sh = TerId.new("t_water_sh"):int_id()
local t_sewage = TerId.new("t_sewage"):int_id()
local t_fungus_wall = TerId.new("t_fungus_wall"):int_id()
local t_fungus_floor_in = TerId.new("t_fungus_floor_in"):int_id()
local t_card_science = TerId.new("t_card_science"):int_id()

local f_null = FurnId.new("f_null"):int_id()
local f_rubble_rock = FurnId.new("f_rubble_rock"):int_id()
local f_fungal_clump = FurnId.new("f_fungal_clump"):int_id()

local tr_null = TrapId.new("tr_null"):int_id()

local fd_gas_vent = FieldTypeId.new("fd_gas_vent"):int_id()
local fd_smoke_vent = FieldTypeId.new("fd_smoke_vent"):int_id()

draw_entry_room = function(data, map)
  for i = 0, 24 do
    for j = 0, 24 do
      local pt = TripointBubMs.new(i, j, data:zlevel())
      if i == 0 or i == 1 or i == 22 or i == 23 or
         j == 0 or j == 1 or j == 22 or j == 23 or
         ( j > 1 and j < 22 and ( i == 10 or i == 13 ) ) then
        map:set_ter_at(pt, t_concrete_wall)
      else
        map:set_ter_at(pt, t_thconc_floor)
      end
    end
  end
  map:set_ter_at(TripointBubMs.new(11, 0, data:zlevel()), t_door_metal_locked)
  map:set_ter_at(TripointBubMs.new(12, 0, data:zlevel()), t_door_metal_locked)
  map:set_ter_at(TripointBubMs.new(11, 1, data:zlevel()), t_floor)
  map:set_ter_at(TripointBubMs.new(12, 1, data:zlevel()), t_floor)
  map:set_ter_at(TripointBubMs.new(10, 0, data:zlevel()), t_card_science)
  map:set_ter_at(TripointBubMs.new(10, 12, data:zlevel()), t_door_metal_c)
  map:set_ter_at(TripointBubMs.new(13, 12, data:zlevel()), t_door_metal_c)
  map:set_ter_at(TripointBubMs.new(10, 11, data:zlevel()), t_door_metal_c)
  map:set_ter_at(TripointBubMs.new(13, 11, data:zlevel()), t_door_metal_c)
  map:set_ter_at(TripointBubMs.new(11, 21, data:zlevel()), t_stairs_down)
  map:set_ter_at(TripointBubMs.new(12, 21, data:zlevel()), t_stairs_down)
  data:nest( "lab_room_7x7", PointRelMs.new( 2, 2 ) )
  data:nest( "lab_room_7x7", PointRelMs.new( 2, 14 ) )
  data:nest( "lab_room_7x7", PointRelMs.new( 14, 2 ) )
  data:nest( "lab_room_7x7", PointRelMs.new( 14, 14 ) )
  if map.is_ot_match( "road", data:east(), ot_match_type ) then
    map:rotate(1)
  elseif map.is_ot_match( "road", data:south(), ot_match_type ) then
    map:rotate(2)
  elseif map.is_ot_match( "road", data:west(), ot_match_type ) then
    map:rotate(3)
  end
end

draw_sewer_room = function(data, map, walls)
  for i = 0, 24 do
    for j = 0, 24 do
      local pt = TripointBubMs.new( i, j, data:zlevel() )
      map:set_ter_at(pt, t_thconc_floor)
      -- If there is a sewer nearby, down the center make a 5 tile wide sewage line
      if ( ( walls.left or walls.right ) and j > see - 3 and j < see + 2 ) or
         ( ( walls.top or walls.bottom ) and i > see - 3 and i < see + 2 ) then
        map:set_ter_at(pt, t_sewage)
      end
      -- Build the edge walls
      if i == 0 or i == 23 then
        if map:get_ter_at(pt) == t_sewage then
          map:set_ter_at(pt, t_bars)
        elseif j == see - 1 or j == see then
          map:set_ter_at(pt, t_door_metal_c)
        else
          map:set_ter_at(pt, t_concrete_wall)
        end
      end
      if j == 0 or j == 24 then
        if map:get_ter_at(pt) == t_sewage then
          map:set_ter_at(pt, t_bars)
        elseif i == see - 1 or i == see then
          map:set_ter_at(pt, t_door_metal_c)
        else
          map:set_ter_at(pt, t_concrete_wall)
        end
      end
    end
  end
end 

shuffle = function(set)
  for i = #set, 1, -1 do
    local j = gapi.rng(1, i)
    set[i], set[j] = set[j], set[i]
  end
  return set
end

-- This is the primary insert stair function, I wish there was the ability to read the square above
-- But it appears that it just registers everything as t_thconc_floor
insert_stairs = function(map, up_id, down_id, zlevel, from_above)
  local valid_points = {}
  for i = 0, 23 do
    for j = 0, 23 do
      local pt = TripointBubMs.new(i, j, zlevel)
      local pt_above = TripointBubMs.new(i, j, zlevel + 1)
      local pt_below = TripointBubMs.new(i, j, zlevel - 1)
      -- If we somehow see a stair link take the stupid stair link
      if ( from_above and map:get_ter_at(pt_above) == t_stairs_down ) or
         ( not from_above and map:get_ter_at(pt_below) == t_stairs_up ) then
        if map:get_ter_at(pt) == t_thconc_floor and map:get_furn_at(pt) == f_null and map:get_trap_at(pt) == tr_null then
          valid_points = {PointBubMs.new(i, j)}
          i = 24
          break
        end
      end
      if map:get_ter_at(pt) == t_thconc_floor and map:get_furn_at(pt) == f_null and map:get_trap_at(pt) == tr_null then
        table.insert(valid_points, PointBubMs.new(i, j))
      end
    end
  end
  if #valid_points > 0 then
    local final_point = valid_points[gapi.rng(1, #valid_points)]
    if( from_above ) then
      map:set_ter_at(TripointBubMs.new(final_point, zlevel), up_id)
    else
      map:set_ter_at(TripointBubMs.new(final_point, zlevel), down_id)
    end
  else
    if( from_above ) then
      insert_stairs_single(map, up_id, zlevel)
    else
      insert_stairs_single(map, down_id, zlevel)
    end
  end
end

draw_lights = function(data, map)
  local light_chance = 0
  if map.is_ot_match( "central_lab", data:id(), ot_match_prefix ) then
    light_chance = 1
  else
    light_chance = math.floor( gapi.rng(1, 12) ^ 1.6 )
  end
  if light_chance > 0 then
    for i = 0, 23 do
      for j = 0, 23 do
        if not( i * j % 2 == 0 or i + j % 4 == 0 ) and gapi.rng( 0, light_chance ) == 1 then
          if map:get_ter_at( TripointBubMs.new(i, j, data:zlevel())) == t_thconc_floor or
             map:get_ter_at( TripointBubMs.new(i, j, data:zlevel())) == t_strconc_floor then
            map:set_ter_at( TripointBubMs.new(i, j, data:zlevel()), t_thconc_floor_olight)
          end
        end
      end
    end
  end
end
    
draw_walls = function(data, map, walls)
  local interior_wall_ter = t_concrete_wall
  local exterior_wall_ter = t_concrete_wall
  if map.is_ot_match( "tower_lab", data:id(), ot_match_prefix ) then
    exterior_wall_ter = t_reinforced_glass
  end
  for i = 0, 23 do
    local pt = TripointBubMs.new( i, 0, data:zlevel() )
    if walls.top == 0 then
      map:set_ter_at( pt, exterior_wall_ter )
      map:set_furn_at( pt, f_null )
      map:clear_items_at( pt )
    end
    pt = TripointBubMs.new( 0, i, data:zlevel() )
    if walls.left == 0 then
      map:set_ter_at( pt, exterior_wall_ter )
      map:set_furn_at( pt, f_null )
      map:clear_items_at( pt )
    end
    pt = TripointBubMs.new( i, 23, data:zlevel() )
    if walls.bottom == 2 then
      if i == 11 or i == 12 then
        map:set_ter_at( pt, t_door_metal_c )
      else
        map:set_ter_at( pt, interior_wall_ter )
      end
    else
      map:set_ter_at( pt, exterior_wall_ter )
    end
    map:set_furn_at( pt, f_null )
    map:clear_items_at( pt )
    pt = TripointBubMs.new( 23, i, data:zlevel() )
    if walls.right == 2 then
      if i == 11 or i == 12 then
        map:set_ter_at( pt, t_door_metal_c )
      else
        map:set_ter_at( pt, interior_wall_ter )
      end
    else
      map:set_ter_at( pt, exterior_wall_ter )
    end
    map:set_furn_at( pt, f_null )
    map:clear_items_at( pt )
  end
end
  
draw_normal_room = function(data, map)
  walls = {
    top=0,
    bottom=0,
    left=0,
    right=0
  }
  boarders = 4
  if map.is_ot_match( "lab", data:north(), ot_match_contains ) then
    walls.top = 2
    boarders = boarders - 1
  end
  if map.is_ot_match( "lab", data:east(), ot_match_contains ) then
    walls.right = 2
    boarders = boarders - 1
  end
  if map.is_ot_match( "lab", data:south(), ot_match_contains ) then
    walls.bottom = 2
    boarders = boarders - 1
  end
  if map.is_ot_match( "lab", data:west(), ot_match_contains ) then
    walls.left = 2
    boarders = boarders - 1
  end
  if map.is_ot_match( "finale", data:id(), ot_match_contains ) then
    data:generate("lab_finale_1level")
  elseif boarders == 3 then
    data:generate("lab_1side")
    if walls.right == 2 then
      map:rotate( 1 )
    elseif walls.bottom == 2 then
      map:rotate( 2 )
    elseif walls.left == 2 then
      map:rotate( 3 )
    end
  else
    data:generate("lab_4side")
  end
  -- Build forth the walls
  draw_walls(data, map, walls)
  -- Ideally this should link stairs
  -- The moment tinymaps can see above and below this will link stairs
  -- We all know this will never happen
  if map.is_ot_match( "stairs", data:above(), ot_match_contains ) then
    insert_stairs(map, t_stairs_up, t_stairs_down, data:zlevel(), true)
  elseif map.is_ot_match( "stairs", data:id(), ot_match_contains ) then
    insert_stairs(map, t_stairs_up, t_stairs_down, data:zlevel(), false)
  end
  draw_lights(data, map)
end

draw_slimepit_room = function(data, map)
  top_wall = 0
  if( map.is_ot_match( "slimepit", data:north(), ot_match_type ) ) then
    top_wall = 12
  end
  right_wall = 24
  if( map.is_ot_match( "slimepit", data:west(), ot_match_type ) ) then
    right_wall = 12
  end
  left_wall = 0
  if( map.is_ot_match( "slimepit", data:east(), ot_match_type ) ) then
    left_wall = 12
  end
  bottom_wall = 24
  if( map.is_ot_match( "slimepit", data:south(), ot_match_type ) ) then
    bottom_wall = 12
  end
  for i = 0, 23 do
    for j = 0, 23 do
      if ( j < top_wall or j > bottom_wall  or i > right_wall or i < left_wall ) then
        local pt = TripointBubMs.new( i, j, data:zlevel() )
        if gapi.rng(1, 5) == 1 then
          -- This pretty closely mimics make_rubble for the purposes
          map:set_ter_at( pt, t_slime )
          map:set_furn_at( pt, f_rubble_rock )
        elseif gapi.rng(1, 5) ~= 1 then
          map:set_ter_at( pt, t_slime )
        end
      end
    end
  end
end

draw_ants_room = function(data, map)
  for i = 0, 23 do
    for j = 0, 23 do
      -- Diamond area that covers 2 spaces on edge
      if i + j > 10 and i + j < 36 and math.abs( i - j ) < 13 then
        local pt = TripointBubMs.new( i, j, data:zlevel() )
        if map:has_ter_flag_at( "DOOR", pt ) or map:has_ter_flag_at( "WALL", pt ) then
          -- If edge
          -- Or 25% of the time
          -- Or 66% of the time in center
          if i == 0 or j == 0 or i == 23 or j == 24 or gapi.rng(1, 4) == 1 or
             ( gapi.rng(1, 3) ~= 3 and ( i == 11 or i == 12 or j == 11 or j == 12 ) ) then
            map:make_rubble( pt, f_null, t_rock_floor )
          end
        elseif gapi.rng(1, 20) == 1 and not map:has_ter_flag_at( "GOES_DOWN", pt ) and not map:has_ter_flag_at( "GOES_UP", pt ) then
          map:destroy( pt )
          -- Can set it to other terrains, we want rock floor
          map:set_ter_at( pt, t_rock_floor )
        end
      end
    end
  end
end
  
  
lab.nearby_overmap_adjustments = {
  {fuzzy="slimepit", func=draw_slimepit_room},
  {fuzzy="ants", func=draw_ants_room}
}

draw_fullflood_room = function(data, map)
  if map.is_ot_match( "stairs", data:id(), ot_match_contains ) or
     map.is_ot_match( "ice", data:id(), ot_match_contains ) then
    return
  end
  fluid = t_water_sh
  if gapi.rng(1, 3) == 1 then
    fluid = t_sewage
  end
  for i = 0, 23 do
    for j = 0, 23 do
      local pt = TripointBubMs.new( i, j, data:zlevel())
      if gapi.rng(1, 10) ~= 1 then
        if map:get_ter_at(pt) == t_thconc_floor or map:get_ter_at(pt) == t_strconc_floor or map:get_ter_at(pt) == t_thconc_floor_olight then
          map:set_ter_at(pt, fluid)
        end
      elseif map:has_ter_flag_at( "DOOR", pt ) and gapi.rng(1, 3) ~= 1 then
        map:make_rubble(pt, f_null, fluid)
      end
    end
  end
end

draw_partflood_room = function(data, map)
  if map.is_ot_match( "stairs", data:id(), ot_match_contains ) or
     map.is_ot_match( "ice", data:id(), ot_match_contains ) then
    return
  end
  fluid = t_water_sh
  if gapi.rng(1, 3) == 1 then
    fluid = t_sewage
  end
  for i = 0, 23 do
    for j = 0, 23 do
      local pt = TripointBubMs.new( i, j, data:zlevel())
      if gapi.rng(1, 5) == 1 then
        if map:get_ter_at(pt) == t_thconc_floor or map:get_ter_at(pt) == t_strconc_floor or map:get_ter_at(pt) == t_thconc_floor_olight then
          map:set_ter_at(pt, fluid)
        end
      elseif map:has_ter_flag_at( "DOOR", pt ) and gapi.rng(1, 3) == 1 then
        map:make_rubble(pt, f_null, fluid)
      end
    end
  end
end

draw_gasleak_room = function(data, map)
  field = fd_smoke_vent
  if gapi.rng(1, 3) == 1 then
    fluid = fd_gas_vent
  end
  for i = 0, 23 do
    for j = 0, 23 do
      local pt = TripointBubMs.new( i, j, data:zlevel())
      if gapi.rng(1, 200) == 1 then
        if map:get_ter_at(pt) == t_thconc_floor or map:get_ter_at(pt) == t_strconc_floor then
          map:add_field_at( pt, field, 1, TimeDuration.from_turns(0) )
        end
      end
    end
  end
end

draw_fungal_room = function(data, map)
  for i = 0, 23 do
    for j = 0, 23 do
      local pt = TripointBubMs.new( i, j, data:zlevel())
      if gapi.rng(1, 5) ~= 1 then
        if map:has_flag_at("FLAT", pt) then
          map:set_ter_at(pt, t_fungus_floor_in)
          if map:has_furn_flag_at("ORGANIC", pt) then
            map:set_furn_at(pt, f_fungal_clump)
          end
        elseif map:has_ter_flag_at("DOOR", pt) then
          map:set_ter_at(pt, t_fungus_floor_in)
        elseif map:has_ter_flag_at("WALL", pt) then
          map:set_ter_at(pt, t_fungus_wall)
        end
      end
    end
  end
end

lab.special_effects = {
  {weight=1, func=draw_fullflood_room},
  {weight=1, func=draw_partflood_room},
  {weight=2, func=draw_gasleak_room},
  {weight=1, func=draw_fungal_room},
}

lab.draw = function(data, map)
  if( data:zlevel() == 0 ) then
    draw_entry_room(data, map)
    return
  end
  sewer_walls = {
    top=false,
    bottom=false,
    left=false,
    right=false
  }
  -- NOTE: May need to reset wall variables here later on
  if map.is_ot_match( "sewer", data:north(), ot_match_type ) then
    sewer_walls.top = true
  end
  if map.is_ot_match( "sewer", data:east(), ot_match_type ) then
    sewer_walls.left = true
  end
  if map.is_ot_match( "sewer", data:south(), ot_match_type ) then
    sewer_walls.bottom = true
  end
  if map.is_ot_match( "sewer", data:west(), ot_match_type ) then
    sewer_walls.right = true
  end

  -- If there are any sewers here
  if sewer_walls.top or sewer_walls.right or sewer_walls.bottom or sewer_walls.left then
    draw_sewer_room(data, map, sewer_walls)
  else
    draw_normal_room(data, map)
  end
  for _, drawing in ipairs(lab.nearby_overmap_adjustments) do
    if map.is_ot_match( drawing.fuzzy, data:north(), ot_match_contains ) or
       map.is_ot_match( drawing.fuzzy, data:south(), ot_match_contains ) or
       map.is_ot_match( drawing.fuzzy, data:east(), ot_match_contains ) or
       map.is_ot_match( drawing.fuzzy, data:west(), ot_match_contains ) then
      drawing.func(data, map)
    end
  end
  if gapi.rng( 1, 10 ) == 1 then
    local total_weight = 0
    for _, special in ipairs(lab.special_effects) do
      total_weight = total_weight + special.weight
    end
    local chosen_weight = gapi.rng(0, total_weight - 1)
    local current_weight = 0

    for _, special in ipairs(lab.special_effects) do
      current_weight = current_weight + special.weight
      if chosen_weight < current_weight then
        special.func(data, map)
        break
      end
    end
  end
end

lab.ice_draw = function(data, map)
  local temperature
  if data:zlevel() == 0 then
    temperature = -20
  else
    temperature = math.floor( -20 * math.log( -1 * data:zlevel() ) - 45 )
  end
  map:set_temperature( TripointBubMs.new(0, 0, data:zlevel()), temperature)
  map:set_temperature( TripointBubMs.new(0, 12, data:zlevel()), temperature)
  map:set_temperature( TripointBubMs.new(12, 0, data:zlevel()), temperature)
  map:set_temperature( TripointBubMs.new(12, 12, data:zlevel()), temperature)
  lab.draw(data, map)
end

return lab
