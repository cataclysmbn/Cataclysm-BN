#pragma once

/// Whether drawing the current frame can be skipped because the player already queued another key,
/// e.g. while holding a movement key.  The skipped invalidation persists, so the final frame is
/// drawn once the queue is empty.
///
/// Never skips when the `DEFER_REDRAW_ON_QUEUED_INPUT` option is off, when safe mode has just
/// stopped the player, while the death cam is up, while the player runs an activity, or on curses
/// builds.
auto should_defer_redraw_for_queued_input() -> bool;

/// Set while the main game's action prompt reads input, so the platform layer may also skip
/// presenting frames.  Popups and menus never set this.
extern bool main_prompt_reads_input;
