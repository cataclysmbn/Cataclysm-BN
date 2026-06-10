local ebook_reader = {}

local ACT_READ_EBOOK = ActivityTypeId.new("ACT_READ_EBOOK")
local ACT_SCAN_EBOOK = ActivityTypeId.new("ACT_SCAN_EBOOK")
local UPS_ID = ItypeId.new("UPS")
local FLAG_USE_UPS = JsonFlagId.new("USE_UPS")
local VAR_STORED_BOOKS = "EBOOK_STORED_BOOKS"
local VAR_LEGACY_STORED_BOOKS = "book_data"
local VAR_STORED_RECIPES = "EBOOK_STORED_RECIPES"
local VAR_LEGACY_RECIPES = "EIPC_RECIPES"
local VAR_ACTIVITY_TOKEN = "EBOOK_ACTIVITY_TOKEN"
local VAR_ACTIVITY_CONSUMED = "EBOOK_ACTIVITY_CONSUMED"
local READ_FINISH_CALLBACK = "EBOOK_FINISH_READING"
local READ_TURN_CALLBACK = "EBOOK_CHECK_READING"
local SCAN_FINISH_CALLBACK = "EBOOK_FINISH_SCANNING"
local SCAN_TURN_CALLBACK = "EBOOK_CHECK_SCANNING"
local CHARGE_INTERVAL_MINUTES = 5

---@param value string
---@return string[]
local function split_ids(value)
  local result = {}
  for token in string.gmatch(value or "", "([^,]+)") do
    if token ~= "" then table.insert(result, token) end
  end
  return result
end

---@param values string[]
---@return string
local function join_ids(values)
  return table.concat(values, ",")
end

---@param values string[]
---@param value string
---@return boolean
local function contains(values, value)
  for _, stored in ipairs(values) do
    if stored == value then return true end
  end
  return false
end

---@param item Item
---@return string
local function item_type_id(item)
  return item:get_type():str()
end

---@param item Item
---@return string
local function item_name(item)
  return item:tname(1, false, 0)
end

---@param book_id string
---@return string
local function stored_book_name(book_id)
  local id = ItypeId.new(book_id)
  if not id:is_valid() then return book_id end
  local book = Item.spawn(id, 1)
  return book:tname(1, false, 0)
end

---@param device Item
---@return string[]
local function stored_books(device)
  local result = split_ids(device:get_var_str(VAR_STORED_BOOKS, ""))
  for _, book_id in ipairs(split_ids(device:get_var_str(VAR_LEGACY_STORED_BOOKS, ""))) do
    if not contains(result, book_id) then table.insert(result, book_id) end
  end
  return result
end

---@param device Item
---@return boolean
local function is_ebook_device(device)
  return device:get_type():obj():can_use("EBOOK_READER")
end

---@param device Item
---@param book_id string
---@return boolean
local function has_stored_book(device, book_id)
  return contains(stored_books(device), book_id)
end

---@param duration TimeDuration
---@return integer
local function charge_intervals_for_duration(duration)
  return math.max(1, math.ceil(duration:to_minutes() / CHARGE_INTERVAL_MINUTES))
end

---@param device Item
---@param duration TimeDuration
---@return integer
local function charge_cost_for_duration(device, duration)
  return charge_intervals_for_duration(duration) * math.max(1, device:get_type():obj():charges_to_use())
end

---@param who Character
---@param device Item
---@param charges integer
---@return boolean
local function has_device_charges(who, device, charges)
  local internal_charges = device:ammo_remaining()
  if internal_charges >= charges then return true end
  return device:has_flag(FLAG_USE_UPS) and who:has_charges(UPS_ID, charges - internal_charges)
end

---@param who Character
---@param device Item
---@param charges integer
---@return boolean
local function drain_device_charges(who, device, charges)
  if not has_device_charges(who, device, charges) then return false end
  local internal_charges = math.min(charges, device:ammo_remaining())
  if internal_charges > 0 then device:ammo_consume(internal_charges, who:bub_pos()) end
  local ups_charges = charges - internal_charges
  return ups_charges == 0 or who:use_charges_if_avail(UPS_ID, ups_charges)
end

