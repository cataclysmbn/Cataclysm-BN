#include "catch/catch.hpp"
#include "coordinates.h"
#include "creature_tracker.h"
#include "memory_fast.h"
#include "monster.h"
#include "state_helpers.h"
#include "type_id.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <vector>

namespace {

const auto mon_zombie = mtype_id("mon_zombie");

auto address_of(const shared_ptr_fast<monster>& critter) -> const monster* { return critter.get(); }

auto members_of(const Creature_tracker& tracker, const mfaction_id& faction)
    -> std::vector<const monster*> {
    using namespace std::views;
    return tracker.factions().at(faction)
         | transform([](const auto& member) -> const monster* { return member.lock().get(); })
         | std::ranges::to<std::vector>();
}

} // namespace

// Target selection walks a faction's members and breaks ties by their place in the list, so two
// processes running the same game must list them alike, wherever the allocator put each monster.
TEST_CASE(
    "faction_members_are_listed_in_the_order_they_joined_not_by_address", "[creature_tracker]") {
    namespace ranges = std::ranges;
    using namespace std::views;
    clear_all_state();
    auto tracker = Creature_tracker{};
    auto zombies =
        iota(0, 6) | transform([](const int x) {
            return make_shared_fast<monster>(mon_zombie, tripoint_bub_ms(10 + x, 10, 0));
        })
        | ranges::to<std::vector>();
    // Joining from the highest address down: a list kept in address order reads backwards.
    ranges::sort(zombies, ranges::greater{}, address_of);
    for (const auto& zombie : zombies) { REQUIRE(tracker.add(zombie)); }
    const auto faction = zombies.front()->faction;
    auto joined = zombies | transform(address_of) | ranges::to<std::vector>();

    CHECK(members_of(tracker, faction) == joined);

    SECTION("a removed member leaves the others in their order") {
        tracker.remove(*zombies[2]);
        joined.erase(joined.begin() + 2);
        CHECK(members_of(tracker, faction) == joined);
    }

    SECTION("a member that changes sides leaves its faction and joins the end of the other") {
        zombies[1]->friendly = -1;
        tracker.update_faction(*zombies[1]);
        joined.erase(joined.begin() + 1);
        CHECK(members_of(tracker, faction) == joined);
        CHECK(members_of(tracker, mfaction_str_id("player").id())
              == std::vector<const monster*>{zombies[1].get()});
    }
}
