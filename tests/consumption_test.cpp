#include "catch/catch.hpp"

#include "avatar.h"
#include "bionics.h"
#include "consumption.h"
#include "effect.h"
#include "item.h"
#include "mutation.h"

namespace consumption_test
{

static const bionic_id bio_digestion( "bio_digestion" );
static const efftype_id effect_foodpoison( "foodpoison" );
static const trait_id trait_POISRESIST( "POISRESIST" );
static const trait_id trait_EATDEAD( "EATDEAD" );

// Doesn't test the chance to get "real" poisoned; that depends on deterministically
// manipulating RNG and I don't want to figure that out yet.
TEST_CASE( "consume_poison", "[consumption]" )
{
    item test_poison{};
    avatar test_consumer{};

    SECTION( "not poison" ) {
        test_poison.poison = 0;
        consume_poison( test_consumer, test_poison );
        bool is_poisoned = static_cast<bool>( test_consumer.get_effect( effect_foodpoison ) );

        CHECK( !is_poisoned );
    }

    SECTION( "is poison" ) {
        test_poison.poison = 10;

        SECTION( "vulnerable" ) {
            consume_poison( test_consumer, test_poison );

            bool is_poisoned = static_cast<bool>( test_consumer.get_effect( effect_foodpoison ) );
            CHECK( is_poisoned );
        }

        SECTION( "immune" ) {
            SECTION( "POISRESIST trait" ) {
                test_consumer.set_mutation( trait_POISRESIST );
                consume_poison( test_consumer, test_poison );

                bool is_poisoned = static_cast<bool>( test_consumer.get_effect( effect_foodpoison ) );
                CHECK( !is_poisoned );
            }

            SECTION( "EATDEAD trait" ) {
                test_consumer.set_mutation( trait_EATDEAD );
                consume_poison( test_consumer, test_poison );

                bool is_poisoned = static_cast<bool>( test_consumer.get_effect( effect_foodpoison ) );
                CHECK( !is_poisoned );
            }

            SECTION( "bio_digestion bionic" ) {
                test_consumer.add_bionic( bio_digestion );
                consume_poison( test_consumer, test_poison );

                bool is_poisoned = static_cast<bool>( test_consumer.get_effect( effect_foodpoison ) );
                CHECK( !is_poisoned );
            }
        }
    }

}

} // consumption_test
