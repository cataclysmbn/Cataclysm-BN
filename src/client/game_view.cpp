#include "avatar.h"
#include "calendar.h"
#include "catacharset.h"
#include "client_display.h"
#include "client_presentation.h"
#include "color.h"
#include "cursesdef.h"
#include "enchantments/enchantment_vision.h"
#include "game.h"
#include "game_constants.h"
#include "game_ui.h"
#include "map/map.h"
#include "memory_fast.h"
#include "monster.h"
#include "options.h"
#include "output.h"
#include "overmap/overmap.h"
#include "overmap/overmap_ui.h"
#include "overmap/overmapbuffer.h"
#include "panels.h"
#include "point_float.h"
#include "profile.h"
#include "sounds.h"
#include "ui_manager.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

// A little helper to draw footstep glyphs.
static void draw_footsteps(const catacurses::window& window, const tripoint_rel_ms& offset) {
    for (const auto& footstep : sounds::get_footstep_markers()) {
        char glyph = '?';
        if (footstep.z() != offset.z()) { // Here z isn't an offset, but a coordinate
            glyph = footstep.z() > offset.z() ? '^' : 'v';
        }

        mvwputch(window, footstep.xy().raw() + offset.xy().raw(), c_yellow, glyph);
    }
}

shared_ptr_fast<ui_adaptor> game::create_or_get_main_ui_adaptor() {
    shared_ptr_fast<ui_adaptor> ui = main_ui_adaptor.lock();
    if (!ui) {
        main_ui_adaptor = ui = make_shared_fast<ui_adaptor>();
        ui->on_redraw([](ui_adaptor& ui) { g->draw(ui); });
        ui->on_screen_resize([this](ui_adaptor& ui) {
            // remove some space for the sidebar, this is the maximal space
            // (using standard font) that the terrain window can have
            const int sidebar_left = panel_manager::get_manager().get_width_left();
            const int sidebar_right = panel_manager::get_manager().get_width_right();

            TERRAIN_WINDOW_HEIGHT = TERMY;
            TERRAIN_WINDOW_WIDTH = TERMX - (sidebar_left + sidebar_right);
            TERRAIN_WINDOW_TERM_WIDTH = TERRAIN_WINDOW_WIDTH;
            TERRAIN_WINDOW_TERM_HEIGHT = TERRAIN_WINDOW_HEIGHT;

            /**
             * In tiles mode w_terrain can have a different font (with a different
             * tile dimension) or can be drawn by cata_tiles which uses tiles that again
             * might have a different dimension then the normal font used everywhere else.
             *
             * TERRAIN_WINDOW_WIDTH/TERRAIN_WINDOW_HEIGHT defines how many squares can
             * be displayed in w_terrain (using it's specific tile dimension), not
             * including partially drawn squares at the right/bottom. You should
             * use it whenever you want to draw specific squares in that window or to
             * determine whether a specific square is draw on screen (or outside the screen
             * and needs scrolling).
             *
             * TERRAIN_WINDOW_TERM_WIDTH/TERRAIN_WINDOW_TERM_HEIGHT defines the size of
             * w_terrain in the standard font dimension (the font that everything else uses).
             * You usually don't have to use it, expect for positioning of windows,
             * because the window positions use the standard font dimension.
             *
             * The code here calculates size available for w_terrain, caps it at
             * max_view_size (the maximal view range than any character can have at
             * any time).
             * It is stored in TERRAIN_WINDOW_*.
             */
            to_map_font_dimension(TERRAIN_WINDOW_WIDTH, TERRAIN_WINDOW_HEIGHT);

            // Position of the player in the terrain window, it is always in the center
            POSX = TERRAIN_WINDOW_WIDTH / 2;
            POSY = TERRAIN_WINDOW_HEIGHT / 2;

            w_terrain = w_terrain_ptr = catacurses::
                newwin(TERRAIN_WINDOW_HEIGHT, TERRAIN_WINDOW_WIDTH, point(sidebar_left, 0));

            // minimap is always MINIMAP_WIDTH x MINIMAP_HEIGHT in size
            w_minimap = w_minimap_ptr =
                catacurses::newwin(MINIMAP_HEIGHT, MINIMAP_WIDTH, point_zero);

            // need to init in order to avoid crash. gets updated by the panel code.
            w_pixel_minimap = catacurses::newwin(1, 1, point_zero);

            ui.position_from_window(catacurses::stdscr);
        });
        ui->mark_resize();
    }
    return ui;
}

