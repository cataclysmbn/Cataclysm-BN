#include "catch/catch.hpp"

#include "magic.h"
#include "magic_enchantment.h"
#include "map.h"
#include "map_helpers.h"
#include "item.h"
#include "options.h"
#include "player.h"
#include "player_helpers.h"
#include "state_helpers.h"

static trait_id trait_CARNIVORE( "CARNIVORE" );
static efftype_id effect_debug_clairvoyance( "debug_clairvoyance" );

static void advance_turn( Character &guy )
{
    guy.process_turn();
    calendar::turn += 1_turns;
}

static item &give_item( Character &guy, const std::string &item_id )
{
    detached_ptr<item> det = item::spawn( item_id );
    item &ret = *det;
    guy.i_add( std::move( det ) );
    guy.recalculate_enchantment_cache();
    return ret;
}

static item &wear_item( Character &guy, const std::string &item_id )
{
    detached_ptr<item> det = item::spawn( item_id );
    item &ret = *det;
    guy.wear_item( std::move( det ), false );
    guy.recalculate_enchantment_cache();
    return ret;
}

static void clear_items( Character &guy )
{
    guy.inv_clear();
    guy.recalculate_enchantment_cache();
}

TEST_CASE( "Enchantments grant mutations", "[magic][enchantment][trait][mutation]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    advance_turn( guy );

    std::string s_relic = "test_relic_gives_trait";

    WHEN( "Character doesn't have trait" ) {
        REQUIRE( !guy.has_trait( trait_CARNIVORE ) );
        AND_WHEN( "Character receives relic" ) {
            give_item( guy, s_relic );
            THEN( "Character gains trait" ) {
                CHECK( guy.has_trait( trait_CARNIVORE ) );
            }
            AND_WHEN( "Turn passes" ) {
                advance_turn( guy );
                THEN( "Character still has trait" ) {
                    CHECK( guy.has_trait( trait_CARNIVORE ) );
                }
                AND_WHEN( "Character loses relic" ) {
                    clear_items( guy );
                    THEN( "Character loses trait" ) {
                        CHECK_FALSE( guy.has_trait( trait_CARNIVORE ) );
                    }
                    AND_WHEN( "Turn passes" ) {
                        advance_turn( guy );
                        THEN( "Character still has no trait" ) {
                            CHECK_FALSE( guy.has_trait( trait_CARNIVORE ) );
                        }
                    }
                }
            }
        }
    }

    WHEN( "Character has trait" ) {
        guy.set_mutation( trait_CARNIVORE );
        REQUIRE( guy.has_trait( trait_CARNIVORE ) );
        AND_WHEN( "Character receives relic" ) {
            give_item( guy, s_relic );
            THEN( "Nothing changes" ) {
                CHECK( guy.has_trait( trait_CARNIVORE ) );
            }
            AND_WHEN( "Turn passes" ) {
                advance_turn( guy );
                THEN( "Nothing changes" ) {
                    CHECK( guy.has_trait( trait_CARNIVORE ) );
                }
                AND_WHEN( "Character loses relic" ) {
                    clear_items( guy );
                    THEN( "Nothing changes" ) {
                        CHECK( guy.has_trait( trait_CARNIVORE ) );
                    }
                    AND_WHEN( "Turn passes" ) {
                        advance_turn( guy );
                        THEN( "Nothing changes" ) {
                            CHECK( guy.has_trait( trait_CARNIVORE ) );
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE( "Enchantments apply effects", "[magic][enchantment][effect]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    advance_turn( guy );

    std::string s_relic = "architect_cube";

    // TODO: multiple enchantments apply same effect of
    //       same or different intensity
    // TODO: enchantments apply effect while char already has effect of
    //       same, stronger or weaker intensity

    WHEN( "Character doesn't have effect" ) {
        REQUIRE( !guy.has_effect( effect_debug_clairvoyance ) );
        AND_WHEN( "Character receives relic" ) {
            give_item( guy, s_relic );
            THEN( "Character still doesn't have effect" ) {
                CHECK_FALSE( guy.has_effect( effect_debug_clairvoyance ) );
            }
            AND_WHEN( "Turn passes" ) {
                advance_turn( guy );
                THEN( "Character receives effect" ) {
                    CHECK( guy.has_effect( effect_debug_clairvoyance ) );
                }
                AND_WHEN( "Character loses relic" ) {
                    clear_items( guy );
                    THEN( "Character still has effect" ) {
                        CHECK( guy.has_effect( effect_debug_clairvoyance ) );
                    }
                    AND_WHEN( "Turn passes" ) {
                        advance_turn( guy );

                        // FIXME: effects should go away after 1 turn!
                        CHECK( guy.has_effect( effect_debug_clairvoyance ) );
                        advance_turn( guy );

                        THEN( "Character loses effect" ) {
                            CHECK_FALSE( guy.has_effect( effect_debug_clairvoyance ) );
                        }
                    }
                }
            }
        }
    }
}

static void tests_stats( Character &guy, int s_base, int d_base, int p_base, int i_base, int s_exp,
                         int d_exp, int p_exp, int i_exp )
{
    guy.str_max = s_base;
    guy.dex_max = d_base;
    guy.per_max = p_base;
    guy.int_max = i_base;

    advance_turn( guy );

    auto check_stats = [&]( int s, int d, int p, int i ) {
        REQUIRE( guy.get_str_base() == s_base );
        REQUIRE( guy.get_dex_base() == d_base );
        REQUIRE( guy.get_per_base() == p_base );
        REQUIRE( guy.get_int_base() == i_base );

        CHECK( guy.get_str() == s );
        CHECK( guy.get_dex() == d );
        CHECK( guy.get_per() == p );
        CHECK( guy.get_int() == i );
    };
    auto check_stats_base = [&]() {
        check_stats( s_base, d_base, p_base, i_base );
    };

    check_stats_base();

    std::string s_relic = "test_relic_mods_stats";

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Stats remain unchanged" ) {
            check_stats_base();
        }
        AND_WHEN( "Turn passes" ) {
            advance_turn( guy );
            THEN( "Stats are modified and don't overflow" ) {
                check_stats( s_exp, d_exp, p_exp, i_exp );
            }
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Stats remain unchanged" ) {
                    check_stats( s_exp, d_exp, p_exp, i_exp );
                }
                AND_WHEN( "Turn passes" ) {
                    advance_turn( guy );
                    THEN( "Stats return to normal" ) {
                        check_stats_base();
                    }
                }
            }
        }
    }
}

TEST_CASE( "Enchantments modify stats", "[magic][enchantment][character]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    SECTION( "base stats 8" ) {
        tests_stats( guy, 8, 8, 8, 8, 20, 6, 5, 0 );
    }
    SECTION( "base stats 12" ) {
        tests_stats( guy, 12, 12, 12, 12, 28, 10, 7, 1 );
    }
    SECTION( "base stats 4" ) {
        tests_stats( guy, 4, 4, 4, 4, 12, 2, 3, 0 );
    }
}

static void tests_speed( Character &guy, int sp_base, int sp_exp )
{
    guy.set_speed_base( sp_base );
    guy.set_speed_bonus( 0 );

    guy.set_moves( 0 );
    advance_turn( guy );

    std::string s_relic = "test_relic_mods_speed";

    auto check_speed = [&]( int speed, int moves ) {
        REQUIRE( guy.get_speed_base() == sp_base );
        REQUIRE( guy.get_speed() == speed );
        REQUIRE( guy.get_moves() == moves );
    };
    auto check_speed_is_base = [&] {
        check_speed( sp_base, sp_base );
    };

    WHEN( "Character has no relics" ) {
        THEN( "Speed and moves gain equel base" ) {
            check_speed_is_base();
        }
    }
    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Nothing changes" ) {
            check_speed_is_base();
        }
        AND_WHEN( "Turn passes" ) {
            guy.set_moves( 0 );
            advance_turn( guy );
            THEN( "Speed changes, moves gain changes" ) {
                check_speed( sp_exp, sp_exp );
            }
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Nothing changes" ) {
                    check_speed( sp_exp, sp_exp );
                }
                AND_WHEN( "Turn passes" ) {
                    guy.set_moves( 0 );
                    advance_turn( guy );
                    THEN( "Speed and moves gain return to normal" ) {
                        check_speed_is_base();
                    }
                }
            }
        }
    }
    WHEN( "Character receives 10 relics" ) {
        for( int i = 0; i < 10; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Nothing changes" ) {
            check_speed_is_base();
        }
        AND_WHEN( "Turn passes" ) {
            guy.set_moves( 0 );
            advance_turn( guy );
            THEN( "Speed and moves gain do not fall below 25% of base" ) {
                check_speed( sp_base / 4, sp_base / 4 );
            }
        }
    }
}

