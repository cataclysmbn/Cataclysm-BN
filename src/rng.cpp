#include "rng.h"
#include "rng_observation.h"
#include "rng_task_trace.h"
#include "weighted_list.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <cmath>
#include <cstdint>
#include <locale>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "calendar.h"
#include "cata_utility.h"
#include "units.h"

namespace
{

constexpr auto child_seed_stream = std::uint64_t
{
    0x6368696c645f5f5f
};

// Main-thread global engine (default, time-seeded).
auto main_rng_engine() -> cata_default_random_engine & // *NOPAD*
{
    // NOLINTNEXTLINE(cata-determinism)
    static auto engine = cata_default_random_engine(
                             std::chrono::high_resolution_clock::now().time_since_epoch().count() );
    return engine;
}

auto has_deterministic_seed() -> std::atomic_bool & // *NOPAD*
{
    static auto value = std::atomic_bool{ false };
    return value;
}

auto deterministic_seed() -> std::atomic_uint & // *NOPAD*
{
    static auto value = std::atomic_uint{ 1u };
    return value;
}

auto deterministic_task_counter() -> std::atomic_uint64_t & // *NOPAD*
{
    static auto value = std::atomic_uint64_t{ 0 };
    return value;
}

auto saved_main_rng_engine() -> std::optional<cata_default_random_engine> & // *NOPAD*
{
    static auto value = std::optional<cata_default_random_engine> {};
    return value;
}

auto splitmix64( std::uint64_t value ) -> std::uint64_t
{
    value += 0x9e3779b97f4a7c15ULL;
    value = ( value ^ ( value >> 30 ) ) * 0xbf58476d1ce4e5b9ULL;
    value = ( value ^ ( value >> 27 ) ) * 0x94d049bb133111ebULL;
    return value ^ ( value >> 31 );
}

auto non_zero_seed( const std::uint64_t value ) -> unsigned int
{
    const auto seed = static_cast<unsigned int>( value & 0x7fffffffu );
    return seed == 0 ? 1u : seed;
}

// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_is_worker = false;
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_worker_engine = cata_default_random_engine {};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_has_task_engine = false;
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task_engine = cata_default_random_engine {};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_has_task_context = false;
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task_context_seed = std::uint64_t
{
    0
};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task_child_counter = std::uint64_t
{
    0
};

struct trace_occurrence_key {
    std::optional<std::size_t> parent;
    rng_task_trace_kind kind;
    std::uint64_t stream;
    std::uint64_t key;
    auto operator<=>( const trace_occurrence_key & ) const = default; // *NOPAD*
};

struct trace_occurrence_hash {
    auto operator()( const trace_occurrence_key &key ) const noexcept -> std::size_t;
};

struct trace_occurrence_equal {
    auto operator()( const trace_occurrence_key &lhs,
                     const trace_occurrence_key &rhs ) const noexcept -> bool;
};

} // namespace

struct rng_task_trace::state {
    std::mutex mutex;
    rng_task_trace_snapshot snapshot;
    std::size_t capacity = 0;
    std::uint64_t epoch = 0;
    // Each entry belongs to at least one retained record, so both the entry
    // count and reserved bucket storage are bounded by the window capacity.
    std::unordered_map<trace_occurrence_key, std::size_t,
        trace_occurrence_hash, trace_occurrence_equal> occurrences;
};

struct rng_task_trace_ticket {
    std::shared_ptr<rng_task_trace::state> owner;
    std::optional<std::size_t> index;
    bool acknowledged = false;
    bool started = false;
    ~rng_task_trace_ticket() {
        if( !owner ) {
            return;
        }
        auto lock = std::lock_guard( owner->mutex );
        if( !acknowledged ) {
            --owner->snapshot.outstanding;
            owner->snapshot.incomplete = true;
            if( index ) {
                owner->snapshot.records[*index].phase = rng_task_trace_phase::cancelled;
            }
        }
    }
};

