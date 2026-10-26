#pragma once

#include "engine_client_component.h"

#include <cstddef>
#include <string>

namespace editor {

/// How many entries a page of an editor list shows.
constexpr auto page_size = std::size_t{50};

struct page_options {
    std::string id;
    std::string label;
    /// How many entries the list has on all its pages.
    std::size_t total = 0;
    std::size_t page = 0;
};
/// The affordance that turns a list to one of its pages, at least one even when it is empty.
auto page_affordance(const page_options& options) -> engine_client::affordance;

} // namespace editor
