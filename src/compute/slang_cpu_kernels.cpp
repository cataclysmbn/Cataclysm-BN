#include "slang_cpu_kernels.h"

#include "debug.h"

#include <array>
#include <string_view>

namespace cata_compute::slang_cpu::kernels
{

namespace
{

struct startup_probe {
    std::string_view name;
    auto( *run )() -> bool;
};

auto run_probe( startup_probe const &probe, bool &all_passed ) -> void
{
    const auto passed = probe.run();
    if( !passed ) {
        DebugLog( DL::Warn, DC::Main )
                << "Slang CPU generated kernel startup probe failed: " << probe.name;
    }
    all_passed = all_passed && passed;
}

} // namespace

auto generated_kernels_available() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    return true;
#else
    return false;
#endif
}

auto run_startup_probes() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto all_passed = true;
    static constexpr auto basic_probes = std::array<startup_probe, 13> {
        {
            { "test_compute", run_test_compute_probe },
            { "fill_uint", run_fill_uint_probe },
            { "fill_float", run_fill_float_probe },
            { "max_uint", run_max_uint_probe },
            { "shift_uint", run_shift_uint_probe },
            { "shift_float", run_shift_float_probe },
            { "clear_seen", run_clear_seen_probe },
            { "clear_seen_view", run_clear_seen_view_probe },
            { "final_visibility", run_final_visibility_probe },
            { "seen", run_seen_probe },
            { "seen_walls", run_seen_walls_probe },
            { "daylight_diffuse", run_daylight_diffuse_probe },
            { "ambient", run_ambient_probe },
        }
    };
    for( const auto &probe : basic_probes ) {
        run_probe( probe, all_passed );
    }
#if defined( CATA_SDL )
    static constexpr auto sdl_probes = std::array<startup_probe, 5> {
        {
            { "transparency", run_transparency_probe },
            { "sight_pairs", run_sight_pairs_probe },
            { "raytrace", run_raytrace_probe },
            { "color_raytrace", run_color_raytrace_probe },
            { "vehicle_optics", run_vehicle_optics_probe },
        }
    };
    for( const auto &probe : sdl_probes ) {
        run_probe( probe, all_passed );
    }
#endif
    return all_passed;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels
