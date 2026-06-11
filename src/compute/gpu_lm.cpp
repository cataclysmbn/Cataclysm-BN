#if defined(CATA_SDL)
#include "gpu_lm.h"

#include "cached_options.h"
#include "calendar.h"
#include "character.h"
#include "debug.h"
#include "effect.h"
#include "field.h"
#include "game.h"
#include "game_constants.h"
#include "gpu_platform.h"
#include "item.h"
#include "itype.h"
#include "lightmap.h"
#include "map.h"
#include "math_defines.h"
#include "monster.h"
#include "npc.h"
#include "path_info.h"
#include "profile.h"
#include "shadowcasting.h"
#include "submap.h"
#include "units_angle.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cata_gpu {

namespace {

static constexpr auto solar_shadow_scatter = 0.09f;
static constexpr auto daylight_diffusion_passes = 16;
static constexpr auto light_source_default_z_frac = 0.5f;
static constexpr auto light_source_vehicle_external_z_frac = 0.8f;
static constexpr auto dxbc_serialized_readback_chunk_bytes = Uint32{64u * 1024u};
static constexpr auto dxbc_serialized_readback_batch_slots = std::size_t{4};

// ---------------------------------------------------------------------------
// Pipeline management
// ---------------------------------------------------------------------------

auto read_blob(std::string const& path) -> std::vector<std::byte> {
    auto ifs = std::ifstream(path, std::ios::binary | std::ios::ate);
    if (!ifs) { return {}; }
    auto const size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0);
    std::vector<std::byte> buf(size);
    ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    return buf;
}

auto preferred_ext(SDL_GPUShaderFormat const fmts) -> std::string_view {
    if (fmts & SDL_GPU_SHADERFORMAT_DXIL) { return ".dxil"; }
    if (fmts & SDL_GPU_SHADERFORMAT_DXBC) { return ".dxbc"; }
    if (fmts & SDL_GPU_SHADERFORMAT_SPIRV) { return ".spv"; }
    if (fmts & SDL_GPU_SHADERFORMAT_MSL) { return ".msl"; }
    return {};
}

auto preferred_fmt(SDL_GPUShaderFormat const fmts) -> SDL_GPUShaderFormat {
    if (fmts & SDL_GPU_SHADERFORMAT_DXIL) { return SDL_GPU_SHADERFORMAT_DXIL; }
    if (fmts & SDL_GPU_SHADERFORMAT_DXBC) { return SDL_GPU_SHADERFORMAT_DXBC; }
    if (fmts & SDL_GPU_SHADERFORMAT_SPIRV) { return SDL_GPU_SHADERFORMAT_SPIRV; }
    if (fmts & SDL_GPU_SHADERFORMAT_MSL) { return SDL_GPU_SHADERFORMAT_MSL; }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

auto shader_format_is_dxbc(SDL_GPUShaderFormat const fmt) -> bool {
    return fmt == SDL_GPU_SHADERFORMAT_DXBC;
}

auto shader_blob_path(SDL_GPUDevice* const device, std::string_view const name) -> std::string {
    auto const ext = preferred_ext(SDL_GetGPUShaderFormats(device));
    if (ext.empty()) { return {}; }
    return PATH_INFO::shaders() + std::string{name} + std::string{ext};
}

auto shader_blob_available(SDL_GPUDevice* const device, std::string_view const name) -> bool {
    auto const path = shader_blob_path(device, name);
    if (path.empty()) { return false; }
    return !read_blob(path).empty();
}

auto packed_visibility_words_per_level(int const cache_xy) -> Uint32 {
    return (static_cast<Uint32>(cache_xy) + 3u) / 4u;
}

auto packed_visibility_level_bytes(int const cache_xy) -> Uint32 {
    return packed_visibility_words_per_level(cache_xy) * static_cast<Uint32>(sizeof(uint32_t));
}

auto unpack_packed_visibility_level(
    void const* const mapped, std::size_t const tiles, std::vector<lit_level>& destination)
    -> void {
    auto const* const words = static_cast<uint32_t const*>(mapped);
    auto tile = std::size_t{0};
    auto destination_span = std::span<lit_level>{destination.data(), destination.size()}.first(
        tiles);
    for (auto& level : destination_span) {
        auto const word = words[tile / 4u];
        auto const shift = static_cast<uint32_t>((tile % 4u) * 8u);
        level = static_cast<lit_level>((word >> shift) & 0xffu);
        ++tile;
    }
}

struct gpu_backend_policy {
    bool reset_lighting_resources_on_rebuild = false;
    bool expand_dirty_transparency_uploads = false;
    bool cycle_upload_transfers = false;
    bool submit_lighting_stages = false;
    bool wait_lighting_stages = false;
    bool serialize_lighting_downloads = false;
    bool serialize_visibility_downloads = false;
    Uint32 serialized_readback_chunk_bytes = 0;
};

auto backend_policy_for_device(SDL_GPUDevice* const device) -> gpu_backend_policy {
    auto const uses_dxbc = shader_format_is_dxbc(preferred_fmt(SDL_GetGPUShaderFormats(device)));
    return gpu_backend_policy{
        .reset_lighting_resources_on_rebuild = false,
        .expand_dirty_transparency_uploads = false,
        .cycle_upload_transfers = uses_dxbc,
        .submit_lighting_stages = uses_dxbc,
        .wait_lighting_stages = false,
        .serialize_lighting_downloads = uses_dxbc,
        .serialize_visibility_downloads = uses_dxbc,
        .serialized_readback_chunk_bytes =
            uses_dxbc ? dxbc_serialized_readback_chunk_bytes : Uint32{0},
    };
}

SDL_GPUComputePipeline* s_ambient_pipeline = nullptr;
SDL_GPUComputePipeline* s_daylight_diffuse_pipeline = nullptr;
SDL_GPUComputePipeline* s_raytrace_pipeline = nullptr;
SDL_GPUComputePipeline* s_color_raytrace_pipeline = nullptr;
auto* s_clear_seen_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_clear_seen_view_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
SDL_GPUComputePipeline* s_seen_pipeline = nullptr;
SDL_GPUComputePipeline* s_seen_walls_pipeline = nullptr;
SDL_GPUComputePipeline* s_visibility_pipeline = nullptr;
auto* s_pack_visibility_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_sight_pairs_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_vehicle_optics_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_shift_float_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_shift_uint_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_fill_float_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_fill_uint_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_max_uint_pipeline = static_cast<SDL_GPUComputePipeline*>(nullptr);
auto* s_pipeline_device = static_cast<SDL_GPUDevice*>(nullptr);

auto release_pipeline(SDL_GPUDevice* const device, SDL_GPUComputePipeline*& pipeline) -> void {
    if (pipeline == nullptr) { return; }
    SDL_ReleaseGPUComputePipeline(device, pipeline);
    pipeline = nullptr;
}

auto release_lm_pipelines(SDL_GPUDevice* const device) -> void {
    release_pipeline(device, s_ambient_pipeline);
    release_pipeline(device, s_daylight_diffuse_pipeline);
    release_pipeline(device, s_raytrace_pipeline);
    release_pipeline(device, s_color_raytrace_pipeline);
    release_pipeline(device, s_clear_seen_pipeline);
    release_pipeline(device, s_clear_seen_view_pipeline);
    release_pipeline(device, s_seen_pipeline);
    release_pipeline(device, s_seen_walls_pipeline);
    release_pipeline(device, s_visibility_pipeline);
    release_pipeline(device, s_pack_visibility_pipeline);
    release_pipeline(device, s_sight_pairs_pipeline);
    release_pipeline(device, s_vehicle_optics_pipeline);
    release_pipeline(device, s_shift_float_pipeline);
    release_pipeline(device, s_shift_uint_pipeline);
    release_pipeline(device, s_fill_float_pipeline);
    release_pipeline(device, s_fill_uint_pipeline);
    release_pipeline(device, s_max_uint_pipeline);
    if (s_pipeline_device == device) { s_pipeline_device = nullptr; }
}

auto ensure_pipeline_device(SDL_GPUDevice* const device) -> bool {
    if (device == nullptr) { return false; }
    if (s_pipeline_device == device) { return true; }
    if (s_pipeline_device != nullptr) {
        if (!SDL_WaitForGPUIdle(s_pipeline_device)) {
            DebugLog(DL::Warn, DC::Main)
                << "SDL_GPU: lm: wait for idle during pipeline device switch failed: "
                << SDL_GetError();
        }
        release_lm_pipelines(s_pipeline_device);
    }
    s_pipeline_device = device;
    return true;
}

auto load_pipeline(
    SDL_GPUDevice* const device, std::string_view const name, int const ro_bufs, int const rw_bufs,
    int const threadcount_x, int const threadcount_y) -> SDL_GPUComputePipeline* {
    auto const fmts = SDL_GetGPUShaderFormats(device);
    auto const fmt = preferred_fmt(fmts);
    auto const ext = preferred_ext(fmts);

    if (fmt == SDL_GPU_SHADERFORMAT_INVALID || ext.empty()) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: lm: no supported shader format for " << name;
        return nullptr;
    }

    auto const path = PATH_INFO::shaders() + std::string{name} + std::string{ext};
    auto const blob = read_blob(path);
    if (blob.empty()) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: shader blob not found: " << path
            << " (run a build with shadercross to compile shaders)";
        return nullptr;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size = blob.size(),
        .code = reinterpret_cast<Uint8 const*>(blob.data()),
        .entrypoint = compute_shader_entrypoint(fmt),
        .format = fmt,
        .num_samplers = 0,
        .num_readonly_storage_textures = 0,
        .num_readonly_storage_buffers = static_cast<Uint32>(ro_bufs),
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers = static_cast<Uint32>(rw_bufs),
        .num_uniform_buffers = 1,
        .threadcount_x = static_cast<Uint32>(threadcount_x),
        .threadcount_y = static_cast<Uint32>(threadcount_y),
        .threadcount_z = 1,
        .props = 0,
    };

    auto* const pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (pipeline == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: pipeline creation failed for " << name << ": " << SDL_GetError();
    }
    return pipeline;
}

auto ensure_seen_pipelines(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_clear_seen_pipeline == nullptr) {
        s_clear_seen_pipeline = load_pipeline(
            device, "lm_clear_seen_compute",
            /*ro=*/0, /*rw=*/2, 64, 1);
    }
    if (s_clear_seen_view_pipeline == nullptr) {
        s_clear_seen_view_pipeline = load_pipeline(
            device, "lm_clear_seen_view_compute",
            /*ro=*/0, /*rw=*/2, 8, 8);
    }
    if (s_seen_pipeline == nullptr) {
        s_seen_pipeline = load_pipeline(
            device, "lm_seen_compute",
            /*ro=*/4, /*rw=*/1, 8, 8);
    }
    if (s_seen_walls_pipeline == nullptr) {
        s_seen_walls_pipeline = load_pipeline(
            device, "lm_seen_walls_compute",
            /*ro=*/4, /*rw=*/1, 8, 8);
    }
    return s_clear_seen_pipeline != nullptr && s_clear_seen_view_pipeline != nullptr
        && s_seen_pipeline != nullptr && s_seen_walls_pipeline != nullptr;
}

auto ensure_pipelines(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_ambient_pipeline == nullptr) {
        s_ambient_pipeline = load_pipeline(
            device, "lm_ambient_compute",
            /*ro=*/4, /*rw=*/2, 64, 1);
    }
    if (s_daylight_diffuse_pipeline == nullptr) {
        s_daylight_diffuse_pipeline = load_pipeline(
            device, "lm_daylight_diffuse_compute",
            /*ro=*/3, /*rw=*/2, 64, 1);
    }
    if (s_raytrace_pipeline == nullptr) {
        s_raytrace_pipeline = load_pipeline(
            device, "lm_raytrace_compute",
            /*ro=*/4, /*rw=*/1, 8, 8);
    }
    return s_ambient_pipeline != nullptr && s_daylight_diffuse_pipeline != nullptr
        && s_raytrace_pipeline != nullptr && ensure_seen_pipelines(device);
}

auto ensure_color_raytrace_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_color_raytrace_pipeline == nullptr) {
        s_color_raytrace_pipeline = load_pipeline(
            device, "lm_color_raytrace_compute",
            /*ro=*/4, /*rw=*/1, 8, 8);
    }
    return s_color_raytrace_pipeline != nullptr;
}

auto ensure_visibility_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_visibility_pipeline == nullptr) {
        s_visibility_pipeline = load_pipeline(
            device, "lm_visibility_compute",
            /*ro=*/5, /*rw=*/1, 64, 1);
    }
    return s_visibility_pipeline != nullptr;
}

auto ensure_pack_visibility_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_pack_visibility_pipeline == nullptr) {
        s_pack_visibility_pipeline = load_pipeline(
            device, "lm_pack_visibility_compute",
            /*ro=*/1, /*rw=*/1, 64, 1);
    }
    return s_pack_visibility_pipeline != nullptr;
}

auto ensure_sight_pairs_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_sight_pairs_pipeline == nullptr) {
        s_sight_pairs_pipeline = load_pipeline(
            device, "lm_sight_pairs_compute",
            /*ro=*/3, /*rw=*/1, 64, 1);
    }
    return s_sight_pairs_pipeline != nullptr;
}

auto ensure_vehicle_optics_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_vehicle_optics_pipeline == nullptr) {
        s_vehicle_optics_pipeline = load_pipeline(
            device, "lm_vehicle_optics_compute",
            /*ro=*/3, /*rw=*/1, 8, 8);
    }
    return s_vehicle_optics_pipeline != nullptr;
}

auto ensure_shift_float_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_shift_float_pipeline == nullptr) {
        s_shift_float_pipeline = load_pipeline(
            device, "lm_shift_float_compute",
            /*ro=*/1, /*rw=*/1, 64, 1);
    }
    return s_shift_float_pipeline != nullptr;
}

auto ensure_shift_uint_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_shift_uint_pipeline == nullptr) {
        s_shift_uint_pipeline = load_pipeline(
            device, "lm_shift_uint_compute",
            /*ro=*/1, /*rw=*/1, 64, 1);
    }
    return s_shift_uint_pipeline != nullptr;
}

auto ensure_fill_float_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_fill_float_pipeline == nullptr) {
        s_fill_float_pipeline = load_pipeline(
            device, "lm_fill_float_compute",
            /*ro=*/0, /*rw=*/1, 64, 1);
    }
    return s_fill_float_pipeline != nullptr;
}

auto ensure_fill_uint_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_fill_uint_pipeline == nullptr) {
        s_fill_uint_pipeline = load_pipeline(
            device, "lm_fill_uint_compute",
            /*ro=*/0, /*rw=*/1, 64, 1);
    }
    return s_fill_uint_pipeline != nullptr;
}

auto ensure_max_uint_pipeline(SDL_GPUDevice* const device) -> bool {
    if (!ensure_pipeline_device(device)) { return false; }
    if (s_max_uint_pipeline == nullptr) {
        s_max_uint_pipeline = load_pipeline(
            device, "lm_max_uint_compute",
            /*ro=*/1, /*rw=*/1, 64, 1);
    }
    return s_max_uint_pipeline != nullptr;
}

// ---------------------------------------------------------------------------
// Persistent GPU lighting resources
// ---------------------------------------------------------------------------

struct gpu_buffer_slot {
    SDL_GPUBuffer* buffer = nullptr;
    Uint32 capacity = 0;
    SDL_GPUBufferUsageFlags usage = 0;
};

struct gpu_transfer_slot {
    SDL_GPUTransferBuffer* buffer = nullptr;
    Uint32 capacity = 0;
};

struct lighting_input_residency {
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
    bool transparency_valid = false;
    bool floor_valid = false;
    bool vehicle_floor_valid = false;
    bool vehicle_obscured_valid = false;
    std::vector<char> transparency_valid_levels;
    std::vector<char> transparency_shader_updated_levels;
};

struct lighting_resource_cache {
    SDL_GPUDevice* device = nullptr;
    gpu_buffer_slot transparency;
    gpu_buffer_slot floor;
    gpu_buffer_slot vehicle_floor;
    gpu_buffer_slot vehicle_obscured;
    gpu_buffer_slot camera;
    gpu_buffer_slot vehicle_optics;
    gpu_buffer_slot source_map;
    gpu_buffer_slot sources;
    gpu_buffer_slot colored_sources;
    gpu_buffer_slot daylight_seed;
    gpu_buffer_slot daylight_diffuse_a;
    gpu_buffer_slot daylight_diffuse_b;
    gpu_buffer_slot static_lm;
    gpu_buffer_slot lm;
    gpu_buffer_slot seen_raw;
    gpu_buffer_slot seen;
    gpu_buffer_slot visibility;
    gpu_buffer_slot visibility_packed;
    gpu_buffer_slot colored_light;
    gpu_buffer_slot sight_pairs;
    gpu_buffer_slot sight_results;
    gpu_buffer_slot shift_float_scratch;
    gpu_buffer_slot shift_floor_scratch;
    gpu_buffer_slot shift_vehicle_floor_scratch;
    gpu_buffer_slot shift_vehicle_obscured_scratch;
    gpu_transfer_slot upload;
    gpu_transfer_slot visibility_upload;
    gpu_transfer_slot lm_download;
    gpu_transfer_slot seen_download;
    gpu_transfer_slot visibility_download;
    gpu_transfer_slot colored_light_download;
    std::array<gpu_transfer_slot, dxbc_serialized_readback_batch_slots>
        serialized_readback_downloads;
    gpu_transfer_slot sight_upload;
    gpu_transfer_slot sight_download;
    std::vector<float> transparency_staging;
    std::vector<uint32_t> floor_staging;
    std::vector<uint32_t> vehicle_floor_staging;
    std::vector<uint32_t> vehicle_obscured_staging;
    std::vector<float> source_map_staging;
    lighting_input_residency inputs;
    std::vector<int> camera_nonzero_levels;
    std::vector<char> seen_valid_levels;
    bool seen_valid = false;
    bool seen_origin_valid = false;
    int seen_origin_x = 0;
    int seen_origin_y = 0;
    bool camera_valid = false;
    bool source_map_valid = false;
    bool static_lighting_valid = false;
    bool static_ambient_signature_valid = false;
    std::size_t static_ambient_signature = 0;
    bool static_source_signature_valid = false;
    std::size_t static_source_signature = 0;
    std::vector<char> static_structural_level_signature_valid;
    std::vector<std::size_t> static_structural_level_signatures;
    bool lighting_outputs_valid = false;
};

struct colored_lighting_buffer_sizes {
    Uint32 source_bytes;
    Uint32 output_bytes;
    Uint32 download_bytes;
};

struct ensure_gpu_buffer_params {
    SDL_GPUDevice* device;
    gpu_buffer_slot* slot;
    SDL_GPUBufferUsageFlags usage;
    Uint32 required_bytes;
    std::string_view name;
};

struct ensure_transfer_buffer_params {
    SDL_GPUDevice* device;
    gpu_transfer_slot* slot;
    SDL_GPUTransferBufferUsage usage;
    Uint32 required_bytes;
    std::string_view name;
};

struct lighting_buffer_sizes {
    Uint32 transparency_bytes;
    Uint32 floor_bytes;
    Uint32 vehicle_floor_bytes;
    Uint32 vehicle_obscured_bytes;
    Uint32 camera_bytes;
    Uint32 vehicle_optics_bytes;
    Uint32 source_map_bytes;
    Uint32 source_bytes;
    Uint32 daylight_bytes;
    Uint32 static_lm_bytes;
    Uint32 output_bytes;
    Uint32 lm_download_bytes;
    Uint32 seen_download_bytes;
    Uint32 visibility_download_bytes;
    Uint32 upload_bytes;
    Uint32 visibility_upload_bytes;
};

struct input_upload_plan {
    std::vector<int> transparency_levels;
    std::vector<int> floor_levels;
    std::vector<int> vehicle_floor_levels;
    std::vector<int> vehicle_obscured_levels;
};

struct camera_zero_plan {
    std::vector<int> upload_levels;
    std::vector<int> nonzero_levels_after_dispatch;
};

struct GpuVehicleOptic {
    int32_t x;
    int32_t y;
    int32_t z_idx;
    uint32_t kind;
    int32_t range;
    int32_t offset_distance;
    uint32_t _pad[2];
};
static_assert(sizeof(GpuVehicleOptic) == 32);

static constexpr auto vehicle_optic_mirror = uint32_t{0};
static constexpr auto vehicle_optic_camera = uint32_t{1};

struct lm_vehicle_optics_push_constants {
    int32_t cache_x;
    int32_t cache_y;
    int32_t cache_xy;
    int32_t z_count;
    uint32_t num_optics;
    int32_t max_radius;
    uint32_t trigdist;
    float visible_threshold;
    int32_t max_view_distance;
    uint32_t _pad[3];
};
static_assert(sizeof(lm_vehicle_optics_push_constants) == 48);

struct lm_sight_pairs_push_constants {
    int32_t cache_x;
    int32_t cache_y;
    int32_t cache_xy;
    int32_t z_count;
    uint32_t pair_count;
    uint32_t _pad[3];
};
static_assert(sizeof(lm_sight_pairs_push_constants) == 32);

struct lm_clear_seen_push_constants {
    uint32_t total_tiles;
    int32_t cache_xy;
    int32_t z_start_idx;
    uint32_t _pad;
};
static_assert(sizeof(lm_clear_seen_push_constants) == 16);

struct lm_clear_seen_view_push_constants {
    int32_t player_x;
    int32_t player_y;
    int32_t cache_x;
    int32_t cache_y;
    int32_t cache_xy;
    int32_t z_count;
    int32_t view_radius;
    int32_t z_start_idx;
    int32_t dispatch_z_count;
    uint32_t _pad[3];
};
static_assert(sizeof(lm_clear_seen_view_push_constants) == 48);

struct lm_shift_float_push_constants {
    int32_t cache_x;
    int32_t cache_y;
    int32_t cache_xy;
    int32_t z_count;
    int32_t shift_x_tiles;
    int32_t shift_y_tiles;
    float fill_value;
    uint32_t _pad;
};
static_assert(sizeof(lm_shift_float_push_constants) == 32);

struct lm_shift_uint_push_constants {
    int32_t cache_x;
    int32_t cache_y;
    int32_t cache_xy;
    int32_t z_count;
    int32_t shift_x_tiles;
    int32_t shift_y_tiles;
    uint32_t fill_value;
    uint32_t _pad;
};
static_assert(sizeof(lm_shift_uint_push_constants) == 32);

struct lm_fill_float_push_constants {
    uint32_t total_tiles;
    int32_t cache_xy;
    int32_t z_start_idx;
    float value;
};
static_assert(sizeof(lm_fill_float_push_constants) == 16);

struct lm_fill_uint_push_constants {
    uint32_t total_tiles;
    uint32_t value;
    uint32_t _pad[2];
};
static_assert(sizeof(lm_fill_uint_push_constants) == 16);

struct lm_max_uint_push_constants {
    uint32_t total_tiles;
    uint32_t _pad[3];
};
static_assert(sizeof(lm_max_uint_push_constants) == 16);

struct lm_daylight_diffuse_push_constants {
    uint32_t total_tiles;
    int32_t cache_x;
    int32_t cache_y;
    int32_t cache_xy;
    int32_t z_count;
    float diffuse_decay;
    float min_light;
    uint32_t _pad;
};
static_assert(sizeof(lm_daylight_diffuse_push_constants) == 32);

lighting_resource_cache s_lighting_resources;

struct pending_gpu_lighting_work {
    bool active = false;
    uint64_t id = 0;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUFence* fence = nullptr;
    map const* m = nullptr;
    input_upload_plan input_uploads;
    std::vector<int> lightmap_levels;
    std::vector<int> all_levels;
    std::vector<int> seen_download_levels;
    std::vector<int> colored_light_download_levels;
    std::vector<std::pair<int, std::size_t>> checked_structural_signatures;
    std::size_t current_ambient_signature = 0;
    std::size_t current_static_source_signature = 0;
    int cache_xy = 0;
    Uint32 lm_download_bytes = 0;
    Uint32 serialized_readback_chunk_bytes = 0;
    bool deferred_download_copy = false;
    bool download_lm_levels = false;
    bool download_player_light = false;
    bool download_colored_light = false;
    bool rebuild_seen = false;
    bool rebuild_static_lighting = false;
    int cache_y = 0;
    int player_x = 0;
    int player_y = 0;
    int player_zlev = 0;
};

auto s_pending_lighting_work = pending_gpu_lighting_work{};
auto s_next_lighting_work_id = uint64_t{1};

struct pending_gpu_visibility_work {
    bool active = false;
    uint64_t id = 0;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUFence* fence = nullptr;
    map const* m = nullptr;
    std::vector<int> visibility_download_levels;
    std::vector<int> camera_nonzero_levels_after_dispatch;
    int cache_xy = 0;
    int player_x = 0;
    int player_y = 0;
    bool rebuild_seen = false;
    bool force_seen_invalidate = false;
    bool deferred_download_copy = false;
    bool packed_download = false;
    Uint32 packed_level_bytes = 0;
};

auto s_pending_visibility_work = pending_gpu_visibility_work{};
auto s_next_visibility_work_id = uint64_t{1};

struct serialized_readback_request {
    SDL_GPUBuffer* source_buffer = nullptr;
    SDL_GPUTransferBuffer* transfer_buffer = nullptr;
    Uint32 source_offset = 0;
    Uint32 size = 0;
    std::string_view label;
};