namespace
{

auto trace_mutex = std::mutex {};
auto active_trace = std::weak_ptr<rng_task_trace::state> {};
auto trace_enabled = std::atomic_bool { false };
auto trace_epoch = std::atomic_uint64_t { 0 };
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_trace = rng_task_trace_token {};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_reserved_trace = rng_task_trace_token {};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_callable_trace = rng_task_trace_token {};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_trace_bound = false;

#if defined(CATA_RNG_TRACE_TESTING)
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_test_probe = static_cast<rng_task_trace_test::probe *>( nullptr );
#endif

auto trace_occurrence_hash::operator()( const trace_occurrence_key &key ) const noexcept
-> std::size_t
{
#if defined(CATA_RNG_TRACE_TESTING)
    if( tl_test_probe ) {
        ++tl_test_probe->occurrence_hashes;
    }
#endif
    // Pure identity hashing; no gameplay engine draws or stream allocations.
    auto value = splitmix64( key.stream );
    value = splitmix64( value ^ key.key );
    value = splitmix64( value ^ static_cast<std::uint64_t>( key.kind ) );
    return static_cast<std::size_t>( splitmix64( value ^
                                     ( key.parent ? *key.parent : 0x726f6f745f5f5f5fULL ) ) );
}

auto trace_occurrence_equal::operator()( const trace_occurrence_key &lhs,
        const trace_occurrence_key &rhs ) const noexcept -> bool
{
#if defined(CATA_RNG_TRACE_TESTING)
    if( tl_test_probe ) {
        ++tl_test_probe->occurrence_comparisons;
    }
#endif
    return lhs == rhs;
}

auto observe_occurrence_index_size( const rng_task_trace::state &state ) -> void
{
#if defined(CATA_RNG_TRACE_TESTING)
    if( tl_test_probe ) {
        tl_test_probe->occurrence_entries = state.occurrences.size();
        tl_test_probe->peak_occurrence_entries = std::max( tl_test_probe->peak_occurrence_entries,
                state.occurrences.size() );
    }
#else
    static_cast<void>( state );
#endif
}

auto invalidate_rng_task_trace() -> void
{
    if( !trace_enabled.load( std::memory_order_acquire ) && !tl_trace ) {
        return;
    }
    auto lock = std::lock_guard( trace_mutex );
    if( const auto trace = active_trace.lock() ) {
        auto state_lock = std::lock_guard( trace->mutex );
        trace->snapshot.incomplete = true;
    }
    if( tl_trace ) {
        auto state_lock = std::lock_guard( tl_trace->owner->mutex );
        tl_trace->owner->snapshot.incomplete = true;
    }
}

auto acknowledge_trace( const rng_task_trace_token &ticket ) -> void
{
    auto lock = std::lock_guard( ticket->owner->mutex );
    if( !ticket->acknowledged ) {
        ticket->acknowledged = true;
        ticket->owner->snapshot.incomplete = ticket->owner->snapshot.incomplete ||
                                             ticket->owner->epoch != trace_epoch.load( std::memory_order_relaxed );
        --ticket->owner->snapshot.outstanding;
        if( ticket->index ) {
            ticket->owner->snapshot.records[*ticket->index].phase = rng_task_trace_phase::restored;
        }
    }
}

} // namespace

#if defined(CATA_RNG_TRACE_TESTING)
rng_task_trace_test::scoped_probe::scoped_probe( probe &value )
    : previous_( tl_test_probe )
{
    tl_test_probe = &value;
}

rng_task_trace_test::scoped_probe::~scoped_probe()
{
    tl_test_probe = previous_;
}
#endif

rng_task_trace::rng_task_trace( const rng_task_trace_options &options )
    : state_( std::make_shared<state>() )
{
    state_->epoch = trace_epoch.load( std::memory_order_relaxed );
    state_->capacity = std::min( options.capacity, std::size_t{ 65536 } );
    state_->snapshot.records.reserve( state_->capacity );
    state_->occurrences.reserve( state_->capacity );
}

rng_task_trace::~rng_task_trace()
{
    stop();
}

