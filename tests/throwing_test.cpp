#include "catch/catch.hpp"

#include <algorithm>
#include <list>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "avatar.h"
#include "ballistics.h"
#include "calendar.h"
#include "damage.h"
#include "dispersion.h"
#include "game.h"
#include "game_constants.h"
#include "inventory.h"
#include "item.h"
#include "line.h"
#include "map_helpers.h"
#include "material.h"
#include "monster.h"
#include "npc.h"
#include "player.h"
#include "player_helpers.h"
#include "point.h"
#include "projectile.h"
#include "ranged.h"
#include "state_helpers.h"
#include "test_statistics.h"
#include "type_id.h"

TEST_CASE( "throwing distance test", "[throwing], [balance]" )
{
    clear_all_state();
    const standard_npc thrower( "Thrower", tripoint( 60, 60, 0 ), {}, 4, 10, 10, 10, 10 );
    item &grenade = *item::spawn_temporary( "grenade" );
    CHECK( thrower.throw_range( grenade ) >= 30 );
    CHECK( thrower.throw_range( grenade ) <= 35 );
}

struct throw_test_data {
    statistics<bool> hits;
    statistics<double> dmg;

    throw_test_data() : dmg( Z95 ) {}
};

struct throw_test_pstats {
    int skill_lvl;
    int str;
    int dex;
    int per;
};

static std::ostream &operator<<( std::ostream &stream, const throw_test_pstats &pstats )
{
    return( stream << "STR: " << pstats.str << " DEX: " << pstats.dex <<
            " PER: " << pstats.per << " SKL: " << pstats.skill_lvl );
}

static const skill_id skill_throw = skill_id( "throw" );

static void reset_player( player &p, const throw_test_pstats &pstats, const tripoint &pos )
{
    clear_character( p );
    CHECK( !p.in_vehicle );
    p.setpos( pos );
    p.str_max = pstats.str;
    p.dex_max = pstats.dex;
    p.per_max = pstats.per;
    p.set_str_bonus( 0 );
    p.set_per_bonus( 0 );
    p.set_dex_bonus( 0 );
    p.set_skill_level( skill_throw, pstats.skill_lvl );
}

// If tests are routinely failing you should:
//  1. Make sure some change hasn't caused some regression
//  2. Make sure test is accurate by testing with a large minimum iterations (min > 5000)
//  3. Increase bounds on thresholds
//  4. Increase max iterations which will make the CI smaller and more likely to
//     fit inside the threshold but also increase the average test length
// In that order.
constexpr int min_throw_test_iterations = 100;
constexpr int max_throw_test_iterations = 10000;