TEST_CASE( "Enchantments modify speed", "[magic][enchantment][speed]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    SECTION( "base = 100" ) {
        tests_speed( guy, 100, 75 );
    }
    SECTION( "base = 80" ) {
        tests_speed( guy, 80, 65 );
    }
    SECTION( "base = 120" ) {
        tests_speed( guy, 120, 85 );
    }
}

static void tests_attack_cost( Character &guy, const item &weap, int item_atk_cost,
                               int guy_atk_cost, int exp_guy_atk_cost )
{
    advance_turn( guy );

    REQUIRE( weap.attack_cost() == item_atk_cost );
    REQUIRE( guy.attack_cost( weap ) == guy_atk_cost );

    std::string s_relic = "test_relic_mods_atk_cost";

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Attack cost changes" ) {
            CHECK( guy.attack_cost( weap ) == exp_guy_atk_cost );
        }
        AND_WHEN( "Character loses relic" ) {
            clear_items( guy );
            THEN( "Attack cost returns to normal" ) {
                CHECK( guy.attack_cost( weap ) == guy_atk_cost );
            }
        }
    }
    WHEN( "Character receives 10 relics" ) {
        for( int i = 0; i < 10; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Attack cost does not drop below 25" ) {
            CHECK( guy.attack_cost( weap ) == 25 );
        }
    }
}

