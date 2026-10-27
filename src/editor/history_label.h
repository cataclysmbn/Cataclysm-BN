#pragma once

#include "editor/document.h"

#include <string>

namespace editor {

/// What `edit` did, in words: the member of the object it changed, and whether that member was
/// added, removed or changed, which the patch's own verb does not tell, since setting a member
/// adds it.
auto step_label(const document_edit& edit) -> std::string;

} // namespace editor
