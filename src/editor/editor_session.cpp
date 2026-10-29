#include "editor/editor_session.h"

#include "action.h"
#include "all_enum_values.h"
#include "avatar.h"
#include "client_display.h"
#include "editor/catalog.h"
#include "editor/document.h"
#include "editor/editor_access.h"
#include "editor/map_session.h"
#include "editor/mapgen_export.h"
#include "editor/mapgen_legend_edit.h"
#include "editor/mapgen_placement.h"
#include "editor/mapgen_stamp.h"
#include "editor/mapgen_stroke.h"
#include "editor/row_id.h"
#include "editor/vehicle_session.h"
#include "engine_client_component.h"
#include "engine_client_world.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "input.h"
#include "item.h"
#include "map/map.h"
#include "output.h"
#include "overmap/overmapbuffer.h"
#include "player_activity.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "ui_manager.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {
namespace {

namespace ec = engine_client;
using namespace map_editor;

/// The documents and view the editor keeps between visits.
auto kept_session = std::optional<session>{};
/// What the next visit opens, as the main menu asked.
auto first = std::optional<recent_document>{};
/// When the turns the editor lets pass began, while they pass.
auto advancing = std::optional<time_point>{};

/// What `editor:export` writes to `path`: the squares the player marked, as the mapgen of whole
/// overmap squares when they are that and else as a nested chunk named after the file, or the
/// overmap squares around the avatar's.
auto export_to(const session& state, const std::string& path)
    -> std::expected<nlohmann::ordered_json, std::string> {
    constexpr auto side = 2 * SEEX;
    if (const auto& area = state.export_area) {
        const auto omt = project_to<coords::omt>(area->corner);
        if (project_to<coords::ms>(omt) == area->corner && area->size.x() % side == 0
            && area->size.y() % side == 0) {
            return export_region(
                omt, point_rel_omt(area->size.x() / side, area->size.y() / side),
                state.export_palette);
        }
        return export_chunk(
            area->corner, area->size, state.export_palette,
            std::filesystem::path(path).stem().string());
    }
    const auto size = state.export_size;
    return export_region(
        get_avatar().abs_omt_pos() - point_rel_omt((size.x() - 1) / 2, (size.y() - 1) / 2), size,
        state.export_palette);
}

/// What the player's actions change: the file open and each opening of it, its entry shown and its
/// steps, what the editor put in the world, and what the player wrote in its forms.
struct session_footprint {
    std::string path;
    std::size_t openings = 0;
    std::size_t object = 0;
    std::size_t steps = 0;
    std::size_t depth = 0;
    std::optional<std::size_t> saved;
    bool stamped = false;
    bool placed = false;
    point_rel_omt export_size;
    std::optional<export_squares> export_area;
    bool apply_on_save = false;
    std::vector<std::string> inputs;
    auto operator==(const session_footprint&) const -> bool = default; // *NOPAD*
};

/// What the next action may change, to tell whether it changed anything.
auto footprint(const session& state) -> session_footprint {
    return {
        .path = state.doc ? state.doc->path().string() : "",
        .openings = state.openings,
        .object = state.object,
        .steps = state.doc ? state.doc->step_count() : 0,
        .depth = state.doc ? state.doc->depth() : 0,
        .saved = state.doc ? state.doc->saved_depth() : std::nullopt,
        .stamped = state.stamped.has_value(),
        .placed = state.placed.has_value(),
        .export_size = state.export_size,
        .export_area = state.export_area,
        .apply_on_save = state.apply_on_save,
        .inputs = {
            state.made.path,       state.made.om_terrain,
            state.made.fill,       state.made.kind,
            state.made.size,       state.import_text,
            state.export_palette,  state.move_x,
            state.move_y,          state.place_kind,
            state.place_x,         state.place_y,
            state.place_value,     state.field_pointer,
            state.field_value,     state.mapping_symbol,
            state.mapping_layer,   state.mapping_value,
            state.special_terrain, state.special_connection}};
}

} // namespace

auto open_map_editor() -> void {
    if (const auto denial = editor_denial(); !denial.empty()) {
        popup(denial);
        return;
    }
    // A protocol client draws the editor; the native frontends keep the map editor they have.
    if (game_client::has_protocol_client()) {
        run_editor_session();
    } else {
        g->look_debug();
    }
}

auto reset_editor_session() -> void {
    kept_session.reset();
    first.reset();
    advancing.reset();
    allow_editor_this_session(false);
}

auto open_vehicle_editor_first() -> void { first = recent_document{.editor = "vehicle"}; }

auto open_first(const recent_document& recent) -> void { first = recent; }