auto rng_task_trace::observe() const -> rng_task_trace_snapshot
{
    auto lock = std::lock_guard( state_->mutex );
    auto snapshot = state_->snapshot;
    snapshot.incomplete = snapshot.incomplete ||
                          ( ( snapshot.accepting || snapshot.outstanding != 0 ) &&
                            state_->epoch != trace_epoch.load( std::memory_order_relaxed ) );
    return snapshot;
}

auto rng_task_trace::stop() -> void
{
    auto lock = std::lock_guard( trace_mutex );
    if( const auto trace = active_trace.lock(); !trace || trace == state_ ) {
        active_trace.reset();
        trace_enabled.store( false, std::memory_order_release );
    }
    auto state_lock = std::lock_guard( state_->mutex );
    if( state_->snapshot.accepting || state_->snapshot.outstanding != 0 ) {
        state_->snapshot.incomplete = state_->snapshot.incomplete ||
                                      state_->epoch != trace_epoch.load( std::memory_order_relaxed );
    }
    state_->snapshot.accepting = false;
}

auto begin_rng_task_trace( const rng_task_trace_options &options ) ->
std::shared_ptr<rng_task_trace> // *NOPAD*
{
    if( !rng_deterministic_seed_active() || tl_has_task_context || tl_trace ) {
        return nullptr;
    }
    auto lock = std::lock_guard( trace_mutex );
    if( !active_trace.expired() ) {
        return nullptr;
    }
    auto trace = std::shared_ptr<rng_task_trace>( new rng_task_trace( options ) );
    active_trace = trace->state_;
    trace_enabled.store( true, std::memory_order_release );
    return trace;
}

auto reserve_rng_task_trace( const rng_task_trace_submission &submission ) -> rng_task_trace_token
{
    // The disabled path does not lock, allocate, serialize or touch RNG state.
    const auto parent = submission.parent ? submission.parent : tl_trace;
    if( !parent && !trace_enabled.load( std::memory_order_acquire ) ) {
        return nullptr;
    }
    auto owner = std::shared_ptr<rng_task_trace::state> {};
    if( parent ) {
        owner = parent->owner;
    } else {
        auto lock = std::lock_guard( trace_mutex );
        if( const auto trace = active_trace.lock() ) {
            owner = trace;
        }
    }
    if( !owner ) {
        return nullptr;
    }
#if defined(CATA_RNG_TRACE_TESTING)
    // This seam holds neither trace_mutex nor owner->mutex. It exposes the
    // root-admission interleaving without changing admission in normal builds.
    if( !parent && tl_test_probe && tl_test_probe->after_root_owner_acquired ) {
        tl_test_probe->after_root_owner_acquired();
    }
#endif
    // Declare before the lock so cancellation on an allocation failure happens
    // after unlocking. Keep the lock through recovery, so failed capture cannot
    // transiently certify a closed window before incomplete evidence is set.
    auto ticket = rng_task_trace_token{};
    auto lock = std::unique_lock<std::mutex> {};
    try {
        lock = std::unique_lock( owner->mutex );
        // Admission linearizes against stop() under this same ledger lock.
        // Retained parents still admit descendants after root admission closes.
        if( !parent && !owner->snapshot.accepting ) {
            return nullptr;
        }
        if( owner->snapshot.records.size() == owner->capacity ) {
            owner->snapshot.overflow = true;
            return nullptr;
        }
#if defined(CATA_RNG_TRACE_TESTING)
        if( tl_test_probe && tl_test_probe->before_context_capture ) {
            tl_test_probe->before_context_capture();
        }
#endif
        const auto context = observe_deterministic_rng();
        ticket = std::make_shared<rng_task_trace_ticket>();
        ticket->owner = owner;
        ++owner->snapshot.outstanding;
        if( !context || ( parent && parent->acknowledged ) ||
            owner->epoch != trace_epoch.load( std::memory_order_relaxed ) ) {
            owner->snapshot.incomplete = true;
        }
        const auto parent_index = parent ? parent->index : std::nullopt;
        const auto identity = trace_occurrence_key{
            .parent = parent_index, .kind = submission.kind,
            .stream = submission.stream, .key = submission.key,
        };
        const auto entry = owner->occurrences.try_emplace( identity, std::size_t{ 0 } );
        observe_occurrence_index_size( *owner );
        const auto index = owner->snapshot.records.size();
        try {
#if defined(CATA_RNG_TRACE_TESTING)
            // Fault injection owns this thread and must not reenter the ledger
            // while its mutex is held. Exercise the append rollback transaction.
            if( tl_test_probe && tl_test_probe->before_record_append ) {
                tl_test_probe->before_record_append();
            }
#endif
            owner->snapshot.records.push_back( {
                .parent = parent_index,
                .kind = submission.kind,
                .stream = submission.stream,
                .key = submission.key,
                .occurrence = entry.first->second,
                .seed = submission.seed,
                .submission_context = context,
            } );
        } catch( ... ) {
            if( entry.second ) {
                owner->occurrences.erase( entry.first );
            }
            observe_occurrence_index_size( *owner );
            throw;
        }
        // Commit the count only after appending. Failed new keys leave no
        // orphan entries; failed repeats neither erase nor advance old counts.
        ++entry.first->second;
        ticket->index = index;
        return ticket;
    } catch( ... ) {
        if( lock.owns_lock() ) {
            owner->snapshot.incomplete = true;
        } else {
            auto recovery_lock = std::lock_guard( owner->mutex );
            owner->snapshot.incomplete = true;
        }
        return nullptr;
    }
}

