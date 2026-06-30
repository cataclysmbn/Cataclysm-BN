#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "character.h"
#include "creature.h"
#include "dispersion.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "ranged.h"
#include "state_helpers.h"
#include "type_id.h"
#include "game.h"

#include <vector>

static constexpr tripoint_bub_ms shooter_pos( 60, 60, 0 );

TEST_CASE( "prone_stance_movement_costs", "[stance][prone][movement]" )
{
    clear_all_state();
    avatar &shooter = g->u;
    clear_character( shooter );
    g->place_player( shooter_pos );

    GIVEN( "character standing" ) {
        shooter.set_movement_mode( character_movemode::CMM_WALK );
        REQUIRE( shooter.movement_mode_is( character_movemode::CMM_WALK ) );

        WHEN( "standing -> crouch" ) {
            shooter.set_moves( 200 );
            shooter.set_movement_mode( character_movemode::CMM_CROUCH );
            THEN( "costs 100 moves" ) {
                CHECK( shooter.get_moves() == 100 );
            }
            AND_WHEN( "crouch -> stand" ) {
                shooter.set_moves( 200 );
                shooter.set_movement_mode( character_movemode::CMM_WALK );
                THEN( "costs 100 moves" ) {
                    CHECK( shooter.get_moves() == 100 );
                }
            }
        }

        WHEN( "standing -> prone" ) {
            shooter.set_moves( 200 );
            shooter.set_movement_mode( character_movemode::CMM_PRONE );
            THEN( "costs 150 moves" ) {
                CHECK( shooter.get_moves() == 50 );
            }
            AND_WHEN( "prone -> stand" ) {
                shooter.set_moves( 200 );
                shooter.set_movement_mode( character_movemode::CMM_WALK );
                THEN( "costs 150 moves" ) {
                    CHECK( shooter.get_moves() == 50 );
                }
            }
        }
    }

    GIVEN( "character crouching" ) {
        shooter.set_movement_mode( character_movemode::CMM_CROUCH );
        REQUIRE( shooter.movement_mode_is( character_movemode::CMM_CROUCH ) );

        WHEN( "crouch -> prone" ) {
            shooter.set_moves( 200 );
            shooter.set_movement_mode( character_movemode::CMM_PRONE );
            THEN( "costs 100 moves" ) {
                CHECK( shooter.get_moves() == 100 );
            }
            AND_WHEN( "prone -> crouch" ) {
                shooter.set_moves( 200 );
                shooter.set_movement_mode( character_movemode::CMM_CROUCH );
                THEN( "costs 100 moves" ) {
                    CHECK( shooter.get_moves() == 100 );
                }
            }
        }
    }

    GIVEN( "character prone" ) {
        shooter.set_movement_mode( character_movemode::CMM_PRONE );
        REQUIRE( shooter.movement_mode_is( character_movemode::CMM_PRONE ) );

        WHEN( "toggle_prone_mode returns to walk" ) {
            shooter.set_moves( 200 );
            shooter.toggle_prone_mode();
            THEN( "costs 150 moves to stand" ) {
                CHECK( shooter.get_moves() == 50 );
                CHECK( shooter.movement_mode_is( character_movemode::CMM_WALK ) );
            }
        }
    }

    GIVEN( "character driving a vehicle" ) {
        shooter.set_movement_mode( character_movemode::CMM_WALK );
        shooter.controlling_vehicle = true;

        WHEN( "trying to go prone while driving" ) {
            shooter.set_movement_mode( character_movemode::CMM_PRONE );
            THEN( "cannot go prone while driving" ) {
                CHECK( shooter.movement_mode_is( character_movemode::CMM_WALK ) );
            }
        }
        shooter.controlling_vehicle = false;
    }
}

