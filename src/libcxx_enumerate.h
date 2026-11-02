#pragma once

#include <version>

// libc++ gets `views::enumerate` in LLVM 23, and Homebrew has no such build for Intel Macs.
#if defined( _LIBCPP_VERSION ) && _LIBCPP_VERSION < 230000 && _LIBCPP_STD_VER >= 23

#include <ranges>
#include <utility>

_LIBCPP_BEGIN_NAMESPACE_STD
namespace ranges::views
{

struct cata_enumerate_fn : range_adaptor_closure<cata_enumerate_fn> {
    template<viewable_range R>
    constexpr auto operator()( R &&r ) const {
        using difference = range_difference_t<R>;
        if constexpr( sized_range<R> ) {
            const auto size = static_cast<difference>( ranges::distance( r ) );
            return zip( iota( difference{ 0 }, size ), std::forward<R>( r ) );
        } else {
            return zip( iota( difference{ 0 } ), std::forward<R>( r ) );
        }
    }
};
inline constexpr auto enumerate = cata_enumerate_fn {};

} // namespace ranges::views
_LIBCPP_END_NAMESPACE_STD

#endif
