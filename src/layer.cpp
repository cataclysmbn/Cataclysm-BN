#include "layer.h"

world_layer get_layer( int z )
{
    for( int i = 0; i < static_cast<int>( world_layer::NUM_LAYERS ); ++i ) {
        world_layer layer = static_cast<world_layer>( i );
        int base = get_layer_base_z( layer );
        if( z >= base + LAYER_Z_MIN && z <= base + LAYER_Z_MAX ) {
            return layer;
        }
    }

    return world_layer::OVERWORLD;
}

bool is_valid_layer_z( int z )
{
    for( int i = 0; i < static_cast<int>( world_layer::NUM_LAYERS ); ++i ) {
        world_layer layer = static_cast<world_layer>( i );
        int base = get_layer_base_z( layer );
        if( z >= base + LAYER_Z_MIN && z <= base + LAYER_Z_MAX ) {
            return true;
        }
    }
    return false;
}

oter_id get_default_layer_terrain(world_layer layer, int relative_z) {
    switch( layer ) {
        default:
            return oter_str_id( "pd_border" );
    }
}
