#include "catch/catch.hpp"
#if defined(TILES)
#    include "client/tiles/loading_images.h"
#endif
#include "item_factory.h"
#include "options.h"
#include "rng.h"
#include "rng_observation.h"
#include "rng_task_trace.h"
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

#if defined(TILES)
TEST_CASE("loading image order does not advance simulation RNG", "[rng][tiles][replay]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(92822);
    const auto expected = rng_get_engine();
    auto paths = std::vector<std::string>{"one.png", "two.png", "three.png"};

    for (const auto sample_index : std::views::iota(0, 64)) {
        static_cast<void>(sample_index);
        game_client::tiles::shuffle_loading_image_paths(paths);
    }

    CHECK(rng_get_engine() == expected);
}
#endif

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

namespace {

struct task_trace_run {
    rng_task_trace_snapshot trace;
    std::optional<rng_observation> root;
    rolls result;
};

struct task_trace_run_options {
    unsigned int workers = 2;
    bool enabled = true;
    bool extra_draw = false;
    bool extra_allocation = false;
    bool observe_often = false;
};

auto run_task_trace(const task_trace_run_options& options) -> task_trace_run {
    rng_set_deterministic_seed(1729);
    const auto trace = options.enabled ? begin_rng_task_trace({.capacity = 32}) : nullptr;
    auto result = rolls{};
    {
        auto pool = cata_thread_pool(options.workers);
        result =
            pool.submit_returning(
                    {.stream = 42, .id = 7},
                    [&]() {
                        if (options.observe_often) {
                            for ([[maybe_unused]] const auto index : std::views::iota(0, 20)) {
                                static_cast<void>(trace->observe());
                                static_cast<void>(observe_deterministic_rng());
                            }
                        }
                        const auto value = sample();
                        if (options.extra_draw) { static_cast<void>(rng_bits()); }
                        if (options.extra_allocation) {
                            static_cast<void>(rng_next_deterministic_call_seed(99));
                        }
                        return value;
                    })
                .get();
    } // Joining the actual workers also joins their RNG scope epilogues.
    if (trace) { trace->stop(); }
    return {.trace = trace ? trace->observe() : rng_task_trace_snapshot{},
            .root = observe_deterministic_rng(),
            .result = result};
}

} // namespace

TEST_CASE("task RNG trace retains draws erased by keyed scope restoration", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    const auto baseline = run_task_trace({});
    const auto changed = run_task_trace({.extra_draw = true});
    REQUIRE(baseline.trace.all_recorded_scopes_restored());
    REQUIRE(changed.trace.all_recorded_scopes_restored());
    REQUIRE(baseline.trace.records.size() == 1);
    REQUIRE(changed.trace.records.size() == 1);
    const auto& before = baseline.trace.records.front();
    const auto& after = changed.trace.records.front();
    CHECK(baseline.root == changed.root);
    CHECK(baseline.result == changed.result);
    CHECK(before.kind == rng_task_trace_kind::keyed_task);
    CHECK(before.stream == 42);
    CHECK(before.key == 7);
    CHECK(before.occurrence == 0);
    CHECK_FALSE(before.parent);
    CHECK(before.initial_context == after.initial_context);
    CHECK(before.final_context != after.final_context);
    CHECK(before.phase == rng_task_trace_phase::restored);
    CHECK(before.outer_context == before.restored_context);
    REQUIRE(before.final_context);
    auto decoded = cata_default_random_engine{};
    auto encoded = std::istringstream{before.final_context->engine_state};
    REQUIRE(static_cast<bool>(encoded >> decoded));
    CHECK(decoded != cata_default_random_engine{before.seed});
}

TEST_CASE("task RNG trace detects allocation-only changes", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    const auto baseline = run_task_trace({});
    const auto changed = run_task_trace({.extra_allocation = true});
    REQUIRE(baseline.trace.records.front().final_context);
    REQUIRE(changed.trace.records.front().final_context);
    const auto& before = *baseline.trace.records.front().final_context;
    const auto& after = *changed.trace.records.front().final_context;
    CHECK(baseline.root == changed.root);
    CHECK(before.engine_state == after.engine_state);
    REQUIRE(before.task);
    REQUIRE(after.task);
    CHECK(after.task->next_child == before.task->next_child + 1);
}

