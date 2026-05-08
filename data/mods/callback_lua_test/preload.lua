gdebug.log_info("Callback Lua Test: Preload")

local mod = game.mod_runtime[game.current_mod]

local gen_bio = BionicDataId.new("bio_perpetual_test")
local taste_bio = BionicDataId.new("bio_taste_blocker")

local on_game_started = function()
  local player = gapi.get_avatar()
  player:create_item(ItypeId.new("stick_long"), 1)
  player:create_item(ItypeId.new("hat_hard"), 1)
  player:add_bionic(gen_bio)
  if player:has_bionic(gen_bio) then
    if not player:activate_bionic(gen_bio) then
      print("Failed to activate gen bio")
    else
      player:toggle_safe_fuel_mod(gen_bio)
    end
  else
    print("Gen bio failed to add")
  end
  player:add_bionic(taste_bio)
  if player:has_bionic(taste_bio) then
    if not player:activate_bionic(taste_bio) then print("Failed to activate taste bio") end
  else
    print("Taste bio failed to add")
  end
end

game.add_hook("on_game_started", function() return on_game_started() end)

game.iwearable_functions["hat_hard"] = {
  can_wear = function(params) return mod.can_equip(true, params) end,
  on_wear = function(params) return mod.on_equip(true, params) end,
  can_takeoff = function(params) return mod.can_unequip(true, params) end,
  on_takeoff = function(params) return mod.on_unequip(true, params) end,
}

game.iequippable_functions["hat_hard"] = {
  on_durability_change = function(params) return mod.on_durability_change(params) end,
}

game.iwieldable_functions["stick_long"] = {
  can_wield = function(params) return mod.can_equip(false, params) end,
  on_wield = function(params) return mod.on_equip(false, params) end,
  can_unwield = function(params) return mod.can_unequip(false, params) end,
  on_unwield = function(params) return mod.on_unequip(false, params) end,
}

game.iequippable_functions["stick_long"] = {
  on_durability_change = function(params) return mod.on_durability_change(params) end,
}

game.istate_functions["stick_long"] = {
  on_tick = function(params) return mod.on_tick(params) end,
  on_pickup = function(params) return mod.on_pickup(params) end,
  on_drop = function(params) return mod.on_drop(params) end,
}

game.bionic_functions["bio_perpetual_test"] = {
  on_activate = function(params) return mod.on_activate(params) end,
  on_deactivate = function(params) return mod.on_deactivate(params) end,
  on_installed = function(params) return mod.on_installed(params) end,
  on_uninstalled = function(params) return mod.on_uninstalled(params) end,
}

game.bionic_functions["bio_taste_blocker"] = {
  on_activate = function(params) return mod.on_activate(params) end,
  on_deactivate = function(params) return mod.on_deactivate(params) end,
  on_installed = function(params) return mod.on_installed(params) end,
  on_uninstalled = function(params) return mod.on_uninstalled(params) end,
}
