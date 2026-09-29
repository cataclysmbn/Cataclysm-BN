#include "../src/map/submap.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game_constants.h"
#include "int_id.h"
#include "json.h"
#include "map/field.h"
#include "type_id.h"

#include <sstream>

TEST_CASE("submap rotation", "[submap]") {
    // Corners are labelled starting from the upper-left one, clockwise.
    // NOLINTNEXTLINE(cata-point-initialization)
    constexpr auto corner_1 = point_sm_ms::zero();
    constexpr auto corner_2 = point_sm_ms{SEEX - 1, 0};
    constexpr auto corner_3 = point_sm_ms{SEEX - 1, SEEY - 1};
    constexpr auto corner_4 = point_sm_ms{0, SEEY - 1};

    constexpr auto center_1 = point_sm_ms{SEEX / 2 - 1, SEEY / 2 - 1};
    constexpr auto center_2 = point_sm_ms{SEEX / 2, SEEY / 2 - 1};
    constexpr auto center_3 = point_sm_ms{SEEX / 2, SEEY / 2};
    constexpr auto center_4 = point_sm_ms{SEEX / 2 - 1, SEEY / 2};

    GIVEN("a submap with marks") {
        submap sm(tripoint_abs_sm::zero(), {});

        sm.set_ter(corner_1, ter_id(1));
        sm.set_ter(corner_2, ter_id(2));
        sm.set_ter(corner_3, ter_id(3));
        sm.set_ter(corner_4, ter_id(4));

        sm.set_ter(center_1, ter_id(1));
        sm.set_ter(center_2, ter_id(2));
        sm.set_ter(center_3, ter_id(3));
        sm.set_ter(center_4, ter_id(4));

        WHEN("it gets rotated for 0 turns (no rotation)") {
            sm.rotate(0);

            CHECK(sm.get_ter(corner_1) == ter_id(1));
            CHECK(sm.get_ter(corner_2) == ter_id(2));
            CHECK(sm.get_ter(corner_3) == ter_id(3));
            CHECK(sm.get_ter(corner_4) == ter_id(4));

            CHECK(sm.get_ter(center_1) == ter_id(1));
            CHECK(sm.get_ter(center_2) == ter_id(2));
            CHECK(sm.get_ter(center_3) == ter_id(3));
            CHECK(sm.get_ter(center_4) == ter_id(4));
        }

        WHEN("it gets rotated for 1 turn") {
            sm.rotate(1);

            CHECK(sm.get_ter(corner_1) == ter_id(4));
            CHECK(sm.get_ter(corner_2) == ter_id(1));
            CHECK(sm.get_ter(corner_3) == ter_id(2));
            CHECK(sm.get_ter(corner_4) == ter_id(3));

            CHECK(sm.get_ter(center_1) == ter_id(4));
            CHECK(sm.get_ter(center_2) == ter_id(1));
            CHECK(sm.get_ter(center_3) == ter_id(2));
            CHECK(sm.get_ter(center_4) == ter_id(3));
        }

        WHEN("it gets rotated for 2 turns") {
            sm.rotate(2);

            CHECK(sm.get_ter(corner_1) == ter_id(3));
            CHECK(sm.get_ter(corner_2) == ter_id(4));
            CHECK(sm.get_ter(corner_3) == ter_id(1));
            CHECK(sm.get_ter(corner_4) == ter_id(2));

            CHECK(sm.get_ter(center_1) == ter_id(3));
            CHECK(sm.get_ter(center_2) == ter_id(4));
            CHECK(sm.get_ter(center_3) == ter_id(1));
            CHECK(sm.get_ter(center_4) == ter_id(2));
        }

        WHEN("it gets rotated for 3 turns") {
            sm.rotate(3);

            CHECK(sm.get_ter(corner_1) == ter_id(2));
            CHECK(sm.get_ter(corner_2) == ter_id(3));
            CHECK(sm.get_ter(corner_3) == ter_id(4));
            CHECK(sm.get_ter(corner_4) == ter_id(1));

            CHECK(sm.get_ter(center_1) == ter_id(2));
            CHECK(sm.get_ter(center_2) == ter_id(3));
            CHECK(sm.get_ter(center_3) == ter_id(4));
            CHECK(sm.get_ter(center_4) == ter_id(1));
        }
    }
}

TEST_CASE(
    "conducted_electricity_survives_submap_serialization", "[submap][field][electric][save]") {
    auto original = submap(tripoint_abs_sm::zero(), {});
    const auto pos = point_sm_ms(5, 5);
    auto& fields = original.get_field(pos);
    REQUIRE(fields.add_field(fd_electricity, 3, 1_turns));
    const auto conducted = GENERATE(false, true);
    fields.find_field(fd_electricity)->electricity_conducted = conducted;
    REQUIRE(fields.add_field(field_type_id("fd_salt_water"), 2, 1_turns));
    original.is_uniform = false;
    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    writer.start_object();
    original.store(writer);
    writer.end_object();
    auto input = std::istringstream(output.str());
    auto reader = JsonIn(input);
    const auto data = reader.get_object();
    data.allow_omitted_members();
    auto restored = submap(tripoint_abs_sm::zero(), {});
    restored.load(*data.get_raw("fields"), "fields", 0, tripoint_abs_ms::zero(), {});
    const auto* spark = restored.get_field(pos).find_field(fd_electricity);
    REQUIRE(spark != nullptr);
    CHECK(spark->electricity_conducted == conducted);
    CHECK(spark->get_field_age() == 1_turns);
    REQUIRE(restored.get_field(pos).find_field(field_type_id("fd_salt_water")) != nullptr);

    // Fresh input rearms a still-live pulse, not just a puddle whose sparks have expired.
    restored.get_field(pos).add_field(fd_electricity, 1, 0_turns);
    CHECK_FALSE(spark->electricity_conducted);
    CHECK(spark->get_field_age() == (conducted ? 0_turns : 1_turns));
}
