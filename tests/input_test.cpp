#include "catch/catch.hpp"
#include "filesystem.h"
#include "input.h"
#include "path_info.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

class user_keybindings_file_guard {
public:
    ~user_keybindings_file_guard() {
        remove_file(PATH_INFO::user_keybindings());
        inp_mngr.init();
    }
};

auto write_user_keybindings(const std::string& entry) -> void {
    auto file = std::ofstream(PATH_INFO::user_keybindings(), std::ios::binary);
    REQUIRE(file.good());
    file << "[\n" << entry << "\n]\n";
}

auto read_user_keybindings() -> std::string {
    auto file = std::ifstream(PATH_INFO::user_keybindings(), std::ios::binary);
    REQUIRE(file.good());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto npc_trade_right_keys() -> std::vector<char> {
    auto ctxt = input_context("NPC_TRADE");
    return ctxt.keys_bound_to("RIGHT");
}

auto contains_key(const std::vector<char>& keys, const char key) -> bool {
    return std::ranges::find(keys, key) != keys.end();
}

} // namespace

TEST_CASE("deleted local keybindings fall back to global bindings", "[input][keybindings]") {
    auto restore_keybindings = user_keybindings_file_guard();

    write_user_keybindings(R"({
  "type": "keybinding",
  "id": "RIGHT",
  "category": "NPC_TRADE",
  "bindings": []
})");
    inp_mngr.init();

    auto keys = npc_trade_right_keys();
    CHECK(keys.empty());

    write_user_keybindings(R"({
  "type": "keybinding",
  "id": "RIGHT",
  "category": "NPC_TRADE",
  "is_deleted": true,
  "bindings": []
})");
    inp_mngr.init();

    keys = npc_trade_right_keys();
    CHECK(contains_key(keys, 'l'));
    CHECK(contains_key(keys, '6'));

    inp_mngr.save();
    CHECK(read_user_keybindings().find("\"is_deleted\": true") != std::string::npos);
}

TEST_CASE("manual combat default bindings reserve backtab for the toggle", "[input][keybindings]") {
    const auto autoattack_key = inp_mngr.get_first_char_for_action("autoattack", "DEFAULTMODE");
    const auto toggle_key = inp_mngr.get_first_char_for_action(
        "toggle_manual_combat_mode",
        "DEFAUL"
        "TMOD"
        "E");
    const auto removed_manual_attack_key = inp_mngr.get_first_char_for_action(
        "manual_attack",
        "DEF"
        "AUL"
        "TMO"
        "D"
        "E");

    CHECK(autoattack_key == inp_mngr.get_keycode("TAB"));
    CHECK(toggle_key == inp_mngr.get_keycode("BACKTAB"));
    CHECK(removed_manual_attack_key == 0);
}
