#pragma once

#include <array>
#include <vector>

#include "bodypart.h"
#include "enums.h"

struct layer_details {

    std::vector<int> pieces;
    int max = 0;
    int total = 0;

    void reset();
    int layer( int encumbrance );

    bool operator ==( const layer_details &rhs ) const {
        return max == rhs.max &&
               total == rhs.total &&
               pieces == rhs.pieces;
    }
};

struct encumbrance_data {
    int encumbrance = 0;
    int armor_encumbrance = 0;
    int layer_penalty = 0;

    std::array<layer_details, static_cast<size_t>( layer_level::MAX_CLOTHING_LAYER )>
    layer_penalty_details;

    void layer( const layer_level level, const int encumbrance ) {
        layer_penalty += layer_penalty_details[static_cast<size_t>( level )].layer( encumbrance );
    }

    void reset() {
        *this = encumbrance_data();
    }

    bool operator ==( const encumbrance_data &rhs ) const {
        return encumbrance == rhs.encumbrance &&
               armor_encumbrance == rhs.armor_encumbrance &&
               layer_penalty == rhs.layer_penalty &&
               layer_penalty_details == rhs.layer_penalty_details;
    }
};

struct char_encumbrance_data {
    std::map<bodypart_str_id, encumbrance_data> elems;
};


