#pragma once

/**
 * Measurement units (de-)serialization.
 */

#include "json.h"
#include "units_def.h"

template<typename T>
T read_from_json_string( JsonIn &jsin, const std::vector<std::pair<std::string, T>> &units )
{
    const size_t pos = jsin.tell();
    size_t i = 0;
    const auto error = [&]( const char *const msg ) {
        jsin.seek( pos + i );
        jsin.error( msg );
    };

    const std::string s = jsin.get_string();
    // returns whether we are at the end of the string
    const auto skip_spaces = [&]() {
        while( i < s.size() && s[i] == ' ' ) {
            ++i;
        }
        return i >= s.size();
    };
    const auto get_unit = [&]() {
        if( skip_spaces() ) {
            error( "invalid quantity string: missing unit" );
        }
        for( const auto &pair : units ) {
            const std::string &unit = pair.first;
            if( s.size() >= unit.size() + i && s.compare( i, unit.size(), unit ) == 0 ) {
                i += unit.size();
                return pair.second;
            }
        }
        error( "invalid quantity string: unknown unit" );
        // above always throws but lambdas cannot be marked [[noreturn]]
        throw;
    };

    if( skip_spaces() ) {
        error( "invalid quantity string: empty string" );
    }
    T result{};
    do {
        int sign_value = +1;
        if( s[i] == '-' ) {
            sign_value = -1;
            ++i;
        } else if( s[i] == '+' ) {
            ++i;
        }
        if( i >= s.size() || !isdigit( s[i] ) ) {
            error( "invalid quantity string: number expected" );
        }
        int value = 0;
        for( ; i < s.size() && isdigit( s[i] ); ++i ) {
            value = value * 10 + ( s[i] - '0' );
        }
        result += units::multiply_any_unit( get_unit(), sign_value * value );
    } while( !skip_spaces() );
    return result;
}

template<typename T>
void dump_to_json_string( T t, JsonOut &jsout,
                          const std::vector<std::pair<std::string, T>> &units )
{
    // deduplicate unit strings and choose the shortest representations
    std::map<T, std::string> sorted_units;
    for( const auto &p : units ) {
        const auto it = sorted_units.find( p.second );
        if( it != sorted_units.end() ) {
            if( p.first.length() < it->second.length() ) {
                it->second = p.first;
            }
        } else {
            sorted_units.emplace( p.second, p.first );
        }
    }
    std::string str;
    bool written = false;
    for( auto it = sorted_units.rbegin(); it != sorted_units.rend(); ++it ) {
        const int val = static_cast<int>( t / it->first );
        if( val != 0 ) {
            if( written ) {
                str += ' ';
            }
            int tmp = val;
            if( tmp < 0 ) {
                str += '-';
                tmp = -tmp;
            }
            const size_t val_beg = str.size();
            while( tmp != 0 ) {
                str += static_cast<char>( '0' + tmp % 10 );
                tmp /= 10;
            }
            std::reverse( str.begin() + val_beg, str.end() );
            str += ' ';
            str += it->second;
            written = true;
            t -= it->first * val;
        }
    }
    if( str.empty() ) {
        str = "0 " + sorted_units.begin()->second;
    }
    jsout.write( str );
}


