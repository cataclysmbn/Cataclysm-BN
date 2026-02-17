-- data/mods/change_hairstyle/main.lua

-- Get the current mod's runtime environment
local mod = game.mod_runtime[game.current_mod]

-- This is our main function, called when the player uses the "Change hairstyle" action
-- who: Character object, representing the user (usually the player)
-- item: Item object, representing the item being used (e.g., scissors)
-- pos: Tripoint object, representing the location of use
mod.change_hairstyle_function = function(who, item, pos)
  -- === Step 1: Find the player's current hairstyle (using the new method) ===
  local current_hairstyle_id = nil
  local current_mutations = who:get_mutations(true) -- Get all of the player's mutations

  for _, mut_id in ipairs(current_mutations) do
    local id_str = mut_id:str()
    -- string.match(id_str, "^hair") checks if the string starts with "hair"
    -- The "^" symbol in pattern matching represents "start of the string"
    if id_str:match("^hair") then
      current_hairstyle_id = mut_id -- Found it! Store it
      -- gdebug.log_info("Found current hairstyle: " .. id_str)
      break -- Break the loop once found
    end
  end

  -- === Step 2: Get all available hairstyles in the game (using the new method) ===
  local hairstyle_options = {}
  local all_mutations_raw = MutationBranchRaw.get_all() -- Get all defined mutations in the game

  for _, mut_data in ipairs(all_mutations_raw) do
    local id_str = mut_data.id:str()
    -- Also use string.match to check
    if id_str:match("^hair") then
      -- If it's a hairstyle, add it to our options list
      table.insert(hairstyle_options, {
        id = mut_data.id,
        name = mut_data:name(), -- Get the hairstyle's display name in the game
      })
    end
  end

  -- If no hairstyle options are found, the game data might have changed
  if #hairstyle_options == 0 then
    gapi.add_msg(MsgType.warning, "Error: No hairstyle mutations found in the game data.")
    return 0
  end

  -- === Step 3: Create and display the UI list ===
  local ui = UiList.new()
  ui:title("Please select a new hairstyle:")

  -- Add all hairstyle options to the UI list
  for i, option in ipairs(hairstyle_options) do
    ui:add(i, option.name)
  end

  local choice_index = ui:query() -- Display the UI and wait for the player's choice, returning the selected index

  -- === Step 4: Process the player's choice ===
  if choice_index > 0 then
    local selected_hairstyle = hairstyle_options[choice_index]

    -- Check if the player selected the same hairstyle they already have
    if current_hairstyle_id and current_hairstyle_id:str() == selected_hairstyle.id:str() then
      gapi.add_msg("You decide to keep your current hairstyle.")
      return 0 -- Consumes nothing
    end

    -- If the player already has a hairstyle, remove the old one first
    if current_hairstyle_id then
      who:remove_mutation(current_hairstyle_id, true)
      -- gdebug.log_info("Removed old hairstyle: " .. current_hairstyle_id:str())
    end

    -- Grant the player the new hairstyle mutation
    who:set_mutation(selected_hairstyle.id)
    -- gdebug.log_info("Set new hairstyle: " .. selected_hairstyle.id:str())

    gapi.add_msg(MsgType.good, "You changed your hairstyle!")

    -- Consume item charge and time
    -- You can return a number to represent the amount of charge consumed, e.g., 1
    -- To make it take time, an activity can be added
    who:assign_activity(ActivityTypeId.new("ACT_HAIRCUT"), 18000, -1, -1, "") -- Takes 5 minutes
    return 1
  else
    gapi.add_msg("You decide not to change your hairstyle for now.")
    return 0 -- Consumes nothing
  end
end
