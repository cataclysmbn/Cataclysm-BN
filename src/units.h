#pragma once

#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "units_def.h"

#include "units_angle.h"
#include "units_volume.h"
#include "units_mass.h"
#include "units_energy.h"
#include "units_money.h"
#include "units_temperature.h"
#include "units_probability.h"

namespace units
{

// Streaming operators for debugging and tests
// (for UI output other functions should be used which render in the user's
// chosen units)
inline std::ostream &operator<<( std::ostream &o, mass_in_milligram_tag )
{
    return o << "mg";
}

inline std::ostream &operator<<( std::ostream &o, volume_in_milliliter_tag )
{
    return o << "ml";
}

inline std::ostream &operator<<( std::ostream &o, energy_in_joule_tag )
{
    return o << "J";
}

inline std::ostream &operator<<( std::ostream &o, money_in_cent_tag )
{
    return o << "cent";
}

inline std::ostream &operator<<( std::ostream &o, angle_in_radians_tag )
{
    return o << "rad";
}

inline std::ostream &operator<<( std::ostream &o, temperature_in_millidegree_celsius_tag )
{
    return o << "mC";
}

inline std::ostream &operator<<( std::ostream &o, probability_in_one_in_million_tag )
{
    return o << "pm";
}

template<typename value_type, typename tag_type>
inline std::ostream &operator<<( std::ostream &o, const quantity<value_type, tag_type> &v )
{
    return o << v.value() << tag_type{};
}

std::string display( units::energy v );

} // namespace units

namespace units
{
static const std::vector<std::pair<std::string, energy>> energy_units = { {
        { "mJ", 1_J }, // Millijoules are depreciated, this is only defined to migrate old saves.
        { "J", 1_J },
        { "kJ", 1_kJ },
    }
};
static const std::vector<std::pair<std::string, mass>> mass_units = { {
        { "mg", 1_milligram },
        { "g", 1_gram },
        { "kg", 1_kilogram },
    }
};
static const std::vector<std::pair<std::string, money>> money_units = { {
        { "cent", 1_cent },
        { "USD", 1_USD },
        { "kUSD", 1_kUSD },
    }
};
static const std::vector<std::pair<std::string, volume>> volume_units = { {
        { "ml", 1_ml },
        { "L", 1_liter }
    }
};
static const std::vector<std::pair<std::string, angle>> angle_units = { {
        { "arcmin", 1_arcmin },
        { "°", 1_degrees },
        { "rad", 1_radians },
    }
};
static const std::vector<std::pair<std::string, temperature>> temperature_units = { {
        { "mC", 1_mc },
        { "C", 1_c },
        // Fahrenheit not supported
        // { "f", 1_f },
    }
};
static const std::vector<std::pair<std::string, probability>> probability_units = { {
        { "pm", 1_pm },
        { "pct", 1_pct },
        { "%", 1_pct }
    }
};
} // namespace units


