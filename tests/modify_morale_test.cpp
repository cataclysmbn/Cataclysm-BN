#include "catch/catch.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "morale_types.h"
#include "point.h"
#include "type_id.h"

static const trait_id trait_ANTIFRUIT( "ANTIFRUIT" );
static const trait_id trait_ANTIJUNK( "ANTIJUNK" );
static const trait_id trait_ANTIWHEAT( "ANTIWHEAT" );
static const trait_id trait_BADTEMPER( "BADTEMPER" );
static const trait_id trait_CANNIBAL( "CANNIBAL" );
static const trait_id trait_CARNIVORE( "CARNIVORE" );
static const trait_id trait_CLAWS( "CLAWS" );
static const trait_id trait_FAT( "FAT" );
static const trait_id trait_HIBERNATE( "HIBERNATE" );
static const trait_id trait_LACTOSE( "LACTOSE" );
static const trait_id trait_LARGE( "LARGE" );
static const trait_id trait_MEATARIAN( "MEATARIAN" );
static const trait_id trait_PADDED_FEET( "PADDED_FEET" );
static const trait_id trait_PROJUNK( "PROJUNK" );
static const trait_id trait_PROJUNK2( "PROJUNK2" );
static const trait_id trait_PSYCHOPATH( "PSYCHOPATH" );
static const trait_id trait_SAPIOVORE( "SAPIOVORE" );
static const trait_id trait_SAPROPHAGE( "SAPROPHAGE" );
static const trait_id trait_THRESH_URSINE( "THRESH_URSINE" );
static const trait_id trait_URSINE_EYE( "URSINE_EYE" );
static const trait_id trait_URSINE_FUR( "URSINE_FUR" );
static const trait_id trait_VEGETARIAN( "VEGETARIAN" );

static const vitamin_id vitamin_human_flesh_vitamin( "human_flesh_vitamin" );

// Test cases for `Character::modify_morale` defined in `src/consumption.cpp`

TEST_CASE( "food enjoyability", "[food][modify_morale][fun]" )
{
    avatar dummy;
    std::pair<int, int> fun;

    GIVEN( "food with positive fun" ) {
        detached_ptr<item> det = item::spawn( "jihelucake", calendar::start_of_cataclysm,
                                              item::default_charges_tag{} );
        item &jihelucake = *det;
        dummy.i_add( std::move( det ) );
        fun = dummy.fun_for( jihelucake );
        REQUIRE( fun.first > 0 );

        THEN( "character gets a morale bonus becase it tastes good" ) {
            dummy.modify_morale( jihelucake );
            CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) >= fun.first );
        }
    }

    GIVEN( "food with negative fun" ) {
        detached_ptr<item> det = item::spawn( "garlic", calendar::start_of_cataclysm,
                                              item::default_charges_tag{} );
        item &garlic = *det;
        dummy.i_add( std::move( det ) );
        fun = dummy.fun_for( garlic );
        REQUIRE( fun.first < 0 );

        THEN( "character gets a morale penalty because it tastes bad" ) {
            dummy.modify_morale( garlic );
            CHECK( dummy.get_morale( MORALE_FOOD_BAD ) <= fun.first );
        }
    }
}

TEST_CASE( "drugs", "[food][modify_morale][drug]" )
{
    avatar dummy;

    const std::vector<std::string> drugs_to_test = {
        {
            "gum",
            "caff_gum",
            "nic_gum",
            "cigar",
            // TODO: Change those to effects
            "lsd",
            "heroin",
            "morphine"
        }
    };

    GIVEN( "avatar has baseline morale" ) {
        dummy.clear_morale();
        dummy.clear_effects();
        REQUIRE( dummy.get_morale_level() == 0 );

        for( std::string drug_name : drugs_to_test ) {
            item &drug = *item::spawn_temporary( drug_name );
            std::pair<int, int> fun = dummy.fun_for( drug );

            REQUIRE( fun.first > 0 );

            THEN( "they enjoy " + drug_name ) {
                dummy.modify_morale( drug );
                CHECK( dummy.get_morale_level() >= fun.first );
            }
        }
    }
}

