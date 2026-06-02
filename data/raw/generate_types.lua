require("docgen_common")

--[[
    Formats the base class list for a LuaLS class annotation.
    Input: bases: A sequence (like sol::vector) of base class name strings.
    Output: A string like ": Base1, Base2" or an empty string if no bases.
  ]]
---@param bases string[]
---@return string
local fmt_bases_luals = function(bases)
  -- Use ipairs as 'bases' should be a sequence (vector)
  if not bases or #bases == 0 then
    return ""
  else
    local mapped_bases = {}
    for _, base_name in ipairs(bases) do
      table.insert(mapped_bases, map_cpp_type_to_lua(base_name, false))
    end
    return " : " .. table.concat(mapped_bases, ", ")
  end
end

--[[
    Formats a single function signature string like "fun(param: type, ...): ret_type".
  ]]
---@param arg_list string[] List of types
---@param ret_type string return type
---@param class_name string The name of the owning class/library
---@param meta string Member metadata
---@return string
local fmt_function_signature = function(arg_list, ret_type, class_name, meta)
  local params = {}

  local clean_arg_list = remove_hidden_args(arg_list)
  local meta_args = get_meta_params(meta)
  local state = nil

  for i, arg_str in ipairs(clean_arg_list) do
    local lua_type = map_cpp_type_to_lua(arg_str, false)
    local arg_name
    if i == 1 and arg_str == class_name then
      arg_name = "self"
    else
      state, arg_name = next(meta_args, state)
      if not arg_name then
        arg_name = "arg" .. i -- Generate placeholder name if needed
      else
        arg_name = string.gsub(arg_name, "[^%w_]", "_")
      end
    end
    table.insert(params, arg_name .. ": " .. lua_type)
  end

  local params_str = table.concat(params, ", ")
  local lua_ret_type = map_cpp_type_to_lua(ret_type, false)
  local ret_str = ""
  if lua_ret_type ~= "nil" then ret_str = ": " .. lua_ret_type end

  return "fun(" .. params_str .. ")" .. ret_str
end

---@type table<string, string>
local operator_metamethod_names = {
  __add = "add",
  __concat = "concat",
  __div = "div",
  __eq = "eq",
  __le = "le",
  __len = "len",
  __lt = "lt",
  __mod = "mod",
  __mul = "mul",
  __pow = "pow",
  __sub = "sub",
  __unm = "unm",
}

---@param member_name string
---@return string?
local get_operator_metamethod_name = function(member_name) return operator_metamethod_names[member_name] end

---@param member table {name:string, overloads:table[]}
---@param class_name string
---@return string
local fmt_operator_annotations = function(member, class_name)
  local operator_name = get_operator_metamethod_name(tostring(member.name))
  if not operator_name or not member.overloads or #member.overloads == 0 then return "" end

  ---@type string[]
  local lines = {}
  ---@type table<string, boolean>
  local seen = {}

  for _, overload in ipairs(member.overloads) do
    local clean_arg_list = remove_hidden_args(overload.args)
    ---@type string[]
    local operand_types = {}

    for i, arg_str in ipairs(clean_arg_list) do
      if not (i == 1 and arg_str == class_name) then
        table.insert(operand_types, map_cpp_type_to_lua(arg_str, false))
      end
    end

    if #operand_types <= 1 then
      local operator_args = ""
      if #operand_types == 1 then operator_args = "(" .. operand_types[1] .. ")" end

      local operator_line = "---@operator "
        .. operator_name
        .. operator_args
        .. ": "
        .. map_cpp_type_to_lua(overload.retval, false)

      if not seen[operator_line] then
        seen[operator_line] = true
        table.insert(lines, operator_line)
      end
    end
  end

  if #lines == 0 then return "" end

  return table.concat(lines, "\n") .. "\n"
end

--[[
    Formats ---@field annotation for variable members.
  ]]
---@param member table {name:string, vartype:string, comment?:string, hasval?:boolean, varval?:any}
---@param _is_static boolean (optional) Not directly used in LuaLS field, but might be relevant contextually
---@return string
local fmt_variable_field = function(member, _is_static)
  local ret = ""
  local member_name = tostring(member.name)
  if not string.match(member_name, "^[%a_][%w_]*$") then
    member_name = "['" .. member_name .. "']" -- Quote non-identifier names
  end
  local lua_type = map_cpp_type_to_lua(member.vartype, false)

  ret = ret .. "---@field " .. member_name .. " " .. lua_type
  if member.comment and member.comment ~= "" then
    ret = ret .. " @" .. string_concat_matches(member.comment, "([^\r\n]+)", "<br />")
  end
  if member.hasval then
    -- Avoid overly long or complex value representations
    local val_str = tostring(member.varval)
    if #val_str < 50 and not string.find(val_str, "\n") then ret = ret .. " # value: " .. val_str end
  end
  ret = ret .. "\n"
  return ret
