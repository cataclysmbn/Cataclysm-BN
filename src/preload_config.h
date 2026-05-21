#pragma once
#include <string_view>

namespace preload_config
{

enum class compute_accel : int {
    auto_select,
    off,
    force,
};

auto load()  -> void;
auto save()  -> void;

auto get_compute_accel()                -> compute_accel;
auto set_compute_accel( compute_accel ) -> void;

auto get_gpu_backend_override()                   -> std::string_view;
auto set_gpu_backend_override( std::string_view ) -> void;

auto compute_accel_from_string( std::string_view ) -> compute_accel;
auto compute_accel_to_string( compute_accel )      -> std::string_view;

} // namespace preload_config
