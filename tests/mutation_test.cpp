#include "catch/catch.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "mutation.h"
#include "npc.h"
#include "options.h"
#include "options_helpers.h"
#include "player.h"
#include "string_id.h"
#include "type_id.h"

static const efftype_id effect_accumulated_mutagen( "accumulated_mutagen" );
static const auto trait_marloss = trait_id( "MARLOSS" );

std::string get_mutations_as_string( const player &p );

// Note: If a category has two mutually-exclusive mutations (like pretty/ugly for Lupine), the
// one they ultimately end up with depends on the order they were loaded from JSON
static void give_all_mutations( player &p, const mutation_category_trait &category,
                                const bool include_postthresh )
{
    const std::vector<trait_id> category_mutations = mutations_category[category.id];

    // Add the threshold mutations first
    if( include_postthresh && !category.threshold_muts.empty() ) {
        for( unsigned int i = 1; i < category.threshold_muts.size();
             i++ ) { // starts at 1 because 0 is NULL for aligning tier to index
            p.set_mutation( category.threshold_muts[i] );
        }
    }

    for( auto &m : category_mutations ) {
        const auto &mdata = m.obj();
        if( include_postthresh || ( !mdata.threshold && mdata.threshold_tier == 0 ) ) {
            int mutation_attempts = 10;
            while( mutation_attempts > 0 && p.mutation_ok( m, false, false ) ) {
                INFO( "Current mutations: " << get_mutations_as_string( p ) );
                INFO( "Mutating towards " << m.c_str() );
                if( !p.mutate_towards( m ) ) {
                    --mutation_attempts;
                }
                CHECK( mutation_attempts > 0 );
            }
        }
    }
}

static int get_total_category_strength( const player &p )
{
    int total = 0;
    for( auto &i : p.mutation_category_level ) {
        total += i.second;
    }

    return total;
}

// Returns the list of mutations a player has as a string, for debugging
std::string get_mutations_as_string( const player &p )
{
    std::ostringstream s;
    for( auto &m : p.get_mutations() ) {
        s << static_cast<std::string>( m ) << " ";
    }
    return s.str();
}

TEST_CASE( "Having all mutations give correct highest category", "[mutations]" )
{
    for( auto &cat : mutation_category_trait::get_all() ) {
        const auto &cur_cat = cat.second;
        const auto &cat_id = cur_cat.id;
        if( cat_id == mutation_category_id( "ANY" ) || cat_id == mutation_category_id( "MYCUS" ) ) {
            continue;
        }

        GIVEN( "The player has all pre-threshold mutations for " + cat_id.str() ) {
            npc dummy;
            give_all_mutations( dummy, cur_cat, false );

            THEN( cat_id.str() + " is the strongest category" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( dummy.get_highest_category() == cat_id );
            }
        }

        GIVEN( "The player has all mutations for " + cat_id.str() ) {
            npc dummy;
            give_all_mutations( dummy, cur_cat, true );

            THEN( cat_id.str() + " is the strongest category" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( dummy.get_highest_category() == cat_id );
            }
        }
    }
}

TEST_CASE( "Having all pre-threshold mutations gives a sensible threshold breach chance",
           "[mutations]" )
{
    const float BREACH_CHANCE_MIN = 0.15f;
    const float BREACH_CHANCE_MAX = 0.5f;

    for( auto &cat : mutation_category_trait::get_all() ) {
        const auto &cur_cat = cat.second;
        const auto &cat_id = cur_cat.id;
        if( cat_id == mutation_category_id( "ANY" ) || cat_id == mutation_category_id( "MYCUS" ) ) {
            continue;
        }

        GIVEN( "The player has all pre-threshold mutations for " + cat_id.str() ) {
            npc dummy;
            give_all_mutations( dummy, cur_cat, false );

            const int category_strength = dummy.mutation_category_level[cat_id];
            const int total_strength = get_total_category_strength( dummy );
            float breach_chance = category_strength / static_cast<float>( total_strength );

            THEN( "Threshold breach chance is at least 0.15" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( breach_chance >= BREACH_CHANCE_MIN );
            }
            THEN( "Threshold breach chance is at most 0.4" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( breach_chance <= BREACH_CHANCE_MAX );
            }
        }
    }
}

static float sum_without_category( const std::map<trait_id, float> &chances,
                                   const mutation_category_id &cat )
{
    float sum = 0.0f;
    for( const auto &c : chances ) {
        const auto &mut_categories = c.first->category;
        if( std::find( mut_categories.begin(), mut_categories.end(), cat ) == mut_categories.end() ) {
            sum += c.second;
        }
    }

    return sum;
}

TEST_CASE( "Gaining a mutation in category makes mutations from other categories less likely",
           "[mutations]" )
{
    for( auto &cat : mutation_category_trait::get_all() ) {
        const auto &cur_cat = cat.second;
        const auto &cat_id = cur_cat.id;
        if( cat_id == mutation_category_id( "ANY" ) || cat_id == mutation_category_id( "MYCUS" ) ) {
            continue;
        }

        npc zero_mut_dummy;
        std::map<trait_id, float> chances_pre = zero_mut_dummy.mutation_chances();
        float sum_pre = sum_without_category( chances_pre, cat_id );
        for( const mutation_branch &mut : mutation_branch::get_all() ) {
            if( zero_mut_dummy.mutation_ok( mut.id, false, false ) ) {
                continue;
            }
            GIVEN( "The player gains a mutation " + mut.name() ) {
                npc dummy;
                dummy.mutate_towards( mut.id );
                THEN( "Sum of chances for mutations not of this category is lower than before" ) {
                    std::map<trait_id, float> chances_post = dummy.mutation_chances();
                    float sum_post = sum_without_category( chances_post, cat_id );
                    CHECK( sum_post < sum_pre );
                }
            }
        }
    }
}

