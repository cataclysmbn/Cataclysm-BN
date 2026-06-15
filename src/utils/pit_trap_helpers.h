#pragma once

#include "trap.h"

namespace pit_trap_helpers {

auto is_pit_trap_type(const trap_id& id) -> bool;
auto is_same_pit_trap_type(const trap& from, const trap& to) -> bool;
auto is_regular_pit_destination_from_pit(const trap& from, const trap& to) -> bool;
auto skips_pit_escape_check(const trap& from, const trap& to) -> bool;

} // namespace pit_trap_helpers
