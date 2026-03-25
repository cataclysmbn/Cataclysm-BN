#include "catch/catch.hpp"

#include <ranges>
#include <string>

#include "calendar.h"
#include "item.h"
#include "itype.h"
#include "procgen/proc_builder.h"
#include "procgen/proc_fact.h"
#include "procgen/proc_item.h"

namespace
{

auto make_proc_test_item( const itype_id &id, const proc::schema_id &schema,
                          const std::string &fp ) -> detached_ptr<item>
{
    auto crafted = item::spawn( id, calendar::turn );
    auto payload = proc::payload{};
    payload.id = schema;
    payload.fp = fp;
    payload.blob.name = crafted->type_name();
    proc::write_payload( *crafted, payload );
    return crafted;
}

auto has_tag( const proc::part_fact &fact, const std::string &tag ) -> bool
{
    return std::ranges::find( fact.tag, tag ) != fact.tag.end();
}

} // namespace

TEST_CASE( "proc_part_fact_normalizes_basic_item_fields", "[proc][fact]" )
{
    const auto apple = item( "apple" );
    const auto fact = proc::normalize_part_fact( apple, { .ix = 3 } );

    CHECK( fact.valid() );
    CHECK( fact.ix == 3 );
    CHECK( fact.id == itype_id( "apple" ) );
    CHECK_FALSE( fact.mat.empty() );
    CHECK( fact.mass_g > 0 );
    CHECK( fact.volume_ml > 0 );
    CHECK( fact.kcal == apple.get_comestible()->default_nutrition.kcal );
    CHECK( fact.hp == Approx( 1.0f ) );
}

TEST_CASE( "proc_part_fact_normalizes_damage_and_charges", "[proc][fact]" )
{
    auto shirt = item( "jeans" );
    REQUIRE( shirt.max_damage() > 0 );
    shirt.set_damage( 1 );

    const auto damaged_fact = proc::normalize_part_fact( shirt, { .ix = 1 } );
    CHECK( damaged_fact.hp < 1.0f );

    const auto battery = item( "battery" );
    const auto battery_fact = proc::normalize_part_fact( battery, {
        .ix = 2,
        .charges = 1,
        .uses = battery.charges
    } );
    CHECK( battery_fact.chg == 1 );
    CHECK( battery_fact.uses == battery.charges );
}

TEST_CASE( "proc_part_fact_assigns_food_role_tags", "[proc][fact]" )
{
    const auto bread = item( "bread" );
    const auto bread_fact = proc::normalize_part_fact( bread, { .ix = 1 } );
    CHECK( has_tag( bread_fact, "bread" ) );

    const auto biscuit = item( "biscuit" );
    const auto biscuit_fact = proc::normalize_part_fact( biscuit, { .ix = 9 } );
    CHECK( has_tag( biscuit_fact, "bread" ) );

    const auto hardtack = item( "hardtack" );
    const auto hardtack_fact = proc::normalize_part_fact( hardtack, { .ix = 10 } );
    CHECK( has_tag( hardtack_fact, "bread" ) );

    const auto brioche = item( "brioche" );
    const auto brioche_fact = proc::normalize_part_fact( brioche, { .ix = 11 } );
    CHECK( has_tag( brioche_fact, "bread" ) );

    const auto meat = item( "meat_cooked" );
    const auto meat_fact = proc::normalize_part_fact( meat, { .ix = 2 } );
    CHECK( has_tag( meat_fact, "meat" ) );

    const auto rollmat = item( "rollmat" );
    const auto rollmat_fact = proc::normalize_part_fact( rollmat, { .ix = 3 } );
    CHECK_FALSE( has_tag( rollmat_fact, "bread" ) );

    const auto fertilizer = item( "fertilizer" );
    const auto fertilizer_fact = proc::normalize_part_fact( fertilizer, { .ix = 4 } );
    CHECK_FALSE( has_tag( fertilizer_fact, "veg" ) );
}

