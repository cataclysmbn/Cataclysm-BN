-- Test relative require
local helper = require("./require_test/helper")

test_data.result_add = helper.add(2, 3)
test_data.result_mul = helper.multiply(4, 5)
