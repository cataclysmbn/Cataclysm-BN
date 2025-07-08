#pragma once

#include <type_traits>

template<typename E>
struct enum_traits;

namespace enum_traits_detail
{

template<typename E>
using last_type = std::decay_t<decltype( enum_traits<E>::last )>;

} // namespace enum_traits_detail

template<typename E, typename U = E>
struct has_enum_traits : std::false_type {};

template<typename E>
struct has_enum_traits<E, enum_traits_detail::last_type<E>> : std::true_type {};


