#include "catch/catch.hpp"

#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "calendar.h"
#include "faction.h"
#include "field.h"
#include "field_type.h"
#include "game.h"
#include "itype.h"
#include "line.h"
#include "map.h"
#include "map_helpers.h"
#include "memory_fast.h"
#include "npc.h"
#include "npc_class.h"
#include "numeric_interval.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player_helpers.h"
#include "point.h"
#include "reality_bubble_helpers.h"
#include "state_helpers.h"
#include "text_snippets.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

class Creature;

static void on_load_test( npc &who, const time_duration &from, const time_duration &to )
{
    calendar::turn = calendar::turn_zero + from;
    who.on_unload();
    calendar::turn = calendar::turn_zero + to;
    who.on_load();
}

static void test_needs( const npc &who, const numeric_interval<int> &kcal_lost,
                        const numeric_interval<int> &thirst,
                        const numeric_interval<int> &fatigue )
{
    int kcal_below_max = who.max_stored_kcal() - who.get_stored_kcal();
    CHECK( kcal_below_max <= kcal_lost.max );
    CHECK( kcal_below_max >= kcal_lost.min );
    CHECK( who.get_thirst() <= thirst.max );
    CHECK( who.get_thirst() >= thirst.min );
    CHECK( who.get_fatigue() <= fatigue.max );
    CHECK( who.get_fatigue() >= fatigue.min );
}

static void create_model( npc &model_npc )
{
    model_npc.randomize( NC_NONE );
    for( const trait_id &tr : model_npc.get_mutations() ) {
        model_npc.unset_mutation( tr );
    }
    model_npc.set_stored_kcal( model_npc.max_stored_kcal() );
    model_npc.set_thirst( 0 );
    model_npc.set_fatigue( 0 );
    model_npc.remove_effect( efftype_id( "sleep" ) );
    // An ugly hack to prevent NPC falling asleep during testing due to massive fatigue
    model_npc.set_mutation( trait_id( "WEB_WEAVER" ) );

}

static std::string get_list_of_npcs( const std::string &title )
{

    std::ostringstream npc_list;
    npc_list << title << ":\n";
    for( const npc &n : g->all_npcs() ) {
        npc_list << "  " << &n << ": " << n.name << '\n';
    }
    return npc_list.str();
}

TEST_CASE( "on_load-sane-values", "[.]" )
{
    clear_all_state();
    SECTION( "Awake for 10 minutes, gaining hunger/thirst/fatigue" ) {
        npc test_npc;
        create_model( test_npc );
        const int five_min_ticks = 2;
        on_load_test( test_npc, 0_turns, 5_minutes * five_min_ticks );
        const int margin = 2;

        const numeric_interval<int> hunger( islot_comestible::kcal_per_nutr * five_min_ticks / 4, margin,
                                            margin );
        const numeric_interval<int> thirst( five_min_ticks / 4, margin, margin );
        const numeric_interval<int> fatigue( five_min_ticks, margin, margin );

        test_needs( test_npc, hunger, thirst, fatigue );
    }

    SECTION( "Awake for 2 days, gaining hunger/thirst/fatigue" ) {
        npc test_npc;
        create_model( test_npc );
        const auto five_min_ticks = 2_days / 5_minutes;
        on_load_test( test_npc, 0_turns, 5_minutes * five_min_ticks );

        const int margin = 20;
        const numeric_interval<int> hunger( islot_comestible::kcal_per_nutr * five_min_ticks / 4, margin,
                                            margin );
        const numeric_interval<int> thirst( five_min_ticks / 4, margin, margin );
        const numeric_interval<int> fatigue( five_min_ticks, margin, margin );

        test_needs( test_npc, hunger, thirst, fatigue );
    }

    SECTION( "Sleeping for 6 hours, gaining hunger/thirst (not testing fatigue due to lack of effects processing)" ) {
        npc test_npc;
        create_model( test_npc );
        test_npc.add_effect( efftype_id( "sleep" ), 6_hours );
        test_npc.set_fatigue( 1000 );
        const auto five_min_ticks = 6_hours / 5_minutes;
        /*
        // Fatigue regeneration starts at 1 per 5min, but linearly increases to 2 per 5min at 2 hours or more
        const int expected_fatigue_change =
            ((1.0f + 2.0f) / 2.0f * 2_hours / 5_minutes ) +
            (2.0f * (6_hours - 2_hours) / 5_minutes);
        */
        on_load_test( test_npc, 0_turns, 5_minutes * five_min_ticks );

        const int margin = 10;
        const numeric_interval<int> hunger( islot_comestible::kcal_per_nutr * five_min_ticks / 8, margin,
                                            margin );
        const numeric_interval<int> thirst( five_min_ticks / 8, margin, margin );
        const numeric_interval<int> fatigue( test_npc.get_fatigue(), 0, 0 );

        test_needs( test_npc, hunger, thirst, fatigue );
    }
}