TEST_CASE("task RNG trace is optional and repeated observations are neutral", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    CHECK_FALSE(begin_rng_task_trace());
    const auto disabled = run_task_trace({.enabled = false});
    const auto enabled = run_task_trace({});
    const auto observed = run_task_trace({.observe_often = true});
    const auto serial = run_task_trace({.workers = 0});
    CHECK(disabled.trace.records.empty());
    CHECK(disabled.result == enabled.result);
    CHECK(disabled.root == enabled.root);
    CHECK(observed.result == enabled.result);
    CHECK(observed.root == enabled.root);
    CHECK(serial.result == enabled.result);
    CHECK(serial.root == enabled.root);
    CHECK(observed.trace.records.front().final_context
          == enabled.trace.records.front().final_context);
    CHECK(
        serial.trace.records.front().final_context == enabled.trace.records.front().final_context);
}

TEST_CASE("task RNG trace owns nested and exception restoration evidence", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto root = observe_deterministic_rng();
    const auto trace = begin_rng_task_trace();
    REQUIRE(trace);
    CHECK_FALSE(begin_rng_task_trace());
    {
        const auto outer = rng_deterministic_task_scope(99);
        CHECK_FALSE(begin_rng_task_trace());
        const auto before = observe_deterministic_rng();
        {
            const auto zero = rng_deterministic_task_scope(0);
            static_cast<void>(normal_roll(0.0, 1.0));
        }
        CHECK(observe_deterministic_rng() == before);
        CHECK_THROWS_AS(
            [&]() {
                const auto one = rng_deterministic_task_scope(1);
                static_cast<void>(normal_roll(0.0, 1.0));
                static_cast<void>(rng_next_deterministic_call_seed(12));
                throw std::runtime_error("task unwind");
            }(),
            std::runtime_error);
        CHECK(observe_deterministic_rng() == before);
        auto serial_pool = cata_thread_pool(0);
        for ([[maybe_unused]] const auto index : std::views::iota(0, 2)) {
            auto future = serial_pool.submit_returning({.stream = 88, .id = 3}, []() -> void {
                static_cast<void>(rng_bits());
                throw std::runtime_error("packaged exception");
            });
            CHECK_THROWS_AS(future.get(), std::runtime_error);
        }
        CHECK(observe_deterministic_rng() == before);
    }
    CHECK(observe_deterministic_rng() == root);
    trace->stop();
    const auto snapshot = trace->observe();
    REQUIRE(snapshot.all_recorded_scopes_restored());
    REQUIRE(snapshot.records.size() == 5);
    const auto& zero = snapshot.records[1];
    const auto& one = snapshot.records[2];
    CHECK(zero.parent == 0);
    CHECK(one.parent == 0);
    CHECK(zero.seed == 0);
    CHECK(one.seed == 1);
    REQUIRE(zero.initial_context);
    REQUIRE(one.initial_context);
    REQUIRE(zero.final_context);
    REQUIRE(one.final_context);
    CHECK(zero.initial_context->engine_state == one.initial_context->engine_state);
    CHECK(zero.final_context->engine_state == one.final_context->engine_state);
    CHECK_FALSE(zero.exception);
    CHECK(one.exception);
    CHECK(zero.outer_context == zero.restored_context);
    CHECK(one.outer_context == one.restored_context);
    CHECK(snapshot.records[3].exception);
    CHECK(snapshot.records[4].exception);
    CHECK(snapshot.records[3].occurrence == 0);
    CHECK(snapshot.records[4].occurrence == 1);
}

