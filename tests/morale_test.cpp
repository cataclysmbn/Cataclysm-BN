#include "catch/catch.hpp"

#include "bodypart.h"
#include "item.h"
#include "morale.h"
#include "morale_types.h"
#include "calendar.h"
#include "type_id.h"

const efftype_id drunk( "test_drunk" );
const efftype_id hangover( "test_hangover" );

const std::vector<bodypart_str_id> temperature_body_parts = {{
        bodypart_str_id( "torso" ),
        bodypart_str_id( "head" ),
        bodypart_str_id( "eyes" ),
        bodypart_str_id( "mouth" ),
        bodypart_str_id( "arm_l" ),
        bodypart_str_id( "arm_r" ),
        bodypart_str_id( "leg_l" ),
        bodypart_str_id( "leg_r" ),
        bodypart_str_id( "hand_l" ),
        bodypart_str_id( "hand_r" ),
        bodypart_str_id( "foot_l" ),
        bodypart_str_id( "foot_r" ),
    }
};

TEST_CASE( "player_morale", "[morale]" )
{
    static const efftype_id effect_cold( "cold" );
    static const efftype_id effect_hot( "hot" );
    static const efftype_id effect_took_prozac( "took_prozac" );

    player_morale m;

    GIVEN( "an empty morale" ) {
        CHECK( m.get_level() == 0 );
    }

    GIVEN( "temporary morale (food)" ) {
        m.add( MORALE_FOOD_GOOD, 20, 40, 20_turns, 10_turns );
        m.add( MORALE_FOOD_BAD, -10, -20, 20_turns, 10_turns );

        CHECK( m.get( MORALE_FOOD_GOOD ) == 20 );
        CHECK( m.get( MORALE_FOOD_BAD ) == -10 );
        CHECK( m.get_level() == 10 );

        WHEN( "it decays" ) {
            AND_WHEN( "it's just started" ) {
                m.decay( 10_turns );
                CHECK( m.get( MORALE_FOOD_GOOD ) == 20 );
                CHECK( m.get( MORALE_FOOD_BAD ) == -10 );
                CHECK( m.get_level() == 10 );
            }
            AND_WHEN( "it's halfway there" ) {
                m.decay( 15_turns );
                CHECK( m.get( MORALE_FOOD_GOOD ) == 10 );
                CHECK( m.get( MORALE_FOOD_BAD ) == -5 );
                CHECK( m.get_level() == 5 );
            }
            AND_WHEN( "it's finished" ) {
                m.decay( 20_turns );
                CHECK( m.get( MORALE_FOOD_GOOD ) == 0 );
                CHECK( m.get( MORALE_FOOD_BAD ) == 0 );
                CHECK( m.get_level() == 0 );
            }
        }

        WHEN( "it gets deleted" ) {
            AND_WHEN( "good one gets deleted" ) {
                m.remove( MORALE_FOOD_GOOD );
                CHECK( m.get_level() == -10 );
            }
            AND_WHEN( "bad one gets deleted" ) {
                m.remove( MORALE_FOOD_BAD );
                CHECK( m.get_level() == 20 );
            }
            AND_WHEN( "both get deleted" ) {
                m.remove( MORALE_FOOD_BAD );
                m.remove( MORALE_FOOD_GOOD );
                CHECK( m.get_level() == 0 );
            }
        }

        WHEN( "it gets cleared" ) {
            m.clear();
            CHECK( m.get_level() == 0 );
        }

        WHEN( "it's added/subtracted (no cap)" ) {
            m.add( MORALE_FOOD_GOOD, 10, 40, 20_turns, 10_turns, false );
            m.add( MORALE_FOOD_BAD, -10, -20, 20_turns, 10_turns, false );

            CHECK( m.get( MORALE_FOOD_GOOD ) == 22 );
            CHECK( m.get( MORALE_FOOD_BAD ) == -14 );
            CHECK( m.get_level() == 8 );

        }

        WHEN( "it's added/subtracted (with a cap)" ) {
            m.add( MORALE_FOOD_GOOD, 5, 10, 20_turns, 10_turns, true );
            m.add( MORALE_FOOD_BAD, -5, -10, 20_turns, 10_turns, true );

            CHECK( m.get( MORALE_FOOD_GOOD ) == 10 );
            CHECK( m.get( MORALE_FOOD_BAD ) == -10 );
            CHECK( m.get_level() == 0 );
        }
    }

    GIVEN( "persistent morale" ) {
        m.set_permanent( MORALE_PERM_MASOCHIST, 5 );

        CHECK( m.get( MORALE_PERM_MASOCHIST ) == 5 );

        WHEN( "it decays" ) {
            m.decay( 100_turns );
            THEN( "nothing happens" ) {
                CHECK( m.get( MORALE_PERM_MASOCHIST ) == 5 );
                CHECK( m.get_level() == 5 );
            }
        }
    }

    GIVEN( "OPTIMISTIC trait" ) {
        m.on_mutation_gain( trait_id( "OPTIMISTIC" ) );
        CHECK( m.get( MORALE_PERM_OPTIMIST ) == 9 );
        CHECK( m.get_level() == 10 );

        WHEN( "lost the trait" ) {
            m.on_mutation_loss( trait_id( "OPTIMISTIC" ) );
            CHECK( m.get( MORALE_PERM_OPTIMIST ) == 0 );
            CHECK( m.get_level() == 0 );
        }
    }

    GIVEN( "BADTEMPER trait" ) {
        m.on_mutation_gain( trait_id( "BADTEMPER" ) );
        CHECK( m.get( MORALE_PERM_BADTEMPER ) == -9 );
        CHECK( m.get_level() == -10 );

        WHEN( "lost the trait" ) {
            m.on_mutation_loss( trait_id( "BADTEMPER" ) );
            CHECK( m.get( MORALE_PERM_BADTEMPER ) == 0 );
            CHECK( m.get_level() == 0 );
        }
    }

    GIVEN( "killed an innocent" ) {
        m.add( MORALE_KILLED_INNOCENT, -100 );

        WHEN( "took prozac" ) {
            m.on_effect_int_change( effect_took_prozac, 1 );

            THEN( "it's not so bad" ) {
                CHECK( m.get_level() == -25 );

                AND_WHEN( "the effect ends" ) {
                    m.on_effect_int_change( effect_took_prozac, 0 );

                    THEN( "guilt returns" ) {
                        CHECK( m.get_level() == -100 );
                    }
                }
            }
        }
    }

    GIVEN( "a set of super fancy bride's clothes" ) {
        // legs, torso | 8 + 2 | 10
        // eyes, mouth | 4 + 2 | 6
        // feet        | 1 + 2 | 3
        const item &dress_wedding = *item::spawn_temporary( "dress_wedding", calendar::start_of_cataclysm );
        const item &veil_wedding = *item::spawn_temporary( "veil_wedding", calendar::start_of_cataclysm );
        const item &heels = *item::spawn_temporary( "heels", calendar::start_of_cataclysm );

        m.on_item_wear( dress_wedding );
        m.on_item_wear( veil_wedding );
        m.on_item_wear( heels );

        WHEN( "not a stylish person" ) {
            m.on_mutation_gain( trait_id( "UNSTYLISH" ) );
            THEN( "just don't care (even if man)" ) {
                CHECK( m.get_level() == 0 );
            }

            AND_WHEN( "not anymore" ) {
                m.on_mutation_loss( trait_id( "UNSTYLISH" ) );
                CHECK( m.get_level() == 19 );
            }
        }

        WHEN( "a stylish person" ) {

            CHECK( m.get_level() == 19 );

            AND_WHEN( "gets naked" ) {
                m.on_item_takeoff( heels ); // the queen took off her sandal ...
                CHECK( m.get_level() == 16 );
                m.on_item_takeoff( veil_wedding );
                CHECK( m.get_level() == 10 );
                m.on_item_takeoff( dress_wedding );
                CHECK( m.get_level() == 0 );
            }
            AND_WHEN( "wearing yet another wedding gown" ) {
                m.on_item_wear( dress_wedding );
                THEN( "it adds nothing" ) {
                    CHECK( m.get_level() == 19 );

                    AND_WHEN( "taking it off" ) {
                        THEN( "your fanciness remains the same" ) {
                            CHECK( m.get_level() == 19 );
                        }
                    }
                }
            }
            AND_WHEN( "tries to be even fancier" ) {
                const item &watch = *item::spawn_temporary( "sf_watch", calendar::start_of_cataclysm );
                m.on_item_wear( watch );
                THEN( "there's a limit" ) {
                    CHECK( m.get_level() == 20 );
                }
            }
        }
    }

    GIVEN( "masochist trait" ) {
        m.on_mutation_gain( trait_id( "MASOCHIST" ) );

        CHECK( m.get( MORALE_PERM_MASOCHIST ) == 0 );

        WHEN( "in pain" ) {
            m.on_stat_change( "perceived_pain", 10 );
            CHECK( m.get( MORALE_PERM_MASOCHIST ) == 4 );
        }

        WHEN( "in an insufferable pain" ) {
            m.on_stat_change( "perceived_pain", 120 );
            THEN( "there's a limit" ) {
                CHECK( m.get( MORALE_PERM_MASOCHIST ) == 25 );
            }
        }
    }

    GIVEN( "cenobite trait" ) {
        m.on_mutation_gain( trait_id( "CENOBITE" ) );

        CHECK( m.get( MORALE_PERM_MASOCHIST ) == 0 );

        WHEN( "in an insufferable pain" ) {
            m.on_stat_change( "perceived_pain", 120 );

            THEN( "there's no limit" ) {
                CHECK( m.get( MORALE_PERM_MASOCHIST ) == 48 );
            }

            AND_WHEN( "took prozac" ) {
                m.on_effect_int_change( effect_took_prozac, 1 );
                THEN( "it spoils all fun" ) {
                    CHECK( m.get( MORALE_PERM_MASOCHIST ) == 16 );
                }
            }
        }
    }

    GIVEN( "a humanoid plant" ) {
        m.on_mutation_gain( trait_id( "PLANT" ) );
        m.on_mutation_gain( trait_id( "FLOWERS" ) );
        m.on_mutation_gain( trait_id( "ROOTS1" ) );

        CHECK( m.get( MORALE_PERM_CONSTRAINED ) == 0 );

        WHEN( "wearing a hat" ) {
            const item &hat = *item::spawn_temporary( "tinfoil_hat", calendar::start_of_cataclysm );

            m.on_item_wear( hat );
            THEN( "the flowers need sunlight" ) {
                CHECK( m.get( MORALE_PERM_CONSTRAINED ) == -10 );

                AND_WHEN( "taking it off again" ) {
                    m.on_item_takeoff( hat );
                    CHECK( m.get( MORALE_PERM_CONSTRAINED ) == 0 );
                }
            }
        }

        WHEN( "wearing a legpouch" ) {
            item &legpouch = *item::spawn_temporary( "legpouch", calendar::start_of_cataclysm );
            legpouch.set_side( side::LEFT );

            m.on_item_wear( legpouch );
            THEN( "half of the roots are suffering" ) {
                CHECK( m.get( MORALE_PERM_CONSTRAINED ) == -5 );
            }
        }

        WHEN( "wearing a pair of boots" ) {
            const item &boots = *item::spawn_temporary( "boots", calendar::start_of_cataclysm );

            m.on_item_wear( boots );
            THEN( "all of the roots are suffering" ) {
                CHECK( m.get( MORALE_PERM_CONSTRAINED ) == -10 );
            }

            AND_WHEN( "even more constrains" ) {
                const item &hat = *item::spawn_temporary( "tinfoil_hat", calendar::start_of_cataclysm );

                m.on_item_wear( hat );
                THEN( "it can't be worse" ) {
                    CHECK( m.get( MORALE_PERM_CONSTRAINED ) == -10 );
                }
            }
        }
    }

    GIVEN( "tough temperature conditions" ) {
        WHEN( "chilly" ) {
            for( auto bp : temperature_body_parts ) {
                m.on_effect_int_change( effect_cold, 1, bp );
            }

            AND_WHEN( "no time has passed" ) {
                CHECK( m.get_level() == 0 );
            }
            AND_WHEN( "1 turn has passed" ) {
                m.decay( 1_turns );
                CHECK( m.get_level() == -2 );
            }
            AND_WHEN( "2 turns have passed" ) {
                m.decay( 2_turns );
                CHECK( m.get_level() == -4 );
            }
            AND_WHEN( "3 turns have passed" ) {
                m.decay( 3_turns );
                CHECK( m.get_level() == -6 );
            }
            AND_WHEN( "6 minutes have passed" ) {
                m.decay( 6_minutes );
                CHECK( m.get_level() == -10 );
            }
        }

        WHEN( "cold" ) {
            for( auto bp : temperature_body_parts ) {
                m.on_effect_int_change( effect_cold, 2, bp );
            }

            AND_WHEN( "no time has passed" ) {
                CHECK( m.get_level() == 0 );
            }
            AND_WHEN( "1 turn has passed" ) {
                m.decay( 1_turns );
                CHECK( m.get_level() == -2 );
            }
            AND_WHEN( "9 turns have passed" ) {
                m.decay( 9_turns );
                CHECK( m.get_level() == -18 );
            }
            AND_WHEN( "1 minute has passed" ) {
                m.decay( 1_minutes );
                CHECK( m.get_level() == -20 );
            }
            AND_WHEN( "6 minutes have passed" ) {
                m.decay( 6_minutes );
                CHECK( m.get_level() == -20 );
            }
            AND_WHEN( "warmed up afterwards" ) {
                for( auto bp : temperature_body_parts ) {
                    m.on_effect_int_change( effect_cold, 0, bp );
                }

                m.decay( 1_minutes );
                CHECK( m.get_level() == 0 );
            }
        }

        WHEN( "warm" ) {
            for( auto bp : temperature_body_parts ) {
                m.on_effect_int_change( effect_hot, 1, bp );
            }

            AND_WHEN( "no time has passed" ) {
                CHECK( m.get_level() == 0 );
            }
            AND_WHEN( "1 turn has passed" ) {
                m.decay( 1_turns );
                CHECK( m.get_level() == -2 );
            }
            AND_WHEN( "2 turns have passed" ) {
                m.decay( 2_turns );
                CHECK( m.get_level() == -4 );
            }
            AND_WHEN( "3 turns have passed" ) {
                m.decay( 3_turns );
                CHECK( m.get_level() == -6 );
            }
            AND_WHEN( "6 minutes have passed" ) {
                m.decay( 6_minutes );
                CHECK( m.get_level() == -10 );
            }
        }

        WHEN( "hot" ) {
            for( auto bp : temperature_body_parts ) {
                m.on_effect_int_change( effect_hot, 2, bp );
            }

            AND_WHEN( "no time has passed" ) {
                CHECK( m.get_level() == 0 );
            }
            AND_WHEN( "1 turn has passed" ) {
                m.decay( 1_turns );
                CHECK( m.get_level() == -2 );
            }
            AND_WHEN( "9 turns have passed" ) {
                m.decay( 9_turns );
                CHECK( m.get_level() == -18 );
            }
            AND_WHEN( "1 minute has passed" ) {
                m.decay( 1_minutes );
                CHECK( m.get_level() == -20 );
            }
            AND_WHEN( "6 minutes have passed" ) {
                m.decay( 6_minutes );
                CHECK( m.get_level() == -20 );
            }
            AND_WHEN( "cooled afterwards" ) {
                for( auto bp : temperature_body_parts ) {
                    m.on_effect_int_change( effect_hot, 0, bp );
                }

                m.decay( 1_minutes );
                CHECK( m.get_level() == 0 );
            }
        }
    }

    GIVEN( "stacking of bonuses" ) {
        m.add( MORALE_FOOD_GOOD, 10, 40, 20_turns, 10_turns );
        m.add( MORALE_BOOK, 10, 40, 20_turns, 10_turns );

        CHECK( m.get( MORALE_FOOD_GOOD ) == 10 );
        CHECK( m.get( MORALE_BOOK ) == 10 );
        CHECK( m.get_level() == 14 );

        WHEN( "a bonus is added" ) {
            m.set_permanent( MORALE_PERM_MASOCHIST, 50 );

            CHECK( m.get( MORALE_FOOD_GOOD ) == 10 );
            CHECK( m.get( MORALE_BOOK ) == 10 );
            CHECK( m.get( MORALE_PERM_MASOCHIST ) == 50 );

            CHECK( m.get_level() == 51 );
        }

        WHEN( "a negative bonus is added" ) {
            m.add( MORALE_WET, -10, -40, 20_turns, 10_turns );

            CHECK( m.get( MORALE_FOOD_GOOD ) == 10 );
            CHECK( m.get( MORALE_BOOK ) == 10 );
            CHECK( m.get( MORALE_WET ) == -10 );

            CHECK( m.get_level() == 4 );
        }

        WHEN( "a bonus is lost" ) {
            m.remove( MORALE_BOOK );

            CHECK( m.get( MORALE_FOOD_GOOD ) == 10 );
            CHECK( m.get( MORALE_BOOK ) == 0 );

            CHECK( m.get_level() == 10 );
        }
    }
}

