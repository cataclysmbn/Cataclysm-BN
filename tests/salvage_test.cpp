#include "catch/catch.hpp"

#include <memory>

#include "avatar.h"
#include "game.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "map.h"
#include "material.h"
#include "player.h"
#include "player_helpers.h"
#include "salvage.h"
#include "string_id.h"
#include "type_id.h"

namespace salvage
{
extern std::unordered_map<material_id, std::set<quality_id>> salvage_material_quality_dictionary;
extern std::set<material_id> all_salvagable_materials;
} // namespace salvage

namespace
{

auto cut_up_yields( const std::string &target ) -> void
{
    map &here = get_map();
    player &you = get_avatar();
    clear_avatar();
    // Nominal dex to avoid yield penalty.
    you.dex_cur = 12;
    //guy.set_skill_level( skill_id( "fabrication" ), 10 );
    here.i_at( you.pos() ).clear();

    CAPTURE( target );
    detached_ptr<item> cut_up_target = item::spawn( target );
    detached_ptr<item> tool = item::spawn( "toolbox_workshop" );

    const std::vector<material_id> &target_materials = cut_up_target->made_of();
    units::mass smallest_yield_mass = units::mass_max;
    for( const material_id &mater : target_materials ) {
        if( const std::optional<itype_id> item_id = mater->salvaged_into() ) {
            smallest_yield_mass = std::min( smallest_yield_mass, item_id->obj().weight );
        }
    }
    REQUIRE( smallest_yield_mass != units::mass_max );

    units::mass cut_up_target_mass = cut_up_target->weight();
    item &item_to_cut = *cut_up_target;
    material_id_list madeof = cut_up_target->made_of();
    here.add_item_or_charges( you.pos(), std::move( cut_up_target ) );

    REQUIRE( smallest_yield_mass <= cut_up_target_mass );

    salvage::complete_salvage( you, item_to_cut, here.getglobal( you.pos() ) );

    map_stack salvaged_items = here.i_at( you.pos() );
    const units::mass salvaged_mass = std::accumulate( salvaged_items.begin(), salvaged_items.end(),
    0_gram, []( const units::mass acc, const item * salvage ) {
        return acc + salvage->weight();
    } );

    CHECK( salvaged_mass <= cut_up_target_mass );
    CHECK( salvaged_mass >= 0_milligram );
    for( auto *item : salvaged_items ) {
        CHECK( item->made_of().size() == 1 );
        auto material = item->made_of().front();
        CHECK( salvage::all_salvagable_materials.contains( material ) );
        bool has_quality = false;
        for( auto &q : tool->get_qualities() ) {
            if( salvage::salvage_material_quality_dictionary[material].contains( q.first ) ) {
                has_quality = true;
                break;
            }
        }
        CHECK( has_quality );
        CHECK( std::ranges::find( madeof, material ) != madeof.end() );
    }
}
} // namespace

TEST_CASE( "cut_up_yields" )
{
    cut_up_yields( "blanket" );
    cut_up_yields( "backpack_hiking" );
    cut_up_yields( "boxpack" );
    cut_up_yields( "case_violin" );
    cut_up_yields( "down_mattress" );
    cut_up_yields( "plastic_boat_hull" );
    cut_up_yields( "bunker_coat" );
    cut_up_yields( "bunker_pants" );
    cut_up_yields( "kevlar" );
    cut_up_yields( "touring_suit" );
    cut_up_yields( "dress_wedding" );
    cut_up_yields( "wetsuit" );
    cut_up_yields( "wetsuit_booties" );
    cut_up_yields( "wetsuit_hood" );
    cut_up_yields( "wetsuit_spring" );
    cut_up_yields( "peacoat" );
    cut_up_yields( "log" );
    cut_up_yields( "stick" );
    cut_up_yields( "stick_long" );
    cut_up_yields( "tazer" );
    cut_up_yields( "control_laptop" );
    cut_up_yields( "voltmeter" );
    cut_up_yields( "eink_tablet_pc" );
    cut_up_yields( "camera" );
    cut_up_yields( "cell_phone" );
    cut_up_yields( "laptop" );
    cut_up_yields( "radio" );
    cut_up_yields( "under_armor" );
    cut_up_yields( "acidchitin_armor_small_quadruped" );
    cut_up_yields( "chitin_armor_small_quadruped" );
    cut_up_yields( "leather_armor_small_quadruped" );
    cut_up_yields( "leatherbone_armor_small_quadruped" );
    cut_up_yields( "kevlar_armor_small_quadruped" );
    cut_up_yields( "rubber_armor_small_quadruped" );
}

TEST_CASE( "cut_up_yields_all", "[.]" )
{
    int i = 0;
    for( const itype *itp : item_controller->all() ) {
        i++;
        SECTION( string_format( "Item number %d", i ) ) {
            cut_up_yields( itp->get_id().str() );
        }
    }
}
