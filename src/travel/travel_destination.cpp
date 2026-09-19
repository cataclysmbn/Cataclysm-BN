#include "travel/travel_destination.h"

#include "avatar.h"
#include "coordinates.h"

auto avatar_knows_travel_destination(const avatar& you, const tripoint_bub_ms& p) -> bool {
    return you.sees(p) || you.has_memorized_tile_for_autodrive(bub_to_abs(p));
}

auto should_draw_travel_line_overlay(const bool target_line, const bool target_known) -> bool {
    return !target_line || target_known;
}
