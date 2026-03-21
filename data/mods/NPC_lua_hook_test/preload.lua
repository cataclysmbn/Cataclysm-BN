gdebug.log_info("LUA HOOKS TEST: PRELOAD ONLINE")

local mod = game.mod_runtime[game.current_mod]

mod.triggered = false
mod.handle_testing = function(params)
  if mod.triggered then return end
  mod.triggered = true
  local test = UiList.new()
  test:title("Choose Current Test")
  test:add_w_desc(1, "Dialogue Tree", "Dialogue start and options will get funky and log results in the lua console.")
  test:add_w_desc(2, "Interaction Check", "You will be prompted to prevent an interaction with an NPC from activating.")
  test:add_w_desc(
    3,
    "Force Talk",
    "When interacting with an npc, you will bypass the npc_menu and immediately being talking."
  )
  test:add_w_desc(4, "Notify", "When interacting with an npc, it will be logged.")
  test = test:query()
  print(test)
  if test == 1 then
    game.add_hook("on_dialogue_start", function(...) return mod.on_dialogue_start(...) end)
    game.add_hook("on_dialogue_option", function(...) return mod.on_dialogue_option(...) end)
    game.add_hook("on_dialogue_end", function(...) return mod.on_dialogue_end(...) end)
  elseif test == 2 then
    game.add_hook("on_try_npc_interaction", function(...) return mod.on_try_npc_interaction(...) end)
  elseif test == 3 then
    game.add_hook("on_try_npc_interaction", function(...) return mod.force_talk(...) end)
  elseif test == 4 then
    game.add_hook("on_npc_interaction", function(...) return mod.on_npc_interaction(...) end)
  end
end

game.add_hook("on_game_started", function(...) return mod.handle_testing(...) end)
game.add_hook("on_game_load", function(...) return mod.handle_testing(...) end)
