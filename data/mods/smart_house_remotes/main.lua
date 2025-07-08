gdebug.log_info("SHR: main.")

--[[
    Main script.
    This file can be loaded multiple times (e.g. when you press a key to
    hot-reload Lua code), so ideally we shouldn't modify here any state defined
    in earlier load stages.
]]
--

local mod = game.mod_runtime[game.current_mod]

--[[
    When we export Lua function, Lua is smart enough not to garbage collect
    local variables and functions that our exported function (closure) depends on.
    So, why do we put everything into the mod runtime table anyway?
    Well, to allow mod interoperability, of course!
    Say, a mod decided to add an item action that reconfigures existing smart remote.
    It could hack together a copy of our 'set_remote_base' function of course,
    but it's much easier (and reliable) to just grab it from globally available
    game.mod_runtime.smart_house_remote_mod.set_remote_base
]]

-- Item id of the remote
mod.item_id = "smart_house_remote"

-- Variable id to store remote's base omt
mod.var_base = "shr_base"

-- Range of remote, horizontal
mod.remote_wireless_range = 24

-- Range of remote, vertical
mod.remote_wireless_range_z = 2

-- Get abs omt of remote's base
mod.get_remote_base_omt = function(item) return item:get_var_tri(mod.var_base, Tripoint.new(0, 0, 0)) end

