local ui = require("lib.ui")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

-- activities
local ACT_READ = mod.cache_static("activities", "ACT_READ", ActivityTypeId)
local ACT_CRAFT = mod.cache_static("activities", "ACT_CRAFT", ActivityTypeId)

-- flags
local flag_ETHEREAL_ITEM = mod.cache_static("flags", "ETHEREAL_ITEM", JsonFlagId)
local flag_TRADER_AVOID = mod.cache_static("flags", "TRADER_AVOID", JsonFlagId)
local flag_MC_USED = mod.cache_static("flags", "MC_USED", JsonFlagId)

--NEVER use <item *> as UID. Just make UID like below.
mod.item_uid = function(item)
  if item:get_var_str("tablet_uid", "nope") == "nope" then
    local uid_str = tostring(gapi.current_turn():to_turn()) .. string.format("%03d", gapi.rng(0, 999))
    item:set_var_str("tablet_uid", uid_str)
  end
  return item:get_var_str("tablet_uid", "nope")
  -- local uid_str = item:get_var_str( "tablet_uid", string.format("%d%03d", gapi.current_turn():to_turn(), gapi.rng(0,999) ) )
end

mod.unzip_var_lib2 = function(storage)
  local book_data = storage:get_var_str("book_data", "")
  local data_list = {}
  if book_data ~= "" then
    for token in string.gmatch(book_data, "([^,]+)") do
      data_list[token] = true
    end
  end
  return data_list
end

---@type fun(storage: Item, str_to_add: string): void
mod.insert_lib2 = function(storage, str_to_add)
  local inserting_book_data = storage:get_var_str("book_data", "")
  if inserting_book_data ~= "" then
    inserting_book_data = inserting_book_data .. "," .. str_to_add
  else
    inserting_book_data = str_to_add
  end
  storage:set_var_str("book_data", inserting_book_data)
end

-- poor person's Object.keys().length
-- https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/keys
---@type fun(tbl: table): integer
mod.table_size = function(tbl)
  local count = 0
  for _, _ in pairs(tbl) do
    count = count + 1
  end
  return count
end

--For colorful UI!
---@type fun(item: Item, ui: UiList): void
mod.ui_coloring = function(item, ui)
  --This is percentage of charges! Lua has no math.round, so I used math.floor with a trick.
  local charge_p = math.floor(item.charges / item:ammo_capacity(false) * 100 + 0.5)
  local color = 0
  ui:title(string.format("%s( %d%%): E-Book mod", item:tname(1, false, 0), charge_p))
  if charge_p == 100 then
    color = Color.c_green
  elseif charge_p >= 75 then
    color = Color.c_light_green
  elseif charge_p >= 50 then
    color = Color.c_yellow
  elseif charge_p >= 25 then
    color = Color.c_light_red
  elseif charge_p <= 0 then
    color = Color.c_red
  end
  ui:title_color(color)
  ui:border_color(color)
end

-- STOP BOTHER ME SAFE_REFERENCE ERROR!!!
mod.item_funeral = function()
  local YOU = gapi.get_avatar()
  local NOW = gapi.current_turn() - gapi.turn_zero()
  gdebug.log_info(string.format("[%d] Call the Requiem for the unmaterialized one.", NOW:to_turns()))
  if storage.hook_assuarance[NOW:to_turns()] and (YOU:has_activity(ACT_READ) or YOU:has_activity(ACT_CRAFT)) then
    -- For more precise, what you read is needed... but I don't know how to get.
    YOU:cancel_activity()
    ui.query_any_key(locale.gettext("Your e-copy is degrading!"))
    gapi.add_msg("You stop what you are doing and watch it vanishes.")
  end
  storage.hook_assuarance[NOW:to_turns()] = nil
  return false
end

