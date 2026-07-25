#if defined(CATA_SDL)
#    include "gpu_transparency.h"

#    include "coordinates.h"
#    include "debug.h"
#    include "field.h"
#    include "gpu_platform.h"
#    include "map.h"
#    include "mapbuffer.h"
#    include "mapdata.h"
#    include "path_info.h"
#    include "profile.h"
#    include "submap.h"

#    include <SDL3/SDL_gpu.h>
#    include <algorithm>
#    include <array>
#    include <cmath>
#    include <cstring>
#    include <fstream>
#    include <ranges>
#    include <string>
#    include <string_view>
#    include <vector>

namespace cata_gpu {

namespace {

auto read_blob(std::string const& path) -> std::vector<std::byte> {
    auto ifs = std::ifstream(path, std::ios::binary | std::ios::ate);
    if (!ifs) { return {}; }
    auto const size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0);
    std::vector<std::byte> buf(size);
    ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    return buf;
}

auto preferred_shader_format(SDL_GPUShaderFormat const fmts) -> SDL_GPUShaderFormat {
    if (fmts & SDL_GPU_SHADERFORMAT_DXIL) { return SDL_GPU_SHADERFORMAT_DXIL; }
    if (fmts & SDL_GPU_SHADERFORMAT_SPIRV) { return SDL_GPU_SHADERFORMAT_SPIRV; }
    if (fmts & SDL_GPU_SHADERFORMAT_MSL) { return SDL_GPU_SHADERFORMAT_MSL; }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

auto preferred_shader_ext(SDL_GPUShaderFormat const fmts) -> std::string_view {
    if (fmts & SDL_GPU_SHADERFORMAT_DXIL) { return ".dxil"; }
    if (fmts & SDL_GPU_SHADERFORMAT_SPIRV) { return ".spv"; }
    if (fmts & SDL_GPU_SHADERFORMAT_MSL) { return ".msl"; }
    return {};
}

SDL_GPUComputePipeline* s_pipeline = nullptr;
auto* s_pipeline_device = static_cast<SDL_GPUDevice*>(nullptr);

struct gpu_buffer_slot {
    SDL_GPUBuffer* buffer = nullptr;
    Uint32 capacity = 0;
};

struct gpu_transfer_slot {
    SDL_GPUTransferBuffer* buffer = nullptr;
    Uint32 capacity = 0;
};

struct transparency_resource_cache {
    SDL_GPUDevice* device = nullptr;
    gpu_buffer_slot submap;
    gpu_buffer_slot ter_lut;
    gpu_buffer_slot furn_lut;
    gpu_buffer_slot compact_output;
    gpu_buffer_slot full_output;
    gpu_transfer_slot upload;
    gpu_transfer_slot download;
    std::size_t ter_lut_signature = 0;
    std::size_t furn_lut_signature = 0;
    bool ter_lut_valid = false;
    bool furn_lut_valid = false;
};

struct ensure_buffer_params {
    SDL_GPUDevice* device = nullptr;
    gpu_buffer_slot* slot = nullptr;
    SDL_GPUBufferUsageFlags usage = 0;
    Uint32 required_bytes = 0;
    std::string_view name;
};

struct ensure_transfer_params {
    SDL_GPUDevice* device = nullptr;
    gpu_transfer_slot* slot = nullptr;
    SDL_GPUTransferBufferUsage usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    Uint32 required_bytes = 0;
    std::string_view name;
};

auto s_resources = transparency_resource_cache{};

auto mix_signature(std::size_t& seed, std::size_t const value) -> void {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

auto lut_signature(std::vector<uint32_t> const& values) -> std::size_t {
    auto seed = values.size();
    std::ranges::for_each(values, [&seed](uint32_t const value) {
        mix_signature(seed, static_cast<std::size_t>(value));
    });
    return seed;
}

auto release_buffer_slot(SDL_GPUDevice* const device, gpu_buffer_slot& slot) -> void {
    if (slot.buffer != nullptr) { SDL_ReleaseGPUBuffer(device, slot.buffer); }
    slot = {};
}

auto release_transfer_slot(SDL_GPUDevice* const device, gpu_transfer_slot& slot) -> void {
    if (slot.buffer != nullptr) { SDL_ReleaseGPUTransferBuffer(device, slot.buffer); }
    slot = {};
}

auto release_transparency_resources(SDL_GPUDevice* const device) -> void {
    release_buffer_slot(device, s_resources.submap);
    release_buffer_slot(device, s_resources.ter_lut);
    release_buffer_slot(device, s_resources.furn_lut);
    release_buffer_slot(device, s_resources.compact_output);
    release_buffer_slot(device, s_resources.full_output);
    release_transfer_slot(device, s_resources.upload);
    release_transfer_slot(device, s_resources.download);
    s_resources = {};
}

auto ensure_resource_device(SDL_GPUDevice* const device) -> void {
    if (s_resources.device == device) { return; }
    if (s_resources.device != nullptr) {
        if (!SDL_WaitForGPUIdle(s_resources.device)) {
            DebugLog(DL::Warn, DC::Main)
                << "SDL_GPU: transparency: wait for idle during resource device switch failed: "
                << SDL_GetError();
        }
        release_transparency_resources(s_resources.device);
    }
    s_resources.device = device;
}

auto ensure_buffer(ensure_buffer_params const& p) -> bool {
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
            << "SDL_GPU: transparency failed to allocate " << p.name << " buffer ("
            << p.required_bytes << " bytes): " << SDL_GetError();
        p.slot->capacity = 0;
        return false;
    }
    p.slot->capacity = p.required_bytes;
    return true;
}

auto ensure_transfer_buffer(ensure_transfer_params const& p) -> bool {
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
            << "SDL_GPU: transparency failed to allocate " << p.name << " transfer buffer ("
            << p.required_bytes << " bytes): " << SDL_GetError();
        p.slot->capacity = 0;
        return false;
    }
    p.slot->capacity = p.required_bytes;
    return true;
}

auto release_pipeline(SDL_GPUDevice* const device) -> void {
    if (s_pipeline == nullptr) { return; }
    SDL_ReleaseGPUComputePipeline(device, s_pipeline);
    s_pipeline = nullptr;
    if (s_pipeline_device == device) { s_pipeline_device = nullptr; }
}

auto ensure_pipeline_device(SDL_GPUDevice* const device) -> bool {
    if (device == nullptr) { return false; }
    if (s_pipeline_device == device) { return true; }
    if (s_pipeline_device != nullptr) {
        if (!SDL_WaitForGPUIdle(s_pipeline_device)) {
            DebugLog(DL::Warn, DC::Main)
                << "SDL_GPU: transparency: wait for idle during pipeline device switch failed: "
                << SDL_GetError();
        }
        release_pipeline(s_pipeline_device);
    }
    s_pipeline_device = device;
    return true;
}

auto ensure_pipeline(SDL_GPUDevice* const device) -> SDL_GPUComputePipeline* {
    if (!ensure_pipeline_device(device)) { return nullptr; }
    if (s_pipeline != nullptr) { return s_pipeline; }

    auto const fmts = SDL_GetGPUShaderFormats(device);
    auto const fmt = preferred_shader_format(fmts);
    auto const ext = preferred_shader_ext(fmts);

    if (fmt == SDL_GPU_SHADERFORMAT_INVALID || ext.empty()) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: transparency: no supported shader format";
        return nullptr;
    }