TEST_CASE( "Enchantments modify attack cost", "[magic][enchantment][melee]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    SECTION( "normal sword" ) {
        tests_attack_cost( guy, *item::spawn_temporary( "test_normal_sword" ), 101, 82, 66 );
    }
    SECTION( "normal sword + ITEM_ATTACK_COST" ) {
        tests_attack_cost( guy, *item::spawn_temporary( "test_relic_sword" ), 86, 78, 63 );
    }
}

static void tests_move_cost( Character &guy, int tile_move_cost, int move_cost, int exp_move_cost )
{
    advance_turn( guy );

    std::string s_relic = "test_relic_mods_mv_cost";

    REQUIRE( guy.run_cost( tile_move_cost ) == move_cost );

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Move cost changes" ) {
            CHECK( guy.run_cost( tile_move_cost ) == exp_move_cost );
        }
        AND_WHEN( "Character loses relic" ) {
            clear_items( guy );
            THEN( "Move cost goes back to normal" ) {
                CHECK( guy.run_cost( tile_move_cost ) == move_cost );
            }
        }
    }
    WHEN( "Character receives 15 relics" ) {
        for( int i = 0; i < 15; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Move cost does not drop below 20" ) {
            CHECK( guy.run_cost( tile_move_cost ) == 20 );
        }
    }
}

TEST_CASE( "Enchantments modify move cost", "[magic][enchantment][move]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    SECTION( "Naked character" ) {
        SECTION( "tile move cost = 100" ) {
            tests_move_cost( guy, 100, 100, 90 );
        }
        SECTION( "tile move cost = 120" ) {
            tests_move_cost( guy, 120, 120, 108 );
        }
    }
    SECTION( "Naked character with PADDED_FEET" ) {
        trait_id tr( "PADDED_FEET" );
        guy.set_mutation( tr );
        REQUIRE( guy.has_trait( tr ) );

        SECTION( "tile move cost = 100" ) {
            tests_move_cost( guy, 100, 90, 81 );
        }
        SECTION( "tile move cost = 120" ) {
            tests_move_cost( guy, 120, 108, 97 );
        }
    }
}