TEST_CASE( "cannibalism", "[food][modify_morale][cannibal]" )
{
    avatar dummy;

    detached_ptr<item> det = item::spawn( "bone_human", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &human = *det;
    dummy.i_add( std::move( det ) );
    REQUIRE( human.has_vitamin( vitamin_human_flesh_vitamin ) );

    GIVEN( "character is not a cannibal or sapiovore" ) {
        REQUIRE_FALSE( dummy.has_trait( trait_CANNIBAL ) );
        REQUIRE_FALSE( dummy.has_trait( trait_SAPIOVORE ) );

        THEN( "they get a large morale penalty for eating humans" ) {
            dummy.clear_morale();
            dummy.modify_morale( human );
            CHECK( dummy.get_morale( MORALE_CANNIBAL ) <= -60 );
        }

        WHEN( "character is a psychopath" ) {
            dummy.toggle_trait( trait_PSYCHOPATH );
            REQUIRE( dummy.has_trait( trait_PSYCHOPATH ) );

            THEN( "their morale is unffected by eating humans" ) {
                dummy.clear_morale();
                dummy.modify_morale( human );
                CHECK( dummy.get_morale( MORALE_CANNIBAL ) == 0 );
            }
        }
    }

    WHEN( "character is a cannibal" ) {
        dummy.toggle_trait( trait_CANNIBAL );
        REQUIRE( dummy.has_trait( trait_CANNIBAL ) );

        THEN( "they get a morale bonus for eating humans" ) {
            dummy.clear_morale();
            dummy.modify_morale( human );
            CHECK( dummy.get_morale( MORALE_CANNIBAL ) >= 20 );
        }
    }
}

TEST_CASE( "sweet junk food", "[food][modify_morale][junk][sweet]" )
{
    avatar dummy;

    GIVEN( "some sweet junk food" ) {
        detached_ptr<item> det = item::spawn( "neccowafers", calendar::start_of_cataclysm,
                                              item::default_charges_tag{} );
        item &necco = *det;
        dummy.i_add( std::move( det ) );

        WHEN( "character has a sweet tooth" ) {
            dummy.toggle_trait( trait_PROJUNK );
            REQUIRE( dummy.has_trait( trait_PROJUNK ) );

            THEN( "they get a morale bonus from its sweetness" ) {
                dummy.clear_morale();
                dummy.modify_morale( necco );
                CHECK( dummy.get_morale( MORALE_SWEETTOOTH ) >= 5 );
                CHECK( dummy.get_morale( MORALE_SWEETTOOTH ) <= 5 );

                AND_THEN( "they enjoy it" ) {
                    CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 0 );
                }
            }
        }

        WHEN( "character is sugar-loving" ) {
            dummy.toggle_trait( trait_PROJUNK2 );
            REQUIRE( dummy.has_trait( trait_PROJUNK2 ) );

            THEN( "they get a significant morale bonus from its sweetness" ) {
                dummy.clear_morale();
                dummy.modify_morale( necco );
                CHECK( dummy.get_morale( MORALE_SWEETTOOTH ) >= 10 );
                CHECK( dummy.get_morale( MORALE_SWEETTOOTH ) <= 50 );

                AND_THEN( "they enjoy it" ) {
                    CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 0 );
                }
            }
        }

        WHEN( "character is a carnivore" ) {
            dummy.toggle_trait( trait_CARNIVORE );
            REQUIRE( dummy.has_trait( trait_CARNIVORE ) );

            THEN( "they get an morale penalty due to indigestion" ) {
                dummy.clear_morale();
                dummy.modify_morale( necco );
                CHECK( dummy.get_morale( MORALE_NO_DIGEST ) <= -25 );
            }
        }
    }
}