TEST_CASE( "on_load-similar-to-per-turn", "[.]" )
{
    clear_all_state();
    SECTION( "Awake for 10 minutes, gaining hunger/thirst/fatigue" ) {
        npc on_load_npc;
        create_model( on_load_npc );
        npc iterated_npc;
        create_model( iterated_npc );
        const int five_min_ticks = 2;
        on_load_test( on_load_npc, 0_turns, 5_minutes * five_min_ticks );
        for( time_duration turn = 0_turns; turn < 5_minutes * five_min_ticks; turn += 1_turns ) {
            iterated_npc.update_body( 1_turns );
        }

        const int margin = 2;
        const numeric_interval<int> hunger( iterated_npc.max_stored_kcal() -
                                            iterated_npc.get_stored_kcal(), margin, margin );
        const numeric_interval<int> thirst( iterated_npc.get_thirst(), margin, margin );
        const numeric_interval<int> fatigue( iterated_npc.get_fatigue(), margin, margin );

        test_needs( on_load_npc, hunger, thirst, fatigue );
    }

    SECTION( "Awake for 6 hours, gaining hunger/thirst/fatigue" ) {
        npc on_load_npc;
        create_model( on_load_npc );
        npc iterated_npc;
        create_model( on_load_npc );
        const auto five_min_ticks = 6_hours / 5_minutes;
        on_load_test( on_load_npc, 0_turns, 5_minutes * five_min_ticks );
        for( time_duration turn = 0_turns; turn < 5_minutes * five_min_ticks; turn += 1_turns ) {
            iterated_npc.update_body( 1_turns );
        }

        const int margin = 10;
        const numeric_interval<int> hunger( iterated_npc.max_stored_kcal() -
                                            iterated_npc.get_stored_kcal(), margin, margin );
        const numeric_interval<int> thirst( iterated_npc.get_thirst(), margin, margin );
        const numeric_interval<int> fatigue( iterated_npc.get_fatigue(), margin, margin );

        test_needs( on_load_npc, hunger, thirst, fatigue );
    }
}

TEST_CASE( "npc_reality_bubble_resize_preserves_zlevel", "[.]" )
{
    CHECK( reality_bubble::local_square_from_global( tripoint( 60, 61, 1 ),
            tripoint( 12, 12, 1 ) ) == tripoint( 48, 49, 1 ) );
    CHECK( reality_bubble::local_square_from_global( tripoint( 60, 61, -1 ),
            tripoint( 12, 12, -1 ) ) == tripoint( 48, 49, -1 ) );
}

TEST_CASE( "snippet-tag-test" )
{
    clear_all_state();
    // Actually used tags
    static const std::set<std::string> npc_talk_tags = {
        {
            "<name_b>", "<thirsty>", "<swear!>",
            "<sad>", "<greet>", "<no>",
            "<im_leaving_you>", "<ill_kill_you>", "<ill_die>",
            "<wait>", "<no_faction>", "<name_g>",
            "<keep_up>", "<yawn>", "<very>",
            "<okay>", "<really>",
            "<let_me_pass>", "<done_mugging>", "<happy>",
            "<drop_it>", "<swear>", "<lets_talk>",
            "<hands_up>", "<move>", "<hungry>",
            "<fuck_you>",
        }
    };

    for( const auto &tag : npc_talk_tags ) {
        for( int i = 0; i < 100; i++ ) {
            CHECK( SNIPPET.random_from_category( tag ).has_value() );
        }
    }

    // Special tags, those should have no replacements
    static const std::set<std::string> special_tags = {
        {
            "<yrwp>", "<mywp>", "<ammo>"
        }
    };

    for( const std::string &tag : special_tags ) {
        for( int i = 0; i < 100; i++ ) {
            CHECK( !SNIPPET.random_from_category( tag ).has_value() );
        }
    }
}

/* Test setup. Player should always be at top-left.
 *
 * U is the player, V is vehicle, # is wall, R is rubble & acid with NPC on it,
 * A is acid with NPC on it, W/M is vehicle & acid with (follower/non-follower) NPC on it,
 * B/C is acid with (follower/non-follower) NPC on it.
 */
constexpr int height = 5, width = 17;
constexpr char setup[height][width + 1] = {
    "U ###############",
    "V #R#AAA#W# # #C#",
    "  #A#A#A# #M#B# #",
    "  ###AAA#########",
    "    #####        ",
};

