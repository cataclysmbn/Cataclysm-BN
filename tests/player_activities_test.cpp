#include "catch/catch.hpp"
#include "map_helpers.h"
#include "player_helpers.h"

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "game.h"
#include "itype.h"
#include "iuse_actor.h"
#include "map.h"
#include "point.h"
#include "player_activity.h"

static const activity_id ACT_NULL = activity_id::NULL_ID();
static const activity_id ACT_BOLTCUTTING( "ACT_BOLTCUTTING" );
static const activity_id ACT_HACKSAW( "ACT_HACKSAW" );
static const activity_id ACT_OXYTORCH( "ACT_OXYTORCH" );

static const furn_str_id furn_t_test_f_boltcut1( "test_f_boltcut1" );
static const furn_str_id furn_t_test_f_boltcut2( "test_f_boltcut2" );
static const furn_str_id furn_t_test_f_boltcut3( "test_f_boltcut3" );
static const furn_str_id furn_t_test_f_hacksaw1( "test_f_hacksaw1" );
static const furn_str_id furn_t_test_f_hacksaw2( "test_f_hacksaw2" );
static const furn_str_id furn_t_test_f_hacksaw3( "test_f_hacksaw3" );
static const furn_str_id furn_t_test_f_oxytorch1( "test_f_oxytorch1" );
static const furn_str_id furn_t_test_f_oxytorch2( "test_f_oxytorch2" );
static const furn_str_id furn_t_test_f_oxytorch3( "test_f_oxytorch3" );

static const itype_id itype_oxyacetylene( "oxyacetylene" );
static const itype_id itype_test_boltcutter( "test_boltcutter" );
static const itype_id itype_test_boltcutter_elec( "test_boltcutter_elec" );
static const itype_id itype_test_hacksaw( "test_hacksaw" );
static const itype_id itype_test_hacksaw_elec( "test_hacksaw_elec" );
static const itype_id itype_test_oxytorch( "test_oxytorch" );

static const quality_id qual_SAW_M( "SAW_M" );
static const quality_id qual_WELD( "WELD" );

static const ter_str_id ter_test_t_oxytorch1( "test_t_oxytorch1" );
static const ter_str_id ter_test_t_oxytorch2( "test_t_oxytorch2" );

static const ter_str_id ter_test_t_boltcut1( "test_t_boltcut1" );
static const ter_str_id ter_test_t_boltcut2( "test_t_boltcut2" );
static const ter_str_id ter_test_t_hacksaw1( "test_t_hacksaw1" );
static const ter_str_id ter_test_t_hacksaw2( "test_t_hacksaw2" );

