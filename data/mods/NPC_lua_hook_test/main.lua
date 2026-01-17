gdebug.log_info("NPC Lua hook test activated.")

local mod = game.mod_runtime[game.current_mod]

mod.on_dialogue_start = function(params)
  local npc = params.npc
  local next_topic = params.next_topic
  print("Dialogue started with " .. npc.name .. " using topic: " .. next_topic)
  print([[Forcing next topic to be "TALK_FRIEND" instead]])
  return "TALK_FRIEND"
end

mod.on_dialogue_option = function(params)
  local npc = params.npc
  local next_topic = params.next_topic
  print("Dialogue option chosen during chat with " .. npc.name .. ". Next topic would be: " .. next_topic)
  if next_topic ~= "TALK_DONE" and next_topic ~= "TALK_NONE" then
    print([[Forcing next topic to be "TALK_FRIEND_GUARD" instead]])
    return "TALK_FRIEND_GUARD"
  else
    print("Dialogue wishes to end")
  end
end

mod.on_dialogue_end = function(params)
  local npc = params.npc
  print("Dialogue ended with " .. npc.name .. ".")
end
