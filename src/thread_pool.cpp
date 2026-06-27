#include "thread_pool.h"

#include <chrono>
#include <functional>
#include <thread>

#include "crash.h"
#include "options.h"
#include "random/rng.h"

namespace
{

auto enqueue_task( std::deque<std::function<void()>> &queue, std::mutex &mutex,
                   std::function<void()> task ) -> void
{
    auto lock = std::lock_guard<std::mutex>( mutex );
    queue.push_back( std::move( task ) );
}

} // namespace

thread_local bool tl_is_worker_thread = false;

auto is_pool_worker_thread() -> bool
{
    return tl_is_worker_thread;
}

cata_thread_pool::cata_thread_pool( unsigned int num_workers )
{
    workers_.reserve( num_workers );
    for( auto i = 0u; i < num_workers; ++i ) {
        workers_.emplace_back( [this, i]() {
            worker_loop( i );
        } );
    }
}

cata_thread_pool::~cata_thread_pool()
{
    {
        auto lock = std::lock_guard<std::mutex>( mutex_ );
        stop_ = true;
    }
    cv_.notify_all();
    for( auto &worker : workers_ ) {
        worker.join();
    }
}

auto cata_thread_pool::worker_loop( const unsigned int worker_index ) -> void
{
    tl_is_worker_thread = true;
    // Windows installs signal handlers per-thread for hardware exception signals
    // (SIGSEGV, SIGFPE, SIGILL).  Re-run the crash handler setup so that crashes
    // on worker threads are caught and logged the same way as main-thread crashes.
    // On POSIX the signal disposition is process-wide, so this is a no-op.
    init_crash_handlers();

    // Seed this worker's thread-local RNG so compute_plan() calls do not
    // race on the main thread's global engine (P-5). Deterministic replay uses
    // stable worker-index seeds plus per-task RNG scopes, so OS thread IDs and
    // scheduling cannot change worker RNG streams.
    const auto seed = rng_deterministic_seed_active() ?
                      rng_deterministic_seed_for( { .stream = 0x776f726b65725f5fULL,
                              .id = worker_index } ) :
                      static_cast<unsigned int>(
                          static_cast<unsigned int>( std::hash<std::thread::id> {}( std::this_thread::get_id() ) ) ^
                          static_cast<unsigned int>(
                              std::chrono::high_resolution_clock::now().time_since_epoch().count() ) );
    rng_set_worker_seed( seed );

    while( true ) {
        auto task = std::function<void()> {};
        {
            auto lock = std::unique_lock<std::mutex>( mutex_ );
            cv_.wait( lock, [this]() {
                return stop_ || !queue_.empty();
            } );
            if( stop_ && queue_.empty() ) {
                return;
            }
            task = std::move( queue_.front() );
            queue_.pop_front();
        }
        task();
    }
}

auto cata_thread_pool::submit( std::function<void()> task ) -> void
{
    enqueue_task( queue_, mutex_, std::move( task ) );
    cv_.notify_one();
}

auto cata_thread_pool::submit( const rng_deterministic_key &key,
                               std::function<void()> task ) -> void
{
    const auto deterministic_seed = rng_deterministic_seed_for_current_context( key );
    if( deterministic_seed ) {
        auto wrapped_task = [task = std::move( task ), deterministic_seed]() {
            [[maybe_unused]] const auto deterministic_scope =
                rng_deterministic_task_scope( *deterministic_seed );
            task();
        };
        enqueue_task( queue_, mutex_, std::move( wrapped_task ) );
    } else {
        enqueue_task( queue_, mutex_, std::move( task ) );
    }
    cv_.notify_one();
}

auto get_thread_pool() -> cata_thread_pool& // *NOPAD*
{
    // Worker count is read once at first call (the static pool is constructed
    // only once).  Changes to THREAD_POOL_WORKERS or MULTITHREADING_ENABLED
    // require a restart.
    static cata_thread_pool pool( []() -> unsigned int {
        // Respect the "disable multi-threading" setting.  This is read via
        // get_option<bool>() directly (not the cached parallel_enabled global)
        // because cache_to_globals() has not yet run at pool-init time.
        if( !get_option<bool>( "MULTITHREADING_ENABLED" ) )
        {
            return 0u;
        }
        const auto workers_opt = get_option<int>( "THREAD_POOL_WORKERS" );
        if( workers_opt > 0 )
        {
            return static_cast<unsigned int>( workers_opt );
        }
        // 0 = auto: hardware_concurrency()-1, leaving one core for the main/SDL thread.
        const auto hc = std::thread::hardware_concurrency();
        return hc > 1u ? hc - 1u : 0u;
    }() );
    return pool;
}
