#if defined(CATA_MCP)

#    include "action.h"
#    include "avatar.h"
#    include "bionics.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "game.h"
#    include "game_session.h"
#    include "gamemode.h"
#    include "init.h"
#    include "input.h"
#    include "json.h"
#    include "map/map.h"
#    include "map/mapdata.h"
#    include "map_helpers.h"
#    include "map_memory.h"
#    include "messages.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "ui_manager.h"
#    include "uistate.h"

#    include <algorithm>
#    include <array>
#    include <functional>
#    include <memory>
#    include <ranges>
#    include <sstream>

namespace {
namespace client = game_client;

/// Test-only member access keeps the real dispatch/exit boundary intact. do_turn()
/// cannot expose that boundary without another draw or calendar advance. Explicit
/// instantiation permits these two member pointers without changing game headers,
/// access macros, the production dispatcher, or either checkout's API.
struct action_member {
    using type = auto (game::*)() -> bool;
    friend auto native_member(action_member) -> type;
};
struct mode_member {
    using type = std::unique_ptr<special_game> game::*;
    friend auto native_member(mode_member) -> type;
};
template <typename Tag, typename Tag::type Member> struct native_test_member {
    friend auto native_member(Tag /*tag*/) -> typename Tag::type { return Member; }
};
template struct native_test_member<action_member, &game::handle_action>;
template struct native_test_member<mode_member, &game::gamemode>;

auto native_mode() -> std::unique_ptr<special_game>& {
    return g.get()->*native_member(mode_member{});
} // *NOPAD*

struct remembered_cell {
    int symbol;
    memorized_terrain_tile terrain;
    memorized_terrain_tile overlay;
};

/// These getters read stored knowledge; they do not acquire perception or draw.
auto remembered(const tripoint_abs_ms& p) -> remembered_cell {
    const auto& you = get_avatar();
    return {.symbol = you.get_memorized_symbol(p),
            .terrain = you.get_terrain_tile(p),
            .overlay = you.get_memorized_tile(p)};
}

auto check_same(const remembered_cell& actual, const remembered_cell& expected) -> void {
    CHECK(actual.symbol == expected.symbol);
    CHECK(actual.terrain == expected.terrain);
    CHECK(actual.overlay == expected.overlay);
}

auto check_unknown(const tripoint_abs_ms& p) -> void {
    const auto cell = remembered(p);
    REQUIRE(cell.symbol == 0);
    REQUIRE(cell.terrain.tile.empty());
    REQUIRE(cell.overlay.tile.empty());
}

/// Drive DEFAULTMODE -> ACTION_BIONICS -> native manager selection -> manager close.
/// No activate_bionic call, synthetic acquisition, or passive world observation is used.
auto dispatch_bionics(const int toggles, const std::function<void()>& before_off = {}) -> void {
    auto phase = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto context = client::active_input_context();
        REQUIRE(context.context != nullptr);
        REQUIRE(getmaxx(g->w_terrain) == 9);
        REQUIRE(getmaxy(g->w_terrain) == 9);
        auto request = client::input_command{};
        if (phase == 0) {
            REQUIRE(context.category == "DEFAULTMODE");
            request.action = action_ident(ACTION_BIONICS);
        } else {
            REQUIRE(context.category == "BIONICS");
            if (phase >= 1 && phase <= toggles) {
                if (phase == 2 && before_off) { before_off(); }
                // Both managers consume this live installed-CBM invlet via ANY_INPUT.
                // No semantic provider/schema/opaque ID is needed or observed.
                const auto shortcut =
                    get_avatar().get_bionic_state(bionic_id("bio_flashlight")).invlet;
                REQUIRE(shortcut == 'z');
                const auto event = input_event{shortcut, input_event_t::keyboard};
                namespace ranges = std::ranges;
                REQUIRE(
                    ranges::contains(context.context->get_registered_actions_copy(), "ANY_INPUT"));
                REQUIRE(context.context->input_to_action(event) == "ERROR");
                ++phase;
                return event;
            }
            request.action = "QUIT";
        }
        REQUIRE(phase < (toggles + 2));
        ++phase;
        const auto event = client::resolve_input_command(request, client::memory::screen_size());
        INFO("native input resolution: " << (event ? "resolved" : event.error()));
        REQUIRE(event.has_value());
        REQUIRE(context.context->input_to_action(*event) == request.action);
        return *event;
    });
    const auto reset_input = on_out_of_scope([]() { client::memory::set_input_provider({}); });
    REQUIRE((g.get()->*native_member(action_member{}))());
    REQUIRE(phase == (toggles + 2));
}

