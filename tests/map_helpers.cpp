#include "map_helpers.h"

#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "distribution_grid.h"
#include "field.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_iterator.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "simulated_island_helpers.h"
#include "submap.h"
#include "submap_load_manager.h"
#include "type_id.h"
#include "vehicle.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// Remove all vehicles from the map
void clear_vehicles() {
    auto& map_buffer = get_map().get_mapbuffer();
    const auto owns_vehicle = [&map_buffer](vehicle* veh) {
        const auto* const owning_submap = map_buffer.lookup_submap_in_memory(veh->abs_sm_pos);
        return owning_submap != nullptr
            && std::ranges::any_of(owning_submap->vehicles, [veh](const auto& candidate) {
                   return candidate.get() == veh;
               });
    };

    std::vector<vehicle*> active_vehicles;
    active_vehicles.reserve(g->m.get_vehicles().size());

    for (wrapped_vehicle& veh : g->m.get_vehicles()) { active_vehicles.push_back(veh.v); }
    for (vehicle* veh : active_vehicles) {
        if (owns_vehicle(veh)) {
            g->m.destroy_vehicle(veh);
        } else {
            map_buffer.unregister_vehicle(veh);
        }
    }

    std::vector<vehicle*> resident_vehicles;
    resident_vehicles.reserve(map_buffer.get_vehicles().size());

    for (vehicle* veh : map_buffer.get_vehicles()) { resident_vehicles.push_back(veh); }

    for (vehicle* veh : resident_vehicles) {
        if (auto* const owning_submap = map_buffer.lookup_submap_in_memory(veh->abs_sm_pos)) {
            const auto vehicle_iter =
                std::ranges::find_if(owning_submap->vehicles, [veh](const auto& candidate) {
                    return candidate.get() == veh;
                });
            if (vehicle_iter != owning_submap->vehicles.end()) {
                owning_submap->vehicles.erase(vehicle_iter);
            }
        }
        map_buffer.unregister_vehicle(veh);
    }
}

