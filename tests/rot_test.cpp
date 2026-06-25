#include "catch/catch.hpp"

#include <memory>
#include <ranges>

#include "avatar.h"
#include "calendar.h"
#include "coordinates.h"
#include "crafting.h"
#include "enums.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "game.h" // Just for get_convection_temperature(), TODO: Remove
#include "requirements.h"
#include "rot.h"
#include "state_helpers.h"
#include "units_temperature.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "weather.h"

static const furn_str_id f_atomic_freezer( "f_atomic_freezer" );
static const furn_str_id f_test_fridge_on( "f_fridge_on" );
static const furn_str_id f_test_minifreezer_on( "f_minifreezer_on" );

static void set_map_temperature( weather_manager &weather, units::temperature new_temperature )
{
    weather.temperature = new_temperature;
    weather.clear_temp_cache();
}

static void ensure_no_temperature_mods( tripoint_bub_ms location )
{
    REQUIRE( get_heat_radiation( location, false ) == 0 );
    REQUIRE( get_convection_temperature( location ) == 0 );
    REQUIRE( get_map().get_temperature( location ) == 0 );
}

struct vehicle_storage_fixture {
    vehicle *veh = nullptr;
    int part_index = -1;
    tripoint_bub_ms pos;
};

static auto make_storage( const vpart_id &storage_part,
                          const bool enabled ) -> vehicle_storage_fixture
{
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    set_map_temperature( get_weather(), 18_c );

    auto &here = get_map();
    const auto vehicle_pos = tripoint_bub_ms( 60, 60, 0 );
    here.set_temperature( vehicle_pos, 100 );
    auto *veh = here.add_vehicle( vproto_id( "none" ), vehicle_pos, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );
    REQUIRE( veh->install_part( tripoint_mnt_veh::zero(), vpart_id( "frame_vertical" ), true ) >= 0 );
    const auto part_index = veh->install_part( tripoint_mnt_veh::zero(), storage_part, true );
    REQUIRE( part_index >= 0 );
    veh->part( part_index ).enabled = enabled;
    here.add_vehicle_to_cache( veh );
    here.build_map_cache( vehicle_pos.z(), true );

    return { .veh = veh, .part_index = part_index, .pos = vehicle_pos };
}

static auto add_food_to_vehicle_part( vehicle &veh, const int part_index,
                                      const itype_id &food_type ) -> void
{
    auto food = item::spawn( food_type );
    REQUIRE( food->goes_bad() );
    REQUIRE_FALSE( veh.add_item( part_index, std::move( food ) ) );
}

static auto add_sashimi_to_vehicle_part( vehicle &veh, const int part_index ) -> void
{
    add_food_to_vehicle_part( veh, part_index, itype_id( "sashimi" ) );
}

static auto make_backpack_with_sashimi() -> detached_ptr<item>
{
    auto backpack = item::spawn( "backpack" );
    backpack->put_in( item::spawn( "rock" ) );
    backpack->put_in( item::spawn( "sashimi" ) );
    REQUIRE( backpack->is_food_container() );
    return backpack;
}

static auto add_backpack_with_sashimi_to_vehicle_part( vehicle &veh, const int part_index ) -> void
{
    REQUIRE_FALSE( veh.add_item( part_index, make_backpack_with_sashimi() ) );
}

static auto nested_sashimi_in( item &container ) -> item *; // *NOPAD*

static auto make_plastic_bag_with_sashimi() -> detached_ptr<item>
{
    auto bag = item::spawn( "bag_plastic" );
    REQUIRE( bag->is_container() );
    bag->put_in( item::spawn( "sashimi" ) );
    return bag;
}

static auto make_sealed_carton_with_rotten_nested_sashimi() -> detached_ptr<item>
{
    auto carton = item::spawn( "carton_unsealed" );
    REQUIRE( carton->is_container() );
    REQUIRE( carton->type->container->seals );
    REQUIRE_FALSE( carton->type->container->preserves );
    carton->put_in( make_plastic_bag_with_sashimi() );
    item *sashimi = nested_sashimi_in( *carton );
    sashimi->set_relative_rot( 3.0 );
    REQUIRE( sashimi->has_rotten_away() );
    return carton;
}

static auto nested_sashimi_in( item &container ) -> item * // *NOPAD*
{
    namespace ranges = std::ranges;
    using namespace std::views;
    const auto nested_food = container.contents.all_items_ptr()
    | filter( []( const item * it ) { return it->typeId() == itype_id( "sashimi" ); } )
    | ranges::to<std::vector>();
    REQUIRE( nested_food.size() == 1 );
    return nested_food.front();
}