TEST_CASE( "junk food that is not ingested", "[modify_morale][junk][no_ingest]" )
{
    avatar dummy;

    detached_ptr<item> det = item::spawn( "caff_gum", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &caff_gum = *det;
    dummy.i_add( std::move( det ) );

    // This is a regression test for gum having "junk" material, and being
    // treated as junk food (despite not being ingested). At the time of
    // writing this test, gum and caffeinated gum are made of "junk", and thus
    // are treated as junk food, but might not always be so. Here we set the
    // relevant flags to cover the scenario we're interested in, namely any
    // comestible having both "junk" and "no ingest" flags.
    caff_gum.set_flag( flag_ALLERGEN_JUNK );
    caff_gum.set_flag( flag_NO_INGEST );

    REQUIRE( caff_gum.has_flag( flag_ALLERGEN_JUNK ) );
    REQUIRE( caff_gum.has_flag( flag_NO_INGEST ) );

    GIVEN( "character has a sweet tooth" ) {
        dummy.toggle_trait( trait_PROJUNK );
        REQUIRE( dummy.has_trait( trait_PROJUNK ) );

        THEN( "they do not get an extra morale bonus for chewing gum" ) {
            dummy.clear_morale();
            dummy.modify_morale( caff_gum );
            CHECK( dummy.get_morale( MORALE_SWEETTOOTH ) == 0 );

            AND_THEN( "they still enjoy it" ) {
                CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 0 );
            }
        }
    }

    GIVEN( "character is sugar-loving" ) {
        dummy.toggle_trait( trait_PROJUNK2 );
        REQUIRE( dummy.has_trait( trait_PROJUNK2 ) );

        THEN( "they do not get an extra morale bonus for chewing gum" ) {
            dummy.clear_morale();
            dummy.modify_morale( caff_gum );
            CHECK( dummy.get_morale( MORALE_SWEETTOOTH ) == 0 );

            AND_THEN( "they still enjoy it" ) {
                CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 0 );
            }
        }
    }

    GIVEN( "character has junk food intolerance" ) {
        dummy.toggle_trait( trait_ANTIJUNK );
        REQUIRE( dummy.has_trait( trait_ANTIJUNK ) );

        THEN( "they do not get a morale penalty for chewing gum" ) {
            dummy.clear_morale();
            dummy.modify_morale( caff_gum );
            CHECK( dummy.get_morale( MORALE_ANTIJUNK ) == 0 );

            AND_THEN( "they still enjoy it" ) {
                CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 0 );
            }
        }
    }
}