-- Defence code of registering twice.
mod.__already_assured = mod.__already_assured or {}
-- This is for on_load_hook. But why is this doubled?
mod.assure_timer_hook = function()
  local NOW = gapi.current_turn() - gapi.turn_zero()
  if storage.hook_assuarance then
    -- gdebug.log_info(string.format("ebook_lua: Data loaded at %d turn", NOW:to_turns()))
    for the_when, val in pairs(storage.hook_assuarance) do
      if not mod.__already_assured[the_when] then
        mod.__already_assured[the_when] = true
        -- Main of assure timer hook.
        if val == "Requiem" then
          local booked_at = TimeDuration.from_turns(the_when)
          gdebug.log_info(string.format("Function [%s]: booked at [%d] turn after.", val, (booked_at - NOW):to_turns()))
          gapi.add_on_every_x_hook(booked_at, mod.item_funeral)
        end
      end
    end
  end
end

--[[
Scanning Process.
1. Check inventory
2. Find a book
3. Check tablet PC
4. Find data of the book
5. If not, store a book
]]
--
---@type fun(user: Character, device: Item): integer
mod.ebook_scan = function(user, device)
  --get all items in your inventory
  local your_items = user:all_items(false)
  local var_unzip = mod.unzip_var_lib2(device)
  local found = {}

  --let's find unscanned books!
  for _, it in ipairs(your_items) do
    --Do the below for each book. But no virtual book!
    if it:is_book() and not it:has_var("aspect") then
      --Oh no. ItypeId is always different at everytime. I should work with string.
      local type_str = it:get_type():str()
      --One book for one type.
      if not var_unzip[type_str] then
        --it:tname( int quantity, bool with_prefix, unsigned int truncate )
        --truncate limits the letters of the name.
        found[type_str] = it:tname(1, false, 0)
      end
    end
  end
  --so, do you have no book?
  if next(found) == nil then
    gapi.add_msg(locale.gettext("You have no book to be scanned."))
    return -1
  else
    local num_found = mod.table_size(found)
    local base_text_parts = {}
    table.insert(
      base_text_parts,
      string.format(locale.gettext("%d books are found. Do you want to scan them all?\n"), num_found)
    )

    local ui_limit = 20
    local dev_limit = math.floor(device.charges / 10)
    local hit_end = num_found > dev_limit

    local ui_scan = UiList.new()
    mod.ui_coloring(device, ui_scan)
    local count = 0
    for _, n_str in pairs(found) do
      if count >= ui_limit then
        table.insert(base_text_parts, string.format(locale.gettext("\n...And more! (%d)"), num_found - count))
        break
      end
      table.insert(base_text_parts, string.format("\n%s", n_str)) -- Use table.insert for building lists of strings
      count = count + 1
    end
    local base_text = table.concat(base_text_parts) -- Use table.concat to join the strings

    ui_scan:text(base_text)
    ui_scan:desc_enabled(true)
    ui_scan:add_w_desc(-1, locale.gettext("Scan all"), locale.gettext("Scan all the books until charges going off."))
    ui_scan:add_w_desc(-1, locale.gettext("Cancel"), locale.gettext("Discard the book list and go back."))
    while true do
      local sel_scan = ui_scan:query()
      if sel_scan == 0 then
        if hit_end then
          local YN = ui.query_yn(locale.gettext("WARNING: Not enough battery to scan all books.\nProceed anyway?"))
          if YN then
            local count2 = 0
            for t_str, _ in pairs(found) do
              mod.insert_lib2(device, t_str)
              count2 = count2 + 1
              if count2 >= dev_limit then break end
            end
            gapi.add_msg(MsgType.good, string.format(locale.gettext("You scanned %d book(s)."), dev_limit))
            return dev_limit * 10
          else --selected NO
            return -1
          end
        else
          for t_str, _ in pairs(found) do
            mod.insert_lib2(device, t_str)
          end
          gapi.add_msg(MsgType.good, string.format(locale.gettext("You scanned %d book(s)."), num_found))
          return num_found * 10
        end
      else --selected Cancel or pressed ESC
        return -1
      end
    end
  end
end

