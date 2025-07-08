#pragma once

#include "cursesdef.h"
#include "detached_ptr.h"
#include "type_id.h"

class item;
class Character;

/** Gets size of single width terminal unit size value in pixels **/
auto termx_to_pixel_value() -> int;
/** Gets size of single height terminal unit size value in pixels **/
auto termy_to_pixel_value() -> int;

struct character_preview_window {
        enum OrientationType : std::uint8_t {
            TOP_LEFT,
            TOP_RIGHT,
            BOTTOM_LEFT,
            BOTTOM_RIGHT
        };
        struct Margin {
            int left = 0;
            int right = 0;
            int top = 0;
            int bottom = 0;
        };
        struct Orientation {
            OrientationType type = TOP_RIGHT;
            Margin margin = Margin{};
        };

        catacurses::window w_preview;

        void init( Character *character );
        /** Window preparations before displaying. Sets desirable position. Could also be usefull for ui-rescale **/
        void prepare( int nlines, int ncols, const Orientation *orientation, int hide_below_ncols );
        void zoom_in();
        void zoom_out();
        void toggle_clothes();
        void display() const;
        /** Use it as you done with preview **/
        void clear() const;
        auto clothes_showing() const -> bool;

    private:
        point pos;
        int termx_pixels = 0;
        int termy_pixels = 0;
        const int MIN_ZOOM = 32;
        const int MAX_ZOOM = 128;
        const int DEFAULT_ZOOM = 128;
        int zoom = DEFAULT_ZOOM;
        int hide_below_ncols = 0;
        int ncols_width = 0;
        int nlines_width = 0;
        Character *character = nullptr;
        std::vector<detached_ptr<item>> clothes;
        std::vector<trait_id> spells;
        bool show_clothes = true;

        auto calc_character_pos() const -> point ;
};


