#pragma once

#include <optional>

#include "coordinates.h"
#include "type_id.h"

namespace catacurses
{
class window;
} // namespace catacurses

class input_context;
class nc_color;

namespace ui
{

namespace omap
{

/**
 * Display overmap centered at the player's position.
 */
void display();
/**
 * Display overmap like with @ref display() and display hordes.
 */
void display_hordes();
/**
 * Display overmap like with @ref display() and display the weather.
 */
void display_weather();
/**
 * Display overmap like with @ref display() and display the weather that is within line of sight.
 */
void display_visible_weather();
/**
 * Display overmap like with @ref display() and display scent traces.
 */
void display_scents();
/**
 * Display overmap like with @ref display() and display distribution grids.
 */
void display_distribution_grids();
/**
 * Display overmap like with @ref display() and display the given zone.
 */
void display_zones( const tripoint_abs_omt &center, const tripoint_abs_omt &select,
                    int iZoneIndex );
/**
 * Display overmap like with @ref display() and enable the overmap editor.
 */
void display_editor();

/**
 * Interactive point choosing; used as the map screen.
 * The map is initially center at the players position.
 * @returns The absolute coordinates of the chosen point or
 * invalid_point if canceled with Escape (or similar key).
 */
tripoint_abs_omt choose_point();

/**
 * Same as above but start at z-level z instead of players
 * current z-level, x and y are taken from the players position.
 */
tripoint_abs_omt choose_point( int z );
/**
 * Interactive point choosing; used as the map screen.
 * The map is initially centered on the @ref origin.
 * @returns The absolute coordinates of the chosen point or
 * invalid_point if canceled with Escape (or similar key).
 */
tripoint_abs_omt choose_point( const tripoint_abs_omt &origin );

} // namespace omap

} // namespace ui

namespace overmap_ui
{
// drawing relevant data, e.g. what to draw.
struct draw_data_t {
    // draw editor.
    bool debug_editor = false;
    // draw scent traces.
    bool debug_scent = false;
    // draw zone location.
    tripoint_abs_omt select = tripoint_abs_omt( -1, -1, -1 );
    // draw location of a zone
    int iZoneIndex = -1;
    // draw distribution grids
    bool debug_grids = false;
};

#if defined(TILES)
struct tiles_redraw_info {
    tripoint_abs_omt center;
    bool blink = false;
};
extern tiles_redraw_info redraw_info;
#endif

auto fmt_omt_coords( const tripoint_abs_omt &coord ) -> std::string;

weather_type_id get_weather_at_point( const point_abs_omt &pos );
std::tuple<char, nc_color, size_t> get_note_display_info( const std::string &note );
std::optional<std::string> get_note_sprite_id( const std::string &note );
} // namespace overmap_ui
