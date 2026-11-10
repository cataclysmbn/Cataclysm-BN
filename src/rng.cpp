#include "rng.h"
#include "presentation_rng.h"
#include "weighted_list.h"

#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <utility>

#include "calendar.h"
#include "cata_utility.h"
#include "debug.h"
#include "units.h"

unsigned int rng_bits()
{
    // Whole uint range.
    static std::uniform_int_distribution<unsigned int> rng_uint_dist;
    return rng_uint_dist( rng_get_engine() );
}

int rng( int lo, int hi )
{
    static std::uniform_int_distribution<int> rng_int_dist;
    if( lo > hi ) {
        std::swap( lo, hi );
    }
    return rng_int_dist( rng_get_engine(), std::uniform_int_distribution<>::param_type( lo, hi ) );
}

double rng_float( double lo, double hi )
{
    static std::uniform_real_distribution<double> rng_real_dist;
    if( lo > hi ) {
        std::swap( lo, hi );
    }
    return rng_real_dist( rng_get_engine(), std::uniform_real_distribution<>::param_type( lo, hi ) );
}

units::angle random_direction()
{
    return rng_float( 0_pi_radians, 2_pi_radians );
}

double normal_roll( double mean, double stddev )
{
    // Do not retain cached distribution state across task scopes.
    auto rng_normal_dist = std::normal_distribution<double> {};
    return rng_normal_dist( rng_get_engine(), std::normal_distribution<>::param_type( mean, stddev ) );
}

double exponential_roll( double lambda )
{
    static std::exponential_distribution<double> rng_exponential_dist;
    return rng_exponential_dist( rng_get_engine(),
                                 std::exponential_distribution<>::param_type( lambda ) );
}

double rng_exponential( double min, double mean )
{
    const double adjusted_mean = mean - min;
    if( adjusted_mean <= 0.0 ) {
        return 0.0;
    }
    // lambda = 1 / mean
    return min + exponential_roll( 1.0 / adjusted_mean );
}

bool one_in( int chance )
{
    return ( chance <= 1 || rng( 0, chance - 1 ) == 0 );
}

bool one_turn_in( const time_duration &duration )
{
    return one_in( to_turns<int>( duration ) );
}

bool x_in_y( double x, double y )
{
    return rng_float( 0.0, 1.0 ) <= x / y;
}

bool check( units::probability p )
{
    return rng( 0, 1000000 - 1 ) < units::to_one_in_million( p );
}

int dice( int number, int sides )
{
    int ret = 0;
    for( int i = 0; i < number; i++ ) {
        ret += rng( 1, sides );
    }
    return ret;
}

// probabilistically round a double to an int
// 1.3 has a 70% chance of rounding to 1, 30% chance to 2.
int roll_remainder( double value )
{
    double integ;
    double frac = modf( value, &integ );
    if( value > 0.0 && value > integ && x_in_y( frac, 1.0 ) ) {
        integ++;
    } else if( value < 0.0 && value < integ && x_in_y( -frac, 1.0 ) ) {
        integ--;
    }

    return integ;
}

// http://www.cse.yorku.ca/~oz/hash.html
// for world seeding.
int djb2_hash( const unsigned char *input )
{
    unsigned int hash = 5381;
    unsigned char c = *input++;
    while( c != '\0' ) {
        hash = ( ( hash << 5 ) + hash ) + c; /* hash * 33 + c */
        c = *input++;
    }
    return hash;
}

double rng_normal( double lo, double hi )
{
    if( lo > hi ) {
        std::swap( lo, hi );
    }

    const double stddev = ( hi - lo ) / 4;
    if( stddev == 0.0 ) {
        return hi;
    }
    double val = normal_roll( ( hi + lo ) / 2, stddev );
    return clamp( val, lo, hi );
}

// Main-thread global engine (default, time-seeded).
static cata_default_random_engine &main_rng_engine()
{
    // NOLINTNEXTLINE(cata-determinism)
    static cata_default_random_engine eng(
        std::chrono::high_resolution_clock::now().time_since_epoch().count() );
    return eng;
}