    auto const path = PATH_INFO::shaders() + "transparency_compute" + std::string{ext};
    auto const blob = read_blob(path);
    if (blob.empty()) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency shader blob not found: " << path
            << " (build with shadercross to compile shaders)";
        return nullptr;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size = blob.size(),
        .code = reinterpret_cast<Uint8 const*>(blob.data()),
        .entrypoint = compute_shader_entrypoint(fmt),
        .format = fmt,
        .num_samplers = 0,
        .num_readonly_storage_textures = 0,
        .num_readonly_storage_buffers = 3, // submap_in, ter_lut, furn_lut
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers = 2, // compact output, full resident output
        .num_uniform_buffers = 1,           // push constants (slot 0)
        .threadcount_x = 12,
        .threadcount_y = 12,
        .threadcount_z = 1,
        .props = 0,
    };

    s_pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (s_pipeline == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency pipeline creation failed: " << SDL_GetError();
    }
    return s_pipeline;
}

} // namespace

// ---------------------------------------------------------------------------

auto shutdown_transparency() -> void {
    auto* const device = get_device();
    if (device == nullptr) { return; }
    if (!SDL_WaitForGPUIdle(device)) {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: transparency: wait for idle during shutdown failed: " << SDL_GetError();
    }
    release_transparency_resources(device);
    release_pipeline(device);
}

