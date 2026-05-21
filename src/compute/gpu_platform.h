#pragma once
#if defined( CATA_SDL )

struct SDL_GPUDevice;

namespace cata_gpu
{

auto init()       -> void;
auto shutdown()   -> void;

// Returns the active GPU device, or nullptr if the device was not created
// (COMPUTE_ACCELERATION=off, creation failed, or shutdown already called).
auto get_device() -> SDL_GPUDevice *;

} // namespace cata_gpu

#endif // defined( CATA_SDL )
