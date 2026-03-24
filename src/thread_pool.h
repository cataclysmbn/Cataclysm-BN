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

/**
 * Persistent thread pool for parallelizing game work.
 *
 * Workers are sized to hardware_concurrency() - 1 so the main thread
 * retains one core for the SDL event loop and game logic.
 *
 * Constraints (must not be violated by submitted work):
 *  - No worker thread may call any Lua API (Lua 5.3 is not reentrant).
 *  - No worker thread may call any SDL rendering API (SDL2 renderer is single-threaded).
 *
 * Threading boundaries for game systems:
 *
 *   cache_reference<T> — reference_map is guarded by reference_map_mutex_ (std::mutex).
 *     Construction and destruction of cache_reference objects is safe from worker threads.
 *
 *   safe_reference<T>  — records_by_pointer / records_by_id are NOT mutex-protected.
 *     next_id is std::atomic (safe for concurrent serialize() calls from save workers).
 *     All other safe_reference operations (fill, remove, register_load, mark_destroyed,
 *     mark_deallocated, cleanup) must run on the main thread only.
 *
 *   cata_arena<T>      — pending_deletion is NOT mutex-protected.
 *     mark_for_destruction() and cleanup() must run on the main thread only.
 *
 * In practice:
 *   • Submaps must not be destroyed on worker threads (destructor calls mark_for_destruction
 *     and safe_reference::mark_destroyed).  Use mapbuffer::drain_pending_submap_destroy()
 *     on the main thread after joining all preload_quad() futures.
 *   • Submap deserialisation IS safe from workers because
 *     active_item_cache constructs cache_reference objects, which are now mutex-guarded.
 *   • save_quad() serialisation IS safe from workers: safe_reference::serialize() only
 *     writes to next_id (atomic) and to per-item records that are never shared across quads.
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

    private:
        void worker_loop();

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

    // Short-circuit: single item — run directly with no dispatch overhead.
    if( n == 1 ) {
        f( begin );
        return;
    }

    cata_thread_pool &pool = get_thread_pool();
    const int nw = static_cast<int>( pool.num_workers() );

    // Serial fallback on single-core machines.
    if( nw == 0 ) {
        for( int i = begin; i < end; ++i ) {
            f( i );
        }
        return;
    }

    const int chunks = std::min( n, nw );
    std::latch latch( chunks );
    std::exception_ptr first_ex;
    std::mutex         ex_mutex;

    for( int c = 0; c < chunks; ++c ) {
        const int chunk_begin = begin + ( n * c / chunks );
        const int chunk_end   = begin + ( n * ( c + 1 ) / chunks );
        pool.submit( [&latch, &f, &first_ex, &ex_mutex, chunk_begin, chunk_end]() {
            try {
                for( int i = chunk_begin; i < chunk_end; ++i ) {
                    f( i );
                }
            } catch( ... ) {
                std::lock_guard<std::mutex> lock( ex_mutex );
                if( !first_ex ) {
                    first_ex = std::current_exception();
                }
            }
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

    if( nw == 0 || num_chunks <= 1 ) {
        for( int i = begin; i < end; ++i ) {
            f( i );
        }
        return;
    }

    std::latch latch( num_chunks );
    std::exception_ptr first_ex;
    std::mutex         ex_mutex;

    for( int c = 0; c < num_chunks; ++c ) {
        const int chunk_begin = begin + c * chunk_size;
        const int chunk_end   = std::min( chunk_begin + chunk_size, end );
        pool.submit( [&latch, &f, &first_ex, &ex_mutex, chunk_begin, chunk_end]() {
            try {
                for( int i = chunk_begin; i < chunk_end; ++i ) {
                    f( i );
                }
            } catch( ... ) {
                std::lock_guard<std::mutex> lock( ex_mutex );
                if( !first_ex ) {
                    first_ex = std::current_exception();
                }
            }
            latch.count_down();
        } );
    }

    latch.wait();
    if( first_ex ) {
        std::rethrow_exception( first_ex );
    }
}
