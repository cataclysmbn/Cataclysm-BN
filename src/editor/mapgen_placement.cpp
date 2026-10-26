#include "editor/mapgen_placement.h"

#include "editor/document.h"
#include "editor/mapgen_check.h"

#include <algorithm>
#include <ranges>

namespace editor {
namespace {

using ordered_json = nlohmann::ordered_json;

auto range_of(const ordered_json& placed, const char* axis) -> placement_range {
    if (!placed.contains(axis)) { return {}; }
    const auto& value = placed[axis];
    if (value.is_array() && value.size() == 2) {
        return {.min = value[0].get<int>(), .max = value[1].get<int>()};
    }
    if (value.is_array() && value.size() == 1) {
        return {.min = value[0].get<int>(), .max = value[0].get<int>()};
    }
    return value.is_number()
             ? placement_range{.min = value.get<int>(), .max = value.get<int>()}
             : placement_range{};
}

auto written(const placement_range& range) -> ordered_json {
    return range.min == range.max ? ordered_json(range.min) : ordered_json{range.min, range.max};
}

auto texts(const std::vector<loader_message>& messages) -> std::vector<std::string> {
    return messages | std::views::transform(&loader_message::text) | std::ranges::to<std::vector>();
}

} // namespace

auto placements_of(const document& doc, const std::size_t object) -> std::vector<placement> {
    const auto& value = doc.objects().at(object).value;
    if (!value.contains("object")) { return {}; }
    auto found = std::vector<placement>{};
    for (const auto& [kind, entries] : value["object"].items()) {
        if (!kind.starts_with("place_") || !entries.is_array()) { continue; }
        for (const auto index : std::views::iota(std::size_t{0}, entries.size())) {
            const auto& placed = entries[index];
            if (!placed.is_object()) { continue; }
            found.push_back(
                {.kind = kind,
                 .index = index,
                 .x = range_of(placed, "x"),
                 .y = range_of(placed, "y"),
                 .value = placed});
        }
    }
    return found;
}

auto move_placement(document& doc, const placement_move& move) -> std::expected<void, std::string> {
    if (move.x.min > move.x.max || move.y.min > move.y.max) {
        return std::unexpected(std::string("a range must not end before it starts"));
    }
    const auto at = ordered_json::json_pointer("/object") / move.kind / move.index;
    return doc.edit(
        move.object,
        {{{"op", "add"}, {"path", (at / "x").to_string()}, {"value", written(move.x)}},
         {{"op", "add"}, {"path", (at / "y").to_string()}, {"value", written(move.y)}}});
}

auto add_placement(document& doc, const placement_add& add) -> std::expected<void, std::string> {
    if (add.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(add.object));
    }
    if (!add.value.is_object()) {
        return std::unexpected(std::string("a placement is a JSON object"));
    }
    if (add.x.min > add.x.max || add.y.min > add.y.max) {
        return std::unexpected(std::string("a range must not end before it starts"));
    }
    auto placed = add.value;
    placed["x"] = written(add.x);
    placed["y"] = written(add.y);
    const auto& value = doc.objects()[add.object].value;
    const auto kind = ordered_json::json_pointer("/object") / add.kind;
    const auto has = value.contains(kind) && value[kind].is_array();
    return set_field(
        doc,
        {.object = add.object,
         .pointer = has ? (kind / "-").to_string() : kind.to_string(),
         .value = has ? placed : ordered_json::array({placed})});
}

auto remove_placement(document& doc, const placement_at& at) -> std::expected<void, std::string> {
    if (at.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(at.object));
    }
    const auto& value = doc.objects()[at.object].value;
    const auto kind = ordered_json::json_pointer("/object") / at.kind;
    if (!value.contains(kind) || !value[kind].is_array() || at.index >= value[kind].size()) {
        return std::unexpected("no placement " + at.kind + " " + std::to_string(at.index));
    }
    const auto last = value[kind].size() == 1;
    return set_field(
        doc,
        {.object = at.object, .pointer = last ? kind.to_string() : (kind / at.index).to_string()});
}

auto placements_inside(const document& doc, const std::size_t object, const placement_area& area)
    -> std::vector<placement> {
    const auto within = [](const placement_range& range, const placement_range& outer) {
        return range.min >= outer.min && range.max <= outer.max;
    };
    return placements_of(doc, object) | std::views::filter([&](const placement& placed) {
               return within(placed.x, area.x) && within(placed.y, area.y);
           })
         | std::views::transform([&](placement placed) {
               placed.x = {.min = placed.x.min - area.x.min, .max = placed.x.max - area.x.min};
               placed.y = {.min = placed.y.min - area.y.min, .max = placed.y.max - area.y.min};
               return placed;
           })
         | std::ranges::to<std::vector>();
}

auto turned(placement placed, const clip_size& size) -> placement {
    const auto x = placed.x;
    placed.x = {.min = size.rows - 1 - placed.y.max, .max = size.rows - 1 - placed.y.min};
    placed.y = x;
    return placed;
}

auto mirrored_x(placement placed, const clip_size& size) -> placement {
    placed.x = {.min = size.columns - 1 - placed.x.max, .max = size.columns - 1 - placed.x.min};
    return placed;
}

auto mirrored_y(placement placed, const clip_size& size) -> placement {
    placed.y = {.min = size.rows - 1 - placed.y.max, .max = size.rows - 1 - placed.y.min};
    return placed;
}

auto set_field(document& doc, const field_edit& edit) -> std::expected<void, std::string> {
    if (edit.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(edit.object));
    }
    const auto& value = doc.objects()[edit.object].value;
    const auto patch =
        edit.value ? ordered_json{{{"op", "add"}, {"path", edit.pointer}, {"value", *edit.value}}}
                   : ordered_json{{{"op", "remove"}, {"path", edit.pointer}}};
    auto next = ordered_json{};
    try {
        next = value.patch(patch);
    } catch (const ordered_json::exception& e) { return std::unexpected(std::string(e.what())); }
    const auto before = texts(check_mapgen(value.dump()));
    const auto after = texts(check_mapgen(next.dump()));
    const auto said = std::ranges::find_if(after, [&](const std::string& text) {
        return !std::ranges::contains(before, text);
    });
    if (said != after.end()) { return std::unexpected(*said); }
    return doc.edit(edit.object, patch);
}

} // namespace editor
