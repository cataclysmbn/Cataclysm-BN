local ui = require("lib.ui")

-- Format in MW/kW with three-point precision
---@param watts integer
local function display_watt(watts)
  watts = watts or 0
  if math.abs(watts) >= 1000000 then
    return string.format("%.3f MW", watts / 1000000.0)
  elseif math.abs(watts) >= 1000 then
    return string.format("%.3f kW", watts / 1000.0)
  else
    return string.format("%d W", watts)
  end
  return ""
end

local voltmeter = {}

---@type fun(who: Character, item: Item, pos: Tripoint): integer
voltmeter.menu = function(who, item, pos)
  local info_msg = voltmeter.get_grid_charge_info(who, item, pos)
  info_msg = info_msg .. "\n" .. voltmeter.get_grid_connections_info(who, item, pos)
  info_msg = info_msg .. "\n\n" .. locale.gettext("Do you want to modify grid connections?")

  local modify = ui.query_yn(info_msg)
  if modify then return voltmeter.modify_grid_connections(who, item, pos) end

  return 0
end

---@type fun(who: Character, item: Item, pos: Tripoint): string
voltmeter.get_grid_charge_info = function(who, item, pos)
  local pos_abs = gapi.get_map():get_abs_ms(pos)
  local grid = gapi.get_distribution_grid_tracker():grid_at(pos_abs)
  local amt = grid:get_resource()
  if not amt then return "" end
  local stat = grid:get_power_stat()

  local msg = string.format(locale.gettext("This electric grid stores %d kJ of electric power."), amt)

  if stat.gen_w > 0 or stat.use_w > 0 then
    msg = msg .. string.format(locale.gettext("\nGeneration: %s"), display_watt(stat.gen_w))
    msg = msg .. string.format(locale.gettext("\nConsumption: %s"), display_watt(stat.use_w))
    msg = msg .. string.format(locale.gettext("\nNet: %s"), display_watt(stat:net_w()))
  end

  return msg
end

---@type fun(who: Character, item: Item, pos: Tripoint): string
voltmeter.get_grid_connections_info = function(who, item, pos)
  local pos_abs_ms = gapi.get_map():get_abs_ms(pos)
  local pos_abs, _ = coords.ms_to_omt(pos_abs_ms)
  local connections = gapi.get_overmap_buffer():electric_grid_connectivity_at(pos_abs)

  local six_dirs = gapi.six_cardinal_directions()
  local connection_names = {}

  for _, delta in ipairs(six_dirs) do
    for _, conn in ipairs(connections) do
      if conn.x == delta.x and conn.y == delta.y and conn.z == delta.z then
        local dir = gapi.direction_from(delta)
        table.insert(connection_names, gapi.direction_name(dir))
        break
      end
    end
  end

  local msg
  if #connection_names == 0 then
    msg = locale.gettext("This electric grid has no connections.")
  else
    msg = string.format(locale.gettext("This electric grid has connections: %s."), table.concat(connection_names, ", "))
  end

  return msg
end

