#pragma once

#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "dialogue.h"
#    include "dialogue_win.h"
#    include "faction.h"
#    include "game.h"
#    include "input.h"
#    include "item.h"
#    include "map_helpers.h"
#    include "npc.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "rng.h"
#    include "skill.h"
#    include "state_helpers.h"
#    include "uistate.h"

#    include <string>
#    include <unordered_map>

namespace dialogue_test {

inline const auto start_topic = std::string{"TALK_TEST_CLIENT_DIALOGUE_START"};
inline const auto first_topic = std::string{"TALK_TEST_CLIENT_DIALOGUE_FIRST"};
inline const auto second_topic = std::string{"TALK_TEST_CLIENT_DIALOGUE_SECOND"};
inline const auto branch_var = std::string{"npctalk_var_test_client_dialogue_branches"};
inline const auto trial_var = std::string{"npctalk_var_test_client_dialogue_trials"};
inline const auto removed_var = std::string{"npctalk_var_test_client_dialogue_removed"};
inline const auto speaker_var = std::string{"npctalk_var_test_client_dialogue_speaker"};

/// Own the real world fixture and the memory widget lifecycle, including exceptional exits.
struct dialogue_fixture {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;
    cata_default_random_engine old_rng = rng_get_engine();
    faction_manager old_factions = *g->faction_manager_ptr;
    uistatedata old_uistate = uistate;
    time_point old_time = calendar::turn;
    std::unordered_map<std::string, std::string> old_values = get_avatar().get_values_map();
    SkillLevel old_speech = get_avatar().get_skill_level_object(skill_id("speech"));
    int old_focus = get_avatar().focus_pool;
    override_option capital_yn{"FORCE_CAPITAL_YN", "false"};
    npc* talker = nullptr;
    dialogue d;
    dialogue_window window;

    dialogue_fixture() {
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        auto& you = get_avatar();
        const auto values = you.get_values_map();
        for (const auto& entry : values) { you.remove_value(entry.first); }
        you.setpos(map_local_to_abs(get_map(), tripoint_bub_ms{60, 60, 0}));
        you.cash = 100;
        // Use ordinary hands, not debug storage/traits, for the native consumption effect.
        auto currency = item::spawn("test_platinum_bit", calendar::turn, 7);
        REQUIRE(you.can_wield(*currency).success());
        REQUIRE_FALSE(you.wield(std::move(currency)));
        you.get_skill_level_object(skill_id("speech")) = SkillLevel{};
        you.focus_pool = 100;
        you.set_moves(1000);
        g->faction_manager_ptr->create_if_needed();
        talker = &spawn_npc(you.bub_pos() + tripoint_east, "test_talker");
        talker->name = "Adapter talker";
        talker->op_of_u = npc_opinion{};
        talker->set_attitude(NPCATT_NULL);
        talker->set_moves(100);
        talker->chatbin.first_topic = start_topic;
        d.alpha = &you;
        d.beta = talker;
        d.add_topic(start_topic);
        TERMX = 80;
        TERMY = 24;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        test_mode = false;
    }

    ~dialogue_fixture() {
        game_client::memory::set_input_provider({});
        test_mode = true;
        clear_all_state();
        *g->faction_manager_ptr = old_factions;
        uistate = old_uistate;
        calendar::turn = old_time;
        auto& you = get_avatar();
        const auto values = you.get_values_map();
        for (const auto& entry : values) { you.remove_value(entry.first); }
        for (const auto& entry : old_values) { you.set_value(entry.first, entry.second); }
        you.get_skill_level_object(skill_id("speech")) = old_speech;
        you.focus_pool = old_focus;
        rng_get_engine() = old_rng;
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_width;
        FULL_SCREEN_HEIGHT = old_height;
        test_mode = old_test_mode;
    }

    /// Restore only fixture inputs actually changed by these responses; do not load/save
    /// through unordered containers or normalize serialized actor state for equality.
    auto reset_effect_state() -> void {
        auto& you = get_avatar();
        you.cash = 100;
        you.primary_weapon().charges = 7;
        you.get_skill_level_object(skill_id("speech")) = SkillLevel{};
        you.focus_pool = 100;
        you.set_moves(1000);
        talker->set_moves(100);
        talker->op_of_u = npc_opinion{};
        talker->remove_value(branch_var);
        talker->remove_value(trial_var);
        talker->remove_value(speaker_var);
    }

    auto opt(const std::string& topic = start_topic) -> talk_topic {
        return d.opt(window, talker->name, talk_topic(topic));
    }
};

/// The key a client sends for a registered action.
inline auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// A key the player types.
inline auto key(const int code) -> input_event {
    return input_event(code, input_event_t::keyboard);
}

} // namespace dialogue_test

#endif // CATA_MCP
