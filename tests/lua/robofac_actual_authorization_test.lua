package.path = package.path .. ";data/json/?.lua"
package.loaded["lua.robofac"] = nil
local robofac = require("lua.robofac")

---@type Avatar
local avatar = gapi.get_avatar()
avatar:set_value("npctalk_var_dialogue_intercom_completed_robofac_intercom_3", "yes")

---@type NPC
local security = test_data.security
---@type Monster
local turret = test_data.turret

robofac.authorize_active_hub01_security({ security })
robofac.authorize_active_hub01_turrets({ turret })

test_data.security_faction = security:get_faction_id():str()
test_data.security_attitude = security:get_attitude()
test_data.turret_authorized = turret.faction == MonsterFactionId.new("robofac_authorized"):int_id()