TEST_CASE( "proc_part_fact_limits_bread_tags_to_sandwichable_bases", "[proc][fact]" )
{
    const auto sweetbread_fact = proc::normalize_part_fact( item( "sweetbread" ), { .ix = 30 } );
    CHECK_FALSE( has_tag( sweetbread_fact, "bread" ) );

    const auto pancake_fact = proc::normalize_part_fact( item( "pancakes" ), { .ix = 31 } );
    CHECK_FALSE( has_tag( pancake_fact, "bread" ) );

    const auto waffle_fact = proc::normalize_part_fact( item( "waffles" ), { .ix = 32 } );
    CHECK_FALSE( has_tag( waffle_fact, "bread" ) );

    const auto cracker_fact = proc::normalize_part_fact( item( "crackers" ), { .ix = 33 } );
    CHECK_FALSE( has_tag( cracker_fact, "bread" ) );

    const auto pretzel_fact = proc::normalize_part_fact( item( "pretzels" ), { .ix = 34 } );
    CHECK_FALSE( has_tag( pretzel_fact, "bread" ) );

    const auto snack_cake_fact = proc::normalize_part_fact( item( "cake2" ), { .ix = 35 } );
    CHECK_FALSE( has_tag( snack_cake_fact, "bread" ) );
}

TEST_CASE( "proc_part_fact_assigns_cond_tags_to_supported_sandwich_condiments", "[proc][fact]" )
{
    const auto butter_fact = proc::normalize_part_fact( item( "butter" ), { .ix = 13 } );
    CHECK( has_tag( butter_fact, "cond" ) );

    const auto horseradish_fact = proc::normalize_part_fact( item( "horseradish" ), { .ix = 14 } );
    CHECK( has_tag( horseradish_fact, "cond" ) );

    const auto sauerkraut_fact = proc::normalize_part_fact( item( "sauerkraut" ), { .ix = 15 } );
    CHECK( has_tag( sauerkraut_fact, "cond" ) );

    const auto honey_fact = proc::normalize_part_fact( item( "honey_bottled" ), { .ix = 16 } );
    CHECK( has_tag( honey_fact, "cond" ) );

    const auto jam_fact = proc::normalize_part_fact( item( "jam_fruit" ), { .ix = 17 } );
    CHECK( has_tag( jam_fact, "cond" ) );

    const auto peanut_butter_fact = proc::normalize_part_fact( item( "peanutbutter" ), { .ix = 18 } );
    CHECK( has_tag( peanut_butter_fact, "cond" ) );

    const auto syrup_fact = proc::normalize_part_fact( item( "syrup" ), { .ix = 19 } );
    CHECK( has_tag( syrup_fact, "cond" ) );
}

TEST_CASE( "proc_part_fact_does_not_tag_non_cheese_dairy_as_cheese", "[proc][fact]" )
{
    const auto butter_fact = proc::normalize_part_fact( item( "butter" ), { .ix = 22 } );
    CHECK_FALSE( has_tag( butter_fact, "cheese" ) );

    const auto milk_fact = proc::normalize_part_fact( item( "milk" ), { .ix = 23 } );
    CHECK_FALSE( has_tag( milk_fact, "cheese" ) );

    const auto cheese_fact = proc::normalize_part_fact( item( "cheese" ), { .ix = 24 } );
    CHECK( has_tag( cheese_fact, "cheese" ) );
}

TEST_CASE( "proc_part_fact_does_not_tag_condiment_spreads_as_veg", "[proc][fact]" )
{
    const auto ketchup_fact = proc::normalize_part_fact( item( "ketchup" ), { .ix = 25 } );
    CHECK( has_tag( ketchup_fact, "cond" ) );
    CHECK_FALSE( has_tag( ketchup_fact, "veg" ) );

    const auto carrot_fact = proc::normalize_part_fact( item( "carrot" ), { .ix = 26 } );
    CHECK( has_tag( carrot_fact, "veg" ) );
}

TEST_CASE( "proc_part_fact_does_not_tag_liquid_stew_bases_as_veg", "[proc][fact]" )
{
    const auto broth_fact = proc::normalize_part_fact( item( "broth" ), { .ix = 36 } );
    CHECK_FALSE( has_tag( broth_fact, "veg" ) );

    const auto broth_bone_fact = proc::normalize_part_fact( item( "broth_bone" ), { .ix = 37 } );
    CHECK_FALSE( has_tag( broth_bone_fact, "veg" ) );
    CHECK_FALSE( has_tag( broth_bone_fact, "meat" ) );
}

