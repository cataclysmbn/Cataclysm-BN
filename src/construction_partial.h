#pragma once

#include <list>

#include "item.h"
#include "type_id.h"
#include "locations.h"

struct partial_con {
    explicit partial_con( const tripoint_bub_ms &loc ) : components( new partial_con_item_location(
                    loc ) ) {}
    explicit partial_con( const tripoint_abs_ms &loc ) : components( new partial_con_item_location(
                    loc ) ) {}
    int counter = 0;
    location_vector<item> components;
    construction_id id = construction_id( -1 );
};


