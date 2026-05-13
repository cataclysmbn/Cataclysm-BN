#if defined(TILES)
#include "cata_tiles.h"

#include "map.h"
#include "monster.h"
#include "character.h"
#include "field.h"
#include "color.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

static constexpr RGBColor RGB_NO_COLOR = TILESET_NO_COLOR;

struct blend_mode_cvt {
    bool operator()( const std::string &str, tint_blend_mode &res ) const {
        res = string_to_tint_blend_mode( str );
        return true;
    }
};

static std::optional<color_tint_pair> tint_from_data_vars( const data_vars::data_set &i )
{
    if( i.contains( TINT_COLOR_VAR_NAME ) || i.contains( TINT_COLOR_FG_VAR_NAME ) ||
        i.contains( TINT_COLOR_BG_VAR_NAME ) ) {

        const auto col = i.get<RGBColor>( TINT_COLOR_VAR_NAME, RGB_NO_COLOR );
        const auto col_bg = i.get<RGBColor>( TINT_COLOR_BG_VAR_NAME, col );
        const auto col_fg = i.get<RGBColor>( TINT_COLOR_FG_VAR_NAME, col );

        tint_config bg_tint{};
        tint_config fg_tint{};

        const auto blend_mode = i.get<tint_blend_mode, blend_mode_cvt>( TINT_MODE_VAR_NAME,
                                tint_blend_mode::tint );
        const auto saturation = i.get<float>( TINT_SATURATION_VAR_NAME, 1.0f );
        const auto contrast = i.get<float>( TINT_CONTRAST_VAR_NAME, 1.0f );
        const auto brightness = i.get<float>( TINT_BRIGHTNESS_VAR_NAME, 1.0f );

        if( col_bg != RGB_NO_COLOR ) {
            bg_tint.color = col_bg;
            bg_tint.blend_mode = blend_mode;
            bg_tint.saturation = saturation;
            bg_tint.contrast = contrast;
            bg_tint.brightness = brightness;
        }

        if( col_fg != RGB_NO_COLOR ) {
            fg_tint.color = col_fg;
            fg_tint.blend_mode = blend_mode;
            fg_tint.saturation = saturation;
            fg_tint.contrast = contrast;
            fg_tint.brightness = brightness;
        }

        return std::make_pair( bg_tint, fg_tint );
    }
    return std::nullopt;
}

auto cata_tiles::get_overmap_color(
    const overmapbuffer &, const tripoint_abs_omt & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_terrain_color(
    const ter_t &, const map &m, const tripoint_bub_ms &p ) -> color_tint_pair
{
    const auto vars = m.ter_vars( p );
    if( vars != nullptr ) {
        const auto ivar_color = tint_from_data_vars( *vars );
        if( ivar_color.has_value() ) {
            return ivar_color.value();
        }
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_furniture_color(
    const furn_t &, const map &m, const tripoint_bub_ms &p ) -> color_tint_pair
{
    const auto vars = m.furn_vars( p );
    if( vars != nullptr ) {
        const auto ivar_color = tint_from_data_vars( *vars );
        if( ivar_color.has_value() ) {
            return ivar_color.value();
        }
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_graffiti_color(
    const map &, const tripoint_bub_ms & )-> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_trap_color(
    const trap &, const map &, tripoint_bub_ms ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_field_color(
    const field &, const map &, const tripoint_bub_ms & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_item_color(
    const item &i, const map &, const tripoint_bub_ms & ) -> color_tint_pair
{
    const auto ivar_color = tint_from_data_vars( i.item_vars() );
    if( ivar_color.has_value() ) {
        return ivar_color.value();
    }

    const auto &data = i.get_flags();
    for( const flag_id &flag : data ) {
        const color_tint_pair *tint = tileset_ptr->get_tint( flag.str() );
        if( tint != nullptr ) {
            return *tint;
        }
    }

    const color_tint_pair *tint = tileset_ptr->get_tint( i.typeId().str() );
    if( tint != nullptr ) {
        return *tint;
    }

    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_vpart_color(
    const optional_vpart_position &vp, const map &, const tripoint_bub_ms &,
    const bool use_roof )-> color_tint_pair
{
    if( vp.has_value() ) {
        if( use_roof ) {
            auto &veh = vp->vehicle();
            const auto part_idx = veh.roof_at_part( vp->part_index() );
            if( part_idx != -1 ) {
                auto [bg, fg] = veh.part( part_idx ).get_color();
                return {bg, fg};
            }
        }
        const auto part_ref = vp.part_displayed();
        if( part_ref ) {
            auto [bg, fg] = part_ref->part().get_color();
            return {bg, fg};
        }
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_monster_color(
    const monster &, const map &, const tripoint_bub_ms & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_character_color(
    const Character &, const map &, const tripoint_bub_ms & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_effect_color(
    const effect &eff, const Character &, const map &, const tripoint_bub_ms & ) -> color_tint_pair
{
    const color_tint_pair *tint = tileset_ptr->get_tint( eff.get_id().str() );
    if( tint != nullptr ) {
        return *tint;
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_bionic_color(
    const bionic &bio, const Character &, const map &, const tripoint_bub_ms & )-> color_tint_pair
{
    const auto &data = bio.id.obj();
    for( const flag_id &flag : data.flags ) {
        const color_tint_pair *tint = tileset_ptr->get_tint( flag.str() );
        if( tint != nullptr ) {
            return *tint;
        }
    }
    const color_tint_pair *tint = tileset_ptr->get_tint( bio.id.str() );
    if( tint != nullptr ) {
        return *tint;
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_mutation_color(
    const mutation &mut, const Character &c, const map &, const tripoint_bub_ms & )-> color_tint_pair
{
    const mutation_branch &mut_branch = mut.first.obj();
    std::string fallback_color;
    color_tint_pair res;

    auto get_tint = [&]( const std::string & ref ) -> bool {
        auto controller = tileset_ptr->get_tint_controller( ref );
        if( controller.first.empty() )
        {
            return false;
        }
        for( const trait_id &other_mut : c.get_mutations() )
        {
            if( !other_mut.obj().types.contains( controller.first ) ) {
                continue;
            }
            const color_tint_pair *tint = tileset_ptr->get_tint( other_mut.str() );
            if( tint != nullptr ) {
                res = *tint;
                return true;
            }
            fallback_color = other_mut.str();
        }
        return false;
    };

    for( const std::string &mut_type : mut_branch.types ) {
        if( get_tint( mut_type ) ) {
            break;
        }
    }
    if( fallback_color.empty() && res == color_tint_pair() ) {
        for( const trait_flag_str_id &mut_flag : mut_branch.flags ) {
            if( get_tint( mut_flag.str() ) ) {
                break;
            }
        }
    }

    if( res != color_tint_pair() ) {
        return res;
    }

    if( !fallback_color.empty() ) {
        if( fallback_color.find( '_' ) == std::string::npos ) {
            return { std::nullopt, std::nullopt };
        }
        fallback_color = fallback_color.substr( fallback_color.rfind( '_' ) + 1 );
        if( fallback_color == "blond" ) {
            fallback_color = "yellow";
        } else if( fallback_color == "gray" ) {
            fallback_color = "light_gray";
        }
        const nc_color curse_color = get_all_colors().name_to_color( "c_" + fallback_color );
        if( curse_color == c_unset ) {
            return { std::nullopt, std::nullopt };
        }
        return color_tint_pair{ curse_color, curse_color };
    }
    const color_tint_pair *tint = tileset_ptr->get_tint( mut.first.str() );
    if( tint != nullptr ) {
        return *tint;
    }
    return { std::nullopt, std::nullopt };
}

#endif
