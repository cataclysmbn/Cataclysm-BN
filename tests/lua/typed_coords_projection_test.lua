local abs_ms = TripointAbsMs.new(25, 26, 2)
local abs_omt = abs_ms:to_omt()

local abs_sm = TripointAbsSm.new(361, 2, -1)
local quotient, remainder = coords.project_remain_om(abs_sm)
local combined = coords.project_combine(quotient, remainder)

test_data["to_omt"] = "TripointAbsOmt(" .. abs_omt:x() .. "," .. abs_omt:y() .. "," .. abs_omt:z() .. ")"
test_data["remain_quotient"] = "TripointAbsOm(" .. quotient:x() .. "," .. quotient:y() .. "," .. quotient:z() .. ")"
test_data["remain_remainder"] = "PointOmSm(" .. remainder:x() .. "," .. remainder:y() .. ")"
test_data["combined"] = "TripointAbsSm(" .. combined:x() .. "," .. combined:y() .. "," .. combined:z() .. ")"
test_data["distance"] = coords.rl_dist(abs_sm, combined + Point.new(3, 0))
