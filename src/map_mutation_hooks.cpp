#include "map_mutation_hooks.h"

#include <utility>

#include "creature.h"
#include "game.h"
#include "map.h"
#include "mapdata.h"
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

} // namespace map_mutation_hooks
