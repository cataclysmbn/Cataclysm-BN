#pragma once

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <latch>
#include <mutex>
#include <thread>
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

        /** Enqueue a callable for execution on a worker thread. */
        void submit( std::function<void()> task );

    private:
        void worker_loop();

        std::vector<std::thread> workers_;
        std::deque<std::function<void()>> queue_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool stop_ = false;
};

/** Returns the process-lifetime thread pool (lazy-initialized, thread-safe). */
cata_thread_pool &get_thread_pool();

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
