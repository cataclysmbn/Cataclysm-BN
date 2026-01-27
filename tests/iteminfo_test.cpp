#include "catch/catch.hpp"

#include <memory>
#include <string>
#include <vector>

#include "avatar.h"
#include "game.h"
#include "item.h"
#include "iteminfo_query.h"
#include "itype.h"
#include "player_helpers.h"
#include "options_helpers.h"
#include "recipe.h"
#include "state_helpers.h"
#include "type_id.h"
#include "value_ptr.h"

namespace
{

/// also replaces spaces with ` to prevent catch2's mandatory line-wraps
/// from rendering the diff unusable
auto escape_newlines( const std::string &input ) -> std::string
{
    std::string output;
    std::size_t pos = 0;
    while( pos != std::string::npos ) {
        std::size_t newlinePos = input.find( '\n', pos );
        if( newlinePos != std::string::npos ) {
            output += input.substr( pos, newlinePos - pos ) + "\\n\n";
            pos = newlinePos + 1;
        } else {
            output += input.substr( pos );
            pos = newlinePos;
        }
    }
    std::replace( output.begin(), output.end(), ' ', '`' );
    return output;
}

void test_info_equals( const item &i, const iteminfo_query &q,
                       const std::string &reference,
                       temperature_flag temperature = temperature_flag::TEMP_NORMAL )
{
    g->u.clear_mutations();
    std::string info = i.info_string( q, 1, temperature );
    CAPTURE( escape_newlines( info ) );
    CAPTURE( escape_newlines( reference ) );
    CHECK( info == reference );
}

void test_info_equals( std::string item_name, const iteminfo_query &q,
                       const std::string &reference,
                       temperature_flag temperature = temperature_flag::TEMP_NORMAL )
{
    item *it = item::spawn_temporary( item_name );
    test_info_equals( *it, q, reference, temperature );
}

void test_info_contains( const item &i, const iteminfo_query &q,
                         const std::string &reference )
{
    g->u.clear_mutations();
    std::string info = i.info_string( q, 1 );
    using Catch::Matchers::Contains;
    REQUIRE_THAT( info, Contains( reference ) );
}

/*
 * Wrap the iteminfo_query() constructor to avoid MacOS clang compiler errors like this:
 *
 * iteminfo_test.cpp:NN: error: call to constructor of 'iteminfo_query' is ambiguous
 *     iteminfo_query q( { iteminfo_parts::BASE_RIGIDITY, iteminfo_parts::ARMOR_ENCUMBRANCE } );
 *                    ^  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  ../src/iteminfo_query.h:245:9: note: candidate constructor
 *     iteminfo_query( const std::string &bits );
 * ../src/iteminfo_query.h:246:9: note: candidate constructor
 *     iteminfo_query( const std::vector<iteminfo_parts> &setBits );
 *
 * Using this wrapper should force it to use the vector constructor.
 */
auto q_vec( const std::vector<iteminfo_parts> &part_flags ) -> iteminfo_query
{
    return iteminfo_query( part_flags );
}

} // namespace

TEST_CASE( "item description and physical attributes", "[item][iteminfo][primary]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::BASE_CATEGORY, iteminfo_parts::BASE_MATERIAL,
                                iteminfo_parts::BASE_VOLUME, iteminfo_parts::BASE_WEIGHT,
                                iteminfo_parts::DESCRIPTION
                              } );

    override_option opt( "USE_METRIC_WEIGHTS", "lbs" );
    SECTION( "volume, weight, category, material, description" ) {
        test_info_equals(
            "test_jug_plastic", q,
            "Material: <color_c_light_blue>Plastic</color>\n"
            "Volume: <color_c_yellow>3.750</color> L  Weight: <color_c_yellow>0.42</color> lbs\n"
            "Category: <color_c_magenta>CONTAINERS</color>\n"
            "--\n"
            "A standard plastic jug used for milk and household cleaning chemicals.\n" );
    }
}

TEST_CASE( "item owner, price, and barter value", "[item][iteminfo][price]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( std::vector<iteminfo_parts>( { iteminfo_parts::BASE_PRICE, iteminfo_parts::BASE_BARTER } ) );

    SECTION( "owner and price" ) {
        detached_ptr<item> my_rock = item::spawn( "test_rock" );
        my_rock->set_owner( g->u );
        test_info_equals(
            *my_rock, q,
            "Owner: Your Followers\n"
            "--\n"
            "Price: $<color_c_yellow>0.00</color>" );
    }

    SECTION( "owner, price and barter value" ) {
        detached_ptr<item> my_pipe = item::spawn( "test_pipe" );
        my_pipe->set_owner( g->u );
        test_info_equals(
            *my_pipe, q,
            "Owner: Your Followers\n"
            "--\n"
            "Price: $<color_c_yellow>75.00</color>  Barter value: $<color_c_yellow>3.00</color>\n" );
    }

    SECTION( "zero price item with no owner" ) {
        test_info_equals(
            "test_rock", q,
            "--\n"
            "Price: $<color_c_yellow>0.00</color>" );
    }
}

