#include "catch/catch.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "avatar_functions.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character_functions.h"
#include "coordinate_conversions.h"
#include "craft_command.h"
#include "crafting.h"
#include "distribution_grid.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "npc.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "point.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "state_helpers.h"
#include "string_id.h"
#include "type_id.h"
#include "value_ptr.h"

class inventory;

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
static const trait_id trait_DEBUG_STORAGE( "DEBUG_STORAGE" );

TEST_CASE( "recipe_subset" )
{
    clear_all_state();
    recipe_subset subset;

    REQUIRE( subset.size() == 0 );
    GIVEN( "a recipe of rum" ) {
        const recipe *r = &recipe_id( "brew_rum" ).obj();

        WHEN( "the recipe is included" ) {
            subset.include( r );

            THEN( "it's in the subset" ) {
                CHECK( subset.size() == 1 );
                CHECK( subset.contains( *r ) );
            }
            THEN( "it has its default difficulty" ) {
                CHECK( subset.get_custom_difficulty( r ) == r->difficulty );
            }
            THEN( "it's in the right category" ) {
                const auto cat_recipes( subset.in_category( "CC_FOOD" ) );

                CHECK( cat_recipes.size() == 1 );
                CHECK( std::find( cat_recipes.begin(), cat_recipes.end(), r ) != cat_recipes.end() );
            }
            THEN( "it uses water" ) {
                const auto comp_recipes( subset.of_component( itype_id( "water" ) ) );

                CHECK( comp_recipes.size() == 1 );
                CHECK( std::find( comp_recipes.begin(), comp_recipes.end(), r ) != comp_recipes.end() );
            }
            AND_WHEN( "the subset is cleared" ) {
                subset.clear();

                THEN( "it's no longer in the subset" ) {
                    CHECK( subset.size() == 0 );
                    CHECK_FALSE( subset.contains( *r ) );
                }
            }
        }
        WHEN( "the recipe is included with higher difficulty" ) {
            subset.include( r, r->difficulty + 1 );

            THEN( "it's harder to perform" ) {
                CHECK( subset.get_custom_difficulty( r ) == r->difficulty + 1 );
            }
            AND_WHEN( "it's included again with default difficulty" ) {
                subset.include( r );

                THEN( "it recovers its normal difficulty" ) {
                    CHECK( subset.get_custom_difficulty( r ) == r->difficulty );
                }
            }
            AND_WHEN( "it's included again with lower difficulty" ) {
                subset.include( r, r->difficulty - 1 );

                THEN( "it becomes easier to perform" ) {
                    CHECK( subset.get_custom_difficulty( r ) == r->difficulty - 1 );
                }
            }
        }
        WHEN( "the recipe is included with lower difficulty" ) {
            subset.include( r, r->difficulty - 1 );

            THEN( "it's easier to perform" ) {
                CHECK( subset.get_custom_difficulty( r ) == r->difficulty - 1 );
            }
            AND_WHEN( "it's included again with default difficulty" ) {
                subset.include( r );

                THEN( "it's still easier to perform" ) {
                    CHECK( subset.get_custom_difficulty( r ) == r->difficulty - 1 );
                }
            }
            AND_WHEN( "it's included again with higher difficulty" ) {
                subset.include( r, r->difficulty + 1 );

                THEN( "it's still easier to perform" ) {
                    CHECK( subset.get_custom_difficulty( r ) == r->difficulty - 1 );
                }
            }
        }
    }
}

