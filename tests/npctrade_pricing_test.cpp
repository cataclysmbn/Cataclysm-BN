#include "catch/catch.hpp"

#include <string>
#include <vector>

#include "calendar.h"
#include "item.h"
#include "npctrade.h"
#include "state_helpers.h"

// https://github.com/cataclysmbn/Cataclysm-BN/issues/6986
TEST_CASE( "low_price_materials_not_free", "[npc][trade][pricing]" )
{
    SECTION( "Materials with low per-unit prices should not be traded for free" ) {
        clear_all_state();

        // Test items that have very low per-unit prices
        auto test_items = std::vector<std::pair<std::string, int>> {
            { "solder_wire", 200 },      // 50 cent / 200 = 0.25 cent per unit
            { "material_quicklime", 50 } // 10 cent / 50 = 0.2 cent per unit
        };

        for( const auto &[name, amount] : test_items ) {
            SECTION( "Testing " + name ) {
                // Create item with default stack size
                auto test_material = item{ name, calendar::turn, amount };

                // Get the price for trade purposes
                auto price = test_material.price( true );

                // The total price should be positive
                CHECK( price > 0 );

                // Create item_pricing to test the per-unit pricing logic
                const auto pricing = item_pricing{ { &test_material }, price, 1 };

                // The per-unit price should not be zero
                CHECK( pricing.price > 0 );

                // Verify that even a single unit has a positive price
                item single_unit( name, calendar::turn, 1 );
                auto single_price = single_unit.price( true );

                // Even a single unit should have a minimum price of 1 cent
                CHECK( single_price > 0 );
            }
        }
    }
}
