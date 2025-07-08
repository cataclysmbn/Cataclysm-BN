#pragma once

#include <algorithm>
#include <array>

#include "coordinate_conversions.h"
#include "point.h"
#include "scent_map.h"

struct scent_block {
    template<typename T>
    using data_block = std::array < std::array < T, SEEY + 2 >, SEEX + 2 >;

    enum data_mode {
        NONE = 0,
        SET = 1,
        MAX = 2
    };

    struct datum {
        data_mode mode;
        int intensity;
    };
    data_block<datum> assignment;

    tripoint origin;
    scent_map &scents;
    int modification_count;

    scent_block( const tripoint &sub, scent_map &scents );

    void commit_modifications();

    point index( const tripoint &p ) const {
        return -origin.xy() + p.xy();
    }

    // We should be working entirely within the range, so don't range check here
    void apply_gas( const tripoint &p, const int nintensity = 0 );
    void apply_slime( const tripoint &p, int intensity );
};


