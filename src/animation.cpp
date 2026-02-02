#include "animation.h"

#include "avatar.h"
#include "cached_options.h"
#include "character.h"
#include "cursesdef.h"
#include "enums.h"
#include "game_constants.h"
#include "game.h"
#include "line.h"
#include "map.h"
#include "monster.h"
#include "mtype.h"
#include "options.h"
#include "output.h"
#include "point.h"
#include "popup.h"
#include "posix_time.h"
#include "ranged.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "weather.h"

#if defined(TILES)
#include <memory>

#include "cata_tiles.h" // all animation functions will be pushed out to a cata_tiles function in some manner
#include "sdltiles.h"
#endif

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{

class basic_animation
{
    public:
        explicit basic_animation( const int scale ) :
            delay( static_cast<size_t>( get_option<int>( "ANIMATION_DELAY" ) ) * scale * 1000000L ) {
        }

        void popup() const {
            static_popup popup;
            popup
            .wait_message( "%s", _( "Hang on a bit…" ) )
            .on_top( true );
        }
        void draw() const {
            g->invalidate_main_ui_adaptor();
            ui_manager::redraw_invalidated();
            refresh_display();
        }
        void progress( bool draw_popup = true ) const {
            if( draw_popup ) { popup(); }
            draw();

            // NOLINTNEXTLINE(cata-no-long): timespec uses long int
            long int remain = delay;
            while( remain > 0 ) {
                // NOLINTNEXTLINE(cata-no-long): timespec uses long int
                long int do_sleep = std::min( remain, 100'000'000L );
                timespec to_sleep = timespec { 0, do_sleep };
                nanosleep( &to_sleep, nullptr );
                inp_mngr.pump_events();
                remain -= do_sleep;
            }
        }

    private:
        // NOLINTNEXTLINE(cata-no-long): timespec uses long int
        long int delay;
};

class explosion_animation : public basic_animation
{
    public:
        explosion_animation() :
            basic_animation( EXPLOSION_MULTIPLIER ) {
        }
};

class bullet_animation : public basic_animation
{
    public:
        bullet_animation() : basic_animation( 1 ) {
        }
};

class wave_animation : public basic_animation
{
    public:
        wave_animation() : basic_animation( 1 ) {
        }
};

bool is_point_visible( const tripoint &p, int margin = 0 )
{
    return g->is_in_viewport( p, margin ) && g->u.sees( p );
}

bool is_radius_visible( const tripoint &center, int radius )
{
    return is_point_visible( center, -radius );
}

bool is_layer_visible( const std::map<tripoint, explosion_tile> &layer )
{
    return std::ranges::any_of( layer,
    []( const std::pair<tripoint, explosion_tile> &element ) {
        return is_point_visible( element.first );
    } );
}

// Convert p to screen position relative to u's current position and view
tripoint relative_view_pos( const avatar &u, const tripoint &p ) noexcept
{
    return p - ( u.pos() + u.view_offset ) + point( POSX, POSY );
}

// Convert p to screen position relative to the current terrain view
tripoint relative_view_pos( const game &g, const tripoint &p ) noexcept
{
    return p - g.ter_view_p + point( POSX, POSY );
}

void draw_explosion_curses( game &g, const tripoint &center, const int r,
                            const nc_color &col )
{
    if( !is_radius_visible( center, r ) ) {
        return;
    }
    // TODO: Make it look different from above/below
    const tripoint p = relative_view_pos( g.u, center );

    explosion_animation anim;

    int frame = 0;
    shared_ptr_fast<game::draw_callback_t> explosion_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        if( r == 0 ) {
            mvwputch( g.w_terrain, point( p.y, p.x ), col, '*' );
        }

        for( int i = 1; i <= frame; ++i ) {
            // corner: top left
            mvwputch( g.w_terrain, p.xy() + point( -i, -i ), col, '/' );
            // corner: top right
            mvwputch( g.w_terrain, p.xy() + point( i, -i ), col, '\\' );
            // corner: bottom left
            mvwputch( g.w_terrain, p.xy() + point( -i, i ), col, '\\' );
            // corner: bottom right
            mvwputch( g.w_terrain, p.xy() + point( i, i ), col, '/' );
            for( int j = 1 - i; j < 0 + i; j++ ) {
                // edge: top
                mvwputch( g.w_terrain, p.xy() + point( j, -i ), col, '-' );
                // edge: bottom
                mvwputch( g.w_terrain, p.xy() + point( j, i ), col, '-' );
                // edge: left
                mvwputch( g.w_terrain, p.xy() + point( -i, j ), col, '|' );
                // edge: right
                mvwputch( g.w_terrain, p.xy() + point( i, j ), col, '|' );
            }
        }
    } );
    g.add_draw_callback( explosion_cb );

    for( frame = 1; frame <= r; ++frame ) {
        anim.progress();
    }
}

