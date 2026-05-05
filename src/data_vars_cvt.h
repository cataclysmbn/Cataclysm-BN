#pragma once

#include <sstream>
#include <iomanip>

#include "point.h"
#include "json.h"
#include "cata_utility.h"

namespace data_vars
{

template<typename T>
struct json_converter {
    using value_type = T;

    // Need a try-catch blocks because JsonIn/Out
    // Doesn't care if you ask it to not throw on error
    // And may just do so anyway

    bool operator()( const T &in_val, std::string &out_val ) const {
        try {
            std::ostringstream os;
            JsonOut jsout{os};
            jsout.write( in_val );
            out_val = os.str();
            return true;
        } catch( JsonError &e ) {
            debugmsg( "Error writing value: %s", e.what() );
            return false;
        }
    }

    bool operator()( const std::string &in_val, T &out_val ) const {
        try {
            std::istringstream is{in_val};
            JsonIn jsin{is};
            return jsin.read( out_val, false );
        } catch( JsonError &e ) {
            debugmsg( "Error reading value '%s': %s", in_val, e.what() );
            return false;
        }
    }
};

template<typename Inner, std::size_t MaxSize, typename ... Args>
struct cached_converter {
        using key_type = std::size_t;
        using value_type = typename Inner::value_type;
    private:
        Inner _conv;

        static inline std::list<std::pair<key_type, value_type>> _list;
        static inline std::unordered_map<key_type, typename decltype( _list )::iterator> _map;

        static constexpr std::size_t _hash( const std::string &s ) {
            return std::hash<std::string> {}( s );
        }

    public:
        cached_converter( Args &&... args ) : _conv( std::forward<Args>( args )... ) {};

        bool operator()( const value_type &val, std::string &repr ) const {
            const bool res = _conv( val, repr );
            if( res ) {
                const auto key = _hash( repr );
                cache_put( key, val );
            }
            return res;
        }

        bool operator()( const std::string &repr, value_type &val ) const {
            const auto key = _hash( repr );
            if( cache_contains( key ) ) {
                val = cache_get( key );
                return true;
            }
            const bool res = _conv( repr, val );
            if( res ) {
                cache_put( key, val );
            }
            return res;
        }

        static void cache_put( const key_type &key, const value_type &value )  {
            const auto it = _map.find( key );
            _list.push_front( {key, value} );
            if( it != _map.end() ) {
                _list.erase( it->second );
                _map.erase( it );
            }
            _map[key] = _list.begin();

            if( _map.size() > MaxSize ) {
                const auto last = _list.rbegin();
                _map.erase( last->first );
                _list.pop_back();
            }
        }

        static const value_type &cache_get( const key_type &key ) {
            const auto it = _map.find( key );
            if( it == _map.end() ) {
                throw std::range_error( "Invalid key" );
            }
            _list.splice( _list.begin(), _list, it->second );
            return it->second->second;
        }

        static bool cache_contains( const key_type &key ) {
            return _map.contains( key );
        }

        static void cache_clear() {
            _list.clear();
            _map.clear();
        }

        static  size_t cache_size() {
            return _map.size();
        }

        static constexpr size_t cache_max_size() {
            return MaxSize;
        }
};

template<typename T, size_t MaxSize = 100>
using cached_json_converter = cached_converter<json_converter<T>, MaxSize>;

template<typename T>
struct type_converter {
    using type = json_converter<T>;
};

} // namespace data_vars
