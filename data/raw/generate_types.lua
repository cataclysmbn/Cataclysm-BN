---@generic T
---@param t T[]
---@param f? fun(a: T, b: T):boolean
---@return T[]
function sort_by(t, f)
  if not f then f = sort_by_tostring end
  table.sort(t, f)
  return t
end

---@param a any
---@param b any
---@return boolean
function sort_by_tostring(a, b)
  if a.k and b.k then return tostring(a.k) < tostring(b.k) end
  return tostring(a) < tostring(b)
end

--- wraps sol2 table/map proxies so it can be sorted.
---@generic T
---@param t? T[]
---@return { k: string, v: T }[]
function wrapped(t)
  local res = {}
  for k, v in pairs(t or {}) do
    table.insert(res, { k = k, v = v })
  end
  return res
end

--[[
    Removes internal arguments like "<this_state>"
  ]]
---@param arg_list string[]
---@return string[]
local remove_hidden_args = function(arg_list)
  local ret = {}
  for _, arg in ipairs(arg_list) do
    if not string.match(arg, "^<.+>$") then table.insert(ret, arg) end
  end
  return ret
end

-- Rudimentary mapping from C++/sol types to LuaLS types.
---@param cpp_type string
---@return string
local map_cpp_type_to_lua = function(cpp_type)
  -- NOTE: This mapping might need refinement based on actual types used
  if not cpp_type then return "any" end -- Handle nil input gracefully
  cpp_type = string.gsub(cpp_type, "const%s+", "") -- Remove const
  cpp_type = string.gsub(cpp_type, "%s+&", "") -- Remove references
  cpp_type = string.gsub(cpp_type, "%s+%*", "") -- Remove pointers (basic)
  cpp_type = string.gsub(cpp_type, "%s+$", "") -- Trim trailing space

  if cpp_type == "std::string" or cpp_type == "string" then
    return "string"
  elseif
    cpp_type == "int"
    or cpp_type == "unsigned int"
    or cpp_type == "long"
    or cpp_type == "unsigned long"
    or cpp_type == "long long"
    or cpp_type == "unsigned long long"
    or cpp_type == "short"
    or cpp_type == "unsigned short"
    or cpp_type == "int8_t"
    or cpp_type == "uint8_t"
    or cpp_type == "int16_t"
    or cpp_type == "uint16_t"
    or cpp_type == "int32_t"
    or cpp_type == "uint32_t"
    or cpp_type == "int64_t"
    or cpp_type == "uint64_t"
    or cpp_type == "size_t"
    or cpp_type == "char"
    or cpp_type == "signed char"
    or cpp_type == "unsigned char"
  then
    return "integer" -- Lua 5.3+ distinguishes integers
  elseif cpp_type == "double" or cpp_type == "float" then
    return "number"
  elseif cpp_type == "bool" then
    return "boolean"
  elseif cpp_type == "sol::table" or cpp_type == "table" then
    return "table"
  elseif cpp_type == "sol::function" or cpp_type == "function" or cpp_type == "std::function" then
    return "function"
  elseif
    cpp_type == "sol::object"
    or cpp_type == "sol::lua_value"
    or cpp_type == "sol::stack_object"
    or cpp_type == "sol::protected_function"
    or cpp_type == "sol::unsafe_function"
  then
    return "any"
  elseif cpp_type == "void" or cpp_type == "nil" or cpp_type == "sol::nil_t" then
    return "nil"
  else
    -- Clean up namespaces and templates for class names
    local clean_type = string.gsub(cpp_type, "::", "_")
    -- Clean spaces
    clean_type = string.gsub(clean_type, "%s+", "")
    -- Try to extract the core type name, handling basic templates roughly
    clean_type = string.match(clean_type, "^[%w_]+%s*<?([%w_]+)?>?$") or clean_type
    clean_type = string.match(clean_type, "[%w_]+$") or clean_type -- Get the last part

    if clean_type == "..." or string.match(clean_type, "<cppval:") then
      clean_type = "any"
    elseif string.match(clean_type, "^Vector%(%w+%)$") then
      clean_type = string.gsub(clean_type, "^Vector%((%w+)%)$", "%1[]")
    elseif string.match(clean_type, "^Set%(%w+%)$") then
      clean_type = string.gsub(clean_type, "^Set%((%w+)%)$", "%1[]")
    elseif string.match(clean_type, "^Array%((%w+),(%d+)%)$") then
      clean_type = string.gsub(clean_type, "^Array%((%w+),(%d+)%)$", "%1[]")
    elseif string.match(clean_type, "^Map%((%w+),(%w+)%)$") then
      clean_type = string.gsub(clean_type, "^Map%((%w+),(%w+)%)$", "table<%1, %2>")
    elseif string.match(clean_type, "^Opt%((%w+)%)$") then
      clean_type = string.gsub(clean_type, "^Opt%((%w+)%)$", "%1?")
    end

    return clean_type or "any" -- Fallback to 'any' if nothing matches
  end
