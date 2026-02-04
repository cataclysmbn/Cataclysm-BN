#include "catch/catch.hpp"

#include "calendar.h"
#include "game.h"
#include "iexamine.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

static const furn_str_id f_smoking_rack( "f_smoking_rack" );
static const furn_str_id f_smoking_rack_active( "f_smoking_rack_active" );

TEST_CASE( "smoking_rack_items_dont_disappear", "[smoking_rack][furniture]" )
{
    // Test for issue #5681: Items disappearing when smoking process completes
    // The bug was that items.insert() uses add_item_or_charges() which tries to
    // overflow items on NOITEM tiles, causing items to be lost when surrounded by walls
    
    clear_all_state();
    map &here = get_map();
    player &dummy = get_player_character();
    
    // Set time to avoid special handling of calendar::turn_zero
    if( calendar::turn <= calendar::start_of_cataclysm ) {
        calendar::turn = calendar::start_of_cataclysm + 1_hours;
    }
    
    const tripoint rack_pos( 60, 60, 0 );
    
    // Set up smoking rack
    here.furn_set( rack_pos, f_smoking_rack );
    here.ter_set( rack_pos, t_grass );
    
    // Verify the rack has NOITEM flag (this is what causes the bug)
    REQUIRE( here.has_flag_furn( TFLAG_NOITEM, rack_pos ) );
    
    // Clear any existing items
    here.i_clear( rack_pos );
    
    // Add some raw meat to smoke
    detached_ptr<item> raw_meat = item::spawn( "meat", calendar::turn );
    raw_meat->set_flag( flag_SMOKABLE );
    REQUIRE( raw_meat->has_flag( flag_SMOKABLE ) );
    
    // Add meat to the rack
    here.add_item( rack_pos, std::move( raw_meat ) );
    
    // Verify meat is there
    map_stack items_before = here.i_at( rack_pos );
    REQUIRE( items_before.size() == 1 );
    
    // Simulate smoking completion by calling on_smoke_out directly
    // This mimics what happens when the fake_smoke_plume timer expires
    const time_point smoke_start = calendar::turn;
    iexamine::on_smoke_out( rack_pos, smoke_start );
    
    // Check that items are still present after smoking completes
    map_stack items_after = here.i_at( rack_pos );
    
    // The original meat should be transformed into smoked meat, not disappear
    CHECK( items_after.size() >= 1 );
    
    // Verify we got a result (smoked meat or meat jerky)
    bool has_smoked_result = false;
    for( const item *it : items_after ) {
        if( it->has_flag( flag_SMOKED ) || it->typeId() == itype_id( "meat_smoked" ) || 
            it->typeId() == itype_id( "jerky" ) ) {
            has_smoked_result = true;
            break;
        }
    }
    
    // The bug would cause items_after to be empty, failing this check
    CHECK( has_smoked_result );
}

TEST_CASE( "smoking_rack_surrounded_by_walls", "[smoking_rack][furniture]" )
{
    // Test the specific scenario where smoking rack is surrounded by walls/furniture
    // This is when the overflow behavior would fail and lose items
    
    clear_all_state();
    map &here = get_map();
    
    if( calendar::turn <= calendar::start_of_cataclysm ) {
        calendar::turn = calendar::start_of_cataclysm + 1_hours;
    }
    
    const tripoint rack_pos( 60, 60, 0 );
    
    // Set up smoking rack
    here.furn_set( rack_pos, f_smoking_rack );
    here.ter_set( rack_pos, t_grass );
    
    // Surround with walls to prevent overflow
    for( int dx = -1; dx <= 1; dx++ ) {
        for( int dy = -1; dy <= 1; dy++ ) {
            if( dx == 0 && dy == 0 ) continue; // Skip the rack itself
            tripoint wall_pos = rack_pos + tripoint( dx, dy, 0 );
            here.ter_set( wall_pos, t_wall );
        }
    }
    
    here.i_clear( rack_pos );
    
    // Add raw meat
    detached_ptr<item> raw_meat = item::spawn( "meat", calendar::turn );
    raw_meat->set_flag( flag_SMOKABLE );
    here.add_item( rack_pos, std::move( raw_meat ) );
    
    // Verify meat is there
    REQUIRE( here.i_at( rack_pos ).size() == 1 );
    
    // Complete smoking process
    iexamine::on_smoke_out( rack_pos, calendar::turn );
    
    // Items should still be present even when surrounded by walls
    map_stack items_after = here.i_at( rack_pos );
    CHECK( items_after.size() >= 1 );
}