---@param kind string
---@param book_id string
---@return string
local function activity_token(kind, book_id)
  return string.format("%s:%s:%s:%09d", kind, book_id, tostring(gapi.current_turn():to_turn()), gapi.rng(0, 999999999))
end

---@param device Item
---@param token string
---@return nil
local function mark_activity_device(device, token)
  device:set_var_str(VAR_ACTIVITY_TOKEN, token)
  device:set_var_num(VAR_ACTIVITY_CONSUMED, 0)
end

---@param device Item
---@return nil
local function clear_activity_device(device)
  device:erase_var(VAR_ACTIVITY_TOKEN)
  device:erase_var(VAR_ACTIVITY_CONSUMED)
end

---@param device Item
---@param book_id string
---@return nil
local function add_stored_book(device, book_id)
  local books = stored_books(device)
  if contains(books, book_id) then return end
  table.insert(books, book_id)
  device:set_var_str(VAR_STORED_BOOKS, join_ids(books))
end

---@param who Character
---@param book_id string
---@return Item?
local function find_device_with_book(who, book_id)
  for _, item in ipairs(who:all_items(false)) do
    if is_ebook_device(item) and has_stored_book(item, book_id) then return item end
  end
  return nil
end

---@param who Character
---@param token string
---@return Item?
local function find_device_by_token(who, token)
  for _, item in ipairs(who:all_items(false)) do
    if is_ebook_device(item) and item:get_var_str(VAR_ACTIVITY_TOKEN, "") == token then return item end
  end
  return nil
end

---@param who Character
---@param book_id string
---@return boolean
local function has_inventory_book(who, book_id)
  for _, item in ipairs(who:all_items(false)) do
    if item:is_book() and item_type_id(item) == book_id then return true end
  end
  return false
end

---@type fun(who: Character, device: Item, book: Item): boolean
local start_scanning_book

---@param who Character
---@return Item[]
local function inventory_books(who)
  local result = {}
  for _, item in ipairs(who:all_items(false)) do
    if item:is_book() then table.insert(result, item) end
  end
  return result
end

---@param who Character
---@param device Item
---@return integer
local function store_book(who, device)
  local choices = inventory_books(who)
  if #choices == 0 then
    gapi.add_msg(MsgType.info, locale.gettext("You do not have any books to store."))
    return 0
  end

  local menu = UiList.new()
  menu:title(locale.gettext("Store which book?"))
  for index, book in ipairs(choices) do
    menu:add(index, item_name(book))
  end
  local choice = menu:query()
  local selected = choices[choice]
  if not selected then return 0 end

  local book_id = item_type_id(selected)
  if has_stored_book(device, book_id) then
    gapi.add_msg(MsgType.info, string.format(locale.gettext("%s is already stored."), item_name(selected)))
    return 0
  end

  start_scanning_book(who, device, selected)
  return 0
end

---@param avatar Avatar
---@param book_id string
---@return boolean
local function report_book_denials(avatar, book_id)
  local id = ItypeId.new(book_id)
  local denials = avatar:read_book_type_denials(id)
  if #denials == 0 then
    gapi.add_msg(MsgType.bad, locale.gettext("You cannot read that right now."))
  else
    for _, reason in ipairs(denials) do
      gapi.add_msg(MsgType.bad, reason)
    end
  end
  return false
end

---@param params { user: Character, activity: PlayerActivity, data: { token: string, charge_cost: integer } }
---@param finish boolean
---@return boolean
local function drain_activity_charges(params, finish)
  local device = find_device_by_token(params.user, params.data.token)
  if not device then return false end
  local charge_cost = params.data.charge_cost or 0
  if charge_cost <= 0 then return true end
  local consumed = math.floor(device:get_var_num(VAR_ACTIVITY_CONSUMED, 0))
  local due = charge_cost
  if not finish then
    local total_moves = math.max(1, params.activity.moves_total)
    local progressed_moves = math.max(0, total_moves - params.activity.moves_left)
    due = math.floor(progressed_moves * charge_cost / total_moves)
  end
  if due <= consumed then return true end
  if not drain_device_charges(params.user, device, due - consumed) then return false end
  device:set_var_num(VAR_ACTIVITY_CONSUMED, due)
  return true
end

