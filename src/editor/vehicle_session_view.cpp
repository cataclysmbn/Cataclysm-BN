#include "avatar.h"
#include "calendar.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/history_label.h"
#include "editor/mapgen_stroke.h"
#include "editor/row_id.h"
#include "editor/session_file.h"
#include "editor/session_page.h"
#include "editor/vehicle_design.h"
#include "editor/vehicle_session_state.h"
#include "editor/workspace.h"
#include "engine_client_component.h"
#include "engine_client_part_type.h"
#include "engine_client_vehicle_look.h"
#include "engine_client_world.h"
#include "item.h"
#include "item_group.h"
#include "line.h"
#include "map/map.h"
#include "output.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor::vehicle_editor {

namespace ec = engine_client;

namespace {

/// The name of the canvas tool `key`, as the map editor names its tools.
auto canvas_tool_label(const std::string_view key) -> std::string {
    if (key == "select") { return _("Select"); }
    if (key == "install") { return _("Pencil"); }
    if (key == "line") { return _("Line"); }
    if (key == "rect") { return _("Rectangle"); }
    if (key == "rect_filled") { return _("Filled rectangle"); }
    if (key == "fill") { return _("Fill"); }
    if (key == "remove") { return _("Eraser"); }
    if (key == "paste") { return _("Paste"); }
    return _("Eyedropper");
}

/// Which way the front of the vehicle shows on the canvas turned `quarters` clockwise.
auto front_label(const int quarters) -> std::string {
    switch (quarters) {
        case 1:
            return _("Down");
        case 2:
            return _("Left");
        case 3:
            return _("Up");
        default:
            return _("Right");
    }
}

/// A JSON array of strings as a comma-separated list.
auto list_text(const nlohmann::ordered_json& value) -> std::string {
    if (!value.is_array()) { return {}; }
    return value | std::views::transform([](const nlohmann::ordered_json& entry) {
               return entry.is_string() ? entry.get<std::string>() : entry.dump();
           })
         | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>();
}

/// The choices of a field that names one of `items`, by name; the one it names now leads when it
/// is none of them, so that the field shows it.
auto item_options(const std::vector<itype_id>& items, const std::string& current)
    -> std::vector<ec::form_option> {
    auto options =
        items | std::views::transform([](const itype_id& id) {
            return ec::form_option{.id = id.str(), .label = item::nname(id)};
        })
        | std::ranges::to<std::vector>();
    if (!current.empty() && !std::ranges::contains(options, current, &ec::form_option::id)) {
        options.insert(options.begin(), {.id = current, .label = current});
    }
    return options;
}

/// What the canvas shows.
struct grid_options {
    const vehicle* veh = nullptr;
    /// The mounts shown, the picked one among them.
    mount_frame frame;
    point cursor;
    /// The part a drawing tool holds, whose refusals the squares show; none for another tool.
    std::optional<vpart_id> held;
    /// How a drag over the canvas uses the tool in hand, as the map editor's canvas has it.
    std::string drag;
    /// How many quarters clockwise the canvas is turned.
    int quarters = 0;
    /// The mounts marked; none for none.
    std::optional<mount_frame> selection;
    /// Where the mounts a click pastes on lie from the mount clicked; none for no paste.
    std::vector<point> stamp;
};
/// The mounts of `frame`, a cell each, west to east and north to south as its blueprint reads,
/// each showing every part of the vehicle `veh` on it and why the part held cannot go there.
auto grid_of(const grid_options& options) -> ec::component {
    const auto& [veh, frame, cursor, held, drag, quarters, selection, stamp] = options;
    const auto refusal = veh && held ? std::optional(mount_refusals(*veh, *held)) : std::nullopt;
    const auto stacks = veh ? mount_stacks(*veh) : std::map<point, std::vector<int>>{};
    const auto& [low, high] = frame;
    const auto cols = high.x - low.x + 1;
    const auto rows = high.y - low.y + 1;
    // How far east and south a step `d` of mounts shows on the canvas turned: the front, east,
    // turns south first.
    const auto turned = [&](const point& d) -> ec::grid_point {
        switch (quarters) {
            case 1:
                return {-d.y, d.x};
            case 2:
                return {-d.x, -d.y};
            case 3:
                return {d.y, -d.x};
            default:
                return {d.x, d.y};
        }
    };
    // Where the mount `at` shows: the turned canvas's north-west corner is the mount that turns
    // there.
    const auto corner = turned(high - low);
    const auto shown_at = [&](const point& at) -> ec::grid_point {
        const auto step = turned(at - low);
        return {step.x - std::min(0, corner.x), step.y - std::min(0, corner.y)};
    };
    auto palette = ec::look_palette{};
    auto cells = std::vector<ec::grid_cell>{};
    for (auto y = low.y; y <= high.y; ++y) {
        for (auto x = low.x; x <= high.x; ++x) {
            const auto at = point(x, y);
            auto cell = ec::grid_cell{
                .at = shown_at(at),
                .id = point_row("mount", at),
                .glyph = " ",
                .affordance = mount_id,
                .selected = at == cursor,
                .refused = refusal ? (*refusal)(at).value_or("") : ""};
            const auto here = stacks.find(at);
            const auto shown =
                here != stacks.end() ? veh->part_displayed_at(tripoint_mnt_veh(x, y, 0)) : -1;
            if (shown >= 0) {
                cell.glyph = std::string(1, veh->part_sym(shown));
                // Every part on the mount, in the order the game installed them.
                cell.label = remove_color_tags(
                    join(here->second | std::views::transform([&](const int index) {
                             return veh->cpart(index).name(false);
                         }) | std::ranges::to<std::vector>(),
                         ", "));
                cell.color = string_from_color(veh->part_color(shown));
                cell.looks = palette.indexes(mount_looks(*veh, here->second, 90 * quarters));
            }
            cells.push_back(std::move(cell));
        }
    }
    // The empty square most of the canvas shows, which the wire leaves out: its refusal is the
    // one most empty squares share.
    auto refusals = std::map<std::string, int>{};
    for (const auto& cell : cells) {
        if (cell.label.empty() && !cell.selected) { ++refusals[cell.refused]; }
    }
    const auto blank = ec::grid_cell{
        .glyph = " ",
        .affordance = mount_id,
        .refused =
            refusals.empty()
                ? std::string()
                : std::ranges::max_element(refusals, {}, &decltype(refusals)::value_type::second)
                      ->first};
    auto regions = std::vector<ec::grid_region>{};
    if (selection) {
        const auto a = shown_at(selection->low);
        const auto b = shown_at(selection->high);
        regions.push_back(
            {.id = "selection",
             .from = {std::min(a.x, b.x), std::min(a.y, b.y)},
             .to = {std::max(a.x, b.x), std::max(a.y, b.y)},
             .label = _("Selection"),
             .selected = true});
    }
    return {
        .id = grid_id,
        .affordances = {mount_id, clear_id, stroke_begin_id, stroke_end_id},
        .body = ec::grid_body{
            .cols = quarters % 2 == 0 ? cols : rows,
            .rows = quarters % 2 == 0 ? rows : cols,
            .cursor = shown_at(cursor),
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas",
            .drag = drag,
            // Every drag is one step, a secondary one erasing as it goes.
            .drag_begin = stroke_begin_id,
            .drag_end = stroke_end_id,
            .regions = std::move(regions),
            .secondary = clear_id,
            .secondary_drag = "trail",
            .blank = blank,
            .stamp = stamp | std::views::transform(turned) | std::ranges::to<std::vector>()}};
}

/// The squares the vehicle `veh` covers on the map when it faces `facing` degrees clockwise from
/// east, as the game turns it, each showing the parts of the mount that lands there.
auto turned_grid(const vehicle& veh, const int facing) -> ec::component {
    const auto dir = units::from_degrees(facing);
    // The game spawns a vehicle turning it about its (0,0) mount, which lands on the spawn square.
    const auto pivot = tripoint_mnt_veh::zero();
    struct landed {
        point at;
        int part;
    };
    auto squares = std::vector<landed>{};
    for (auto index = 0; index < veh.part_count(); ++index) {
        if (veh.cpart(index).removed) { continue; }
        const auto mount = veh.cpart(index).mount;
        const auto shown = veh.part_displayed_at(mount);
        if (shown < 0 || std::ranges::contains(squares, shown, &landed::part)) { continue; }
        auto at = point_rel_ms{};
        veh.coord_translate(dir, pivot, mount, at);
        squares.push_back({.at = at.raw(), .part = shown});
    }
    if (squares.empty()) { return {.id = turned_id, .body = ec::grid_body{}}; }
    const auto [low, high] = frame_around(
        squares | std::views::transform(&landed::at) | std::ranges::to<std::vector>(), 0);
    auto palette = ec::look_palette{};
    auto stacks = mount_stacks(veh);
    const auto cells =
        squares | std::views::transform([&](const landed& square) {
            return ec::grid_cell{
                .at = {square.at.x - low.x, square.at.y - low.y},
                .glyph = std::string(1, veh.part_sym(square.part)),
                .label = remove_color_tags(veh.cpart(square.part).name(false)),
                .color = string_from_color(veh.part_color(square.part)),
                .looks = palette.indexes(
                    mount_looks(veh, stacks[veh.cpart(square.part).mount.xy().raw()], facing))};
        })
        | std::ranges::to<std::vector>();
    return {
        .id = turned_id,
        .label = _("Facing"),
        .body = ec::grid_body{
            .cols = high.x - low.x + 1,
            .rows = high.y - low.y + 1,
            .cells = cells,
            .looks = std::move(palette.looks)}};
}

/// The parts on the picked mount, in the order the game installs them; those of the world's
/// vehicle edited in place with their condition.
auto stack_of(const session& state) -> ec::component {
    const auto design = state.live ? std::vector<design_part>{} : design_parts(state.prototype());
    const auto picked = state.picked_here();
    const auto row_of_part = [&](const std::size_t at) {
        auto row = ec::row{.id = index_row("part", at), .selected = at == picked};
        if (!state.live) {
            const auto id = vpart_id(design[at].part);
            row.label = id.is_valid() ? id->name() : design[at].part;
            if (id.is_valid()) {
                row.look = engine_client::world::vehicle_part_type_appearance(*id);
            }
            return row;
        }
        const auto index = static_cast<int>(at);
        const auto& part = state.live->cpart(index);
        row.label = part.info().name();
        row.columns.push_back(
            {.key = "health",
             .label = _("Health %"),
             .value = std::to_string(static_cast<int>(std::round(part.health_percent() * 100))),
             .color = ""});
        if (part.is_fuel_store(false) || part.is_turret()) {
            row.columns.push_back(
                {.key = "fuel",
                 .label = part.is_turret() ? _("Rounds") : _("Fuel"),
                 .value = std::to_string(part.ammo_remaining()),
                 .color = ""});
        }
        row.look = engine_client::world::vehicle_part_own_appearance(*state.live, index, 270);
        return row;
    };
    return ec::list(
        {.id = stack_id,
         .rows = state.parts_here() | std::views::transform(row_of_part)
               | std::ranges::to<std::vector>(),
         .selection = "single",
         .activate = pick_id,
         .empty = state.live ? std::string{} : _("This mount holds no part.")});
}

/// The picked part's health, fuel and cargo, for `part`, or else the vehicle's name.
auto live_form(const session& state, const bool part_fields) -> ec::component {
    const auto& veh = *state.live;
    auto fields = std::vector<ec::form_field>{};
    if (!part_fields) {
        fields.push_back({.id = "name", .label = _("Name"), .value = veh.name});
    } else if (state.live_part >= 0 && state.live_part < veh.part_count()) {
        const auto& part = veh.cpart(state.live_part);
        fields.push_back(
            {.id = "health",
             .label = _("Health %"),
             .value = std::to_string(static_cast<int>(std::round(part.health_percent() * 100))),
             .type = "integer",
             .min = 0,
             .max = 100});
        if (part.is_fuel_store(false) || part.is_turret()) {
            fields.push_back(
                {.id = "fuel",
                 .label = part.is_turret() ? _("Rounds") : _("Fuel"),
                 .value = std::to_string(part.ammo_remaining()),
                 .type = "integer",
                 .min = 0});
            const auto current =
                part.ammo_current().is_null() ? std::string{} : part.ammo_current().str();
            fields.push_back(
                {.id = "fuel_type",
                 .label = part.is_turret() ? _("Ammunition") : _("Fuel type"),
                 .value = current,
                 .type = "select",
                 .options = item_options(part_fuels(veh, state.live_part), current)});
        }
        if (veh.part_flag(state.live_part, VPFLAG_CARGO)) {
            fields.push_back({.id = "cargo", .label = _("Add item"), .value = ""});
        }
    }
    return {.id = live_form_id,
            .affordances = {live_set_id},
            .body = ec::form_body{.fields = std::move(fields)}};
}

/// A tab of a tab strip whose selected tab is `selected`.
auto tab_of(const char* const id, const std::string& label, const std::string& selected)
    -> ec::tab_entry {
    const auto key = std::string_view(id).substr(std::string_view(id).find(':') + 1);
    return {.id = id, .label = label, .selected = key == selected};
}

/// The prototype part the field form points at, with the members a modder sets on it: the fuel
/// of a tank, the ammunition of a turret.
auto part_form(const session& state) -> std::optional<ec::component> {
    const auto index = state.picked_here();
    if (!index) { return std::nullopt; }
    const auto design = design_parts(state.prototype());
    const auto id = vpart_id(design[*index].part);
    const auto value = [&](const char* const key) {
        return state.value_at(design[*index].pointer + "/" + key);
    };
    auto fields = std::vector<ec::form_field>{};
    if (const auto shapes = state.catalog.shapes_of(id); shapes.size() > 1) {
        fields.push_back(
            {.id = "shape",
             .label = _("Shape"),
             .value = design[*index].part,
             .type = "select",
             .options =
                 std::views::zip(shapes, part_labels(shapes))
                 | std::views::transform([](const auto& entry) {
                       const auto& [shape, label] = entry;
                       return ec::form_option{.id = shape->get_id().str(), .label = label};
                   })
                 | std::ranges::to<std::vector>()});
    }
    if (id.is_valid() && id->has_flag(VPFLAG_FLUIDTANK)) {
        // The fuels the game's vehicles fill tanks with lead, then every other liquid.
        const auto liquids = tank_fuels();
        auto fuels =
            state.catalog.tank_fuels | std::views::filter([&](const itype_id& fuel) {
                return std::ranges::contains(liquids, fuel);
            })
            | std::ranges::to<std::vector>();
        std::ranges::copy(
            liquids | std::views::filter([&](const itype_id& fuel) {
                return !std::ranges::contains(fuels, fuel);
            }),
            std::back_inserter(fuels));
        auto options = item_options(fuels, value("fuel"));
        options.insert(options.begin(), {.id = "", .label = _("None")});
        fields.push_back(
            {.id = "fuel",
             .label = _("Fuel"),
             .value = value("fuel"),
             .type = "select",
             .options = std::move(options)});
    }
    if (id.is_valid() && id->has_flag("TURRET")) {
        fields.push_back(
            {.id = "ammo",
             .label = _("Ammo %"),
             .value = value("ammo"),
             .type = "integer",
             .min = 0,
             .max = 100});
        const auto pointer = design[*index].pointer;
        const auto at = nlohmann::ordered_json::json_pointer(pointer);
        const auto& entry =
            state.prototype().contains(at)
                ? state.prototype()[at]
                : nlohmann::ordered_json::object();
        const auto qty =
            entry.is_object()
                ? entry.value("ammo_qty", nlohmann::ordered_json())
                : nlohmann::ordered_json();
        const auto end = [&](const std::size_t side) {
            return qty.is_array() && qty.size() == 2 ? qty[side].dump() : std::string{};
        };
        fields.push_back(
            {.id = "ammo_types",
             .label = _("Ammo types"),
             .value = entry.is_object()
                        ? list_text(entry.value("ammo_types", nlohmann::ordered_json()))
                        : std::string{}});
        fields.push_back(
            {.id = "ammo_min",
             .label = _("Rounds, fewest"),
             .value = end(0),
             .type = "integer",
             .min = 0});
        fields.push_back(
            {.id = "ammo_max",
             .label = _("Rounds, most"),
             .value = end(1),
             .type = "integer",
             .min = 0});
    }
    if (fields.empty()) { return std::nullopt; }
    return ec::component{
        .id = part_form_id,
        .label = id.is_valid() ? id->name() : design[*index].part,
        .affordances = {part_set_id},
        .body = ec::form_body{.fields = std::move(fields)}};
}

auto parts_view(const session& state) -> ec::screen_view {
    const auto row_of = [](const char* const component) {
        return ec::arg_shape{.row = ec::row_shape{.component_id = component}};
    };
    const auto text_of = [](const char* const component) {
        return ec::
            arg_shape{.row = ec::row_shape{.component_id = component}, .text = ec::text_shape{}};
    };
    auto parts = std::vector<ec::component>{};
    auto affordances = std::vector<ec::affordance>{};
    if (!state.notice.empty()) {
        parts.push_back(
            {.id = "vehicle:notice",
             .body = ec::text_body{.markup = state.notice, .tone = "bad", .role = "prompt"}});
    }
    if (!state.said.empty()) {
        parts.push_back(
            {.id = "vehicle:status", .body = ec::text_body{.markup = state.said, .tone = "good"}});
    }
    const auto found = search_catalog(
        {.kind = catalog_kind::vehicle,
         .filter = state.filter,
         .page = state.page,
         .page_size = page_size});
    const auto vehicles = get_map().get_vehicles();
    const auto here = get_avatar().bub_pos().xy().raw();
    // The world's vehicles nearest first, the nearest picked, each with how far and which way it
    // stands, which tells apart two of one name.
    const auto distance_of = [&](const std::size_t index) {
        return rl_dist(here, vehicles[index].pos.xy().raw());
    };
    auto nearest =
        std::views::iota(std::size_t{0}, vehicles.size()) | std::ranges::to<std::vector>();
    std::ranges::stable_sort(nearest, {}, distance_of);
    auto world_rows =
        nearest | std::views::transform([&](const std::size_t index) {
            const auto at = vehicles[index].pos.xy().raw();
            const auto distance = distance_of(index);
            return ec::row{
                .id = index_row("world", index),
                .label = vehicles[index].v->name,
                .columns = {
                    {.key = "distance",
                     .label = _("Distance"),
                     .value = string_format(vgettext("%d square", "%d squares", distance), distance),
                     .color = ""},
                    {.key = "direction",
                     .label = _("Direction"),
                     .value =
                         distance == 0 ? std::string{} : direction_name(direction_from(here, at)),
                     .color = ""}}};
        })
        | std::ranges::to<std::vector>();
    if (!world_rows.empty()) { world_rows.front().selected = true; }
    // The left column: the prototypes to open, or the world's vehicles to export, copy or edit.
    parts.push_back(ec::tabs(
        {.id = sources_id,
         .tabs = {tab_of("source:prototypes", _("Prototypes"), state.source_tab),
                  tab_of("source:world", _("World"), state.source_tab)},
         .select = source_id,
         .child =
             state.source_tab == "world"
                 ? ec::list({.id = world_id,
                             .rows = world_rows,
                             .selection = "single",
                             .activate = edit_id,
                             .empty = _("No vehicle is near you.")})
                 : ec::list(
                       {.id = prototypes_id,
                        .rows =
                            found.entries | std::views::transform([&](const catalog_entry& entry) {
                                return ec::row{
                                    .id = "prototype:" + entry.id,
                                    .label = entry.name,
                                    .selected =
                                        state.doc && state.prototype().value("id", "") == entry.id,
                                    .columns = {
                                        {.key = "id",
                                         .label = _("ID"),
                                         .value = entry.id,
                                         .color = ""}}};
                            })
                            | std::ranges::to<std::vector>(),
                        .total = found.total,
                        .offset = state.page * page_size,
                        .selection = "single",
                        .activate = open_id,
                        .filterable = true,
                        .filter = state.filter,
                        .search = filter_id,
                        .page = page_id,
                        .empty = state.filter.empty()
                                   ? _("No vehicle prototype is loaded.")
                                   //~ %s is the text the player searched the prototypes for.
                                   : string_format(_("No vehicle matches “%s”."), state.filter)})}));
    affordances.push_back(
        ec::invoke_affordance({.id = source_id, .label = _("Show"), .args = row_of(sources_id)}));
    if (state.source_tab == "world") {
        const auto none =
            vehicles.empty() ? std::string(_("No vehicle is near you.")) : std::string{};
        affordances.push_back(ec::invoke_affordance(
            {.id = export_id,
             .label = _("Export design"),
             .description = _(
                 "Writes the vehicle's design as a prototype in a new file of your "
                 "mods and opens it."),
             .args = {.row = ec::row_shape{.component_id = world_id},
                      .text = ec::text_shape{.initial = state.suggested("my_vehicle")}},
             .enabled = none.empty(),
             .denial = none}));
        affordances.push_back(ec::invoke_affordance(
            {.id = copy_id,
             .label = _("Copy vehicle"),
             .description = _(
                 "Keeps the vehicle with all its state, so Paste vehicle puts it in "
                 "any world."),
             .args = row_of(world_id),
             .enabled = none.empty(),
             .denial = none}));
        affordances.push_back(ec::invoke_affordance(
            {.id = edit_id,
             .label = _("Edit in place"),
             .description = _(
                 "Edits the vehicle where it stands in the world; each change "
                 "takes effect at once."),
             .args = row_of(world_id),
             .enabled = none.empty(),
             .denial = none}));
    } else {
        affordances.push_back(ec::invoke_affordance(
            {.id = open_id, .label = _("Open"), .args = row_of(prototypes_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = filter_id, .label = _("Filter"), .args = {.text = ec::text_shape{}}}));
        affordances.push_back(page_affordance(
            {.id = page_id, .label = _("Page"), .total = found.total, .page = state.page}));
    }
    affordances.push_back(ec::invoke_affordance(
        {.id = paste_id,
         .label = _("Paste vehicle"),
         .description = _("Puts an exact copy of the copied vehicle south of you.")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = new_id,
         .label = _("New vehicle"),
         .description = _("Starts a vehicle of that name on a frame, in a new file of your mods."),
         .args = {.text = ec::text_shape{.initial = _("My vehicle")}},
         .keys = {"CTRL+N"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = std::string(show_prefix) + "prototypes", .label = _("Prototypes"), .keys = {"p"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = std::string(show_prefix) + "world", .label = _("World vehicles"), .keys = {"w"}}));
    if (!state.live && !state.doc) {
        return {
            .title = _("Vehicle Editor"),
            .root = ec::split({.id = "vehicle:body", .children = std::move(parts)}),
            .affordances = std::move(affordances),
            .focus_component = state.source_tab == "world" ? world_id : prototypes_id,
            .focus_row =
                state.source_tab == "world" && !world_rows.empty()
                    ? world_rows.front().id
                    : std::string{}};
    }
    const auto* const veh = state.shown_vehicle();
    const auto mount = tripoint_mnt_veh(state.cursor.x, state.cursor.y, 0);
    const auto picked = state.picked_name();
    // The status line: the vehicle, its file, whether it is saved, and the picked mount.
    auto status = std::vector<ec::panel_stat>{};
    if (!state.live) {
        status.push_back(
            {.key = "file", .label = _("File"), .value = shown_path(state.doc->path())});
        // Whether the document is dirty renders all of it.
        const auto dirty = state.doc->dirty();
        status.push_back(
            {.key = "saved",
             .label = _("Saved"),
             .value = dirty ? _("No") : _("Yes"),
             .tone = dirty ? "bad" : "good"});
    }
    status.push_back(
        {.key = "mount",
         .label = _("Mount"),
         .value = string_format("%d,%d", state.cursor.x, state.cursor.y)});
    status.push_back(
        {.key = "front", .label = _("Front"), .value = front_label(state.view_quarters)});
    parts.push_back(ec::panel(
        {.id = state.live ? "vehicle:live" : "vehicle:document",
         .title = state.live ? state.live->name
                             : state.prototype().value("name", state.prototype().value("id", "")),
         .stats = std::move(status)}));
    if (!state.live && !state.built.messages.empty()) {
        auto problems =
            state.built.messages | std::views::enumerate
            | std::views::transform([](const auto& entry) {
                  const auto& [index, message] = entry;
                  return ec::row{.id = index_row("message", index), .label = message.text};
              })
            | std::ranges::to<std::vector>();
        if (!state.built.blueprint && state.doc->can_undo()) {
            problems.push_back(
                {.id = "message:undo",
                 .label = _("Undo puts the prototype back as it was before this change.")});
        }
        parts.push_back(
            ec::list({.id = problems_id, .rows = std::move(problems), .selection = "none"}));
    }
    // Every step of the open document or of the vehicle edited in place, to go back to.
    parts.push_back(ec::list(
        {.id = history_id,
         .rows = step_rows(state.history()),
         .selection = "single",
         .activate = jump_id,
         .empty = _("No edits yet.")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = jump_id, .label = _("Go to step"), .args = row_of(history_id), .role = "select"}));
    // The canvas tools, the mirrors they draw with, and the mounts they act on.
    const auto mirror_row = [](const char* const key, const std::string& label, const bool on) {
        return ec::row{
            .id = std::string("canvas_tool:") + key,
            .label = label,
            .columns = {
                {.key = "state",
                 .label = _("State"),
                 .value = on ? _("On") : _("Off"),
                 .color = ""}}};
    };
    auto tool_rows =
        canvas_tools | std::views::transform([&](const canvas_tool& entry) {
            return ec::row{
                .id = std::string("canvas_tool:") + entry.key,
                .label = canvas_tool_label(entry.key),
                .hotkey = entry.hotkey,
                .selected = state.canvas_tool == entry.key};
        })
        | std::ranges::to<std::vector>();
    tool_rows.push_back(mirror_row("mirror_sides", _("Mirror left and right"), state.mirror_sides));
    tool_rows.push_back(mirror_row("mirror_ends", _("Mirror front and back"), state.mirror_ends));
    parts.push_back(ec::list(
        {.id = canvas_tools_id,
         .rows = std::move(tool_rows),
         .selection = "single",
         .activate = canvas_tool_id}));
    affordances.push_back(ec::invoke_affordance(
        {.id = canvas_tool_id,
         .label = _("Use"),
         .args = row_of(canvas_tools_id),
         .role = "select"}));
    const auto nothing_copied = state.clip.empty() ? _("Copy or cut squares first.") : "";
    for (const auto& entry : canvas_tools) {
        auto keys = std::vector<std::string>{entry.hotkey};
        if (*entry.other_key != '\0') { keys.emplace_back(entry.other_key); }
        const auto denial =
            entry.key == std::string_view("paste") ? std::string(nothing_copied) : std::string{};
        affordances.push_back(ec::invoke_affordance(
            {.id = entry.affordance,
             .label = canvas_tool_label(entry.key),
             .keys = keys,
             .enabled = denial.empty(),
             .denial = denial}));
    }
    affordances.push_back(ec::invoke_affordance(
        {.id = copy_mounts_id,
         .label = _("Copy"),
         .description = _("Copies the parts on the marked squares, or on the picked one."),
         .keys = {"CTRL+C"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = cut_mounts_id,
         .label = _("Cut"),
         .description = _(
             "Copies the parts on the marked squares, or on the picked one, and "
             "removes them."),
         .keys = {"CTRL+X"}}));
    // With squares marked, the Delete key deletes them, else the picked part.
    affordances.push_back(ec::invoke_affordance(
        {.id = delete_mounts_id,
         .label = _("Delete"),
         .description = _("Removes the parts on the marked squares, or on the picked one."),
         .keys =
             state.selection ? std::vector<std::string>{"DELETE"} : std::vector<std::string>{}}));
    affordances.push_back(
        ec::invoke_affordance({.id = mirror_sides_id, .label = _("Mirror left and right")}));
    affordances.push_back(
        ec::invoke_affordance({.id = mirror_ends_id, .label = _("Mirror front and back")}));
    affordances.push_back(ec::invoke_affordance(
        {.id = view_cw_id, .label = _("Turn view clockwise"), .keys = {"z"}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = view_ccw_id, .label = _("Turn view counterclockwise"), .keys = {"Z"}}));
    affordances.push_back(
        ec::invoke_affordance({.id = stroke_begin_id, .label = _("Begin stroke")}));
    affordances.push_back(ec::invoke_affordance({.id = stroke_end_id, .label = _("End stroke")}));
    const auto held = vpart_id(state.brush);
    const auto& used = state.tool();
    parts.push_back(grid_of(
        {.veh = veh,
         .frame = state.canvas_bounds(),
         .cursor = state.cursor,
         .held = used.kind && held.is_valid() ? std::optional(held) : std::nullopt,
         .drag = used.spans ? "span"
               : used.kind == shape_kind::point || used.key == std::string_view("remove")
                   ? "trail"
                   : "",
         .quarters = state.view_quarters,
         .selection = state.selection,
         .stamp = used.key == std::string_view("paste")
                    ? state.clip | std::views::transform(&clip_mount::offset)
                          | std::ranges::to<std::vector>()
                    : std::vector<point>{}}));
    // The design column: what the picked mount holds, the settings of the picked part and the
    // vehicle's, its cargo and spawning, and its JSON.
    const auto settings =
        state.live ? state.live_part >= 0 && state.live_part < veh->part_count()
                   : part_form(state).has_value();
    const auto tab_label = [&](const std::string_view key) -> std::string {
        if (key == "mount") { return _("Parts here"); }
        if (key == "part") { return state.live ? _("Part condition") : _("Part settings"); }
        if (key == "vehicle") { return state.live ? _("Name") : _("Name and flags"); }
        if (key == "cargo") { return _("Items here"); }
        if (key == "spawn") { return _("Test spawn"); }
        return _("Raw JSON");
    };
    const auto settings_denial =
        settings ? std::string{}
        : state.live || picked
            ? std::string(_("The picked part has nothing to set."))
            : std::string(_("Pick a part under Parts here first."));
    const auto shown_tabs = design_tabs | std::views::take(state.shown_tab_count());
    // A tab that has nothing to show, or that the vehicle lacks, shows the mount's parts instead.
    const auto shown_tab =
        (state.tool_tab == "part" && !settings)
                || !std::ranges::contains(shown_tabs | std::views::transform(&design_tab::key),
                                          std::string_view(state.tool_tab))
            ? std::string("mount")
            : state.tool_tab;
    auto tabs = std::vector<ec::tab_entry>{};
    for (const auto& entry : shown_tabs) {
        const auto key = std::string_view(entry.key);
        const auto enabled = key != "part" || settings;
        const auto denial = enabled ? std::string{} : settings_denial;
        tabs.push_back(
            {.id = std::string("tool:") + entry.key,
             .label = tab_label(key),
             .selected = key == shown_tab,
             .enabled = enabled,
             .denial = denial,
             .hotkey = entry.hotkey});
        affordances.push_back(ec::invoke_affordance(
            {.id = std::string(show_prefix) + entry.key,
             .label = tab_label(key),
             .keys = {entry.hotkey},
             .enabled = enabled,
             .denial = denial}));
    }
    const auto palette = state.parts();
    // The parts on the picked mount.
    const auto on_mount =
        veh ? veh->parts_at_relative(mount, false) | std::views::transform([&](const int index) {
                  return veh->part_info(index).get_id();
              }) | std::ranges::to<std::vector>()
            : std::vector<vpart_id>{};
    // The categories of parts are tabs, as Factorio's item groups.
    const auto installable = [&] {
        return ec::tabs(
            {.id = part_groups_id,
             .tabs = palette.groups | std::views::transform([&](const part_category group) {
                         return ec::tab_entry{
                             .id = string_format("group:%d", static_cast<int>(group)),
                             .label = category_name(group),
                             .selected = group == palette.group};
                     })
                   | std::ranges::to<std::vector>(),
             .select = part_group_id,
             .child = ec::list(
                 {.id = parts_id,
                  .rows = palette.shown | std::views::transform([&](const auto& entry) {
                              const auto& info = *entry.part;
                              // Whether it fits the picked mount, and why not, as the game
                              // rules; for the part in hand only when the last action brought
                              // no news of its own and it is not on that mount already.
                              const auto news = !state.said.empty() || !state.notice.empty();
                              const auto there = std::ranges::contains(on_mount, info.get_id());
                              const auto refused =
                                  veh && !(entry.held && (news || there))
                                      ? mount_refusal(*veh, state.cursor, info.get_id())
                                      : std::nullopt;
                              return ec::row{
                                  .id = entry.id,
                                  .label = entry.label,
                                  .description = refused.value_or(""),
                                  .selected = entry.held,
                                  .columns =
                                      {{.key = "id",
                                        .label = _("ID"),
                                        .value = info.get_id().str(),
                                        .color = ""}},
                                  .look = engine_client::world::vehicle_part_type_appearance(info)};
                          })
                        | std::ranges::to<std::vector>(),
                  .total = palette.total,
                  .offset = state.part_page * page_size,
                  .selection = "single",
                  .activate = choose_id,
                  .filterable = true,
                  .filter = state.part_filter,
                  .search = part_filter_id,
                  .page = part_page_id,
                  .empty = state.part_filter.empty()
                             ? _("No vehicle part is loaded.")
                             //~ %s is the text the player searched the vehicle parts for.
                             : string_format(_("No part matches “%s”."), state.part_filter)})});
    };
    // Remove and the moves act on the picked part, which they name.
    const auto stack_actions = [&] {
        auto actions = std::vector<ec::affordance>{ec::invoke_affordance(
            {.id = remove_id,
             .label = picked ? string_format(_("Remove %s"), *picked) : _("Remove part"),
             .keys =
                 state.selection ? std::vector<std::string>{} : std::vector<std::string>{"DELETE"},
             .enabled = picked.has_value(),
             .denial = picked ? std::string{} : _("This mount has no part to remove.")})};
        if (!state.live) {
            const auto move = [&](const char* const id, const int step) {
                const auto can = state.can_move(step);
                const auto label =
                    step < 0
                        ? (picked ? string_format(_("Move %s up"), *picked) : _("Move up"))
                        : (picked ? string_format(_("Move %s down"), *picked) : _("Move down"));
                return ec::invoke_affordance(
                    {.id = id,
                     .label = label,
                     .keys = {step < 0 ? "[" : "]"},
                     .enabled = can,
                     .denial = can ? std::string{}
                                   : _("No part on this mount is installed on that side of it.")});
            };
            actions.push_back(move(earlier_id, -1));
            actions.push_back(move(later_id, 1));
        }
        return actions;
    }();
    const auto with_actions = [&](ec::component stack) {
        std::ranges::copy(stack_actions | std::views::transform(&ec::affordance::id),
                          std::back_inserter(stack.affordances));
        return stack;
    };
    const auto tool = [&]() -> ec::component {
        const auto& tab = shown_tab;
        if (state.live) {
            if (tab == "part" || tab == "vehicle") { return live_form(state, tab == "part"); }
            return with_actions(stack_of(state));
        }
        if (tab == "part") { return std::move(*part_form(state)); }
        if (tab == "vehicle") {
            return {
                .id = props_id,
                .affordances = {prop_set_id},
                .body = ec::form_body{
                    .fields = {
                        {.id = "name",
                         .label = _("Name"),
                         .value = state.prototype().value("name", "")},
                        {.id = "id", .label = _("ID"), .value = state.prototype().value("id", "")},
                        {.id = "color_palette",
                         .label = _("Colour palette"),
                         .value = state.prototype().value("color_palette", "")},
                        {.id = "flags",
                         .label = _("Flags"),
                         .value = list_text(
                             state.prototype().value("flags", nlohmann::ordered_json()))}}}};
        }
        if (tab == "cargo") {
            const auto& items = state.prototype()["items"];
            return ec::list(
                {.id = items_id,
                 .rows = state.item_spawns() | std::views::transform([&](const std::size_t index) {
                             const auto& entry = items[index];
                             return ec::row{
                                 .id = index_row("item", index),
                                 .label = item_spawn_name(entry),
                                 .description =
                                     entry.contains("item_groups")
                                         ? std::string(_("Item group"))
                                         : std::string{},
                                 .columns = {
                                     {.key = "chance",
                                      .label = _("Chance %"),
                                      .value = std::to_string(entry.value("chance", 0)),
                                      .color = ""}}};
                         })
                       | std::ranges::to<std::vector>(),
                 .selection = "single"});
        }
        if (tab == "spawn") {
            auto form = ec::component{
                .id = spawn_form_id,
                .affordances = {spawn_set_id, spawn_id},
                .body = ec::form_body{
                    .fields =
                        {{.id = "facing",
                          .label = _("Facing"),
                          .value = std::to_string(state.spawning.facing),
                          .type = "integer",
                          .min = 0,
                          .max = 359},
                         {.id = "fuel",
                          .label = _("Fuel %"),
                          .value = std::to_string(state.spawning.fuel),
                          .type = "integer",
                          .min = 0,
                          .max = 100},
                         {.id = "condition",
                          .label = _("Condition"),
                          .value = std::to_string(state.spawning.condition),
                          .type = "select",
                          .options = {{.id = "0", .label = _("Intact")},
                                      {.id = "-1", .label = _("Lightly damaged")},
                                      {.id = "1", .label = _("Disabled")}}}},
                    .submit = spawn_id}};
            if (!state.built.blueprint) { return form; }
            return ec::split(
                {.id = "vehicle:spawn",
                 .children = {std::move(form),
                              turned_grid(*state.built.blueprint, state.spawning.facing)}});
        }
        if (tab == "json") {
            return {
                .id = field_id,
                .affordances = {field_set_id, field_apply_id},
                .body = ec::form_body{
                    .fields =
                        {{.id = "pointer", .label = _("Pointer"), .value = state.field_pointer},
                         {.id = "value",
                          .label = _("Value"),
                          .value = state.field_value,
                          .multiline = true}},
                    .submit = field_apply_id}};
        }
        // The picked part's settings right under the parts it is among.
        if (auto form = part_form(state)) {
            return ec::split(
                {.id = "vehicle:mount_parts",
                 .relation = "detail",
                 .active = stack_id,
                 .children = ec::children_of(with_actions(stack_of(state)), std::move(*form))});
        }
        return with_actions(stack_of(state));
    };
    parts.push_back(
        ec::tabs({.id = tools_id, .tabs = std::move(tabs), .select = tool_id, .child = tool()}));
    // The vehicle's figures, under the design they follow from.
    if (veh) { parts.push_back(ec::panel({.id = performance_id, .stats = design_stats(*veh)})); }
    parts.push_back(installable());
    // The items an item spawn puts, while the item spawns show.
    if (!state.live && shown_tab == "cargo") {
        const auto found = search_catalog(
            {.kind = state.item_kind,
             .filter = state.item_filter,
             .page = state.item_page,
             .page_size = page_size});
        const auto group = state.item_kind == catalog_kind::item_group;
        parts.push_back(ec::tabs(
            {.id = item_kinds_id,
             .tabs = {{.id = "kind:item", .label = _("Items"), .selected = !group},
                      {.id = "kind:item_group", .label = _("Item groups"), .selected = group}},
             .select = item_kind_id,
             .child = ec::list(
                 {.id = item_catalog_id,
                  .rows =
                      found.entries | std::views::transform([&](const catalog_entry& entry) {
                          const auto type = itype_id(entry.id);
                          return ec::row{
                              .id = (group ? "group:" : "item:") + entry.id,
                              .label = entry.name,
                              .columns =
                                  {{.key = "id", .label = _("ID"), .value = entry.id, .color = ""}},
                              .look = !group && type.is_valid()
                                        ? std::optional(engine_client::world::item_appearance(
                                              *item::spawn_temporary(type, calendar::turn)))
                                        : std::nullopt};
                      })
                      | std::ranges::to<std::vector>(),
                  .total = found.total,
                  .offset = state.item_page * page_size,
                  .selection = "single",
                  .activate = item_pick_id,
                  .filterable = true,
                  .filter = state.item_filter,
                  .search = item_filter_id,
                  .page = item_page_id,
                  .empty = state.item_filter.empty()
                             ? _("Nothing of this kind is loaded.")
                             //~ %s is the text the player searched the items for.
                             : string_format(_("Nothing matches “%s”."), state.item_filter)})}));
        affordances.push_back(ec::invoke_affordance(
            {.id = item_kind_id, .label = _("Show"), .args = row_of(item_kinds_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = item_filter_id, .label = _("Filter items"), .args = {.text = ec::text_shape{}}}));
        affordances.push_back(page_affordance(
            {.id = item_page_id,
             .label = _("Items page"),
             .total = found.total,
             .page = state.item_page}));
        affordances.push_back(ec::invoke_affordance(
            {.id = item_pick_id, .label = _("Add here"), .args = row_of(item_catalog_id)}));
    }
    affordances.push_back(
        ec::invoke_affordance({.id = tool_id, .label = _("Tab"), .args = row_of(tools_id)}));
    affordances.push_back(
        ec::invoke_affordance({.id = mount_id, .label = _("Mount"), .args = row_of(grid_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = clear_id,
         .label = _("Remove top part"),
         .description = _("Removes the part on top of the square, whatever tool is in hand."),
         .args = row_of(grid_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = part_filter_id, .label = _("Filter parts"), .args = {.text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = choose_id, .label = _("Choose"), .args = row_of(parts_id), .role = "select"}));
    affordances.push_back(ec::invoke_affordance(
        {.id = part_group_id, .label = _("Category"), .args = row_of(part_groups_id)}));
    affordances.push_back(page_affordance(
        {.id = part_page_id,
         .label = _("Parts page"),
         .total = palette.total,
         .page = state.part_page}));
    /// Offers `value` while the tool tab `tab` is shown, the one whose parts it acts on.
    const auto on = [&](const std::string_view tab, ec::affordance value) {
        if (shown_tab == tab) { affordances.push_back(std::move(value)); }
    };
    on("mount",
       ec::invoke_affordance(
           {.id = pick_id, .label = _("Pick"), .args = row_of(stack_id), .role = "select"}));
    std::ranges::copy(stack_actions, std::back_inserter(affordances));
    if (state.live) {
        for (const auto* const tab : {"part", "vehicle"}) {
            on(tab,
               ec::invoke_affordance(
                   {.id = live_set_id, .label = _("Value"), .args = text_of(live_form_id)}));
        }
        std::ranges::move(
            undo_affordances(
                {.ids = {.undo = undo_id, .redo = redo_id},
                 .can_undo = !state.live_undo.empty(),
                 .can_redo = !state.live_redo.empty()}),
            std::back_inserter(affordances));
        affordances.push_back(ec::invoke_affordance(
            {.id = done_id,
             .label = _("Done"),
             .description = _("Stops editing the vehicle in place; its changes stay in the world."),
             .keys = {"CTRL+D"}}));
    } else {
        const auto& doc = *state.doc;
        on("vehicle",
           ec::invoke_affordance(
               {.id = prop_set_id, .label = _("Value"), .args = text_of(props_id)}));
        if (shown_tab == "part" || (shown_tab == "mount" && settings)) {
            affordances.push_back(ec::invoke_affordance(
                {.id = part_set_id, .label = _("Value"), .args = text_of(part_form_id)}));
        }
        on("cargo",
           ec::invoke_affordance(
               {.id = item_add_id, .label = _("Add item"), .args = {.text = ec::text_shape{}}}));
        on("cargo",
           ec::invoke_affordance(
               {.id = item_remove_id, .label = _("Remove item"), .args = row_of(items_id)}));
        on("cargo",
           ec::invoke_affordance(
               {.id = item_chance_id,
                .label = _("Chance"),
                .args = {.row = ec::row_shape{.component_id = items_id},
                         .count = ec::count_shape{.min = 1, .max = 100, .initial = 100}}}));
        on("json",
           ec::invoke_affordance(
               {.id = field_set_id, .label = _("Value"), .args = text_of(field_id)}));
        on("json", ec::invoke_affordance({.id = field_apply_id, .label = _("Set")}));
        on("spawn",
           ec::invoke_affordance(
               {.id = spawn_set_id, .label = _("Value"), .args = text_of(spawn_form_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = spawn_id,
             .label = _("Spawn"),
             .description = _(
                 "Puts this design in the game in place of its prototype and "
                 "spawns it east of you."),
             .keys = {"F5"}}));
        affordances.push_back(
            ec::invoke_affordance({.id = turn_cw_id, .label = _("Turn clockwise"), .keys = {"t"}}));
        affordances.push_back(ec::invoke_affordance(
            {.id = turn_ccw_id, .label = _("Turn counterclockwise"), .keys = {"T"}}));
        std::ranges::
            move(history_affordances(doc, {.undo = undo_id, .redo = redo_id, .save = save_id}),
                 std::back_inserter(affordances));
        affordances.push_back(ec::invoke_affordance(
            {.id = save_as_id,
             .label = _("Save as"),
             .args =
                 {.text = ec::
                      text_shape{.initial = state.suggested(state.prototype().value("id", ""))}},
             .keys = {"F12"}}));
    }
    return {
        .title = _("Vehicle Editor"),
        .root = ec::split({.id = "vehicle:body", .active = grid_id, .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = grid_id,
        .focus_row = point_row("mount", state.cursor)};
}

} // namespace

/// The screen with what each part is for, and the menu of each action that is no part's own: with
/// nothing open, the vehicles to open are the page.
auto session_view(const session& state) -> ec::screen_view {
    const auto file = std::string(_("File"));
    const auto edit = std::string(_("Edit"));
    const auto view_menu = std::string(_("View"));
    const auto world = std::string(_("World"));
    auto view = parts_view(state);
    auto groups = std::map<std::string, std::string, std::less<>>{
        {new_id, file},         {save_id, file},          {save_as_id, file},
        {undo_id, edit},        {redo_id, edit},          {remove_id, edit},
        {earlier_id, edit},     {later_id, edit},         {copy_mounts_id, edit},
        {cut_mounts_id, edit},  {delete_mounts_id, edit}, {mirror_sides_id, edit},
        {mirror_ends_id, edit}, {view_cw_id, view_menu},  {view_ccw_id, view_menu},
        {spawn_id, world},      {turn_cw_id, world},      {turn_ccw_id, world},
        {paste_id, world},      {done_id, world}};
    for (const auto& entry : canvas_tools) { groups.emplace(entry.affordance, edit); }
    for (const auto& entry : view.affordances) {
        if (entry.id.starts_with(show_prefix)) { groups.emplace(entry.id, view_menu); }
    }
    const auto opened = state.live || state.doc;
    lay_out(
        view,
        {.slots =
             {{sources_id,
               // Folded out of the way of the vehicle open.
               {.slot = opened ? "outline" : "canvas", .label = _("Vehicles"), .collapsed = opened}},
              {"vehicle:document", {.slot = "status"}},
              {"vehicle:live", {.slot = "status"}},
              {grid_id, {.slot = "canvas"}},
              {canvas_tools_id, {.slot = "tools", .label = _("Tools")}},
              {tools_id, {.slot = "inspector", .label = _("Design")}},
              {performance_id, {.slot = "inspector", .label = _("Performance")}},
              {part_groups_id, {.slot = "palette", .label = _("Parts")}},
              {problems_id, {.slot = "log", .label = _("Problems")}},
              {history_id, {.slot = "log", .label = _("History")}},
              {"vehicle:notice", {.slot = "log"}},
              {"vehicle:status", {.slot = "log"}},
              {item_kinds_id, {.slot = "palette", .label = _("Items to add")}}},
         .groups = std::move(groups),
         .menus = {file, edit, view_menu, world}});
    return view;
}

} // namespace editor::vehicle_editor
