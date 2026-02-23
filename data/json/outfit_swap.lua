local outfit_swap = {}

local function restore_outfit(who, items, map, target)
  for i = #items, 1, -1 do
    local restored = who:wear_detached(items[i], false)
    if not restored then
      local leftover = map:add_item(target, items[i])
      if leftover then gapi.add_msg("Couldn't restore an item to the tile.") end
    end
  end
end

outfit_swap.swap_outfit = function()
  local who = gapi.get_avatar()
  local target = gapi.look_around()
  if not target then
    gapi.add_msg("No target tile selected.")
    return 1
  end

  local map = gapi.get_map()
  local stack = map:get_items_at(target)
  local items = stack:items()
  if #items == 0 then
    gapi.add_msg("No items to swap.")
    return 1
  end

  local wearable = {}
  for i = 1, #items do
    if who:can_wear(items[i], true) then table.insert(wearable, items[i]) end
  end
  if #wearable == 0 then
    gapi.add_msg("No wearable items to swap with.")
    return 1
  end

  local worn = who:get_worn_items()
  local removed = {}

  for i = 1, #worn do
    local detached = who:remove_worn(worn[i])
    if not detached then
      gapi.add_msg("You can't take that off right now.")
      restore_outfit(who, removed, map, target)
      return 1
    end
    table.insert(removed, detached)
  end

  for i = 1, #wearable do
    local detached = map:detach_item_at(target, wearable[i])
    if detached then
      local wear_failed = who:wear_detached(detached, true)
      -- wear_detached returns false on success because the owned pointer is consumed
      if wear_failed then
        local leftover = map:add_item(target, detached)
        if leftover then gapi.add_msg("Couldn't return an item to the tile.") end
        restore_outfit(who, removed, map, target)
        return 1
      end
    end
  end

  for i = 1, #removed do
    local leftover = map:add_item(target, removed[i])
    if leftover then gapi.add_msg("Couldn't drop a worn item on the tile.") end
  end

  gapi.add_msg("Outfit swap complete.")
  return 1
end

outfit_swap.menu = outfit_swap.swap_outfit

return outfit_swap
