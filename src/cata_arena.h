#pragma once

#include <unordered_map>
#include <set>

#include "safe_reference.h"

template <typename T>
class cata_arena
{
    private:
        std::set<T *> pending_deletion;

        static cata_arena<T> &get_instance() {
            static cata_arena<T> instance;
            return instance;
        }

        void mark_for_destruction_internal( T *alloc ) {
            pending_deletion.insert( alloc );
            safe_reference<T>::mark_destroyed( alloc );
            cache_reference<T>::mark_destroyed( alloc );
        }

        bool cleanup_internal() {
            if( pending_deletion.empty() ) {
                return false;
            }
            std::set<T *> dcopy = std::set<T *>( pending_deletion );
            pending_deletion.clear();
            for( T * const &p : dcopy ) {
                safe_reference<T>::mark_deallocated( p );
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


