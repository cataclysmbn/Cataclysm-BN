#include "avatar.h"
#include "book_scanning.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "digital_media.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "json.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "recipe.h"
#include "skill.h"
#include "state_helpers.h"

#include <sstream>

namespace {
auto finish_scan(avatar& who) -> void {
    for (auto turn = 0; turn < 300 && !who.activity->is_null(); ++turn) {
        calendar::turn += 1_turns;
        who.moves = 100;
        who.activity->do_turn(who);
    }
    REQUIRE(who.activity->is_null());
}
} // namespace

TEST_CASE(
    "scanned_books_retain_their_contents_as_transferable_digital_files",
    "[digital_media][book_scanning]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    set_time(calendar::turn_zero + 12_hours);
    auto& who = get_avatar();
    clear_character(who);
    auto& reader = who.i_add(item::spawn("wrist_comp_budget"));
    reader.ammo_set(itype_id("battery"), 10);
    auto& book = who.i_add(item::spawn("test_scannable_book"));
    REQUIRE_FALSE(is_digital_file(book));
    REQUIRE(start_book_scan(who, reader, book));
    CHECK(reader.contents.empty());
    finish_scan(who);
    REQUIRE(reader.contents.num_item_stacks() == 1);
    auto& file = reader.contents.front();
    CHECK(is_digital_file(file));
    CHECK(file.typeId() == book.typeId());
    CHECK(file.type_name() != book.type_name());
    CHECK(file.weight() == 0_gram);
    CHECK(file.volume() == 0_ml);
    CHECK(file.type->book->skill == book.type->book->skill);
    CHECK(reader.ammo_remaining() < 10);
    CHECK(who.has_item(book));
    CHECK_FALSE(is_digital_file(book));
    CHECK(book.volume() == 100_ml);
    CHECK_FALSE(start_book_scan(who, reader, book));
    CHECK(reader.can_contain(file));
    CHECK_FALSE(reader.can_contain(book));
    CHECK_FALSE(item::spawn("bottle_plastic")->can_contain(file));
    CHECK(digital_reader_for(who, file));

    who.set_skill_level(skill_id("electronics"), 1);
    const auto& recipe = recipe_id("test_soldering_iron_digital_media").obj();
    auto inv = inventory{};
    inv.add_item(reader, false);
    CHECK(who.get_recipes_from_books(inv).contains(recipe));
    REQUIRE(who.read(&file, true));
    REQUIRE_FALSE(who.activity->is_null());
    who.cancel_activity();

    auto card = item::spawn("sd_memory_card");
    REQUIRE(
        upload_digital_files(who, {.source = reader, .destination = *card, .file = &file}) == 1);
    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    card->serialize(writer);
    auto input = std::istringstream(output.str());
    auto parser = JsonIn(input);
    const auto loaded = item::spawn(parser);
    auto& other_reader = who.i_add(item::spawn("test_digital_music_reader"));
    REQUIRE(download_digital_files(other_reader, *loaded, who) == 1);
    const auto& imported = other_reader.contents.front();
    CHECK(imported.typeId() == book.typeId());
    CHECK(imported.weight() == 0_gram);
    CHECK(imported.volume() == 0_ml);
    CHECK(imported.get_var("DIGITAL_FILE_ID") == file.get_var("DIGITAL_FILE_ID"));
    CHECK(digital_reader_for(who, imported));
}

TEST_CASE("book_scanning_stops_when_its_requirements_are_lost", "[digital_media][book_scanning]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    set_time(calendar::turn_zero + 12_hours);
    auto& who = get_avatar();
    clear_character(who);
    auto& reader = who.i_add(item::spawn("wrist_comp_budget"));
    reader.ammo_set(itype_id("battery"), 10);
    auto& book = who.i_add(item::spawn("test_scannable_book"));
    REQUIRE(start_book_scan(who, reader, book));

    auto removed = detached_ptr<item>{};
    SECTION("power is exhausted") {
        reader.ammo_unset();
        finish_scan(who);
    }
    SECTION("the reader is removed") {
        removed = reader.detach();
        finish_scan(who);
    }
    SECTION("the book is removed") {
        removed = book.detach();
        finish_scan(who);
    }
    SECTION("the player cancels") { who.cancel_activity(); }
    CHECK(reader.contents.empty());
    CHECK_FALSE(is_digital_file(book));
}
