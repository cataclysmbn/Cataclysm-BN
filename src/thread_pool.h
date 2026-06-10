#pragma once

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "random/rng.h"

namespace thread_pool_detail
{

constexpr auto parallel_for_seed_stream = std::uint64_t { 0x706172666f725f5f };
constexpr auto parallel_for_chunked_seed_stream = std::uint64_t { 0x7061726368756e6b };

template<typename F>
auto run_with_index_seed( const std::optional<unsigned int> &parent_seed,
                          const std::uint64_t stream, const int index, F &&f ) -> void
{
    if( parent_seed ) {
        [[maybe_unused]] const auto deterministic_scope = rng_deterministic_task_scope(
                    rng_deterministic_child_seed( *parent_seed, { .stream = stream,
                            .id = static_cast<std::uint64_t>( index )
                                                                } ) );
        f( index );
    } else {
        f( index );
    }
}

} // namespace thread_pool_detail

/**
 * Persistent thread pool for parallelizing game work.
 *
 * Workers are sized to hardware_concurrency() - 1 so the main thread
 * retains one core for the SDL event loop and game logic.
 *
 * Constraints (must not be violated by submitted work):
 *  - No worker thread may call any Lua API (Lua 5.3 is not reentrant).
 *  - No worker thread may call any SDL rendering API (SDL renderer is single-threaded).
 *
 * Threading boundaries for game systems:
 *
 *   cache_reference<T> — reference_map is guarded by reference_map_mutex_ (std::mutex).
 *     Construction and destruction of cache_reference objects is safe from worker threads.
 *
 *   safe_reference<T>  — records_by_pointer / records_by_id are NOT mutex-protected.
 *     next_id is std::atomic (safe for concurrent serialize() calls from save workers).
 *     Direct safe_reference operations other than serialize() must run on the main thread only.
 *     cata_arena serializes its own mark_destroyed()/mark_deallocated() calls.
 *
 *   cata_arena<T>      — pending_deletion is mutex-protected.
 *     mark_for_destruction() can overlap cleanup(), which drains outside the lock.
 *
 * In practice:
 *   • Submaps must not be destroyed on worker threads.  Use mapbuffer::drain_pending_submap_destroy()
 *     on the main thread after joining all preload_omt() futures.
 *   • Submap deserialisation IS safe from workers because
 *     active_item_cache constructs cache_reference objects, which are now mutex-guarded.
 *   • save_omt() serialisation IS safe from workers: safe_reference::serialize() only
 *     writes to next_id (atomic) and to per-item records that are never shared across omts.
 *   • overmapbuffer::add_extra() and add_note() ARE safe from generation workers:
 *     both acquire extras_mutex_ (a per-overmapbuffer std::mutex) after get_om_global() returns.
 *   • Auto-note discovery (auto_note_settings) and Lua spawn hooks in place_npc() are
 *     main-thread-only and are skipped on worker threads via is_pool_worker_thread().
 */
class cata_thread_pool
{
    public:
        explicit cata_thread_pool( unsigned int num_workers );
        ~cata_thread_pool();

        cata_thread_pool( const cata_thread_pool & ) = delete;
        cata_thread_pool &operator=( const cata_thread_pool & ) = delete;

        unsigned int num_workers() const {
            return static_cast<unsigned int>( workers_.size() );
        }

        size_t queue_size() const {
            std::lock_guard<std::mutex> lk( mutex_ );
            return queue_.size();
        }

        /** Enqueue a callable for execution on a worker thread. */
        void submit( std::function<void()> task );
        /** Enqueue a callable with a stable replay-deterministic task key. */
        void submit( const rng_deterministic_key &key, std::function<void()> task );

        /**
         * Enqueue a callable that returns a value and get a future for its result.
         *
         * std::packaged_task is move-only, so it is wrapped in a shared_ptr to satisfy
         * the copyability requirement of std::function<void()>.
         *
         * Usage:
         *   std::future<int> f = pool.submit_returning( []() { return 42; } );
         *   int result = f.get();
         */
        template<typename F, typename... Args>
        auto submit_returning( F &&f, Args &&...args )
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
            using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
            auto task = std::make_shared<std::packaged_task<R()>>(
                            std::bind( std::forward<F>( f ), std::forward<Args>( args )... )
                        );
            std::future<R> fut = task->get_future();
            if( num_workers() == 0 ) {
                // Single-core fallback: execute synchronously on the calling thread
                // to avoid enqueuing work that would never be processed by a worker.
                ( *task )();
            } else {
                submit( [task]() {
                    ( *task )();
                } );
            }
            return fut;
        }

        template<typename F, typename... Args>
        auto submit_returning( const rng_deterministic_key &key, F &&f, Args &&...args )
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
            using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
            auto task = std::make_shared<std::packaged_task<R()>>(
                            std::bind( std::forward<F>( f ), std::forward<Args>( args )... )
                        );
            std::future<R> fut = task->get_future();
            const auto deterministic_seed = rng_deterministic_seed_for_current_context( key );
            if( num_workers() == 0 ) {
                if( deterministic_seed ) {
                    [[maybe_unused]] const auto deterministic_scope =
                        rng_deterministic_task_scope( *deterministic_seed );
                    ( *task )();
                } else {
                    ( *task )();
                }
            } else {
                submit( key, [task]() {
                    ( *task )();
                } );
            }
            return fut;
        }

    private:
        void worker_loop( unsigned int worker_index );

        std::vector<std::thread> workers_;
        std::deque<std::function<void()>> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        bool stop_ = false;
};