static void tests_metabolic_rate( Character &guy, float norm, float exp )
{
    advance_turn( guy );

    std::string s_relic = "test_relic_mods_metabolism";

    REQUIRE( guy.metabolic_rate_base() == Approx( norm ) );

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Metabolic rate changes" ) {
            CHECK( guy.metabolic_rate_base() == Approx( exp ) );
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Metabolic rate goes back to normal" ) {
                    CHECK( guy.metabolic_rate_base() == Approx( norm ) );
                }
            }
        }
    }
    WHEN( "Character receives 15 relics" ) {
        for( int i = 0; i < 15; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Metabolic rate does not go below 0" ) {
            CHECK( guy.metabolic_rate_base() == Approx( 0.0f ) );
        }
    }
}

TEST_CASE( "Enchantments modify metabolic rate", "[magic][enchantment][metabolism]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    advance_turn( guy );

    const float normal_mr = get_option<float>( "PLAYER_HUNGER_RATE" );
    REQUIRE( guy.metabolic_rate_base() == normal_mr );
    REQUIRE( normal_mr == 1.0f );

    SECTION( "Clean character" ) {
        tests_metabolic_rate( guy, 1.0f, 0.9f );
    }
    SECTION( "Character with HUNGER trait" ) {
        trait_id tr( "HUNGER" );
        guy.set_mutation( tr );
        REQUIRE( guy.has_trait( tr ) );

        tests_metabolic_rate( guy, 1.5f, 1.35f );
    }
}

struct mana_test_case {
    int idx;
    int intellect;
    int norm_cap;
    int exp_cap;
    double norm_regen_amt_8h;
    double exp_regen_amt_8h;
};

static const std::vector<mana_test_case> mana_test_data = {{
        {0, 8, 1000, 800, 1000.0, 560.0},
        {1, 12, 1400, 1080, 1400.0, 686.0},
    }
};

static void tests_mana_pool( Character &guy, const mana_test_case &t )
{
    double norm_regen_rate = t.norm_regen_amt_8h / to_turns<double>( time_duration::from_hours( 8 ) );
    double exp_regen_rate = t.exp_regen_amt_8h / to_turns<double>( time_duration::from_hours( 8 ) );

    advance_turn( guy );

    guy.int_max = t.intellect;
    guy.int_cur = guy.int_max;
    REQUIRE( guy.get_int() == t.intellect );

    REQUIRE( guy.magic->max_mana( guy ) == t.norm_cap );
    REQUIRE( guy.magic->mana_regen_rate( guy ) == Approx( norm_regen_rate ) );

    const std::string s_relic = "test_relic_mods_manapool";

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Mana pool capacity and regen rate change" ) {
            CHECK( guy.magic->max_mana( guy ) == t.exp_cap );
            CHECK( guy.magic->mana_regen_rate( guy ) == Approx( exp_regen_rate ) );
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Mana pool capacity and regen rate go back to normal" ) {
                    REQUIRE( guy.magic->max_mana( guy ) == t.norm_cap );
                    REQUIRE( guy.magic->mana_regen_rate( guy ) == Approx( norm_regen_rate ) );
                }
            }
        }
    }
    WHEN( "Character receives 10 relics" ) {
        for( int i = 0; i < 10; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Mana pool capacity and regen rate don't drop below 0" ) {
            REQUIRE( guy.magic->max_mana( guy ) == 0 );
            REQUIRE( guy.magic->mana_regen_rate( guy ) == Approx( 0.0 ) );
        }
    }
}

static void tests_mana_pool_section( const mana_test_case &t )
{
    CAPTURE( t.idx );

    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    tests_mana_pool( guy, t );
}

TEST_CASE( "Mana pool", "[magic][enchantment][mana]" )
{
    clear_all_state();
    for( const mana_test_case &it : mana_test_data ) {
        tests_mana_pool_section( it );
    }
}

static float measure_stamina_gain_rate( Character &guy )
{
    int gained_total = 0;
    // Stamina regen rate is supposed to decrease over time as character gains stamina,
    // so we measure 100 times on same level instead of doing update_stamina( 100 )
    for( int i = 0; i < 100; i++ ) {
        guy.set_stamina( 0 );
        if( guy.get_stamina() != 0 ) {
            // Hide this behind an if check to avoid spamming check counter
            REQUIRE( guy.get_stamina() == 0 );
        }
        guy.update_stamina( 1 );
        gained_total += guy.get_stamina();
    }
    return gained_total / 100.0f;
}