--[[
Loading Process.
1. Check data of tablet PC
2. Make UiList of book data
3. Select one of them
4. ~~avatar::read runs!~~ <-- NO!
5. Spawn book but virtual.
]]
--
---@type fun(reader: Character, device: Item): integer
mod.ebook_load = function(reader, device)
  local ui_load = UiList.new()
  local tmp_arr = {}
  local lifetime = 60
  mod.ui_coloring(device, ui_load)
  ui_load:text(locale.gettext("Choose the book you want to get."))
  local unzip_var = mod.unzip_var_lib2(device)
  for type_str, _ in pairs(unzip_var) do
    local itype_id = ItypeId.new(type_str)
    local entry = {}
    entry.type = itype_id
    entry.name = gapi.create_item(itype_id, 1):tname(1, false, 32)
    table.insert(tmp_arr, entry)
    ui_load:add(-1, entry.name)
  end

  local minute = 0
  local sel = 0
  local u_input_too_big_minute = false
  local puip_str = PopupInputStr.new()
  puip_str:desc(locale.gettext("How long time do you need?(Digits only)"))
  puip_str:title(string.format(locale.gettext("Input the minute( up to %d ): "), device.charges - 10))
  -- Repeat when getting minute more than 0 AND less than charges-10.
  -- If you cancel to input number, minute is 0, and it will get back and show you data select menu.
  while true do
    if not u_input_too_big_minute then sel = ui_load:query() end
    u_input_too_big_minute = false
    if sel < 0 then
      --You canceled to select the book.
      return -1
    else
      minute = puip_str:query_int()
      if minute <= 0 then
        --You canceled to input minute.
        --Go back to select book menu.
      elseif minute > device.charges - 10 then
        --You input too big. Try again.
        ui.query_any_key(locale.gettext("Input value is over than available charges.\nPlease check your input."))
        u_input_too_big_minute = true
      else
        local selected = tmp_arr[sel + 1]
        --You got the book on the real world!
        reader:add_item_with_id(selected.type, 1)
        --Then lemme see it.
        local book_in_real
        for _, it in pairs(reader:all_items(false)) do
          if it:get_type() == selected.type then
            if it:get_var_str("aspect", "real") == "real" then
              book_in_real = it
              break
            end
          end
        end

        --The book should be labeled.
        book_in_real:set_var_str(
          "item_label",
          string.format(locale.gettext("[e-copy] %s"), string.gsub(selected.name, " %(unread%)", ""))
        ) -- no more "X (unread) (unread)"
        book_in_real:set_var_str("aspect", "virtual") --real, actual, virtual.
        --The book should have no weight nor volume.
        book_in_real:set_var_num("weight", 0)
        book_in_real:set_var_num("volume", 0)
        --The book should be gone.
        book_in_real:set_flag(flag_ETHEREAL_ITEM)
        lifetime = lifetime * minute + 1
        book_in_real:set_var_num("ethereal", lifetime)
        --The book should be untradable. But it looks not effective.
        book_in_real:set_flag(flag_TRADER_AVOID)
        --The book should be no charges. Wait, what? Did it spawn with charges? It may be a bug from character::add_item_with_id.
        if book_in_real.charges ~= 0 then
          gdebug.log_info(
            string.format("%s has %d charge(s). Why does it happen?", selected.type:str(), book_in_real.charges)
          )
          book_in_real.charges = 0
        end

        local the_when = gapi.current_turn() + TimeDuration.from_turns(lifetime - 1)
        book_in_real:set_var_num("its_fate_time", the_when:to_turn())
        mod.turntimer_hook(lifetime - 1, mod.item_funeral)

        local msg = string.format(
          locale.gettext(
            "You printed a physical copy of %s.\nIt’s virtually weightless and will degrade naturally in about \n[%d minutes.]\nYou can return it to the device to recover some energy."
          ),
          selected.name,
          TimeDuration.from_turns(lifetime):to_minutes()
        )
        ui.query_any_key(msg)
        reader:mod_moves(-100)
        return minute
      end
    end
  end