TEST_CASE( "Food lookup finds nested food after non-food contents", "[item][food]" )
{
    SECTION( "top-level food after non-food content" ) {
        auto backpack = item::spawn( "backpack" );
        backpack->put_in( item::spawn( "rock" ) );
        backpack->put_in( item::spawn( "sashimi" ) );

        REQUIRE( backpack->is_food_container() );
        auto *food = backpack->get_food();
        REQUIRE( food != nullptr );
        CHECK( food->typeId() == itype_id( "sashimi" ) );
    }

    SECTION( "nested food after non-food content" ) {
        auto outer_bag = item::spawn( "bag_canvas" );
        auto inner_bag = item::spawn( "bag_plastic" );
        inner_bag->put_in( item::spawn( "rock" ) );
        inner_bag->put_in( item::spawn( "sashimi" ) );
        outer_bag->put_in( std::move( inner_bag ) );

        REQUIRE( outer_bag->is_food_container() );
        const auto &const_outer_bag = *outer_bag;
        const auto *food = const_outer_bag.get_food();
        REQUIRE( food != nullptr );
        CHECK( food->typeId() == itype_id( "sashimi" ) );
    }
}

static auto add_bread_to_vehicle_part( vehicle &veh, const int part_index ) -> void
{
    add_food_to_vehicle_part( veh, part_index, itype_id( "bread" ) );
}

static auto add_canned_red_sauce_to_vehicle_part( vehicle &veh, const int part_index ) -> void
{
    auto sauce = item::in_its_container( item::spawn( "sauce_red" ) );
    REQUIRE( sauce->goes_bad_after_opening( true ) );
    REQUIRE_FALSE( veh.add_item( part_index, std::move( sauce ) ) );
}

static auto complete_recipe_from_components( Character &crafter, const recipe_id &recipe_to_make,
        std::vector<detached_ptr<item>> components ) -> void
{
    auto craft = item::spawn( &recipe_to_make.obj(), 1, std::move( components ),
                              std::vector<item_comp> {} );
    complete_craft( crafter, *craft );
}

static auto completed_items( Character &who, const itype_id &type ) -> std::vector<item *>
{
    return who.items_with( [&type]( const item & it ) { return it.typeId() == type; } );
}

static auto move_to_inventory_with_attempt_detach( item &stored ) -> item * // *NOPAD*
{
    item *carried = nullptr;
    stored.attempt_detach( [&carried]( detached_ptr<item> &&it ) {
        carried = &get_avatar().i_add( std::move( it ) );
        return detached_ptr<item>();
    } );
    return carried;
}

static auto process_storage_for( const time_duration duration ) -> void
{
    constexpr auto interval = 20_minutes;
    const auto intervals = to_turns<int>( duration ) / to_turns<int>( interval );
    for( const auto _ : std::views::iota( 0, intervals ) ) {
        static_cast<void>( _ );
        calendar::turn += interval;
        get_map().process_items();
    }
}

static auto prepare_map_storage_test() -> void
{
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    set_map_temperature( get_weather(), 18_c );
}

static auto add_food_to_map( const tripoint_bub_ms &pos, const itype_id &food_type ) -> void
{
    auto food = item::spawn( food_type );
    REQUIRE( food->goes_bad() );
    get_map().add_item( pos, std::move( food ) );
    REQUIRE( get_map().i_at( pos ).size() == 1 );
}

static auto add_sashimi_to_map( const tripoint_bub_ms &pos ) -> void
{
    add_food_to_map( pos, itype_id( "sashimi" ) );
}

static auto add_backpack_with_sashimi_to_map( const tripoint_bub_ms &pos ) -> void
{
    get_map().add_item( pos, make_backpack_with_sashimi() );
    REQUIRE( get_map().i_at( pos ).size() == 1 );
}

