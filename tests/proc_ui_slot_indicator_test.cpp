#include "catch/catch.hpp"

#include "procgen/proc_ui_slot_indicator.h"

TEST_CASE( "proc_ui_slot_indicator_uses_ascii_cells_for_required_and_optional_slots", "[proc][ui]" )
{
    const auto slot = proc::slot_data{
        .role = "bread",
        .min = 2,
        .max = 3,
    };

    CHECK( proc::slot_indicator( slot, 0 ) == "[_ _ .]" );
    CHECK( proc::slot_indicator( slot, 1 ) == "[# _ .]" );
    CHECK( proc::slot_indicator( slot, 2 ) == "[# # .]" );
    CHECK( proc::slot_indicator( slot, 3 ) == "[# # #]" );
}

TEST_CASE( "proc_ui_slot_indicator_handles_empty_and_optional_only_slots", "[proc][ui]" )
{
    CHECK( proc::slot_indicator( proc::slot_data{ .max = 0 }, 0 ) == "[]" );
    CHECK( proc::slot_indicator( proc::slot_data{ .min = 0, .max = 2 }, 0 ) == "[. .]" );
    CHECK( proc::slot_indicator( proc::slot_data{ .min = 0, .max = 2 }, 1 ) == "[# .]" );
}
