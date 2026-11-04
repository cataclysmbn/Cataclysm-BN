#if defined(_WIN32)
#    include "client_backend.h"
#    include "platform_win.h"

auto getWindowHandle() -> HWND {
    return game_client::backend_selected()
             ? static_cast<HWND>(game_client::active_backend().native_window_handle())
             : nullptr;
}
#endif
