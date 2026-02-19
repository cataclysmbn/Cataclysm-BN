#include "thread_pool.h"

#include <thread>

cata_thread_pool::cata_thread_pool( unsigned int num_workers )
{
    workers_.reserve( num_workers );
    for( unsigned int i = 0; i < num_workers; ++i ) {
        workers_.emplace_back( [this]() {
            worker_loop();
        } );
    }
}

cata_thread_pool::~cata_thread_pool()
{
    {
        std::lock_guard<std::mutex> lock( mutex_ );
        stop_ = true;
    }
    cv_.notify_all();
    for( std::thread &worker : workers_ ) {
        worker.join();
    }
}

void cata_thread_pool::worker_loop()
{
    while( true ) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock( mutex_ );
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

void cata_thread_pool::submit( std::function<void()> task )
{
    {
        std::lock_guard<std::mutex> lock( mutex_ );
        queue_.push_back( std::move( task ) );
    }
    cv_.notify_one();
}

cata_thread_pool &get_thread_pool()
{
    // Worker count: hardware_concurrency()-1 so the main thread keeps one core
    // for the SDL event loop.  Falls to 0 (serial) on single-core machines.
    static cata_thread_pool pool( []() {
        const unsigned int hc = std::thread::hardware_concurrency();
        return hc > 1u ? hc - 1u : 0u;
    }
    () );
    return pool;
}