end

--[[
    Formats ---@field annotation for function members, handling overloads.
  ]]
---@param signatures string[]
---@return string
local fmt_signature_union = function(signatures)
  if #signatures <= 1 then return signatures[1] or "function" end

  local wrapped = {}
  for _, signature in ipairs(signatures) do
    table.insert(wrapped, "(" .. signature .. ")")
  end
  return table.concat(wrapped, " | ")
end

---@param member table {name:string, comment?:string, overloads:table[]}
---@param class_name string The name of the owning class/library
---@return string
local fmt_function_field = function(member, class_name)
  local operator_annotations = fmt_operator_annotations(member, class_name)
  if operator_annotations ~= "" then return operator_annotations end

  local ret = ""
  local member_name = tostring(member.name)
  if not string.match(member_name, "^[%a_][%w_]*$") then
    member_name = "['" .. member_name .. "']" -- Quote non-identifier names
  end

  ---@type string[]
  local signatures = {}
  ---@type table<string, boolean>
  local seen_signatures = {}
  local add_signature = function(signature)
    if seen_signatures[signature] then return end
    seen_signatures[signature] = true
    table.insert(signatures, signature)
  end

  if member.overloads and #member.overloads > 0 then
    for _, overload in ipairs(member.overloads) do
      add_signature(fmt_function_signature(overload.args, overload.retval, class_name, member.comment))
    end
  else
    -- Fallback if no overload data? Maybe treat as any function?
    -- Or assume a default signature if possible? Let's assume 'any' for now if data is missing.
    -- Log a warning maybe?
    print(
      "Warning: No overload data found for function member: "
        .. class_name
        .. "."
        .. member_name
        .. ". Using 'function' type.\n"
    )
    add_signature("function")
  end

  local signature_union = fmt_signature_union(signatures)
  ret = ret .. "---@field " .. member_name .. " " .. signature_union
  if member.comment and member.comment ~= "" then
    local op = function(m)
      if string.match(m, "^@param") then
        return nil
      else
        return m
      end
    end
    ret = ret .. " @" .. string_concat_matches(member.comment, "([^\r\n]+)", "<br />", op)
  end
  return ret .. "\n"
end

--[[
    Formats ---@overload annotations and function stub for constructors ('new' function).
  ]]
---@param typename string Class name (C++ name, e.g., "TypeId")
---@param ctors string[][] @ List of constructor argument lists (C++ types).
--- Each inner table is a list of C++ type strings for one constructor.
--- An empty inner table {} signifies a constructor with no arguments.
--- If ctors is nil or an empty table, only the basic new() stub and @return are generated.
---@return string EmmyLua annotation string for the constructor.
local fmt_constructor_field = function(typename, ctors)
  local type = map_cpp_type_to_lua(typename, true)

  ---@type string[]
  local lines = {}

  table.insert(lines, "---@return " .. type)
  for _, cpp_arg_list in ipairs(ctors) do
    if cpp_arg_list and #cpp_arg_list > 0 then
      table.insert(lines, "---@overload " .. fmt_function_signature(cpp_arg_list, typename, typename, ""))
    end
  end
  table.insert(lines, "function " .. type .. ".new() end")

  return table.concat(lines, "\n") .. "\n"
end