void game::invalidate_main_ui_adaptor() const {
    shared_ptr_fast<ui_adaptor> ui = main_ui_adaptor.lock();
    if (ui) { ui->invalidate_ui(); }
}

void game::mark_main_ui_adaptor_resize() const {
    shared_ptr_fast<ui_adaptor> ui = main_ui_adaptor.lock();
    if (ui) { ui->mark_resize(); }
}

game::draw_callback_t::draw_callback_t(const std::function<void()>& cb): cb(cb) {}

game::draw_callback_t::~draw_callback_t() {
    if (added) { g->invalidate_main_ui_adaptor(); }
}

void game::draw_callback_t::operator()() {
    if (cb) { cb(); }
}

void game::add_draw_callback(const shared_ptr_fast<draw_callback_t>& cb) {
    draw_callbacks.erase(
        std::remove_if(draw_callbacks.begin(), draw_callbacks.end(),
                       [](const weak_ptr_fast<draw_callback_t>& cbw) { return cbw.expired(); }),
        draw_callbacks.end());
    draw_callbacks.emplace_back(cb);
    cb->added = true;
    invalidate_main_ui_adaptor();
}

void game::draw(ui_adaptor& ui) {
    if (test_mode) { return; }
    ZoneScopedN("game_draw");
    const auto player_visibility_was_dirty = !player_visibility_cache_current();

    // temporary fix for updating visibility for minimap
    ter_view_p.z() = (u.bub_pos() + u.view_offset).z();

    werase(w_terrain);
    {
        ZoneScopedN("game_draw_terrain");
        draw_ter();
    }
    if ((mon_info_cache_dirty || player_visibility_was_dirty)
        && player_visibility_cache_current()) {
        ZoneScopedN("game_draw_mon_info_update");
        mon_info_update();
    }
    {
        ZoneScopedN("game_draw_callbacks");
        std::erase_if(draw_callbacks, [](const auto& cb) { return cb.expired(); });
        for (const auto& cbw : draw_callbacks) {
            if (const shared_ptr_fast<draw_callback_t> cb = cbw.lock()) { (*cb)(); }
        }
    }
    {
        ZoneScopedN("game_draw_wrefresh");
        wnoutrefresh(w_terrain);
    }

    draw_panels(true);

    // Ensure that the cursor lands on the character when everything is drawn.
    // This allows screen readers to describe the area around the player, making it
    // much easier to play with them
    // (e.g. for blind players)
    ui.set_cursor(w_terrain, -u.view_offset.xy().raw() + point(POSX, POSY));
}

