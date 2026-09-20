#include "sprite_fx.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>

namespace
{

constexpr auto k_strips = 8;
constexpr auto k_tree_amp = 1.5f;
constexpr auto k_young_amp = 1.0f;
constexpr auto k_shrub_amp = 0.55f;
constexpr auto k_sway_radians_per_second = 4.2f;
constexpr auto k_still_scale = 0.35f;
constexpr auto k_gale_wind_extra = 1.4f;
constexpr auto k_heavy_precip_extra = 0.4f;
constexpr auto k_gale_windspeed = 40.0f;
constexpr auto k_shelter_still_factor = 0.5f;
constexpr auto k_shelter_gust_factor = 0.15f;

auto sway_dx( const sprite_fx &fx, const float v ) -> float
{
    const auto top_weight = 1.0f - v;
    return fx.amplitude_px * std::sin( fx.time + fx.phase ) * top_weight;
}

} // namespace

auto plant_sway_amplitude( const bool tree, const bool young, const bool shrub )
-> std::optional<float>
{
    if( tree ) {
        return k_tree_amp;
    }
    if( young ) {
        return k_young_amp;
    }
    if( shrub ) {
        return k_shrub_amp;
    }
    return std::nullopt;
}

auto plant_sway_phase( const int x, const int y ) -> float
{
    const auto seed = static_cast<unsigned>( x ) + static_cast<unsigned>( y ) * 65536u;
    return static_cast<float>( seed % 6283u ) * 0.001f;
}

auto plant_sway_time( const int elapsed_ms ) -> float
{
    return static_cast<float>( elapsed_ms ) * 0.001f * k_sway_radians_per_second;
}

auto plant_sway_weather_scale( const plant_sway_weather &weather ) -> float
{
    const auto wind_t = std::clamp( static_cast<float>( weather.windspeed_mph ) / k_gale_windspeed,
                                    0.0f, 1.0f );
    const auto precip_t = std::clamp( static_cast<float>( weather.precip_rank ) / 4.0f, 0.0f, 1.0f );
    const auto gust = wind_t * k_gale_wind_extra + precip_t * k_heavy_precip_extra;
    if( weather.sheltered ) {
        return k_still_scale * k_shelter_still_factor + gust * k_shelter_gust_factor;
    }
    return k_still_scale + gust;
}

auto plant_sway_frame_budget_ms( const std::string_view quality ) -> std::optional<int>
{
    if( quality == "8" ) {
        return 125;
    }
    if( quality == "30" ) {
        return 33;
    }
    if( quality == "60" ) {
        return 16;
    }
    return std::nullopt;
}

auto make_plant_sway_fx( const plant_sway_params &params ) -> sprite_fx
{
    const auto amp = plant_sway_amplitude( params.tree, params.young, params.shrub );
    if( !amp ) {
        return {};
    }
    return sprite_fx{
        .kind = sprite_fx_kind::sway,
        .amplitude_px = *amp * plant_sway_weather_scale( params.weather ),
        .phase = plant_sway_phase( params.x, params.y ),
        .time = plant_sway_time( params.elapsed_ms ),
    };
}

auto build_sway_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh
{
    if( fx.kind != sprite_fx_kind::sway || dest.w <= 0.0f || dest.h <= 0.0f ) {
        return {};
    }

    auto mesh = sprite_fx_mesh{};
    const auto row_count = k_strips + 1;
    mesh.vertices.reserve( static_cast<std::size_t>( row_count ) * 2 );
    mesh.indices.reserve( static_cast<std::size_t>( k_strips ) * 6 );

    for( int row = 0; row < row_count; ++row ) {
        const auto v = static_cast<float>( row ) / static_cast<float>( k_strips );
        const auto dx = sway_dx( fx, v );
        const auto y = dest.y + dest.h * v;
        mesh.vertices.push_back( sprite_fx_vertex{
            .x = dest.x + dx,
            .y = y,
            .u = 0.0f,
            .v = v,
        } );
        mesh.vertices.push_back( sprite_fx_vertex{
            .x = dest.x + dest.w + dx,
            .y = y,
            .u = 1.0f,
            .v = v,
        } );
    }

    for( int strip = 0; strip < k_strips; ++strip ) {
        const auto top_left = strip * 2;
        const auto top_right = top_left + 1;
        const auto bot_left = top_left + 2;
        const auto bot_right = top_left + 3;
        mesh.indices.push_back( top_left );
        mesh.indices.push_back( top_right );
        mesh.indices.push_back( bot_left );
        mesh.indices.push_back( top_right );
        mesh.indices.push_back( bot_right );
        mesh.indices.push_back( bot_left );
    }

    return mesh;
}
