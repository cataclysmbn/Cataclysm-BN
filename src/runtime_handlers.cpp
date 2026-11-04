#include "runtime_handlers.h"
#include "cursesdef.h"
#include "debug.h"
#include "init.h"
#include "game.h"
#include "replay/replay.h"

#include <exception>
#include <iostream>

[[ noreturn ]]
void exit_handler( int status )
{
    try {
        replay::finish();
    } catch( const std::exception &error ) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    DynamicDataLoader::get_instance().unload_data();
    deinitDebug();
    g.reset();
    catacurses::endwin();
    exit( status );
}