constexpr explosion_neighbors operator | ( explosion_neighbors lhs, explosion_neighbors rhs )
{
    return static_cast<explosion_neighbors>( static_cast< int >( lhs ) | static_cast< int >( rhs ) );
}

constexpr explosion_neighbors operator ^ ( explosion_neighbors lhs, explosion_neighbors rhs )
{
    return static_cast<explosion_neighbors>( static_cast< int >( lhs ) ^ static_cast< int >( rhs ) );
}

void draw_custom_explosion_curses( game &g,
                                   const std::list< std::map<tripoint, explosion_tile> > &layers )
{
    // calculate screen offset relative to player + view offset position
    const tripoint center = g.u.pos() + g.u.view_offset;
    const tripoint topleft( center.x - ( getmaxx( g.w_terrain ) / 2 ),
                            center.y - ( getmaxy( g.w_terrain ) / 2 ), 0 );

    explosion_animation anim;

    auto last_layer_it = layers.begin();
    shared_ptr_fast<game::draw_callback_t> explosion_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        for( auto it = layers.begin(); it != std::next( last_layer_it ); ++it ) {
            for( const auto &pr : *it ) {
                // update tripoint in relation to top left corner of curses window
                // mvwputch already filters out of bounds coordinates
                const tripoint p = pr.first - topleft;
                const explosion_neighbors ngh = pr.second.neighborhood;
                const nc_color col = pr.second.color;

                switch( ngh ) {
                    // '^', 'v', '<', '>'
                    case N_NORTH:
                        mvwputch( g.w_terrain, p.xy(), col, '^' );
                        break;
                    case N_SOUTH:
                        mvwputch( g.w_terrain, p.xy(), col, 'v' );
                        break;
                    case N_WEST:
                        mvwputch( g.w_terrain, p.xy(), col, '<' );
                        break;
                    case N_EAST:
                        mvwputch( g.w_terrain, p.xy(), col, '>' );
                        break;
                    // '|' and '-'
                    case N_NORTH | N_SOUTH:
                    case N_NORTH | N_SOUTH | N_WEST:
                    case N_NORTH | N_SOUTH | N_EAST:
                        mvwputch( g.w_terrain, p.xy(), col, '|' );
                        break;
                    case N_WEST | N_EAST:
                    case N_WEST | N_EAST | N_NORTH:
                    case N_WEST | N_EAST | N_SOUTH:
                        mvwputch( g.w_terrain, p.xy(), col, '-' );
                        break;
                    // '/' and '\'
                    case N_NORTH | N_WEST:
                    case N_SOUTH | N_EAST:
                        mvwputch( g.w_terrain, p.xy(), col, '/' );
                        break;
                    case N_SOUTH | N_WEST:
                    case N_NORTH | N_EAST:
                        mvwputch( g.w_terrain, p.xy(), col, '\\' );
                        break;
                    case N_NO_NEIGHBORS:
                        mvwputch( g.w_terrain, p.xy(), col, '*' );
                        break;
                    case N_WEST | N_EAST | N_NORTH | N_SOUTH:
                        break;
                }
            }
        }
    } );
    g.add_draw_callback( explosion_cb );

    for( last_layer_it = layers.begin(); last_layer_it != layers.end(); ++last_layer_it ) {
        if( is_layer_visible( *last_layer_it ) ) {
            anim.progress();
        }
    }
}

[[maybe_unused]]
auto get_bullet_dir( const std::vector<tripoint> &trajectory, size_t i ) -> direction
{
    return i == 0 && trajectory.size() > 1 ?
           direction_from( trajectory[i], trajectory[i + 1] ) :
           ( i >= 1 && i < trajectory.size() ) ?
           direction_from( trajectory[i - 1], trajectory[i] ) :
           direction::NORTH;
}

[[maybe_unused]] auto get_bullet_rotation( direction dir ) -> int
{
    switch( dir ) {
        case direction::NORTH:
            return 0;
        case direction::NORTHEAST:
            return 5;
        case direction::EAST:
            return 3;
        case direction::SOUTHEAST:
            return 8;
        case direction::SOUTH:
            return 2;
        case direction::SOUTHWEST:
            return 7;
        case direction::WEST:
            return 1;
        case direction::NORTHWEST:
            return 6;
        default:
            return 0;
    }
}

} // namespace

