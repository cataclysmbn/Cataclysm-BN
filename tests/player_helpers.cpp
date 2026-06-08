#include "catch/catch.hpp"

#include "player_helpers.h"

#include <cstddef>
#include <list>
#include <memory>
#include <vector>

#include "avatar.h"
#include "bionics.h"
#include "character.h"
#include "character_id.h"
#include "character_functions.h"
#include "consumption.h"
#include "coordinates.h"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"
#include "make_static.h"
#include "map.h"
#include "material.h"
#include "npc.h"
#include "pimpl.h"
#include "player.h"
#include "player_activity.h"
#include "stomach.h"
#include "string_id.h"
#include "type_id.h"

int get_remaining_charges( const std::string &tool_id )
{
    const inventory crafting_inv = g->u.crafting_inventory();
    std::vector<item *> items =
    crafting_inv.items_with( [tool_id]( const item & i ) {
        return i.typeId() == itype_id( tool_id );
    } );
    int remaining_charges = 0;
    for( const item * const &instance : items ) {
        remaining_charges += instance->ammo_remaining();
    }
    return remaining_charges;
}

bool player_has_item_of_type( const std::string &type )
{

    std::vector<item *> inv_items = g->u.inv_dump();

    return std::any_of( inv_items.begin(), inv_items.end(), [&]( const item * const & i ) {
        return i->type->get_id() == itype_id( type );
    } );
}

void clear_character( player &dummy, bool debug_storage )
{
    character_funcs::normalize( dummy );

    // Remove first worn item until there are none left.
    std::vector<detached_ptr<item>> temp;
    while( dummy.takeoff( dummy.i_at( -2 ), &temp ) );
    dummy.inv_clear();
    dummy.remove_primary_weapon();
    dummy.clear_mutations();

    // Prevent spilling, but don't cause encumbrance
    if( debug_storage && !dummy.has_trait( trait_id( "DEBUG_STORAGE" ) ) ) {
        dummy.set_mutation( trait_id( "DEBUG_STORAGE" ) );
    }

    dummy.clear_bionics();
    dummy.set_power_level( 0_J );
    dummy.set_max_power_level( 0_J );

    dummy.recalculate_enchantment_cache();

    // Clear stomach and then eat a nutritious meal to normalize stomach
    // contents (needs to happen before clear_morale).
    dummy.stomach.empty();
    dummy.consumption_history->elems.clear();
    item &food = *item::spawn_temporary( "debug_nutrition" );
    dummy.eat( food );

    // This sets HP to max, clears addictions and morale,
    // and sets hunger, thirst, fatigue and such to zero
    dummy.environmental_revert_effect();

    dummy.clear_skills();
    dummy.clear_morale();
    dummy.activity->set_to_null();
    //Make sure any lingering safe references from the activity are removed
    dummy.activity->targets.clear();
    dummy.reset_chargen_attributes();
    dummy.set_pain( 0 );
    dummy.reset_bonuses();
    dummy.set_moves( -100 );

    // Restore all stamina and go to walk mode
    dummy.set_stamina( dummy.get_stamina_max() );
    dummy.set_movement_mode( CMM_WALK );

    // Set HP to max here and also later, for disabled/broken limbs
    dummy.set_all_parts_hp_to_max();

    // Make sure we don't carry around weird effects.
    dummy.clear_effects(); // mark effects for removal
    dummy.process_effects(); // actually remove them
    dummy.set_underwater( false );

    // Make stats nominal.
    dummy.str_max = 8;
    dummy.dex_max = 8;
    dummy.int_max = 8;
    dummy.per_max = 8;
    dummy.set_str_bonus( 0 );
    dummy.set_dex_bonus( 0 );
    dummy.set_int_bonus( 0 );
    dummy.set_per_bonus( 0 );
    dummy.reset_bonuses();
    dummy.set_speed_base( 100 );
    dummy.set_speed_bonus( 0 );
    dummy.set_all_parts_hp_to_max();

    dummy.cash = 0;
    dummy.dodges_left = 1;

    const tripoint_bub_ms spot( 60, 60, 0 );
    dummy.setpos( spot );

    dummy.invalidate_crafting_inventory();
}

void clear_avatar()
{
    clear_character( g->u );
}

void process_activity( player &dummy )
{
    do {
        dummy.moves += dummy.get_speed();
        while( dummy.moves > 0 && dummy.activity ) {
            dummy.activity->do_turn( dummy );
        }
    } while( dummy.activity );
}

npc &spawn_npc( const point_bub_ms &p, const std::string &npc_class )
{
    const string_id<npc_template> test_guy( npc_class );
    const character_id model_id = g->m.place_npc( p, test_guy, true );
    g->load_npcs();

    npc *guy = g->find_npc( model_id );
    REQUIRE( guy != nullptr );
    CHECK( !guy->in_vehicle );
    return *guy;
}

void give_and_activate_bionic( player &p, const bionic_id &bioid )
{
    INFO( "bionic " + bioid.str() + " is valid" );
    REQUIRE( bioid.is_valid() );

    p.add_bionic( bioid );
    INFO( "dummy has gotten " + bioid.str() + " bionic " );
    REQUIRE( p.has_bionic( bioid ) );

    bionic &bio = p.get_bionic_state( bioid );
    REQUIRE( bio.id == bioid );

    // turn on if possible
    if( bio.id->has_flag( STATIC( flag_id( "BIONIC_TOGGLED" ) ) ) && !bio.powered ) {
        const std::vector<itype_id> fuel_opts = bio.info().fuel_opts;
        if( !fuel_opts.empty() ) {
            p.set_value( fuel_opts.front().str(), "2" );
        }
        p.activate_bionic( bio );
        INFO( "bionic " + bio.id.str() + " is active " );
        REQUIRE( p.has_active_bionic( bioid ) );
        if( !fuel_opts.empty() ) {
            p.remove_value( fuel_opts.front().str() );
        }
    }
}

void arm_character( player &shooter, const std::string &gun_type,
                    const std::vector<std::string> &mods,
                    const std::string &ammo_type )
{
    shooter.remove_primary_weapon();

    itype_id gun_id( gun_type );
    // Give shooter a loaded gun of the requested type.
    detached_ptr<item> det = item::spawn( gun_id );
    item &gun = *det;
    shooter.i_add( std::move( det ) );
    const itype_id ammo_id = ammo_type.empty() ? gun.ammo_default() : itype_id( ammo_type );
    if( gun.magazine_integral() ) {
        det = item::spawn( ammo_id, calendar::turn, gun.ammo_capacity() );
        item &ammo = *det;
        shooter.i_add( std::move( det ) );
        REQUIRE( gun.is_reloadable_with( ammo_id ) );
        REQUIRE( shooter.can_reload( gun, ammo_id ) );
        gun.reload( shooter, ammo, gun.ammo_capacity() );
    } else {
        const itype_id magazine_id = gun.magazine_default();
        det = item::spawn( magazine_id );
        item &magazine = *det;
        shooter.i_add( std::move( det ) );
        det = item::spawn( ammo_id, calendar::turn, magazine.ammo_capacity() );
        item &ammo = *det;
        shooter.i_add( std::move( det ) );
        REQUIRE( magazine.is_reloadable_with( ammo_id ) );
        REQUIRE( shooter.can_reload( magazine, ammo_id ) );
        magazine.reload( shooter, ammo, magazine.ammo_capacity() );
        gun.reload( shooter, magazine, magazine.ammo_capacity() );
    }
    for( const auto &mod : mods ) {
        gun.put_in( item::spawn( itype_id( mod ) ) );
    }
    shooter.wield( gun );
}