end

---@type fun(reader: Character, device: Item): integer
mod.ebook_return = function(reader, device)
  --let's find the books
  local your_items = reader:all_items(false)
  local virtual_books = {}
  for _, it in ipairs(your_items) do
    if it:has_var("aspect") then table.insert(virtual_books, it) end
  end
  if #virtual_books <= 0 then
    ui.query_any_key(locale.gettext("There is no book to return."))
    return -1
  end
  local letter_limit = 24
  local ui_return = UiList.new()
  mod.ui_coloring(device, ui_return)
  ui_return:text("Choose the book to return.")
  for key, it in ipairs(virtual_books) do
    ui_return:add_w_col(
      key,
      it:tname(1, false, letter_limit),
      "NO_DESC",
      string.format("+%d%%", math.floor(it:get_var_num("ethereal", 0) / 60 / device:ammo_capacity(false) * 100))
    )
  end
  local sel_return = ui_return:query()
  if sel_return <= 0 then
    return -1
  else
    local it = virtual_books[sel_return]
    local ammo_type = device:ammo_current()
    device:ammo_set(ammo_type, device.charges + math.floor(it:get_var_num("ethereal", 0) / 60))
    local the_when = math.floor(it:get_var_num("its_fate_time", 0))
    storage.hook_assuarance[the_when] = nil
    it:set_var_num("ethereal", 0)
    --DON'T READ VANISHING ITEM EVER!!!
    reader:mod_moves(-250)
    return 0
  end
end

---@type fun(reader: Character, device: Item): integer
mod.check_lib = function(reader, device)
  local uid = mod.item_uid(device)
  local book_data = mod.unzip_var_lib2(device)

  if next(book_data) == nil then
    ui.query_any_key(locale.gettext("There is no book in the device."))
    return -1
  end

  local book_data_items = {}
  for k_ity_str, _ in pairs(book_data) do
    table.insert(book_data_items, gapi.create_item(ItypeId.new(k_ity_str), 1):tname(1, false, 0))
  end
  local book_data_list_str = table.concat(book_data_items, "\n")
  ui.query_any_key(
    string.format(locale.gettext("UID:%s\n\nThis device has book data as below:\n%s"), uid, book_data_list_str)
  )
  return -1
end

