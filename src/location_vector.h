#pragma once

#include <vector>
#include <functional>
#include <memory>

#include "detached_ptr.h"

template<typename T>
class location;

struct tripoint;

class item;

template<typename T>
class location_vector;

//TODO!: This should probably just be a static swap now and leave the std version alone.
namespace std
{
template<typename T>
void swap( location_vector<T> &lhs, location_vector<T> &rhs ) noexcept ;
} // namespace std

template<typename T>
class location_vector
{
    private:
        std::unique_ptr<location<T>> loc;
        std::vector<T *> contents;
        bool destroyed = false;

        template<typename U>
        friend void std::swap( location_vector<U> &lhs, location_vector<U> &rhs ) noexcept ;

    public:
        struct const_iterator;
        struct iterator {
            public:
                friend const_iterator;
                friend location_vector;
                using iterator_category = std::random_access_iterator_tag;
                using difference_type   = std::ptrdiff_t;
                using value_type        = T*;
                using pointer           = T **;
                using reference         = T *&;

                iterator( );
                iterator( typename std::vector<T *>::iterator it, const location_vector<T> &home );
                iterator( const iterator &source );
                iterator( iterator &&source ) noexcept ;
                iterator &operator=( const iterator &source );
                iterator &operator=( iterator &&source ) noexcept ;

                reference operator*() const {
                    return *it;
                }
                pointer operator->() {
                    return &*it;
                }

                iterator &operator++() {
                    it++;
                    return *this;
                }

                iterator &operator--() {
                    it--;
                    return *this;
                }

                iterator operator++( int ) {
                    iterator tmp = *this;
                    ++( *this );
                    return tmp;
                }
                iterator operator--( int ) {
                    iterator tmp = *this;
                    --( *this );
                    return tmp;
                }

                iterator &operator+=( difference_type t ) {
                    it += t;
                    return *this;
                }

                difference_type operator-( const iterator &rhs ) const {
                    return it - rhs.it;
                }

                friend iterator operator+( difference_type n, const iterator &term ) {
                    return term + n;
                }

                friend iterator operator+( const iterator &term, difference_type n ) {
                    return iterator( term.it + n, *term.home );
                }

                friend iterator operator-( const iterator &term, difference_type n ) {
                    return iterator( term.it - n, *term.home );
                }

                friend bool operator== ( const iterator &a, const iterator &b ) {
                    return a.it == b.it;
                };
                friend bool operator!= ( const iterator &a, const iterator &b ) {
                    return a.it != b.it;
                };

                friend bool operator< ( const iterator &a, const iterator &b ) {
                    return a.it < b.it;
                };
                friend bool operator<= ( const iterator &a, const iterator &b ) {
                    return a.it <= b.it;
                };
                friend bool operator> ( const iterator &a, const iterator &b ) {
                    return a.it > b.it;
                };
                friend bool operator>= ( const iterator &a, const iterator &b ) {
                    return a.it >= b.it;
                };

            private:
                typename std::vector<T *>::iterator it;
                const location_vector<T> *home = nullptr;
        };

        struct const_iterator {
            public:
                friend iterator;
                friend location_vector;
                using iterator_category = std::random_access_iterator_tag;
                using difference_type   = std::ptrdiff_t;
                using value_type        = T * const;
                using pointer           = T * const*;
                using reference         = T * const&;

                const_iterator( );
                const_iterator( typename std::vector<T *>::const_iterator it, const location_vector<T> &home );
                const_iterator( const iterator &source );
                const_iterator( iterator &&source );
                const_iterator( const const_iterator &source );
                const_iterator( const_iterator &&source ) noexcept ;
                const_iterator &operator=( const const_iterator &source );
                const_iterator &operator=( const_iterator &&source ) noexcept ;

                reference operator*() const {
                    return *it;
                }
                pointer operator->() {
                    return &*it;
                }

                const_iterator &operator++() {
                    it++;
                    return *this;
                }

                const_iterator &operator--() {
                    it--;
                    return *this;
                }

                const_iterator operator++( int ) {
                    const_iterator tmp = *this;
                    ++( *this );
                    return tmp;
                }
                const_iterator operator--( int ) {
                    const_iterator tmp = *this;
                    --( *this );
                    return tmp;
                }

                const_iterator &operator+=( difference_type t ) {
                    it += t;
                    return *this;
                }

                difference_type operator-( const const_iterator &rhs ) const {
                    return it - rhs.it;
                }

                friend const_iterator operator+( difference_type n, const const_iterator &term ) {
                    return term + n;
                }

                friend const_iterator operator+( const const_iterator &term, difference_type n ) {
                    return const_iterator( term.it + n, *term.home );
                }

                friend const_iterator operator-( const const_iterator &term, difference_type n ) {
                    return const_iterator( term.it - n, *term.home );
                }

                friend bool operator== ( const const_iterator &a, const const_iterator &b ) {
                    return a.it == b.it;
                };
                friend bool operator!= ( const const_iterator &a, const const_iterator &b ) {
                    return a.it != b.it;
                };

                friend bool operator< ( const const_iterator &a, const const_iterator &b ) {
                    return a.it < b.it;
                };
                friend bool operator<= ( const const_iterator &a, const const_iterator &b ) {
                    return a.it <= b.it;
                };
                friend bool operator> ( const const_iterator &a, const const_iterator &b ) {
                    return a.it > b.it;
                };
                friend bool operator>= ( const const_iterator &a, const const_iterator &b ) {
                    return a.it >= b.it;
                };

            private:
                typename std::vector<T *>::const_iterator it;
                const location_vector<T> *home;
        };
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        location_vector( location<T> *loc );
        location_vector( location<T> *loc, std::vector<detached_ptr<T>> &from );
        location_vector( location_vector && ) = delete;
        location_vector &operator=( location_vector && ) noexcept ;

        ~location_vector();

        location<T> *get_location() const;

        void push_back( detached_ptr<T> &&obj );
        size_t size() const;
        bool empty() const;
        T *back() const;
        T *front() const;
        detached_ptr<T> remove( T * );
        const std::vector<T *> &as_vector() const;

        iterator erase( const_iterator it,
                        detached_ptr<T> *out = nullptr );
        iterator insert( iterator it,
                         detached_ptr<T> &&obj );

        iterator insert( iterator it,
                         typename std::vector<detached_ptr<T>>::iterator start,
                         typename std::vector<detached_ptr<T>>::iterator end );
        const_iterator begin() const;
        const_iterator end() const;
        iterator begin();
        iterator end();
        const_reverse_iterator rbegin() const;
        const_reverse_iterator rend() const;
        const_reverse_iterator crbegin() const;
        const_reverse_iterator crend() const;
        reverse_iterator rbegin();
        reverse_iterator rend();
        const_iterator cbegin() const;
        const_iterator cend() const;
        std::vector<detached_ptr<T>> clear();

        void remove_with( std::function < detached_ptr<T>( detached_ptr<T> && ) > cb );

        void move_by( tripoint offset );

        /** this is needed until vehicles are GOs */
        void set_loc_hack( location<T> *loc );

        void on_destroy();
};