TEST_CASE( "item rigidity", "[item][iteminfo][rigidity]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::BASE_RIGIDITY, iteminfo_parts::ARMOR_ENCUMBRANCE } );

    SECTION( "non-rigid items indicate their flexible volume/encumbrance" ) {
        test_info_equals(
            "test_waterskin", q,
            R"(--
<color_c_white>Coverage</color>: <color_c_yellow>5</color>% <color_c_cyan>(for all parts)</color>
<color_c_white>Encumbrance</color>: <color_c_yellow>0-3</color> (When Full) <color_c_cyan>(for all parts)</color>
--
* This item is <color_c_cyan>not rigid</color>.  Its volume and encumbrance increase with contents.
)" );

        test_info_equals(
            "test_backpack", q,
            R"(--
<color_c_white>Coverage</color>: <color_c_yellow>30</color>% <color_c_cyan>(for all parts)</color>
<color_c_white>Encumbrance</color>: <color_c_yellow>2-15</color> (When Full) <color_c_cyan>(for all parts)</color>
--
* This item is <color_c_cyan>not rigid</color>.  Its volume and encumbrance increase with contents.
)" );
    }

    SECTION( "rigid items do not indicate they are rigid, since almost all items are" ) {
        test_info_equals(
            "test_briefcase", q,
            R"(--
<color_c_white>Coverage</color>: <color_c_yellow>10</color>% <color_c_cyan>(for all parts)</color>
<color_c_white>Encumbrance</color>: <color_c_yellow>30</color> <color_c_cyan>(for all parts)</color>
)" );


        test_info_equals( "test_jug_plastic", q, "" );
        test_info_equals( "test_pipe", q, "" );
        test_info_equals( "test_pine_nuts", q, "" );
    }
}

TEST_CASE( "weapon attack ratings and moves", "[item][iteminfo][weapon]" )
{
    clear_all_state();
    // new DPS calculations depend on the avatar's stats, so make sure they're consistent
    REQUIRE( g->u.get_str() == 8 );
    REQUIRE( g->u.get_dex() == 8 );
    iteminfo_query q = q_vec( { iteminfo_parts::BASE_DAMAGE, iteminfo_parts::BASE_TOHIT,
                                iteminfo_parts::BASE_MOVES
                              } );

    SECTION( "bash damage" ) {
        test_info_equals(
            "test_rock", q,
            "--\n"
            "<color_c_white>Melee damage</color>: Bash: <color_c_yellow>7</color>"
            "  To-hit bonus: <color_c_yellow>-2</color>\n"
            "Moves per attack: <color_c_yellow>79</color>\n"
            "Stamina Cost: <color_c_yellow>91</color>\n"
            "Typical damage per second:\n"
            "Best: <color_c_yellow>5.20</color>"
            "  Vs. Agile: <color_c_yellow>1.62</color>"
            "  Vs. Armored: <color_c_yellow>0.00</color>\n" );
    }

    SECTION( "bash and cut damage" ) {
        test_info_equals(
            "test_halligan", q,
            "--\n"
            "<color_c_white>Melee damage</color>: Bash: <color_c_yellow>20</color>"
            "  Cut: <color_c_yellow>5</color>"
            "  To-hit bonus: <color_c_yellow>+2</color>\n"
            "Moves per attack: <color_c_yellow>145</color>\n"
            "Stamina Cost: <color_c_yellow>275</color>\n"
            "Typical damage per second:\n"
            "Best: <color_c_yellow>10.94</color>"
            "  Vs. Agile: <color_c_yellow>5.60</color>"
            "  Vs. Armored: <color_c_yellow>0.78</color>\n" );
    }

    SECTION( "bash and pierce damage" ) {
        test_info_equals(
            "pointy_stick", q,
            "--\n"
            "<color_c_white>Melee damage</color>: Bash: <color_c_yellow>5</color>"
            "  Pierce: <color_c_yellow>11</color>"
            "  To-hit bonus: <color_c_yellow>+0</color>\n"
            "Moves per attack: <color_c_yellow>100</color>\n"
            "Stamina Cost: <color_c_yellow>106</color>\n"
            "Typical damage per second:\n"
            "Best: <color_c_yellow>9.61</color>"
            "  Vs. Agile: <color_c_yellow>3.95</color>"
            "  Vs. Armored: <color_c_yellow>0.00</color>\n"
        );
    }

    SECTION( "melee and ranged damaged" ) {
        test_info_equals(
            "arrow_wood", q,
            "--\n"
            "<color_c_white>Melee damage</color>: Bash: <color_c_yellow>2</color>"
            "  Cut: <color_c_yellow>1</color>"
            "  To-hit bonus: <color_c_yellow>+0</color>\n"
            "Moves per attack: <color_c_yellow>65</color>\n"
            "Stamina Cost: <color_c_yellow>68</color>\n"
            "Typical damage per second:\n"
            "Best: <color_c_yellow>5.25</color>"
            "  Vs. Agile: <color_c_yellow>2.09</color>"
            "  Vs. Armored: <color_c_yellow>0.00</color>\n" );
    }

    SECTION( "no damage" ) {
        test_info_equals( "test_rag", q, "" );
    }
}

