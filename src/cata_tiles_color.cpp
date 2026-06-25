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
#include "omdata.h"

static constexpr RGBColor RGB_NO_COLOR = TILESET_NO_COLOR;

namespace
{

struct blend_mode_cvt {
    bool operator()( const std::string &str, tint_blend_mode &res ) const {
        res = string_to_tint_blend_mode( str );
        return true;
    }
};

template<typename T>
struct tint_helper {};

template <> struct tint_helper<item> {
    static auto get_flags( const item &t ) {
        return std::ranges::transform_view( t.get_flags(), []( auto & v ) { return v.str(); } );
    }
    static auto &get_typestr( const item &t ) { return t.typeId().str(); }
};

template<>
struct tint_helper<ter_t> {
    static auto &get_flags( const ter_t &t ) { return t.get_flags(); }
    static auto &get_typestr( const ter_t &t ) { return t.id.str(); }
};

template<>
struct tint_helper<furn_t> {
    static auto &get_flags( const furn_t &t ) { return t.get_flags(); }
    static auto &get_typestr( const furn_t &t ) { return t.id.str(); }
};

template<>
struct tint_helper<vehicle_part> {
    static auto &get_flags( const vehicle_part &t ) { return t.info().get_flags(); }
    static auto &get_typestr( const vehicle_part &t ) { return t.info().get_id().str(); }
};

template<>
struct tint_helper<monster> {
    static auto &get_typestr( const monster &t ) { return t.type->id.str(); }
};

template <> struct tint_helper<bionic> {
    static auto get_flags( const bionic &t ) {
        return std::ranges::transform_view( t.id->flags, []( auto & v ) { return v.str(); } );
    }
    static auto &get_typestr( const bionic &t ) { return t.id.str(); }
};

template<>
struct tint_helper<effect> {
    static auto &get_typestr( const effect &t ) { return t.get_id().str(); }
};

template<>
struct tint_helper<oter_t> {
    static auto &get_typestr( const oter_t &t ) { return t.id.str(); }
};

template<typename T>
color_tint_pair tint_from_tileset(
    const std::unique_ptr<tileset> &tileset_ptr,
    const T &v,
    const color_tint_pair &defVal = {} )
{
    using helper = tint_helper<T>;

    color_tint_pair tint = defVal;

    if constexpr( requires { helper::get_flags( v ); } ) {
        for( const auto &flag : helper::get_flags( v ) ) {
            if( tileset_ptr->try_get_tint( flag, tint ) ) {
                return tint;
            }
        }
    }

    if constexpr( requires { helper::get_typestr( v ); } ) {
        if( tileset_ptr->try_get_tint( helper::get_typestr( v ), tint ) ) {
            return tint;
        }
    }

    return tint;
}

color_tint_pair tint_from_data_vars( const data_vars::data_set &i, const color_tint_pair &defVal = {} )
{
    auto [bg_tint, fg_tint] = defVal;

    if( !std::ranges::any_of( i, []( auto & f ) { return f.first.starts_with( TINT_VAR_PREFIX ); } ) ) {
        return defVal;
    }

    constexpr blend_mode_cvt blendCvt{};

    auto color = RGB_NO_COLOR;
    auto scl = 0.0f;
    auto blend = tint_blend_mode::tint;

    {
        if( i.try_get( TINT_COLOR_BG_VAR_NAME, color )
            || i.try_get( TINT_COLOR_VAR_NAME, color ) ) {
            bg_tint.color = color;
        }
        if( i.try_get( TINT_MODE_BG_VAR_NAME, blend, blendCvt )
            || i.try_get( TINT_MODE_VAR_NAME, blend, blendCvt ) ) {
            bg_tint.blend_mode = blend;
        }
        if( i.try_get( TINT_SATURATION_BG_VAR_NAME, scl )
            || i.try_get( TINT_SATURATION_VAR_NAME, scl ) ) {
            bg_tint.saturation = scl;
        }
        if( i.try_get( TINT_CONTRAST_BG_VAR_NAME, scl )
            || i.try_get( TINT_CONTRAST_VAR_NAME, scl ) ) {
            bg_tint.contrast = scl;
        }
        if( i.try_get( TINT_BRIGHTNESS_BG_VAR_NAME, scl )
            || i.try_get( TINT_BRIGHTNESS_VAR_NAME, scl ) ) {
            bg_tint.brightness = scl;
        }
    }

    {
        if( i.try_get( TINT_COLOR_FG_VAR_NAME, color )
            || i.try_get( TINT_COLOR_VAR_NAME, color ) ) {
            fg_tint.color = color;
        }
        if( i.try_get( TINT_MODE_FG_VAR_NAME, blend, blendCvt )
            || i.try_get( TINT_MODE_VAR_NAME, blend, blendCvt ) ) {
            fg_tint.blend_mode = blend;
        }
        if( i.try_get( TINT_SATURATION_FG_VAR_NAME, scl )
            || i.try_get( TINT_SATURATION_VAR_NAME, scl ) ) {
            fg_tint.saturation = scl;
        }
        if( i.try_get( TINT_CONTRAST_FG_VAR_NAME, scl )
            || i.try_get( TINT_CONTRAST_VAR_NAME, scl ) ) {
            fg_tint.contrast = scl;
        }
        if( i.try_get( TINT_BRIGHTNESS_FG_VAR_NAME, scl )
            || i.try_get( TINT_BRIGHTNESS_VAR_NAME, scl ) ) {
            fg_tint.brightness = scl;
        }
    }

    return color_tint_pair{bg_tint, fg_tint};
}

} // namespace

