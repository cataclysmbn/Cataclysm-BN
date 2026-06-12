#include "compute_backend.h"

#include "debug.h"
#include "preload_config.h"
#include "slang_cpu_backend.h"

#if defined(CATA_SDL)
#include "gpu_platform.h"
#endif

namespace cata_compute {

namespace {

auto s_status = backend_status{};

auto log_backend_selection() -> void {
    DebugLog(DL::Info, DC::Main) << "Compute backend selected: " << s_status.name << " ("
                                 << s_status.detail << ")";
}

auto sdl_gpu_status() -> backend_status {
#if defined(CATA_SDL)
    if (cata_gpu::get_device() != nullptr) {
        return {
            .kind = backend_kind::sdl_gpu,
            .available = true,
            .supports_lighting = true,
            .supports_visibility = true,
            .supports_transparency = true,
            .supports_sight_pairs = true,
            .name = "sdl_gpu",
            .detail = "SDL_GPU compute backend",
        };
    }
#endif
    return {};
}

auto select_slang_cpu_after_auto_fallback() -> void {
    DebugLog(DL::Info, DC::Main) << "Compute backend auto-select is falling back to Slang CPU "
                                    "after SDL_GPU startup "
                                    "validation failed";
    slang_cpu::init();
    s_status = slang_cpu::status();
    if (s_status.available && !preload_config::loaded_existing_config()) {
        preload_config::set_compute_accel(preload_config::compute_accel::cpu);
        preload_config::save();
        DebugLog(DL::Info, DC::Main)
            << "Compute backend first-launch fallback persisted as CPU "
               "shader backend";
    }
}

} // namespace

auto init() -> void {
    if (preload_config::get_compute_accel() == preload_config::compute_accel::cpu) {
        slang_cpu::init();
        s_status = slang_cpu::status();
        log_backend_selection();
        return;
    }
#if defined(CATA_SDL)
    cata_gpu::init();
    s_status = sdl_gpu_status();
    if (!s_status.available
        && preload_config::get_compute_accel() == preload_config::compute_accel::auto_select) {
        select_slang_cpu_after_auto_fallback();
    }
#else
    slang_cpu::init();
    s_status = slang_cpu::status();
#endif
    log_backend_selection();
}

auto shutdown() -> void {
#if defined(CATA_SDL)
    cata_gpu::shutdown();
#endif
    slang_cpu::shutdown();
    s_status = {};
}

auto active_backend() -> backend_status { return s_status; }

auto backend_available() -> bool { return active_backend().available; }

auto active_backend_name() -> std::string_view { return active_backend().name; }

#if defined(CATA_SDL)

namespace {

auto active_sdl_gpu_device() -> SDL_GPUDevice* { return cata_gpu::get_device(); }

auto to_sdl_lighting_params(lighting_params const& p) -> cata_gpu::run_gpu_lighting_params {
    return {
        .m = p.m,
        .dirty_levels = p.dirty_levels,
        .seen_dirty_levels = p.seen_dirty_levels,
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_zlev = p.player_zlev,
        .transparency_dirty = p.transparency_dirty,
        .transparency_dirty_levels = p.transparency_dirty_levels,
        .floor_dirty = p.floor_dirty,
        .floor_dirty_levels = p.floor_dirty_levels,
        .vehicle_floor_dirty = p.vehicle_floor_dirty,
        .vehicle_floor_dirty_levels = p.vehicle_floor_dirty_levels,
        .vehicle_obscured_dirty = p.vehicle_obscured_dirty,
        .vehicle_obscured_dirty_levels = p.vehicle_obscured_dirty_levels,
        .rebuild_seen_cache = p.rebuild_seen_cache,
        .download_seen_cache = p.download_seen_cache,
        .download_lightmap = p.download_lightmap,
        .vision_block_mask = p.vision_block_mask,
        .angled_sunlight_shadows = p.angled_sunlight_shadows,
        .direct_sunlight = p.direct_sunlight,
        .sun_dx_per_z = p.sun_dx_per_z,
        .sun_dy_per_z = p.sun_dy_per_z,
    };
}

auto to_sdl_visibility_params(visibility_params const& p) -> cata_gpu::run_gpu_visibility_params {
    return {
        .m = p.m,
        .download_levels = p.download_levels,
        .zlev = p.zlev,
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_zlev = p.player_zlev,
        .g_light_level = p.g_light_level,
        .u_clairvoyance = p.u_clairvoyance,
        .u_unimpaired_range = p.u_unimpaired_range,
        .vision_threshold = p.vision_threshold,
        .visibility_scale_factor = p.visibility_scale_factor,
        .detail_range = p.detail_range,
        .vision_block_mask = p.vision_block_mask,
        .rebuild_seen_cache = p.rebuild_seen_cache,
    };
}

auto to_sdl_begin_sight_pairs_params(begin_sight_pairs_params const& p)
    -> cata_gpu::begin_gpu_sight_pairs_params {
    return {
        .m = p.m,
        .pairs = p.pairs,
        .zlev = p.zlev,
    };
}

auto to_sdl_run_sight_pairs_params(run_sight_pairs_params const& p)
    -> cata_gpu::run_gpu_sight_pairs_params {
    return {
        .m = p.m,
        .pairs = p.pairs,
        .results = p.results,
        .zlev = p.zlev,
    };
}

} // namespace

auto resident_lighting_ready_for_visibility(resident_lighting_ready_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::resident_lighting_ready_for_visibility(p);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::resident_lighting_ready_for_visibility({
        .device = device,
        .cache_x = p.cache_x,
        .cache_y = p.cache_y,
        .z_count = p.z_count,
    });
}

auto resident_lighting_ready_for_sight_pairs(resident_sight_pair_inputs_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::resident_lighting_ready_for_sight_pairs(p);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::resident_lighting_ready_for_sight_pairs({
        .device = device,
        .m = p.m,
        .pairs = p.pairs,
        .zlev = p.zlev,
    });
}

auto prepare_lighting_transparency_output(prepare_lighting_transparency_output_params const& p)
    -> resident_transparency_output {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::prepare_lighting_transparency_output(p);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return {}; }
    auto output = cata_gpu::prepare_lighting_transparency_output({
        .device = device,
        .cache_x = p.cache_x,
        .cache_y = p.cache_y,
        .z_count = p.z_count,
        .zlev = p.zlev,
    });
    if (output.buffer == nullptr) { return {}; }
    return {
        .backend = backend_kind::sdl_gpu,
        .id = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(output.buffer)),
        .output_offset = output.output_offset,
        .sdl = output,
    };
}

