#include "monster_oracle.h"

#include "behavior.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "monster.h"

#include <memory>

namespace behavior {

auto monster_oracle_t::has_special() const -> status_t {
    if (subject->shortest_special_cooldown() == 0) { return running; }
    return failure;
}

auto monster_oracle_t::not_hallucination() const -> status_t {
    return subject->is_hallucination() ? failure : running;
}

auto monster_oracle_t::items_available() const -> status_t {
    if (!get_map().has_flag(TFLAG_SEALED, subject->bub_pos())
        && get_map().has_items(subject->bub_pos())) {
        return running;
    }
    return failure;
}

} // namespace behavior