#if defined(TILES)
void explosion_handler::draw_explosion( const tripoint &p, const int r, const nc_color &col,
                                        const std::string &exp_name )
{
    if( test_mode ) {
        // avoid segfault from null tilecontext in tests
        return;
    }

    if( !use_tiles ) {
        draw_explosion_curses( *g, p, r, col );
        return;
    }

    if( !is_radius_visible( p, r ) ) {
        return;
    }

    explosion_animation anim;

    int i = 1;
    shared_ptr_fast<game::draw_callback_t> explosion_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        // TODO: not xpos ypos?
        tilecontext->init_explosion( p, i, exp_name );
    } );
    g->add_draw_callback( explosion_cb );

    const bool visible = is_radius_visible( p, r );
    for( i = 1; i <= r; i++ ) {
        if( visible ) {
            anim.progress();
        }
    }

    if( r > 0 ) {
        tilecontext->void_explosion();
    }
}
#else
void explosion_handler::draw_explosion( const tripoint &p, const int r, const nc_color &col,
                                        const std::string & )
{
    draw_explosion_curses( *g, p, r, col );
}
#endif

void explosion_handler::draw_custom_explosion( const tripoint &,
        const std::map<tripoint, nc_color> &all_area,
        const std::string &exp_name )
{
#if !defined(TILES)
    ( void )exp_name;
#endif
    if( test_mode ) {
        // avoid segfault from null tilecontext in tests
        return;
    }

    constexpr explosion_neighbors all_neighbors = N_NORTH | N_SOUTH | N_WEST | N_EAST;
    // We will "shell" the explosion area
    // Each phase will strip a single layer of points
    // A layer contains all points that have less than 4 neighbors in cardinal directions
    // Layers will first be generated, then drawn in inverse order

    // Start by getting rid of everything except current z-level
    std::map<tripoint, explosion_tile> neighbors;
#if defined(TILES)
    if( !use_tiles ) {
        for( const auto &pr : all_area ) {
            const tripoint relative_point = relative_view_pos( g->u, pr.first );
            if( relative_point.z == 0 ) {
                neighbors[pr.first] = explosion_tile{ N_NO_NEIGHBORS, pr.second };
            }
        }
    } else {
        // In tiles mode, the coordinates have to be absolute
        const tripoint view_center = relative_view_pos( g->u, g->u.pos() );
        for( const auto &pr : all_area ) {
            // Relative point is only used for z level check
            const tripoint relative_point = relative_view_pos( g->u, pr.first );
            if( relative_point.z == view_center.z ) {
                neighbors[pr.first] = explosion_tile{ N_NO_NEIGHBORS, pr.second };
            }
        }
    }
#else
    for( const auto &pr : all_area ) {
        const tripoint relative_point = relative_view_pos( g->u, pr.first );
        if( relative_point.z == 0 ) {
            neighbors[pr.first] = explosion_tile{ N_NO_NEIGHBORS, pr.second };
        }
    }
#endif

    // Searches for a neighbor, sets the neighborhood flag on current point and on the neighbor
    const auto set_neighbors = [&]( const tripoint & pos,
                                    explosion_neighbors & ngh,
                                    explosion_neighbors here,
    explosion_neighbors there ) {
        if( ( ngh & here ) == N_NO_NEIGHBORS ) {
            auto other = neighbors.find( pos );
            if( other != neighbors.end() ) {
                ngh = ngh | here;
                other->second.neighborhood = other->second.neighborhood | there;
            }
        }
    };

    // If the point we are about to remove has a neighbor in a given direction
    // unset that neighbor's flag that our current point is its neighbor
    const auto unset_neighbor = [&]( const tripoint & pos,
                                     const explosion_neighbors ngh,
                                     explosion_neighbors here,
    explosion_neighbors there ) {
        if( ( ngh & here ) != N_NO_NEIGHBORS ) {
            auto other = neighbors.find( pos );
            if( other != neighbors.end() ) {
                other->second.neighborhood = ( other->second.neighborhood | there ) ^ there;
            }
        }
    };

    // Find all neighborhoods
    for( auto &pr : neighbors ) {
        const tripoint &pt = pr.first;
        explosion_neighbors &ngh = pr.second.neighborhood;

        set_neighbors( pt + point_west, ngh, N_WEST, N_EAST );
        set_neighbors( pt + point_east, ngh, N_EAST, N_WEST );
        set_neighbors( pt + point_north, ngh, N_NORTH, N_SOUTH );
        set_neighbors( pt + point_south, ngh, N_SOUTH, N_NORTH );
    }

    // We need to save the layers because we will draw them in reverse order
    std::list< std::map<tripoint, explosion_tile> > layers;
    while( !neighbors.empty() ) {
        std::map<tripoint, explosion_tile> layer;
        bool changed = false;
        // Find a layer that can be drawn
        for( const auto &pr : neighbors ) {
            if( pr.second.neighborhood != all_neighbors ) {
                changed = true;
                layer.insert( pr );
            }
        }
        if( !changed ) {
            // An error, but a minor one - let it slide
            return;
        }
        // Remove the layer from the area to process
        for( const auto &pr : layer ) {
            const tripoint &pt = pr.first;
            const explosion_neighbors ngh = pr.second.neighborhood;

            unset_neighbor( pt + point_west, ngh, N_WEST, N_EAST );
            unset_neighbor( pt + point_east, ngh, N_EAST, N_WEST );
            unset_neighbor( pt + point_north, ngh, N_NORTH, N_SOUTH );
            unset_neighbor( pt + point_south, ngh, N_SOUTH, N_NORTH );
            neighbors.erase( pr.first );
        }

        layers.push_front( std::move( layer ) );
    }

#if defined(TILES)
    if( !use_tiles ) {
        draw_custom_explosion_curses( *g, layers );
        return;
    }

    explosion_animation anim;
    // We need to draw all explosions up to now
    std::map<tripoint, explosion_tile> combined_layer;

    shared_ptr_fast<game::draw_callback_t> explosion_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        tilecontext->init_custom_explosion_layer( combined_layer, exp_name );
    } );
    g->add_draw_callback( explosion_cb );

    for( const auto &layer : layers ) {
        combined_layer.insert( layer.begin(), layer.end() );
        if( is_layer_visible( layer ) ) {
            anim.progress();
        }
    }

    tilecontext->void_custom_explosion();