static void check_npc_movement( const tripoint &origin )
{
    const efftype_id effect_bouldering( "bouldering" );

    INFO( "Should not crash from infinite recursion" );
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            switch( setup[y][x] ) {
                case 'A':
                case 'R':
                case 'W':
                case 'M':
                case 'B':
                case 'C':
                    tripoint p = origin + point( x, y );
                    npc *guy = g->critter_at<npc>( p );
                    REQUIRE( guy != nullptr );
                    guy->move();
                    break;
            }
        }
    }

    INFO( "NPC on acid should not acquire unstable footing status" );
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            if( setup[y][x] == 'A' ) {
                tripoint p = origin + point( x, y );
                npc *guy = g->critter_at<npc>( p );
                REQUIRE( guy != nullptr );
                CHECK( !guy->has_effect( effect_bouldering ) );
            }
        }
    }

    INFO( "NPC on rubbles should not lose unstable footing status" );
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            if( setup[y][x] == 'R' ) {
                tripoint p = origin + point( x, y );
                npc *guy = g->critter_at<npc>( p );
                REQUIRE( guy != nullptr );
                CHECK( guy->has_effect( effect_bouldering ) );
            }
        }
    }

    INFO( "NPC in vehicle should not escape from dangerous terrain" );
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            switch( setup[y][x] ) {
                case 'W':
                case 'M':
                    CAPTURE( setup[y][x] );
                    tripoint p = origin + point( x, y );
                    npc *guy = g->critter_at<npc>( p );
                    CHECK( guy != nullptr );
                    break;
            }
        }
    }

    INFO( "NPC not in vehicle should escape from dangerous terrain" );
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            switch( setup[y][x] ) {
                case 'B':
                case 'C':
                    tripoint p = origin + point( x, y );
                    npc *guy = g->critter_at<npc>( p );
                    CHECK( guy == nullptr );
                    break;
            }
        }
    }
}

TEST_CASE( "npc-movement" )
{
    clear_all_state();
    const ter_id t_reinforced_glass( "t_reinforced_glass" );
    const ter_id t_floor( "t_floor" );
    const furn_id f_rubble( "f_rubble" );
    const furn_id f_null( "f_null" );
    const vpart_id vpart_frame_vertical( "frame_vertical" );
    const vpart_id vpart_seat( "seat" );

    g->place_player( tripoint( 60, 60, 0 ) );

    Character &player_character = get_player_character();
    map &here = get_map();
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            const char type = setup[y][x];
            const tripoint p = player_character.pos() + point( x, y );
            // create walls
            if( type == '#' ) {
                here.ter_set( p, t_reinforced_glass );
            } else {
                here.ter_set( p, t_floor );
            }
            // spawn acid
            // a copy is needed because we will remove elements from it
            const field fs = here.field_at( p );
            for( const auto &f : fs ) {
                here.remove_field( p, f.first );
            }
            if( type == 'A' || type == 'R' || type == 'W' || type == 'M'
                || type == 'B' || type == 'C' ) {

                here.add_field( p, fd_acid, 3 );
            }
            // spawn rubbles
            if( type == 'R' ) {
                here.furn_set( p, f_rubble );
            } else {
                here.furn_set( p, f_null );
            }
            // create vehicles
            if( type == 'V' || type == 'W' || type == 'M' ) {
                vehicle *veh = here.add_vehicle( vproto_id( "none" ), p, 270_degrees, 0, 0 );
                REQUIRE( veh != nullptr );
                veh->install_part( point_zero, vpart_frame_vertical );
                veh->install_part( point_zero, vpart_seat );
                here.add_vehicle_to_cache( veh );
            }
            // spawn npcs
            if( type == 'A' || type == 'R' || type == 'W' || type == 'M'
                || type == 'B' || type == 'C' ) {

                shared_ptr_fast<npc> guy = make_shared_fast<npc>();
                do {
                    guy->randomize();
                    // Repeat until we get an NPC vulnerable to acid
                } while( guy->is_immune_field( fd_acid ) );
                guy->spawn_at_precise( {g->get_levx(), g->get_levy()}, p );
                // Set the shopkeep mission; this means that
                // the NPC deems themselves to be guarding and stops them
                // wandering off in search of distant ammo caches, etc.
                guy->mission = NPC_MISSION_SHOPKEEP;
                // This prevents npcs occasionally teleporting away
                guy->assign_activity( activity_id( "ACT_MEDITATE" ) );
                //Sometimes they spawn with sledge hammers and bash down the walls
                guy->remove_primary_weapon();
                ACTIVE_OVERMAP_BUFFER.insert_npc( guy );
                g->load_npcs();
                guy->set_attitude( ( type == 'M' || type == 'C' ) ? NPCATT_NULL : NPCATT_FOLLOW );
            }
        }
    }

    // check preconditions
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            const char type = setup[y][x];
            const tripoint p = player_character.pos() + point( x, y );
            if( type == '#' ) {
                REQUIRE( !here.passable( p ) );
            } else {
                REQUIRE( here.passable( p ) );
            }
            if( type == 'R' ) {
                REQUIRE( here.has_flag( "UNSTABLE", p ) );
            } else {
                REQUIRE( !here.has_flag( "UNSTABLE", p ) );
            }
            if( type == 'V' || type == 'W' || type == 'M' ) {
                REQUIRE( here.veh_at( p ).part_with_feature( VPFLAG_BOARDABLE, true ).has_value() );
            } else {
                REQUIRE( !here.veh_at( p ).part_with_feature( VPFLAG_BOARDABLE, true ).has_value() );
            }
            npc *guy = g->critter_at<npc>( p );
            if( type == 'A' || type == 'R' || type == 'W' || type == 'M'
                || type == 'B' || type == 'C' ) {

                REQUIRE( guy != nullptr );
                REQUIRE( guy->is_dangerous_fields( here.field_at( p ) ) );
            } else {
                REQUIRE( guy == nullptr );
            }
        }
    }

    SECTION( "NPCs escape dangerous terrain by pushing other NPCs" ) {
        check_npc_movement( player_character.pos() );
    }

    SECTION( "Player in vehicle & NPCs escaping dangerous terrain" ) {
        const tripoint origin = player_character.pos();

        for( int y = 0; y < height; ++y ) {
            for( int x = 0; x < width; ++x ) {
                if( setup[y][x] == 'V' ) {
                    g->place_player( player_character.pos() + point( x, y ) );
                    break;
                }
            }
        }

        check_npc_movement( origin );
    }
}

