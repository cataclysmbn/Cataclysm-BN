#include "catch/catch.hpp"

#include "item_factory_test.h"

#include <fstream>
#include <memory>

#include "item_factory.h"
#include "json.h"

namespace item_factory_test
{

TEST_CASE( "load_tool_use_action", "[item_factory]" )
{
    std::ifstream file( TEST_ITEM_JSON_FILENAME, std::ios::binary );
    JsonIn jsin( file );
    JsonObject all_test_items( jsin.get_object() );
    Item_factory test_factory;

    JsonObject test_extends = all_test_items.get_object( TEST_MEMBER_EXTENDS );
    test_extends.allow_omitted_members();
    JsonObject test_sets = all_test_items.get_object( TEST_MEMBER_SETS );
    test_sets.allow_omitted_members();

    SECTION( BASE_MEMBER_IS_STRING ) {
        JsonObject test_base_item = all_test_items.get_object( BASE_MEMBER_IS_STRING );
        test_base_item.allow_omitted_members();

        test_factory.load_tool( test_base_item, TEST_ITEM_SOURCE );

        SECTION( TEST_MEMBER_EXTENDS ) {
            test_factory.load_tool( test_extends, TEST_ITEM_SOURCE );
            test_factory.finalize();

            const itype *test_item = get_item( test_factory, BASE_ITEM_NAME );
            REQUIRE( test_item->can_use( BASE_IUSE_ID ) );

            test_item = get_item( test_factory, TEST_ITEM_NAME );
            REQUIRE( test_item->can_use( BASE_IUSE_ID ) );
            REQUIRE( test_item->can_use( TEST_IUSE_ID ) );
        }

        SECTION( TEST_MEMBER_SETS ) {
            test_factory.load_tool( test_sets, TEST_ITEM_SOURCE );
            test_factory.finalize();

            const itype *test_item = get_item( test_factory, BASE_ITEM_NAME );
            REQUIRE( test_item->can_use( BASE_IUSE_ID ) );

            test_item = get_item( test_factory, TEST_ITEM_NAME );
            REQUIRE( !test_item->can_use( BASE_IUSE_ID ) );
            REQUIRE( test_item->can_use( TEST_IUSE_ID ) );
        }

        // TODO: Implement delete if needed?
    }

    SECTION( BASE_MEMBER_IS_ARRAY ) {
        JsonObject test_base_item = all_test_items.get_object( BASE_MEMBER_IS_ARRAY );
        test_base_item.allow_omitted_members();

        test_factory.load_tool( test_base_item, TEST_ITEM_SOURCE );

        SECTION( TEST_MEMBER_EXTENDS ) {
            test_factory.load_tool( test_extends, TEST_ITEM_SOURCE );
            test_factory.finalize();

            const itype *test_item = get_item( test_factory, BASE_ITEM_NAME );
            REQUIRE( test_item->can_use( BASE_IUSE_ID ) );

            test_item = get_item( test_factory, TEST_ITEM_NAME );
            REQUIRE( test_item->can_use( BASE_IUSE_ID ) );
            REQUIRE( test_item->can_use( TEST_IUSE_ID ) );
        }

        SECTION( TEST_MEMBER_SETS ) {
            test_factory.load_tool( test_sets, TEST_ITEM_SOURCE );
            test_factory.finalize();

            const itype *test_item = get_item( test_factory, BASE_ITEM_NAME );
            REQUIRE( test_item->can_use( BASE_IUSE_ID ) );

            test_item = get_item( test_factory, TEST_ITEM_NAME );
            REQUIRE( !test_item->can_use( BASE_IUSE_ID ) );
            REQUIRE( test_item->can_use( TEST_IUSE_ID ) );
        }

        // TODO: Implement delete if needed?
    }

    SECTION( BASE_MEMBER_MISSING ) {
        JsonObject test_base_item = all_test_items.get_object( BASE_MEMBER_MISSING );
        test_base_item.allow_omitted_members();

        test_factory.load_tool( test_base_item, TEST_ITEM_SOURCE );

        SECTION( TEST_MEMBER_EXTENDS ) {
            test_factory.load_tool( test_extends, TEST_ITEM_SOURCE );
            test_factory.finalize();

            const itype *test_item = get_item( test_factory, BASE_ITEM_NAME );
            REQUIRE( test_item->has_use( ) == false );

            test_item = get_item( test_factory, TEST_ITEM_NAME );
            REQUIRE( test_item->can_use( TEST_IUSE_ID ) );
        }

        SECTION( TEST_MEMBER_SETS ) {
            test_factory.load_tool( test_sets, TEST_ITEM_SOURCE );
            test_factory.finalize();

            const itype *test_item = get_item( test_factory, BASE_ITEM_NAME );
            REQUIRE( test_item->has_use( ) == false );

            test_item = get_item( test_factory, TEST_ITEM_NAME );
            REQUIRE( test_item->can_use( TEST_IUSE_ID ) );
        }
    }


    all_test_items.allow_omitted_members();
    file.close();
}

const itype *get_item( Item_factory &test_factory, const std::string &name )
{
    std::vector <const itype *> test_items = test_factory.find( [&name]( const itype & item ) {
        return item.get_id() == itype_id( name );
    } );

    REQUIRE( test_items.size() == 1 );
    return test_items[0];
}

} // namespace item_factory_test
