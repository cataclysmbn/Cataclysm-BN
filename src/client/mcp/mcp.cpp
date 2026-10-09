#if defined(CATA_MCP)

#    include "client_backend.h"
#    include "client_memory.h"
#    include "cursesdef.h"
#    include "engine_client_session.h"
#    include "game_constants.h"
#    include "mcp_session.h"
#    include "options.h"
#    include "output.h"

namespace game_client::mcp {

auto initialize_native() -> void {
    const auto width = std::max(FULL_SCREEN_WIDTH, get_option<int>("TERMINAL_X"));
    const auto height = std::max(FULL_SCREEN_HEIGHT, get_option<int>("TERMINAL_Y"));
    memory::initialize(width, height);
}

auto shutdown_native() -> void {
    bn::mcp::finish_session();
    memory::shutdown();
}

auto set_cursor_native(const int visibility) -> void { memory::set_cursor(visibility); }

auto draw_window_native(const catacurses::window& window) -> void { memory::draw_window(window); }

auto clear_window_native(const catacurses::window& window) -> void { memory::clear_window(window); }

auto present_native() -> void { memory::present(); }

auto set_timeout_native(const int timeout) -> void { memory::set_timeout(timeout); }

auto pump_events_native() -> void {}

auto read_input_native() -> input_event {
    return engine_client::process_session().delivered(memory::read_input());
}

} // namespace game_client::mcp

namespace game_client {
namespace {

class mcp_backend final: public backend {
public:
    auto prepare() -> void override {
        if (!test_mode) { bn::mcp::start_session(); }
    }
    auto initialize() -> void override { mcp::initialize_native(); }
    auto shutdown() -> void override { mcp::shutdown_native(); }
    auto present() -> void override { mcp::present_native(); }
    auto draw_window(const catacurses::window& window) -> void override {
        mcp::draw_window_native(window);
    }
    auto clear_window(const catacurses::window& window) -> void override {
        mcp::clear_window_native(window);
    }
    auto set_cursor(const int visibility) -> void override { mcp::set_cursor_native(visibility); }
    auto set_timeout(const int timeout_ms) -> void override { mcp::set_timeout_native(timeout_ms); }
    auto read_input(const int timeout_ms) -> input_event override {
        mcp::set_timeout_native(timeout_ms);
        return mcp::read_input_native();
    }
    auto pump_events() -> void override { mcp::pump_events_native(); }
    auto resize(const point cell_size) -> void override {
        memory::resize(cell_size.x, cell_size.y);
    }
    auto projected_size() const -> point override { return memory::screen_size(); }
    auto capabilities() const -> client_capabilities override {
        return {.tiles = false, .mouse = true, .gamepad = false};
    }
};

} // namespace

auto make_mcp_backend() -> backend_ptr { return std::make_unique<mcp_backend>(); }

} // namespace game_client

#endif // CATA_MCP
