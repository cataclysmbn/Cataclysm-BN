#pragma once

#include <compare>
#include <type_traits>
#include "concepts_utility.h"

class JsonIn;
class JsonOut;

namespace units
{

template<Arithmetic V, typename U>
class quantity
{
    public:
        using value_type = V;
        using unit_type = U;
        using this_type = quantity<value_type, unit_type>;

        /**
         * Create an empty quantity - its @ref value_ is value initialized.
         * It does not need an explicitly named unit, it's always 0: 0 l == 0 ml == 0 Ml.
         */
        constexpr quantity() : value_() {
        }
        /**
         * Construct from value. This is supposed to be wrapped into a static
         * function (e.g. `from_liter(int)` ) to provide context.
         */
        constexpr quantity( value_type v, unit_type ) : value_( v ) {
        }
        /**
         * Conversion from other value type, e.g. from `quantity<int, foo>` to
         * `quantity<float, foo>`. The unit type stays the same!
         */
        template<Arithmetic other_value_type>
        constexpr quantity( const quantity<other_value_type, unit_type> &other ) : value_( other.value() ) {
        }

        /**
         * Access the raw dimensionless value. Use it in a properly named wrapper function only.
         */
        constexpr const value_type &value() const {
            return value_;
        }

        /**
         * The spaceship comparators, they compare the base value only.
         */
        /**@{*/
        template<Arithmetic other_value_type>
        constexpr friend bool operator==( const quantity<value_type, unit_type> &lhs,
                                          const quantity<other_value_type, unit_type> &rhs ) {
            return ( lhs <=> rhs ) == 0; // *NOPAD*
        }

        template<Arithmetic other_value_type>
        constexpr friend auto operator<=> ( const quantity<value_type, unit_type> &lhs, // *NOPAD*
                                            const quantity<other_value_type, unit_type> &rhs ) {
            return lhs.value() <=> rhs.value();  // *NOPAD*
        }
        /**@}*/

        /**
         * Addition and subtraction of quantities of the same unit type. Result is
         * a quantity with the same unit as the input.
         * Functions are templated to allow combining quantities with different `value_type`, e.g.
         * \code
         *   quantity<int, foo> a = ...;
         *   quantity<double, foo> b = ...;
         *   auto sum = a + b;
         *   static_assert(std::is_same<decltype(sum), quantity<double, foo>>::value);
         * \endcode
         *
         * Note that `+=` and `-=` accept any type as `value_type` for the other operand, but
         * they convert this back to the type of the right hand, like in `int a; a += 0.4;`
         * \code
         *   quantity<int, foo> a( 10, foo{} );
         *   quantity<double, foo> b( 0.5, foo{} );
         *   a += b;
         *   assert( a == quantity<int, foo>( 10 + 0.5, foo{} ) );
         *   assert( a == quantity<int, foo>( 10, foo{} ) );
         * \endcode
         */
        /**@{*/
        template<Arithmetic other_value_type>
        constexpr auto operator+( const quantity<other_value_type, unit_type> &rhs ) const
        -> quantity<std::common_type_t<value_type, other_value_type>, unit_type> {
            return { value_ + rhs.value(), unit_type{} };
        }

        template<Arithmetic other_value_type>
        constexpr auto operator-( const quantity<other_value_type, unit_type> &rhs ) const
        -> quantity<std::common_type_t<value_type, other_value_type>, unit_type>  {
            return { value_ - rhs.value(), unit_type{} };
        }

        template<Arithmetic other_value_type>
        auto operator+=( const quantity<other_value_type, unit_type> &rhs ) -> this_type & { // *NOPAD*
            value_ += rhs.value();
            return *this;
        }

        template<Arithmetic other_value_type>
        auto operator-=( const quantity<other_value_type, unit_type> &rhs ) -> this_type & { // *NOPAD*
            value_ -= rhs.value();
            return *this;
        }
        /**@}*/

        constexpr auto operator-() const -> this_type {
            return this_type( -value_, unit_type{} );
        }

        constexpr auto operator==( const this_type &rhs ) const -> bool {
            return value_ == rhs.value_;
        }
        constexpr auto operator<( const this_type &rhs ) const -> bool {
            return value_ < rhs.value_;
        }
        constexpr auto operator<=( const this_type &rhs ) const -> bool {
            return value_ <= rhs.value_;
        }

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );

    private:
        value_type value_;
};

template<typename ...T>
struct quantity_details {
    using common_zero_point = std::true_type;
};

template<Arithmetic V, typename U>
constexpr auto fabs( quantity<V, U> q ) -> quantity<V, U>
{
    return quantity<V, U>( std::fabs( q.value() ), U{} );
}

template<Arithmetic V, typename U>
constexpr auto fmod( quantity<V, U> num, quantity<V, U> den ) -> quantity<V, U>
{
    return quantity<V, U>( std::fmod( num.value(), den.value() ), U{} );
}