template <typename CopyResult>
auto readback_buffer_region(
    SDL_GPUDevice* const device, serialized_readback_request const& request, CopyResult copy_result)
    -> bool {
    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: serialized readback command buffer acquisition failed for "
            << request.label << ": " << SDL_GetError();
        return false;
    }

    auto* const cp = SDL_BeginGPUCopyPass(cmd);
    auto const source = SDL_GPUBufferRegion{
        .buffer = request.source_buffer,
        .offset = request.source_offset,
        .size = request.size,
    };
    auto const destination = SDL_GPUTransferBufferLocation{
        .transfer_buffer = request.transfer_buffer,
        .offset = 0,
    };
    SDL_DownloadFromGPUBuffer(cp, &source, &destination);
    SDL_EndGPUCopyPass(cp);

    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: serialized readback command buffer submission failed for "
            << request.label << ": " << SDL_GetError();
        return false;
    }
    auto const wait_succeeded = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    if (!wait_succeeded) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: serialized readback fence wait failed for " << request.label << ": "
            << SDL_GetError();
        return false;
    }

    SDL_ClearError();
    auto const* mapped = SDL_MapGPUTransferBuffer(device, request.transfer_buffer, false);
    if (mapped == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: serialized readback transfer map failed for " << request.label
            << " bytes=" << request.size << ": " << SDL_GetError();
        return false;
    }
    copy_result(mapped);
    SDL_UnmapGPUTransferBuffer(device, request.transfer_buffer);
    return true;
}

struct chunked_readback_request {
    serialized_readback_request region;
    Uint32 chunk_bytes = 0;
};

struct pending_gpu_sight_pairs_work {
    bool active = false;
    uint64_t id = 0;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUFence* fence = nullptr;
    std::size_t pair_count = 0;
};

auto s_pending_sight_pairs_work = pending_gpu_sight_pairs_work{};
auto s_next_sight_pairs_work_id = uint64_t{1};

auto reset_input_residency_for_shape(int cache_x, int cache_y, int z_count) -> void;

auto release_pending_lighting_work(SDL_GPUDevice* const device) -> void {
    if (s_pending_lighting_work.fence != nullptr && device != nullptr) {
        SDL_ReleaseGPUFence(device, s_pending_lighting_work.fence);
    }
    s_pending_lighting_work = {};
}

auto release_pending_visibility_work(SDL_GPUDevice* const device) -> void {
    if (s_pending_visibility_work.fence != nullptr && device != nullptr) {
        SDL_ReleaseGPUFence(device, s_pending_visibility_work.fence);
    }
    s_pending_visibility_work = {};
}

auto release_pending_sight_pairs_work(SDL_GPUDevice* const device) -> void {
    if (s_pending_sight_pairs_work.fence != nullptr && device != nullptr) {
        SDL_ReleaseGPUFence(device, s_pending_sight_pairs_work.fence);
    }
    s_pending_sight_pairs_work = {};
}

auto release_buffer_slot(SDL_GPUDevice* const device, gpu_buffer_slot& slot) -> void {
    if (slot.buffer != nullptr) {
        SDL_ReleaseGPUBuffer(device, slot.buffer);
        slot.buffer = nullptr;
        slot.capacity = 0;
        slot.usage = 0;
    }
}

auto release_transfer_slot(SDL_GPUDevice* const device, gpu_transfer_slot& slot) -> void {
    if (slot.buffer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, slot.buffer);
        slot.buffer = nullptr;
        slot.capacity = 0;
    }
}

auto release_lighting_resources(SDL_GPUDevice* const device) -> void {
    release_pending_lighting_work(device);
    release_pending_visibility_work(device);
    release_pending_sight_pairs_work(device);
    release_buffer_slot(device, s_lighting_resources.transparency);
    release_buffer_slot(device, s_lighting_resources.floor);
    release_buffer_slot(device, s_lighting_resources.vehicle_floor);
    release_buffer_slot(device, s_lighting_resources.vehicle_obscured);
    release_buffer_slot(device, s_lighting_resources.camera);
    release_buffer_slot(device, s_lighting_resources.vehicle_optics);
    release_buffer_slot(device, s_lighting_resources.source_map);
    release_buffer_slot(device, s_lighting_resources.sources);
    release_buffer_slot(device, s_lighting_resources.colored_sources);
    release_buffer_slot(device, s_lighting_resources.daylight_seed);
    release_buffer_slot(device, s_lighting_resources.daylight_diffuse_a);
    release_buffer_slot(device, s_lighting_resources.daylight_diffuse_b);
    release_buffer_slot(device, s_lighting_resources.static_lm);
    release_buffer_slot(device, s_lighting_resources.lm);
    release_buffer_slot(device, s_lighting_resources.seen_raw);
    release_buffer_slot(device, s_lighting_resources.seen);
    release_buffer_slot(device, s_lighting_resources.visibility);
    release_buffer_slot(device, s_lighting_resources.visibility_packed);
    release_buffer_slot(device, s_lighting_resources.colored_light);
    release_buffer_slot(device, s_lighting_resources.sight_pairs);
    release_buffer_slot(device, s_lighting_resources.sight_results);
    release_buffer_slot(device, s_lighting_resources.shift_float_scratch);
    release_buffer_slot(device, s_lighting_resources.shift_floor_scratch);
    release_buffer_slot(device, s_lighting_resources.shift_vehicle_floor_scratch);
    release_buffer_slot(device, s_lighting_resources.shift_vehicle_obscured_scratch);
    release_transfer_slot(device, s_lighting_resources.upload);
    release_transfer_slot(device, s_lighting_resources.visibility_upload);
    release_transfer_slot(device, s_lighting_resources.lm_download);
    release_transfer_slot(device, s_lighting_resources.seen_download);
    release_transfer_slot(device, s_lighting_resources.visibility_download);
    release_transfer_slot(device, s_lighting_resources.colored_light_download);
    for (auto& slot : s_lighting_resources.serialized_readback_downloads) {
        release_transfer_slot(device, slot);
    }
    release_transfer_slot(device, s_lighting_resources.sight_upload);
    release_transfer_slot(device, s_lighting_resources.sight_download);
    s_lighting_resources.transparency_staging = {};
    s_lighting_resources.floor_staging = {};
    s_lighting_resources.vehicle_floor_staging = {};
    s_lighting_resources.vehicle_obscured_staging = {};
    s_lighting_resources.source_map_staging = {};
    s_lighting_resources.inputs = {};
    s_lighting_resources.camera_nonzero_levels = {};
    s_lighting_resources.seen_valid = false;
    s_lighting_resources.seen_origin_valid = false;
    s_lighting_resources.seen_valid_levels = {};
    s_lighting_resources.camera_valid = false;
    s_lighting_resources.source_map_valid = false;
    s_lighting_resources.static_lighting_valid = false;
    s_lighting_resources.static_ambient_signature_valid = false;
    s_lighting_resources.static_ambient_signature = 0;
    s_lighting_resources.static_source_signature_valid = false;
    s_lighting_resources.static_source_signature = 0;
    s_lighting_resources.static_structural_level_signature_valid = {};
    s_lighting_resources.static_structural_level_signatures = {};
    s_lighting_resources.lighting_outputs_valid = false;
    s_lighting_resources.device = nullptr;
}

auto ensure_resource_device(SDL_GPUDevice* const device) -> void {
    if (s_lighting_resources.device == device) { return; }
    if (s_lighting_resources.device != nullptr) {
        if (!SDL_WaitForGPUIdle(s_lighting_resources.device)) {
            DebugLog(DL::Warn, DC::Main)
                << "SDL_GPU: lm: wait for idle during device switch failed: " << SDL_GetError();
        }
        release_lighting_resources(s_lighting_resources.device);
    }
    s_lighting_resources.device = device;
}

auto ensure_gpu_buffer(ensure_gpu_buffer_params const& p) -> bool {
    if (p.slot->buffer != nullptr && p.slot->capacity >= p.required_bytes
        && p.slot->usage == p.usage) {
        return true;
    }

    release_buffer_slot(p.device, *p.slot);
    auto const ci = SDL_GPUBufferCreateInfo{
        .usage = p.usage,
        .size = p.required_bytes,
        .props = 0,
    };
    p.slot->buffer = SDL_CreateGPUBuffer(p.device, &ci);
    if (p.slot->buffer == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: failed to allocate " << p.name << " buffer (" << p.required_bytes
            << " bytes): " << SDL_GetError();
        p.slot->capacity = 0;
        return false;
    }
    p.slot->capacity = p.required_bytes;
    p.slot->usage = p.usage;
    return true;
}

auto ensure_transfer_buffer(ensure_transfer_buffer_params const& p) -> bool {
    if (p.slot->buffer != nullptr && p.slot->capacity >= p.required_bytes) { return true; }

    release_transfer_slot(p.device, *p.slot);
    auto const ci = SDL_GPUTransferBufferCreateInfo{
        .usage = p.usage,
        .size = p.required_bytes,
        .props = 0,
    };
    p.slot->buffer = SDL_CreateGPUTransferBuffer(p.device, &ci);
    if (p.slot->buffer == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: failed to allocate " << p.name << " transfer buffer ("
            << p.required_bytes << " bytes): " << SDL_GetError();
        p.slot->capacity = 0;
        return false;
    }
    p.slot->capacity = p.required_bytes;
    return true;
}

auto ensure_serialized_readback_downloads(SDL_GPUDevice* const device, Uint32 const required_bytes)
    -> bool {
    auto ok = true;
    for (auto& slot : s_lighting_resources.serialized_readback_downloads) {
        ok = ensure_transfer_buffer({
                 .device = device,
                 .slot = &slot,
                 .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                 .required_bytes = std::max(required_bytes, Uint32{1}),
                 .name = "serialized_readback_download",
             })
          && ok;
    }
    return ok;
}

struct batched_readback_chunk {
    SDL_GPUTransferBuffer* transfer_buffer = nullptr;
    Uint32 request_offset = 0;
    Uint32 bytes = 0;
};

template <typename CopyChunk>
auto readback_buffer_region_chunked_batched(
    SDL_GPUDevice* const device, chunked_readback_request const& request, CopyChunk copy_chunk)
    -> bool {
    auto const chunk_bytes = request.chunk_bytes == 0 ? request.region.size : request.chunk_bytes;
    if (!ensure_serialized_readback_downloads(device, chunk_bytes)) { return false; }

    auto offset = Uint32{0};
    while (offset < request.region.size) {
        auto chunks = std::array<batched_readback_chunk, dxbc_serialized_readback_batch_slots>{};
        auto chunk_count = std::size_t{0};
        while (chunk_count < chunks.size() && offset < request.region.size) {
            auto const bytes = std::min(chunk_bytes, request.region.size - offset);
            chunks[chunk_count] = batched_readback_chunk{
                .transfer_buffer =
                    s_lighting_resources.serialized_readback_downloads[chunk_count].buffer,
                .request_offset = offset,
                .bytes = bytes,
            };
            offset += bytes;
            ++chunk_count;
        }

        auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
        if (cmd == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: batched readback command buffer acquisition failed for "
                << request.region.label << ": " << SDL_GetError();
            return false;
        }

        auto* const cp = SDL_BeginGPUCopyPass(cmd);
        for (auto const& chunk : chunks | std::views::take(chunk_count)) {
            auto const source = SDL_GPUBufferRegion{
                .buffer = request.region.source_buffer,
                .offset = request.region.source_offset + chunk.request_offset,
                .size = chunk.bytes,
            };
            auto const destination = SDL_GPUTransferBufferLocation{
                .transfer_buffer = chunk.transfer_buffer,
                .offset = 0,
            };
            SDL_DownloadFromGPUBuffer(cp, &source, &destination);
        }
        SDL_EndGPUCopyPass(cp);

        auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        if (fence == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: batched readback command buffer submission failed for "
                << request.region.label << ": " << SDL_GetError();
            return false;
        }
        auto const wait_succeeded = SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
        if (!wait_succeeded) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: batched readback fence wait failed for " << request.region.label
                << ": " << SDL_GetError();
            return false;
        }

        for (auto const& chunk : chunks | std::views::take(chunk_count)) {
            SDL_ClearError();
            auto const* mapped = SDL_MapGPUTransferBuffer(device, chunk.transfer_buffer, false);
            if (mapped == nullptr) {
                DebugLog(DL::Error, DC::Main)
                    << "SDL_GPU: lm: batched readback transfer map failed for "
                    << request.region.label << " bytes=" << chunk.bytes << ": " << SDL_GetError();
                return false;
            }
            copy_chunk(mapped, chunk.request_offset, chunk.bytes);
            SDL_UnmapGPUTransferBuffer(device, chunk.transfer_buffer);
        }
    }
    return true;
}

auto ensure_lighting_resources(SDL_GPUDevice* const device, lighting_buffer_sizes const& sizes)
    -> bool {
    ensure_resource_device(device);

    auto const read_usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);

    return ensure_gpu_buffer({
               .device = device,
               .slot = &s_lighting_resources.transparency,
               .usage = read_write_usage,
               .required_bytes = sizes.transparency_bytes,
               .name = "transparency",
           })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.floor,
            .usage = read_write_usage,
            .required_bytes = sizes.floor_bytes,
            .name = "floor",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.vehicle_floor,
            .usage = read_write_usage,
            .required_bytes = sizes.vehicle_floor_bytes,
            .name = "vehicle_floor",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.vehicle_obscured,
            .usage = read_write_usage,
            .required_bytes = sizes.vehicle_obscured_bytes,
            .name = "vehicle_obscured",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.camera,
            .usage = read_write_usage,
            .required_bytes = sizes.camera_bytes,
            .name = "camera",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.vehicle_optics,
            .usage = read_usage,
            .required_bytes = sizes.vehicle_optics_bytes,
            .name = "vehicle_optics",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.source_map,
            .usage = read_usage,
            .required_bytes = sizes.source_map_bytes,
            .name = "source_map",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.sources,
            .usage = read_usage,
            .required_bytes = sizes.source_bytes,
            .name = "sources",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.daylight_seed,
            .usage = read_write_usage,
            .required_bytes = sizes.daylight_bytes,
            .name = "daylight_seed",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.daylight_diffuse_a,
            .usage = read_write_usage,
            .required_bytes = sizes.daylight_bytes,
            .name = "daylight_diffuse_a",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.daylight_diffuse_b,
            .usage = read_write_usage,
            .required_bytes = sizes.daylight_bytes,
            .name = "daylight_diffuse_b",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.static_lm,
            .usage = read_write_usage,
            .required_bytes = sizes.static_lm_bytes,
            .name = "static_lm",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.lm,
            .usage = read_write_usage,
            .required_bytes = sizes.output_bytes,
            .name = "lm",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.seen_raw,
            .usage = read_write_usage,
            .required_bytes = sizes.transparency_bytes,
            .name = "seen_raw",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.seen,
            .usage = read_write_usage,
            .required_bytes = sizes.transparency_bytes,
            .name = "seen",
        })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.visibility,
            .usage = read_write_usage,
            .required_bytes = sizes.output_bytes,
            .name = "visibility",
        })
        && ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.upload,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .required_bytes = sizes.upload_bytes,
            .name = "upload",
        })
        && ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.visibility_upload,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .required_bytes = sizes.visibility_upload_bytes,
            .name = "visibility_upload",
        })
        && ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.lm_download,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .required_bytes = sizes.lm_download_bytes,
            .name = "lm_download",
        })
        && ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.seen_download,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .required_bytes = sizes.seen_download_bytes,
            .name = "seen_download",
        })
        && ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.visibility_download,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .required_bytes = sizes.visibility_download_bytes,
            .name = "visibility_download",
        });
}

auto ensure_visibility_packed_resource(SDL_GPUDevice* const device, Uint32 const required_bytes)
    -> bool {
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);
    return ensure_gpu_buffer({
        .device = device,
        .slot = &s_lighting_resources.visibility_packed,
        .usage = read_write_usage,
        .required_bytes = required_bytes,
        .name = "visibility_packed",
    });
}

auto ensure_colored_lighting_resources(
    SDL_GPUDevice* const device, colored_lighting_buffer_sizes const& sizes) -> bool {
    ensure_resource_device(device);

    auto const read_usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);
    return ensure_gpu_buffer({
               .device = device,
               .slot = &s_lighting_resources.colored_sources,
               .usage = read_usage,
               .required_bytes = sizes.source_bytes,
               .name = "colored_sources",
           })
        && ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.colored_light,
            .usage = read_write_usage,
            .required_bytes = sizes.output_bytes,
            .name = "colored_light",
        })
        && ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.colored_light_download,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .required_bytes = sizes.download_bytes,
            .name = "colored_light_download",
        });
}

auto ensure_lighting_transparency_resource(prepare_lighting_transparency_output_params const& p)
    -> bool {
    ensure_resource_device(p.device);
    reset_input_residency_for_shape(p.cache_x, p.cache_y, p.z_count);

    auto const volume_tiles = static_cast<Uint32>(p.z_count * p.cache_x * p.cache_y);
    auto const transparency_bytes = static_cast<Uint32>(volume_tiles * sizeof(float));
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);

    return ensure_gpu_buffer({
        .device = p.device,
        .slot = &s_lighting_resources.transparency,
        .usage = read_write_usage,
        .required_bytes = transparency_bytes,
        .name = "transparency",
    });
}

auto ensure_float_shift_scratch(SDL_GPUDevice* const device, Uint32 const required_bytes) -> bool {
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);
    return ensure_gpu_buffer({
        .device = device,
        .slot = &s_lighting_resources.shift_float_scratch,
        .usage = read_write_usage,
        .required_bytes = required_bytes,
        .name = "shift_float_scratch",
    });
}

auto ensure_uint_shift_scratch(
    SDL_GPUDevice* const device, gpu_buffer_slot& slot, Uint32 const required_bytes,
    std::string_view const name) -> bool {
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);
    return ensure_gpu_buffer({
        .device = device,
        .slot = &slot,
        .usage = read_write_usage,
        .required_bytes = required_bytes,
        .name = name,
    });
}

// ---------------------------------------------------------------------------
// Light source collection
// ---------------------------------------------------------------------------

auto make_source(
    int const x, int const y, int const zlev, float const luminance, uint32_t const flags = 0u)
    -> GpuLightSource {
    return GpuLightSource{
        .x = x,
        .y = y,
        .z_idx = zlev + OVERMAP_DEPTH,
        .flags = flags,
        .luminance = luminance,
        .radius = compute_light_radius(luminance),
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .cone_cos = 1.0f,
        .z_frac = (flags & light_source_external_vehicle) != 0u
                    ? light_source_vehicle_external_z_frac
                    : light_source_default_z_frac,
        ._pad = {},
    };
}

auto make_directional_source(
    tripoint_bub_ms const& pos, float const luminance, units::angle const direction,
    units::angle const width, uint32_t const flags) -> GpuLightSource {
    auto source =
        make_source(pos.x(), pos.y(), pos.z(), luminance, flags | light_source_directional);
    source.dir_x = static_cast<float>(units::cos(direction));
    source.dir_y = static_cast<float>(units::sin(direction));
    source.cone_cos = static_cast<float>(units::cos(width / 2.0));
    return source;
}

auto color_is_set(RGBColor const& color) -> bool {
    return color.a != 0 && (color.r != 0 || color.g != 0 || color.b != 0);
}

auto pack_rgb(RGBColor const& color) -> uint32_t {
    return (static_cast<uint32_t>(color.r) << 16) | (static_cast<uint32_t>(color.g) << 8)
         | static_cast<uint32_t>(color.b);
}

auto make_colored_source(GpuLightSource const& source, uint32_t const color_rgb)
    -> GpuColoredLightSource {
    return GpuColoredLightSource{
        .x = source.x,
        .y = source.y,
        .z_idx = source.z_idx,
        .flags = source.flags,
        .luminance = source.luminance,
        .radius = source.radius,
        .dir_x = source.dir_x,
        .dir_y = source.dir_y,
        .cone_cos = source.cone_cos,
        .z_frac = source.z_frac,
        .color_rgb = color_rgb,
        ._pad = 0u,
    };
}

enum class light_source_kind : int {
    static_emitter,
    static_local_emitter,
    field,
    active_item,
    vehicle,
    character,
    monster,
};

struct source_collection_stats {
    int static_sources = 0;
    int static_local_sources = 0;
    int field_sources = 0;
    int active_item_sources = 0;
    int vehicle_sources = 0;
    int character_sources = 0;
    int monster_sources = 0;
};

struct source_collection {
    std::vector<GpuLightSource> sources;
    std::vector<GpuLightSource> static_sources;
    std::vector<GpuLightSource> dynamic_sources;
    std::vector<GpuColoredLightSource> colored_sources;
    source_collection_stats stats;
};

struct raytrace_source_bucket {
    Uint32 source_offset = 0;
    Uint32 source_count = 0;
    int max_radius = 0;
    Uint32 groups_xy = 0;
};

struct source_accumulator {
    map const& m;
    std::vector<int> const& dirty_levels;
    int cache_xy = 0;
    std::unordered_map<std::size_t, std::size_t> source_indices;
    std::unordered_map<std::size_t, std::size_t> static_source_indices;
    std::unordered_map<std::size_t, std::size_t> dynamic_source_indices;
    std::vector<GpuLightSource> sources;
    std::vector<GpuLightSource> static_sources;
    std::vector<GpuLightSource> dynamic_sources;
    std::vector<GpuColoredLightSource> colored_sources;
    std::vector<light_source_kind> source_kinds;
    bool collect_colored_sources = false;
};

struct colored_item_light {
    float luminance = 0.0f;
    uint32_t color_rgb = 0u;
};

struct add_colored_source_options {
    source_accumulator& acc;
    GpuLightSource source;
    std::optional<uint32_t> color_rgb;
};

struct add_colored_point_source_options {
    source_accumulator& acc;
    tripoint_bub_ms pos;
    float luminance = 0.0f;
    std::optional<uint32_t> color_rgb;
    uint32_t flags = 0u;
};

auto increment_source_stat(source_collection_stats& stats, light_source_kind const kind) -> void {
    switch (kind) {
        case light_source_kind::static_emitter:
            ++stats.static_sources;
            break;
        case light_source_kind::static_local_emitter:
            ++stats.static_local_sources;
            break;
        case light_source_kind::field:
            ++stats.field_sources;
            break;
        case light_source_kind::active_item:
            ++stats.active_item_sources;
            break;
        case light_source_kind::vehicle:
            ++stats.vehicle_sources;
            break;
        case light_source_kind::character:
            ++stats.character_sources;
            break;
        case light_source_kind::monster:
            ++stats.monster_sources;
            break;
    }
}

auto make_source_stats(std::vector<light_source_kind> const& kinds) -> source_collection_stats {
    auto stats = source_collection_stats{};
    for (auto const kind : kinds) { increment_source_stat(stats, kind); }
    return stats;
}

auto upsert_source(
    std::vector<GpuLightSource>& sources, std::unordered_map<std::size_t, std::size_t>& indices,
    std::size_t const source_slot, tripoint_bub_ms const& pos, float const luminance)
    -> std::size_t {
    auto const [iter, inserted] = indices.emplace(source_slot, sources.size());
    if (inserted) {
        sources.push_back(make_source(pos.x(), pos.y(), pos.z(), luminance));
        return sources.size() - 1;
    }

    auto& source = sources[iter->second];
    if (luminance > source.luminance) {
        source.luminance = luminance;
        source.radius = compute_light_radius(luminance);
    }
    return iter->second;
}

auto source_pos(GpuLightSource const& source) -> tripoint_bub_ms {
    return tripoint_bub_ms(source.x, source.y, source.z_idx - OVERMAP_DEPTH);
}

auto source_is_static_raytrace(light_source_kind const kind) -> bool {
    return kind == light_source_kind::static_emitter
        || kind == light_source_kind::static_local_emitter;
}

auto static_emitter_kind(map_data_common_t const& data) -> light_source_kind {
    return data.has_flag("LOCAL_LIGHT")
             ? light_source_kind::static_local_emitter
             : light_source_kind::static_emitter;
}

template <typename Source> auto source_radius_ceiling(Source const& source) -> int {
    return std::max(0, static_cast<int>(std::ceil(source.radius)));
}

auto raytrace_groups_for_radius(int const radius) -> Uint32 {
    return static_cast<Uint32>((2 * radius + 1 + 7) / 8);
}

template <typename Source>
auto make_raytrace_buckets(std::vector<Source>& sources, Uint32 const base_offset = 0)
    -> std::vector<raytrace_source_bucket> {
    if (sources.empty()) { return {}; }

    std::ranges::sort(sources, [](Source const& lhs, Source const& rhs) {
        auto const lhs_groups = raytrace_groups_for_radius(source_radius_ceiling(lhs));
        auto const rhs_groups = raytrace_groups_for_radius(source_radius_ceiling(rhs));
        if (lhs_groups != rhs_groups) { return lhs_groups < rhs_groups; }
        return lhs.radius < rhs.radius;
    });

    auto buckets = std::vector<raytrace_source_bucket>{};
    auto next_offset = base_offset;
    for (auto const& source : sources) {
        auto const radius = source_radius_ceiling(source);
        auto const groups_xy = raytrace_groups_for_radius(radius);
        if (buckets.empty() || buckets.back().groups_xy != groups_xy) {
            buckets.push_back(raytrace_source_bucket{
                .source_offset = next_offset,
                .source_count = 0,
                .max_radius = radius,
                .groups_xy = groups_xy,
            });
        }
        auto& bucket = buckets.back();
        ++bucket.source_count;
        bucket.max_radius = std::max(bucket.max_radius, radius);
        ++next_offset;
    }
    return buckets;
}

auto raytrace_work_units(std::vector<raytrace_source_bucket> const& buckets) -> int64_t {
    auto work = int64_t{0};
    for (auto const& bucket : buckets) {
        auto const groups = static_cast<int64_t>(bucket.groups_xy);
        work += static_cast<int64_t>(bucket.source_count) * groups * groups;
    }
    return work;
}

template <typename Source>
auto source_z_level_count(Source const& source, int const z_count) -> int {
    auto const z_span = static_cast<int>(std::ceil(source.radius / Z_LEVEL_SCALE));
    auto const z_min = std::max(0, source.z_idx - z_span);
    auto const z_max = std::min(z_count - 1, source.z_idx + z_span);
    return std::max(0, z_max - z_min + 1);
}

template <typename Source>
auto raytrace_z_work_units(std::vector<Source> const& sources) -> int64_t {
    auto work = int64_t{0};
    for (auto const& source : sources) {
        auto const groups = static_cast<int64_t>(
            raytrace_groups_for_radius(source_radius_ceiling(source)));
        work +=
            groups * groups * static_cast<int64_t>(source_z_level_count(source, OVERMAP_LAYERS));
    }
    return work;
}