TEST_CASE( "techniques when wielded", "[item][iteminfo][weapon]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_TECHNIQUES } );

    test_info_equals(
        "test_halligan", q,
        "--\n"
        "<color_c_white>Techniques when wielded</color>:"
        " <color_c_light_blue>Brutal Strike</color>: <color_c_cyan>Stun 1 turn, knockback 1 tile, crit only</color>,"
        " <color_c_light_blue>Sweep Attack</color>: <color_c_cyan>Down 2 turns</color>, and"
        " <color_c_light_blue>Block</color>: <color_c_cyan>Medium blocking ability</color>\n" );
}

TEST_CASE( "armor coverage and protection values", "[item][iteminfo][armor]" )
{
    SECTION( "shows coverage, encumbrance, and protection for armor with coverage" ) {

        item &longshirt = *item::spawn_temporary( "test_longshirt" );
        REQUIRE( longshirt.get_covered_body_parts().any() );
        REQUIRE( longshirt.get_coverage( bodypart_id( "torso" ) ) == 90 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "arm_l" ) ) == 90 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "arm_r" ) ) == 90 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "leg_l" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "leg_r" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "head" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( longshirt.get_coverage( bodypart_id( "foot_r" ) ) == 0 );

        test_info_equals( longshirt, q_vec( { iteminfo_parts::ARMOR_BODYPARTS } ),
                          "--\n"
                          "<color_c_white>Covers</color>:"
                          " The <color_c_cyan>torso</color>."
                          " The <color_c_cyan>arms</color>. \n" ); // NOLINT(cata-text-style)
        test_info_equals( longshirt, q_vec( { iteminfo_parts::ARMOR_LAYER } ),
                          "--\n"
                          "Layer: <color_c_light_blue>Normal</color>. \n" ); // NOLINT(cata-text-style)

        // Warmth display
        REQUIRE( longshirt.get_warmth() == 5 );
        test_info_equals( longshirt, q_vec( { iteminfo_parts::ARMOR_COVERAGE, iteminfo_parts::ARMOR_WARMTH } ),
                          "--\nWarmth: <color_c_yellow>5</color>\n" );

        REQUIRE( longshirt.get_avg_encumber( get_player_character() ) == 3 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "torso" ) ) == 3 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "arm_l" ) ) == 3 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "arm_r" ) ) == 3 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "leg_l" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "leg_r" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "head" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( longshirt.get_encumber( get_player_character(), bodypart_id( "foot_r" ) ) == 0 );

        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "torso" ) ) == 3 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "arm_l" ) ) == 3 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "arm_r" ) ) == 3 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "leg_l" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "leg_r" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "head" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( longshirt.get_encumber_when_containing( get_player_character(), longshirt.get_storage(),
                 bodypart_id( "foot_r" ) ) == 0 );


        test_info_equals( longshirt, q_vec( { iteminfo_parts::ARMOR_ENCUMBRANCE } ),
                            ""
R"(--
<color_c_white>Coverage</color>: <color_c_yellow>90</color>% <color_c_cyan>(for all parts)</color>
<color_c_white>Encumbrance</color>: <color_c_yellow>3</color> <color_c_cyan>(for all parts)</color>
)");

        item &swat_armor = *item::spawn_temporary( "test_swat_armor" );
        REQUIRE( swat_armor.get_covered_body_parts().any() );

        test_info_equals( swat_armor, q_vec( { iteminfo_parts::ARMOR_BODYPARTS } ),
                          "--\n"
                          "<color_c_white>Covers</color>:"
                          " The <color_c_cyan>torso</color>."
                          " The <color_c_cyan>arms</color>."
                          " The <color_c_cyan>legs</color>. \n" ); // NOLINT(cata-text-style)

        test_info_equals( swat_armor, q_vec( { iteminfo_parts::ARMOR_LAYER } ),
                          "--\n"
                          "Layer: <color_c_light_blue>Normal</color>. \n" ); // NOLINT(cata-text-style)

        REQUIRE( swat_armor.get_avg_coverage() == 95 );
        REQUIRE( swat_armor.get_warmth() == 35 );
        test_info_equals( swat_armor, q_vec( { iteminfo_parts::ARMOR_COVERAGE, iteminfo_parts::ARMOR_WARMTH } ),
                          "--\nWarmth: <color_c_yellow>35</color>\n" );

        REQUIRE( swat_armor.get_coverage( bodypart_id( "torso" ) ) == 95 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "leg_l" ) ) == 95 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "leg_r" ) ) == 95 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "arm_l" ) ) == 95 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "arm_r" ) ) == 95 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "head" ) ) == 0 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "foot_r" ) ) == 0 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "eyes" ) ) == 0 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( swat_armor.get_coverage( bodypart_id( "hand_l" ) ) == 0 );

        REQUIRE( swat_armor.get_avg_encumber( get_player_character() ) == 12 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "torso" ) ) == 12 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "leg_l" ) ) == 12 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "leg_r" ) ) == 12 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "arm_l" ) ) == 12 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "arm_r" ) ) == 12 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "head" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "foot_r" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "eyes" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber( get_player_character(), bodypart_id( "hand_r" ) ) == 0 );

        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "torso" ) ) == 25 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "arm_l" ) ) == 25 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "arm_r" ) ) == 25 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "leg_l" ) ) == 25 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "leg_r" ) ) == 25 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "head" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( swat_armor.get_encumber_when_containing( get_player_character(), swat_armor.get_storage(),
                 bodypart_id( "foot_r" ) ) == 0 );

        test_info_equals( swat_armor, q_vec( { iteminfo_parts::ARMOR_ENCUMBRANCE } ),
                          R"(--
<color_c_white>Coverage</color>: <color_c_yellow>95</color>% <color_c_cyan>(for all parts)</color>
<color_c_white>Encumbrance</color>: <color_c_yellow>12-25</color> (When Full) <color_c_cyan>(for all parts)</color>
)");

        // Test copy-from
        item &faux_fur_pants = *item::spawn_temporary( "test_pants_faux_fur" );
        REQUIRE( faux_fur_pants.get_covered_body_parts().any() );

        test_info_equals( faux_fur_pants, q_vec( { iteminfo_parts::ARMOR_BODYPARTS } ),
                          "--\n"
                          "<color_c_white>Covers</color>:"
                          " The <color_c_cyan>legs</color>. \n" ); // NOLINT(cata-text-style)

        test_info_equals( faux_fur_pants, q_vec( { iteminfo_parts::ARMOR_LAYER } ),
                          "--\n"
                          "Layer: <color_c_light_blue>Normal</color>. \n" ); // NOLINT(cata-text-style)

        REQUIRE( faux_fur_pants.get_avg_coverage() == 95 );
        REQUIRE( faux_fur_pants.get_warmth() == 70 );
        test_info_equals( faux_fur_pants, q_vec( { iteminfo_parts::ARMOR_COVERAGE, iteminfo_parts::ARMOR_WARMTH } ),
                          "--\nWarmth: <color_c_yellow>70</color>\n" );

        REQUIRE( faux_fur_pants.get_avg_coverage() == 95 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "leg_l" ) ) == 95 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "leg_r" ) ) == 95 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "arm_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "arm_r" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "foot_r" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "head" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "eyes" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_coverage( bodypart_id( "hand_r" ) ) == 0 );

        REQUIRE( faux_fur_pants.get_avg_encumber( get_player_character() ) == 16 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "leg_l" ) ) == 16 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "leg_r" ) ) == 16 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "arm_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "arm_r" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "foot_r" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "head" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "eyes" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber( get_player_character(), bodypart_id( "hand_r" ) ) == 0 );

        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "torso" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "arm_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "arm_r" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "leg_l" ) ) == 20 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "leg_r" ) ) == 20 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "head" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( faux_fur_pants.get_encumber_when_containing( get_player_character(),
                 faux_fur_pants.get_storage(),
                 bodypart_id( "foot_r" ) ) == 0 );

        item &faux_fur_suit = *item::spawn_temporary( "test_portion_faux_fur_pants_suit" );
        REQUIRE( faux_fur_suit.get_covered_body_parts().any() );

        test_info_equals( faux_fur_suit, q_vec( { iteminfo_parts::ARMOR_BODYPARTS } ),
                          "--\n"
                          "<color_c_white>Covers</color>:"
                          " The <color_c_cyan>head</color>."
                          " The <color_c_cyan>torso</color>."
                          " The <color_c_cyan>arms</color>."
                          " The <color_c_cyan>legs</color>. \n" ); // NOLINT(cata-text-style)

        test_info_equals( faux_fur_suit, q_vec( { iteminfo_parts::ARMOR_LAYER } ),
                          "--\n"
                          "Layer: <color_c_light_blue>Normal</color>. \n" ); // NOLINT(cata-text-style)

        REQUIRE( faux_fur_suit.get_avg_coverage() == 75 );
        REQUIRE( faux_fur_suit.get_warmth() == 5 );
        test_info_equals( faux_fur_suit, q_vec( { iteminfo_parts::ARMOR_COVERAGE, iteminfo_parts::ARMOR_WARMTH } ),
                          "--\nWarmth: <color_c_yellow>5</color>\n" );

        REQUIRE( faux_fur_suit.get_avg_coverage() == 75 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "torso" ) ) == 100 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "leg_l" ) ) == 50 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "leg_r" ) ) == 100 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "arm_l" ) ) == 50 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "arm_r" ) ) == 100 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "head" ) ) == 50 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "eyes" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_coverage( bodypart_id( "foot_r" ) ) == 0 );

        REQUIRE( faux_fur_suit.get_avg_encumber( get_player_character() ) == 7 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "torso" ) ) == 10 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "leg_l" ) ) == 5 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "leg_r" ) ) == 10 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "arm_l" ) ) == 5 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "arm_r" ) ) == 10 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "head" ) ) == 5 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "eyes" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "foot_r" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber( get_player_character(), bodypart_id( "hand_r" ) ) == 0 );

        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "torso" ) ) == 25 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "arm_l" ) ) == 5 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "arm_r" ) ) == 25 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "leg_l" ) ) == 5 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "leg_r" ) ) == 25 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "hand_l" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "hand_r" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "head" ) ) == 5 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "mouth" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "foot_l" ) ) == 0 );
        REQUIRE( faux_fur_suit.get_encumber_when_containing( get_player_character(),
                 faux_fur_suit.get_storage(),
                 bodypart_id( "foot_r" ) ) == 0 );

        test_info_equals( faux_fur_suit, q_vec( { iteminfo_parts::ARMOR_ENCUMBRANCE } ),
                          R"(--
<color_c_white>Coverage</color>:
  L. Arm, Head, L. Leg : <color_c_yellow>50</color>%
  R. Arm, R. Leg, Torso: <color_c_yellow>100</color>%
<color_c_white>Encumbrance</color>: <color_c_red>(poor fit)</color>
  L. Arm, Head, L. Leg : <color_c_yellow>5</color>
  R. Arm, R. Leg, Torso: <color_c_yellow>10-25</color> (When Full)
)" );
    }

    iteminfo_query q = q_vec( { iteminfo_parts::ARMOR_BODYPARTS, iteminfo_parts::ARMOR_LAYER,
                                iteminfo_parts::ARMOR_COVERAGE, iteminfo_parts::ARMOR_WARMTH,
                                iteminfo_parts::ARMOR_ENCUMBRANCE, iteminfo_parts::ARMOR_PROTECTION
                              } );

    SECTION( "shows coverage, encumbrance, and protection for armor with coverage" ) {
        test_info_equals(
            "test_longshirt", q,
              R"(--
<color_c_white>Covers</color>: The <color_c_cyan>torso</color>. The <color_c_cyan>arms</color>. )"
            // NOLINTNEXTLINE(cata-text-style)
            R"(
Layer: <color_c_light_blue>Normal</color>. )"
            // NOLINTNEXTLINE(cata-text-style)
            R"(
