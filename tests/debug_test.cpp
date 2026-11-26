#include "catch/catch.hpp"
#include "debug.h"
#include "debug_log_capture.h"

#include <stdexcept>

TEST_CASE("expected error logs do not poison the test run", "[debug]") {
    const auto already_observed = debug_has_error_been_observed();
    const auto errors = capture_debug_errors_during([]() {
        DebugLog(DL::Error, DC::Main) << "expected failure";
    });
    CHECK(errors == "expected failure\n");
    CHECK(debug_has_error_been_observed() == already_observed);
}

TEST_CASE("error log captures recover after callback exceptions", "[debug]") {
    const auto already_observed = debug_has_error_been_observed();
    REQUIRE_THROWS_AS(
        capture_debug_errors_during([]() {
            DebugLog(DL::Error, DC::Main) << "expected failure before exception";
            throw std::runtime_error("callback failure");
        }),
        std::runtime_error);
    const auto errors = capture_debug_errors_during([]() {
        DebugLog(DL::Error, DC::Main) << "next expected failure";
    });
    CHECK(errors == "next expected failure\n");
    CHECK(debug_has_error_been_observed() == already_observed);
}

TEST_CASE("nested error log captures preserve the outer capture", "[debug]") {
    const auto errors = capture_debug_errors_during([]() {
        REQUIRE_THROWS_AS(capture_debug_errors_during([]() {}), std::logic_error);
        DebugLog(DL::Error, DC::Main) << "outer expected failure";
    });
    CHECK(errors == "outer expected failure\n");
}
