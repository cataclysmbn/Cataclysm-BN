package.path = package.path .. ";data/json/?.lua"
package.loaded["lua.iuse.voltmeter"] = nil

local voltmeter = require("lua/iuse/voltmeter")
local pos = TripointBubMs.new(60, 60, 0)

local charge_ok, charge_info = pcall(function() return voltmeter.get_grid_charge_info(nil, nil, pos) end)
local connections_ok, connections_info = pcall(function() return voltmeter.get_grid_connections_info(nil, nil, pos) end)

test_data["charge_ok"] = charge_ok
test_data["charge_info_type"] = type(charge_info)
test_data["connections_ok"] = connections_ok
test_data["connections_info_type"] = type(connections_info)
