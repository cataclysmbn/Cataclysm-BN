#pragma once

#include "engine_client_component.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace editor {

/// What each part of an editor's screen is for and which menu each of its affordances sits in, by
/// id.
struct workspace_part {
    std::string slot;
    /// The heading of a part that has none of its own, localized; empty for none.
    std::string label = {};
    /// The part starts folded to its heading.
    bool collapsed = false;
};
struct workspace_layout {
    std::map<std::string, workspace_part, std::less<>> slots;
    std::map<std::string, std::string, std::less<>> groups;
    /// The menus in the order the menu bar has them.
    std::vector<std::string> menus = {};
};
/// Names the slot and heading of each part of the root split and the menu of each affordance that
/// `layout` lists, the affordances of each menu after those of the menus before it.
auto lay_out(engine_client::screen_view& view, const workspace_layout& layout) -> void;

} // namespace editor
