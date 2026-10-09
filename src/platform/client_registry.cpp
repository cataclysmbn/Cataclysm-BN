#include "client_backend.h"

#include <stdexcept>

namespace game_client {
auto register_builtin_backends() -> void {
#if defined(CATA_HAS_TILES)
    register_backend(client_kind::tiles, make_tiles_backend);
#endif
#if defined(CATA_HAS_CURSES)
    register_backend(client_kind::curses, make_curses_backend);
#endif
#if defined(CATA_HAS_MCP)
    register_backend(client_kind::mcp, make_mcp_backend);
#endif
#if defined(CATA_HAS_IMGUI)
    register_backend(client_kind::imgui, make_imgui_backend);
#endif
}
auto default_client_kind() -> client_kind {
#if defined(CATA_HAS_TILES)
    return client_kind::tiles;
#elif defined(CATA_HAS_CURSES)
    return client_kind::curses;
#elif defined(CATA_HAS_IMGUI)
    return client_kind::imgui;
#elif defined(CATA_HAS_MCP)
    return client_kind::mcp;
#else
    throw std::logic_error("No client backend was compiled");
#endif
}
} // namespace game_client
