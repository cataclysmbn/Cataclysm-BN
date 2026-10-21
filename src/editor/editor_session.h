#pragma once

namespace editor {

/// The map editor screen a protocol client draws: opens a data file, draws on a mapgen's rows with
/// the shape tools, says what the loader reports, and saves; returns when the player leaves it.
auto run_editor_session() -> void;

} // namespace editor
