#pragma once

#include "coordinates.h"

#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

/// Controlled presentation clock and owned causal diagnostics, not a presentation API.
/// Only usable in test_mode. No callbacks or borrowed world objects are retained.
namespace explosion_handler::testing
{

struct clock_options {
    /// Advance on each clock read, including zero-millisecond waits.
    long long clock_read_ms = 1;
    /// Charge the fake redraw instead of invoking a renderer.
    long long redraw_ms = 0;
    /// Exercise is_animated's option/queue-count policy despite test_mode.
    bool bypass_test_mode = true;
    /// Stands in for the native renderer; runs inside the same presentation scope as a real redraw.
    std::function < auto() -> void > render;
};

struct trace_entry {
    std::string kind;
    tripoint_bub_ms position;
    float scheduled_time = 0;
    /// Processed logical event time, not presentation elapsed time.
    float relative_time = 0;
    /// Present only for dispatches; ordinals restart for each FIFO explosion.
    std::optional<std::uint64_t> insertion_ordinal = std::nullopt;
    std::string detail;
    auto operator<=>( const trace_entry & ) const -> std::partial_ordering = default; // *NOPAD*
};

struct clock_observation {
    long long elapsed_ms = 0;
    long long sleep_ms = 0;
    long long redraw_ms = 0;
    int redraws = 0;
    std::vector<trace_entry> trace;
};

/// Single game-thread scope. Presentation counters are separate from the causal trace.
class scoped_explosion_clock
{
    public:
        explicit scoped_explosion_clock( const clock_options &options );
        ~scoped_explosion_clock();
        scoped_explosion_clock( const scoped_explosion_clock & ) = delete;
        auto operator=( const scoped_explosion_clock & ) -> scoped_explosion_clock & = delete; // *NOPAD*
        auto observation() const -> clock_observation;
};

} // namespace explosion_handler::testing