auto cata_tiles::get_overmap_color(
    overmapbuffer &o, const tripoint_abs_omt &p ) const -> color_tint_pair
{
    const oter_t oter = o.ter( p ).obj();
    return tint_from_tileset( tileset_ptr, oter );
}

auto cata_tiles::get_terrain_color(
    const ter_t &t, const map &m, const tripoint_bub_ms &p ) const -> color_tint_pair
{
    auto tint = tint_from_tileset( tileset_ptr, t );

    const auto vars = m.ter_vars( p );
    if( vars != nullptr ) {
        return tint_from_data_vars( *vars, tint );
    }

    return tint;
}

auto cata_tiles::get_furniture_color(
    const furn_t &f, const map &m, const tripoint_bub_ms &p ) const -> color_tint_pair
{
    auto tint = tint_from_tileset( tileset_ptr, f );

    const auto vars = m.furn_vars( p );
    if( vars != nullptr ) {
        return tint_from_data_vars( *vars, tint );
    }

    return tint;
}

auto cata_tiles::get_graffiti_color(
    const map &, const tripoint_bub_ms & ) const -> color_tint_pair
{
    return color_tint_pair{};
}

auto cata_tiles::get_trap_color(
    const trap &tr, const map &, tripoint_bub_ms ) const -> color_tint_pair
{
    return tint_from_tileset( tileset_ptr, tr );
}

auto cata_tiles::get_field_color(
    const field &f, const map &, const tripoint_bub_ms & ) const -> color_tint_pair
{
    return tint_from_tileset( tileset_ptr, f );
}

auto cata_tiles::get_item_color(
    const item &i, const map &, const tripoint_bub_ms & ) const -> color_tint_pair
{
    const auto tint = tint_from_tileset( tileset_ptr, i );
    return tint_from_data_vars( i.item_vars(), tint );
}

auto cata_tiles::get_vpart_color(
    const optional_vpart_position &vp, const map &, const tripoint_bub_ms &,
    const bool use_roof ) const -> color_tint_pair
{
    color_tint_pair tint{};
    if( !vp.has_value() ) {
        return tint;
    }

    vehicle_part *vpp = nullptr;
    if( use_roof ) {
        auto &veh = vp->vehicle();
        const auto part_idx = veh.roof_at_part( vp->part_index() );
        if( part_idx != -1 ) {
            vpp = &veh.part( part_idx );
        }
    } else {
        const auto part_ref = vp.part_displayed();
        if( part_ref ) {
            vpp = &part_ref->part();
        }
    }

    if( vpp == nullptr ) {
        return tint;
    }

    tint = tint_from_tileset( tileset_ptr, *vpp );
    auto [bg, fg] = vpp->get_color();
    std::tie( tint.first.color, tint.second.color ) = std::make_pair( bg, fg );
    return tint;
}

auto cata_tiles::get_monster_color(
    const monster &mon, const map &, const tripoint_bub_ms & ) const -> color_tint_pair
{
    return tint_from_tileset( tileset_ptr, mon );
}

auto cata_tiles::get_character_color(
    const Character &, const map &, const tripoint_bub_ms & ) const -> color_tint_pair
{
    return color_tint_pair{};
}

auto cata_tiles::get_effect_color(
    const effect &eff, const Character &, const map &,
    const tripoint_bub_ms & ) const -> color_tint_pair
{
    return tint_from_tileset( tileset_ptr, eff );
}

auto cata_tiles::get_bionic_color(
    const bionic &bio, const Character &, const map &,
    const tripoint_bub_ms & ) const -> color_tint_pair
{
    return tint_from_tileset( tileset_ptr, bio );
}

auto cata_tiles::get_mutation_color(
    const mutation &mut, const Character &c, const map &,
    const tripoint_bub_ms & ) const -> color_tint_pair
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
