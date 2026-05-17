#pragma once

#include "catalua_impl.h"
#include "sounds.h"

namespace cata::detail
{
auto register_sound_channel_end_listener( sol::this_state lua_state,
        sfx::channel channel, sol::protected_function callback ) -> void;
auto poll_sound_channel_listeners( lua_state &state ) -> void;
} // namespace cata::detail
