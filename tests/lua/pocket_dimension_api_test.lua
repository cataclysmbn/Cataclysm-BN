local dimension_id = test_data["target_dimension_id"]
local target = test_data["target_omt"]
local return_ms = test_data["return_ms"]
local bounds_min = test_data["bounds_min_omt"]
local bounds_max = test_data["bounds_max_omt"]
local outside = test_data["outside_omt"]
local outside_ms = test_data["outside_ms"]
local outside_local = test_data["outside_local"]
local terrain = { { { "forest", "field" }, { "field", "forest" } } }

---@param overrides? DimensionTravelOptions
local function options(overrides)
  local opts = {
    dimension_id = dimension_id,
    target_omt = target,
    world_type = "pocket_dimension",
    bounds_min_omt = bounds_min,
    bounds_max_omt = bounds_max,
  }
  for key, value in pairs(overrides or {}) do
    opts[key] = value
  end
  return opts
end

---@param expected_dimension string
---@param expected_pos? TripointAbsMs
local function check_location(expected_dimension, expected_pos)
  assert(gapi.get_current_dimension_id() == expected_dimension)
  assert(gapi.get_map():get_bound_dimension() == expected_dimension)
  if expected_pos then assert(gapi.get_avatar():abs_pos() == expected_pos) end
end

---@param opts DimensionTravelOptions
local function enter(opts)
  assert(gapi.place_player_dimension_at(opts))
  check_location(opts.dimension_id or "")
end

local function return_home()
  enter({ dimension_id = "", target_ms = return_ms })
  check_location("", return_ms)
end

---@param id string
local function reject_cleanup(id)
  local current = gapi.get_current_dimension_id()
  local pos = gapi.get_avatar():abs_pos()
  for _, cleanup in ipairs({ gapi.delete_dimension, gapi.reset_dimension }) do
    assert(not cleanup(id))
    check_location(current, pos)
  end
end

check_location("", return_ms)
return_home() -- Same-dimension travel is a successful no-op.
local mistyped_fields = {
  dimension_id = 123,
  target_ms = {},
  target_omt = "bad",
  world_type = 123,
  bounds_min_omt = {},
  bounds_max_omt = {},
  boundary_terrain = 123,
  boundary_overmap_terrain = 123,
  pregen_special_id = 123,
  pregen_special_omt = {},
}
for field, value in pairs(mistyped_fields) do
  local opts = { dimension_id = "", target_ms = return_ms, target_omt = return_ms:to_omt() }
  opts[field] = value
  local ok, result = pcall(gapi.place_player_dimension_at, opts)
  assert(ok and result == false, field)
  check_location("", return_ms)
end
local partial_bounds = options()
partial_bounds.bounds_max_omt = nil
local invalid = {
  missing_target = { dimension_id = "" },
  partial_bounds = partial_bounds,
  missing_special = options({ pregen_special_id = "lua_test_missing_special" }),
  missing_terrain = options({ overmap_terrain = { { { "lua_test_missing_omt" } } } }),
  layout_without_bounds = { dimension_id = dimension_id, target_omt = target, overmap_terrain = terrain },
  oversized_layout = options({ bounds_max_omt = bounds_min, overmap_terrain = terrain }),
  named_layout = options({ overmap_terrain = { label = { { "forest" } } } }),
  sparse_layout = options({ overmap_terrain = { [2] = { { "forest" } } } }),
  zero_index_layout = options({ overmap_terrain = { [0] = { { "field" } } } }),
  negative_index_layout = options({ overmap_terrain = { [-1] = { { "field" } } } }),
  fractional_index_layout = options({ overmap_terrain = { [1.5] = { { "field" } } } }),
  boolean_index_layout = options({ overmap_terrain = { [false] = { { "field" } } } }),
  non_table_layout = options({ overmap_terrain = false }),
  non_table_layer = options({ overmap_terrain = { false } }),
  non_table_row = options({ overmap_terrain = { { false } } }),
  non_string_cell = options({ overmap_terrain = { { { false } } } }),
  sparse_row = options({ overmap_terrain = { { { [2] = "field" } } } }),
  named_rows = options({ overmap_terrain = { { label = { "field" } } } }),
  empty_layer = options({ overmap_terrain = { {} } }),
  unsafe_id = options({ dimension_id = "lua/test" }),
  nul_id = options({ dimension_id = "lua\0test" }),
  dot_id = options({ dimension_id = "." }),
  reversed_bounds = options({ bounds_min_omt = bounds_max, bounds_max_omt = bounds_min }),
  outside_target = options({ target_omt = outside }),
  mismatched_targets = options({ target_ms = outside_ms }),
  overworld_bounds = {
    dimension_id = "",
    target_ms = return_ms,
    bounds_min_omt = bounds_min,
    bounds_max_omt = bounds_max,
  },
  missing_boundary_terrain = options({ boundary_terrain = "t_lua_test_missing_border" }),
  missing_boundary_overmap = options({ boundary_overmap_terrain = "lua_test_missing_border" }),
  outside_special = options({ pregen_special_id = "Riverside Dwelling", pregen_special_omt = outside }),
  overlapping_special = options({ pregen_special_id = "Riverside Dwelling", overmap_terrain = terrain }),
}
for name, opts in pairs(invalid) do
  assert(not gapi.place_player_dimension_at(opts), name)
  check_location("", return_ms)
end
for _, id in ipairs({ "", ".", "..", "lua_missing_pocket" }) do
  reject_cleanup(id)
end
assert(gapi.delete_dimension("lua_test_unloaded_delete"))
assert(gapi.reset_dimension("lua_test_unloaded_reset"))

enter(
  options({ dimension_id = dimension_id .. "_special", pregen_special_id = "Riverside Dwelling", overmap_terrain = {} })
)
return_home()
enter(options({ overmap_terrain = terrain }))
assert(not gapi.get_map():is_out_of_bounds(gapi.get_avatar():bub_pos()))
assert(gapi.get_map():is_out_of_bounds(outside_local))
return_home()
assert(not gapi.get_map():is_out_of_bounds(outside_local))
assert(not gapi.place_player_dimension_at({ dimension_id = dimension_id, target_omt = outside }))
check_location("", return_ms)

enter({ dimension_id = dimension_id, target_omt = bounds_max })
assert(gapi.get_map():is_out_of_bounds(outside_local))
local same_dimension_pos = gapi.get_avatar():abs_pos()
enter({ dimension_id = dimension_id, target_ms = return_ms })
check_location(dimension_id, same_dimension_pos)
reject_cleanup("")
reject_cleanup(dimension_id)

for _, operation in ipairs({ "reset_dimension", "delete_dimension" }) do
  return_home()
  assert(gapi[operation](dimension_id))
  check_location("", return_ms)
  local opts = operation == "reset_dimension" and { dimension_id = dimension_id, target_omt = target }
    or options({ overmap_terrain = terrain })
  enter(opts)
  assert(gapi.get_map():is_out_of_bounds(outside_local))
end
return_home()
