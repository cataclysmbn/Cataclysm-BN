#if defined(CATA_SDL)
#include "gpu_platform.h"

#include "debug.h"
#include "gpu_lm.h"
#include "gpu_transparency.h"
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

namespace cata_gpu {

namespace {

SDL_GPUDevice* s_device = nullptr;

struct gpu_device_create_attempt {
    std::string label;
    std::string driver;
    bool require_vulkan_hardware = false;
    bool prefer_low_power = false;
};

auto create_gpu_device(gpu_device_create_attempt const& attempt) -> SDL_GPUDevice* {
    auto const props = SDL_CreateProperties();
    if (props == 0) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: property creation failed for " << attempt.label << ": " << SDL_GetError();
        return nullptr;
    }

    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
#if defined(SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN)
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, false);
#endif
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
    if (attempt.prefer_low_power) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, true);
    }
    if (!attempt.driver.empty()) {
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, attempt.driver.c_str());
    }
#if defined(SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN)
    if (attempt.driver.empty() || attempt.driver == "vulkan") {
        SDL_SetBooleanProperty(
            props, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN,
            attempt.require_vulkan_hardware);
    }
#endif

    auto* const device = SDL_CreateGPUDeviceWithProperties(props);
    auto const error = std::string{SDL_GetError()};
    SDL_DestroyProperties(props);
    if (device == nullptr) {
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: device creation attempt failed (" << attempt.label << "): " << error;
    }
    return device;
}

auto software_attempts() -> std::vector<gpu_device_create_attempt> {
    return {
        {
            .label = "vulkan software-capable",
            .driver = "vulkan",
            .require_vulkan_hardware = false,
        },
        {
            .label = "default software-capable",
            .driver = "",
            .require_vulkan_hardware = false,
            .prefer_low_power = true,
        },
    };
}

auto add_attempt(
    std::vector<gpu_device_create_attempt>& attempts, gpu_device_create_attempt attempt) -> void {
    auto const duplicate = std::ranges::any_of(attempts, [&attempt](auto const& existing) {
        return existing.driver == attempt.driver
            && existing.require_vulkan_hardware == attempt.require_vulkan_hardware
            && existing.prefer_low_power == attempt.prefer_low_power;
    });
    if (!duplicate) { attempts.emplace_back(std::move(attempt)); }
}

auto make_device_attempts(preload_config::compute_accel accel, std::string backend)
    -> std::vector<gpu_device_create_attempt> {
    using preload_config::compute_accel;

    if (backend == "auto") { backend.clear(); }
    if (backend == "software") {
        accel = compute_accel::software;
        backend.clear();
        DebugLog(DL::Info, DC::Main) << "SDL_GPU: backend override 'software' selects the "
                                        "software-capable policy";
    }

    auto attempts = std::vector<gpu_device_create_attempt>{};
    if (accel == compute_accel::software) {
        if (!backend.empty()) {
            add_attempt(
                attempts,
                {
                    .label = backend + " software-capable",
                    .driver = backend,
                    .require_vulkan_hardware = false,
                    .prefer_low_power = true,
                });
        }
        for (auto attempt : software_attempts()) { add_attempt(attempts, std::move(attempt)); }
        return attempts;
    }

    add_attempt(
        attempts,
        {
            .label = backend.empty() ? "default hardware" : backend + " hardware",
            .driver = backend,
            .require_vulkan_hardware = true,
        });

    if (accel != compute_accel::force) {
        if (!backend.empty() && backend != "vulkan") {
            add_attempt(
                attempts,
                {
                    .label = backend + " selected",
                    .driver = backend,
                    .require_vulkan_hardware = false,
                });
        }
        for (auto attempt : software_attempts()) { add_attempt(attempts, std::move(attempt)); }
    }

    return attempts;
}

auto shader_formats_to_string(SDL_GPUShaderFormat const formats) -> std::string {
    using entry_t = std::pair<SDL_GPUShaderFormat, std::string_view>;
    static constexpr std::array<entry_t, 3> entries{{
        {SDL_GPU_SHADERFORMAT_SPIRV, "SPIRV"},
        {SDL_GPU_SHADERFORMAT_DXIL, "DXIL"},
        {SDL_GPU_SHADERFORMAT_MSL, "MSL"},
    }};
    std::string result;
    std::ranges::for_each(
        entries | std::views::filter([&formats](entry_t const& e) {
            return static_cast<bool>(formats & e.first);
        }),
        [&result](entry_t const& e) {
            if (!result.empty()) { result += ' '; }
            result += e.second;
        });
    return result.empty() ? std::string{"(none)"} : result;
}