TEST_CASE( "prone_stance_weapon_dispersion", "[stance][prone][ranged]" )
{
    clear_all_state();
    avatar &shooter = g->u;
    clear_character( shooter );
    g->place_player( shooter_pos );
    build_test_map( ter_id( "t_dirt" ) );

    GIVEN( "a rifle with bipod" ) {
        arm_character( shooter, "ar15", { "bipod" } );

        double walk_dispersion = 0.0;
        double crouch_dispersion = 0.0;
        double prone_dispersion = 0.0;

        WHEN( "standing" ) {
            shooter.set_movement_mode( character_movemode::CMM_WALK );
            dispersion_sources const walk_sources =
                ranged::get_weapon_dispersion( shooter, shooter.primary_weapon() );
            walk_dispersion = walk_sources.max();
        }
        WHEN( "crouching" ) {
            shooter.set_movement_mode( character_movemode::CMM_CROUCH );
            dispersion_sources const crouch_sources =
                ranged::get_weapon_dispersion( shooter, shooter.primary_weapon() );
            crouch_dispersion = crouch_sources.max();
        }
        WHEN( "prone" ) {
            shooter.set_movement_mode( character_movemode::CMM_PRONE );
            dispersion_sources const prone_sources =
                ranged::get_weapon_dispersion( shooter, shooter.primary_weapon() );
            prone_dispersion = prone_sources.max();
        }

        THEN( "prone dispersion is lower than crouch dispersion" ) {
            REQUIRE( walk_dispersion > 0.0 );
            REQUIRE( prone_dispersion < crouch_dispersion );
            REQUIRE( crouch_dispersion < walk_dispersion );
        }
    }
}

TEST_CASE( "prone_stance_MOUNTED_GUN", "[stance][prone][ranged]" )
{
    clear_all_state();
    avatar &shooter = g->u;
    clear_character( shooter );
    g->place_player( shooter_pos );
    build_test_map( ter_id( "t_dirt" ) );

    GIVEN( "a MOUNTED_GUN weapon (M2HB Browning)" ) {
        arm_character( shooter, "m2browning" );

        WHEN( "standing without mountable terrain nearby" ) {
            shooter.set_movement_mode( character_movemode::CMM_WALK );
            THEN( "can_use_heavy_weapon returns false" ) {
                REQUIRE_FALSE( ranged::can_use_heavy_weapon( shooter, get_map(), shooter.bub_pos() ) );
            }
        }

        WHEN( "crouching without mountable terrain nearby" ) {
            shooter.set_movement_mode( character_movemode::CMM_CROUCH );
            THEN( "can_use_heavy_weapon returns false" ) {
                REQUIRE_FALSE( ranged::can_use_heavy_weapon( shooter, get_map(), shooter.bub_pos() ) );
            }
        }

        WHEN( "prone" ) {
            shooter.set_movement_mode( character_movemode::CMM_PRONE );
            THEN( "can_use_heavy_weapon returns true" ) {
                REQUIRE( ranged::can_use_heavy_weapon( shooter, get_map(), shooter.bub_pos() ) );
            }
        }
    }
}

TEST_CASE( "prone_stance_target_size", "[stance][prone][ranged]" )
{
    clear_all_state();
    avatar &shooter = g->u;
    clear_character( shooter );
    g->place_player( shooter_pos );

    GIVEN( "a medium-sized character" ) {
        WHEN( "standing" ) {
            shooter.set_movement_mode( character_movemode::CMM_WALK );
            THEN( "target size is medium (0.5)" ) {
                CHECK( shooter.ranged_target_size() == Approx( 0.5 ).margin( 0.001 ) );
            }
        }
        WHEN( "crouching" ) {
            shooter.set_movement_mode( character_movemode::CMM_CROUCH );
            THEN( "target size is small (0.25)" ) {
                CHECK( shooter.ranged_target_size() == Approx( 0.25 ).margin( 0.001 ) );
            }
        }
        WHEN( "prone" ) {
            shooter.set_movement_mode( character_movemode::CMM_PRONE );
            THEN( "target size is tiny (0.1)" ) {
                CHECK( shooter.ranged_target_size() == Approx( 0.1 ).margin( 0.001 ) );
            }
        }
    }
}