auto mark_lighting_transparency_level_updated(const int zlev) -> void {
    if (active_backend().kind == backend_kind::slang_cpu) {
        slang_cpu::mark_lighting_transparency_level_updated(zlev);
        return;
    }

    cata_gpu::mark_lighting_transparency_level_updated(zlev);
}

auto lighting_transparency_level_is_valid(const int zlev) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::lighting_transparency_level_is_valid(zlev);
    }

    return cata_gpu::lighting_transparency_level_is_valid(zlev);
}

auto invalidate_lighting_transparency_levels(std::vector<int> const& levels) -> void {
    if (active_backend().kind == backend_kind::slang_cpu) {
        slang_cpu::invalidate_lighting_transparency_levels(levels);
        return;
    }

    cata_gpu::invalidate_lighting_transparency_levels(levels);
}

auto shift_lighting_resident_inputs(shift_lighting_residency_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::shift_lighting_resident_inputs(p);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::shift_lighting_resident_inputs({
        .device = device,
        .cache_x = p.cache_x,
        .cache_y = p.cache_y,
        .z_count = p.z_count,
        .shift_x_submaps = p.shift_x_submaps,
        .shift_y_submaps = p.shift_y_submaps,
    });
}

auto rebuild_transparency_luts(transparency_luts& luts) -> void {
    cata_gpu::rebuild_transparency_luts(luts);
}

