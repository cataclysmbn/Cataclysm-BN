#pragma once

#include "coordinates.h"
#include "type_id.h"

#include <optional>
#include <string>

namespace catacurses {
class window;
} // namespace catacurses

class input_context;
class nc_color;
struct regional_settings;

namespace ui {

namespace omap {

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
void display_zones(const tripoint_abs_omt& center, const tripoint_abs_omt& select, int iZoneIndex);
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
auto choose_point() -> tripoint_abs_omt;

/**
 * Same as above but start at z-level z instead of players
 * current z-level, x and y are taken from the players position.
 */
auto choose_point(int z) -> tripoint_abs_omt;
/**
 * Interactive point choosing; used as the map screen.
 * The map is initially centered on the @ref origin.
 * @returns The absolute coordinates of the chosen point or
 * invalid_point if canceled with Escape (or similar key).
 */
auto choose_point(const tripoint_abs_omt& origin) -> tripoint_abs_omt;

} // namespace omap

} // namespace ui

namespace overmap_ui {
// drawing relevant data, e.g. what to draw.
struct draw_data_t {
    // draw editor.
    bool debug_editor = false;
    // draw scent traces.
    bool debug_scent = false;
    // draw zone location.
    tripoint_abs_omt select = tripoint_abs_omt(-1, -1, -1);
    // draw location of a zone
    int iZoneIndex = -1;
    // draw distribution grids
    bool debug_grids = false;
};

struct tiles_redraw_info {
    tripoint_abs_omt center;
    bool blink = false;
};
extern tiles_redraw_info redraw_info;

auto fmt_omt_coords(const tripoint_abs_omt& coord) -> std::string;

/// The tile the overmap draws for a square: its terrain type id (the "overmap_terrain" tile), the
/// quarter turns and the connection subtile the native tile selection picked.
struct omt_tile {
    std::string id;
    int rotation = 0;
    int subtile = -1;
};
/// The terrain of a square with its connections to the neighbours, as the tiles view selects it.
auto omt_tile_at(const tripoint_abs_omt& omp) -> omt_tile;
/// The tile for a square the player may not have seen: unknown terrain, or the region's display
/// terrain.
auto omt_tile_shown(const tripoint_abs_omt& omp, bool seen, const regional_settings& region)
    -> omt_tile;

auto get_weather_at_point(const point_abs_omt& pos) -> weather_type_id;
auto get_note_display_info(const std::string& note) -> std::tuple<char, nc_color, size_t>;
auto get_note_sprite_id(const std::string& note) -> std::optional<std::string>;
} // namespace overmap_ui
