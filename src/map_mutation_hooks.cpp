#include "map_mutation_hooks.h"

#include <utility>

#include "avatar.h"
#include "creature.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "iuse_actor.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "mapdata.h"
#include "messages.h"
#include "translations.h"

namespace
{

static const auto effect_crushed = efftype_id( "crushed" );

} // namespace

namespace map_mutation_hooks
{

auto on_furniture_changed( const furniture_changed_options &options ) -> void
{
    if( g == nullptr ) {
        return;
    }

    auto &here = g->m;
    if( here.get_bound_dimension() != options.dim_id ) {
        return;
    }

    const auto local = abs_to_map_local( here, options.p );
    if( !here.inbounds( local ) ) {
        return;
    }

    if( options.old_furniture == f_rubble && options.new_furniture == f_null ) {
        if( auto *const critter = g->critter_at<Creature>( options.p ) ) {
            critter->remove_effect( effect_crushed );
        }
    }
}

auto prepare_item_for_placement( const item_placement_options &options ) -> bool
{
    if( !options.item_to_place ) {
        return false;
    }

    if( g == nullptr ) {
        return true;
    }

    auto &here = g->m;
    if( here.get_bound_dimension() != options.dim_id ) {
        return true;
    }

    const auto local = abs_to_map_local( here, options.p );
    if( !here.inbounds( local ) ) {
        return true;
    }

    if( options.item_to_place->is_food() ) {
        options.item_to_place = item::process( std::move( options.item_to_place ), nullptr, local, false );
        if( !options.item_to_place ) {
            return false;
        }
    }

    if( options.item_to_place->made_of( LIQUID ) && here.has_flag( "SWIMMABLE", local ) ) {
        return false;
    }

    if( here.has_flag( "DESTROY_ITEM", local ) ) {
        return false;
    }

    if( options.item_to_place->has_flag( flag_ACT_IN_FIRE ) &&
        here.get_field( local, fd_fire ) != nullptr ) {
        if( options.item_to_place->has_flag( flag_BOMB ) && options.item_to_place->is_transformable() ) {
            options.item_to_place->convert( dynamic_cast<const iuse_transform *>(
                                                options.item_to_place->type->get_use( "transform" )->get_actor_ptr() )->target );
        }
        options.item_to_place->activate();
    }

    options.item_to_place->on_map_placement( here, local );
    return true;
}

} // namespace map_mutation_hooks
