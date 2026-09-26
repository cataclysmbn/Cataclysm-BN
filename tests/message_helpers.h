#pragma once

#include <functional>
#include <string>
#include <vector>

/// Capture this thread's otherwise discarded test messages. Nested captures are isolated.
auto capture_messages_during(const std::function<void()>& callback) -> std::vector<std::string>;