struct rng_task_trace_scope {
    rng_task_trace_token ticket;
    rng_task_trace_token outer;
    const rng_deterministic_task_scope *scope = nullptr;
    std::unique_ptr<rng_task_trace_scope> previous;
    int exceptions = std::uncaught_exceptions();
    bool accepted = false;

    explicit rng_task_trace_scope( const rng_task_trace_token &reserved )
        : ticket( reserved ), outer( tl_trace ) {
        auto lock = std::lock_guard( ticket->owner->mutex );
        if( ticket->started || ticket->acknowledged ) {
            ticket->owner->snapshot.incomplete = true;
            return;
        }
        ticket->started = true;
        if( ticket->index ) {
            auto &record = ticket->owner->snapshot.records[*ticket->index];
            record.phase = rng_task_trace_phase::running;
            record.outer_context = observe_deterministic_rng();
        }
        tl_trace = ticket;
        accepted = true;
    }

    auto initial() -> void {
        auto lock = std::lock_guard( ticket->owner->mutex );
        if( ticket->index ) {
            ticket->owner->snapshot.records[*ticket->index].initial_context = observe_deterministic_rng();
        }
    }

    auto finish() -> void {
        auto lock = std::lock_guard( ticket->owner->mutex );
        if( ticket->index ) {
            auto &record = ticket->owner->snapshot.records[*ticket->index];
            record.final_context = observe_deterministic_rng();
            record.exception = record.exception || std::uncaught_exceptions() > exceptions;
            if( !record.final_context ) {
                ticket->owner->snapshot.incomplete = true;
            }
        }
    }

    auto restored() -> void {
        tl_trace = outer;
        {
            auto lock = std::lock_guard( ticket->owner->mutex );
            if( ticket->index ) {
                ticket->owner->snapshot.records[*ticket->index].restored_context = observe_deterministic_rng();
            }
        }
        acknowledge_trace( ticket );
    }
};

namespace
{
// Only traced live scopes are retained, never completed events on TLS.
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_trace_scope = std::unique_ptr<rng_task_trace_scope> {};
} // namespace

rng_task_trace_binding::rng_task_trace_binding( const rng_task_trace_token &token )
    : old_token_( tl_reserved_trace ), old_callable_( tl_callable_trace ),
      old_bound_( tl_trace_bound )
{
    tl_reserved_trace = token;
    tl_callable_trace = token;
    tl_trace_bound = true;
}

rng_task_trace_binding::~rng_task_trace_binding()
{
    tl_reserved_trace = old_token_;
    tl_callable_trace = old_callable_;
    tl_trace_bound = old_bound_;
}