TEST_CASE( "available_recipes", "[recipes]" )
{
    clear_all_state();
    const recipe *r = &recipe_id( "magazine_battery_light_mod" ).obj();
    avatar dummy;

    REQUIRE( dummy.get_skill_level( r->skill_used ) == 0 );
    REQUIRE_FALSE( dummy.knows_recipe( r ) );
    REQUIRE( r->skill_used );

    GIVEN( "a recipe that can be automatically learned" ) {
        WHEN( "the player has lower skill" ) {
            for( const std::pair<const skill_id, int> &skl : r->required_skills ) {
                dummy.set_skill_level( skl.first, skl.second - 1 );
            }

            THEN( "he can't craft it" ) {
                CHECK_FALSE( dummy.knows_recipe( r ) );
            }
        }
        WHEN( "the player has just the skill that's required" ) {
            dummy.set_skill_level( r->skill_used, r->difficulty );
            for( const std::pair<const skill_id, int> &skl : r->required_skills ) {
                dummy.set_skill_level( skl.first, skl.second );
            }

            THEN( "he can craft it now!" ) {
                CHECK( dummy.knows_recipe( r ) );

                AND_WHEN( "his skill rusts" ) {
                    dummy.set_skill_level( r->skill_used, 0 );
                    for( const std::pair<const skill_id, int> &skl : r->required_skills ) {
                        dummy.set_skill_level( skl.first, 0 );
                    }

                    THEN( "he still remembers how to craft it" ) {
                        CHECK( dummy.knows_recipe( r ) );
                    }
                }
            }
        }
    }

    GIVEN( "an appropriate book" ) {
        detached_ptr<item> det = item::spawn( "manual_electronics" );
        item &craftbook = *det;
        dummy.i_add( std::move( det ) );

        REQUIRE( craftbook.is_book() );
        REQUIRE_FALSE( craftbook.type->book->recipes.empty() );
        REQUIRE_FALSE( dummy.knows_recipe( r ) );

        WHEN( "the player read it and has an appropriate skill" ) {
            dummy.do_read( &craftbook );
            dummy.set_skill_level( r->skill_used, 2 );
            // Secondary skills are just set to be what the autolearn requires
            // but the primary is not
            for( const std::pair<const skill_id, int> &skl : r->required_skills ) {
                dummy.set_skill_level( skl.first, skl.second );
            }

            AND_WHEN( "he searches for the recipe in the book" ) {
                THEN( "he finds it!" ) {
                    CHECK( dummy.get_recipes_from_books( dummy.crafting_inventory() ).contains( *r ) );
                }
                THEN( "it's easier in the book" ) {
                    CHECK( dummy.get_recipes_from_books( dummy.crafting_inventory() ).get_custom_difficulty( r ) == 2 );
                }
                THEN( "he still hasn't the recipe memorized" ) {
                    CHECK_FALSE( dummy.knows_recipe( r ) );
                }
            }
            AND_WHEN( "he gets rid of the book" ) {
                craftbook.detach( );

                THEN( "he can't brew the recipe anymore" ) {
                    CHECK_FALSE( dummy.get_recipes_from_books( dummy.crafting_inventory() ).contains( *r ) );
                }
            }
        }
    }

    GIVEN( "an eink pc with a sushi recipe" ) {
        const recipe *r2 = &recipe_id( "sushi_rice" ).obj();
        detached_ptr<item> det = item::spawn( "eink_tablet_pc" );
        item &eink = *det;
        dummy.i_add( std::move( det ) );
        eink.set_var( "EIPC_RECIPES", ",sushi_rice," );
        REQUIRE_FALSE( dummy.knows_recipe( r2 ) );

        WHEN( "the player holds it and has an appropriate skill" ) {
            dummy.set_skill_level( r2->skill_used, 2 );

            AND_WHEN( "he searches for the recipe in the tablet" ) {
                THEN( "he finds it!" ) {
                    CHECK( dummy.get_recipes_from_books( dummy.crafting_inventory() ).contains( *r2 ) );
                }
                THEN( "he still hasn't the recipe memorized" ) {
                    CHECK_FALSE( dummy.knows_recipe( r2 ) );
                }
            }
            AND_WHEN( "he gets rid of the tablet" ) {
                eink.detach( );

                THEN( "he can't make the recipe anymore" ) {
                    CHECK_FALSE( dummy.get_recipes_from_books( dummy.crafting_inventory() ).contains( *r2 ) );
                }
            }
        }
    }
}