TEST_CASE( "Rate of rotting" )
{
    SECTION( "Passage of time" ) {
        weather_manager weather;
        // Item rot is a time duration.
        // At 65 F (18,3 C) item rots at rate of 1h/1h
        // So the level of rot should be about same as the item age
        // In preserving containers and in freezer the item should not rot at all

        // Items created at turn zero are handled differently, so ensure we're
        // not there.
        if( calendar::turn <= calendar::start_of_cataclysm ) {
            calendar::turn = calendar::start_of_cataclysm + 1_minutes;
        }

        detached_ptr<item> normal_item = item::spawn( "meat_cooked" );
        detached_ptr<item> freeze_item = item::spawn( "offal_canned" );
        detached_ptr<item> sealed_item = item::in_its_container( item::spawn( "offal_canned" ) );

        set_map_temperature( weather, 18_c );
        ensure_no_temperature_mods( tripoint_bub_ms::zero() );
        REQUIRE( weather.get_temperature( tripoint_abs_ms::zero() ) == 18_c );

        normal_item = item::process( std::move( normal_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_NORMAL, weather );
        sealed_item = item::process( std::move( sealed_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_NORMAL, weather );
        freeze_item = item::process( std::move( freeze_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_NORMAL, weather );

        // Item should exist with no rot when it is brand new
        CHECK( normal_item->get_rot() == 0_turns );
        CHECK( sealed_item->get_rot() == 0_turns );
        CHECK( freeze_item->get_rot() == 0_turns );

        INFO( "Initial turn: " << to_turn<int>( calendar::turn ) );

        calendar::turn += 20_minutes;
        normal_item = item::process( std::move( normal_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_NORMAL, weather );
        sealed_item = item::process( std::move( sealed_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_NORMAL, weather );
        freeze_item = item::process( std::move( freeze_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_FREEZER, weather );

        // After 20 minutes the normal item should have 20 minutes of rot
        CHECK( to_turns<int>( normal_item->get_rot() )
               == Approx( to_turns<int>( 20_minutes ) ).epsilon( 0.01 ) );
        // Item in freezer and in preserving container should have no rot
        CHECK( sealed_item->get_rot() == 0_turns );
        CHECK( freeze_item->get_rot() == 0_turns );

        // Move time 110 minutes
        calendar::turn += 110_minutes;
        // TODO: Check >1 hour normal processing as well - can't be "simply done" because of weather globals
        sealed_item = item::process( std::move( sealed_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_NORMAL, weather );
        freeze_item = item::process( std::move( freeze_item ), nullptr, tripoint_bub_ms::zero(), false,
                                     temperature_flag::TEMP_FREEZER, weather );
        // In freezer and in preserving container still should be no rot
        CHECK( sealed_item->get_rot() == 0_turns );
        CHECK( freeze_item->get_rot() == 0_turns );
    }
}

TEST_CASE( "Preserving containers stop contained food rot" )
{
    SECTION( "direct rot queries do not age food in a sealed can" ) {
        prepare_map_storage_test();

        auto sealed_can = item::in_its_container( item::spawn( "offal_canned" ) );
        REQUIRE( sealed_can->goes_bad_after_opening( true ) );
        REQUIRE( !sealed_can->contents.empty() );

        item &food = sealed_can->contents.front();
        REQUIRE( food.goes_bad() );

        calendar::turn += 20_days;

        CHECK( food.get_rot() == 0_turns );
        CHECK( !food.rotten() );
    }

    SECTION( "removed food starts rotting when opened" ) {
        prepare_map_storage_test();

        auto sealed_jar = item::in_container( itype_id( "jar_glass_sealed" ),
                                              item::spawn( "meat_cooked" ) );
        REQUIRE( sealed_jar->goes_bad_after_opening( true ) );
        REQUIRE( !sealed_jar->contents.empty() );

        calendar::turn += 20_days;

        auto removed = detached_ptr<item>();
        sealed_jar->contents.front().attempt_detach( [&removed]( detached_ptr<item> &&it ) {
            removed = std::move( it );
            return detached_ptr<item>();
        } );

        REQUIRE( removed );
        REQUIRE( removed->goes_bad() );
        CHECK( removed->get_rot() == 0_turns );

        calendar::turn += 20_minutes;

        CHECK( removed->get_rot() > 0_turns );
    }

    SECTION( "directly removed food starts fresh when opened" ) {
        prepare_map_storage_test();

        auto sealed_jar = item::in_container( itype_id( "jar_glass_sealed" ),
                                              item::spawn( "meat_cooked" ) );
        item &food = sealed_jar->contents.front();

        calendar::turn += 20_days;

        auto removed = sealed_jar->contents.remove_top( &food );

        REQUIRE( removed );
        CHECK( removed->get_rot() == 0_turns );
    }

    SECTION( "filtered removed food starts fresh when opened" ) {
        prepare_map_storage_test();

        auto sealed_jar = item::in_container( itype_id( "jar_glass_sealed" ),
                                              item::spawn( "meat_cooked" ) );

        calendar::turn += 20_days;

        auto removed = detached_ptr<item>();
        sealed_jar->contents.remove_top_items_with( [&removed]( detached_ptr<item> &&it ) {
            removed = std::move( it );
            return detached_ptr<item>();
        } );

        REQUIRE( removed );
        CHECK( removed->get_rot() == 0_turns );
    }

    SECTION( "cleared preserved food starts fresh when opened" ) {
        prepare_map_storage_test();

        auto sealed_jar = item::in_container( itype_id( "jar_glass_sealed" ),
                                              item::spawn( "meat_cooked" ) );

        calendar::turn += 20_days;

        auto removed = sealed_jar->contents.clear_items();

        REQUIRE( removed.size() == 1 );
        CHECK( removed.front()->get_rot() == 0_turns );
    }

    SECTION( "split preserved charges start fresh when opened" ) {
        prepare_map_storage_test();

        auto sealed_can = item::in_its_container( item::spawn( "sauce_red" ) );
        item &food = sealed_can->contents.front();
        REQUIRE( food.count_by_charges() );

        calendar::turn += 20_days;

        auto removed = detached_ptr<item>();
        REQUIRE( food.attempt_split( 3, [&removed]( detached_ptr<item> &&it ) {
            removed = std::move( it );
            return detached_ptr<item>();
        } ) );

        REQUIRE( removed );
        CHECK( removed->charges == 3 );
        CHECK( removed->get_rot() == 0_turns );
    }

    SECTION( "sealed outer container keeps nested rotten food from vanishing" ) {
        prepare_map_storage_test();

        auto outer = item::spawn( "bag_canvas" );
        auto inner = item::spawn( "bag_plastic" );
        inner->put_in( item::spawn( "sashimi" ) );
        outer->put_in( std::move( inner ) );
        REQUIRE( outer->needs_processing() );

        calendar::turn += 25_hours;
        outer = item::process( std::move( outer ), nullptr, tripoint_bub_ms::zero(), false,
                               temperature_flag::TEMP_NORMAL, get_weather() );

        namespace ranges = std::ranges;
        using namespace std::views;
        auto nested_food = outer->contents.all_items_ptr()
        | filter( []( const item * it ) { return it->typeId() == itype_id( "sashimi" ); } )
        | ranges::to<std::vector>();
        REQUIRE( nested_food.size() == 1 );
        CHECK( nested_food.front()->get_rot() > 0_turns );
    }
}

TEST_CASE( "Items rot away" )
{
    SECTION( "Item in reality bubble rots away" ) {
        weather_manager weather;
        // Item should rot away when it has 2x of its shelf life in rot.

        if( calendar::turn <= calendar::start_of_cataclysm ) {
            calendar::turn = calendar::start_of_cataclysm + 1_minutes;
        }

        detached_ptr<item> test_item = item::spawn( "meat_cooked" );

        // Process item once to set all of its values.
        test_item = item::process( std::move( test_item ), nullptr, tripoint_bub_ms::zero(), false,
                                   temperature_flag::TEMP_HEATER, weather );

        // Set rot to >2 days and process again. process_rot should destroy the item.
        calendar::turn += 20_minutes;
        test_item->mod_rot( 4_days );
        test_item = item::process_rot( std::move( test_item ), false, tripoint_bub_ms::zero(), nullptr,
                                       temperature_flag::TEMP_HEATER, weather );
        CHECK( !test_item );
    }

    SECTION( "Item on map rots away" ) {
        weather_manager weather;
        const tripoint_bub_ms loc;

        if( calendar::turn <= calendar::start_of_cataclysm ) {
            calendar::turn = calendar::start_of_cataclysm + 1_minutes;
        }

        detached_ptr<item> test_item = item::process( item::spawn( "meat_cooked" ), nullptr,
                                       tripoint_bub_ms::zero(),
                                       false, temperature_flag::TEMP_HEATER, weather );
        map &m = get_map();
        m.add_item_or_charges( loc, std::move( test_item ), false );

        REQUIRE( m.i_at( loc ).size() == 1 );

        calendar::turn += 20_minutes;
        m.i_at( loc ).only_item().mod_rot( 7_days );
        m.process_items();

        CHECK( m.i_at( loc ).empty() );
    }
}

TEST_CASE( "Items don't rot away on map load if in a freezer" )
{
    map m( 2 );
    weather_manager weather;
    if( calendar::turn <= calendar::start_of_cataclysm ) {
        calendar::turn = calendar::start_of_cataclysm + 1_minutes;
    }

    constexpr tripoint_abs_sm non_tested_location = tripoint_abs_sm( 0, 0, 0 );
    constexpr tripoint_abs_sm test_location = tripoint_abs_sm( 100, 100, 0 );
    m.load( test_location.xy(), false );

    const tripoint_bub_ms freezer_pnt = {13, 13, 0};
    const tripoint_bub_ms powered_freezer_pnt = {14, 13, 0};
    const tripoint_bub_ms sealed_pnt = {15, 13, 0};
    const tripoint_bub_ms normal_pnt = {16, 13, 0};
    m.furn_set( freezer_pnt, f_atomic_freezer );
    m.furn_set( powered_freezer_pnt, f_test_minifreezer_on );
    m.furn_set( sealed_pnt, furn_str_id::NULL_ID() );
    m.furn_set( normal_pnt, furn_str_id::NULL_ID() );
    m.ter_set( freezer_pnt, t_grass );
    m.ter_set( powered_freezer_pnt, t_grass );
    m.ter_set( sealed_pnt, t_grass );
    m.ter_set( normal_pnt, t_grass );
    REQUIRE( m.has_flag_furn( TFLAG_FREEZER, freezer_pnt ) );
    REQUIRE( m.has_flag_furn( TFLAG_FREEZER, powered_freezer_pnt ) );

    detached_ptr<item> normal_item_d = item::spawn( "meat_cooked" );
    item &normal_item = *normal_item_d;
    detached_ptr<item> freeze_item_d = item::spawn( "offal_canned" );
    item &freeze_item = *freeze_item_d;
    auto powered_freeze_item_d = item::spawn( "meat_cooked" );
    item &powered_freeze_item = *powered_freeze_item_d;
    detached_ptr<item> sealed_item_d = item::in_its_container( item::spawn( "offal_canned" ) );
    item &sealed_item = *sealed_item_d;

    set_map_temperature( weather, 18_c );

    m.i_clear( freezer_pnt );
    m.i_clear( powered_freezer_pnt );
    m.i_clear( sealed_pnt );
    m.i_clear( normal_pnt );

    m.add_item( freezer_pnt, std::move( freeze_item_d ) );
    m.add_item( powered_freezer_pnt, std::move( powered_freeze_item_d ) );
    m.add_item( sealed_pnt, std::move( sealed_item_d ) );
    m.add_item( normal_pnt, std::move( normal_item_d ) );

    REQUIRE( normal_item.get_rot() == 0_turns );
    REQUIRE( sealed_item.get_rot() == 0_turns );
    REQUIRE( freeze_item.get_rot() == 0_turns );
    REQUIRE( powered_freeze_item.get_rot() == 0_turns );

    auto freezer_stack = m.i_at( freezer_pnt );
    REQUIRE( freezer_stack.size() == 1 );
    auto powered_freezer_stack = m.i_at( powered_freezer_pnt );
    REQUIRE( powered_freezer_stack.size() == 1 );
    auto sealed_stack = m.i_at( sealed_pnt );
    REQUIRE( sealed_stack.size() == 1 );
    auto normal_stack = m.i_at( normal_pnt );
    REQUIRE( normal_stack.size() == 1 );

    INFO( "Initial turn: " << to_turn<int>( calendar::turn ) );

    // Change the date outside the location, to force @ref map::actualize to proc rot
    m.load( non_tested_location.xy(), false );
    calendar::turn += 365_days;
    m.load( test_location.xy(), false );

    CHECK( m.has_flag_furn( TFLAG_FREEZER, freezer_pnt ) );
    CHECK( m.has_flag_furn( TFLAG_FREEZER, powered_freezer_pnt ) );
    auto freezer_stack_after = m.i_at( freezer_pnt );
    REQUIRE( freezer_stack_after.size() == 1 );
    auto powered_freezer_stack_after = m.i_at( powered_freezer_pnt );
    REQUIRE( powered_freezer_stack_after.size() == 1 );
    CHECK( powered_freezer_stack_after.only_item().get_rot() == 0_turns );
    CHECK( !powered_freezer_stack_after.only_item().rotten() );
    auto sealed_stack_after = m.i_at( sealed_pnt );
    REQUIRE( sealed_stack_after.size() == 1 );
    auto normal_stack_after = m.i_at( normal_pnt );
    REQUIRE( normal_stack_after.empty() );
}

TEST_CASE( "Vehicle storage temperature controls food rot" )
{
    SECTION( "powered freezers preserve food when removed after missed processing" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), true );
        add_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        auto freezer_items = fixture.veh->get_items( fixture.part_index );
        REQUIRE( freezer_items.size() == 1 );

        calendar::turn += 21_days;
        auto *carried = move_to_inventory_with_attempt_detach( freezer_items.only_item() );

        REQUIRE( carried != nullptr );
        CHECK( carried->get_rot() == 0_turns );

        get_avatar().process_items();

        CHECK( !carried->rotten() );
        CHECK( carried->get_rot() == 0_turns );
    }

    SECTION( "powered freezers preserve nested food when removed after missed processing" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), true );
        add_backpack_with_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        auto freezer_items = fixture.veh->get_items( fixture.part_index );
        REQUIRE( freezer_items.size() == 1 );

        calendar::turn += 21_days;
        auto *carried = move_to_inventory_with_attempt_detach( freezer_items.only_item() );
        REQUIRE( carried != nullptr );
        auto *food = nested_sashimi_in( *carried );

        CHECK( food->get_rot() == 0_turns );
        CHECK( !food->rotten() );
    }

    SECTION( "powered fridges catch up partial rot when removed after missed processing" ) {
        auto fixture = make_storage( vpart_id( "minifridge" ), true );
        add_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        auto fridge_items = fixture.veh->get_items( fixture.part_index );
        REQUIRE( fridge_items.size() == 1 );

        calendar::turn += 24_hours;
        auto *carried = move_to_inventory_with_attempt_detach( fridge_items.only_item() );

        REQUIRE( carried != nullptr );
        CHECK( carried->get_relative_rot() > 0.0 );
        CHECK( carried->get_relative_rot() < 1.0 );

        const auto rot_after_detach = carried->get_rot();
        get_avatar().process_items();

        CHECK( carried->get_rot() == rot_after_detach );
    }

    SECTION( "unpowered freezers do not protect food while it remains stored" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), false );
        add_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        process_storage_for( 25_hours );

        CHECK( fixture.veh->get_items( fixture.part_index ).empty() );
    }

    SECTION( "vehicle seat storage rots food while it remains stored" ) {
        auto fixture = make_storage( vpart_id( "seat" ), true );
        add_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        process_storage_for( 25_hours );

        CHECK( fixture.veh->get_items( fixture.part_index ).empty() );
    }

    SECTION( "preserved food in vehicle cargo is fresh when opened for crafting" ) {
        auto fixture = make_storage( vpart_id( "box" ), true );
        add_canned_red_sauce_to_vehicle_part( *fixture.veh, fixture.part_index );

        calendar::turn += 20_days;

        auto cargo = get_map().veh_at( fixture.pos ).part_with_feature( "CARGO", true );
        REQUIRE( cargo.has_value() );
        REQUIRE( static_cast<int>( cargo->part_index() ) == fixture.part_index );

        auto quantity = 1;
        auto components = get_map().use_amount( fixture.pos, PICKUP_RANGE, itype_id( "sauce_red" ),
                                                quantity, return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->get_rot() == 0_turns );
        CHECK( !components.front()->rotten() );
    }

    SECTION( "powered freezer cargo protects food after non-food container contents" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), true );
        auto backpack = item::spawn( "backpack" );
        backpack->put_in( item::spawn( "rock" ) );
        backpack->put_in( item::spawn( "sashimi" ) );
        REQUIRE( backpack->is_food_container() );
        REQUIRE_FALSE( fixture.veh->add_item( fixture.part_index, std::move( backpack ) ) );

        process_storage_for( 25_hours );

        auto remaining = fixture.veh->get_items( fixture.part_index );
        REQUIRE( remaining.size() == 1 );
        namespace ranges = std::ranges;
        using namespace std::views;
        auto nested_food = remaining.only_item().contents.all_items_ptr()
        | filter( []( const item * it ) { return it->typeId() == itype_id( "sashimi" ); } )
        | ranges::to<std::vector>();
        REQUIRE( nested_food.size() == 1 );
        CHECK( nested_food.front()->get_rot() == 0_turns );
        CHECK( !nested_food.front()->rotten() );
    }

    SECTION( "powered freezer cargo keeps whole food fresh when consumed for crafting" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), true );
        add_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        calendar::turn += 21_days;

        auto quantity = 1;
        auto components = get_map().use_amount( fixture.pos, PICKUP_RANGE, itype_id( "sashimi" ),
                                                quantity, return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->get_rot() == 0_turns );
        CHECK( !components.front()->rotten() );
    }

    SECTION( "powered freezer cargo keeps crafted food fresh from frozen ingredients" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), true );
        add_food_to_vehicle_part( *fixture.veh, fixture.part_index, itype_id( "meat" ) );

        calendar::turn += 21_days;

        auto quantity = 1;
        auto components = get_map().use_amount( fixture.pos, PICKUP_RANGE, itype_id( "meat" ),
                                                quantity, return_true<item> );
        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );

        auto &avatar = get_avatar();
        complete_recipe_from_components( avatar, recipe_id( "meat_cooked" ), std::move( components ) );

        const auto crafted = completed_items( avatar, itype_id( "meat_cooked" ) );
        REQUIRE( crafted.size() == 1 );
        CHECK( crafted.front()->get_rot() == 0_turns );
        CHECK( !crafted.front()->rotten() );
    }

    SECTION( "powered fridge cargo catches up whole food rot when consumed for crafting" ) {
        auto fixture = make_storage( vpart_id( "minifridge" ), true );
        add_sashimi_to_vehicle_part( *fixture.veh, fixture.part_index );

        calendar::turn += 24_hours;

        auto quantity = 1;
        auto components = get_map().use_amount( fixture.pos, PICKUP_RANGE, itype_id( "sashimi" ),
                                                quantity, return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->get_relative_rot() > 0.0 );
        CHECK( components.front()->get_relative_rot() < 1.0 );
    }

    SECTION( "powered freezer cargo keeps charge food fresh when consumed for crafting" ) {
        auto fixture = make_storage( vpart_id( "minifreezer" ), true );
        add_bread_to_vehicle_part( *fixture.veh, fixture.part_index );

        calendar::turn += 20_days;

        auto quantity = 1;
        auto components = get_map().use_charges( fixture.pos, 0, itype_id( "bread" ), quantity,
                          return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->charges == 1 );
        CHECK( components.front()->get_rot() == 0_turns );
        CHECK( !components.front()->rotten() );

        auto remaining = fixture.veh->get_items( fixture.part_index );
        REQUIRE( remaining.size() == 1 );
        CHECK( remaining.only_item().charges == 1 );
        CHECK( remaining.only_item().get_rot() == 0_turns );
    }

    SECTION( "powered fridge cargo catches up charge food rot when consumed for crafting" ) {
        auto fixture = make_storage( vpart_id( "minifridge" ), true );
        add_bread_to_vehicle_part( *fixture.veh, fixture.part_index );

        calendar::turn += 24_hours;

        auto quantity = 1;
        auto components = get_map().use_charges( fixture.pos, 0, itype_id( "bread" ), quantity,
                          return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->charges == 1 );
        CHECK( components.front()->get_relative_rot() > 0.0 );
        CHECK( components.front()->get_relative_rot() < 1.0 );

        auto remaining = fixture.veh->get_items( fixture.part_index );
        REQUIRE( remaining.size() == 1 );
        CHECK( remaining.only_item().charges == 1 );
        CHECK( remaining.only_item().get_relative_rot() > 0.0 );
        CHECK( remaining.only_item().get_relative_rot() < 1.0 );
    }
}

