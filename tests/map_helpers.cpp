#include "map_helpers.h"

#include "../src/map/map.h"
#include "../src/map/mapdata.h"
#include "../src/map/submap.h"
#include "../src/overmap/overmap.h"
#include "../src/overmap/overmapbuffer.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "distribution_grid.h"
#include "game.h"
#include "game_constants.h"
#include "map/field.h"
#include "map/mapbuffer.h"
#include "map_iterator.h"
#include "npc.h"
#include "type_id.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

auto test_map_bub_origin_at_z(const map& here, const int z) -> tripoint_bub_ms {
    const auto map_origin = tripoint_abs_ms(project_to<coords::ms>(here.get_abs_sub()), z);
    return abs_to_bub(map_origin);
}

auto test_map_bub_max_at_z(const map& here, const int z) -> tripoint_bub_ms {
    const auto origin = test_map_bub_origin_at_z(here, z);
    const auto map_width = here.getmapsize() * SEEX;
    const auto map_height = here.getmapsize() * SEEY;
    return tripoint_bub_ms(origin.x() + map_width - 1, origin.y() + map_height - 1, z);
}

// This is an absolute fixture position for tests that need to move the player
// away from the fixture center.  Bubble coordinates are derived only at the
// legacy player/map API boundary.
auto test_out_of_way_position(const int z) -> tripoint_abs_ms {
    return test_origin_at_z(z) + tripoint_rel_ms(SEEX - 1, SEEY - 1, 0);
}

auto mark_test_overmap_seen_without_generation(const point_abs_omt& pos, const int z) -> void {
    auto& overmap_buffer = get_overmapbuffer(g->get_current_dimension_id());
    const auto overmap_location = overmap_buffer.get_om_global(tripoint_abs_omt(pos, z));
    overmap_location.om->seen(overmap_location.local) = true;
}

} // namespace

// Remove all vehicles from the map
void clear_vehicles() {
    std::vector<vehicle*> vehicles;
    vehicles.reserve(g->m.get_vehicles().size());

    for (wrapped_vehicle& veh : g->m.get_vehicles()) { vehicles.push_back(veh.v); }

    for (vehicle* veh : vehicles) { g->m.destroy_vehicle(veh); }
}

void wipe_map_terrain() {
    auto& here = get_map();
    for (int z = -2; z <= OVERMAP_HEIGHT; ++z) {
        const ter_id terrain = z == 0 ? t_grass : z < 0 ? t_rock : t_open_air;
        const auto map_origin = test_map_bub_origin_at_z(here, z);
        const auto map_max = test_map_bub_max_at_z(here, z);
        for (const tripoint_bub_ms& p : here.points_in_rectangle(map_origin, map_max)) {
            here.set(p, terrain, f_null);
        }
    }
    clear_vehicles();
    g->m.invalidate_map_cache(0);
    g->m.build_map_cache(0, true);
}

void clear_creatures() {
    // Remove any interfering monsters.
    g->clear_zombies();
}

void clear_npcs() {
    // Reload to ensure that all active NPCs are in the overmapbuffer.
    g->reload_npcs();
    for (npc& n : g->all_npcs()) { n.die(nullptr); }
    g->cleanup_dead();
}

void clear_fields(const int zlevel) {
    auto& here = get_map();
    const int mapsize = here.getmapsize();
    for (int x = 0; x < mapsize; ++x) {
        for (int y = 0; y < mapsize; ++y) {
            const auto grid_pos = here.get_abs_sub() + tripoint_rel_sm(x, y, zlevel);
            submap* const sm = here.get_mapbuffer().find_resident_submap(grid_pos);
            if (sm == nullptr || sm->field_count == 0) { continue; }

            const auto clear_field_at = [&](const point_sm_ms& local) {
                const auto p = abs_to_bub(project_combine(grid_pos, local));
                field& field_at_pos = sm->get_field(local);
                if (field_at_pos.field_count() == 0) { return; }

                std::vector<field_type_id> fields;
                std::ranges::transform(
                    field_at_pos, std::back_inserter(fields),
                    [](const std::pair<const field_type_id, field_entry>& pr) {
                        return pr.second.get_field_type();
                    });

                std::ranges::for_each(fields, [&](const field_type_id& f) {
                    here.remove_field(p, f);
                });
            };

            const auto field_positions = sm->field_cache;
            std::ranges::for_each(field_positions, clear_field_at);
            if (sm->field_count != 0) {
                std::ranges::for_each(submap_tiles(), clear_field_at);
                sm->field_count = 0;
            }
            sm->field_cache.clear();
        }
    }
}

