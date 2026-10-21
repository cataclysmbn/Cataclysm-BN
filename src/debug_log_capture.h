#pragma once

#include <functional>
#include <string>
#include <vector>

/// Capture expected error logs without marking the test run as failed.
/// Callers must check the returned messages; callbacks must join any logging workers.
/// Nested captures are not supported. Existing observed errors are preserved.
auto capture_debug_errors_during( const std::function < auto() -> void > &function ) -> std::string;

/// The debug messages `function` raises, one entry each, in order; none of them is shown or logged.
auto capture_each_debugmsg_during( const std::function < auto() -> void > &function ) ->
std::vector<std::string>;
