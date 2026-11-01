#include "vehicle_stairs.h"

#include "map/mapbuffer.h"
#include "map/mapdata.h"
#include "translations.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

#include <algorithm>
#include <cmath>

namespace {

auto supported_endpoint(const vpart_position& vp) -> bool {
    const auto& veh = vp.vehicle();
    const auto& part = veh.cpart(vp.part_index());
    return part.is_available()
        && std::ranges::any_of(veh.parts_at_relative(part.mount, false), [&](const auto index) {
               const auto& frame = veh.cpart(index);
               return frame.is_available() && frame.info().location == "structure"
                   && !frame.info().has_flag("PROTRUSION")
                   && !frame.info().has_flag(VPFLAG_EXTENDABLE);
           });
}

} // namespace

auto vehicle_stair_at(mapbuffer& buffer, const tripoint_abs_ms& from, const int dz)
    -> std::expected<std::optional<tripoint_abs_ms>, vehicle_step_failure> {
    if (std::abs(dz) != 1) { return std::nullopt; }
    const auto source_tile = buffer.get_abs_tile(from);
    if (!source_tile) { return std::unexpected(vehicle_step_failure::outside_domain); }
    const auto up = dz > 0;
    const auto source_flag = up ? "VEHICLE_STAIRS_UP" : "VEHICLE_STAIRS_DOWN";
    const auto target_flag = up ? "VEHICLE_STAIRS_DOWN" : "VEHICLE_STAIRS_UP";
    const auto source =
        buffer.veh_at(from, {.mode = mapbuffer_lookup_mode::resident_only})
            .part_with_feature(source_flag, false);
    if (!source) { return std::nullopt; }
    const auto& veh = source->vehicle();
    if (!supported_endpoint(*source)) { return std::unexpected(vehicle_step_failure::unavailable); }
    const auto mount = source->mount() + tripoint_rel_veh(0, 0, dz);
    const auto index = veh.part_with_feature(mount, target_flag, false);
    if (index < 0) { return std::unexpected(vehicle_step_failure::unpaired); }
    const auto target = vpart_position(source->vehicle(), index);
    const auto to = veh.abs_part_location(index);
    if (to.xy() != from.xy() || to.z() - from.z() != dz) {
        return std::unexpected(vehicle_step_failure::wrong_geometry);
    }
    const auto target_tile = buffer.get_abs_tile(to);
    if (!target_tile) { return std::unexpected(vehicle_step_failure::outside_domain); }
    if (!supported_endpoint(target)) { return std::unexpected(vehicle_step_failure::unavailable); }
    // Only the paired vehicle's landing barrier is exempt. Its lower roof is never exempt.
    if (!source_tile->passable_ter_furn() || !target_tile->passable_ter_furn()
        || source->obstacle_at_part() || target.obstacle_at_part()
        || veh.roof_at_part(up ? static_cast<int>(source->part_index()) : index) >= 0
        || buffer.has_floor(
            up ? to : from,
            {.visible_only = (up ? target_tile : source_tile)->get_ter_t().has_flag(TFLAG_RAMP_DOWN),
             .terrain_only = true})) {
        return std::unexpected(vehicle_step_failure::blocked);
    }
    return to;
}

auto vehicle_step_failure_message(const vehicle_step_failure failure) -> std::string {
    switch (failure) {
        case vehicle_step_failure::unpaired:
            return _("This vehicle passage has no matching endpoint on the adjacent deck.");
        case vehicle_step_failure::unavailable:
            return _("The vehicle passage or its supporting frame is missing, broken, or carried.");
        case vehicle_step_failure::wrong_geometry:
            return _("There is no vehicle passage between those positions.");
        case vehicle_step_failure::outside_domain:
            return _("The vehicle passage is outside the active area.");
        case vehicle_step_failure::blocked:
            return _("The vehicle passage is blocked.");
    }
    return _("The vehicle passage is unavailable.");
}