TEST_CASE( "boltcut", "[activity][boltcut]" )
{
    map &mp = get_map();
    avatar &dummy = get_avatar();

    auto setup_dummy = [&dummy]() -> item & {
        item &cutter = dummy.i_add( item::spawn( itype_test_boltcutter ) );
        dummy.wield( cutter );

        REQUIRE( dummy.primary_weapon().typeId() == itype_test_boltcutter );

        return cutter;
    };

    auto setup_activity = [&dummy]( item & cutter ) -> void {
        auto act = std::make_unique<boltcutting_activity_actor>(
            tripoint_zero, safe_reference<item>( cutter )
        );
        act->testing = true;
        dummy.assign_activity( std::make_unique<player_activity>( std::move( act ) ) );
    };

    SECTION( "boltcut start checks" ) {
        GIVEN( "a tripoint with nothing" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, t_null );
            REQUIRE( mp.ter( tripoint_zero ) == t_null );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            THEN( "boltcutting activity can't start" ) {
                CHECK( dummy.activity->id() == ACT_NULL );
            }
        }

        GIVEN( "a tripoint with invalid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, t_dirt );
            REQUIRE( mp.ter( tripoint_zero ) == t_dirt );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            THEN( "boltcutting activity can't start" ) {
                CHECK( dummy.activity->id() == ACT_NULL );
            }
        }

        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_boltcut1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_boltcut1 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            THEN( "boltcutting activity can start" ) {
                CHECK( dummy.activity->id() == ACT_BOLTCUTTING );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_boltcut1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_boltcut1 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            THEN( "boltcutting activity can start" ) {
                CHECK( dummy.activity->id() == ACT_BOLTCUTTING );
            }
        }

        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_boltcut1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_boltcut1 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );
            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );

            WHEN( "terrain has a duration of 10 seconds" ) {
                REQUIRE( ter_test_t_boltcut1->boltcut->duration() == 10_seconds );
                THEN( "moves_left is equal to 10 seconds" ) {
                    CHECK( dummy.activity->get_moves_left() == to_moves<int>( 10_seconds ) );
                }
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_boltcut1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_boltcut1 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );
            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );

            WHEN( "furniture has a duration of 5 seconds" ) {
                REQUIRE( furn_t_test_f_boltcut1->boltcut->duration() == 5_seconds );
                THEN( "moves_left is equal to 5 seconds" ) {
                    CHECK( dummy.activity->get_moves_left() == to_moves<int>( 5_seconds ) );
                }
            }
        }
    }

    SECTION( "boltcut turn checks" ) {
        GIVEN( "player is in mid activity" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_boltcut3 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_boltcut3 );

            item &boltcutter_elec = dummy.i_add( item::spawn( itype_test_boltcutter_elec,
                                                 calendar::start_of_cataclysm, 2 ) );
            dummy.wield( boltcutter_elec );

            REQUIRE( dummy.primary_weapon().typeId() == itype_test_boltcutter_elec );

            setup_activity( boltcutter_elec );
            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );
            process_activity( dummy );

            WHEN( "player runs out of charges" ) {
                REQUIRE( dummy.activity->id() == ACT_NULL );

                THEN( "player recharges with fuel" ) {
                    boltcutter_elec.ammo_set( boltcutter_elec.ammo_default(), -1 );

                    AND_THEN( "player can resume the activity" ) {
                        setup_activity( boltcutter_elec );
                        dummy.moves = dummy.get_speed();
                        dummy.activity->do_turn( dummy );
                        CHECK( dummy.activity->id() == ACT_BOLTCUTTING );
                        CHECK( dummy.activity->get_moves_left() < to_moves<int>
                               ( furn_t_test_f_boltcut3->boltcut->duration() ) );
                    }
                }
            }
        }
    }

    SECTION( "boltcut finish checks" ) {
        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_boltcut1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_boltcut1 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "terrain gets converted to new terrain type" ) {
                CHECK( mp.ter( tripoint_zero ) == t_dirt );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_boltcut1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_boltcut1 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "furniture gets converted to new furniture type" ) {
                CHECK( mp.furn( tripoint_zero ) == f_null );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_boltcut2 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_boltcut2 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "furniture gets converted to new furniture type" ) {
                CHECK( mp.furn( tripoint_zero ) == furn_t_test_f_boltcut1 );
            }
        }


        GIVEN( "a tripoint with a valid furniture with byproducts" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_boltcut2 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_boltcut2 );

            item &boltcutter = setup_dummy();
            setup_activity( boltcutter );

            REQUIRE( ter_test_t_boltcut2->boltcut->byproducts().size() == 2 );

            REQUIRE( dummy.activity->id() == ACT_BOLTCUTTING );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            const itype_id test_amount( "test_rock" );
            const itype_id test_random( "test_2x4" );

            WHEN( "boltcut acitivy finishes" ) {
                CHECK( dummy.activity->id() == ACT_NULL );

                THEN( "player receives the items" ) {
                    int count_amount = 0;
                    int count_random = 0;
                    for( const auto &it : get_map().i_at( tripoint_zero ) ) {
                        // can't use switch here
                        const itype_id it_id = it->typeId();
                        if( it_id == test_amount ) {
                            count_amount += it->charges;
                        } else if( it_id == test_random ) {
                            count_random += 1;
                        }
                    }

                    CHECK( count_amount == 3 );
                    CHECK( ( 7 <= count_random && count_random <= 9 ) );
                }
            }
        }
    }
}

