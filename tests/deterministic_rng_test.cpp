#include "catch/catch.hpp"
#include "debug.h"
#include "presentation_rng.h"
#include "rng.h"
#include "thread_pool.h"

#include <algorithm>
#include <array>
#include <future>
#include <new>
#include <numeric>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

struct restore_rng {
    cata_default_random_engine saved = rng_get_engine();
    restore_rng() { REQUIRE_FALSE(rng_deterministic_seed_active()); }
    ~restore_rng() {
        rng_clear_deterministic_seed();
        rng_get_engine() = saved;
    }
};

struct rolls {
    int integer;
    double normal;
    auto operator<=>(const rolls&) const = default; // *NOPAD*
};

auto sample() -> rolls { return {.integer = rng(1, 1000000), .normal = normal_roll(10.0, 2.0)}; }

auto run_keyed_tasks(unsigned int workers, bool reverse) -> std::vector<rolls> {
    rng_set_deterministic_seed(424242);
    auto pool = cata_thread_pool(workers);
    auto indices = std::vector<int>(16);
    std::iota(indices.begin(), indices.end(), 0);
    if (reverse) { std::ranges::reverse(indices); }
    auto tasks = std::vector<std::future<rolls>>(indices.size());
    for (const auto index : indices) {
        tasks[index] = pool.submit_returning(
            {.stream = 0x74657374, .id = static_cast<std::uint64_t>(index)}, sample);
    }
    auto result = std::vector<rolls>{};
    for (auto& task : tasks) { result.push_back(task.get()); }
    return result;
}

} // namespace

TEST_CASE("deterministic RNG restores the caller engine", "[rng][thread_pool]") {
    const auto restore = restore_rng{};
    const auto original = rng_get_engine();
    CHECK_FALSE(rng_deterministic_seed_for_current_context({.stream = 1, .id = 2}));
    rng_set_deterministic_seed(0);
    const auto zero = sample();
    rng_set_deterministic_seed(1);
    CHECK(sample() == zero);
    rng_clear_deterministic_seed();
    CHECK(rng_get_engine() == original);
    CHECK_FALSE(rng_deterministic_seed_active());
}

TEST_CASE(
    "nested deterministic tasks restore both random and child streams", "[rng][thread_pool]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(17);
    auto expected = rolls{};
    {
        const auto scope = rng_deterministic_task_scope(1234);
        (void)sample();
        expected = sample();
    }
    {
        const auto scope = rng_deterministic_task_scope(1234);
        (void)sample();
        const auto first = rng_next_deterministic_call_seed(99);
        REQUIRE(first);
        CHECK(*first == rng_deterministic_child_seed(1234, {.stream = 99, .id = 0}));
        {
            const auto inner = rng_deterministic_task_scope(9999);
            (void)sample();
            (void)rng_next_deterministic_call_seed(99);
        }
        CHECK(sample() == expected);
        const auto second = rng_next_deterministic_call_seed(99);
        REQUIRE(second);
        CHECK(*second == rng_deterministic_child_seed(1234, {.stream = 99, .id = 1}));
    }
}

TEST_CASE("keyed RNG tasks ignore worker count and submission order", "[rng][thread_pool]") {
    const auto restore = restore_rng{};
    const auto serial = run_keyed_tasks(0, false);
    CHECK(run_keyed_tasks(1, false) == serial);
    CHECK(run_keyed_tasks(4, false) == serial);
    CHECK(run_keyed_tasks(4, true) == serial);
}

TEST_CASE("deterministic parallel loops retain per-index random streams", "[rng][thread_pool]") {
    const auto restore = restore_rng{};
    const auto run = [](const int chunk_size) {
        rng_set_deterministic_seed(731);
        auto result = std::vector<rolls>(32);
        parallel_for_chunked(0, static_cast<int>(result.size()), chunk_size, [&](const auto index) {
            result[index] = sample();
        });
        return result;
    };
    CHECK(run(3) == run(7));
    CHECK(run(32) == run(3));
}

TEST_CASE(
    "a draw on a worker outside a task scope is reported only in deterministic mode",
    "[rng][thread_pool][deterministic_rng]") {
    const auto restore = restore_rng{};
    auto pool = cata_thread_pool(1);
    const auto unscoped = [&pool]() {
        return capture_debugmsg_during([&pool]() { pool.submit_returning(sample).get(); });
    };
    CHECK(unscoped().empty());
    rng_set_deterministic_seed(424242);
    CHECK_THAT(unscoped(), Catch::Contains("outside a deterministic task scope"));
    const auto keyed = capture_debugmsg_during([&pool]() {
        pool.submit_returning({.stream = 1, .id = 2}, sample).get();
    });
    CHECK(keyed.empty());
}

TEST_CASE("presentation randomness leaves the game engine alone", "[rng]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(99);
    const auto expected = sample();
    rng_set_deterministic_seed(99);
    for (auto draw = 0; draw < 100; ++draw) {
        const auto value = presentation_rng(3, 1);
        CHECK(value >= 1);
        CHECK(value <= 3);
    }
    CHECK(sample() == expected);
}
