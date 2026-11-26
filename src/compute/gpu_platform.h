#pragma once
#if defined(CATA_SDL)

#    include <SDL3/SDL_gpu.h>

namespace cata_gpu {

auto init() -> void;
auto shutdown() -> void;

// Returns the active GPU device, or nullptr if device creation failed or
// shutdown already ran.
auto get_device() -> SDL_GPUDevice*;

// SDL_shadercross emits Metal compute kernels as main0 while the other
// backends keep the HLSL entry point name.
auto compute_shader_entrypoint(SDL_GPUShaderFormat format) -> char const*;

} // namespace cata_gpu

#endif // defined( CATA_SDL )
