#pragma once

#include <optional>

#include "cursesdef.h"
#include "point.h"
#include "sdl_wrappers.h"
#include "type_id.h"
#include "units_angle.h"

class vehicle;

#if defined(TILES)

/**
 * A window that displays a vehicle using graphical tiles.
 * Used by the vehicle interaction screen as an alternative to ASCII display.
 */
struct vehicle_preview_window {
    public:
        vehicle_preview_window() = default;
        ~vehicle_preview_window();

        /**
         * Prepare the preview window for display.
         * @param win The catacurses window that defines the display area
         */
        void prepare( const catacurses::window &win );

        /**
         * Display the vehicle with tiles.
         * @param veh The vehicle to display
         * @param cursor_offset Offset from vehicle origin to cursor position (negative of dd)
         * @param highlight_part Index of the part to highlight, or -1 for none
         */
        void display( const vehicle &veh, point cursor_offset, int highlight_part );

        /** Clean up after display (restore zoom level) */
        void clear();

        /** Zoom in (larger tiles) */
        void zoom_in();

        /** Zoom out (smaller tiles) */
        void zoom_out();

        /** Get current zoom level */
        int get_zoom() const {
            return zoom;
        }

    private:
        // The window we're rendering into (for bounds calculation)
        catacurses::window w_preview;

        // Window position in terminal units
        point win_pos;
        // Window size in terminal units
        int win_cols = 0;
        int win_lines = 0;

        // Pixel dimensions of a terminal cell
        int termx_pixels = 0;
        int termy_pixels = 0;

        // Zoom settings
        static constexpr int MIN_ZOOM = 8;
        static constexpr int MAX_ZOOM = 64;
        static constexpr int DEFAULT_ZOOM = 16;
        int zoom = DEFAULT_ZOOM;
        bool saved_original_zoom = false;
        float original_zoom = DEFAULT_ZOOM;

        /**
         * Draw a single vehicle part at the given pixel position.
         * @param vp_id The vehicle part type to draw
         * @param pixel_pos Position in pixels (SDL coordinates)
         * @param part_mod 0=normal, 1=open, 2=broken
         * @param veh_facing Vehicle facing direction
         * @param bg_color Background color tint (for painting)
         * @param fg_color Foreground color tint (for painting)
         */
        void draw_vpart_at_pixel( const vpart_id &vp_id, point pixel_pos,
                                  int part_mod, units::angle veh_facing,
                                  std::optional<SDL_Color> bg_color = std::nullopt,
                                  std::optional<SDL_Color> fg_color = std::nullopt );

        /** Draw a highlight overlay at the given pixel position */
        void draw_highlight_at_pixel( point pixel_pos );

        /** Draw a cursor at the given pixel position */
        void draw_cursor_at_pixel( point pixel_pos );

        /** Calculate the center of the window in pixels */
        point calc_window_center_pixels() const;
};

#endif // TILES