static void tests_stamina( Character &guy,
                           int cap_norm, int cap_exp,
                           float rate_norm, float rate_exp
                         )
{
    advance_turn( guy );

    std::string s_relic = "test_relic_mods_stamina";

    REQUIRE( guy.get_stamina_max() == cap_norm );
    REQUIRE( measure_stamina_gain_rate( guy ) == Approx( rate_norm ) );

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Stamina cap changes" ) {
            CHECK( guy.get_stamina_max() == cap_exp );
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Stamina cap goes back to normal" ) {
                    CHECK( guy.get_stamina_max() == cap_norm );
                }
            }
        }
        THEN( "Stamina gain rate changes" ) {
            CHECK( measure_stamina_gain_rate( guy ) == Approx( rate_exp ) );
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Stamina gain rate goes back to normal" ) {
                    CHECK( measure_stamina_gain_rate( guy ) == Approx( rate_norm ) );
                }
            }
        }
    }
    WHEN( "Character receives 15 relics" ) {
        for( int i = 0; i < 15; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Stamina cap does not go below 0.1 of PLAYER_MAX_STAMINA" ) {
            const int base_cap = get_option<int>( "PLAYER_MAX_STAMINA" );
            CHECK( guy.get_stamina_max() == ( base_cap / 10 ) );
        }
        THEN( "Stamina gain rate does not go below 0" ) {
            CHECK( measure_stamina_gain_rate( guy ) == Approx( 0.0f ) );
        }
    }
}

TEST_CASE( "Enchantments modify stamina", "[magic][enchantment][stamina]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    advance_turn( guy );

    REQUIRE( guy.get_stim() == 0 );

    const int normal_cap = get_option<int>( "PLAYER_MAX_STAMINA" );
    REQUIRE( normal_cap == 10000 );
    REQUIRE( guy.get_stamina_max() == normal_cap );

    const float normal_rate = get_option<float>( "PLAYER_BASE_STAMINA_REGEN_RATE" );
    REQUIRE( normal_rate == Approx( 20.0f ) );
    REQUIRE( measure_stamina_gain_rate( guy ) == Approx( normal_rate ) );

    guy.set_stamina( 0 );
    REQUIRE( guy.get_stamina() == 0 );

    SECTION( "Clean character" ) {
        tests_stamina( guy, 10000, 9000, 20.0f, 18.0f );
    }
    SECTION( "Character with GOODCARDIO trait" ) {
        trait_id tr( "GOODCARDIO" );
        guy.set_mutation( tr );
        REQUIRE( guy.has_trait( tr ) );

        tests_stamina( guy, 12500, 11250, 25.0f, 23.0f );
    }
    SECTION( "Character with PERSISTENCE_HUNTER trait" ) {
        trait_id tr( "PERSISTENCE_HUNTER" );
        guy.set_mutation( tr );
        REQUIRE( guy.has_trait( tr ) );

        tests_stamina( guy, 10000, 9000, 22.0f, 20.0f );
    }
    SECTION( "Character with GOODCARDIO and PERSISTENCE_HUNTER traits" ) {
        {
            trait_id tr( "GOODCARDIO" );
            guy.set_mutation( tr );
            REQUIRE( guy.has_trait( tr ) );
        }
        {
            trait_id tr( "PERSISTENCE_HUNTER" );
            guy.set_mutation( tr );
            REQUIRE( guy.has_trait( tr ) );
        }
        tests_stamina( guy, 12500, 11250, 27.0f, 25.0f );
    }
}

template<typename F>
static void tests_need_rate( Character &guy, const std::string &s_relic, float norm, float exp,
                             F getter )
{
    advance_turn( guy );

    REQUIRE( getter( guy ) == Approx( norm ) );

    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Need rate changes" ) {
            CHECK( getter( guy ) == Approx( exp ) );
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Need rate goes back to normal" ) {
                    CHECK( getter( guy ) == Approx( norm ) );
                }
            }
        }
    }
    WHEN( "Character receives 15 relics" ) {
        for( int i = 0; i < 15; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Need rate does not go below 0" ) {
            CHECK( getter( guy ) == Approx( 0.0f ) );
        }
    }
}

