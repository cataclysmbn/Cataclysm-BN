#pragma once

/// `MCP_ONLY( ... )` is code that only the protocol engine `cataclysm-bn-mcp` runs; native builds do not compile it.
/// `protocol_engine` is the same fact where a plain `if` reads better.
#if defined(CATA_MCP)
#   define MCP_ONLY(...) __VA_ARGS__
inline constexpr auto protocol_engine = true;
#   include <algorithm>
#   include <cstdint>
#   include <limits>
#   include <map>
#   include <random>
#   include <ranges>
#   include <stdexcept>
#   include <tuple>
#   include <vector>
#   include "npctalk.h"
#   include "sounds.h"
#   include "technique_reference.h"
#   include "client_choice.h"
#   include "client_command.h"
#   include "client_input.h"
#   include "client_interaction.h"
#   include "loading_ui_client.h"
#   include "tile_parts.h"
#else
#   define MCP_ONLY(...)
inline constexpr auto protocol_engine = false;
#endif
