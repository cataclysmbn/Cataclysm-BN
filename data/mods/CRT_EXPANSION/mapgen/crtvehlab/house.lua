local crt_lab_veh_house = {}

crt_lab_veh_house.draw = function(data, map)
  data:generate( "crt_lab_veh_house_backdrop" )
  data:nest( "crt_lab_veh_house_living", Point.new( 1, 1 ) )
  data:nest( "crt_lab_veh_house_living", Point.new( 1, 16 ) )
end

research_1 = function(data, map)
  data:nest( "crt_lab_veh_house_research_living", Point.new( 1, 1 ) )
  data:nest( "crt_lab_veh_house_research_bed", Point.new( 15, 1 ) )
  data:nest( "crt_lab_veh_house_research_office", Point.new( 15, 6 ) )
end
return crt_lab_veh_house