/// Use only baseline/P2-common APIs. In particular the fixture never calls map_perception::acquire.
template <typename Test> auto with_dark_room(const Test& test) -> void {
    const auto memory = client::memory::scoped_state{};
    const auto mode = restore_on_out_of_scope(test_mode);
    const auto turn = restore_on_out_of_scope(calendar::turn);
    const auto rng = restore_on_out_of_scope(rng_get_engine());
    const auto threshold = restore_on_out_of_scope(g_visible_threshold);
    const auto running = game_session::running();
    const auto sort = uistate.bionic_sort_mode;
    auto messages = std::ostringstream{};
    auto writer = JsonOut{messages};
    writer.start_object();
    Messages::serialize(writer);
    writer.end_object();
    const auto restore_messages = on_out_of_scope([saved = messages.str()]() {
        auto stream = std::istringstream{saved};
        auto reader = JsonIn{stream};
        Messages::deserialize(reader.get_object());
    });
    auto saved_avatar = std::move(get_avatar());
    auto saved_gamemode = std::move(native_mode());
    const auto restore_actor = on_out_of_scope([&]() {
        get_avatar() = std::move(saved_avatar);
        native_mode() = std::move(saved_gamemode);
        game_session::set_running(running);
        uistate.bionic_sort_mode = sort;
    });
    get_avatar() = avatar{};
    native_mode() = std::make_unique<special_game>();
    test_mode = true;
    clear_all_state();
    const auto clear_fixture = on_out_of_scope([]() {
        test_mode = true;
        clear_all_state();
    });
    clear_character(get_avatar(), false);
    const auto animations = override_option("ANIMATIONS", "false");
    const auto realtime = override_option("TURN_DURATION", "0");
    const auto width = restore_on_out_of_scope(TERMX);
    const auto height = restore_on_out_of_scope(TERMY);
    const auto full_width = restore_on_out_of_scope(FULL_SCREEN_WIDTH);
    const auto full_height = restore_on_out_of_scope(FULL_SCREEN_HEIGHT);
    const auto posx = restore_on_out_of_scope(POSX);
    const auto posy = restore_on_out_of_scope(POSY);
    const auto terrain_width = restore_on_out_of_scope(TERRAIN_WINDOW_WIDTH);
    const auto terrain_height = restore_on_out_of_scope(TERRAIN_WINDOW_HEIGHT);
    const auto term_width = restore_on_out_of_scope(TERRAIN_WINDOW_TERM_WIDTH);
    const auto term_height = restore_on_out_of_scope(TERRAIN_WINDOW_TERM_HEIGHT);
    const auto terrain = restore_on_out_of_scope(g->w_terrain);
    const auto view = restore_on_out_of_scope(g->ter_view_p);
    TERMX = 100;
    TERMY = 40;
    FULL_SCREEN_WIDTH = 80;
    FULL_SCREEN_HEIGHT = 24;
    client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);

    auto& here = get_map();
    auto& you = get_avatar();
    for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            here.ter_set(tripoint_bub_ms(x, y, -1), ter_id("t_floor"));
            here.ter_set(tripoint_bub_ms(x, y, 0), ter_id("t_floor"));
        }
    }
    clear_fields(-1);
    clear_items(-1);
    you.Character::setpos(map_local_to_abs(here, tripoint_bub_ms(60, 60, -1)));
    you.recalc_sight_limits();
    you.add_bionic(bionic_id("bio_power_storage"));
    you.add_bionic(bionic_id("bio_flashlight"));
    you.get_bionic_state(bionic_id("bio_flashlight")).invlet = 'z';
    you.set_power_level(50_kJ);
    you.set_moves(1000);
    uistate.bionic_sort_mode = bionic_ui_sort_mode::NONE;
    game_session::set_running(true);
    calendar::turn = calendar::turn_zero + 12_hours;
    g->reset_light_level();

    // Initialize the real main UI once, then keep a small terrain viewport. Native
    // manager redraws remain enabled; every input boundary checks its dimensions.
    test_mode = false;
    const auto main_ui = g->create_or_get_main_ui_adaptor();
    ui_manager::redraw();
    g->w_terrain = catacurses::newwin(9, 9, point_zero);
    POSX = 4;
    POSY = 4;
    TERRAIN_WINDOW_WIDTH = TERRAIN_WINDOW_TERM_WIDTH = 9;
    TERRAIN_WINDOW_HEIGHT = TERRAIN_WINDOW_TERM_HEIGHT = 9;
    test();
}
} // namespace