TEST_CASE( "Enchantments modify thirst rate", "[magic][enchantment][thirst]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    advance_turn( guy );

    std::string s_relic = "test_relic_mods_thirst";
    const auto getter = []( const Character & guy ) -> float {
        return guy.calc_needs_rates().thirst;
    };

    const float normal_rate = get_option<float>( "PLAYER_THIRST_RATE" );
    REQUIRE( normal_rate == Approx( 1.0f ) );
    REQUIRE( getter( guy ) == Approx( normal_rate ) );

    SECTION( "Clean character" ) {
        tests_need_rate( guy, s_relic, 1.0f, 0.9f, getter );
    }
    SECTION( "Character with THIRST trait" ) {
        trait_id tr( "THIRST" );
        guy.set_mutation( tr );
        REQUIRE( guy.has_trait( tr ) );

        tests_need_rate( guy, s_relic, 1.5f, 1.4f, getter );
    }
}

TEST_CASE( "Enchantments modify fatigue rate", "[magic][enchantment][fatigue]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    advance_turn( guy );

    std::string s_relic = "test_relic_mods_fatigue";
    const auto getter = []( const Character & guy ) -> float {
        return guy.calc_needs_rates().fatigue;
    };

    const float normal_rate = get_option<float>( "PLAYER_THIRST_RATE" );
    REQUIRE( normal_rate == Approx( 1.0f ) );
    REQUIRE( getter( guy ) == Approx( normal_rate ) );

    SECTION( "Clean character" ) {
        tests_need_rate( guy, s_relic, 1.0f, 0.9f, getter );
    }
    SECTION( "Character with WAKEFUL trait" ) {
        trait_id tr( "WAKEFUL" );
        guy.set_mutation( tr );
        REQUIRE( guy.has_trait( tr ) );

        tests_need_rate( guy, s_relic, 0.85f, 0.75f, getter );
    }
}

static void check_num_dodges( const Character &guy, int num )
{
    CHECK( guy.get_num_dodges() == num );
    CHECK( guy.dodges_left == num );
}

static void tests_num_dodges( Character &guy )
{
    // Must have some moves to gain dodges
    guy.moves = 1;
    guy.dodges_left = 0;

    advance_turn( guy );

    REQUIRE( guy.get_num_dodges_base() == 1 );
    REQUIRE( guy.get_num_dodges_bonus() == 0 );
    REQUIRE( guy.get_num_dodges() == 1 );
    REQUIRE( guy.dodges_left == 1 );

    std::string s_relic = "test_relic_mods_dodges";

    WHEN( "Character has no relics" ) {
        THEN( "Dodges bonus remain unaffected" ) {
            guy.moves = 1;
            guy.dodges_left = 0;
            advance_turn( guy );
            check_num_dodges( guy, 1 );
        }
    }
    WHEN( "Character receives relic" ) {
        give_item( guy, s_relic );
        THEN( "Nothing changes" ) {
            check_num_dodges( guy, 1 );
        }
        AND_WHEN( "Turn passes" ) {
            guy.moves = 1;
            guy.dodges_left = 0;
            advance_turn( guy );
            THEN( "Dodge bonus changes, dodges increase" ) {
                check_num_dodges( guy, 2 );
            }
            AND_WHEN( "Character loses relic" ) {
                clear_items( guy );
                THEN( "Nothing changes" ) {
                    check_num_dodges( guy, 2 );
                }
                AND_WHEN( "Turn passes" ) {
                    guy.moves = 1;
                    guy.dodges_left = 0;
                    advance_turn( guy );
                    THEN( "Dodge bonus and dodge gain return to normal" ) {
                        check_num_dodges( guy, 1 );
                    }
                }
            }
        }
    }
    WHEN( "Character receives 10 relics" ) {
        for( int i = 0; i < 10; i++ ) {
            give_item( guy, s_relic );
        }
        THEN( "Nothing changes" ) {
            check_num_dodges( guy, 1 );
        }
        AND_WHEN( "Turn passes" ) {
            guy.moves = 1;
            guy.dodges_left = 0;
            advance_turn( guy );
            THEN( "Dodge bonus and dodge gain increase by 10" ) {
                check_num_dodges( guy, 11 );
            }
        }
    }
}

TEST_CASE( "Enchantments grant bonus dodges", "[magic][enchantment][dodge]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    tests_num_dodges( guy );
}

