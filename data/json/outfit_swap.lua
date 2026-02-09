local outfit_swap = {}

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

  local worn = who:get_worn_items()
  for i = 1, #worn do
    local detached = who:remove_worn(worn[i])
    if detached then
      local leftover = map:add_item(target, detached)
      if leftover then
        gapi.add_msg("Couldn't drop a worn item on the tile.")
      end
    end
  end

  for i = 1, #items do
    if who:can_wear(items[i], true) then
      local detached = map:detach_item_at(target, items[i])
      if detached then
        local worn_ok = who:wear_detached(detached, false)
        if not worn_ok then
          local leftover = map:add_item(target, detached)
          if leftover then
            gapi.add_msg("Couldn't return an item to the tile.")
          end
        end
      end
    end
  end

  gapi.add_msg("Outfit swap complete.")
  return 1
end

outfit_swap.menu = outfit_swap.swap_outfit

return outfit_swap