TEST_CASE( "Contained item keeps parent location while temporarily detached" )
{
    prepare_map_storage_test();

    auto container = item::spawn( "bag_plastic" );
    REQUIRE( container->is_container() );
    REQUIRE( container->contents.insert_item( item::spawn( "sashimi" ) ).success() );

    auto checked = false;
    container->contents.remove_top_items_with( [&]( detached_ptr<item> &&it ) {
        checked = true;
        CHECK( it->parent_item() == &*container );
        CHECK( rot::temp::for_location( get_map(), *it ) == temperature_flag::TEMP_NORMAL );
        return std::move( it );
    } );

    CHECK( checked );
}

TEST_CASE( "Sealed containers keep rotten nested contents on location removal", "[item][rot]" )
{
    prepare_map_storage_test();
    const auto pos = tripoint_bub_ms( 60, 60, 0 );
    get_map().set_temperature( pos, 100 );
    get_map().add_item( pos, make_sealed_carton_with_rotten_nested_sashimi() );

    auto items = get_map().i_at( pos );
    REQUIRE( items.size() == 1 );
    auto *carried = move_to_inventory_with_attempt_detach( items.only_item() );
    REQUIRE( carried != nullptr );
    auto *food = nested_sashimi_in( *carried );

    CHECK( food->rotten() );
    CHECK( food->has_rotten_away() );
}

