#include "input_pending.h"

#include "avatar.h"
#include "game.h"
#include "options.h"
#include "player_activity.h"

#if defined(TILES)
#    include "sdl_wrappers.h"
#endif

auto main_prompt_reads_input = false;

auto should_defer_redraw_for_queued_input() -> bool
{
    if( !get_option<bool>( "DEFER_REDRAW_ON_QUEUED_INPUT" ) || g == nullptr ||
        g->uquit != QUIT_NO || g->safe_mode == SAFE_MODE_STOP ) {
        return false;
    }
    const auto &u = get_avatar();
    if( u.activity && *u.activity ) {
        return false;
    }
#if defined(TILES)
    SDL_PumpEvents();
    return SDL_HasEvent( SDL_EVENT_KEY_DOWN );
#else
    return false;
#endif
}
