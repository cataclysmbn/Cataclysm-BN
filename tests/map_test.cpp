#include "catch/catch.hpp"

#include <memory>
#include <vector>

#include "avatar.h"
#include "avatar_action.h"
#include "computer.h"
#include "construction_partial.h"
#include "coordinates.h"
#include "cata_utility.h"
#include "data_vars.h"
#include "enums.h"
#include "field_type.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "map_helpers.h"
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "state_helpers.h"
#include "submap.h"
#include "submap_load_manager.h"
#include "type_id.h"
#include "vehicle.h"

namespace
{

static const auto effect_in_pit = efftype_id( "in_pit" );
static const auto skill_dodge = skill_id( "dodge" );

struct adjacent_pit_move {
    tripoint_bub_ms origin;
    tripoint_bub_ms destination;
};

auto setup_adjacent_pit_move( const ter_id &origin_terrain,
                              const ter_id &destination_terrain ) -> adjacent_pit_move
{
    clear_all_state();
    auto &here = get_map();
    const auto origin = tripoint_bub_ms( 60, 60, 0 );
    const auto destination = origin + tripoint_rel_ms::east();

    g->place_player( origin );
    here.ter_set( origin, origin_terrain );
    here.ter_set( destination, destination_terrain );
    g->u.add_known_trap( origin, here.tr_at( origin ) );
    g->u.add_known_trap( destination, here.tr_at( destination ) );
    g->u.add_effect( effect_in_pit, 1_turns, bodypart_str_id::NULL_ID() );
    g->u.str_cur = 0;
    g->u.dex_cur = 0;
    g->u.set_skill_level( skill_dodge, 0 );
    g->u.moves = 1000;

    return { .origin = origin, .destination = destination };
}

auto setup_adjacent_pit_move( const ter_id &terrain ) -> adjacent_pit_move
{
    return setup_adjacent_pit_move( terrain, terrain );
}

auto add_absolute_test_submap( mapbuffer &buffer, const tripoint_abs_sm &pos,
                               const ter_id &terrain ) -> submap *
{
    auto sm = std::make_unique<submap>( pos );
    sm->set_all_ter( terrain );
    REQUIRE( buffer.add_submap( pos, sm ) );
    return buffer.lookup_submap_in_memory( pos );
}

} // namespace

TEST_CASE( "moving_between_adjacent_pit_traps" )
{
    SECTION( "regular pit movement skips warning, escape check, and repeated damage" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit" ) );
        const auto hp_before = g->u.get_hp();

        CHECK( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() == hp_before );
        CHECK( g->u.has_effect( effect_in_pit ) );
    }

    SECTION( "same spiked pit movement skips only the escape check" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit_spiked" ) );
        const auto dangerous_prompt = override_option( "DANGEROUS_TERRAIN_WARNING_PROMPT", "IGNORE" );
        const auto hp_before = g->u.get_hp();

        CHECK_FALSE( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() < hp_before );
    }

    SECTION( "same glass pit movement skips only the escape check" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit_glass" ) );
        const auto dangerous_prompt = override_option( "DANGEROUS_TERRAIN_WARNING_PROMPT", "IGNORE" );
        const auto hp_before = g->u.get_hp();

        CHECK_FALSE( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() < hp_before );
    }

    SECTION( "glass pit to regular pit skips warning, escape check, and repeated damage" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit_glass" ), ter_id( "t_pit" ) );
        const auto hp_before = g->u.get_hp();

        CHECK( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() == hp_before );
        CHECK( g->u.has_effect( effect_in_pit ) );
    }

    SECTION( "different pit trap movement remains dangerous" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit" ) );
        auto &here = get_map();
        here.ter_set( positions.destination, ter_id( "t_pit_spiked" ) );
        g->u.add_known_trap( positions.destination, here.tr_at( positions.destination ) );

        CHECK_FALSE( g->get_dangerous_tile( positions.destination ).empty() );
    }
}

TEST_CASE( "destroy_grabbed_furniture" )
{
    clear_all_state();
    GIVEN( "Furniture grabbed by the player" ) {
        const tripoint_bub_ms test_origin( 60, 60, 0 );
        map &here = get_map();
        g->u.setpos( test_origin );
        const tripoint_bub_ms grab_point = test_origin + tripoint_rel_ms::east();
        here.furn_set( grab_point, furn_id( "f_chair" ) );
        g->u.grab( OBJECT_FURNITURE, tripoint_rel_ms::east() );
        WHEN( "The furniture grabbed by the player is destroyed" ) {
            here.destroy( grab_point );
            THEN( "The player's grab is released" ) {
                CHECK( g->u.get_grab_type() == OBJECT_NONE );
                CHECK( g->u.grab_point == tripoint_rel_ms::zero() );
            }
        }
    }
}

