#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct bash_params {
    // Initial strength
    int strength;
    // Make a sound?
    bool silent;
    // Essentially infinite bash strength + some
    bool destroy;
    // Do we want to bash floor if no furn/wall exists?
    bool bash_floor;
    /**
     * Value from 0.0 to 1.0 that affects interpolation between str_min and str_max
     * At 0.0, the bash is against str_min of targeted objects
     * This is required for proper "piercing" bashing, so that one strong hit
     * can destroy a wall and a floor under it rather than only one at a time.
     */
    float roll;
    /*
     * Are we bashing this location from above?
     * Used in determining what sort of terrain the location will turn into,
     * since if we bashed from above and destroyed it, it probably shouldn't
     * have a roof either.
    */
    bool bashing_from_above;
    /**
     * Hack to prevent infinite recursion.
     * TODO: Remove, properly unwrap the calls instead
     */
    bool do_recurse = true;
    // Was this bash action directly caused by the avatar?
    bool caused_by_player = false;
};

struct bash_results {
    bash_results( bool did_bash, bool success, bool bashed_solid )
        : did_bash( did_bash ), success( success ), bashed_solid( bashed_solid )
    {}
    bash_results() = default;
    // Was anything hit?
    bool did_bash = false;
    // Was anything destroyed?
    bool success = false;
    // Did we bash furniture, terrain or vehicle
    bool bashed_solid = false;
    // If there was recurrent bashing, it will be here
    std::vector<bash_results> subresults;

    bash_results &operator|=( const bash_results &other );
};