auto mix_signature(std::size_t& seed, std::size_t const value) -> void {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

auto quantized_signature_float(float const value, float const scale) -> std::size_t {
    return static_cast<std::size_t>(std::lround(value * scale));
}

auto ambient_signature(lm_ambient_push_constants const& ambient) -> std::size_t {
    auto seed = std::size_t{0};
    mix_signature(seed, static_cast<std::size_t>(ambient.angled_sunlight_shadows));
    mix_signature(seed, static_cast<std::size_t>(ambient.direct_sunlight));
    mix_signature(seed, quantized_signature_float(ambient.sun_dx_per_z, 8.0f));
    mix_signature(seed, quantized_signature_float(ambient.sun_dy_per_z, 8.0f));
    mix_signature(seed, quantized_signature_float(ambient.inside_light, 4.0f));
    mix_signature(seed, quantized_signature_float(ambient.solar_shadow_light, 4.0f));
    for (auto const& natural_light : ambient.natural_light) {
        for (auto const value : natural_light) {
            mix_signature(seed, quantized_signature_float(value, 4.0f));
        }
    }
    return seed;
}

auto source_signature(std::vector<GpuLightSource> const& sources) -> std::size_t {
    auto seed = std::size_t{0};
    mix_signature(seed, sources.size());
    for (auto const& source : sources) {
        mix_signature(seed, static_cast<std::size_t>(source.x));
        mix_signature(seed, static_cast<std::size_t>(source.y));
        mix_signature(seed, static_cast<std::size_t>(source.z_idx));
        mix_signature(seed, static_cast<std::size_t>(source.flags));
        mix_signature(seed, quantized_signature_float(source.luminance, 4.0f));
        mix_signature(seed, quantized_signature_float(source.dir_x, 1024.0f));
        mix_signature(seed, quantized_signature_float(source.dir_y, 1024.0f));
        mix_signature(seed, quantized_signature_float(source.cone_cos, 1024.0f));
        mix_signature(seed, quantized_signature_float(source.z_frac, 1024.0f));
    }
    return seed;
}

auto structural_level_signature(map const& m, int const z, int const cache_x, int const cache_y)
    -> std::size_t {
    auto seed = std::size_t{0};
    mix_signature(seed, static_cast<std::size_t>(cache_x));
    mix_signature(seed, static_cast<std::size_t>(cache_y));
    mix_signature(seed, static_cast<std::size_t>(z + OVERMAP_DEPTH));

    auto const& lc = m.get_cache_ref(z);
    for (auto const value : lc.transparency_cache) {
        mix_signature(seed, static_cast<std::size_t>(std::bit_cast<uint32_t>(value)));
    }
    for (auto const value : lc.floor_cache) {
        mix_signature(seed, static_cast<std::size_t>(static_cast<unsigned char>(value)));
    }
    for (auto const value : lc.vehicle_floor_cache) {
        mix_signature(seed, static_cast<std::size_t>(static_cast<unsigned char>(value)));
    }
    for (auto const& value : lc.vehicle_obscured_cache) {
        auto const packed =
            (value.nw ? std::size_t{1} : std::size_t{0})
            | (value.ne ? std::size_t{2} : std::size_t{0});
        mix_signature(seed, packed);
    }

    return seed;
}

auto dirty_level_index(source_accumulator const& acc, int const zlev) -> int {
    auto const iter = std::ranges::find(acc.dirty_levels, zlev);
    if (iter == acc.dirty_levels.end()) { return -1; }
    return static_cast<int>(std::distance(acc.dirty_levels.begin(), iter));
}

auto color_rgb_from_optional(std::optional<RGBColor> const& color) -> std::optional<uint32_t> {
    if (!color || !color_is_set(*color)) { return std::nullopt; }
    return pack_rgb(*color);
}

auto colored_source_can_be_added(source_accumulator const& acc, GpuLightSource const& source)
    -> bool {
    if (source.luminance <= LIGHT_AMBIENT_LOW) { return false; }

    auto const pos = source_pos(source);
    if (!acc.m.inbounds(pos)) { return false; }
    if (dirty_level_index(acc, pos.z()) < 0) { return false; }

    auto const& lc = acc.m.get_cache_ref(pos.z());
    return lc.inbounds(pos.xy());
}

auto add_colored_source(add_colored_source_options const& opt) -> void {
    if (!opt.acc.collect_colored_sources || !opt.color_rgb) { return; }
    if (!colored_source_can_be_added(opt.acc, opt.source)) { return; }

    opt.acc.colored_sources.push_back(make_colored_source(opt.source, *opt.color_rgb));
}

auto add_colored_point_source(add_colored_point_source_options const& opt) -> void {
    add_colored_source({
        .acc = opt.acc,
        .source = make_source(opt.pos.x(), opt.pos.y(), opt.pos.z(), opt.luminance, opt.flags),
        .color_rgb = opt.color_rgb,
    });
}

auto item_light_color(item const& itm) -> std::optional<uint32_t> {
    return color_rgb_from_optional(itm.typeId().obj().light_color);
}

auto character_colored_item_light(Character const& ch) -> std::optional<colored_item_light> {
    auto best = std::optional<colored_item_light>{};
    ch.has_item_with([&best](item const& itm) {
        auto const color_rgb = item_light_color(itm);
        auto const luminance = static_cast<float>(itm.getlight_emit());
        if (color_rgb && luminance > LIGHT_AMBIENT_LOW && (!best || luminance > best->luminance)) {
            best = colored_item_light{
                .luminance = luminance,
                .color_rgb = *color_rgb,
            };
        }
        return false;
    });
    return best;
}

auto field_light_color(field_entry const& entry) -> std::optional<uint32_t> {
    return color_rgb_from_optional(
        entry.get_field_type().obj().get_light_color(entry.get_field_intensity() - 1));
}

auto map_data_light_color(map_data_common_t const& data) -> std::optional<uint32_t> {
    return color_rgb_from_optional(data.light_color);
}

auto add_source(
    source_accumulator& acc, tripoint_bub_ms const& pos, float const luminance,
    light_source_kind const kind) -> void {
    if (luminance <= LIGHT_AMBIENT_LOW || !acc.m.inbounds(pos)) { return; }

    auto const level_index = dirty_level_index(acc, pos.z());
    if (level_index < 0) { return; }

    auto const& lc = acc.m.get_cache_ref(pos.z());
    if (!lc.inbounds(pos.xy())) { return; }

    auto const source_slot =
        static_cast<std::size_t>(level_index) * static_cast<std::size_t>(acc.cache_xy)
        + static_cast<std::size_t>(lc.idx(pos.x(), pos.y()));
    auto const old_size = acc.sources.size();
    auto const source_index =
        upsert_source(acc.sources, acc.source_indices, source_slot, pos, luminance);
    if (acc.sources.size() != old_size) {
        acc.source_kinds.push_back(kind);
    } else if (acc.sources[source_index].luminance <= luminance) {
        acc.source_kinds[source_index] = kind;
    }

    if (source_is_static_raytrace(kind)) {
        upsert_source(acc.static_sources, acc.static_source_indices, source_slot, pos, luminance);
    } else {
        upsert_source(acc.dynamic_sources, acc.dynamic_source_indices, source_slot, pos, luminance);
    }
}

auto append_source(
    source_accumulator& acc, GpuLightSource const& source, light_source_kind const kind) -> void {
    if (source.luminance <= LIGHT_AMBIENT_LOW) { return; }

    auto const pos = source_pos(source);
    if (!acc.m.inbounds(pos)) { return; }

    auto const level_index = dirty_level_index(acc, pos.z());
    if (level_index < 0) { return; }

    auto const& lc = acc.m.get_cache_ref(pos.z());
    if (!lc.inbounds(pos.xy())) { return; }

    acc.sources.push_back(source);
    acc.source_kinds.push_back(kind);

    if (source_is_static_raytrace(kind)) {
        acc.static_sources.push_back(source);
    } else {
        acc.dynamic_sources.push_back(source);
    }
}


auto add_field_sources(source_accumulator& acc, tripoint_bub_ms const& pos, field const& fields)
    -> void {
    for (auto const& field_pair : fields) {
        if (!field_pair.first.is_valid()) { continue; }
        auto const luminance = field_pair.second.light_emitted();
        add_source(acc, pos, luminance, light_source_kind::field);
        add_colored_point_source({
            .acc = acc,
            .pos = pos,
            .luminance = luminance,
            .color_rgb = field_light_color(field_pair.second),
        });
    }
}

auto static_emitter_tiles(submap& sm) -> std::vector<point_sm_ms> const& {
    if (!sm.emitter_cache.has_value()) {
        ZoneScopedN("gpu_lm_rebuild_static_emitter_cache");
        auto emitters = std::vector<point_sm_ms>{};
        for (auto const sm_ms : submap_tiles()) {
            auto const terrain = sm.get_ter(sm_ms);
            auto const furniture = sm.get_furn(sm_ms);
            if (terrain->light_emitted > LIGHT_AMBIENT_LOW
                || furniture->light_emitted > LIGHT_AMBIENT_LOW) {
                emitters.push_back(sm_ms);
            }
        }
        sm.emitter_cache = std::move(emitters);
    }
    return *sm.emitter_cache;
}

auto add_static_emitter_sources(source_accumulator& acc) -> void {
    ZoneScopedN("gpu_lm_collect_static_emitters");
    for (auto const z : acc.dirty_levels) {
        auto const& lc = acc.m.get_cache_ref(z);
        for (auto const smx : std::views::iota(0, lc.cache_mapsize)) {
            for (auto const smy : std::views::iota(0, lc.cache_mapsize)) {
                auto const grid = tripoint_bub_sm(smx, smy, z);
                auto* const sm = acc.m.get_submap_at_grid(grid);
                if (sm == nullptr) { continue; }

                for (auto const sm_ms : static_emitter_tiles(*sm)) {
                    auto const pos = project_combine(grid, sm_ms);
                    auto const terrain = sm->get_ter(sm_ms);
                    auto const terrain_luminance = static_cast<float>(terrain->light_emitted);
                    add_source(acc, pos, terrain_luminance, static_emitter_kind(terrain.obj()));
                    add_colored_point_source({
                        .acc = acc,
                        .pos = pos,
                        .luminance = terrain_luminance,
                        .color_rgb = map_data_light_color(terrain.obj()),
                    });

                    auto const furniture = sm->get_furn(sm_ms);
                    auto const furniture_luminance = static_cast<float>(furniture->light_emitted);
                    add_source(acc, pos, furniture_luminance, static_emitter_kind(furniture.obj()));
                    add_colored_point_source({
                        .acc = acc,
                        .pos = pos,
                        .luminance = furniture_luminance,
                        .color_rgb = map_data_light_color(furniture.obj()),
                    });
                }
            }
        }
    }
}

auto add_field_sources(source_accumulator& acc) -> void {
    ZoneScopedN("gpu_lm_collect_field_sources");
    for (auto const z : acc.dirty_levels) {
        auto const& lc = acc.m.get_cache_ref(z);
        for (auto const smx : std::views::iota(0, lc.cache_mapsize)) {
            for (auto const smy : std::views::iota(0, lc.cache_mapsize)) {
                auto const grid = tripoint_bub_sm(smx, smy, z);
                auto const* const sm = acc.m.get_submap_at_grid(grid);
                if (sm == nullptr || sm->field_count == 0) { continue; }

                for (auto const sm_ms : sm->field_cache) {
                    auto const& fields = sm->get_field(sm_ms);
                    if (fields.field_count() == 0) { continue; }
                    add_field_sources(acc, project_combine(grid, sm_ms), fields);
                }
            }
        }
    }
}

auto add_active_item_sources(source_accumulator& acc) -> void {
    ZoneScopedN("gpu_lm_collect_active_item_sources");
    for (tripoint_abs_sm const& abs_pos : acc.m.get_submaps_with_active_items()) {
        auto const local_pos = abs_to_bub(abs_pos);
        if (dirty_level_index(acc, local_pos.z()) < 0) { continue; }

        auto* const sm = acc.m.get_submap_at_grid(local_pos);
        if (sm == nullptr || sm->active_items.empty()) { continue; }

        for (item const* const itm : sm->active_items.get()) {
            if (itm == nullptr) { continue; }
            auto luminance = 0.0f;
            auto width = 0_degrees;
            auto direction = 0_degrees;
            if (itm->getlight(luminance, width, direction)) {
                auto const pos = tripoint_bub_ms(itm->position());
                add_source(acc, pos, luminance, light_source_kind::active_item);
                add_colored_point_source({
                    .acc = acc,
                    .pos = pos,
                    .luminance = luminance,
                    .color_rgb = item_light_color(*itm),
                });
            }
        }
    }
}

auto active_vehicle_light_parts(vehicle& veh) -> std::vector<vpart_reference> {
    auto lights = std::vector<vpart_reference>{};
    for (vpart_reference const& part : veh.get_all_parts()) {
        auto const& vehicle_part = part.part();
        if (vehicle_part.enabled && vehicle_part.is_available() && vehicle_part.is_light()) {
            lights.push_back(part);
        }
    }
    return lights;
}

auto vehicle_light_is_directional(vpart_info const& info) -> bool {
    return info.has_flag(VPFLAG_CONE_LIGHT) || info.has_flag(VPFLAG_WIDE_CONE_LIGHT)
        || info.has_flag(VPFLAG_HALF_CIRCLE_LIGHT);
}

auto vehicle_light_arc_width(vpart_info const& info) -> units::angle {
    if (info.has_flag(VPFLAG_CONE_LIGHT)) { return 45_degrees; }
    if (info.has_flag(VPFLAG_WIDE_CONE_LIGHT)) { return 90_degrees; }
    if (info.has_flag(VPFLAG_HALF_CIRCLE_LIGHT)) { return 180_degrees; }
    return 360_degrees;
}

auto vehicle_light_is_external(vpart_reference const& part) -> bool {
    auto const& info = part.info();
    return info.location == "on_roof" || vehicle_light_is_directional(info)
        || info.rotating_light.has_value();
}

auto vehicle_light_flags(vpart_reference const& part) -> uint32_t {
    return vehicle_light_is_external(part) ? light_source_external_vehicle : uint32_t{0};
}

auto vehicle_circle_light_is_active(vpart_info const& info, bool const odd_turn) -> bool {
    if (!info.has_flag(VPFLAG_CIRCLE_LIGHT)) { return true; }
    return (odd_turn && info.has_flag(VPFLAG_ODDTURN))
        || (!odd_turn && info.has_flag(VPFLAG_EVENTURN))
        || (!(info.has_flag(VPFLAG_EVENTURN) || info.has_flag(VPFLAG_ODDTURN)));
}

auto vehicle_light_color(vpart_reference const& part) -> std::optional<uint32_t> {
    if (part.info().light_color && color_is_set(*part.info().light_color)) {
        return pack_rgb(*part.info().light_color);
    }

    auto const [bg, fg] = part.part().get_color(true);
    if (color_is_set(fg)) { return pack_rgb(fg); }
    if (color_is_set(bg)) { return pack_rgb(bg); }
    return std::nullopt;
}

auto append_vehicle_source(
    source_accumulator& acc, GpuLightSource const& source, std::optional<uint32_t> const color_rgb)
    -> void {
    append_source(acc, source, light_source_kind::vehicle);
    add_colored_source({
        .acc = acc,
        .source = source,
        .color_rgb = color_rgb,
    });
}

auto add_vehicle_sources(source_accumulator& acc) -> void {
    ZoneScopedN("gpu_lm_collect_vehicle_sources");
    auto const odd_turn = calendar::once_every(2_turns);
    for (auto const& wrapped : const_cast<map&>(acc.m).get_vehicles()) {
        auto* const veh = wrapped.v;
        if (veh == nullptr) { continue; }

        auto const lights = active_vehicle_light_parts(*veh);
        auto vehicle_luminance = 0.0f;
        auto iteration = 1.0f;
        for (vpart_reference const& part : lights) {
            auto const& info = part.info();
            if (info.has_flag(VPFLAG_CONE_LIGHT) || info.has_flag(VPFLAG_WIDE_CONE_LIGHT)) {
                vehicle_luminance += info.bonus / iteration;
                iteration = iteration * 1.1f;
            }
        }

        for (vpart_reference const& part : lights) {
            auto const& info = part.info();
            auto const& vehicle_part = part.part();
            auto const pos = part.pos();
            if (!acc.m.inbounds(pos)) { continue; }

            auto const flags = vehicle_light_flags(part);
            auto const color_rgb =
                acc.collect_colored_sources ? vehicle_light_color(part) : std::optional<uint32_t>{};
            if (info.has_flag(VPFLAG_CONE_LIGHT) || info.has_flag(VPFLAG_WIDE_CONE_LIGHT)) {
                if (vehicle_luminance > lit_level::LIT) {
                    append_vehicle_source(
                        acc,
                        make_directional_source(
                            pos, vehicle_luminance, veh->face.dir() + vehicle_part.direction,
                            vehicle_light_arc_width(info), flags),
                        color_rgb);
                }
            } else if (info.rotating_light) {
                auto const& rotating_light = *info.rotating_light;
                auto const base_direction = veh->face.dir() + vehicle_part.direction;
                auto const direction = rotating_light.direction_at(base_direction, calendar::turn);
                for (auto const beam_index : std::views::iota(0, rotating_light.beam_count())) {
                    auto const beam_direction =
                        direction + rotating_light.beam_spacing() * static_cast<double>(beam_index);
                    append_vehicle_source(
                        acc,
                        make_directional_source(
                            pos, static_cast<float>(info.bonus), beam_direction,
                            rotating_light.arc_width(), flags),
                        color_rgb);
                }
            } else if (info.has_flag(VPFLAG_HALF_CIRCLE_LIGHT)) {
                append_vehicle_source(
                    acc,
                    make_directional_source(
                        pos, static_cast<float>(info.bonus),
                        veh->face.dir() + vehicle_part.direction, vehicle_light_arc_width(info),
                        flags),
                    color_rgb);
            } else if (info.has_flag(VPFLAG_CIRCLE_LIGHT)) {
                if (vehicle_circle_light_is_active(info, odd_turn)) {
                    append_vehicle_source(
                        acc,
                        make_source(pos.x(), pos.y(), pos.z(), static_cast<float>(info.bonus),
                                    flags),
                        color_rgb);
                }
            } else {
                append_vehicle_source(
                    acc,
                    make_source(pos.x(), pos.y(), pos.z(), static_cast<float>(info.bonus), flags),
                    color_rgb);
            }
        }

        for (vpart_reference const& part : veh->get_all_parts()) {
            if (!part.has_feature(VPFLAG_CARGO) || part.has_feature("COVERED")) { continue; }

            auto const pos = part.pos();
            if (!acc.m.inbounds(pos)) { continue; }

            auto const part_index = static_cast<int>(part.part_index());
            for (auto const& itm : veh->get_items(part_index)) {
                auto luminance = 0.0f;
                auto width = 0_degrees;
                auto direction = 0_degrees;
                if (itm->getlight(luminance, width, direction)) {
                    add_source(acc, pos, luminance, light_source_kind::vehicle);
                    add_colored_point_source({
                        .acc = acc,
                        .pos = pos,
                        .luminance = luminance,
                        .color_rgb = item_light_color(*itm),
                    });
                }
            }
        }
    }
}

auto add_character_sources(source_accumulator& acc) -> void {
    ZoneScopedN("gpu_lm_collect_character_sources");
    static const efftype_id effect_onfire("onfire");

    auto add_char = [&](Character const& ch) {
        auto const& pos = ch.bub_pos();
        if (!acc.m.inbounds(pos)) { return; }
        if (ch.has_effect(effect_onfire)) {
            add_source(acc, pos, 8.0f, light_source_kind::character);
        }
        add_source(acc, pos, ch.active_light(), light_source_kind::character);
        if (auto const colored_light = character_colored_item_light(ch)) {
            add_colored_point_source({
                .acc = acc,
                .pos = pos,
                .luminance = colored_light->luminance,
                .color_rgb = colored_light->color_rgb,
            });
        }
    };

    add_char(get_player_character());
    for (npc const& guy : g->all_npcs()) { add_char(guy); }
}

auto add_monster_sources(source_accumulator& acc) -> void {
    ZoneScopedN("gpu_lm_collect_monster_sources");
    static const efftype_id effect_onfire("onfire");

    for (monster const& critter : g->all_monsters()) {
        if (critter.is_hallucination()) { continue; }
        auto const& pos = critter.bub_pos();
        if (!acc.m.inbounds(pos)) { continue; }
        if (critter.has_effect(effect_onfire)) {
            add_source(acc, pos, 8.0f, light_source_kind::monster);
        }
        add_source(acc, pos, static_cast<float>(critter.type->luminance),
                   light_source_kind::monster);
    }
}

auto collect_sources(
    map const& m, std::vector<int> const& dirty_levels, bool const collect_colored_sources)
    -> source_collection {
    if (dirty_levels.empty()) { return {}; }

    auto const& lc0 = m.get_cache_ref(dirty_levels.front());
    auto acc = source_accumulator{
        .m = m,
        .dirty_levels = dirty_levels,
        .cache_xy = lc0.cache_x * lc0.cache_y,
        .source_indices = {},
        .static_source_indices = {},
        .dynamic_source_indices = {},
        .sources = {},
        .static_sources = {},
        .dynamic_sources = {},
        .colored_sources = {},
        .source_kinds = {},
        .collect_colored_sources = collect_colored_sources,
    };
    acc.source_indices.reserve(256);
    acc.static_source_indices.reserve(256);
    acc.dynamic_source_indices.reserve(256);
    acc.source_kinds.reserve(256);

    add_static_emitter_sources(acc);
    add_field_sources(acc);
    add_active_item_sources(acc);
    add_vehicle_sources(acc);
    add_character_sources(acc);
    add_monster_sources(acc);

    return source_collection{
        .sources = std::move(acc.sources),
        .static_sources = std::move(acc.static_sources),
        .dynamic_sources = std::move(acc.dynamic_sources),
        .colored_sources = std::move(acc.colored_sources),
        .stats = make_source_stats(acc.source_kinds),
    };
}

auto source_is_on_dirty_level(std::vector<int> const& dirty_levels, int const zlev) -> bool {
    return std::ranges::find(dirty_levels, zlev) != dirty_levels.end();
}

auto write_source_map_to_level_caches(
    map const& m, std::vector<int> const& dirty_levels, std::vector<GpuLightSource> const& sources)
    -> void {
    for (int const z : dirty_levels) {
        auto& lc = const_cast<level_cache&>(m.get_cache_ref(z));
        std::ranges::fill(lc.sm, 0.0f);
    }
    for (auto const& source : sources) {
        if ((source.flags & light_source_external_vehicle) != 0u) { continue; }
        auto const zlev = source.z_idx - OVERMAP_DEPTH;
        if (!source_is_on_dirty_level(dirty_levels, zlev)) { continue; }
        auto& lc = const_cast<level_cache&>(m.get_cache_ref(zlev));
        if (source.x < 0 || source.y < 0 || source.x >= lc.cache_x || source.y >= lc.cache_y) {
            continue;
        }
        auto& value = lc.sm[lc.idx(source.x, source.y)];
        value = std::max(value, source.luminance);
    }
}

auto clear_colored_light_caches(map const& m, std::vector<int> const& levels) -> void {
    for (auto const z : levels) {
        auto& lc = const_cast<level_cache&>(m.get_cache_ref(z));
        if (!lc.colored_light_cache_active) { continue; }
        std::ranges::fill(lc.colored_light_cache, 0u);
        lc.colored_light_cache_active = false;
    }
}

// ---------------------------------------------------------------------------
// Input buffer packing (CPU → flat 3D arrays for GPU upload)
// ---------------------------------------------------------------------------

auto make_all_levels(int const z_count) -> std::vector<int> {
    auto levels = std::vector<int>(z_count);
    std::iota(levels.begin(), levels.end(), -OVERMAP_DEPTH);
    return levels;
}

auto reset_input_residency_for_shape(int const cache_x, int const cache_y, int const z_count)
    -> void {
    auto& inputs = s_lighting_resources.inputs;
    if (inputs.cache_x == cache_x && inputs.cache_y == cache_y && inputs.z_count == z_count) {
        return;
    }
    inputs = lighting_input_residency{
        .cache_x = cache_x,
        .cache_y = cache_y,
        .z_count = z_count,
        .transparency_valid = false,
        .floor_valid = false,
        .vehicle_floor_valid = false,
        .vehicle_obscured_valid = false,
        .transparency_valid_levels = std::vector<char>(static_cast<std::size_t>(z_count), '\0'),
        .transparency_shader_updated_levels =
            std::vector<char>(static_cast<std::size_t>(z_count), '\0'),
    };
    s_lighting_resources.seen_valid = false;
    s_lighting_resources.seen_origin_valid = false;
    s_lighting_resources.seen_valid_levels =
        std::vector<char>(static_cast<std::size_t>(z_count), '\0');
    s_lighting_resources.camera_valid = false;
    s_lighting_resources.camera_nonzero_levels = {};
    s_lighting_resources.source_map_valid = false;
    s_lighting_resources.static_lighting_valid = false;
    s_lighting_resources.static_structural_level_signature_valid =
        std::vector<char>(static_cast<std::size_t>(z_count), '\0');
    s_lighting_resources.static_structural_level_signatures =
        std::vector<std::size_t>(static_cast<std::size_t>(z_count), std::size_t{0});
    s_lighting_resources.lighting_outputs_valid = false;
}

auto z_to_resident_index(int const zlev) -> int { return zlev + OVERMAP_DEPTH; }

auto resident_index_is_valid(int const idx, std::vector<char> const& levels) -> bool {
    return idx >= 0 && static_cast<std::size_t>(idx) < levels.size();
}

auto transparency_level_is_valid(int const zlev) -> bool {
    auto const& levels = s_lighting_resources.inputs.transparency_valid_levels;
    auto const idx = z_to_resident_index(zlev);
    return resident_index_is_valid(idx, levels) && levels[static_cast<std::size_t>(idx)] != '\0';
}

auto transparency_level_shader_updated(int const zlev) -> bool {
    auto const& levels = s_lighting_resources.inputs.transparency_shader_updated_levels;
    auto const idx = z_to_resident_index(zlev);
    return resident_index_is_valid(idx, levels) && levels[static_cast<std::size_t>(idx)] != '\0';
}

auto refresh_transparency_valid_flag() -> void {
    auto& inputs = s_lighting_resources.inputs;
    inputs.transparency_valid =
        !inputs.transparency_valid_levels.empty()
        && std::ranges::all_of(inputs.transparency_valid_levels, [](char const value) {
               return value != '\0';
           });
}

auto invalidate_resident_transparency() -> void {
    auto& inputs = s_lighting_resources.inputs;
    std::ranges::fill(inputs.transparency_valid_levels, '\0');
    std::ranges::fill(inputs.transparency_shader_updated_levels, '\0');
    refresh_transparency_valid_flag();
}

auto refresh_seen_valid_flag() -> void {
    auto& resources = s_lighting_resources;
    resources.seen_valid =
        !resources.seen_valid_levels.empty()
        && std::ranges::all_of(resources.seen_valid_levels, [](char const value) {
               return value != '\0';
           });
}

auto reset_seen_residency(int const z_count) -> void {
    auto& resources = s_lighting_resources;
    resources.seen_valid = false;
    resources.seen_origin_valid = false;
    resources.seen_valid_levels = std::vector<char>(static_cast<std::size_t>(z_count), '\0');
}

auto seen_levels_valid(std::vector<int> const& levels, int const player_x, int const player_y)
    -> bool {
    auto const& resources = s_lighting_resources;
    if (!resources.seen_origin_valid || resources.seen_origin_x != player_x
        || resources.seen_origin_y != player_y) {
        return false;
    }
    return std::ranges::all_of(levels, [&](int const z) {
        auto const idx = z_to_resident_index(z);
        return resident_index_is_valid(idx, resources.seen_valid_levels)
            && resources.seen_valid_levels[static_cast<std::size_t>(idx)] != '\0';
    });
}

auto mark_seen_rebuild_success(
    std::vector<int> const& levels, int const player_x, int const player_y,
    bool const force_invalidate) -> void {
    auto& resources = s_lighting_resources;
    if (resources.seen_valid_levels.size() != static_cast<std::size_t>(OVERMAP_LAYERS)
        || force_invalidate || !resources.seen_origin_valid || resources.seen_origin_x != player_x
        || resources.seen_origin_y != player_y) {
        resources.seen_valid_levels =
            std::vector<char>(static_cast<std::size_t>(OVERMAP_LAYERS), '\0');
    }
    for (auto const z : levels) {
        auto const idx = z_to_resident_index(z);
        if (resident_index_is_valid(idx, resources.seen_valid_levels)) {
            resources.seen_valid_levels[static_cast<std::size_t>(idx)] = '\1';
        }
    }
    resources.seen_origin_valid = true;
    resources.seen_origin_x = player_x;
    resources.seen_origin_y = player_y;
    refresh_seen_valid_flag();
}

auto mark_transparency_levels_valid(std::vector<int> const& levels) -> void {
    auto& inputs = s_lighting_resources.inputs;
    for (auto const z : levels) {
        auto const idx = z_to_resident_index(z);
        if (resident_index_is_valid(idx, inputs.transparency_valid_levels)) {
            inputs.transparency_valid_levels[static_cast<std::size_t>(idx)] = '\1';
        }
    }
    refresh_transparency_valid_flag();
}

auto clear_transparency_shader_update_marks() -> void {
    std::ranges::fill(s_lighting_resources.inputs.transparency_shader_updated_levels, '\0');
}

auto select_input_upload_levels(
    bool const dirty, bool const resident_valid, std::vector<int> const* const dirty_levels,
    std::vector<int> const& all_levels) -> std::vector<int> {
    if (!dirty && resident_valid) { return {}; }
    if (!resident_valid || dirty_levels == nullptr || dirty_levels->empty()) { return all_levels; }

    auto levels = *dirty_levels;
    std::ranges::sort(levels);
    levels.erase(std::ranges::unique(levels).begin(), levels.end());
    return levels;
}

auto select_transparency_upload_levels(
    bool const dirty, std::vector<int> const* const dirty_levels,
    std::vector<int> const& all_levels) -> std::vector<int> {
    auto levels = std::vector<int>{};

    for (auto const z : all_levels) {
        if (!transparency_level_is_valid(z)) { levels.push_back(z); }
    }

    if (dirty) {
        auto const& dirty_source =
            (dirty_levels == nullptr || dirty_levels->empty()) ? all_levels : *dirty_levels;
        for (auto const z : dirty_source) {
            if (!transparency_level_shader_updated(z)) { levels.push_back(z); }
        }
    }

    std::ranges::sort(levels);
    levels.erase(std::ranges::unique(levels).begin(), levels.end());
    return levels;
}

auto make_input_upload_plan(run_gpu_lighting_params const& p, std::vector<int> const& all_levels)
    -> input_upload_plan {
    auto const& inputs = s_lighting_resources.inputs;
    return input_upload_plan{
        .transparency_levels = select_transparency_upload_levels(
            p.transparency_dirty, p.transparency_dirty_levels, all_levels),
        .floor_levels = select_input_upload_levels(
            p.floor_dirty, inputs.floor_valid, p.floor_dirty_levels, all_levels),
        .vehicle_floor_levels = select_input_upload_levels(
            p.vehicle_floor_dirty, inputs.vehicle_floor_valid, p.vehicle_floor_dirty_levels,
            all_levels),
        .vehicle_obscured_levels = select_input_upload_levels(
            p.vehicle_obscured_dirty, inputs.vehicle_obscured_valid,
            p.vehicle_obscured_dirty_levels, all_levels),
    };
}

auto has_structural_upload(input_upload_plan const& plan) -> bool {
    return !plan.transparency_levels.empty() || !plan.floor_levels.empty()
        || !plan.vehicle_floor_levels.empty() || !plan.vehicle_obscured_levels.empty();
}

auto commit_input_upload_plan(input_upload_plan const& plan) -> void {
    auto& inputs = s_lighting_resources.inputs;
    if (!plan.transparency_levels.empty()) {
        mark_transparency_levels_valid(plan.transparency_levels);
    }
    if (!plan.floor_levels.empty()) { inputs.floor_valid = true; }
    if (!plan.vehicle_floor_levels.empty()) { inputs.vehicle_floor_valid = true; }
    if (!plan.vehicle_obscured_levels.empty()) { inputs.vehicle_obscured_valid = true; }
}

auto sorted_unique(std::vector<int> levels) -> std::vector<int> {
    std::ranges::sort(levels);
    levels.erase(std::ranges::unique(levels).begin(), levels.end());
    return levels;
}

auto make_seen_download_levels(
    run_gpu_lighting_params const& p, bool const seen_was_valid, std::vector<int> const& all_levels)
    -> std::vector<int> {
    if (!seen_was_valid) { return all_levels; }

    auto levels = std::vector<int>{};
    if (p.seen_dirty_levels != nullptr) {
        std::ranges::copy(*p.seen_dirty_levels, std::back_inserter(levels));
    }
    levels.push_back(p.player_zlev);
    return sorted_unique(std::move(levels));
}

auto make_visibility_download_levels(
    run_gpu_visibility_params const& p, std::vector<int> const& all_levels) -> std::vector<int> {
    if (p.download_levels == nullptr || p.download_levels->empty()) { return all_levels; }

    auto levels = std::vector<int>{};
    std::ranges::copy_if(*p.download_levels, std::back_inserter(levels), [](int const z) {
        return z >= -OVERMAP_DEPTH && z <= OVERMAP_HEIGHT;
    });
    levels = sorted_unique(std::move(levels));
    if (levels.empty() && p.zlev >= -OVERMAP_DEPTH && p.zlev <= OVERMAP_HEIGHT) {
        levels.push_back(p.zlev);
    }
    return levels;
}

struct visibility_dispatch_plan {
    int z_start_idx = 0;
    int z_count = OVERMAP_LAYERS;
    Uint32 tiles = 0;
    bool selected_range = false;
};

struct z_level_range {
    int z_start = 0;
    int z_count = 0;
};

auto make_z_level_ranges(std::vector<int> const& levels) -> std::vector<z_level_range> {
    auto ranges = std::vector<z_level_range>{};
    if (levels.empty()) { return ranges; }

    auto range_start = levels.front();
    auto previous = levels.front();
    for (auto const z : levels | std::views::drop(1)) {
        if (z == previous + 1) {
            previous = z;
            continue;
        }
        ranges.push_back(z_level_range{
            .z_start = range_start,
            .z_count = previous - range_start + 1,
        });
        range_start = z;
        previous = z;
    }
    ranges.push_back(z_level_range{
        .z_start = range_start,
        .z_count = previous - range_start + 1,
    });
    return ranges;
}

auto make_visibility_dispatch_plan(
    std::vector<int> const& levels, int const cache_xy, int const total_z_count)
    -> visibility_dispatch_plan {
    auto fallback = visibility_dispatch_plan{
        .z_start_idx = 0,
        .z_count = total_z_count,
        .tiles = static_cast<Uint32>(total_z_count * cache_xy),
        .selected_range = false,
    };
    if (levels.empty()) { return fallback; }

    auto contiguous = true;
    auto previous = levels.front();
    for (auto const z : levels | std::views::drop(1)) {
        if (z != previous + 1) {
            contiguous = false;
            break;
        }
        previous = z;
    }
    if (!contiguous) { return fallback; }

    auto const selected_z_count = static_cast<int>(levels.size());
    return visibility_dispatch_plan{
        .z_start_idx = levels.front() + OVERMAP_DEPTH,
        .z_count = selected_z_count,
        .tiles = static_cast<Uint32>(selected_z_count * cache_xy),
        .selected_range = selected_z_count != total_z_count,
    };
}

auto make_camera_zero_plan(
    std::vector<int> const& all_levels, int const zlev, bool const has_optics) -> camera_zero_plan {
    auto nonzero_levels = std::vector<int>{};
    if (has_optics) { nonzero_levels.push_back(zlev); }
    if (!s_lighting_resources.camera_valid) {
        return camera_zero_plan{
            .upload_levels = all_levels,
            .nonzero_levels_after_dispatch = std::move(nonzero_levels),
        };
    }

    auto upload_levels = nonzero_levels;
    std::ranges::
        copy(s_lighting_resources.camera_nonzero_levels, std::back_inserter(upload_levels));
    return camera_zero_plan{
        .upload_levels = sorted_unique(std::move(upload_levels)),
        .nonzero_levels_after_dispatch = std::move(nonzero_levels),
    };
}

auto find_vehicle_for_optics_origin(map const& m, tripoint_bub_ms const& origin) -> vehicle* {
    auto const vp = m.veh_at(origin);
    if (vp) { return &vp->vehicle(); }

    auto const& origin_cache = m.get_cache_ref(origin.z());
    auto const it = std::ranges::find_if(origin_cache.vehicle_list, [&](vehicle* const candidate) {
        if (candidate == nullptr) { return false; }
        return !candidate->get_parts_at(origin, std::string{}, part_status_flag::any).empty();
    });
    return it != origin_cache.vehicle_list.end() ? *it : nullptr;
}

auto collect_vehicle_optics(map const& m, tripoint_bub_ms const& origin, int const target_z)
    -> std::vector<GpuVehicleOptic> {
    auto optics = std::vector<GpuVehicleOptic>{};
    auto* const veh = find_vehicle_for_optics_origin(m, origin);
    if (veh == nullptr) { return optics; }

    auto const& target_cache = m.get_cache_ref(target_z);
    auto optic_parts = std::vector<int>{};
    auto cam_control = -1;

    for (vpart_reference const& part_ref : veh->get_avail_parts(VPFLAG_EXTENDS_VISION)) {
        auto const optic_pos = part_ref.pos();
        if (!target_cache.inbounds(optic_pos.xy())) { continue; }
        auto const is_camera_control = part_ref.info().has_flag("CAMERA_CONTROL");
        auto const part_index = static_cast<int>(part_ref.part_index());
        if (is_camera_control) {
            if (square_dist(origin, optic_pos) <= 1 && veh->camera_on) { cam_control = part_index; }
            continue;
        }
        optic_parts.push_back(part_index);
    }

    optics.reserve(optic_parts.size());
    for (auto const part_index : optic_parts) {
        auto const& part_info = veh->part_info(part_index);
        auto const optic_pos = veh->bub_part_location(part_index);
        if (!target_cache.inbounds(optic_pos.xy())) { continue; }
        auto const is_camera = part_info.has_flag("CAMERA");
        if (is_camera) {
            if (cam_control < 0) { continue; }
            auto const durability = std::max(part_info.durability, 1);
            auto const raw_range = part_info.bonus * veh->part(part_index).hp() / durability;
            auto const optic_range = std::clamp(raw_range, 0, g_max_view_distance);
            if (optic_range <= 0) { continue; }
            optics.push_back(GpuVehicleOptic{
                .x = optic_pos.x(),
                .y = optic_pos.y(),
                .z_idx = target_z + OVERMAP_DEPTH,
                .kind = vehicle_optic_camera,
                .range = optic_range,
                .offset_distance = 0,
                ._pad = {},
            });
            continue;
        }

        auto const offset_distance = rl_dist(origin, optic_pos);
        auto const optic_range = std::max(0, g_max_view_distance - offset_distance);
        if (optic_range <= 0) { continue; }
        optics.push_back(GpuVehicleOptic{
            .x = optic_pos.x(),
            .y = optic_pos.y(),
            .z_idx = target_z + OVERMAP_DEPTH,
            .kind = vehicle_optic_mirror,
            .range = optic_range,
            .offset_distance = offset_distance,
            ._pad = {},
        });
    }

    return optics;
}

// Pack selected z-levels into a compact upload buffer.  Copy commands place
// each packed level into its resident 3D-buffer slice.
auto pack_float_cache(
    map const& m, std::vector<int> const& levels, int const cache_xy, auto const cache_accessor,
    std::vector<float>& out) -> void {
    out.resize(levels.size() * static_cast<std::size_t>(cache_xy));
    auto level_index = std::size_t{0};
    for (auto const z : levels) {
        auto const& lc = m.get_cache_ref(z);
        auto const& src = cache_accessor(lc);
        auto* dst = out.data() + level_index * cache_xy;
        std::ranges::copy(src, dst);
        ++level_index;
    }
}

// Pack selected z-levels into a compact uint upload buffer.
// Non-zero char → uint 1; zero → uint 0.
auto pack_char_cache_uint(
    map const& m, std::vector<int> const& levels, int const cache_xy, auto const cache_accessor,
    std::vector<uint32_t>& out) -> void {
    out.resize(levels.size() * static_cast<std::size_t>(cache_xy));
    auto level_index = std::size_t{0};
    for (auto const z : levels) {
        auto const& lc = m.get_cache_ref(z);
        auto const& src = cache_accessor(lc);
        auto* dst = out.data() + level_index * cache_xy;
        std::ranges::transform(src, dst, [](char const c) -> uint32_t { return c != 0 ? 1u : 0u; });
        ++level_index;
    }
}

auto pack_vehicle_obscured_cache_uint(
    map const& m, std::vector<int> const& levels, int const cache_xy, std::vector<uint32_t>& out)
    -> void {
    out.resize(levels.size() * static_cast<std::size_t>(cache_xy));
    auto level_index = std::size_t{0};
    for (auto const z : levels) {
        auto const& lc = m.get_cache_ref(z);
        auto* dst = out.data() + level_index * cache_xy;
        std::ranges::
            transform(lc.vehicle_obscured_cache, dst, [](diagonal_blocks const& value) -> uint32_t {
                return (value.nw ? 1u : 0u) | (value.ne ? 2u : 0u);
            });
        ++level_index;
    }
}

struct record_seen_rebuild_params {
    SDL_GPUCommandBuffer* cmd = nullptr;
    SDL_GPUBuffer* transparency_buf = nullptr;
    SDL_GPUBuffer* floor_buf = nullptr;
    SDL_GPUBuffer* vehicle_floor_buf = nullptr;
    SDL_GPUBuffer* vehicle_obscured_buf = nullptr;
    SDL_GPUBuffer* seen_raw_buf = nullptr;
    SDL_GPUBuffer* seen_buf = nullptr;
    int player_x = 0;
    int player_y = 0;
    int player_zlev = 0;
    int cache_x = 0;
    int cache_y = 0;
    int cache_xy = 0;
    int z_count = 0;
    int z_start_idx = 0;
    int dispatch_z_count = OVERMAP_LAYERS;
    uint32_t vision_block_mask = 0;
};

auto record_seen_rebuild(record_seen_rebuild_params const& p) -> void {
    auto const view_radius = g_max_view_distance;
    auto const diam = static_cast<Uint32>(2 * view_radius + 1);
    auto const g_seen = (diam + 7) / 8;

    {
        auto const rw_seen = std::array<SDL_GPUStorageBufferReadWriteBinding, 2>{
            SDL_GPUStorageBufferReadWriteBinding{
                .buffer = p.seen_raw_buf,
                .cycle = false,
                .padding1 = 0,
                .padding2 = 0,
                .padding3 = 0,
            },
            SDL_GPUStorageBufferReadWriteBinding{
                .buffer = p.seen_buf,
                .cycle = false,
                .padding1 = 0,
                .padding2 = 0,
                .padding3 = 0,
            },
        };
        if (s_lighting_resources.seen_origin_valid) {
            ZoneScopedN("gpu_lm_record_clear_seen_view");
            auto* const cp = SDL_BeginGPUComputePass(
                p.cmd, nullptr, 0, rw_seen.data(), static_cast<Uint32>(rw_seen.size()));
            SDL_BindGPUComputePipeline(cp, s_clear_seen_view_pipeline);

            auto const clear_push = lm_clear_seen_view_push_constants{
                .player_x = s_lighting_resources.seen_origin_x,
                .player_y = s_lighting_resources.seen_origin_y,
                .cache_x = p.cache_x,
                .cache_y = p.cache_y,
                .cache_xy = p.cache_xy,
                .z_count = p.z_count,
                .view_radius = view_radius,
                .z_start_idx = p.z_start_idx,
                .dispatch_z_count = p.dispatch_z_count,
                ._pad = {},
            };
            SDL_PushGPUComputeUniformData(p.cmd, 0, &clear_push, sizeof(clear_push));
            SDL_DispatchGPUCompute(cp, g_seen, g_seen, static_cast<Uint32>(p.dispatch_z_count));
            SDL_EndGPUComputePass(cp);
        } else {
            ZoneScopedN("gpu_lm_record_clear_seen_full");
            auto* const cp = SDL_BeginGPUComputePass(
                p.cmd, nullptr, 0, rw_seen.data(), static_cast<Uint32>(rw_seen.size()));
            SDL_BindGPUComputePipeline(cp, s_clear_seen_pipeline);

            auto const clear_push = lm_clear_seen_push_constants{
                .total_tiles = static_cast<Uint32>(p.dispatch_z_count * p.cache_xy),
                .cache_xy = p.cache_xy,
                .z_start_idx = p.z_start_idx,
                ._pad = 0u,
            };
            SDL_PushGPUComputeUniformData(p.cmd, 0, &clear_push, sizeof(clear_push));
            SDL_DispatchGPUCompute(cp, (clear_push.total_tiles + 63) / 64, 1, 1);
            SDL_EndGPUComputePass(cp);
        }
    }

    auto const seen_push = lm_seen_push_constants{
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
        .cache_x = p.cache_x,
        .cache_y = p.cache_y,
        .cache_xy = p.cache_xy,
        .z_count = p.z_count,
        .view_radius = view_radius,
        .z_scale = Z_LEVEL_SCALE,
        .z_start_idx = p.z_start_idx,
        .dispatch_z_count = p.dispatch_z_count,
        .trigdist = trigdist ? 1u : 0u,
        .vision_block_mask = p.vision_block_mask,
        ._pad = {},
    };

    {
        auto const rw_seen = SDL_GPUStorageBufferReadWriteBinding{
            .buffer = p.seen_raw_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
        auto* const cp = SDL_BeginGPUComputePass(p.cmd, nullptr, 0, &rw_seen, 1);
        SDL_BindGPUComputePipeline(cp, s_seen_pipeline);

        auto const ro_bufs = std::array<
            SDL_GPUBuffer*,
            4>{p.transparency_buf, p.floor_buf, p.vehicle_floor_buf, p.vehicle_obscured_buf};
        SDL_BindGPUComputeStorageBuffers(cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

        SDL_PushGPUComputeUniformData(p.cmd, 0, &seen_push, sizeof(seen_push));
        SDL_DispatchGPUCompute(cp, g_seen, g_seen, static_cast<Uint32>(p.dispatch_z_count));
        SDL_EndGPUComputePass(cp);
    }

    {
        auto const rw_seen = SDL_GPUStorageBufferReadWriteBinding{
            .buffer = p.seen_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
        auto* const cp = SDL_BeginGPUComputePass(p.cmd, nullptr, 0, &rw_seen, 1);
        SDL_BindGPUComputePipeline(cp, s_seen_walls_pipeline);

        auto const ro_bufs = std::array<
            SDL_GPUBuffer*,
            4>{p.transparency_buf, p.seen_raw_buf, p.vehicle_floor_buf, p.vehicle_obscured_buf};
        SDL_BindGPUComputeStorageBuffers(cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

        SDL_PushGPUComputeUniformData(p.cmd, 0, &seen_push, sizeof(seen_push));
        SDL_DispatchGPUCompute(cp, g_seen, g_seen, static_cast<Uint32>(p.dispatch_z_count));
        SDL_EndGPUComputePass(cp);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto prepare_lighting_transparency_output(prepare_lighting_transparency_output_params const& p)
    -> resident_transparency_output {
    if (p.device == nullptr || p.cache_x <= 0 || p.cache_y <= 0 || p.z_count <= 0) { return {}; }
    auto const z_idx = z_to_resident_index(p.zlev);
    if (z_idx < 0 || z_idx >= p.z_count) { return {}; }
    if (!ensure_lighting_transparency_resource(p)) { return {}; }

    auto const cache_xy = static_cast<uint32_t>(p.cache_x * p.cache_y);
    return resident_transparency_output{
        .buffer = s_lighting_resources.transparency.buffer,
        .output_offset = static_cast<uint32_t>(z_idx) * cache_xy,
    };
}

auto mark_lighting_transparency_level_updated(int const zlev) -> void {
    auto& inputs = s_lighting_resources.inputs;
    auto const idx = z_to_resident_index(zlev);
    if (!resident_index_is_valid(idx, inputs.transparency_valid_levels)
        || !resident_index_is_valid(idx, inputs.transparency_shader_updated_levels)) {
        return;
    }

    inputs.transparency_valid_levels[static_cast<std::size_t>(idx)] = '\1';
    inputs.transparency_shader_updated_levels[static_cast<std::size_t>(idx)] = '\1';
    refresh_transparency_valid_flag();
}

auto lighting_transparency_level_is_valid(int const zlev) -> bool {
    return transparency_level_is_valid(zlev);
}

auto invalidate_lighting_transparency_levels(std::vector<int> const& levels) -> void {
    auto& inputs = s_lighting_resources.inputs;
    for (auto const z : levels) {
        auto const idx = z_to_resident_index(z);
        if (resident_index_is_valid(idx, inputs.transparency_valid_levels)) {
            inputs.transparency_valid_levels[static_cast<std::size_t>(idx)] = '\0';
        }
        if (resident_index_is_valid(idx, inputs.transparency_shader_updated_levels)) {
            inputs.transparency_shader_updated_levels[static_cast<std::size_t>(idx)] = '\0';
        }
    }
    refresh_transparency_valid_flag();
}

auto resident_lighting_ready_for_visibility(resident_lighting_visibility_params const& p) -> bool {
    if (p.device == nullptr || p.cache_x <= 0 || p.cache_y <= 0 || p.z_count <= 0) { return false; }

    ensure_resource_device(p.device);
    reset_input_residency_for_shape(p.cache_x, p.cache_y, p.z_count);

    auto const& inputs = s_lighting_resources.inputs;
    return inputs.cache_x == p.cache_x && inputs.cache_y == p.cache_y && inputs.z_count == p.z_count
        && inputs.transparency_valid && inputs.floor_valid && inputs.vehicle_floor_valid
        && inputs.vehicle_obscured_valid && s_lighting_resources.source_map_valid
        && s_lighting_resources.lighting_outputs_valid;
}

auto shift_lighting_resident_inputs(shift_lighting_residency_params const& p) -> bool {
    ZoneScopedN("gpu_lm_shift_resident_inputs");

    if (p.device == nullptr || p.cache_x <= 0 || p.cache_y <= 0 || p.z_count <= 0) { return false; }

    auto const shift_x_tiles = p.shift_x_submaps * SEEX;
    auto const shift_y_tiles = p.shift_y_submaps * SEEY;
    if (shift_x_tiles == 0 && shift_y_tiles == 0) { return true; }

    ensure_resource_device(p.device);
    reset_input_residency_for_shape(p.cache_x, p.cache_y, p.z_count);

    auto& inputs = s_lighting_resources.inputs;
    auto invalidate_shift_inputs = [&]() {
        std::ranges::fill(inputs.transparency_valid_levels, '\0');
        std::ranges::fill(inputs.transparency_shader_updated_levels, '\0');
        refresh_transparency_valid_flag();
        inputs.floor_valid = false;
        inputs.vehicle_floor_valid = false;
        inputs.vehicle_obscured_valid = false;
    };

    // map::shift changes bubble coordinates.  Derived outputs are invalidated,
    // while structural inputs are shifted in-place so the next lighting build
    // only uploads newly dirty edge bands.
    reset_seen_residency(p.z_count);
    s_lighting_resources.camera_valid = false;
    s_lighting_resources.camera_nonzero_levels = {};
    s_lighting_resources.source_map_valid = false;
    s_lighting_resources.static_lighting_valid = false;
    s_lighting_resources.lighting_outputs_valid = false;

    auto const has_valid_transparency = std::ranges::
        any_of(inputs.transparency_valid_levels, [](char const value) { return value != '\0'; });
    auto const shift_transparency =
        has_valid_transparency && s_lighting_resources.transparency.buffer != nullptr;
    auto const shift_floor = inputs.floor_valid && s_lighting_resources.floor.buffer != nullptr;
    auto const shift_vehicle_floor =
        inputs.vehicle_floor_valid && s_lighting_resources.vehicle_floor.buffer != nullptr;
    auto const shift_vehicle_obscured =
        inputs.vehicle_obscured_valid && s_lighting_resources.vehicle_obscured.buffer != nullptr;

    if (has_valid_transparency && !shift_transparency) {
        std::ranges::fill(inputs.transparency_valid_levels, '\0');
        refresh_transparency_valid_flag();
    }
    if (inputs.floor_valid && !shift_floor) { inputs.floor_valid = false; }
    if (inputs.vehicle_floor_valid && !shift_vehicle_floor) { inputs.vehicle_floor_valid = false; }
    if (inputs.vehicle_obscured_valid && !shift_vehicle_obscured) {
        inputs.vehicle_obscured_valid = false;
    }
    if (!shift_transparency && !shift_floor && !shift_vehicle_floor && !shift_vehicle_obscured) {
        return true;
    }

    if (shift_transparency && !ensure_shift_float_pipeline(p.device)) {
        invalidate_shift_inputs();
        return false;
    }
    if ((shift_floor || shift_vehicle_floor || shift_vehicle_obscured)
        && !ensure_shift_uint_pipeline(p.device)) {
        invalidate_shift_inputs();
        return false;
    }

    auto const cache_xy = p.cache_x * p.cache_y;
    auto const volume_tiles = static_cast<Uint32>(p.z_count * cache_xy);
    auto const transparency_bytes = static_cast<Uint32>(volume_tiles * sizeof(float));
    auto const uint_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    if (shift_transparency && !ensure_float_shift_scratch(p.device, transparency_bytes)) {
        invalidate_shift_inputs();
        return false;
    }
    if (shift_floor
        && !ensure_uint_shift_scratch(
            p.device, s_lighting_resources.shift_floor_scratch, uint_bytes,
            "shift_floor_scratch")) {
        invalidate_shift_inputs();
        return false;
    }
    if (shift_vehicle_floor
        && !ensure_uint_shift_scratch(
            p.device, s_lighting_resources.shift_vehicle_floor_scratch, uint_bytes,
            "shift_vehicle_floor_scratch")) {
        invalidate_shift_inputs();
        return false;
    }
    if (shift_vehicle_obscured
        && !ensure_uint_shift_scratch(
            p.device, s_lighting_resources.shift_vehicle_obscured_scratch, uint_bytes,
            "shift_vehicle_obscured_scratch")) {
        invalidate_shift_inputs();
        return false;
    }

    auto* const cmd = SDL_AcquireGPUCommandBuffer(p.device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: shift command buffer acquisition failed: " << SDL_GetError();
        invalidate_shift_inputs();
        return false;
    }

    {
        ZoneScopedN("gpu_lm_shift_record_commands");
        auto const record_float_shift =
            [&](SDL_GPUBuffer* const src_buf, SDL_GPUBuffer* const dst_buf,
                float const fill_value) {
                auto const rw_shifted = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = dst_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_shifted, 1);
                SDL_BindGPUComputePipeline(cp, s_shift_float_pipeline);
                auto const ro_bufs = std::array<SDL_GPUBuffer*, 1>{src_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));
                auto const push = lm_shift_float_push_constants{
                    .cache_x = p.cache_x,
                    .cache_y = p.cache_y,
                    .cache_xy = cache_xy,
                    .z_count = p.z_count,
                    .shift_x_tiles = shift_x_tiles,
                    .shift_y_tiles = shift_y_tiles,
                    .fill_value = fill_value,
                    ._pad = 0u,
                };
                SDL_PushGPUComputeUniformData(cmd, 0, &push, sizeof(push));
                SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
                SDL_EndGPUComputePass(cp);
            };

        auto const record_uint_shift =
            [&](SDL_GPUBuffer* const src_buf, SDL_GPUBuffer* const dst_buf,
                uint32_t const fill_value) {
                auto const rw_shifted = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = dst_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_shifted, 1);
                SDL_BindGPUComputePipeline(cp, s_shift_uint_pipeline);
                auto const ro_bufs = std::array<SDL_GPUBuffer*, 1>{src_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));
                auto const push = lm_shift_uint_push_constants{
                    .cache_x = p.cache_x,
                    .cache_y = p.cache_y,
                    .cache_xy = cache_xy,
                    .z_count = p.z_count,
                    .shift_x_tiles = shift_x_tiles,
                    .shift_y_tiles = shift_y_tiles,
                    .fill_value = fill_value,
                    ._pad = 0u,
                };
                SDL_PushGPUComputeUniformData(cmd, 0, &push, sizeof(push));
                SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
                SDL_EndGPUComputePass(cp);
            };

        if (shift_transparency) {
            record_float_shift(
                s_lighting_resources.transparency.buffer,
                s_lighting_resources.shift_float_scratch.buffer, LIGHT_TRANSPARENCY_OPEN_AIR);
        }
        if (shift_floor) {
            record_uint_shift(s_lighting_resources.floor.buffer,
                              s_lighting_resources.shift_floor_scratch.buffer, 0u);
        }
        if (shift_vehicle_floor) {
            record_uint_shift(s_lighting_resources.vehicle_floor.buffer,
                              s_lighting_resources.shift_vehicle_floor_scratch.buffer, 0u);
        }
        if (shift_vehicle_obscured) {
            record_uint_shift(s_lighting_resources.vehicle_obscured.buffer,
                              s_lighting_resources.shift_vehicle_obscured_scratch.buffer, 0u);
        }
    }

    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: shift command buffer submission failed: " << SDL_GetError();
        invalidate_shift_inputs();
        return false;
    }
    auto wait_succeeded = true;
    {
        ZoneScopedN("gpu_lm_shift_fence_wait");
        wait_succeeded = SDL_WaitForGPUFences(p.device, true, &fence, 1);
    }
    SDL_ReleaseGPUFence(p.device, fence);
    if (!wait_succeeded) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: lm: shift fence wait failed: " << SDL_GetError();
        invalidate_shift_inputs();
        return false;
    }

    if (shift_transparency) {
        std::swap(s_lighting_resources.transparency, s_lighting_resources.shift_float_scratch);
        clear_transparency_shader_update_marks();
    }
    if (shift_floor) {
        std::swap(s_lighting_resources.floor, s_lighting_resources.shift_floor_scratch);
    }
    if (shift_vehicle_floor) {
        std::swap(s_lighting_resources.vehicle_floor,
                  s_lighting_resources.shift_vehicle_floor_scratch);
    }
    if (shift_vehicle_obscured) {
        std::swap(s_lighting_resources.vehicle_obscured,
                  s_lighting_resources.shift_vehicle_obscured_scratch);
    }
    TracyPlot("GPU LM Shift Transparency", shift_transparency ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Shift Floor", shift_floor ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Shift Vehicle Floor", shift_vehicle_floor ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Shift Vehicle Obscured", shift_vehicle_obscured ? int64_t{1} : int64_t{0});
    return true;
}

auto compute_light_radius(float const luminance) -> float {
    if (luminance <= LIGHT_AMBIENT_LOW) { return 0.0f; }

    auto radius = 0;
    for (auto const distance : std::views::iota(1, g_max_view_distance + 1)) {
        auto const intensity =
            luminance
            / (std::exp(LIGHT_TRANSPARENCY_OPEN_AIR * static_cast<float>(distance))
               * static_cast<float>(distance));
        if (intensity <= LIGHT_AMBIENT_LOW) { break; }
        radius = distance;
    }
    return static_cast<float>(radius);
}

auto daylight_diffusion_decay_for_pass(int const pass) -> float {
    auto const air_decay = std::exp(-LIGHT_TRANSPARENCY_OPEN_AIR);
    if (pass == 0) { return air_decay; }
    auto const distance = static_cast<float>(pass + 1);
    return air_decay * (distance - 1.0f) / distance;
}

auto begin_gpu_lighting(SDL_GPUDevice* const device, run_gpu_lighting_params const& p)
    -> gpu_lighting_work {
    ZoneScopedN("begin_gpu_lighting");

    if (p.m == nullptr || p.dirty_levels == nullptr) { return {}; }
    if (s_pending_lighting_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: lighting begin requested while previous "
               "lighting work is pending";
        return {};
    }
    if (s_pending_visibility_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: lighting begin requested while visibility "
               "work is pending";
        return {};
    }
    if (s_pending_sight_pairs_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: lighting begin requested while sight-pair "
               "work is pending";
        return {};
    }
    ensure_resource_device(device);
    if (!ensure_pipelines(device)) { return {}; }

    auto lightmap_levels = sorted_unique(*p.dirty_levels);
    if (lightmap_levels.empty() && !p.rebuild_seen_cache && !p.download_seen_cache
        && s_lighting_resources.seen_valid) {
        return {};
    }

    auto const& lc0 = p.m->get_cache_ref(p.player_zlev);
    auto const cache_x = lc0.cache_x;
    auto const cache_y = lc0.cache_y;
    auto const cache_xy = cache_x * cache_y;
    auto const z_count = OVERMAP_LAYERS;
    auto all_levels = make_all_levels(z_count);
    auto const backend_policy = backend_policy_for_device(device);
    auto const has_resident_resources =
        s_lighting_resources.device == device
        && s_lighting_resources.transparency.buffer != nullptr;
    if (backend_policy.reset_lighting_resources_on_rebuild && !lightmap_levels.empty()
        && has_resident_resources) {
        if (!SDL_WaitForGPUIdle(device)) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: DXBC wait for idle before lighting "
                   "resource reset failed: "
                << SDL_GetError();
            return {};
        }
        release_lighting_resources(device);
    }

    // ── Collect light sources ────────────────────────────────────────────────
    auto all_sources = std::vector<GpuLightSource>{};
    auto static_sources = std::vector<GpuLightSource>{};
    auto dynamic_sources = std::vector<GpuLightSource>{};
    auto colored_sources = std::vector<GpuColoredLightSource>{};
    auto source_stats = source_collection_stats{};
    auto const rewrites_full_lighting_volume = !lightmap_levels.empty();
    auto const requested_selected_lightmap_levels =
        !lightmap_levels.empty() && lightmap_levels != all_levels;
    auto const& source_collection_levels =
        rewrites_full_lighting_volume ? all_levels : lightmap_levels;
    auto const collect_colored_sources = colored_lighting && !source_collection_levels.empty();
    if (!source_collection_levels.empty()) {
        ZoneScopedN("gpu_lm_collect_sources");
        auto collection = collect_sources(*p.m, source_collection_levels, collect_colored_sources);
        source_stats = collection.stats;
        all_sources = std::move(collection.sources);
        static_sources = std::move(collection.static_sources);
        dynamic_sources = std::move(collection.dynamic_sources);
        colored_sources = std::move(collection.colored_sources);
        write_source_map_to_level_caches(*p.m, source_collection_levels, all_sources);
        if (collect_colored_sources && colored_sources.empty()) {
            clear_colored_light_caches(*p.m, source_collection_levels);
        }
    }

    // ── Pack CPU input buffers into compact upload slices ────────────────────
    // Full-volume uploads are still used when residency is invalid or precise
    // dirty levels are unavailable.  Otherwise each packed level is uploaded
    // into its resident 3D-buffer slice.
    reset_input_residency_for_shape(cache_x, cache_y, z_count);
    auto input_uploads = make_input_upload_plan(p, all_levels);
    clear_transparency_shader_update_marks();
    if (backend_policy.expand_dirty_transparency_uploads
        && !input_uploads.transparency_levels.empty()
        && input_uploads.transparency_levels != all_levels) {
        input_uploads.transparency_levels = all_levels;
    }
    if (!s_lighting_resources.source_map_valid && lightmap_levels.empty()) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: seen-only rebuild requested before resident "
               "source-map is valid";
        return {};
    }
    auto source_map_upload_levels =
        rewrites_full_lighting_volume || !s_lighting_resources.source_map_valid
            ? all_levels
            : lightmap_levels;
    std::ranges::sort(source_map_upload_levels);
    source_map_upload_levels.erase(
        std::ranges::unique(source_map_upload_levels).begin(), source_map_upload_levels.end());

    auto& transparency_cpu = s_lighting_resources.transparency_staging;
    auto& floor_cpu = s_lighting_resources.floor_staging;
    auto& vehicle_floor_cpu = s_lighting_resources.vehicle_floor_staging;
    auto& vehicle_obscured_cpu = s_lighting_resources.vehicle_obscured_staging;
    auto& source_map_cpu = s_lighting_resources.source_map_staging;

    if (has_structural_upload(input_uploads)) {
        ZoneScopedN("gpu_lm_pack_inputs");
        if (!input_uploads.transparency_levels.empty()) {
            pack_float_cache(
                *p.m, input_uploads.transparency_levels, cache_xy,
                [](level_cache const& lc) -> std::vector<float> const& {
                    return lc.transparency_cache;
                },
                transparency_cpu);
        }
        if (!input_uploads.floor_levels.empty()) {
            pack_char_cache_uint(
                *p.m, input_uploads.floor_levels, cache_xy,
                [](level_cache const& lc) -> std::vector<char> const& { return lc.floor_cache; },
                floor_cpu);
        }
        if (!input_uploads.vehicle_floor_levels.empty()) {
            pack_char_cache_uint(
                *p.m, input_uploads.vehicle_floor_levels, cache_xy,
                [](level_cache const& lc) -> std::vector<char> const& {
                    return lc.vehicle_floor_cache;
                },
                vehicle_floor_cpu);
        }
        if (!input_uploads.vehicle_obscured_levels.empty()) {
            pack_vehicle_obscured_cache_uint(
                *p.m, input_uploads.vehicle_obscured_levels, cache_xy, vehicle_obscured_cpu);
        }
    }
    {
        ZoneScopedN("gpu_lm_pack_source_map");
        pack_float_cache(
            *p.m, source_map_upload_levels, cache_xy,
            [](level_cache const& lc) -> std::vector<float> const& { return lc.sm; },
            source_map_cpu);
    }

    // ── Compute ambient constants per z-level ────────────────────────────────
    auto ambient_push = lm_ambient_push_constants{};
    {
        ambient_push.inside_light = LIGHT_AMBIENT_LOW;
        ambient_push.cache_x = cache_x;
        ambient_push.cache_y = cache_y;
        ambient_push.cache_xy = cache_xy;
        ambient_push.z_count = z_count;
        ambient_push.overmap_depth = OVERMAP_DEPTH;
        ambient_push.angled_sunlight_shadows = p.angled_sunlight_shadows ? 1u : 0u;
        ambient_push.direct_sunlight = p.direct_sunlight ? 1u : 0u;
        ambient_push.sun_dx_per_z = p.sun_dx_per_z;
        ambient_push.sun_dy_per_z = p.sun_dy_per_z;
        auto const max_shadow_light =
            static_cast<float>(default_daylight_level()) * solar_shadow_scatter;
        ambient_push.solar_shadow_light = std::
            max(static_cast<float>(LIGHT_AMBIENT_LOW),
                std::min(g->natural_light_level(0), max_shadow_light));
        for (auto const zi : std::views::iota(0, OVERMAP_LAYERS)) {
            ambient_push.natural_light[zi / 4][zi % 4] = g->natural_light_level(zi - OVERMAP_DEPTH);
        }
    }
    auto const current_ambient_signature = ambient_signature(ambient_push);
    auto const ambient_changed =
        !s_lighting_resources.static_ambient_signature_valid
        || s_lighting_resources.static_ambient_signature != current_ambient_signature;
    auto const structural_upload = has_structural_upload(input_uploads);
    auto structural_upload_levels = std::vector<int>{};
    if (structural_upload) {
        std::ranges::
            copy(input_uploads.transparency_levels, std::back_inserter(structural_upload_levels));
        std::ranges::copy(input_uploads.floor_levels, std::back_inserter(structural_upload_levels));
        std::ranges::
            copy(input_uploads.vehicle_floor_levels, std::back_inserter(structural_upload_levels));
        std::ranges::copy(
            input_uploads.vehicle_obscured_levels, std::back_inserter(structural_upload_levels));
        structural_upload_levels = sorted_unique(std::move(structural_upload_levels));
    }
    auto checked_structural_signatures = std::vector<std::pair<int, std::size_t>>{};
    auto structural_changed = false;
    if (!lightmap_levels.empty() && s_lighting_resources.static_lighting_valid
        && !structural_upload_levels.empty()) {
        ZoneScopedN("gpu_lm_structural_signature");
        checked_structural_signatures.reserve(structural_upload_levels.size());
        for (auto const z : structural_upload_levels) {
            auto const signature = structural_level_signature(*p.m, z, cache_x, cache_y);
            checked_structural_signatures.emplace_back(z, signature);
            auto const zi = z_to_resident_index(z);
            auto const valid_idx = resident_index_is_valid(
                zi, s_lighting_resources.static_structural_level_signature_valid);
            if (!valid_idx
                || s_lighting_resources.static_structural_level_signature_valid[static_cast<
                       std::size_t>(zi)]
                       == '\0'
                || s_lighting_resources.static_structural_level_signatures[static_cast<std::size_t>(
                       zi)]
                       != signature) {
                structural_changed = true;
            }
        }
    }
    auto const current_static_source_signature = source_signature(static_sources);
    auto const static_sources_changed =
        !lightmap_levels.empty()
        && (!s_lighting_resources.static_source_signature_valid
            || s_lighting_resources.static_source_signature != current_static_source_signature);
    auto const rebuild_static_lighting =
        !lightmap_levels.empty()
        && (!s_lighting_resources.static_lighting_valid || structural_changed
            || static_sources_changed);

    auto static_raytrace_buckets = std::vector<raytrace_source_bucket>{};
    auto dynamic_raytrace_buckets = std::vector<raytrace_source_bucket>{};
    auto trace_sources = std::vector<GpuLightSource>{};
    if (rebuild_static_lighting) {
        static_raytrace_buckets = make_raytrace_buckets(static_sources);
        trace_sources = static_sources;
    }
    auto const dynamic_source_offset = static_cast<Uint32>(trace_sources.size());
    if (!dynamic_sources.empty()) {
        dynamic_raytrace_buckets = make_raytrace_buckets(dynamic_sources, dynamic_source_offset);
        std::ranges::move(dynamic_sources, std::back_inserter(trace_sources));
    }
    auto const raytrace_buckets = [&]() {
        auto buckets = static_raytrace_buckets;
        std::ranges::copy(dynamic_raytrace_buckets, std::back_inserter(buckets));
        return buckets;
    }();
    auto const num_src = static_cast<Uint32>(trace_sources.size());
    auto const all_num_src = static_cast<Uint32>(all_sources.size());
    auto colored_raytrace_buckets = make_raytrace_buckets(colored_sources);
    auto const num_colored_src = static_cast<Uint32>(colored_sources.size());

    auto max_radius = 0;
    for (auto const& src : trace_sources) {
        max_radius = std::max(max_radius, static_cast<int>(std::ceil(src.radius)));
    }
    auto const raytrace_work = raytrace_work_units(raytrace_buckets);
    auto const raytrace_z_work = raytrace_z_work_units(trace_sources);
    auto const colored_raytrace_work = raytrace_work_units(colored_raytrace_buckets);
    auto const colored_raytrace_z_work = raytrace_z_work_units(colored_sources);
    auto const legacy_raytrace_groups = raytrace_groups_for_radius(max_radius);
    auto const legacy_raytrace_work =
        static_cast<int64_t>(num_src) * static_cast<int64_t>(legacy_raytrace_groups)
        * static_cast<int64_t>(legacy_raytrace_groups);
    auto const legacy_raytrace_z_work = legacy_raytrace_work * static_cast<int64_t>(z_count);

    // ── Ensure GPU buffers ───────────────────────────────────────────────────
    auto const volume_tiles = static_cast<Uint32>(z_count * cache_xy);
    auto const t_bytes = static_cast<Uint32>(volume_tiles * sizeof(float));
    auto const f_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const vf_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const vo_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const camera_bytes = t_bytes;
    auto const source_map_bytes = t_bytes;
    // lm and seen buffers cover all z_count * cache_xy tiles.
    auto const out_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const float_level_bytes = static_cast<Uint32>(cache_xy * sizeof(float));
    auto const uint_level_bytes = static_cast<Uint32>(cache_xy * sizeof(uint32_t));
    auto const lm_level_bytes = static_cast<Uint32>(cache_xy * sizeof(uint32_t));
    auto const full_lm_download_bytes =
        static_cast<Uint32>(lightmap_levels.size()) * lm_level_bytes;
    auto const download_lm_levels = p.download_lightmap && !lightmap_levels.empty();
    auto const download_player_light =
        !download_lm_levels && !lightmap_levels.empty()
        && std::ranges::find(lightmap_levels, p.player_zlev) != lightmap_levels.end()
        && p.player_zlev >= -OVERMAP_DEPTH && p.player_zlev <= OVERMAP_HEIGHT && p.player_x >= 0
        && p.player_x < cache_x && p.player_y >= 0 && p.player_y < cache_y;
    auto const lm_download_bytes =
        (download_lm_levels ? full_lm_download_bytes : Uint32{0})
        + (download_player_light ? static_cast<Uint32>(sizeof(uint32_t)) : Uint32{0});
    auto const source_upload_bytes =
        num_src > 0 ? static_cast<Uint32>(num_src * sizeof(GpuLightSource)) : Uint32{0};
    auto const colored_source_upload_bytes =
        num_colored_src > 0
            ? static_cast<Uint32>(num_colored_src * sizeof(GpuColoredLightSource))
            : Uint32{0};
    auto const source_buffer_bytes =
        std::max(source_upload_bytes, static_cast<Uint32>(sizeof(GpuLightSource)));
    auto const colored_source_buffer_bytes =
        std::max(colored_source_upload_bytes, static_cast<Uint32>(sizeof(GpuColoredLightSource)));

    auto const seen_was_valid = s_lighting_resources.seen_valid;
    auto const rebuild_seen = p.rebuild_seen_cache || (p.download_seen_cache && !seen_was_valid);
    auto seen_download_levels = std::vector<int>{};
    if (p.download_seen_cache) {
        seen_download_levels =
            rebuild_seen ? make_seen_download_levels(p, seen_was_valid, all_levels) : all_levels;
    }
    auto const transparency_upload_bytes =
        static_cast<Uint32>(input_uploads.transparency_levels.size()) * float_level_bytes;
    auto const floor_upload_bytes =
        static_cast<Uint32>(input_uploads.floor_levels.size()) * uint_level_bytes;
    auto const vehicle_floor_upload_bytes =
        static_cast<Uint32>(input_uploads.vehicle_floor_levels.size()) * uint_level_bytes;
    auto const vehicle_obscured_upload_bytes =
        static_cast<Uint32>(input_uploads.vehicle_obscured_levels.size()) * uint_level_bytes;
    auto const source_map_upload_bytes =
        static_cast<Uint32>(source_map_upload_levels.size()) * float_level_bytes;
    auto const download_colored_light = collect_colored_sources && !colored_sources.empty();
    auto colored_light_download_levels = download_colored_light ? all_levels : std::vector<int>{};
    auto const colored_light_download_bytes =
        static_cast<Uint32>(colored_light_download_levels.size()) * uint_level_bytes;
    auto const needs_download_copy =
        download_lm_levels || download_player_light || download_colored_light
        || !seen_download_levels.empty();
    auto const deferred_download_copy =
        backend_policy.serialize_lighting_downloads && needs_download_copy;
    auto const serialized_readback_chunk_bytes =
        backend_policy.serialized_readback_chunk_bytes == 0
            ? lm_level_bytes
            : std::min(backend_policy.serialized_readback_chunk_bytes, lm_level_bytes);
    auto const serialized_uint_level_download_bytes =
        deferred_download_copy ? serialized_readback_chunk_bytes : uint_level_bytes;
    auto const serialized_float_level_download_bytes =
        deferred_download_copy
            ? std::min(serialized_readback_chunk_bytes, float_level_bytes)
            : float_level_bytes;
    auto const lm_download_resource_bytes =
        deferred_download_copy
            ? std::max(serialized_readback_chunk_bytes, static_cast<Uint32>(sizeof(uint32_t)))
            : std::max(lm_download_bytes, Uint32{1});
    auto const seen_download_resource_bytes =
        deferred_download_copy
            ? std::max(serialized_float_level_download_bytes, static_cast<Uint32>(sizeof(float)))
            : std::max(static_cast<Uint32>(seen_download_levels.size()) * float_level_bytes,
                       Uint32{1});
    auto upload_total = source_upload_bytes;
    upload_total += colored_source_upload_bytes;
    upload_total += transparency_upload_bytes;
    upload_total += floor_upload_bytes;
    upload_total += vehicle_floor_upload_bytes;
    upload_total += vehicle_obscured_upload_bytes;
    upload_total += source_map_upload_bytes;
    auto const upload_buffer_bytes = std::max(upload_total, Uint32{1});
    auto const visibility_upload_total = camera_bytes;

    TracyPlot("GPU LM Dirty Levels", static_cast<int64_t>(lightmap_levels.size()));
    TracyPlot("GPU LM DXBC Stage Submit",
              backend_policy.submit_lighting_stages ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM DXBC Stage Wait",
              backend_policy.wait_lighting_stages ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Serialized Chunk Bytes",
              static_cast<int64_t>(serialized_readback_chunk_bytes));
    TracyPlot(
        "GPU LM Serialized Batch Slots",
        deferred_download_copy
            ? static_cast<int64_t>(dxbc_serialized_readback_batch_slots)
            : int64_t{0});
    TracyPlot("GPU LM Rewrite Full Volume", rewrites_full_lighting_volume ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Requested Selected Rewrite",
              requested_selected_lightmap_levels ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Rebuild Seen", rebuild_seen ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Sources", static_cast<int64_t>(all_num_src));
    TracyPlot("GPU LM Trace Sources", static_cast<int64_t>(num_src));
    TracyPlot("GPU LM Colored Sources", static_cast<int64_t>(num_colored_src));
    TracyPlot("GPU LM Static Rebuild", rebuild_static_lighting ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Ambient Changed", ambient_changed ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Static Sources Changed", static_sources_changed ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Structural Upload", structural_upload ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Structural Signature Checked",
              checked_structural_signatures.empty() ? int64_t{0} : int64_t{1});
    TracyPlot("GPU LM Structural Signature Levels",
              static_cast<int64_t>(checked_structural_signatures.size()));
    TracyPlot("GPU LM Structural Changed", structural_changed ? int64_t{1} : int64_t{0});
    TracyPlot("GPU LM Sources Static", static_cast<int64_t>(source_stats.static_sources));
    TracyPlot("GPU LM Sources Static Local",
              static_cast<int64_t>(source_stats.static_local_sources));
    TracyPlot("GPU LM Sources Field", static_cast<int64_t>(source_stats.field_sources));
    TracyPlot("GPU LM Sources Active Item", static_cast<int64_t>(source_stats.active_item_sources));
    TracyPlot("GPU LM Sources Vehicle", static_cast<int64_t>(source_stats.vehicle_sources));
    TracyPlot("GPU LM Sources Character", static_cast<int64_t>(source_stats.character_sources));
    TracyPlot("GPU LM Sources Monster", static_cast<int64_t>(source_stats.monster_sources));
    TracyPlot("GPU LM Max Radius", static_cast<int64_t>(max_radius));
    TracyPlot("GPU LM Raytrace Buckets", static_cast<int64_t>(raytrace_buckets.size()));
    TracyPlot("GPU LM Raytrace Work", raytrace_work);
    TracyPlot("GPU LM Raytrace Work Legacy", legacy_raytrace_work);
    TracyPlot("GPU LM Raytrace Z Work", raytrace_z_work);
    TracyPlot("GPU LM Raytrace Z Work Legacy", legacy_raytrace_z_work);
    TracyPlot("GPU LM Colored Raytrace Work", colored_raytrace_work);
    TracyPlot("GPU LM Colored Raytrace Z Work", colored_raytrace_z_work);
    TracyPlot("GPU LM Source Map Levels", static_cast<int64_t>(source_map_upload_levels.size()));
    TracyPlot("GPU LM Download Levels",
              download_lm_levels ? static_cast<int64_t>(lightmap_levels.size()) : int64_t{0});
    TracyPlot("GPU LM Download Player Light", download_player_light ? int64_t{1} : int64_t{0});
    TracyPlot(
        "GPU LM Colored Download Levels",
        download_colored_light
            ? static_cast<int64_t>(colored_light_download_levels.size())
            : int64_t{0});
    TracyPlot(
        "GPU LM Input Upload Levels",
        static_cast<int64_t>(
            input_uploads.transparency_levels.size() + input_uploads.floor_levels.size()
            + input_uploads.vehicle_floor_levels.size()
            + input_uploads.vehicle_obscured_levels.size()));
    TracyPlot("GPU LM Transparency Upload Levels",
              static_cast<int64_t>(input_uploads.transparency_levels.size()));
    TracyPlot("GPU LM Floor Upload Levels", static_cast<int64_t>(input_uploads.floor_levels.size()));
    TracyPlot("GPU LM Vehicle Floor Upload Levels",
              static_cast<int64_t>(input_uploads.vehicle_floor_levels.size()));
    TracyPlot("GPU LM Vehicle Obscured Upload Levels",
              static_cast<int64_t>(input_uploads.vehicle_obscured_levels.size()));
    TracyPlot("GPU LM Upload KiB", static_cast<int64_t>(upload_total / 1024u));

    {
        ZoneScopedN("gpu_lm_ensure_resources");
        if (!ensure_lighting_resources(
                device,
                {
                    .transparency_bytes = t_bytes,
                    .floor_bytes = f_bytes,
                    .vehicle_floor_bytes = vf_bytes,
                    .vehicle_obscured_bytes = vo_bytes,
                    .camera_bytes = camera_bytes,
                    .vehicle_optics_bytes = static_cast<Uint32>(sizeof(GpuVehicleOptic)),
                    .source_map_bytes = source_map_bytes,
                    .source_bytes = source_buffer_bytes,
                    .daylight_bytes = out_bytes,
                    .static_lm_bytes = out_bytes,
                    .output_bytes = out_bytes,
                    .lm_download_bytes = lm_download_resource_bytes,
                    .seen_download_bytes = seen_download_resource_bytes,
                    .visibility_download_bytes = out_bytes,
                    .upload_bytes = upload_buffer_bytes,
                    .visibility_upload_bytes = visibility_upload_total,
                })) {
            return {};
        }
    }
    if (download_colored_light) {
        ZoneScopedN("gpu_lm_ensure_colored_resources");
        if (!ensure_color_raytrace_pipeline(device)
            || !ensure_colored_lighting_resources(
                device,
                {
                    .source_bytes = colored_source_buffer_bytes,
                    .output_bytes = out_bytes,
                    .download_bytes =
                        deferred_download_copy
                            ? std::max(serialized_uint_level_download_bytes,
                                       static_cast<Uint32>(sizeof(uint32_t)))
                            : std::max(colored_light_download_bytes, Uint32{1}),
                })) {
            return {};
        }
    }
    if (!lightmap_levels.empty()
        && (!ensure_fill_uint_pipeline(device) || !ensure_max_uint_pipeline(device))) {
        return {};
    }

    auto* const t_buf = s_lighting_resources.transparency.buffer;
    auto* const f_buf = s_lighting_resources.floor.buffer;
    auto* const vf_buf = s_lighting_resources.vehicle_floor.buffer;
    auto* const vo_buf = s_lighting_resources.vehicle_obscured.buffer;
    auto* const source_map_buf = s_lighting_resources.source_map.buffer;
    auto* const src_buf = s_lighting_resources.sources.buffer;
    auto* const colored_src_buf = s_lighting_resources.colored_sources.buffer;
    auto* const daylight_seed_buf = s_lighting_resources.daylight_seed.buffer;
    auto* const daylight_diffuse_a_buf = s_lighting_resources.daylight_diffuse_a.buffer;
    auto* const daylight_diffuse_b_buf = s_lighting_resources.daylight_diffuse_b.buffer;
    auto* const static_lm_buf = s_lighting_resources.static_lm.buffer;
    auto* const lm_buf = s_lighting_resources.lm.buffer;
    auto* const colored_light_buf = s_lighting_resources.colored_light.buffer;
    auto* const seen_raw_buf = s_lighting_resources.seen_raw.buffer;
    auto* const seen_buf = s_lighting_resources.seen.buffer;
    auto* const upload_tbuf = s_lighting_resources.upload.buffer;
    auto* const lm_dl_tbuf = s_lighting_resources.lm_download.buffer;
    auto* const colored_light_dl_tbuf = s_lighting_resources.colored_light_download.buffer;
    auto* const seen_dl_tbuf = s_lighting_resources.seen_download.buffer;

    // ── Upload: single transfer buffer covering all inputs ───────────────────
    if (upload_total > 0) {
        ZoneScopedN("gpu_lm_stage_upload");
        auto* const mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, upload_tbuf, backend_policy.cycle_upload_transfers));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: upload transfer buffer map failed: " << SDL_GetError();
            return {};
        }
        auto off = Uint32{0};
        if (transparency_upload_bytes > 0) {
            std::memcpy(mapped + off, transparency_cpu.data(), transparency_upload_bytes);
            off += transparency_upload_bytes;
        }
        if (floor_upload_bytes > 0) {
            std::memcpy(mapped + off, floor_cpu.data(), floor_upload_bytes);
            off += floor_upload_bytes;
        }
        if (vehicle_floor_upload_bytes > 0) {
            std::memcpy(mapped + off, vehicle_floor_cpu.data(), vehicle_floor_upload_bytes);
            off += vehicle_floor_upload_bytes;
        }
        if (vehicle_obscured_upload_bytes > 0) {
            std::memcpy(mapped + off, vehicle_obscured_cpu.data(), vehicle_obscured_upload_bytes);
            off += vehicle_obscured_upload_bytes;
        }
        if (source_map_upload_bytes > 0) {
            std::memcpy(mapped + off, source_map_cpu.data(), source_map_upload_bytes);
            off += source_map_upload_bytes;
        }
        if (source_upload_bytes > 0) {
            std::memcpy(mapped + off, trace_sources.data(), source_upload_bytes);
            off += source_upload_bytes;
        }
        if (colored_source_upload_bytes > 0) {
            std::memcpy(mapped + off, colored_sources.data(), colored_source_upload_bytes);
        }
        SDL_UnmapGPUTransferBuffer(device, upload_tbuf);
    }

    // ── Build command buffer ─────────────────────────────────────────────────
    s_lighting_resources.lighting_outputs_valid = false;
    auto* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: command buffer acquisition failed: " << SDL_GetError();
        return {};
    }

    auto submit_lighting_stage = [&](std::string_view const label, bool const reacquire) -> bool {
        if (!backend_policy.submit_lighting_stages) { return true; }
        if (cmd == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: lighting stage submission requested without a command buffer "
                   "after "
                << label;
            return false;
        }

        if (backend_policy.wait_lighting_stages) {
            auto* stage_fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
            if (stage_fence == nullptr) {
                DebugLog(DL::Error, DC::Main)
                    << "SDL_GPU: lm: lighting stage command buffer submission failed after "
                    << label << ": " << SDL_GetError();
                cmd = nullptr;
                return false;
            }

            auto const wait_succeeded = SDL_WaitForGPUFences(device, true, &stage_fence, 1);
            SDL_ReleaseGPUFence(device, stage_fence);
            if (!wait_succeeded) {
                DebugLog(DL::Error, DC::Main)
                    << "SDL_GPU: lm: lighting stage fence wait failed after " << label << ": "
                    << SDL_GetError();
                cmd = nullptr;
                return false;
            }
        } else if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: lighting stage command buffer submission failed after " << label
                << ": " << SDL_GetError();
            cmd = nullptr;
            return false;
        }

        if (!reacquire) {
            cmd = nullptr;
            return true;
        }

        cmd = SDL_AcquireGPUCommandBuffer(device);
        if (cmd == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: lighting stage command buffer reacquire failed after " << label
                << ": " << SDL_GetError();
            return false;
        }
        return true;
    };

    {
        ZoneScopedN("gpu_lm_record_commands");

        // [Pass 1] Copy: upload all input buffers.
        if (upload_total > 0) {
            auto off = Uint32{0};
            auto upload_copy_commands = Uint32{0};

            struct upload_level_ranges_params {
                SDL_GPUCopyPass* cp;
                SDL_GPUBuffer* dst;
                std::vector<int> const* levels;
                Uint32 level_bytes;
            };

            auto make_upload_location = [&](Uint32 const offset) {
                return SDL_GPUTransferBufferLocation{
                    .transfer_buffer = upload_tbuf,
                    .offset = offset,
                };
            };

            auto upload_whole_region =
                [&](SDL_GPUCopyPass* const cp, SDL_GPUBuffer* const dst, Uint32 const bytes,
                    bool const cycle) {
                    auto const src_loc = make_upload_location(off);
                    auto const dst_reg = SDL_GPUBufferRegion{
                        .buffer = dst,
                        .offset = 0,
                        .size = bytes,
                    };
                    SDL_UploadToGPUBuffer(cp, &src_loc, &dst_reg, cycle);
                    off += bytes;
                    ++upload_copy_commands;
                };

            auto upload_level_ranges = [&](upload_level_ranges_params const& p, bool const cycle) {
                auto packed_level_index = Uint32{0};
                for (auto const& range : make_z_level_ranges(*p.levels)) {
                    auto const range_bytes = static_cast<Uint32>(range.z_count) * p.level_bytes;
                    auto const src_loc = make_upload_location(
                        off + packed_level_index * p.level_bytes);
                    auto const dst_reg = SDL_GPUBufferRegion{
                        .buffer = p.dst,
                        .offset =
                            static_cast<Uint32>(range.z_start + OVERMAP_DEPTH) * p.level_bytes,
                        .size = range_bytes,
                    };
                    SDL_UploadToGPUBuffer(p.cp, &src_loc, &dst_reg, cycle);
                    packed_level_index += static_cast<Uint32>(range.z_count);
                    ++upload_copy_commands;
                }
                off += static_cast<Uint32>(p.levels->size()) * p.level_bytes;
            };

            auto const should_cycle_upload = [&](std::vector<int> const& levels) {
                return backend_policy.cycle_upload_transfers && levels == all_levels;
            };

            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            upload_level_ranges(
                {
                    .cp = cp,
                    .dst = t_buf,
                    .levels = &input_uploads.transparency_levels,
                    .level_bytes = float_level_bytes,
                },
                should_cycle_upload(input_uploads.transparency_levels));
            upload_level_ranges(
                {
                    .cp = cp,
                    .dst = f_buf,
                    .levels = &input_uploads.floor_levels,
                    .level_bytes = uint_level_bytes,
                },
                should_cycle_upload(input_uploads.floor_levels));
            upload_level_ranges(
                {
                    .cp = cp,
                    .dst = vf_buf,
                    .levels = &input_uploads.vehicle_floor_levels,
                    .level_bytes = uint_level_bytes,
                },
                should_cycle_upload(input_uploads.vehicle_floor_levels));
            upload_level_ranges(
                {
                    .cp = cp,
                    .dst = vo_buf,
                    .levels = &input_uploads.vehicle_obscured_levels,
                    .level_bytes = uint_level_bytes,
                },
                should_cycle_upload(input_uploads.vehicle_obscured_levels));
            upload_level_ranges(
                {
                    .cp = cp,
                    .dst = source_map_buf,
                    .levels = &source_map_upload_levels,
                    .level_bytes = float_level_bytes,
                },
                should_cycle_upload(source_map_upload_levels));
            if (source_upload_bytes > 0) {
                upload_whole_region(
                    cp, src_buf, source_upload_bytes, backend_policy.cycle_upload_transfers);
            }
            if (colored_source_upload_bytes > 0) {
                upload_whole_region(
                    cp, colored_src_buf, colored_source_upload_bytes,
                    backend_policy.cycle_upload_transfers);
            }
            SDL_EndGPUCopyPass(cp);
            TracyPlot("GPU LM Upload Copy Commands", static_cast<int64_t>(upload_copy_commands));
            if (!submit_lighting_stage("input upload", true)) { return {}; }
        }

        if (!lightmap_levels.empty()) {
            auto dispatch_raytrace =
                [&](SDL_GPUBuffer* const target_buf,
                    std::vector<raytrace_source_bucket> const& buckets) -> bool {
                if (buckets.empty()) { return true; }

                auto const rw_lm = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = target_buf,
                    .cycle = false,
                    .padding1 = 0,
                    .padding2 = 0,
                    .padding3 = 0,
                };
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_lm, 1);
                SDL_BindGPUComputePipeline(cp, s_raytrace_pipeline);

                auto const ro_bufs = std::array<SDL_GPUBuffer*, 4>{t_buf, f_buf, vf_buf, src_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

                for (auto const& bucket : buckets) {
                    auto const raytrace_push = lm_raytrace_push_constants{
                        .cache_x = cache_x,
                        .cache_y = cache_y,
                        .cache_xy = cache_xy,
                        .z_count = z_count,
                        .z_scale = Z_LEVEL_SCALE,
                        .num_sources = bucket.source_count,
                        .max_radius = bucket.max_radius,
                        .source_offset = bucket.source_offset,
                    };
                    SDL_PushGPUComputeUniformData(cmd, 0, &raytrace_push, sizeof(raytrace_push));
                    SDL_DispatchGPUCompute(
                        cp, bucket.source_count, bucket.groups_xy, bucket.groups_xy);
                }
                SDL_EndGPUComputePass(cp);
                return true;
            };

            auto dispatch_color_raytrace =
                [&](std::vector<raytrace_source_bucket> const& buckets) -> bool {
                if (buckets.empty()) { return true; }

                auto const rw_color = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = colored_light_buf,
                    .cycle = false,
                    .padding1 = 0,
                    .padding2 = 0,
                    .padding3 = 0,
                };
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_color, 1);
                SDL_BindGPUComputePipeline(cp, s_color_raytrace_pipeline);

                auto const ro_bufs =
                    std::array<SDL_GPUBuffer*, 4>{t_buf, f_buf, vf_buf, colored_src_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

                for (auto const& bucket : buckets) {
                    auto const raytrace_push = lm_raytrace_push_constants{
                        .cache_x = cache_x,
                        .cache_y = cache_y,
                        .cache_xy = cache_xy,
                        .z_count = z_count,
                        .z_scale = Z_LEVEL_SCALE,
                        .num_sources = bucket.source_count,
                        .max_radius = bucket.max_radius,
                        .source_offset = bucket.source_offset,
                    };
                    SDL_PushGPUComputeUniformData(cmd, 0, &raytrace_push, sizeof(raytrace_push));
                    SDL_DispatchGPUCompute(
                        cp, bucket.source_count, bucket.groups_xy, bucket.groups_xy);
                }
                SDL_EndGPUComputePass(cp);
                return true;
            };

            auto fill_uint_buffer = [&](SDL_GPUBuffer* const target_buf, uint32_t const value) {
                auto const rw_target = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = target_buf,
                    .cycle = false,
                    .padding1 = 0,
                    .padding2 = 0,
                    .padding3 = 0};
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_target, 1);
                SDL_BindGPUComputePipeline(cp, s_fill_uint_pipeline);
                auto const fill_push = lm_fill_uint_push_constants{
                    .total_tiles = volume_tiles,
                    .value = value,
                    ._pad = {},
                };
                SDL_PushGPUComputeUniformData(cmd, 0, &fill_push, sizeof(fill_push));
                SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
                SDL_EndGPUComputePass(cp);
            };

            if (rebuild_static_lighting) {
                // [Pass 2] Compute: static terrain/furniture source contribution.
                fill_uint_buffer(static_lm_buf, 0u);
                if (!submit_lighting_stage("static lm clear", true)) { return {}; }
                if (!dispatch_raytrace(static_lm_buf, static_raytrace_buckets)) { return {}; }
                if (!static_raytrace_buckets.empty()
                    && !submit_lighting_stage("static raytrace", true)) {
                    return {};
                }
            }

            // [Pass 3] Compute: ambient/weather/sun base for this frame.
            {
                auto const rw_bufs = std::array<SDL_GPUStorageBufferReadWriteBinding, 2>{
                    SDL_GPUStorageBufferReadWriteBinding{
                        .buffer = lm_buf,
                        .cycle = false,
                        .padding1 = 0,
                        .padding2 = 0,
                        .padding3 = 0},
                    SDL_GPUStorageBufferReadWriteBinding{
                        .buffer = daylight_seed_buf,
                        .cycle = false,
                        .padding1 = 0,
                        .padding2 = 0,
                        .padding3 = 0},
                };
                auto* const cp = SDL_BeginGPUComputePass(
                    cmd, nullptr, 0, rw_bufs.data(), static_cast<Uint32>(rw_bufs.size()));
                SDL_BindGPUComputePipeline(cp, s_ambient_pipeline);

                auto const ro_bufs =
                    std::array<SDL_GPUBuffer*, 4>{f_buf, t_buf, source_map_buf, vf_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

                SDL_PushGPUComputeUniformData(cmd, 0, &ambient_push, sizeof(ambient_push));
                SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
                SDL_EndGPUComputePass(cp);
                if (!submit_lighting_stage("ambient", true)) { return {}; }
            }

            // [Pass 4] Compute: local daylight diffusion through transparent openings.
            if (p.direct_sunlight) {
                auto dispatch_daylight_diffusion =
                    [&](SDL_GPUBuffer* const source_buf, SDL_GPUBuffer* const target_buf,
                        float const diffuse_decay) {
                        auto const rw_bufs = std::array<SDL_GPUStorageBufferReadWriteBinding, 2>{
                            SDL_GPUStorageBufferReadWriteBinding{
                                .buffer = target_buf,
                                .cycle = false,
                                .padding1 = 0,
                                .padding2 = 0,
                                .padding3 = 0},
                            SDL_GPUStorageBufferReadWriteBinding{
                                .buffer = lm_buf,
                                .cycle = false,
                                .padding1 = 0,
                                .padding2 = 0,
                                .padding3 = 0},
                        };
                        auto* const cp = SDL_BeginGPUComputePass(
                            cmd, nullptr, 0, rw_bufs.data(), static_cast<Uint32>(rw_bufs.size()));
                        SDL_BindGPUComputePipeline(cp, s_daylight_diffuse_pipeline);
                        auto const ro_bufs =
                            std::array<SDL_GPUBuffer*, 3>{daylight_seed_buf, source_buf, t_buf};
                        SDL_BindGPUComputeStorageBuffers(
                            cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));
                        auto const diffuse_push = lm_daylight_diffuse_push_constants{
                            .total_tiles = volume_tiles,
                            .cache_x = cache_x,
                            .cache_y = cache_y,
                            .cache_xy = cache_xy,
                            .z_count = z_count,
                            .diffuse_decay = diffuse_decay,
                            .min_light = LIGHT_AMBIENT_LOW,
                            ._pad = 0,
                        };
                        SDL_PushGPUComputeUniformData(cmd, 0, &diffuse_push, sizeof(diffuse_push));
                        SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
                        SDL_EndGPUComputePass(cp);
                    };

                auto* source_buf = daylight_seed_buf;
                auto* target_buf = daylight_diffuse_a_buf;
                for (auto const pass : std::views::iota(0, daylight_diffusion_passes)) {
                    dispatch_daylight_diffusion(
                        source_buf, target_buf, daylight_diffusion_decay_for_pass(pass));
                    source_buf = target_buf;
                    target_buf = pass % 2 == 0 ? daylight_diffuse_b_buf : daylight_diffuse_a_buf;
                }
                if (!submit_lighting_stage("daylight diffusion", true)) { return {}; }
            }

            // [Pass 5] Compute: max cached static-source contribution into frame lm.
            auto max_uint_buffer =
                [&](SDL_GPUBuffer* const target_buf, SDL_GPUBuffer* const source_buf) {
                    auto const rw_lm = SDL_GPUStorageBufferReadWriteBinding{
                        .buffer = target_buf,
                        .cycle = false,
                        .padding1 = 0,
                        .padding2 = 0,
                        .padding3 = 0};
                    auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_lm, 1);
                    SDL_BindGPUComputePipeline(cp, s_max_uint_pipeline);
                    auto const ro_bufs = std::array<SDL_GPUBuffer*, 1>{source_buf};
                    SDL_BindGPUComputeStorageBuffers(
                        cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));
                    auto const max_push = lm_max_uint_push_constants{
                        .total_tiles = volume_tiles,
                        ._pad = {},
                    };
                    SDL_PushGPUComputeUniformData(cmd, 0, &max_push, sizeof(max_push));
                    SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
                    SDL_EndGPUComputePass(cp);
                };
            max_uint_buffer(lm_buf, static_lm_buf);
            auto const static_merge_is_final_stage =
                dynamic_raytrace_buckets.empty() && !download_colored_light && !rebuild_seen
                && deferred_download_copy;
            if (!submit_lighting_stage("static lm merge", !static_merge_is_final_stage)) {
                return {};
            }

            // [Pass 6] Compute: dynamic per-source ray casting over ambient/static base.
            if (!dispatch_raytrace(lm_buf, dynamic_raytrace_buckets)) { return {}; }
            auto const dynamic_raytrace_is_final_stage =
                !download_colored_light && !rebuild_seen && deferred_download_copy;
            if (!dynamic_raytrace_buckets.empty()
                && !submit_lighting_stage("dynamic raytrace", !dynamic_raytrace_is_final_stage)) {
                return {};
            }

            if (download_colored_light) {
                fill_uint_buffer(colored_light_buf, 0u);
                if (!submit_lighting_stage("colored lm clear", true)) { return {}; }
                if (!dispatch_color_raytrace(colored_raytrace_buckets)) { return {}; }
                auto const colored_raytrace_is_final_stage =
                    !rebuild_seen && deferred_download_copy;
                if (!colored_raytrace_buckets.empty()
                    && !submit_lighting_stage(
                        "colored raytrace", !colored_raytrace_is_final_stage)) {
                    return {};
                }
            }
        }

        if (rebuild_seen) {
            record_seen_rebuild({
                .cmd = cmd,
                .transparency_buf = t_buf,
                .floor_buf = f_buf,
                .vehicle_floor_buf = vf_buf,
                .vehicle_obscured_buf = vo_buf,
                .seen_raw_buf = seen_raw_buf,
                .seen_buf = seen_buf,
                .player_x = p.player_x,
                .player_y = p.player_y,
                .player_zlev = p.player_zlev,
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .z_start_idx = 0,
                .dispatch_z_count = z_count,
                .vision_block_mask = p.vision_block_mask,
            });
            if (!submit_lighting_stage("seen rebuild", !deferred_download_copy)) { return {}; }
        }

        // [Pass 7] Copy: download dirty lm levels and requested seen_cache results.
        if (needs_download_copy && !deferred_download_copy) {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);

            auto lm_download_offset = Uint32{0};
            if (download_lm_levels) {
                for (int const z : lightmap_levels) {
                    auto const zi = z + OVERMAP_DEPTH;
                    auto const lm_buffer_offset = static_cast<Uint32>(zi) * lm_level_bytes;
                    auto const src_lm = SDL_GPUBufferRegion{
                        .buffer = lm_buf,
                        .offset = lm_buffer_offset,
                        .size = lm_level_bytes,
                    };
                    auto const dst_lm = SDL_GPUTransferBufferLocation{
                        .transfer_buffer = lm_dl_tbuf,
                        .offset = lm_download_offset,
                    };
                    SDL_DownloadFromGPUBuffer(cp, &src_lm, &dst_lm);
                    lm_download_offset += lm_level_bytes;
                }
            }
            if (download_player_light) {
                auto const zi = p.player_zlev + OVERMAP_DEPTH;
                auto const player_idx = static_cast<Uint32>(
                    zi * cache_xy + p.player_x * cache_y + p.player_y);
                auto const src_lm = SDL_GPUBufferRegion{
                    .buffer = lm_buf,
                    .offset = static_cast<Uint32>(player_idx * sizeof(uint32_t)),
                    .size = static_cast<Uint32>(sizeof(uint32_t)),
                };
                auto const dst_lm = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = lm_dl_tbuf,
                    .offset = lm_download_offset,
                };
                SDL_DownloadFromGPUBuffer(cp, &src_lm, &dst_lm);
            }
            if (download_colored_light) {
                auto colored_download_offset = Uint32{0};
                for (auto const z : colored_light_download_levels) {
                    auto const zi = z + OVERMAP_DEPTH;
                    auto const src_color = SDL_GPUBufferRegion{
                        .buffer = colored_light_buf,
                        .offset = static_cast<Uint32>(zi) * uint_level_bytes,
                        .size = uint_level_bytes,
                    };
                    auto const dst_color = SDL_GPUTransferBufferLocation{
                        .transfer_buffer = colored_light_dl_tbuf,
                        .offset = colored_download_offset,
                    };
                    SDL_DownloadFromGPUBuffer(cp, &src_color, &dst_color);
                    colored_download_offset += uint_level_bytes;
                }
            }
            if (!seen_download_levels.empty()) {
                auto seen_download_offset = Uint32{0};
                for (int const z : seen_download_levels) {
                    auto const zi = z + OVERMAP_DEPTH;
                    auto const seen_buffer_offset = static_cast<Uint32>(zi) * float_level_bytes;
                    auto const src_seen = SDL_GPUBufferRegion{
                        .buffer = seen_buf,
                        .offset = seen_buffer_offset,
                        .size = float_level_bytes,
                    };
                    auto const dst_seen = SDL_GPUTransferBufferLocation{
                        .transfer_buffer = seen_dl_tbuf,
                        .offset = seen_download_offset,
                    };
                    SDL_DownloadFromGPUBuffer(cp, &src_seen, &dst_seen);
                    seen_download_offset += float_level_bytes;
                }
            }

            SDL_EndGPUCopyPass(cp);
        }
    }

    // Submit seen-only updates without a CPU fence.  SDL_GPU preserves command
    // buffer submission order, so the later visibility pass can consume these
    // resident buffers and its own download fence will cover the CPU readback.
    auto const defer_seen_only_wait =
        lightmap_levels.empty() && rebuild_seen && upload_total == 0 && lm_download_bytes == 0
        && seen_download_levels.empty();
    TracyPlot("GPU LM Deferred Submit", defer_seen_only_wait ? int64_t{1} : int64_t{0});
    auto* fence = static_cast<SDL_GPUFence*>(nullptr);
    if (cmd == nullptr) {
        TracyPlot("GPU LM Submit Already Complete", int64_t{1});
    } else if (defer_seen_only_wait) {
        ZoneScopedN("gpu_lm_submit_deferred");
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: command buffer submission failed: " << SDL_GetError();
            return {};
        }
    } else {
        fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        if (fence == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: command buffer submission failed: " << SDL_GetError();
            return {};
        }
    }

    auto const work_id = s_next_lighting_work_id++;
    if (s_next_lighting_work_id == 0) { s_next_lighting_work_id = 1; }
    s_pending_lighting_work = pending_gpu_lighting_work{
        .active = true,
        .id = work_id,
        .device = device,
        .fence = fence,
        .m = p.m,
        .input_uploads = std::move(input_uploads),
        .lightmap_levels = std::move(lightmap_levels),
        .all_levels = std::move(all_levels),
        .seen_download_levels = std::move(seen_download_levels),
        .colored_light_download_levels = std::move(colored_light_download_levels),
        .checked_structural_signatures = std::move(checked_structural_signatures),
        .current_ambient_signature = current_ambient_signature,
        .current_static_source_signature = current_static_source_signature,
        .cache_xy = cache_xy,
        .lm_download_bytes = lm_download_bytes,
        .serialized_readback_chunk_bytes =
            deferred_download_copy ? serialized_readback_chunk_bytes : Uint32{0},
        .deferred_download_copy = deferred_download_copy,
        .download_lm_levels = download_lm_levels,
        .download_player_light = download_player_light,
        .download_colored_light = download_colored_light,
        .rebuild_seen = rebuild_seen,
        .rebuild_static_lighting = rebuild_static_lighting,
        .cache_y = cache_y,
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_zlev = p.player_zlev,
    };
    TracyPlot("GPU LM Pending Work", int64_t{1});
    return gpu_lighting_work{.id = work_id};
}

