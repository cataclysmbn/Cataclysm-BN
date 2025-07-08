#pragma once
#ifndef CATA_TESTS_MAP_SETUP_HELPERS_H
#define CATA_TESTS_MAP_SETUP_HELPERS_H

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "point.h"
#include "catch/catch.hpp"

namespace map_helpers
{
struct canvas_legend {
    private:
        std::unordered_map<char32_t, std::string> data;

    public:
        canvas_legend( std::unordered_map<char32_t, std::string> &&data ) : data( data ) {}
        ~canvas_legend() = default;

        static constexpr char32_t key_invalid = U'?';

        const std::string &entry( char32_t c ) const;
        char32_t key_for( const std::string &s ) const;
};

struct canvas {
    private:
        tripoint size_cache;
        std::vector<std::vector<std::u32string>> data;

        tripoint calc_size() const;

    public:
        canvas() = default;
        canvas( const tripoint &size );
        canvas( std::vector<std::u32string> &&level ) {
            data.emplace_back( std::move( level ) );
            size_cache = calc_size();
            assert_size( size() );
        }
        // Moved out to a builder method to prevent compilers (and users) from
        // getting confused by all these curly braces.
        static inline canvas make_multilevel( std::vector<std::vector<std::u32string>> &&data ) {
            canvas c;
            c.data = std::move( data );
            c.size_cache = c.calc_size();
            c.assert_size( c.size() );
            return c;
        }
        canvas( const canvas & ) = default;
        canvas( canvas && ) = default;
        ~canvas() = default;

        bool operator==( const canvas &rhs ) const {
            return data == rhs.data;
        }

        inline const tripoint &size() const {
            return size_cache;
        }

        void assert_size( const tripoint &sz ) const;
        std::string to_string() const;

        inline bool in_bounds( const tripoint &p ) const {
            return p.x >= 0 &&
                   p.y >= 0 &&
                   p.z >= 0 &&
                   p.x < size().x &&
                   p.y < size().y &&
                   p.z < size().z;
        }

        inline void set( const tripoint &p, char32_t val ) {
            assert( in_bounds( p ) );
            data[p.z][p.y][p.x] = val;
        }
        inline char32_t get( const tripoint &p ) const {
            assert( in_bounds( p ) );
            return data[p.z][p.y][p.x];
        }

        std::vector<tripoint> replace( char32_t what, char32_t with );
        tripoint replace_unique( char32_t what, char32_t with );
        std::optional<tripoint> replace_opt( char32_t what, char32_t with );

        canvas rotated( int turns ) const;
};

struct canvas_adapter {
    private:
        const canvas_legend *l = nullptr;
        std::function<std::string( const tripoint & )> getter;
        std::function<void( const tripoint &, const std::string & )> setter;

    public:
        canvas_adapter() = default;
        canvas_adapter( const canvas_legend &l ) {
            with_legend( l );
        };
        ~canvas_adapter() = default;

        inline canvas_adapter &with_legend( const canvas_legend &l ) {
            this->l = &l;
            return *this;
        }
        inline canvas_adapter &with_getter(
            std::function<std::string( const tripoint & )> f
        ) {
            getter = f;
            return *this;
        }
        inline canvas_adapter &with_setter(
            std::function<void( const tripoint &, const std::string & )> f
        ) {
            setter = f;
            return *this;
        }

        void set_all( const canvas &c );

        canvas extract_to_canvas( const tripoint &sz );

        void check_matches_expected( const canvas &expected, bool require );
};
} // namespace map_helpers

namespace Catch
{
template<>
struct StringMaker<map_helpers::canvas> {
    static std::string convert( const map_helpers::canvas &c ) {
        return c.to_string();
    }
};
} // namespace Catch

#endif // CATA_TESTS_MAP_SETUP_HELPERS_H