---@diagnostic disable-next-line: undefined-global
-- Main function to generate the LuaLS type definition file content.
---@return string
doc_gen_func.impl = function()
  local full_ret = [[---@meta
-- Generated by generate_types.lua - DO NOT EDIT MANUALLY

---@class HookResult
---@field allowed boolean @whether the action is allowed (true) or blocked (false), defaults to true

---@class HookParams
---@field results table @shared table to store results from chained hook calls
---@field prev any @result of the previous hook call in the chain, nil for the first call
---@field [string] any

---@alias HookCallback fun(params: HookParams): HookResult

---@class HookConfig
---@field fn HookCallback @function to call
---@field priority? integer @priority (defaults to 0, higher runs earlier)
---@field mod_id? string @mod id (defaults to current mod)

---@alias HookEntry HookCallback | HookConfig

---@class ItemUseParams
---@field user Character
---@field item Item
---@field pos TripointBubMs

---@class ItemEquipCheckParams
---@field user Character
---@field item Item

---@class ItemEquipParams : ItemEquipCheckParams
---@field move_cost integer

---@class ItemStateParams
---@field user Character
---@field item Item
---@field pos TripointBubMs

---@class ItemDurabilityChangeParams
---@field user Character
---@field item Item
---@field old_damage integer
---@field new_damage integer

---@class BionicCallbackParams
---@field user Character
---@field bionic Bionic

---@class IuseFunctionTable
---@field use fun(params: ItemUseParams): integer
---@field can_use? fun(params: ItemUseParams): boolean

---@class IwieldableFunctionTable
---@field on_wield? fun(params: ItemEquipParams)
---@field on_unwield? fun(params: ItemEquipCheckParams)
---@field can_wield? fun(params: ItemEquipCheckParams): boolean
---@field can_unwield? fun(params: ItemEquipCheckParams): boolean

---@class IwearableFunctionTable
---@field on_wear? fun(params: ItemEquipParams)
---@field on_takeoff? fun(params: ItemEquipCheckParams)
---@field can_wear? fun(params: ItemEquipCheckParams): boolean
---@field can_takeoff? fun(params: ItemEquipCheckParams): boolean

---@class IequippableFunctionTable
---@field on_durability_change? fun(params: ItemDurabilityChangeParams)
---@field on_repair? fun(params: ItemDurabilityChangeParams)
---@field on_break? fun(params: ItemDurabilityChangeParams)

---@class IstateFunctionTable
---@field on_tick? fun(params: ItemStateParams)
---@field on_pickup? fun(params: ItemStateParams)
---@field on_drop? fun(params: ItemStateParams): boolean

---@class BionicFunctionTable
---@field on_activate? fun(params: BionicCallbackParams)
---@field on_deactivate? fun(params: BionicCallbackParams)
---@field on_installed? fun(params: BionicCallbackParams)
---@field on_removed? fun(params: BionicCallbackParams)

---@alias MapgenFunction fun(...: any): any

---@class game
---@field active_mods string[]
---@field mod_runtime table<string, any>
---@field mod_storage table<string, any>
---@field on_every_x_hooks table
---@field iuse_functions table<string, fun(params: ItemUseParams): integer | IuseFunctionTable>
---@field iwieldable_functions table<string, IwieldableFunctionTable>
---@field iwearable_functions table<string, IwearableFunctionTable>
---@field iequippable_functions table<string, IequippableFunctionTable>
---@field istate_functions table<string, IstateFunctionTable>
---@field imelee_functions table<string, table<string, function>>
---@field iranged_functions table<string, table<string, function>>
---@field bionic_functions table<string, BionicFunctionTable>
---@field mutation_functions table<string, table<string, function>>
---@field horde_behaviours table<string, function>
---@field mapgen_functions table<string, MapgenFunction>
---@field hooks hooks
---@field current_mod string
---@field current_mod_path string
---@field cata_internal table
---@field add_hook fun(hook_name: string, entry: HookEntry) @Registers a hook.
game = {}

---@class OnPlayerTryMoveParams
---@field player Avatar
---@field from TripointBubMs
---@field to TripointBubMs
---@field movement_mode CharacterMoveMode
---@field via_ramp boolean
---@field mounted boolean
---@field mount Creature?
on_player_try_move = {}

---@class OnNPCTryMoveParams
---@field npc Npc
---@field from TripointBubMs
---@field to TripointBubMs
---@field movement_mode CharacterMoveMode
---@field via_ramp boolean
---@field mounted boolean
---@field mount Creature?
on_npc_try_move = {}

---@class OnMonsterTryMoveParams
---@field monster Monster
---@field from TripointBubMs
---@field to TripointBubMs
---@field force boolean
on_monster_try_move = {}

---@class OnCharacterTryMoveParams
---@field char Character
---@field from TripointBubMs
---@field to TripointBubMs
---@field movement_mode CharacterMoveMode
---@field via_ramp boolean
---@field mounted boolean
---@field mount Creature?
on_character_try_move = {}

---@class OnCraftResultParams
---@field crafter Character
---@field craft Item?
---@field item Item
---@field recipe RecipeRaw
---@field batch_size integer
---@field hot_result boolean
---@field dehydrated_result boolean
on_craft_result = {}

---@class OnCharacterResetStatsParams
---@field character Character
on_character_reset_stats = {}

---@class OnCharacterEffectAddedParams
---@field character Character
---@field effect Effect
on_character_effect_added = {}

---@class OnCharacterEffectParams
---@field character Character
---@field effect Effect
on_character_effect = {}

---@class OnMonEffectAddedParams
---@field mon Monster
---@field effect Effect
on_mon_effect_added = {}

---@class OnMonEffectParams
---@field mon Monster
---@field effect Effect
on_mon_effect = {}

---@class OnMonDeathParams
---@field mon Monster
---@field killer Creature?
on_mon_death = {}

---@class OnCharacterDeathParams
---@field char Character
---@field killer Creature?
on_character_death = {}

---@class OnShootParams
---@field shooter Character
---@field target_pos TripointBubMs
---@field shots integer
---@field gun Item
---@field ammo Item?
on_shoot = {}

---@class OnThrowParams
---@field thrower Character
---@field target_pos TripointBubMs
---@field throw_from_pos TripointBubMs
---@field thrown Item
on_throw = {}

---@class OnTryNPCInterationParams
---@field npc Npc
on_try_npc_interaction = {}

---@class OnNPCInterationParams
---@field npc Npc
on_npc_interaction = {}

---@class OnTryMonsterInteractionParams
---@field monster Monster
on_try_monster_interaction = {}

---@class OnDialogueStartParams
---@field npc Npc
---@field next_topic string
---@field prev string?
on_dialogue_start = {}

---@class OnDialogueOptionParams
---@field npc Npc
---@field next_topic string
---@field prev string?
on_dialogue_option = {}

---@class OnDialogueEndParams
---@field npc Npc
on_dialogue_end = {}

---@class OnCreatureDodgedParams
---@field char Character | Creature
---@field source Creature?
---@field difficulty integer
on_creature_dodged = {}

---@class OnCreatureBlockedParams
---@field char Character
---@field source Creature?
---@field bodypart_id BodyPartTypeId
---@field damage_instance DamageInstance
---@field damage_blocked number
on_creature_blocked = {}

---@class OnCreaturePerformedTechniqueParams
---@field char Character
---@field technique MartialArtsTechniqueRaw
---@field target Creature
---@field damage_instance DamageInstance
---@field move_cost integer
on_creature_performed_technique = {}

---@class OnCreatureMeleeAttackedParams
---@field char Character | Monster
---@field target Creature
---@field success boolean
on_creature_melee_attacked = {}

---@class OnMapgenPostprocessParams
---@field map Map
---@field omt TripointAbsOmt
---@field when TimePoint
on_mapgen_postprocess = {}

---@class OnExplodeParams
---@field pos TripointBubMs
---@field damage integer
---@field radius integer
---@field fire boolean
on_explosion_start = {}

---@class OnCreatureSpawnParams
---@field creature Creature
on_creature_spawn = {}

---@class OnMonsterSpawnParams
---@field monster Monster
on_monster_spawn = {}

---@class OnNpcSpawnParams
---@field npc Npc
on_npc_spawn = {}

---@class OnCreatureLoadedParams
---@field creature Creature
on_creature_loaded = {}

---@class OnMonsterLoadedParams
---@field monster Monster
on_monster_loaded = {}

---@class OnNpcLoadedParams
---@field npc Npc
on_npc_loaded = {}

]]

  ---@diagnostic disable-next-line: undefined-global
  local dt = catadoc

  -- Process Classes and Libraries (Types and Libs)
  ---@param section_name string Display name for the section header
  ---@param section_data table Data table (#types or #libs)
  ---@param is_class boolean True if processing classes (affects 'self', bases, constructors)
  ---@return string Generated Lua code snippet for this section
  local process_section = function(section_name, section_data, is_class)
    local ret = ""
    local section_sorted = sort_by(section_data)
    if #section_sorted == 0 then return "" end -- Skip empty sections

    ret = ret .. "--================---- " .. section_name .. " ----================\n\n"

    for _, item in ipairs(section_sorted) do
      local name = item.k -- Class or Library name
      local data = item.v or {}
      local comment = data.type_comment or data.lib_comment or ""
      local bases = data["#bases"] or {}
      local ctors = data["#construct"] or {} -- Only used if is_class is true
      local members = data["#member"] or {}

      -- Class/Lib Annotation Start
      local bases_str = is_class and fmt_bases_luals(bases) or ""
      local comment_annot = string_concat_matches(comment, "([^\r\n]+)", "\n", function(m) return "--- " .. m end)
      if comment_annot ~= "" then ret = ret .. comment_annot .. "\n" end
      ret = ret .. "---@class " .. name .. bases_str .. "\n"

      -- Process Members (Variables and Functions)
      ---@type { member: table, value: string }[]
      local formatted = {}
      for _, member in ipairs(members) do
        local member_name_str = tostring(member.name)

        -- Skip potentially problematic internal names if necessary
        -- Example: if string.match(member_name_str, "^__") then goto continue end

        if member.type == "var" then
          -- Determine if it's static based on context (this might need info from `data` if available)
          -- For now, assume instance vars for classes, static for libs unless specified otherwise
          local is_static_var = not is_class -- Simple assumption, may need refinement
          --   ret = ret .. fmt_variable_field(member, is_static_var)
          table.insert(formatted, { member = member, value = fmt_variable_field(member, is_static_var) })
        elseif member.type == "func" then
          -- Libraries expose functions statically (.), classes expose methods dynamically (:) by default in sol2
          -- Pass 'is_class' to fmt_function_field to decide if 'self' should be added.
          table.insert(formatted, { member = member, value = fmt_function_field(member, name) })
        else
          -- Fallback
          table.insert(formatted, {
            member = member,
            value = fmt_variable_field(
              { name = member_name_str, vartype = "any", comment = "Unknown member type" },
              not is_class
            ),
          })
        end
      end

      for _, item in
        ipairs(sort_by(formatted, function(a, b)
          if field_sort_less(a.member, b.member) then return true end
          if field_sort_less(b.member, a.member) then return false end
          return a.value < b.value
        end))
      do
        ret = ret .. item.value
      end
      ret = ret .. name .. " = {}\n"

      if is_class then ret = ret .. fmt_constructor_field(name, ctors) end
      ret = ret .. "\n"
    end
    return ret
  end

  -- Generate sections
  full_ret = full_ret .. process_section("Classes", wrapped(dt["#types"]), true)
  full_ret = full_ret .. process_section("Libraries", wrapped(dt["#libs"]), false)

  -- Process Enums (Remain largely unchanged, ensure sorting/formatting is robust)
  local enums_sorted = sort_by(wrapped(dt["#enums"]))
  if #enums_sorted > 0 then full_ret = full_ret .. "--=================---- Enums ----=================\n\n" end
  for _, item in ipairs(enums_sorted) do
    local enumname = item.k
    local dt_enum = item.v or {}
    local comment = dt_enum.enum_comment

    local comment_annot = string_concat_matches(comment, "([^\r\n]+)", "\n", function(m) return "--- " .. m end)
    if comment_annot ~= "" then full_ret = full_ret .. comment_annot .. "\n" end

    full_ret = full_ret .. "---@enum " .. enumname .. "\n"
    full_ret = full_ret .. enumname .. " = {\n"

    ---@type { k: string, v: string | number | boolean }[]
    local entries_filtered = {}
    for k, v in pairs(dt_enum["entries"] or {}) do
      if type(v) == "string" or type(v) == "number" or type(v) == "boolean" then
        table.insert(entries_filtered, { k = k, v = v })
      end
    end

    local entries_sorted_by_v = sort_by(entries_filtered, function(a, b) return a.v < b.v end)

    local table_entries = {}
    for _, entry_item in ipairs(entries_sorted_by_v) do
      local key = tostring(entry_item.k)
      local value = entry_item.v
      local key_str
      local value_str

      -- Format key (quote if not a valid identifier)
      if string.match(key, "^[%a_][%w_]*$") and not tonumber(key) then -- Ensure it's not purely numeric either
        key_str = key
      else
        key_str = "['" .. string.gsub(key, "['\\]", "\\%1") .. "']" -- Escape quotes and backslashes within the key
      end

      -- Format value (quote strings)
      if type(value) == "string" then
        value_str = '"'
          .. string.gsub(
            value,
            '[%c"\\]',
            { ['"'] = '\\"', ["\\"] = "\\\\", ["\n"] = "\\n", ["\r"] = "\\r", ["\t"] = "\\t" }
          )
          .. '"' -- Basic escaping
      else
        value_str = tostring(value) -- numbers, booleans
      end

      table.insert(table_entries, "\t" .. key_str .. " = " .. value_str)
    end

    full_ret = full_ret .. table.concat(table_entries, ",\n") .. ",\n"
    full_ret = full_ret .. "}\n\n"
  end

  -- No second pass needed anymore

  return full_ret
end