// tighter thresholds here will increase accuracy but also increase average test
// time since more samples are required to get a more accurate test
static void test_throwing_player_versus(
    player &p, const std::string &mon_id, const std::string &throw_id,
    const int range, const throw_test_pstats &pstats,
    const epsilon_threshold &hit_thresh, const epsilon_threshold &dmg_thresh )
{
    const tripoint monster_start = { 30 + range, 30, 0 };
    const tripoint player_start = { 30, 30, 0 };
    bool hit_thresh_met = false;
    bool dmg_thresh_met = false;
    throw_test_data data;


    do {
        reset_player( p, pstats, player_start );
        p.set_moves( 1000 );
        p.set_stamina( p.get_stamina_max() );
        detached_ptr<item> det = item::spawn( throw_id );
        item &it = *det;
        monster &mon = spawn_test_monster( mon_id, monster_start );
        mon.set_moves( 0 );

        double actual_hit_chance =
            ranged::hit_chance(
                dispersion_sources( ranged::throwing_dispersion( p, it, &mon, false ) ),
                range,
                mon.ranged_target_size() );
        if( std::fabs( actual_hit_chance - hit_thresh.midpoint ) > hit_thresh.epsilon / 4.0 ) {
            CAPTURE( hit_thresh.midpoint );
            CAPTURE( hit_thresh.epsilon / 4.0 );
            CAPTURE( actual_hit_chance );
            CAPTURE( range );
            CAPTURE( mon.ranged_target_size() );
            FAIL_CHECK( "Expected and calculated midpoints must be within epsilon/4 or the test is too fragile" );
            return;
        }

        dealt_projectile_attack atk = ranged::throw_item( p, mon.pos(), std::move( det ), std::nullopt );
        data.hits.add( atk.hit_critter != nullptr );
        data.dmg.add( atk.dealt_dam.total_damage() );

        if( data.hits.n() >= min_throw_test_iterations ) {
            // ideally we should actually still checking the threshold after we
            // meet it but we're busy people and don't have time for that
            if( !hit_thresh_met ) {
                hit_thresh_met = data.hits.test_threshold( hit_thresh );
            }
            // don't do an else here because it's possible we just made
            // hit_thresh_met true
            if( hit_thresh_met ) {
                // commenting this out is a super easy way to force all the
                // test to fail if you want to reset the baseline after
                // making balance changes or if many of the tests are failing
                dmg_thresh_met = data.dmg.test_threshold( dmg_thresh );
            }
        }
        g->remove_zombie( mon );
        // only need to check dmg_thresh_met because it can only be true if
        // hit_thresh_met first
    } while( !dmg_thresh_met && data.hits.n() < max_throw_test_iterations );

    INFO( "Monster: '" << mon_id << "' Item: '" << throw_id );
    INFO( "Range: " << range << " Pstats: " << pstats );
    INFO( "Total throws: " << data.hits.n() );
    INFO( "Ratio: " << data.hits.avg() * 100 << "%" );
    INFO( "Hit Lower: " << data.hits.lower() * 100 << "% Hit Upper: " << data.hits.upper() * 100 <<
          "%" );
    INFO( "Hit Thresh: " << ( hit_thresh.midpoint - hit_thresh.epsilon ) * 100 << "% - " <<
          ( hit_thresh.midpoint + hit_thresh.epsilon ) * 100 << "%" );
    INFO( "Adj Wald error: " << data.hits.margin_of_error() );
    INFO( "Avg total damage: " << data.dmg.avg() );
    INFO( "Dmg Lower: " << data.dmg.lower() << " Dmg Upper: " << data.dmg.upper() );
    INFO( "Dmg Thresh: " << dmg_thresh.midpoint - dmg_thresh.epsilon << " - " <<
          dmg_thresh.midpoint + dmg_thresh.epsilon );
    INFO( "Margin of error: " << data.hits.margin_of_error() );
    CHECK( dmg_thresh_met );
}

constexpr throw_test_pstats lo_skill_base_stats = { 0, 8, 8, 8 };
constexpr throw_test_pstats mid_skill_base_stats = { MAX_SKILL / 2, 8, 8, 8 };
constexpr throw_test_pstats hi_skill_base_stats = { MAX_SKILL, 8, 8, 8 };
constexpr throw_test_pstats hi_skill_athlete_stats = { MAX_SKILL, 12, 12, 12 };

