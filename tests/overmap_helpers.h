#pragma once

#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "coordinates.h"
#    include "game.h"
#    include "game_constants.h"
#    include "mission.h"
#    include "overmap/omdata.h"
#    include "overmap/overmap.h"
#    include "overmap/overmapbuffer.h"
#    include "type_id.h"

#    include <array>
#    include <cstddef>
#    include <string>
#    include <utility>
#    include <vector>

namespace overmap_helpers {

/// Sets every square the window can show, so that the picture comes from the code alone: terrain
/// that cycles through the kinds the cell logic tells apart, every fifth square unseen, some
/// explored and none with a note or a route mark an earlier case left.
inline void lay_terrain(const tripoint_abs_omt& center) {
    constexpr auto kinds = std::array{
        "forest", "field", "road_ns", "road_ew", "forest_trail_ns", "lake_surface", "forest_thick"};
    for (auto y = -25; y <= 25; ++y) {
        for (auto x = -40; x <= 40; ++x) {
            const auto at = center + point(x, y);
            ACTIVE_OVERMAP_BUFFER
                .ter_set(at, oter_id(kinds[static_cast<std::size_t>((x * 7 + y * 3 + 1000) % 7)]));
            ACTIVE_OVERMAP_BUFFER.set_seen(at, (x + 2 * y + 1001) % 5 != 0);
            const auto explored = (x * 3 + y + 1000) % 11 == 0 && ACTIVE_OVERMAP_BUFFER.seen(at);
            if (ACTIVE_OVERMAP_BUFFER.is_explored(at) != explored) {
                ACTIVE_OVERMAP_BUFFER.toggle_explored(at);
            }
            if (ACTIVE_OVERMAP_BUFFER.is_path(at)) { ACTIVE_OVERMAP_BUFFER.toggle_path(at); }
            if (ACTIVE_OVERMAP_BUFFER.has_note(at)) { ACTIVE_OVERMAP_BUFFER.delete_note(at); }
        }
    }
}

/// Sets the generated cities one overmap around a square aside while it lives: they depend on the
/// seed and name the squares near them, so only the cities a case lays exist.
struct cities_aside {
    struct held {
        overmap* om;
        std::vector<city> cities;
    };
    std::vector<held> aside;

    explicit cities_aside(const tripoint_abs_omt& center) {
        // The closest city is looked for one overmap around the square.
        for (auto y = -1; y <= 1; ++y) {
            for (auto x = -1; x <= 1; ++x) {
                auto* const om =
                    ACTIVE_OVERMAP_BUFFER.get_existing_om_global(center.xy() + point(x, y) * OMAPX)
                        .om;
                if (om != nullptr) {
                    aside.push_back({.om = om, .cities = std::exchange(om->cities, {})});
                }
            }
        }
    }
    cities_aside(const cities_aside&) = delete;
    auto operator=(const cities_aside&) -> cities_aside& = delete;
    ~cities_aside() {
        for (auto& [om, cities] : aside) { om->cities = std::move(cities); }
    }
};

/// A city of the overmap the avatar stands on, `offset` squares from `center`.
inline void lay_city(const tripoint_abs_omt& center, const point& offset, const std::string& name) {
    const auto at = center + offset;
    ACTIVE_OVERMAP_BUFFER.set_seen(at, true);
    auto& overmap = *ACTIVE_OVERMAP_BUFFER.get_om_global(at).om;
    auto made = city(ACTIVE_OVERMAP_BUFFER.get_om_global(at).local.xy(), 4);
    made.name = name;
    overmap.cities.push_back(made);
}

/// Makes the avatar's active mission and puts its target at `at`.
inline void lay_mission(const tripoint_abs_omt& at) {
    auto& you = get_avatar();
    you.reset_all_missions();
    mission* assigned = nullptr;
    for (const mission_type& type : mission_type::get_all()) {
        if (type.goal == MGOAL_ASSASSINATE) {
            assigned = mission::reserve_new(type.id, you.getID());
            break;
        }
    }
    REQUIRE(assigned != nullptr);
    if (assigned->get_assigned_player_id() == you.getID()) {
        you.on_mission_assignment(*assigned);
    } else {
        assigned->assign(you);
    }
    assigned->set_target(at);
}

} // namespace overmap_helpers

#endif
