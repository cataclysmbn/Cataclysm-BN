#if defined(CATA_SDL)
#include "gpu_platform.h"

#include "debug.h"
#include "filesystem.h"
#include "gpu_lm.h"
#include "gpu_transparency.h"
#include "path_info.h"
#include "preload_config.h"
#include "string_utils.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cata_gpu {

auto compute_shader_entrypoint(SDL_GPUShaderFormat const format) -> char const* {
    return format == SDL_GPU_SHADERFORMAT_MSL ? "main0" : "main";
}

namespace {

SDL_GPUDevice* s_device = nullptr;

struct gpu_device_create_attempt {
    std::string label;
    std::string driver;
    bool require_vulkan_hardware = false;
    bool prefer_low_power = false;
};

struct gpu_device_info {
    std::string name;
    std::string driver_name;
    std::string driver_version;
    std::string driver_info;
};

auto env_is_set(char const* const name) -> bool {
    auto const* const value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

auto set_env_if_empty(char const* const name, std::string const& value) -> bool {
    if (env_is_set(name)) { return false; }
#if defined(_WIN32)
    return _putenv_s(name, value.c_str()) == 0;
#else
    return setenv(name, value.c_str(), 0) == 0;
#endif
}

#if defined(_WIN32)
auto sibling_path(std::string const& path, std::string_view const filename) -> std::string {
    auto const separator = path.find_last_of("/\\");
    if (separator == std::string::npos) { return std::string{filename}; }
    return path.substr(0, separator + 1) + std::string{filename};
}

auto lavapipe_manifest_has_runtime(std::string const& manifest_path) -> bool {
    auto const runtime_path = sibling_path(manifest_path, "vulkan_lvp.dll");
    if (file_exist(runtime_path)) { return true; }

    DebugLog(DL::Warn, DC::Main) << "SDL_GPU: Lavapipe ICD manifest found without sibling "
                                    "vulkan_lvp.dll: "
                                 << manifest_path;
    return false;
}
#else
auto lavapipe_manifest_has_runtime(std::string const&) -> bool { return true; }
#endif

auto path_for_vulkan_icd_env(std::string const& path) -> std::string {
    try {
        return std::filesystem::absolute(std::filesystem::path{path}).lexically_normal().string();
    } catch (std::filesystem::filesystem_error const&) { return path; }
}

auto find_lavapipe_icd_manifest() -> std::string {
    auto const candidates = std::vector<std::string>{
        PATH_INFO::base_path() + "mesa/x64/lvp_icd.x86_64.json",
        PATH_INFO::base_path() + "build-data/mesa/x64/lvp_icd.x86_64.json",
        PATH_INFO::base_path() + "mesa/lvp_icd.x86_64.json",
        PATH_INFO::base_path() + "mesa/lvp_icd.json",
        PATH_INFO::base_path() + "lvp_icd.x86_64.json",
        PATH_INFO::base_path() + "lvp_icd.json",
        PATH_INFO::base_path() + "vulkan/icd.d/lvp_icd.x86_64.json",
        PATH_INFO::base_path() + "vulkan/icd.d/lvp_icd.json",
        PATH_INFO::base_path() + "share/vulkan/icd.d/lvp_icd.x86_64.json",
        PATH_INFO::base_path() + "share/vulkan/icd.d/lvp_icd.json",
        PATH_INFO::datadir() + "vulkan/icd.d/lvp_icd.x86_64.json",
        PATH_INFO::datadir() + "vulkan/icd.d/lvp_icd.json",
        "mesa/x64/lvp_icd.x86_64.json",
        "build-data/mesa/x64/lvp_icd.x86_64.json",
        "mesa/lvp_icd.x86_64.json",
        "mesa/lvp_icd.json",
        "lvp_icd.x86_64.json",
        "lvp_icd.json",
        "vulkan/icd.d/lvp_icd.x86_64.json",
        "vulkan/icd.d/lvp_icd.json",
        "data/vulkan/icd.d/lvp_icd.x86_64.json",
        "data/vulkan/icd.d/lvp_icd.json",
#if defined(_WIN32)
        PATH_INFO::base_path() + "lvp_icd.x86.json",
        PATH_INFO::base_path() + "mesa/x86/lvp_icd.x86.json",
        PATH_INFO::base_path() + "build-data/mesa/x86/lvp_icd.x86.json",
        PATH_INFO::base_path() + "mesa/lvp_icd.x86.json",
        PATH_INFO::base_path() + "vulkan/icd.d/lvp_icd.x86.json",
        "lvp_icd.x86.json",
        "mesa/x86/lvp_icd.x86.json",
        "build-data/mesa/x86/lvp_icd.x86.json",
        "mesa/lvp_icd.x86.json",
        "vulkan/icd.d/lvp_icd.x86.json",
#else
        "/usr/share/vulkan/icd.d/lvp_icd.json",
        "/usr/local/share/vulkan/icd.d/lvp_icd.json",
        "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json",
        "/usr/local/share/vulkan/icd.d/lvp_icd.x86_64.json",
        "/usr/share/vulkan/icd.d/lvp_icd.aarch64.json",
        "/usr/local/share/vulkan/icd.d/lvp_icd.aarch64.json",
        "/usr/share/vulkan/icd.d/lvp_icd.i686.json",
        "/usr/local/share/vulkan/icd.d/lvp_icd.i686.json",
#endif
    };

    for (auto const& path : candidates) {
        if (file_exist(path) && lavapipe_manifest_has_runtime(path)) { return path; }
    }
    return {};
}

auto pin_lavapipe_icd_for_software_mode() -> void {
    if (env_is_set("VK_DRIVER_FILES") || env_is_set("VK_ICD_FILENAMES")) {
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: software mode using existing Vulkan ICD "
               "environment override";
        return;
    }

    auto const icd_manifest = find_lavapipe_icd_manifest();
    if (icd_manifest.empty()) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: software mode requested, but no Lavapipe ICD "
               "manifest was found";
        return;
    }

    auto const env_icd_manifest = path_for_vulkan_icd_env(icd_manifest);
    auto const driver_files_set = set_env_if_empty("VK_DRIVER_FILES", env_icd_manifest);
    auto const icd_filenames_set = set_env_if_empty("VK_ICD_FILENAMES", env_icd_manifest);
    if (driver_files_set || icd_filenames_set) {
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: software mode pinned Vulkan ICD manifest: " << env_icd_manifest;
    } else {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: failed to pin Lavapipe ICD manifest for software mode: "
            << env_icd_manifest;
    }
}

auto get_gpu_device_info(SDL_GPUDevice* const device) -> gpu_device_info {
#if defined(SDL_PROP_GPU_DEVICE_NAME_STRING)
    auto const gpu_device_property_string =
        [](SDL_PropertiesID const props, char const* const name) -> std::string {
        auto const* const value = SDL_GetStringProperty(props, name, "");
        return value != nullptr ? std::string{value} : std::string{};
    };
    auto const props = SDL_GetGPUDeviceProperties(device);
    if (props == 0) { return {}; }
    return {
        .name = gpu_device_property_string(props, SDL_PROP_GPU_DEVICE_NAME_STRING),
        .driver_name = gpu_device_property_string(props, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING),
        .driver_version =
            gpu_device_property_string(props, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING),
        .driver_info = gpu_device_property_string(props, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING),
    };
#else
    (void)device;
    return {};
#endif
}

auto device_string_for_log(std::string value) -> std::string {
    std::ranges::replace(value, '\n', ' ');
    std::ranges::replace(value, '\r', ' ');
    return value.empty() ? std::string{"unknown"} : value;
}

auto log_gpu_device_info(gpu_device_info const& info) -> void {
    DebugLog(DL::Info, DC::Main) << "SDL_GPU: device=" << device_string_for_log(info.name)
                                 << "  driver_name=" << device_string_for_log(info.driver_name)
                                 << "  driver_version="
                                 << device_string_for_log(info.driver_version);
    if (!info.driver_info.empty()) {
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: driver_info=" << device_string_for_log(info.driver_info);
    }
}

auto is_recognized_software_device(gpu_device_info const& info) -> bool {
    auto const combined =
        info.name + " " + info.driver_name + " " + info.driver_version + " " + info.driver_info;
    static constexpr std::array<std::string_view, 7> markers{{
        "lavapipe",
        "llvmpipe",
        "swiftshader",
        "microsoft basic render driver",
        "microsoft basic renderer",
        "software rasterizer",
        "warp",
    }};
    return std::ranges::any_of(markers, [&combined](std::string_view const marker) {
        return lcmatch(combined, std::string{marker});
    });
}

auto compiled_gpu_drivers_to_string() -> std::string {
    auto const count = SDL_GetNumGPUDrivers();
    if (count <= 0) { return "(none)"; }

    auto drivers = std::string{};
    for (auto const index : std::views::iota(0, count)) {
        auto const* const driver = SDL_GetGPUDriver(index);
        if (!drivers.empty()) { drivers += ' '; }
        drivers += driver != nullptr ? driver : "unknown";
    }
    return drivers;
}

auto create_gpu_device(gpu_device_create_attempt const& attempt) -> SDL_GPUDevice* {
    auto const props = SDL_CreateProperties();
    if (props == 0) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: property creation failed for " << attempt.label << ": " << SDL_GetError();
        return nullptr;
    }

    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
    SDL_SetBooleanProperty(
        props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);