void game::draw_panels(bool force_draw) {
    ZoneScopedN("draw_panels");
    static int previous_turn = -1;
    const int current_turn = to_turns<int>(calendar::turn - calendar::turn_zero);
    const bool draw_this_turn = current_turn > previous_turn || force_draw;
    auto& mgr = panel_manager::get_manager();
    int y = 0;
    const bool sidebar_right = get_option<std::string>("SIDEBAR_POSITION") == "right";
    int spacer = get_option<bool>("SIDEBAR_SPACERS") ? 1 : 0;
    int log_height = 0;
    for (const window_panel& panel : mgr.get_current_layout()) {
        if (panel.get_height() != -2 && panel.toggle && panel.render()) {
            log_height += panel.get_height() + spacer;
        }
    }
    log_height = std::max(TERMY - log_height, 3);
    for (const window_panel& panel : mgr.get_current_layout()) {
        if (panel.render()) {
            // height clamped to window height.
            int h = std::min(panel.get_height(), TERMY - y);
            if (h == -2) { h = log_height; }
            h += spacer;
            if (panel.toggle && panel.render() && h > 0) {
                if (panel.always_draw || draw_this_turn) {
                    panel.draw(
                        u,
                        catacurses::newwin(
                            h, panel.get_width(),
                            point(sidebar_right ? TERMX - panel.get_width() : 0, y)));
                }
                if (show_panel_adm) {
                    const std::string panel_name = _(panel.get_name());
                    const int panel_name_width = utf8_width(panel_name);
                    auto label = catacurses::newwin(
                        1, panel_name_width,
                        point(sidebar_right ? TERMX - panel.get_width() - panel_name_width - 1
                                            : panel.get_width() + 1,
                              y));
                    werase(label);
                    mvwprintz(label, point_zero, c_light_red, panel_name);
                    wnoutrefresh(label);
                    label = catacurses::newwin(
                        h, 1,
                        point(sidebar_right ? TERMX - panel.get_width() - 1 : panel.get_width(), y));
                    werase(label);
                    if (h == 1) {
                        mvwputch(label, point_zero, c_light_red, LINE_OXOX);
                    } else {
                        mvwputch(label, point_zero, c_light_red, LINE_OXXX);
                        for (int i = 1; i < h - 1; i++) {
                            mvwputch(label, point(0, i), c_light_red, LINE_XOXO);
                        }
                        mvwputch(label, point(0, h - 1), c_light_red,
                                 sidebar_right ? LINE_XXOO : LINE_XOOX);
                    }
                    wnoutrefresh(label);
                }
                y += h;
            }
        }
    }
    previous_turn = current_turn;
}

void game::draw_pixel_minimap(const catacurses::window& w) { w_pixel_minimap = w; }

static void draw_critter_internal(
    const catacurses::window& w, const Creature& critter, const tripoint_bub_ms& center,
    bool inverted, const map& m, const avatar& u) {
    const int my = POSY + (critter.bub_pos().y() - center.y());
    const int mx = POSX + (critter.bub_pos().x() - center.x());
    if (!is_valid_in_w_terrain(point(mx, my))) { return; }
    if (critter.bub_pos().z() != center.z()) {
        static constexpr tripoint up_tripoint(tripoint_above);
        if (critter.bub_pos().z() == center.z() - 1 && (debug_mode || u.sees(critter))
            && m.valid_move(critter.bub_pos(), critter.bub_pos() + up_tripoint, false, true)) {
            // Monster is below
            // TODO: Make this show something more informative than just green 'v'
            // TODO: Allow looking at this mon with look command
            // TODO: Redraw this after weather etc. animations
            mvwputch(w, point(mx, my), c_green_cyan, 'v');
        }
        return;
    }
    if (u.sees(critter) || &critter == &u) {
        critter.draw(w, center.xy(), inverted);
        return;
    }

    if (u.sees_with_infrared(critter)
        || u.sees_with_specials(critter) != enchantment_vision_id::NULL_ID()) {
        mvwputch(w, point(mx, my), c_red, '?');
    }
}

void game::draw_critter(const Creature& critter, const tripoint_bub_ms& center) {
    draw_critter_internal(w_terrain, critter, center, false, m, u);
}

void game::draw_critter_highlighted(const Creature& critter, const tripoint_bub_ms& center) {
    draw_critter_internal(w_terrain, critter, center, true, m, u);
}

bool game::is_in_viewport(const tripoint_bub_ms& p, int margin) const {
    const tripoint_rel_ms diff(u.bub_pos() + u.view_offset - p);

    return (std::abs(diff.x()) <= getmaxx(w_terrain) / 2 - margin)
        && (std::abs(diff.y()) <= getmaxy(w_terrain) / 2 - margin);
}