-- Get abs ms of remote's base
mod.get_remote_base_abs_ms = function(item)
  local p_omt = mod.get_remote_base_omt(item)
  return coords.omt_to_ms(p_omt) + Point.new(const.OMT_MS_SIZE // 2, const.OMT_MS_SIZE // 2)
end

-- Set remote's base abs omt
mod.set_remote_base = function(item, p_omt) item:set_var_tri(mod.var_base, p_omt) end

-- Look for spawned remotes and bind them to given omt
mod.on_mapgen_postprocess_hook = function(map, p_omt, when)
  local mapsize = map:get_map_size()
  local item_id = mod.item_id
  for y = 0, mapsize - 1 do
    for x = 0, mapsize - 1 do
      local p = Tripoint.new(x, y, 0)
      -- TODO: Check whether using has_items_at() gives a speedup in Lua.
      --       In C++, it's supposed to be faster then !i_at( p ).empty()
      if map:has_items_at(p) then
        local items = map:get_items_at(p):as_item_stack()
        for _, item in pairs(items) do
          if item:get_type():str() == item_id then mod.set_remote_base(item, p_omt) end
        end
      end
    end
  end
end

-- List of terrain transformations supported by the mod
mod.get_transform_list = function()
  return {
    {
      -- terrain id when open
      o = "t_window_domestic",
      -- terrain id when closed
      c = "t_curtains",
      -- terrain id that belongs to this group, but can't be closed/opened
      -- TODO: some 'inert' tiles can belong to multiple transforms
      i = {
        "t_window",
        "t_window_frame",
        "t_window_open",
        "t_window_empty",
        "t_window_no_curtains",
        "t_window_no_curtains_open",
      },
      -- action name in UI
      name_open = locale.gettext("Open curtains"),
      name_close = locale.gettext("Close curtains"),
      -- power required to open or close
      power = 5,
    },
    {
      -- TODO: garage doors should check whether there's something obstructing them.
      o = "t_door_metal_locked_o",
      c = "t_door_metal_locked",
      name_open = locale.gettext("Raise garage door"),
      name_close = locale.gettext("Lower garage door"),
      power = 20,
    },
  }
end

-- Caches transforms list for lookups
mod.cache_transforms = function(tlist)
  local to_close_list = {}
  local to_open_list = {}
  local idx = 0
  for _, v in pairs(tlist) do
    idx = idx + 1
    to_open_list[v.c] = idx
    to_close_list[v.o] = idx
  end
  return to_close_list, to_open_list
end

-- These 2 functions use breadth-first search to find all tiles belonging to given block
mod.get_neighbours_at = function(opts, block, p)
  local k = tostring(p)
  local v = opts[k]
  if v ~= nil and v.idx == block.idx then
    opts[k] = nil
    block.points[#block.points + 1] = p
    if v.can_open then block.can_open_num = block.can_open_num + 1 end
    if v.can_close then block.can_close_num = block.can_close_num + 1 end
    mod.get_neighbours(opts, block, p)
  end
end
mod.get_neighbours = function(opts, block, p)
  mod.get_neighbours_at(opts, block, p + Tripoint.new(1, 0, 0))
  mod.get_neighbours_at(opts, block, p + Tripoint.new(-1, 0, 0))
  mod.get_neighbours_at(opts, block, p + Tripoint.new(0, 1, 0))
  mod.get_neighbours_at(opts, block, p + Tripoint.new(0, -1, 0))
end

-- Helper func to check whether value is in array
local find_in_array = function(arr, val)
  for k, v in pairs(arr) do
    if v == val then return k end
  end
  return nil
end

-- Helper func to check whether tile is considered as inert for one of the blocks
local check_is_tile_inert = function(tlist, val)
  for idx, transform in pairs(tlist) do
    if transform.i and find_in_array(transform.i, val) then return idx end
  end
  return nil
end

--[[
    Look for multitile 'blocks' in given omt (e.g. a multitile window, or a multitile garage door).
    Returns array of tables:
    {
        idx = int,           --Index of transform entry in transforms list
        points = {},         --Array of points that belong to the multitile
        can_open_num = int,  --Number of tiles that can be opened
        can_close_num = int, --Number of tiles that can be closed
        can_open = bool,     --Whether block can be opened
        can_close = bool,    --Whether block can be closed
    }
]]
--
mod.build_target_list = function(map, pos_omt)
  -- First, find all tiles that can be opened, closed or belong to 'inert' group
  local act_tiles = {}
  local tlist = mod.get_transform_list()
  local to_close_list, to_open_list = mod.cache_transforms(tlist)
  local p_zero = map:get_local_ms(coords.omt_to_ms(pos_omt))
  local iter_max = const.OMT_MS_SIZE - 1
  for y = 0, iter_max do
    for x = 0, iter_max do
      local p = p_zero + Tripoint.new(x, y, 0)
      local t = map:get_ter_at(p):str_id()

      local idx_found = to_close_list[t:str()]
      local can_open = false
      local can_close = false
      if idx_found then
        can_close = true
      else
        idx_found = to_open_list[t:str()]
        if idx_found then
          can_open = true
        else
          idx_found = check_is_tile_inert(tlist, t:str())
        end
      end

      if idx_found then
        act_tiles[tostring(p)] = { p = p, idx = idx_found, can_open = can_open, can_close = can_close }
      end
    end
  end

  -- Use breadth-first to sort tiles into multitile blocks
  local ret = {}

  local next = next
  -- While (table has entries) do
  while next(act_tiles) ~= nil do
    -- Remove first available entry from table
    local k, v = next(act_tiles)
    act_tiles[k] = nil

    -- Build block of neighboring tiles with same idx
    local block = {
      points = { v.p },
      idx = v.idx,
      can_open_num = v.can_open and 1 or 0,
      can_close_num = v.can_close and 1 or 0,
    }
    mod.get_neighbours(act_tiles, block, v.p)
    block.can_open = block.can_open_num > 0
    block.can_close = block.can_close_num > 0

    -- Add block to list
    ret[#ret + 1] = block
  end

  return ret
end

-- Close all closable tiles in block
mod.block_close = function(block, transform)
  for _, p in pairs(block.points) do
    local ter_at_p = gapi.get_map():get_ter_at(p):str_id()
    if ter_at_p:str() == transform.o then gapi.get_map():set_ter_at(p, TerId.new(transform.c):int_id()) end
  end
end

-- Open all openable tiles in block
mod.block_open = function(block, transform)
  for _, p in pairs(block.points) do
    local ter_at_p = gapi.get_map():get_ter_at(p):str_id()
    if ter_at_p:str() == transform.c then gapi.get_map():set_ter_at(p, TerId.new(transform.o):int_id()) end
  end
end

-- Show 'not enough power' error
mod.show_low_power_error = function()
  local pp = QueryPopup.new()
  --~ Message on the remote, stylized as calculator led display.
  --~ Shown when there's not enough grid charge.
  pp:message(locale.gettext("Low Current At Endpoint"))
  -- This color is awful, but it's a cheap LCD display, what did you expect?
  pp:message_color(Color.i_green)
  pp:allow_any_key(true)
  pp:query()
end

-- Show 'no signal' error
mod.show_no_signal_error = function()
  local pp = QueryPopup.new()
  --~ Message on the remote, stylized as calculator led display.
  --~ Shown when player is too far away from the area.
  pp:message(locale.gettext("No Signal"))
  pp:message_color(Color.i_green)
  pp:allow_any_key(true)
  pp:query()
end

-- Show 'no valid blocks' error
mod.show_no_endpoints_error = function()
  local pp = QueryPopup.new()
  --~ Message on the remote, stylized as calculator led display.
  --~ Shown when there's nothing to activate.
  pp:message(locale.gettext("No Endpoints Available"))
  pp:message_color(Color.i_green)
  pp:allow_any_key(true)
  pp:query()
end

-- Add message indicating the remote works
mod.show_msg_remote_working = function() gapi.add_msg(locale.gettext("The remote beeps quietly.")) end

-- Open or close all tiles in block
mod.invoke_block = function(block, grid)
  local tlist = mod.get_transform_list()
  local transform = tlist[block.idx]
  local power_available = grid:get_resource(true)

  if block.can_open then
    local power_needed = transform.power * block.can_open_num
    if power_needed > power_available then
      mod.show_low_power_error()
      return 0
    end
    grid:mod_resource(-power_needed, true)
    mod.block_open(block, transform)
  elseif block.can_close then
    local power_needed = transform.power * block.can_close_num
    if power_needed > power_available then
      mod.show_low_power_error()
      return 0
    end
    grid:mod_resource(-power_needed, true)
    mod.block_close(block, transform)
  end
  return 1
end

-- Main iuse function. Returns amount of charges consumed from item.
mod.iuse_function = function(who, item, pos)
  local user_pos = gapi.get_map():get_abs_ms(pos)

  -- Uncomment this so on activation the remote reconfigures itself to work in user's omt
  --[[
    mod.set_remote_base( item, coords.ms_to_omt( user_pos ) )
    gapi.add_msg(locale.gettext("Remote reconfigured!"))
    --if true then
    --    return 0
    --end
    ]]

  local base_pos = mod.get_remote_base_abs_ms(item)

  -- Check distance to wireless base the remote is bound to.
  -- The base does not physically exist in game world, but we imagine
  -- it's tucked away into a hoouse wall or something.
  if
    math.abs(user_pos.z - base_pos.z) > mod.remote_wireless_range_z
    or coords.rl_dist(user_pos, base_pos) > mod.remote_wireless_range
  then
    mod.show_no_signal_error()
    return 0
  end

  local base_pos_omt = mod.get_remote_base_omt(item)
  local grid = gapi.get_distribution_grid_tracker():get_grid_at_abs_ms(base_pos)
  local power_available = grid:get_resource(true)

  -- If house has no power, the wireless base also has no power and can't emit signal.
  if power_available == 0 then
    mod.show_no_signal_error()
    return 0
  end

  -- Get list of all available multitile formations in base's omt
  local targets = mod.build_target_list(gapi.get_map(), base_pos_omt)

  -- Ignore ones that are completely inert (e.g. window got all its curtains torn down).
  local sel_list = {}
  local transforms = mod.get_transform_list()
  for block_idx, block in pairs(targets) do
    if block.can_open then
      sel_list[#sel_list + 1] = {
        block = block,
        do_open = true,
        text = transforms[block.idx].name_open .. " #" .. tostring(block_idx),
      }
    elseif block.can_close then
      sel_list[#sel_list + 1] = {
        block = block,
        do_open = false,
        text = transforms[block.idx].name_close .. " #" .. tostring(block_idx),
      }
    end
  end

  -- No available blocks? Too bad.
  if next(sel_list) == nil then
    mod.show_no_endpoints_error()
    return 0
  end

  -- If only 1 is available, don't ask
  if #sel_list == 1 then
    mod.show_msg_remote_working()
    return mod.invoke_block(sel_list[1].block, grid)
  end

  -- Query which block to activate
  local ui = UiList.new()
  --~ Title of the menu prompting player to select what to activate with the remote.
  --~ Typically it's doors, garage doors or window curtains.
  ui:title(locale.gettext("Select endpoint"))
  for i = 1, #sel_list do
    ui:add(i, sel_list[i].text)
  end
  local eidx = ui:query()

  -- Canceled by player
  if eidx < 1 then
    gapi.add_msg(locale.gettext("Nevermind."))
    return 0
  end

  -- Activate desired block
  mod.show_msg_remote_working()
  return mod.invoke_block(sel_list[eidx].block, grid)
end
