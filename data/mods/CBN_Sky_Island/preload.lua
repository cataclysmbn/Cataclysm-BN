-- Sky Islands BN Port - Proof of Concept
-- preload.lua - Hook and iuse registration

local mod = game.mod_runtime[game.current_mod]
local util = require("util")

-- Register item use functions
game.iuse_functions["SKYISLAND_WARP_OBELISK"] = {
  use = function(params) return mod.use_warp_obelisk(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_RETURN_OBELISK"] = {
  use = function(params) return mod.use_return_obelisk(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_HEART_MENU"] = {
  use = function(params) return mod.use_heart_menu(params.user, params.item, params.pos) end
}

-- Upgrade item activations
game.iuse_functions["SKYISLAND_UPGRADE_STABILITY1"] = {
  use = function(params) return mod.use_upgrade_stability1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_STABILITY2"] = {
  use = function(params) return mod.use_upgrade_stability2(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_STABILITY3"] = {
  use = function(params) return mod.use_upgrade_stability3(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_SCOUTING1"] = {
  use = function(params) return mod.use_upgrade_scouting1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_SCOUTING2"] = {
  use = function(params) return mod.use_upgrade_scouting2(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_EXITS1"] = {
  use = function(params) return mod.use_upgrade_exits1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_RAIDLENGTH1"] = {
  use = function(params) return mod.use_upgrade_raidlength1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_RAIDLENGTH2"] = {
  use = function(params) return mod.use_upgrade_raidlength2(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_BASEMENTS"] = {
  use = function(params) return mod.use_upgrade_basements(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_ROOFS"] = {
  use = function(params) return mod.use_upgrade_roofs(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_LABS"] = {
  use = function(params) return mod.use_upgrade_labs(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_SCOUTING_CLAIRVOYANCE1"] = {
  use = function(params) return mod.use_upgrade_scouting_clairvoyance1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_SCOUTING_CLAIRVOYANCE2"] = {
  use = function(params) return mod.use_upgrade_scouting_clairvoyance2(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_BONUSMISSIONS1"] = {
  use = function(params) return mod.use_upgrade_bonusmissions1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_BONUSMISSIONS2"] = {
  use = function(params) return mod.use_upgrade_bonusmissions2(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_BONUSMISSIONS3"] = {
  use = function(params) return mod.use_upgrade_bonusmissions3(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_BONUSMISSIONS4"] = {
  use = function(params) return mod.use_upgrade_bonusmissions4(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_BONUSMISSIONS5"] = {
  use = function(params) return mod.use_upgrade_bonusmissions5(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_HARDMISSIONS1"] = {
  use = function(params) return mod.use_upgrade_hardmissions1(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_HARDMISSIONS2"] = {
  use = function(params) return mod.use_upgrade_hardmissions2(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_SLAUGHTER"] = {
  use = function(params) return mod.use_upgrade_slaughter(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_UPGRADE_LANDING_FLIGHT"] = {
  use = function(params) return mod.use_upgrade_landing_flight(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_PROOF_DETERMINATION"] = function(...)
  return mod.use_proof_determination(...)
end

game.iuse_functions["SKYISLAND_PROOF_MASTERY"] = function(...)
  return mod.use_proof_mastery(...)
end

-- Utility item activations
game.iuse_functions["SKYISLAND_QUICKHEAL"] = {
  use = function(params) return mod.use_quickheal(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_EARTHBOUND_PILL"] = {
  use = function(params) return mod.use_earthbound_pill(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_SKYWARD_BEACON"] = {
  use = function(params) return mod.use_skyward_beacon(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_WARP_CRYSTAL"] = {
  use = function(params) return mod.use_warp_crystal(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_ANIMAL_TELEPORTER"] = {
  use = function(params) return mod.use_animal_teleporter(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_WARP_FOCUS"] = {
  use = function(params) return mod.use_warp_focus(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_IMPRINT_AUTODOC"] = {
  use = function(params) return mod.use_imprint_autodoc(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_IMPRINT_AUTODOC_COUCH"] = {
  use = function(params) return mod.use_imprint_autodoc_couch(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_IMPRINT_NANOFAB_BODY"] = {
  use = function(params) return mod.use_imprint_nanofab_body(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_IMPRINT_NANOFAB_PANEL"] = {
  use = function(params) return mod.use_imprint_nanofab_panel(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_IMPRINT_CVD_BODY"] = {
  use = function(params) return mod.use_imprint_cvd_body(params.user, params.item, params.pos) end
}

game.iuse_functions["SKYISLAND_IMPRINT_CVD_PANEL"] = {
  use = function(params) return mod.use_imprint_cvd_panel(params.user, params.item, params.pos) end
}

-- Register hooks
table.insert(game.hooks.on_game_started, function(...)
  return mod.on_game_started(...)
end)

table.insert(game.hooks.on_game_load, function(...)
  return mod.on_game_load(...)
end)

table.insert(game.hooks.on_game_save, function(...)
  return mod.on_game_save(...)
end)

-- table.insert(game.hooks.on_char_death, function(...)
--   return mod.on_char_death(...)
-- end)

table.insert(game.hooks.on_character_death, function(...)
  return mod.on_character_death(...)
end)

util.debug_log("Sky Islands PoC preload complete")
