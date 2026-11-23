#include "catch/catch.hpp"
#include "utils/success_chance.h"

namespace {
auto best(const int computer, const int intelligence) -> success_chance::hacker {
    return success_chance::hacker{.computer = computer, .intelligence = intelligence}.best();
}
} // namespace

TEST_CASE("success_chance_boundaries", "[success_chance]") {
    SECTION("computer hack at computer 0: INT 8 fails security 1, INT 9 passes security 6") {
        CHECK_FALSE(success_chance::computer_hack(best(0, 8), 1));
        CHECK(success_chance::computer_hack(best(0, 9), 6));
        CHECK_FALSE(success_chance::computer_hack(best(0, 9), 7));
        CHECK(success_chance::computer_hack(best(0, 1), 0));
    }
    SECTION("robot override: computer 0 at INT 8 needs difficulty 5 or less") {
        CHECK(success_chance::robot_override(best(0, 8), 5, false) >= -2);
        CHECK(success_chance::robot_override(best(0, 8), 6, false) < -2);
        CHECK(success_chance::robot_override(best(1, 8), 9, false) >= -2);
        CHECK(success_chance::robot_override(best(1, 8), 10, false) < -2);
        CHECK(success_chance::robot_override(best(0, 0), 100, true) >= 0);
    }
    SECTION("vehicle hack: INT 8 needs computer 5") {
        CHECK(success_chance::vehicle_hack(best(4, 8)) < 0);
        CHECK(success_chance::vehicle_hack(best(5, 8)) >= 0);
    }
    SECTION("memory card decrypt: computer 2 needs INT 8") {
        CHECK(success_chance::decrypt_memory_card(best(1, 30)) <= 0);
        CHECK(success_chance::decrypt_memory_card(best(2, 7)) <= 0);
        CHECK(success_chance::decrypt_memory_card(best(2, 8)) > 0);
        // INT 0: rng(1, 0) returns 0 or 1.
        CHECK(success_chance::decrypt_memory_card(best(6, 0)) > 0);
    }
    SECTION("fish trap: survival 0 with 20 bait needs spot 5") {
        CHECK(success_chance::fish_trap(4, 20, 0) < 0);
        CHECK(success_chance::fish_trap(5, 20, 0) >= 0);
        CHECK(success_chance::fish_trap(4, 20, 2) >= 0);
    }
}
