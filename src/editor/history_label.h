#pragma once

#include "editor/document.h"
#include "engine_client_component.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/// What `edit` did, in words: the member of the object it changed, and whether that member was
/// added, removed or changed, which the patch's own verb does not tell, since setting a member
/// adds it.
auto step_label(const document_edit& edit) -> std::string;

/// The steps of a history in words: what each step did, oldest first, the steps undone after
/// those done; how many are done now; and how many were done when it was last saved, if ever.
struct history_steps {
    std::vector<std::string> labels;
    std::size_t depth = 0;
    std::optional<std::size_t> saved = std::nullopt;
};

/// The label function of the steps of a history.
using edit_label_t = std::function<auto(const document_edit&)->std::string>;

/// What `step` did: the name its group gave it, else `label` of its last edit.
auto step_name(const document_step& step, const edit_label_t& label = step_label) -> std::string;

/// `doc`'s steps, each named by `step_name`.
auto document_steps(const document& doc, const edit_label_t& label = step_label) -> history_steps;

/// A row `step:<n>` per state of a history, the opened one first, the current one selected and
/// the saved one saying so.
auto step_rows(const history_steps& steps) -> std::vector<engine_client::row>;

} // namespace editor
