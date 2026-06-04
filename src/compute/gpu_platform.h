#pragma once
#if defined(CATA_SDL)

struct SDL_GPUDevice;

namespace cata_gpu {

auto init() -> void;
auto shutdown() -> void;

// Returns the active GPU device, or nullptr if device creation failed or
// shutdown already ran.
auto get_device() -> SDL_GPUDevice*;

} // namespace cata_gpu

#endif // defined( CATA_SDL )
