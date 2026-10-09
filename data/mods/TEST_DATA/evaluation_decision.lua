-- Explicitly loaded by evaluation_decision_test.cpp, never by mod startup.
-- This exercises real bound objects. It deliberately does not isolate callback state.

---@class EvaluationDecisionWearParams
---@field who Character
---@field item Item

---@class EvaluationDecisionItemWearParams
---@field user Character
---@field item Item

---@param mode string
local function begin(mode)
  local depth = 0

  if mode == "item_pcall" or mode == "item_bad" then
    ---@param params EvaluationDecisionItemWearParams
    local function can_wear(params)
      assert(params.user ~= nil and params.item:get_type():str() == "test_preview_callback_coat")
      if mode == "item_bad" then return {} end
      pcall(require("lib.ui").query_yn, "Item callback?")
      return true
    end
    return can_wear
  end

  ---@param params EvaluationDecisionWearParams
  local function try_wear(params)
    if params.item:get_type():str() ~= "test_preview_callback_coat" then return { allowed = true } end
    local ui = require("lib.ui")
    if mode == "pcall" then
      -- A permissive mod fallback must not complete an unresolved evaluation.
      pcall(ui.query_yn, "Allow this fixture coat?")
      return { allowed = true }
    elseif mode == "nested" then
      depth = depth + 1
      if depth == 1 then
        pcall(function() params.who:can_wear(params.item, false) end)
        depth = depth - 1
        return { allowed = true }
      end
      depth = depth - 1
      return { allowed = ui.query_yn("Allow this fixture coat?") }
    elseif mode == "repeat" then
      local first = ui.query_yn("Allow this fixture coat?")
      local second = ui.query_yn("Allow this fixture coat?")
      return { allowed = first and second, message = "Fixture coat refused." }
    elseif mode == "error" then
      error("ordinary fixture callback error")
    end
    return {
      allowed = ui.query_yn("Allow this fixture coat?"),
      message = "Fixture coat refused.",
    }
  end
  game.add_hook("on_character_try_wear", try_wear)
end

return begin
