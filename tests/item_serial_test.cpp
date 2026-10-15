#if defined(CATA_MCP)

#    include "avatar.h"
#    include "cata_arena.h"
#    include "catch/catch.hpp"
#    include "item.h"
#    include "item_serial.h"
#    include "json.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "replay/state_hash.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <set>
#    include <span>
#    include <sstream>
#    include <string>
#    include <vector>

namespace {

constexpr auto spot = tripoint_bub_ms(62, 62, 0);

auto fresh_game() -> void {
    clear_all_state();
    game_client::reset_item_serials();
    build_test_map(ter_id("t_floor"));
    get_avatar().setpos(spot);
}

auto serialized(const item& thing) -> std::string {
    auto out = std::ostringstream{};
    auto json = JsonOut(out);
    thing.serialize(json);
    return out.str();
}

auto digests(const replay::state_hash& hash) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};
    for (const auto& part : hash.parts) { result.push_back(part.name + part.digest); }
    return result;
}

auto on_ground() -> std::vector<item*> {
    auto result = std::vector<item*>{};
    for (auto* const thing : get_map().i_at(spot)) { result.push_back(thing); }
    return result;
}

} // namespace

TEST_CASE("equal items get different row ids that follow each item", "[item_serial]") {
    fresh_game();
    get_map().add_item(spot, item::spawn("rock"));
    get_map().add_item(spot, item::spawn("rock"));
    const auto items = on_ground();
    REQUIRE(items.size() == 2);
    const auto first = game_client::item_row_id(*items[0]);
    const auto second = game_client::item_row_id(*items[1]);
    CHECK(first != second);
    CHECK(first.starts_with("item:"));
    // Asking again, in any order, names the same items the same.
    CHECK(game_client::item_row_id(*items[1]) == second);
    CHECK(game_client::item_row_id(*items[0]) == first);
}

TEST_CASE(
    "a row id survives the item moving between the ground and the inventory", "[item_serial]") {
    fresh_game();
    get_map().add_item(spot, item::spawn("rock"));
    auto* const rock = on_ground().front();
    const auto id = game_client::item_row_id(*rock);

    auto& carried = get_avatar().i_add(get_map().i_rem(spot, rock));
    CHECK(game_client::item_row_id(carried) == id);
    get_map().add_item(spot, get_avatar().inv_remove_item(&carried));
    CHECK(game_client::item_row_id(*on_ground().front()) == id);
}

TEST_CASE(
    "a row id stays with its item when another item of its type is removed", "[item_serial]") {
    fresh_game();
    for (const auto* const type : {"rock", "rock", "rock"}) {
        get_map().add_item(spot, item::spawn(type));
    }
    const auto items = on_ground();
    REQUIRE(items.size() == 3);
    const auto kept = game_client::item_row_id(*items[2]);
    const auto removed = game_client::item_row_id(*items[0]);
    get_map().i_rem(spot, items[0]);
    cleanup_arenas();
    const auto left = on_ground();
    REQUIRE(left.size() == 2);
    CHECK(game_client::item_row_id(*left.back()) == kept);
    CHECK(game_client::item_row_id(*left.front()) != removed);
}

TEST_CASE("a destroyed item's serial is dropped and never given again", "[item_serial]") {
    fresh_game();
    get_map().add_item(spot, item::spawn("rock"));
    const auto before = game_client::item_serial_count();
    const auto gone = game_client::item_row_id(*on_ground().front());
    CHECK(game_client::item_serial_count() == before + 1);
    get_map().i_clear(spot);
    cleanup_arenas();
    CHECK(game_client::item_serial_count() == before);

    auto seen = std::set<std::string>{gone};
    for (auto round = 0; round < 20; ++round) {
        get_map().add_item(spot, item::spawn("rock"));
        CHECK(seen.insert(game_client::item_row_id(*on_ground().front())).second);
        get_map().i_clear(spot);
        cleanup_arenas();
    }
}

TEST_CASE("serials are numbered again from the start after a load", "[item_serial]") {
    fresh_game();
    get_map().add_item(spot, item::spawn("rock"));
    CHECK(game_client::item_row_id(*on_ground().front()) == "item:1");
    get_map().add_item(spot, item::spawn("rock"));
    CHECK(game_client::item_row_id(*on_ground().back()) == "item:2");
    game_client::reset_item_serials();
    CHECK(game_client::item_serial_count() == 0);
    CHECK(game_client::item_row_id(*on_ground().back()) == "item:1");
}

TEST_CASE(
    "a stack is named by its lowest member and keeps the name when another member goes",
    "[item_serial]") {
    fresh_game();
    for (const auto* const type : {"rock", "rock", "rock"}) {
        get_map().add_item(spot, item::spawn(type));
    }
    const auto items = on_ground();
    const auto members = std::vector<const item*>(items.begin(), items.end());
    const auto id = game_client::stack_row_id(members);
    CHECK(id.starts_with("stack:"));
    const auto reversed = std::vector<const item*>(members.rbegin(), members.rend());
    CHECK(game_client::stack_row_id(reversed) == id);
    // A member that is not the lowest leaves: the stack keeps its name.
    const auto without_highest = std::vector<const item*>{items[0], items[1]};
    CHECK(game_client::stack_row_id(without_highest) == id);
    // The lowest member leaves: the rest are another group. One item alone is named as an item.
    const auto without_lowest = std::vector<const item*>{items[1], items[2]};
    CHECK(game_client::stack_row_id(without_lowest) != id);
    CHECK(game_client::stack_row_id(std::span(members).first(1))
          == game_client::item_row_id(*items[0]));
}

TEST_CASE(
    "publishing row ids changes neither the state hash nor the saved item",
    "[item_serial][state_hash]") {
    fresh_game();
    get_map().add_item(spot, item::spawn("rock"));
    get_avatar().i_add(item::spawn("rock"));
    const auto ground = on_ground().front();
    auto carried = std::vector<const item*>{};
    for (const auto* const thing : get_avatar().inv_dump()) { carried.push_back(thing); }
    REQUIRE_FALSE(carried.empty());
    const auto hash = digests(replay::hash_game_state());
    const auto saved = serialized(*ground);
    const auto saved_carried = serialized(*carried.front());

    static_cast<void>(game_client::item_row_id(*ground));
    static_cast<void>(game_client::stack_row_id(carried));

    CHECK(digests(replay::hash_game_state()) == hash);
    CHECK(serialized(*ground) == saved);
    CHECK(serialized(*carried.front()) == saved_carried);
    CHECK(saved.find("\"id\"") == std::string::npos);
}

#endif