#else
    draw_custom_explosion_curses( *g, layers );
#endif
}

namespace
{

void draw_bullet_curses( map &m, const tripoint &t, const char bullet, const tripoint *const p )
{
    if( !is_point_visible( t ) ) {
        return;
    }

    const tripoint vp = g->u.pos() + g->u.view_offset;

    if( vp.z != t.z ) {
        return;
    }

    shared_ptr_fast<game::draw_callback_t> bullet_cb = make_shared_fast<game::draw_callback_t>( [&]() {
        if( p != nullptr && p->z == vp.z ) {
            m.drawsq( g->w_terrain, *p, drawsq_params().center( vp ) );
        }
        mvwputch( g->w_terrain, t.xy() - vp.xy() + point( POSX, POSY ), c_red, bullet );
    } );
    g->add_draw_callback( bullet_cb );
    bullet_animation().progress();
}

} // namespace

#if defined(TILES)
void game::draw_bullet( const tripoint &t, const int i,
                        const std::vector<tripoint> &trajectory, const char bullet,
                        const std::string &custom_sprite )
{
    if( !use_tiles ) {
        draw_bullet_curses( m, t, bullet, nullptr );
        return;
    }

    if( !is_point_visible( t ) ) {
        return;
    }

    auto sprite = std::string{};
    if( !custom_sprite.empty() ) {
        sprite = custom_sprite;
    } else if( bullet == '*' ) {
        sprite = "animation_bullet_normal_0deg";
    } else if( bullet == '#' ) {
        sprite = "animation_bullet_flame";
    } else if( bullet == '`' ) {
        sprite = "animation_bullet_shrapnel";
    }

    const auto rotation = get_bullet_rotation( get_bullet_dir( trajectory, static_cast<size_t>( i ) ) );
    auto bullet_cb = make_shared_fast<draw_callback_t>( [&]() {
        tilecontext->init_draw_bullet( t, sprite, rotation );
    } );
    add_draw_callback( bullet_cb );

    bullet_animation().progress();
    tilecontext->void_bullet();
}
#else
void game::draw_bullet( const tripoint &t, const int i, const std::vector<tripoint> &trajectory,
                        const char bullet, const std::string & )
{
    draw_bullet_curses( m, t, bullet, &trajectory[i] );
}
#endif

namespace
{
// short visual animation (player, monster, ...) (hit, dodge, ...)
// cTile is a UTF-8 strings, and must be a single cell wide!
void hit_animation( const avatar &u, const tripoint &center, nc_color cColor,
                    const std::string &cTile )
{
    const tripoint init_pos = relative_view_pos( u, center );
    // Only show animation if initially visible
    if( init_pos.z == 0 && is_valid_in_w_terrain( init_pos.xy() ) ) {
        shared_ptr_fast<game::draw_callback_t> hit_cb = make_shared_fast<game::draw_callback_t>( [&]() {
            // In case the window is resized during waiting, we always re-calculate the animation position
            const tripoint pos = relative_view_pos( u, center );
            if( pos.z == 0 && is_valid_in_w_terrain( pos.xy() ) ) {
                mvwprintz( g->w_terrain, pos.xy(), cColor, cTile );
            }
        } );
        g->add_draw_callback( hit_cb );

        ui_manager::redraw();
        inp_mngr.set_timeout( get_option<int>( "ANIMATION_DELAY" ) );
        // Skip input (if any), because holding down a key with nanosleep can get yourself killed
        inp_mngr.get_input_event();
        inp_mngr.reset_timeout();
    }
}

void draw_hit_mon_curses( const tripoint &center, const monster &m, const avatar &u,
                          const bool dead )
{
    hit_animation( u, center, red_background( m.type->color ), dead ? "%" : m.symbol() );
}

} // namespace

