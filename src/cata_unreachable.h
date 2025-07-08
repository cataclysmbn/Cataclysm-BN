#pragma once

namespace cata
{

/**
 * @brief Marks unreachable code.
 *
 * Utility function to mark unreachable code to help compiler with optimizations.
 *
 * Usage:
 *     void ( bool always_true )
 *     {
 *       if ( always_true ) {
 *         return;
 *       } else {
 *         // If always_true happens to be false, this will cause Undefined Behavior.
 *         cata::unreachable();
 *       }
 *     }
 *
 * Source: https://stackoverflow.com/a/65258501
 */
#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with GCC (-std=c++0x or above)
[[noreturn]] inline __attribute__( ( always_inline ) ) void unreachable()
{
    __builtin_unreachable();
}
#elif defined(_MSC_VER) // MSVC
[[noreturn]] __forceinline void unreachable()
{
    __assume( false );
}
#else // ???
inline void unreachable() {}
#endif

} // namespace cata