auto reopens_after_waiting() -> bool {
    const auto& waiting = get_avatar().activity;
    if (!advancing || (waiting && *waiting)) { return false; }
    const auto passed = to_turns<int>(calendar::turn - *std::exchange(advancing, std::nullopt));
    (kept_session ? *kept_session : kept_session.emplace()).status =
        string_format(_("%d turns passed."), passed);
    return true;
}

auto run_editor_session() -> void {
    const auto opening = std::exchange(first, std::nullopt);
    if (opening && opening->editor == "vehicle") {
        if (!run_vehicle_editor(opening->name)) { end_editor_game(); }
        return;
    }
    auto ctxt = input_context("EDITOR");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");
    auto& state = kept_session ? *kept_session : kept_session.emplace();
    state.notice.clear();
    if (opening) { state.open(opening->name); }
    const auto screen = ec::screen_scope(
        ctxt, "editor",
        {.role = "workspace",
         .view = [&](const ec::boundary_state& /*boundary*/) { return session_view(state); }});
    while (true) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto invoked = ec::invoked(ctxt.get_raw_input());
        if (!invoked) {
            if (action == "QUIT" && state.leave()) {
                state.status.clear();
                end_editor_game();
                return;
            }
            continue;
        }
        const auto& id = invoked->affordance_id;
        const auto row = invoked->args.row.value_or("");
        // A notice stays until an action changes something, goes right or brings a notice of its
        // own.
        const auto before = footprint(state);
        auto kept = std::exchange(state.notice, {});
        state.status.clear();
        const auto keep = [&] {
            if (state.notice.empty() && state.status.empty() && footprint(state) == before) {
                state.notice = std::move(kept);
            }
        };
        state.watch();
        if (id == open_id) {
            state.open(invoked->args.text.value_or(""));
        } else if (id == open_oter_id) {
            state.open_mapgen_of(invoked->args.text.value_or(""));
        } else if (id == open_here_id) {
            state.open_mapgen_of(
                get_overmapbuffer(get_map().get_bound_dimension())
                    .ter(get_avatar().abs_omt_pos())
                    .id()
                    .str());
        } else if (id == export_id) {
            const auto path = invoked->args.text.value_or("");
            const auto exported =
                check_new_path(path)
                    .and_then([&] { return export_to(state, path); })
                    .and_then([&](const nlohmann::ordered_json& entry) {
                        return write_new_file(path, nlohmann::ordered_json::array({entry}));
                    });
            if (exported) {
                state.open(path);
            } else {
                state.notice = exported.error();
            }
        } else if (id == export_area_id) {
            if (const auto area = choose_area(_("Export which squares?"), {}, false)) {
                const auto corner = bub_to_abs(area->first);
                state.export_area = export_squares{
                    .corner = corner,
                    .size = (bub_to_abs(area->second) - corner).xy() + point_rel_ms(1, 1)};
            }
        } else if (id == export_set_id && row == "palette") {
            state.export_palette = invoked->args.text.value_or("");
        } else if (id == export_set_id && (row == "width" || row == "height")) {
            const auto text = invoked->args.text.value_or("");
            auto count = 1;
            std::from_chars(text.data(), text.data() + text.size(), count);
            count = std::clamp(count, 1, max_export);
            state.export_area.reset();
            state.export_size =
                row == "width"
                    ? point_rel_omt(count, state.export_size.y())
                    : point_rel_omt(state.export_size.x(), count);
        } else if (id == advance_id) {
            // The game's turns run outside the editor: it waits them as the avatar and opens again.
            auto wait = std::make_unique<player_activity>(
                activity_id("ACT_WAIT"),
                to_moves<int>(time_duration::from_turns(
                    std::clamp<std::int64_t>(invoked->args.count.value_or(1), 1, max_advance))),
                0);
            for (const auto type : all_enum_values<distraction_type>()) {
                wait->ignore_distraction(type);
            }
            // The wait is all the avatar does: what it was doing would hold the editor back after.
            get_avatar().cancel_activity();
            get_avatar().assign_activity(std::move(wait), false);
            advancing = calendar::turn;
            return;
        } else if (id == unstamp_id && state.stamped) {
            state.unstamp();
            state.status = _("Put back what the stamp replaced.");
        } else if (id == import_set_id) {
            state.import_text = invoked->args.text.value_or("");
        } else if (id == import_id) {
            state.import_pasted();
        } else if (id == browser_open_id || id == recent_open_id) {
            state.open(std::string(row_rest(row, "file").value_or("")));
        } else if (id == browser_filter_id) {
            state.browser_filter = invoked->args.text.value_or("");
            state.browser_page = 0;
        } else if (id == browser_page_id) {
            state.browser_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == vehicle_id) {
            if (run_vehicle_editor()) { return; }
        } else if (id == new_set_id) {
            const auto text = invoked->args.text.value_or("");
            if (row == "kind") {
                // An untouched fill follows the kind between terrains and overmap terrains.
                const auto fill_of = [](const std::string& kind) {
                    return kind == "special" || kind == "building" ? "field" : "t_floor";
                };
                if (state.made.fill == fill_of(state.made.kind)) {
                    state.made.fill = fill_of(text);
                }
            }
            (row == "path"         ? state.made.path
             : row == "om_terrain" ? state.made.om_terrain
             : row == "kind"       ? state.made.kind
             : row == "size"
                 ? state.made.size
                 : state.made.fill) = text;
        } else if (id == new_apply_id) {
            const auto made = create_mapgen(state.made);
            if (made) {
                const auto path = std::exchange(state.made.path, free_new_path());
                // The next file is named afresh, at the next free path; its kind, fill and size
                // stay as chosen.
                state.made.om_terrain.clear();
                state.open(path);
            } else {
                state.notice = made.error();
            }
        } else if (!state.doc) {
            keep();
            continue;
        } else if (id == object_id) {
            if (const auto index = row_index(row, "object")) { state.show_object(*index); }
        } else if (id == level_up_id || id == level_down_id) {
            state.climb(id == level_up_id ? 1 : -1);
        } else if (id == connection_set_id) {
            state.special_connection = invoked->args.text.value_or("");
        } else if (id == open_terrain_id) {
            if (const auto name = row_rest(row, "terrain")) {
                state.open_mapgen_of(std::string(*name));
            }
        } else if (id == open_square_id) {
            if (const auto at = row_point(row, "square")) { state.open_square(*at); }
        } else if (id == tool_id && state.on_special()) {
            const auto found = std::ranges::find(special_tools, row, &tool::id);
            if (found != special_tools.end()) { state.special_tool = &*found; }
        } else if (id == tool_id) {
            const auto found = std::ranges::find(tools, row, &tool::id);
            // An entry without rows takes only the placements tool.
            if (found != tools.end()
                && (!state.on_mapgen() || state.has_rows()
                    || found->region == region_use::placements)) {
                state.unforced = nullptr;
                state.current = &*found;
                state.anchor.reset();
            }
        } else if (id == symbol_id) {
            state.pick_symbol(std::string(row_rest(row, "symbol").value_or("")));
        } else if (id == inside_id) {
            state.room_fill = row_rest(row, "symbol").value_or("");
        } else if (id == brush_id) {
            state.brush = static_cast<int>(invoked->args.count.value_or(1));
        } else if (id == mirror_x_id) {
            state.mirror_x = !state.mirror_x;
        } else if (id == mirror_y_id) {
            state.mirror_y = !state.mirror_y;
        } else if (id == copy_id && state.selected) {
            state.copy();
        } else if (id == cut_id && state.selected) {
            state.cut();
        } else if (id == rotate_id || id == flip_x_id || id == flip_y_id) {
            const auto size = state.clip_size_now();
            for (auto& entry : state.clip_placed) {
                entry =
                    id == rotate_id ? turned(entry, size)
                    : id == flip_x_id
                        ? mirrored_x(entry, size)
                        : mirrored_y(entry, size);
            }
            state.clip =
                id == rotate_id ? rotated(state.clip)
                : id == flip_x_id
                    ? mirrored_x(state.clip)
                    : mirrored_y(state.clip);
        } else if (id == jump_id) {
            const auto depth = row_index(row, "step");
            if (depth && state.doc->jump(*depth)) { state.reload(); }
        } else if (id == paint_id) {
            if (const auto at = row_point(row, "square")) { state.paint(*at); }
        } else if (id == stroke_begin_id) {
            state.doc->begin_group();
        } else if (id == stroke_end_id) {
            state.doc->end_group();
        } else if (id == extract_palette_id) {
            state.extract_palette(invoked->args.text.value_or(""));
        } else if (id == undo_id || id == redo_id) {
            state.step_history(id == undo_id);
        } else if (id == placement_id) {
            const auto placed = placements_of(*state.doc, state.object);
            const auto found = std::ranges::find(placed, row, placement_row);
            if (found != placed.end()) { state.pick(*found); }
        } else if (id == placement_span_id) {
            state.span_placement(row, invoked->args.text.value_or(""));
        } else if (id == move_set_id) {
            (row == "x" ? state.move_x : state.move_y) = invoked->args.text.value_or("");
        } else if (id == move_apply_id && state.picked) {
            state.move_picked();
        } else if (id == placement_delete_id && state.picked) {
            state.delete_picked();
        } else if (id == place_set_id) {
            const auto text = invoked->args.text.value_or("");
            (row == "kind" ? state.place_kind
             : row == "x"  ? state.place_x
             : row == "y"
                 ? state.place_y
                 : state.place_value) = text;
        } else if (id == place_apply_id) {
            state.add_placed();
        } else if (id == field_set_id) {
            (row == "pointer" ? state.field_pointer : state.field_value) =
                invoked->args.text.value_or("");
        } else if (id == field_apply_id) {
            state.set_picked_field();
        } else if (id == diagnostic_id) {
            const auto index = row_index(row, "message");
            if (index && *index < state.diagnostics.size()) {
                state.focus = state.square_named(state.diagnostics[*index].pointer);
            }
        } else if (id == catalog_kind_id) {
            const auto found = std::ranges::find_if(catalog_uses, [&](const catalog_use& use) {
                return row == std::string("catalog:") + use.id;
            });
            if (found != catalog_uses.end()) {
                // Each tab keeps its own search.
                state.catalog_searches[state.catalog->id] =
                    {.filter = state.catalog_filter, .page = state.catalog_page};
                state.catalog = &*found;
                const auto kept = state.catalog_searches[found->id];
                state.catalog_filter = kept.filter;
                state.catalog_page = kept.page;
            }
        } else if (id == catalog_filter_id) {
            state.catalog_filter = invoked->args.text.value_or("");
            state.catalog_page = 0;
        } else if (id == catalog_page_id) {
            state.catalog_page = static_cast<std::size_t>(invoked->args.count.value_or(0));
        } else if (id == catalog_pick_id) {
            const auto entries = state.catalog_entries().entries;
            const auto found = std::ranges::find(entries, row, [](const catalog_entry& entry) {
                return "entry:" + entry.id;
            });
            if (found != entries.end() && state.on_special()) {
                state.special_terrain = found->id;
            } else if (found != entries.end() && state.has_rows()) {
                state.take(*state.catalog, found->id, catalog_row_label(*found));
            } else if (found != entries.end()) {
                state.mapping_layer = state.catalog->layer;
                state.mapping_value = mapping_of(*state.catalog, found->id);
            }
        } else if (id == mapping_set_id) {
            state.set_mapping(row, invoked->args.text.value_or(""));
        } else if (id == mapping_apply_id) {
            state.apply_mapping();
        } else if (id == preview_id) {
            state.seed = static_cast<unsigned int>(invoked->args.count.value_or(1));
            state.reload();
        } else if (id == reroll_id && state.seed) {
            state.seed = *state.seed + 1;
            state.reload();
        } else if (id == turn_id) {
            const auto found = std::ranges::find_if(preview_turns, [&](const preview_turn& turn) {
                return row == string_format("turn:%d", turn.degrees);
            });
            if (found != preview_turns.end()) {
                state.direction = found->direction;
                state.reload();
            }
        } else if (id == pin_set_id) {
            state.pin(row, invoked->args.text.value_or(""));
        } else if (id == loot_id) {
            if (state.selected) { state.roll_loot_in(*state.selected); }
        } else if (id == apply_id) {
            state.apply();
        } else if (id == apply_on_save_id && state.on_update()) {
            // An update mapgen is never applied, so the toggle is turned away as it is shown.
            state.notice = state.square_refusal().value_or("");
        } else if (id == apply_on_save_id) {
            state.apply_on_save = !state.apply_on_save;
        } else if (id == stamp_id) {
            state.stamp_here();
        } else if (id == place_special_id) {
            state.place_special();
        } else if (id == test_id && state.placed_here()) {
            g->place_player_overmap(*state.placed_here());
            return;
        } else if (id == test_id && (state.stamped || (state.on_mapgen() && in_editor_game()))) {
            // Nothing stamped yet in the editor's world: the mapgen goes where the avatar is first.
            if (!state.stamped) { state.stamp_here(); }
            if (state.stamped) {
                // The avatar plays in the middle of the stamped square; the editor keeps its
                // document.
                g->place_player(abs_to_bub(
                    project_to<coords::ms>(state.stamped->at) + tripoint_rel_ms(SEEX, SEEY, 0)));
                return;
            }
        } else if (id == save_id) {
            state.save();
        } else if (id == save_as_id) {
            state.save_as(invoked->args.text.value_or(""));
        }
        keep();
    }
}

} // namespace editor
