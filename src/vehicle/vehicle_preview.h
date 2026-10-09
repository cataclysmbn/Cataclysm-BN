#pragma once

#include "coordinates.h"
#include "cursesdef.h"

#include <memory>

class vehicle;

/// Optional graphical vehicle view; its lifetime is owned by the shared vehicle UI.
struct vehicle_preview_window {
    virtual ~vehicle_preview_window() = default;
    virtual auto prepare(const catacurses::window& win) -> void = 0;
    virtual auto display(const vehicle& veh, tripoint_mnt_veh cursor, int highlight_part)
        -> void = 0;
    virtual auto clear() -> void = 0;
    virtual auto zoom_in() -> void = 0;
    virtual auto zoom_out() -> void = 0;
    virtual auto get_zoom() const -> int = 0;
};

namespace game_client {
using vehicle_preview_factory = auto (*)() -> std::unique_ptr<vehicle_preview_window>;
auto set_vehicle_preview_factory(vehicle_preview_factory factory) -> void;
auto make_vehicle_preview() -> std::unique_ptr<vehicle_preview_window>;
auto install_tiles_vehicle_preview() -> void;
} // namespace game_client
