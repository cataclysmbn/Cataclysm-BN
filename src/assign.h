#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <optional>

#include "calendar.h"
#include "color.h"
#include "damage.h"
#include "debug.h"
#include "flat_set.h"
#include "json.h"
#include "units.h"
#include "units_serde.h"
#include "concepts_utility.h"

namespace detail
{
template<typename ...T>
class is_optional_helper : public std::false_type
{
};
template<typename T>
class is_optional_helper<std::optional<T>> : public std::true_type
{
};
} // namespace detail
template<typename T>
class is_optional : public detail::is_optional_helper<std::decay_t<T>>
{
};

/**
 * Check whether strict checks are enabled for given mod.
 */
bool is_strict_enabled( const std::string &src );

void report_strict_violation( const JsonObject &jo, const std::string &message,
                              const std::string &name );

template <Arithmetic T>
bool assign( const JsonObject &jo, const std::string &name, T &val, bool = false,
             T lo = std::numeric_limits<T>::lowest(), T hi = std::numeric_limits<T>::max() )
{
    T out;
    double scalar;

    // Object via which to report errors which differs for proportional/relative values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as rules
    // such as +10% are well-formed independent of whether they affect base value
    if( relative.read( name, out ) ) {
        err = relative;
        out += val;

    } else if( proportional.read( name, scalar ) ) {
        err = proportional;
        if( scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1", name );
        }
        out = val * scalar;

    } else if( !jo.read( name, out ) ) {
        return false;
    }

    if( out < lo || out > hi ) {
        err.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

// Overload assign specifically for bool to avoid warnings,
// and also to avoid potentially nonsensical interactions between relative and proportional.
bool assign( const JsonObject &jo, const std::string &name, bool &val, bool strict = false );

template <Arithmetic T>
bool assign( const JsonObject &jo, const std::string &name, std::pair<T, T> &val,
             bool = false, T lo = std::numeric_limits<T>::lowest(), T hi = std::numeric_limits<T>::max() )
{
    std::pair<T, T> out;

    if( jo.has_array( name ) ) {
        auto arr = jo.get_array( name );
        arr.read( 0, out.first );
        arr.read( 1, out.second );

    } else if( jo.read( name, out.first ) ) {
        out.second = out.first;

    } else {
        return false;
    }

    if( out.first > out.second ) {
        std::swap( out.first, out.second );
    }

    if( out.first < lo || out.second > hi ) {
        jo.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

// Note: is_optional excludes any types based on std::optional, which is
// handled below in a separate function.
template < typename T>
bool assign( const JsonObject &jo, const std::string &name, T &val, bool = false )
requires( std::is_class_v<T> && !is_optional<T>::value ) //*NOPAD*
{
    T out;
    if( !jo.read( name, out ) ) {
        return false;
    }

    val = out;

    return true;
}

namespace details
{

template <typename T, typename Set>
bool assign_set( const JsonObject &jo, const std::string &name, Set &val )
{
    JsonObject add = jo.get_object( "extend" );
    add.allow_omitted_members();
    JsonObject del = jo.get_object( "delete" );
    del.allow_omitted_members();

    if( jo.has_string( name ) || jo.has_array( name ) ) {
        val = jo.get_tags<T, Set>( name );

        if( add.has_member( name ) || del.has_member( name ) ) {
            // ill-formed to (re)define a value and then extend/delete within same definition
            jo.throw_error( "multiple assignment of value", name );
        }
        return true;
    }

    bool res = false;

    if( add.has_string( name ) || add.has_array( name ) ) {
        auto tags = add.get_tags<T>( name );
        val.insert( tags.begin(), tags.end() );
        res = true;
    }

    if( del.has_string( name ) || del.has_array( name ) ) {
        for( const auto &e : del.get_tags<T>( name ) ) {
            val.erase( e );
        }
        res = true;
    }

    return res;
}
} // namespace details


template <typename T>
typename std::enable_if<std::is_constructible<T, std::string>::value, bool>::type assign(
    const JsonObject &jo, const std::string &name, std::set<T> &val, bool = false )
{
    return details::assign_set<T, std::set<T>>( jo, name, val );
}

template <typename T>
typename std::enable_if<std::is_constructible<T, std::string>::value, bool>::type assign(
    const JsonObject &jo, const std::string &name, cata::flat_set<T> &val, bool = false )
{
    return details::assign_set<T, cata::flat_set<T>>( jo, name, val );
}

// Map of sets, like in the case of magazines for a gun
template <typename T1, typename T2>
typename std::enable_if <
std::is_constructible<T1, std::string>::value &&
std::is_constructible<T2, std::string>::value,
    bool
    >::type assign(
        const JsonObject &jo, const std::string &name, std::map<T1, std::set<T2>> &val, bool = false )
{
    JsonObject add = jo.get_object( "extend" );
    add.allow_omitted_members();
    JsonObject del = jo.get_object( "delete" );
    del.allow_omitted_members();

    if( jo.has_array( name ) ) {
        val.clear();

        for( JsonArray jarr : jo.get_array( name ) ) {
            T1 lhs = T1( jarr.get_string( 0 ) );
            for( const auto &e : jarr.get_tags( 1 ) ) {
                val[lhs].insert( T2( e ) );
            }
        }

        if( add.has_member( name ) || del.has_member( name ) ) {
            // ill-formed to (re)define a value and then extend/delete within same definition
            jo.throw_error( "multiple assignment of value", name );
        }
        return true;
    }

    bool res = false;

    if( add.has_array( name ) ) {
        JsonArray jarr_out = add.get_array( name );
        while( jarr_out.has_more() ) {
            JsonArray jarr = jarr_out.next_array();
            T1 lhs = T1( jarr.get_string( 0 ) );
            for( const auto &e : jarr.get_tags( 1 ) ) {
                val[lhs].insert( T2( e ) );
            }
        }

        res = true;
    }

    if( del.has_array( name ) ) {
        JsonArray jarr_out = add.get_array( name );
        while( jarr_out.has_more() ) {
            JsonArray jarr = jarr_out.next_array();
            T1 lhs = T1( jarr.get_string( 0 ) );
            auto iter = val.find( lhs );
            if( iter != val.end() ) {
                for( const auto &e : jarr.get_tags<T2>( 1 ) ) {
                    iter->second.erase( e );
                }
            }
            if( iter->second.empty() ) {
                val.erase( lhs );
            }
        }

        res = true;
    }

    return res;
}

/**
 * Load map from JSON array of [key, value] pairs.
 * Supports extend by pair and delete by key.
 */
template <typename T>
bool assign_map_from_array( const JsonObject &jo, const std::string &name, T &val, bool = false )
{
    using K = typename T::key_type;
    using V = typename T::mapped_type;

    JsonObject add = jo.get_object( "extend" );
    add.allow_omitted_members();
    JsonObject del = jo.get_object( "delete" );
    del.allow_omitted_members();

    if( jo.has_array( name ) ) {
        val.clear();

        std::vector<std::pair<K, V>> tmp;
        jo.get_raw( name )->read( tmp, true );
        for( const auto &it : tmp ) {
            val.insert( std::move( it ) );
        }

        if( add.has_member( name ) || del.has_member( name ) ) {
            // ill-formed to (re)define a value and then extend/delete within same definition
            jo.throw_error( "multiple assignment of value", name );
        }

        return true;
    }

    bool res = false;

    if( add.has_array( name ) ) {
        std::vector<std::pair<K, V>> tmp;
        add.get_raw( name )->read( tmp, true );

        for( const auto &it : tmp ) {
            val[std::move( it.first )] = std::move( it.second );
        }

        res = true;
    }

    if( del.has_array( name ) ) {
        std::vector<K> tmp;
        del.get_raw( name )->read( tmp, true );

        for( const auto &it : tmp ) {
            val.erase( it );
        }

        res = true;
    }

    return res;
}

bool assign( const JsonObject &jo, const std::string &name, units::volume &val,
             bool strict = false,
             const units::volume lo = units::volume_min,
             const units::volume hi = units::volume_max );

bool assign( const JsonObject &jo,
             const std::string &name,
             units::mass &val,
             bool strict = false,
             const units::mass lo = units::mass_min,
             const units::mass hi = units::mass_max );

bool assign( const JsonObject &jo,
             const std::string &name,
             units::money &val,
             bool strict = false,
             const units::money lo = units::money_min,
             const units::money hi = units::money_max );

bool assign( const JsonObject &jo,
             const std::string &name,
             units::energy &val,
             bool strict = false,
             const units::energy lo = units::energy_min,
             const units::energy hi = units::energy_max );

// Kinda hacky way to avoid allowing multiplying temperature
// For example, in 10 * 0 Fahrenheit, 10 * 0 Celsius - what's the expected result of those?
template < typename lvt, typename ut, typename s>
inline units::quantity<lvt, ut> mult_unit( const JsonObject &, const std::string &,
        const units::quantity<lvt, ut> &val, const s scalar )
requires units::quantity_details<ut>::common_zero_point::value {
    return val * scalar;
}

template < typename lvt, typename ut, typename s>
inline units::quantity<lvt, ut> mult_unit( const JsonObject &err, const std::string &name,
        const units::quantity<lvt, ut> &, const s )
requires( !units::quantity_details<ut>::common_zero_point::value )
{
    err.throw_error( "Multiplying units with multiple scales with different zero points is not well defined",
                     name );
}

template<typename T, typename F>
inline bool assign_unit_common( const JsonObject &jo, const std::string &name, T &val, F parse,
                                bool, const T lo, const T hi )
{
    T out;

    // Object via which to report errors which differs for proportional/relative values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as rules
    // such as +10% are well-formed independent of whether they affect base value
    if( relative.has_member( name ) ) {
        T tmp;
        err = relative;
        if( !parse( err, tmp ) ) {
            err.throw_error( "invalid relative value specified", name );
        }
        out = val + tmp;

    } else if( proportional.has_member( name ) ) {
        double scalar;
        err = proportional;
        if( !err.read( name, scalar ) || scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1", name );
        }
        out = mult_unit( err, name, val, scalar );

    } else if( !parse( jo, out ) ) {
        return false;
    }

    if( out < lo || out > hi ) {
        err.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             units::probability &val,
             bool strict = false,
             const units::probability lo = units::probability_min,
             const units::probability hi = units::probability_max );

bool assign( const JsonObject &jo,
             const std::string &name,
             units::temperature &val,
             bool strict = false,
             const units::temperature lo = units::temperature_min,
             const units::temperature hi = units::temperature_max );

bool assign( const JsonObject &jo,
             const std::string &name,
             nc_color &val,
             const bool strict = false );

class time_duration;

template<typename T>
inline bool
read_with_factor( const JsonObject &jo, const std::string &name, T &val, const T &factor )
requires std::is_same_v<std::decay_t<T>, time_duration> {
    int tmp;
    if( jo.read( name, tmp, false ) )
    {
        // JSON contained a raw number -> apply factor
        val = tmp * factor;
        return true;
    } else if( jo.has_string( name ) )
    {
        // JSON contained a time duration string -> no factor
        val = read_from_json_string<time_duration>( *jo.get_raw( name ), time_duration::units );
        return true;
    }
    return false;
}

// This is a function template not a real function as that allows it to be defined
// even when time_duration is *not* defined yet. When called with anything else but
// time_duration as `val`, SFINAE (the enable_if) will disable this function and it
// will be ignored. If it is called with time_duration, it is available and the
// *caller* is responsible for including the "calendar.h" header.
template<typename T>
inline typename
std::enable_if<std::is_same<typename std::decay<T>::type, time_duration>::value, bool>::type assign(
    const JsonObject &jo, const std::string &name, T &val, bool, const T &factor )
{
    T out{};
    double scalar;

    // Object via which to report errors which differs for proportional/relative values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as rules
    // such as +10% are well-formed independent of whether they affect base value
    if( read_with_factor( relative, name, out, factor ) ) {
        err = relative;
        out = out + val;

    } else if( proportional.read( name, scalar ) ) {
        err = proportional;
        if( scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1", name );
        }
        out = val * scalar;

    } else if( !read_with_factor( jo, name, out, factor ) ) {
        return false;
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             resistances &val,
             bool strict = false );

template<typename T>
inline bool assign( const JsonObject &jo, const std::string &name, std::optional<T> &val,
                    const bool strict = false )
{
    if( !jo.has_member( name ) ) {
        return false;
    }
    if( jo.has_null( name ) ) {
        val.reset();
        return true;
    }
    if( !val ) {
        val.emplace();
    }
    return assign( jo, name, *val, strict );
}

constexpr float float_max = std::numeric_limits<float>::max();

void assign_dmg_relative( damage_instance &out,
                          const damage_instance &val,
                          damage_instance relative,
                          bool &strict );

void assign_dmg_proportional( const JsonObject &jo,
                              const std::string &name,
                              damage_instance &out,
                              const damage_instance &val,
                              damage_instance proportional,
                              bool &strict );

void check_assigned_dmg( const JsonObject &err,
                         const std::string &name,
                         const damage_instance &out,
                         const damage_instance &lo_inst,
                         const damage_instance &hi_inst );

bool assign( const JsonObject &jo,
             const std::string &name,
             damage_instance &val,
             bool strict = false,
             const damage_instance &lo =
                 damage_instance( DT_NULL, 0.0f, 0.0f, 0.0f, 0.0f ),
             const damage_instance &hi = damage_instance( DT_NULL,
                     float_max,
                     float_max,
                     float_max,
                     float_max ) );