// This crashes subsequent testcases for some reason.
TEST_CASE( "crafting_with_a_companion", "[.]" )
{
    clear_all_state();
    const recipe *r = &recipe_id( "brew_mead" ).obj();
    avatar dummy;

    REQUIRE( dummy.get_skill_level( r->skill_used ) == 0 );
    REQUIRE_FALSE( dummy.knows_recipe( r ) );
    REQUIRE( r->skill_used );

    GIVEN( "a companion who can help with crafting" ) {
        standard_npc who( "helper" );

        who.set_attitude( NPCATT_FOLLOW );
        who.spawn_at_sm( tripoint_zero );

        g->load_npcs();

        CHECK( !dummy.in_vehicle );
        dummy.setpos( who.pos() );
        const auto helpers( character_funcs::get_crafting_helpers( dummy ) );

        REQUIRE( std::find( helpers.begin(), helpers.end(), &who ) != helpers.end() );
        REQUIRE_FALSE( dummy.get_available_recipes( dummy.crafting_inventory(), &helpers ).contains( *r ) );
        REQUIRE_FALSE( who.knows_recipe( r ) );

        WHEN( "you have the required skill" ) {
            dummy.set_skill_level( r->skill_used, r->difficulty );

            AND_WHEN( "he knows the recipe" ) {
                who.learn_recipe( r );

                THEN( "he helps you" ) {
                    CHECK( dummy.get_available_recipes( dummy.crafting_inventory(), &helpers ).contains( *r ) );
                }
            }
            AND_WHEN( "he has the cookbook in his inventory" ) {
                detached_ptr<item> det = item::spawn( "brewing_cookbook" );
                item &cookbook = *det;
                who.i_add( std::move( det ) );

                REQUIRE( cookbook.is_book() );
                REQUIRE_FALSE( cookbook.type->book->recipes.empty() );

                THEN( "he shows it to you" ) {
                    CHECK( dummy.get_available_recipes( dummy.crafting_inventory(), &helpers ).contains( *r ) );
                }
            }
        }
    }
}

static void prep_craft( const recipe_id &rid, std::vector<detached_ptr<item>> &tools,
                        bool expect_craftable )
{
    clear_avatar();
    const tripoint test_origin( 60, 60, 0 );
    g->u.setpos( test_origin );
    g->u.wear_item( item::spawn( "backpack" ), false );
    for( detached_ptr<item> &gear : tools ) {
        g->u.i_add( std::move( gear ) );
    }

    const recipe &r = rid.obj();

    // Ensure adequate skill for all "required" skills
    for( const std::pair<const skill_id, int> &skl : r.required_skills ) {
        g->u.set_skill_level( skl.first, skl.second );
    }
    // and just in case "used" skill difficulty is higher, set that too
    g->u.set_skill_level( r.skill_used, std::max( r.difficulty,
                          g->u.get_skill_level( r.skill_used ) ) );

    const inventory &crafting_inv = g->u.crafting_inventory();
    bool can_craft = r.deduped_requirements().can_make_with_inventory(
                         crafting_inv, r.get_component_filter() );
    REQUIRE( can_craft == expect_craftable );
}

static time_point midnight = calendar::turn_zero + 0_hours;
static time_point midday = calendar::turn_zero + 12_hours;

