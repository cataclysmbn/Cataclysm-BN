#include "editor/vehicle_session.h"

#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/vehicle_design.h"
#include "engine_client_component.h"
#include "engine_client_world.h"
#include "input.h"
#include "output.h"
#include "path_info.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "units_utility.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace editor {
namespace {

namespace ec = engine_client;

constexpr auto prototypes_id = "vehicle:prototypes";
constexpr auto grid_id = "vehicle:grid";
constexpr auto stack_id = "vehicle:stack";
constexpr auto parts_id = "vehicle:parts";
constexpr auto field_id = "vehicle:field";
constexpr auto diagnostics_id = "vehicle:diagnostics";

constexpr auto open_id = "vehicle:open";
constexpr auto filter_id = "vehicle:filter";
constexpr auto page_id = "vehicle:page";
constexpr auto mount_id = "vehicle:mount";
constexpr auto pick_id = "vehicle:pick";
constexpr auto remove_id = "vehicle:remove";
constexpr auto install_id = "vehicle:install";
constexpr auto part_filter_id = "vehicle:part_filter";
constexpr auto part_page_id = "vehicle:part_page";
constexpr auto field_set_id = "vehicle:field_set";
constexpr auto field_apply_id = "vehicle:field_apply";
constexpr auto undo_id = "vehicle:undo";
constexpr auto redo_id = "vehicle:redo";
constexpr auto save_id = "vehicle:save";
constexpr auto save_as_id = "vehicle:save_as";

constexpr auto page_size = std::size_t{50};

auto pages_of(const std::size_t total) -> std::int64_t {
    return static_cast<std::int64_t>(std::max<std::size_t>(1, (total + page_size - 1) / page_size));
}

auto mount_text(const point& at) -> std::string { return string_format("mount:%d,%d", at.x, at.y); }

auto mount_of(const std::string& id) -> std::optional<point> {
    auto at = point{};
    if (std::sscanf(id.c_str(), "mount:%d,%d", &at.x, &at.y) != 2) { return std::nullopt; }
    return at;
}

/// Whether `path` lies inside `root`.
auto inside(const std::filesystem::path& path, const std::string& root) -> bool {
    auto error = std::error_code{};
    const auto base = std::filesystem::weakly_canonical(root, error);
    const auto relative = std::filesystem::weakly_canonical(path, error).lexically_relative(base);
    return !error && !relative.empty() && *relative.begin() != "..";
}

struct session {
    std::optional<document> doc = std::nullopt;
    /// The prototype's object in `doc`.
    std::size_t object = 0;
    std::string notice;
    /// What the game builds from the object now.
    built_design built;
    point cursor;
    std::string filter;
    std::size_t page = 0;
    std::string part_filter;
    std::size_t part_page = 0;
    /// What the field form sets: a pointer into the prototype and a value as JSON or bare text;
    /// an empty value removes it.
    std::string field_pointer = "/name";
    std::string field_value;

    auto prototype() const -> const nlohmann::ordered_json& { // *NOPAD*
        return doc->objects()[object].value;
    }

    auto prototypes() const -> catalog_page {
        return search_catalog(
            {.kind = catalog_kind::vehicle, .filter = filter, .page = page, .page_size = page_size});
    }

    /// The parts that match the filter, ordered by id: every part on all pages, and those on this
    /// one.
    struct part_page_entries {
        std::vector<const vpart_info*> shown;
        std::size_t total = 0;
    };
    auto parts() const -> part_page_entries {
        auto found =
            vpart_info::get_all() | std::views::filter([&](const vpart_info& info) {
                return part_filter.empty() || lcmatch(info.get_id().str(), part_filter)
                    || lcmatch(info.name(), part_filter);
            })
            | std::views::transform([](const vpart_info& info) { return &info; })
            | std::ranges::to<std::vector>();
        std::ranges::sort(found, {}, [](const vpart_info* info) { return info->get_id().str(); });
        const auto total = found.size();
        const auto first = std::min(total, part_page * page_size);
        return {.shown = {found.begin() + first, found.begin() + std::min(total, first + page_size)},
                .total = total};
    }

