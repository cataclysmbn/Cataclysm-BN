#include "catch/catch.hpp"
#include "thread_pool.h"

#include <algorithm>
#include <atomic>
#include <stdexcept>

TEST_CASE("nested_parallel_loops_complete_with_saturated_workers", "[thread_pool]") {
    const auto outer_count = std::max(2, static_cast<int>(get_thread_pool().num_workers()));
    const auto chunked = GENERATE(false, true);
    auto visits = std::atomic<int>{0};
    parallel_for(0, outer_count, [&](int /*outer*/) {
        const auto visit = [&](int /*inner*/) { visits.fetch_add(1); };
        if (chunked) {
            parallel_for_chunked(0, 7, 2, visit);
        } else {
            parallel_for(0, 7, visit);
        }
    });
    CHECK(visits.load() == outer_count * 7);
}

TEST_CASE("nested_parallel_loop_exceptions_reach_caller", "[thread_pool]") {
    const auto chunked = GENERATE(false, true);
    const auto fail = [](int /*index*/) { throw std::runtime_error("nested failure"); };
    CHECK_THROWS_WITH(
        parallel_for(0, 2,
                     [&](int /*outer*/) {
                         if (chunked) {
                             parallel_for_chunked(0, 3, 2, fail);
                         } else {
                             parallel_for(0, 3, fail);
                         }
                     }),
        "nested failure");
    auto visits = std::atomic<int>{0};
    parallel_for(0, 3, [&](int /*index*/) { visits.fetch_add(1); });
    CHECK(visits.load() == 3);
}
