#pragma once

#include <bitset>
#include <type_traits>

#include "enum_traits.h"

template<typename E>
class enum_bitset
{
        static_assert( std::is_enum_v<E>, "the template argument is not an enum." );
        static_assert( has_enum_traits<E>::value,
                       "a specialization of 'enum_traits<E>' template containing 'last' element of the enum must be defined somewhere.  "
                       "The `last` constant must be of the same type as the enum iteslf."
                     );

    public:
        enum_bitset() = default;
        enum_bitset( const enum_bitset & ) = default;
        enum_bitset &operator=( const enum_bitset & ) = default;

        bool operator==( const enum_bitset &rhs ) const noexcept {
            return bits == rhs.bits;
        }

        bool operator!=( const enum_bitset &rhs ) const noexcept {
            return !( *this == rhs );
        }

        enum_bitset &operator&=( const enum_bitset &rhs ) noexcept {
            bits &= rhs.bits;
            return *this;
        }

        enum_bitset &operator|=( const enum_bitset &rhs ) noexcept {
            bits |= rhs.bits;
            return *this;
        }

        bool operator[]( E e ) const {
            return bits[ get_pos( e ) ];
        }

        enum_bitset &operator[]( E e ) {
            return bits[ get_pos( e ) ];
        }

        enum_bitset &set( E e, bool val = true ) {
            bits.set( get_pos( e ), val );
            return *this;
        }

        enum_bitset &set_all() {
            bits.set();
            return *this;
        }

        enum_bitset &clear( E e ) {
            bits.reset( get_pos( e ) );
            return *this;
        }

        enum_bitset &clear_all() {
            bits.reset();
            return *this;
        }

        bool test( E e ) const {
            return bits.test( get_pos( e ) );
        }

        bool test_all() const {
            return bits.all();
        }

        bool test_any() const {
            return bits.any();
        }

        size_t count() const {
            return bits.count();
        }

        static constexpr size_t size() noexcept {
            return get_pos( enum_traits<E>::last );
        }

    private:
        static constexpr size_t get_pos( E e ) noexcept {
            return static_cast<size_t>( static_cast<std::underlying_type_t<E>>( e ) );
        }

        std::bitset<enum_bitset<E>::size()> bits;
};


