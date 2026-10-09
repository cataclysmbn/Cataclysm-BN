#include "platform/client_platform.h"

#include <cstdlib>
#include <exception>
#include <iostream>

#if defined(_WIN32)
#    include "platform_win.h"
#endif

namespace {
auto launch(int argc, char** argv) -> int {
    try {
        return run_game(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Cataclysm BN: " << error.what() << '\n';
        return 1;
    }
}
} // namespace

#if defined(_WIN32)
auto APIENTRY
WinMain(HINSTANCE /*instance*/, HINSTANCE /*previous*/, LPSTR /*command*/, int /*show*/) -> int {
    return launch(__argc, __argv);
}
#elif defined(__ANDROID__)
extern "C" auto SDL_main(int argc, char** argv) -> int { return launch(argc, argv); }
#else
auto main(int argc, char** argv) -> int { return launch(argc, argv); }
#endif