/**
 * Multiplication and division with scalars. Result is a quantity with the same unit
 * as the input.
 * Functions are templated to allow scaling with different types:
 * \code
 *   quantity<int, foo> a{ 10, foo{} };
 *   auto b = a * 4.52;
 *   static_assert(std::is_same<decltype(b), quantity<double, foo>>::value);
 * \endcode
 *
 * Note that the result for `*=` and `/=` is calculated using the given types, but is
 * implicitly converted back to `value_type` as it is stored in the operand.
 * \code
 *   quantity<int, foo> a{ 10, foo{} };
 *   a *= 4.52;
 *   assert( a == quantity<int, foo>( 10 * 4.52, foo{} ) );
 *   assert( a != quantity<int, foo>( 10 * (int)4.52, foo{} ) );
 *   assert( a == quantity<int, foo>( 45, foo{} ) );
 * \endcode
 *
 * Division of a quantity with a quantity of the same unit yields a dimensionless
 * scalar value, with the same type as the division of the contained `value_type`s:
 * \code
 *   quantity<int, foo> a{ 10, foo{} };
 *   quantity<double, foo> b{ 20, foo{} };
 *   auto proportion = a / b;
 *   static_assert(std::is_same<decltype(proportion), double>::value);
 *   assert( proportion == 10 / 20.0 );
 * \endcode
 *
 */
/**@{*/
// the decltype in the result type ensures the returned type has the same scalar type
// as you would get when performing the operation directly:
// `int * double` => `double` and `char * int` => `int`
// st is scalar type (dimensionless)
// lvt is the value type (of a quantity) on the left side, rvt is the value type on the right side
// ut is unit type (same for left and right side)
// The enable_if ensures no ambiguity, the compiler may otherwise not be able to decide whether
// "quantity / scalar" or "quantity / other_quanity" is meant.

// scalar * quantity<foo, unit> == quantity<decltype(foo * scalar), unit>
template<Arithmetic lvt, typename ut, Arithmetic st>
constexpr auto operator*( const st &factor, const quantity<lvt, ut> &rhs )
{
    static_assert( quantity_details<ut>::common_zero_point::value,
                   "Units with multiple scales with different zero should not be multiplied/divided/etc.  directly." );
    return quantity{ factor * rhs.value(), ut{} };
}

// same as above only with inverse order of operands: quantity * scalar
template<Arithmetic lvt, typename ut, Arithmetic st>
constexpr auto operator*( const quantity<lvt, ut> &lhs, const st &factor )
{
    static_assert( quantity_details<ut>::common_zero_point::value,
                   "Units with multiple scales with different zero should not be multiplied/divided/etc.  directly." );
    return quantity{ lhs.value() *factor, ut{} };
}

// Explicit "yes, I know what I'm doing" multiplication
template<Arithmetic lvt, typename ut, Arithmetic st>
constexpr auto multiply_any_unit( const quantity<lvt, ut> &lhs, const st &factor )
{
    return quantity{ lhs.value() *factor, ut{} };
}

template<typename t, Arithmetic st>
constexpr auto multiply_any_unit( const t &lhs, const st &factor )
{
    return lhs * factor;
}

// quantity<foo, unit> * quantity<bar, unit> is not supported
template<Arithmetic lvt, typename ut, Arithmetic rvt>
void operator*( quantity<lvt, ut>, quantity<rvt, ut> ) = delete;

// operator *=
template<Arithmetic lvt, typename ut, Arithmetic st>
inline auto operator*=( quantity<lvt, ut> &lhs, const st &factor ) -> quantity<lvt, ut> &
{
    lhs = lhs * factor;
    return lhs;
}

// and the revers of the multiplication above:
// quantity<foo, unit> / scalar == quantity<decltype(foo / scalar), unit>
template<Arithmetic lvt, typename ut, Arithmetic rvt>
constexpr auto operator/( const quantity<lvt, ut> &lhs, const rvt &divisor )
{
    return quantity{ lhs.value() / divisor, ut{} };
}

// scalar / quantity<foo, unit> is not supported
template<Arithmetic lvt, typename ut, Arithmetic rvt>
void operator/( lvt, quantity<rvt, ut> ) = delete;

// quantity<foo, unit> / quantity<bar, unit> == decltype(foo / bar)
template<Arithmetic lvt, typename ut, Arithmetic rvt>
constexpr auto operator/( const quantity<lvt, ut> &lhs, const quantity<rvt, ut> &rhs )
{
    return lhs.value() / rhs.value();
}

// operator /=
template<Arithmetic lvt, typename ut, Arithmetic st>
inline auto operator/=( quantity<lvt, ut> &lhs, const st &divisor ) -> quantity<lvt, ut> &
{
    lhs = lhs / divisor;
    return lhs;
}

// remainder:
// quantity<foo, unit> % scalar == quantity<decltype(foo % scalar), unit>
template<Arithmetic lvt, typename ut, Arithmetic rvt>
constexpr auto operator%( const quantity<lvt, ut> &lhs, const rvt &divisor )
{
    return quantity{ lhs.value() % divisor, ut{} };
}

// scalar % quantity<foo, unit> is not supported
template<Arithmetic lvt, typename ut, Arithmetic rvt>
void operator%( lvt, quantity<rvt, ut> ) = delete;

// quantity<foo, unit> % quantity<bar, unit> == decltype(foo % bar)
template<Arithmetic lvt, typename ut, Arithmetic rvt>
constexpr auto operator%( const quantity<lvt, ut> &lhs, const quantity<rvt, ut> &rhs )
{
    return quantity{ lhs.value() % rhs.value(), ut{} };
}

// operator %=
template<Arithmetic lvt, typename ut, Arithmetic st>
inline auto operator%=( quantity<lvt, ut> &lhs, const st &divisor ) ->  quantity<lvt, ut> &
{
    lhs = lhs % divisor;
    return lhs;
}

template<Arithmetic lvt, typename ut, Arithmetic rvt>
inline auto operator%=( quantity<lvt, ut> &lhs, const quantity<rvt, ut> &rhs )
-> quantity<lvt, ut> &
{
    lhs = lhs % rhs;
    return lhs;
}
/**@}*/
} // namespace units


