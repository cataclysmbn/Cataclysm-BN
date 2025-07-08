#pragma once

#include <memory>

#include "cursesdef.h"
#include "point.h"

class ui_adaptor;

class live_view
{
    public:
        live_view();
        ~live_view();

        void init();
        void show( const tripoint &p );
        bool is_enabled();
        void hide();

    private:
        tripoint mouse_position;

        catacurses::window win;
        std::unique_ptr<ui_adaptor> ui;
};


