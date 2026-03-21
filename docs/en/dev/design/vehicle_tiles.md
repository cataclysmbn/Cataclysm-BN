# Vehicle Tiles Display

## Overview

This document describes the implementation plan for adding tile-based rendering to the vehicle interaction screen (`veh_interact`), making it easier to visualize vehicles during construction and modification.

## Goal

Add graphical tile rendering to the vehicle construction screen as an alternative to ASCII display, improving usability especially for players unfamiliar with the ASCII representations.

## Current Architecture

### Vehicle Interaction Screen (`src/veh_interact.cpp`)

The current `display_veh()` function (line 2284) uses ASCII rendering:

```cpp
void veh_interact::display_veh()
{
    werase( w_disp );
    const point h_size = point( getmaxx( w_disp ), getmaxy( w_disp ) ) / 2;

    // Iterate over structural parts
    std::vector<int> structural_parts = veh->all_standalone_parts();
    for( auto &structural_part : structural_parts ) {
        const int p = structural_part;
        int sym = veh->part_sym( p );      // ASCII symbol
        nc_color col = veh->part_color( p ); // ncurses color

        const point q = ( veh->part( p ).mount + dd ).rotate( 3 );
        mvwputch( w_disp, h_size + q, col, special_symbol( sym ) );
    }
}
```

Key members in `veh_interact`:

- `catacurses::window w_disp` - The vehicle display window
- `point dd` - Current cursor offset (negative of cursor position)
- `int cpart` - Currently selected part index
- `vehicle *veh` - The vehicle being modified

### Tile Rendering System

**Core class**: `cata_tiles` (`src/cata_tiles.cpp`)

**Vehicle part rendering** (`draw_vpart()` at line 3612):

```cpp
bool cata_tiles::draw_vpart( const tripoint &p, lit_level ll, int &height_3d,
                             const bool ( &invisible )[5], int z_drop )
{
    const vpart_id &vp_id = veh.part_id_string( veh_part, z_drop > 0, part_mod );
    const int subtile = part_mod == 1 ? open_ : part_mod == 2 ? broken : 0;
    const int rotation = std::round( to_degrees( veh.face.dir() ) );
    const std::string vpname = "vp_" + vp_id.str();

    const tile_search_params tile = {vpname, C_VEHICLE_PART, empty_string, subtile, rotation};
    return draw_from_id_string( tile, p, bgCol, fgCol, ll, true, z_drop, false, height_3d );
}
```

Key parameters:

- **Tile ID**: `"vp_" + vpart_id.str()` (e.g., `"vp_frame"`, `"vp_engine_v8"`)
- **Category**: `C_VEHICLE_PART`
- **Subtile**: 0 (normal), `open_` (open door), `broken` (damaged)
- **Rotation**: Degrees from `veh.face.dir()`

### Template: Character Preview (`src/character_preview.cpp`)

This file demonstrates how to render tiles in a UI screen outside the main map:

```cpp
class char_preview_adapter : public cata_tiles
{
public:
    static char_preview_adapter *convert( cata_tiles *ct ) {
        return static_cast<char_preview_adapter *>( ct );
    }

    void display_avatar_preview_with_overlays( const avatar &ch, const point &p, bool with_clothing ) {
        const tile_search_params tile { ent_name, C_NONE, "", corner, rotation };
        draw_from_id_string(
            tile, tripoint( p, 0 ), std::nullopt, std::nullopt,
            lit_level::BRIGHT, false, 0, true, height_3d );
        //                              ^ as_independent_entity = true
    }
};
```

Key patterns:

1. **Adapter class**: Static cast to access protected `draw_from_id_string`
2. **Zoom control**: `tilecontext->set_draw_scale( zoom )`
3. **Pixel positioning**: Convert terminal units to pixels via `termx_to_pixel_value()`
4. **Independent rendering**: `as_independent_entity = true` allows rendering outside map bounds

---

## Implementation Plan

### 1.1 Create Vehicle Preview Adapter Class

**File**: `src/vehicle_preview.h` / `src/vehicle_preview.cpp`

```cpp
#if defined(TILES)

class veh_preview_adapter : public cata_tiles
{
public:
    static veh_preview_adapter *convert( cata_tiles *ct ) {
        return static_cast<veh_preview_adapter *>( ct );
    }

    // Draw a single vehicle part at pixel position
    void draw_vpart_at_pixel( const vpart_id &id, const point &pixel_pos,
                               int part_mod, units::angle rotation, bool highlight );

    // Draw entire vehicle centered in a window
    void draw_vehicle_preview( const vehicle &veh, const catacurses::window &win,
                                point cursor_offset, int highlight_part );
};

struct vehicle_preview_window {
    catacurses::window w_preview;

    void prepare( int nlines, int ncols );
    void display( const vehicle &veh, point cursor_offset, int highlight_part ) const;
    void zoom_in();
    void zoom_out();
    void clear() const;

private:
    int zoom = 64;  // Default zoom level
    static constexpr int MIN_ZOOM = 16;
    static constexpr int MAX_ZOOM = 128;

    point calc_center_pixel() const;
};

#endif // TILES
```

### 1.2 Add Graphics Option

**File**: `src/options.cpp`

Add new option in `add_options_graphics()`:

```cpp
#if defined(TILES)
    add( "VEHICLE_EDIT_TILES", graphics, translate_marker( "Graphical vehicle display" ),
         translate_marker( "If true, the vehicle interaction screen will display vehicle parts using graphical tiles instead of ASCII symbols." ),
         true, COPT_CURSES_HIDE );
#endif
```

