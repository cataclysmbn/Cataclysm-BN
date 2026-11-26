#pragma once

class avatar;
class item;

namespace ranged {

/// Limits for aim_until_ready, taken from the chosen aim mode and the target.
struct aim_until_ready_options {
    const item& relevant;
    int threshold;
    int sight_dispersion;
    double min_recoil;
};

/// Spends moves on aiming until recoil reaches the aim mode threshold or the aim cap.
auto aim_until_ready(avatar& you, const aim_until_ready_options& opts) -> void;

} // namespace ranged
