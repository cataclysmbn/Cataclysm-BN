#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_interaction_data.h"
#    include "diary.h"
#    include "item.h"
#    include "item_serial.h"
#    include "json.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "page_serial.h"
#    include "path_info.h"
#    include "replay/replay.h"
#    include "replay/row_locator.h"
#    include "state_helpers.h"

#    include <filesystem>
#    include <fstream>
#    include <iterator>
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

auto on_ground() -> std::vector<item*> {
    auto result = std::vector<item*>{};
    for (auto* const thing : get_map().i_at(spot)) { result.push_back(thing); }
    return result;
}

auto choosing(const std::string& row) -> game_client::interaction_event {
    return {.operation = game_client::interaction_operation::invoke,
            .target_id = "pickup:select",
            .arguments = game_client::interaction_arguments{.row = row}};
}

/// What a session that published other things first, or fewer, makes of a recorded input.
auto played(const game_client::interaction_event& recorded) -> game_client::interaction_event {
    game_client::reset_item_serials();
    return replay::resolve_row_ids(recorded);
}

} // namespace

TEST_CASE(
    "a recorded item row names the item, not the serial of the session that recorded it",
    "[replay][row_locator]") {
    fresh_game();
    for (auto i = 0; i < 3; ++i) { get_map().add_item(spot, item::spawn("rock")); }
    get_map().add_item(spot, item::spawn("stick"));
    const auto items = on_ground();
    REQUIRE(items.size() == 4);
    // The recording session published the stick and a rock before the one the player chose.
    game_client::item_row_id(*items[3]);
    game_client::item_row_id(*items[0]);
    const auto chosen_id = game_client::item_row_id(*items[2]);

    const auto recorded = replay::locate_row_ids(choosing(chosen_id));
    CHECK(*recorded.arguments->row != chosen_id);
    CHECK(recorded.arguments->row->starts_with("item:@t"));

    // The playing session publishes nothing before the input.
    const auto resolved = played(recorded);
    CHECK(*resolved.arguments->row == game_client::item_row_id(*items[2]));
    CHECK(game_client::item_serial_count() == 1);
}

TEST_CASE(
    "a recorded stack row names every member and resolves to the stack of the playing session",
    "[replay][row_locator]") {
    fresh_game();
    for (auto i = 0; i < 3; ++i) { get_map().add_item(spot, item::spawn("rock")); }
    const auto items = on_ground();
    REQUIRE(items.size() == 3);
    const auto members = std::vector<const item*>{items[2], items[1], items[0]};
    const auto stack_id = game_client::stack_row_id(members);
    REQUIRE(stack_id.starts_with("stack:"));

    const auto recorded = replay::locate_row_ids(choosing(stack_id));
    CHECK(recorded.arguments->row->starts_with("stack:@t"));
    const auto resolved = played(recorded);
    // Played in the opposite order, the serials differ, and so does the lowest one.
    const auto again = std::vector<const item*>{items[0], items[1], items[2]};
    CHECK(*resolved.arguments->row == game_client::stack_row_id(again));
}

TEST_CASE(
    "a recorded row finds an item in a container, in the inventory and on a neighbour",
    "[replay][row_locator]") {
    fresh_game();
    auto box = item::spawn("box_small");
    box->put_in(item::spawn("rock"));
    get_map().add_item(spot, std::move(box));
    auto& carried = get_avatar().i_add(item::spawn("stick"));
    const auto inside = on_ground().front()->contents.all_items_top().front();
    const auto in_box = replay::locate_row_ids(choosing(game_client::item_row_id(*inside)));
    const auto in_hand = replay::locate_row_ids(choosing(game_client::item_row_id(carried)));
    CHECK(in_box.arguments->row->starts_with("item:@t"));
    CHECK(in_hand.arguments->row->starts_with("item:@u"));

    CHECK(*played(in_box).arguments->row == game_client::item_row_id(*inside));
    CHECK(*played(in_hand).arguments->row == game_client::item_row_id(carried));
}

TEST_CASE("a recorded row of an item that is gone is left as it is", "[replay][row_locator]") {
    fresh_game();
    CHECK(*replay::locate_row_ids(choosing("item:40")).arguments->row == "item:40");
    CHECK(*replay::resolve_row_ids(choosing("item:@t1,2,3:9")).arguments->row == "item:@t1,2,3:9");
    CHECK(*replay::resolve_row_ids(choosing("item:@x")).arguments->row == "item:@x");
}

TEST_CASE("typed text that looks like a row id is recorded untouched", "[replay][row_locator]") {
    fresh_game();
    get_map().add_item(spot, item::spawn("rock"));
    const auto id = game_client::item_row_id(*on_ground().front());
    auto typed = game_client::interaction_event{
        .operation = game_client::interaction_operation::fill,
        .schema_id = "search",
        .target_id = "search:field",
        .value = id};
    CHECK(replay::locate_row_ids(typed).value == id);
}

TEST_CASE(
    "a recorded diary page row names the page by its place in the diary", "[replay][row_locator]") {
    fresh_game();
    auto text = std::istringstream(
        R"({"owner":"x","pages":[{"text":"a"},{"text":"b"},{"text":"c"}]})");
    auto in = JsonIn(text);
    auto* const book = get_avatar().get_avatar_diary();
    book->deserialize(in);
    REQUIRE(book->page_count() == 3);
    game_client::diary_page_row_id(*book->page_at(0));
    const auto chosen_id = game_client::diary_page_row_id(*book->page_at(2));

    const auto recorded = replay::locate_row_ids(choosing(chosen_id));
    CHECK(*recorded.arguments->row == "page:@d2");
    const auto resolved = played(recorded);
    CHECK(*resolved.arguments->row == game_client::diary_page_row_id(*book->page_at(2)));
}

TEST_CASE(
    "a recording names an item by where it is and plays back without the publications",
    "[replay][row_locator]") {
    fresh_game();
    const auto path = std::filesystem::path(PATH_INFO::user_dir()) / "row-locator-replay.jsonl";
    replay::configure_recording(path.string(), {.rng_seed = 7});
    replay::start();
    for (auto i = 0; i < 3; ++i) { get_map().add_item(spot, item::spawn("rock")); }
    const auto items = on_ground();
    REQUIRE(items.size() == 3);
    // The recorded session published two other items first (a paging client, a map screen).
    game_client::item_row_id(*items[2]);
    game_client::item_row_id(*items[1]);
    const auto chosen = game_client::item_row_id(*items[0]);
    auto input = input_event{};
    input.type = input_event_t::interaction;
    input.interaction = choosing(chosen);
    replay::record_input_event(input);
    replay::finish();

    auto recording = std::ifstream(path, std::ios::binary);
    const auto text = std::string(std::istreambuf_iterator<char>(recording), {});
    CHECK(text.find("item:@t") != std::string::npos);
    CHECK(text.find("\"" + chosen + "\"") == std::string::npos);

    replay::configure_playback(path.string(), {.rng_seed = 7});
    replay::start();
    const auto played_back = replay::next_input_event();
    REQUIRE(played_back);
    CHECK(*played_back->interaction->arguments->row == game_client::item_row_id(*items[0]));
    CHECK(game_client::item_serial_count() == 1);
    replay::stop();
    std::filesystem::remove(path);
}

#endif
