#include "catch/catch.hpp"

#include <memory>
#include <string>
#include <vitamin.h>

#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "flag.h"
#include "item.h"
#include "itype.h"
#include "state_helpers.h"
#include "type_id.h"

static const vitamin_id vitamin_egg_allergen( "egg_allergen" );
static const vitamin_id vitamin_fruit_allergen( "fruit_allergen" );
static const vitamin_id vitamin_human_flesh_vitamin( "human_flesh_vitamin" );
static const vitamin_id vitamin_junk_allergen( "junk_allergen" );
static const vitamin_id vitamin_meat_allergen( "meat_allergen" );
static const vitamin_id vitamin_milk_allergen( "milk_allergen" );
static const vitamin_id vitamin_nut_allergen( "nut_allergen" );
static const vitamin_id vitamin_veggy_allergen( "veggy_allergen" );
static const vitamin_id vitamin_wheat_allergen( "wheat_allergen" );

// Character "edible rating" tests, covering the `can_eat` and `will_eat` functions

static void expect_can_eat( avatar &dummy, item &food )
{
    auto rate_can = dummy.can_eat( food );
    CHECK( rate_can.success() );
    CHECK( rate_can.str().empty() );
    CHECK( rate_can.value() == edible_rating::edible );
}

static void expect_cannot_eat( avatar &dummy, item &food, std::string expect_reason,
                               edible_rating expect_rating = edible_rating::inedible )
{
    auto rate_can = dummy.can_eat( food );
    CHECK_FALSE( rate_can.success() );
    CHECK( rate_can.str() == expect_reason );
    CHECK( rate_can.value() == expect_rating );
}

static void expect_will_eat( avatar &dummy, item &food, std::string expect_consequence,
                             edible_rating expect_rating )
{
    // will_eat returns the first element in a vector of ret_val<edible_rating>
    // this function only looks at the first (since each is tested separately)
    auto rate_will = dummy.will_eat( food );
    CHECK( rate_will.str() == expect_consequence );
    CHECK( rate_will.value() == expect_rating );
}

static avatar prepare_avatar()
{
    avatar dummy;
    dummy.set_stored_kcal( 100 );
    return dummy;
}

TEST_CASE( "cannot eat non-comestible", "[can_eat][will_eat][edible_rating][nonfood]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();
    GIVEN( "something not edible" ) {
        item *rag = item::spawn_temporary( "rag" );

        THEN( "they cannot eat it" ) {
            expect_cannot_eat( dummy, *rag, "That doesn't look edible.", edible_rating::inedible );
        }
    }
}

TEST_CASE( "who can eat while underwater", "[can_eat][edible_rating][underwater]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();
    item &sushi = *item::spawn_temporary( "sushi_fishroll" );
    item &water = *item::spawn_temporary( "water_clean" );

    GIVEN( "character is underwater" ) {
        dummy.set_underwater( true );
        REQUIRE( dummy.is_underwater() );

        WHEN( "they have no special mutation" ) {
            REQUIRE_FALSE( dummy.has_trait( trait_id( "WATERSLEEP" ) ) );

            THEN( "they cannot eat" ) {
                expect_cannot_eat( dummy, sushi, "You can't do that while underwater." );
            }

            THEN( "they cannot drink" ) {
                expect_cannot_eat( dummy, water, "You can't do that while underwater." );
            }
        }

        WHEN( "they have the Aqueous Repose mutation" ) {
            dummy.toggle_trait( trait_id( "WATERSLEEP" ) );
            REQUIRE( dummy.has_trait( trait_id( "WATERSLEEP" ) ) );

            THEN( "they can eat" ) {
                expect_can_eat( dummy, sushi );
            }

            /*
            // FIXME: This fails, despite what it says in the mutation description
            THEN( "they cannot drink" ) {
                expect_cannot_eat( dummy, water, "You can't do that while underwater." );
            }
            */
        }
    }
}

