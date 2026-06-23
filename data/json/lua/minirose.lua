local M = {}

local minirose_id = BionicDataId.new("bio_minirose")
local mininuke_act_id = ItypeId.new("mininuke_act")

---@param pos TripointBubMs
local function detonate_mininuke_at(pos)
  local nuke = gapi.create_item(mininuke_act_id, 1)
  nuke:set_charges(0)
  nuke:invoke_at(pos)
end

---@param who Character
local function detonate_minirose(who)
  if not who:has_bionic(minirose_id) then return end
  who:remove_bionic(minirose_id)
  detonate_mininuke_at(who:bub_pos())
end

---@param who Character
---@return boolean
local function confirm_manual_detonation(who)
  if not who:is_avatar() then return true end

  local prompt = QueryPopup.new()
  prompt:message(locale.gettext("Detonate your minirose CBM now? Answering no arms its dead man's switch."))
  prompt:message_color(Color.c_red)
  return prompt:query_yn() == "YES"
end

---@param params BionicCallbackParams
function M.on_activate(params)
  local who = params.user
  if not confirm_manual_detonation(who) then
    gapi.add_msg(MsgType.info, locale.gettext("You arm the dead man's switch of minirose."))
    return
  end
  detonate_minirose(who)
end

---@class MiniroseDeathParams
---@field char Character?

---@param params MiniroseDeathParams
function M.on_character_death(params)
  local who = params.char
  if who and who:has_active_bionic(minirose_id) then detonate_minirose(who) end
end

return M
