#pragma once

#include <algorithm>
#include <array>

#include "coordinates.h"
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

    tripoint_bub_ms origin;
    scent_map &scents;
    int modification_count;

    scent_block( const tripoint_bub_sm &sub, scent_map &scents );

    void commit_modifications();

    point index( const tripoint_bub_ms &p ) const {
        return -origin.xy().raw() + p.xy().raw();
    }

    // We should be working entirely within the range, so don't range check here
    void apply_gas( const tripoint_bub_ms &p, const int nintensity = 0 );
    void apply_slime( const tripoint_bub_ms &p, int intensity );
};


