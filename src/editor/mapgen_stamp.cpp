#include "editor/mapgen_stamp.h"

#include "calendar.h"
#include "debug_log_capture.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/submap.h"
#include "mapgen/mapgen_constructor.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "rng.h"
#include "string_formatter.h"
#include "translations.h"

#include <array>
#include <optional>
#include <ranges>

namespace editor {
namespace {

/// The four submaps of an overmap square on its level.
auto submaps_of(const tripoint_abs_omt& omt) -> std::array<tripoint_abs_sm, 4> {
    const auto base = project_to<coords::sm>(omt);
    return {base, base + point_rel_sm::east(), base + point_rel_sm::south(),
            base + point_rel_sm::south_east()};
}

/// What the map caches of the level hold of the submaps just swapped.
auto refresh_caches(map& here, const int z) -> void {
    here.set_transparency_cache_dirty(z);
    here.set_outside_cache_dirty(z);
    here.set_floor_cache_dirty(z);
    here.set_pathfinding_cache_dirty(z);
    here.set_suspension_cache_dirty(z);
    here.set_absorption_cache_dirty(z);
    here.clear_vehicle_cache();
    here.clear_vehicle_list(z);
    for (const auto x : std::views::iota(0, here.getmapsize())) {
        for (const auto y : std::views::iota(0, here.getmapsize())) {
            const auto* const sm = here.get_mapbuffer().lookup_submap_in_memory(
                map_local_to_abs(here, tripoint_bub_sm(x, y, z)));
            here.update_vehicle_list(sm, z);
        }
    }
    here.reset_vehicle_cache();
    here.build_map_cache(z, false);
}

auto square_at(const stamp& stamped, const std::size_t index) -> tripoint_abs_omt {
    return stamped.at + point_rel_omt(index % stamped.grid.x(), index / stamped.grid.x());
}

} // namespace

auto stamp_mapgen(const std::string_view text, const preview_options& options)
    -> std::expected<stamp, std::string> {
    auto failed = std::optional<std::string>{};
    auto stamped = stamp{.at = options.at, .grid = point_rel_omt(1, 1)};
    capture_each_debugmsg_during([&] {
        const auto loaded = load_detached_mapgens(text);
        if (!loaded) {
            failed = loaded.error();
            return;
        }
        if (loaded->functions.empty()) {
            failed = _("Only an overmap terrain's mapgen can be stamped.");
            return;
        }
        for (const auto& function : loaded->functions) {
            if (auto pinned = pin_parameters(*function, options.pins); !pinned) {
                failed = pinned.error();
                return;
            }
        }
        stamped.grid = loaded->grid;
        auto terrains = std::vector<oter_id>{};
        for (const auto& name : loaded->om_terrains) {
            const auto terrain = overmap_terrain(name, options.direction);
            if (!terrain) {
                failed = string_format(_("No overmap terrain is named %s."), name);
                return;
            }
            terrains.push_back(*terrain);
        }
        auto& here = get_map();
        auto& overmap = get_overmapbuffer(here.get_bound_dimension());
        const auto squares = std::views::iota(std::size_t{0}, loaded->functions.size());
        // Only what the map has loaded can be swapped.
        for (const auto index : squares) {
            for (const auto& sm : submaps_of(square_at(stamped, index))) {
                if (here.get_mapbuffer().lookup_submap_in_memory(sm) == nullptr) {
                    failed = _("The entry reaches past the part of the map loaded around you.");
                    return;
                }
            }
        }
        const auto old =
            squares | std::views::transform([&](const auto index) {
                return overmap.ter(square_at(stamped, index));
            })
            | std::ranges::to<std::vector>();
        for (const auto index : squares) {
            overmap.ter_set(square_at(stamped, index), terrains[index]);
        }
        stamped.replaced = std::make_shared<mapbuffer>();
        stamped.replaced->set_dimension_id(here.get_bound_dimension());
        for (const auto index : squares) {
            auto constructor = mapgen_constructor(*stamped.replaced);
            const auto seeded = rng_deterministic_task_scope(options.seed + index);
            constructor.generate(
                square_at(stamped, index), calendar::turn,
                {.use_selected_mapgen = true, .selected_mapgen = loaded->functions[index]});
        }
        for (const auto index : squares) { overmap.ter_set(square_at(stamped, index), old[index]); }
        // What the stamp holds is the entry's terrain, which swapping puts in the overmap.
        stamped.was = std::move(terrains);
    });
    if (failed) { return std::unexpected(*failed); }
    swap_stamp(stamped);
    return stamped;
}

auto swap_stamp(stamp& stamped) -> void {
    auto& here = get_map();
    auto& overmap = get_overmapbuffer(here.get_bound_dimension());
    for (const auto index : std::views::iota(std::size_t{0}, stamped.was.size())) {
        const auto omt = square_at(stamped, index);
        for (const auto& sm : submaps_of(omt)) {
            auto* const live = here.get_mapbuffer().lookup_submap_in_memory(sm);
            auto* const held = stamped.replaced->lookup_submap_in_memory(sm);
            if (live != nullptr && held != nullptr) { submap::swap(*live, *held); }
        }
        // The overmap terrain swaps with what the stamp held of it.
        const auto now = overmap.ter(omt);
        overmap.ter_set(omt, stamped.was[index]);
        stamped.was[index] = now;
    }
    refresh_caches(here, stamped.at.z());
}

} // namespace editor
