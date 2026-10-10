#include "animation.h"

#include "avatar.h"
#include "cached_options.h"
#include "engine_client_presentation.h"
#include "character.h"
#include "client_display.h"
#include "client_animation.h"
#include "coordinates.h"
#include "cursesdef.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "line.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map_memory.h"
#include "monster.h"
#include "mtype.h"
#include "options.h"
#include "output.h"
#include "point.h"
#include "popup.h"
#include "posix_time.h"
#include "ranged.h"
#include "translations.h"
#include "travel/travel_destination.h"
#include "type_id.h"
#include "ui_manager.h"
#include "weather/weather.h"

#include <algorithm>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

auto has_memorized_terrain_at( avatar &you, map &/*here*/, const tripoint_bub_ms &target ) -> bool
{
    if( !you.should_show_map_memory() ) {
        return false;
    }

    const auto abs_target = bub_to_abs( target );
    return ter_str_id( you.get_terrain_tile( abs_target ).tile ).is_valid() ||
           ter_str_id( you.get_memorized_tile( abs_target ).tile ).is_valid();
}

class basic_animation
{
    public:
        explicit basic_animation( const int scale ) :
            scale( scale ) {
        }
        void progress( bool draw_popup = true ) const {
            game_client::progress_animation( { .multiplier = scale, .draw_popup = draw_popup } );
        }

