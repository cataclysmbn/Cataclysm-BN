local M = {}

---@param params OnCraftResultParams
local choose_color = function(params)
  if params.crafting_menu then return end
  local item = params.item
  if item == nil or not item:has_use( "paint_stuff" ) then return end

  local allcolors = rgb_colors.get_all_named_colors()
  local menu = UiList.new()
  menu:title(locale.gettext("Choose Color"))

  local iter = 0;
  for col, name in pairs(allcolors) do
    menu:add( iter, locale.gettext( name ) )
    iter = iter + 1
  end

  local choice = menu:query()

  if choice < 0 then
    local col = rgb_colors.get_random("")
    item:set_var_col( "PAINT_COLOR", col )
    item:set_var_col( "tint_color", col)
    return
  end

  iter = 0
  for col, name in pairs(allcolors) do
    if iter == choice then
      item:set_var_col( "PAINT_COLOR", col )
      item:set_var_col( "tint_color", col)
      break
    end
    iter = iter + 1
  end
end

---@type fun(params: OnCraftResultParams)
M.on_craft_result = choose_color

return M