    // For Intel Haswell and Broadwell iGPU, requires <= 8 shader storage resource slots across
    // *all* stages
    SDL_SetBooleanProperty(
        props, SDL_PROP_GPU_DEVICE_CREATE_D3D12_ALLOW_FEWER_RESOURCE_SLOTS_BOOLEAN, true);

    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
    SDL_SetBooleanProperty(
        props, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, attempt.prefer_low_power);

    if (!attempt.driver.empty()) {
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, attempt.driver.c_str());
    }
    if (attempt.driver.empty() || attempt.driver == "vulkan") {
        SDL_SetBooleanProperty(
            props, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN,
            attempt.require_vulkan_hardware);
    }

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
#if defined(__ANDROID__)
    return {};
#else
    return {
        {
            .label = "vulkan software-required",
            .driver = "vulkan",
            .require_vulkan_hardware = false,
        },
    };
#endif
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
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: backend override 'software' selects the "
               "software-required policy";
    }

    auto attempts = std::vector<gpu_device_create_attempt>{};
    if (accel == compute_accel::software) {
#if defined(__ANDROID__)
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: software compute acceleration is not available on Android; "
               "hardware Vulkan 1.1 is required";
#endif
        if (!backend.empty()) {
            add_attempt(
                attempts,
                {
                    .label = backend + " software-required",
                    .driver = backend,
                    .require_vulkan_hardware = false,
                    .prefer_low_power = true,
                });
        }
        for (auto attempt : software_attempts()) { add_attempt(attempts, std::move(attempt)); }
        return attempts;
    }

    if (backend.empty()) {
#if defined(__ANDROID__)
        add_attempt(
            attempts,
            {
                .label = "vulkan hardware",
                .driver = "vulkan",
                .require_vulkan_hardware = true,
            });
#endif
        add_attempt(
            attempts,
            {
                .label = "default hardware",
                .driver = backend,
                .require_vulkan_hardware = true,
            });
    } else {
        add_attempt(
            attempts,
            {
                .label = backend + " hardware",
                .driver = backend,
                .require_vulkan_hardware = true,
            });
    }

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
    static constexpr std::array<entry_t, 4> entries{{
        {SDL_GPU_SHADERFORMAT_SPIRV, "SPIRV"},
        {SDL_GPU_SHADERFORMAT_DXBC, "DXBC"},
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
// Priority: DXIL (modern D3D12) > DXBC (older D3D12) > SPIRV (Vulkan) > MSL (Metal).
auto select_shader_format(SDL_GPUShaderFormat const formats)
    -> std::pair<SDL_GPUShaderFormat, std::string_view> {
    if (formats & SDL_GPU_SHADERFORMAT_DXIL) { return {SDL_GPU_SHADERFORMAT_DXIL, ".dxil"}; }
    if (formats & SDL_GPU_SHADERFORMAT_DXBC) { return {SDL_GPU_SHADERFORMAT_DXBC, ".dxbc"}; }
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) { return {SDL_GPU_SHADERFORMAT_SPIRV, ".spv"}; }
    if (formats & SDL_GPU_SHADERFORMAT_MSL) { return {SDL_GPU_SHADERFORMAT_MSL, ".msl"}; }
    return {SDL_GPU_SHADERFORMAT_INVALID, ""};
}

auto probe_shader(
    SDL_GPUDevice* const device, SDL_GPUShaderFormat const fmt, std::string_view const ext)
    -> bool {
    auto const path = PATH_INFO::shaders() + "test_compute" + std::string{ext};
    auto const blob = read_file_bytes(path);
    if (blob.empty()) {
        DebugLog(DL::Info, DC::Main)
            << "SDL_GPU: shader blob not found: " << path
            << " (run a build with shadercross to compile shaders)";
        return false;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size = blob.size(),
        .code = reinterpret_cast<Uint8 const*>(blob.data()),
        .entrypoint = compute_shader_entrypoint(fmt),
        .format = fmt,
        .num_samplers = 0,
        .num_readonly_storage_textures = 0,
        .num_readonly_storage_buffers = 0,
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers = 2,
        .num_uniform_buffers = 0,
        .threadcount_x = 12,
        .threadcount_y = 12,
        .threadcount_z = 1,
        .props = 0,
    };

    auto* const pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (pipeline == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: compute pipeline creation failed for " << path << ": " << SDL_GetError();
        return false;
    }

    static constexpr auto probe_value = uint32_t{0xC0DECAFEu};
    static constexpr auto probe_count = std::size_t{12 * 12};
    auto const probe_bytes = static_cast<Uint32>(probe_count * sizeof(probe_value));
    auto const input_buffer_info = SDL_GPUBufferCreateInfo{
        .usage =
            SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
        .size = probe_bytes,
        .props = 0,
    };
    auto* const input_buffer = SDL_CreateGPUBuffer(device, &input_buffer_info);
    if (input_buffer == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe input buffer creation failed: " << SDL_GetError();
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto const output_buffer_info = SDL_GPUBufferCreateInfo{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
        .size = probe_bytes,
        .props = 0,
    };
    auto* const output_buffer = SDL_CreateGPUBuffer(device, &output_buffer_info);
    if (output_buffer == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe output buffer creation failed: " << SDL_GetError();
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto const upload_transfer_info = SDL_GPUTransferBufferCreateInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = probe_bytes,
        .props = 0,
    };
    auto* const upload_buffer = SDL_CreateGPUTransferBuffer(device, &upload_transfer_info);
    if (upload_buffer == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe upload buffer creation failed: " << SDL_GetError();
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto const transfer_info = SDL_GPUTransferBufferCreateInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
        .size = probe_bytes,
        .props = 0,
    };
    auto* const download_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (download_buffer == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe download buffer creation failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto* const upload_mapped = static_cast<uint32_t*>(
        SDL_MapGPUTransferBuffer(device, upload_buffer, false));
    if (upload_mapped == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe upload map failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, download_buffer);
        SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }
    for (auto const idx : std::views::iota(std::size_t{0}, probe_count)) {
        upload_mapped[idx] = static_cast<uint32_t>(idx);
    }
    SDL_UnmapGPUTransferBuffer(device, upload_buffer);

    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe command buffer acquisition failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, download_buffer);
        SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto* const upload_pass = SDL_BeginGPUCopyPass(cmd);
    auto const input_src = SDL_GPUTransferBufferLocation{
        .transfer_buffer = upload_buffer,
        .offset = 0,
    };
    auto const input_dst = SDL_GPUBufferRegion{
        .buffer = input_buffer,
        .offset = 0,
        .size = probe_bytes,
    };
    SDL_UploadToGPUBuffer(upload_pass, &input_src, &input_dst, false);
    SDL_EndGPUCopyPass(upload_pass);

    auto const rw_bindings = std::array<SDL_GPUStorageBufferReadWriteBinding, 2>{{
        {
            .buffer = input_buffer,
            .cycle = false,
            .padding1 = 0,
            .padding2 = 0,
            .padding3 = 0,
        },
        {
            .buffer = output_buffer,
            .cycle = false,
            .padding1 = 0,
            .padding2 = 0,
            .padding3 = 0,
        },
    }};
    auto* const cp = SDL_BeginGPUComputePass(
        cmd, nullptr, 0, rw_bindings.data(), static_cast<Uint32>(rw_bindings.size()));
    SDL_BindGPUComputePipeline(cp, pipeline);
    SDL_DispatchGPUCompute(cp, 1, 1, 1);
    SDL_EndGPUComputePass(cp);

    auto* const copy_pass = SDL_BeginGPUCopyPass(cmd);
    auto const output_src = SDL_GPUBufferRegion{
        .buffer = output_buffer,
        .offset = 0,
        .size = probe_bytes,
    };
    auto const output_dst = SDL_GPUTransferBufferLocation{
        .transfer_buffer = download_buffer,
        .offset = 0,
    };
    SDL_DownloadFromGPUBuffer(copy_pass, &output_src, &output_dst);
    SDL_EndGPUCopyPass(copy_pass);

    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe command buffer submission failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, download_buffer);
        SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto const wait_succeeded = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    if (!wait_succeeded) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe fence wait failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, download_buffer);
        SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto const* const mapped = static_cast<uint32_t const*>(
        SDL_MapGPUTransferBuffer(device, download_buffer, false));
    if (mapped == nullptr) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe download map failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, download_buffer);
        SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
        SDL_ReleaseGPUBuffer(device, output_buffer);
        SDL_ReleaseGPUBuffer(device, input_buffer);
        SDL_ReleaseGPUComputePipeline(device, pipeline);
        return false;
    }

    auto const observed = std::span{mapped, probe_count};
    auto const indices = std::views::iota(std::size_t{0}, probe_count);
    auto const mismatch = std::ranges::find_if(indices, [&observed](std::size_t const idx) {
        return observed[idx] != (probe_value ^ static_cast<uint32_t>(idx));
    });
    auto mismatch_index = probe_count;
    auto mismatch_value = uint32_t{};
    if (mismatch != std::ranges::end(indices)) {
        mismatch_index = *mismatch;
        mismatch_value = observed[mismatch_index];
    }
    SDL_UnmapGPUTransferBuffer(device, download_buffer);
    SDL_ReleaseGPUTransferBuffer(device, download_buffer);
    SDL_ReleaseGPUTransferBuffer(device, upload_buffer);
    SDL_ReleaseGPUBuffer(device, output_buffer);
    SDL_ReleaseGPUBuffer(device, input_buffer);
    SDL_ReleaseGPUComputePipeline(device, pipeline);

    if (mismatch_index != probe_count) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: shader probe returned " << mismatch_value << " at index " << mismatch_index
            << ", expected " << (probe_value ^ static_cast<uint32_t>(mismatch_index));
        return false;
    }

    DebugLog(DL::Info, DC::Main) << "SDL_GPU: shader probe OK (" << path << ")";
    return true;
}

} // namespace

