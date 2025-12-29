#pragma once

#include "explosion.h"
#include "point.h"

#include <string>
#include <deque>

namespace explosion_handler
{

enum class ExplosionType {
    Regular,
    Flashbang,
    ResonanceCascade,
    Shockwave
};

struct queued_explosion {
    queued_explosion() = default;
    queued_explosion( const tripoint &pos, ExplosionType type,
                      Creature *source ) : pos( pos ), type( type ), source( source ) {}

    /** Origin */
    tripoint pos;
    /** Explosion type */
    ExplosionType type = ExplosionType::Regular;
    /** Data for Regular explosion */
    explosion_data exp_data;
    /** Data for Shockwave explosion */
    shockwave_data swave_data;
    /** Graphical name for the explosion */
    std::string graphics_name;
    /** Whether it affects player */
    bool affects_player = false;
    /** Who's responsible of the explosion */
    Creature *source;
};

namespace explosion_funcs
{

void regular( const queued_explosion &qe );
void flashbang( const queued_explosion &qe );
void resonance_cascade( const queued_explosion &qe );
void shockwave( const queued_explosion &qe );

} // namespace explosion_funcs

class explosion_queue
{
    private:
        std::deque<queued_explosion> elems;

    public:
        void add( queued_explosion &&exp ) {
            elems.push_back( std::move( exp ) );
        }

        void execute();

        void clear() {
            elems.clear();
        }
};

explosion_queue &get_explosion_queue();

} // namespace explosion_handler


