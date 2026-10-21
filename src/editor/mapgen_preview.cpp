#include "editor/mapgen_preview.h"

#include "calendar.h"
#include "debug_log_capture.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "mapgen/mapgen_constructor.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "rng.h"
#include "trap.h"
#include "vehicle/vpart_position.h"

#include <optional>
#include <ranges>

namespace editor {
namespace {

/// The overmap terrain an entry's `om_terrain` names: itself, or its turn to `direction` when it
/// rotates.
auto overmap_terrain(const std::string& name, const std::string& direction)
    -> std::optional<oter_id> {
    if (const auto plain = oter_str_id(name); plain.is_valid()) { return plain.id(); }
    if (const auto turned = oter_str_id(name + "_" + direction); turned.is_valid()) {
        return turned.id();
    }
    return std::nullopt;
}

} // namespace

auto preview_mapgen(const std::string_view text, const preview_options& options)
    -> std::expected<mapgen_preview, std::string> {
    auto preview = mapgen_preview{};
    auto failed = std::optional<std::string>{};
    preview.messages = capture_each_debugmsg_during([&] {
        const auto loaded = load_detached_mapgens(text);
        if (!loaded) {
            failed = loaded.error();
            return;
        }
        if (loaded->functions.empty()) {
            failed = "only an overmap terrain's mapgen has a preview";
            return;
        }
        for (const auto& function : loaded->functions) {
            if (auto pinned = pin_parameters(*function, options.pins); !pinned) {
                failed = pinned.error();
                return;
            }
        }
        const auto& grid = loaded->grid;
        preview.size = point_rel_ms(SEEX * 2 * grid.x(), SEEY * 2 * grid.y());
        preview.squares.resize(preview.size.x() * preview.size.y());
        const auto dimension = get_map().get_bound_dimension();
        auto& overmap = get_overmapbuffer(dimension);
        const auto squares = std::views::iota(std::size_t{0}, loaded->functions.size());
        const auto square_of = [&](const std::size_t index) {
            return point_rel_omt(index % grid.x(), index / grid.x());
        };
        auto terrains = std::vector<oter_id>{};
        for (const auto& name : loaded->om_terrains) {
            const auto terrain = overmap_terrain(name, options.direction);
            if (!terrain) {
                failed = "no overmap terrain " + name;
                return;
            }
            terrains.push_back(*terrain);
        }
        // Every square is placed before any is generated, as an overmap special places them.
        const auto was =
            squares | std::views::transform([&](const auto index) {
                return overmap.ter(options.at + square_of(index));
            })
            | std::ranges::to<std::vector>();
        for (const auto index : squares) {
            overmap.ter_set(options.at + square_of(index), terrains[index]);
        }
        for (const auto index : squares) {
            const auto square = square_of(index);
            const auto at = options.at + square;
            auto buffer = mapbuffer{};
            buffer.set_dimension_id(dimension);
            auto constructor = mapgen_constructor(buffer);
            {
                const auto seeded = rng_deterministic_task_scope(options.seed + index);
                constructor.generate(
                    at, calendar::turn,
                    {.use_selected_mapgen = true, .selected_mapgen = loaded->functions[index]});
            }
            for (const auto p : constructor.points_on_zlevel()) {
                const auto x = square.x() * SEEX * 2 + p.x();
                const auto y = square.y() * SEEY * 2 + p.y();
                preview.squares[y * preview.size.x() + x] =
                    {.ter = constructor.ter(p).id().str(),
                     .furn = constructor.furn(p).id().str(),
                     .trap = constructor.tr_at(p).id.str(),
                     .items = constructor.i_at(p).size(),
                     .vehicle = constructor.veh_at(p).has_value()};
            }
        }
        for (const auto index : squares) {
            overmap.ter_set(options.at + square_of(index), was[index]);
        }
    });
    if (failed) { return std::unexpected(*failed); }
    return preview;
}

} // namespace editor
