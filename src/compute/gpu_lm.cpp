#if defined(CATA_SDL)
#include "gpu_lm.h"

#include "cached_options.h"
#include "character.h"
#include "debug.h"
#include "effect.h"
#include "game.h"
#include "game_constants.h"
#include "gpu_platform.h"
#include "lightmap.h"
#include "map.h"
#include "monster.h"
#include "npc.h"
#include "path_info.h"
#include "profile.h"
#include "shadowcasting.h"

#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cata_gpu {

namespace {

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
    if (fmts & SDL_GPU_SHADERFORMAT_SPIRV) { return ".spv"; }
    if (fmts & SDL_GPU_SHADERFORMAT_MSL) { return ".msl"; }
    return {};
}

auto preferred_fmt(SDL_GPUShaderFormat const fmts) -> SDL_GPUShaderFormat {
    if (fmts & SDL_GPU_SHADERFORMAT_DXIL) { return SDL_GPU_SHADERFORMAT_DXIL; }
    if (fmts & SDL_GPU_SHADERFORMAT_SPIRV) { return SDL_GPU_SHADERFORMAT_SPIRV; }
    if (fmts & SDL_GPU_SHADERFORMAT_MSL) { return SDL_GPU_SHADERFORMAT_MSL; }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

SDL_GPUComputePipeline* s_ambient_pipeline = nullptr;
SDL_GPUComputePipeline* s_raytrace_pipeline = nullptr;
SDL_GPUComputePipeline* s_seen_pipeline = nullptr;
SDL_GPUComputePipeline* s_seen_walls_pipeline = nullptr;
SDL_GPUComputePipeline* s_visibility_pipeline = nullptr;

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
        .entrypoint = "main",
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

auto ensure_pipelines(SDL_GPUDevice* const device) -> bool {
    if (s_ambient_pipeline == nullptr) {
        s_ambient_pipeline = load_pipeline(
            device, "lm_ambient_compute",
            /*ro=*/2, /*rw=*/1, 64, 1);
    }
    if (s_raytrace_pipeline == nullptr) {
        s_raytrace_pipeline = load_pipeline(
            device, "lm_raytrace_compute",
            /*ro=*/4, /*rw=*/1, 8, 8);
    }
    if (s_seen_pipeline == nullptr) {
        s_seen_pipeline = load_pipeline(
            device, "lm_seen_compute",
            /*ro=*/3, /*rw=*/1, 8, 8);
    }
    if (s_seen_walls_pipeline == nullptr) {
        s_seen_walls_pipeline = load_pipeline(
            device, "lm_seen_walls_compute",
            /*ro=*/3, /*rw=*/1, 8, 8);
    }
    return s_ambient_pipeline != nullptr && s_raytrace_pipeline != nullptr
        && s_seen_pipeline != nullptr && s_seen_walls_pipeline != nullptr;
}

auto ensure_visibility_pipeline(SDL_GPUDevice* const device) -> bool {
    if (s_visibility_pipeline == nullptr) {
        s_visibility_pipeline = load_pipeline(
            device, "lm_visibility_compute",
            /*ro=*/5, /*rw=*/1, 64, 1);
    }
    return s_visibility_pipeline != nullptr;
}

// ---------------------------------------------------------------------------
// Persistent GPU lighting resources
// ---------------------------------------------------------------------------

struct gpu_buffer_slot {
    SDL_GPUBuffer* buffer = nullptr;
    Uint32 capacity = 0;
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
};

struct lighting_resource_cache {
    SDL_GPUDevice* device = nullptr;
    gpu_buffer_slot transparency;
    gpu_buffer_slot floor;
    gpu_buffer_slot vehicle_floor;
    gpu_buffer_slot camera;
    gpu_buffer_slot source_map;
    gpu_buffer_slot sources;
    gpu_buffer_slot lm;
    gpu_buffer_slot seen_raw;
    gpu_buffer_slot seen;
    gpu_buffer_slot visibility;
    gpu_transfer_slot upload;
    gpu_transfer_slot visibility_upload;
    gpu_transfer_slot lm_download;
    gpu_transfer_slot seen_download;
    gpu_transfer_slot visibility_download;
    std::vector<float> transparency_staging;
    std::vector<uint32_t> floor_staging;
    std::vector<uint32_t> vehicle_floor_staging;
    std::vector<float> camera_staging;
    std::vector<float> source_map_staging;
    lighting_input_residency inputs;
    bool seen_valid = false;
    bool lighting_outputs_valid = false;
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
    Uint32 camera_bytes;
    Uint32 source_map_bytes;
    Uint32 source_bytes;
    Uint32 output_bytes;
    Uint32 lm_download_bytes;
    Uint32 visibility_download_bytes;
    Uint32 upload_bytes;
    Uint32 visibility_upload_bytes;
};

struct input_upload_plan {
    bool transparency;
    bool floor;
    bool vehicle_floor;
};

lighting_resource_cache s_lighting_resources;

auto release_buffer_slot(SDL_GPUDevice* const device, gpu_buffer_slot& slot) -> void {
    if (slot.buffer != nullptr) {
        SDL_ReleaseGPUBuffer(device, slot.buffer);
        slot.buffer = nullptr;
        slot.capacity = 0;
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
    release_buffer_slot(device, s_lighting_resources.transparency);
    release_buffer_slot(device, s_lighting_resources.floor);
    release_buffer_slot(device, s_lighting_resources.vehicle_floor);
    release_buffer_slot(device, s_lighting_resources.camera);
    release_buffer_slot(device, s_lighting_resources.source_map);
    release_buffer_slot(device, s_lighting_resources.sources);
    release_buffer_slot(device, s_lighting_resources.lm);
    release_buffer_slot(device, s_lighting_resources.seen_raw);
    release_buffer_slot(device, s_lighting_resources.seen);
    release_buffer_slot(device, s_lighting_resources.visibility);
    release_transfer_slot(device, s_lighting_resources.upload);
    release_transfer_slot(device, s_lighting_resources.visibility_upload);
    release_transfer_slot(device, s_lighting_resources.lm_download);
    release_transfer_slot(device, s_lighting_resources.seen_download);
    release_transfer_slot(device, s_lighting_resources.visibility_download);
    s_lighting_resources.transparency_staging = {};
    s_lighting_resources.floor_staging = {};
    s_lighting_resources.vehicle_floor_staging = {};
    s_lighting_resources.camera_staging = {};
    s_lighting_resources.source_map_staging = {};
    s_lighting_resources.inputs = {};
    s_lighting_resources.seen_valid = false;
    s_lighting_resources.lighting_outputs_valid = false;
    s_lighting_resources.device = nullptr;
}

auto ensure_resource_device(SDL_GPUDevice* const device) -> void {
    if (s_lighting_resources.device == device) { return; }
    if (s_lighting_resources.device != nullptr) {
        release_lighting_resources(s_lighting_resources.device);
    }
    s_lighting_resources.device = device;
}

auto ensure_gpu_buffer(ensure_gpu_buffer_params const& p) -> bool {
    if (p.slot->buffer != nullptr && p.slot->capacity >= p.required_bytes) { return true; }

    release_buffer_slot(p.device, *p.slot);
    auto const ci = SDL_GPUBufferCreateInfo{
        .usage = p.usage,
        .size = p.required_bytes,
        .props = 0,
    };
    p.slot->buffer = SDL_CreateGPUBuffer(p.device, &ci);
    if (p.slot->buffer == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: failed to allocate " << p.name << " buffer ("
            << p.required_bytes << " bytes): " << SDL_GetError();
        p.slot->capacity = 0;
        return false;
    }
    p.slot->capacity = p.required_bytes;
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

auto ensure_lighting_resources(SDL_GPUDevice* const device, lighting_buffer_sizes const& sizes)
    -> bool {
    ensure_resource_device(device);

    auto const read_usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    auto const read_write_usage = static_cast<SDL_GPUBufferUsageFlags>(
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);

    return ensure_gpu_buffer({
               .device = device,
               .slot = &s_lighting_resources.transparency,
               .usage = read_usage,
               .required_bytes = sizes.transparency_bytes,
               .name = "transparency",
           })
        && ensure_gpu_buffer({
               .device = device,
               .slot = &s_lighting_resources.floor,
               .usage = read_usage,
               .required_bytes = sizes.floor_bytes,
               .name = "floor",
           })
        && ensure_gpu_buffer({
               .device = device,
               .slot = &s_lighting_resources.vehicle_floor,
               .usage = read_usage,
               .required_bytes = sizes.vehicle_floor_bytes,
               .name = "vehicle_floor",
           })
        && ensure_gpu_buffer({
               .device = device,
               .slot = &s_lighting_resources.camera,
               .usage = read_usage,
               .required_bytes = sizes.camera_bytes,
               .name = "camera",
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
               .required_bytes = sizes.transparency_bytes,
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

// ---------------------------------------------------------------------------
// Light source collection
// ---------------------------------------------------------------------------

auto make_source(int const x, int const y, int const zlev, float const luminance)
    -> GpuLightSource {
    return GpuLightSource{
        .x = x,
        .y = y,
        .z_idx = zlev + OVERMAP_DEPTH,
        .luminance = luminance,
        .radius = compute_light_radius(luminance),
        ._pad = {},
    };
}

auto collect_sources(map const& m, std::vector<int> const& dirty_levels)
    -> std::vector<GpuLightSource> {
    auto sources = std::vector<GpuLightSource>{};

    // Collect omnidirectional point sources from the touched source-tile list.
    // These were populated by generate_lightmap_worker (collect-only mode)
    // and cover terrain, furniture, item, and vehicle circular lights.
    for (int const z : dirty_levels) {
        auto const& lc = m.get_cache_ref(z);
        auto const& lsb = lc.light_source_buffer;
        for (auto const point : lc.light_source_points) {
            float const lum = lsb[lc.idx(point.x(), point.y())];
            if (lum > LIGHT_AMBIENT_LOW) {
                sources.push_back(make_source(point.x(), point.y(), z, lum));
            }
        }
    }

    // Collect character lights (player + NPCs).
    // effect_haslight is a CPU-path artifact that marks characters lit by the
    // CPU lightmap; it must not be used here because the GPU derives lm itself.
    // The character's actual emitted light is captured by active_light() and
    // by light_source_buffer (populated during generate_lightmap_worker).
    static const efftype_id effect_onfire("onfire");

    auto add_char = [&](Character const& ch) {
        auto const& pos = ch.bub_pos();
        if (!m.inbounds(pos)) { return; }
        if (ch.has_effect(effect_onfire)) {
            sources.push_back(make_source(pos.x(), pos.y(), pos.z(), 8.0f));
        }
        float const held = ch.active_light();
        if (held > LIGHT_AMBIENT_LOW) {
            sources.push_back(make_source(pos.x(), pos.y(), pos.z(), held));
        }
    };

    add_char(get_player_character());
    for (npc const& guy : g->all_npcs()) { add_char(guy); }

    // Collect monster lights.
    for (monster const& critter : g->all_monsters()) {
        if (critter.is_hallucination()) { continue; }
        auto const& mp = critter.bub_pos();
        if (!m.inbounds(mp)) { continue; }
        if (critter.has_effect(effect_onfire)) {
            sources.push_back(make_source(mp.x(), mp.y(), mp.z(), 8.0f));
        }
        if (critter.type->luminance > 0) {
            sources.push_back(
                make_source(mp.x(), mp.y(), mp.z(), static_cast<float>(critter.type->luminance)));
        }
    }

    return sources;
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
        auto const zlev = source.z_idx - OVERMAP_DEPTH;
        if (!source_is_on_dirty_level(dirty_levels, zlev)) {
            continue;
        }
        auto& lc = const_cast<level_cache&>(m.get_cache_ref(zlev));
        if (source.x < 0 || source.y < 0 || source.x >= lc.cache_x || source.y >= lc.cache_y) {
            continue;
        }
        auto& value = lc.sm[lc.idx(source.x, source.y)];
        value = std::max(value, source.luminance);
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
    };
    s_lighting_resources.seen_valid = false;
    s_lighting_resources.lighting_outputs_valid = false;
}

auto make_input_upload_plan(run_gpu_lighting_params const& p) -> input_upload_plan {
    auto const& inputs = s_lighting_resources.inputs;
    return input_upload_plan{
        .transparency = p.transparency_dirty || !inputs.transparency_valid,
        .floor = p.floor_dirty || !inputs.floor_valid,
        .vehicle_floor = p.vehicle_floor_dirty || !inputs.vehicle_floor_valid,
    };
}

auto has_structural_upload(input_upload_plan const& plan) -> bool {
    return plan.transparency || plan.floor || plan.vehicle_floor;
}

auto commit_input_upload_plan(input_upload_plan const& plan) -> void {
    auto& inputs = s_lighting_resources.inputs;
    if (plan.transparency) {
        inputs.transparency_valid = true;
    }
    if (plan.floor) {
        inputs.floor_valid = true;
    }
    if (plan.vehicle_floor) {
        inputs.vehicle_floor_valid = true;
    }
}

// Pack a per-z-level std::vector<float> cache into a combined 3D buffer.
// Accumulates into `out`, advancing `offset` by each z-level's contribution.
auto pack_float_cache(
    map const& m, std::vector<int> const& levels, int const z_count, int const cache_xy,
    auto const cache_accessor, std::vector<float>& out) -> void {
    out.assign(static_cast<std::size_t>(z_count * cache_xy), 0.0f);
    for (int const z : levels) {
        auto const& lc = m.get_cache_ref(z);
        auto const zi = z + OVERMAP_DEPTH;
        auto const& src = cache_accessor(lc);
        auto* dst = out.data() + zi * cache_xy;
        std::ranges::copy(src, dst);
    }
}

// Pack a per-z-level std::vector<char> cache into a combined 3D uint buffer.
// Non-zero char → uint 1; zero → uint 0.
auto pack_char_cache_uint(
    map const& m, std::vector<int> const& levels, int const z_count, int const cache_xy,
    auto const cache_accessor, std::vector<uint32_t>& out) -> void {
    out.assign(static_cast<std::size_t>(z_count * cache_xy), 0u);
    for (int const z : levels) {
        auto const& lc = m.get_cache_ref(z);
        auto const zi = z + OVERMAP_DEPTH;
        auto const& src = cache_accessor(lc);
        auto* dst = out.data() + zi * cache_xy;
        std::ranges::transform(src, dst, [](char const c) -> uint32_t { return c != 0 ? 1u : 0u; });
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto compute_light_radius(float const luminance) -> float {
    if (luminance <= LIGHT_AMBIENT_LOW) { return 0.0f; }
    auto const raw =
        -std::log(LIGHT_AMBIENT_LOW / luminance) * (1.0f / LIGHT_TRANSPARENCY_OPEN_AIR);
    return std::min(raw, static_cast<float>(MAX_VIEW_DISTANCE));
}

auto run_gpu_lighting(SDL_GPUDevice* const device, run_gpu_lighting_params const& p) -> bool {
    ZoneScopedN( "run_gpu_lighting" );

    if (p.dirty_levels == nullptr || p.dirty_levels->empty()) { return false; }
    if (!ensure_pipelines(device)) { return false; }
    ensure_resource_device(device);

    auto const& lc0 = p.m->get_cache_ref((*p.dirty_levels)[0]);
    auto const cache_x = lc0.cache_x;
    auto const cache_y = lc0.cache_y;
    auto const cache_xy = cache_x * cache_y;
    auto const z_count = OVERMAP_LAYERS;

    // ── Collect light sources ────────────────────────────────────────────────
    auto sources = std::vector<GpuLightSource>{};
    {
        ZoneScopedN( "gpu_lm_collect_sources" );
        sources = collect_sources(*p.m, *p.dirty_levels);
        write_source_map_to_level_caches(*p.m, *p.dirty_levels, sources);
    }
    auto const num_src = static_cast<Uint32>(sources.size());

    // Compute max dispatch radius (clamp to 0 when no sources).
    auto max_radius = 0;
    for (auto const& src : sources) {
        max_radius = std::max(max_radius, static_cast<int>(std::ceil(src.radius)));
    }
    auto const groups_xy = static_cast<Uint32>((2 * max_radius + 1 + 7) / 8);

    // ── Pack CPU input buffers into flat 3D arrays ───────────────────────────
    // Transparency, floor, and vehicle_floor are packed for ALL z-levels so
    // the seen and ambient shaders have correct data for cross-z-level floor
    // blocking and ceiling checks even when only a subset of levels is dirty.
    reset_input_residency_for_shape(cache_x, cache_y, z_count);
    auto const input_uploads = make_input_upload_plan(p);
    auto const all_levels = make_all_levels(z_count);

    auto& transparency_cpu = s_lighting_resources.transparency_staging;
    auto& floor_cpu = s_lighting_resources.floor_staging;
    auto& vehicle_floor_cpu = s_lighting_resources.vehicle_floor_staging;

    if (has_structural_upload(input_uploads)) {
        ZoneScopedN( "gpu_lm_pack_inputs" );
        if (input_uploads.transparency) {
            pack_float_cache(
                *p.m, all_levels, z_count, cache_xy,
                [](level_cache const& lc) -> std::vector<float> const& {
                    return lc.transparency_cache;
                },
                transparency_cpu);
        }
        if (input_uploads.floor) {
            pack_char_cache_uint(
                *p.m, all_levels, z_count, cache_xy,
                [](level_cache const& lc) -> std::vector<char> const& { return lc.floor_cache; },
                floor_cpu);
        }
        if (input_uploads.vehicle_floor) {
            pack_char_cache_uint(
                *p.m, all_levels, z_count, cache_xy,
                [](level_cache const& lc) -> std::vector<char> const& {
                    return lc.vehicle_floor_cache;
                },
                vehicle_floor_cpu);
        }
    }

    // ── Compute ambient constants per z-level ────────────────────────────────
    auto ambient_push = lm_ambient_push_constants{};
    {
        auto const outside_light = g->natural_light_level(0);
        ambient_push.inside_light =
            (outside_light > LIGHT_SOURCE_BRIGHT) ? LIGHT_AMBIENT_DIM * 0.8f : LIGHT_AMBIENT_LOW;
        ambient_push.cache_x = cache_x;
        ambient_push.cache_y = cache_y;
        ambient_push.cache_xy = cache_xy;
        ambient_push.z_count = z_count;
        ambient_push.overmap_depth = OVERMAP_DEPTH;
        ambient_push.angled_sunlight_shadows = p.angled_sunlight_shadows ? 1u : 0u;
        ambient_push.direct_sunlight = p.direct_sunlight ? 1u : 0u;
        ambient_push.sun_dx_per_z = p.sun_dx_per_z;
        ambient_push.sun_dy_per_z = p.sun_dy_per_z;
        for (int zi = 0; zi < OVERMAP_LAYERS; ++zi) {
            ambient_push.natural_light[zi / 4][zi % 4] = g->natural_light_level(zi - OVERMAP_DEPTH);
        }
    }

    // ── Ensure GPU buffers ───────────────────────────────────────────────────
    auto const volume_tiles = static_cast<Uint32>(z_count * cache_xy);
    auto const t_bytes = static_cast<Uint32>(volume_tiles * sizeof(float));
    auto const f_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const vf_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const camera_bytes = t_bytes;
    auto const source_map_bytes = t_bytes;
    // lm and seen buffers cover all z_count * cache_xy tiles.
    auto const out_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const lm_level_bytes = static_cast<Uint32>(cache_xy * sizeof(uint32_t));
    auto const lm_download_bytes =
        static_cast<Uint32>(p.dirty_levels->size()) * lm_level_bytes;
    auto const src_bytes =
        num_src > 0 ? static_cast<Uint32>(num_src * sizeof(GpuLightSource))
                    : static_cast<Uint32>(sizeof(GpuLightSource)); // dummy slot

    auto const rebuild_seen = p.rebuild_seen_cache || !s_lighting_resources.seen_valid;
    auto const seen_zero_bytes = t_bytes;
    auto upload_total = src_bytes;
    if (input_uploads.transparency) {
        upload_total += t_bytes;
    }
    if (input_uploads.floor) {
        upload_total += f_bytes;
    }
    if (input_uploads.vehicle_floor) {
        upload_total += vf_bytes;
    }
    if (rebuild_seen) {
        upload_total += seen_zero_bytes + seen_zero_bytes;
    }
    auto const visibility_upload_total = camera_bytes + source_map_bytes;

    {
        ZoneScopedN( "gpu_lm_ensure_resources" );
        if (!ensure_lighting_resources(device, {
                .transparency_bytes = t_bytes,
                .floor_bytes = f_bytes,
                .vehicle_floor_bytes = vf_bytes,
                .camera_bytes = camera_bytes,
                .source_map_bytes = source_map_bytes,
                .source_bytes = src_bytes,
                .output_bytes = out_bytes,
                .lm_download_bytes = lm_download_bytes,
                .visibility_download_bytes = out_bytes,
                .upload_bytes = upload_total,
                .visibility_upload_bytes = visibility_upload_total,
            })) {
            return false;
        }
    }

    auto* const t_buf = s_lighting_resources.transparency.buffer;
    auto* const f_buf = s_lighting_resources.floor.buffer;
    auto* const vf_buf = s_lighting_resources.vehicle_floor.buffer;
    auto* const src_buf = s_lighting_resources.sources.buffer;
    auto* const lm_buf = s_lighting_resources.lm.buffer;
    auto* const seen_raw_buf = s_lighting_resources.seen_raw.buffer;
    auto* const seen_buf = s_lighting_resources.seen.buffer;
    auto* const upload_tbuf = s_lighting_resources.upload.buffer;
    auto* const lm_dl_tbuf = s_lighting_resources.lm_download.buffer;
    auto* const seen_dl_tbuf = s_lighting_resources.seen_download.buffer;

    // ── Upload: single transfer buffer covering all inputs ───────────────────
    {
        ZoneScopedN( "gpu_lm_stage_upload" );
        auto* const mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, upload_tbuf, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: upload transfer buffer map failed: " << SDL_GetError();
            return false;
        }
        auto off = Uint32{0};
        if (input_uploads.transparency) {
            std::memcpy(mapped + off, transparency_cpu.data(), t_bytes);
            off += t_bytes;
        }
        if (input_uploads.floor) {
            std::memcpy(mapped + off, floor_cpu.data(), f_bytes);
            off += f_bytes;
        }
        if (input_uploads.vehicle_floor) {
            std::memcpy(mapped + off, vehicle_floor_cpu.data(), vf_bytes);
            off += vf_bytes;
        }
        if (num_src > 0) {
            std::memcpy(mapped + off, sources.data(), src_bytes);
        } else {
            std::memset(mapped + off, 0, src_bytes);
        }
        off += src_bytes;
        if (rebuild_seen) {
            std::memset(mapped + off, 0, seen_zero_bytes + seen_zero_bytes);
        }
        SDL_UnmapGPUTransferBuffer(device, upload_tbuf);
    }

    // ── Build command buffer ─────────────────────────────────────────────────
    s_lighting_resources.lighting_outputs_valid = false;
    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: command buffer acquisition failed: " << SDL_GetError();
        return false;
    }

    {
        ZoneScopedN( "gpu_lm_record_commands" );

        // [Pass 1] Copy: upload all input buffers.
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            auto off = Uint32{0};

            auto upload = [&](SDL_GPUBuffer* dst, Uint32 const bytes) {
                auto const src_loc = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = upload_tbuf,
                    .offset = off,
                };
                auto const dst_reg = SDL_GPUBufferRegion{
                    .buffer = dst,
                    .offset = 0,
                    .size = bytes,
                };
                SDL_UploadToGPUBuffer(cp, &src_loc, &dst_reg, false);
                off += bytes;
            };
            if (input_uploads.transparency) {
                upload(t_buf, t_bytes);
            }
            if (input_uploads.floor) {
                upload(f_buf, f_bytes);
            }
            if (input_uploads.vehicle_floor) {
                upload(vf_buf, vf_bytes);
            }
            upload(src_buf, src_bytes);
            if (rebuild_seen) {
                upload(seen_raw_buf, seen_zero_bytes);
                upload(seen_buf, seen_zero_bytes);
            }

            SDL_EndGPUCopyPass(cp);
        }

        // [Pass 2] Compute: ambient initialisation.
        // Writes lm_all[tile] = asuint(ambient_value) for every tile.
        // Uses terrain floor_all and transparency_all to determine sky access
        // physically.  Vehicle roofs do not affect sunlight.
        {
            auto const rw_lm = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = lm_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_lm, 1);
            SDL_BindGPUComputePipeline(cp, s_ambient_pipeline);

            auto const ro_bufs = std::array<SDL_GPUBuffer*, 2>{f_buf, t_buf};
            SDL_BindGPUComputeStorageBuffers(
                cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

            SDL_PushGPUComputeUniformData(cmd, 0, &ambient_push, sizeof(ambient_push));

            auto const total_tiles = static_cast<Uint32>(z_count * cache_xy);
            SDL_DispatchGPUCompute(cp, (total_tiles + 63) / 64, 1, 1);
            SDL_EndGPUComputePass(cp);
        }

        // [Pass 3] Compute: per-source ray casting.
        // InterlockedMax(lm_all[tile], asuint(intensity)) for each source.
        if (num_src > 0) {
            auto const rw_lm = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = lm_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
            auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_lm, 1);
            SDL_BindGPUComputePipeline(cp, s_raytrace_pipeline);

            auto const ro_bufs = std::array<SDL_GPUBuffer*, 4>{t_buf, f_buf, vf_buf, src_buf};
            SDL_BindGPUComputeStorageBuffers(
                cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

            auto const raytrace_push = lm_raytrace_push_constants{
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .z_scale = Z_LEVEL_SCALE,
                .num_sources = num_src,
                .max_radius = max_radius,
                ._pad = 0u,
            };
            SDL_PushGPUComputeUniformData(cmd, 0, &raytrace_push, sizeof(raytrace_push));
            SDL_DispatchGPUCompute(cp, num_src, groups_xy, groups_xy);
            SDL_EndGPUComputePass(cp);
        }

        if (rebuild_seen) {
            auto const seen_push = lm_seen_push_constants{
                .player_x = p.player_x,
                .player_y = p.player_y,
                .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = z_count,
                .view_radius = MAX_VIEW_DISTANCE,
                .z_scale = Z_LEVEL_SCALE,
                ._pad = {},
            };
            auto const diam = static_cast<Uint32>(2 * MAX_VIEW_DISTANCE + 1);
            auto const g_seen = (diam + 7) / 8;

            // [Pass 4] Compute: raw seen_cache ray casting from player.
            {
                auto const rw_seen = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = seen_raw_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_seen, 1);
                SDL_BindGPUComputePipeline(cp, s_seen_pipeline);

                auto const ro_bufs = std::array<SDL_GPUBuffer*, 3>{t_buf, f_buf, vf_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

                SDL_PushGPUComputeUniformData(cmd, 0, &seen_push, sizeof(seen_push));
                SDL_DispatchGPUCompute(cp, g_seen, g_seen, static_cast<Uint32>(z_count));
                SDL_EndGPUComputePass(cp);
            }

            // [Pass 4b] Compute: surface/edge visibility expansion.
            {
                auto const rw_seen = SDL_GPUStorageBufferReadWriteBinding{
                    .buffer = seen_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
                auto* const cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw_seen, 1);
                SDL_BindGPUComputePipeline(cp, s_seen_walls_pipeline);

                auto const ro_bufs = std::array<SDL_GPUBuffer*, 3>{t_buf, seen_raw_buf, vf_buf};
                SDL_BindGPUComputeStorageBuffers(
                    cp, 0, ro_bufs.data(), static_cast<Uint32>(ro_bufs.size()));

                SDL_PushGPUComputeUniformData(cmd, 0, &seen_push, sizeof(seen_push));
                SDL_DispatchGPUCompute(cp, g_seen, g_seen, static_cast<Uint32>(z_count));
                SDL_EndGPUComputePass(cp);
            }
        }

        // [Pass 5] Copy: download dirty lm levels and rebuilt seen_cache results.
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);

            auto lm_download_offset = Uint32{0};
            for (int const z : *p.dirty_levels) {
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
            if (rebuild_seen) {
                auto const src_seen = SDL_GPUBufferRegion{
                    .buffer = seen_buf,
                    .offset = 0,
                    .size = t_bytes,
                };
                auto const dst_seen = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = seen_dl_tbuf,
                    .offset = 0,
                };
                SDL_DownloadFromGPUBuffer(cp, &src_seen, &dst_seen);
            }

            SDL_EndGPUCopyPass(cp);
        }
    }

    // Submit and wait for all GPU work to complete.
    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: command buffer submission failed: " << SDL_GetError();
        return false;
    }
    {
        ZoneScopedN( "gpu_lm_fence_wait" );
        SDL_WaitForGPUFences(device, true, &fence, 1);
    }
    SDL_ReleaseGPUFence(device, fence);

    // ── Download results to CPU level_cache ──────────────────────────────────
    {
        ZoneScopedN( "gpu_lm_unpack_download" );
        // lm_all stores uint (bit-reinterpretation of positive floats).
        // Copying uint bytes directly into float storage is valid since the
        // bit pattern is preserved.
        auto const* lm_mapped = static_cast<uint32_t const*>(
            SDL_MapGPUTransferBuffer(device, lm_dl_tbuf, false));
        auto const* seen_mapped = rebuild_seen
                                  ? static_cast<float const*>(
                                      SDL_MapGPUTransferBuffer(device, seen_dl_tbuf, false))
                                  : nullptr;
        if (lm_mapped == nullptr || (rebuild_seen && seen_mapped == nullptr)) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: download transfer buffer map failed: " << SDL_GetError();
            if (lm_mapped != nullptr) { SDL_UnmapGPUTransferBuffer(device, lm_dl_tbuf); }
            if (seen_mapped != nullptr) { SDL_UnmapGPUTransferBuffer(device, seen_dl_tbuf); }
            return false;
        }

        auto lm_level_index = std::size_t{0};
        for (int const z : *p.dirty_levels) {
            // get_cache_ref is the public accessor; const_cast is safe because the
            // underlying level_cache is non-const — the const qualifier is only on
            // the return type of the accessor.
            auto& lc = const_cast<level_cache&>(p.m->get_cache_ref(z));
            auto const sz = static_cast<std::size_t>(cache_xy);

            auto const* lm_src = lm_mapped + lm_level_index * cache_xy;
            // Reinterpret uint bits as float values for lm.
            std::memcpy(lc.lm.data(), lm_src, sz * sizeof(float));
            ++lm_level_index;
        }

        if (rebuild_seen) {
            std::ranges::
                for_each(std::views::iota(-OVERMAP_DEPTH, OVERMAP_HEIGHT + 1), [&](int const z) {
                auto& lc = const_cast<level_cache&>(p.m->get_cache_ref(z));
                auto const zi = z + OVERMAP_DEPTH;
                auto const sz = static_cast<std::size_t>(cache_xy);
                auto const* seen_src = seen_mapped + zi * cache_xy;
                std::ranges::copy(std::span{seen_src, sz}, lc.seen_cache.begin());
                lc.seen_cache_dirty = false;
            });
        }

        SDL_UnmapGPUTransferBuffer(device, lm_dl_tbuf);
        if (rebuild_seen) {
            SDL_UnmapGPUTransferBuffer(device, seen_dl_tbuf);
        }
    }

    commit_input_upload_plan(input_uploads);
    if (rebuild_seen) {
        s_lighting_resources.seen_valid = true;
    }
    s_lighting_resources.lighting_outputs_valid = true;
    return true;
}

