#if defined( CATA_SDL )
#include "gpu_platform.h"

#include "debug.h"
#include "path_info.h"
#include "preload_config.h"

#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cata_gpu
{

namespace
{

auto shader_formats_to_string( SDL_GPUShaderFormat const formats ) -> std::string
{
    using entry_t = std::pair<SDL_GPUShaderFormat, std::string_view>;
    static constexpr std::array<entry_t, 3> entries{ {
        { SDL_GPU_SHADERFORMAT_SPIRV, "SPIRV" },
        { SDL_GPU_SHADERFORMAT_DXIL,  "DXIL"  },
        { SDL_GPU_SHADERFORMAT_MSL,   "MSL"   },
    } };
    std::string result;
    std::ranges::for_each(
        entries | std::views::filter( [&formats]( entry_t const &e ) {
            return static_cast<bool>( formats & e.first );
        } ),
        [&result]( entry_t const &e ) {
            if( !result.empty() ) {
                result += ' ';
            }
            result += e.second;
        }
    );
    return result.empty() ? std::string{ "(none)" } : result;
}

auto read_file_bytes( std::string const &path ) -> std::vector<std::byte>
{
    auto ifs = std::ifstream( path, std::ios::binary | std::ios::ate );
    if( !ifs ) {
        return {};
    }
    auto const size = static_cast<std::size_t>( ifs.tellg() );
    ifs.seekg( 0 );
    std::vector<std::byte> buf( size );
    ifs.read( reinterpret_cast<char *>( buf.data() ), static_cast<std::streamsize>( size ) );
    return buf;
}

// Select the preferred shader format and corresponding file extension for the device.
// Priority: DXIL (D3D12/Windows) > SPIRV (Vulkan) > MSL (Metal).
auto select_shader_format( SDL_GPUShaderFormat const formats )
-> std::pair<SDL_GPUShaderFormat, std::string_view>
{
    if( formats & SDL_GPU_SHADERFORMAT_DXIL  ) { return { SDL_GPU_SHADERFORMAT_DXIL,  ".dxil" }; }
    if( formats & SDL_GPU_SHADERFORMAT_SPIRV ) { return { SDL_GPU_SHADERFORMAT_SPIRV, ".spv"  }; }
    if( formats & SDL_GPU_SHADERFORMAT_MSL   ) { return { SDL_GPU_SHADERFORMAT_MSL,   ".msl"  }; }
    return { SDL_GPU_SHADERFORMAT_INVALID, "" };
}

auto probe_shader( SDL_GPUDevice *const device, SDL_GPUShaderFormat const fmt,
                   std::string_view const ext ) -> void
{
    auto const path = PATH_INFO::shaders() + "test_compute" + std::string{ ext };
    auto const blob = read_file_bytes( path );
    if( blob.empty() ) {
        DebugLog( DL::Info, DC::Main )
                << "SDL_GPU: shader blob not found: " << path
                << " (run a build with shadercross to compile shaders)";
        return;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size                   = blob.size(),
        .code                        = reinterpret_cast<Uint8 const *>( blob.data() ),
        .entrypoint                  = "main",
        .format                      = fmt,
        .num_samplers                = 0,
        .num_readonly_storage_textures = 0,
        .num_readonly_storage_buffers  = 0,
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers  = 0,
        .num_uniform_buffers         = 0,
        .threadcount_x               = 1,
        .threadcount_y               = 1,
        .threadcount_z               = 1,
        .props                       = 0,
    };

    auto *const pipeline = SDL_CreateGPUComputePipeline( device, &info );
    if( pipeline == nullptr ) {
        DebugLog( DL::Warn, DC::Main )
                << "SDL_GPU: compute pipeline creation failed for " << path
                << ": " << SDL_GetError();
        return;
    }

    DebugLog( DL::Info, DC::Main ) << "SDL_GPU: shader probe OK (" << path << ")";
    SDL_ReleaseGPUComputePipeline( device, pipeline );
}

} // namespace

auto init() -> void
{
    using preload_config::compute_accel;

    auto const accel = preload_config::get_compute_accel();
    if( accel == compute_accel::off ) {
        DebugLog( DL::Info, DC::Main ) << "SDL_GPU: disabled (COMPUTE_ACCELERATION=off)";
        return;
    }

    auto const backend_sv  = preload_config::get_gpu_backend_override();
    auto const backend_str = std::string{ backend_sv };
    if( !backend_str.empty() ) {
        DebugLog( DL::Info, DC::Main ) << "SDL_GPU: backend override: " << backend_str;
    }

    auto *const device = SDL_CreateGPUDevice(
                             SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
                             false,
                             backend_str.empty() ? nullptr : backend_str.c_str()
                         );

    if( device == nullptr ) {
        auto const level = ( accel == compute_accel::force ) ? DL::Error : DL::Warn;
        DebugLog( level, DC::Main ) << "SDL_GPU: device creation failed: " << SDL_GetError();
        return;
    }

    const char *const driver  = SDL_GetGPUDeviceDriver( device );
    const auto        formats = SDL_GetGPUShaderFormats( device );

    DebugLog( DL::Info, DC::Main )
            << "SDL_GPU: driver="  << ( driver != nullptr ? driver : "unknown" )
            << "  formats=" << shader_formats_to_string( formats );

    auto const [fmt, ext] = select_shader_format( formats );
    if( fmt != SDL_GPU_SHADERFORMAT_INVALID ) {
        probe_shader( device, fmt, ext );
    }

    SDL_DestroyGPUDevice( device );
}

auto shutdown() -> void {}

} // namespace cata_gpu
#endif // defined( CATA_SDL )