TEST_CASE( "proc_part_fact_does_not_treat_finished_sandwiches_as_raw_ingredients", "[proc][fact]" )
{
    const auto cheese_sandwich = item( "sandwich_cheese" );
    const auto cheese_fact = proc::normalize_part_fact( cheese_sandwich, { .ix = 5 } );
    CHECK_FALSE( has_tag( cheese_fact, "bread" ) );
    CHECK_FALSE( has_tag( cheese_fact, "cheese" ) );

    const auto meat_sandwich = item( "sandwich_t" );
    const auto meat_fact = proc::normalize_part_fact( meat_sandwich, { .ix = 6 } );
    CHECK_FALSE( has_tag( meat_fact, "meat" ) );

    const auto veg_sandwich = item( "sandwich_veggy" );
    const auto veg_fact = proc::normalize_part_fact( veg_sandwich, { .ix = 7 } );
    CHECK_FALSE( has_tag( veg_fact, "veg" ) );

    const auto sauce_sandwich = item( "sandwich_sauce" );
    const auto sauce_fact = proc::normalize_part_fact( sauce_sandwich, { .ix = 8 } );
    CHECK_FALSE( has_tag( sauce_fact, "cond" ) );
}

TEST_CASE( "proc_part_fact_marks_finished_stews_and_curries_as_dishes", "[proc][fact]" )
{
    const auto veggie_soup = item( "soup_veggy" );
    const auto soup_fact = proc::normalize_part_fact( veggie_soup, { .ix = 11 } );
    CHECK( has_tag( soup_fact, "dish" ) );
    CHECK_FALSE( has_tag( soup_fact, "veg" ) );

    const auto meat_curry = item( "curry_meat" );
    const auto curry_fact = proc::normalize_part_fact( meat_curry, { .ix = 12 } );
    CHECK( has_tag( curry_fact, "dish" ) );
    CHECK_FALSE( has_tag( curry_fact, "veg" ) );
    CHECK_FALSE( has_tag( curry_fact, "meat" ) );
}

TEST_CASE( "proc_part_fact_marks_prepared_meals_as_dishes", "[proc][fact]" )
{
    const auto veggie_pie = item( "pie_veggy" );
    const auto pie_fact = proc::normalize_part_fact( veggie_pie, { .ix = 27 } );
    CHECK( has_tag( pie_fact, "dish" ) );
    CHECK_FALSE( has_tag( pie_fact, "bread" ) );
    CHECK_FALSE( has_tag( pie_fact, "veg" ) );

    const auto cheese_fries = item( "cheese_fries" );
    const auto fries_fact = proc::normalize_part_fact( cheese_fries, { .ix = 28 } );
    CHECK( has_tag( fries_fact, "dish" ) );
    CHECK_FALSE( has_tag( fries_fact, "cheese" ) );

    const auto burrito = item( "homemade_burrito" );
    const auto burrito_fact = proc::normalize_part_fact( burrito, { .ix = 29 } );
    CHECK( has_tag( burrito_fact, "dish" ) );
    CHECK_FALSE( has_tag( burrito_fact, "bread" ) );
    CHECK_FALSE( has_tag( burrito_fact, "meat" ) );
}

TEST_CASE( "proc_part_fact_treats_payload_marked_raw_food_as_finished_dish", "[proc][fact]" )
{
    const auto proc_bread_item = make_proc_test_item( itype_id( "bread" ),
                                 proc::schema_id( "sandwich" ),
                                 "sandwich:bread" );
    const auto proc_bread = proc::normalize_part_fact( *proc_bread_item, { .ix = 20 } );
    CHECK( has_tag( proc_bread, "dish" ) );
    CHECK_FALSE( has_tag( proc_bread, "bread" ) );
    CHECK_FALSE( proc_bread.proc.empty() );

    const auto proc_carrot_item = make_proc_test_item( itype_id( "carrot" ), proc::schema_id( "stew" ),
                                  "stew:carrot" );
    const auto proc_carrot = proc::normalize_part_fact( *proc_carrot_item, { .ix = 21 } );
    CHECK( has_tag( proc_carrot, "dish" ) );
    CHECK_FALSE( has_tag( proc_carrot, "veg" ) );
    CHECK_FALSE( proc_carrot.proc.empty() );
}