TEST_CASE("parallel RNG traces retain real per-index scope consumption", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    struct loop_result {
        std::shared_ptr<rng_task_trace> trace;
        std::optional<rng_observation> root;
    };
    const auto run = [](const bool extra_draw) {
        rng_set_deterministic_seed(731);
        const auto trace = begin_rng_task_trace({.capacity = 128});
        REQUIRE(trace);
        // Serial fallback is supported. An explicit CLI worker fixture still
        // checks that the singleton was constructed with its requested count.
        const auto requested_workers = get_option<int>("THREAD_POOL_WORKERS");
        if (get_option<bool>("MULTITHREADING_ENABLED") && requested_workers > 0) {
            REQUIRE(get_thread_pool().num_workers() > 0);
            CHECK(get_thread_pool().num_workers() == static_cast<unsigned int>(requested_workers));
        }
        parallel_for_chunked(0, 8, 2, [&](const auto index) {
            static_cast<void>(sample());
            if (extra_draw && index == 5) { static_cast<void>(rng_bits()); }
            // Nested pool submission on the actual index-owning context.
            auto serial_pool = cata_thread_pool(0);
            serial_pool.submit_returning({.stream = 100, .id = 1}, sample).get();
        });
        parallel_for(0, 4, [](const auto /*index*/) { static_cast<void>(sample()); });
        // The helpers' latches do not acknowledge chunk-wrapper restoration.
        trace->stop();
        return loop_result{.trace = trace, .root = observe_deterministic_rng()};
    };
    const auto baseline = run(false);
    const auto changed = run(true);
    CHECK(baseline.root == changed.root);
    const auto before = baseline.trace->observe();
    const auto after = changed.trace->observe();
    const auto select = [](const auto& snapshot) -> const rng_task_trace_record& {
        const auto record = std::ranges::find_if(snapshot.records, [](const auto& entry) {
            return entry.kind == rng_task_trace_kind::parallel_index && entry.key == 5;
        });
        REQUIRE(record != snapshot.records.end());
        return *record;
    };
    const auto& before_index = select(before);
    const auto& after_index = select(after);
    REQUIRE(before_index.parent);
    CHECK(before.records[*before_index.parent].kind == rng_task_trace_kind::parallel_call);
    CHECK(before_index.initial_context == after_index.initial_context);
    CHECK(before_index.final_context != after_index.final_context);
    CHECK(before_index.phase == rng_task_trace_phase::restored);
    CHECK(
        std::ranges::count_if(
            before.records,
            [](const auto& record) { return record.kind == rng_task_trace_kind::parallel_index; })
        == 12);
}

TEST_CASE(
    "task RNG trace reports outstanding scopes independently of readiness", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto trace = begin_rng_task_trace();
    REQUIRE(trace);
    auto released = std::promise<void>{};
    const auto release = released.get_future().share();
    auto ready = std::promise<void>{};
    const auto body_ready = ready.get_future();
    {
        auto pool = cata_thread_pool(1);
        auto future = pool.submit_returning({.stream = 1, .id = 2}, [&]() {
            auto serial_pool = cata_thread_pool(0);
            auto nested = serial_pool.submit_returning({.stream = 3, .id = 4}, sample);
            nested.get(); // A real packaged future is ready while outer scope is live.
            ready.set_value();
            release.wait();
            // Descendants retain the original window even after stop().
            serial_pool.submit_returning({.stream = 5, .id = 6}, sample).get();
            static_cast<void>(rng_bits());
        });
        body_ready.wait();
        trace->stop();
        const auto snapshot = trace->observe();
        released.set_value(); // Always release before assertions can unwind the pool.
        CHECK(pool.queue_size() == 0);
        CHECK(snapshot.outstanding == 1);
        CHECK_FALSE(snapshot.all_recorded_scopes_restored());
        REQUIRE(snapshot.records.size() == 2);
        CHECK(snapshot.records[0].phase == rng_task_trace_phase::running);
        CHECK_FALSE(snapshot.records[0].final_context);
        CHECK(snapshot.records[1].phase == rng_task_trace_phase::restored);
        future.get();
    }
    const auto final = trace->observe();
    CHECK(final.all_recorded_scopes_restored());
    REQUIRE(final.records.size() == 3);
    CHECK(final.records[2].parent == 0);
    CHECK(final.records[2].key == 6);
}