Warmth: <color_c_yellow>5</color>
--
<color_c_white>Coverage</color>: <color_c_yellow>90</color>% <color_c_cyan>(for all parts)</color>
<color_c_white>Encumbrance</color>: <color_c_yellow>3</color> <color_c_cyan>(for all parts)</color>
<color_c_white>Protection</color>: Bash: <color_c_yellow>1</color>  Cut: <color_c_yellow>1</color>  Ballistic: <color_c_yellow>1</color>
  Acid: <color_c_yellow>0</color>  Fire: <color_c_yellow>0</color>  Environmental: <color_c_yellow>0</color>
)" );
    }

    SECTION( "omits irrelevant info if it covers nothing" ) {
        test_info_equals(
            "test_ear_plugs", q,
            "--\n"
            "<color_c_white>Covers</color>: <color_c_cyan>Nothing</color>.\n" );
    }
}

TEST_CASE( "ranged weapon attributes", "[item][iteminfo][weapon][ranged][gun]" )
{
    clear_all_state();
    SECTION( "skill used" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_USEDSKILL } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "Skill used: <color_c_cyan>Archery</color>\n" );
    }

    SECTION( "ammo capacity of weapon" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_CAPACITY } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "Capacity: <color_c_yellow>1</color> round of arrows\n" );
    }

    SECTION( "default ammo when weapon is unloaded" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_DEFAULT_AMMO } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "Weapon is <color_c_red>not loaded</color>, so stats below assume the default ammo:"
            " <color_c_light_blue>wooden broadhead arrow</color>\n" );
    }

    SECTION( "weapon damage including floating-point multiplier" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_DAMAGE, iteminfo_parts::GUN_DAMAGE_LOADEDAMMO,
                                    iteminfo_parts::GUN_DAMAGE_TOTAL, iteminfo_parts::GUN_DAMAGEMULT,
                                    iteminfo_parts::GUN_DAMAGEMULT_AMMO, iteminfo_parts::GUN_DAMAGEMULT_TOTAL
                                  } );
        detached_ptr<item> crossbow = item::spawn( "test_compbow" );
        crossbow->ammo_set( itype_id( "test_arrow" ) );
        test_info_equals(
            *crossbow, q,
            "--\n"
            "<color_c_white>Ranged damage</color>: <color_c_yellow>18</color>"
            "<color_c_yellow>+0</color> = <color_c_yellow>18</color>\n"
            "Damage multiplier: <color_c_yellow>1.00</color>*<color_c_yellow>0.75</color> "
            "= <color_c_yellow>0.75</color>\n" );
    }

    SECTION( "time to reload weapon" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_RELOAD_TIME } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "Reload time: <color_c_yellow>110</color> moves \n" ); // NOLINT(cata-text-style)
    }

    SECTION( "weapon firing modes" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_FIRE_MODES } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "<color_c_white>Fire modes</color>: manual (1)\n" );
    }

    SECTION( "weapon mods" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_GUN_MODS } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "<color_c_white>Mods</color>: <color_c_white>0/2</color> accessories;"
            " <color_c_white>0/1</color> dampening; <color_c_white>0/1</color> sights;"
            " <color_c_white>0/1</color> stabilizer; <color_c_white>0/1</color> underbarrel.\n" );
    }

    SECTION( "weapon dispersion" ) {
        iteminfo_query q = q_vec( { iteminfo_parts::GUN_DISPERSION } );
        test_info_equals(
            "test_compbow", q,
            "--\n"
            "Dispersion: <color_c_yellow>850</color>\n" );
    }
}

