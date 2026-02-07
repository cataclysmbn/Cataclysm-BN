--[[
    Removes internal arguments like "<this_state>"
  ]]
---@param arg_list string[]
---@return string[]
function remove_hidden_args(arg_list)
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

function string_concat_matches(str, pat, sep, op)
  if str == nil or str == "" then return "" end
  local tbl = {}
  for match in string.gmatch(str, pat) do
    if op then match = op(match) end
    if match and match ~= "" then table.insert(tbl, match) end
  end
  return table.concat(tbl, sep)
end

function get_meta_params(meta)
  local tbl = {}
  if meta == nil or meta == "" then return tbl end
  for line in string.gmatch(meta, "[^\r\n]+") do
    local name = string.match(line, "^@param (.*)$")
    if name ~= nil then table.insert(tbl, name) end
  end
  return tbl
end

---@param a any
---@param b any
---@return boolean
-- NOTE: String comparison in Lua is locale-dependent (uses strcoll).
-- C locale is forced in generate_lua_docs() (catalua.cpp) for consistent ordering.
local function default_sort_fn(a, b)
  if a.k and b.k then return tostring(a.k) < tostring(b.k) end
  return tostring(a) < tostring(b)
end

---@generic T
---@param t T[]
---@param f? fun(a: T, b: T):boolean
---@return T[]
function sort_by(t, f)
  if not f then f = default_sort_fn end
  table.sort(t, f)
  return t
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

---comment
---@param str string
---@return table
function extract_cpp_types(str)
  local tbl = {}
  local tmp = str
  while true do
    local match = tmp:find("CppVal", 1, true)
    if match ~= nil then
      local before, token, after = extract_token(tmp, "<", ">", match)
      if before ~= "" then table.insert(tbl, { before, false }) end
      if token ~= "" then table.insert(tbl, { token, true }) end
      tmp = after
    else
      if tmp ~= "" then table.insert(tbl, { tmp, false }) end
      break
    end
  end
  return tbl
end

--- func desc
---@param str string
---@param open string
---@param close string
---@param from integer
---@return string before, string match, string after
function extract_token(str, open, close, from)
  local j = from
  local depth = 0
  while true do
    local openBracket = str:find(open, j, true) or #str
    local closeBracket = str:find(close, j, true) or #str

    if openBracket == nil and closeBracket == nil then break end

    if openBracket < closeBracket then
      depth = depth + 1
      j = openBracket + 1
    else
      depth = depth - 1
      j = closeBracket + 1
      if depth == 0 then break end
    end
  end
  local left = str:sub(1, from - 1)
  local mid = str:sub(from, j - 1)
  local right = str:sub(j, #str)

  return left, mid, right
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

-- Rudimentary mapping from C++/sol types to LuaLS types.
---@param cpp_type string
---@param keep_cppval boolean
---@return string
function map_cpp_type_to_lua(cpp_type, keep_cppval)
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

    if clean_type == "..." or (not keep_cppval and string.match(clean_type, "^CppVal")) then
      clean_type = "any"
    elseif string.match(clean_type, "^Vector%(%S+%)$") then
      clean_type = string.gsub(
        clean_type,
        "^Vector%((%S+)%)$",
        function(k) return ("%s[]"):format(map_cpp_type_to_lua(k, keep_cppval)) end
      )
    elseif string.match(clean_type, "^Set%(%S+%)$") then
      clean_type = string.gsub(
        clean_type,
        "^Set%((%S+)%)$",
        function(k) return ("%s[]"):format(map_cpp_type_to_lua(k, keep_cppval)) end
      )
    elseif string.match(clean_type, "^Array%(%S+,%d+%)$") then
      clean_type = string.gsub(
        clean_type,
        "^Array%((%S+),(%d+)%)$",
        function(k) return ("%s[]"):format(map_cpp_type_to_lua(k, keep_cppval)) end
      )
    elseif string.match(clean_type, "^Dict%(%S+,%S+%)$") then
      clean_type = string.gsub(
        clean_type,
        "^Dict%((%S+),(%S+)%)$",
        function(k, v) return ("table<%s, %s>"):format(map_cpp_type_to_lua(k, keep_cppval), map_cpp_type_to_lua(v, keep_cppval)) end
      )
    elseif string.match(clean_type, "^Opt%(%S+%)$") then
      clean_type = string.gsub(
        clean_type,
        "^Opt%((%S+)%)$",
        function(k) return ("%s?"):format(map_cpp_type_to_lua(k, keep_cppval)) end
      )
    elseif string.match(clean_type, "^Pair%(%S+,%S+%)$") then
      clean_type = string.gsub(
        clean_type,
        "^Pair%((%S+),(%S+)%)$",
        function(k, v) return ("(%s, %s)"):format(map_cpp_type_to_lua(k, keep_cppval), map_cpp_type_to_lua(v, keep_cppval)) end
      )
    end
    return clean_type or "any" -- Fallback to 'any' if nothing matches
  end
end