    private:
        int scale;
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

bool is_point_visible( const tripoint_bub_ms &p, int margin = 0 )
{
    return g->is_in_viewport( p, margin ) && g->u.sees( p );
}

bool is_radius_visible( const tripoint_bub_ms &center, int radius )
{
    return is_point_visible( center, -radius );
}

bool is_layer_visible( const std::map<tripoint_bub_ms, explosion_tile> &layer )
{
    return std::ranges::any_of( layer,
    []( const std::pair<tripoint_bub_ms, explosion_tile> &element ) {
        return is_point_visible( element.first );
    } );
}

// Convert p to screen position relative to u's current position and view
tripoint_rel_ms relative_view_pos( const avatar &u, const tripoint_bub_ms &p ) noexcept
{
    return p - ( u.bub_pos() + u.view_offset ) + point_rel_ms( POSX, POSY );
}

// Convert p to screen position relative to the current terrain view
tripoint_rel_ms relative_view_pos( const game &g, const tripoint_bub_ms &p ) noexcept
{
    return p - g.ter_view_p + point( POSX, POSY );
}

void draw_explosion_curses( game &g, const tripoint_bub_ms &center, const int r,
                            const nc_color &col )
{
    if( !is_radius_visible( center, r ) ) {
        return;
    }
    // TODO: Make it look different from above/below
    const auto p = relative_view_pos( g.u, center );

    explosion_animation anim;

    int frame = 0;
    shared_ptr_fast<game::draw_callback_t> explosion_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        if( r == 0 ) {
            mvwputch( g.w_terrain, point( p.y(), p.x() ), col, '*' );
        }

        for( int i = 1; i <= frame; ++i ) {
            // corner: top left
            mvwputch( g.w_terrain, p.xy().raw() + point( -i, -i ), col, '/' );
            // corner: top right
            mvwputch( g.w_terrain, p.xy().raw() + point( i, -i ), col, '\\' );
            // corner: bottom left
            mvwputch( g.w_terrain, p.xy().raw() + point( -i, i ), col, '\\' );
            // corner: bottom right
            mvwputch( g.w_terrain, p.xy().raw() + point( i, i ), col, '/' );
            for( int j = 1 - i; j < 0 + i; j++ ) {
                // edge: top
                mvwputch( g.w_terrain, p.xy().raw() + point( j, -i ), col, '-' );
                // edge: bottom
                mvwputch( g.w_terrain, p.xy().raw() + point( j, i ), col, '-' );
                // edge: left
                mvwputch( g.w_terrain, p.xy().raw() + point( -i, j ), col, '|' );
                // edge: right
                mvwputch( g.w_terrain, p.xy().raw() + point( i, j ), col, '|' );
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
                                   const std::list< std::map<tripoint_bub_ms, explosion_tile> > &layers )
{
    // calculate screen offset relative to player + view offset position
    const auto center = g.u.bub_pos() + g.u.view_offset;
    const tripoint topleft( center.x() - ( getmaxx( g.w_terrain ) / 2 ),
                            center.y() - ( getmaxy( g.w_terrain ) / 2 ), 0 );

    explosion_animation anim;

    auto last_layer_it = layers.begin();
    shared_ptr_fast<game::draw_callback_t> explosion_cb =
    make_shared_fast<game::draw_callback_t>( [&]() {
        for( auto it = layers.begin(); it != std::next( last_layer_it ); ++it ) {
            for( const auto &pr : *it ) {
                // update tripoint in relation to top left corner of curses window
                // mvwputch already filters out of bounds coordinates
                const auto p = pr.first - topleft;
                const explosion_neighbors ngh = pr.second.neighborhood;
                const nc_color col = pr.second.color;

                switch( ngh ) {
                    // '^', 'v', '<', '>'
                    case N_NORTH:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '^' );
                        break;
                    case N_SOUTH:
                        mvwputch( g.w_terrain, p.xy().raw(), col, 'v' );
                        break;
                    case N_WEST:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '<' );
                        break;
                    case N_EAST:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '>' );
                        break;
                    // '|' and '-'
                    case N_NORTH | N_SOUTH:
                    case N_NORTH | N_SOUTH | N_WEST:
                    case N_NORTH | N_SOUTH | N_EAST:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '|' );
                        break;
                    case N_WEST | N_EAST:
                    case N_WEST | N_EAST | N_NORTH:
                    case N_WEST | N_EAST | N_SOUTH:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '-' );
                        break;
                    // '/' and '\'
                    case N_NORTH | N_WEST:
                    case N_SOUTH | N_EAST:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '/' );
                        break;
                    case N_SOUTH | N_WEST:
                    case N_NORTH | N_EAST:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '\\' );
                        break;
                    case N_NO_NEIGHBORS:
                        mvwputch( g.w_terrain, p.xy().raw(), col, '*' );
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

} // namespace

void explosion_handler::draw_explosion( const tripoint_bub_ms &p, const int r, const nc_color &col,
                                        const std::string &exp_name )
{
    engine_client::presentation::record_explosion( p, r, col );
    if( !game_client::animation().draw_explosion( { .position = p, .radius = r,
            .color = col, .name = exp_name } ) ) {
        draw_explosion_curses( *g, p, r, col );
    }
}

void explosion_handler::draw_custom_explosion( const tripoint_bub_ms &position,
        const std::map<tripoint_bub_ms, nc_color> &all_area,
        const std::string &exp_name )
{
    engine_client::presentation::record_custom_explosion( position, all_area );
    if( test_mode ) {
        // Avoid drawing animation state during tests.
        return;
    }

    constexpr explosion_neighbors all_neighbors = N_NORTH | N_SOUTH | N_WEST | N_EAST;
    // We will "shell" the explosion area
    // Each phase will strip a single layer of points
    // A layer contains all points that have less than 4 neighbors in cardinal directions
    // Layers will first be generated, then drawn in inverse order

    // Start by getting rid of everything except current z-level
    std::map<tripoint_bub_ms, explosion_tile> neighbors;
    const auto view_center = relative_view_pos( g->u, g->u.bub_pos() );
    for( const auto &pr : all_area ) {
        const tripoint_rel_ms relative_point = relative_view_pos( g->u, pr.first );
        if( relative_point.z() == ( game_client::has_tiles() ? view_center.z() : 0 ) ) {
            neighbors[pr.first] = explosion_tile{ N_NO_NEIGHBORS, pr.second };
        }
    }

    // Searches for a neighbor, sets the neighborhood flag on current point and on the neighbor
    const auto set_neighbors = [&]( const tripoint_bub_ms & pos,
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
    const auto unset_neighbor = [&]( const tripoint_bub_ms & pos,
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
        const tripoint_bub_ms &pt = pr.first;
        explosion_neighbors &ngh = pr.second.neighborhood;

        set_neighbors( pt + point_west, ngh, N_WEST, N_EAST );
        set_neighbors( pt + point_east, ngh, N_EAST, N_WEST );
        set_neighbors( pt + point_north, ngh, N_NORTH, N_SOUTH );
        set_neighbors( pt + point_south, ngh, N_SOUTH, N_NORTH );
    }

    // We need to save the layers because we will draw them in reverse order
    std::list< std::map<tripoint_bub_ms, explosion_tile> > layers;
    while( !neighbors.empty() ) {
        std::map<tripoint_bub_ms, explosion_tile> layer;
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
            const tripoint_bub_ms &pt = pr.first;
            const explosion_neighbors ngh = pr.second.neighborhood;

            unset_neighbor( pt + point_west, ngh, N_WEST, N_EAST );
            unset_neighbor( pt + point_east, ngh, N_EAST, N_WEST );
            unset_neighbor( pt + point_north, ngh, N_NORTH, N_SOUTH );
            unset_neighbor( pt + point_south, ngh, N_SOUTH, N_NORTH );
            neighbors.erase( pr.first );
        }

        layers.push_front( std::move( layer ) );
    }

    if( !game_client::animation().draw_custom_explosion( { .position = position,
            .area = &all_area, .name = exp_name, .layers = &layers } ) ) {
        draw_custom_explosion_curses( *g, layers );
    }
}

namespace
{

void draw_bullet_curses( map &m, const tripoint_bub_ms &t, const char bullet,
                         const tripoint_bub_ms *const p )
{
    if( !is_point_visible( t ) ) {
        return;
    }

    const auto vp = g->u.bub_pos() + g->u.view_offset;

    if( vp.z() != t.z() ) {
        return;
    }

    shared_ptr_fast<game::draw_callback_t> bullet_cb = make_shared_fast<game::draw_callback_t>( [&]() {
        if( p != nullptr && p->z() == vp.z() ) {
            m.drawsq( g->w_terrain, *p, drawsq_params().center( vp ) );
        }
        mvwputch( g->w_terrain, ( t.xy() - vp.xy() ).raw() + point( POSX, POSY ), c_red, bullet );
    } );
    g->add_draw_callback( bullet_cb );
    bullet_animation().progress();
}

} // namespace

void game::draw_bullet( const tripoint_bub_ms &t, const int i,
                        const std::vector<tripoint_bub_ms> &trajectory, const char bullet,
                        const std::string &custom_sprite )
{
    engine_client::presentation::record_bullet( { .at = t, .index = i, .bullet = bullet,
            .custom_sprite = custom_sprite } );
    refresh_player_visibility_cache_if_needed();
    if( !game_client::animation().draw_bullet( { .position = t, .index = i,
            .trajectory = &trajectory, .bullet = bullet, .custom_sprite = custom_sprite } ) ) {
        draw_bullet_curses( m, t, bullet, &trajectory[i] );
    }
}

namespace
{

auto get_longest_trajectory_size( const std::vector<std::vector<tripoint_bub_ms>> &trajectories ) ->
size_t
{
    auto longest_trajectory_size = size_t{ 0 };
    for( const auto &trajectory : trajectories ) {
        longest_trajectory_size = std::max( longest_trajectory_size, trajectory.size() );
    }
    return longest_trajectory_size;
}


auto draw_bullet_trajectories_curses( game &g,
                                      const draw_bullet_trajectories_options &options ) -> void
{
    if( options.draw_as_line ) {
        auto bullet_cb = make_shared_fast<game::draw_callback_t>( [&]() {
            auto &here = get_map();
            for( const auto &trajectory : options.trajectories ) {
                for( size_t point_index = 1; point_index < trajectory.size(); point_index++ ) {
                    const auto &point = trajectory[point_index];
                    if( !is_point_visible( point ) ) {
                        continue;
                    }

                    here.drawsq( g.w_terrain, point, drawsq_params().highlight( true ) );
                }
            }
        } );
        g.add_draw_callback( bullet_cb );
        bullet_animation().progress( false );
        return;
    }

    const auto longest_trajectory_size = get_longest_trajectory_size( options.trajectories );
    for( size_t step = 1; step < longest_trajectory_size; step++ ) {
        auto bullet_cb = make_shared_fast<game::draw_callback_t>( [ &, step]() {
            auto &here = get_map();
            const auto view_pos = g.u.bub_pos() + g.u.view_offset;
            for( const auto &trajectory : options.trajectories ) {
                if( step >= trajectory.size() || !is_point_visible( trajectory[step] ) ) {
                    continue;
                }

                if( trajectory[step - 1].z() == view_pos.z() ) {
                    here.drawsq( g.w_terrain, trajectory[step - 1], drawsq_params().center( view_pos ) );
                }
                if( trajectory[step].z() != view_pos.z() ) {
                    continue;
                }

                mvwputch( g.w_terrain, ( trajectory[step].xy() - view_pos.xy() ).raw() + point( POSX, POSY ),
                          c_red, options.bullet );
            }
        } );
        g.add_draw_callback( bullet_cb );
        bullet_animation().progress();
    }
}

} // namespace

void draw_bullet_trajectories( const draw_bullet_trajectories_options &options )
{
    if( options.trajectories.empty() ) {
        return;
    }

    engine_client::presentation::record_trajectories( options );
    g->refresh_player_visibility_cache_if_needed();

    if( !game_client::animation().draw_bullet_trajectories( { .trajectories = &options } ) ) {
        draw_bullet_trajectories_curses( *g, options );
    }
}

namespace
{
// short visual animation (player, monster, ...) (hit, dodge, ...)
// cTile is a UTF-8 strings, and must be a single cell wide!
void hit_animation( const avatar &u, const tripoint_bub_ms &center, nc_color cColor,
                    const std::string &cTile )
{
    const auto init_pos = relative_view_pos( u, center );
    // Only show animation if initially visible
    if( init_pos.z() == 0 && is_valid_in_w_terrain( init_pos.xy().raw() ) ) {
        shared_ptr_fast<game::draw_callback_t> hit_cb = make_shared_fast<game::draw_callback_t>( [&]() {
            // In case the window is resized during waiting, we always re-calculate the animation position
            const auto pos = relative_view_pos( u, center );
            if( pos.z() == 0 && is_valid_in_w_terrain( pos.xy().raw() ) ) {
                mvwprintz( g->w_terrain, pos.xy().raw(), cColor, cTile );
            }
        } );
        g->add_draw_callback( hit_cb );

        // Pace without reading input: a read would be a client-dependent replay record, and
        // would block on (then discard) an MCP command.
        game_client::progress_animation( { .draw_popup = false } );
    }
}

void draw_hit_mon_curses( const tripoint_bub_ms &center, const monster &m, const avatar &u,
                          const bool dead )
{
    hit_animation( u, center, red_background( m.type->color ), dead ? "%" : m.symbol() );
}

} // namespace

void game::draw_hit_mon( const tripoint_bub_ms &p, const monster &m, const bool dead )
{
    if( !game_client::animation().draw_hit_mon( { .position = p, .target = &m, .dead = dead } ) ) {
        draw_hit_mon_curses( p, m, u, dead );
    }
}

namespace
{
void draw_hit_player_curses( const game &g, const Character &who, const int dam )
{
    nc_color const col = !dam ? yellow_background( who.symbol_color() ) : red_background(
                             who.symbol_color() );
    hit_animation( g.u, who.bub_pos(), col, who.symbol() );
}
} //namespace

void game::draw_hit_player( const Character &p, const int dam )
{
    if( !game_client::animation().draw_hit_player( { .position = p.bub_pos(), .target = &p, .damage = dam } ) ) {
        draw_hit_player_curses( *this, p, dam );
    }
}

/* Line drawing code, not really an animation but should be separated anyway */
namespace
{
void draw_line_curses( game &g, const tripoint_bub_ms &center,
                       const std::vector<tripoint_bub_ms> &ret,
                       bool noreveal )
{
    drawsq_params params = drawsq_params().highlight( true ).center( center );
    for( const tripoint_bub_ms &p : ret ) {
        const auto critter = g.critter_at( p, true );

        // NPCs and monsters get drawn with inverted colors
        if( critter && g.u.sees( *critter ) ) {
            critter->draw( g.w_terrain, center, true );
        } else if( noreveal && !g.u.sees( p ) ) {
            // Draw a meaningless symbol. Avoids revealing tile, but keeps feedback
            const char sym = '?';
            const nc_color col = c_dark_gray;
            const catacurses::window &w = g.w_terrain;
            const int k = p.x() + ( getmaxx( w ) / 2 ) - center.x();
            const int j = p.y() + ( getmaxy( w ) / 2 ) - center.y();
            mvwputch( w, point( k, j ), col, sym );
        } else {
            // This function reveals tile at p and writes it to the player's memory
            get_map().drawsq( g.w_terrain, p, params );
        }
    }
}
} //namespace

void game::draw_line( const tripoint_bub_ms &p, const tripoint_bub_ms &center,
                      const std::vector<tripoint_bub_ms> &points, bool noreveal )
{
    if( !noreveal && !avatar_knows_travel_destination( u, p ) &&
        !has_memorized_terrain_at( u, get_map(), p ) ) {
        return;
    }
    if( !game_client::animation().draw_line( { .position = p, .center = center,
            .points = &points, .no_reveal = noreveal } ) ) {
        draw_line_curses( *this, center, points, noreveal );
    }
}

namespace
{
void draw_line_curses( game &g, const std::vector<tripoint_bub_ms> &points )
{
    map &here = get_map();
    for( const tripoint_bub_ms &p : points ) {
        here.drawsq( g.w_terrain, p, drawsq_params().highlight( true ) );
    }

    const auto p = points.empty() ? tripoint {POSX, POSY, 0} :
                   relative_view_pos( g.u, points.back() ).raw();
    mvwputch( g.w_terrain, p.xy(), c_white, 'X' );
}
} //namespace

void draw_line_of( const draw_sprite_line_options &options )
{
    engine_client::presentation::record_line( options );
    if( !game_client::animation().draw_line_of( options ) ) {
        g->draw_line( options.p, options.points );
    }
}
void game::draw_line( const tripoint_bub_ms &p, const std::vector<tripoint_bub_ms> &points )
{
    draw_line_curses( *this, points );
    game_client::animation().draw_trail_line( { .position = p, .center = tripoint_bub_ms::zero(),
            .points = &points, .no_reveal = false } );
}

void game::draw_cursor( const tripoint_bub_ms &p )
{
    const auto rp = relative_view_pos( *this, p );
    mvwputch_inv( w_terrain, rp.xy().raw(), c_light_green, 'X' );
    game_client::animation().draw_cursor( { .position = p } );
}

void game::draw_highlight( const tripoint_bub_ms &p )
{
    game_client::animation().draw_highlight( { .position = p } );
}

namespace
{
void draw_weather_curses( const catacurses::window &win, const weather_printable &w )
{
    for( const auto &drop : w.vdrops ) {
        mvwputch( win, point( drop.first, drop.second ), w.colGlyph, w.get_symbol() );
    }
}
} //namespace

void game::draw_weather( const weather_printable &w )
{
    if( !game_client::animation().draw_weather( { .weather = &w } ) ) {
        draw_weather_curses( w_terrain, w );
    }
}

namespace
{
void draw_sct_curses( const game &g )
{
    const auto off = relative_view_pos( g.u, tripoint_bub_ms::zero() );

    for( const auto &text : SCT.vSCT ) {
        const int dy = off.y() + text.getPosY();
        const int dx = off.x() + text.getPosX();

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

void game::draw_sct()
{
    if( !game_client::animation().draw_sct() ) {
        draw_sct_curses( *this );
    }
}

namespace
{
void draw_zones_curses( const catacurses::window &w, const zone_draw_options &options )
{
    nc_color    const col = invert_color( c_light_green );
    const bool has_points = !options.points.empty();
    if( has_points ) {
        std::ranges::for_each( options.points, [&]( const tripoint_bub_ms & location ) {
            mvwputch( w, point( location.x() - options.offset.x(), location.y() - options.offset.y() ),
                      col, '~' );
        } );
    } else {
        if( options.end.x() < options.start.x() || options.end.y() < options.start.y() ||
            options.end.z() < options.start.z() ) {
            return;
        }

        const std::string line( options.end.x() - options.start.x() + 1, '~' );
        const int x = options.start.x() - options.offset.x();

        for( int y = options.start.y(); y <= options.end.y(); ++y ) {
            mvwprintz( w, point( x, y - options.offset.y() ), col, line );
        }
    }

    const auto bounds = [&]() -> std::optional<std::pair<point_bub_ms, point_bub_ms>> {
        if( has_points )
        {
            const auto min_x = std::ranges::minmax_element( options.points, {}, [](
                const tripoint_bub_ms & p ) { return p.x(); } );
            const auto min_y = std::ranges::minmax_element( options.points, {}, [](
            const tripoint_bub_ms & p ) { return p.y(); } );
            return std::pair<point_bub_ms, point_bub_ms>( point_bub_ms( min_x.min->x(), min_y.min->y() ),
                    point_bub_ms( min_x.max->x(), min_y.max->y() ) );
        }
        return std::pair<point_bub_ms, point_bub_ms>( options.start.xy(), options.end.xy() );
    }();

    if( !bounds.has_value() ) {
        return;
    }

    const auto min_local = bounds->first;
    const auto max_local = bounds->second;
    const int width = max_local.x() - min_local.x() + 1;
    const int height = max_local.y() - min_local.y() + 1;
    if( width <= 0 || height <= 0 ) {
        return;
    }

    const std::string label = string_format( _( "(%dx%d)" ), width, height );
    const point center_local( ( min_local.x() + max_local.x() ) / 2,
                              ( min_local.y() + max_local.y() ) / 2 );
    const auto label_pos = point(
                               std::clamp( center_local.x - static_cast<int>( label.size() ) / 2,
                                           0, getmaxx( w ) - static_cast<int>( label.size() ) ),
                               std::clamp( center_local.y - options.offset.y(), 0,
                                           getmaxy( w ) - 1 ) );
    mvwprintz( w, label_pos, c_white, label );
}
} //namespace

void game::draw_zones( const zone_draw_options &options )
{
    if( !game_client::animation().draw_zones( { .zones = &options } ) ) {
        draw_zones_curses( w_terrain, options );
    }
}

void game::draw_radiation_override( const tripoint_bub_ms &p, const int rad )
{
    game_client::animation().draw_radiation_override( { .position = p, .radiation = rad } );
}

void game::draw_terrain_override( const tripoint_bub_ms &p, const ter_id &id )
{
    game_client::animation().draw_terrain_override( { .position = p, .terrain = id } );
}

void game::draw_furniture_override( const tripoint_bub_ms &p, const furn_id &id )
{
    game_client::animation().draw_furniture_override( { .position = p, .furniture = id } );
}

void game::draw_graffiti_override( const tripoint_bub_ms &p, const bool has )
{
    game_client::animation().draw_graffiti_override( { .position = p, .has_graffiti = has } );
}

void game::draw_trap_override( const tripoint_bub_ms &p, const trap_id &id )
{
    game_client::animation().draw_trap_override( { .position = p, .trap = id } );
}

void game::draw_field_override( const tripoint_bub_ms &p, const field_type_id &id )
{
    game_client::animation().draw_field_override( { .position = p, .field = id } );
}

void game::draw_item_override( const tripoint_bub_ms &p, const itype_id &id, const mtype_id &mid,
                               const bool hilite )
{
    game_client::animation().draw_item_override( { .position = p, .item = id, .monster = mid,
            .highlight = hilite } );
}

void game::draw_vpart_override( const tripoint_bub_ms &p, const vpart_id &id, const int part_mod,
                                const units::angle veh_dir, const bool hilite, tripoint_mnt_veh mount )
{
    game_client::animation().draw_vehicle_part_override( { .position = p, .part = id, .part_mod = part_mod,
            .direction = veh_dir, .highlight = hilite, .mount = mount } );
}

void game::draw_below_override( const tripoint_bub_ms &p, const bool draw )
{
    game_client::animation().draw_below_override( { .position = p, .draw = draw } );
}

void game::draw_monster_override( const tripoint_bub_ms &p, const mtype_id &id, const int count,
                                  const bool more, const Attitude att )
{
    game_client::animation().draw_monster_override( { .position = p, .monster = id, .count = count,
            .more = more, .attitude = att } );
}

bucketed_points bucket_by_distance( const tripoint_bub_ms &origin,
                                    const std::map<tripoint_bub_ms, double> &to_bucket )
{
    std::map<int, one_bucket> by_distance;
    for( const auto& [pt, val] : to_bucket ) {
        int dist = trig_dist_squared( origin, pt );
        by_distance[dist].emplace_back( point_with_value{ pt.raw(), val} );
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

static void draw_cone_aoe_curses( const tripoint_bub_ms &, const bucketed_points &waves )
{
    // Calculate screen offset relative to player + view offset position
    const avatar &u = get_avatar();
    const auto center = u.bub_pos() + u.view_offset;
    const tripoint topleft( center.x() - ( catacurses::getmaxx( g->w_terrain ) / 2 ),
                            center.y() - ( catacurses::getmaxy( g->w_terrain ) / 2 ), 0 );

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
void draw_cone_aoe( const tripoint_bub_ms &origin, const std::map<tripoint_bub_ms, double> &aoe )
{
    if( test_mode ) {
        return;
    }

    if( !game_client::animation().draw_cone_aoe( { .origin = origin, .coverage = &aoe } ) ) {
        bucketed_points buckets = bucket_by_distance( origin, aoe );
        // That hardcoded value could be improved... Not sure about the name
        size_t max_bucket_count = std::min<size_t>( 10, aoe.size() );
        bucketed_points waves = optimal_bucketing( buckets, max_bucket_count );
        draw_cone_aoe_curses( origin, waves );
    }
}
} // namespace ranged

bool minimap_requires_animation()
{
    return game_client::animation().minimap_requires_animation();
}

bool terrain_requires_animation()
{
    return game_client::animation().terrain_requires_animation();
}