// map_bounds_checking removed: the basic inbounds() cuboid check is trivial.
// A meaningful bounds test would involve pocket dimensions and dimension_bounds,
// which require more involved setup (mapgen, dimension transitions, etc.).

TEST_CASE( "tinymap_absolute_inbounds_uses_loaded_anchor" )
{
    clear_all_state();

    auto &buffer = MAPBUFFER;
    const auto anchor = tripoint_abs_sm( 1200, -1200, 0 );
    const auto cleanup = on_out_of_scope( [&]() {
        buffer.unload_omt( project_to<coords::omt>( anchor ), false );
    } );

    const auto offsets = std::vector<tripoint_rel_sm> {
        tripoint_rel_sm( 0, 0, 0 ),
        tripoint_rel_sm( 1, 0, 0 ),
        tripoint_rel_sm( 0, 1, 0 ),
        tripoint_rel_sm( 1, 1, 0 ),
    };

    for( const auto &offset : offsets ) {
        add_absolute_test_submap( buffer, anchor + offset, ter_id( "t_rock" ) );
    }

    tinymap tm;
    tm.bind_submaps_for_hook( anchor );

    CHECK( tm.inbounds( anchor ) );
    CHECK( tm.inbounds( anchor + tripoint_rel_sm( 1, 1, 0 ) ) );
    CHECK_FALSE( tm.inbounds( anchor + tripoint_rel_sm( -1, 0, 0 ) ) );
    CHECK_FALSE( tm.inbounds( anchor + tripoint_rel_sm( 2, 0, 0 ) ) );
    CHECK_FALSE( tm.inbounds( tripoint_abs_sm( 0, 0, 0 ) ) );
}

TEST_CASE( "mapbuffer_vehicle_lookup_uses_absolute_coordinates" )
{
    clear_all_state();

    auto &here = get_map();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    const auto local_pos = tripoint_bub_ms( 60, 60, 0 );
    here.ter_set( local_pos, ter_id( "t_floor" ) );

    auto *const veh = here.add_vehicle( vproto_id( "none" ), local_pos, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );
    REQUIRE( veh->install_part( tripoint_mnt_veh::zero(), vpart_id( "frame_vertical" ) ) >= 0 );
    REQUIRE( veh->install_part( tripoint_mnt_veh::zero(), vpart_id( "windshield" ), true ) >= 0 );
    here.add_vehicle_to_cache( veh );

    const auto abs_pos = map_local_to_abs( here, local_pos );
    const auto vp = MAPBUFFER.veh_at( abs_pos );
    REQUIRE( vp.has_value() );
    CHECK( &vp->vehicle() == veh );
    CHECK( MAPBUFFER.passable( abs_pos ) == false );
}

TEST_CASE( "place_player_can_safely_move_multiple_submaps" )
{
    clear_all_state();
    // Regression test for the situation where game::place_player would misuse
    // map::shift if the resulting shift exceeded a single submap, leading to a
    // broken active item cache.
    g->place_player( tripoint_bub_ms::zero() );
    CHECK( get_map().check_submap_active_item_consistency().empty() );
    CHECK( get_map().get_abs_sub() == player_reality_bubble_origin() );
}

