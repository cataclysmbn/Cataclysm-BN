#include "runtime_handlers.h"
#include "cursesdef.h"
#include "debug.h"
#include "init.h"
#include "game.h"

[[ noreturn ]]
void exit_handler( int status )
{
    DynamicDataLoader::get_instance().unload_data();
    deinitDebug();
    g.reset();
    catacurses::endwin();
    exit( status );
}