auto finish_gpu_lighting(SDL_GPUDevice* const device, gpu_lighting_work const& work) -> bool {
    ZoneScopedN("finish_gpu_lighting");

    if (work.id == 0) { return false; }
    if (!s_pending_lighting_work.active || s_pending_lighting_work.id != work.id) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: lm: finish requested for missing lighting work";
        return false;
    }
    if (s_pending_lighting_work.device != device) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: lm: finish requested on a different GPU device";
        return false;
    }

    auto pending = std::move(s_pending_lighting_work);
    s_pending_lighting_work = {};
    if (pending.fence != nullptr) {
        auto wait_succeeded = true;
        ZoneScopedN("gpu_lm_fence_wait");
        wait_succeeded = SDL_WaitForGPUFences(device, true, &pending.fence, 1);
        SDL_ReleaseGPUFence(device, pending.fence);
        pending.fence = nullptr;
        if (!wait_succeeded) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: lighting fence wait failed: " << SDL_GetError();
            return false;
        }
    }

    auto* const lm_dl_tbuf = s_lighting_resources.lm_download.buffer;
    auto* const colored_light_dl_tbuf = s_lighting_resources.colored_light_download.buffer;
    auto* const seen_dl_tbuf = s_lighting_resources.seen_download.buffer;

    auto downloads_unpacked_by_deferred = false;
    if (pending.deferred_download_copy) {
        ZoneScopedN("gpu_lm_deferred_download_copy");
        auto* const lm_buf = s_lighting_resources.lm.buffer;
        auto* const colored_light_buf = s_lighting_resources.colored_light.buffer;
        auto* const seen_buf = s_lighting_resources.seen.buffer;
        auto const float_level_bytes = static_cast<Uint32>(pending.cache_xy * sizeof(float));
        auto const uint_level_bytes = static_cast<Uint32>(pending.cache_xy * sizeof(uint32_t));
        auto const lm_level_bytes = static_cast<Uint32>(pending.cache_xy * sizeof(uint32_t));

        if (pending.download_lm_levels) {
            for (auto const z : pending.lightmap_levels) {
                auto const zi = z + OVERMAP_DEPTH;
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto* const dest = reinterpret_cast<std::byte*>(lc.lm.data());
                if (!readback_buffer_region_chunked_batched(
                        device,
                        {
                            .region =
                                {
                                    .source_buffer = lm_buf,
                                    .transfer_buffer = lm_dl_tbuf,
                                    .source_offset = static_cast<Uint32>(zi) * lm_level_bytes,
                                    .size = lm_level_bytes,
                                    .label = "lm level",
                                },
                            .chunk_bytes = pending.serialized_readback_chunk_bytes,
                        },
                        [&](void const* const mapped, Uint32 const offset, Uint32 const bytes) {
                            std::memcpy(dest + offset, mapped, bytes);
                        })) {
                    return false;
                }
                lc.lm_cpu_cache_valid = true;
            }
        }
        if (pending.download_player_light) {
            auto const zi = pending.player_zlev + OVERMAP_DEPTH;
            auto const player_idx = static_cast<Uint32>(
                zi * pending.cache_xy + pending.player_x * pending.cache_y + pending.player_y);
            auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(pending.player_zlev));
            auto const player_tile_idx = lc.idx(pending.player_x, pending.player_y);
            if (!readback_buffer_region(
                    device,
                    {
                        .source_buffer = lm_buf,
                        .transfer_buffer = lm_dl_tbuf,
                        .source_offset = static_cast<Uint32>(player_idx * sizeof(uint32_t)),
                        .size = static_cast<Uint32>(sizeof(uint32_t)),
                        .label = "player light",
                    },
                    [&](void const* const mapped) {
                        std::memcpy(&lc.lm[player_tile_idx], mapped, sizeof(float));
                    })) {
                return false;
            }
        }
        if (pending.download_colored_light) {
            for (auto const z : pending.colored_light_download_levels) {
                auto const zi = z + OVERMAP_DEPTH;
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto* const dest = reinterpret_cast<std::byte*>(lc.colored_light_cache.data());
                if (!readback_buffer_region_chunked_batched(
                        device,
                        {
                            .region =
                                {
                                    .source_buffer = colored_light_buf,
                                    .transfer_buffer = colored_light_dl_tbuf,
                                    .source_offset = static_cast<Uint32>(zi) * uint_level_bytes,
                                    .size = uint_level_bytes,
                                    .label = "colored light level",
                                },
                            .chunk_bytes = pending.serialized_readback_chunk_bytes,
                        },
                        [&](void const* const mapped, Uint32 const offset, Uint32 const bytes) {
                            std::memcpy(dest + offset, mapped, bytes);
                        })) {
                    return false;
                }
                lc.colored_light_cache_active =
                    std::ranges::any_of(lc.colored_light_cache, [](uint32_t const value) {
                        return value != 0u;
                    });
            }
        }
        if (!pending.seen_download_levels.empty()) {
            for (auto const z : pending.seen_download_levels) {
                auto const zi = z + OVERMAP_DEPTH;
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto* const dest = reinterpret_cast<std::byte*>(lc.seen_cache.data());
                if (!readback_buffer_region_chunked_batched(
                        device,
                        {
                            .region =
                                {
                                    .source_buffer = seen_buf,
                                    .transfer_buffer = seen_dl_tbuf,
                                    .source_offset = static_cast<Uint32>(zi) * float_level_bytes,
                                    .size = float_level_bytes,
                                    .label = "seen level",
                                },
                            .chunk_bytes = pending.serialized_readback_chunk_bytes,
                        },
                        [&](void const* const mapped, Uint32 const offset, Uint32 const bytes) {
                            std::memcpy(dest + offset, mapped, bytes);
                        })) {
                    return false;
                }
                lc.seen_cache_dirty = false;
            }
        }
        downloads_unpacked_by_deferred = true;
    }

    // ── Download results to CPU level_cache ──────────────────────────────────
    if (!downloads_unpacked_by_deferred) {
        ZoneScopedN("gpu_lm_unpack_download");
        // lm_all stores uint (bit-reinterpretation of positive floats).
        // Copying uint bytes directly into float storage is valid since the
        // bit pattern is preserved.
        SDL_ClearError();
        auto const* lm_mapped =
            pending.lm_download_bytes == 0
                ? nullptr
                : static_cast<uint32_t const*>(SDL_MapGPUTransferBuffer(device, lm_dl_tbuf, false));
        auto const* seen_mapped =
            pending.seen_download_levels.empty()
                ? nullptr
                : static_cast<float const*>(SDL_MapGPUTransferBuffer(device, seen_dl_tbuf, false));
        auto const* colored_light_mapped =
            pending.download_colored_light
                ? static_cast<uint32_t const*>(
                      SDL_MapGPUTransferBuffer(device, colored_light_dl_tbuf, false))
                : nullptr;
        if ((pending.lm_download_bytes > 0 && lm_mapped == nullptr)
            || (!pending.seen_download_levels.empty() && seen_mapped == nullptr)
            || (pending.download_colored_light && colored_light_mapped == nullptr)) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: download transfer buffer map failed"
                << " lm_bytes=" << pending.lm_download_bytes
                << " seen_levels=" << pending.seen_download_levels.size()
                << " colored=" << pending.download_colored_light << ": " << SDL_GetError();
            if (lm_mapped != nullptr) { SDL_UnmapGPUTransferBuffer(device, lm_dl_tbuf); }
            if (seen_mapped != nullptr) { SDL_UnmapGPUTransferBuffer(device, seen_dl_tbuf); }
            if (colored_light_mapped != nullptr) {
                SDL_UnmapGPUTransferBuffer(device, colored_light_dl_tbuf);
            }
            return false;
        }

        auto lm_level_index = std::size_t{0};
        if (pending.download_lm_levels) {
            for (auto const z : pending.lightmap_levels) {
                // get_cache_ref is the public accessor; const_cast is safe because the
                // underlying level_cache is non-const — the const qualifier is only on
                // the return type of the accessor.
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto const sz = static_cast<std::size_t>(pending.cache_xy);

                auto const* lm_src = lm_mapped + lm_level_index * pending.cache_xy;
                // Reinterpret uint bits as float values for lm.
                std::memcpy(lc.lm.data(), lm_src, sz * sizeof(float));
                lc.lm_cpu_cache_valid = true;
                ++lm_level_index;
            }
        }
        if (pending.download_player_light) {
            auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(pending.player_zlev));
            auto const player_tile_idx = lc.idx(pending.player_x, pending.player_y);
            auto const* player_light_src =
                lm_mapped
                + (pending.download_lm_levels
                       ? pending.lightmap_levels.size() * pending.cache_xy
                       : 0);
            std::memcpy(&lc.lm[player_tile_idx], player_light_src, sizeof(float));
        }

        if (!pending.seen_download_levels.empty()) {
            auto seen_level_index = std::size_t{0};
            for (auto const z : pending.seen_download_levels) {
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto const sz = static_cast<std::size_t>(pending.cache_xy);
                auto const* seen_src = seen_mapped + seen_level_index * pending.cache_xy;
                std::ranges::copy(std::span{seen_src, sz}, lc.seen_cache.begin());
                lc.seen_cache_dirty = false;
                ++seen_level_index;
            }
        }
        if (pending.download_colored_light) {
            auto colored_level_index = std::size_t{0};
            for (auto const z : pending.colored_light_download_levels) {
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto const sz = static_cast<std::size_t>(pending.cache_xy);
                auto const* color_src =
                    colored_light_mapped + colored_level_index * pending.cache_xy;
                auto const color_span = std::span{color_src, sz};
                std::ranges::copy(color_span, lc.colored_light_cache.begin());
                lc.colored_light_cache_active = std::ranges::
                    any_of(color_span, [](uint32_t const value) { return value != 0u; });
                ++colored_level_index;
            }
        }
        if (lm_mapped != nullptr) { SDL_UnmapGPUTransferBuffer(device, lm_dl_tbuf); }
        if (seen_mapped != nullptr) { SDL_UnmapGPUTransferBuffer(device, seen_dl_tbuf); }
        if (colored_light_mapped != nullptr) {
            SDL_UnmapGPUTransferBuffer(device, colored_light_dl_tbuf);
        }
    }

    commit_input_upload_plan(pending.input_uploads);
    if (!pending.lightmap_levels.empty()) {
        s_lighting_resources.static_ambient_signature_valid = true;
        s_lighting_resources.static_ambient_signature = pending.current_ambient_signature;
    }
    if (pending.rebuild_static_lighting) {
        s_lighting_resources.static_lighting_valid = true;
        s_lighting_resources.static_source_signature_valid = true;
        s_lighting_resources.static_source_signature = pending.current_static_source_signature;
        for (auto const& [z, signature] : pending.checked_structural_signatures) {
            auto const zi = z_to_resident_index(z);
            if (!resident_index_is_valid(
                    zi, s_lighting_resources.static_structural_level_signature_valid)) {
                continue;
            }
            s_lighting_resources
                .static_structural_level_signature_valid[static_cast<std::size_t>(zi)] = '\1';
            s_lighting_resources.static_structural_level_signatures[static_cast<std::size_t>(zi)] =
                signature;
        }
    }
    if (pending.rebuild_seen) {
        mark_seen_rebuild_success(pending.all_levels, pending.player_x, pending.player_y, true);
    }
    s_lighting_resources.source_map_valid = true;
    s_lighting_resources.lighting_outputs_valid = true;
    TracyPlot("GPU LM Pending Work", int64_t{0});
    return true;
}