TEST_CASE( "who can eat inedible animal food", "[can_eat][edible_rating][inedible][animal]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    // Note: There are similar conditions for INEDIBLE food with FELINE or LUPINE flags, but
    // "birdfood" and "cattlefodder" are the only INEDIBLE items that exist in the game.

    GIVEN( "food for animals" ) {
        item &birdfood = *item::spawn_temporary( "birdfood" );
        item &cattlefodder = *item::spawn_temporary( "cattlefodder" );

        REQUIRE( birdfood.has_flag( flag_INEDIBLE ) );
        REQUIRE( cattlefodder.has_flag( flag_INEDIBLE ) );

        REQUIRE( birdfood.has_flag( flag_BIRD ) );
        REQUIRE( cattlefodder.has_flag( flag_CATTLE ) );

        WHEN( "character is not bird or cattle" ) {
            REQUIRE_FALSE( dummy.has_trait( trait_id( "THRESH_BIRD" ) ) );
            REQUIRE_FALSE( dummy.has_trait( trait_id( "THRESH_CATTLE" ) ) );

            std::string expect_reason = "That doesn't look edible to you.";

            THEN( "they cannot eat bird food" ) {
                expect_cannot_eat( dummy, birdfood, "That doesn't look edible to you." );
            }

            THEN( "they cannot eat cattle fodder" ) {
                expect_cannot_eat( dummy, cattlefodder, "That doesn't look edible to you." );
            }
        }

        WHEN( "character is a bird" ) {
            dummy.toggle_trait( trait_id( "THRESH_BIRD" ) );
            REQUIRE( dummy.has_trait( trait_id( "THRESH_BIRD" ) ) );

            THEN( "they can eat bird food" ) {
                expect_can_eat( dummy, birdfood );
            }
        }

        WHEN( "character is cattle" ) {
            dummy.toggle_trait( trait_id( "THRESH_CATTLE" ) );
            REQUIRE( dummy.has_trait( trait_id( "THRESH_CATTLE" ) ) );

            THEN( "they can eat cattle fodder" ) {
                expect_can_eat( dummy, cattlefodder );
            }
        }
    }
}

TEST_CASE( "what herbivores can eat", "[can_eat][edible_rating][herbivore]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    GIVEN( "character is an herbivore" ) {
        dummy.toggle_trait( trait_id( "HERBIVORE" ) );
        REQUIRE( dummy.has_trait( trait_id( "HERBIVORE" ) ) );

        std::string expect_reason = "The thought of eating that makes you feel sick.";

        THEN( "they cannot eat meat" ) {
            item &meat = *item::spawn_temporary( "meat_cooked" );
            REQUIRE( meat.has_vitamin( vitamin_meat_allergen ) );

            expect_cannot_eat( dummy, meat, expect_reason, edible_rating::inedible_mutation );
        }

        THEN( "they cannot eat eggs" ) {
            item &eggs = *item::spawn_temporary( "scrambled_eggs" );
            REQUIRE( eggs.has_vitamin( vitamin_egg_allergen ) );

            expect_cannot_eat( dummy, eggs, expect_reason, edible_rating::inedible_mutation );
        }
    }
}

TEST_CASE( "what carnivores can eat", "[can_eat][edible_rating][carnivore]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    GIVEN( "character is a carnivore" ) {
        dummy.toggle_trait( trait_id( "CARNIVORE" ) );
        REQUIRE( dummy.has_trait( trait_id( "CARNIVORE" ) ) );

        std::string expect_reason = "Eww.  Inedible plant stuff!";

        THEN( "they cannot eat veggies" ) {
            item &veggy = *item::spawn_temporary( "veggy" );
            REQUIRE( veggy.has_vitamin( vitamin_veggy_allergen ) );

            expect_cannot_eat( dummy, veggy, expect_reason, edible_rating::inedible_mutation );
        }

        THEN( "they cannot eat fruit" ) {
            item &apple = *item::spawn_temporary( "apple" );
            REQUIRE( apple.has_vitamin( vitamin_fruit_allergen ) );

            expect_cannot_eat( dummy, apple, expect_reason, edible_rating::inedible_mutation );
        }

        THEN( "they cannot eat wheat" ) {
            item &bread = *item::spawn_temporary( "sourdough_bread" );
            REQUIRE( bread.has_vitamin( vitamin_wheat_allergen ) );

            expect_cannot_eat( dummy, bread, expect_reason, edible_rating::inedible_mutation );
        }

        THEN( "they cannot eat nuts" ) {
            item &nuts = *item::spawn_temporary( "pine_nuts" );
            REQUIRE( nuts.has_vitamin( vitamin_nut_allergen ) );

            expect_cannot_eat( dummy, nuts, expect_reason, edible_rating::inedible_mutation );
        }

        THEN( "they can eat junk food, but are allergic to it" ) {
            item &chocolate = *item::spawn_temporary( "chocolate" );
            REQUIRE( chocolate.has_vitamin( vitamin_junk_allergen ) );

            expect_can_eat( dummy, chocolate );
            expect_will_eat( dummy, chocolate, "Your stomach won't be happy (allergy).",
                             edible_rating::allergy );
        }
    }
}

