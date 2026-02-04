local sonar = {}

local underwater_patterns = {
  "river",
  "lake",
  "lake_bed_lab",
  "ocean",
  "bay",
  "sea",
  "water",
  "cargo_ship",
  "shipwreck",
  "sealab",
}

local function oter_id_to_string(oter_id)
  if oter_id.str then return oter_id:str() end
  if oter_id.str_id then return oter_id:str_id():str() end
  return tostring(oter_id)
end

local function is_underwater_oter(oter_id)
  local id_str = oter_id_to_string(oter_id)
  for _, pattern in ipairs(underwater_patterns) do
    if string.find(id_str, pattern, 1, true) then return true end
  end
  return false
end

sonar.register = function(mod)
  mod.sonar_scan = function(who, item, pos)
    local map = gapi.get_map()
    local abs_ms = map:get_abs_ms(pos)
    local center_omt = coords.ms_to_omt(abs_ms)
    local radius = 7
    local depth_steps = 5
    local any_revealed = false
    local start_omt = center_omt
    if center_omt.z >= 0 and is_underwater_oter(overmapbuffer.ter(center_omt)) then
      start_omt = Tripoint.new(center_omt.x, center_omt.y, center_omt.z - 1)
    end
    for depth_index = 0, depth_steps - 1 do
      local scan_omt = Tripoint.new(start_omt.x, start_omt.y, start_omt.z - depth_index)
      if not is_underwater_oter(overmapbuffer.ter(scan_omt)) then break end
      if overmapbuffer.reveal(scan_omt, radius, is_underwater_oter) then any_revealed = true end
    end

    if any_revealed then
      gapi.add_msg(locale.gettext("The sonar pulse maps nearby underwater terrain."))
    else
      gapi.add_msg(locale.gettext("The sonar pulse finds nothing new."))
    end

    return 1
  end
end

return sonar
