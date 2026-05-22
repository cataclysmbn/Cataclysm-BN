-- Sky Islands BN Port - Item Use Functions
-- Lua iuse handlers for various items

local iuse = {}

-- Helper: Check if furniture exists in adjacent tiles
local function has_adjacent_furniture(player, furniture_id)
  local map = gapi.get_map()
  local player_pos = player:get_pos_ms()

  -- Check all 8 adjacent tiles + current tile
  for dx = -1, 1 do
    for dy = -1, 1 do
      local check_pos = Tripoint.new(player_pos.x + dx, player_pos.y + dy, player_pos.z)
      local furn = map:get_furn_at(check_pos)
      if furn == furniture_id:int_id() then
        return true
      end
    end
  end
  return false
end

-- Helper: Check if terrain exists in adjacent tiles
local function has_adjacent_terrain(player, terrain_id)
  local map = gapi.get_map()
  local player_pos = player:get_pos_ms()

  -- Check all 8 adjacent tiles + current tile
  for dx = -1, 1 do
    for dy = -1, 1 do
      local check_pos = Tripoint.new(player_pos.x + dx, player_pos.y + dy, player_pos.z)
      local ter = map:get_ter_at(check_pos)
      if ter == terrain_id:int_id() then
        return true
      end
    end
  end
  return false
end

-- Generic imprint function for furniture-based copyplates
local function imprint_furniture_copyplate(player, furn_id_str, inert_id_str, active_id_str, machine_name)
  -- Check for adjacent furniture
  if not has_adjacent_furniture(player, FurnId.new(furn_id_str)) then
    gapi.add_msg(string.format(locale.gettext("You need to be standing next to a working %s to imprint this copyplate."), machine_name))
    return 0
  end

  -- Remove inert item and add active one
  local inert_plate = player:get_item_with_id(ItypeId.new(inert_id_str), false)
  player:remove_item(inert_plate)
  local active_id = ItypeId.new(active_id_str)
  player:add_item_with_id(active_id, 1)

  gapi.add_msg(string.format(locale.gettext("The copyplate hums and glows as it absorbs the %s's schematics!"), machine_name))
  gapi.add_msg(string.format(locale.gettext("You now have an activated %s copyplate."), machine_name))

  return 1
end

-- Generic imprint function for terrain-based copyplates
local function imprint_terrain_copyplate(player, ter_id_str, inert_id_str, active_id_str, machine_name)
  -- Check for adjacent terrain
  if not has_adjacent_terrain(player, TerId.new(ter_id_str)) then
    gapi.add_msg(string.format(locale.gettext("You need to be standing next to a working %s to imprint this copyplate."), machine_name))
    return 0
  end

  -- Remove inert item and add active one
  local inert_plate = player:get_item_with_id(ItypeId.new(inert_id_str), false)
  player:remove_item(inert_plate)
  local active_id = ItypeId.new(active_id_str)
  player:add_item_with_id(active_id, 1)

  gapi.add_msg(string.format(locale.gettext("The copyplate hums and glows as it absorbs the %s's schematics!"), machine_name))
  gapi.add_msg(string.format(locale.gettext("You now have an activated %s copyplate."), machine_name))

  return 1
end

-- Autodoc (furniture-based)
function iuse.imprint_autodoc(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end
  return imprint_furniture_copyplate(player, "f_autodoc",
    "skyisland_autodoc_inert", "skyisland_autodoc_active", "Autodoc")
end

function iuse.imprint_autodoc_couch(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end
  return imprint_furniture_copyplate(player, "f_autodoc_couch",
    "skyisland_autodoc_couch_inert", "skyisland_autodoc_couch_active", "Autodoc Couch")
end

-- Nanofabricator (terrain-based)
function iuse.imprint_nanofab_body(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end
  return imprint_terrain_copyplate(player, "t_nanofab_body",
    "skyisland_nanofab_body_inert", "skyisland_nanofab_body_active", "Nanofabricator")
end

function iuse.imprint_nanofab_panel(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end
  return imprint_terrain_copyplate(player, "t_nanofab",
    "skyisland_nanofab_panel_inert", "skyisland_nanofab_panel_active", "Nanofabricator Control Panel")
end

-- CVD Machine (terrain-based)
function iuse.imprint_cvd_body(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end
  return imprint_terrain_copyplate(player, "t_cvdbody",
    "skyisland_cvd_body_inert", "skyisland_cvd_body_active", "CVD Machine")
end

function iuse.imprint_cvd_panel(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end
  return imprint_terrain_copyplate(player, "t_cvdmachine",
    "skyisland_cvd_panel_inert", "skyisland_cvd_panel_active", "CVD Control Panel")
end

return iuse
