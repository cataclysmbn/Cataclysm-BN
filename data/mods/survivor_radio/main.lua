local survivor_radio = require("./survivor_radio")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.survivor_radio = survivor_radio
survivor_radio.register(mod)

mod.test_spawn_and_erase_npc = function()
  local you = gapi.get_avatar()
  local here = gapi.get_map()
  local pos = you:get_pos_ms()
  local npc = here:place_npc(Point.new(pos.x, pos.y), "radio_bandit")
  if not npc then
    gapi.add_msg("Failed to spawn test NPC.")
    return
  end
  local erase_turn = gapi.current_turn():to_turn() + TimeDuration.from_minutes(1):to_turns()
  mod.queue_npc_erase(npc:getID():get_value(), erase_turn)
  gapi.add_msg("Spawned NPC; will erase in 1 minute.")
end

gapi.register_action_menu_entry({
  id = "bn_test_spawn_erase_npc",
  name = "Test: spawn + erase NPC",
  category = "debug",
  fn = function() return mod.test_spawn_and_erase_npc() end,
})

mod.spawn_debug_party = function()
  if mod.survivor_radio and mod.survivor_radio.debug_spawn_party then mod.survivor_radio.debug_spawn_party() end
end

gapi.register_action_menu_entry({
  id = "bn_debug_spawn_party",
  name = "Test: spawn survivor radio party",
  category = "debug",
  fn = function() return mod.spawn_debug_party() end,
})

mod.queue_npc_erase = function(npc_id, erase_turn)
  local queue = storage.test_npc_erase_queue or {}
  table.insert(queue, { id = npc_id, erase_turn = erase_turn })
  storage.test_npc_erase_queue = queue
end

mod.on_test_npc_erase_tick = function()
  local queue = storage.test_npc_erase_queue or {}
  if #queue == 0 then return end
  local now = gapi.current_turn():to_turn()
  local remaining = {}
  for _, entry in ipairs(queue) do
    if now >= entry.erase_turn then
      local npc = gapi.get_npc_by_id(CharacterId.new(entry.id))
      if npc then npc:erase() end
    else
      table.insert(remaining, entry)
    end
  end
  storage.test_npc_erase_queue = remaining
end

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...) return mod.on_test_npc_erase_tick(...) end)