end

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
      table.insert(mapped_bases, map_cpp_type_to_lua(base_name))
    end
    return " : " .. table.concat(mapped_bases, ", ")
  end
end

--[[
    Formats --- comment annotations
    Input: comment string
    Output: Multiline string --- comments or ""
  ]]
---@param comment? string
---@return string
local fmt_comment_annotation = function(comment)
  if not comment or comment == "" then return "" end
  local comment_lines = {}
  for line in string.gmatch(comment .. "\n", "([^\n]*)\n") do
    table.insert(comment_lines, "--- " .. line)
  end
  return table.concat(comment_lines, "\n")
end

--[[
    Formats a single function signature string like "fun(param: type, ...): ret_type".
  ]]
---@param arg_list string[] List of "name:type" or "type"
---@param ret_type string C++ return type name
---@param class_name string The name of the owning class/library
---@param is_method boolean True if it's a method requiring 'self'
---@return string
local fmt_function_signature = function(arg_list, ret_type, class_name, is_method)
  local params = {}
  local mapped_class_name = map_cpp_type_to_lua(class_name)

  if is_method then table.insert(params, "self: " .. mapped_class_name) end

  local clean_arg_list = remove_hidden_args(arg_list)
  for i, arg_str in ipairs(clean_arg_list) do
    local name, type = string.match(arg_str, "^([^:]+):(.+)$")
    if not name then
      type = arg_str
      name = "arg" .. i -- Generate placeholder name if needed
    end
    local safe_name = string.gsub(name, "[^%w_]", "_")
    local lua_type = map_cpp_type_to_lua(type)
    table.insert(params, safe_name .. ": " .. lua_type)
  end

  local params_str = table.concat(params, ", ")
  local lua_ret_type = map_cpp_type_to_lua(ret_type)
  local ret_str = ""
  if lua_ret_type ~= "nil" then ret_str = ": " .. lua_ret_type end

  return "fun(" .. params_str .. ")" .. ret_str
end

--[[
    Formats ---@field annotation for variable members.
  ]]
