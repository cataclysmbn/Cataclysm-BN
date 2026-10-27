#include "editor/workspace.h"

#include <algorithm>

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
    const auto rank = [&](const engine_client::affordance& entry) {
        return std::ranges::
            distance(layout.menus.begin(), std::ranges::find(layout.menus, entry.group));
    };
    std::ranges::stable_sort(view.affordances, std::less{}, rank);
}

} // namespace editor
