#pragma once
#ifndef CATA_TESTS_CATA_GENERATORS_H
#    define CATA_TESTS_CATA_GENERATORS_H

// Some Catch2 Generators for generating our data types

#    include "catch/catch.hpp"
#    include "game_constants.h"

struct point;
struct tripoint;

auto random_points(int low = -1000, int high = 1000) -> Catch::Generators::GeneratorWrapper<point>;

auto random_tripoints(
    int low = -1000, int high = 1000, int zlow = -OVERMAP_DEPTH, int zhigh = OVERMAP_HEIGHT)
    -> Catch::Generators::GeneratorWrapper<tripoint>;

#endif // CATA_TESTS_CATA_GENERATORS_H