auto rng_task_trace_note_exception() -> void
{
    if( tl_callable_trace ) {
        auto lock = std::lock_guard( tl_callable_trace->owner->mutex );
        if( tl_callable_trace->index ) {
            tl_callable_trace->owner->snapshot.records[*tl_callable_trace->index].exception = true;
        }
    }
}

rng_task_trace_call::rng_task_trace_call( const rng_task_trace_submission &submission )
    : token_( reserve_rng_task_trace( submission ) ), outer_( tl_trace ),
      exceptions_( std::uncaught_exceptions() )
{
    if( token_ ) {
        tl_trace = token_;
    }
}

rng_task_trace_call::~rng_task_trace_call()
{
    if( token_ ) {
        if( std::uncaught_exceptions() > exceptions_ ) {
            auto lock = std::lock_guard( token_->owner->mutex );
            if( token_->index ) {
                token_->owner->snapshot.records[*token_->index].exception = true;
            }
        }
        tl_trace = outer_;
        acknowledge_trace( token_ );
    }
}

auto observe_deterministic_rng() -> std::optional<rng_observation>
{
    const auto seed = rng_deterministic_seed_value();
    if( !seed ) {
        return std::nullopt;
    }
    const auto source = tl_has_task_engine ? rng_engine_source::task :
                        tl_is_worker ? rng_engine_source::worker : rng_engine_source::simulation;
    auto task = std::optional<rng_task_observation> {};
    if( tl_has_task_context ) {
        task = rng_task_observation{
            .seed = tl_task_context_seed, .next_child = tl_task_child_counter,
        };
    }
    auto engine_state = std::ostringstream{};
    engine_state.exceptions( std::ios::badbit | std::ios::failbit );
    engine_state.imbue( std::locale::classic() );
#if defined(CATA_RNG_TRACE_TESTING)
    if( tl_test_probe && tl_test_probe->configure_engine_output ) {
        tl_test_probe->configure_engine_output( engine_state );
    }
#endif
    engine_state << rng_get_engine();
    return rng_observation{
        .source = source,
        .root_seed = *seed,
        .next_root_call = deterministic_task_counter().load( std::memory_order_relaxed ),
        .task = task,
        .engine_state = std::move( engine_state ).str(),
    };
}

auto rng_get_engine() -> cata_default_random_engine & // *NOPAD*
{
    if( tl_has_task_engine ) {
        return tl_task_engine;
    }
    if( tl_is_worker ) {
        return tl_worker_engine;
    }
    return main_rng_engine();
}

void rng_set_engine_seed( const unsigned int seed )
{
    if( seed != 0 ) {
        main_rng_engine().seed( seed );
    }
}

auto rng_bits() -> unsigned int
{
    // Whole uint range.
    static std::uniform_int_distribution<unsigned int> rng_uint_dist;
    return rng_uint_dist( rng_get_engine() );
}

auto rng( int lo, int hi ) -> int
{
    static std::uniform_int_distribution<int> rng_int_dist;
    if( lo > hi ) {
        std::swap( lo, hi );
    }
    return rng_int_dist( rng_get_engine(), std::uniform_int_distribution<>::param_type( lo, hi ) );
}

auto rng_float( double lo, double hi ) -> double
{
    static std::uniform_real_distribution<double> rng_real_dist;
    if( lo > hi ) {
        std::swap( lo, hi );
    }
    return rng_real_dist( rng_get_engine(), std::uniform_real_distribution<>::param_type( lo, hi ) );
}

auto random_direction() -> units::angle
{
    return rng_float( 0_pi_radians, 2_pi_radians );
}

auto normal_roll( const double mean, const double stddev ) -> double
{
    // Do not retain cached distribution state across task scopes.
    auto distribution = std::normal_distribution<double> {};
    return distribution( rng_get_engine(), std::normal_distribution<>::param_type( mean, stddev ) );
}

auto exponential_roll( const double lambda ) -> double
{
    static std::exponential_distribution<double> rng_exponential_dist;
    return rng_exponential_dist( rng_get_engine(),
                                 std::exponential_distribution<>::param_type( lambda ) );
}