void game::draw_ter(const bool draw_sounds) {
    draw_ter(u.bub_pos() + u.view_offset, is_looking, draw_sounds);
}

void game::draw_ter(const tripoint_bub_ms& center, const bool looking, const bool draw_sounds) {
    ZoneScopedN("draw_ter");
    ter_view_p = center;

    m.draw(w_terrain, center);

    if (draw_sounds) {
        draw_footsteps(
            w_terrain,
            tripoint_rel_ms(-center.x(), -center.y(), center.z()) + point_rel_ms(POSX, POSY));
    }

    for (Creature& critter : all_creatures()) { draw_critter(critter, center); }

    if (!destination_preview.empty() && u.view_offset.z() == 0) {
        // Draw auto-move preview trail
        const tripoint_bub_ms& final_destination = destination_preview.back();
        auto line_center = u.bub_pos() + u.view_offset;
        draw_line(final_destination, line_center, destination_preview, true);
        mvwputch(
            w_terrain,
            final_destination.xy().raw() - u.view_offset.xy().raw()
                + point(POSX - u.bub_pos().x(), POSY - u.bub_pos().y()),
            c_white, 'X');
    }

    if ((u.controlling_vehicle || remoteveh()) && !looking) {
        draw_veh_dir_indicator(false);
        draw_veh_dir_indicator(true);
    }
    // Place the cursor over the player as is expected by screen readers.
    wmove(w_terrain, -center.xy().raw() + g->u.bub_pos().xy().raw() + point(POSX, POSY));
}

std::optional<tripoint_rel_ms> game::get_veh_dir_indicator_location(bool next) {
    if (!get_option<bool>("VEHICLE_DIR_INDICATOR")) { return std::nullopt; }
    if (vehicle* veh = remoteveh()) {
        rl_vec2d face = next ? veh->dir_vec() : veh->face_vec();
        float r = 10.0;
        return tripoint_rel_ms(
            static_cast<int>(r * face.x), static_cast<int>(r * face.y), veh->bub_ms_location().z());
    }
    const optional_vpart_position vp = m.veh_at(u.bub_pos());
    if (!vp) { return std::nullopt; }
    vehicle* const veh = &vp->vehicle();
    rl_vec2d face = next ? veh->dir_vec() : veh->face_vec();
    float r = 10.0;
    return tripoint_rel_ms(
        static_cast<int>(r * face.x), static_cast<int>(r * face.y), u.bub_pos().z());
}

void game::draw_veh_dir_indicator(bool next) {
    if (const std::optional<tripoint_rel_ms> indicator_offset = get_veh_dir_indicator_location(
            next)) {
        auto col = next ? c_white : c_dark_gray;
        mvwputch(w_terrain,
                 indicator_offset->xy().raw() - u.view_offset.xy().raw() + point(POSX, POSY), col,
                 'X');
    }
}