auto run_gpu_lighting(SDL_GPUDevice* const device, run_gpu_lighting_params const& p) -> bool {
    ZoneScopedN("run_gpu_lighting");
    auto const work = begin_gpu_lighting(device, p);
    if (work.id == 0) { return false; }
    return finish_gpu_lighting(device, work);
}

auto begin_gpu_visibility(SDL_GPUDevice* const device, run_gpu_visibility_params const& p)
    -> gpu_visibility_work {
    ZoneScopedN("begin_gpu_visibility");

    if (p.m == nullptr) { return {}; }
    if (s_pending_visibility_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility begin requested while previous "
               "visibility work is pending";
        return {};
    }
    if (s_pending_lighting_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility begin requested while lighting "
               "work is pending";
        return {};
    }
    if (s_pending_sight_pairs_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility begin requested while sight-pair "
               "work is pending";
        return {};
    }
    ensure_resource_device(device);
    if (!ensure_visibility_pipeline(device)) { return {}; }

    static_assert(static_cast<uint32_t>(lit_level::DARK) == 0u);
    static_assert(static_cast<uint32_t>(lit_level::LOW) == 1u);
    static_assert(static_cast<uint32_t>(lit_level::BRIGHT_ONLY) == 2u);
    static_assert(static_cast<uint32_t>(lit_level::LIT) == 3u);
    static_assert(static_cast<uint32_t>(lit_level::BRIGHT) == 4u);
    static_assert(static_cast<uint32_t>(lit_level::BLANK) == 6u);

    auto const& lc0 = p.m->get_cache_ref(p.zlev);
    auto const cache_x = lc0.cache_x;
    auto const cache_y = lc0.cache_y;
    auto const cache_xy = cache_x * cache_y;
    auto const z_count = OVERMAP_LAYERS;
    auto const backend_policy = backend_policy_for_device(device);
    auto const volume_tiles = static_cast<Uint32>(z_count * cache_xy);
    auto const float_volume_bytes = static_cast<Uint32>(volume_tiles * sizeof(float));
    auto const uint_volume_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const uint_level_bytes = static_cast<Uint32>(cache_xy * sizeof(uint32_t));
    auto const source_bytes = static_cast<Uint32>(sizeof(GpuLightSource));
    reset_input_residency_for_shape(cache_x, cache_y, z_count);
    auto const all_levels = make_all_levels(z_count);
    auto visibility_download_levels = make_visibility_download_levels(p, all_levels);
    auto const visibility_dispatch =
        make_visibility_dispatch_plan(visibility_download_levels, cache_xy, z_count);
    auto const rebuild_seen =
        p.rebuild_seen_cache
        || !seen_levels_valid(visibility_download_levels, p.player_x, p.player_y);
    if (rebuild_seen && !ensure_seen_pipelines(device)) { return {}; }
    auto const visibility_download_bytes =
        static_cast<Uint32>(visibility_download_levels.size()) * uint_level_bytes;
    auto const visibility_download_total_bytes = visibility_download_bytes;
    auto const deferred_visibility_download =
        backend_policy.serialize_visibility_downloads && !visibility_download_levels.empty();
    auto requested_packed_visibility_download = deferred_visibility_download;
    if (requested_packed_visibility_download) {
        static auto pack_visibility_shader_available = std::optional<bool>{};
        if (!pack_visibility_shader_available.has_value()) {
            pack_visibility_shader_available = shader_blob_available(
                device,
                "lm_pack_visibility_"
                "compute");
        }
        if (!*pack_visibility_shader_available) {
            requested_packed_visibility_download = false;
            static auto reported_missing_pack_visibility_shader = false;
            if (!reported_missing_pack_visibility_shader) {
                reported_missing_pack_visibility_shader = true;
                DebugLog(DL::Info, DC::Main)
                    << "SDL_GPU: lm: optional packed visibility shader is not available; "
                       "falling back to uncompressed visibility readback";
            }
        }
    }
    auto const use_packed_visibility_download =
        requested_packed_visibility_download && ensure_pack_visibility_pipeline(device);
    auto const packed_level_bytes = packed_visibility_level_bytes(cache_xy);
    auto const packed_volume_bytes = static_cast<Uint32>(z_count) * packed_level_bytes;
    auto const visibility_download_resource_bytes =
        deferred_visibility_download
            ? (use_packed_visibility_download ? packed_level_bytes : uint_level_bytes)
            : std::max(visibility_download_total_bytes, Uint32{1});
    auto vehicle_optics = collect_vehicle_optics(
        *p.m, tripoint_bub_ms{p.player_x, p.player_y, p.player_zlev}, p.zlev);
    if (!vehicle_optics.empty() && !ensure_vehicle_optics_pipeline(device)) { return {}; }
    auto camera_zero = make_camera_zero_plan(all_levels, p.zlev, !vehicle_optics.empty());
    auto const camera_clear_ranges = make_z_level_ranges(camera_zero.upload_levels);
    auto camera_clear_tiles = int64_t{0};
    for (auto const& range : camera_clear_ranges) {
        camera_clear_tiles += static_cast<int64_t>(range.z_count) * cache_xy;
    }
    if (!camera_zero.upload_levels.empty() && !ensure_fill_float_pipeline(device)) { return {}; }
    auto const num_optics = static_cast<Uint32>(vehicle_optics.size());
    auto const camera_optics = static_cast<
        int64_t>(std::ranges::count_if(vehicle_optics, [](GpuVehicleOptic const& optic) {
        return optic.kind == vehicle_optic_camera;
    }));
    auto const vehicle_optics_upload_bytes =
        num_optics > 0 ? static_cast<Uint32>(num_optics * sizeof(GpuVehicleOptic)) : Uint32{0};
    auto const vehicle_optics_buffer_bytes =
        std::max(vehicle_optics_upload_bytes, static_cast<Uint32>(sizeof(GpuVehicleOptic)));
    auto const visibility_upload_bytes = vehicle_optics_upload_bytes;
    auto const visibility_upload_total = std::max(visibility_upload_bytes, 1u);
    auto max_optic_radius = 0;
    for (auto const& optic : vehicle_optics) {
        max_optic_radius = std::max(max_optic_radius, optic.range);
    }
    auto const optic_groups_xy = static_cast<Uint32>((2 * max_optic_radius + 1 + 7) / 8);

    if (!s_lighting_resources.inputs.transparency_valid || !s_lighting_resources.source_map_valid
        || !s_lighting_resources.lighting_outputs_valid) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility requested before resident "
               "lighting inputs are valid";
        return {};
    }
    if (rebuild_seen
        && (!s_lighting_resources.inputs.floor_valid
            || !s_lighting_resources.inputs.vehicle_floor_valid
            || !s_lighting_resources.inputs.vehicle_obscured_valid)) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: seen rebuild requested before resident "
               "structural inputs are valid";
        return {};
    }
    TracyPlot("GPU Visibility Rebuild Seen", rebuild_seen ? int64_t{1} : int64_t{0});
    TracyPlot("GPU Visibility Download Levels",
              static_cast<int64_t>(visibility_download_levels.size()));
    TracyPlot("GPU Visibility Dispatch Levels", static_cast<int64_t>(visibility_dispatch.z_count));
    TracyPlot("GPU Visibility Dispatch Selected",
              visibility_dispatch.selected_range ? int64_t{1} : int64_t{0});
    TracyPlot("GPU Visibility Seen Dispatch Levels",
              rebuild_seen ? static_cast<int64_t>(visibility_dispatch.z_count) : int64_t{0});
    TracyPlot("GPU Visibility Camera Clear Levels",
              static_cast<int64_t>(camera_zero.upload_levels.size()));
    TracyPlot("GPU Visibility Camera Clear Ranges",
              static_cast<int64_t>(camera_clear_ranges.size()));
    TracyPlot("GPU Visibility Camera Clear Tiles", camera_clear_tiles);
    TracyPlot("GPU Visibility Vehicle Optics", static_cast<int64_t>(num_optics));
    TracyPlot("GPU Visibility Camera Optics", camera_optics);
    TracyPlot("GPU Visibility Mirror Optics", static_cast<int64_t>(num_optics) - camera_optics);
    TracyPlot("GPU Visibility Packed Download",
              use_packed_visibility_download ? int64_t{1} : int64_t{0});
    TracyPlot(
        "GPU Visibility Packed Level Bytes",
        use_packed_visibility_download ? static_cast<int64_t>(packed_level_bytes) : int64_t{0});
    TracyPlot(
        "GPU Visibility Packed Batch Slots",
        use_packed_visibility_download
            ? static_cast<int64_t>(dxbc_serialized_readback_batch_slots)
            : int64_t{0});

    if (!ensure_lighting_resources(
            device,
            {
                .transparency_bytes = float_volume_bytes,
                .floor_bytes = uint_volume_bytes,
                .vehicle_floor_bytes = uint_volume_bytes,
                .vehicle_obscured_bytes = uint_volume_bytes,
                .camera_bytes = float_volume_bytes,
                .vehicle_optics_bytes = vehicle_optics_buffer_bytes,
                .source_map_bytes = float_volume_bytes,
                .source_bytes = source_bytes,
                .daylight_bytes = uint_volume_bytes,
                .static_lm_bytes = uint_volume_bytes,
                .output_bytes = uint_volume_bytes,
                .lm_download_bytes = static_cast<Uint32>(cache_xy * sizeof(uint32_t)),
                .seen_download_bytes = Uint32{1},
                .visibility_download_bytes = visibility_download_resource_bytes,
                .upload_bytes = float_volume_bytes,
                .visibility_upload_bytes = visibility_upload_total,
            })) {
        return {};
    }
    if (use_packed_visibility_download
        && !ensure_visibility_packed_resource(device, packed_volume_bytes)) {
        return {};
    }

    auto* const camera_buf = s_lighting_resources.camera.buffer;
    auto* const vehicle_optics_buf = s_lighting_resources.vehicle_optics.buffer;
    auto* const source_map_buf = s_lighting_resources.source_map.buffer;
    auto* const t_buf = s_lighting_resources.transparency.buffer;
    auto* const f_buf = s_lighting_resources.floor.buffer;
    auto* const vf_buf = s_lighting_resources.vehicle_floor.buffer;
    auto* const vo_buf = s_lighting_resources.vehicle_obscured.buffer;
    auto* const lm_buf = s_lighting_resources.lm.buffer;
    auto* const seen_raw_buf = s_lighting_resources.seen_raw.buffer;
    auto* const seen_buf = s_lighting_resources.seen.buffer;
    auto* const visibility_buf = s_lighting_resources.visibility.buffer;
    auto* const visibility_packed_buf = s_lighting_resources.visibility_packed.buffer;
    auto* const upload_tbuf = s_lighting_resources.visibility_upload.buffer;
    auto* const visibility_dl_tbuf = s_lighting_resources.visibility_download.buffer;

    if (visibility_upload_bytes > 0) {
        ZoneScopedN("gpu_visibility_stage_upload");
        auto* const mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, upload_tbuf, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: visibility upload transfer buffer map failed: " << SDL_GetError();
            return {};
        }
        if (vehicle_optics_upload_bytes > 0) {
            std::memcpy(mapped, vehicle_optics.data(), vehicle_optics_upload_bytes);
        }
        SDL_UnmapGPUTransferBuffer(device, upload_tbuf);
    }

    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility command buffer acquisition failed: " << SDL_GetError();
        return {};
    }

    {
        ZoneScopedN("gpu_visibility_record_commands");
        if (!camera_clear_ranges.empty()) {
            ZoneScopedN("gpu_visibility_record_camera_clear");
            auto const rw_camera = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = camera_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_camera, 1);
            SDL_BindGPUComputePipeline(cp, s_fill_float_pipeline);
            for (auto const& range : camera_clear_ranges) {
                auto const clear_push = lm_fill_float_push_constants{
                    .total_tiles = static_cast<Uint32>(range.z_count * cache_xy),
                    .cache_xy = cache_xy,
                    .z_start_idx = range.z_start + OVERMAP_DEPTH,
                    .value = 0.0f,
                };
                SDL_PushGPUComputeUniformData(cmd, 0, &clear_push, sizeof(clear_push));
                SDL_DispatchGPUCompute(cp, (clear_push.total_tiles + 63) / 64, 1, 1);
            }
            SDL_EndGPUComputePass(cp);
        }
        if (visibility_upload_bytes > 0) {
            ZoneScopedN("gpu_visibility_record_upload");
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            if (vehicle_optics_upload_bytes > 0) {
                auto const src_loc = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = upload_tbuf,
                    .offset = 0,
                };
                auto const dst_reg = SDL_GPUBufferRegion{
                    .buffer = vehicle_optics_buf,
                    .offset = 0,
                    .size = vehicle_optics_upload_bytes,
                };
                SDL_UploadToGPUBuffer(cp, &src_loc, &dst_reg, false);
            }
            SDL_EndGPUCopyPass(cp);
        }
        if (rebuild_seen) {
            record_seen_rebuild({
                .cmd = cmd,
                .transparency_buf = t_buf,
                .floor_buf = f_buf,
                .vehicle_floor_buf = vf_buf,
                .vehicle_obscured_buf = vo_buf,
                .seen_raw_buf = seen_raw_buf,
                .seen_buf = seen_buf,
                .player_x = p.player_x,
                .player_y = p.player_y,
                .player_zlev = p.player_zlev,
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .z_start_idx = visibility_dispatch.z_start_idx,
                .dispatch_z_count = visibility_dispatch.z_count,
                .vision_block_mask = p.vision_block_mask,
            });
        }
        if (num_optics > 0) {
            ZoneScopedN("gpu_visibility_record_vehicle_optics");
            auto const rw_camera = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = camera_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_camera, 1);
            SDL_BindGPUComputePipeline(cp, s_vehicle_optics_pipeline);

            auto const ro_bufs = std::array<SDL_GPUBuffer*, 3>{t_buf, seen_buf, vehicle_optics_buf};
            SDL_BindGPUComputeStorageBuffers(
                cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

            auto const optics_push = lm_vehicle_optics_push_constants{
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .num_optics = num_optics,
                .max_radius = max_optic_radius,
                .trigdist = trigdist ? 1u : 0u,
                .visible_threshold = g_visible_threshold,
                .max_view_distance = g_max_view_distance,
                ._pad = {},
            };
            SDL_PushGPUComputeUniformData(cmd, 0, &optics_push, sizeof(optics_push));
            SDL_DispatchGPUCompute(cp, num_optics, optic_groups_xy, optic_groups_xy);
            SDL_EndGPUComputePass(cp);
        }
        {
            ZoneScopedN("gpu_visibility_record_dispatch");
            auto const rw_visibility = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = visibility_buf,
                .cycle = false,
                .padding1 = 0,
                .padding2 = 0,
                .padding3 = 0,
            };
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_visibility, 1);
            SDL_BindGPUComputePipeline(cp, s_visibility_pipeline);

            auto const ro_bufs =
                std::array<SDL_GPUBuffer*, 5>{t_buf, lm_buf, seen_buf, camera_buf, source_map_buf};
            SDL_BindGPUComputeStorageBuffers(
                cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

            auto const visibility_push = lm_visibility_push_constants{
                .player_x = p.player_x,
                .player_y = p.player_y,
                .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .trigdist = trigdist ? 1u : 0u,
                .u_clairvoyance = p.u_clairvoyance,
                .u_unimpaired_range = p.u_unimpaired_range,
                .g_light_level = p.g_light_level,
                .vision_threshold = p.vision_threshold,
                .visibility_scale_factor = p.visibility_scale_factor,
                .visible_threshold = g_visible_threshold,
                .detail_range = p.detail_range,
                .z_start_idx = visibility_dispatch.z_start_idx,
                .dispatch_z_count = visibility_dispatch.z_count,
                ._pad = {},
            };
            SDL_PushGPUComputeUniformData(cmd, 0, &visibility_push, sizeof(visibility_push));
            SDL_DispatchGPUCompute(cp, (visibility_dispatch.tiles + 63) / 64, 1, 1);
            SDL_EndGPUComputePass(cp);
        }
        if (use_packed_visibility_download) {
            ZoneScopedN("gpu_visibility_record_pack_download");
            auto const rw_visibility_packed = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = visibility_packed_buf,
                .cycle = false,
                .padding1 = 0,
                .padding2 = 0,
                .padding3 = 0,
            };
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_visibility_packed, 1);
            SDL_BindGPUComputePipeline(cp, s_pack_visibility_pipeline);

            auto const ro_bufs = std::array<SDL_GPUBuffer*, 1>{visibility_buf};
            SDL_BindGPUComputeStorageBuffers(
                cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

            auto const pack_push = lm_pack_visibility_push_constants{
                .total_words = static_cast<Uint32>(visibility_dispatch.z_count)
                             * packed_visibility_words_per_level(cache_xy),
                .cache_xy = cache_xy,
                .words_per_level = static_cast<int32_t>(
                    packed_visibility_words_per_level(cache_xy)),
                .z_start_idx = visibility_dispatch.z_start_idx,
                .dispatch_z_count = visibility_dispatch.z_count,
                .z_count = z_count,
                ._pad = {},
            };
            SDL_PushGPUComputeUniformData(cmd, 0, &pack_push, sizeof(pack_push));
            SDL_DispatchGPUCompute(cp, (pack_push.total_words + 63) / 64, 1, 1);
            SDL_EndGPUComputePass(cp);
        }
        if (!deferred_visibility_download) {
            ZoneScopedN("gpu_visibility_record_download");
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            auto visibility_download_offset = Uint32{0};
            for (auto const z : visibility_download_levels) {
                auto const zi = z + OVERMAP_DEPTH;
                auto const src_visibility = SDL_GPUBufferRegion{
                    .buffer = visibility_buf,
                    .offset = static_cast<Uint32>(zi) * uint_level_bytes,
                    .size = uint_level_bytes,
                };
                auto const dst_visibility = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = visibility_dl_tbuf,
                    .offset = visibility_download_offset,
                };
                SDL_DownloadFromGPUBuffer(cp, &src_visibility, &dst_visibility);
                visibility_download_offset += uint_level_bytes;
            }
            SDL_EndGPUCopyPass(cp);
        }
    }

    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility command buffer submission failed: " << SDL_GetError();
        return {};
    }

    auto const work_id = s_next_visibility_work_id++;
    if (s_next_visibility_work_id == 0) { s_next_visibility_work_id = 1; }
    s_pending_visibility_work = pending_gpu_visibility_work{
        .active = true,
        .id = work_id,
        .device = device,
        .fence = fence,
        .m = p.m,
        .visibility_download_levels = std::move(visibility_download_levels),
        .camera_nonzero_levels_after_dispatch = std::move(
            camera_zero.nonzero_levels_after_dispatch),
        .cache_xy = cache_xy,
        .player_x = p.player_x,
        .player_y = p.player_y,
        .rebuild_seen = rebuild_seen,
        .force_seen_invalidate = p.rebuild_seen_cache,
        .deferred_download_copy = deferred_visibility_download,
        .packed_download = use_packed_visibility_download,
        .packed_level_bytes = packed_level_bytes,
    };
    TracyPlot("GPU Visibility Pending Work", int64_t{1});
    return gpu_visibility_work{.id = work_id};
}