auto rng_exponential( const double min, const double mean ) -> double
{
    const auto adjusted_mean = mean - min;
    if( adjusted_mean <= 0.0 ) {
        return 0.0;
    }
    return min + exponential_roll( 1.0 / adjusted_mean );
}

auto one_in( const int chance ) -> bool
{
    return chance <= 1 || rng( 0, chance - 1 ) == 0;
}

auto one_turn_in( const time_duration &duration ) -> bool
{
    return one_in( to_turns<int>( duration ) );
}

auto x_in_y( const double x, const double y ) -> bool
{
    return rng_float( 0.0, 1.0 ) <= x / y;
}

auto check( const units::probability p ) -> bool
{
    return rng( 0, 1000000 - 1 ) < units::to_one_in_million( p );
}

auto dice( const int number, const int sides ) -> int
{
    auto result = 0;
    for( auto index = 0; index < number; ++index ) {
        result += rng( 1, sides );
    }
    return result;
}

auto roll_remainder( const double value ) -> int
{
    double integral;
    const auto fraction = std::modf( value, &integral );
    if( value > 0.0 && value > integral && x_in_y( fraction, 1.0 ) ) {
        ++integral;
    } else if( value < 0.0 && value < integral && x_in_y( -fraction, 1.0 ) ) {
        --integral;
    }
    return static_cast<int>( integral );
}

auto djb2_hash( const unsigned char *input ) -> int
{
    auto hash = 5381u;
    auto character = *input++;
    while( character != '\0' ) {
        hash = ( ( hash << 5 ) + hash ) + character;
        character = *input++;
    }
    return static_cast<int>( hash );
}

auto rng_normal( double lo, double hi ) -> double
{
    if( lo > hi ) {
        std::swap( lo, hi );
    }
    const auto stddev = ( hi - lo ) / 4;
    if( stddev == 0.0 ) {
        return hi;
    }
    return clamp( normal_roll( ( hi + lo ) / 2, stddev ), lo, hi );
}

auto rng_set_deterministic_seed( unsigned int seed ) -> void
{
    invalidate_rng_task_trace();
    trace_epoch.fetch_add( 1, std::memory_order_relaxed );
    if( seed == 0 ) {
        seed = 1;
    }
    if( !has_deterministic_seed().load( std::memory_order_acquire ) ) {
        saved_main_rng_engine() = main_rng_engine();
    }
    deterministic_seed().store( seed, std::memory_order_relaxed );
    deterministic_task_counter().store( 0, std::memory_order_relaxed );
    has_deterministic_seed().store( true, std::memory_order_release );
    main_rng_engine().seed( seed );
}

auto rng_clear_deterministic_seed() -> void
{
    invalidate_rng_task_trace();
    trace_epoch.fetch_add( 1, std::memory_order_relaxed );
    if( auto &saved_engine = saved_main_rng_engine(); saved_engine ) {
        main_rng_engine() = *saved_engine;
        saved_engine.reset();
    }
    has_deterministic_seed().store( false, std::memory_order_release );
    deterministic_task_counter().store( 0, std::memory_order_relaxed );
}

auto rng_deterministic_seed_active() -> bool
{
    return has_deterministic_seed().load( std::memory_order_acquire );
}

auto rng_deterministic_seed_value() -> std::optional<unsigned int>
{
    if( !rng_deterministic_seed_active() ) {
        return std::nullopt;
    }
    return deterministic_seed().load( std::memory_order_relaxed );
}

auto rng_deterministic_seed_for( const rng_deterministic_key &key ) -> unsigned int
{
    auto value = static_cast<std::uint64_t>( deterministic_seed().load( std::memory_order_relaxed ) );
    value = splitmix64( value ^ key.stream );
    value = splitmix64( value ^ key.id );
    return non_zero_seed( value );
}

auto rng_deterministic_child_seed( const unsigned int parent_seed,
                                   const rng_deterministic_key &key ) -> unsigned int
{
    auto value = static_cast<std::uint64_t>( parent_seed );
    value = splitmix64( value ^ child_seed_stream ^ key.stream );
    value = splitmix64( value ^ key.id );
    return non_zero_seed( value );
}

