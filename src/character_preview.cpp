#if defined(TILES)
#include "character_preview.h"
#include "bionics.h"
#include "color.h"
#include "hsv_color.h"
#include "magic.h"
#include "messages.h"
#include "type_id.h"
#include "character.h"
#include "profession.h"
#include "sdltiles.h"
#include "output.h"
#include "cata_tiles.h"
#include "cursesport.h"
#include "game.h"
#include "avatar.h"
#include "overlay_ordering.h"
#include "effect.h"

auto termx_to_pixel_value() -> int
{
    return projected_window_width() / TERMX;
}

auto termy_to_pixel_value() -> int
{
    return projected_window_height() / TERMY;
}

// @brief adapter to get access to protected functions of cata_tiles
// exclusively for use by character_preview ui
class char_preview_adapter : public cata_tiles
{
    public:
        static char_preview_adapter *convert( cata_tiles *ct ) {
            return static_cast<char_preview_adapter *>( ct );
        }

        void display_avatar_preview_with_overlays( const avatar &ch, const point &p, bool with_clothing ) {
            std::string ent_name = ch.male ? "player_male" : "player_female";

            int height_3d = 0;
            int prev_height_3d = 0;
            if( ch.facing == FD_RIGHT ) {
                const tile_search_params tile { ent_name, C_NONE, "", corner, 0 };
                draw_from_id_string(
                    tile, tripoint( p, 0 ), std::nullopt, std::nullopt,
                    lit_level::BRIGHT, false, 0, true, height_3d );
            } else if( ch.facing == FD_LEFT ) {
                const tile_search_params tile { ent_name, C_NONE, "", corner, 4 };
                draw_from_id_string(
                    tile, tripoint( p, 0 ), std::nullopt, std::nullopt,
                    lit_level::BRIGHT, false, 0, true, height_3d );
            }

            auto get_overlay_color = [&]<typename T>( T && arg ) {
                using Decayed = std::remove_reference_t<T>;
                using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
                if constexpr( std::is_same_v<PtrBase, item> ) {
                    return get_item_color( *arg );
                } else if constexpr( std::is_same_v<PtrBase, effect> ) {
                    return get_effect_color( *arg, ch );
                } else if constexpr( std::is_same_v<PtrBase, bionic> ) {
                    return get_bionic_color( *arg, ch );
                } else if constexpr( std::is_same_v<PtrBase, mutation> ) {
                    return get_mutation_color( *arg, ch );
                } else {
                    return color_tint_pair{ std::nullopt, std::nullopt };
                }
            };

            auto should_override = [&]<typename T>( T && arg ) {
                auto check = [&]( const mutation & mut ) {
                    mutation_branch branch = mut.first.obj();
                    for( const std::string &mut_type : branch.types ) {
                        auto controller = tileset_ptr->get_tint_controller( mut_type );
                        if( controller.first.empty() ) {
                            continue;
                        }
                        return controller.second;
                    }
                    return false;
                };
                using Decayed = std::remove_reference_t<T>;
                using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
                if constexpr( std::is_same_v<PtrBase, mutation> ) {
                    return check( *arg );
                }
                return false;
            };

            auto is_hair_style = [&]<typename T>( T && arg ) {
                auto check = [&]( const mutation & mut ) {
                    if( mut.first.obj().types.contains( "hair_style" ) ) {
                        return true;
                    }
                    return false;
                };
                using Decayed = std::remove_reference_t<T>;
                using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
                if constexpr( std::is_same_v<PtrBase, mutation> ) {
                    return check( *arg );
                }
                return false;
            };

            auto result = get_overlay_ids( ch, with_clothing );
            for( const auto &[overlay_id, entry] : result.overlays ) {
                tint_config overlay_bg_color = std::nullopt;
                tint_config overlay_fg_color = std::nullopt;
                std::string draw_id = overlay_id;
                bool found = false;

                if( !std::visit( should_override, entry ) ) {
                    // Legacy hair color injection: try to find a tile with the hair color in the name
                    if( std::visit( is_hair_style, entry ) ) {
                        for( const trait_id &other_mut : ch.get_mutations() ) {
                            if( !other_mut.obj().types.contains( "hair_color" ) ) {
                                continue;
                            }
                            const std::string color_id = other_mut.str();
                            if( draw_id.find( color_id ) != std::string::npos ) {
                                break;
                            }
                            const size_t hair_pos = draw_id.find( "hair_" );
                            if( hair_pos == std::string::npos ) {
                                continue;
                            }
                            const std::string prefix = draw_id.substr( 0, hair_pos );
                            std::string suffix = draw_id.substr( hair_pos );
                            suffix = suffix.substr( suffix.find( '_' ) );
                            const std::string new_id = prefix + color_id + suffix;
                            // draw_id is set to the resolved tile ID if found
                            found = find_overlay_looks_like( ch.male, new_id, draw_id );
                            break;
                        }
                    }
                }

                if( !found ) {
                    auto pair = std::visit( get_overlay_color, entry );
                    overlay_bg_color = pair.first;
                    overlay_fg_color = pair.second;
                    found = find_overlay_looks_like( ch.male, overlay_id, draw_id );
                }

                if( found ) {
                    int overlay_height_3d = prev_height_3d;
                    if( ch.facing == FD_RIGHT ) {
                        const tile_search_params tile { draw_id, C_NONE, "", corner, /*rota*/ 0 };
                        draw_from_id_string(
                            tile, tripoint( p, 0 ), overlay_bg_color, overlay_fg_color,
                            lit_level::BRIGHT, false, 0, true, overlay_height_3d );
                    } else if( ch.facing == FD_LEFT ) {
                        const tile_search_params tile { draw_id, C_NONE, "", corner, /*rota*/ 4 };
                        draw_from_id_string(
                            tile, tripoint( p, 0 ), overlay_bg_color, overlay_fg_color,
                            lit_level::BRIGHT, false, 0, true, overlay_height_3d );
                    }
                    height_3d = std::max( height_3d, overlay_height_3d );
                }
            }
        }
    private:
        using overlay_entry = Character::overlay_entry;
        struct overlay_result {
            std::vector<overlay_entry> overlays;
            avatar temp_avatar;
        };

