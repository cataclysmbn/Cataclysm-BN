local crt_lab_veh_main = {}

local mains = { "crt_lab_veh_house", "crt_lab_veh_training", "crt_lab_veh_wind", "crt_lab_veh_hall" }

local mains_weight = { 100, 50, 100, 250 }

local total_weight = 500

crt_lab_veh_main.draw = function(data, map)
  local weight = gapi.rng(0, total_weight)
  local count = 0
  local main
  for i, add in ipairs(mains_weight) do
    count = count + add
    if count >= weight then
      main = mains[i]
      break
    end
  end
  data:generate(main)
end

return crt_lab_veh_main
