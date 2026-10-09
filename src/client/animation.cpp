#include "cached_options.h"
#include "client_animation.h"
#include "game.h"
#include "input.h"
#include "options.h"
#include "output.h"
#include "popup.h"
#include "posix_time.h"
#include "translations.h"
#include "ui_manager.h"

#include <algorithm>
#include <utility>

namespace game_client {
namespace {
auto service = std::unique_ptr<animation_service>{};
}

auto animation() -> animation_service& // *NOPAD*
{
    static auto fallback = animation_service{};
    return service ? *service : fallback;
}

auto set_animation(std::unique_ptr<animation_service> replacement) -> void {
    service = std::move(replacement);
}

auto progress_animation(const animation_progress_options& options) -> void {
    if (options.draw_popup) {
        static_popup popup;
        popup.wait_message("%s", _("Hang on a bit…")).on_top(true);
    }
    g->invalidate_main_ui_adaptor();
    ui_manager::redraw_invalidated();
    refresh_display();
    auto remaining =
        static_cast<long int>(get_option<int>("ANIMATION_DELAY")) * options.multiplier * 1000000L;
    while (remaining > 0) {
        const auto sleep_for = std::min(remaining, 100'000'000L);
        const auto delay = timespec{0, sleep_for};
        nanosleep(&delay, nullptr);
        inp_mngr.pump_events();
        remaining -= sleep_for;
    }
}
} // namespace game_client
