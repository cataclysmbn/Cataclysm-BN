#pragma once

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wmissing-noreturn"
#  pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#define CATALUA_SOL_WRAPPED

#include "sol/sol.hpp"

#undef CATALUA_SOL_WRAPPED

#include "detached_ptr.h"
template <typename T>
struct sol::unique_usertype_traits<detached_ptr<T>> {
    using type = T;
    using actual_type = detached_ptr<T>;
    static constexpr bool value = true;
    static bool is_null( const actual_type &ptr ) { return ptr == nullptr; }
    static type *get( const actual_type &ptr ) { return ptr.get(); }
};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif


