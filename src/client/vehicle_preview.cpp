#include "vehicle/vehicle_preview.h"
namespace game_client {
namespace {
auto factory = vehicle_preview_factory{};
}
auto set_vehicle_preview_factory(vehicle_preview_factory value) -> void { factory = value; }
auto make_vehicle_preview() -> std::unique_ptr<vehicle_preview_window> {
    return factory ? factory() : nullptr;
}
} // namespace game_client
