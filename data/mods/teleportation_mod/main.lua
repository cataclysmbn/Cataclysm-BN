gdebug.log_info("Teleporter: main")

--[[ CONFIG ]]
--

-- how many energy units a station gets per hour, default = 1, 1 charge teleports you one tile
charge_multiplier = 1
-- how much power in kj is needed for 1 teleport energy unit when charging from grid
power_charge_kj_multiplier = 1000

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

--Item id (static)

mod.storage = storage

-- Variable ids

-- Table of anchor positions
mod.anchor_list = {}
-- Table of station tables - [1] = time, [2] = charge, [3] = position(omt)
mod.station_list = {}
-- saved variables... these are separate from the active variables so I didn't mess with the saved tables constantly... also less chances of saves breaking on alt-f4, not that you'd do that ;)
storage.anchor_omt = {}
storage.station_placement_time = {}
storage.station_charge = {}
storage.station_pos = {}
-- also the default values
storage.charge_multiplier = 1
storage.power_charge_kj_multiplier = 1000
-- NOTE: due to an engine bug, the saved table keys need to be strings, not ints, that's why there's a mishmash of keys (tostring and tonumber functions used)... while this will be fixed in the foreseeable future (ty Olanti), there's no need to change this in the mod - everything should still work

-- helper function for counting tables with non-integer keys, #table does not work for those
mod.count_table = function(save_table)
  local count = 0
  for _, _ in pairs(save_table) do
    count = count + 1
  end
  return count
end

-- LOADING

mod.load_saved_anchors = function()
  if mod.count_table(storage.anchor_omt) == 0 then
    print("Data not loaded - no anchors are placed.")
  else
    for i in pairs(storage.anchor_omt) do
      mod.anchor_list[tonumber(i)] = storage.anchor_omt[i]
      print("Data found! anchor = " .. tostring(mod.anchor_list[tonumber(i)]))
    end
  end
end

mod.load_saved_station_charge = function()
  if mod.count_table(storage.station_charge) == 0 then
    print("Data not loaded - no stations are placed. (charge)")
  else
    for i in pairs(storage.station_charge) do
      mod.station_list[tonumber(i)] = {}
      mod.station_list[tonumber(i)][2] = storage.station_charge[i]
      print("Data found! station charge = " .. tostring(mod.station_list[tonumber(i)][2]))
    end
  end
end

mod.load_saved_station_placement_time = function()
  if mod.count_table(storage.station_placement_time) == 0 then
    print("Data not loaded - no stations are placed. (time)")
  else
    for i in pairs(storage.station_placement_time) do
      mod.station_list[tonumber(i)][1] = storage.station_placement_time[i]
      print("Data found! station time = " .. tostring(mod.station_list[tonumber(i)][1]))
    end
  end
end

mod.load_saved_station_pos = function()
  if mod.count_table(storage.station_pos) == 0 then
    print("Data not loaded - no stations are placed. (pos)")
  else
    for i in pairs(storage.station_pos) do
      mod.station_list[tonumber(i)][3] = storage.station_pos[i]
      print("Data found! station pos = " .. tostring(mod.station_list[tonumber(i)][3]))
    end
  end
end

mod.on_game_load_hook = function()
  gdebug.log_info("Teleporters: on_load")

  mod.load_saved_anchors()

  -- order below is important, since the data tables on load are initialized in the mod.load_saved_station_charge() function

  mod.load_saved_station_charge()
  mod.load_saved_station_placement_time()
  mod.load_saved_station_pos()
  charge_multiplier = storage.charge_multiplier
  power_charge_kj_multiplier = storage.power_charge_kj_multiplier
end

-- SAVING

mod.save_station_charge = function(num)
  storage.station_charge[tostring(num)] = mod.station_list[num][2]
  print("Saved charge to list" .. tostring(mod.station_list[num][2]))
end