TEST_CASE( "food allergies and intolerances", "[food][modify_morale][allergy]" )
{
    avatar dummy;
    int penalty = -75;

    GIVEN( "character is vegetarian" ) {
        dummy.toggle_trait( trait_VEGETARIAN );
        REQUIRE( dummy.has_trait( trait_VEGETARIAN ) );

        THEN( "they get a morale penalty for eating meat" ) {
            detached_ptr<item> det = item::spawn( "meat", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &meat = *det;
            dummy.i_add( std::move( det ) );
            REQUIRE( meat.has_flag( flag_ALLERGEN_MEAT ) );
            dummy.clear_morale();
            dummy.modify_morale( meat );
            CHECK( dummy.get_morale( MORALE_VEGETARIAN ) <= penalty );
        }
    }

    GIVEN( "character is lactose intolerant" ) {
        dummy.toggle_trait( trait_LACTOSE );
        REQUIRE( dummy.has_trait( trait_LACTOSE ) );

        THEN( "they get a morale penalty for drinking milk" ) {
            detached_ptr<item> det = item::spawn( "milk", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &milk = *det;
            dummy.i_add( std::move( det ) );
            REQUIRE( milk.has_flag( flag_ALLERGEN_MILK ) );
            dummy.clear_morale();
            dummy.modify_morale( milk );
            CHECK( dummy.get_morale( MORALE_LACTOSE ) <= penalty );
        }
    }

    GIVEN( "character is grain intolerant" ) {
        dummy.toggle_trait( trait_ANTIWHEAT );
        REQUIRE( dummy.has_trait( trait_ANTIWHEAT ) );

        THEN( "they get a morale penalty for eating wheat" ) {
            detached_ptr<item> det = item::spawn( "wheat", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &wheat = *det;
            dummy.i_add( std::move( det ) );
            REQUIRE( wheat.has_flag( flag_ALLERGEN_WHEAT ) );
            dummy.clear_morale();
            dummy.modify_morale( wheat );
            CHECK( dummy.get_morale( MORALE_ANTIWHEAT ) <= penalty );
        }
    }

    GIVEN( "character hates vegetables" ) {
        dummy.toggle_trait( trait_MEATARIAN );
        REQUIRE( dummy.has_trait( trait_MEATARIAN ) );

        THEN( "they get a morale penalty for eating vegetables" ) {
            detached_ptr<item> det = item::spawn( "broccoli", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &veggy = *det;
            dummy.i_add( std::move( det ) );
            REQUIRE( veggy.has_flag( flag_ALLERGEN_VEGGY ) );
            dummy.clear_morale();
            dummy.modify_morale( veggy );
            CHECK( dummy.get_morale( MORALE_MEATARIAN ) <= penalty );
        }
    }

    GIVEN( "character hates fruit" ) {
        dummy.toggle_trait( trait_ANTIFRUIT );
        REQUIRE( dummy.has_trait( trait_ANTIFRUIT ) );

        THEN( "they get a morale penalty for eating fruit" ) {
            detached_ptr<item> det = item::spawn( "apple", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &fruit = *det;
            dummy.i_add( std::move( det ) );
            REQUIRE( fruit.has_flag( flag_ALLERGEN_FRUIT ) );
            dummy.clear_morale();
            dummy.modify_morale( fruit );
            CHECK( dummy.get_morale( MORALE_ANTIFRUIT ) <= penalty );
        }
    }

    GIVEN( "character has a junk food intolerance" ) {
        dummy.toggle_trait( trait_ANTIJUNK );
        REQUIRE( dummy.has_trait( trait_ANTIJUNK ) );

        THEN( "they get a morale penalty for eating junk food" ) {
            detached_ptr<item> det = item::spawn( "neccowafers", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &junk = *det;
            dummy.i_add( std::move( det ) );
            REQUIRE( junk.has_flag( flag_ALLERGEN_JUNK ) );
            dummy.clear_morale();
            dummy.modify_morale( junk );
            CHECK( dummy.get_morale( MORALE_ANTIJUNK ) <= penalty );
        }
    }
}

TEST_CASE( "saprophage character", "[food][modify_morale][saprophage]" )
{
    avatar dummy;

    GIVEN( "character is a saprophage, preferring rotted food" ) {
        dummy.clear_morale();
        dummy.toggle_trait( trait_SAPROPHAGE );
        REQUIRE( dummy.has_trait( trait_SAPROPHAGE ) );

        AND_GIVEN( "some rotten chewable food" ) {
            detached_ptr<item> det = item::spawn( "jihelucake", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &jihelucake = *det;
            dummy.i_add( std::move( det ) );
            // food rot > 1.0 is rotten
            jihelucake.set_relative_rot( 1.5 );
            REQUIRE( jihelucake.rotten() );

            THEN( "they enjoy it" ) {
                dummy.modify_morale( jihelucake );
                CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 10 );
            }
        }

        AND_GIVEN( "some fresh chewable food" ) {
            detached_ptr<item> det = item::spawn( "jihelucake", calendar::start_of_cataclysm,
                                                  item::default_charges_tag{} );
            item &jihelucake = *det;
            dummy.i_add( std::move( det ) );
            // food rot < 0.1 is fresh
            jihelucake.set_relative_rot( 0.0 );
            REQUIRE( jihelucake.is_fresh() );

            THEN( "they get a morale penalty due to indigestion" ) {
                dummy.modify_morale( jihelucake );
                CHECK( dummy.get_morale( MORALE_NO_DIGEST ) <= -25 );
            }
        }
    }
}

TEST_CASE( "ursine honey", "[food][modify_morale][ursine][honey]" )
{
    avatar dummy;

    detached_ptr<item> det = item::spawn( "honeycomb", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &honeycomb = *det;
    dummy.i_add( std::move( det ) );
    REQUIRE( honeycomb.has_flag( flag_URSINE_HONEY ) );

    GIVEN( "character is post-threshold ursine" ) {
        dummy.toggle_trait( trait_THRESH_URSINE );
        REQUIRE( dummy.has_trait( trait_THRESH_URSINE ) );

        AND_GIVEN( "they have a lot of ursine mutations" ) {
            dummy.mutate_towards( trait_FAT );
            dummy.mutate_towards( trait_LARGE );
            dummy.mutate_towards( trait_CLAWS );
            dummy.mutate_towards( trait_BADTEMPER );
            dummy.mutate_towards( trait_HIBERNATE );
            dummy.mutate_towards( trait_URSINE_FUR );
            dummy.mutate_towards( trait_URSINE_EYE );
            dummy.mutate_towards( trait_PADDED_FEET );
            REQUIRE( dummy.mutation_category_level[mutation_category_id( "URSINE" )] > 40 );

            THEN( "they get an extra honey morale bonus for eating it" ) {
                dummy.modify_morale( honeycomb );
                CHECK( dummy.get_morale( MORALE_HONEY ) > 0 );

                AND_THEN( "they enjoy it" ) {
                    CHECK( dummy.get_morale( MORALE_FOOD_GOOD ) > 0 );
                }
            }
        }
    }
}