TEST_CASE( "ammunition", "[item][iteminfo][ammo]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::AMMO_REMAINING_OR_TYPES, iteminfo_parts::AMMO_DAMAGE_VALUE,
                                iteminfo_parts::AMMO_DAMAGE_PROPORTIONAL, iteminfo_parts::AMMO_DAMAGE_AP,
                                iteminfo_parts::AMMO_DAMAGE_RANGE, iteminfo_parts::AMMO_DAMAGE_DISPERSION,
                                iteminfo_parts::AMMO_DAMAGE_RECOIL
                              } );

    SECTION( "simple item with ammo damage" ) {
        test_info_equals(
            "test_rock", q,
            "--\n"
            "<color_c_white>Ammunition type</color>: rocks\n"
            "Damage: <color_c_yellow>7</color>\n"
            "Range: <color_c_yellow>10</color>  Dispersion: <color_c_yellow>14</color>\n" );
    }
}

TEST_CASE( "nutrients in food", "[item][iteminfo][food]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::FOOD_NUTRITION, iteminfo_parts::FOOD_VITAMINS,
                                iteminfo_parts::FOOD_QUENCH
                              } );

    SECTION( "nutrient values of regular item" ) {
        test_info_equals(
            "icecream", q,
            "--\n"
            "Nutrition will <color_cyan>vary with available ingredients</color>.\n"
            "<color_c_white>Calories (kcal)</color>: <color_c_yellow>325</color>  "
            "Quench: <color_c_yellow>0</color>\n"
            "Vitamins (RDA): Calcium (9%), Vitamin A (9%), and Vitamin B12 (11%)\n" );
    }
}

