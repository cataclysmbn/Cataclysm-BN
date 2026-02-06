local slimepit = {}

slimepit.draw = function(data, map)
  local ot_match_prefix = 2
  local ter = data:id()
  local z = data:zlevel()
  if z == 0 then
    gapi.add_msg( "rock" )
    map:draw_fill_background( "t_dirt" )
  else
    gapi.add_msg("rock")
    map:draw_fill_background("t_rock_floor")
  end
  -- for dir=0, 3 do
    -- if not map.is_ot_match( "slimepit", data, ot_match_prefix ) then
      -- Use SEEX for 12
      -- data.set_dir( dir, 12 )
    -- end
  -- end
  for i=0, 24 do
    gapi.add_msg( "loop1" )
    for j=0, 24 do
      gapi.add_msg( "loop2" )
      if gapi.rng( 0, 4 ) == 1 then
        gapi.add_msg( "set ter" )
        map:set_ter_at( Tripoint.new( i, j, z ), TerId.new( "t_slime" ):int_id() )
      end
    end
  end
  if ter == "slimepit_down" then
    gapi.add_msg( "set ter" )
    map:set_ter_at( Tripoint.new( gapi.rng( 3, 20 ), gapi.rng( 3, 20 ), z ), TerId.new( "t_slope_down" ):int_id() )
  end
  if data:above() == "slimepit_down" then
    local r = gapi:rng( 1, 4 )
    if r == 1 then
      gapi.add_msg( "set ter" )
      map:set_ter_at( Tripoint.new( gapi.rng( 0, 2 ), gapi.rng( 0, 2 ), z ), TerId.new( "t_slope_up" ):int_id() )
    elseif r == 2 then
      gapi.add_msg( "set ter" )
      map:set_ter_at( Tripoint.new( gapi.rng( 0, 2 ), gapi.rng( 21, 23 ), z ), TerId.new( "t_slope_up" ):int_id() )
    elseif r == 3 then
      gapi.add_msg( "set ter" )
      map:set_ter_at( Tripoint.new( gapi.rng( 21, 23 ), gapi.rng( 0, 2 ), z ), TerId.new( "t_slope_up" ):int_id() )
    else
      gapi.add_msg( "set ter" )
      map:set_ter_at( Tripoint.new( gapi.rng( 21, 23 ), gapi.rng( 21, 23 ), z ), TerId.new( "t_slope_up" ):int_id() )
    end
  end
end

return slimepit
