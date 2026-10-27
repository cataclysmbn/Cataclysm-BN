#include "editor/workspace.h"

namespace editor {

auto lay_out(engine_client::screen_view& view, const workspace_layout& layout) -> void {
    const auto named = [](const auto& names, const std::string& id) {
        const auto found = names.find(id);
        return found == names.end() ? std::string{} : found->second;
    };
    if (view.root) {
        for (auto& part : view.root->children) { part.slot = named(layout.slots, part.id); }
    }
    for (auto& affordance : view.affordances) {
        affordance.group = named(layout.groups, affordance.id);
    }
}

} // namespace editor