---@type fun(who: Character, item: Item, pos: Tripoint): integer
voltmeter.modify_grid_connections = function(who, item, pos)
  local pos_abs_ms = gapi.get_map():get_abs_ms(pos)
  local pos_abs, _ = coords.ms_to_omt(pos_abs_ms)
  local connections = gapi.get_overmap_buffer():electric_grid_connectivity_at(pos_abs)

  local six_dirs = gapi.six_cardinal_directions()
  local connection_present = {}
  local menu = UiList.new()
  menu:title(locale.gettext("Modify Grid Connections"))

  for i, delta in ipairs(six_dirs) do
    -- Check if connection exists in this direction
    local found = false
    for _, conn in ipairs(connections) do
      if conn.x == delta.x and conn.y == delta.y and conn.z == delta.z then
        found = true
        break
      end
    end
    connection_present[i] = found

    local dir = gapi.direction_from(delta)
    local name = gapi.direction_name(dir)
    local format_str = found and locale.gettext("Remove connection in direction: %s")
      or locale.gettext("Add connection in direction: %s")

    local new_z = pos.z + delta.z
    local enabled = new_z >= -10 and new_z <= 10

    menu:add(i - 1, string.format(format_str, name))
    if not enabled then menu.entries[i].enable = false end
  end

  local choice = menu:query()
  if choice < 0 then return 0 end

  local idx = choice + 1
  local delta = six_dirs[idx]
  local destination_pos_abs = pos_abs + delta

  if connection_present[idx] then
    -- Remove connection
    gapi.get_overmap_buffer():remove_grid_connection(pos_abs, destination_pos_abs)
    gapi.add_msg(MsgType.good, locale.gettext("Grid connection removed."))
  else
    -- Add connection
    local lhs_locations = gapi.get_overmap_buffer():electric_grid_at(pos_abs)
    local rhs_locations = gapi.get_overmap_buffer():electric_grid_at(destination_pos_abs)

    -- Check if same grid
    local cost_mult = 0
    local same_grid = true
    if #lhs_locations ~= #rhs_locations then
      same_grid = false
    else
      -- Simple comparison (this may need refinement)
      same_grid = false
      for _, loc1 in ipairs(lhs_locations) do
        local found_match = false
        for _, loc2 in ipairs(rhs_locations) do
          if loc1.x == loc2.x and loc1.y == loc2.y and loc1.z == loc2.z then
            found_match = true
            break
          end
        end
        if not found_match then break end
      end
    end

    if same_grid then
      cost_mult = 0
    else
      cost_mult = #lhs_locations + #rhs_locations
    end

    local grid_connection_string
    if cost_mult == 0 then
      grid_connection_string = string.format(
        locale.gettext("You are connecting two locations in the same grid, with %d elements."),
        math.max(#lhs_locations, #rhs_locations)
      )
    elseif #lhs_locations == 1 or #rhs_locations == 1 then
      grid_connection_string = string.format(
        locale.gettext("You are extending a grid with %d elements."),
        math.max(#lhs_locations, #rhs_locations)
      )
    else
      grid_connection_string = string.format(
        locale.gettext("You are connecting a grid with %d elements to a grid with %d elements."),
        #lhs_locations,
        #rhs_locations
      )
    end

    -- Get the requirement and multiply by cost
    local requirement_base = get_requirement("add_grid_connection")
    if not requirement_base then
      gapi.add_msg(MsgType.warning, locale.gettext("Error: requirement not found."))
      return 0
    end

    local reqs = requirement_base * cost_mult
    local crafting_inv = who:crafting_inventory()

    -- Check if player has required items/tools
    if not reqs:can_make_with_inventory(crafting_inv) then
      local missing_info = string.format("%s\n%s\n%s", grid_connection_string, reqs:list_missing(), reqs:list_all())
      ui.popup(missing_info)
      return 0
    end

    -- Confirm with player
    local confirm
    if cost_mult == 0 then
      confirm = ui.query_yn(
        string.format(
          "%s\n%s",
          grid_connection_string,
          locale.gettext("This action will not consume any resources.\nAre you sure?")
        )
      )
    else
      confirm = ui.query_yn(
        string.format("%s\n%s\n%s", grid_connection_string, reqs:list_all(), locale.gettext("Are you sure?"))
      )
    end

    if confirm then
      -- Consume items and tools
      for _, component_list in ipairs(reqs:get_components()) do
        who:consume_items(component_list)
      end
      for _, tool_list in ipairs(reqs:get_tools()) do
        who:consume_tools(tool_list)
      end
      who:invalidate_crafting_inventory()

      local success = gapi.get_overmap_buffer():add_grid_connection(pos_abs, destination_pos_abs)
      if success then
        gapi.add_msg(MsgType.good, locale.gettext("Grid connection established."))
        return item:get_type():obj():charges_to_use()
      else
        gapi.add_msg(MsgType.warning, locale.gettext("Failed to establish grid connection."))
      end
    end
  end

  return 0
end

return voltmeter
