#pragma once

#include <vector>

#include "cursesdef.h"
#include "point.h"

class lightson_game
{
    private:
        catacurses::window w_border;
        catacurses::window w;
        // rows, columns
        point level_size;
        std::vector<bool> level;
        std::vector<point> change_coords;
        // row, column
        point position;
        bool win;

        void new_level();
        void reset_level();
        void generate_change_coords( int changes );
        void draw_level();
        bool check_win();
        void toggle_lights();
        void toggle_lights_at( point pt );
        bool get_value_at( point pt );
        void set_value_at( point pt, bool value );
        void toggle_value_at( point pt );

    public:
        int start_game();
        lightson_game() = default;
};


