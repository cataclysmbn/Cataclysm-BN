#include "catch/catch.hpp"
#include "rng.h"
#include "rng_observation.h"

#include <future>
#include <locale>
#include <ranges>
#include <sstream>
#include <vector>

namespace {

struct restore_rng_observation_test {
    cata_default_random_engine saved = rng_get_engine();
    restore_rng_observation_test() { REQUIRE_FALSE(rng_deterministic_seed_active()); }
    ~restore_rng_observation_test() {
        rng_clear_deterministic_seed();
        rng_get_engine() = saved;
    }
};

struct native_rng_trace {
    std::vector<unsigned int> bits;
    std::vector<int> integers;
    std::vector<double> real;
    std::vector<unsigned int> children;
    cata_default_random_engine engine;
    std::optional<rng_observation> observation;
    bool passive = true;
};

/// Compare the owned diagnostic against the real engine, without using a draw.
auto observation_matches_engine(const rng_observation& observation) -> bool {
    auto input = std::istringstream{observation.engine_state};
    input.imbue(std::locale::classic());
    auto decoded = cata_default_random_engine{};
    input >> decoded;
    return !input.fail() && decoded == rng_get_engine();
}

auto native_trace(const bool observing) -> native_rng_trace {
    auto trace = native_rng_trace{};
    for (const auto index : std::views::iota(0, 16)) {
        if (observing) {
            const auto engine = rng_get_engine();
            const auto first = observe_deterministic_rng();
            const auto second = observe_deterministic_rng();
            trace.passive =
                trace.passive && first && first == second && rng_get_engine() == engine
                && observation_matches_engine(*first);
        }
        trace.bits.push_back(rng_bits());
        trace.integers.push_back(rng(1000000, -1000000));
        trace.integers.push_back(dice(3, 6));
        trace.real.push_back(rng_float(10.0, -10.0));
        trace.real.push_back(normal_roll(10.0, 2.0));
        trace.real.push_back(exponential_roll(0.25));
        const auto child = rng_next_deterministic_call_seed(0x74657374);
        if (child) { trace.children.push_back(*child); }
        static_cast<void>(index);
    }
    trace.engine = rng_get_engine();
    trace.observation = observe_deterministic_rng();
    return trace;
}

auto check_same_native_trace(const native_rng_trace& expected, const native_rng_trace& observed)
    -> void {
    CHECK(observed.passive);
    CHECK(observed.bits == expected.bits);
    CHECK(observed.integers == expected.integers);
    CHECK(observed.real == expected.real);
    CHECK(observed.children == expected.children);
    CHECK(observed.engine == expected.engine);
    CHECK(observed.observation == expected.observation);
}

} // namespace

TEST_CASE(
    "RNG observation is unavailable outside deterministic execution", "[rng][rng_observation]") {
    const auto restore = restore_rng_observation_test{};
    const auto engine = rng_get_engine();
    CHECK_FALSE(observe_deterministic_rng());
    CHECK_FALSE(observe_deterministic_rng());
    CHECK(rng_get_engine() == engine);
}

TEST_CASE(
    "RNG observation preserves native simulation draws and root allocation order",
    "[rng][rng_observation]") {
    const auto restore = restore_rng_observation_test{};
    rng_set_deterministic_seed(731);
    const auto expected = native_trace(false);
    rng_set_deterministic_seed(731);
    const auto observed = native_trace(true);
    check_same_native_trace(expected, observed);
    REQUIRE(observed.observation);
    CHECK(observed.observation->source == rng_engine_source::simulation);
    CHECK(observed.observation->root_seed == 731);
    CHECK(observed.observation->next_root_call == 16);
    CHECK_FALSE(observed.observation->task);
}

TEST_CASE(
    "RNG observation preserves suspended task engines and child allocation order",
    "[rng][rng_observation][thread_pool]") {
    const auto restore = restore_rng_observation_test{};
    rng_set_deterministic_seed(731);
    const auto main = observe_deterministic_rng();
    auto expected = native_rng_trace{};
    {
        const auto task = rng_deterministic_task_scope(1234);
        expected = native_trace(false);
    }
    {
        const auto task = rng_deterministic_task_scope(1234);
        const auto observed = native_trace(true);
        check_same_native_trace(expected, observed);
        REQUIRE(observed.observation);
        CHECK(observed.observation->source == rng_engine_source::task);
        CHECK(observed.observation->next_root_call == 0);
        REQUIRE(observed.observation->task);
        CHECK(observed.observation->task->seed == 1234);
        CHECK(observed.observation->task->next_child == 16);
        {
            const auto inner = rng_deterministic_task_scope(9999);
            const auto nested = native_trace(true);
            REQUIRE(nested.observation);
            REQUIRE(nested.observation->task);
            CHECK(nested.observation->task->seed == 9999);
        }
        CHECK(observe_deterministic_rng() == observed.observation);
    }
    CHECK(observe_deterministic_rng() == main);
}

TEST_CASE(
    "RNG observation preserves native worker draws and task precedence",
    "[rng][rng_observation][thread_pool]") {
    const auto restore = restore_rng_observation_test{};
    rng_set_deterministic_seed(731);
    const auto main_engine = rng_get_engine();
    const auto worker =
        std::async(std::launch::async, []() {
            rng_set_worker_seed(92821);
            const auto expected = native_trace(false);
            rng_set_worker_seed(92821);
            return expected;
        }).get();
    // Root stream allocation is process-wide, even for independent worker engines.
    rng_set_deterministic_seed(731);
    const auto observed =
        std::async(std::launch::async, []() {
            rng_set_worker_seed(92821);
            return native_trace(true);
        }).get();
    check_same_native_trace(worker, observed);
    REQUIRE(observed.observation);
    CHECK(observed.observation->source == rng_engine_source::worker);
    CHECK(observed.observation->next_root_call == 16);
    CHECK_FALSE(observed.observation->task);
    CHECK(rng_get_engine() == main_engine);

    const auto task_source =
        std::async(std::launch::async, []() {
            rng_set_worker_seed(92821);
            const auto before = observe_deterministic_rng();
            auto source = rng_engine_source::simulation;
            {
                const auto task = rng_deterministic_task_scope(1234);
                source = observe_deterministic_rng()->source;
                static_cast<void>(rng_bits());
            }
            return before == observe_deterministic_rng() && source == rng_engine_source::task;
        }).get();
    CHECK(task_source);
}