auto finish_gpu_visibility(SDL_GPUDevice* const device, gpu_visibility_work const& work) -> bool {
    ZoneScopedN("finish_gpu_visibility");

    if (work.id == 0) { return false; }
    if (!s_pending_visibility_work.active || s_pending_visibility_work.id != work.id) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: finish requested for missing visibility "
               "work";
        return false;
    }
    if (s_pending_visibility_work.device != device) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility finish requested on a different "
               "GPU device";
        return false;
    }

    auto pending = std::move(s_pending_visibility_work);
    s_pending_visibility_work = {};
    if (pending.fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility finish requested without a GPU "
               "fence";
        return false;
    }
    auto wait_succeeded = true;
    {
        ZoneScopedN("gpu_visibility_fence_wait");
        wait_succeeded = SDL_WaitForGPUFences(device, true, &pending.fence, 1);
    }
    SDL_ReleaseGPUFence(device, pending.fence);
    pending.fence = nullptr;
    if (!wait_succeeded) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility fence wait failed: " << SDL_GetError();
        return false;
    }

    auto* const visibility_dl_tbuf = s_lighting_resources.visibility_download.buffer;
    auto* const visibility_buf = s_lighting_resources.visibility.buffer;
    auto* const visibility_packed_buf = s_lighting_resources.visibility_packed.buffer;
    auto const uint_level_bytes = static_cast<Uint32>(pending.cache_xy * sizeof(uint32_t));
    auto visibility_unpacked_by_deferred = false;

    if (pending.deferred_download_copy) {
        ZoneScopedN("gpu_visibility_deferred_download_copy");

        if (pending.packed_download) {
            auto const packed_words_per_level =
                pending.packed_level_bytes / static_cast<Uint32>(sizeof(uint32_t));
            auto const ranges = make_z_level_ranges(pending.visibility_download_levels);
            TracyPlot("GPU Visibility Packed Ranges", static_cast<int64_t>(ranges.size()));
            for (auto const& range : ranges) {
                auto packed_range = std::vector<uint32_t>(
                    static_cast<std::size_t>(range.z_count) * packed_words_per_level);
                auto* const dest = reinterpret_cast<std::byte*>(packed_range.data());
                auto const zi = range.z_start + OVERMAP_DEPTH;
                auto const range_bytes =
                    static_cast<Uint32>(range.z_count) * pending.packed_level_bytes;
                if (!readback_buffer_region_chunked_batched(
                        device,
                        {
                            .region =
                                {
                                    .source_buffer = visibility_packed_buf,
                                    .transfer_buffer = visibility_dl_tbuf,
                                    .source_offset =
                                        static_cast<Uint32>(zi) * pending.packed_level_bytes,
                                    .size = range_bytes,
                                    .label = "packed visibility range",
                                },
                            .chunk_bytes = dxbc_serialized_readback_chunk_bytes,
                        },
                        [&](void const* const mapped, Uint32 const offset, Uint32 const bytes) {
                            std::memcpy(dest + offset, mapped, bytes);
                        })) {
                    return false;
                }
                for (auto const rel_z : std::views::iota(0, range.z_count)) {
                    auto const z = range.z_start + rel_z;
                    auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                    auto const* src =
                        packed_range.data()
                        + static_cast<std::size_t>(rel_z) * packed_words_per_level;
                    unpack_packed_visibility_level(
                        src, static_cast<std::size_t>(pending.cache_xy), lc.visibility_cache);
                    lc.visibility_cache_dirty = false;
                }
            }
        } else {
            for (auto const z : pending.visibility_download_levels) {
                auto const zi = z + OVERMAP_DEPTH;
                auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
                auto const sz = static_cast<std::size_t>(pending.cache_xy);
                if (!readback_buffer_region(
                        device,
                        {
                            .source_buffer = visibility_buf,
                            .transfer_buffer = visibility_dl_tbuf,
                            .source_offset = static_cast<Uint32>(zi) * uint_level_bytes,
                            .size = uint_level_bytes,
                            .label = "visibility level",
                        },
                        [&](void const* const mapped) {
                            auto const* src = static_cast<uint32_t const*>(mapped);
                            std::ranges::transform(
                                std::span{src, sz}, lc.visibility_cache.begin(),
                                [](uint32_t const value) { return static_cast<lit_level>(value); });
                            lc.visibility_cache_dirty = false;
                        })) {
                    return false;
                }
            }
        }
        visibility_unpacked_by_deferred = true;
    }

    if (!visibility_unpacked_by_deferred) {
        ZoneScopedN("gpu_visibility_unpack_download");
        auto const* visibility_mapped = static_cast<uint32_t const*>(
            SDL_MapGPUTransferBuffer(device, visibility_dl_tbuf, false));
        if (visibility_mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: visibility download transfer buffer map failed: "
                << SDL_GetError();
            return false;
        }

        auto visibility_level_index = std::size_t{0};
        for (auto const z : pending.visibility_download_levels) {
            auto& lc = const_cast<level_cache&>(pending.m->get_cache_ref(z));
            auto const sz = static_cast<std::size_t>(pending.cache_xy);
            auto const* src = visibility_mapped + visibility_level_index * pending.cache_xy;
            std::ranges::transform(
                std::span{src, sz}, lc.visibility_cache.begin(),
                [](uint32_t const value) { return static_cast<lit_level>(value); });
            lc.visibility_cache_dirty = false;
            ++visibility_level_index;
        }

        SDL_UnmapGPUTransferBuffer(device, visibility_dl_tbuf);
    }

    if (pending.rebuild_seen) {
        mark_seen_rebuild_success(
            pending.visibility_download_levels, pending.player_x, pending.player_y,
            pending.force_seen_invalidate);
    }
    s_lighting_resources.camera_valid = true;
    s_lighting_resources.camera_nonzero_levels = pending.camera_nonzero_levels_after_dispatch;
    TracyPlot("GPU Visibility Pending Work", int64_t{0});
    return true;
}