TEST_CASE( "player_morale_from_effects", "[morale][effect]" )
{
    player_morale m;

    GIVEN( "effects with morale bonuses and penalties" ) {
        m.on_effect_int_change( drunk, 1 );
        REQUIRE( m.get_level() == 5 );

        WHEN( "intensity increases" ) {
            m.on_effect_int_change( drunk, 2 );
            THEN( "so does the bonus" ) {
                CHECK( m.get_level() == 15 );
                AND_WHEN( "intensity increases again" ) {
                    m.on_effect_int_change( drunk, 3 );
                    THEN( "bonus increases by expected amount" ) {
                        CHECK( m.get_level() == 25 );
                    }
                }
            }
        }

        WHEN( "intensity increases and drops back" ) {
            m.on_effect_int_change( drunk, 2 );
            m.on_effect_int_change( drunk, 1 );
            THEN( "bonus stays the same" ) {
                CHECK( m.get_level() == 5 );
            }
        }

        WHEN( "a second effect with the same morale type, but negative sign, is added" ) {
            m.on_effect_int_change( hangover, 1 );
            THEN( "the morale effect is the sum of highest bonus and highest penalty" ) {
                CHECK( m.get_level() == -5 );
                AND_WHEN( "first effect increases in intensity" ) {
                    m.on_effect_int_change( drunk, 2 );
                    THEN( "morale effect rises by expected amount" ) {
                        CHECK( m.get_level() == 5 );
                    }
                }
                AND_WHEN( "second effect increases in intensity" ) {
                    m.on_effect_int_change( hangover, 2 );
                    THEN( "morale effect drops by expected amount" ) {
                        CHECK( m.get_level() == -25 );
                    }
                }
                AND_WHEN( "both effect increase in intensity" ) {
                    m.on_effect_int_change( drunk, 2 );
                    m.on_effect_int_change( hangover, 2 );
                    THEN( "the morale effect is the sum of the new highest bonus and new highest penalty" ) {
                        CHECK( m.get_level() == -15 );
                    }
                }
            }
        }

        WHEN( "a second effect with the same morale type is added and the first effect is removed" ) {
            m.on_effect_int_change( hangover, 1 );
            m.on_effect_int_change( drunk, 0 );
            THEN( "the morale effect is equal to that of the second effect" ) {
                CHECK( m.get_level() == -10 );
            }
        }
    }

    GIVEN( "effect with bonus, item with bonus and independent bonus" ) {
        m.on_effect_int_change( drunk, 1 );
        m.add( MORALE_FOOD_GOOD, 10, 0, 1_hours, 1_hours, false,
               *itype_id( "test_pine_nuts" ) );
        m.add( MORALE_BOOK, 10 );
        THEN( "square root of sum of squares rule applies" ) {
            CHECK( m.get_level() == 15 );
        }
    }
}
