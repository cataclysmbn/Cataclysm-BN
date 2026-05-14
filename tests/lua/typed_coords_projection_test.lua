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
test_data["distance"] = coords.rl_dist(abs_sm, combined + Point.new(3, 0))

-- Validate the project_remain_omt example used in the typed-coordinates documentation.
-- Input: abs_ms(25, 26, 2).  Map squares per omt = 24.
-- Expected quotient:  TripointAbsOmt(1, 1, 2)  (floor(25/24)=1, floor(26/24)=1, z carried)
-- Expected remainder: PointOmtMs(1, 2)          (25-24=1, 26-24=2, origin from omt scale)
-- Combined must reconstruct the original coordinate exactly.
local doc_ms = coords.tripoint_abs_ms(25, 26, 2)
local doc_quotient, doc_remainder = doc_ms:project_remain_omt()
local doc_combined = coords.project_combine(doc_quotient, doc_remainder)

test_data["doc_remain_omt_quotient"] = tostring(doc_quotient)
test_data["doc_remain_omt_remainder"] = tostring(doc_remainder)
test_data["doc_remain_omt_combined"] = tostring(doc_combined)
