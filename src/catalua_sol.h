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

#ifdef __clang__
#  pragma clang diagnostic pop
#endif


