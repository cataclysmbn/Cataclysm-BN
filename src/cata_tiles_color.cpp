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

static auto SDL_Color_from_string( const std::string &str ) -> SDL_Color
{
    return static_cast<SDL_Color>( str.starts_with( '#' ) ? rgb_from_hex_string(
                                       str ) : curses_color_to_RGB( color_from_string( str ) ) );
}

auto cata_tiles::get_overmap_color(
    const overmapbuffer &, const tripoint_abs_omt & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_terrain_color(
    const ter_t &/*ter*/, const map&, const tripoint& ) -> color_tint_pair
{
    // TODO: Add tint handling when terrain vars are implemented
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_furniture_color(
    const furn_t &/*furn*/, const map&, const tripoint& ) -> color_tint_pair
{
    // TODO: Add tint handling when furniture vars are implemented
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_graffiti_color(
    const map &, const tripoint & )-> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_trap_color(
    const trap &, const map &, tripoint ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_field_color(
    const field &, const map &, const tripoint & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_item_color(
    const item &i, const map &, const tripoint & ) -> color_tint_pair
{
    return get_item_color( i );
}

auto cata_tiles::get_item_color(
    const item &i ) -> color_tint_pair
{
    if ( i.has_var( "tint_bg" ) || i.has_var( "tint_fg" ) ) {
        auto bg_col = SDL_Color_from_string( i.get_var( "tint_bg" ) );
        auto fg_col = SDL_Color_from_string( i.get_var( "tint_fg" ) );

        tint_config bg_tint;
        tint_config fg_tint;

        auto blend_mode = string_to_tint_blend_mode( i.get_var( "blend_mode" ) );
        float saturation = i.has_var( "saturation" ) ? stof( i.get_var( "saturation" ) ) : 1.0f;
        float contrast = i.has_var( "contrast" ) ? stof( i.get_var( "contrast" ) ) : 1.0f;
        float brightness = i.has_var( "brightness" ) ? stof( i.get_var( "brightness" ) ) : 1.0f;

        if ( i.has_var( "tint_bg" ) ) {
            bg_tint.color = bg_col;
            if( i.has_var( "blend_mode" ) ) {
                bg_tint.blend_mode = blend_mode;
            } else if ( i.has_var( "bg_blend_mode" ) ) {
                bg_tint.blend_mode = string_to_tint_blend_mode( i.get_var( "bg_blend_mode" ) );
            }
            if( i.has_var( "saturation" ) ) {
                bg_tint.saturation = saturation;
            } else if ( i.has_var( "bg_saturation" ) ) {
                bg_tint.saturation = stof( i.get_var( "bg_saturation" ) );
            }
            if( i.has_var( "contrast" ) ) {
                bg_tint.contrast = contrast;
            } else if ( i.has_var( "bg_contrast" ) ) {
                bg_tint.contrast = stof( i.get_var( "bg_contrast" ) );
            }
            if( i.has_var( "brightness" ) ) {
                bg_tint.brightness = brightness;
            } else if ( i.has_var( "bg_brightness" ) ) {
                bg_tint.brightness = stof( i.get_var( "bg_brightness" ) );
            }
        } else { bg_tint = std::nullopt; }

        if ( i.has_var( "tint_fg" ) ) {
            fg_tint.color = fg_col;
            if( i.has_var( "blend_mode" ) ) {
                fg_tint.blend_mode = blend_mode;
            } else if ( i.has_var( "fg_blend_mode" ) ) {
                fg_tint.blend_mode = string_to_tint_blend_mode( i.get_var( "fg_blend_mode" ) );
            }
            if( i.has_var( "saturation" ) ) {
                fg_tint.saturation = saturation;
            } else if ( i.has_var( "fg_saturation" ) ) {
                fg_tint.saturation = stof( i.get_var( "fg_saturation" ) );
            }
            if( i.has_var( "contrast" ) ) {
                fg_tint.contrast = contrast;
            } else if ( i.has_var( "fg_contrast" ) ) {
                fg_tint.contrast = stof( i.get_var( "fg_contrast" ) );
            }
            if( i.has_var( "brightness" ) ) {
                fg_tint.brightness = brightness;
            } else if ( i.has_var( "fg_brightness" ) ) {
                fg_tint.brightness = stof( i.get_var( "fg_brightness" ) );
            }
        } else { fg_tint = std::nullopt; }
        return { bg_tint, fg_tint };
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
    const optional_vpart_position &vp, const map &, const tripoint & )-> color_tint_pair
{
    if( vp ) {
        const vehicle &veh = vp->vehicle();
        int veh_part = vp->part_index();
        auto part_info = veh.part_info( veh_part );
        // TODO: add tint handling once vehicle vars are implemented
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_monster_color(
    const monster &, const map &, const tripoint & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_character_color(
    const Character &, const map &, const tripoint & ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_effect_color(
    const effect &eff, const Character &c, const map &, const tripoint & ) -> color_tint_pair
{
    return get_effect_color( eff, c );
}

auto cata_tiles::get_effect_color(
    const effect &eff, const Character & ) -> color_tint_pair
{
    const color_tint_pair *tint = tileset_ptr->get_tint( eff.get_id().str() );
    if( tint != nullptr ) {
        return *tint;
    }
    return { std::nullopt, std::nullopt };
}

auto cata_tiles::get_bionic_color(
    const bionic &bio, const Character &c, const map &, const tripoint & )-> color_tint_pair
{
    return get_bionic_color( bio, c );
}

auto cata_tiles::get_bionic_color(
    const bionic &bio, const Character & )-> color_tint_pair
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
    const mutation &mut, const Character &c, const map &, const tripoint & )-> color_tint_pair
{
    return get_mutation_color( mut, c );
}

auto cata_tiles::get_mutation_color(
    const mutation &mut, const Character &c ) -> color_tint_pair
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
