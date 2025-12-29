#if defined(TILES)
#include "character_preview.h"
#include "bionics.h"
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

        // This will need to stay in sync with cata_tiles::draw_entity_with_overlays
        void display_avatar_preview_with_overlays( const avatar &ch, const point &p, bool with_clothing ) {
            // ch is never an npc so we can set ent_name directly
            std::string ent_name = ch.male ? "player_male" : "player_female";

            int height_3d = 0;
            int prev_height_3d = 0;
            // depending on the toggle flip sprite left or right
            if( ch.facing == FD_RIGHT ) {
                draw_from_id_string( ent_name, C_NONE, "", tripoint( p, 0 ), corner, 0, lit_level::BRIGHT, false,
                                     height_3d, 0, true );
            } else if( ch.facing == FD_LEFT ) {
                draw_from_id_string( ent_name, C_NONE, "", tripoint( p, 0 ), corner, 4, lit_level::BRIGHT, false,
                                     height_3d, 0, true );
            }

            // next up, draw all the overlays, need to construct them locally
            std::vector<std::string> overlays = get_overlay_ids( ch, with_clothing );
            for( const std::string &overlay : overlays ) {
                std::string draw_id = overlay;
                if( find_overlay_looks_like( ch.male, overlay, draw_id ) ) {
                    int overlay_height_3d = prev_height_3d;
                    if( ch.facing == FD_RIGHT ) {
                        draw_from_id_string( draw_id, C_NONE, "", tripoint( p, 0 ), corner, /*rota*/ 0, lit_level::BRIGHT,
                                             false, overlay_height_3d, 0,
                                             true );
                    } else if( ch.facing == FD_LEFT ) {
                        draw_from_id_string( draw_id, C_NONE, "", tripoint( p, 0 ), corner, /*rota*/ 4, lit_level::BRIGHT,
                                             false, overlay_height_3d, 0,
                                             true );
                    }
                    // the tallest height-having overlay is the one that counts
                    height_3d = std::max( height_3d, overlay_height_3d );
                }
            }
        }
    private:
        // @brief This is basically a copy of Character::get_overlay_ids but builds up ids we care about
        std::vector<std::string> get_overlay_ids( const avatar &av, bool with_clothing ) {
            std::vector<std::string> rval;
            std::multimap<int, std::string> mutation_sorting;

            // first get effects
            for( const auto &eff : av.get_all_effects() ) { // only returns non-removed effects
                rval.emplace_back( "effect_" + eff.first.str() );
            }
            // then get mutations
            for( const auto &mut : av.my_mutations ) {
                std::string overlay_id = ( mut.second.powered ? "active_" : "" ) + mut.first.str();
                int order = get_overlay_order_of_mutation( overlay_id );
                mutation_sorting.insert( std::pair<int, std::string>( order, overlay_id ) );
            }

            // add any profession bionics
            // we'll use a temporary character for this and clothing so we aren't modifying the base character
            avatar t_av;
            for( const bionic_id &bio : av.prof->CBMs() ) {
                t_av.add_bionic( bio );
            }
            for( const bionic &bio : *t_av.my_bionics ) {
                std::string overlay_id = ( bio.powered ? "active_" : "" ) + bio.id.str();
                int order = get_overlay_order_of_mutation( overlay_id );
                mutation_sorting.insert( std::pair<int, std::string>( order, overlay_id ) );
            }

            for( auto &mutorder : mutation_sorting ) {
                rval.push_back( "mutation_" + mutorder.second );
            }

            // now that we have bionics applied we can see what clothing we can wear
            if( with_clothing ) {
                for( const auto &it : av.prof->items( av.male, av.get_mutations() ) ) {
                    if( it->is_armor() && av.can_wear( *it ).success() ) {
                        t_av.wear_item( item::spawn( *std::move( it ) ), false );
                    }
                }
                for( const item * const &worn_item : t_av.worn ) {
                    rval.push_back( "worn_" + worn_item->typeId().str() );
                }
            }
            return rval;
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
