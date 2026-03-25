#pragma once

namespace proc
{

inline auto wrap_cursor( const int current, const int delta, const int size ) -> int
{
    if( size <= 0 ) {
        return 0;
    }

    const auto wrapped = ( current + delta ) % size;
    return wrapped < 0 ? wrapped + size : wrapped;
}

} // namespace proc
