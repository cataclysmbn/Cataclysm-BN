#pragma once

#include <cassert>
#include <ranges>

#include "point.h"

// This is a template parameter, it's usually SDL_Rect, but that way the class
// can be used without include any SDL header.
template<typename RectType>
class rect_range : public std::ranges::view_interface<rect_range<RectType>>
{
    private:
        int width;
        int height;
        point count;

    public:
        rect_range( const int w, const int h, point c ) : width( w ), height( h ),
            count( c ) {
        }

        class iterator
        {
            private:
                friend class rect_range;
                int width = 0;
                int height = 0;
                int count_x = 0;
                int range_size = 0;
                int index = 0;

                auto same_range( const iterator &rhs ) const -> bool {
                    return width == rhs.width && height == rhs.height &&
                           count_x == rhs.count_x && range_size == rhs.range_size;
                }

            public:
                using value_type = RectType;
                using difference_type = int;
                using pointer = const RectType *;
                using reference = RectType;
                using iterator_category = std::random_access_iterator_tag;
                using iterator_concept = std::random_access_iterator_tag;

                iterator() = default;

                iterator( const rect_range *const r, const int i ) : index( i ) {
                    if( r ) {
                        width = r->width;
                        height = r->height;
                        count_x = r->count.x;
                        range_size = r->count.x * r->count.y;
                    }
                }

                auto operator==( const iterator &rhs ) const -> bool = default;
                auto operator<=>( const iterator &rhs ) const = default; // *NOPAD*

                auto operator*() const -> reference {
                    return { ( index % count_x ) *width, ( index / count_x ) *height, width, height };
                }

                auto operator+( const int offset ) const -> iterator {
                    auto tmp = *this;
                    tmp.index += offset;
                    return tmp;
                }

                auto operator-( const int offset ) const -> iterator {
                    auto tmp = *this;
                    tmp.index -= offset;
                    return tmp;
                }

                friend auto operator+( const difference_type offset, iterator it ) -> iterator {
                    it += offset;
                    return it;
                }

                auto operator-( const iterator &rhs ) const -> difference_type {
                    assert( same_range( rhs ) );
                    return index - rhs.index;
                }

                auto operator++() -> iterator& { // *NOPAD*
                    ++index;
                    return *this;
                }

                auto operator++( int ) -> iterator {
                    auto tmp = *this;
                    ++( *this );
                    return tmp;
                }

                auto operator--() -> iterator& { // *NOPAD*
                    --index;
                    return *this;
                }

                auto operator--( int ) -> iterator {
                    auto tmp = *this;
                    --( *this );
                    return tmp;
                }

                auto operator+=( const int offset ) -> iterator& { // *NOPAD*
                    index += offset;
                    return *this;
                }

                auto operator-=( const int offset ) -> iterator& { // *NOPAD*
                    index -= offset;
                    return *this;
                }

                auto operator[]( const int offset ) const -> reference {
                    return *( *this + offset );
                }
        };

        auto begin() const -> iterator {
            return iterator( this, 0 );
        }

        auto end() const -> iterator {
            return iterator( this, count.x * count.y );
        }
};