        overlay_result get_overlay_ids( const avatar &av, bool with_clothing ) {
            overlay_result result;
            std::multimap<int, overlay_entry> mutation_sorting;

            for( const auto &[eff_type, eff_by_part] : av.get_all_effects() ) {
                const effect &eff = eff_by_part.begin()->second;
                if( !eff.is_removed() ) {
                    result.overlays.emplace_back( overlay_entry{
                        "effect_" + eff_type.str(),
                        &eff
                    } );
                }
            }

            for( const mutation &mut : av.my_mutations ) {
                if( !mut.second.show_sprite ) {
                    continue;
                }
                std::string overlay_id = ( mut.second.powered ? "active_" : "" ) + mut.first.str();
                int order = get_overlay_order_of_mutation( overlay_id );
                mutation_sorting.insert( { order, overlay_entry{ overlay_id, &mut } } );
            }

            for( const bionic_id &bio : av.prof->CBMs() ) {
                result.temp_avatar.add_bionic( bio );
            }
            for( const bionic &bio : *av.my_bionics ) {
                if( !bio.id->included ) {
                    result.temp_avatar.add_bionic( bio.id );
                }
            }
            for( const bionic &bio : *result.temp_avatar.my_bionics ) {
                if( !bio.show_sprite ) {
                    continue;
                }
                std::string overlay_id = ( bio.powered ? "active_" : "" ) + bio.id.str();
                int order = get_overlay_order_of_mutation( overlay_id );
                mutation_sorting.insert( { order, overlay_entry{ overlay_id, &bio } } );
            }

            for( auto &[order, entry] : mutation_sorting ) {
                result.overlays.emplace_back( overlay_entry{
                    "mutation_" + entry.id,
                    entry.entry
                } );
            }

            if( with_clothing ) {
                static const flag_id json_flag_auto_wield( "auto_wield" );
                std::vector<itype_id> wielded_items;
                for( const auto &it : av.prof->items( av.male, av.get_mutations() ) ) {
                    if( it->has_flag( json_flag_auto_wield ) ) {
                        wielded_items.push_back( it->typeId() );
                    } else if( it->is_armor() && av.can_wear( *it ).success() ) {
                        result.temp_avatar.wear_item( item::spawn( *std::move( it ) ), false );
                    }
                }
                for( const item * const &worn_item : result.temp_avatar.worn ) {
                    result.overlays.emplace_back( overlay_entry{
                        "worn_" + worn_item->typeId().str(),
                        worn_item
                    } );
                }
                for( const itype_id &wielded : wielded_items ) {
                    result.overlays.emplace_back( overlay_entry{
                        "wielded_" + wielded.str(),
                        std::monostate{}
                    } );
                }
            }
            return result;
        }
};

