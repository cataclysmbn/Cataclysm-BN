#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "character_martial_arts.h"
#    include "client_screen_support.h"
#    include "debug_menu.h"
#    include "game.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "martialarts.h"
#    include "monster.h"
#    include "monstergenerator.h"
#    include "mtype.h"
#    include "mutation.h"
#    include "player_helpers.h"
#    include "reload/reload_ui.h"
#    include "state_helpers.h"
#    include "ui.h"

#    include <string>

namespace {

using namespace client_screen_test;

} // namespace

TEST_CASE(
    "the technique reference opens from the technique menu through its F1 key affordance",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.martial_arts_data->add_martialart(matype_id("style_brawling"));
    you.martial_arts_data->set_style(matype_id("style_brawling"));
    auto& target = spawn_test_monster("mon_zombie", you.bub_pos() + tripoint_east);
    g->manual_combat_mode = true;
    const auto guard = screen_guard{};
    auto stage = 0;
    auto menus = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        if (snapshot.context == "YESNO") {
            const auto yes = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::action_id);
            return choose(snapshot, yes->id);
        }
        if (snapshot.context == "MARTIAL_ARTS") {
            ++stage;
            CHECK(snapshot.title == "Technique Reference");
            CHECK(snapshot.message.find("Active martial art") != std::string::npos);
            return cancel(snapshot);
        }
        // The technique menu itself.
        ++menus;
        if (stage == 0) {
            CHECK(has_key(snapshot, KEY_F(1)));
            return press(KEY_F(1));
        }
        return cancel(snapshot);
    });
    for (auto attempts = 0; attempts < 60 && stage == 0; ++attempts) {
        you.melee_attack(target, true);
        target.set_hp(target.get_hp_max());
    }
    CHECK(stage == 1);
    CHECK(menus >= 1);
    g->manual_combat_mode = false;
}

TEST_CASE(
    "the reload picker lowers the amount to load through its left key affordance",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    auto& magazine = you.i_add(item::spawn("glockmag", calendar::start_of_cataclysm, 0));
    you.i_add(item::spawn("9mm", calendar::start_of_cataclysm, 10));
    get_map().add_item_or_charges(
        you.bub_pos() + tripoint_east, item::spawn("9mm", calendar::start_of_cataclysm, 5));
    const auto guard = screen_guard{};
    auto stage = 0;
    auto before = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        switch (stage++) {
            case 0: {
                CHECK(has_key(snapshot, KEY_LEFT));
                CHECK(has_key(snapshot, KEY_RIGHT));
                before = row(snapshot, "entry:0")->label;
                return press(KEY_LEFT);
            }
            default:
                CHECK(row(snapshot, "entry:0")->label != before);
                return choose(snapshot, "entry:0");
        }
    });
    const auto selected = reload_ui::select_ammo(you, magazine);
    REQUIRE(selected.ammo != nullptr);
    CHECK(stage == 2);
    CHECK(selected.qty() < selected.ammo->charges);
}

#endif // CATA_MCP