// This tries to actually run the whole craft activity, which is more thorough,
// but slow
static int actually_test_craft( const recipe_id &rid, std::vector<detached_ptr<item>> &tools,
                                int interrupt_after_turns )
{
    avatar &you = get_avatar();
    prep_craft( rid, tools, true );
    set_time( midday ); // Ensure light for crafting
    const recipe &rec = rid.obj();
    REQUIRE( morale_crafting_speed_multiplier( you, rec ) == 1.0 );
    REQUIRE( lighting_crafting_speed_multiplier( you, rec ) == 1.0 );
    REQUIRE( !you.activity );

    // This really shouldn't be needed, but for some reason the tests fail for mingw builds without it
    you.learn_recipe( &rec );
    REQUIRE( you.has_recipe( &rec, you.crafting_inventory(),
                             character_funcs::get_crafting_helpers( you ) ) != -1 );

    you.make_craft( rid, 1 );
    REQUIRE( you.activity );
    REQUIRE( you.activity->id() == activity_id( "ACT_CRAFT" ) );
    int turns = 0;
    while( you.activity->id() == activity_id( "ACT_CRAFT" ) ) {
        if( turns >= interrupt_after_turns ) {
            set_time( midnight ); // Kill light to interrupt crafting
        }
        ++turns;
        you.moves = 100;
        you.activity->do_turn( you );
    }
    return turns;
}

static void add_tool( std::vector<detached_ptr<item>> &tools, const char *type, int count = 1 )
{
    while( count-- ) {
        tools.push_back( item::spawn( type ) );
    }
}

TEST_CASE( "tools use charge to craft", "[crafting][charge]" )
{
    clear_all_state();
    std::vector<detached_ptr<item>> tools;

    GIVEN( "recipe and required tools/materials" ) {
        recipe_id carver( "carver_off" );
        // Uses fabrication skill
        // Requires electronics 3
        // Difficulty 4
        // Learned from advanced_electronics or textbook_electronics

        // Tools needed:
        add_tool( tools, "screwdriver" );
        add_tool( tools, "mold_plastic" );

        // Materials needed
        add_tool( tools, "solder_wire", 10 );
        add_tool( tools, "plastic_chunk", 6 );
        add_tool( tools, "blade", 2 );
        add_tool( tools, "cable", 5 );
        add_tool( tools, "motor_tiny" );
        add_tool( tools, "power_supply" );
        add_tool( tools, "scrap" );

        // Charges needed to craft:
        // - 10 charges of soldering iron
        // - 10 charges of surface heat

        WHEN( "each tool has enough charges" ) {
            tools.push_back( item::spawn( "hotplate", calendar::start_of_cataclysm, 20 ) );
            tools.push_back( item::spawn( "soldering_iron", calendar::start_of_cataclysm, 20 ) );

            THEN( "crafting succeeds, and uses charges from each tool" ) {
                int turns = actually_test_craft( recipe_id( "carver_off" ), tools, INT_MAX );
                CAPTURE( turns );
                CHECK( get_remaining_charges( "hotplate" ) == 10 );
                CHECK( get_remaining_charges( "soldering_iron" ) == 10 );
            }
        }

        WHEN( "multiple tools have enough combined charges" ) {
            tools.emplace_back( item::spawn( "hotplate", calendar::start_of_cataclysm, 5 ) );
            tools.emplace_back( item::spawn( "hotplate", calendar::start_of_cataclysm, 5 ) );
            tools.emplace_back( item::spawn( "soldering_iron", calendar::start_of_cataclysm, 5 ) );
            tools.emplace_back( item::spawn( "soldering_iron", calendar::start_of_cataclysm, 5 ) );

            THEN( "crafting succeeds, and uses charges from multiple tools" ) {
                actually_test_craft( recipe_id( "carver_off" ), tools, INT_MAX );
                CHECK( get_remaining_charges( "hotplate" ) == 0 );
                CHECK( get_remaining_charges( "soldering_iron" ) == 0 );
            }
        }

        WHEN( "UPS-modded tools have enough charges" ) {
            detached_ptr<item> hotplate = item::spawn( "hotplate", calendar::start_of_cataclysm, 0 );
            hotplate->put_in( item::spawn( "battery_ups" ) );
            tools.push_back( std::move( hotplate ) );
            detached_ptr<item> soldering_iron = item::spawn( "soldering_iron", calendar::start_of_cataclysm,
                                                0 );
            soldering_iron->put_in( item::spawn( "battery_ups" ) );
            tools.push_back( std::move( soldering_iron ) );
            tools.push_back( item::spawn( "UPS_off", calendar::start_of_cataclysm, 500 ) );

            THEN( "crafting succeeds, and uses charges from the UPS" ) {
                actually_test_craft( recipe_id( "carver_off" ), tools, INT_MAX );
                CHECK( get_remaining_charges( "hotplate" ) == 0 );
                CHECK( get_remaining_charges( "soldering_iron" ) == 0 );
                CHECK( get_remaining_charges( "UPS_off" ) == 480 );
            }
        }

        WHEN( "UPS-modded tools do not have enough charges" ) {
            detached_ptr<item> hotplate = item::spawn( "hotplate", calendar::start_of_cataclysm, 0 );
            hotplate->put_in( item::spawn( "battery_ups" ) );
            tools.push_back( std::move( hotplate ) );
            detached_ptr<item> soldering_iron = item::spawn( "soldering_iron", calendar::start_of_cataclysm,
                                                0 );
            soldering_iron->put_in( item::spawn( "battery_ups" ) );
            tools.push_back( std::move( soldering_iron ) );
            tools.push_back( item::spawn( "UPS_off", calendar::start_of_cataclysm, 10 ) );

            THEN( "crafting fails, and no charges are used" ) {
                prep_craft( recipe_id( "carver_off" ), tools, false );
                CHECK( get_remaining_charges( "UPS_off" ) == 10 );
            }
        }
    }
}