#if defined(TILES)
void game::draw_hit_mon( const tripoint &p, const monster &m, const bool dead )
{
    if( test_mode ) {
        // avoid segfault from null tilecontext in tests
        return;
    }

    if( !use_tiles ) {
        draw_hit_mon_curses( p, m, u, dead );
        return;
    }

    shared_ptr_fast<draw_callback_t> hit_cb = make_shared_fast<draw_callback_t>( [&]() {
        tilecontext->init_draw_hit( p, m.type->id.str() );
    } );
    add_draw_callback( hit_cb );

    bullet_animation().progress();
}
#else
void game::draw_hit_mon( const tripoint &p, const monster &m, const bool dead )
{
    draw_hit_mon_curses( p, m, u, dead );
}
#endif

namespace
{
void draw_hit_player_curses( const game &g, const Character &who, const int dam )
{
    nc_color const col = !dam ? yellow_background( who.symbol_color() ) : red_background(
                             who.symbol_color() );
    hit_animation( g.u, who.pos(), col, who.symbol() );
}
} //namespace

#if defined(TILES)
void game::draw_hit_player( const Character &p, const int dam )
{
    if( test_mode ) {
        // avoid segfault from null tilecontext in tests
        return;
    }

    if( !use_tiles ) {
        draw_hit_player_curses( *this, p, dam );
        return;
    }

    static const std::string player_male   {"player_male"};
    static const std::string player_female {"player_female"};
    static const std::string npc_male      {"npc_male"};
    static const std::string npc_female    {"npc_female"};

    const std::string &type = p.is_player() ? ( p.male ? player_male : player_female )
                              : p.male ? npc_male : npc_female;

    shared_ptr_fast<draw_callback_t> hit_cb = make_shared_fast<draw_callback_t>( [&]() {
        tilecontext->init_draw_hit( p.pos(), type );
    } );
    add_draw_callback( hit_cb );

    bullet_animation().progress();
}
#else
void game::draw_hit_player( const Character &who, const int dam )
{
    draw_hit_player_curses( *this, who, dam );
}
#endif

/* Line drawing code, not really an animation but should be separated anyway */
namespace
{
void draw_line_curses( game &g, const tripoint &center, const std::vector<tripoint> &ret,
                       bool noreveal )
{
    drawsq_params params = drawsq_params().highlight( true ).center( center );
    for( const tripoint &p : ret ) {
        const auto critter = g.critter_at( p, true );

        // NPCs and monsters get drawn with inverted colors
        if( critter && g.u.sees( *critter ) ) {
            critter->draw( g.w_terrain, center, true );
        } else if( noreveal && !g.u.sees( p ) ) {
            // Draw a meaningless symbol. Avoids revealing tile, but keeps feedback
            const char sym = '?';
            const nc_color col = c_dark_gray;
            const catacurses::window &w = g.w_terrain;
            const int k = p.x + ( getmaxx( w ) / 2 ) - center.x;
            const int j = p.y + ( getmaxy( w ) / 2 ) - center.y;
            mvwputch( w, point( k, j ), col, sym );
        } else {
            // This function reveals tile at p and writes it to the player's memory
            get_map().drawsq( g.w_terrain, p, params );
        }
    }
}
} //namespace

#if defined(TILES)
void game::draw_line( const tripoint &p, const tripoint &center,
                      const std::vector<tripoint> &points, bool noreveal )
{
    if( !u.sees( p ) ) {
        return;
    }

    if( !use_tiles ) {
        draw_line_curses( *this, center, points, noreveal );
        return;
    }

    tilecontext->init_draw_line( p, points, "line_target", true );
}
#else
void game::draw_line( const tripoint &p, const tripoint &center,
                      const std::vector<tripoint> &points, bool noreveal )
{
    if( !u.sees( p ) ) {
        return;
    }

    draw_line_curses( *this, center, points, noreveal );
}
#endif

namespace
{
void draw_line_curses( game &g, const std::vector<tripoint> &points )
{
    map &here = get_map();
    for( const tripoint &p : points ) {
        here.drawsq( g.w_terrain, p, drawsq_params().highlight( true ) );
    }

    const tripoint p = points.empty() ? tripoint {POSX, POSY, 0} :
                       relative_view_pos( g.u, points.back() );
    mvwputch( g.w_terrain, p.xy(), c_white, 'X' );
}
} //namespace