TEST_CASE(
    "native bionics close commits newly illuminated offscreen knowledge before another draw",
    "[map_perception_bionics_boundary][bionics][mcp]") {
    with_dark_room([]() {
        auto& here = get_map();
        auto& you = get_avatar();
        const auto targets =
            std::array{you.bub_pos() + point_east * 6, you.bub_pos() + point_east * 8};
        const auto occluded = you.bub_pos() + point_north * 6;
        const auto distant = you.bub_pos() + point_west * 40;
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            here.ter_set(tripoint_bub_ms(x, you.bub_pos().y() - 3, -1), ter_id("t_wall"));
        }
        for (const auto& target : targets) { here.furn_set(target, furn_id("f_table")); }
        const auto occluded_abs = map_local_to_abs(here, occluded);
        const auto distant_abs = map_local_to_abs(here, distant);
        const auto clock = calendar::turn;
        const auto power = you.get_power_level();
        const auto darkness = you.active_light();
        REQUIRE_FALSE(you.get_bionic_state(bionic_id("bio_flashlight")).powered);
        REQUIRE(darkness == 0.0f);
        // A completed native no-op action warms acquisition on the P2 candidate,
        // and native visibility/draw caches on the baseline. Repeat with no mutations.
        dispatch_bionics(false);
        dispatch_bionics(false);
        REQUIRE_FALSE(here.visibility_caches_dirty());
        REQUIRE_FALSE(here.get_cache_ref(-1).lightmap_dirty);
        for (const auto& target : targets) {
            REQUIRE(target.x() - you.bub_pos().x() > getmaxx(g->w_terrain) - POSX - 1);
            REQUIRE_FALSE(you.sees(target));
            check_unknown(map_local_to_abs(here, target));
        }
        REQUIRE_FALSE(you.sees(occluded));
        REQUIRE_FALSE(you.sees(distant));
        check_unknown(occluded_abs);
        check_unknown(distant_abs);
        const auto hidden_before = remembered(occluded_abs);
        const auto distant_before = remembered(distant_abs);
        REQUIRE(calendar::turn == clock);
        REQUIRE(you.get_power_level() == power);

        dispatch_bionics(true);
        // Critical oracle: first stored-memory reads after handle_action returns.
        // No draw, visibility query, cache rebuild or test-side acquire intervenes.
        const auto at_close = std::array{
            remembered(map_local_to_abs(here, targets[0])),
            remembered(map_local_to_abs(here, targets[1]))};
        for (const auto index : std::views::iota(std::size_t{0}, targets.size())) {
            INFO("native light-on manager close; target " << targets[index]);
            CHECK(at_close[index].symbol == here.furn(targets[index])->symbol());
            CHECK(at_close[index].terrain.tile == "t_floor");
            CHECK(at_close[index].overlay.tile == "f_table");
        }
        check_same(remembered(occluded_abs), hidden_before);
        check_same(remembered(distant_abs), distant_before);
        REQUIRE(you.get_bionic_state(bionic_id("bio_flashlight")).powered);
        REQUIRE(you.get_power_level() == power - bionic_id("bio_flashlight")->power_activate);
        REQUIRE(you.active_light() > darkness);
        REQUIRE(calendar::turn == clock);

        // Only AFTER the critical oracle: independently prove the native illumination
        // and occlusion fixture, then draw as a positive legacy symbol-writing control.
        here.build_map_cache(-1);
        here.update_visibility_cache(-1);
        for (const auto& target : targets) { REQUIRE(you.sees(target)); }
        REQUIRE_FALSE(you.sees(occluded));
        REQUIRE_FALSE(you.sees(distant));
        g->draw_ter(false);
        const auto prior = std::array{
            remembered(map_local_to_abs(here, targets[0])),
            remembered(map_local_to_abs(here, targets[1]))};
        for (const auto& cell : prior) { REQUIRE(cell.symbol != 0); }
        const auto light_on_power = you.get_power_level();
        dispatch_bionics(true);
        const auto after_off = std::array{
            remembered(map_local_to_abs(here, targets[0])),
            remembered(map_local_to_abs(here, targets[1]))};
        REQUIRE_FALSE(you.get_bionic_state(bionic_id("bio_flashlight")).powered);
        REQUIRE(you.active_light() == darkness);
        REQUIRE(you.get_power_level()
                == light_on_power - bionic_id("bio_flashlight")->power_deactivate);
        REQUIRE(calendar::turn == clock);
        for (const auto index : std::views::iota(std::size_t{0}, targets.size())) {
            check_same(after_off[index], prior[index]);
        }
        check_same(remembered(occluded_abs), hidden_before);
        check_same(remembered(distant_abs), distant_before);
        here.build_map_cache(-1);
        here.update_visibility_cache(-1);
        for (const auto& target : targets) { REQUIRE_FALSE(you.sees(target)); }
    });
}