TEST_CASE( "food freshness and lifetime", "[item][iteminfo][food]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::FOOD_ROT, iteminfo_parts::FOOD_ROT_STORAGE} );

    // Ensure test character has no skill estimating spoilage
    g->u.clear_skills();
    REQUIRE_FALSE( g->u.can_estimate_rot() );

    SECTION( "food is fresh" ) {
        test_info_equals(
            "test_pine_nuts", q,
            "--\n"
            "* This food is <color_c_yellow>perishable</color>, and at room temperature has"
            " an estimated nominal shelf life of <color_c_cyan>3 seasons</color>.\n"
            "* Current storage conditions <color_c_red>do not</color> protect this item"
            " from rot.\n"
            "* This food looks as <color_c_green>fresh</color> as it can be.\n" );
    }

    SECTION( "food is old" ) {
        detached_ptr<item> nuts = item::spawn( "test_pine_nuts" );
        nuts->mod_rot( nuts->type->comestible->spoils );
        test_info_equals(
            *nuts, q,
            "--\n"
            "* This food is <color_c_yellow>perishable</color>, and at room temperature has"
            " an estimated nominal shelf life of <color_c_cyan>3 seasons</color>.\n"
            "* Current storage conditions <color_c_red>do not</color> protect this item"
            " from rot.\n"
            "* This food looks <color_c_red>old</color>.  It's on the brink of becoming inedible.\n"
        );
    }

    SECTION( "food is stored in a fridge" ) {
        test_info_equals(
            "test_pine_nuts", q,
            "--\n"
            "* This food is <color_c_yellow>perishable</color>, and at room temperature"
            " has an estimated nominal shelf life of <color_c_cyan>3 seasons</color>.\n"
            "* Current storage conditions <color_c_yellow>partially</color> protect this"
            " item from rot.  It will stay fresh at least <color_c_cyan>3 years</color>.\n"
            "* This food looks as <color_c_green>fresh</color> as it can be.\n",
            temperature_flag::TEMP_FRIDGE
        );
    }

    SECTION( "liquid food is stored in a container in a fridge" ) {
        detached_ptr<item> food_item = item::in_container( itype_id( "can_medium_unsealed" ),
                                       item::spawn( itype_id( "milk" ) ) );
        test_info_equals(
            *food_item, q,
            "--\n"
            "* This food is <color_c_yellow>perishable</color>, and at room temperature"
            " has an estimated nominal shelf life of <color_c_cyan>1 day</color>.\n"
            "* Current storage conditions <color_c_yellow>partially</color> protect this"
            " item from rot.  It will stay fresh at least <color_c_cyan>4 days</color>.\n"
            "* This food looks as <color_c_green>fresh</color> as it can be.\n",
            temperature_flag::TEMP_FRIDGE
        );
    }
}