void wipe_map_terrain() {
    auto& here = get_map();
    for (int z = -2; z <= OVERMAP_HEIGHT; ++z) {
        const ter_id terrain = z == 0 ? t_grass : z < 0 ? t_rock : t_open_air;
        for (int x = 0; x < T_MAPSIZE_X; ++x) {
            for (int y = 0; y < T_MAPSIZE_Y; ++y) {
                g->m.set(tripoint_bub_ms{x, y, z}, terrain, f_null);
            }
        }
    }
    clear_vehicles();
    g->m.clear_vehicle_cache();
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
            const tripoint_bub_sm grid_pos(x, y, zlevel);
            submap* const sm = here.get_mapbuffer().lookup_submap_in_memory(
                map_local_to_abs(here, grid_pos));
            if (sm == nullptr || sm->field_count == 0) { continue; }

            const auto clear_field_at = [&](const point_sm_ms& local) {
                const tripoint_bub_ms p = project_combine(grid_pos, local);
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
    const int mapsize = g->m.getmapsize() * SEEX;
    auto& here = get_map();
    for (int x = 0; x < mapsize; ++x) {
        for (int y = 0; y < mapsize; ++y) { here.i_clear(tripoint_bub_ms{x, y, zlevel}); }
    }
}

void clear_overmap() {
    MAPBUFFER.clear();
    ACTIVE_OVERMAP_BUFFER.clear();
    g->m.bind_dimension(g->m.get_bound_dimension());
}

void clear_map() {
    // Clearing all z-levels is rather slow, so just clear the ones I know the
    // tests use for now.
    g->update_map(g->u);
    for (int z = -2; z <= 0; ++z) { clear_fields(z); }
    wipe_map_terrain();
    clear_npcs();
    clear_creatures();
    g->m.clear_traps();
    for (int z = -2; z <= 0; ++z) { clear_items(z); }
    // Reset the distribution grid tracker so that stale grids from a previous
    // test's Catch2 WHEN section do not bleed into the next run.  The tracker
    // is a global singleton; grid_at() rebuilds on demand, so clearing here is safe.
    get_distribution_grid_tracker().clear();

    // Ensure simulated islands exist so simulated_tiles_in_radius and
    // for_each_simulated_submap work for tests that use this map.
    ensure_simulated_islands_for(test_origin);
}

void put_player_underground() {
    // Make sure the player doesn't block the path of the monster being tested.
    g->u.setpos(test_origin + tripoint_rel_ms::below() * 2);
}

auto move_player_out_of_the_way() -> void {
    g->u.setpos(map_local_to_abs(
        get_map(), tripoint_bub_ms::zero() + tripoint_rel_ms::below() * g->u.abs_pos().z()));
}

monster& spawn_test_monster(const std::string& monster_type, const tripoint_bub_ms& start) {
    monster* const added = g->place_critter_at(mtype_id(monster_type), start);
    REQUIRE(added);
    return *added;
}

// Build a map of size MAPSIZE_X x MAPSIZE_Y around tripoint_bub_ms::zero() with a given
// terrain on z=0, and no furniture, traps, or items.  Other z-levels are
// filled with default terrain so the mapbuffer has resident submaps across
// the full vertical range — otherwise map::shift interior lookups at
// z=-OVERMAP_DEPTH would find nothing.
void build_test_map(const ter_id& terrain) {
    clear_vehicles();
    MAPBUFFER.clear();
    g->m.set_abs_sub(bub_abs_sub());

    // Step 1: create uniform submaps for ALL z-levels.
    //   z=0: uniform terrain (overwritten per-tile below)
    //   z<0: uniform t_rock
    //   z>0: uniform t_open_air
    for (int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z) {
        const ter_id z_terrain = z == 0 ? terrain : z < 0 ? ter_id("t_rock") : ter_id("t_open_air");
        for (int smx = 0; smx < T_MAPSIZE; ++smx) {
            for (int smy = 0; smy < T_MAPSIZE; ++smy) {
                const tripoint_abs_sm abs_sm(g->m.get_abs_sub() + point_rel_sm(smx, smy), z);
                auto sm = std::make_unique<submap>(abs_sm, g->m.get_bound_dimension());
                sm->is_uniform = true;
                sm->set_all_ter(z_terrain);
                sm->last_touched = calendar::turn;
                MAPBUFFER.add_submap(abs_sm, sm);
            }
        }
    }

    // Step 2: per-tile setup for z=0 (terrain, furniture, traps, items).
    for (const tripoint_bub_ms& p : g->m.points_in_rectangle(
             tripoint_bub_ms::zero(), tripoint_bub_ms(T_MAPSIZE_X, T_MAPSIZE_Y, 0))) {
        g->m.furn_set(p, furn_id("f_null"));
        g->m.ter_set(p, terrain);
        g->m.trap_set(p, trap_id("tr_null"));
        g->m.i_clear(p);
    }

    // Step 3: rebind dimension — triggers refresh_active_submap_view which
    // now finds the load region and delegates to its internal view.
    g->m.bind_dimension(g->m.get_bound_dimension());
    g->m.invalidate_map_cache(0);
    g->m.build_map_cache(0, true);

    // Step 4: register simulated columns.
    const tripoint_abs_ms center =
        map_local_to_abs(g->m, tripoint_bub_ms(T_MAPSIZE_X - 1, T_MAPSIZE_Y - 1, 0));
    const point_abs_sm center_sm = project_to<coords::sm>(center).xy();
    const int radius = HALF_MAPSIZE + 1;
    std::unordered_set<point_abs_sm> columns;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            columns.insert(center_sm + point_rel_sm(dx, dy));
        }
    }
    MAPBUFFER.set_simulated_submaps(columns);

    // Step 5: sync with submap_loader for consistent entity registration.
    submap_loader.update();
}

void build_water_test_map(const ter_id& surface, const ter_id& mid, const ter_id& bottom) {
    constexpr int z_surface = 0;
    constexpr int z_bottom = -2;

    map& here = get_map();
    for (const tripoint_bub_ms& p : here.points_in_rectangle(
             tripoint_bub_ms::zero(), tripoint_bub_ms(T_MAPSIZE_X, T_MAPSIZE_Y, z_bottom))) {

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


tripoint_bub_ms bub_test_origin() { return abs_to_bub(test_origin); }


point_abs_sm bub_abs_sub() {
    return reality_bubble_origin_from_player(test_origin, T_BUBBLE_SIZE).xy();
}