    auto reload() -> void {
        built = build_design(prototype().dump());
        field_value = value_at(field_pointer);
    }

    /// The JSON at `pointer` in the prototype; empty when there is none.
    auto value_at(const std::string& pointer) const -> std::string {
        try {
            const auto at = nlohmann::ordered_json::json_pointer(pointer);
            if (!prototype().contains(at)) { return {}; }
            const auto& value = prototype()[at];
            return value.is_string() ? value.get<std::string>() : value.dump();
        } catch (const nlohmann::ordered_json::exception&) { return {}; }
    }

    auto open(const std::string& id) -> void {
        const auto path = prototype_file(id);
        if (!path) {
            notice = string_format(_("No data file of the loaded mods defines %s."), id);
            return;
        }
        auto opened = document::open(*path);
        if (!opened) {
            notice = opened.error();
            return;
        }
        const auto& objects = opened->objects();
        const auto found = std::ranges::find_if(objects, [&](const document_object& entry) {
            return entry.value.is_object() && entry.value.value("type", "") == "vehicle"
                && entry.value.value("id", "") == id;
        });
        if (found == objects.end()) {
            notice = string_format(_("No data file of the loaded mods defines %s."), id);
            return;
        }
        object = static_cast<std::size_t>(found - objects.begin());
        doc = std::move(*opened);
        cursor = point{};
        field_pointer = "/name";
        reload();
    }

    /// Replaces the prototype by `next` as one undo step.
    auto change(const std::expected<nlohmann::ordered_json, std::string>& next) -> void {
        if (!next) {
            notice = next.error();
            return;
        }
        const auto edited = doc->edit(object, nlohmann::ordered_json::diff(prototype(), *next));
        if (!edited) { notice = edited.error(); }
        reload();
    }

    auto install(const std::string& part) -> void {
        if (!built.blueprint) {
            notice = _("The game cannot build this prototype; fix what it reports first.");
            return;
        }
        change(install_part(prototype(), *built.blueprint, {.mount = cursor, .part = part}));
    }

    auto remove(const std::size_t index) -> void {
        if (!built.blueprint) {
            notice = _("The game cannot build this prototype; fix what it reports first.");
            return;
        }
        change(remove_part(prototype(), *built.blueprint, index));
    }

    auto set_field() -> void {
        auto next = prototype();
        try {
            const auto at = nlohmann::ordered_json::json_pointer(field_pointer);
            if (field_value.empty()) {
                next.at(at.parent_pointer()).erase(at.back());
            } else {
                const auto parsed = nlohmann::ordered_json::parse(field_value, nullptr, false);
                next[at] = parsed.is_discarded() ? nlohmann::ordered_json(field_value) : parsed;
            }
        } catch (const nlohmann::ordered_json::exception& e) {
            notice = e.what();
            return;
        }
        if (!next.is_object() || next.value("type", "") != "vehicle" || !next.contains("id")) {
            notice = _("A prototype keeps its type and id.");
            return;
        }
        change(next);
    }

    auto written(const std::expected<void, save_error>& result) -> bool {
        if (!result) {
            switch (result.error()) {
                case save_error::changed_on_disk:
                    notice = _("The file changed on disk since it was opened; nothing was saved.");
                    break;
                case save_error::exists:
                    notice = _("That file already exists; nothing was saved.");
                    break;
                case save_error::unwritable:
                    notice = _("The file could not be written; save it as a file in your mods.");
                    break;
            }
        }
        return result.has_value();
    }

    auto save() -> bool { return written(doc->save()); }

    auto save_as(const std::string& name) -> void {
        const auto path = std::filesystem::path(name);
        if (path.extension() != ".json" || !inside(path, PATH_INFO::user_moddir())) {
            notice = string_format(
                _("Save as writes a .json file in your mods, %s."), PATH_INFO::user_moddir());
            return;
        }
        written(doc->save_as(path));
    }