---@param member table {name:string, vartype:string, comment?:string, hasval?:boolean, varval?:any}
---@param is_static boolean (optional) Not directly used in LuaLS field, but might be relevant contextually
---@return string
local fmt_variable_field = function(member, is_static)
  local ret = ""
  local member_name = tostring(member.name)
  if not string.match(member_name, "^[%a_][%w_]*$") then
    member_name = "['" .. member_name .. "']" -- Quote non-identifier names
  end
  local lua_type = map_cpp_type_to_lua(member.vartype)

  ret = ret .. "---@field " .. member_name .. " " .. lua_type
  if member.comment and member.comment ~= "" then ret = ret .. " @" .. member.comment end
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
---@param member table {name:string, comment?:string, overloads:table[]}
---@param class_name string The name of the owning class/library
---@param is_method boolean True if it's a method (defined with ':')
---@return string
local fmt_function_field = function(member, class_name, is_method)
  local ret = ""
  local member_name = tostring(member.name)
  if not string.match(member_name, "^[%a_][%w_]*$") then
    member_name = "['" .. member_name .. "']" -- Quote non-identifier names
  end

  local signatures = {}
  if member.overloads and #member.overloads > 0 then
    for _, overload in ipairs(member.overloads) do
      table.insert(signatures, fmt_function_signature(overload.args, overload.retval, class_name, is_method))
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
    table.insert(signatures, "function")
  end

  local signature_union = table.concat(signatures, " | ")
  ret = ret .. "---@field " .. member_name .. " " .. signature_union
  if member.comment and member.comment ~= "" then ret = ret .. " @" .. member.comment end
  return ret .. "\n"
end

--[[
    Formats ---@overload annotations and function stub for constructors ('new' function).
  ]]
---@param typename string Class name (C++ name, e.g., "TypeId")
---@param ctors string[][] List of constructor argument lists (C++ types).
--                      Each inner table is a list of C++ type strings for one constructor.
--                      An empty inner table {} signifies a constructor with no arguments.
--                      If ctors is nil or an empty table, only the basic new() stub and @return are generated.
---@return string EmmyLua annotation string for the constructor.
local fmt_constructor_field = function(typename, ctors)
  local type = map_cpp_type_to_lua(typename)

  ---@type string[]
  local lines = {}

  table.insert(lines, "---@return " .. type)
  for _, cpp_arg_list in ipairs(ctors) do
    if cpp_arg_list and #cpp_arg_list > 0 then
      table.insert(lines, "---@overload " .. fmt_function_signature(cpp_arg_list, typename, typename, false))
    end
  end
  table.insert(lines, "function " .. type .. ".new() end")

  return table.concat(lines, "\n") .. "\n"
end

---@param member { name: string, type: "var" | "func" }
function field_sort_order(member)
  if member.name == "NULL_ID" then return -1 end

  if string.match(member.name, "^__") then return 4 end -- metamethods
  if member.name == "deserialize" then return 3 end
  if member.name == "serialize" then return 2 end
  if member.type == "func" then return 1 end
  return 0
end

---@diagnostic disable-next-line: undefined-global
-- Main function to generate the LuaLS type definition file content.
---@return string
doc_gen_func.impl = function()
  local full_ret = [[---@meta
-- Generated by generate_types.lua - DO NOT EDIT MANUALLY

---@class game
---@field active_mods string[]
---@field mod_runtime table<string, any>
---@field mod_storage table<string, any>
---@field on_every_x_hooks table
---@field iuse_functions table
---@field hooks hooks
---@field current_mod string
---@field current_mod_path string
---@field cata_internal table
game = {}

---@class OnCharacterResetStatsParams
---@field character Character
on_character_reset_stats = {}

---@class OnMapgenPostprocessParams
---@field map Map
---@field omt Tripoint
---@field when TimePoint
on_mapgen_postprocess = {}

---@class OnMonDeathParams
---@field mon Monster
---@field killer Character
on_mon_death = {}
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
      local comment_annot = fmt_comment_annotation(comment)
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
          table.insert(formatted, { member = member, value = fmt_function_field(member, name, false) })
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
          local a_priority = field_sort_order(a.member)
          local b_priority = field_sort_order(b.member)

          if a_priority ~= b_priority then return a_priority < b_priority end
          if a.member.name ~= b.member.name then return a.member.name < b.member.name end
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
    local enum_comment = dt_enum.enum_comment

    local comment_annot = fmt_comment_annotation(enum_comment)
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

    full_ret = full_ret .. table.concat(table_entries, ",\n") .. "\n"
    full_ret = full_ret .. "}\n\n"
  end

  -- No second pass needed anymore

  return full_ret
end
