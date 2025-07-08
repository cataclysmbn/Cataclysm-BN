#pragma once

#include <memory>

template<typename T>
class location;

template<typename T>
class detached_ptr;

template<typename T, bool error_if_null = true>
class location_ptr
{
    private:
        friend detached_ptr<T>;
        friend location_ptr < T, !error_if_null >;
        T *ptr = nullptr;
        std::unique_ptr<location<T>> loc;

        void update_location();

        void unset_location();

    public:
        location_ptr( location<T> *loc );
        location_ptr( const location_ptr & ) = delete;
        location_ptr( location_ptr && ) noexcept;
        location_ptr &operator=( location_ptr<T, error_if_null> & ) = delete;

        location_ptr<T, error_if_null> &operator=( detached_ptr<T> &&source );
        location_ptr<T, error_if_null> &operator=( location_ptr<T, true> &&source );
        location_ptr<T, error_if_null> &operator=( location_ptr<T, false> &&source );

        ~location_ptr();

        detached_ptr<T> swap( detached_ptr<T> &&with );

        detached_ptr<T> release();

        T *get() const;

        explicit operator bool() const;

        bool operator!() const;

        T &operator*() const;

        T *operator->() const;

        bool operator==( const T &against ) const;

        bool operator==( const T *against ) const;

        template <typename U>
        bool operator!=( const U against ) const;

        /** this is needed until vehicles are GOs */
        void set_loc_hack( location<T> *loc );
        location<T> *get_loc_hack() const;
};


