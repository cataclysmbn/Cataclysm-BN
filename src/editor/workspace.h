#pragma once

#include "engine_client_component.h"

#include <functional>
#include <map>
#include <string>

namespace editor {

/// What each part of an editor's screen is for and which menu each of its affordances sits in, by
/// id.
struct workspace_layout {
    std::map<std::string, std::string, std::less<>> slots;
    std::map<std::string, std::string, std::less<>> groups;
};
/// Names the slot of each part of the root split and the menu of each affordance that `layout`
/// lists.
auto lay_out(engine_client::screen_view& view, const workspace_layout& layout) -> void;

} // namespace editor