auto rebuild_transparency_luts(transparency_luts& luts) -> void {
    auto const& all_ter = ter_t::get_all();
    luts.ter_transparent.resize(all_ter.size());
    std::ranges::transform(all_ter, luts.ter_transparent.begin(), [](ter_t const& t) -> uint32_t {
        return t.transparent ? 1u : 0u;
    });

    auto const& all_furn = furn_t::get_all();
    luts.furn_transparent.resize(all_furn.size());
    std::ranges::transform(all_furn, luts.furn_transparent.begin(), [](furn_t const& f) -> uint32_t {
        return f.transparent ? 1u : 0u;
    });
}

auto gather_transparency_refs(map const& m, int const zlev)
    -> std::vector<transparency_submap_ref> {
    auto const& lc = m.get_cache_ref(zlev);
    auto const mapsize = lc.cache_mapsize;

    auto refs = std::vector<transparency_submap_ref>{};
    refs.reserve(static_cast<std::size_t>(mapsize * mapsize));

    for (int smx = 0; smx < mapsize; ++smx) {
        for (int smy = 0; smy < mapsize; ++smy) {
            auto const sm_pos = tripoint_bub_sm{smx, smy, zlev};
            auto* const sm = m.get_mapbuffer().lookup_submap_in_memory(map_local_to_abs(m, sm_pos));
            if (sm == nullptr) { continue; }
            auto const sm_offset = project_to<coords::ms>(sm_pos);
            refs.push_back({sm, sm_offset.x(), sm_offset.y()});
        }
    }
    return refs;
}

auto prepare_transparency_inputs(
    std::span<transparency_submap_ref const> refs, std::vector<transparency_submap_in>& out)
    -> void {
    out.clear();
    out.reserve(refs.size());

    for (auto const& ref : refs) {
        auto const* const sm = ref.sm;
        auto& rec = out.emplace_back();

        rec.cache_offset_x = ref.offset_x;
        rec.cache_offset_y = ref.offset_y;
        rec.output_offset = ref.output_offset;

        for (const auto p : submap_tiles()) {
            auto const tile = static_cast<std::size_t>(p.x() * SEEY + p.y());

            rec.ter_ids[tile] = static_cast<uint32_t>(sm->get_ter(p).to_i());
            rec.furn_ids[tile] = static_cast<uint32_t>(sm->get_furn(p).to_i());
            rec.outside_flags[tile] = sm->outside_cache[p.x()][p.y()] ? 1u : 0u;

            auto opacity = 1.0f;
            for (auto const& [ftype, fentry] : sm->get_field(p)) {
                if (!ftype.is_valid()) { break; }
                if (!fentry.is_transparent()) { opacity *= fentry.translucency(); }
            }
            rec.field_opacity[tile] = opacity;
        }
    }
}