TEST_CASE("task RNG trace overflow and incomplete windows deny proof", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    {
        const auto trace = begin_rng_task_trace({.capacity = 1});
        REQUIRE(trace);
        {
            const auto outer = rng_deterministic_task_scope(1);
            const auto inner = rng_deterministic_task_scope(2);
            CHECK(trace->observe().outstanding == 1); // Lower bound after overflow.
            auto serial_pool = cata_thread_pool(0);
            auto failed =
                serial_pool.submit_returning({.stream = 4, .id = 5}, [] [[noreturn]] () -> void {
                    throw std::runtime_error("unrecorded overflow exception");
                });
            CHECK_THROWS_AS(failed.get(), std::runtime_error);
        }
        trace->stop();
        const auto snapshot = trace->observe();
        CHECK(snapshot.records.size() == 1);
        CHECK(snapshot.outstanding == 0);
        CHECK(snapshot.overflow);
        CHECK_FALSE(snapshot.records.front().exception);
        CHECK_FALSE(snapshot.all_recorded_scopes_restored());
    }
    {
        const auto trace = begin_rng_task_trace({.capacity = 0});
        REQUIRE(trace);
        { const auto scope = rng_deterministic_task_scope(7); }
        trace->stop();
        CHECK(trace->observe().records.empty());
        CHECK(trace->observe().overflow);
        CHECK_FALSE(trace->observe().all_recorded_scopes_restored());
    }
    {
        const auto trace = begin_rng_task_trace();
        REQUIRE(trace);
        { const auto abandoned = reserve_rng_task_trace({.seed = 3}); }
        trace->stop();
        CHECK(trace->observe().incomplete);
        CHECK(trace->observe().outstanding == 0);
        CHECK(trace->observe().records.front().phase == rng_task_trace_phase::cancelled);
        CHECK_FALSE(trace->observe().all_recorded_scopes_restored());
    }
    {
        const auto trace = begin_rng_task_trace();
        REQUIRE(trace);
        const auto pending = reserve_rng_task_trace({.seed = 3});
        trace->stop();
        // Even a same-seed reset with stopped but pending work denies proof.
        rng_set_deterministic_seed(1729);
        {
            const auto binding = rng_task_trace_binding(pending);
            const auto scope = rng_deterministic_task_scope(3);
        }
        CHECK(trace->observe().incomplete);
        CHECK_FALSE(trace->observe().all_recorded_scopes_restored());
    }
}

TEST_CASE("worker packaged exceptions retain keyed submission occurrences", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto root = observe_deterministic_rng();
    const auto trace = begin_rng_task_trace();
    REQUIRE(trace);
    {
        auto pool = cata_thread_pool(2);
        auto futures = std::vector<std::future<void>>{};
        for ([[maybe_unused]] const auto index : std::views::iota(0, 2)) {
            futures.push_back(pool.submit_returning({.stream = 41, .id = 6}, []() -> void {
                static_cast<void>(normal_roll(0.0, 1.0));
                throw std::runtime_error("worker packaged exception");
            }));
        }
        for (auto& future : futures) { CHECK_THROWS_AS(future.get(), std::runtime_error); }
    }
    trace->stop();
    const auto snapshot = trace->observe();
    REQUIRE(snapshot.all_recorded_scopes_restored());
    REQUIRE(snapshot.records.size() == 2);
    CHECK(snapshot.records[0].occurrence == 0);
    CHECK(snapshot.records[1].occurrence == 1);
    for (const auto& record : snapshot.records) {
        CHECK(record.exception);
        CHECK(record.phase == rng_task_trace_phase::restored);
        CHECK(record.outer_context == record.restored_context);
        REQUIRE(record.initial_context);
        REQUIRE(record.final_context);
        CHECK(record.initial_context->engine_state != record.final_context->engine_state);
    }
    CHECK(observe_deterministic_rng() == root);
}