TEST_CASE( "Map powered fridge and freezer furniture controls food rot" )
{
    SECTION( "powered freezer furniture preserves food" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_minifreezer_on );
        add_sashimi_to_map( pos );

        process_storage_for( 25_hours );

        auto items = get_map().i_at( pos );
        REQUIRE( items.size() == 1 );
        CHECK( items.only_item().get_rot() == 0_turns );
        CHECK( !items.only_item().rotten() );
    }

    SECTION( "powered fridge furniture partially protects food" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_fridge_on );
        add_sashimi_to_map( pos );

        process_storage_for( 24_hours );

        auto items = get_map().i_at( pos );
        REQUIRE( items.size() == 1 );
        CHECK( items.only_item().get_relative_rot() > 0.0 );
        CHECK( items.only_item().get_relative_rot() < 1.0 );
    }

    SECTION( "powered freezer furniture keeps food fresh when removed after missed processing" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_minifreezer_on );
        add_sashimi_to_map( pos );

        calendar::turn += 21_days;

        auto items = get_map().i_at( pos );
        REQUIRE( items.size() == 1 );
        auto *carried = move_to_inventory_with_attempt_detach( items.only_item() );

        REQUIRE( carried != nullptr );
        CHECK( carried->get_rot() == 0_turns );
        CHECK( !carried->rotten() );
    }

    SECTION( "powered freezer furniture keeps nested food fresh after missed processing" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_minifreezer_on );
        add_backpack_with_sashimi_to_map( pos );

        calendar::turn += 21_days;

        auto items = get_map().i_at( pos );
        REQUIRE( items.size() == 1 );
        auto *carried = move_to_inventory_with_attempt_detach( items.only_item() );
        REQUIRE( carried != nullptr );
        auto *food = nested_sashimi_in( *carried );

        CHECK( food->get_rot() == 0_turns );
        CHECK( !food->rotten() );
    }

    SECTION( "powered freezer furniture keeps whole food fresh when consumed for crafting" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_minifreezer_on );
        add_sashimi_to_map( pos );

        calendar::turn += 21_days;

        auto quantity = 1;
        auto components = get_map().use_amount( pos, PICKUP_RANGE, itype_id( "sashimi" ), quantity,
                                                return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->get_rot() == 0_turns );
        CHECK( !components.front()->rotten() );
    }

    SECTION( "powered fridge furniture catches up whole food rot when consumed for crafting" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_fridge_on );
        add_sashimi_to_map( pos );

        calendar::turn += 24_hours;

        auto quantity = 1;
        auto components = get_map().use_amount( pos, PICKUP_RANGE, itype_id( "sashimi" ), quantity,
                                                return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->get_relative_rot() > 0.0 );
        CHECK( components.front()->get_relative_rot() < 1.0 );
    }

    SECTION( "powered freezer furniture keeps charge food fresh when consumed for crafting" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_minifreezer_on );
        add_food_to_map( pos, itype_id( "bread" ) );

        calendar::turn += 20_days;

        auto quantity = 1;
        auto components = get_map().use_charges( pos, 0, itype_id( "bread" ), quantity,
                          return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->charges == 1 );
        CHECK( components.front()->get_rot() == 0_turns );
        CHECK( !components.front()->rotten() );

        auto remaining = get_map().i_at( pos );
        REQUIRE( remaining.size() == 1 );
        CHECK( remaining.only_item().charges == 1 );
        CHECK( remaining.only_item().get_rot() == 0_turns );
    }

    SECTION( "powered fridge furniture catches up charge food rot when consumed for crafting" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        get_map().furn_set( pos, f_test_fridge_on );
        add_food_to_map( pos, itype_id( "bread" ) );

        calendar::turn += 24_hours;

        auto quantity = 1;
        auto components = get_map().use_charges( pos, 0, itype_id( "bread" ), quantity,
                          return_true<item> );

        REQUIRE( quantity == 0 );
        REQUIRE( components.size() == 1 );
        CHECK( components.front()->charges == 1 );
        CHECK( components.front()->get_relative_rot() > 0.0 );
        CHECK( components.front()->get_relative_rot() < 1.0 );

        auto remaining = get_map().i_at( pos );
        REQUIRE( remaining.size() == 1 );
        CHECK( remaining.only_item().charges == 1 );
        CHECK( remaining.only_item().get_relative_rot() > 0.0 );
        CHECK( remaining.only_item().get_relative_rot() < 1.0 );
    }

    SECTION( "unprotected map storage reports stale rot when inspected before processing" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        add_sashimi_to_map( pos );

        calendar::turn += 20_days;

        auto items = get_map().i_at( pos );
        REQUIRE( items.size() == 1 );
        CHECK( items.only_item().get_rot() > 0_turns );
        CHECK_FALSE( items.only_item().is_fresh() );
    }

    SECTION( "unprotected map storage rots food normally" ) {
        prepare_map_storage_test();
        const auto pos = tripoint_bub_ms( 60, 60, 0 );
        get_map().set_temperature( pos, 100 );
        add_sashimi_to_map( pos );

        process_storage_for( 25_hours );

        CHECK( get_map().i_at( pos ).empty() );
    }
}
