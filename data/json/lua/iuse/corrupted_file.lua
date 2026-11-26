local M = {}
local storage = game.mod_storage[game.current_mod]
local ghost_id = MonsterTypeId.new("mon_signal_maw")

---@param params ItemUseParams
function M.open(params)
  local file_id = params.item:get_var_str("DIGITAL_FILE_ID", "")
  if file_id == "" then
    gapi.add_msg(MsgType.info, locale.gettext("The file cannot be opened.  Try importing it again."))
    return 0
  end

  storage.spent_corrupted_files = storage.spent_corrupted_files or {}
  if storage.spent_corrupted_files[file_id] then
    gapi.add_msg(MsgType.info, locale.gettext("The preview is gone.  There is only static."))
    return 0
  end

  local creature = gapi.place_monster_around(ghost_id, params.user:get_pos_ms(), 1)
  if not creature then
    gapi.add_msg(
      MsgType.warning,
      locale.gettext("A pale figure presses against the screen from the inside, but cannot find room to emerge.")
    )
    return 0
  end

  storage.spent_corrupted_files[file_id] = true
  creature.friendly = 0
  params.user:mod_moves(-100)
  gapi.add_msg(
    MsgType.bad,
    locale.gettext(
      "A pale face flashes across the screen.  A wavering human shape pulls itself free, trailing static.  The device falls silent.  The corrupted file is gone."
    )
  )
  return 1
end

return M
