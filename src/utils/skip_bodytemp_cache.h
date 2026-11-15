#pragma once

/// Forgets the body temperature inputs that activity skips reuse, so a new or loaded game
/// calculates them again.
auto reset_skip_bodytemp_cache() -> void;
