#include "catch/catch.hpp"
#include "editor/row_id.h"

TEST_CASE("a row id names its point and index only after its own kind", "[editor]") {
    CHECK(editor::row_point(editor::point_row("square", point(3, -4)), "square") == point(3, -4));
    CHECK(editor::row_index(editor::index_row("part", 12), "part") == 12);
    CHECK(editor::row_rest("symbol:#", "symbol") == "#");
    CHECK(editor::row_rest("symbol:", "symbol") == "");

    CHECK_FALSE(editor::row_point("mount:3,4", "square"));
    CHECK_FALSE(editor::row_point("square:3", "square"));
    CHECK_FALSE(editor::row_point("square:3,4x", "square"));
    CHECK_FALSE(editor::row_index("parts:1", "part"));
    CHECK_FALSE(editor::row_index("part:-1", "part"));
    CHECK_FALSE(editor::row_index("part:", "part"));
    CHECK_FALSE(editor::row_rest("", "symbol"));
    CHECK_FALSE(editor::row_rest("symbolic", "symbol"));
}