auto init() -> void {
    using preload_config::compute_accel;

    if (s_device != nullptr) { return; }

    auto const backend_sv = preload_config::get_gpu_backend_override();
    auto const backend_str = std::string{backend_sv};
    auto const accel = preload_config::get_compute_accel();
    auto const require_software_device =
        accel == compute_accel::software || backend_str == "software";
    if (require_software_device) { pin_lavapipe_icd_for_software_mode(); }

    if (!backend_str.empty()) {
        DebugLog(DL::Info, DC::Main) << "SDL_GPU: backend override: " << backend_str;
    }
    DebugLog(DL::Info, DC::Main) << "SDL_GPU: compiled drivers=" << compiled_gpu_drivers_to_string();

    auto* device = static_cast<SDL_GPUDevice*>(nullptr);
    auto selected_device_info = gpu_device_info{};
    for (auto const& attempt : make_device_attempts(accel, backend_str)) {
        device = create_gpu_device(attempt);
        if (device != nullptr) {
            selected_device_info = get_gpu_device_info(device);
            if (require_software_device && !is_recognized_software_device(selected_device_info)) {
                DebugLog(DL::Warn, DC::Main)
                    << "SDL_GPU: rejected device for software-required policy: " << attempt.label;
                log_gpu_device_info(selected_device_info);
                SDL_DestroyGPUDevice(device);
                device = nullptr;
                continue;
            }

            const char* const candidate_driver = SDL_GetGPUDeviceDriver(device);
            const auto candidate_formats = SDL_GetGPUShaderFormats(device);
            DebugLog(DL::Info, DC::Main)
                << "SDL_GPU: candidate driver="
                << (candidate_driver != nullptr ? candidate_driver : "unknown")
                << "  formats=" << shader_formats_to_string(candidate_formats);
            log_gpu_device_info(selected_device_info);

            auto const [fmt, ext] = select_shader_format(candidate_formats);
            if (fmt == SDL_GPU_SHADERFORMAT_INVALID || !probe_shader(device, fmt, ext)) {
                DebugLog(DL::Warn, DC::Main)
                    << "SDL_GPU: rejected device after compute dispatch probe: " << attempt.label;
                SDL_DestroyGPUDevice(device);
                device = nullptr;
                continue;
            }

            DebugLog(DL::Info, DC::Main) << "SDL_GPU: selected device policy: " << attempt.label;
            break;
        }
    }

    if (device == nullptr) {
#if defined(__ANDROID__)
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: device creation failed; Android builds require Vulkan 1.1-capable "
               "hardware and a working system Vulkan loader";
        return;
#else
        auto const level =
            (accel == compute_accel::force || require_software_device) ? DL::Error : DL::Warn;
        DebugLog(level, DC::Main) << "SDL_GPU: device creation failed; install/enable a hardware "
                                     "GPU driver or "
                                  << "a software Vulkan driver such as Lavapipe for the shader "
                                     "fallback path";
        return;
#endif
    }

    const char* const driver = SDL_GetGPUDeviceDriver(device);
    const auto formats = SDL_GetGPUShaderFormats(device);

    DebugLog(DL::Info, DC::Main) << "SDL_GPU: driver=" << (driver != nullptr ? driver : "unknown")
                                 << "  formats=" << shader_formats_to_string(formats);
    log_gpu_device_info(selected_device_info);

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