This option:

- Only appears when compiled with TILES support
- Hidden in curses-only builds (`COPT_CURSES_HIDE`)
- Defaults to `true` (use tiles when available)

### 1.3 Modify `veh_interact` Class

**File**: `src/veh_interact.h`

Add members:

```cpp
#if defined(TILES)
    std::unique_ptr<vehicle_preview_window> tile_preview;
#endif
```

Add methods:

```cpp
#if defined(TILES)
    void display_veh_tiles();  // New tile-based rendering
#endif
```

### 1.4 Implement `display_veh_tiles()`

**File**: `src/veh_interact.cpp`

```cpp
#if defined(TILES)
void veh_interact::display_veh_tiles()
{
    if( !tile_preview ) {
        tile_preview = std::make_unique<vehicle_preview_window>();
        tile_preview->prepare( getmaxy( w_disp ), getmaxx( w_disp ) );
    }

    tile_preview->display( *veh, dd, cpart );
}
#endif

void veh_interact::display_veh()
{
#if defined(TILES)
    if( is_draw_tiles_mode() && get_option<bool>( "VEHICLE_EDIT_TILES" ) ) {
        display_veh_tiles();
        return;
    }
#endif
    // ... existing ASCII implementation ...
}
```

### 1.5 Rendering Logic

In `vehicle_preview_window::display()`:

```cpp
void vehicle_preview_window::display( const vehicle &veh, point cursor_offset,
                                       int highlight_part ) const
{
    werase( w_preview );

    // Get window dimensions in pixels
    const int win_w_px = getmaxx( w_preview ) * termx_to_pixel_value();
    const int win_h_px = getmaxy( w_preview ) * termy_to_pixel_value();
    const point center_px = { win_w_px / 2, win_h_px / 2 };

    // Get tile dimensions at current zoom
    const int tile_w = tilecontext->get_tile_width();
    const int tile_h = tilecontext->get_tile_height();

    auto *adapter = veh_preview_adapter::convert( &*tilecontext );

    // Draw all vehicle parts
    for( int p : veh.all_standalone_parts() ) {
        const vehicle_part &part = veh.part( p );
        const point mount = part.mount;

        // Calculate position relative to cursor
        // Note: rotate(3) is used in ASCII mode for display orientation
        const point rel = ( mount + cursor_offset ).rotate( 3 );

        // Convert to pixel position (centered in window)
        const point pixel_pos = center_px + point( rel.x * tile_w, rel.y * tile_h );

        // Get part rendering info
        char part_mod = 0;
        const vpart_id &vp_id = veh.part_id_string( p, false, part_mod );
        const units::angle rotation = veh.face.dir();
        const bool is_highlighted = ( p == highlight_part );

        adapter->draw_vpart_at_pixel( vp_id, pixel_pos, part_mod, rotation, is_highlighted );
    }

    // Draw crosshair at center (current cursor position)
    draw_cursor_crosshair( center_px );

    wnoutrefresh( w_preview );
}
```

---

## File Changes Summary

### New Files

- `src/vehicle_preview.h` - Tile preview adapter class
- `src/vehicle_preview.cpp` - Tile preview implementation

### Modified Files

- `src/veh_interact.h` - Add tile preview member
- `src/veh_interact.cpp` - Integrate tile display
- `src/options.cpp` - Add `VEHICLE_EDIT_TILES` option
- `src/Makefile` / `CMakeLists.txt` - Add new source files

### Options

- `VEHICLE_EDIT_TILES` (Graphics) - Toggle graphical vehicle display (default: true)

---

## Technical Considerations

### Conditional Compilation

All tile-related code must be wrapped in `#if defined(TILES)`:

```cpp
#if defined(TILES)
#include "cata_tiles.h"
#include "sdltiles.h"
// ... tile code ...
#endif
```

### Coordinate Systems

The vehicle screen uses multiple coordinate systems:

1. **Mount coordinates**: Vehicle-local (origin at vehicle center)
2. **Screen coordinates**: Terminal units (cols/rows)
3. **Pixel coordinates**: SDL pixels (for tile rendering)

Conversion:

```cpp
// Mount -> Screen (with cursor offset and rotation)
point screen = ( mount + cursor_offset ).rotate( 3 ) + window_center;

// Screen -> Pixel
point pixel = screen * point( termx_to_pixel_value(), termy_to_pixel_value() );
```

### Performance

- Cache tile lookups where possible
- Only redraw on changes (use dirty flags)
- Limit zoom levels to prevent excessive texture scaling

### Fallback Behavior

- If tiles fail to load, fall back to ASCII automatically
- Curses-only build must still compile and work

---

## Testing Checklist

- [ ] Tiles render correctly for all vehicle parts
- [ ] Zoom in/out works smoothly
- [ ] Cursor position is clearly visible
- [ ] Part highlighting works (selected part stands out)
- [ ] `VEHICLE_EDIT_TILES` option appears in Graphics settings (tiles build only)
- [ ] Disabling option falls back to ASCII display
- [ ] Curses-only build still compiles (no TILES defined)
- [ ] Performance is acceptable for large vehicles (50+ parts)
- [ ] Open doors show correct "open" tile variant
- [ ] Broken parts show correct "broken" tile variant
- [ ] Vehicle rotation is displayed correctly