void clear_items(const int zlevel) {
    auto& here = get_map();
    const auto map_origin = test_map_bub_origin_at_z(here, zlevel);
    const auto map_max = test_map_bub_max_at_z(here, zlevel);
    for (const tripoint_bub_ms& p : here.points_in_rectangle(map_origin, map_max)) {
        here.i_clear(p);
    }
}

void clear_overmap() {
    MAPBUFFER.clear();
    ACTIVE_OVERMAP_BUFFER.clear();
}

void clear_map() {
    // The fixture owns the full resident vertical range.  Clear fields and items
    // on every z-level that can be part of the loaded map so a test cannot leak
    // data into a later vertical or out-of-bubble fixture.
    for (int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z) { clear_fields(z); }
    wipe_map_terrain();
    clear_npcs();
    clear_creatures();
    g->m.clear_traps();
    for (int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z) { clear_items(z); }
    // Reset the distribution grid tracker so that stale grids from a previous
    // test's Catch2 WHEN section do not bleed into the next run.  The tracker
    // is a global singleton; grid_at() rebuilds on demand, so clearing here is safe.
    get_distribution_grid_tracker().clear();
}

void put_player_underground() {
    // Make sure the player doesn't block the path of the monster being tested.
    g->u.setpos(test_out_of_way_position(-2));
}

auto move_player_out_of_the_way() -> void {
    g->u.setpos(test_out_of_way_position(g->u.abs_pos().z()));
}

auto spawn_test_monster(const std::string& monster_type, const tripoint_bub_ms& start)
    -> monster& { // *NOPAD*
    monster* const added = g->place_critter_at(mtype_id(monster_type), start);
    REQUIRE(added);
    return *added;
}

// Build the active test map around the absolute test origin with a given terrain, and no
// furniture, traps, or items.
void build_test_map(const ter_id& terrain) {
    auto& here = get_map();
    const auto map_origin = test_map_bub_origin_at_z(here, 0);
    const auto map_max = test_map_bub_max_at_z(here, 0);
    for (const tripoint_bub_ms& p : here.points_in_rectangle(map_origin, map_max)) {
        here.furn_set(p, furn_id("f_null"));
        here.ter_set(p, terrain);
        here.trap_set(p, trap_id("tr_null"));
        here.i_clear(p);
    }

    g->m.invalidate_map_cache(0);
    g->m.build_map_cache(0, true);
}

void build_water_test_map(const ter_id& surface, const ter_id& mid, const ter_id& bottom) {
    constexpr int z_surface = 0;
    constexpr int z_bottom = -2;

    map& here = get_map();
    const auto min_omt = project_to<coords::omt>(here.get_abs_sub());
    const auto max_omt = project_to<coords::omt>(
        here.get_abs_sub() + point_rel_sm(here.getmapsize() - 1, here.getmapsize() - 1));
    for (int x = min_omt.x(); x <= max_omt.x(); ++x) {
        for (int y = min_omt.y(); y <= max_omt.y(); ++y) {
            for (const int z : {z_bottom, -1, z_surface}) {
                mark_test_overmap_seen_without_generation(point_abs_omt(x, y), z);
            }
        }
    }
    for (const tripoint_bub_ms& p : here.points_in_rectangle(
             test_map_bub_origin_at_z(here, z_surface), test_map_bub_max_at_z(here, z_bottom))) {

        if (p.z() == z_surface) {
            here.ter_set(p, surface);
        } else if (p.z() < z_surface && p.z() > z_bottom) {
            here.ter_set(p, mid);
        } else if (p.z() == z_bottom) {
            here.ter_set(p, bottom);
        }
    }

    for (const int z : {z_bottom, -1, z_surface}) {
        here.invalidate_map_cache(z);
        here.build_map_cache(z, true);
    }
}

void set_time(const time_point& time) {
    calendar::turn = time;
    g->reset_light_level();
    const auto z = g->u.bub_pos().z();
    g->m.invalidate_map_cache(z);
    g->m.build_map_cache(z);
    g->m.update_visibility_cache(z);
}

auto test_origin_at_z(const int z) -> tripoint_abs_ms {
    return tripoint_abs_ms(test_origin.xy(), z);
}

auto test_map_origin() -> point_abs_sm {
    return reality_bubble_origin_from_player(test_origin, g_reality_bubble_size).xy();
}

auto bub_test_origin() -> tripoint_bub_ms { return bub_test_origin(test_origin.z()); }

auto bub_test_origin(const int z) -> tripoint_bub_ms { return abs_to_bub(test_origin_at_z(z)); }
