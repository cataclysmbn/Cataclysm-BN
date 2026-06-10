#include "map_mutation_hooks.h"

#include "avatar.h"
#include "creature.h"
#include "game.h"
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

    auto &here = get_map();
    if( here.get_bound_dimension() != options.dim_id || !here.inbounds( options.p ) ) {
        return;
    }

    const auto local = abs_to_map_local( here, options.p );
    const auto &old_furniture_type = options.old_furniture.obj();
    const auto &new_furniture_type = options.new_furniture.obj();

    auto &you = g->u;
    if( you.get_grab_type() == OBJECT_FURNITURE &&
        you.bub_pos() + you.grab_point == local &&
        !new_furniture_type.is_movable() ) {
        add_msg( _( "The %s you were grabbing is destroyed!" ), old_furniture_type.name() );
        you.grab( OBJECT_NONE );
    }

    if( options.old_furniture == f_rubble && options.new_furniture == f_null ) {
        if( auto *const critter = g->critter_at<Creature>( options.p ) ) {
            critter->remove_effect( effect_crushed );
        }
    }
}

} // namespace map_mutation_hooks