auto run_gpu_visibility(SDL_GPUDevice* const device, run_gpu_visibility_params const& p) -> bool {
    ZoneScopedN( "run_gpu_visibility" );

    if (p.m == nullptr) { return false; }
    if (!ensure_visibility_pipeline(device)) { return false; }
    ensure_resource_device(device);

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
    auto const volume_tiles = static_cast<Uint32>(z_count * cache_xy);
    auto const float_volume_bytes = static_cast<Uint32>(volume_tiles * sizeof(float));
    auto const uint_volume_bytes = static_cast<Uint32>(volume_tiles * sizeof(uint32_t));
    auto const source_bytes = static_cast<Uint32>(sizeof(GpuLightSource));
    auto const visibility_upload_total = float_volume_bytes + float_volume_bytes;

    reset_input_residency_for_shape(cache_x, cache_y, z_count);
    if (!s_lighting_resources.inputs.transparency_valid ||
        !s_lighting_resources.seen_valid ||
        !s_lighting_resources.lighting_outputs_valid) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility requested before resident lighting inputs are valid";
        return false;
    }

    if (!ensure_lighting_resources(device, {
            .transparency_bytes = float_volume_bytes,
            .floor_bytes = uint_volume_bytes,
            .vehicle_floor_bytes = uint_volume_bytes,
            .camera_bytes = float_volume_bytes,
            .source_map_bytes = float_volume_bytes,
            .source_bytes = source_bytes,
            .output_bytes = uint_volume_bytes,
            .lm_download_bytes = static_cast<Uint32>(cache_xy * sizeof(uint32_t)),
            .visibility_download_bytes = uint_volume_bytes,
            .upload_bytes = source_bytes + float_volume_bytes + float_volume_bytes,
            .visibility_upload_bytes = visibility_upload_total,
        })) {
        return false;
    }

    auto const all_levels = make_all_levels(z_count);
    auto& camera_cpu = s_lighting_resources.camera_staging;
    auto& source_map_cpu = s_lighting_resources.source_map_staging;
    {
        ZoneScopedN( "gpu_visibility_pack_inputs" );
        pack_float_cache(
            *p.m, all_levels, z_count, cache_xy,
            [](level_cache const& lc) -> std::vector<float> const& { return lc.camera_cache; },
            camera_cpu);
        pack_float_cache(
            *p.m, all_levels, z_count, cache_xy,
            [](level_cache const& lc) -> std::vector<float> const& { return lc.sm; },
            source_map_cpu);
    }

    auto* const camera_buf = s_lighting_resources.camera.buffer;
    auto* const source_map_buf = s_lighting_resources.source_map.buffer;
    auto* const t_buf = s_lighting_resources.transparency.buffer;
    auto* const lm_buf = s_lighting_resources.lm.buffer;
    auto* const seen_buf = s_lighting_resources.seen.buffer;
    auto* const visibility_buf = s_lighting_resources.visibility.buffer;
    auto* const upload_tbuf = s_lighting_resources.visibility_upload.buffer;
    auto* const visibility_dl_tbuf = s_lighting_resources.visibility_download.buffer;

    {
        ZoneScopedN( "gpu_visibility_stage_upload" );
        auto* const mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, upload_tbuf, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: visibility upload transfer buffer map failed: " << SDL_GetError();
            return false;
        }
        auto off = Uint32{0};
        std::memcpy(mapped + off, camera_cpu.data(), float_volume_bytes);
        off += float_volume_bytes;
        std::memcpy(mapped + off, source_map_cpu.data(), float_volume_bytes);
        SDL_UnmapGPUTransferBuffer(device, upload_tbuf);
    }

    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility command buffer acquisition failed: " << SDL_GetError();
        return false;
    }

    {
        ZoneScopedN( "gpu_visibility_record_commands" );
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            auto off = Uint32{0};
            auto upload = [&](SDL_GPUBuffer* dst, Uint32 const bytes) {
                auto const src_loc = SDL_GPUTransferBufferLocation{
                    .transfer_buffer = upload_tbuf,
                    .offset = off,
                };
                auto const dst_reg = SDL_GPUBufferRegion{
                    .buffer = dst,
                    .offset = 0,
                    .size = bytes,
                };
                SDL_UploadToGPUBuffer(cp, &src_loc, &dst_reg, false);
                off += bytes;
            };
            upload(camera_buf, float_volume_bytes);
            upload(source_map_buf, float_volume_bytes);
            SDL_EndGPUCopyPass(cp);
        }
        {
            auto const rw_visibility = SDL_GPUStorageBufferReadWriteBinding{
                .buffer = visibility_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
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
                ._pad = {},
            };
            SDL_PushGPUComputeUniformData(cmd, 0, &visibility_push, sizeof(visibility_push));
            SDL_DispatchGPUCompute(cp, (volume_tiles + 63) / 64, 1, 1);
            SDL_EndGPUComputePass(cp);
        }
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            auto const src_visibility = SDL_GPUBufferRegion{
                .buffer = visibility_buf,
                .offset = 0,
                .size = uint_volume_bytes,
            };
            auto const dst_visibility = SDL_GPUTransferBufferLocation{
                .transfer_buffer = visibility_dl_tbuf,
                .offset = 0,
            };
            SDL_DownloadFromGPUBuffer(cp, &src_visibility, &dst_visibility);
            SDL_EndGPUCopyPass(cp);
        }
    }

    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: lm: visibility command buffer submission failed: " << SDL_GetError();
        return false;
    }
    {
        ZoneScopedN( "gpu_visibility_fence_wait" );
        SDL_WaitForGPUFences(device, true, &fence, 1);
    }
    SDL_ReleaseGPUFence(device, fence);

    {
        ZoneScopedN( "gpu_visibility_unpack_download" );
        auto const* visibility_mapped = static_cast<uint32_t const*>(
            SDL_MapGPUTransferBuffer(device, visibility_dl_tbuf, false));
        if (visibility_mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: lm: visibility download transfer buffer map failed: " << SDL_GetError();
            return false;
        }

        std::ranges::for_each(all_levels, [&](int const z) {
            auto& lc = const_cast<level_cache&>(p.m->get_cache_ref(z));
            auto const zi = z + OVERMAP_DEPTH;
            auto const sz = static_cast<std::size_t>(cache_xy);
            auto const* src = visibility_mapped + zi * cache_xy;
            std::ranges::transform(std::span{src, sz}, lc.visibility_cache.begin(),
            [](uint32_t const value) {
                return static_cast<lit_level>(value);
            });
            lc.visibility_cache_dirty = false;
        });

        SDL_UnmapGPUTransferBuffer(device, visibility_dl_tbuf);
    }

    return true;
}

auto shutdown_lm() -> void {
    auto* const device = get_device();
    if (device == nullptr) { return; }
    release_lighting_resources(device);
    if (s_ambient_pipeline != nullptr) {
        SDL_ReleaseGPUComputePipeline(device, s_ambient_pipeline);
        s_ambient_pipeline = nullptr;
    }
    if (s_raytrace_pipeline != nullptr) {
        SDL_ReleaseGPUComputePipeline(device, s_raytrace_pipeline);
        s_raytrace_pipeline = nullptr;
    }
    if (s_seen_pipeline != nullptr) {
        SDL_ReleaseGPUComputePipeline(device, s_seen_pipeline);
        s_seen_pipeline = nullptr;
    }
    if (s_seen_walls_pipeline != nullptr) {
        SDL_ReleaseGPUComputePipeline(device, s_seen_walls_pipeline);
        s_seen_walls_pipeline = nullptr;
    }
    if (s_visibility_pipeline != nullptr) {
        SDL_ReleaseGPUComputePipeline(device, s_visibility_pipeline);
        s_visibility_pipeline = nullptr;
    }
}

} // namespace cata_gpu
#endif // defined( CATA_SDL )