TEST_CASE(
    "same native bionics manager preserves intermediate flashlight knowledge",
    "[map_perception_bionics_boundary][bionics][mcp]") {
    with_dark_room([]() {
        auto& here = get_map();
        auto& you = get_avatar();
        const auto target = you.bub_pos() + point_east * 8;
        const auto occluded = you.bub_pos() + point_north * 6;
        const auto distant = you.bub_pos() + point_west * 40;
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            here.ter_set(tripoint_bub_ms(x, you.bub_pos().y() - 3, -1), ter_id("t_wall"));
        }
        here.furn_set(target, furn_id("f_table"));
        const auto target_abs = map_local_to_abs(here, target);
        const auto occluded_abs = map_local_to_abs(here, occluded);
        const auto distant_abs = map_local_to_abs(here, distant);
        const auto clock = calendar::turn;
        const auto power = you.get_power_level();
        dispatch_bionics(0);
        REQUIRE_FALSE(you.get_bionic_state(bionic_id("bio_flashlight")).powered);
        REQUIRE(you.active_light() == 0.0f);
        check_unknown(target_abs);
        check_unknown(occluded_abs);
        check_unknown(distant_abs);
        auto intermediate = remembered_cell{};
        dispatch_bionics(2, [&]() {
            // First stored-knowledge reads before returning OFF in the SAME manager.
            // No test-side acquisition, visibility query or cache rebuild intervenes.
            REQUIRE(you.get_bionic_state(bionic_id("bio_flashlight")).powered);
            intermediate = remembered(target_abs);
            REQUIRE(intermediate.terrain.tile == "t_floor");
            REQUIRE(intermediate.overlay.tile == "f_table");
            REQUIRE(intermediate.symbol == here.furn(target)->symbol());
            check_unknown(occluded_abs);
            check_unknown(distant_abs);
        });
        REQUIRE_FALSE(you.get_bionic_state(bionic_id("bio_flashlight")).powered);
        REQUIRE(you.active_light() == 0.0f);
        REQUIRE(calendar::turn == clock);
        REQUIRE(
            you.get_power_level()
            == power - bionic_id("bio_flashlight")->power_activate
                   - bionic_id("bio_flashlight")->power_deactivate);
        check_same(remembered(target_abs), intermediate);
        check_unknown(occluded_abs);
        check_unknown(distant_abs);
    });
}

#endif
