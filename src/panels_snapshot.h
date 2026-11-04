#pragma once

#include "engine_client_state.h"

class avatar;

/// The classic sidebar's content as values, from the getters the native panels draw with.
auto sidebar_snapshot( const avatar &u ) -> engine_client::sidebar_value;