mod.save_station_placement_time = function(num)
  storage.station_placement_time[tostring(num)] = mod.station_list[num][1]
  print("Saved time to list" .. tostring(mod.station_list[num][1]))
end

mod.save_station_pos = function(num)
  storage.station_pos[tostring(num)] = mod.station_list[num][3]
  print("Saved pos to list" .. tostring(mod.station_list[num][3]))
end

mod.save_anchor_omt = function()
  for i in pairs(mod.anchor_list) do
    storage.anchor_omt[tostring(i)] = mod.anchor_list[i]
    print("Saved to anchor list" .. tostring(mod.anchor_list[i]))
  end
end

mod.on_game_save_hook = function()
  gdebug.log_info("Teleporters: on_save")

  mod.save_anchor_omt()

  mod.update_station_charge()

  for i in pairs(mod.station_list) do
    mod.save_station_charge(i)
    mod.save_station_placement_time(i)
    mod.save_station_pos(i)
  end
end

-- main function

mod.add_anchor_to_list = function(pos)
  --print("adding to list")
  --print(pos)
  table.insert(mod.anchor_list, pos)
  --print(#mod.anchor_list, pos)
  mod.save_anchor_omt()
end

mod.iuse_function_anchor = function(who, item, pos)
  local a = { "teleporter_anchor_deployed" }

  local player_abs_pos = gapi.get_map():get_abs_ms(pos)
  --print(player_abs_pos)

  local player_map_pos = gapi.get_map():get_local_ms(player_abs_pos)
  --print(player_map_pos)
  local player_omt = coords.ms_to_omt(player_abs_pos)

  local no_furn = gapi.get_map():get_furn_at(player_map_pos)
  b = tostring(no_furn)
  --print(b)
  anchor_omt = tostring(player_omt)

  if b == "FurnIntId[0][f_null]" then
    --print (FurnId.new("teleporter_anchor_deployed"):int_id())
    gapi.get_map():set_furn_at(player_map_pos, FurnId.new(a[1]):int_id())

    mod.add_anchor_to_list(player_omt)

    gapi.add_msg("The teleporter anchor was placed at ")
    gapi.add_msg(anchor_omt)
  else
    gapi.add_msg("Can only be placed on a square with no existing furniture.")
    return 0
  end
  return 1
end

mod.add_station_to_list = function(pos)
  local num = #mod.station_list + 1
  --print(num)
  local turn = gapi.current_turn()
  local charge = 0

  mod.station_list[num] = {}
  mod.station_list[num][1] = turn
  mod.station_list[num][2] = charge
  mod.station_list[num][3] = pos

  mod.save_station_placement_time(num)
  mod.save_station_charge(num)
  mod.save_station_pos(num)
end

mod.iuse_function_station = function(who, item, pos)
  local a = { "teleporter_station_deployed" }
  local player_abs_pos = gapi.get_map():get_abs_ms(pos)
  --print(player_abs_pos)

  local player_map_pos = gapi.get_map():get_local_ms(player_abs_pos)
  --print(player_map_pos)

  local player_omt = coords.ms_to_omt(player_abs_pos)
  --print(player_omt)

  local no_furn = gapi.get_map():get_furn_at(player_map_pos)
  b = tostring(no_furn)
  --print(b)
  station_omt = tostring(player_omt)

  if b == "FurnIntId[0][f_null]" then
    gapi.get_map():set_furn_at(player_map_pos, FurnId.new(a[1]):int_id())
    mod.add_station_to_list(player_omt)
  else
    gapi.add_msg("Can only be placed on a square with no existing furniture.")
    return 0
  end

  return 1
  --end
end

mod.update_station_charge = function()
  -- go through station list, compare times, change charge depending on time passed
  if mod.count_table(mod.station_list) > 0 then
    for i in pairs(mod.station_list) do
      local station_time = mod.station_list[i][1]
      local current_time = gapi.current_turn()
      local time_difference_TimeDuration = TimePoint.__sub(current_time, station_time)
      local time_difference = TimeDuration.to_seconds(time_difference_TimeDuration)
      --print (time_difference)
      if time_difference > 60 then
        local added_charge = time_difference * charge_multiplier / 3600
        mod.station_list[i][2] = mod.station_list[i][2] + added_charge
        mod.station_list[i][1] = current_time
      end
    end
  else
    print("No stations in network!")
  end
end

mod.do_station_charge = function(choose, grid, power_available, chosen_station_list)
  local available_units = (tonumber(power_available) / power_charge_kj_multiplier)
  --print(available_units)
  if available_units < 1 then
    gapi.add_msg(locale.gettext("Power too low for charging."))
    return 0
  end
  local uic = UiList.new()
  uic:title(locale.gettext("How much power do you want to charge the station with?"))
  for i = 1, available_units do
    uic:add(i, tostring(i))
  end
  local pick = uic:query()
  if pick < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  else
    --print(mod.station_list[chosen_station_list[choose]][2])
    mod.station_list[chosen_station_list[choose]][2] = mod.station_list[chosen_station_list[choose]][2] + pick
    --print(mod.station_list[chosen_station_list[choose]][2])
    grid:mod_resource(-(pick * power_charge_kj_multiplier), true)
    gapi.add_msg(locale.gettext("The station was charged."))
    return 1
  end
end

mod.charge_stations_from_grid = function(pos)
  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local abs_omt = coords.ms_to_omt(abs_pos)
  local grid = gapi.get_distribution_grid_tracker():get_grid_at_abs_ms(abs_pos)
  local power_available = grid:get_resource(true)
  local chosen_station_list = {}

  if power_available == 0 then
    gapi.add_msg(locale.gettext("No grid or no power in grid"))
    return 0
  end
  if power_available > 0 then
    local ui_charge = UiList.new()
    ui_charge:title(locale.gettext("Charge which station?"))
    local j = 1
    for i in pairs(mod.station_list) do
      if mod.station_list[i][3] == abs_omt then
        local a = tostring(j) .. "Station with charge " .. tostring(mod.station_list[i][2])
        ui_charge:add(j, a)
        chosen_station_list[j] = i
        j = j + 1
      end
      if i == mod.count_table(mod.station_list) then
        if j == 1 then
          gapi.add_msg(locale.gettext("No stations found on your tile."))
          return 0
        end
      end
    end
    local choose = ui_charge:query()
    -- Canceled by player
    if choose < 1 then
      gapi.add_msg(locale.gettext("Nevermind."))
      return 0
    end
    return mod.do_station_charge(choose, grid, power_available, chosen_station_list)
  end
end

mod.teleport_to_target = function(who, anchor, distance, teleporter_list_key, picked_teleporter)
  local a = teleporter_list_key[picked_teleporter]
  print(a)
  local removed_charge = -distance
  mod.station_list[a][2] = mod.station_list[a][2] + removed_charge

  gapi.add_msg("Teleporting Player to " .. tostring(anchor))
  gapi.add_msg(
    "Station at: "
      .. tostring(mod.station_list[teleporter_list_key[picked_teleporter]][3])
      .. "now has "
      .. mod.station_list[teleporter_list_key[picked_teleporter]][2]
      .. " units of charge left"
  )
  --need exposed debug teleport for this to function
  gapi.place_player_overmap_at(anchor)
  return 1
end

mod.pick_teleporter = function(who, eidx, pos)
  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local abs_omt = coords.ms_to_omt(abs_pos)
  local anchor = mod.anchor_list[eidx]
  local distance = coords.rl_dist(abs_omt, anchor)
  local teleporter_list_key = {}
  local ui_pick_teleporter = UiList.new()
  ui_pick_teleporter:title(locale.gettext("Use which station?"))

  local j = 1
  for i in pairs(mod.station_list) do
    if mod.station_list[i][2] > distance then
      local a = "Teleporter at "
        .. tostring(mod.station_list[i][3])
        .. "with charge "
        .. tostring(mod.station_list[i][2])
      ui_pick_teleporter:add(j, a)
      teleporter_list_key[j] = i

      j = j + 1
    end
    if i == mod.count_table(mod.station_list) then
      if j == 1 then
        gapi.add_msg(
          locale.gettext(
            "No stations with enough charge. The required charge is " .. tostring(distance) .. " energy units."
          )
        )
        return 0
      end
    end
  end
  local picked_teleporter = ui_pick_teleporter:query()
  if picked_teleporter < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  else
    for i in pairs(teleporter_list_key) do
      print(tostring(teleporter_list_key[i]))
    end

    return mod.teleport_to_target(who, anchor, distance, teleporter_list_key, picked_teleporter)
  end
end

mod.pick_teleport_destination = function(who, pos)
  local pos = pos
  local ui_teleport = UiList.new()
  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local abs_omt = coords.ms_to_omt(abs_pos)
  ui_teleport:title(locale.gettext("Select teleportation target"))

  for i in pairs(mod.anchor_list) do
    local anchor = mod.anchor_list[i]
    local distance = coords.rl_dist(abs_omt, anchor)
    local a = "Coordinates: " .. tostring(mod.anchor_list[i]) .. "  Distance: " .. tostring(distance)
    ui_teleport:add(i, a)
  end

  local eidx = ui_teleport:query()
  if eidx < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  end

  return mod.pick_teleporter(who, eidx, pos)
end

mod.get_anchor_distance = function(pos, i)
  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local abs_omt = coords.ms_to_omt(abs_pos)
  local anchor = mod.anchor_list[i]
  local distance = coords.rl_dist(abs_omt, anchor)
  return distance
end

mod.create_network_message = function(pos)
  b = [[Stations:
]]
  for i in pairs(mod.station_list) do
    local a = "Station: " .. tostring(i)
    a = a .. " Last updated: " .. tostring(mod.station_list[i][1])
    a = a .. " Charge: " .. tostring(mod.station_list[i][2])
    a = a .. " Position: " .. tostring(mod.station_list[i][3])
    a = a .. [[
]]
    print(a)
    b = b .. a
  end
  b = b .. [[------------------------------

Anchors:
]]
  for i in pairs(mod.anchor_list) do
    local a = ""
    a = a .. " Position: " .. tostring(mod.anchor_list[i])
    a = a .. " Distance: " .. mod.get_anchor_distance(pos, i)
    a = a .. [[
]]
    print(a)
    b = b .. a
  end
  return b
end

mod.network_info = function(pos)
  -- spam out all stations and anchors

  local ui_network_info = QueryPopup.new()
  local message_text = mod.create_network_message(pos)
  ui_network_info:message(message_text)
  ui_network_info:message_color(Color.c_white)
  ui_network_info:allow_any_key(true)
  ui_network_info:query()
  return 1
end

mod.show_readme_info = function()
  local ui_mod_info = QueryPopup.new()
  local modinfo = [[
Teleporters - the future, today!

Teleporter stations provide the teleportation service,
while anchors function as targets.
This controller is used to teleport to an anchor from
anywhere, provided you have enough power in a station.

The stations fill passively, the current rate is
]] .. charge_multiplier .. [[ units per hour.

You can also fill the stations from the grid,
using this controller, by standing on the same overmap
tile as the station you wish to fill, and drawing
from the grid on that specific overmap. If multiple
stations are present, you can pick which one to fill.

The current rate is ]] .. power_charge_kj_multiplier .. [[kJ per 1 unit
of charge. Both rates can be changed in the config. ]]
  ui_mod_info:message(modinfo)
  ui_mod_info:message_color(Color.c_white)
  ui_mod_info:allow_any_key(true)
  ui_mod_info:query()
  return 0
end

mod.config_set_variables = function()
  local ui_pick_vars = UiList.new()
  ui_pick_vars:title(locale.gettext("Change which variable?"))
  ui_pick_vars:add(1, locale.gettext("Passive charge per hour"))
  ui_pick_vars:add(2, locale.gettext("kJ to charge ratio"))
  local pick_chargekjmult_action = ui_pick_vars:query()
  if pick_chargekjmult_action < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  elseif pick_chargekjmult_action == 1 then
    return mod.setvar_charge_multiplier()
  elseif pick_chargekjmult_action == 2 then
    return mod.setvar_power_charge_kj_multiplier()
  end
end

mod.setvar_charge_multiplier = function()
  local ui_pick_chargemult = UiList.new()
  ui_pick_chargemult:title(locale.gettext("Set passive charge to:"))
  ui_pick_chargemult:add(1, locale.gettext("Default: ") .. tostring(1))
  ui_pick_chargemult:add(2, locale.gettext("Half: ") .. tostring(storage.charge_multiplier * 0.5))
  ui_pick_chargemult:add(3, locale.gettext("Double: ") .. tostring(storage.charge_multiplier * 2))
  local pick_chargemult_action = ui_pick_chargemult:query()
  if pick_chargemult_action < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  elseif pick_chargemult_action == 1 then
    storage.charge_multiplier = 1
    charge_multiplier = storage.charge_multiplier
    return 0
  elseif pick_chargemult_action == 2 then
    storage.charge_multiplier = storage.charge_multiplier * 0.5
    charge_multiplier = storage.charge_multiplier
    return 0
  elseif pick_chargemult_action == 3 then
    storage.charge_multiplier = storage.charge_multiplier * 2
    charge_multiplier = storage.charge_multiplier
    return 0
  end
end

mod.setvar_power_charge_kj_multiplier = function()
  local ui_pick_kjmult = UiList.new()
  ui_pick_kjmult:title(locale.gettext("Set kJ to charge to:"))
  ui_pick_kjmult:add(1, locale.gettext("Default: ") .. tostring(1000))
  ui_pick_kjmult:add(2, locale.gettext("Half: ") .. tostring(storage.power_charge_kj_multiplier * 0.5))
  ui_pick_kjmult:add(3, locale.gettext("Double: ") .. tostring(storage.power_charge_kj_multiplier * 2))
  local pick_kjmult_action = ui_pick_kjmult:query()
  if pick_kjmult_action < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  elseif pick_kjmult_action == 1 then
    storage.power_charge_kj_multiplier = 1000
    power_charge_kj_multiplier = storage.power_charge_kj_multiplier
    return 0
  elseif pick_kjmult_action == 2 then
    storage.power_charge_kj_multiplier = storage.power_charge_kj_multiplier * 0.5
    power_charge_kj_multiplier = storage.power_charge_kj_multiplier
    return 0
  elseif pick_kjmult_action == 3 then
    storage.power_charge_kj_multiplier = storage.power_charge_kj_multiplier * 2
    power_charge_kj_multiplier = storage.power_charge_kj_multiplier
    return 0
  end
end

mod.scan_omt_remove_furn = function(abs_omt, ftype)
  local furniture_name = ""
  local spawn_item = ""
  if ftype == "station" then
    furniture_name = tostring(FurnId.new("teleporter_station_deployed"):int_id())
    spawn_item = "teleporter_station_undeployed"
    --print(furniture_name)
  elseif ftype == "anchor" then
    furniture_name = tostring(FurnId.new("teleporter_anchor_deployed"):int_id())
    spawn_item = "teleporter_anchor_undeployed"
    --print(furniture_name)
  end
  local x, y = 0, 0
  local scantile = Point.new(x, y)
  local scansquare = coords.omt_to_ms(abs_omt, scantile)
  local mapsize = const.OMT_MS_SIZE
  local furn_at_tile = ""

  for i = 1, const.OMT_MS_SIZE do
    for j = 1, const.OMT_MS_SIZE do
      x = i - 1
      y = j - 1
      scantile = Point.new(x, y)
      scansquare = coords.omt_to_ms(abs_omt, scantile)
      local xyz = gapi.get_map():get_local_ms(scansquare)
      furn_at_tile = tostring(gapi.get_map():get_furn_at(xyz))
      --print(tostring(xyz) .. tostring(furn_at_tile))
      --gapi.get_map():set_furn_at( xyz, FurnId.new("f_fridge"):int_id() )
      if furn_at_tile == furniture_name then
        gapi.get_map():set_furn_at(xyz, FurnId.new("f_null"):int_id())
        gapi.get_map():create_item_at(xyz, ItypeId.new(spawn_item), 1)
        return 0
      elseif i == const.OMT_MS_SIZE and j == const.OMT_MS_SIZE then
        gapi.add_msg(locale.gettext("Appropriate furniture not found. Something went terribly wrong!"))
        return 0
      end
    end
  end
end

mod.update_station_table = function()
  for j in pairs(mod.station_list) do
    if j ~= 1 then
      if mod.station_list[j - 1] == nil then
        mod.station_list[j - 1] = {}
        for k in pairs(mod.station_list[j]) do
          mod.station_list[j - 1][k] = mod.station_list[j][k]
        end
        mod.station_list[j] = nil
      end
    end
  end
end

mod.remove_placed_furniture = function(pos)
  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local abs_omt = coords.ms_to_omt(abs_pos)
  local ui_remove_furn = UiList.new()
  ui_remove_furn:title(locale.gettext("Remove station or anchor?"))
  ui_remove_furn:add(1, locale.gettext("Station"))
  ui_remove_furn:add(2, locale.gettext("Anchor"))
  local pick_removefurn_action = ui_remove_furn:query()
  if pick_removefurn_action < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  elseif pick_removefurn_action == 1 then
    for i in pairs(mod.station_list) do
      if mod.station_list[i][3] == abs_omt then
        mod.station_list[i] = nil
        mod.update_station_table()
        mod.scan_omt_remove_furn(abs_omt, "station")
        return 0
      elseif i == mod.count_table(mod.station_list) then
        gapi.add_msg(locale.gettext("No stations found on this tile."))
        return 0
      end
    end
  elseif pick_removefurn_action == 2 then
    for i in pairs(mod.anchor_list) do
      if mod.anchor_list[i] == abs_omt then
        mod.anchor_list[i] = nil
        for j in pairs(mod.anchor_list) do
          if j ~= 1 then
            if mod.anchor_list[j - 1] == nil then
              mod.anchor_list[j - 1] = mod.anchor_list[j]
              mod.anchor_list[j] = nil
            end
          end
        end
        mod.scan_omt_remove_furn(abs_omt, "anchor")
        return 0
      elseif i == mod.count_table(mod.anchor_list) then
        gapi.add_msg(locale.gettext("No anchors found on this tile."))
        return 0
      end
    end
  end
end

mod.iuse_function_controller = function(who, item, pos)
  --print("used teleporter remote")
  mod.update_station_charge()

  local ui_action = UiList.new()
  ui_action:title(locale.gettext("Do what?"))
  ui_action:add(1, locale.gettext("Teleport"))
  ui_action:add(2, locale.gettext("Charge stations"))
  ui_action:add(3, locale.gettext("Check network status"))
  ui_action:add(4, locale.gettext("Remove anchors and stations"))
  ui_action:add(5, locale.gettext("Info"))
  ui_action:add(6, locale.gettext("Config"))
  local pick_action = ui_action:query()
  if pick_action < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  elseif pick_action == 1 then
    return mod.pick_teleport_destination(who, pos)
  elseif pick_action == 2 then
    return mod.charge_stations_from_grid(pos)
  elseif pick_action == 3 then
    return mod.network_info(pos)
  elseif pick_action == 4 then
    return mod.remove_placed_furniture(pos)
  elseif pick_action == 5 then
    return mod.show_readme_info()
  elseif pick_action == 6 then
    return mod.config_set_variables()
  end
end
