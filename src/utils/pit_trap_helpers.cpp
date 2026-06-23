#include "utils/pit_trap_helpers.h"

namespace pit_trap_helpers {

auto is_pit_trap_type(const trap_id& id) -> bool {
    return id == tr_pit || id == tr_spike_pit || id == tr_glass_pit;
}

auto is_same_pit_trap_type(const trap& from, const trap& to) -> bool {
    return from.loadid == to.loadid && is_pit_trap_type(from.loadid);
}

auto is_regular_pit_destination_from_pit(const trap& from, const trap& to) -> bool {
    return is_pit_trap_type(from.loadid) && to.loadid == tr_pit;
}

auto skips_pit_escape_check(const trap& from, const trap& to) -> bool {
    return is_same_pit_trap_type(from, to) || is_regular_pit_destination_from_pit(from, to);
}

} // namespace pit_trap_helpers