void character_preview_window::init( Character *character )
{
    this->character = character;
}


void character_preview_window::prepare( const int nlines, const int ncols,
                                        const Orientation *orientation, const int hide_below_ncols )
{
    zoom = DEFAULT_ZOOM;
    tilecontext->set_draw_scale( zoom );
    termx_pixels = termx_to_pixel_value();
    termy_pixels = termy_to_pixel_value();
    this->hide_below_ncols = hide_below_ncols;

    // Trying to ensure that tile will fit in border
    const int win_width = ncols * termx_pixels;
    const int win_height = nlines * termy_pixels;
    int t_width = tilecontext->get_tile_width();
    int t_height = tilecontext->get_tile_height();
    while( zoom != MIN_ZOOM && ( win_width < t_width || win_height < t_height ) ) {
        zoom_out();
        t_width = tilecontext->get_tile_width();
        t_height = tilecontext->get_tile_height();
    }

    // Final size of character preview window
    const int box_ncols = t_width / termx_pixels + 4;
    const int box_nlines = t_height / termy_pixels + 3;

    // Setting window just a little bit more than a tile itself
    point start;
    switch( orientation->type ) {
        case( TOP_LEFT ):
            start = point_zero;
            break;
        case( TOP_RIGHT ):
            start = point{TERMX - box_ncols, 0};
            break;
        case( BOTTOM_LEFT ):
            start = point{0, TERMY - box_nlines};
            break;
        case( BOTTOM_RIGHT ):
            start = point{TERMX - box_ncols, TERMY - box_nlines};
            break;
    }

    start.x += orientation->margin.left - orientation->margin.right;
    start.y += orientation->margin.top - orientation->margin.bottom;
    w_preview = catacurses::newwin( box_nlines, box_ncols, start );
    ncols_width = box_ncols;
    nlines_width = box_nlines;
    pos = start;
}

auto character_preview_window::calc_character_pos() const -> point
{
    const int t_width = tilecontext->get_tile_width();
    const int t_height = tilecontext->get_tile_height();
    return point(
               pos.x * termx_pixels + ncols_width * termx_pixels / 2 - t_width / 2,
               pos.y * termy_pixels + nlines_width * termy_pixels / 2 - t_height / 2
           );
}

void character_preview_window::zoom_in()
{
    zoom = zoom * 2 % ( MAX_ZOOM * 2 );
    if( zoom == 0 ) {
        zoom = MIN_ZOOM;
    }
    tilecontext->set_draw_scale( zoom );
}

void character_preview_window::zoom_out()
{
    zoom = zoom / 2;
    if( zoom < MIN_ZOOM ) {
        zoom = MAX_ZOOM;
    }
    tilecontext->set_draw_scale( zoom );
}

void character_preview_window::toggle_clothes()
{
    show_clothes = !show_clothes;
}

void character_preview_window::display() const
{
    // If device width is too small - ignore display
    if( TERMX - ncols_width < hide_below_ncols ) {
        return;
    }

    // Drawing UI across character tile
    werase( w_preview );
    draw_border( w_preview, BORDER_COLOR, _( "CHARACTER PREVIEW" ), BORDER_COLOR );
    wnoutrefresh( w_preview );

    // Drawing character itself
    const point pos = calc_character_pos();
    // tilecontext->display_character( *character, pos );
    char_preview_adapter::convert( &*tilecontext )->display_avatar_preview_with_overlays( *
            ( character->as_avatar() ), pos, show_clothes );
}

void character_preview_window::clear() const
{
    Messages::clear_messages();
    tilecontext->set_draw_scale( DEFAULT_TILESET_ZOOM );
}

auto character_preview_window::clothes_showing() const -> bool
{
    return !show_clothes;
}

#endif // TILES