TEST_CASE( "what you can eat with a mycus dependency", "[can_eat][edible_rating][mycus]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    GIVEN( "character is mycus-dependent" ) {
        dummy.toggle_trait( trait_id( "M_DEPENDENT" ) );
        REQUIRE( dummy.has_trait( trait_id( "M_DEPENDENT" ) ) );

        THEN( "they cannot eat normal food" ) {
            item &nuts = *item::spawn_temporary( "pine_nuts" );
            REQUIRE_FALSE( nuts.has_flag( flag_MYCUS_OK ) );

            expect_cannot_eat( dummy, nuts, "We can't eat that.  It's not right for us.",
                               edible_rating::inedible_mutation );
        }

        THEN( "they can eat mycus food" ) {
            item &berry = *item::spawn_temporary( "marloss_berry" );
            REQUIRE( berry.has_flag( flag_MYCUS_OK ) );

            expect_can_eat( dummy, berry );
        }
    }
}

TEST_CASE( "what you can drink with a proboscis", "[can_eat][edible_rating][proboscis]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    GIVEN( "character has a proboscis" ) {
        dummy.toggle_trait( trait_id( "PROBOSCIS" ) );
        REQUIRE( dummy.has_trait( trait_id( "PROBOSCIS" ) ) );

        // Cannot drink
        std::string expect_reason = "Ugh, you can't drink that!";

        GIVEN( "a drink that is 'eaten' (USE_EAT_VERB)" ) {
            item &soup = *item::spawn_temporary( "soup_veggy" );
            REQUIRE( soup.has_flag( flag_USE_EAT_VERB ) );

            THEN( "they cannot drink it" ) {
                expect_cannot_eat( dummy, soup, expect_reason, edible_rating::inedible_mutation );
            }
        }

        GIVEN( "food that must be chewed" ) {
            item &jihelucake = *item::spawn_temporary( "jihelucake" );
            REQUIRE( jihelucake.get_comestible()->comesttype == "FOOD" );

            THEN( "they cannot drink it" ) {
                expect_cannot_eat( dummy, jihelucake, expect_reason, edible_rating::inedible_mutation );
            }
        }

        // Can drink

        GIVEN( "a drink that is not 'eaten'" ) {
            item &broth = *item::spawn_temporary( "broth" );
            REQUIRE_FALSE( broth.has_flag( flag_USE_EAT_VERB ) );

            THEN( "they can drink it" ) {
                expect_can_eat( dummy, broth );
            }
        }

        GIVEN( "some medicine" ) {
            item &aspirin = *item::spawn_temporary( "aspirin" );
            REQUIRE( aspirin.is_medication() );

            THEN( "they can consume it" ) {
                expect_can_eat( dummy, aspirin );
            }
        }
    }
}

TEST_CASE( "can eat with nausea", "[will_eat][edible_rating][nausea]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();
    item &jihelucake = *item::spawn_temporary( "jihelucake" );
    const efftype_id effect_nausea( "nausea" );

    GIVEN( "character has nausea" ) {
        dummy.add_effect( effect_nausea, 10_minutes );
        REQUIRE( dummy.has_effect( effect_nausea ) );

        THEN( "they can eat food, but it nauseates them" ) {
            expect_can_eat( dummy, jihelucake );
            expect_will_eat( dummy, jihelucake,
                             "You still feel nauseous and will probably puke it all up again.",
                             edible_rating::nausea );
        }
    }
}

