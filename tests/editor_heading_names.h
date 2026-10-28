#pragma once

#include "engine_client_component.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

/// Adds the labels of the tabs in `value` and the parts inside it to `labels`.
inline auto add_tab_labels(const engine_client::component& value, std::vector<std::string>& labels)
    -> void {
    if (const auto* const tabs = std::get_if<engine_client::tabs_body>(&value.body)) {
        std::ranges::copy(tabs->tabs | std::views::transform(&engine_client::tab_entry::label),
                          std::back_inserter(labels));
    }
    for (const auto& child : value.children) { add_tab_labels(child, labels); }
}

/// The headings of the parts of a workspace's root that an action or a tab of the screen is also
/// named, which a reader cannot tell apart from them.
inline auto headings_named_like_actions(const engine_client::screen_view& screen)
    -> std::vector<std::string> {
    namespace ec = engine_client;
    if (!screen.root) { return {}; }
    auto actions =
        screen.affordances | std::views::transform(&ec::affordance::label)
        | std::ranges::to<std::vector>();
    add_tab_labels(*screen.root, actions);
    return screen.root->children | std::views::transform(&ec::component::label)
         | std::views::filter([&](const std::string& heading) {
               return !heading.empty() && std::ranges::contains(actions, heading);
           })
         | std::ranges::to<std::vector>();
}
