#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>

enum class rng_engine_source { simulation, worker, task };

struct rng_task_observation {
    std::uint64_t seed = 0;
    std::uint64_t next_child = 0;
    auto operator<=>( const rng_task_observation & ) const = default; // *NOPAD*
};

/// Owned diagnostic state of the calling RNG context and its stream allocation.
/// This is not player-visible data or a process-wide checkpoint: other threads
/// and suspended outer contexts need separate safe-boundary handling.
struct rng_observation {
    rng_engine_source source = rng_engine_source::simulation;
    unsigned int root_seed = 0;
    std::uint64_t next_root_call = 0;
    std::optional<rng_task_observation> task;
    std::string engine_state;
    auto operator<=>( const rng_observation & ) const = default; // *NOPAD*
};

/// Observe without drawing randomness or allocating a stream. Unavailable outside
/// deterministic mode, so observation cannot initialize a time-seeded engine.
/// Call from the owning thread; a replay-wide checkpoint needs a safe boundary.
auto observe_deterministic_rng() -> std::optional<rng_observation>;
