require("docgen_common")

local slug_for = function(typename, membername)
  local typename = string.gsub(tostring(typename), "%s", "")
  if membername == nil then
    return ("#sol::%s"):format(typename)
  else
    return ("#sol::%s::%s"):format(typename, membername)
  end
end

local table_contains = function(tbl, key)
  for k, v in pairs(tbl) do
    if tostring(k) == tostring(key) then return true end
  end
  return false
end

local linkify_types = function(str_, blockquote)
  local dt = catadoc
  local types_table = dt["#types"]
  local enums_table = dt["#enums"]

  local tokens = extract_cpp_types(tostring(str_))

  local ret = ""

  for _, token in pairs(tokens) do
    local str, is_cppval = table.unpack(token)
    if is_cppval then
      str = string.gsub(str, "<", "&lt;")
      str = string.gsub(str, ">", "&gt;")
    else
      str = string.gsub(str, "[%a%d]+", function(k)
        if table_contains(types_table, k) or table_contains(enums_table, k) then
          local sub = ("[%s](#sol::%s)"):format(k, k)
          if blockquote then sub = "<code>" .. sub .. "</code>" end
          return sub
        end
        return k
      end)
    end
    ret = ret .. str
  end

  return ret
end

local fmt_arg_list = function(arg_list, meta)
  local ret = " "
  local arg_list = remove_hidden_args(arg_list)
  if #arg_list == 0 then return ret end

  local meta = get_meta_params(meta)
  local state
  for i, v in pairs(arg_list) do
    state, name = next(meta, state)
    if state ~= nil then
      arg_list[i] = name .. ": " .. v
    else
      arg_list[i] = v
    end
  end

  ret = ret .. table.concat(arg_list, ", ")
  return ret .. " "
end

local fmt_one_constructor =
  function(typename, ctor) return typename .. ".new(" .. linkify_types(fmt_arg_list(ctor), false) .. ")" end

local fmt_constructors = function(typename, ctors)
  if #ctors == 0 then
    return "  No constructors.\n"
  else
    local ret = ""
    for k, v in pairs(ctors) do
      ret = ret .. "* " .. fmt_one_constructor(typename, v) .. "  \n"
    end
    return ret
  end
end

local function fmt_one_member_var(typename, member)
  local ret = ""
  local lua_rv = map_cpp_type_to_lua(member.vartype, true)
  if member.hasval then
    ret = ret .. (" 🇨 Constant --> <code>%s</code>"):format(linkify_types(lua_rv, false))
    ret = ret .. " = `" .. tostring(member.varval) .. "`"
  else
    ret = ret .. (" 🇻 Variable --> <code>%s</code>"):format(linkify_types(lua_rv, false))
  end
  ret = ret .. "  \n"
  return ret
end

local function fmt_one_member_func(typename, member)
  local ret = ""
  local name, state
  for _, overload in pairs(member.overloads) do
    local is_method = typename ~= nil and overload.args[1] == typename
    local lua_rv = map_cpp_type_to_lua(overload.retval, true)
    local lua_args = {}

    for k, v in pairs(overload.args) do
      lua_args[k] = map_cpp_type_to_lua(v, true)
    end

    if is_method then lua_args = { table.unpack(lua_args, 2) } end

    local sigFmt = overload.retval ~= "nil" and "(%s) -> %s" or "(%s)"
    local sigStr = sigFmt:format(fmt_arg_list(lua_args, member.comment), lua_rv)
    sigStr = linkify_types(sigStr, false)

    if is_method then
      ret = ret .. (" 🇲 Method --> <code>%s</code>  \n"):format(sigStr)
    else
      ret = ret .. (" 🇫 Function --> <code>%s</code>  \n"):format(sigStr)
    end
  end
  return ret
end

local fmt_one_member = function(typename, member)
  local ret = ("#### %s {%s}\n"):format(member.name, slug_for(typename, member.name))
  if member.type == "var" then
    ret = ret .. fmt_one_member_var(typename, member)
  elseif member.type == "func" then
    ret = ret .. fmt_one_member_func(typename, member)
  else
    error("  Unknown member type " .. tostring(member.type))
  end

  if member.comment then
    local com = string_concat_matches(member.comment, "[^\r\n]+", "\n", function(m)
      if string.match(m, "^@param") then return nil end
      return "> " .. linkify_types(m, true)
    end)
    ret = ret .. com
  end

  return ret
