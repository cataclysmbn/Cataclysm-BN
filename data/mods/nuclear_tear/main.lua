local mod = game.mod_runtime[game.current_mod]

local NUKE_DAMAGE_THRESHOLD = 300

local fd_fatigue_id = FieldTypeId.new("fd_fatigue"):int_id()

---@param params OnExplodeParams
mod.on_explosion = function(params)
  local pos = params.pos
  local damage = params.damage
  local radius = params.radius

  local m = gapi.get_map()

  if damage < NUKE_DAMAGE_THRESHOLD then return end

  ---@type Tripoint[]
  local points_to_expand = {}
  for _, check_pos in ipairs(m:points_in_radius(pos, radius // 2, radius // 2)) do
    local field_at_pos = m:has_field_at(check_pos, fd_fatigue_id)

    if field_at_pos then
      local field_name = m:get_field_name_at(check_pos, fd_fatigue_id)
      local is_remove = gapi.rng(1, 4) ~= 1

      if is_remove then
        m:remove_field_at(check_pos, fd_fatigue_id)
        gapi.add_msg(MsgType.good, string.format(locale.gettext("The blast seals the %s!"), field_name))
      else
        table.insert(points_to_expand, check_pos)
      end
    end
  end
  for _, check_pos in ipairs(points_to_expand) do
    local field_name = m:get_field_name_at(check_pos, fd_fatigue_id)
    local spawned = false
    for _, adj_pos in ipairs(m:points_in_radius(check_pos, 1)) do
      if not (adj_pos.x == check_pos.x and adj_pos.y == check_pos.y) and gapi.rng(1, 3) == 1 then
        local success = m:add_field_at(adj_pos, fd_fatigue_id, gapi.rng(1, 3), TimeDuration.from_turns(0))
        if success then
          gapi.add_msg(
            MsgType.bad,
            string.format(locale.gettext("The blast destabilizes the %s, causing it to widen!"), field_name)
          )
          spawned = true
          break
        end
      end
    end
    if spawned == false then
      gapi.add_msg(
        MsgType.bad,
        string.format(locale.gettext("The blast destabilizes the %s, but it holds!"), field_name)
      )
    end
  end
end
