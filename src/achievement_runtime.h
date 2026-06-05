#pragma once

class achievements_tracker;

namespace achievement_runtime
{

auto set_active_tracker( achievements_tracker *tracker ) -> void;
auto active_tracker() -> achievements_tracker *; // *NOPAD*

} // namespace achievement_runtime
