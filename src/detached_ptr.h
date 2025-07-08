#pragma once

template <typename T>
class game_object;
template <typename T, bool error_if_null>
class location_ptr;
template <typename T>
class location_vector;
template <typename T>
class location_visitable;
class location_inventory;

template<typename T>
class detached_ptr
{
    private:
        friend T;
        friend game_object<T>;
        friend location_ptr<T, true>;
        friend location_ptr<T, false>;
        friend location_vector<T>;
        friend location_inventory;
        friend location_visitable<location_inventory>;
        T *ptr = nullptr;
    public:
        detached_ptr();
        detached_ptr( detached_ptr &&source ) noexcept;
        detached_ptr<T> &operator=( detached_ptr &&source ) noexcept;

        detached_ptr( const detached_ptr & ) = delete;
        detached_ptr<T> &operator=( const detached_ptr & ) = delete;

        explicit detached_ptr( location_ptr<T, true> &&loc );
        explicit detached_ptr( location_ptr<T, false> &&loc );
        ~detached_ptr();

        T *get() const;

        explicit operator bool() const;
        bool operator!() const;
        T &operator*() const;
        T *operator->() const;
        bool operator==( const T &against ) const;
        bool operator==( const T *against ) const;
        template <typename U>
        bool operator!=( const U against ) const;
    private:
        explicit detached_ptr( T *obj );
        T *release();
};

