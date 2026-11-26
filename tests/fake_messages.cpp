#include "enums.h"
#include "messages.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class JsonObject;
class JsonOut;

namespace catacurses {
class window;
} // namespace catacurses

/**
 * Stubs to turn all Messages calls into no-ops for unit testing.
 */

auto Messages::recent_messages(size_t) -> std::vector<std::pair<std::string, std::string>> {
    return std::vector<std::pair<std::string, std::string>>();
}
void Messages::add_msg(std::string) {}
void Messages::add_msg(const game_message_params&, std::string) {}
void Messages::clear_messages() {}
void Messages::deactivate() {}
auto Messages::size() -> size_t { return 0; }
auto Messages::has_undisplayed_messages() -> bool { return false; }
void Messages::display_messages() {}
void Messages::display_messages(const catacurses::window&, int, int, int, int) {}
void Messages::serialize(JsonOut&) {}
void Messages::deserialize(const JsonObject&) {}

void add_msg(std::string) {}
void add_msg(const game_message_params&, std::string) {}
