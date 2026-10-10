#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "enums.h"

namespace Messages
{

/// How far a reader has followed the native message log: the last message and its repeat count.
struct feed_cursor {
    std::uint64_t id = 0;
    int count = 0;
};
/// A message as the native log shows it, without the "x N" suffix.
struct feed_entry {
    std::uint64_t id = 0;
    std::string text;
    game_message_type type = m_neutral;
    int count = 1;
};

/// Messages added after `after` in log order, plus the cursor's own message when it repeated since.
/// Messages hidden by the cooldown are skipped as the native sidebar does.
auto feed_since( const feed_cursor &after ) -> std::vector<feed_entry>;

} // namespace Messages
