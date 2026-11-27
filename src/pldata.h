#pragma once

#include "addiction_type.h"
#include "calendar.h"
#include "enum_conversions.h"
#include "enum_traits.h"

class JsonIn;
class JsonOut;
template <typename E> struct enum_traits;

enum class character_type : int {
    CUSTOM,
    RANDOM,
    TEMPLATE,
    NOW,
    FULL_RANDOM,
};

class addiction
{
    public:
        addiction_type_id type;
        int intensity = 0;
        time_duration sated = 1_hours;

        addiction() = default;
        addiction( const addiction_type_id &t, const int i = 1 ) : type {t}, intensity {i} { }

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
};

struct social_modifiers {
    int lie = 0;
    int persuade = 0;
    int intimidate = 0;

    social_modifiers &operator+=( const social_modifiers &other ) {
        this->lie += other.lie;
        this->persuade += other.persuade;
        this->intimidate += other.intimidate;
        return *this;
    }
};

inline social_modifiers operator+( social_modifiers lhs, const social_modifiers &rhs )
{
    lhs += rhs;
    return lhs;
}


