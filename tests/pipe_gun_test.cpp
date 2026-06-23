#include "catch/catch.hpp"

#include <memory>

#include "avatar.h"
#include "avatar_functions.h"
#include "calendar.h"
#include "coordinates.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "player_helpers.h"
#include "ranged.h"
#include "state_helpers.h"
#include "flag.h"
#include "type_id.h"
#include "value_ptr.h"

TEST_CASE( "modular_pipe_rifle_craft_and_convert", "[gun]" )
{
    clear_all_state();
    avatar &you = get_avatar();
    clear_avatar();
    you.wear_item( item::spawn( "backpack", calendar::start_of_cataclysm ), false );

    // Spawn pipe rifle and wield it
    you.wield( item::spawn( "pipe_rifle", calendar::start_of_cataclysm ) );
    item &gun = you.primary_weapon();
    REQUIRE( gun.typeId() == itype_id( "pipe_rifle" ) );

    // --- No kit: no ammo type, can't fire ---
    CHECK( gun.ammo_default().is_null() );
    CHECK( gun.ammo_remaining() == 0 );
    CHECK( gun.gunmods().empty() );

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

        const tripoint_bub_ms target = you.bub_pos() + tripoint_rel_ms( 5, 0, 0 );
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

        const tripoint_bub_ms target = you.bub_pos() + tripoint_rel_ms( 5, 0, 0 );
        CHECK( ranged::fire_gun( you, target, 1 ) == 1 );
        CHECK( gun.ammo_remaining() == 0 );

        // Remove .223 kit → gun reverts to no ammo type
        item *mod = gun.gunmods().front();
        CHECK( avatar_funcs::gunmod_remove( you, gun, *mod ) );
        CHECK( gun.gunmods().empty() );
    }
    CHECK( gun.ammo_default().is_null() );
}
