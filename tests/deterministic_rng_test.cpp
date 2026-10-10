#include "catch/catch.hpp"
#include "item_factory.h"
#include "loading_ui_client.h"
#include "options.h"
#include "rng.h"
#include "rng_observation.h"
#include "sdlsound.h"
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

TEST_CASE("audio variant selection does not advance simulation RNG", "[rng][sound][replay]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(92821);
    const auto expected = rng_get_engine();

    for (const auto sample_index : std::views::iota(0, 64)) {
        static_cast<void>(sample_index);
        CHECK(sfx::presentation_random_effect_index(3) < 3);
    }

    CHECK(rng_get_engine() == expected);
}

TEST_CASE("loading image order does not advance simulation RNG", "[rng][replay]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(92822);
    const auto expected = rng_get_engine();
    auto paths = std::vector<std::string>{"one.png", "two.png", "three.png"};

    for (const auto sample_index : std::views::iota(0, 64)) {
        static_cast<void>(sample_index);
        game_client::shuffle_loading_image_paths(paths);
    }

    CHECK(rng_get_engine() == expected);
}

TEST_CASE("item definition validation does not advance simulation RNG", "[rng][item][replay]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(92823);
    const auto expected = rng_get_engine();

    item_controller->check_definitions();

    CHECK(rng_get_engine() == expected);
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

TEST_CASE("RNG observation does not enable deterministic mode", "[rng][rng_observation]") {
    const auto restore = restore_rng{};
    const auto before = rng_get_engine();
    CHECK_FALSE(observe_deterministic_rng());
    CHECK_FALSE(rng_deterministic_seed_active());
    CHECK(rng_get_engine() == before);
}

TEST_CASE(
    "RNG observation preserves and exposes the complete selected engine",
    "[rng][rng_observation]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(0);
    const auto engine_before = rng_get_engine();
    const auto before = observe_deterministic_rng();
    REQUIRE(before);
    CHECK(before->source == rng_engine_source::simulation);
    CHECK(before->root_seed == 1);
    CHECK_FALSE(before->task);
    CHECK(observe_deterministic_rng() == before);
    CHECK(rng_get_engine() == engine_before);

    auto decoded = cata_default_random_engine{};
    auto encoded = std::istringstream{before->engine_state};
    REQUIRE(static_cast<bool>(encoded >> decoded));
    CHECK(decoded == engine_before);

    static_cast<void>(rng_bits());
    const auto after = observe_deterministic_rng();
    REQUIRE(after);
    CHECK(after->engine_state != before->engine_state);
    CHECK(after->next_root_call == before->next_root_call);
    CHECK(observe_deterministic_rng() == after);
}

TEST_CASE(
    "RNG observation detects stream allocation without an engine draw", "[rng][rng_observation]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto before = observe_deterministic_rng();
    REQUIRE(before);
    REQUIRE(rng_next_deterministic_call_seed(42));
    const auto after = observe_deterministic_rng();
    REQUIRE(after);
    CHECK(after->engine_state == before->engine_state);
    CHECK(after->next_root_call == before->next_root_call + 1);
    CHECK(after != before);
    CHECK(observe_deterministic_rng() == after);
}

TEST_CASE(
    "RNG observation distinguishes nested task seed and child state", "[rng][rng_observation]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto simulation = observe_deterministic_rng();
    REQUIRE(simulation);
    {
        const auto outer_scope = rng_deterministic_task_scope(0);
        const auto before = observe_deterministic_rng();
        REQUIRE(before);
        REQUIRE(before->task);
        CHECK(before->source == rng_engine_source::task);
        CHECK(before->task->seed == 0);
        CHECK(before->task->next_child == 0);
        {
            const auto inner_scope = rng_deterministic_task_scope(1);
            const auto inner = observe_deterministic_rng();
            REQUIRE(inner);
            REQUIRE(inner->task);
            CHECK(inner->engine_state == before->engine_state);
            CHECK(inner->task->seed == 1);
            CHECK(inner != before);
        }
        CHECK(observe_deterministic_rng() == before);
        REQUIRE(rng_next_deterministic_call_seed(42));
        const auto after = observe_deterministic_rng();
        REQUIRE(after);
        REQUIRE(after->task);
        CHECK(after->engine_state == before->engine_state);
        CHECK(after->next_root_call == before->next_root_call);
        CHECK(after->task->next_child == 1);
        CHECK(observe_deterministic_rng() == after);
        CHECK_THROWS_AS(
            [&]() {
                const auto inner_scope = rng_deterministic_task_scope(99);
                static_cast<void>(rng_bits());
                static_cast<void>(rng_next_deterministic_call_seed(42));
                throw std::runtime_error("unwind the nested RNG context");
            }(),
            std::runtime_error);
        CHECK(observe_deterministic_rng() == after);
    }
    CHECK(observe_deterministic_rng() == simulation);
}

TEST_CASE("RNG observations retain owned worker and task states", "[rng][rng_observation]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto simulation = observe_deterministic_rng();
    REQUIRE(simulation);
    struct observations {
        std::optional<rng_observation> worker;
        std::optional<rng_observation> task;
        std::optional<rng_observation> restored;
    };
    const auto result =
        std::async(std::launch::async, []() {
            rng_set_worker_seed(311);
            auto captured = observations{
                .worker = observe_deterministic_rng(),
                .task = std::nullopt,
                .restored = std::nullopt};
            {
                const auto scope = rng_deterministic_task_scope(17);
                static_cast<void>(rng_bits());
                captured.task = observe_deterministic_rng();
            }
            captured.restored = observe_deterministic_rng();
            return captured;
        }).get();
    REQUIRE(result.worker);
    REQUIRE(result.task);
    REQUIRE(result.task->task);
    CHECK(result.worker->source == rng_engine_source::worker);
    CHECK_FALSE(result.worker->task);
    CHECK(result.task->source == rng_engine_source::task);
    CHECK(result.task->task->seed == 17);
    CHECK(result.restored == result.worker);
    CHECK(observe_deterministic_rng() == simulation);
    const auto saved_worker = result.worker;
    static_cast<void>(rng_bits());
    CHECK(result.worker == saved_worker);
}

TEST_CASE(
    "zero-worker keyed and per-index tasks restore the owning context", "[rng][thread_pool]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto root = observe_deterministic_rng();
    REQUIRE(root);
    auto serial = cata_thread_pool(0);
    CHECK(serial.num_workers() == 0);
    const auto keyed =
        serial
            .submit_returning({.stream = 31, .id = 37}, []() { return observe_deterministic_rng(); })
            .get();
    REQUIRE(keyed);
    CHECK(keyed->source == rng_engine_source::task);
    CHECK(observe_deterministic_rng() == root);
    auto per_index = std::vector<std::optional<rng_observation>>(4);
    parallel_for_chunked(0, 4, 1, [&](const auto index) {
        per_index[index] = observe_deterministic_rng();
        const auto nested = serial.submit_returning({.stream = 41, .id = 43}, sample).get();
        static_cast<void>(nested);
    });
    parallel_for(0, 1, [](const auto /*index*/) { static_cast<void>(sample()); });
    for (const auto& observation : per_index) {
        REQUIRE(observation);
        CHECK(observation->source == rng_engine_source::task);
        REQUIRE(observation->task);
    }
    const auto after = observe_deterministic_rng();
    REQUIRE(after);
    CHECK(after->engine_state == root->engine_state);
    CHECK(after->next_root_call == root->next_root_call + 2);
}
