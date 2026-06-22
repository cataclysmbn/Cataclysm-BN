#include "catch/catch.hpp"

#include <memory>
#include <string>
#include <vector>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "calendar.h"
#include "coordinates.h"
#include "craft_command.h"
#include "crafting.h"
#include "game.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "ranged.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "state_helpers.h"
#include "type_id.h"
#include "value_ptr.h"

TEST_CASE( "modular_pipe_rifle_craft_and_convert", "[gun]" )
{
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    avatar &you = get_avatar();
    clear_avatar();
    you.wear_item( item::spawn( "backpack", bday ), false );

    // Set skills for crafting
    you.set_skill_level( skill_id( "fabrication" ), 2 );
    you.set_skill_level( skill_id( "mechanics" ), 1 );

    // Learn pipe_rifle recipe
    // cata_make_recipe is not available in test builds so use the recipe_id directly
    const recipe &r = recipe_id( "pipe_rifle" ).obj();
    you.learn_recipe( &r );

    // Add components
    you.i_add( item::spawn( "pipe", bday ) );
    you.i_add( item::spawn( "2x4", bday ) );
    you.i_add( item::spawn( "scrap", bday, 2 ) );

    // Add tools providing SAW_M and SCREW qualities
    you.i_add( item::spawn( "hacksaw", bday, 100 ) );
    you.i_add( item::spawn( "screwdriver", bday, 100 ) );

    // Craft
    set_time( calendar::turn_zero + 12_hours );
    you.make_craft( r.ident(), 1 );
    REQUIRE( you.activity );
    REQUIRE( you.activity->id() == activity_id( "ACT_CRAFT" ) );
    process_activity( you );

    // Verify pipe rifle was crafted
    REQUIRE( player_has_item_of_type( "pipe_rifle" ) );

    // Find and wield
    item *pipe_rifle = nullptr;
    for( item *i : you.inv_dump() ) {
        if( i->typeId() == itype_id( "pipe_rifle" ) ) {
            pipe_rifle = i;
            break;
        }
    }
    REQUIRE( pipe_rifle != nullptr );
    you.wield( *pipe_rifle );
    item &gun = you.primary_weapon();
    REQUIRE( gun.typeId() == itype_id( "pipe_rifle" ) );

    const tripoint_bub_ms target = you.bub_pos() + tripoint_rel_ms( 5, 0, 0 );

    // --- No kit: no ammo type, can't fire ---
    CHECK( gun.ammo_default().is_null() );
    CHECK( gun.ammo_remaining() == 0 );
    CHECK( gun.gunmods().empty() );
    CHECK( ranged::fire_gun( you, target, 1 ) == 0 );

    // --- Install .22 kit, load .22 LR, fire, remove kit ---
    {
        detached_ptr<item> kit = item::spawn( "retool_pipe_22" );
        REQUIRE( gun.is_gunmod_compatible( *kit ).success() );
        gun.put_in( std::move( kit ) );

        CHECK( gun.gunmods().size() == 1 );
        CHECK_FALSE( gun.ammo_default().is_null() );
        CHECK( gun.ammo_remaining() == 0 );

        gun.ammo_set( itype_id( "22_lr" ), 1 );
        CHECK( gun.ammo_remaining() == 1 );

        CHECK( ranged::fire_gun( you, target, 1 ) == 1 );
        CHECK( gun.ammo_remaining() == 0 );

        // Remove .22 kit → gun reverts to no ammo type
        item *mod = gun.gunmods().front();
        CHECK( avatar_funcs::gunmod_remove( you, gun, *mod ) );
        CHECK( gun.gunmods().empty() );
    }
    CHECK( gun.ammo_default().is_null() );

    // --- Install .223 kit, load .223, fire, remove kit ---
    {
        detached_ptr<item> kit = item::spawn( "retool_pipe_223" );
        REQUIRE( gun.is_gunmod_compatible( *kit ).success() );
        gun.put_in( std::move( kit ) );

        CHECK( gun.gunmods().size() == 1 );
        CHECK_FALSE( gun.ammo_default().is_null() );
        CHECK( gun.ammo_remaining() == 0 );

        gun.ammo_set( itype_id( "223" ), 1 );
        CHECK( gun.ammo_remaining() == 1 );

        CHECK( ranged::fire_gun( you, target, 1 ) == 1 );
        CHECK( gun.ammo_remaining() == 0 );

        // Remove .223 kit → gun reverts to no ammo type
        item *mod = gun.gunmods().front();
        CHECK( avatar_funcs::gunmod_remove( you, gun, *mod ) );
        CHECK( gun.gunmods().empty() );
    }
    CHECK( gun.ammo_default().is_null() );
}
