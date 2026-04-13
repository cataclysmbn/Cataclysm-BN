local crt_lab_veh_hall = {}

shuffle = function(set)
  for i = #set, 1, -1 do
    local j = gapi.rng( 1, i )
    set[i], set[j] = set[j], set[i]
  end
  return set
end

engine = function(data, map, point)
  rooms = shuffle( {
    "crt_lab_veh_hall_engine",
    "crt_lab_veh_hall_engine_test",
    "crt_lab_veh_smallveh",
    "crt_lab_veh_smallveh"
  } )
  draw_hall_set( data, map, rooms, point )
end

gen_amenities = function(data, map, point)
  rooms = shuffle( {
    "crt_lab_veh_hall_milfood",
    "crt_lab_veh_hall_scifood",
    "crt_lab_veh_hall_exercise",
    "crt_lab_veh_hall_dodge_train",
    "crt_lab_veh_hall_library",
    "crt_lab_veh_hall_bath_good",
    "crt_lab_veh_hall_bath_bad",
  } )
  draw_hall_set( data, map, rooms, point )
end

mil_amenities = function(data, map, point)
  rooms = shuffle( {
    "crt_lab_veh_hall_milfood",
    "crt_lab_veh_hall_exercise",
    "crt_lab_veh_hall_dodge_train",
    "crt_lab_veh_hall_bath_bad"
  } )
  draw_hall_set( data, map, rooms, point )
end

all = function(data, map, point)
  rooms = shuffle( {
    "crt_lab_veh_hall_milfood",
    "crt_lab_veh_hall_scifood",
    "crt_lab_veh_hall_exercise",
    "crt_lab_veh_hall_library",
    "crt_lab_veh_hall_dodge_train",
    "crt_lab_veh_hall_bath_good",
    "crt_lab_veh_hall_bath_bad",
    "crt_lab_veh_hall_engine",
    "crt_lab_veh_hall_engine_test",
    "crt_lab_veh_smallveh"
  } )
  draw_hall_set( data, map, rooms, point )
end

local halls = { engine, all, gen_amenities, mil_amenities }

local halls_weight = { 100, 200, 50, 150 }

local total_weight = 500

draw_hall_set = function(data, map, set, pos)
  data:nest( set[1], Point.new( pos.x, pos.y ) )
  data:nest( set[2], Point.new( pos.x + 15, pos.y ) )
  map:rotate( 2 )
  
  data:nest( set[3], Point.new( pos.x, pos.y ) )
  data:nest( set[4], Point.new( pos.x + 15, pos.y ) )
  map:rotate( 2 )
end

crt_lab_veh_hall.draw = function(data, map)
  data:generate( "crt_lab_veh_hall_frame" )
  -- Generate rotates the map but we need unrotated maps for nested generation
  -- Rotation has 4 states, you rotate 3 need 1 more to get back to normal and so on
  map:rotate( 4 - data:get_rotation() )
  local weight = gapi.rng( 0, total_weight )
  local count = 0
  local hall
  for i, add in ipairs(halls_weight) do
    count = count + add
    if count >= weight then
      hall = halls[i]
      break
    end
  end
  hall( data, map, Point.new( 1, 1 ) )
  
  -- Now that all the nested is dealt with rerotate the map
  map:rotate( data:get_rotation() )
end

return crt_lab_veh_hall