TEST_CASE( "proc_part_fact_assigns_trail_mix_tags", "[proc][fact]" )
{
    const auto peanut_fact = proc::normalize_part_fact( item( "peanut" ), { .ix = 38 } );
    CHECK( has_tag( peanut_fact, "trail_nut" ) );

    const auto dry_fruit_fact = proc::normalize_part_fact( item( "dry_fruit" ), { .ix = 39 } );
    CHECK( has_tag( dry_fruit_fact, "trail_dried" ) );

    const auto cranberry_fact = proc::normalize_part_fact( item( "cranberries" ), { .ix = 40 } );
    CHECK( has_tag( cranberry_fact, "trail_dried" ) );

    const auto chocolate_fact = proc::normalize_part_fact( item( "chocolate" ), { .ix = 41 } );
    CHECK( has_tag( chocolate_fact, "trail_sweet" ) );
}

TEST_CASE( "proc_part_fact_assigns_general_ingredient_category_tags", "[proc][fact]" )
{
    const auto fish_fact = proc::normalize_part_fact( item( "fish" ), { .ix = 42 } );
    CHECK( has_tag( fish_fact, "ingredient" ) );
    CHECK( has_tag( fish_fact, "meat" ) );
    CHECK( has_tag( fish_fact, "fish" ) );
    CHECK( has_tag( fish_fact, "raw" ) );
    CHECK( has_tag( fish_fact, "solid" ) );

    const auto egg_fact = proc::normalize_part_fact( item( "egg_chicken" ), { .ix = 43 } );
    CHECK( has_tag( egg_fact, "egg" ) );
    CHECK( has_tag( egg_fact, "raw" ) );
    CHECK( has_tag( egg_fact, "solid" ) );

    const auto noodle_fact = proc::normalize_part_fact( item( "spaghetti_raw" ), { .ix = 44 } );
    CHECK( has_tag( noodle_fact, "noodle" ) );
    CHECK( has_tag( noodle_fact, "grain" ) );
    CHECK( has_tag( noodle_fact, "wheat" ) );

    const auto sauce_fact = proc::normalize_part_fact( item( "sauce_red" ), { .ix = 45 } );
    CHECK( has_tag( sauce_fact, "ingredient" ) );
    CHECK( has_tag( sauce_fact, "liquid" ) );
    CHECK( has_tag( sauce_fact, "sauce" ) );

    const auto spread_fact = proc::normalize_part_fact( item( "peanutbutter" ), { .ix = 46 } );
    CHECK( has_tag( spread_fact, "spread" ) );
    CHECK( has_tag( spread_fact, "cond" ) );
    CHECK( has_tag( spread_fact, "nut" ) );

    const auto broth_fact = proc::normalize_part_fact( item( "broth" ), { .ix = 47 } );
    CHECK( has_tag( broth_fact, "broth" ) );
    CHECK( has_tag( broth_fact, "liquid" ) );
}

TEST_CASE( "proc_part_fact_search_finds_general_ingredient_categories", "[proc][fact][search]" )
{
    const auto fish_fact = proc::normalize_part_fact( item( "fish" ), { .ix = 48 } );
    CHECK( proc::part_matches_search( fish_fact, {}, "tag:fish tag:raw" ) );
    CHECK_FALSE( proc::part_matches_search( fish_fact, {}, "tag:egg" ) );

    const auto noodle_fact = proc::normalize_part_fact( item( "spaghetti_raw" ), { .ix = 49 } );
    CHECK( proc::part_matches_search( noodle_fact, {}, "tag:noodle tag:wheat" ) );

    const auto sauce_fact = proc::normalize_part_fact( item( "sauce_red" ), { .ix = 50 } );
    CHECK( proc::part_matches_search( sauce_fact, {}, "tag:sauce tag:liquid" ) );

    const auto egg_fact = proc::normalize_part_fact( item( "egg_chicken" ), { .ix = 51 } );
    CHECK( proc::part_matches_search( egg_fact, {}, "tag:egg tag:raw" ) );
}