TEST_CASE("parallel exception traces preserve the outer allocation context", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto trace = begin_rng_task_trace();
    REQUIRE(trace);
    {
        const auto outer = rng_deterministic_task_scope(77);
        const auto before = observe_deterministic_rng();
        REQUIRE(before);
        REQUIRE(before->task);
        CHECK_THROWS_AS(
            parallel_for_chunked(
                0, 4, 1,
                [](const auto index) {
                    static_cast<void>(rng_bits());
                    if (index == 2) { throw std::runtime_error("index exception"); }
                }),
            std::runtime_error);
        const auto after = observe_deterministic_rng();
        REQUIRE(after);
        REQUIRE(after->task);
        CHECK(after->engine_state == before->engine_state);
        CHECK(after->task->seed == before->task->seed);
        CHECK(after->task->next_child == before->task->next_child + 1);
    }
    trace->stop();
    const auto snapshot = trace->observe();
    CHECK_FALSE(snapshot.incomplete);
    CHECK(
        std::ranges::count_if(
            snapshot.records,
            [](const auto& record) {
                return record.kind == rng_task_trace_kind::parallel_index && record.exception;
            })
        == 1);
    CHECK(
        std::ranges::count_if(
            snapshot.records,
            [](const auto& record) {
                return record.kind == rng_task_trace_kind::parallel_call && record.exception;
            })
        == 1);
}

TEST_CASE("task RNG trace distinguishes queued and running reservations", "[rng][rng_trace]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto trace = begin_rng_task_trace();
    REQUIRE(trace);
    auto entered = std::promise<void>{};
    auto ready = entered.get_future();
    auto released = std::promise<void>{};
    const auto release = released.get_future().share();
    {
        auto pool = cata_thread_pool(1);
        auto first = pool.submit_returning({.stream = 1, .id = 0}, [&]() {
            entered.set_value();
            release.wait();
        });
        ready.wait();
        auto second = pool.submit_returning({.stream = 1, .id = 1}, sample);
        trace->stop();
        const auto snapshot = trace->observe();
        const auto queued = pool.queue_size();
        released.set_value();
        CHECK(queued == 1);
        CHECK(snapshot.outstanding == 2);
        CHECK_FALSE(snapshot.all_recorded_scopes_restored());
        REQUIRE(snapshot.records.size() == 2);
        CHECK(snapshot.records[0].phase == rng_task_trace_phase::running);
        CHECK(snapshot.records[1].phase == rng_task_trace_phase::submitted);
        CHECK_FALSE(snapshot.records[1].initial_context);
        CHECK_FALSE(snapshot.records[1].final_context);
        first.get();
        second.get();
    }
    CHECK(trace->observe().all_recorded_scopes_restored());
}

#if defined(CATA_RNG_TRACE_TESTING)
namespace {

/// Forward a real serialized prefix, then fail inside the engine's formatted insertion.
class short_rng_streambuf: public std::streambuf {
public:
    explicit short_rng_streambuf(std::size_t limit): limit_(limit) {}
    auto attach(std::streambuf* destination) -> std::streambuf* { // *NOPAD*
        destination_ = destination;
        return this;
    }
    auto written() const -> std::size_t { return written_; }
    auto rejected() const -> std::size_t { return rejected_; }

protected:
    auto overflow(int_type value) -> int_type override {
        if (traits_type::eq_int_type(value, traits_type::eof())) {
            return traits_type::not_eof(value);
        }
        if (written_ == limit_) {
            ++rejected_;
            return traits_type::eof();
        }
        const auto result = destination_->sputc(traits_type::to_char_type(value));
        if (!traits_type::eq_int_type(result, traits_type::eof())) { ++written_; }
        return result;
    }

private:
    std::streambuf* destination_ = nullptr;
    const std::size_t limit_;
    std::size_t written_ = 0;
    std::size_t rejected_ = 0;
};

/// Release the paused submitting thread even if snapshot capture/assertion throws.
class release_trace_submission {
public:
    explicit release_trace_submission(std::promise<void>& released): released_(released) {}
    ~release_trace_submission() { release(); }
    auto release() -> void {
        if (!done_) {
            released_.set_value();
            done_ = true;
        }
    }

private:
    std::promise<void>& released_;
    bool done_ = false;
};

} // namespace

