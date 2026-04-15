#include "catch/catch.hpp"

#include "avatar.h"
#include "calendar.h"
#include "flag.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units_volume.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

namespace
{

static const itype_id fuel_type_battery( "battery" );
static const itype_id benchmark_cargo_item( "RAM" );

struct cargo_benchmark_options {
    int cargo_item_count;
    bool include_rechargeable_item = false;
};

struct cargo_benchmark_fixture {
    avatar *crafter = nullptr;
    vehicle *veh = nullptr;
    item *rechargeable_item = nullptr;
};

auto cargo_part_indices( vehicle &veh ) -> std::vector<int>
{
    auto parts = std::vector<int>();
    for( const vpart_reference &vp : veh.get_any_parts( "CARGO" ) ) {
        const auto free_ml = units::to_milliliter<int>( veh.free_volume( vp.part_index() ) );
        if( free_ml >= 50000 ) {
            parts.push_back( vp.part_index() );
        }
    }
    return parts;
}

auto fill_vehicle_cargo( vehicle &veh, const cargo_benchmark_options &opts ) -> item *
{
    const auto cargo_parts = cargo_part_indices( veh );
    REQUIRE( !cargo_parts.empty() );

    for( const auto part : cargo_parts ) {
        veh.get_items( part ).clear();
    }

    for( int i = 0; i < opts.cargo_item_count; ++i ) {
        const auto part = cargo_parts[ i % cargo_parts.size() ];
        detached_ptr<item> det = item::spawn( benchmark_cargo_item );
        REQUIRE( !veh.add_item( part, std::move( det ) ) );
    }

    if( !opts.include_rechargeable_item ) {
        return nullptr;
    }

    const auto recharge_part = cargo_parts.front();
    detached_ptr<item> det = item::spawn( itype_id( "light_battery_cell" ) );
    item *const rechargeable = &*det;
    rechargeable->ammo_unset();
    REQUIRE( rechargeable->has_flag( flag_RECHARGE ) );
    REQUIRE( !veh.add_item( recharge_part, std::move( det ) ) );
    return rechargeable;
}

auto make_cargo_benchmark_fixture( const cargo_benchmark_options &opts ) -> cargo_benchmark_fixture
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );

    auto &u = get_avatar();
    u.setpos( tripoint( 10, 10, 0 ) );

    vehicle *const veh = get_map().add_vehicle( vproto_id( "aapc-mg" ), u.pos(), 0_degrees, 100,
                         0 );
    REQUIRE( veh != nullptr );
    veh->update_time( calendar::turn_zero );

    auto has_recharger = false;
    for( const vpart_reference &vp : veh->get_any_parts( "RECHARGE" ) ) {
        vp.part().enabled = true;
        has_recharger = true;
    }
    REQUIRE( has_recharger );

    return cargo_benchmark_fixture{
        .crafter = &u,
        .veh = veh,
        .rechargeable_item = fill_vehicle_cargo( *veh, opts ),
    };
}

auto refill_vehicle_battery( vehicle &veh ) -> void
{
    const auto missing = veh.fuel_capacity( fuel_type_battery ) - veh.fuel_left( fuel_type_battery );
    if( missing > 0 ) {
        veh.charge_battery( missing );
    }
}

} // namespace

TEST_CASE( "crafting inventory rebuild benchmark near cargo-heavy vehicle",
           "[.][benchmark][crafting][vehicle]" )
{
    static constexpr auto cargo_item_count = 8196;
    const auto fixture = make_cargo_benchmark_fixture( cargo_benchmark_options{
        .cargo_item_count = cargo_item_count,
    } );

    BENCHMARK_ADVANCED( "crafting_inventory rebuild with 8196 vehicle cargo items" )
    ( Catch::Benchmark::Chronometer meter ) {
        meter.measure( [&fixture]() {
            fixture.crafter->invalidate_crafting_inventory();
            return fixture.crafter->crafting_inventory().size();
        } );
    };
}

TEST_CASE( "vehicle cargo recharge benchmark in item-heavy area",
           "[.][benchmark][vehicle][process_items]" )
{
    static constexpr auto cargo_item_count = 8196;
    const auto fixture = make_cargo_benchmark_fixture( cargo_benchmark_options{
        .cargo_item_count = cargo_item_count,
        .include_rechargeable_item = true,
    } );
    REQUIRE( fixture.rechargeable_item != nullptr );

    BENCHMARK_ADVANCED( "process_items with recharge station and 8196 cargo items" )
    ( Catch::Benchmark::Chronometer meter ) {
        meter.measure( [&fixture]() {
            fixture.rechargeable_item->ammo_unset();
            refill_vehicle_battery( *fixture.veh );
            get_map().process_items();
            return fixture.rechargeable_item->ammo_remaining();
        } );
    };
}
