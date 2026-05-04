#pragma once

#include "point.h"

class map;

struct take_down_deployed_furniture_options {
    map &here;
    tripoint furniture_pos;
    tripoint drop_pos;
};

auto take_down_deployed_furniture( const take_down_deployed_furniture_options &opts ) -> void;