auto read_file_bytes(std::string const& path) -> std::vector<std::byte> {
    auto ifs = std::ifstream(path, std::ios::binary | std::ios::ate);
    if (!ifs) { return {}; }
    auto const size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0);
    std::vector<std::byte> buf(size);
    ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    return buf;
}

// Select the preferred shader format and corresponding file extension for the device.
// Priority: DXIL (D3D12/Windows) > SPIRV (Vulkan) > MSL (Metal).
auto select_shader_format(SDL_GPUShaderFormat const formats)
    -> std::pair<SDL_GPUShaderFormat, std::string_view> {
    if (formats & SDL_GPU_SHADERFORMAT_DXIL) { return {SDL_GPU_SHADERFORMAT_DXIL, ".dxil"}; }
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) { return {SDL_GPU_SHADERFORMAT_SPIRV, ".spv"}; }
    if (formats & SDL_GPU_SHADERFORMAT_MSL) { return {SDL_GPU_SHADERFORMAT_MSL, ".msl"}; }
    return {SDL_GPU_SHADERFORMAT_INVALID, ""};
}

auto probe_shader(
    SDL_GPUDevice* const device, SDL_GPUShaderFormat const fmt,
    std::string_view const ext) -> void {
    auto const path = PATH_INFO::shaders() + "test_compute" + std::string{ext};
    auto const blob = read_file_bytes(path);
    if (blob.empty()) {
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: shader blob not found: " << path
            << " (run a build with shadercross to compile shaders)";
        return;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size = blob.size(),
        .code = reinterpret_cast<Uint8 const*>(blob.data()),
        .entrypoint = "main",
        .format = fmt,
        .num_samplers = 0,
        .num_readonly_storage_textures = 0,
        .num_readonly_storage_buffers = 0,
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers = 0,
        .num_uniform_buffers = 0,
        .threadcount_x = 1,
        .threadcount_y = 1,
        .threadcount_z = 1,
        .props = 0,
    };

    auto* const pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (pipeline == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: compute pipeline creation failed for " << path << ": " << SDL_GetError();
        return;
    }

    DebugLog(DL::Info, DC::Main) << "SDL_GPU: shader probe OK (" << path << ")";
    SDL_ReleaseGPUComputePipeline(device, pipeline);
}

} // namespace

auto init() -> void {
    using preload_config::compute_accel;

    if (s_device != nullptr) { return; }

    auto const accel = preload_config::get_compute_accel();

    auto const backend_sv = preload_config::get_gpu_backend_override();
    auto const backend_str = std::string{backend_sv};
    if (!backend_str.empty()) {
        DebugLog(DL::Info, DC::Main) << "SDL_GPU: backend override: " << backend_str;
    }

    auto* device = static_cast<SDL_GPUDevice*>(nullptr);
    for (auto const& attempt : make_device_attempts(accel, backend_str)) {
        device = create_gpu_device(attempt);
        if (device != nullptr) {
            DebugLog(DL::Info, DC::Main) << "SDL_GPU: selected device policy: " << attempt.label;
            break;
        }
    }

    if (device == nullptr) {
        auto const level = (accel == compute_accel::force) ? DL::Error : DL::Warn;
        DebugLog(level, DC::Main) << "SDL_GPU: device creation failed; install/enable a hardware "
                                     "GPU driver or "
                                  << "a software Vulkan driver such as Lavapipe for the shader "
                                     "fallback path";
        return;
    }

    const char* const driver = SDL_GetGPUDeviceDriver(device);
    const auto formats = SDL_GetGPUShaderFormats(device);

    DebugLog(DL::Info, DC::Main) << "SDL_GPU: driver=" << (driver != nullptr ? driver : "unknown")
                                 << "  formats=" << shader_formats_to_string(formats);

    auto const [fmt, ext] = select_shader_format(formats);
    if (fmt != SDL_GPU_SHADERFORMAT_INVALID) { probe_shader(device, fmt, ext); }

    s_device = device;
}

auto shutdown() -> void {
    if (s_device != nullptr) {
        shutdown_transparency();
        shutdown_lm();
        SDL_DestroyGPUDevice(s_device);
        s_device = nullptr;
    }
}

auto get_device() -> SDL_GPUDevice* { return s_device; }

} // namespace cata_gpu
#endif // defined( CATA_SDL )