TEST_CASE(
    "failed engine stream writes cannot certify complete RNG trace evidence",
    "[rng][rng_trace][rng_trace_serialization_red]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    auto serial = cata_thread_pool(0);
    const auto expected = serial.submit_returning({.stream = 67, .id = 71}, sample).get();
    const auto root = observe_deterministic_rng();
    REQUIRE(root);
    const auto trace = begin_rng_task_trace({.capacity = 1});
    REQUIRE(trace);
    // Submission, outer, initial, final and restored captures all use actual engine insertion.
    const auto failure_at = GENERATE(1, 2, 3, 4, 5);
    const auto prefix_bytes = GENERATE(std::size_t{0}, std::size_t{1});
    auto captures = 0;
    auto output = short_rng_streambuf{prefix_bytes};
    auto snapshot = rng_task_trace_snapshot{};
    {
        auto probe = rng_task_trace_test::probe{
            .configure_engine_output =
                [&](auto& stream) {
                    if (++captures == failure_at) { stream.rdbuf(output.attach(stream.rdbuf())); }
                },
        };
        const auto installed = rng_task_trace_test::scoped_probe(probe);
        CHECK(serial.submit_returning({.stream = 67, .id = 71}, sample).get() == expected);
        trace->stop();
        snapshot = trace->observe();
    }
    CAPTURE(failure_at, prefix_bytes, captures, output.written(), output.rejected());
    REQUIRE(captures >= failure_at);
    REQUIRE(output.written() == prefix_bytes);
    REQUIRE(output.rejected() > 0);
    CHECK(snapshot.incomplete);
    CHECK_FALSE(snapshot.all_recorded_scopes_restored());
    CHECK(snapshot.outstanding == 0);
    for (const auto& record : snapshot.records) {
        const auto contexts = std::array{
            &record.submission_context, &record.outer_context,    &record.initial_context,
            &record.final_context,      &record.restored_context,
        };
        for (const auto* context : contexts) {
            CHECK_FALSE((context->has_value() && (*context)->engine_state.size() == prefix_bytes));
        }
    }
    CHECK(observe_deterministic_rng() == root);
}

TEST_CASE(
    "closed RNG trace cannot certify then admit a parentless root",
    "[rng][rng_trace][rng_trace_red][rng_trace_admission]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto trace = begin_rng_task_trace();
    REQUIRE(trace);
    auto owner_acquired = std::promise<void>{};
    const auto paused = owner_acquired.get_future();
    auto released = std::promise<void>{};
    const auto resume = released.get_future().share();
    auto before = rng_task_trace_snapshot{};
    auto after = rng_task_trace_snapshot{};
    {
        auto worker = cata_thread_pool(1);
        auto release_on_exit = release_trace_submission(released);
        auto submission = worker.submit_returning([&]() {
            auto probe = rng_task_trace_test::probe{
                .after_root_owner_acquired =
                    [&]() {
                        owner_acquired.set_value();
                        resume.wait();
                    },
            };
            const auto installed = rng_task_trace_test::scoped_probe(probe);
            // The outer unkeyed worker job has no RNG task parent. Submit the
            // real keyed work through a synchronous pool on its owning thread.
            auto serial = cata_thread_pool(0);
            serial.submit_returning({.stream = 19, .id = 23}, sample).get();
        });
        paused.wait(); // Root owner resolved, but ledger admission has not begun.
        trace->stop();
        before = trace->observe();
        release_on_exit.release();
        submission.get();
        after = trace->observe();
    }
    const auto gained_parentless_root =
        after.records.size() > before.records.size()
        && std::ranges::any_of(after.records, [](const auto& record) {
               return !record.parent && record.kind == rng_task_trace_kind::keyed_task;
           });
    CAPTURE(before.accepting, before.outstanding, before.records.size(),
            before.all_recorded_scopes_restored(), after.outstanding, after.records.size(),
            gained_parentless_root);
    // A linearizable implementation may either reject the late root or keep
    // the earlier reservation outstanding. It must not certify and then grow.
    CHECK_FALSE((before.all_recorded_scopes_restored() && gained_parentless_root));
}

