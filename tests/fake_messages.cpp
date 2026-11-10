#if defined(CATA_MCP)
#    include "calendar.h"
#endif
#include "enums.h"
#if defined(CATA_MCP)
#    include "message_feed.h"
#endif
#include "messages.h"

#include <cstddef>
#if defined(CATA_MCP)
#    include <cstdint>
#    include <optional>
#endif
#include <string>
#include <utility>
#include <vector>

class JsonObject;
class JsonOut;

namespace catacurses {
class window;
} // namespace catacurses

/**
#if defined(CATA_MCP)
 * Stubs for Messages in unit tests. The log itself is real enough to follow: lines keep their
 * serial, a repeat of the last line raises its count, and `feed_since` reads them as the game does.
 * Everything that draws or saves is a no-op.
#else
 * Stubs to turn all Messages calls into no-ops for unit testing.
#endif
 */
#if defined(CATA_MCP)
namespace {
struct fake_line {
    std::uint64_t serial = 0;
    std::string text;
    game_message_type type = m_neutral;
    int count = 1;
    int turn = 0;
};
auto fake_log = std::vector<fake_line>{};
auto fake_serial = std::uint64_t{0};

auto fake_add(std::string text, const game_message_type type) -> void {
    if (text.empty()) { return; }
    const auto turn = to_turn<int>(calendar::turn);
    if (!fake_log.empty() && fake_log.back().text == text && fake_log.back().type == type) {
        ++fake_log.back().count;
        fake_log.back().turn = turn;
        return;
    }
    fake_log.push_back(
        {.serial = ++fake_serial, .text = std::move(text), .type = type, .turn = turn});
}
} // namespace
#endif

auto Messages::recent_messages(size_t) -> std::vector<std::pair<std::string, std::string>> {
    return std::vector<std::pair<std::string, std::string>>();
}
#if defined(CATA_MCP)
void Messages::add_msg(std::string text) { fake_add(std::move(text), m_neutral); }
void Messages::add_msg(const game_message_params& params, std::string text) {
    fake_add(std::move(text), params.type);
}
void Messages::clear_messages() { fake_log.clear(); }
#else
void Messages::add_msg(std::string) {}
void Messages::add_msg(const game_message_params&, std::string) {}
void Messages::clear_messages() {}
#endif
void Messages::deactivate() {}
#if defined(CATA_MCP)
auto Messages::feed_since(const feed_cursor& after) -> std::vector<feed_entry> {
    auto result = std::vector<feed_entry>{};
    for (const auto& line : fake_log) {
        if (line.serial > after.id || (line.serial == after.id && line.count > after.count)) {
            result.push_back(
                {.id = line.serial,
                 .text = line.text,
                 .type = line.type,
                 .count = line.count,
                 .turn = line.turn});
        }
    }
    return result;
}
auto Messages::size() -> size_t { return fake_log.size(); }
#else
auto Messages::size() -> size_t { return 0; }
#endif
auto Messages::has_undisplayed_messages() -> bool { return false; }
void Messages::display_messages() {}
void Messages::display_messages(const catacurses::window&, int, int, int, int) {}
void Messages::serialize(JsonOut&) {}
void Messages::deserialize(const JsonObject&) {}

#if defined(CATA_MCP)
void add_msg(std::string text) { Messages::add_msg(std::move(text)); }
void add_msg(const game_message_params& params, std::string text) {
    Messages::add_msg(params, std::move(text));
}
#else
void add_msg(std::string) {}
void add_msg(const game_message_params&, std::string) {}
#endif