TEST_CASE( "tool_use", "[crafting][tool]" )
{
    clear_all_state();
    SECTION( "clean_water" ) {
        std::vector<detached_ptr<item>> tools;
        tools.push_back( item::spawn( "hotplate", calendar::start_of_cataclysm, 20 ) );
        detached_ptr<item> plastic_bottle = item::spawn( "bottle_plastic" );
        plastic_bottle->put_in( item::spawn( "water", calendar::start_of_cataclysm, 2 ) );
        tools.push_back( std::move( plastic_bottle ) );
        tools.push_back( item::spawn( "pot" ) );

        // Can't actually test crafting here since crafting a liquid currently causes a ui prompt
        prep_craft( recipe_id( "water_clean" ), tools, true );
    }
    SECTION( "clean_water_in_occupied_cooking_vessel" ) {
        std::vector<detached_ptr<item>> tools;
        tools.push_back( item::spawn( "hotplate", calendar::start_of_cataclysm, 20 ) );
        detached_ptr<item> plastic_bottle = item::spawn( "bottle_plastic" );
        plastic_bottle->put_in( item::spawn( "water", calendar::start_of_cataclysm, 2 ) );
        tools.push_back( std::move( plastic_bottle ) );
        detached_ptr<item> jar = item::spawn( "jar_glass" );
        // If it's not watertight the water will spill.
        REQUIRE( jar->is_watertight_container() );
        jar->put_in( item::spawn( "water", calendar::start_of_cataclysm, 2 ) );
        tools.push_back( std::move( jar ) );

        prep_craft( recipe_id( "water_clean" ), tools, false );
    }
}

TEST_CASE( "Component same as tool", "[crafting][tool]" )
{
    clear_all_state();
    SECTION( "primitive_hammer with one rock" ) {
        std::vector<detached_ptr<item>> tools;
        tools.push_back( item::spawn( "rock" ) );
        tools.push_back( item::spawn( "2x4" ) );
        tools.push_back( item::spawn( "thread", calendar::turn, 100 ) );

        prep_craft( recipe_id( "primitive_hammer" ), tools, false );
    }
    SECTION( "primitive_hammer with two rocks" ) {
        std::vector<detached_ptr<item>> tools;
        tools.push_back( item::spawn( "rock" ) );
        tools.push_back( item::spawn( "rock" ) );
        tools.push_back( item::spawn( "2x4" ) );
        tools.push_back( item::spawn( "thread", calendar::turn, 100 ) );

        prep_craft( recipe_id( "primitive_hammer" ), tools, true );
    }
}

