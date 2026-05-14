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
