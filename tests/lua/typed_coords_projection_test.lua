local abs_ms = coords.tripoint_abs_ms(25, 26, 2)
local abs_omt = abs_ms:to_omt()
local named_abs_omt = TripointAbsMs.new(25, 26, 2):to_omt()

local abs_sm = coords.tripoint_abs_sm(361, 2, -1)
local quotient, remainder = coords.project_remain_om(abs_sm)
local combined = coords.project_combine(quotient, remainder)

test_data["to_omt"] = tostring(abs_omt)
test_data["named_to_omt"] = tostring(named_abs_omt)
test_data["remain_quotient"] = tostring(quotient)
test_data["remain_remainder"] = tostring(remainder)
test_data["combined"] = tostring(combined)
test_data["distance"] = coords.rl_dist(abs_sm, combined + coords.point_rel_sm(3, 0))

-- Validate the project_remain_omt example used in the typed-coordinates documentation.
-- Input: abs_ms(25, 26, 2).  Map squares per omt = 24.
-- Expected quotient:  TripointAbsOmt(1, 1, 2)  (floor(25/24)=1, floor(26/24)=1, z carried)
-- Expected remainder: PointOmtMs(1, 2)          (25-24=1, 26-24=2, origin from omt scale)
-- Combined must reconstruct the original coordinate exactly.
local doc_ms = coords.tripoint_abs_ms(25, 26, 2)
local doc_quotient, doc_remainder = doc_ms:project_remain_omt()
local doc_combined = coords.project_combine(doc_quotient, doc_remainder)
local doc_method_combined = doc_quotient:project_combine(doc_remainder)

test_data["doc_remain_omt_quotient"] = tostring(doc_quotient)
test_data["doc_remain_omt_remainder"] = tostring(doc_remainder)
test_data["doc_remain_omt_combined"] = tostring(doc_combined)
test_data["doc_remain_omt_method_combined"] = tostring(doc_method_combined)

test_data["typed_param"] = accept_abs_omt(coords.tripoint_abs_omt(1, 2, 3))

local raw_param_ok = pcall(function() return accept_abs_omt(Tripoint.new(1, 2, 3)) end)
test_data["raw_param_ok"] = raw_param_ok

local wrong_coord_ok = pcall(function() return accept_abs_omt(coords.tripoint_abs_ms(24, 48, 0)) end)
test_data["wrong_coord_ok"] = wrong_coord_ok

local raw_abs_omt = Tripoint.new(1, 2, 3):reinterpret_as("abs", "omt")
test_data["raw_reinterpreted"] = tostring(raw_abs_omt)
test_data["raw_reinterpreted_param"] = accept_abs_omt(raw_abs_omt)

local typed_reinterpreted = coords.tripoint_abs_ms(1, 2, 3):reinterpret_as("abs", "omt")
test_data["typed_reinterpreted"] = tostring(typed_reinterpreted)

local raw_delta = Point.new(3, 0):reinterpret_as("rel", "sm")
test_data["raw_delta_arithmetic"] = tostring(combined + raw_delta)