TEST_CASE( "hacksaw", "[activity][hacksaw]" )
{
    map &mp = get_map();
    avatar &dummy = get_avatar();

    auto setup_dummy = [&dummy]() -> item & {
        item &saw = dummy.i_add( item::spawn( itype_test_hacksaw ) );
        dummy.wield( saw );

        REQUIRE( dummy.primary_weapon().typeId() == itype_test_hacksaw );
        REQUIRE( dummy.max_quality( qual_SAW_M ) == 10 );

        return saw;
    };

    auto setup_activity = [&dummy]( item & saw ) -> void {
        auto act = std::make_unique<hacksaw_activity_actor>(
            tripoint_zero, safe_reference<item>( saw )
        );
        act->testing = true;
        dummy.assign_activity( std::make_unique<player_activity>( std::move( act ) ) );
    };

    SECTION( "hacksaw start checks" ) {
        GIVEN( "a tripoint with nothing" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, t_null );
            REQUIRE( mp.ter( tripoint_zero ) == t_null );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            THEN( "hacksaw activity can't start" ) {
                CHECK( dummy.activity->id() == ACT_NULL );
            }
        }

        GIVEN( "a tripoint with invalid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, t_dirt );
            REQUIRE( mp.ter( tripoint_zero ) == t_dirt );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            THEN( "hacksaw activity can't start" ) {
                CHECK( dummy.activity->id() == ACT_NULL );
            }
        }

        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_hacksaw1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_hacksaw1 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            THEN( "hacksaw activity can start" ) {
                CHECK( dummy.activity->id() == ACT_HACKSAW );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_hacksaw1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_hacksaw1 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            THEN( "hacksaw activity can start" ) {
                CHECK( dummy.activity->id() == ACT_HACKSAW );
            }
        }

        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_hacksaw1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_hacksaw1 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );
            REQUIRE( dummy.activity->id() == ACT_HACKSAW );

            WHEN( "terrain has a duration of 10 minutes" ) {
                REQUIRE( ter_test_t_hacksaw1->hacksaw->duration() == 10_minutes );
                THEN( "moves_left is equal to 10 minutes" ) {
                    CHECK( dummy.activity->get_moves_left() == to_moves<int>( 10_minutes ) );
                }
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_hacksaw1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_hacksaw1 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );
            REQUIRE( dummy.activity->id() == ACT_HACKSAW );

            WHEN( "furniture has a duration of 5 minutes" ) {
                REQUIRE( furn_t_test_f_hacksaw1->hacksaw->duration() == 5_minutes );
                THEN( "moves_left is equal to 5 minutes" ) {
                    CHECK( dummy.activity->get_moves_left() == to_moves<int>( 5_minutes ) );
                }
            }
        }
    }

    SECTION( "hacksaw turn checks" ) {
        GIVEN( "player is in mid activity" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_hacksaw3 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_hacksaw3 );

            item &hacksaw_elec = dummy.i_add( item::spawn( itype_test_hacksaw_elec,
                                              calendar::start_of_cataclysm, 1 ) );
            dummy.wield( hacksaw_elec );

            REQUIRE( dummy.primary_weapon().typeId() == itype_test_hacksaw_elec );
            REQUIRE( dummy.max_quality( qual_SAW_M ) == 10 );

            setup_activity( hacksaw_elec );
            REQUIRE( dummy.activity->id() == ACT_HACKSAW );
            process_activity( dummy );

            WHEN( "player runs out of charges" ) {
                REQUIRE( dummy.activity->id() == ACT_NULL );

                THEN( "player recharges with fuel" ) {
                    hacksaw_elec.ammo_set( hacksaw_elec.ammo_default(), -1 );

                    AND_THEN( "player can resume the activity" ) {
                        setup_activity( hacksaw_elec );
                        dummy.moves = dummy.get_speed();
                        dummy.activity->do_turn( dummy );
                        CHECK( dummy.activity->id() == ACT_HACKSAW );
                        CHECK( dummy.activity->get_moves_left() < to_moves<int>
                               ( furn_t_test_f_hacksaw3->hacksaw->duration() ) );
                    }
                }
            }
        }
    }

    SECTION( "hacksaw finish checks" ) {
        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_hacksaw1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_hacksaw1 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            REQUIRE( dummy.activity->id() == ACT_HACKSAW );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "terrain gets converted to new terrain type" ) {
                CHECK( mp.ter( tripoint_zero ) == t_dirt );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_hacksaw1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_hacksaw1 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            REQUIRE( dummy.activity->id() == ACT_HACKSAW );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "furniture gets converted to new furniture type" ) {
                CHECK( mp.furn( tripoint_zero ) == f_null );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_hacksaw2 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_hacksaw2 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            REQUIRE( dummy.activity->id() == ACT_HACKSAW );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "furniture gets converted to new furniture type" ) {
                CHECK( mp.furn( tripoint_zero ) == furn_t_test_f_hacksaw1 );
            }
        }


        GIVEN( "a tripoint with a valid furniture with byproducts" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_hacksaw2 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_hacksaw2 );

            item &hacksaw = setup_dummy();
            setup_activity( hacksaw );

            REQUIRE( ter_test_t_hacksaw2->hacksaw->byproducts().size() == 2 );

            REQUIRE( dummy.activity->id() == ACT_HACKSAW );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            const itype_id test_amount( "test_rock" );
            const itype_id test_random( "test_2x4" );

            WHEN( "hacksaw acitivy finishes" ) {
                CHECK( dummy.activity->id() == ACT_NULL );

                THEN( "player receives the items" ) {
                    int count_amount = 0;
                    int count_random = 0;
                    for( const auto &it : get_map().i_at( tripoint_zero ) ) {
                        // can't use switch here
                        const itype_id it_id = it->typeId();
                        if( it_id == test_amount ) {
                            count_amount += it->charges;
                        } else if( it_id == test_random ) {
                            count_random += 1;
                        }
                    }

                    CHECK( count_amount == 3 );
                    CHECK( ( 7 <= count_random && count_random <= 9 ) );
                }
            }
        }
    }
}

