#pragma once

#include <memory>

#include "coordinates.h"
#include "cursesdef.h"

class ui_adaptor;

class live_view
{
    public:
        live_view();
        ~live_view();

        void init();
        void show( const tripoint_bub_ms &p );
        bool is_enabled();
        void hide();

    private:
        tripoint_bub_ms mouse_position;

        catacurses::window win;
        std::unique_ptr<ui_adaptor> ui;
};