void game::draw_minimap() {

    // Draw the box
    werase(w_minimap);
    draw_border(w_minimap);

    const tripoint_abs_omt curs = u.abs_omt_pos();
    const point_abs_omt curs2(curs.xy());
    const tripoint_abs_omt targ = u.get_active_mission_target();
    bool drew_mission = targ == overmap::invalid_tripoint;

    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            const point_abs_omt om(curs2 + point(i, j));
            nc_color ter_color;
            tripoint_abs_omt omp(om, get_levz());
            std::string ter_sym;
            const bool seen = get_overmapbuffer(current_dimension_id_).seen(omp);
            const bool vehicle_here = get_overmapbuffer(current_dimension_id_).has_vehicle(omp);
            if (get_overmapbuffer(current_dimension_id_).has_note(omp)) {

                const std::string& note_text = get_overmapbuffer(current_dimension_id_).note(omp);

                const auto note_info = overmap_ui::get_note_display_info(note_text);
                ter_color = std::get<1>(note_info);
                ter_sym = std::string(1, std::get<0>(note_info));
            } else if (!seen) {
                ter_sym = " ";
                ter_color = c_black;
            } else if (vehicle_here) {
                ter_color = c_cyan;
                ter_sym = "c";
            } else {
                const oter_id& cur_ter = get_overmapbuffer(current_dimension_id_).ter(omp);
                ter_sym = cur_ter->get_symbol();
                if (get_overmapbuffer(current_dimension_id_).is_explored(omp)) {
                    ter_color = c_dark_gray;
                } else {
                    ter_color = cur_ter->get_color();
                }
            }
            if (!drew_mission && targ.xy() == omp.xy()) {
                // If there is a mission target, and it's not on the same
                // overmap terrain as the player character, mark it.
                // TODO: Inform player if the mission is above or below
                drew_mission = true;
                if (i != 0 || j != 0) { ter_color = red_background(ter_color); }
            }
            if (i == 0 && j == 0) {
                mvwputch_hi(w_minimap, point(3, 3), ter_color, ter_sym);
            } else {
                mvwputch(w_minimap, point(3 + i, 3 + j), ter_color, ter_sym);
            }
        }
    }

    // Print arrow to mission if we have one!
    if (!drew_mission) {
        double slope =
            curs2.x() != targ.x()
                ? static_cast<double>(targ.y() - curs2.y()) / (targ.x() - curs2.x())
                : 4;

        if (curs2.x() == targ.x() || std::fabs(slope) > 3.5) { // Vertical slope
            if (targ.y() > curs2.y()) {
                mvwputch(w_minimap, point(3, 6), c_red, "*");
            } else {
                mvwputch(w_minimap, point(3, 0), c_red, "*");
            }
        } else {
            int arrowx = -1;
            int arrowy = -1;
            if (std::fabs(slope) >= 1.) { // y diff is bigger!
                arrowy = targ.y() > curs2.y() ? 6 : 0;
                arrowx = static_cast<int>(3 + 3 * (targ.y() > curs2.y() ? slope : (0 - slope)));
                if (arrowx < 0) { arrowx = 0; }
                if (arrowx > 6) { arrowx = 6; }
            } else {
                arrowx = targ.x() > curs2.x() ? 6 : 0;
                arrowy = static_cast<int>(3 + 3 * (targ.x() > curs2.x() ? slope : -slope));
                if (arrowy < 0) { arrowy = 0; }
                if (arrowy > 6) { arrowy = 6; }
            }
            char glyph = '*';
            if (targ.z() > u.bub_pos().z()) {
                glyph = '^';
            } else if (targ.z() < u.bub_pos().z()) {
                glyph = 'v';
            }

            mvwputch(w_minimap, point(arrowx, arrowy), c_red, glyph);
        }
    }

    const int sight_points = g->u.overmap_sight_range(g->light_level(g->u.abs_pos().z()));
    for (int i = -3; i <= 3; i++) {
        for (int j = -3; j <= 3; j++) {
            if (i > -3 && i < 3 && j > -3 && j < 3) {
                continue; // only do hordes on the border, skip inner map
            }
            const tripoint_abs_omt omp(curs2 + point(i, j), g->u.abs_pos().z());
            if (get_overmapbuffer(current_dimension_id_).get_horde_size(omp)
                >= HORDE_VISIBILITY_SIZE) {
                if (get_overmapbuffer(current_dimension_id_).seen(omp)
                    && g->u.overmap_los(omp, sight_points)) {
                    mvwputch(
                        w_minimap, point(i + 3, j + 3), c_green,
                        get_overmapbuffer(current_dimension_id_).get_horde_size(omp)
                                > HORDE_VISIBILITY_SIZE * 2
                            ? 'Z'
                            : 'z');
                }
            }
        }
    }

    wnoutrefresh(w_minimap);
}
