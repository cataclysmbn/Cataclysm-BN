#pragma once

#include "enum_bitset.h"

#include <bitset>
#include <set>
#include <map>
#include <vector>

namespace reader_detail
{
template<typename T>
struct handler {
    static constexpr bool is_container = false;
    static constexpr bool is_indexable_container = false;
};

template<typename T> concept Container = handler<T>::is_container;

template<typename T> concept IndexableContainer = handler<T>::is_indexable_container;

template<typename T>
struct handler<std::set<T>> {
    void clear( std::set<T> &container ) const {
        container.clear();
    }
    void insert( std::set<T> &container, const T &data ) const {
        container.insert( data );
    }
    void erase( std::set<T> &container, const T &data ) const {
        container.erase( data );
    }
    static constexpr bool is_container = true;
    static constexpr bool is_indexable_container = false;
};

template<size_t N>
struct handler<std::bitset<N>> {
    void clear( std::bitset<N> &container ) const {
        container.reset();
    }
    template<typename T>
    void insert( std::bitset<N> &container, const T &data ) const {
        container.set( data );
    }
    template<typename T>
    void erase( std::bitset<N> &container, const T &data ) const {
        container.reset( data );
    }
    static constexpr bool is_container = true;
    static constexpr bool is_indexable_container = false;
};

template<typename E>
struct handler<enum_bitset<E>> {
    void clear( enum_bitset<E> &container ) const {
        container.clear_all();
    }
    template<typename T>
    void insert( enum_bitset<E> &container, const T &data ) const {
        container.set( data );
    }
    template<typename T>
    void erase( enum_bitset<E> &container, const T &data ) const {
        container.clear( data );
    }
    static constexpr bool is_container = true;
    static constexpr bool is_indexable_container = false;
};

template<typename T>
struct handler<std::vector<T>> {
    void clear( std::vector<T> &container ) const {
        container.clear();
    }
    void insert( std::vector<T> &container, const T &data ) const {
        container.push_back( data );
    }
    template<typename E>
    void erase( std::vector<T> &container, const E &data ) const {
        erase_if( container, [&data]( const T & e ) {
            return e == data;
        } );
    }
    template<typename P>
    void erase_if( std::vector<T> &container, const P &predicate ) const {
        const auto iter = std::find_if( container.begin(), container.end(), predicate );
        if( iter != container.end() ) {
            container.erase( iter );
        }
    }
    template<typename E>
    void replace( std::vector<T> &container, const E &data, const int index ) const {
        container.at( index ) = data;
    }
    static constexpr bool is_container = true;
    static constexpr bool is_indexable_container = true;
};
} // namespace reader_detail


