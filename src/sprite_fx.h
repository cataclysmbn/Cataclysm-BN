#pragma once

#include <optional>
#include <string_view>
#include <vector>

/// Kind of per-sprite cosmetic deformation. Additional kinds can share the mesh path.
enum class sprite_fx_kind : int {
    none,
    sway,
};

struct sprite_fx {
    sprite_fx_kind kind = sprite_fx_kind::none;
    float amplitude_px = 0.0f;
    float phase = 0.0f;
    float time = 0.0f;
};

struct sprite_fx_rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct sprite_fx_vertex {
    float x = 0.0f;
    float y = 0.0f;
    /// Horizontal UV in sprite-local 0..1 space (not atlas space).
    float u = 0.0f;
    /// Vertical UV in sprite-local 0..1 space; 0 is the top of the sprite.
    float v = 0.0f;
};

struct sprite_fx_mesh {
    std::vector<sprite_fx_vertex> vertices;
    std::vector<int> indices;
};

struct plant_sway_weather {
    int windspeed_mph = 0;
    /// 0 = none … 4 = heavy, matching precip_class.
    int precip_rank = 0;
    bool sheltered = false;
};

struct plant_sway_params {
    int elapsed_ms = 0;
    plant_sway_weather weather{};
    int x = 0;
    int y = 0;
    bool tree = false;
    bool young = false;
    bool shrub = false;
};

/// Pixel amplitude for a plant tile. TREE wins over YOUNG over SHRUB.
auto plant_sway_amplitude( bool tree, bool young, bool shrub ) -> std::optional<float>;

/// Per-tile phase so a forest does not lockstep. Range is about 0..2π.
auto plant_sway_phase( int x, int y ) -> float;

/// Continuous sway clock from elapsed milliseconds. 4.2 rad/s is about a 1.5s cycle.
auto plant_sway_time( int elapsed_ms ) -> float;

/// Still air stays quiet; gales and heavy precip lean harder. Shelter damps both.
auto plant_sway_weather_scale( const plant_sway_weather &weather ) -> float;

/// Frame budget in ms for a Plant sway quality id ("off", "8", "30", "60").
auto plant_sway_frame_budget_ms( std::string_view quality ) -> std::optional<int>;

auto make_plant_sway_fx( const plant_sway_params &params ) -> sprite_fx;

/// Horizontal strip mesh. Top vertices shift by amplitude * sin(time + phase);
/// the base stays planted. Empty when fx.kind is none.
auto build_sway_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh;
