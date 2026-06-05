#pragma once

#include <mutex>
#include <set>
#include <unordered_map>

#include "safe_reference.h"

template <typename T>
class cata_arena
{
    private:
        std::set<T *> pending_deletion;
        std::mutex pending_deletion_mutex;

        static cata_arena<T> &get_instance() {
            // Heap-allocated and never deleted — intentional. This avoids the static
            // destruction order fiasco where MAPBUFFER_REGISTRY (a global) calls
            // mark_for_destruction() during its own destruction, after a function-local
            // static cata_arena would already be destroyed. The OS reclaims all process
            // memory on exit, so not running ~cata_arena() is harmless.
            static cata_arena<T> *instance = new cata_arena<T>();
            return *instance;
        }

        void mark_for_destruction_internal( T *alloc ) {
            auto lk = std::lock_guard( pending_deletion_mutex );
            safe_reference<T>::mark_destroyed( alloc );
            cache_reference<T>::mark_destroyed( alloc );
            pending_deletion.insert( alloc );
        }

        bool cleanup_internal() {
            auto dcopy = std::set<T *> {};
            {
                auto lk = std::lock_guard( pending_deletion_mutex );
                if( pending_deletion.empty() ) {
                    return false;
                }
                dcopy.swap( pending_deletion );
                for( T * const &p : dcopy ) {
                    safe_reference<T>::mark_deallocated( p );
                }
            }
            for( T * const &p : dcopy ) {
                delete p;
            }
            return true;
        }

        cata_arena() = default;

    public:
        cata_arena( const cata_arena<T> & ) = delete;
        cata_arena( cata_arena<T> && ) = delete;

        using value_type = T;

        static void mark_for_destruction( T *alloc ) {
            get_instance().mark_for_destruction_internal( alloc );
        }

        static bool cleanup() {
            return get_instance().cleanup_internal();
        }

        ~cata_arena() {
            while( cleanup_internal() ) {}
        }
};


void cleanup_arenas();


