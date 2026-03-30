#include "catch/catch.hpp"

#include "color.h"
#include "procgen/proc_ui_slot_indicator.h"

TEST_CASE( "proc_ui_slot_indicator_uses_ascii_cells_for_required_and_optional_slots", "[proc][ui]" )
{
    const auto slot = proc::slot_data{
        .role = "bread",
        .min = 2,
        .max = 3,
        .ok = {},
        .no = {},
    };

    CHECK( proc::slot_indicator( slot, 0 ) == "[! ! .]" );
    CHECK( proc::slot_indicator( slot, 1 ) == "[# ! .]" );
    CHECK( proc::slot_indicator( slot, 2 ) == "[# # .]" );
    CHECK( proc::slot_indicator( slot, 3 ) == "[# # #]" );
}

TEST_CASE( "proc_ui_slot_indicator_handles_empty_and_optional_only_slots", "[proc][ui]" )
{
    CHECK( proc::slot_indicator( proc::slot_data{ .role = "empty", .max = 0, .ok = {}, .no = {} },
                                 0 ) == "[]" );
    CHECK( proc::slot_indicator( proc::slot_data{ .role = "optional", .min = 0, .max = 2, .ok = {}, .no = {} },
                                 0 ) == "[. .]" );
    CHECK( proc::slot_indicator( proc::slot_data{ .role = "optional", .min = 0, .max = 2, .ok = {}, .no = {} },
                                 1 ) == "[# .]" );
}

TEST_CASE( "proc_ui_slot_indicator_uses_semantic_background_colors", "[proc][ui]" )
{
    const auto cells = proc::slot_indicator_cells( proc::slot_data{ .role = "bread", .min = 2, .max = 3, .ok = {}, .no = {} },
                       1, false );

    REQUIRE( cells.size() == 3 );
    CHECK( cells[0].glyph == '#' );
    CHECK( cells[0].color == c_black_green );
    CHECK( cells[1].glyph == '!' );
    CHECK( cells[1].color == c_light_gray_red );
    CHECK( cells[2].glyph == '.' );
    CHECK( cells[2].color == c_black_white );
}

TEST_CASE( "proc_ui_slot_indicator_brightens_selected_slot_background_cells", "[proc][ui]" )
{
    const auto cells = proc::slot_indicator_cells( proc::slot_data{ .role = "bread", .min = 2, .max = 3, .ok = {}, .no = {} },
                       1, true );

    REQUIRE( cells.size() == 3 );
    CHECK( cells[0].color == c_white_green );
    CHECK( cells[1].color == c_white_red );
    CHECK( cells[2].color == c_white_white );
}