TEST_CASE( "mapbuffer_resident_lookup_uses_absolute_coordinates" )
{
    clear_all_state();

    auto &buffer = MAPBUFFER;
    const auto sm_pos = tripoint_abs_sm( 1200, -1200, 0 );
    const auto resident_only = mapbuffer_lookup_options {
        .mode = mapbuffer_lookup_mode::resident_only
    };
    const auto cleanup = on_out_of_scope( [&]() {
        buffer.unload_omt( project_to<coords::omt>( sm_pos ), false );
    } );

    auto *const sm = add_absolute_test_submap( buffer, sm_pos, ter_id( "t_rock" ) );
    REQUIRE( sm != nullptr );

    CHECK( buffer.get_submap( sm_pos, resident_only ) == sm );

    const auto tile_pos = project_to<coords::ms>( sm_pos ) + tripoint_rel_ms( 3, 4, 0 );
    const auto local_tile_pos = point_sm_ms( 3, 4 );
    const auto terrain = buffer.get_ter( tile_pos, resident_only );
    REQUIRE( terrain.has_value() );
    CHECK( *terrain == ter_id( "t_rock" ) );
    CHECK( buffer.set_ter( tile_pos, ter_id( "t_dirt" ), resident_only ) );
    CHECK( buffer.get_ter( tile_pos, resident_only ) == ter_id( "t_dirt" ) );
    REQUIRE( buffer.ter_vars( tile_pos, resident_only ) != nullptr );
    buffer.ter_vars( tile_pos, resident_only )->set( "test_var", "terrain" );
    CHECK( sm->get_ter_vars( local_tile_pos ).get( "test_var" ) == "terrain" );

    const auto furniture = furn_str_id( "f_console_table" ).id();
    CHECK( buffer.get_furn( tile_pos, resident_only ) == f_null );
    CHECK( buffer.set_furn( tile_pos, furniture, resident_only ) );
    CHECK( buffer.get_furn( tile_pos, resident_only ) == furniture );
    REQUIRE( buffer.furn_vars( tile_pos, resident_only ) != nullptr );
    buffer.furn_vars( tile_pos, resident_only )->set( "test_var", "furniture" );
    CHECK( sm->get_furn_vars( local_tile_pos ).get( "test_var" ) == "furniture" );

    const auto trap = trap_str_id( "tr_bubblewrap" ).id();
    CHECK( buffer.get_trap( tile_pos, resident_only ) == tr_null );
    CHECK( buffer.set_trap( tile_pos, trap, resident_only ) );
    CHECK( buffer.get_trap( tile_pos, resident_only ) == trap );

    CHECK( buffer.get_radiation( tile_pos, resident_only ) == 0 );
    CHECK( buffer.set_radiation( tile_pos, 7, resident_only ) );
    CHECK( buffer.get_radiation( tile_pos, resident_only ) == 7 );
    CHECK( buffer.adjust_radiation( tile_pos, 5, resident_only ) == 12 );
    CHECK( buffer.get_radiation( tile_pos, resident_only ) == 12 );

    CHECK( buffer.get_lum( tile_pos, resident_only ) == 0 );
    CHECK( buffer.set_lum( tile_pos, 3, resident_only ) );
    CHECK( buffer.get_lum( tile_pos, resident_only ) == 3 );
    CHECK( buffer.get_temperature( tile_pos, resident_only ) == 0 );
    CHECK( buffer.set_temperature( tile_pos, 42, resident_only ) );
    CHECK( buffer.get_temperature( tile_pos, resident_only ) == 42 );
    CHECK( buffer.get_field( tile_pos, resident_only ) == &sm->get_field( local_tile_pos ) );
    CHECK_FALSE( buffer.has_field_at( tile_pos, resident_only ) );
    CHECK( buffer.get_field_entry( tile_pos, fd_fire, resident_only ) == nullptr );
    CHECK( buffer.get_field_age( tile_pos, fd_fire, resident_only ) == -1_turns );
    CHECK( buffer.get_field_intensity( tile_pos, fd_fire, resident_only ) == 0 );
    CHECK( buffer.add_field( tile_pos, {
        .type = fd_fire,
        .intensity = 1,
        .age = 5_turns,
        .lookup = resident_only,
    } ) );
    REQUIRE( buffer.get_field_entry( tile_pos, fd_fire, resident_only ) != nullptr );
    CHECK( buffer.has_field_at( tile_pos, resident_only ) );
    CHECK( sm->field_count == 1 );
    REQUIRE_FALSE( sm->field_cache.empty() );
    CHECK( sm->field_cache.back() == local_tile_pos );
    CHECK( buffer.get_field_age( tile_pos, fd_fire, resident_only ) == 5_turns );
    CHECK( buffer.get_field_intensity( tile_pos, fd_fire, resident_only ) == 1 );
    CHECK( buffer.set_field_age( tile_pos, {
        .type = fd_fire,
        .age = 10_turns,
        .lookup = resident_only,
    } ) == 10_turns );
    CHECK( buffer.mod_field_age( tile_pos, {
        .type = fd_fire,
        .age = 2_turns,
        .lookup = resident_only,
    } ) == 12_turns );
    CHECK( buffer.set_field_intensity( tile_pos, {
        .type = fd_fire,
        .intensity = 3,
        .lookup = resident_only,
    } ) == 3 );
    const auto max_fire_intensity = fd_fire.obj().get_max_intensity();
    CHECK( buffer.mod_field_intensity( tile_pos, {
        .type = fd_fire,
        .intensity = 2,
        .lookup = resident_only,
    } ) == max_fire_intensity );
    CHECK( buffer.get_field_intensity( tile_pos, fd_fire, resident_only ) == max_fire_intensity );
    CHECK( buffer.remove_field( tile_pos, fd_fire, resident_only ) );
    CHECK_FALSE( buffer.remove_field( tile_pos, fd_fire, resident_only ) );
    CHECK_FALSE( buffer.has_field_at( tile_pos, resident_only ) );
    CHECK( sm->field_count == 0 );
    CHECK( buffer.get_items( tile_pos, resident_only ) == &sm->get_items( local_tile_pos ) );
    CHECK( sm->get_items( local_tile_pos ).empty() );
    CHECK( buffer.set_furn( tile_pos, f_null, resident_only ) );
    auto aspirin_stack = item::spawn( "aspirin", calendar::start_of_cataclysm,
                                      item::default_charges_tag() );
    auto aspirin_charges = aspirin_stack->charges;
    CHECK_FALSE( buffer.add_item_or_charges( tile_pos, std::move( aspirin_stack ), {
        .overflow = false,
        .lookup = resident_only,
    } ) );
    REQUIRE( sm->get_items( local_tile_pos ).size() == 1 );
    CHECK( sm->get_items( local_tile_pos ).front()->charges == aspirin_charges );
    auto more_aspirin = item::spawn( "aspirin", calendar::start_of_cataclysm,
                                     item::default_charges_tag() );
    aspirin_charges += more_aspirin->charges;
    CHECK_FALSE( buffer.add_item_or_charges( tile_pos, std::move( more_aspirin ), {
        .overflow = false,
        .lookup = resident_only,
    } ) );
    REQUIRE( sm->get_items( local_tile_pos ).size() == 1 );
    CHECK( sm->get_items( local_tile_pos ).front()->charges == aspirin_charges );
    auto blocked_item = item::spawn( "rock", calendar::start_of_cataclysm );
    CHECK( buffer.set_furn( tile_pos, furn_str_id( "f_no_item" ).id(), resident_only ) );
    auto returned_blocked_item = buffer.add_item_or_charges( tile_pos, std::move( blocked_item ), {
        .overflow = false,
        .lookup = resident_only,
    } );
    CHECK( returned_blocked_item != nullptr );
    CHECK( sm->get_items( local_tile_pos ).size() == 1 );
    CHECK( buffer.set_furn( tile_pos, furniture, resident_only ) );
    auto removed_aspirin = buffer.clear_items( tile_pos, resident_only );
    CHECK( removed_aspirin.size() == 1 );

    auto active_item = item::spawn( "firecracker_act", calendar::start_of_cataclysm,
                                    item::default_charges_tag() );
    active_item->activate();
    REQUIRE( active_item->needs_processing() );
    auto *const active_item_ptr = &*active_item;
    CHECK_FALSE( buffer.add_item( tile_pos, std::move( active_item ), resident_only ) );
    CHECK( sm->get_items( local_tile_pos ).size() == 1 );
    CHECK_FALSE( sm->active_items.empty() );
    auto removed_active_item = buffer.remove_item( tile_pos, active_item_ptr, resident_only );
    CHECK( removed_active_item != nullptr );
    CHECK( sm->get_items( local_tile_pos ).empty() );
    CHECK( sm->active_items.empty() );

    CHECK( buffer.get_lum( tile_pos, resident_only ) == 0 );
    auto light_item = item::spawn( "glowstick_lit", calendar::start_of_cataclysm,
                                   item::default_charges_tag() );
    REQUIRE( light_item->is_emissive() );
    CHECK_FALSE( buffer.add_item( tile_pos, std::move( light_item ), resident_only ) );
    CHECK( buffer.get_lum( tile_pos, resident_only ) == 1 );
    auto cleared_items = buffer.clear_items( tile_pos, resident_only );
    CHECK( cleared_items.size() == 1 );
    CHECK( buffer.get_lum( tile_pos, resident_only ) == 0 );
    CHECK( sm->active_items.empty() );
    CHECK_FALSE( buffer.has_graffiti_at( tile_pos, resident_only ) );
    CHECK( buffer.graffiti_at( tile_pos, resident_only ) == "" );
    CHECK( buffer.set_graffiti( tile_pos, "absolute graffiti", resident_only ) );
    CHECK( buffer.has_graffiti_at( tile_pos, resident_only ) );
    CHECK( buffer.graffiti_at( tile_pos, resident_only ) == "absolute graffiti" );
    CHECK( buffer.delete_graffiti( tile_pos, resident_only ) );
    CHECK_FALSE( buffer.has_graffiti_at( tile_pos, resident_only ) );
    CHECK( buffer.set_signage( tile_pos, "absolute signage", resident_only ) );
    CHECK( buffer.get_signage( tile_pos, resident_only ) == "" );
    CHECK( buffer.delete_signage( tile_pos, resident_only ) );
    CHECK_FALSE( buffer.has_computer( tile_pos, resident_only ) );
    CHECK( buffer.set_computer( tile_pos, computer( "absolute terminal", 1 ), resident_only ) );
    CHECK( buffer.has_computer( tile_pos, resident_only ) );
    CHECK( buffer.get_computer( tile_pos, resident_only ) != nullptr );
    CHECK( buffer.delete_computer( tile_pos, resident_only ) );
    CHECK_FALSE( buffer.has_computer( tile_pos, resident_only ) );
    CHECK( buffer.add_computer( tile_pos, {
        .name = "absolute generated terminal",
        .security = 2,
        .lookup = resident_only,
    } ) != nullptr );
    CHECK( buffer.get_ter( tile_pos, resident_only ) == t_console );
    CHECK( buffer.has_computer( tile_pos, resident_only ) );
    CHECK( buffer.partial_con_at( tile_pos, resident_only ) == nullptr );
    CHECK( buffer.partial_con_set( tile_pos, std::make_unique<partial_con>( tile_pos ),
                                   resident_only ) );
    CHECK( buffer.partial_con_at( tile_pos, resident_only ) != nullptr );
    CHECK( buffer.partial_con_remove( tile_pos, resident_only ) );
    CHECK( buffer.partial_con_at( tile_pos, resident_only ) == nullptr );

    const auto missing_sm = sm_pos + tripoint_rel_sm( 10, 0, 0 );
    const auto missing_tile = project_to<coords::ms>( missing_sm );
    CHECK( buffer.get_submap( missing_sm, resident_only ) == nullptr );
    CHECK_FALSE( buffer.get_ter( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_ter( missing_tile, ter_id( "t_dirt" ), resident_only ) );
    CHECK( buffer.ter_vars( missing_tile, resident_only ) == nullptr );
    CHECK_FALSE( buffer.get_furn( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_furn( missing_tile, furniture, resident_only ) );
    CHECK( buffer.furn_vars( missing_tile, resident_only ) == nullptr );
    CHECK_FALSE( buffer.get_trap( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_trap( missing_tile, trap, resident_only ) );
    CHECK_FALSE( buffer.get_radiation( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_radiation( missing_tile, 7, resident_only ) );
    CHECK_FALSE( buffer.adjust_radiation( missing_tile, 5, resident_only ).has_value() );
    CHECK_FALSE( buffer.get_lum( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_lum( missing_tile, 3, resident_only ) );
    CHECK_FALSE( buffer.get_temperature( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_temperature( missing_tile, 42, resident_only ) );
    CHECK( buffer.get_field( missing_tile, resident_only ) == nullptr );
    CHECK_FALSE( buffer.has_field_at( missing_tile, resident_only ) );
    CHECK( buffer.get_field_entry( missing_tile, fd_fire, resident_only ) == nullptr );
    CHECK_FALSE( buffer.get_field_age( missing_tile, fd_fire, resident_only ).has_value() );
    CHECK_FALSE( buffer.get_field_intensity( missing_tile, fd_fire, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_field_age( missing_tile, {
        .type = fd_fire,
        .age = 10_turns,
        .lookup = resident_only,
    } ).has_value() );
    CHECK_FALSE( buffer.set_field_intensity( missing_tile, {
        .type = fd_fire,
        .intensity = 3,
        .lookup = resident_only,
    } ).has_value() );
    CHECK_FALSE( buffer.add_field( missing_tile, {
        .type = fd_fire,
        .intensity = 1,
        .lookup = resident_only,
    } ) );
    CHECK_FALSE( buffer.remove_field( missing_tile, fd_fire, resident_only ) );
    CHECK( buffer.get_items( missing_tile, resident_only ) == nullptr );
    auto missing_item = item::spawn( "rock" );
    auto unplaced_item = buffer.add_item( missing_tile, std::move( missing_item ), resident_only );
    CHECK( unplaced_item != nullptr );
    CHECK( buffer.remove_item( missing_tile, &*unplaced_item, resident_only ) == nullptr );
    CHECK( buffer.clear_items( missing_tile, resident_only ).empty() );
    CHECK_FALSE( buffer.has_graffiti_at( missing_tile, resident_only ) );
    CHECK_FALSE( buffer.graffiti_at( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_graffiti( missing_tile, "missing", resident_only ) );
    CHECK_FALSE( buffer.delete_graffiti( missing_tile, resident_only ) );
    CHECK_FALSE( buffer.has_signage( missing_tile, resident_only ) );
    CHECK_FALSE( buffer.get_signage( missing_tile, resident_only ).has_value() );
    CHECK_FALSE( buffer.set_signage( missing_tile, "missing", resident_only ) );
    CHECK_FALSE( buffer.delete_signage( missing_tile, resident_only ) );
    CHECK_FALSE( buffer.has_computer( missing_tile, resident_only ) );
    CHECK( buffer.get_computer( missing_tile, resident_only ) == nullptr );
    CHECK_FALSE( buffer.set_computer( missing_tile, computer( "missing", 1 ), resident_only ) );
    CHECK( buffer.add_computer( missing_tile, {
        .name = "missing",
        .security = 1,
        .lookup = resident_only,
    } ) == nullptr );
    CHECK_FALSE( buffer.delete_computer( missing_tile, resident_only ) );
    CHECK( buffer.partial_con_at( missing_tile, resident_only ) == nullptr );
    CHECK_FALSE( buffer.partial_con_set( missing_tile, std::make_unique<partial_con>( missing_tile ),
                                         resident_only ) );
    CHECK_FALSE( buffer.partial_con_remove( missing_tile, resident_only ) );
}

TEST_CASE( "mapbuffer_simulated_lookup_uses_load_manager_membership" )
{
    clear_all_state();

    static const dimension_id dim_id( "mapbuffer_lookup_test_dim" );
    auto &buffer = MAPBUFFER_REGISTRY.get( dim_id );
    const auto sm_pos = tripoint_abs_sm( 1300, -1300, 0 );
    auto full_handle = load_request_handle {};
    const auto cleanup = on_out_of_scope( [&]() {
        submap_loader.release_load( full_handle );
        MAPBUFFER_REGISTRY.unload_dimension( dim_id );
    } );
    auto *const sm = add_absolute_test_submap( buffer, sm_pos, ter_id( "t_rock" ) );
    REQUIRE( sm != nullptr );

    const auto lazy_handle = submap_loader.request_load( load_request_source::lazy_border,
                             dim_id, sm_pos, 0 );
    CHECK( buffer.get_submap( sm_pos ) == nullptr );
    submap_loader.release_load( lazy_handle );

    full_handle = submap_loader.request_load( load_request_source::script, dim_id, sm_pos, 0 );
    CHECK( buffer.get_submap( sm_pos ) == sm );
}

TEST_CASE( "mapbuffer_load_or_generate_lookup_is_explicit" )
{
    clear_all_state();

    auto &buffer = MAPBUFFER;
    const auto sm_pos = tripoint_abs_sm( 1400, -1400, 0 );
    const auto cleanup = on_out_of_scope( [&]() {
        buffer.unload_omt( project_to<coords::omt>( sm_pos ), false );
    } );
    const auto load_from_disk = mapbuffer_lookup_options {
        .mode = mapbuffer_lookup_mode::load_from_disk
    };
    const auto load_or_generate = mapbuffer_lookup_options {
        .mode = mapbuffer_lookup_mode::load_or_generate
    };
    const auto resident_only = mapbuffer_lookup_options {
        .mode = mapbuffer_lookup_mode::resident_only
    };

    REQUIRE( buffer.lookup_submap_in_memory( sm_pos ) == nullptr );
    CHECK( buffer.get_submap( sm_pos, load_from_disk ) == nullptr );
    CHECK( buffer.lookup_submap_in_memory( sm_pos ) == nullptr );

    submap *const generated = buffer.get_submap( sm_pos, load_or_generate );
    REQUIRE( generated != nullptr );
    CHECK( buffer.lookup_submap_in_memory( sm_pos ) == generated );
    CHECK( buffer.get_ter( project_to<coords::ms>( sm_pos ), resident_only ).has_value() );
}

TEST_CASE( "creature_mapbuffer_cache_tracks_dimension_registry_slots" )
{
    clear_all_state();

    static const dimension_id dim_id( "creature_mapbuffer_cache_test_dim" );
    const auto cleanup = on_out_of_scope( [&]() {
        MAPBUFFER_REGISTRY.unload_dimension( dim_id );
    } );
    MAPBUFFER_REGISTRY.unload_dimension( dim_id );

    auto critter = monster( mtype_id( "mon_zombie" ) );
    critter.set_dimension( dim_id );

    CHECK( critter.find_mapbuffer() == nullptr );

    mapbuffer &created = critter.get_mapbuffer();
    CHECK( &created == MAPBUFFER_REGISTRY.find( dim_id ) );
    CHECK( critter.find_mapbuffer() == &created );

    MAPBUFFER_REGISTRY.unload_dimension( dim_id );
    CHECK( critter.find_mapbuffer() == nullptr );

    mapbuffer &recreated = critter.get_mapbuffer();
    CHECK( &recreated == MAPBUFFER_REGISTRY.find( dim_id ) );
}

TEST_CASE( "free_bubble_conversions_follow_avatar_position" )
{
    clear_states( state::avatar | state::creature );

    auto &you = get_avatar();
    const auto player_sm = tripoint_abs_sm( 100, 200, 2 );
    const auto player_offset = tripoint_rel_ms( 3, 4, 0 );
    const auto player_abs = project_to<coords::ms>( player_sm ) + player_offset;
    you.Character::setpos( player_abs );

    const auto expected_origin = player_sm -
                                 tripoint_rel_sm( g_half_mapsize, g_half_mapsize, 0 );
    const auto expected_bub = tripoint_bub_ms( g_half_mapsize_x + player_offset.x(),
                              g_half_mapsize_y + player_offset.y(), player_abs.z() );

    CHECK( player_reality_bubble_origin() == expected_origin );
    CHECK( abs_to_bub( player_abs ) == expected_bub );
    CHECK( bub_to_abs( expected_bub ) == player_abs );
    CHECK( abs_to_bub( player_sm ) == tripoint_bub_sm( g_half_mapsize, g_half_mapsize,
            player_abs.z() ) );
    CHECK( bub_to_abs( tripoint_bub_sm( g_half_mapsize, g_half_mapsize, player_abs.z() ) ) ==
           player_sm );
    CHECK( abs_to_bub( player_sm.xy() ) == point_bub_sm( g_half_mapsize, g_half_mapsize ) );
    CHECK( bub_to_abs( point_bub_sm( g_half_mapsize, g_half_mapsize ) ) == player_sm.xy() );
    CHECK( you.bub_pos() == expected_bub );
    CHECK( g->critter_at<avatar>( player_abs ) == &you );

    const auto moved_bub = tripoint_bub_ms( g_half_mapsize_x + 1, g_half_mapsize_y + 2,
                                            player_abs.z() );
    const auto moved_abs = bub_to_abs( moved_bub );
    you.Character::setpos( moved_abs );
    CHECK( you.abs_pos() == moved_abs );
    CHECK( you.bub_pos() == moved_bub );
}

TEST_CASE( "reality_bubble_origin_helpers_use_explicit_size" )
{
    const auto player_sm = tripoint_abs_sm( 100, 200, 2 );
    const auto player_abs = project_to<coords::ms>( player_sm ) + tripoint_rel_ms( 3, 4, 0 );

    const auto size_four_origin = reality_bubble_origin_from_player( player_abs, 4 );
    CHECK( size_four_origin == player_sm - tripoint_rel_sm( 5, 5, 0 ) );
    CHECK( reality_bubble_center_from_origin( size_four_origin, 4 ) == player_sm );

    const auto size_zero_origin = reality_bubble_origin_from_player( player_abs, 0 );
    CHECK( size_zero_origin == player_sm - tripoint_rel_sm( 1, 1, 0 ) );
    CHECK( reality_bubble_center_from_origin( size_zero_origin, 0 ) == player_sm );
}

TEST_CASE( "avatar_setpos_updates_map_from_absolute_position" )
{
    clear_all_state();

    auto &here = get_map();
    auto &you = get_avatar();
    const auto old_origin = here.get_abs_sub();
    const auto destination = tripoint_bub_ms( g_half_mapsize_x + SEEX, g_half_mapsize_y, 0 );
    const auto destination_abs = map_local_to_abs( here, destination );

    you.setpos( destination_abs );

    CHECK( here.get_abs_sub() == old_origin + tripoint_rel_sm( 1, 0, 0 ) );
    CHECK( here.get_abs_sub() == player_reality_bubble_origin() );
    CHECK( you.abs_pos() == destination_abs );
    CHECK( you.bub_pos() == tripoint_bub_ms( g_half_mapsize_x, g_half_mapsize_y, 0 ) );
    CHECK( g->update_map( you ) == point_rel_sm::zero() );
}

TEST_CASE( "monster_tracker_uses_absolute_positions" )
{
    clear_all_state();

    auto &here = get_map();
    auto &you = get_avatar();
    const auto player_center = tripoint_bub_ms( g_half_mapsize_x, g_half_mapsize_y, 0 );
    you.setpos( map_local_to_abs( here, player_center ) );

    const auto monster_start = player_center + point_rel_ms( 2, 0 );
    auto *const mon = g->place_critter_at( mtype_id( "mon_zombie" ), monster_start );
    REQUIRE( mon != nullptr );
    const auto monster_abs = mon->abs_pos();

    CHECK( mon->bub_pos() == monster_start );
    CHECK( g->critter_at<monster>( monster_start ) == mon );
    CHECK( g->critter_at<monster>( monster_abs ) == mon );

    you.setpos( you.abs_pos() + tripoint_rel_ms( SEEX, 0, 0 ) );
    const auto player_shifted_monster_pos = abs_to_bub( monster_abs );
    CHECK( mon->abs_pos() == monster_abs );
    CHECK( mon->bub_pos() == player_shifted_monster_pos );
    CHECK( g->critter_at<monster>( monster_abs ) == mon );
    CHECK( g->critter_at<monster>( player_shifted_monster_pos ) == mon );
    CHECK( g->critter_at<monster>( monster_start ) == nullptr );

    const auto moved_abs = monster_abs + tripoint_rel_ms( 1, 0, 0 );
    mon->setpos( moved_abs );
    const auto moved_bub = abs_to_bub( moved_abs );
    CHECK( mon->abs_pos() == moved_abs );
    CHECK( g->critter_at<monster>( moved_bub ) == mon );
    CHECK( g->critter_at<monster>( player_shifted_monster_pos ) == nullptr );
}

TEST_CASE( "placed_monsters_inherit_bound_dimension" )
{
    clear_all_state();

    auto &here = get_map();
    const auto original_dim = here.get_bound_dimension();
    const auto test_dim = dimension_id( "placed_monsters_inherit_bound_dimension" );
    const auto cleanup = on_out_of_scope( [&]() {
        g->clear_zombies();
        here.bind_dimension( original_dim );
        MAPBUFFER_REGISTRY.unload_dimension( test_dim );
    } );

    here.bind_dimension( test_dim );

    const auto monster_pos = tripoint_bub_ms( g_half_mapsize_x + 2, g_half_mapsize_y, 0 );
    auto *const mon = g->place_critter_at( mtype_id( "mon_zombie" ), monster_pos );

    REQUIRE( mon != nullptr );
    CHECK( mon->get_dimension() == test_dim );
}

static std::ostream &operator<<( std::ostream &os, const ter_id &tid )
{
    os << tid.id().c_str();
    return os;
}

TEST_CASE( "bash_through_roof_can_destroy_multiple_times" )
{
    clear_all_state();
    map &here = get_map();
    REQUIRE( here.has_zlevels() );

    static const ter_str_id t_fragile_roof( "t_fragile_roof" );
    static const ter_str_id t_strong_roof( "t_strong_roof" );
    static const ter_str_id t_rock_floor_no_roof( "t_rock_floor_no_roof" );
    static const ter_str_id t_open_air( "t_open_air" );
    static const tripoint_bub_ms p( 65, 65, 1 );
    WHEN( "A wall has a matching roof above it, but the roof turns to a stronger roof on successful bash" ) {
        static const ter_str_id t_fragile_wall( "t_fragile_wall" );
        here.ter_set( p + tripoint_below, t_fragile_wall );
        here.ter_set( p, t_fragile_roof );
        AND_WHEN( "The roof is bashed with only enough strength to destroy the weaker roof type" ) {
            here.bash( p, 10, false, false, true );
            THEN( "The roof turns to the stronger type and the wall doesn't change" ) {
                CHECK( here.ter( p ) == t_strong_roof );
                CHECK( here.ter( p + tripoint_below ) == t_fragile_wall );
            }
        }

        AND_WHEN( "The roof is bashed with enough strength to destroy any roof" ) {
            here.bash( p, 1000, false, false, true );
            THEN( "Both the roof and the wall are destroyed" ) {
                CHECK( here.ter( p ) == t_open_air );
                CHECK( here.ter( p + tripoint_below ) == t_rock_floor_no_roof );
            }
        }
    }

    WHEN( "A passable floor has a matching roof above it, but both the roof and the floor turn into stronger variants on destroy" ) {
        static const ter_str_id t_fragile_floor( "t_fragile_floor" );
        here.ter_set( p + tripoint_below, t_fragile_floor );
        here.ter_set( p, t_fragile_roof );
        AND_WHEN( "The roof is bashed with only enough strength to destroy the weaker roof type" ) {
            here.bash( p, 10, false, false, true );
            THEN( "The roof turns to the stronger type and the floor doesn't change" ) {
                CHECK( here.ter( p ) == t_strong_roof );
                CHECK( here.ter( p + tripoint_below ) == t_fragile_floor );
            }
        }

        AND_WHEN( "The roof is bashed with enough strength to destroy any roof" ) {
            here.bash( p, 1000, false, false, true );
            THEN( "Both the roof and the floor are completely destroyed to default terrain" ) {
                CHECK( here.ter( p ) == t_open_air );
                CHECK( here.ter( p + tripoint_below ) == t_rock_floor_no_roof );
            }
        }
    }
}