#if defined(TILES)
void draw_line_of( const draw_sprite_line_options &options )
{
    if( !use_tiles ) {
        draw_line_curses( *g, options.points );
        return;
    }
    std::vector<tripoint> ps;
    std::vector<std::string> ids;
    std::vector<int> rots;

    ps.reserve( options.points.size() );
    ids.reserve( options.points.size() );
    rots.reserve( options.points.size() );

    for( size_t i = 0; i < options.points.size(); ++i ) {
        if( !is_point_visible( options.points[i] ) ) { continue; }

        int rotation;
        if( options.rotate ) {
            const int step = static_cast<int>( i % 8 );
            const int cardinal_rotations[] = {0, 1, 2, 3};
            const int diagonal_rotations[] = {5, 6, 7, 8};
            rotation = ( step % 2 == 0 ) ? cardinal_rotations[step / 2] : diagonal_rotations[step / 2];
        } else {
            rotation = get_bullet_rotation( get_bullet_dir( options.points, i ) );
        }

        ps.push_back( options.points[i] );
        ids.push_back( options.sprite );
        rots.push_back( rotation );
    }
    if( ps.empty() ) { return; }

    auto bullets_cb = make_shared_fast<game::draw_callback_t>( [&] {
        tilecontext->init_draw_bullets( ps, ids, rots );
    } );
    g->add_draw_callback( bullets_cb );
    bullet_animation().progress( false );
    tilecontext->void_bullet();
}
void game::draw_line( const tripoint &p, const std::vector<tripoint> &points )
{
    draw_line_curses( *this, points );

    if( test_mode ) {
        // avoid segfault from null tilecontext in tests
        return;
    }
    tilecontext->init_draw_line( p, points, "line_trail", false );
}
#else
void draw_line_of( const draw_sprite_line_options &options )
{
    g->draw_line( options.p, options.points );
}
void game::draw_line( const tripoint &/*p*/, const std::vector<tripoint> &points )
{
    draw_line_curses( *this, points );
}
#endif

#if defined(TILES)
void game::draw_cursor( const tripoint &p )
{
    const tripoint rp = relative_view_pos( *this, p );
    mvwputch_inv( w_terrain, rp.xy(), c_light_green, 'X' );
    tilecontext->init_draw_cursor( p );
}
#else
void game::draw_cursor( const tripoint &p )
{
    const tripoint rp = relative_view_pos( *this, p );
    mvwputch_inv( w_terrain, rp.xy(), c_light_green, 'X' );
}
#endif

#if defined(TILES)
void game::draw_highlight( const tripoint &p )
{
    if( test_mode ) {
        // avoid segfault from null tilecontext in tests
        return;
    }

    tilecontext->init_draw_highlight( p );
}
#else
void game::draw_highlight( const tripoint & )
{
    // Do nothing
}
#endif

namespace
{
void draw_weather_curses( const catacurses::window &win, const weather_printable &w )
{
    for( const auto &drop : w.vdrops ) {
        mvwputch( win, point( drop.first, drop.second ), w.colGlyph, w.get_symbol() );
    }
}
} //namespace

#if defined(TILES)
void game::draw_weather( const weather_printable &w )
{
    if( !use_tiles ) {
        draw_weather_curses( w_terrain, w );
        return;
    }

    tilecontext->init_draw_weather( w, w.wtype->animation.tile );
}
#else
void game::draw_weather( const weather_printable &w )
{
    draw_weather_curses( w_terrain, w );
}
#endif

namespace
{
void draw_sct_curses( const game &g )
{
    const tripoint off = relative_view_pos( g.u, tripoint_zero );

    for( const auto &text : SCT.vSCT ) {
        const int dy = off.y + text.getPosY();
        const int dx = off.x + text.getPosX();

        if( !is_valid_in_w_terrain( point( dx, dy ) ) ) {
            continue;
        }

        const bool is_old = text.getStep() >= scrollingcombattext::iMaxSteps / 2;

        nc_color const col1 = msgtype_to_color( text.getMsgType( "first" ),  is_old );
        nc_color const col2 = msgtype_to_color( text.getMsgType( "second" ), is_old );

        mvwprintz( g.w_terrain, point( dx, dy ), col1, text.getText( "first" ) );
        wprintz( g.w_terrain, col2, text.getText( "second" ) );
    }
}
} //namespace

#if defined(TILES)
void game::draw_sct()
{
    if( use_tiles ) {
        tilecontext->init_draw_sct();
    } else {
        draw_sct_curses( *this );
    }
}
#else
void game::draw_sct()
{
    draw_sct_curses( *this );
}
#endif

