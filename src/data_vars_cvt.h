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

template<typename T>
struct type_converter {
    using type = json_converter<T>;
};

} // namespace data_vars
