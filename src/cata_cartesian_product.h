#pragma once

// Compatibility shim for std::views::cartesian_product (C++23, P2374R4).
//
// Apple's system libc++ ships without this feature even when the compiler
// accepts -std=c++23, so we provide a minimal lazy fallback that covers
// the 2-range and 3-range cases used in the codebase.
//
// Usage:  replace  std::views::cartesian_product(...)
//         with     cata::views::cartesian_product(...)

#include <ranges>
#include <tuple>
#include <version>

#if defined( __cpp_lib_ranges_cartesian_product ) && \
    __cpp_lib_ranges_cartesian_product >= 202207L

namespace cata::views
{
using std::views::cartesian_product;
} // namespace cata::views

#else // fallback implementation

namespace cata::views
{

namespace detail
{

// --- 2-range cartesian product ---

template<std::ranges::input_range R1, std::ranges::input_range R2>
class cartesian_product_view_2
    : public std::ranges::view_interface<cartesian_product_view_2<R1, R2>>
{
        R1 r1_;
        R2 r2_;
    public:
        class iterator
        {
                using It1 = std::ranges::iterator_t<R1>;
                using It2 = std::ranges::iterator_t<R2>;
                It1 it1_;
                It2 it2_;
                It1 end1_;
                It2 begin2_;
                It2 end2_;
            public:
                using value_type =
                    std::tuple<std::ranges::range_value_t<R1>, std::ranges::range_value_t<R2>>;
                using difference_type = std::ptrdiff_t;
                using iterator_category = std::input_iterator_tag;

                iterator() = default;
                iterator( It1 it1, It1 end1, It2 begin2, It2 end2 )
                    : it1_( it1 ), it2_( begin2 ), end1_( end1 ), begin2_( begin2 ), end2_( end2 ) {
                    if( it1_ != end1_ && begin2_ == end2_ ) {
                        it1_ = end1_;
                    }
                }

                auto operator*() const -> value_type {
                    return { *it1_, *it2_ };
                }
                auto operator++() -> iterator & { // *NOPAD*
                    ++it2_;
                    if( it2_ == end2_ ) {
                        it2_ = begin2_;
                        ++it1_;
                    }
                    return *this;
                }
                auto operator++( int ) -> iterator {
                    auto tmp = *this;
                    ++( *this );
                    return tmp;
                }
                friend auto operator==( const iterator &a, const iterator &b ) -> bool {
                    return a.it1_ == b.it1_ && ( a.it1_ == a.end1_ || a.it2_ == b.it2_ );
                }
        };

        cartesian_product_view_2() = default;
        cartesian_product_view_2( R1 r1, R2 r2 )
            : r1_( std::move( r1 ) ), r2_( std::move( r2 ) ) {}

        auto begin() -> iterator {
            return iterator( std::ranges::begin( r1_ ), std::ranges::end( r1_ ),
                             std::ranges::begin( r2_ ), std::ranges::end( r2_ ) );
        }
        auto end() -> iterator {
            return iterator( std::ranges::end( r1_ ), std::ranges::end( r1_ ),
                             std::ranges::begin( r2_ ), std::ranges::end( r2_ ) );
        }
};

// --- 3-range cartesian product ---

template<std::ranges::input_range R1, std::ranges::input_range R2,
         std::ranges::input_range R3>
class cartesian_product_view_3
    : public std::ranges::view_interface<cartesian_product_view_3<R1, R2, R3>>
{
        R1 r1_;
        R2 r2_;
        R3 r3_;
    public:
        class iterator
        {
                using It1 = std::ranges::iterator_t<R1>;
                using It2 = std::ranges::iterator_t<R2>;
                using It3 = std::ranges::iterator_t<R3>;
                It1 it1_;
                It2 it2_;
                It3 it3_;
                It1 end1_;
                It2 begin2_;
                It2 end2_;
                It3 begin3_;
                It3 end3_;
            public:
                using value_type =
                    std::tuple<std::ranges::range_value_t<R1>, std::ranges::range_value_t<R2>,
                    std::ranges::range_value_t<R3>>;
                using difference_type = std::ptrdiff_t;
                using iterator_category = std::input_iterator_tag;

                iterator() = default;
                iterator( It1 it1, It1 end1, It2 begin2, It2 end2, It3 begin3, It3 end3 )
                    : it1_( it1 ), it2_( begin2 ), it3_( begin3 ),
                      end1_( end1 ), begin2_( begin2 ), end2_( end2 ),
                      begin3_( begin3 ), end3_( end3 ) {
                    if( it1_ != end1_ && ( begin2_ == end2_ || begin3_ == end3_ ) ) {
                        it1_ = end1_;
                    }
                }

                auto operator*() const -> value_type {
                    return { *it1_, *it2_, *it3_ };
                }
                auto operator++() -> iterator & { // *NOPAD*
                    ++it3_;
                    if( it3_ == end3_ ) {
                        it3_ = begin3_;
                        ++it2_;
                        if( it2_ == end2_ ) {
                            it2_ = begin2_;
                            ++it1_;
                        }
                    }
                    return *this;
                }
                auto operator++( int ) -> iterator {
                    auto tmp = *this;
                    ++( *this );
                    return tmp;
                }
                friend auto operator==( const iterator &a, const iterator &b ) -> bool {
                    if( a.it1_ == a.end1_ && b.it1_ == b.end1_ ) {
                        return true;
                    }
                    return a.it1_ == b.it1_ && a.it2_ == b.it2_ && a.it3_ == b.it3_;
                }
        };

        cartesian_product_view_3() = default;
        cartesian_product_view_3( R1 r1, R2 r2, R3 r3 )
            : r1_( std::move( r1 ) ), r2_( std::move( r2 ) ), r3_( std::move( r3 ) ) {}

        auto begin() -> iterator {
            return iterator( std::ranges::begin( r1_ ), std::ranges::end( r1_ ),
                             std::ranges::begin( r2_ ), std::ranges::end( r2_ ),
                             std::ranges::begin( r3_ ), std::ranges::end( r3_ ) );
        }
        auto end() -> iterator {
            return iterator( std::ranges::end( r1_ ), std::ranges::end( r1_ ),
                             std::ranges::begin( r2_ ), std::ranges::end( r2_ ),
                             std::ranges::begin( r3_ ), std::ranges::end( r3_ ) );
        }
};

} // namespace detail

struct cartesian_product_fn {
    template<std::ranges::viewable_range R1, std::ranges::viewable_range R2>
    auto operator()( R1 &&r1, R2 &&r2 ) const {
        return detail::cartesian_product_view_2<std::views::all_t<R1>, std::views::all_t<R2>>(
                   std::views::all( std::forward<R1>( r1 ) ),
                   std::views::all( std::forward<R2>( r2 ) ) );
    }
    template<std::ranges::viewable_range R1, std::ranges::viewable_range R2,
             std::ranges::viewable_range R3>
    auto operator()( R1 &&r1, R2 &&r2, R3 &&r3 ) const {
        return detail::cartesian_product_view_3<std::views::all_t<R1>, std::views::all_t<R2>,
               std::views::all_t<R3>>(
                   std::views::all( std::forward<R1>( r1 ) ),
                   std::views::all( std::forward<R2>( r2 ) ),
                   std::views::all( std::forward<R3>( r3 ) ) );
    }
};

inline constexpr cartesian_product_fn cartesian_product{};

} // namespace cata::views

#endif