// Resume the first in progress craft found in the player's inventory
static int resume_craft()
{
    avatar &you = get_avatar();
    std::vector<item *> crafts = you.items_with( []( const item & itm ) {
        return itm.is_craft();
    } );
    REQUIRE( crafts.size() == 1 );
    item *craft = crafts.front();
    set_time( midday ); // Ensure light for crafting
    REQUIRE( crafting_speed_multiplier( you, *craft, bench_location{bench_type::hands, you.pos()} ) ==
             1.0 );
    REQUIRE( !you.activity );
    avatar_funcs::use_item( you, *craft );
    REQUIRE( you.activity );
    REQUIRE( you.activity->id() == activity_id( "ACT_CRAFT" ) );
    int turns = 0;
    while( you.activity->id() == activity_id( "ACT_CRAFT" ) ) {
        ++turns;
        you.moves = 100;
        you.activity->do_turn( you );
    }
    return turns;
}

static void verify_inventory( const std::vector<std::string> &has,
                              const std::vector<std::string> &hasnt )
{
    std::ostringstream os;
    os << "Inventory:\n";
    for( const item *i : g->u.inv_dump() ) {
        os << "  " << i->typeId().str() << " (" << i->charges << ")\n";
    }
    os << "Wielded:\n" << g->u.primary_weapon().tname() << "\n";
    INFO( os.str() );
    for( const std::string &i : has ) {
        INFO( "expecting " << i );
        const bool has_item =
            player_has_item_of_type( i ) || g->u.primary_weapon().type->get_id() == itype_id( i );
        REQUIRE( has_item );
    }
    for( const std::string &i : hasnt ) {
        INFO( "not expecting " << i );
        const bool hasnt_item =
            !player_has_item_of_type( i ) && !( g->u.primary_weapon().type->get_id() == itype_id( i ) );
        REQUIRE( hasnt_item );
    }
}

