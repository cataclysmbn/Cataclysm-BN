#include "editor/session_page.h"

#include <algorithm>
#include <cstdint>

namespace editor {

auto page_affordance(const page_options& options) -> engine_client::affordance {
    const auto pages = std::max<std::size_t>(1, (options.total + page_size - 1) / page_size);
    return engine_client::invoke_affordance(
        {.id = options.id,
         .label = options.label,
         .args = {
             .count = engine_client::count_shape{
                 .min = 0,
                 .max = static_cast<std::int64_t>(pages - 1),
                 .initial = static_cast<std::int64_t>(options.page)}}});
}

} // namespace editor