---@param who Character
---@param device Item
---@param book_id string
---@param charge_cost integer
---@param kind string
---@return string?
local function prepare_activity_device(who, device, book_id, charge_cost, kind)
  if not has_device_charges(who, device, charge_cost) then
    local message = locale.gettext("Your %s needs %d charge to read %s.")
    if kind == "scan" then message = locale.gettext("Your %s needs %d charge to scan %s.") end
    gapi.add_msg(MsgType.bad, string.format(message, item_name(device), charge_cost, stored_book_name(book_id)))
    return nil
  end
  local token = activity_token(kind, book_id)
  mark_activity_device(device, token)
  return token
end

---@param who Character
---@param device Item
---@param book Item
---@return boolean
function start_scanning_book(who, device, book)
  local avatar = who:as_avatar()
  if not avatar then return false end

  local book_id = item_type_id(book)
  local id = ItypeId.new(book_id)
  if not avatar:can_read_book_type(id) then return report_book_denials(avatar, book_id) end

  local duration = avatar:time_to_read_book_type(id)
  local charge_cost = charge_cost_for_duration(device, duration)
  local token = prepare_activity_device(who, device, book_id, charge_cost, "scan")
  if not token then return false end

  avatar:assign_lua_activity({
    type = ACT_SCAN_EBOOK,
    duration = duration,
    on_finish = SCAN_FINISH_CALLBACK,
    on_turn = SCAN_TURN_CALLBACK,
    name = string.format(locale.gettext("scanning %s"), item_name(book)),
    data = { book_id = book_id, token = token, charge_cost = charge_cost },
  })
  gapi.add_msg(
    MsgType.info,
    string.format(locale.gettext("You start scanning %s into %s."), item_name(book), item_name(device))
  )
  return true
end

---@param who Character
---@param book_id string
---@return boolean
local function start_reading_book(who, book_id)
  local avatar = who:as_avatar()
  if not avatar then return false end

  local device = find_device_with_book(who, book_id)
  if not device then
    gapi.add_msg(MsgType.bad, locale.gettext("That stored book is no longer available."))
    return false
  end

  local id = ItypeId.new(book_id)
  if not avatar:can_read_book_type(id) then return report_book_denials(avatar, book_id) end

  local duration = avatar:time_to_read_book_type(id)
  local charge_cost = charge_cost_for_duration(device, duration)
  local token = prepare_activity_device(who, device, book_id, charge_cost, "read")
  if not token then return false end

  avatar:assign_lua_activity({
    type = ACT_READ_EBOOK,
    duration = duration,
    on_finish = READ_FINISH_CALLBACK,
    on_turn = READ_TURN_CALLBACK,
    name = string.format(locale.gettext("reading %s"), stored_book_name(book_id)),
    data = { book_id = book_id, token = token, charge_cost = charge_cost },
  })
  return true
end

---@param params ItemUseParams
---@return integer
function ebook_reader.menu(params)
  return store_book(params.user, params.item)
end

---@param params { user: Character, activity: PlayerActivity, data: { book_id: string, token: string, charge_cost: integer } }
---@return nil
function ebook_reader.check_reading(params)
  local device = find_device_by_token(params.user, params.data.token)
  if not device or not has_stored_book(device, params.data.book_id) then
    gapi.add_msg(MsgType.bad, locale.gettext("You stop reading because the stored book is no longer available."))
    params.user:cancel_activity()
    return
  end
  if not drain_activity_charges(params, false) then
    gapi.add_msg(MsgType.bad, locale.gettext("You stop reading because the e-book reader's batteries are dead."))
    params.user:cancel_activity()
  end
end

---@param params { user: Character, activity: PlayerActivity, data: { book_id: string, token: string, charge_cost: integer } }
---@return nil
function ebook_reader.finish_reading(params)
  local avatar = params.user:as_avatar()
  if not avatar then return end
  local device = find_device_by_token(params.user, params.data.token)
  if not device or not has_stored_book(device, params.data.book_id) then
    gapi.add_msg(MsgType.bad, locale.gettext("You cannot finish reading because the stored book is no longer available."))
    return
  end
  if not drain_activity_charges(params, true) then
    gapi.add_msg(MsgType.bad, locale.gettext("You cannot finish reading because the e-book reader's batteries are dead."))
    return
  end
  clear_activity_device(device)
  avatar:finish_reading_book_type(ItypeId.new(params.data.book_id))