/** Returns the process-lifetime thread pool (lazy-initialized, thread-safe). */
cata_thread_pool &get_thread_pool();

/**
 * Returns true when the calling thread is a pool worker thread.
 *
 * Use this to guard main-thread-only APIs (Lua, SDL) that must not be called
 * from worker threads.  Set via a thread_local flag in worker_loop().
 */
bool is_pool_worker_thread();

/**
 * Submit a range of work items and block until all complete.
 *
 * Divides [begin, end) into up to num_workers sub-ranges and dispatches each
 * to a worker thread, then blocks until all workers have returned.
 *
 * Falls through to a direct serial loop when:
 *   - n <= 1 (trivial range, avoid dispatch overhead), or
 *   - num_workers == 0 (single-core machine).
 *
 * F must be callable as  void F(int index)
 */
template<typename F>
void parallel_for( int begin, int end, F &&f )
{
    const int n = end - begin;
    if( n <= 0 ) {
        return;
    }

    const auto call_seed = rng_next_deterministic_call_seed(
                               thread_pool_detail::parallel_for_seed_stream );
    const auto run_index = [&f, &call_seed]( const int i ) {
        thread_pool_detail::run_with_index_seed( call_seed,
                thread_pool_detail::parallel_for_seed_stream, i, f );
    };

    // Short-circuit: single item — run directly with no dispatch overhead.
    if( n == 1 ) {
        run_index( begin );
        return;
    }

    cata_thread_pool &pool = get_thread_pool();
    const int nw = static_cast<int>( pool.num_workers() );

    // Serial fallback on single-core machines.
    if( nw == 0 ) {
        for( int i = begin; i < end; ++i ) {
            run_index( i );
        }
        return;
    }

    const int chunks = std::min( n, nw );
    std::exception_ptr first_ex;
    std::mutex         ex_mutex;

    const auto run_chunk = [&]( const int chunk_begin, const int chunk_end ) {
        try {
            for( int i = chunk_begin; i < chunk_end; ++i ) {
                run_index( i );
            }
        } catch( ... ) {
            std::lock_guard<std::mutex> lock( ex_mutex );
            if( !first_ex ) {
                first_ex = std::current_exception();
            }
        }
    };

    std::latch latch( chunks );
    for( int c = 0; c < chunks; ++c ) {
        const int chunk_begin = begin + ( n * c / chunks );
        const int chunk_end   = begin + ( n * ( c + 1 ) / chunks );
        pool.submit( { .stream = thread_pool_detail::parallel_for_seed_stream,
                       .id = static_cast<std::uint64_t>( chunk_begin ) },
        [&latch, &run_chunk, chunk_begin, chunk_end]() {
            run_chunk( chunk_begin, chunk_end );
            latch.count_down();
        } );
    }
    latch.wait();
    if( first_ex ) {
        std::rethrow_exception( first_ex );
    }
}

/**
 * Like parallel_for, but dispatches one task per chunk_size indices rather
 * than dividing the range evenly by number of workers.  Useful when the
 * natural work unit has a known, fixed size.
 *
 * Falls through to a serial loop when nw == 0 or num_chunks <= 1.
 *
 * F must be callable as  void F(int index)
 */
template<typename F>
void parallel_for_chunked( int begin, int end, int chunk_size, F &&f )
{
    if( end <= begin || chunk_size <= 0 ) {
        return;
    }

    cata_thread_pool &pool = get_thread_pool();
    const int nw = static_cast<int>( pool.num_workers() );

    const int n = end - begin;
    const int num_chunks = ( n + chunk_size - 1 ) / chunk_size;

    const auto call_seed = rng_next_deterministic_call_seed(
                               thread_pool_detail::parallel_for_chunked_seed_stream );
    const auto run_index = [&f, &call_seed]( const int i ) {
        thread_pool_detail::run_with_index_seed( call_seed,
                thread_pool_detail::parallel_for_chunked_seed_stream, i, f );
    };

    if( num_chunks <= 1 ) {
        for( int i = begin; i < end; ++i ) {
            run_index( i );
        }
        return;
    }

    std::exception_ptr first_ex;
    std::mutex         ex_mutex;
    const auto run_chunk = [&]( const int chunk_begin, const int chunk_end ) {
        try {
            for( int i = chunk_begin; i < chunk_end; ++i ) {
                run_index( i );
            }
        } catch( ... ) {
            std::lock_guard<std::mutex> lock( ex_mutex );
            if( !first_ex ) {
                first_ex = std::current_exception();
            }
        }
    };

    if( nw == 0 ) {
        for( int c = 0; c < num_chunks; ++c ) {
            const int chunk_begin = begin + c * chunk_size;
            const int chunk_end   = std::min( chunk_begin + chunk_size, end );
            run_chunk( chunk_begin, chunk_end );
        }
    } else {
        std::latch latch( num_chunks );
        for( int c = 0; c < num_chunks; ++c ) {
            const int chunk_begin = begin + c * chunk_size;
            const int chunk_end   = std::min( chunk_begin + chunk_size, end );
            pool.submit( { .stream = thread_pool_detail::parallel_for_chunked_seed_stream,
                           .id = static_cast<std::uint64_t>( chunk_begin ) },
            [&latch, &run_chunk, chunk_begin, chunk_end]() {
                run_chunk( chunk_begin, chunk_end );
                latch.count_down();
            } );
        }
        latch.wait();
    }
    if( first_ex ) {
        std::rethrow_exception( first_ex );
    }
}
