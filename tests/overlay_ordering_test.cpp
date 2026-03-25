#include "catch/catch.hpp"

#include <ranges>
#include <string>
#include <vector>

#include "avatar.h"
#include "cata_utility.h"
#include "item.h"
#include "overlay_ordering.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

namespace
{

const auto trait_elfa_ears = trait_id( "ELFA_EARS" );
const auto backpack_overlay_id = std::string( "worn_backpack" );

auto find_overlay_position( const std::vector<Character::overlay_entry> &overlays,
                            const std::string &overlay_id ) -> std::vector<Character::overlay_entry>::const_iterator
{
    return std::ranges::find( overlays, overlay_id, &Character::overlay_entry::id );
}

} // namespace

TEST_CASE( "overlay ordering can move clothing below mutation overlays", "[graphics][overlay]" )
{
    clear_all_state();
    auto &player_character = get_avatar();
    player_character.set_mutation( trait_elfa_ears );
    REQUIRE( !player_character.wear_item( item::spawn( "backpack" ), false ) );

    const auto default_overlays = player_character.get_overlay_ids();
    const auto default_mutation = find_overlay_position( default_overlays, "mutation_ELFA_EARS" );
    const auto default_bag = find_overlay_position( default_overlays, backpack_overlay_id );

    REQUIRE( default_mutation != default_overlays.end() );
    REQUIRE( default_bag != default_overlays.end() );
    CHECK( default_bag < default_mutation );

    const auto existing_order = base_mutation_overlay_ordering.find( backpack_overlay_id );
    const auto has_existing_order = existing_order != base_mutation_overlay_ordering.end();
    const auto prior_order = has_existing_order ? existing_order->second : 0;
    const auto restore_overlay_order = on_out_of_scope( [&]() {
        if( has_existing_order ) {
            base_mutation_overlay_ordering[backpack_overlay_id] = prior_order;
            return;
        }
        base_mutation_overlay_ordering.erase( backpack_overlay_id );
    } );
    ( void )restore_overlay_order;

    base_mutation_overlay_ordering[backpack_overlay_id] = 10000;

    const auto overridden_overlays = player_character.get_overlay_ids();
    const auto overridden_mutation = find_overlay_position( overridden_overlays, "mutation_ELFA_EARS" );
    const auto overridden_bag = find_overlay_position( overridden_overlays, backpack_overlay_id );

    REQUIRE( overridden_mutation != overridden_overlays.end() );
    REQUIRE( overridden_bag != overridden_overlays.end() );
    CHECK( overridden_mutation < overridden_bag );
}
