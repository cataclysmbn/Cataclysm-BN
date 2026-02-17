gdebug.log_info("Ressurection: main")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

--Item id (static)

mod.storage = storage

-- Variable ids

-- Table of anchor positions
mod.anchor_list = {}
-- saved variables... these are separate from the active variables so I didn't mess with the saved tables constantly... also less chances of saves breaking on alt-f4, not that you'd do that ;)
storage.anchor_omt = {}
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

mod.on_game_load_hook = function()
  gdebug.log_info("Resurrection: on_load")
  mod.load_saved_anchors()
end

-- SAVING

mod.save_anchor_omt = function()
  for i in pairs(mod.anchor_list) do
    storage.anchor_omt[tostring(i)] = mod.anchor_list[i]
    print("Saved to anchor list" .. tostring(mod.anchor_list[i]))
  end
end

mod.on_game_save_hook = function()
  gdebug.log_info("Resurrection: on_save")
  mod.save_anchor_omt()
end

mod.on_character_death_hook = function()
  local who = gapi.get_avatar()
  local anchor_pos = mod.pick_teleport_destination(who)
  if anchor_pos ~= nil then
    local player_abs = gapi.get_map():get_abs_ms(who:get_pos_ms())
    local distance = math.abs(coords.rl_dist(player_abs, anchor_pos))
    who:drop_inv(math.ceil(distance / 50))
    gapi.add_msg("Respawning Player at " .. tostring(anchor_pos))

    -- Convert abs_ms to abs_omt
    local omt_pos = coords.ms_to_omt(anchor_pos)
    gapi.place_player_overmap_at(omt_pos)

    -- Convert abs_ms to local_ms
    local local_pos = gapi.get_map():get_local_ms(anchor_pos)
    gapi.place_player_local_at(local_pos)

    who:set_all_parts_hp_cur(10)

    --gapi.get_map():set_furn_at(local_pos, FurnId.new("f_null"))
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
  local f_anchor = FurnId.new("resurrection_anchor_deployed"):int_id()
  local f_null = FurnId.new("f_null"):int_id()

  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local furn_id = gapi.get_map():get_furn_at(pos)

  if furn_id == f_null then
    mod.add_anchor_to_list(abs_pos)
    gapi.get_map():set_furn_at(pos, f_anchor)
    gapi.add_msg("The resurrection anchor was placed at your position")
  else
    gapi.add_msg("Can only be placed on a square with no existing furniture.")
    return 0
  end
  return 1
end

mod.pick_teleport_destination = function(who)
  local pos = who:get_pos_ms()
  local player_abs = gapi.get_map():get_abs_ms(pos)
  local min_dist = math.maxinteger
  local anchor_idx = 0

  for idx, anchor_abs_ms in pairs(mod.anchor_list) do
    gapi.add_msg("Anchor found at " .. tostring(anchor_abs_ms))
    local distance = coords.rl_dist(player_abs, anchor_abs_ms)
    gapi.add_msg("Distance: " .. tostring(distance))
    if distance < min_dist then
      anchor_idx = idx
      min_dist = distance
    end
  end

  if anchor_idx == 0 then return nil end
  return mod.anchor_list[anchor_idx]
end

mod.remove_placed_furniture = function(pos)
  local abs_pos = gapi.get_map():get_abs_ms(pos)
  local abs_omt = coords.ms_to_omt(abs_pos)

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
