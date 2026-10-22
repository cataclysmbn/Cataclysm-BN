#pragma once

#include "coordinates.h"
#include "editor/mapgen_preview.h"
#include "type_id.h"

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class mapbuffer;

namespace editor {

/// A mapgen entry generated into the live world. It holds what it replaced, so that it can be
/// taken back.
struct stamp {
    /// The overmap square of the entry's first square.
    tripoint_abs_omt at;
    point_rel_omt grid;
    /// The overmap terrain of each square that the world does not hold now, row by row.
    std::vector<oter_id> was;
    /// Holds the submaps the stamp replaced, as long as it is stamped.
    std::shared_ptr<mapbuffer> replaced;
};

/// Generates the entry in `text` over the loaded squares at `options.at` as a preview does, and
/// sets their overmap terrain to the entry's; the error says why nothing changed.
auto stamp_mapgen(std::string_view text, const preview_options& options)
    -> std::expected<stamp, std::string>;

/// Swaps the submaps and the overmap terrain the stamp holds with the world's: the first swap
/// puts the entry in, the next takes it out again, and so on.
auto swap_stamp(stamp& stamped) -> void;

} // namespace editor