---@type fun(reader: Character, device: Item): integer
mod.mc_io = function(reader, device)
  local uid = device:get_var_str("tablet_uid", "nope")
  local your_mc = {}
  local your_items = reader:all_items(false)
  --Let's make your_mc!
  for _, it in ipairs(your_items) do
    if it:has_flag(flag_MC_USED) then table.insert(your_mc, it) end
  end
  -- Escape when no mc.
  if #your_mc == 0 then
    ui.query_any_key(locale.gettext("You don't have any empty memory card."))
    return -1
  end
  -- Now we have your_mc table.
  local mc_sel_ui = UiList.new()
  mod.ui_coloring(device, mc_sel_ui)
  mc_sel_ui:text(string.format(locale.gettext("UID: %s\n\nSelect a memory card."), uid))
  for k_mc, v_mc in ipairs(your_mc) do
    mc_sel_ui:add(k_mc, string.format("%s", v_mc:tname(1, false, 0)))
  end
  while true do
    -- Refresh names in case card names changed (e.g. after upload)
    for k_num, entry in ipairs(mc_sel_ui.entries) do -- Assuming entries is a sequence
      if your_mc[k_num] then -- Guard against potential issues if mc_sel_ui.entries and your_mc get out of sync
        entry.txt = your_mc[k_num]:tname(1, false, 0)
      end
    end
    local ans_mc_sel = mc_sel_ui:query()
    if ans_mc_sel <= 0 then
      return -1
    else
      local mc_menu_ui = UiList.new()
      mc_menu_ui:desc_enabled(true)
      mod.ui_coloring(device, mc_menu_ui)
      mc_menu_ui:add_w_desc(-1, "List book data", "Show the book data list from memory card.")
      mc_menu_ui:add_w_desc(-1, "Download book data", "Download book data from memory card.")
      mc_menu_ui:add_w_desc(-1, "Upload book data", "Upload book data to memory card.")
      mc_menu_ui:add_w_desc(-1, "Remove all book data", "Remove all book data of memory card.")
      while true do
        local that_mc = your_mc[ans_mc_sel]
        mc_menu_ui:text(
          string.format(
            locale.gettext("UID: %s\n\nMemory card data I/O online.\nCurrent card: %s"),
            uid,
            that_mc:tname(1, false, 0)
          )
        )
        local data_in_dev = mod.unzip_var_lib2(device)
        local data_in_card = mod.unzip_var_lib2(that_mc)
        local ans_mc_menu = mc_menu_ui:query()
        if ans_mc_menu < 0 then
          break
        elseif ans_mc_menu == 0 then --List
          local book_data_mc_items = {}
          for k_ity_str, _ in pairs(data_in_card) do
            table.insert(book_data_mc_items, gapi.create_item(ItypeId.new(k_ity_str), 1):tname(1, false, 0))
          end
          local book_data_mc_text = table.concat(book_data_mc_items, "\n")
          ui.query_any_key(
            string.format(locale.gettext("Book data of %s:\n%s"), that_mc:tname(1, false, 0), book_data_mc_text)
          )
        elseif ans_mc_menu == 1 then --Download
          local dl_count = 0
          for type_str, _ in pairs(data_in_card) do
            if not data_in_dev[type_str] then
              mod.insert_lib2(device, type_str)
              dl_count = dl_count + 1
            end
          end
          if dl_count == 0 then
            ui.query_any_key(string.format(locale.gettext("%s has nothing new books."), that_mc:tname(1, false, 0)))
          else
            ui.query_any_key(
              string.format(locale.gettext("%1$d books downloaded from %2$s."), dl_count, that_mc:tname(1, false, 0))
            )
          end
        elseif ans_mc_menu == 2 then --Upload
          local ul_count = 0
          for key, _ in pairs(data_in_dev) do
            if not data_in_card[key] then
              mod.insert_lib2(that_mc, key)
              ul_count = ul_count + 1
            end
          end
          if ul_count == 0 then
            ui.query_any_key(
              string.format(locale.gettext("%s has the same books already."), that_mc:tname(1, false, 0))
            )
          else
            ui.query_any_key(
              string.format(
                locale.gettext("%1$d books uploaded on %2$s.\nNaming the card is recommended."),
                ul_count,
                that_mc:tname(1, false, 0)
              )
            )
            that_mc:set_var_str("name", locale.gettext("e-book saved SD-Memory Card"))
          end
        elseif ans_mc_menu == 3 then --Remove all
          if not that_mc:has_var("book_data") then
            ui.query_any_key(locale.gettext("It has no book data."))
          else
            local yn1 = ui.query_yn(
              string.format(
                locale.gettext("Do you want to delete all of book data of %s? It's irretrievable!"),
                that_mc:tname(1, false, 0)
              )
            )
            if yn1 then
              local yn2 = ui.query_yn(locale.gettext("Are you sure to delete book data in the card?"))
              if yn2 then
                that_mc:erase_var("name")
                that_mc:erase_var("book_data")
                ui.query_any_key(locale.gettext("The memory card is reset successfully."))
                break
              end
            end
          end
        end
      end
    end
  end
end

-- cloud sync. but that never works in this world.
---@type fun(reader: Character, device: Item): integer
mod.cloud_sync = function(reader, device)
  ui.query_any_key(
    locale.gettext(
      "ERROR: Network Connection Failed\n\nConnection denied by Protocol Zero. Server access is temporarily suspended.\nPlease wait for further updates."
    )
  )
  return -1
end