auto dispatch_transparency(dispatch_transparency_params const& p) -> bool {
    ZoneScopedN("dispatch_transparency");
    if (p.device == nullptr || p.luts == nullptr || p.submaps == nullptr || p.submaps->empty()
        || p.cache_size <= 0 || p.out_buffer == nullptr) {
        return false;
    }

    auto* const device = p.device;
    ensure_resource_device(device);
    auto const& luts = *p.luts;
    auto const& submaps = *p.submaps;
    auto const use_external_output = p.output.buffer != nullptr;
    auto const output_offset = use_external_output ? p.output.output_offset : 0u;
    auto push = p.push;
    push.output_offset = output_offset;
    auto* const pipeline = ensure_pipeline(device);
    if (pipeline == nullptr) { return false; }

    auto const submap_bytes = static_cast<Uint32>(submaps.size() * sizeof(transparency_submap_in));
    auto const ter_lut_bytes = static_cast<Uint32>(luts.ter_transparent.size() * sizeof(uint32_t));
    auto const fur_lut_bytes = static_cast<Uint32>(luts.furn_transparent.size() * sizeof(uint32_t));
    auto const compact_output_bytes = static_cast<Uint32>(
        submaps.size() * SEEX * SEEY * sizeof(float));
    auto const full_output_bytes = static_cast<Uint32>(p.cache_size * sizeof(float));

    if (ter_lut_bytes == 0 || fur_lut_bytes == 0) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency LUTs are empty — call "
               "rebuild_transparency_luts first";
        return false;
    }

    auto const read_usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    auto const write_usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    auto const old_ter_lut_buf = s_resources.ter_lut.buffer;
    auto const old_furn_lut_buf = s_resources.furn_lut.buffer;
    auto const full_output_required_bytes =
        use_external_output ? Uint32{1} : std::max(full_output_bytes, Uint32{1});
    if (!ensure_buffer({
            .device = device,
            .slot = &s_resources.submap,
            .usage = read_usage,
            .required_bytes = submap_bytes,
            .name = "submap",
        })) {
        return false;
    }
    if (!ensure_buffer({
            .device = device,
            .slot = &s_resources.ter_lut,
            .usage = read_usage,
            .required_bytes = ter_lut_bytes,
            .name = "terrain_lut",
        })) {
        return false;
    }
    if (old_ter_lut_buf != s_resources.ter_lut.buffer) { s_resources.ter_lut_valid = false; }
    if (!ensure_buffer({
            .device = device,
            .slot = &s_resources.furn_lut,
            .usage = read_usage,
            .required_bytes = fur_lut_bytes,
            .name = "furniture_lut",
        })) {
        return false;
    }
    if (old_furn_lut_buf != s_resources.furn_lut.buffer) { s_resources.furn_lut_valid = false; }
    if (!ensure_buffer({
            .device = device,
            .slot = &s_resources.compact_output,
            .usage = write_usage,
            .required_bytes = compact_output_bytes,
            .name = "compact_output",
        })) {
        return false;
    }
    if (!use_external_output
        && !ensure_buffer({
            .device = device,
            .slot = &s_resources.full_output,
            .usage = write_usage,
            .required_bytes = full_output_required_bytes,
            .name = "full_output",
        })) {
        return false;
    }
    if (!ensure_transfer_buffer({
            .device = device,
            .slot = &s_resources.download,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .required_bytes = compact_output_bytes,
            .name = "download",
        })) {
        return false;
    }

    auto const ter_lut_signature = lut_signature(luts.ter_transparent);
    auto const furn_lut_signature = lut_signature(luts.furn_transparent);
    auto const upload_ter_lut =
        !s_resources.ter_lut_valid || s_resources.ter_lut_signature != ter_lut_signature;
    auto const upload_furn_lut =
        !s_resources.furn_lut_valid || s_resources.furn_lut_signature != furn_lut_signature;
    auto upload_bytes = submap_bytes;
    upload_bytes += upload_ter_lut ? ter_lut_bytes : Uint32{0};
    upload_bytes += upload_furn_lut ? fur_lut_bytes : Uint32{0};
    if (!ensure_transfer_buffer({
            .device = device,
            .slot = &s_resources.upload,
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .required_bytes = std::max(upload_bytes, Uint32{1}),
            .name = "upload",
        })) {
        return false;
    }

    auto* const submap_buf = s_resources.submap.buffer;
    auto* const ter_lut_buf = s_resources.ter_lut.buffer;
    auto* const fur_lut_buf = s_resources.furn_lut.buffer;
    auto* const compact_output_buf = s_resources.compact_output.buffer;
    auto* const full_output_buf =
        use_external_output ? p.output.buffer : s_resources.full_output.buffer;
    auto* const upload_tbuf = s_resources.upload.buffer;
    auto* const download_tbuf = s_resources.download.buffer;
    if (full_output_buf == nullptr) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: transparency missing full output buffer";
        return false;
    }

    // --- Map upload buffer and copy input data ---
    {
        ZoneScopedN("gpu_transparency_stage_upload");
        auto* const mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, upload_tbuf, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: transparency upload transfer map failed: " << SDL_GetError();
            return false;
        }
        auto offset = Uint32{0};
        std::memcpy(mapped + offset, submaps.data(), submap_bytes);
        offset += submap_bytes;
        if (upload_ter_lut) {
            std::memcpy(mapped + offset, luts.ter_transparent.data(), ter_lut_bytes);
            offset += ter_lut_bytes;
        }
        if (upload_furn_lut) {
            std::memcpy(mapped + offset, luts.furn_transparent.data(), fur_lut_bytes);
        }
        SDL_UnmapGPUTransferBuffer(device, upload_tbuf);
    }

    auto* const cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency command buffer acquisition failed: " << SDL_GetError();
        return false;
    }

    {
        ZoneScopedN("gpu_transparency_record_commands");
        // --- Copy pass: upload all inputs ---
        auto* const cp = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTransferBufferLocation const submap_src{.transfer_buffer = upload_tbuf, .offset = 0};
        SDL_GPUBufferRegion const
            submap_dst{.buffer = submap_buf, .offset = 0, .size = submap_bytes};
        SDL_UploadToGPUBuffer(cp, &submap_src, &submap_dst, false);

        auto offset = submap_bytes;
        if (upload_ter_lut) {
            SDL_GPUTransferBufferLocation const
                ter_src{.transfer_buffer = upload_tbuf, .offset = offset};
            SDL_GPUBufferRegion const
                ter_dst{.buffer = ter_lut_buf, .offset = 0, .size = ter_lut_bytes};
            SDL_UploadToGPUBuffer(cp, &ter_src, &ter_dst, false);
            offset += ter_lut_bytes;
        }
        if (upload_furn_lut) {
            SDL_GPUTransferBufferLocation const
                fur_src{.transfer_buffer = upload_tbuf, .offset = offset};
            SDL_GPUBufferRegion const
                fur_dst{.buffer = fur_lut_buf, .offset = 0, .size = fur_lut_bytes};
            SDL_UploadToGPUBuffer(cp, &fur_src, &fur_dst, false);
        }

        SDL_EndGPUCopyPass(cp);

        // --- Compute pass ---
        {
            auto const rw_bindings = std::array<SDL_GPUStorageBufferReadWriteBinding, 2>{{
                {
                    .buffer = compact_output_buf,
                    .cycle = false,
                    .padding1 = 0,
                    .padding2 = 0,
                    .padding3 = 0,
                },
                {
                    .buffer = full_output_buf,
                    .cycle = false,
                    .padding1 = 0,
                    .padding2 = 0,
                    .padding3 = 0,
                },
            }};
            auto* const cp = SDL_BeginGPUComputePass(
                cmd, nullptr, 0, rw_bindings.data(), static_cast<Uint32>(rw_bindings.size()));
            SDL_BindGPUComputePipeline(cp, pipeline);

            SDL_GPUBuffer* const ro_bufs[3] = {submap_buf, ter_lut_buf, fur_lut_buf};
            SDL_BindGPUComputeStorageBuffers(cp, 0, ro_bufs, 3);

            // Uniform slot 0 → cbuffer Constants (register b0, space2) in HLSL.
            SDL_PushGPUComputeUniformData(cmd, 0, &push, sizeof(push));

            SDL_DispatchGPUCompute(cp, static_cast<Uint32>(submaps.size()), 1, 1);
            SDL_EndGPUComputePass(cp);
        }

        // --- Copy pass: download output ---
        {
            auto* const cp = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUBufferRegion const
                output_src{.buffer = compact_output_buf, .offset = 0, .size = compact_output_bytes};
            SDL_GPUTransferBufferLocation const
                output_dst{.transfer_buffer = download_tbuf, .offset = 0};
            SDL_DownloadFromGPUBuffer(cp, &output_src, &output_dst);
            SDL_EndGPUCopyPass(cp);
        }
    }

    // --- Submit and wait ---
    auto* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency command buffer submission failed: " << SDL_GetError();
        return false;
    }
    auto wait_succeeded = true;
    {
        ZoneScopedN("gpu_transparency_fence_wait");
        wait_succeeded = SDL_WaitForGPUFences(device, true, &fence, 1);
    }
    SDL_ReleaseGPUFence(device, fence);
    if (!wait_succeeded) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency fence wait failed: " << SDL_GetError();
        return false;
    }
    if (upload_ter_lut) {
        s_resources.ter_lut_signature = ter_lut_signature;
        s_resources.ter_lut_valid = true;
    }
    if (upload_furn_lut) {
        s_resources.furn_lut_signature = furn_lut_signature;
        s_resources.furn_lut_valid = true;
    }

    // --- Map download buffer and copy results out ---
    auto const compact_output_count = submaps.size() * static_cast<std::size_t>(SEEX * SEEY);
    p.out_buffer->resize(compact_output_count);
    {
        ZoneScopedN("gpu_transparency_unpack_download");
        auto const* const mapped = static_cast<float const*>(
            SDL_MapGPUTransferBuffer(device, download_tbuf, false));
        if (mapped == nullptr) {
            DebugLog(DL::Error, DC::Main)
                << "SDL_GPU: transparency download transfer map failed: " << SDL_GetError();
            return false;
        }
        std::ranges::copy(std::span{mapped, compact_output_count}, p.out_buffer->begin());
        SDL_UnmapGPUTransferBuffer(device, download_tbuf);
    }

    return true;
}