TEST_CASE( "Mutating with full mutagen accumulation results in multiple mutations",
           "[mutations][.]" )
{
    REQUIRE( get_option<bool>( "BALANCED_MUTATIONS" ) );
    GIVEN( "Player with maximum intensity accumulated mutagen" ) {
        npc dummy;
        dummy.add_effect( effect_accumulated_mutagen, 30_days, bodypart_str_id::NULL_ID() );
        AND_GIVEN( "The player mutates" ) {
            dummy.mutate();
            THEN( "The player has >3 mutations" ) {
                CHECK( dummy.get_mutations().size() > 3 );
            }
        }
    }
}

TEST_CASE( "Mutating marloss does not crash on missing category data", "[mutations]" )
{
    npc dummy;

    CHECK( dummy.mutate_towards( trait_marloss ) );
    CHECK( dummy.has_trait( trait_marloss ) );
}

// Returns the sum of mutation point costs for all mutations a character currently has.
static auto mutation_cost_sum( const Character &c ) -> int
{
    return std::ranges::fold_left(
    c.get_mutations() | std::views::transform( []( const trait_id & m ) {
        return m->cost;
    } ), 0, std::plus<int> {} );
}

// Marked [.] so it only runs when explicitly requested — statistical and slow.
TEST_CASE( "mutate_category direction system trends selection toward target", "[mutations][.]" )
{
    REQUIRE( get_option<bool>( "BALANCED_MUTATIONS" ) );

    static const mutation_category_id cat_alpha( "ALPHA" );
    static const trait_id trait_ROT3( "ROT3" );
    constexpr int trials = 10000;

    SECTION( "High score target yields mostly positive mutations" ) {
        override_option target_opt( "MUTATION_SCORE_TARGET", "20" );

        auto positive = 0;
        auto negative = 0;

        for( auto i = 0; i < trials; ++i ) {
            npc dummy;
            const auto before = mutation_cost_sum( dummy );
            dummy.mutate_category( cat_alpha );
            const auto delta = mutation_cost_sum( dummy ) - before;
            if( delta > 0 )      { ++positive; }
            else if( delta < 0 ) { ++negative; }
        }

        const auto changed = positive + negative;
        REQUIRE( changed > 0 );
        const auto positive_rate = static_cast<float>( positive ) / static_cast<float>( changed );
        INFO( "Positive: " << positive << "  Negative: " << negative
              << "  Rate: " << positive_rate );
        CHECK( positive_rate > 0.80f );
    }

    SECTION( "Low score target yields lower positive rate than high score target" ) {
        auto positive_high = 0;
        auto changed_high  = 0;
        auto positive_low  = 0;
        auto changed_low   = 0;

        {
            override_option target_opt( "MUTATION_SCORE_TARGET", "20" );
            for( auto i = 0; i < trials; ++i ) {
                npc dummy;
                const auto before = mutation_cost_sum( dummy );
                dummy.mutate_category( cat_alpha );
                const auto delta = mutation_cost_sum( dummy ) - before;
                if( delta != 0 ) {
                    ++changed_high;
                    if( delta > 0 ) { ++positive_high; }
                }
            }
        }
        {
            override_option target_opt( "MUTATION_SCORE_TARGET", "-20" );
            for( auto i = 0; i < trials; ++i ) {
                npc dummy;
                const auto before = mutation_cost_sum( dummy );
                dummy.mutate_category( cat_alpha );
                const auto delta = mutation_cost_sum( dummy ) - before;
                if( delta != 0 ) {
                    ++changed_low;
                    if( delta > 0 ) { ++positive_low; }
                }
            }
        }

        REQUIRE( changed_high > 0 );
        REQUIRE( changed_low > 0 );
        const auto rate_high = static_cast<float>( positive_high ) / static_cast<float>( changed_high );
        const auto rate_low  = static_cast<float>( positive_low )  / static_cast<float>( changed_low );
        INFO( "High target positive rate: " << rate_high
              << "  Low target positive rate: " << rate_low );
        CHECK( rate_high > rate_low );
    }

    SECTION( "High score target preferentially removes existing bad mutations" ) {
        REQUIRE( trait_ROT3.is_valid() );
        override_option target_opt( "MUTATION_SCORE_TARGET", "20" );

        auto removed = 0;

        for( auto i = 0; i < trials; ++i ) {
            npc dummy;
            dummy.set_mutation( trait_ROT3 );
            dummy.mutate_category( cat_alpha );
            if( !dummy.has_trait( trait_ROT3 ) ) {
                ++removed;
            }
        }

        const auto removal_rate = static_cast<float>( removed ) / static_cast<float>( trials );
        INFO( "ROT3 removal rate: " << removal_rate );
        CHECK( removal_rate > 0.30f );
    }
}