auto run_gpu_visibility(SDL_GPUDevice* const device, run_gpu_visibility_params const& p) -> bool {
    ZoneScopedN("run_gpu_visibility");
    auto const work = begin_gpu_visibility(device, p);
    if (work.id == 0) { return false; }
    return finish_gpu_visibility(device, work);
}

auto sight_pair_levels(std::vector<GpuSightPair> const& pairs, int const z_count)
    -> std::vector<int> {
    auto levels = std::vector<int>{};
    levels.reserve(pairs.size());
    for (auto const& pair : pairs) {
        auto const min_z_idx = std::min(pair.from_z_idx, pair.to_z_idx);
        auto const max_z_idx = std::max(pair.from_z_idx, pair.to_z_idx);
        std::ranges::copy(
            std::views::iota(min_z_idx, max_z_idx + 1)
                | std::views::filter([z_count](int const z_idx) {
                      return z_idx >= 0 && z_idx < z_count;
                  })
                | std::views::transform([](int const z_idx) { return z_idx - OVERMAP_DEPTH; }),
            std::back_inserter(levels));
    }
    return sorted_unique(std::move(levels));
}

auto resident_lighting_ready_for_sight_pairs(resident_sight_pair_inputs_params const& p) -> bool {
    if (p.device == nullptr || p.m == nullptr || p.pairs == nullptr || p.pairs->empty()) {
        return false;
    }

    ensure_resource_device(p.device);

    auto const& lc0 = p.m->get_cache_ref(p.zlev);
    auto const cache_x = lc0.cache_x;
    auto const cache_y = lc0.cache_y;
    auto const z_count = OVERMAP_LAYERS;
    reset_input_residency_for_shape(cache_x, cache_y, z_count);

    auto const levels = sight_pair_levels(*p.pairs, z_count);
    auto const& inputs = s_lighting_resources.inputs;
    return inputs.cache_x == cache_x && inputs.cache_y == cache_y && inputs.z_count == z_count
        && inputs.floor_valid && s_lighting_resources.floor.buffer != nullptr
        && s_lighting_resources.transparency.buffer != nullptr
        && std::ranges::all_of(levels, [](int const z) { return transparency_level_is_valid(z); });
}