TEST_CASE( "Item enchantments modify item damage", "[magic][enchantment]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    SECTION( "Cut damage" ) {
        item &base = give_item( guy, "test_balanced_sword" );
        item &impr = give_item( guy, "test_relic_mods_cut_dmg" );

        REQUIRE( base.damage_melee( damage_type::DT_CUT ) == 32 );
        CHECK( impr.damage_melee( damage_type::DT_CUT ) == 17 );
    }
    SECTION( "Stab damage" ) {
        item &base = give_item( guy, "test_screwdriver" );
        item &impr = give_item( guy, "test_relic_mods_stab_dmg" );

        REQUIRE( base.damage_melee( damage_type::DT_STAB ) == 6 );
        CHECK( impr.damage_melee( damage_type::DT_STAB ) == 4 );
    }
    SECTION( "Bash damage" ) {
        item &base = give_item( guy, "test_halligan" );
        item &impr = give_item( guy, "test_relic_mods_bash_dmg" );

        REQUIRE( base.damage_melee( damage_type::DT_BASH ) == 20 );
        CHECK( impr.damage_melee( damage_type::DT_BASH ) == 11 );
    }
}

static int calc_damage_absorb( Character &guy, damage_type dt, int amount )
{
    static const bodypart_id torso( "torso" );
    damage_instance dmg( dt, amount );
    guy.absorb_hit( torso, dmg );
    assert( dmg.damage_units.size() == 1 );
    return amount - dmg.damage_units[0].amount;
}

TEST_CASE( "Armor enchantments", "[magic][enchantment][armor]" )
{
    clear_all_state();
    Character &guy = get_player_character();
    clear_character( *guy.as_player(), true );

    REQUIRE( calc_damage_absorb( guy, damage_type::DT_CUT, 10 ) == 0 );
    REQUIRE( calc_damage_absorb( guy, damage_type::DT_BASH, 10 ) == 0 );
    REQUIRE( calc_damage_absorb( guy, damage_type::DT_STAB, 10 ) == 0 );

    SECTION( "Armor item with no enchantments" ) {
        wear_item( guy, "test_hazmat_suit" );

        SECTION( "Cut" ) {
            // 10 (incoming) - 4 (base item cut armor) = 6 (4 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_CUT, 10 ) == 4 );
        }
        SECTION( "Bash" ) {
            // 10 (incoming) - 4 (base item bash armor) = 6 (4 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_BASH, 10 ) == 4 );
        }
        SECTION( "Stab" ) {
            // 10 (incoming) - 3 (base item stab armor) = 7 (3 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_STAB, 10 ) == 3 );
        }
    }

    SECTION( "Armor item with enchantment that trades bash armor for cut armor" ) {
        wear_item( guy, "test_relic_item_armor_mod" );

        SECTION( "Cut" ) {
            // 10 (incoming) + (10 * -0.5 + 3) (enchantment) - 4 (base item cut armor) = 4 (6 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_CUT, 10 ) == 6 );
        }
        SECTION( "Bash" ) {
            // 10 (incoming) + (10 * 0.5 - 3) (enchantment) - 4 (base item bash armor) = 8 (2 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_BASH, 10 ) == 2 );
        }
        SECTION( "Stab" ) {
            // 10 (incoming) - 3 (base item stab armor) = 7 (3 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_STAB, 10 ) == 3 );
        }
    }

    SECTION( "Armor item with no enchantments + socks of protection" ) {
        wear_item( guy, "test_hazmat_suit" );
        // The socks provide character-wide protection regardless of what body parts they cover
        wear_item( guy, "test_relic_char_armor_mod" );

        SECTION( "Cut" ) {
            // 10 (incoming) + (10 * -0.5 - 2) (enchantment) - 4 (base item cut armor) = -1 (10 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_CUT, 10 ) == 10 );
        }
        SECTION( "Bash" ) {
            // 10 (incoming) - 4 (base item bash armor) = 6 (4 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_BASH, 10 ) == 4 );
        }
        SECTION( "Stab" ) {
            // 10 (incoming) + (10 * -0.1 - 3) (enchantment) - 3 (base item stab armor) = 3 (7 absorbed)
            CHECK( calc_damage_absorb( guy, damage_type::DT_STAB, 10 ) == 7 );
        }
    }
}