TEST_CASE( "item conductivity", "[item][iteminfo][conductivity]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_CONDUCTIVITY } );

    SECTION( "non-conductive items" ) {
        test_info_equals(
            "test_2x4", q,
            "--\n"
            "* This item <color_c_green>does not conduct</color> electricity.\n" );
        test_info_equals(
            "test_fire_ax", q,
            "--\n"
            "* This item <color_c_green>does not conduct</color> electricity.\n" );
    }

    SECTION( "conductive items" ) {
        test_info_equals(
            "test_pipe", q,
            "--\n"
            "* This item <color_c_red>conducts</color> electricity.\n" );
        test_info_equals(
            "test_halligan", q,
            "--\n"
            "* This item <color_c_red>conducts</color> electricity.\n" );
    }
}

TEST_CASE( "list of item qualities", "[item][iteminfo][quality]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::QUALITIES } );

    SECTION( "Halligan bar" ) {
        test_info_equals(
            "test_halligan", q,
            "--\n"
            "Has level <color_c_cyan>1 digging</color> quality.\n"
            "Has level <color_c_cyan>2 hammering</color> quality.\n"
            "Has level <color_c_cyan>4 prying</color> quality.\n" );
    }

    SECTION( "bottle jack" ) {
        override_option opt( "USE_METRIC_WEIGHTS", "lbs" );
        test_info_equals(
            "test_jack_small", q,
            "--\n"
            "Has level <color_c_cyan>4 jacking</color> quality and is rated at <color_c_cyan>4409</color> lbs\n" );
    }

    SECTION( "sonic screwdriver" ) {
        test_info_equals(
            "test_sonic_screwdriver", q,
            "--\n"
            "Has level <color_c_cyan>30 lockpicking</color> quality.\n"
            "Has level <color_c_cyan>2 prying</color> quality.\n"
            "Has level <color_c_cyan>2 screw driving</color> quality.\n"
            "Has level <color_c_cyan>1 fine screw driving</color> quality.\n"
            "Has level <color_c_cyan>1 bolt turning</color> quality.\n" );
    }
}

