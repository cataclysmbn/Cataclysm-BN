#include "editor/workspace.h"

namespace editor {

auto lay_out(engine_client::screen_view& view, const workspace_layout& layout) -> void {
    if (view.root) {
        for (auto& part : view.root->children) {
            const auto found = layout.slots.find(part.id);
            if (found == layout.slots.end()) { continue; }
            part.slot = found->second.slot;
            if (part.label.empty()) { part.label = found->second.label; }
        }
    }
    for (auto& affordance : view.affordances) {
        const auto found = layout.groups.find(affordance.id);
        affordance.group = found == layout.groups.end() ? std::string{} : found->second;
    }
}

} // namespace editor