TEST_CASE( "oxytorch", "[activity][oxytorch]" )
{
    map &mp = get_map();
    avatar &dummy = get_avatar();

    auto setup_dummy = [&dummy]() -> item & {
        item &torch = dummy.i_add( item::spawn( itype_test_oxytorch ) );
        torch.ammo_set( itype_oxyacetylene, -1 );
        dummy.wield( torch );

        REQUIRE( dummy.primary_weapon().typeId() == itype_test_oxytorch );
        REQUIRE( dummy.max_quality( qual_WELD ) == 10 );

        return torch;
    };

    auto setup_activity = [&dummy]( item & torch ) -> void {
        auto act = std::make_unique<oxytorch_activity_actor>(
            tripoint_zero, safe_reference<item>( torch )
        );
        act->testing = true;
        dummy.assign_activity( std::make_unique<player_activity>( std::move( act ) ) );
    };

    SECTION( "oxytorch start checks" ) {
        GIVEN( "a tripoint with nothing" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, t_null );
            REQUIRE( mp.ter( tripoint_zero ) == t_null );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            THEN( "oxytorch activity can't start" ) {
                CHECK( dummy.activity->id() == ACT_NULL );
            }
        }

        GIVEN( "a tripoint with invalid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, t_dirt );
            REQUIRE( mp.ter( tripoint_zero ) == t_dirt );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            THEN( "oxytorch activity can't start" ) {
                CHECK( dummy.activity->id() == ACT_NULL );
            }
        }

        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_oxytorch1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_oxytorch1 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            THEN( "oxytorch activity can start" ) {
                CHECK( dummy.activity->id() == ACT_OXYTORCH );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_oxytorch1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_oxytorch1 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            THEN( "oxytorch activity can start" ) {
                CHECK( dummy.activity->id() == ACT_OXYTORCH );
            }
        }

        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_oxytorch1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_oxytorch1 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );
            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );

            WHEN( "terrain has a duration of 10 seconds" ) {
                REQUIRE( ter_test_t_oxytorch1->oxytorch->duration() == 10_seconds );
                THEN( "moves_left is equal to 10 seconds" ) {
                    CHECK( dummy.activity->get_moves_left() == to_moves<int>( 10_seconds ) );
                }
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_oxytorch1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_oxytorch1 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );
            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );

            WHEN( "furniture has a duration of 5 seconds" ) {
                REQUIRE( furn_t_test_f_oxytorch1->oxytorch->duration() == 5_seconds );
                THEN( "moves_left is equal to 5 seconds" ) {
                    CHECK( dummy.activity->get_moves_left() == to_moves<int>( 5_seconds ) );
                }
            }
        }
    }

    SECTION( "oxytorch turn checks" ) {
        GIVEN( "player is in mid activity" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_oxytorch3 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_oxytorch3 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );
            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );
            process_activity( dummy );

            WHEN( "player runs out of fuel" ) {
                REQUIRE( dummy.activity->id() == ACT_NULL );

                THEN( "player recharges with fuel" ) {
                    welding_torch.ammo_set( itype_oxyacetylene, -1 );

                    AND_THEN( "player can resume the activity" ) {
                        setup_activity( welding_torch );
                        dummy.moves = dummy.get_speed();
                        dummy.activity->do_turn( dummy );
                        CHECK( dummy.activity->id() == ACT_OXYTORCH );
                        CHECK( dummy.activity->get_moves_left() < to_moves<int>
                               ( furn_t_test_f_oxytorch3->oxytorch->duration() ) );
                    }
                }
            }
        }
    }

    SECTION( "oxytorch finish checks" ) {
        GIVEN( "a tripoint with valid terrain" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_oxytorch1 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_oxytorch1 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "terrain gets converted to new terrain type" ) {
                CHECK( mp.ter( tripoint_zero ) == t_dirt );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_oxytorch1 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_oxytorch1 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "furniture gets converted to new furniture type" ) {
                CHECK( mp.furn( tripoint_zero ) == f_null );
            }
        }

        GIVEN( "a tripoint with valid furniture" ) {
            clear_map();
            clear_avatar();

            mp.furn_set( tripoint_zero, furn_t_test_f_oxytorch2 );
            REQUIRE( mp.furn( tripoint_zero ) == furn_t_test_f_oxytorch2 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            THEN( "furniture gets converted to new furniture type" ) {
                CHECK( mp.furn( tripoint_zero ) == furn_t_test_f_oxytorch1 );
            }
        }


        GIVEN( "a tripoint with a valid furniture with byproducts" ) {
            clear_map();
            clear_avatar();

            mp.ter_set( tripoint_zero, ter_test_t_oxytorch2 );
            REQUIRE( mp.ter( tripoint_zero ) == ter_test_t_oxytorch2 );

            item &welding_torch = setup_dummy();
            setup_activity( welding_torch );

            REQUIRE( ter_test_t_oxytorch2->oxytorch->byproducts().size() == 2 );

            REQUIRE( dummy.activity->id() == ACT_OXYTORCH );
            process_activity( dummy );
            REQUIRE( dummy.activity->id() == ACT_NULL );

            const itype_id test_amount( "test_rock" );
            const itype_id test_random( "test_2x4" );

            WHEN( "oxytorch acitivy finishes" ) {
                CHECK( dummy.activity->id() == ACT_NULL );

                THEN( "player receives the items" ) {
                    int count_amount = 0;
                    int count_random = 0;
                    for( const auto &it : get_map().i_at( tripoint_zero ) ) {
                        // can't use switch here
                        const itype_id it_id = it->typeId();
                        if( it_id == test_amount ) {
                            count_amount += it->charges;
                        } else if( it_id == test_random ) {
                            count_random += 1;
                        }
                    }

                    CHECK( count_amount == 3 );
                    CHECK( ( 7 <= count_random && count_random <= 9 ) );
                }
            }
        }
    }
}
