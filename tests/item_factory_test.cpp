#include "item_factory_test.h"

#include "catch/catch.hpp"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "type_id.h"

#include <fstream>
#include <memory>
#include <ranges>

namespace item_factory_test {

TEST_CASE("load_tool_use_action", "[item_factory]") {
    std::ifstream file(TEST_ITEM_JSON_FILENAME, std::ios::binary);
    JsonIn jsin(file);
    JsonObject all_test_items(jsin.get_object());
    Item_factory test_factory;

    JsonObject test_extends = all_test_items.get_object(TEST_MEMBER_EXTENDS);
    test_extends.allow_omitted_members();
    JsonObject test_sets = all_test_items.get_object(TEST_MEMBER_SETS);
    test_sets.allow_omitted_members();

    SECTION(BASE_MEMBER_IS_STRING) {
        JsonObject test_base_item = all_test_items.get_object(BASE_MEMBER_IS_STRING);
        test_base_item.allow_omitted_members();

        test_factory.load_tool(test_base_item, TEST_ITEM_SOURCE);

        SECTION(TEST_MEMBER_EXTENDS) {
            test_factory.load_tool(test_extends, TEST_ITEM_SOURCE);
            test_factory.finalize();

            const itype* test_item = get_item(test_factory, BASE_ITEM_NAME);
            REQUIRE(test_item->can_use(BASE_IUSE_ID));

            test_item = get_item(test_factory, TEST_ITEM_NAME);
            REQUIRE(test_item->can_use(BASE_IUSE_ID));
            REQUIRE(test_item->can_use(TEST_IUSE_ID));
        }

        SECTION(TEST_MEMBER_SETS) {
            test_factory.load_tool(test_sets, TEST_ITEM_SOURCE);
            test_factory.finalize();

            const itype* test_item = get_item(test_factory, BASE_ITEM_NAME);
            REQUIRE(test_item->can_use(BASE_IUSE_ID));

            test_item = get_item(test_factory, TEST_ITEM_NAME);
            REQUIRE(!test_item->can_use(BASE_IUSE_ID));
            REQUIRE(test_item->can_use(TEST_IUSE_ID));
        }

        // TODO: Implement delete if needed?
    }

    SECTION(BASE_MEMBER_IS_ARRAY) {
        JsonObject test_base_item = all_test_items.get_object(BASE_MEMBER_IS_ARRAY);
        test_base_item.allow_omitted_members();

        test_factory.load_tool(test_base_item, TEST_ITEM_SOURCE);

        SECTION(TEST_MEMBER_EXTENDS) {
            test_factory.load_tool(test_extends, TEST_ITEM_SOURCE);
            test_factory.finalize();

            const itype* test_item = get_item(test_factory, BASE_ITEM_NAME);
            REQUIRE(test_item->can_use(BASE_IUSE_ID));

            test_item = get_item(test_factory, TEST_ITEM_NAME);
            REQUIRE(test_item->can_use(BASE_IUSE_ID));
            REQUIRE(test_item->can_use(TEST_IUSE_ID));
        }

        SECTION(TEST_MEMBER_SETS) {
            test_factory.load_tool(test_sets, TEST_ITEM_SOURCE);
            test_factory.finalize();

            const itype* test_item = get_item(test_factory, BASE_ITEM_NAME);
            REQUIRE(test_item->can_use(BASE_IUSE_ID));

            test_item = get_item(test_factory, TEST_ITEM_NAME);
            REQUIRE(!test_item->can_use(BASE_IUSE_ID));
            REQUIRE(test_item->can_use(TEST_IUSE_ID));
        }

        // TODO: Implement delete if needed?
    }

    SECTION(BASE_MEMBER_MISSING) {
        JsonObject test_base_item = all_test_items.get_object(BASE_MEMBER_MISSING);
        test_base_item.allow_omitted_members();

        test_factory.load_tool(test_base_item, TEST_ITEM_SOURCE);

        SECTION(TEST_MEMBER_EXTENDS) {
            test_factory.load_tool(test_extends, TEST_ITEM_SOURCE);
            test_factory.finalize();

            const itype* test_item = get_item(test_factory, BASE_ITEM_NAME);
            REQUIRE(test_item->has_use() == false);

            test_item = get_item(test_factory, TEST_ITEM_NAME);
            REQUIRE(test_item->can_use(TEST_IUSE_ID));
        }

        SECTION(TEST_MEMBER_SETS) {
            test_factory.load_tool(test_sets, TEST_ITEM_SOURCE);
            test_factory.finalize();

            const itype* test_item = get_item(test_factory, BASE_ITEM_NAME);
            REQUIRE(test_item->has_use() == false);

            test_item = get_item(test_factory, TEST_ITEM_NAME);
            REQUIRE(test_item->can_use(TEST_IUSE_ID));
        }
    }


    all_test_items.allow_omitted_members();
    file.close();
}

/// Checks that every item with prying quality offers the "Pry crate, window or door" action.
/// Door examine and the activate menu look only for that action, so a crash axe (prying 2) could
/// not pry (#10930). Walks every loaded item type with PRY >= 1, e.g. crash_axe, and expects the
/// CROWBAR use.
TEST_CASE("items_with_prying_quality_can_pry", "[item_factory]") {
    using namespace std::views;
    const auto pry = quality_id("PRY");
    const auto has_pry = [&](const itype* type) -> bool {
        return type->qualities.contains(pry) && type->qualities.at(pry) > 0;
    };
    for (const auto* type : item_controller->all() | filter(has_pry)) {
        CAPTURE(type->get_id());
        CHECK(type->can_use("CROWBAR"));
    }
}

auto get_item(Item_factory& test_factory, const std::string& name) -> const itype* {
    std::vector<const itype*> test_items = test_factory.find([&name](const itype& item) {
        return item.get_id() == itype_id(name);
    });

    REQUIRE(test_items.size() == 1);
    return test_items[0];
}

} // namespace item_factory_test
