#pragma once

#include "editor/recent_documents.h"

namespace editor {

/// The map editor screen a protocol client draws: opens a data file, draws on a mapgen's rows with
/// the shape tools, says what the loader reports, and saves; returns when the player leaves it,
/// keeping the open document and the view for the next visit.
auto run_editor_session() -> void;

/// Forgets the document and view the editor keeps between visits, for a new game.
auto reset_editor_session() -> void;

/// Makes the next visit open the vehicle editor, as the main menu's vehicle editor does, and leave
/// with it.
auto open_vehicle_editor_first() -> void;

/// Makes the next visit open `recent` in its editor, as the main menu's recent documents do; a
/// vehicle editor's document is left with that editor.
auto open_first(const recent_document& recent) -> void;

} // namespace editor