auto begin_gpu_sight_pairs(SDL_GPUDevice* const device, begin_gpu_sight_pairs_params const& p)
    -> gpu_sight_pairs_work {
    ZoneScopedN("begin_gpu_sight_pairs");

    if (p.m == nullptr || p.pairs == nullptr) { return {}; }
    if (p.pairs->empty()) { return {}; }
    if (s_pending_lighting_work.active || s_pending_visibility_work.active
        || s_pending_sight_pairs_work.active) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: sight pair batch requested while other lm "
               "GPU work is pending";
        return {};
    }
    ensure_resource_device(device);
    if (!ensure_sight_pairs_pipeline(device)) { return {}; }

    auto const& lc0 = p.m->get_cache_ref(p.zlev);
    auto const cache_x = lc0.cache_x;
    auto const cache_y = lc0.cache_y;
    auto const cache_xy = cache_x * cache_y;
    auto const z_count = OVERMAP_LAYERS;
    reset_input_residency_for_shape(cache_x, cache_y, z_count);

    auto const sight_levels = sight_pair_levels(*p.pairs, z_count);
    auto const& inputs = s_lighting_resources.inputs;
    auto const resident_terrain_ready =
        inputs.floor_valid && std::ranges::all_of(sight_levels, [](int const z) {
            return transparency_level_is_valid(z);
        });
    if (!resident_terrain_ready || s_lighting_resources.transparency.buffer == nullptr
        || s_lighting_resources.floor.buffer == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: sight pair query requested before resident "
               "transparency/floor inputs are valid";
        return {};
    }
    auto const pair_count = static_cast<Uint32>(p.pairs->size());
    auto const pair_bytes = static_cast<Uint32>(p.pairs->size() * sizeof(GpuSightPair));
    auto const result_bytes = static_cast<Uint32>(p.pairs->size() * sizeof(uint32_t));
    auto const upload_bytes = pair_bytes;
    auto const read_usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);
    if (!ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.sight_pairs,
            .usage = read_usage,
            .required_bytes = pair_bytes,
            .name = "sight_pairs",
        })
        || !ensure_gpu_buffer({
            .device = device,
            .slot = &s_lighting_resources.sight_results,
            .usage = read_write_usage,
            .required_bytes = result_bytes,
            .name = "sight_results",
        })
        || !ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.sight_upload,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .required_bytes = upload_bytes,
            .name = "sight_upload",
        })
        || !ensure_transfer_buffer({
            .device = device,
            .slot = &s_lighting_resources.sight_download,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .required_bytes = result_bytes,
            .name = "sight_download",
        })) {
        return {};
    }

    TracyPlot("GPU Sight Pair Count", static_cast<int64_t>(pair_count));
    TracyPlot("GPU Sight Terrain Upload Levels", int64_t{0});
    TracyPlot("GPU Sight Terrain Upload Bytes", int64_t{0});
    TracyPlot("GPU Sight Resident Terrain Levels", static_cast<int64_t>(sight_levels.size()));
    {
        ZoneScopedN("gpu_sight_stage_upload");
        auto* const mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, s_lighting_resources.sight_upload.buffer, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: sight upload transfer buffer map failed: " << SDL_GetError();
            return {};
        }
        std::memcpy(mapped, p.pairs->data(), pair_bytes);
        SDL_UnmapGPUTransferBuffer(device, s_lighting_resources.sight_upload.buffer);
    }

    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: sight command buffer acquisition failed: " << SDL_GetError();
        return {};
    }

    {
        ZoneScopedN("gpu_sight_record_commands");
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            auto upload_copy_commands = Uint32{0};
            if (pair_bytes > 0) {
                auto const src_pairs = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = s_lighting_resources.sight_upload.buffer,
                    .offset = 0,
                };
                auto const dst_pairs = SDL_GPUBufferRegion{
                    .buffer = s_lighting_resources.sight_pairs.buffer,
                    .offset = 0,
                    .size = pair_bytes,
                };
                SDL_UploadToGPUBuffer(cp, &src_pairs, &dst_pairs, false);
                ++upload_copy_commands;
            }
            TracyPlot("GPU Sight Upload Copy Commands", static_cast<int64_t>(upload_copy_commands));
            SDL_EndGPUCopyPass(cp);
        }
        {
            auto const rw_results = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = s_lighting_resources.sight_results.buffer,
                .cycle = false,
                .padding1 = 0,
                .padding2 = 0,
                .padding3 = 0,
            };
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_results, 1);
            SDL_BindGPUComputePipeline(cp, s_sight_pairs_pipeline);

            auto const ro_bufs = std::array<SDL_GPUBuffer*, 3>{
                s_lighting_resources.transparency.buffer,
                s_lighting_resources.floor.buffer,
                s_lighting_resources.sight_pairs.buffer,
            };
            SDL_BindGPUComputeStorageBuffers(
                cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

            auto const push = lm_sight_pairs_push_constants{
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .pair_count = pair_count,
                ._pad = {},
            };
            SDL_PushGPUComputeUniformData(cmd, 0, &push, sizeof(push));
            SDL_DispatchGPUCompute(cp, (pair_count + 63) / 64, 1, 1);
            SDL_EndGPUComputePass(cp);
        }
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            auto const src_results = SDL_GPUBufferRegion{
                .buffer = s_lighting_resources.sight_results.buffer,
                .offset = 0,
                .size = result_bytes,
            };
            auto const dst_results = SDL_GPUTransferBufferLocation{
                .transfer_buffer = s_lighting_resources.sight_download.buffer,
                .offset = 0,
            };
            SDL_DownloadFromGPUBuffer(cp, &src_results, &dst_results);
            SDL_EndGPUCopyPass(cp);
        }
    }

    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: sight command buffer submission failed: " << SDL_GetError();
        return {};
    }

    auto const work_id = s_next_sight_pairs_work_id++;
    if (s_next_sight_pairs_work_id == 0) { s_next_sight_pairs_work_id = 1; }
    s_pending_sight_pairs_work = pending_gpu_sight_pairs_work{
        .active = true,
        .id = work_id,
        .device = device,
        .fence = fence,
        .pair_count = p.pairs->size(),
    };
    TracyPlot("GPU Sight Pending Work", int64_t{1});
    return gpu_sight_pairs_work{.id = work_id};
}

auto finish_gpu_sight_pairs(
    SDL_GPUDevice* const device, gpu_sight_pairs_work const& work, std::vector<uint32_t>& results)
    -> bool {
    ZoneScopedN("finish_gpu_sight_pairs");

    if (work.id == 0) {
        results.clear();
        return true;
    }
    if (!s_pending_sight_pairs_work.active || s_pending_sight_pairs_work.id != work.id) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: finish requested for missing sight-pair "
               "work";
        return false;
    }
    if (s_pending_sight_pairs_work.device != device) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: sight-pair finish requested on a different "
               "GPU device";
        return false;
    }

    auto pending = std::move(s_pending_sight_pairs_work);
    s_pending_sight_pairs_work = {};
    if (pending.fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: sight-pair finish requested without a GPU "
               "fence";
        return false;
    }

    auto wait_succeeded = true;
    {
        ZoneScopedN("gpu_sight_fence_wait");
        wait_succeeded = SDL_WaitForGPUFences(device, true, &pending.fence, 1);
    }
    SDL_ReleaseGPUFence(device, pending.fence);
    pending.fence = nullptr;
    if (!wait_succeeded) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: lm: sight fence wait failed: " << SDL_GetError();
        return false;
    }

    {
        ZoneScopedN("gpu_sight_unpack_download");
        auto const* mapped = static_cast<uint32_t const*>(
            SDL_MapGPUTransferBuffer(device, s_lighting_resources.sight_download.buffer, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: sight download transfer buffer map failed: " << SDL_GetError();
            return false;
        }
        results.assign(mapped, mapped + pending.pair_count);
        SDL_UnmapGPUTransferBuffer(device, s_lighting_resources.sight_download.buffer);
    }

    TracyPlot("GPU Sight Pending Work", int64_t{0});
    return true;
}

auto run_gpu_sight_pairs(SDL_GPUDevice* const device, run_gpu_sight_pairs_params const& p) -> bool {
    ZoneScopedN("run_gpu_sight_pairs");

    if (p.results == nullptr) { return false; }
    auto const work = begin_gpu_sight_pairs(
        device,
        {
            .m = p.m,
            .pairs = p.pairs,
            .zlev = p.zlev,
        });
    if (work.id == 0) {
        p.results->assign(p.pairs == nullptr ? std::size_t{0} : p.pairs->size(), 0u);
        return p.pairs != nullptr && p.pairs->empty();
    }
    return finish_gpu_sight_pairs(device, work, *p.results);
}

auto shutdown_lm() -> void {
    auto* const device = get_device();
    if (device == nullptr) { return; }
    if (!SDL_WaitForGPUIdle(device)) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: lm: wait for idle during shutdown failed: " << SDL_GetError();
    }
    release_lighting_resources(device);
    release_lm_pipelines(device);
}

} // namespace cata_gpu
#endif // defined( CATA_SDL )
