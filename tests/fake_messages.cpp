#include "enums.h"
#include "message_feed.h"
#include "messages.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class JsonObject;
class JsonOut;

namespace catacurses {
class window;
} // namespace catacurses

/**
 * Stubs for Messages in unit tests. The log itself is real enough to follow: lines keep their
 * serial, a repeat of the last line raises its count, and `feed_since` reads them as the game does.
 * Everything that draws or saves is a no-op.
 */
namespace {
struct fake_line {
    std::uint64_t serial = 0;
    std::string text;
    game_message_type type = m_neutral;
    int count = 1;
};
auto fake_log = std::vector<fake_line>{};
auto fake_serial = std::uint64_t{0};

auto fake_add(std::string text, const game_message_type type) -> void {
    if (text.empty()) { return; }
    if (!fake_log.empty() && fake_log.back().text == text && fake_log.back().type == type) {
        ++fake_log.back().count;
        return;
    }
    fake_log.push_back({.serial = ++fake_serial, .text = std::move(text), .type = type});
}
} // namespace

auto Messages::recent_messages(size_t) -> std::vector<std::pair<std::string, std::string>> {
    return std::vector<std::pair<std::string, std::string>>();
}
void Messages::add_msg(std::string text) { fake_add(std::move(text), m_neutral); }
void Messages::add_msg(const game_message_params& params, std::string text) {
    fake_add(std::move(text), params.type);
}
void Messages::clear_messages() { fake_log.clear(); }
void Messages::deactivate() {}
auto Messages::feed_since(const feed_cursor& after) -> std::vector<feed_entry> {
    auto result = std::vector<feed_entry>{};
    for (const auto& line : fake_log) {
        if (line.serial > after.id || (line.serial == after.id && line.count > after.count)) {
            result.push_back(
                {.id = line.serial, .text = line.text, .type = line.type, .count = line.count});
        }
    }
    return result;
}
auto Messages::size() -> size_t { return fake_log.size(); }
auto Messages::has_undisplayed_messages() -> bool { return false; }
void Messages::display_messages() {}
void Messages::display_messages(const catacurses::window&, int, int, int, int) {}
void Messages::serialize(JsonOut&) {}
void Messages::deserialize(const JsonObject&) {}

void add_msg(std::string) {}
void add_msg(const game_message_params&, std::string) {}
