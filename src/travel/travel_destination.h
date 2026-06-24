#pragma once

#include "coordinates.h"

class avatar;

auto avatar_knows_travel_destination(const avatar& you, const tripoint_bub_ms& p) -> bool;
auto should_draw_travel_line_overlay(bool target_line, bool target_known) -> bool;
