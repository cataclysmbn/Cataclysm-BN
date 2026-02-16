#pragma once

#include "item_factory.h"
#include "iuse.h"
#include "json.h"

namespace item_factory_test
{
const std::string TEST_ITEM_JSON_FILENAME = "tests/data/item_factory.json";

const std::string BASE_MEMBER_IS_STRING = "base_member_is_string";
const std::string BASE_MEMBER_IS_ARRAY = "base_member_is_array";
const std::string BASE_MEMBER_MISSING = "base_member_missing";

const std::string TEST_MEMBER_EXTENDS = "test_member_extends";
const std::string TEST_MEMBER_SETS = "test_member_sets";

const std::string BASE_ITEM_NAME = "base_tool";
const std::string TEST_ITEM_NAME = "test_tool";
const std::string TEST_ITEM_SOURCE = "testing";
const std::string USE_ACTION = "use_action";

const std::string BASE_IUSE_ID = "CAMERA";
const std::string TEST_IUSE_ID = "RPGDIE";

const itype *get_item( Item_factory &test_factory, const std::string &name );
} // namespace item_factory_test
