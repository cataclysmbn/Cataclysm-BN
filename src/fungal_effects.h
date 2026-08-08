#pragma once

#include "coordinates.h"

class mapbuffer;
class game;
class Creature;

class fungal_effects
{
    private:
        // Dependency injection to try to be less global
        game &gm;
        mapbuffer &m;
    public:
        fungal_effects( game &g, mapbuffer &mp );
        fungal_effects( const fungal_effects & ) = delete;
        fungal_effects( fungal_effects && ) = delete;

        void marlossify( const tripoint_abs_ms &p );
        /** Makes spores at p. source is used for kill counting */
        void create_spores( const tripoint_abs_ms &p, Creature *origin = nullptr );
        void fungalize( const tripoint_abs_ms &p, Creature *origin = nullptr, double spore_chance = 0.0 );

        void spread_fungus( const tripoint_abs_ms &p );
        void spread_fungus_one_tile( const tripoint_abs_ms &p, int growth );
};


