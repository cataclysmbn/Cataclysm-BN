#pragma once

#include <cstdint>

/// A rule's identity within the session: given when the rule is made or loaded, never saved, and never
/// reused. A copy of a rule takes a new one.
inline auto next_rule_serial() -> std::uint64_t
{
    static auto last = std::uint64_t{ 0 };
    return ++last;
}