namespace
{
void draw_zones_curses( const catacurses::window &w, const tripoint &start, const tripoint &end,
                        const tripoint &offset )
{
    if( end.x < start.x || end.y < start.y || end.z < start.z ) {
        return;
    }

    nc_color    const col = invert_color( c_light_green );
    const std::string line( end.x - start.x + 1, '~' );
    int         const x = start.x - offset.x;

    for( int y = start.y; y <= end.y; ++y ) {
        mvwprintz( w, point( x, y - offset.y ), col, line );
    }
}
} //namespace

#if defined(TILES)
void game::draw_zones( const tripoint &start, const tripoint &end, const tripoint &offset )
{
    if( use_tiles ) {
        tilecontext->init_draw_zones( start, end, offset );
    } else {
        draw_zones_curses( w_terrain, start, end, offset );
    }
}
#else
void game::draw_zones( const tripoint &start, const tripoint &end, const tripoint &offset )
{
    draw_zones_curses( w_terrain, start, end, offset );
}
#endif

#if defined(TILES)
void game::draw_radiation_override( const tripoint &p, const int rad )
{
    if( use_tiles ) {
        tilecontext->init_draw_radiation_override( p, rad );
    }
}
#else
void game::draw_radiation_override( const tripoint &, const int )
{
}
#endif

#if defined(TILES)
void game::draw_terrain_override( const tripoint &p, const ter_id &id )
{
    if( use_tiles ) {
        tilecontext->init_draw_terrain_override( p, id );
    }
}
#else
void game::draw_terrain_override( const tripoint &, const ter_id & )
{
}
#endif

#if defined(TILES)
void game::draw_furniture_override( const tripoint &p, const furn_id &id )
{
    if( use_tiles ) {
        tilecontext->init_draw_furniture_override( p, id );
    }
}
#else
void game::draw_furniture_override( const tripoint &, const furn_id & )
{
}
#endif

#if defined(TILES)
void game::draw_graffiti_override( const tripoint &p, const bool has )
{
    if( use_tiles ) {
        tilecontext->init_draw_graffiti_override( p, has );
    }
}
#else
void game::draw_graffiti_override( const tripoint &, const bool )
{
}
#endif

#if defined(TILES)
void game::draw_trap_override( const tripoint &p, const trap_id &id )
{
    if( use_tiles ) {
        tilecontext->init_draw_trap_override( p, id );
    }
}
#else
void game::draw_trap_override( const tripoint &, const trap_id & )
{
}
#endif

#if defined(TILES)
void game::draw_field_override( const tripoint &p, const field_type_id &id )
{
    if( use_tiles ) {
        tilecontext->init_draw_field_override( p, id );
    }
}
#else
void game::draw_field_override( const tripoint &, const field_type_id & )
{
}
#endif

#if defined(TILES)
void game::draw_item_override( const tripoint &p, const itype_id &id, const mtype_id &mid,
                               const bool hilite )
{
    if( use_tiles ) {
        tilecontext->init_draw_item_override( p, id, mid, hilite );
    }
}
#else
void game::draw_item_override( const tripoint &, const itype_id &, const mtype_id &,
                               const bool )
{
}
#endif

#if defined(TILES)
void game::draw_vpart_override(
    const tripoint &p, const vpart_id &id, const int part_mod, const units::angle veh_dir,
    const bool hilite, point mount )
{
    if( use_tiles ) {
        tilecontext->init_draw_vpart_override( p, id, part_mod, veh_dir, hilite, mount );
    }
}
#else
void game::draw_vpart_override( const tripoint &, const vpart_id &, const int,
                                const units::angle, const bool, point )
{
}
#endif

#if defined(TILES)
void game::draw_below_override( const tripoint &p, const bool draw )
{
    if( use_tiles ) {
        tilecontext->init_draw_below_override( p, draw );
    }
}
#else
void game::draw_below_override( const tripoint &, const bool )
{
}
#endif

#if defined(TILES)
void game::draw_monster_override( const tripoint &p, const mtype_id &id, const int count,
                                  const bool more, const Attitude att )
{
    if( use_tiles ) {
        tilecontext->init_draw_monster_override( p, id, count, more, att );
    }
}
#else
void game::draw_monster_override( const tripoint &, const mtype_id &, const int,
                                  const bool, const Attitude )
{
}
#endif

bucketed_points bucket_by_distance( const tripoint &origin,
                                    const std::map<tripoint, double> &to_bucket )
{
    std::map<int, one_bucket> by_distance;
    for( const auto& [pt, val] : to_bucket ) {
        int dist = trig_dist_squared( origin, pt );
        by_distance[dist].emplace_back( point_with_value{ pt, val} );
    }
    bucketed_points buckets;
    for( const auto& [_, bucket] : by_distance ) {
        buckets.emplace_back( bucket );
    }
    return buckets;
}

