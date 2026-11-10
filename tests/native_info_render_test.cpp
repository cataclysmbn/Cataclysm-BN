#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "effect.h"
#    include "game.h"
#    include "item.h"
#    include "item_factory.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "monster.h"
#    include "npc.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <format>
#    include <string>
#    include <vector>

/// Pins what the native look/target info panels print, so that sharing their fact gathering with
/// the engine client keeps the player's panels byte-equal: every line with its text and colour
/// runs.
namespace {

struct panel {
    catacurses::window window = catacurses::newwin(50, 70, point_zero);
};

auto trimmed(std::string text) -> std::string {
    while (!text.empty() && text.back() == ' ') { text.pop_back(); }
    return text;
}

/// Each non-blank row as `<fg>text` runs, split where the foreground colour changes.
auto rendered(const catacurses::window& window) -> std::vector<std::string> {
    const auto* const native = window.get<cata_cursesport::WINDOW>();
    auto rows = std::vector<std::string>{};
    for (const auto& row : native->line) {
        auto line = std::string{};
        auto fg = -1;
        for (const auto& cell : row.chars) {
            if (cell.ch == " ") {
                line += ' ';
                continue;
            }
            if (static_cast<int>(cell.FG) != fg) {
                fg = static_cast<int>(cell.FG);
                line += std::format("<{}>", fg);
            }
            line += cell.ch;
        }
        if (!trimmed(line).empty()) { rows.push_back(trimmed(line)); }
    }
    return rows;
}

struct fixed_square {
    tripoint_bub_ms at = {60, 60, 0};
    explicit fixed_square() {
        clear_all_state();
        build_test_map(ter_id("t_floor"));
        g->place_player(at);
        get_map().invalidate_map_cache(0);
        get_map().build_map_cache(0, true);
        get_map().update_visibility_cache(0);
    }
};

auto print_tile(const tripoint_bub_ms& at, int last_line) -> std::vector<std::string> {
    const auto surface = panel{};
    auto line = 1;
    g->print_all_tile_info(
        at, surface.window, "", 1, line, last_line, get_map().get_visibility_variables_cache());
    return rendered(surface.window);
}

} // namespace

TEST_CASE("the native look panel prints a monster as it always did", "[native_info]") {
    const auto square = fixed_square{};
    auto& zombie = spawn_test_monster("mon_zombie", square.at + tripoint_east);
    zombie.add_effect(efftype_id("poison"), 10_turns);
    get_map().update_visibility_cache(0);
    const auto surface = panel{};
    const auto end = zombie.print_info(surface.window, 1, 20, 1);
    CHECK(end > 1);
    const auto expected = std::vector<std::string>{
        " <2>█████ <10>zombie <15>Poisoned                                       <1>Hostile!",
        " <1>Can see to your current location",
        " <6>It looks slower than you.",
        " <1>It can act right now.",
        " <6>Origin: 'Bright Nights'",
        " <7>A human body, swaying as it moves, an unstoppable rage visible in",
        " <7>its oily black eyes.",
    };
    CHECK(rendered(surface.window) == expected);
}

TEST_CASE("the native look panel prints an npc as it always did", "[native_info]") {
    const auto square = fixed_square{};
    auto& talker = spawn_npc(square.at + tripoint_east, "test_talker");
    talker.name = "Test Npc";
    // The npc class rolls its traits at random; a visible one would add a line.
    talker.clear_mutations();
    talker.remove_worn_items_with([](auto&&) { return detached_ptr<item>(); });
    talker.wield(item::spawn("knife_combat"));
    talker.wear_item(item::spawn("jeans"), false);
    talker.wear_item(item::spawn("tshirt"), false);
    talker.add_effect(efftype_id("poison"), 10_turns);
    get_map().update_visibility_cache(0);
    const auto surface = panel{};
    const auto end = talker.print_info(surface.window, 1, 30, 1);
    CHECK(end > 1);
    const auto expected = std::vector<std::string>{
        " <2>█████ <13>Test Npc                                         Wants to talk",
        " <11>Aware of your presence                                       <15>Neutral",
        " <7>Wielding: <1>combat knife",
        " <7>Wearing: <10>|| <7>jeans (poor fit) and <10>|| <7>t-shirt (poor fit)",
    };
    CHECK(rendered(surface.window) == expected);
}

TEST_CASE("the native look panel prints a crowded tile as it always did", "[native_info]") {
    const auto square = fixed_square{};
    const auto at = square.at + tripoint_east;
    auto& here = get_map();
    here.furn_set(at, furn_id("f_table"));
    here.add_field(at, field_type_id("fd_blood"), 1);
    for (const auto* type :
         {"test_1kg_cube", "stick", "2x4", "pebble", "rag", "nail", "hammer", "lighter",
          "bottle_plastic", "string_6", "knife_combat", "hat_knit", "jeans", "tshirt"}) {
        REQUIRE(itype_id(type).is_valid());
        here.add_item_or_charges(at, item::spawn(type));
    }
    here.update_visibility_cache(0);
    const auto tall = std::vector<std::string>{
        "                                                      <7>Move cost: 200",
        " <6>wooden floor                                              <0>very dark",
        " <7>Interlocking wooden tiles that are more than likely treated against",
        " <7>fire, with wooden posts and beams supporting a roof.",
        " <1>table",
        " <7>Sit down when you eat!  Examine to flip it on its side if you're",
        " <7>feeling angry, or want a barricade in a hurry.",
        " <8>Concealment: 40%",
        " <8>Cover: 25%",
        " <8>Smashable. Flat. Mountable. Supports a roof. Prone to collapse.",
        " <8>Short.",
        " <1>blood splatter",
        " <10>|| <7>jeans (poor fit)",
        " <10>|| <7>knit hat (poor fit)",
        " <10>|| <7>t-shirt (poor fit)",
        " <7>TEST 1kg cube",
        " <7>combat knife",
        " <7>hammer",
        " <7>heavy stick",
        " <7>lighter",
        " <7>nail",
        " <7>pebble",
        " <7>plank",
        " <7>plastic bottle",
        " <7>rag",
        " <7>short string",
    };
    CHECK(print_tile(at, 48) == tall);
    const auto short_window = std::vector<std::string>{
        "                                                      <7>Move cost: 200",
        " <6>wooden floor                                              <0>very dark",
        " <7>Interlocking wooden tiles that are more than likely treated against",
        " <7>fire, with wooden posts and beams supporting a roof.",
        " <1>table",
        " <7>Sit down when you eat!  Examine to flip it on its side if you're",
        " <7>feeling angry, or want a barricade in a hurry.",
        " <8>Concealment: 40%",
        " <8>Cover: 25%",
        " <8>Smashable. Flat. Mountable. Supports a roof. Prone to collapse.",
        " <8>Short.",
        " <1>blood splatter",
        " <11>More items here…",
    };
    CHECK(print_tile(at, 14) == short_window);
}

#endif
