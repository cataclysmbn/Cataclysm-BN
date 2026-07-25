#include "catch/catch.hpp"
#include "damage.h"
#include "state_helpers.h"
#include "type_id.h"
#include "veh_type.h"

#include <array>

TEST_CASE("verify_copy_from_gets_damage_reduction", "[vehicle]") {
    clear_all_state();
    // Picking halfboard_horizontal as a vpart which is likely to remain
    // defined via copy-from, and which should have non-zero damage reduction.
    const vpart_info& vp = vpart_id("halfboard_horizontal").obj();
    CHECK(vp.damage_reduction.type_resist(DT_BASH) != 0);
}