bucketed_points optimal_bucketing( const bucketed_points &buckets, size_t max_buckets )
{
    if( buckets.size() <= max_buckets ) {
        return buckets;
    }
    assert( max_buckets > 1 );

    std::vector<size_t> sizes = {};
    for( const one_bucket &bc : buckets ) {
        sizes.emplace_back( bc.size() );
    }

    bucketed_points optimal = buckets;
    // TODO: Good algorithm here, this one is a greedy finder of smallest adjacent size sums
    for( size_t i = 0; i < buckets.size() - max_buckets; i++ ) {
        auto smallest = sizes.begin();
        size_t smallest_sum = *smallest + *( smallest + 1 );
        for( auto iter = sizes.begin() + 1; ( iter + 1 ) != sizes.end(); iter++ ) {
            size_t sum = *iter + *( iter + 1 );
            if( sum < smallest_sum ) {
                smallest = iter;
                smallest_sum = sum;
            }
        }

        size_t distance = std::distance( sizes.begin(), smallest );
        sizes[distance] += sizes[distance + 1];
        sizes.erase( smallest + 1 );
        auto left_bucket = std::next( optimal.begin(), distance );
        auto right_bucket = std::next( left_bucket );
        left_bucket->insert( left_bucket->end(), right_bucket->begin(), right_bucket->end() );
        optimal.erase( right_bucket );
    }

    return optimal;
}

static void draw_cone_aoe_curses( const tripoint &, const bucketed_points &waves )
{
    // Calculate screen offset relative to player + view offset position
    const avatar &u = get_avatar();
    const tripoint center = u.pos() + u.view_offset;
    const tripoint topleft( center.x - ( catacurses::getmaxx( g->w_terrain ) / 2 ),
                            center.y - ( catacurses::getmaxy( g->w_terrain ) / 2 ), 0 );

    auto it = waves.begin();
    shared_ptr_fast<game::draw_callback_t> wave_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        // All the buckets up until now
        for( auto inner_it = waves.begin(); inner_it != std::next( it ); inner_it++ ) {
            for( const point_with_value &pr : *inner_it ) {
                // update tripoint in relation to top left corner of curses window
                // mvwputch already filters out of bounds coordinates
                const tripoint p = pr.pt - topleft;
                int intensity = ( pr.val >= 1.0 ) + ( pr.val >= 0.5 ) + ( inner_it == it );
                nc_color col;
                switch( intensity ) {
                    case 3:
                        col = c_red;
                        break;
                    case 2:
                        col = c_yellow;
                        break;
                    case 1:
                        col = c_white;
                        break;
                    default:
                        col = c_dark_gray;
                        break;
                }

                // TODO: Prettier
                mvwputch( g->w_terrain, p.xy(), col, '*' );
            }
        }
    } );
    g->add_draw_callback( wave_cb );

    wave_animation anim;
    for( it = waves.begin(); it != waves.end(); it++ ) {
        anim.progress();
    }
}

namespace ranged
{
void draw_cone_aoe( const tripoint &origin, const std::map<tripoint, double> &aoe )
{
    if( test_mode ) {
        return;
    }

    bucketed_points buckets = bucket_by_distance( origin, aoe );
    // That hardcoded value could be improved... Not sure about the name
    size_t max_bucket_count = std::min<size_t>( 10, aoe.size() );
    bucketed_points waves = optimal_bucketing( buckets, max_bucket_count );

#if defined(TILES)
    if( !use_tiles ) {
        draw_cone_aoe_curses( origin, waves );
        return;
    }

    // This is copied from explosion code
    // Not sure if it couldn't be cleaner, without that lambda capture thing
    one_bucket combined_layer;
    combined_layer.reserve( aoe.size() );

    wave_animation anim;

    shared_ptr_fast<game::draw_callback_t> wave_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        tilecontext->init_draw_cone_aoe( origin, combined_layer );
    } );
    g->add_draw_callback( wave_cb );

    for( const one_bucket &layer : waves ) {
        // Older layers get a fade effect
        for( point_with_value &pv : combined_layer ) {
            pv.val *= 1.0 - ( 2.0 / max_bucket_count );
        }
        combined_layer.insert( combined_layer.end(), layer.begin(), layer.end() );
        if( std::ranges::any_of( combined_layer,
        []( const point_with_value & element ) {
        return is_point_visible( element.pt );
        } ) ) {
            anim.progress();
        }
    }

    tilecontext->void_cone_aoe();
#else
    draw_cone_aoe_curses( origin, waves );
#endif
}
} // namespace ranged

bool minimap_requires_animation()
{
#if defined(TILES)
    return tilecontext->minimap_requires_animation();
#else
    return false;
#endif // TILES
}

bool terrain_requires_animation()
{
#if defined(TILES)
    return tilecontext->terrain_requires_animation();
#else
    return false;
#endif // TILES
}
