#pragma once

#include "explosion.h"
#include "coordinates.h"

#include <algorithm>
#include <string>
#include <deque>
#include <utility>

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
    queued_explosion( const tripoint_bub_ms &pos, ExplosionType type,
                      Creature *source ) : pos( pos ), type( type ), source( source ) {}

    /** Origin */
    tripoint_bub_ms pos;
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
    Creature *source = nullptr;
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
        int explosion_count = 0;
        /// > 0 while execute() drains are deferred to the turn loop (issue #9696).
        int drain_deferral_depth = 0;
        /// A drain was requested but suppressed by the deferral.
        bool deferred_drain_requested = false;

    public:
        /// OOM/hang backstop (not the #9696 fix): bounds one runaway execute()
        /// drain and, via add(), the queue size. No legitimate chain comes near it.
        static constexpr int max_pending_explosions = 2000;

        void add( queued_explosion &&exp ) {
            // Drop when saturated to bound memory; only a runaway saturates.
            if( static_cast<int>( elems.size() ) < max_pending_explosions ) {
                elems.push_back( std::move( exp ) );
            }
        }

        void execute();

        void clear() {
            elems.clear();
            deferred_drain_requested = false;
        }

        auto empty() const -> bool { return elems.empty(); }

        auto get_count() const -> int { return explosion_count; }

        /// Null queued sources naming a creature that is about to be destroyed.
        auto invalidate_source( const Creature *source ) -> void {
            for( queued_explosion &exp : elems ) {
                if( exp.source == source ) {
                    exp.source = nullptr;
                }
            }
        }

        /// Whether any queued explosion still names this source.
        auto references_source( const Creature *source ) const -> bool {
            return std::ranges::contains( elems, source, &queued_explosion::source );
        }

        /// Drain deferral -- see scoped_drain_deferral (issue #9696).
        void push_drain_deferral() { drain_deferral_depth++; }
        void pop_drain_deferral() { drain_deferral_depth--; }
        auto drains_deferred() const -> bool { return drain_deferral_depth > 0; }

        /// Consume the pending suppressed-drain request, if any.
        auto take_deferred_drain_request() -> bool {
            return std::exchange( deferred_drain_requested, false );
        }
};

explosion_queue &get_explosion_queue();

/// RAII: defers explosion-queue drains while map item processing runs, so a
/// re-entrant drain can't sympathetically re-detonate a detached-but-in-stack
/// item forever (issue #9696). The turn-loop drain empties the queue afterwards.
class scoped_drain_deferral
{
    public:
        scoped_drain_deferral() { get_explosion_queue().push_drain_deferral(); }
        ~scoped_drain_deferral() { get_explosion_queue().pop_drain_deferral(); }
        scoped_drain_deferral( const scoped_drain_deferral & ) = delete;
        auto operator=( const scoped_drain_deferral & ) -> scoped_drain_deferral & = delete; // *NOPAD*
};

} // namespace explosion_handler