end

---@param params { user: Character, activity: PlayerActivity, data: { book_id: string, token: string, charge_cost: integer } }
---@return nil
function ebook_reader.check_scanning(params)
  local device = find_device_by_token(params.user, params.data.token)
  if not device or not has_inventory_book(params.user, params.data.book_id) then
    gapi.add_msg(MsgType.bad, locale.gettext("You stop scanning because the device or book is no longer available."))
    params.user:cancel_activity()
    return
  end
  if not drain_activity_charges(params, false) then
    gapi.add_msg(MsgType.bad, locale.gettext("You stop scanning because the e-book reader's batteries are dead."))
    params.user:cancel_activity()
  end
end

---@param params { user: Character, activity: PlayerActivity, data: { book_id: string, token: string, charge_cost: integer } }
---@return nil
function ebook_reader.finish_scanning(params)
  local device = find_device_by_token(params.user, params.data.token)
  if not device or not has_inventory_book(params.user, params.data.book_id) then
    gapi.add_msg(MsgType.bad, locale.gettext("You cannot finish scanning because the device or book is no longer available."))
    return
  end
  if not drain_activity_charges(params, true) then
    gapi.add_msg(MsgType.bad, locale.gettext("You cannot finish scanning because the e-book reader's batteries are dead."))
    return
  end
  add_stored_book(device, params.data.book_id)
  clear_activity_device(device)
  gapi.add_msg(MsgType.good, string.format(locale.gettext("Stored %s."), stored_book_name(params.data.book_id)))
end

---@param results table
---@param recipe_id RecipeId
---@param difficulty integer
---@return nil
local function add_available_recipe(results, recipe_id, difficulty)
  results[#results + 1] = { recipe = recipe_id, difficulty = difficulty }
end

---@param results table
---@param recipes string[]
---@return nil
local function add_stored_recipes(results, recipes)
  for _, id in ipairs(recipes) do
    local recipe_id = RecipeId.new(id)
    if recipe_id:is_valid() then add_available_recipe(results, recipe_id, recipe_id:obj().difficulty) end
  end
end

---@param results table
---@param entry BookRecipe
---@return nil
local function add_stored_book_recipe_entry(results, entry)
  local recipe = entry.recipe
  add_available_recipe(results, recipe:recipe_id(), entry.skill_level)
end

---@param results table
---@param books string[]
---@return nil
local function add_stored_book_recipes(results, books)
  for _, book in ipairs(books) do
    local book_id = ItypeId.new(book)
    if book_id:is_valid() then
      local book_slot = book_id:obj():slot_book()
      if book_slot then
        for maybe_entry, maybe_value in pairs(book_slot.recipes) do
          local entry = maybe_value
          if type(entry) ~= "userdata" then entry = maybe_entry end
          if type(entry) == "userdata" then add_stored_book_recipe_entry(results, entry) end
        end
      end
    end
  end
end

---@param params { item: Item, reader: Character, results: table }
---@return table
function ebook_reader.available_recipes(params)
  if not is_ebook_device(params.item) then return {} end
  local results = {}
  add_stored_book_recipes(results, stored_books(params.item))
  add_stored_recipes(results, split_ids(params.item:get_var_str(VAR_STORED_RECIPES, "")))
  add_stored_recipes(results, split_ids(params.item:get_var_str(VAR_LEGACY_RECIPES, "")))
  return results
end

---@param params { reader: Character, results: table }
---@return table
function ebook_reader.virtual_books(params)
  if not params.reader:as_avatar() then return {} end
  local results = {}
  local seen = {}
  for _, device in ipairs(params.reader:all_items(false)) do
    if is_ebook_device(device) then
      for _, book_id in ipairs(stored_books(device)) do
        if not seen[book_id] then
          seen[book_id] = true
          results[#results + 1] = {
            book_id = ItypeId.new(book_id),
            caption_suffix = " <color_light_blue>(E)</color>",
          }
        end
      end
    end
  end
  return results
end

---@param params { book_id: ItypeId, reader: Character, results: table }
---@return nil
function ebook_reader.read_virtual_book(params)
  params.results.handled = start_reading_book(params.reader, params.book_id:str())
end

return ebook_reader
