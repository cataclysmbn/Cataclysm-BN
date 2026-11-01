#pragma once

#include "coordinates.h"

#include <expected>
#include <optional>
#include <string>

class mapbuffer;

enum class vehicle_step_failure { unpaired, unavailable, wrong_geometry, outside_domain, blocked };

/// Empty success means no endpoint; an error must not authorize terrain/rope fallback.
/// Live, actor-independent topology. No loading, boarding, or other world mutation.
auto vehicle_stair_at(mapbuffer& buffer, const tripoint_abs_ms& from, int dz)
    -> std::expected<std::optional<tripoint_abs_ms>, vehicle_step_failure>;
auto vehicle_step_failure_message(vehicle_step_failure failure) -> std::string;
