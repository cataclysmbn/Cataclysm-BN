local ui = require("lib.ui")

local SMELTER_FULL = FurnId.new("f_ore_smelter_full")
local SMELTER_EMPTY = FurnId.new("f_ore_smelter_empty")

local SMELT_DURATION = TimeDuration.from_hours(4):to_turns()

local VKEY_STARTED = "os_started"
local VKEY_RESOURCE_PREFIX = "os_res_%s"

local ORE_IRON = ItypeId.new("iron_ore")
local ORE_COPPER = ItypeId.new("copper_ore")
local ORE_LEAD = ItypeId.new("lead_ore")
local ORE_TIN = ItypeId.new("tin_ore")
local ORE_SILVER = ItypeId.new("silver_ore")
local ORE_GOLD = ItypeId.new("gold_ore")
local CHARCOAL = ItypeId.new("charcoal")

local RES_STEEL_LUMP = ItypeId.new("steel_lump")
local RES_COPPER = ItypeId.new("copper")
local RES_LEAD = ItypeId.new("lead")
local RES_TIN = ItypeId.new("tin")
local RES_SILVER = ItypeId.new("silver_small")
local RES_GOLD = ItypeId.new("gold_small")

---@type ItypeId[]
local ALL_ORES = {
    ORE_IRON,
    ORE_COPPER,
    ORE_LEAD,
    ORE_TIN,
    ORE_SILVER,
    ORE_GOLD
}
local FUEL_PER_ORE = 16
local ORE_TO_RESULT = {
    [ORE_IRON:str()] = RES_STEEL_LUMP,
    [ORE_COPPER:str()] = RES_COPPER,
    [ORE_LEAD:str()] = RES_LEAD,
    [ORE_TIN:str()] = RES_TIN,
    [ORE_SILVER:str()] = RES_SILVER,
    [ORE_GOLD:str()] = RES_GOLD
}

local MSG_NO_ORES = locale.gettext("The smelter does not have suitable metal to smelt.")
local MSG_NO_CHARCOAL = locale.gettext("The smelter requires charcoal to function.")
local MSG_NOT_ENOUGH_CHARCOAL = locale.gettext("Not enough charcoal. Available: %s Required: %s")
local MSG_CONFIRM_FIRE = locale.gettext("Smelting %s ores will require %s charcoal. \n\n Continue?")
local MSG_REMAINING_TIME = locale.gettext("Smelting will be finished in about %s hours.")
local MSG_RETRIEVE = locale.gettext("You retrieve the materials from the smelter.")


---@param map Map
---@param pos TripointBubMs
---@param amount integer
local function consume_charcoal(map, pos, amount)
    ---@type Item[]
    local charcoal_items = gapi.get_items_if({ type = CHARCOAL, pos = pos })
    local consumed_fuel = 0
    local consumed_items = {}
    for _, item in ipairs(charcoal_items) do
        if consumed_fuel + item.charges > amount then
            item:set_charges(item.charges - (amount - consumed_fuel))
            break
        else
            consumed_fuel = consumed_fuel + item.charges
            table.insert(consumed_items, item)
        end
    end
    for _, item in ipairs(consumed_items) do
      map:remove_item_at(pos, item)
    end
end

-- Use a furn_var to store amount of ore types on the smelter
---@param map Map
---@param pos TripointBubMs
---@param ores Item[]
local function store_ores(map, pos, ores)
    local consumed_ores = {}
    ---@type table<ItypeId, integer>
    local ores_per_type = {}
    for _, mtl_item in ipairs(ores) do
        local itm_type_str = mtl_item:get_type():str()
        if ores_per_type[itm_type_str] == nil then
            ores_per_type[itm_type_str] = 1
        else
            ores_per_type[itm_type_str] = ores_per_type[itm_type_str] + 1
        end
        table.insert(consumed_ores, mtl_item)
    end
    for itm_type_str, ore_count in pairs(ores_per_type) do
        map:set_furn_var_at(pos, string.format(VKEY_RESOURCE_PREFIX, itm_type_str), tostring(ore_count))
    end
    for _, item in ipairs(consumed_ores) do
      map:remove_item_at(pos, item)
    end
end


-- Query ores and charcoal and confirm then start process
---@param params ExamineParams
local examine_empty = function(params)
    ---@type Item[]
    local metal_items = gapi.get_items_if({
        type = ALL_ORES,
        pos = params.pos,
        radius = 0,
    })
    if not metal_items or #metal_items == 0 then
        gapi.add_msg(MSG_NO_ORES)
        return
    end
    ---@type Item[]
    local charcoal_items = gapi.get_items_if({
        type = CHARCOAL,
        pos = params.pos,
    })
    if not charcoal_items or #charcoal_items == 0 then
        gapi.add_msg(MSG_NO_CHARCOAL)
        return
    end

    local required_fuel = #metal_items * FUEL_PER_ORE
    local available_fuel = 0
    for _, item in ipairs(charcoal_items) do
        available_fuel = available_fuel + item.charges
    end

    if available_fuel < required_fuel then
        gapi.add_msg(string.format(MSG_NOT_ENOUGH_CHARCOAL, available_fuel, required_fuel))
        return
    end

    local confirm = ui.query_yn(string.format(MSG_CONFIRM_FIRE, #metal_items, required_fuel))
    if not confirm then return end

    local map = gapi.get_map()
    map:set_furn_at(params.pos, SMELTER_FULL:int_id())
    store_ores(map, params.pos, metal_items)
    consume_charcoal(map, params.pos, required_fuel)
    map:set_furn_var_at(params.pos, VKEY_STARTED, tostring(gapi.current_turn():to_turn()))
end


---@param params ExamineParams
local examine_full = function(params)
    local map = gapi.get_map()
    local started_str = map:get_furn_var_at(params.pos, VKEY_STARTED)
    local started_int = tonumber(started_str)
    local started_turns
    if started_int == nil then
        gdebug.log_error("Furniture did not have starting time.")
        started_turns = gapi.turn_zero():to_turn()
    else
        started_turns = started_int
    end
    
    local elapsed_turns = gapi.current_turn():to_turn() - started_turns
    if elapsed_turns < SMELT_DURATION then
      local remaining = SMELT_DURATION - elapsed_turns
      gapi.add_msg(string.format(MSG_REMAINING_TIME, TimeDuration.from_turns(remaining):to_hours()))
      return
    end
    
    local contained_ores = {}
    for _, ore_type in ipairs(ALL_ORES) do
        local ore_type_str = ore_type:str()
        local var_ore_key = string.format(VKEY_RESOURCE_PREFIX, ore_type_str)
        local ore_amount_str = map:get_furn_var_at(params.pos, var_ore_key)
        if ore_amount_str and ore_amount_str ~= "" then
            local ore_amount = tonumber(ore_amount_str)
            if ore_amount and ore_amount > 0 then
              local res_type = ORE_TO_RESULT[ore_type_str]
              map:create_item_at(params.pos, res_type, ore_amount)
              map:erase_furn_var_at(params.pos, var_ore_key)
            end
        end
    end
    map:set_furn_at(params.pos, SMELTER_EMPTY:int_id())
    map:erase_furn_var_at(params.pos, VKEY_STARTED)
    gapi.add_msg(MSG_RETRIEVE)
end

return {
    examine_empty = examine_empty,
    examine_full = examine_full
}