TEST_CASE( "repairable and with what tools", "[item][iteminfo][repair]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_REPAIREDWITH } );

    test_info_contains(
        *item::spawn( "test_halligan" ), q,
        "<color_c_white>Repair</color> using advanced 3D printer, charcoal forge, Advanced Grid 3D Printer, grid forge, grid welder, electric forge, integrated toolset, arc welder, or makeshift arc welder.\n" );

    test_info_contains(
        *item::spawn( "test_hazmat_suit" ), q,
        "<color_c_white>Repair</color> using advanced 3D printer, 3D printer, Grid 3D Printer, Advanced Grid 3D Printer, grid soldering iron, alcohol-fueled soldering iron, electric soldering iron, TEST soldering iron, or integrated toolset.\n" );

    test_info_contains(
        *item::spawn( "test_rock" ), q, "* This item is <color_c_red>not repairable</color>.\n" );

    test_info_contains(
        *item::spawn( "test_socks" ), q,
        "* This item can be <color_c_green>reinforced</color>.\n" );
}

TEST_CASE( "disassembly time and yield", "[item][iteminfo][disassembly]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_COMPONENTS_DISASSEMBLE } );

    test_info_equals(
        "test_soldering_iron", q,
        "--\n"
        "<color_c_white>Disassembly</color> takes about 20 minutes, requires 1 tool"
        " with <color_c_cyan>cutting of 1</color> or more and 1 tool with"
        " <color_c_cyan>screw driving of 1</color> or more and <color_c_white>might"
        " yield</color>: electronic scrap (2), copper (1), scrap metal (1), and copper"
        " wire (5).\n" );

    test_info_equals(
        "test_sheet_metal", q,
        "--\n"
        "<color_c_white>Disassembly</color> takes about 2 minutes, requires 1 tool"
        " with <color_c_cyan>metal sawing of 2</color> or more and <color_c_white>might"
        " yield</color>: TEST small metal sheet (24).\n" );
}

TEST_CASE( "item description flags", "[item][iteminfo]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_FLAGS } );

    test_info_equals(
        "test_halligan", q,
        "--\n"
        "* This item can be clipped on to a <color_c_cyan>belt loop</color> of the appropriate size.\n"
        "* As a weapon, this item is <color_c_green>well-made</color> and will"
        " <color_c_cyan>withstand the punishment of combat</color>.\n" );

    test_info_equals(
        "test_hazmat_suit", q,
        "--\n"
        "* This gear <color_c_green>completely protects</color> you from"
        " <color_c_cyan>electric discharges</color>.\n"
        "* This gear <color_c_green>completely protects</color> you from"
        " <color_c_cyan>any gas</color>.\n"
        "* This gear is generally <color_c_cyan>worn over</color> clothing.\n"
        "* This clothing <color_c_green>completely protects</color> you from"
        " <color_c_cyan>radiation</color>.\n"
        "* This clothing is designed to keep you <color_c_cyan>dry</color> in the rain.\n"
        "* This clothing <color_c_cyan>won't let water through</color>."
        "  Unless you jump in the river or something like that.\n" );
}

TEST_CASE( "show available recipes with item as an ingredient", "[item][iteminfo][recipes]" )
{
    clear_all_state();
    iteminfo_query q = q_vec( { iteminfo_parts::DESCRIPTION_APPLICABLE_RECIPES } );
    const recipe *purtab = &recipe_id( "pur_tablets" ).obj();
    g->u.clear_mutations();

    GIVEN( "character has a potassium iodide tablet and no skill" ) {
        detached_ptr<item> det = item::spawn( "iodine" );
        item &iodine = *det;
        g->u.i_add( std::move( det ) );
        g->u.clear_skills();

        THEN( "nothing is craftable from it" ) {
            test_info_equals(
                iodine, q,
                "--\nYou know of nothing you could craft with it.\n" );
        }

        WHEN( "they acquire the needed skills" ) {
            g->u.set_skill_level( purtab->skill_used, purtab->difficulty );
            REQUIRE( g->u.get_skill_level( purtab->skill_used ) == purtab->difficulty );

            THEN( "still nothing is craftable from it" ) {
                test_info_equals(
                    iodine, q,
                    "--\nYou know of nothing you could craft with it.\n" );
            }

            WHEN( "they have no book, but have the recipe memorized" ) {
                g->u.learn_recipe( purtab );
                REQUIRE( g->u.knows_recipe( purtab ) );

                THEN( "they can use potassium iodide tablets to craft it" ) {
                    test_info_equals(
                        iodine, q,
                        "--\n"
                        "You could use it to craft: "
                        "<color_c_dark_gray>water purification tablet</color>\n" );
                }
            }

            WHEN( "they have the recipe in a book, but not memorized" ) {
                g->u.i_add( item::spawn( "textbook_chemistry" ) );

                THEN( "they can use potassium iodide tablets to craft it" ) {
                    test_info_equals(
                        iodine, q,
                        "--\n"
                        "You could use it to craft: "
                        "<color_c_dark_gray>water purification tablet</color>\n" );
                }
            }
        }
    }
}