#    if defined(CATA_GPU_VERIFY)

auto verify_transparency_against_cpu(map const& m, int const zlev, float const sight_penalty)
    -> void {
    auto* const device = get_device();
    if (device == nullptr) {
        DebugLog(DL::Warn, DC::Main) << "SDL_GPU: transparency verify skipped — no device";
        return;
    }

    auto const& lc = m.get_cache_ref(zlev);
    auto const cache_size = lc.cache_x * lc.cache_y;

    // LUTs are stable for the lifetime of a loaded world; rebuild on first call.
    static auto s_luts = transparency_luts{};
    static auto s_luts_valid = false;
    if (!s_luts_valid) {
        rebuild_transparency_luts(s_luts);
        s_luts_valid = true;
    }

    static auto s_inputs = std::vector<transparency_submap_in>{};
    auto const refs = gather_transparency_refs(m, zlev);
    prepare_transparency_inputs(refs, s_inputs);

    auto const push = transparency_push_constants{
        .sight_penalty = sight_penalty,
        .cache_y = lc.cache_y,
        .num_submaps = static_cast<uint32_t>(s_inputs.size()),
        .output_offset = 0,
    };

    static auto s_gpu_result = std::vector<float>{};
    const bool dispatched = dispatch_transparency({
        .device = device,
        .luts = &s_luts,
        .submaps = &s_inputs,
        .push = push,
        .cache_size = cache_size,
        .out_buffer = &s_gpu_result,
        .output = {},
    });

    if (!dispatched || s_gpu_result.empty()) {
        DebugLog(DL::Error, DC::Main) << "SDL_GPU: transparency verify: dispatch returned no data";
        return;
    }
    auto const expected_compact_result_size = refs.size() * static_cast<std::size_t>(SEEX * SEEY);
    if (s_gpu_result.size() != expected_compact_result_size) {
        DebugLog(DL::Error, DC::Main)
            << "SDL_GPU: transparency verify: dispatch returned " << s_gpu_result.size()
            << " compact values, expected " << expected_compact_result_size;
        return;
    }

    // Compare GPU output against each submap's local transparency_cache[][],
    // which is the direct output of rebuild_transparency_cache.  The flat
    // level cache (lc.transparency_cache) is NOT used here because it carries
    // stale values for non-dirty submaps when rebuild_all=false — those skipped
    // submaps haven't been rewritten to the flat cache, but the per-submap
    // arrays always reflect the last actual computation for that submap's terrain.
    static constexpr float threshold = 1e-5f;
    auto diff_count = 0;
    auto ref_index = std::size_t{0};
    for (auto const& ref : refs) {
        if (diff_count >= 20) { break; }
        for (const auto p : submap_tiles()) {
            if (diff_count >= 20) { break; }
            auto const cx = ref.offset_x + p.x();
            auto const cy = ref.offset_y + p.y();
            auto const flat_idx = static_cast<std::size_t>(cx * push.cache_y + cy);
            auto const compact_idx =
                ref_index * static_cast<std::size_t>(SEEX * SEEY)
                + static_cast<std::size_t>(p.x() * SEEY + p.y());
            auto const gpu_val = s_gpu_result[compact_idx];
            auto const cpu_val = ref.sm->transparency_cache[p.x()][p.y()];
            if (std::fabs(gpu_val - cpu_val) > threshold) {
                ++diff_count;
                DebugLog(DL::Warn, DC::Main)
                    << "SDL_GPU: transparency mismatch flat_idx=" << flat_idx
                    << " compact_idx=" << compact_idx << " gpu=" << gpu_val << " cpu=" << cpu_val;
            }
        }
        ++ref_index;
    }

    if (diff_count == 0) {
        DebugLog(DL::Info, DC::Main) << "SDL_GPU: transparency verify OK (zlev=" << zlev << ")";
    } else {
        DebugLog(DL::Warn, DC::Main)
            << "SDL_GPU: transparency verify FAILED zlev=" << zlev << " mismatches=" << diff_count;
    }
}

auto verify_transparency_readback(
    SDL_GPUDevice* /*device*/, float const* /*cpu_reference*/, int /*cache_size*/) -> void {
    // Legacy no-op hook retained for CATA_GPU_VERIFY call sites.
}

#    endif // defined( CATA_GPU_VERIFY )

} // namespace cata_gpu
#endif // defined( CATA_SDL )