// Per-worker-thread engine.  Inactive (flag=false) on the main thread.
// NOLINTNEXTLINE(cata-determinism)
static thread_local bool tl_is_worker = false;
// NOLINTNEXTLINE(cata-determinism)
static thread_local cata_default_random_engine tl_worker_engine;

namespace
{

auto deterministic_seed = std::atomic<std::optional<unsigned int>> {};
auto deterministic_task_counter = std::atomic_uint64_t { 0 };
auto saved_main_rng_engine = std::optional<cata_default_random_engine> {};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task = std::optional<rng_task_context> {};

} // namespace

auto rng_get_engine() -> cata_default_random_engine & // *NOPAD*
{
    if( tl_task ) {
        return tl_task->engine;
    }
    if( tl_is_worker ) {
        // A worker's engine depends on which tasks it ran before, so in deterministic mode only a task scope may draw there.
        if( rng_deterministic_seed_active() ) {
            debugmsg( "rng draw on a worker thread outside a deterministic task scope" );
        }
        return tl_worker_engine;
    }
    return main_rng_engine();
}

auto rng_set_deterministic_seed( const unsigned int seed ) -> void
{
    if( !rng_deterministic_seed_active() ) {
        saved_main_rng_engine = main_rng_engine();
    }
    deterministic_task_counter = 0;
    deterministic_seed = std::max( seed, 1u );
    main_rng_engine().seed( seed );
}

auto rng_clear_deterministic_seed() -> void
{
    if( saved_main_rng_engine ) {
        main_rng_engine() = *std::exchange( saved_main_rng_engine, std::nullopt );
    }
    deterministic_seed = std::nullopt;
    deterministic_task_counter = 0;
}

auto rng_deterministic_seed_active() -> bool
{
    return deterministic_seed.load().has_value();
}

auto rng_deterministic_child_seed( const unsigned int parent_seed,
                                   const rng_deterministic_key &key ) -> unsigned int
{
    // The standard fixes seed_seq's output, so every platform derives the same child.
    auto words = std::seed_seq{ parent_seed, static_cast<std::uint32_t>( key.stream ), static_cast<std::uint32_t>( key.stream >> 32 ),
                                static_cast<std::uint32_t>( key.id ), static_cast<std::uint32_t>( key.id >> 32 ) };
    auto child = std::array<std::uint32_t, 1> {};
    words.generate( child.begin(), child.end() );
    return child[0];
}

auto rng_deterministic_seed_for_current_context(
    const rng_deterministic_key &key ) -> std::optional<unsigned int>
{
    const auto seed = deterministic_seed.load();
    if( !seed ) {
        return std::nullopt;
    }
    return rng_deterministic_child_seed( tl_task ? tl_task->seed : *seed, key );
}

auto rng_next_deterministic_call_seed( const std::uint64_t stream ) -> std::optional<unsigned int>
{
    if( !rng_deterministic_seed_active() ) {
        return std::nullopt;
    }
    const auto id = tl_task ? tl_task->child_counter++ : deterministic_task_counter++;
    return rng_deterministic_seed_for_current_context( { .stream = stream, .id = id } );
}

rng_deterministic_task_scope::rng_deterministic_task_scope( const std::optional<unsigned int> seed )
{
    if( seed ) {
        old_ = std::exchange( tl_task, rng_task_context{ .engine = cata_default_random_engine( *seed ), .seed = *seed } );
    }
}

rng_deterministic_task_scope::~rng_deterministic_task_scope()
{
    if( old_ ) {
        tl_task = *old_;
    }
}

void rng_set_engine_seed( unsigned int seed )
{
    if( seed != 0 ) {
        main_rng_engine().seed( seed );
    }
}

void rng_set_worker_seed( unsigned int seed )
{
    tl_is_worker = true;
    tl_worker_engine.seed( seed );
}

auto presentation_rng( int lo, int hi ) -> int
{
    // NOLINTNEXTLINE(cata-determinism)
    thread_local auto engine = cata_default_random_engine( std::random_device{}() );
    if( lo > hi ) {
        std::swap( lo, hi );
    }
    return std::uniform_int_distribution<int>( lo, hi )( engine );
}

namespace weighted_list_detail
{
unsigned int gen_rand_i()
{
    return rng_bits();
}
} // namespace weighted_list_detail
