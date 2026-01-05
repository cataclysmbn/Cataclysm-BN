-- Test dotted require with lib namespace
local helper = require("lib.require_test.helper")

test_data.result_add = helper.add(10, 20)
test_data.result_mul = helper.multiply(3, 7)