TEST_CASE(
    "distinct task keys have capacity-linear occurrence lookup work",
    "[rng][rng_trace][rng_trace_red][rng_trace_lookup]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto capacity = GENERATE(std::size_t{4096}, std::size_t{65536});
    const auto trace = begin_rng_task_trace({.capacity = capacity});
    REQUIRE(trace);
    auto probe = rng_task_trace_test::probe{};
    const auto installed = rng_task_trace_test::scoped_probe(probe);
    auto serial = cata_thread_pool(0);
    for (const auto key : std::views::iota(std::size_t{0}, capacity)) {
        serial.submit_returning({.stream = 29, .id = key}, []() {}).get();
    }
    trace->stop();
    const auto snapshot = trace->observe();
    REQUIRE(snapshot.records.size() == capacity);
    CHECK(snapshot.all_recorded_scopes_restored());
    CHECK(std::ranges::all_of(snapshot.records, [](const auto& record) {
        return record.occurrence == 0 && record.phase == rng_task_trace_phase::restored;
    }));
    // Count actual occurrence-key comparisons, never wall-clock duration.
    // A deliberately generous amortized eight-comparison budget permits a
    // bounded occurrence index, but denies a broad retained-record scan.
    const auto comparison_budget = std::uint64_t{8} * capacity;
    CAPTURE(capacity, probe.occurrence_comparisons, comparison_budget);
    CHECK(probe.occurrence_comparisons <= comparison_budget);
    // Keep the committed equality-work assertion, and also account for actual
    // hash invocations so an unused or uninstrumented index cannot fake green.
    CAPTURE(probe.occurrence_hashes, probe.occurrence_entries, probe.peak_occurrence_entries);
    CHECK(probe.occurrence_hashes >= capacity);
    CHECK(probe.occurrence_hashes + probe.occurrence_comparisons <= comparison_budget);
    CHECK(probe.occurrence_entries == capacity);
    CHECK(probe.peak_occurrence_entries <= capacity);
}
TEST_CASE(
    "occurrence index rolls back failed appends without orphan keys or skipped counts",
    "[rng][rng_trace][rng_trace_index]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto root = observe_deterministic_rng();
    const auto trace = begin_rng_task_trace({.capacity = 3});
    REQUIRE(trace);
    auto probe = rng_task_trace_test::probe{};
    const auto installed = rng_task_trace_test::scoped_probe(probe);
    auto serial = cata_thread_pool(0);
    const auto fail_append = [] [[noreturn]] () -> void { throw std::bad_alloc{}; };
    probe.before_record_append = fail_append;
    // A failing distinct-key history may greatly exceed capacity without
    // retaining even one orphan index entry or suppressing actual task work.
    for (const auto key : std::views::iota(std::uint64_t{0}, std::uint64_t{128})) {
        CHECK(serial.submit_returning({.stream = 47, .id = key}, []() { return 42; }).get() == 42);
        CHECK(probe.occurrence_entries == 0);
    }
    CHECK(probe.peak_occurrence_entries == 1);
    CHECK(trace->observe().outstanding == 0);
    CHECK(trace->observe().records.empty());
    CHECK(trace->observe().incomplete);
    probe.before_record_append = nullptr;
    serial.submit_returning({.stream = 47, .id = 37}, sample).get();
    probe.before_record_append = fail_append;
    serial.submit_returning({.stream = 47, .id = 37}, sample).get();
    CHECK(probe.occurrence_entries == 1);
    probe.before_record_append = nullptr;
    serial.submit_returning({.stream = 47, .id = 37}, sample).get();
    serial.submit_returning({.stream = 47, .id = 38}, sample).get();
    const auto entries_before_overflow = probe.occurrence_entries;
    serial.submit_returning({.stream = 47, .id = 39}, sample).get();
    trace->stop();
    const auto snapshot = trace->observe();
    REQUIRE(snapshot.records.size() == 3);
    CHECK(snapshot.records[0].occurrence == 0);
    CHECK(snapshot.records[1].occurrence == 1);
    CHECK(snapshot.records[2].occurrence == 0);
    CHECK(probe.occurrence_entries == entries_before_overflow);
    CHECK(probe.occurrence_entries == 2);
    CHECK(probe.peak_occurrence_entries <= 3);
    CHECK(snapshot.outstanding == 0);
    CHECK(snapshot.overflow);
    CHECK(snapshot.incomplete);
    CHECK_FALSE(snapshot.all_recorded_scopes_restored());
    CHECK(observe_deterministic_rng() == root);
}
TEST_CASE(
    "trace capture allocation failure denies restoration evidence without suppressing work",
    "[rng][rng_trace][rng_trace_index]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto root = observe_deterministic_rng();
    const auto trace = begin_rng_task_trace({.capacity = 1});
    REQUIRE(trace);
    auto probe = rng_task_trace_test::probe{
        .before_context_capture = [] [[noreturn]] () -> void { throw std::bad_alloc{}; },
    };
    const auto installed = rng_task_trace_test::scoped_probe(probe);
    auto serial = cata_thread_pool(0);
    CHECK(serial.submit_returning({.stream = 53, .id = 59}, []() { return 42; }).get() == 42);
    trace->stop();
    const auto snapshot = trace->observe();
    CHECK(snapshot.incomplete);
    CHECK_FALSE(snapshot.overflow);
    CHECK(snapshot.outstanding == 0);
    CHECK(snapshot.records.empty());
    CHECK(probe.occurrence_entries == 0);
    CHECK(probe.occurrence_hashes == 0);
    CHECK_FALSE(snapshot.all_recorded_scopes_restored());
    CHECK(observe_deterministic_rng() == root);
}
#endif

