#include "catch/catch.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "ammo.h"
#include "avatar.h"
#include "calendar.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "point.h"
#include "state_helpers.h"
#include "string_id.h"
#include "type_id.h"
#include "units.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_functions.h"
#include "vehicle_part.h"

static std::vector<const vpart_info *> turret_types()
{
    std::vector<const vpart_info *> res;

    for( const auto &e : vpart_info::all() ) {
        if( e.second.has_flag( "TURRET" ) ) {
            res.push_back( &e.second );
        }
    }

    return res;
}

static auto biggest_tank( const ammotype &ammo ) -> const vpart_info *
{
    std::vector<const vpart_info *> res;

    for( const auto &e : vpart_info::all() ) {
        const auto &vp = e.second;
        if( !item::spawn_temporary( vp.item )->is_watertight_container() ) {
            continue;
        }

        const itype *fuel = &*vp.fuel_type;
        if( fuel->ammo && fuel->ammo->type == ammo ) {
            res.push_back( &vp );
        }
    }

    if( res.empty() ) { return nullptr; }

    return *std::ranges::max_element( res, {}, &vpart_info::size );
}

TEST_CASE( "vehicle_turret", "[vehicle][gun][magazine][.]" )
{
    clear_all_state();
    map &here = get_map();
    avatar &player_character = get_avatar();
    for( auto e : turret_types() ) {
        SECTION( e->name() ) {
            vehicle *veh = here.add_vehicle( vproto_id( "none" ), point( 65, 65 ), 270_degrees, 0, 0 );
            REQUIRE( veh );

            const int idx = veh->install_part( point_zero, e->get_id(), true );
            REQUIRE( idx >= 0 );

            REQUIRE( veh->install_part( point_zero, vpart_id( "storage_battery" ), true ) >= 0 );
            veh->charge_battery( 10000 );

            auto ammo =
                ammotype( veh->turret_query( veh->part( idx ) ).base().ammo_default().str() );

            if( veh->part_flag( idx, "USE_TANKS" ) ) {
                auto *tank = biggest_tank( ammo );
                REQUIRE( tank );
                INFO( tank->get_id().str() );

                auto tank_idx = veh->install_part( point_zero, tank->get_id(), true );
                REQUIRE( tank_idx >= 0 );
                REQUIRE( veh->part( tank_idx ).ammo_set( ammo->default_ammotype() ) );

            } else if( ammo ) {
                veh->part( idx ).ammo_set( ammo->default_ammotype() );
            }

            auto qry = veh->turret_query( veh->part( idx ) );
            REQUIRE( qry );

            REQUIRE( qry.query() == turret_data::status::ready );
            REQUIRE( qry.range() > 0 );

            player_character.setpos( veh->global_part_pos3( idx ) );
            REQUIRE( qry.fire( player_character, player_character.pos() + point( qry.range(), 0 ) ) > 0 );

            here.destroy_vehicle( veh );
        }
    }
}

TEST_CASE( "vehicle_turret_autoloader_integral_magazine", "[vehicle][gun][turret][autoload]" )
{
    clear_all_state();
    map &here = get_map();
    vehicle *veh = here.add_vehicle( vproto_id( "none" ), point( 65, 65 ), 270_degrees, 0, 0 );
    REQUIRE( veh );

    const auto turret_part_id = vpart_id( "mounted_rebar_rifle" );
    const auto autoloader_part_id = vpart_id( "turret_autoloader" );
    const auto cargo_part_id = vpart_id( "box" );
    const auto battery_part_id = vpart_id( "storage_battery" );

    const auto turret_index = veh->install_part( point_zero, turret_part_id, true );
    REQUIRE( turret_index >= 0 );
    REQUIRE( veh->install_part( point_zero, autoloader_part_id, true ) >= 0 );
    const auto cargo_index = veh->install_part( point_zero, cargo_part_id, true );
    REQUIRE( cargo_index >= 0 );
    REQUIRE( veh->install_part( point_zero, battery_part_id, true ) >= 0 );
    veh->charge_battery( 10000 );

    vehicle_part &turret_part = veh->part( turret_index );
    item &gun = turret_part.get_base();
    REQUIRE( gun.magazine_integral() );
    const auto ammo_capacity = gun.ammo_capacity();
    REQUIRE( ammo_capacity > 1 );

    const auto ammo_id = itype_id( "rebar_rail" );
    gun.ammo_set( ammo_id, 0 );
    const auto ammo_stack = ammo_capacity * 2;
    auto remaining = veh->add_item( cargo_index, item::spawn( ammo_id, calendar::turn, ammo_stack ) );
    REQUIRE( !remaining );

    auto current_ammo = gun.ammo_remaining();
    auto last_ammo = current_ammo;
    auto tries = 0;
    const auto max_tries = ammo_capacity * 2;
    while( current_ammo < ammo_capacity && tries < max_tries ) {
        calendar::turn += 1_minutes;
        vehicle_funcs::try_autoload_turret( *veh, turret_part );
        current_ammo = gun.ammo_remaining();
        if( current_ammo > last_ammo ) {
            last_ammo = current_ammo;
            tries = 0;
        } else {
            ++tries;
        }
    }
    REQUIRE( gun.ammo_remaining() == ammo_capacity );
}