TEST_CASE( "basic_throwing_sanity_tests", "[throwing],[balance]" )
{
    clear_all_state();
    player &p = g->u;

    SECTION( "test_player_vs_zombie_rock_basestats" ) {
        test_throwing_player_versus( p, "mon_zombie", "rock", 1, lo_skill_base_stats, { 0.99, 0.10 }, { 10, 3 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 5, lo_skill_base_stats, { 0.77, 0.10 }, { 5.5, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 10, lo_skill_base_stats, { 0.27, 0.10 }, { 2, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 15, lo_skill_base_stats, { 0.13, 0.10 }, { 1, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 20, lo_skill_base_stats, { 0.095, 0.10 }, { 0.5, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 25, lo_skill_base_stats, { 0.08, 0.10 }, { 0.5, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 30, lo_skill_base_stats, { 0.06, 0.10 }, { 0.5, 2 } );
    }

    SECTION( "test_player_vs_zombie_javelin_iron_basestats" ) {
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 1, lo_skill_base_stats, { 1.00, 0.10 }, { 33, 5 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 5, lo_skill_base_stats, { 0.64, 0.10 }, { 13, 3 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 10, lo_skill_base_stats, { 0.20, 0.10 }, { 4, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 15, lo_skill_base_stats, { 0.11, 0.10 }, { 1.29, 3 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 20, lo_skill_base_stats, { 0.08, 0.10 }, { 1.66, 2 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 25, lo_skill_base_stats, { 0.06, 0.10 }, { 1.0, 2 } );
    }
    // Changes to aimed crit damage and hit location makes high skill throwing perform more consistantly.
    // Tests fail when not adjusted to account for this increase in performance.
    SECTION( "test_player_vs_zombie_rock_athlete" ) {
        test_throwing_player_versus( p, "mon_zombie", "rock", 1, hi_skill_athlete_stats, { 1.00, 0.10 }, { 16.5, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 5, hi_skill_athlete_stats, { 1.00, 0.10 }, { 16.5, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 10, hi_skill_athlete_stats, { 1.00, 0.10 }, { 16.27, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 15, hi_skill_athlete_stats, { 0.97, 0.10 }, { 12.83, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 20, hi_skill_athlete_stats, { 0.77, 0.10 }, { 9.10, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 25, hi_skill_athlete_stats, { 0.58, 0.10 }, { 6.54, 6 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 30, hi_skill_athlete_stats, { 0.43, 0.10 }, { 4.90, 4 } );
    }

    SECTION( "test_player_vs_zombie_javelin_iron_athlete" ) {
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 1, hi_skill_athlete_stats, { 1.00, 0.10 }, { 59.00, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 5, hi_skill_athlete_stats, { 1.00, 0.10 }, {  50.55, 8 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 10, hi_skill_athlete_stats, { 1.00, 0.10 }, { 40.00, 10 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 15, hi_skill_athlete_stats, { 0.97, 0.10 }, { 38.00, 10 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 20, hi_skill_athlete_stats, { 0.77, 0.10 }, { 30.00, 10 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 25, hi_skill_athlete_stats, { 0.58, 0.10 }, { 25.00, 10 } );
        test_throwing_player_versus( p, "mon_zombie", "javelin_iron", 30, hi_skill_athlete_stats, { 0.43, 0.10 }, { 15.31, 10 } );
    }
}

TEST_CASE( "throwing_skill_impact_test", "[throwing],[balance]" )
{
    clear_all_state();
    player &p = g->u;
    // we already cover low stats in the sanity tests and we only cover a few
    // ranges here because what we're really trying to capture is the effect
    // the throwing skill has while the sanity tests are more explicit.
    SECTION( "mid_skill_basestats_rock" ) {
        test_throwing_player_versus( p, "mon_zombie", "rock", 5, mid_skill_base_stats, { 1.00, 0.10 }, { 12, 6 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 10, mid_skill_base_stats, { 0.92, 0.10 }, { 11.6, 4 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 15, mid_skill_base_stats, { 0.62, 0.10 }, { 7, 2 } );
    }

    SECTION( "hi_skill_basestats_rock" ) {
        test_throwing_player_versus( p, "mon_zombie", "rock", 5, hi_skill_base_stats, { 1.00, 0.10 }, { 18, 5 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 10, hi_skill_base_stats, { 1.00, 0.10 }, { 16, 5 } );
        test_throwing_player_versus( p, "mon_zombie", "rock", 15, hi_skill_base_stats, { 0.97, 0.10 }, { 15, 4 } );
    }
}

TEST_CASE( "time_to_throw_independent_of_number_of_projectiles", "[throwing],[balance]" )
{
    clear_all_state();
    player &p = g->u;

    detached_ptr<item> det = item::spawn( "throwing_stick", calendar::turn, 10 );
    item &thrown = *det;
    REQUIRE( thrown.charges > 1 );
    REQUIRE( thrown.count_by_charges() );
    p.wield( std::move( det ) );
    int initial_moves = -1;
    while( thrown.charges > 0 ) {
        const int cost = ranged::throw_cost( p, thrown );
        if( initial_moves < 0 ) {
            initial_moves = cost;
        } else {
            CHECK( initial_moves == cost );
        }
        thrown.charges--;
    }
}
