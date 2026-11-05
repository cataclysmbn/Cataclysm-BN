#include "cata_utility.h"
#include "enums.h"
#include "message_helpers.h"
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

namespace {
thread_local auto* captured_messages = static_cast<std::vector<std::string>*>(nullptr);
} // namespace

/// Messages remain no-ops unless a test explicitly captures them.
auto capture_messages_during(const std::function<void()>& callback) -> std::vector<std::string> {
    auto result = std::vector<std::string>();
    const auto restore = restore_on_out_of_scope(captured_messages);
    captured_messages = &result;
    callback();
    return result;
}

auto Messages::recent_messages(size_t) -> std::vector<std::pair<std::string, std::string>> {
    return std::vector<std::pair<std::string, std::string>>();
}
auto Messages::add_msg(std::string message) -> void {
    if (captured_messages != nullptr) { captured_messages->push_back(std::move(message)); }
}
auto Messages::add_msg(const game_message_params& /*params*/, std::string message) -> void {
    Messages::add_msg(std::move(message));
}
void Messages::clear_messages() {}
void Messages::deactivate() {}
auto Messages::size() -> size_t { return 0; }
auto Messages::has_undisplayed_messages() -> bool { return false; }
void Messages::display_messages() {}
void Messages::display_messages(const catacurses::window&, int, int, int, int) {}
void Messages::serialize(JsonOut&) {}
void Messages::deserialize(const JsonObject&) {}

auto add_msg(std::string message) -> void { Messages::add_msg(std::move(message)); }
auto add_msg(const game_message_params& params, std::string message) -> void {
    Messages::add_msg(params, std::move(message));
}