TEST_CASE( "total crafting time with or without interruption", "[crafting][time][resume]" )
{
    clear_all_state();
    GIVEN( "a recipe and all the required tools and materials to craft it" ) {
        recipe_id test_recipe( "crude_picklock" );
        int expected_time_taken = test_recipe->batch_time( 1, 1, 0 );
        int expected_turns_taken = divide_round_up( expected_time_taken, 100 );

        std::vector<detached_ptr<item>> tools;
        tools.push_back( item::spawn( "hammer" ) );

        // Will interrupt after 2 turns, so craft needs to take at least that long
        REQUIRE( expected_turns_taken > 2 );
        int actual_turns_taken;

        WHEN( "crafting begins, and continues until the craft is completed" ) {
            tools.push_back( item::spawn( "scrap", calendar::start_of_cataclysm, 1 ) );
            actual_turns_taken = actually_test_craft( test_recipe, tools, INT_MAX );

            THEN( "it should take the expected number of turns" ) {
                CHECK( actual_turns_taken == expected_turns_taken );

                AND_THEN( "the finished item should be in the inventory" ) {
                    verify_inventory( { "crude_picklock" }, { "scrap" } );
                }
            }
        }

        WHEN( "crafting begins, but is interrupted after 2 turns" ) {
            tools.push_back( item::spawn( "scrap", calendar::start_of_cataclysm, 1 ) );
            actual_turns_taken = actually_test_craft( test_recipe, tools, 2 );
            REQUIRE( actual_turns_taken == 3 );

            THEN( "the in-progress craft should be in the inventory" ) {
                verify_inventory( { "craft" }, { "crude_picklock" } );

                AND_WHEN( "crafting resumes until the craft is finished" ) {
                    actual_turns_taken = resume_craft();

                    THEN( "it should take the remaining number of turns" ) {
                        CHECK( actual_turns_taken == expected_turns_taken - 2 );

                        AND_THEN( "the finished item should be in the inventory" ) {
                            verify_inventory( { "crude_picklock" }, { "craft" } );
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE( "debug hammerspace", "[crafting]" )
{
    clear_all_state();
    static const recipe_id test_recipe( "nodachi" );

    GIVEN( "A character with debug hammerspace trait" ) {
        avatar dummy;
        dummy.toggle_trait( trait_DEBUG_HS );
        // TODO: Shouldn't be needed!
        dummy.set_body();
        // TODO: Debug vision should handle this part
        dummy.toggle_trait( trait_DEBUG_STORAGE );
        dummy.i_add( item::spawn( itype_id( "atomic_lamp" ) ) );
        REQUIRE( character_funcs::can_see_fine_details( dummy ) );

        WHEN( "The character tries to craft a no-dachi" ) {
            craft_command command( &*test_recipe, 1, false, &dummy );
            item *craft_item = dummy.start_craft( command, dummy.pos() );

            THEN( "The craft item is created" ) {
                REQUIRE( ( craft_item && !craft_item->is_null() ) );
                AND_WHEN( "The character spends a second performing the activity set when starting the craft" ) {
                    dummy.set_moves( 100 );
                    dummy.activity->do_turn( dummy );
                    THEN( "The activity isn't finished yet" ) {
                        CHECK( !dummy.activity->is_null() );
                    }
                }
            }
        }
    }
}

TEST_CASE( "oven electric grid", "[crafting][overmap][grids][slow]" )
{
    clear_all_state();
    map &m = get_map();
    avatar &u = get_avatar();
    constexpr tripoint start_pos = tripoint( 60, 60, 0 );
    const tripoint_abs_ms start_pos_abs( m.getabs( start_pos ) );
    u.setpos( start_pos );
    clear_avatar();
    GIVEN( "player is near an oven on an electric grid with a battery on it" ) {
        // TODO: clear_grids()
        auto om = overmap_buffer.get_om_global( u.global_omt_location() );
        om.om->set_electric_grid_connections( om.local, {} );

        m.furn_set( start_pos + point( 10, 0 ), furn_str_id( "f_battery" ) );
        m.furn_set( start_pos + point_east, furn_str_id( "f_oven" ) );

        distribution_grid_tracker grid_tracker;
        grid_tracker.load( m );
        distribution_grid &grid = grid_tracker.grid_at( start_pos_abs + point( 10, 0 ) );
        REQUIRE( !grid.empty() );
        // We need the grid to be the same for both the oven and the battery
        REQUIRE( &grid == &grid_tracker.grid_at( start_pos_abs + point_east ) );
        WHEN( "the grid is charged with 10 units of power" ) {
            grid.mod_resource( 10 );
            REQUIRE( grid.get_resource() == 10 );
            AND_WHEN( "crafting inventory is built" ) {
                u.invalidate_crafting_inventory();
                const inventory &crafting_inv = u.crafting_inventory();
                THEN( "it contains an oven item with 10 charges" ) {
                    REQUIRE( crafting_inv.has_charges( itype_id( "fake_oven" ), 10 ) );
                }
            }

            // Massive ladder of when/then
            // Any way to clean it up without re-running the slow test for every check?
            AND_WHEN( "the player is near a pot and a chunk of meat" ) {
                u.invalidate_crafting_inventory();
                m.add_item( u.pos(), item::spawn( "pot" ) );
                m.add_item( u.pos(), item::spawn( "meat" ) );
                THEN( "cooked meat can be crafted" ) {
                    const recipe &r = *recipe_id( "meat_cooked" );
                    const inventory &crafting_inv = u.crafting_inventory();
                    const deduped_requirement_data &ddrd = r.deduped_requirements();
                    bool can_craft = ddrd.can_make_with_inventory( crafting_inv, r.get_component_filter() );
                    REQUIRE( can_craft );
                    AND_THEN( "there is only one possible tool/component selection" ) {
                        const auto filter = r.get_component_filter();
                        const auto tool_options = ddrd.feasible_alternatives(
                                                      u.crafting_inventory(), filter, 1, cost_adjustment::start_only );
                        REQUIRE( tool_options.size() == 1u );
                        AND_WHEN( "the player crafts cooked meat" ) {
                            u.make_craft( r.ident(), 1 );
                            REQUIRE( u.activity->id() == activity_id( "ACT_CRAFT" ) );
                            // TODO: Nice way to finish a craft job
                            for( size_t i = 0; i < 10000; i++ ) {
                                u.set_moves( 100000 );
                                u.activity->do_turn( u );
                                if( u.activity->id() == activity_id::NULL_ID() ) {
                                    break;
                                }
                            }
                            REQUIRE( u.activity->id() == activity_id::NULL_ID() );
                            THEN( "the crafting inventory now contains cooked meat" ) {
                                u.invalidate_crafting_inventory();
                                //TODO: Reinstate the below check. Prior to isolation improvements it would pass when run as part of the full suite but fail alone
                                //CHECK( u.crafting_inventory().has_amount( itype_id( "meat_cooked" ), 1 ) );
                                AND_THEN( "the grid contains less than 10 power" ) {
                                    CHECK( grid.get_resource() < 10 );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE( "tool selection ui", "[crafting][ui]" )
{
    clear_all_state();
    npc dummy;

    std::vector<tool_comp> tools;
    comp_selection<tool_comp> expected_result;

    inventory map_inv;

    GIVEN( "One compatible tool in map inventory" ) {
        tool_comp tool_component( itype_id( "test_soldering_iron" ), 10 );
        tools = {tool_component};
        map_inv.add_item( *item::spawn_temporary( tool_component.type, calendar::start_of_cataclysm,
                          100 ), false );

        comp_selection<tool_comp> result = crafting::select_tool_component( tools, 1, map_inv,
                                           &dummy, false,
                                           DEFAULT_HOTKEYS, cost_adjustment::none );

        THEN( "That tool is selected and is from map" ) {
            REQUIRE( map_inv.size() == 1 );
            CAPTURE( map_inv.find_item( 0 ).display_name() );
            CHECK( result.comp.type == tool_component.type );
            CHECK( result.use_from == usage_from::map );
        }
    }

    GIVEN( "Two compatible tools in map inventory, one free and one costly" ) {
        tool_comp costly( itype_id( "test_soldering_iron" ), 10 );
        tool_comp free( itype_id( "test_screwdriver" ), -1 );
        tools = {costly, free};
        map_inv.add_item( *item::spawn_temporary( costly.type ), false );
        map_inv.add_item( *item::spawn_temporary( free.type ), false );

        comp_selection<tool_comp> result = crafting::select_tool_component( tools, 1, map_inv,
                                           &dummy, false,
                                           DEFAULT_HOTKEYS, cost_adjustment::none );

        THEN( "Use from is set to none" ) {
            CHECK( result.use_from == usage_from::none );
        }
    }

    GIVEN( "Two compatible tools in map inventory, one with enough charges for entire craft and one with half" ) {
        tool_comp too_little( itype_id( "test_soldering_iron" ), 200 );
        tool_comp enough( itype_id( "test_battery_tool" ), 100 );
        tools = {too_little, enough};
        map_inv.add_item( *item::spawn_temporary( too_little.type, calendar::start_of_cataclysm, 100 ),
                          false );
        map_inv.add_item( *item::spawn_temporary( enough.type, calendar::start_of_cataclysm, 100 ), false );

        comp_selection<tool_comp> result = crafting::select_tool_component( tools, 1, map_inv,
                                           &dummy, false,
                                           DEFAULT_HOTKEYS, cost_adjustment::none );

        THEN( "The tool with enough charges is selected" ) {
            CHECK( result.comp.type == enough.type );
        }
    }
}
