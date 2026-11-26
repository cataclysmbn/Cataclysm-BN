#include "photo_morale.h"

#include "calendar.h"
#include "character.h"
#include "morale_types.h"
#include "rng.h"
#include "type_id.h"

#include <string>

auto apply_photo_viewing_morale(Character& viewer) -> void {
    if (viewer.has_trait(trait_id("PSYCHOPATH"))) { return; }
    const auto ready_at = viewer.get_value_as_int("digital_photos_ready").value_or(0);
    if (time_point::from_turn(ready_at) > calendar::turn) { return; }
    viewer.add_morale(MORALE_PHOTOS, rng(15, 30), 100);
    viewer.set_value(
        "digital_photos_ready", std::to_string(to_turn<int>(calendar::turn + 10_minutes)));
}
