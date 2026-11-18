#include "clothing_utils.h"

#include "character.h"
#include "flag.h"
#include "item.h"

auto is_compact(const item& it, const Character& c) -> bool {
    return it.has_flag(flag_COMPACT) || (it.has_flag(flag_FIT) && it.get_avg_encumber(c) <= 10);
}