    /// Whether to leave: at once with nothing unsaved, else as the player answers.
    auto leave() -> bool {
        if (!doc || !doc->dirty()) { return true; }
        auto menu = uilist{};
        menu.text = _("Your changes to this file are not saved.");
        menu.addentry(0, true, 's', _("Save and leave"));
        menu.addentry(1, true, 'd', _("Leave without saving"));
        menu.addentry(2, true, 'c', _("Stay"));
        menu.query();
        if (menu.ret == 1) { doc.reset(); }
        return menu.ret == 1 || (menu.ret == 0 && save());
    }
};

/// The mounts of the prototype and a ring of free ones around them, a cell each, west to east and
/// north to south as its blueprint reads.
auto grid_of(const session& state) -> ec::component {
    const auto parts = design_parts(state.prototype());
    auto mounts =
        parts | std::views::transform(&design_part::mount) | std::ranges::to<std::vector>();
    mounts.push_back(state.cursor);
    const auto [min_x, max_x] = std::ranges::minmax(mounts | std::views::transform(&point::x));
    const auto [min_y, max_y] = std::ranges::minmax(mounts | std::views::transform(&point::y));
    const auto origin = point(min_x - 1, min_y - 1);
    auto palette = ec::look_palette{};
    auto cells = std::vector<ec::grid_cell>{};
    const auto* const veh = state.built.blueprint.get();
    for (auto y = min_y - 1; y <= max_y + 1; ++y) {
        for (auto x = min_x - 1; x <= max_x + 1; ++x) {
            const auto at = point(x, y);
            auto cell = ec::grid_cell{
                .at = {x - origin.x, y - origin.y},
                .id = mount_text(at),
                .glyph = " ",
                .affordance = mount_id,
                .selected = at == state.cursor};
            const auto shown = veh ? veh->part_displayed_at(tripoint_mnt_veh(x, y, 0)) : -1;
            if (shown >= 0) {
                cell.glyph = std::string(1, veh->part_sym(shown));
                cell.label = remove_color_tags(veh->cpart(shown).name());
                cell.color = string_from_color(veh->part_color(shown));
                cell.looks = palette.indexes({engine_client::world::vehicle_part_appearance(
                    *veh, shown,
                    static_cast<int>(std::round(to_degrees(
                        normalize(veh->part_display_direction(shown) - veh->face.dir())))))});
            }
            cells.push_back(std::move(cell));
        }
    }
    return {
        .id = grid_id,
        .affordances = {mount_id},
        .body = ec::grid_body{
            .cols = max_x - min_x + 3,
            .rows = max_y - min_y + 3,
            .cursor = ec::grid_point{state.cursor.x - origin.x, state.cursor.y - origin.y},
            .cells = std::move(cells),
            .looks = std::move(palette.looks),
            .role = "canvas"}};
}

auto session_view(const session& state) -> ec::screen_view {
    const auto row_of = [](const char* const component) {
        return ec::arg_shape{.row = ec::row_shape{.component_id = component}};
    };
    auto parts = std::vector<ec::component>{};
    auto affordances = std::vector<ec::affordance>{};
    const auto found = state.prototypes();
    parts.push_back(ec::list(
        {.id = prototypes_id,
         .rows =
             found.entries | std::views::transform([&](const catalog_entry& entry) {
                 return ec::row{
                     .id = "prototype:" + entry.id,
                     .label = entry.name,
                     .selected = state.doc && state.prototype().value("id", "") == entry.id,
                     .columns = {{.key = "id", .label = _("ID"), .value = entry.id, .color = ""}}};
             })
             | std::ranges::to<std::vector>(),
         .total = found.total,
         .offset = state.page * page_size,
         .selection = "single",
         .activate = open_id,
         .filterable = true}));
    affordances.push_back(
        ec::invoke_affordance({.id = open_id, .label = _("Open"), .args = row_of(prototypes_id)}));
    affordances.push_back(ec::invoke_affordance(
        {.id = filter_id, .label = _("Filter"), .args = {.text = ec::text_shape{}}}));
    affordances.push_back(ec::invoke_affordance(
        {.id = page_id,
         .label = _("Page"),
         .args = {
             .count = ec::count_shape{
                 .min = 0,
                 .max = pages_of(found.total) - 1,
                 .initial = static_cast<std::int64_t>(state.page)}}}));
    if (state.doc) {
        const auto& doc = *state.doc;
        auto stats = std::vector<ec::panel_stat>{
            {.key = "saved",
             .label = _("Saved"),
             .value = doc.dirty() ? _("No") : _("Yes"),
             .tone = doc.dirty() ? "bad" : "good"},
            {.key = "mount",
             .label = _("Mount"),
             .value = string_format("%d,%d", state.cursor.x, state.cursor.y)}};
        if (state.built.blueprint) {
            std::ranges::move(design_stats(*state.built.blueprint), std::back_inserter(stats));
        }
        parts.push_back(ec::panel(
            {.id = "vehicle:document",
             .title = state.prototype().value("name", state.prototype().value("id", "")),
             .subtitle = doc.path().generic_string(),
             .stats = std::move(stats)}));
        parts.push_back(grid_of(state));
        const auto design = design_parts(state.prototype());
        parts.push_back(ec::list(
            {.id = stack_id,
             .rows = design | std::views::enumerate | std::views::filter([&](const auto& entry) {
                         return std::get<1>(entry).mount == state.cursor;
                     })
                   | std::views::transform([&](const auto& entry) {
                         const auto& [index, part] = entry;
                         const auto id = vpart_id(part.part);
                         return ec::row{
                             .id = string_format("part:%d", index),
                             .label = id.is_valid() ? id->name() : part.part,
                             .description = part.pointer,
                             .selected = state.field_pointer == part.pointer};
                     })
                   | std::ranges::to<std::vector>(),
             .selection = "single",
             .activate = pick_id}));
        const auto installable = state.parts();
        const auto mount = tripoint_mnt_veh(state.cursor.x, state.cursor.y, 0);
        parts.push_back(ec::list(
            {.id = parts_id,
             .rows = installable.shown | std::views::transform([&](const vpart_info* info) {
                         const auto mounts =
                             state.built.blueprint
                             && state.built.blueprint->can_mount(mount, info->get_id());
                         return ec::row{
                             .id = "vpart:" + info->get_id().str(),
                             .label = info->name(),
                             .denial = mounts ? std::string{} : _("It cannot be mounted here."),
                             .enabled = mounts,
                             .columns = {
                                 {.key = "id",
                                  .label = _("ID"),
                                  .value = info->get_id().str(),
                                  .color = ""}}};
                     })
                   | std::ranges::to<std::vector>(),
             .total = installable.total,
             .offset = state.part_page * page_size,
             .selection = "single",
             .activate = install_id,
             .filterable = true}));
        parts.push_back(ec::component{
            .id = field_id,
            .affordances = {field_set_id, field_apply_id},
            .body = ec::form_body{
                .fields =
                    {{.id = "pointer", .label = _("Pointer"), .value = state.field_pointer},
                     {.id = "value",
                      .label = _("Value"),
                      .value = state.field_value,
                      .multiline = true}},
                .submit = field_apply_id}});
        parts.push_back(ec::list(
            {.id = diagnostics_id,
             .rows = state.built.messages | std::views::enumerate
                   | std::views::transform([](const auto& entry) {
                         const auto& [index, message] = entry;
                         return ec::row{
                             .id = string_format("message:%d", index),
                             .label = message.text,
                             .selectable = false};
                     })
                   | std::ranges::to<std::vector>(),
             .selection = "none"}));
        affordances.push_back(
            ec::invoke_affordance({.id = mount_id, .label = _("Mount"), .args = row_of(grid_id)}));
        affordances.push_back(
            ec::invoke_affordance({.id = pick_id, .label = _("Edit"), .args = row_of(stack_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = remove_id, .label = _("Remove"), .args = row_of(stack_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = install_id, .label = _("Install"), .args = row_of(parts_id)}));
        affordances.push_back(ec::invoke_affordance(
            {.id = part_filter_id, .label = _("Filter parts"), .args = {.text = ec::text_shape{}}}));
        affordances.push_back(ec::invoke_affordance(
            {.id = part_page_id,
             .label = _("Parts page"),
             .args = {
                 .count = ec::count_shape{
                     .min = 0,
                     .max = pages_of(installable.total) - 1,
                     .initial = static_cast<std::int64_t>(state.part_page)}}}));
        affordances.push_back(ec::invoke_affordance(
            {.id = field_set_id,
             .label = _("Value"),
             .args = {.row = ec::row_shape{.component_id = field_id}, .text = ec::text_shape{}}}));
        affordances.push_back(ec::invoke_affordance({.id = field_apply_id, .label = _("Set")}));
        affordances.push_back(
            ec::invoke_affordance({.id = undo_id, .label = _("Undo"), .enabled = doc.can_undo()}));
        affordances.push_back(
            ec::invoke_affordance({.id = redo_id, .label = _("Redo"), .enabled = doc.can_redo()}));
        affordances.push_back(
            ec::invoke_affordance({.id = save_id, .label = _("Save"), .enabled = doc.dirty()}));
        affordances.push_back(ec::invoke_affordance(
            {.id = save_as_id, .label = _("Save as"), .args = {.text = ec::text_shape{}}}));
    }
    if (!state.notice.empty()) {
        parts.push_back(
            {.id = "vehicle:notice", .body = ec::text_body{.markup = state.notice, .tone = "bad"}});
    }
    return {
        .title = _("Vehicle Editor"),
        .root = ec::split(
            {.id = "vehicle:body",
             .active = state.doc ? grid_id : prototypes_id,
             .children = std::move(parts)}),
        .affordances = std::move(affordances),
        .focus_component = state.doc ? grid_id : prototypes_id,
        .focus_row = state.doc ? mount_text(state.cursor) : ""};
}

auto kept_session = std::optional<session>{};

/// The index a row id names after its prefix.
auto index_of(const std::string& row, const char* const format) -> std::optional<std::size_t> {
    auto index = std::size_t{0};
    return std::sscanf(row.c_str(), format, &index) == 1 ? std::optional(index) : std::nullopt;
}

} // namespace

auto reset_vehicle_editor() -> void { kept_session.reset(); }

auto run_vehicle_editor() -> void {
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto& state = kept_session ? *kept_session : kept_session.emplace();
    state.notice.clear();
    const auto screen = ec::screen_scope(
        ctxt, "vehicle_editor",
        {.role = "screen",
         .view = [&](const ec::boundary_state& /*boundary*/) { return session_view(state); }});
    while (true) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto invoked = ec::invoked(ctxt.get_raw_input());
        if (!invoked) {
            if (action == "QUIT" && state.leave()) { return; }
            continue;
        }
        const auto& id = invoked->affordance_id;
        const auto row = invoked->args.row.value_or("");
        const auto text = invoked->args.text.value_or("");
        state.notice.clear();
        if (id == open_id) {
            state.open(row.substr(std::string_view("prototype:").size()));
        } else if (id == filter_id) {
            state.filter = text;
            state.page = 0;
        } else if (id == page_id) {
            state.page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (!state.doc) {
            continue;
        } else if (id == mount_id) {
            if (const auto at = mount_of(row)) { state.cursor = *at; }
        } else if (id == pick_id) {
            if (const auto index = index_of(row, "part:%zu")) {
                const auto design = design_parts(state.prototype());
                if (*index < design.size()) {
                    state.field_pointer = design[*index].pointer;
                    state.field_value = state.value_at(state.field_pointer);
                }
            }
        } else if (id == remove_id) {
            if (const auto index = index_of(row, "part:%zu")) { state.remove(*index); }
        } else if (id == install_id) {
            state.install(row.substr(std::string_view("vpart:").size()));
        } else if (id == part_filter_id) {
            state.part_filter = text;
            state.part_page = 0;
        } else if (id == part_page_id) {
            state.part_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == field_set_id) {
            if (row == "pointer") {
                state.field_pointer = text;
                state.field_value = state.value_at(text);
            } else {
                state.field_value = text;
            }
        } else if (id == field_apply_id) {
            state.set_field();
        } else if (id == undo_id || id == redo_id) {
            if (id == undo_id ? state.doc->undo() : state.doc->redo()) { state.reload(); }
        } else if (id == save_id) {
            state.save();
        } else if (id == save_as_id) {
            state.save_as(text);
        }
    }
}

} // namespace editor
