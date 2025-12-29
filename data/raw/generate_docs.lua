--[[
    Receives a table and returns sorted array where each value is table of type { k=k, v=v }.

    Sort by key by default, but may also use provided sort function f(a, b)
    where a and b - tables of type {k=k, v=v}
    (the function must return whether a is less than b).
]]
--
local sorted_by = function(t, f)
  if not f then f = function(a, b) return (a.k < b.k) end end
  local sorted = {}
  for k, v in pairs(t) do
    table.insert(sorted, { k = k, v = v })
  end
  table.sort(sorted, f)
  return sorted
end

local remove_hidden_args = function(arg_list)
  local ret = {}
  for _, arg in pairs(arg_list) do
    if arg == "<this_state>" then
      -- sol::this_state is only visible to C++ side
    else
      table.insert(ret, arg)
    end
  end
  return ret
end

local fmt_arg_list = function(arg_list)
  local ret = ""
  local arg_list = remove_hidden_args(arg_list)
  if #arg_list == 0 then return ret end
  local is_first = true
  for _, arg in pairs(arg_list) do
    if not is_first then ret = ret .. "," end
    ret = ret .. " " .. arg
    is_first = false
  end
  return ret .. " "
end

local fmt_one_constructor = function(typename, ctor) return typename .. ".new(" .. fmt_arg_list(ctor) .. ")" end

local fmt_constructors = function(typename, ctors)
  if #ctors == 0 then
    return "  No constructors.\n"
  else
    local ret = ""
    for k, v in pairs(ctors) do
      ret = ret .. "#### `" .. fmt_one_constructor(typename, v) .. "`\n"
    end
    return ret
  end
end

local fmt_one_member = function(typename, member)
  local ret = "#### " .. tostring(member.name) .. "\n"

  if member.comment then ret = ret .. member.comment .. "\n" end

  if member.type == "var" then
    ret = ret .. "  Variable of type `" .. member.vartype .. "`"
    if member.hasval then ret = ret .. " value: `" .. tostring(member.varval) .. "`" end
    ret = ret .. "\n"
  elseif member.type == "func" then
    for _, overload in pairs(member.overloads) do
      ret = ret .. "  Function `(" .. fmt_arg_list(overload.args) .. ")"
      if overload.retval ~= "nil" then ret = ret .. " -> " .. overload.retval end
      ret = ret .. "`\n"
    end
  else
    error("Unknown member type " .. tostring(member.type))
  end

  return ret
end

local fmt_members = function(typename, members)
  if #members == 0 then
    return "  No members.\n"
  else
    local ret = ""

    local members_sorted = sorted_by(members)

    for _, it in pairs(members_sorted) do
      ret = ret .. fmt_one_member(typename, it.v) .. "\n"
    end
    return ret
  end
end

local fmt_bases = function(typename, bases)
  if #bases == 0 then
    return "  No base classes.\n"
  else
    local ret = ""
    for k, v in pairs(bases) do
      ret = ret .. "- `" .. v .. "`\n"
    end
    return ret
  end
end

local fmt_enum_entries = function(typename, entries)
  if next(entries) == nil then
    return "  No entries.\n"
  else
    local ret = ""

    local entries_filtered = {}
    for k, v in pairs(entries) do
      -- TODO: this should not be needed
      if type(v) ~= "table" and type(v) ~= "function" then entries_filtered[k] = v end
    end

    local entries_sorted = sorted_by(entries_filtered, function(a, b) return a.v < b.v end)
    for _, it in pairs(entries_sorted) do
      ret = ret .. "- `" .. tostring(it.k) .. "` = `" .. tostring(it.v) .. "`\n"
    end
    return ret
  end
end

doc_gen_func.impl = function()
  local ret = [[---
edit: false
---

# Lua API reference

> [!NOTE]
>
> This page is auto-generated from [`data/raw/generate_docs.lua`][generate_docs]
and should not be edited directly.

[generate_docs]: https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/raw/generate_docs.lua
]]

  local dt = catadoc

  local types_table = dt["#types"]

  local types_sorted = sorted_by(types_table)
  for _, it in pairs(types_sorted) do
    local typename = it.k
    local dt_type = it.v
    local type_comment = dt_type.type_comment
    ret = ret .. "## " .. typename .. "\n"

    if type_comment then ret = ret .. type_comment .. "\n" end

    local bases = dt_type["#bases"]
    local ctors = dt_type["#construct"]
    local members = dt_type["#member"]

    ret = ret
      .. "### Bases\n"
      .. fmt_bases(typename, bases)
      .. "\n"
      .. "### Constructors\n"
      .. fmt_constructors(typename, ctors)
      .. "\n"
      .. "### Members\n"
      .. fmt_members(typename, members)
      .. "\n"
  end

  ret = ret .. "# Enums\n\n"

  local enums_table = dt["#enums"]

  local enums_sorted = sorted_by(enums_table)
  for _, it in pairs(enums_sorted) do
    local typename = it.k
    local dt_type = it.v
    ret = ret .. "## " .. typename .. "\n"

    local entries = dt_type["entries"]

    ret = ret .. "### Entries\n" .. fmt_enum_entries(typename, entries) .. "\n"
  end

  ret = ret .. "# Libraries\n\n"

  local libs_table = dt["#libs"]

  local libs_sorted = sorted_by(libs_table)
  for _, it in pairs(libs_sorted) do
    local typename = it.k
    local dt_lib = it.v
    local lib_comment = dt_lib.lib_comment
    ret = ret .. "## " .. typename .. "\n"

    if lib_comment then ret = ret .. lib_comment .. "\n" end

    local members = dt_lib["#member"]

    ret = ret .. "### Members\n" .. fmt_members(nil, members) .. "\n"
  end

  return ret
end