auto prepare_transparency_inputs(
    std::span<transparency_submap_ref const> refs, std::vector<transparency_submap_in>& out)
    -> void {
    cata_gpu::prepare_transparency_inputs(refs, out);
}

auto dispatch_transparency(dispatch_transparency_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::dispatch_transparency(p);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::dispatch_transparency({
        .device = device,
        .luts = p.luts,
        .submaps = p.submaps,
        .push = p.push,
        .cache_size = p.cache_size,
        .out_buffer = p.out_buffer,
        .output =
            {
                .buffer = p.output.backend == backend_kind::sdl_gpu ? p.output.sdl.buffer : nullptr,
                .output_offset = p.output_offset,
            },
    });
}

#if defined(CATA_GPU_VERIFY)
auto verify_transparency_against_cpu(map const& m, const int zlev, const float sight_penalty)
    -> void {
    cata_gpu::verify_transparency_against_cpu(m, zlev, sight_penalty);
}
#endif

auto begin_lighting(lighting_params const& p) -> lighting_work {
    if (active_backend().kind == backend_kind::slang_cpu) { return slang_cpu::begin_lighting(p); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return {}; }
    auto work = cata_gpu::begin_gpu_lighting(device, to_sdl_lighting_params(p));
    return {
        .backend = backend_kind::sdl_gpu,
        .id = work.id,
        .sdl = work,
    };
}

auto finish_lighting(lighting_work const& work) -> bool {
    if (work.backend == backend_kind::slang_cpu) { return slang_cpu::finish_lighting(work); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr || work.backend != backend_kind::sdl_gpu) { return false; }
    return cata_gpu::finish_gpu_lighting(device, work.sdl);
}

auto run_lighting(lighting_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) { return slang_cpu::run_lighting(p); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::run_gpu_lighting(device, to_sdl_lighting_params(p));
}

auto begin_visibility(visibility_params const& p) -> visibility_work {
    if (active_backend().kind == backend_kind::slang_cpu) { return slang_cpu::begin_visibility(p); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return {}; }
    auto work = cata_gpu::begin_gpu_visibility(device, to_sdl_visibility_params(p));
    return {
        .backend = backend_kind::sdl_gpu,
        .id = work.id,
        .sdl = work,
    };
}

auto finish_visibility(visibility_work const& work) -> bool {
    if (work.backend == backend_kind::slang_cpu) { return slang_cpu::finish_visibility(work); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr || work.backend != backend_kind::sdl_gpu) { return false; }
    return cata_gpu::finish_gpu_visibility(device, work.sdl);
}

auto run_visibility(visibility_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) { return slang_cpu::run_visibility(p); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::run_gpu_visibility(device, to_sdl_visibility_params(p));
}

auto begin_sight_pairs(begin_sight_pairs_params const& p) -> sight_pairs_work {
    if (active_backend().kind == backend_kind::slang_cpu) {
        return slang_cpu::begin_sight_pairs(p);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return {}; }
    auto work = cata_gpu::begin_gpu_sight_pairs(device, to_sdl_begin_sight_pairs_params(p));
    return {
        .backend = backend_kind::sdl_gpu,
        .id = work.id,
        .sdl = work,
    };
}

auto finish_sight_pairs(sight_pairs_work const& work, std::vector<uint32_t>& results) -> bool {
    if (work.backend == backend_kind::slang_cpu) {
        return slang_cpu::finish_sight_pairs(work, results);
    }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr || work.backend != backend_kind::sdl_gpu) { return false; }
    return cata_gpu::finish_gpu_sight_pairs(device, work.sdl, results);
}

auto run_sight_pairs(run_sight_pairs_params const& p) -> bool {
    if (active_backend().kind == backend_kind::slang_cpu) { return slang_cpu::run_sight_pairs(p); }

    auto* const device = active_sdl_gpu_device();
    if (device == nullptr) { return false; }
    return cata_gpu::run_gpu_sight_pairs(device, to_sdl_run_sight_pairs_params(p));
}

#endif

} // namespace cata_compute