end

local fmt_members = function(typename, members)
  if #members == 0 then
    return "  No members.\n"
  else
    local ret = ""

    local ss = function(a, b) return field_sort_order(a.v) < field_sort_order(b.v) end

    local members_sorted = sort_by(wrapped(members), ss)

    -- Hide operators and serialization methods
    local is_hidden = function(member)
      if member.comment and member.comment:find("DEPRECATED") then return true end
      if member.name:find("^__") then return true end
      if member.name == "serialize" then return true end
      if member.name == "deserialize" then return true end
      return false
    end

    for _, it in pairs(members_sorted) do
      if not is_hidden(it.v) then ret = ret .. fmt_one_member(typename, it.v) .. "\n" end
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

    local entries_sorted = sort_by(wrapped(entries_filtered), function(a, b) return a.v < b.v end)
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

> [!WARNING]
>
> In Lua, functions can be called with a `:` and pass the object itself as the first argument, eg:
>
> Members where this behaviour is intended to be used are marked as 🇲 Methods<br/>
> Their signature documentation hides the first argument to reflect that
>
> * Call 🇫 Function members with a `.`
> * Call 🇲 Method members with a `:`
>
> Alternatively, you can still call 🇲 Methods with a `.`, from the class type or the variable itself
> but a value of the given type must be passed as the first parameter (that is hidden)
>
> All of these do the same thing:
> * ```
>   print(Angle.from_radians(3):to_degrees())
>   ```
> * ```
>   print(Angle.to_degrees(Angle.from_radians(3)))
>   ```
> * ```
>   local a = Angle.from_radians(3)
>   print(a:to_degrees())
>   ```
> * ```
>   local a = Angle.from_radians(3)
>   print(a.to_degrees(a))
>   ```
> * ```
>   local a = Angle.from_radians(3)
>   print(Angle.to_degrees(a))
>   ```

[generate_docs]: https://github.com/cataclysmbn/Cataclysm-BN/blob/main/data/raw/generate_docs.lua
]]

  local dt = catadoc

  local types_table = dt["#types"]

  local types_sorted = sort_by(wrapped(types_table))
  for _, it in pairs(types_sorted) do
    local typename = it.k
    local dt_type = it.v
    local type_comment = dt_type.type_comment
    ret = ret .. ("## %s {%s}\n"):format(typename, slug_for(typename))

    if type_comment then
      ret = ret
        .. string_concat_matches(
          type_comment,
          "[^\r\n]+",
          "  \n",
          function(m) return "> " .. linkify_types(m, true) end
        )
        .. "\n"
    end

    local bases = dt_type["#bases"]
    local ctors = dt_type["#construct"]
    local members = dt_type["#member"]

    ret = ret
      .. ("### Bases {#sol::%s::@bases}\n"):format(typename)
      .. fmt_bases(typename, bases)
      .. "\n"
      .. ("### Constructors {#sol::%s::@ctors}\n"):format(typename)
      .. fmt_constructors(typename, ctors)
      .. "\n"
      .. ("### Members {#sol::%s::@members}\n"):format(typename)
      .. fmt_members(typename, members)
      .. "\n"
  end

  ret = ret .. "# Enums\n\n"

  local enums_table = dt["#enums"]

  local enums_sorted = sort_by(wrapped(enums_table))
  for _, it in pairs(enums_sorted) do
    local typename = it.k
    local dt_type = it.v
    ret = ret .. ("## %s {%s}\n"):format(typename, slug_for(typename))

    local entries = dt_type["entries"]

    ret = ret .. "### Entries\n" .. fmt_enum_entries(typename, entries) .. "\n"
  end

  ret = ret .. "# Libraries\n\n"

  local libs_table = dt["#libs"]

  local libs_sorted = sort_by(wrapped(libs_table))
  for _, it in pairs(libs_sorted) do
    local typename = it.k
    local dt_lib = it.v
    local lib_comment = dt_lib.lib_comment
    ret = ret .. ("## %s {%s}\n"):format(typename, slug_for(typename))

    if lib_comment then ret = ret .. lib_comment .. "\n" end

    local members = dt_lib["#member"]

    ret = ret .. "### Members\n" .. fmt_members(nil, members) .. "\n"
  end

  return ret
end