TEST_CASE(
    "zero-worker keyed and per-index traces preserve owning contexts",
    "[rng][rng_trace][rng_trace_zero_workers]") {
    const auto restore = restore_rng{};
    rng_set_deterministic_seed(1729);
    const auto root = observe_deterministic_rng();
    REQUIRE(root);
    const auto trace = begin_rng_task_trace({.capacity = 128});
    REQUIRE(trace);
    auto serial = cata_thread_pool(0);
    CHECK(serial.num_workers() == 0);
    const auto keyed =
        serial
            .submit_returning({.stream = 31, .id = 37}, []() { return observe_deterministic_rng(); })
            .get();
    REQUIRE(keyed);
    CHECK(keyed->source == rng_engine_source::task);
    CHECK(observe_deterministic_rng() == root);
    const auto global_workers = get_thread_pool().num_workers();
    if (!get_option<bool>("MULTITHREADING_ENABLED")) { CHECK(global_workers == 0); }
    auto per_index = std::vector<std::optional<rng_observation>>(4);
    parallel_for_chunked(0, 4, 1, [&](const auto index) {
        per_index[index] = observe_deterministic_rng();
        const auto nested = serial.submit_returning({.stream = 41, .id = 43}, sample).get();
        static_cast<void>(nested);
    });
    parallel_for(0, 1, [](const auto /*index*/) { static_cast<void>(sample()); });
    trace->stop();
    const auto snapshot = trace->observe();
    CHECK_FALSE(snapshot.overflow);
    CHECK_FALSE(snapshot.incomplete);
    CHECK(
        std::ranges::count_if(
            snapshot.records,
            [](const auto& record) {
                return record.kind == rng_task_trace_kind::parallel_index
                    && record.phase == rng_task_trace_phase::restored;
            })
        == 5);
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