-- Reset library. Double caution.
---@type fun(reader: Character, device: Item): integer
mod.reset_lib = function(reader, device)
  local yn1 = ui.query_yn(
    string.format(
      locale.gettext("Do you want to delete all of book data of %s? It's irretrievable!"),
      device:tname(1, false, 0)
    )
  )
  if yn1 == "YES" then
    local yn2 = ui.query_yn(locale.gettext("Are you sure to delete book data in the device?"))
    if yn2 == "YES" then
      device:erase_var("book_data")
      ui.query_any_key(locale.gettext("The device is reset successfully."))
      return 0
    end
  end
  return -1
end

---@type fun(reader: Character, device: Item): integer
mod.ebook_info = function(reader, device)
  local uid = mod.item_uid(device)
  local ui_info = UiList.new()
  mod.ui_coloring(device, ui_info)
  ui_info:desc_enabled(true)
  ui_info:add_w_desc(-1, "Check library", "Check the book data the device has.")
  ui_info:add_w_desc(-1, "Memory card menu", "Operate book data with memory card.")
  ui_info:add_w_desc(-1, "Cloud sync", "Read / Write data from cloud server. It needs network connection.")
  ui_info:add_w_desc(-1, "Reset library", "Delete ALL book data in this device.")
  while true do
    local day = (gapi.current_turn() - gapi.turn_zero()):to_days() + 3
    ui_info:text(
      string.format(
        locale.gettext("UID: %s\n\nNetwork: Offline (Cloud unreachable)\n\nLast Sync: %d days ago"),
        uid,
        day
      )
    )
    local ans_info = ui_info:query()
    if ans_info < 0 then
      return -1
    elseif ans_info == 0 then
      local ch_lib = mod.check_lib(reader, device)
      if ch_lib ~= -1 then return ch_lib end
    elseif ans_info == 1 then
      local mc_io = mod.mc_io(reader, device)
      if mc_io ~= -1 then return mc_io end
    elseif ans_info == 2 then
      local clo_syn = mod.cloud_sync(reader, device)
      if clo_syn ~= -1 then return clo_syn end
    elseif ans_info == 3 then
      local res_lib = mod.reset_lib(reader, device)
      if res_lib ~= -1 then return res_lib end
    end
  end
end

-- The first action for this Lua
---@type fun(who: Character, item: Item, pos: Tripoint): integer
mod.ebook_ui = function(who, item, pos)
  local unzip_var = mod.unzip_var_lib2(item)
  local var_count = mod.table_size(unzip_var)
  local uilist = UiList.new()
  uilist:desc_enabled(true)
  uilist:text(
    string.format(
      locale.gettext("Welcome to e-book library!\nThis device currently holds %d book(s).\nNetwork sync unavailable."),
      var_count
    )
  )
  uilist:add_w_desc(
    -1,
    locale.gettext("Scan book(s)"),
    locale.gettext("Scan all the books you have. It will progress instantly.")
  )
  uilist:add_w_desc(
    -1,
    locale.gettext("Load book(s)"),
    locale.gettext("Temporarily prints a physical copy from the device’s library.")
  )
  uilist:add_w_desc(
    -1,
    locale.gettext("Return book(s)"),
    locale.gettext("Absorbs the copy back into the device and restores partial power.")
  )
  uilist:add_w_desc(-1, locale.gettext("Info.."), locale.gettext("Shows UID, list of data, explanation for customers!"))

  if var_count == 0 then
    uilist.entries[2].ctxt = locale.gettext("No book in device!")
    uilist.entries[2].enable = false
  end

  local actions = {
    [0] = mod.ebook_scan,
    [1] = mod.ebook_load,
    [2] = mod.ebook_return,
    [3] = mod.ebook_info,
  }

  while true do
    mod.ui_coloring(item, uilist)
    local ans = uilist:query()
    if ans < 0 then
      gapi.add_msg("Never mind.")
      return 0
    end

    local action_func = actions[ans]
    if action_func then
      local result = action_func(who, item)
      if result ~= -1 then return result end
    else
      gdebug.log_info("ebook_lua: Error occurred. What the heck did you select?")
      return 0
    end
  end
end