TEST_CASE( "can eat with allergies", "[will_eat][edible_rating][allergy]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();
    item &fruit = *item::spawn_temporary( "apple" );
    REQUIRE( fruit.has_vitamin( vitamin_fruit_allergen ) );

    GIVEN( "character hates fruit" ) {
        dummy.toggle_trait( trait_id( "ANTIFRUIT" ) );
        REQUIRE( dummy.has_trait( trait_id( "ANTIFRUIT" ) ) );

        THEN( "they can eat fruit, but won't like it" ) {
            expect_can_eat( dummy, fruit );
            expect_will_eat( dummy, fruit, "Your stomach won't be happy (allergy).", edible_rating::allergy );
        }
    }
}

TEST_CASE( "who will eat rotten food", "[will_eat][edible_rating][rotten]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    GIVEN( "food just barely rotten" ) {
        item &jihelucake_rotten = *item::spawn_temporary( "jihelucake" );
        jihelucake_rotten.set_relative_rot( 1.01 );
        REQUIRE( jihelucake_rotten.rotten() );

        WHEN( "character is normal" ) {
            REQUIRE_FALSE( dummy.has_trait( trait_id( "SAPROPHAGE" ) ) );
            REQUIRE_FALSE( dummy.has_trait( trait_id( "SAPROVORE" ) ) );

            THEN( "they can eat it, though they are disgusted by it" ) {
                expect_can_eat( dummy, jihelucake_rotten );

                auto conseq = dummy.will_eat( jihelucake_rotten, false );
                CHECK( conseq.value() == edible_rating::rotten );
                CHECK( conseq.str() == "This is rotten and smells awful!" );
            }
        }

        WHEN( "character is a saprovore" ) {
            dummy.toggle_trait( trait_id( "SAPROVORE" ) );
            REQUIRE( dummy.has_trait( trait_id( "SAPROVORE" ) ) );

            THEN( "they can eat it, and don't mind that it is rotten" ) {
                expect_can_eat( dummy, jihelucake_rotten );

                auto conseq = dummy.will_eat( jihelucake_rotten, false );
                CHECK( conseq.value() == edible_rating::edible );
                CHECK( conseq.str().empty() );
            }
        }

        WHEN( "character is a saprophage" ) {
            dummy.toggle_trait( trait_id( "SAPROPHAGE" ) );
            REQUIRE( dummy.has_trait( trait_id( "SAPROPHAGE" ) ) );

            THEN( "they can eat it, but would prefer it to be more rotten" ) {
                expect_can_eat( dummy, jihelucake_rotten );

                auto conseq = dummy.will_eat( jihelucake_rotten, false );
                CHECK( conseq.value() == edible_rating::allergy_weak );
                CHECK( conseq.str() == "Your stomach won't be happy (not rotten enough)." );
            }
        }
    }
}

TEST_CASE( "who will eat human flesh", "[will_eat][edible_rating][cannibal]" )
{
    clear_all_state();
    avatar dummy = prepare_avatar();

    GIVEN( "some human flesh" ) {
        item &flesh = *item::spawn_temporary( "human_flesh" );
        REQUIRE( flesh.has_vitamin( vitamin_human_flesh_vitamin ) );

        WHEN( "character is not a cannibal" ) {
            REQUIRE_FALSE( dummy.has_trait( trait_id( "CANNIBAL" ) ) );

            THEN( "they can eat it, but feel sick about it" ) {
                expect_can_eat( dummy, flesh );
                expect_will_eat( dummy, flesh, "The thought of eating human flesh makes you feel sick.",
                                 edible_rating::cannibalism );
            }
        }

        WHEN( "character is a cannibal" ) {
            dummy.toggle_trait( trait_id( "CANNIBAL" ) );
            REQUIRE( dummy.has_trait( trait_id( "CANNIBAL" ) ) );

            THEN( "they can eat it without any qualms" ) {
                expect_can_eat( dummy, flesh );
                expect_will_eat( dummy, flesh, "", edible_rating::edible );
            }
        }
    }
}