auto rng_deterministic_seed_for_current_context(
    const rng_deterministic_key &key ) -> std::optional<unsigned int>
{
    if( !rng_deterministic_seed_active() ) {
        return std::nullopt;
    }
    if( tl_has_task_context ) {
        return rng_deterministic_child_seed( static_cast<unsigned int>( tl_task_context_seed ), key );
    }
    return rng_deterministic_seed_for( key );
}

auto rng_next_deterministic_call_seed( const std::uint64_t stream ) -> std::optional<unsigned int>
{
    if( !rng_deterministic_seed_active() ) {
        return std::nullopt;
    }
    if( tl_has_task_context ) {
        const auto child_index = tl_task_child_counter++;
        return rng_deterministic_child_seed( static_cast<unsigned int>( tl_task_context_seed ),
        { .stream = stream, .id = child_index } );
    }
    const auto task_index = deterministic_task_counter().fetch_add( 1, std::memory_order_relaxed );
    return rng_deterministic_seed_for( { .stream = child_seed_stream ^ stream, .id = task_index } );
}

rng_deterministic_task_scope::rng_deterministic_task_scope( const unsigned int seed )
    : old_has_task_engine_( tl_has_task_engine ),
      old_task_engine_( tl_task_engine ),
      old_has_task_context_( tl_has_task_context ),
      old_task_context_seed_( tl_task_context_seed ),
      old_task_child_counter_( tl_task_child_counter )
{
    const auto ticket = std::exchange( tl_trace_bound, false ) ?
                        std::exchange( tl_reserved_trace, nullptr ) :
                        reserve_rng_task_trace( { .seed = seed } );
    try {
        if( ticket ) {
            auto trace = std::make_unique<rng_task_trace_scope>( ticket );
            if( trace->accepted ) {
                trace->scope = this;
                trace->previous = std::move( tl_trace_scope );
                tl_trace_scope = std::move( trace );
            }
        }
    } catch( ... ) {
        auto lock = std::lock_guard( ticket->owner->mutex );
        ticket->owner->snapshot.incomplete = true;
    }
    tl_has_task_engine = true;
    tl_task_engine.seed( seed == 0 ? 1u : seed );
    tl_has_task_context = true;
    tl_task_context_seed = seed;
    tl_task_child_counter = 0;
    try {
        if( tl_trace_scope && tl_trace_scope->scope == this ) {
            tl_trace_scope->initial();
        }
    } catch( ... ) {
        auto lock = std::lock_guard( tl_trace_scope->ticket->owner->mutex );
        tl_trace_scope->ticket->owner->snapshot.incomplete = true;
    }
}

rng_deterministic_task_scope::~rng_deterministic_task_scope()
{
    // Diagnostic failures must not prevent gameplay scope restoration.
    const auto traced = tl_trace_scope && tl_trace_scope->scope == this;
    try {
        if( traced ) {
            tl_trace_scope->finish();
        }
    } catch( ... ) {
        auto lock = std::lock_guard( tl_trace_scope->ticket->owner->mutex );
        tl_trace_scope->ticket->owner->snapshot.incomplete = true;
    }
    tl_has_task_engine = old_has_task_engine_;
    tl_task_engine = old_task_engine_;
    tl_has_task_context = old_has_task_context_;
    tl_task_context_seed = old_task_context_seed_;
    tl_task_child_counter = old_task_child_counter_;
    try {
        if( traced ) {
            tl_trace_scope->restored();
        }
    } catch( ... ) {
        tl_trace = tl_trace_scope->outer;
        auto lock = std::lock_guard( tl_trace_scope->ticket->owner->mutex );
        tl_trace_scope->ticket->owner->snapshot.incomplete = true;
    }
    if( traced ) {
        auto previous = std::move( tl_trace_scope->previous );
        tl_trace_scope = std::move( previous );
    }
}

void rng_set_worker_seed( const unsigned int seed )
{
    tl_is_worker = true;
    tl_worker_engine.seed( seed );
}

namespace weighted_list_detail
{
auto gen_rand_i() -> unsigned int
{
    return rng_bits();
}
} // namespace weighted_list_detail