TEST_CASE( "npc_can_target_player" )
{
    clear_all_state();
    // Set to daytime for visibiliity
    calendar::turn = calendar::turn_zero + 12_hours;

    g->faction_manager_ptr->create_if_needed();

    g->place_player( tripoint_zero );

    clear_npcs();
    clear_creatures();

    Character &player_character = get_player_character();
    npc &hostile = spawn_npc( player_character.pos().xy() + point_south, "thug" );
    REQUIRE( rl_dist( player_character.pos(), hostile.pos() ) <= 1 );
    hostile.set_attitude( NPCATT_KILL );
    hostile.name = "Enemy NPC";

    INFO( get_list_of_npcs( "NPCs after spawning one" ) );

    hostile.regen_ai_cache();
    REQUIRE( hostile.current_target() != nullptr );
    CHECK( hostile.current_target() == static_cast<Creature *>( &player_character ) );
}

TEST_CASE( "npc_move_through_vehicle_holes" )
{
    clear_all_state();
    g->place_player( tripoint( 65, 55, 0 ) );
    tripoint origin( 60, 60, 0 );

    get_map().add_vehicle( vproto_id( "apc" ), origin, -45_degrees, 0, 0 );
    get_map().build_map_cache( 0 );

    tripoint mon_origin = origin + tripoint( -2, 1, 0 );

    shared_ptr_fast<npc> guy = make_shared_fast<npc>();
    guy->randomize();
    guy->spawn_at_precise( {g->get_levx(), g->get_levy()}, mon_origin );

    ACTIVE_OVERMAP_BUFFER.insert_npc( guy );
    g->load_npcs();

    guy->move_to( mon_origin + tripoint_north_west, true, nullptr );

    const npc *m = g->critter_at<npc>( mon_origin );
    CHECK( m != nullptr );

    const npc *m2 = g->critter_at<npc>( mon_origin + tripoint_north_west );
    CHECK( m2 == nullptr );

}

TEST_CASE( "random npc spawn chance" )
{
    CHECK( npc_overmap::spawn_chance_in_hour( 0, 1.0 ) == Approx( 1.0 / 24.0 ) );
    CHECK( npc_overmap::spawn_chance_in_hour( 0, 100.0 ) == 1.0 );

    static constexpr int days_in_year = 14 * 4;
    CHECK( npc_overmap::spawn_chance_in_hour( days_in_year, 1.0 ) == Approx( 1.0 / 24.0 ) );

    CHECK( npc_overmap::spawn_chance_in_hour( 2 * days_in_year, 1.0 ) == Approx( 0.5 / 24.0 ) );
    CHECK( npc_overmap::spawn_chance_in_hour( 4 * days_in_year, 1.0 ) == Approx( 0.25 / 24.0 ) );
    CHECK( npc_overmap::spawn_chance_in_hour( 8 * days_in_year, 1.0 ) == Approx( 0.125 / 24.0 ) );
}
