gdebug.log_info("Callback Lua Test: Main")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]
mod.storage = storage

local player = function() return gapi.get_avatar() end

local player_name = function() return player().name end

local iname = function(item) return item:tname(1, true, 0) end

local query = function(prompt)
  local q = QueryPopup.new()
  q:message(prompt)
  return q:query_yn() == "YES"
end

mod.can_equip = function(wear, params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  if wear then
    return query("TEST | Wear this item?")
  else
    return query("TEST | Equip this item?")
  end
end

mod.on_equip = function(wear, params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  local move_cost = params.move_cost
  if wear then
    gapi.add_msg("TEST | You don your " .. iname(item) .. ".")
  else
    gapi.add_msg("TEST | You wield your " .. iname(item) .. ". It takes " .. tostring(move_cost) .. " moves.")
  end
end

mod.can_unequip = function(wear, params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  if wear then
    return query("TEST | Take off this item?")
  else
    return query("TEST | Unwield this item?")
  end
end

mod.on_unequip = function(wear, params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  if wear then
    gapi.add_msg("TEST | You take your " .. iname(item) .. " off.")
  else
    gapi.add_msg("TEST | You unwield your " .. iname(item) .. ".")
  end
end

mod.on_tick = function(params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  local pos = params.pos
  gapi.add_msg("TEST | " .. iname(item) .. " ticks at " .. tostring(pos))
end

mod.on_pickup = function(params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  gapi.add_msg("TEST | You pick up the " .. iname(item))
end

mod.on_drop = function(params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  local pos = params.pos
  return query("TEST |Drop this item?")
end

mod.on_durability_change = function(params)
  local user = params.user
  if user ~= player() then return end
  local item = params.item
  local change = params.old_damage - params.new_damage
  if change > 0 then
    gapi.add_msg("TEST | Your " .. iname(item) .. " took " .. tostring(change) .. " damage")
  else
    gapi.add_msg("TEST | Your " .. iname(item) .. " was repaired for " .. tostring(-change))
  end
end

mod.on_activate = function(params)
  local user = params.user
  local bionic = params.bionic
  gapi.add_msg("TEST | " .. bionic:info():name() .. " activated by " .. user.name)
end

mod.on_deactivate = function(params)
  local user = params.user
  local bionic = params.bionic
  gapi.add_msg("TEST | " .. bionic:info():name() .. " deactivated by " .. user.name)
end

mod.on_install = function(params)
  local user = params.user
  local bionic = params.bionic
  gapi.add_msg("TEST | " .. bionic:info():name() .. " installed into " .. user.name)
end

mod.on_uninstall = function(params)
  local user = params.user
  local bionic = params.bionic
  gapi.add_msg("TEST | " .. bionic:info():name() .. " uninstalled from " .. user.name)
end
