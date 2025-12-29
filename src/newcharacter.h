#pragma once

#include <string>

#include "type_id.h"

class Character;

struct points_left {
    int stat_points = 0;
    int trait_points = 0;
    int skill_points = 0;

    enum point_limit : int {
        FREEFORM = 0,
        ONE_POOL,
        MULTI_POOL,
        TRANSFER,
    };
    point_limit limit = point_limit::FREEFORM;

    points_left();
    void init_from_options();
    // Highest amount of points to spend on stats without points going invalid
    int stat_points_left() const;
    int trait_points_left() const;
    int skill_points_left() const;
    bool is_freeform();
    bool is_valid();
    bool has_spare();
    std::string to_string();
};

namespace newcharacter
{
/** Returns the id of a random starting trait that costs >= 0 points */
trait_id random_good_trait();
/** Returns the id of a random starting trait that costs < 0 points */
trait_id random_bad_trait();
/**
 * Adds mandatory scenario and profession traits unless character already has them.
 * And if they do, refunds the points.
 */
void add_traits( Character &ch );
void add_traits( Character &ch, points_left &points );

/** Returns true if character has a conflicting trait to the entered trait. */
bool has_conflicting_trait( const Character &ch, const trait_id &t );
/** Returns true if charcater has a trait which upgrades into the entered trait. */
bool has_lower_trait( const Character &ch, const trait_id &t );
/** Returns true if character has a trait which is an upgrade of the entered trait. */
bool has_higher_trait( const Character &ch, const trait_id &t );
/** Returns true if character has a trait that shares a type with the entered trait. */
bool has_same_type_trait( const Character &ch, const trait_id &t );
} // namespace newcharacter


