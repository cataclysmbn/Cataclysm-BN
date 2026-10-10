#include "catch/catch.hpp"
#include "compute/gpu_failure.h"
#include "debug.h"

TEST_CASE("a gpu failure is reported once per message, not on every frame", "[gpu][debug]") {
    const auto first = capture_debugmsg_during([] {
        cata_gpu::report_failure_once("dispatch failed (test one)");
    });
    CHECK(first.contains("dispatch failed (test one)"));
    const auto repeat = capture_debugmsg_during([] {
        cata_gpu::report_failure_once("dispatch failed (test one)");
    });
    CHECK(repeat.empty());
    const auto other = capture_debugmsg_during([] {
        cata_gpu::report_failure_once("completion failed (test two)");
    });
    CHECK(other.contains("completion failed (test two)"));
}
