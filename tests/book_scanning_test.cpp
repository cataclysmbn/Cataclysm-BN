#include "avatar.h"
#include "book_scanning.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "character_functions.h"
#include "digital_media.h"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "json.h"
#include "map/map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "skill.h"
#include "state_helpers.h"
#include "text_snippets.h"

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

TEST_CASE("scanning_preserves_distinct_snippets", "[digital_media][book_scanning]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    set_time(calendar::turn_zero + 12_hours);
    auto& who = get_avatar();
    clear_character(who);
    auto& reader = who.i_add(item::spawn("wrist_comp_budget"));
    reader.ammo_set(itype_id("battery"), 10);
    auto& text = who.i_add(item::spawn("test_scannable_snippet"));
    REQUIRE_FALSE(text.is_book());
    REQUIRE(is_scannable_text(text));
    auto identities = std::vector<std::string>{};
    for (const auto& id :
         {snippet_id("test_scannable_snippet_first"),
          snippet_id("test_scannable_snippet_second")}) {
        text.set_snippet(id);
        REQUIRE(start_book_scan(who, reader, text));
        finish_scan(who);
        CHECK_FALSE(is_digital_file(text));
        CHECK(text.snip_id == id);
        auto& file = reader.contents.back();
        CHECK(file.snip_id == id);
        CHECK(file.info_string().find(SNIPPET.get_snippet_ref_by_id(id).translated())
              != std::string::npos);
        CHECK(file.type_name().find("digital copy") != std::string::npos);
        CHECK(file.volume() == 1_ml);
        CHECK(file.weight() == 0_gram);
        CHECK(digital_reader_for(who, file));
        CHECK_FALSE(is_scannable_text(file));
        CHECK_FALSE(start_book_scan(who, reader, text));
        identities.push_back(file.get_var("DIGITAL_FILE_ID"));
    }
    REQUIRE(identities.size() == 2);
    CHECK(identities.front() != identities.back());
    CHECK(reader.contents.num_item_stacks() == 2);

    auto& card = who.i_add(item::spawn("sd_memory_card"));
    REQUIRE(upload_digital_files(who, {.source = reader, .destination = card}) == 2);
    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    card.serialize(writer);
    auto input = std::istringstream(output.str());
    auto parser = JsonIn(input);
    const auto loaded = item::spawn(parser);
    auto& other_reader = who.i_add(item::spawn("wrist_comp_budget"));
    other_reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(other_reader, *loaded, who) == 2);
    CHECK(other_reader.contents.front().snip_id == reader.contents.front().snip_id);
    CHECK(other_reader.contents.back().snip_id == reader.contents.back().snip_id);
    CHECK(download_digital_files(other_reader, *loaded, who) == 0);

    auto& rag = who.i_add(item::spawn("rag"));
    CHECK_FALSE(is_scannable_text(rag));
    CHECK_FALSE(start_book_scan(who, reader, rag));
}

TEST_CASE("scanned_books_can_be_read_in_darkness", "[digital_media][book_scanning][reading]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    set_time(calendar::turn_zero + 12_hours);
    auto& who = get_avatar();
    clear_character(who);
    auto& reader = who.i_add(item::spawn("wrist_comp_budget"));
    reader.ammo_set(itype_id("battery"), 10);
    auto& book = who.i_add(item::spawn("test_scannable_book"));
    REQUIRE(start_book_scan(who, reader, book));
    finish_scan(who);
    REQUIRE_FALSE(reader.contents.empty());
    auto& file = reader.contents.front();
    REQUIRE(is_digital_file(file));
    who.do_read(&file);
    REQUIRE(who.has_identified(file.typeId()));

    set_time(calendar::turn_zero);
    constexpr auto pos = tripoint_bub_ms(60, 60, 0);
    g->place_player(pos);
    auto& here = get_map();
    here.ter_set(pos, ter_id("t_floor"));
    here.furn_set(pos, furn_id("f_null"));
    here.ter_set(pos + tripoint_above, ter_id("t_flat_roof"));
    here.invalidate_map_cache(pos.z());
    here.build_map_cache(pos.z());
    here.update_visibility_cache(pos.z());
    REQUIRE_FALSE(character_funcs::can_see_fine_details(who));

    auto reasons = std::vector<std::string>{};
    SECTION("a powered screen allows normal speed reading and learning") {
        CHECK(who.get_book_reader(book, reasons) == nullptr);
        reasons.clear();
        CHECK(who.get_book_reader(file, reasons) == &who);
        CHECK(reasons.empty());
        const auto dark_reading_time = who.time_to_read(file, who);
        CHECK(who.time_to_read(book, who) > dark_reading_time);
        auto& lamp = who.i_add(item::spawn("atomic_lamp"));
        REQUIRE(character_funcs::can_see_fine_details(who));
        CHECK(who.time_to_read(file, who) == dark_reading_time);
        const auto removed_lamp = lamp.detach();
        REQUIRE_FALSE(character_funcs::can_see_fine_details(who));

        REQUIRE(who.read(&file, true));
        REQUIRE_FALSE(who.activity->is_null());
        for (auto turn = 0; turn < 300 && !who.activity->is_null(); ++turn) {
            calendar::turn += 1_turns;
            who.moves = 100;
            who.activity->do_turn(who);
        }
        CHECK(who.activity->is_null());
        CHECK(who.get_skill_level_object(skill_id("electronics")).exercise() > 0);
    }
    SECTION("a screen still requires power") {
        reader.ammo_unset();
        CHECK(who.get_book_reader(file, reasons) == nullptr);
    }
    SECTION("a screen cannot overcome blindness") {
        who.add_effect(efftype_id("blind"), 1_hours);
        REQUIRE(who.is_blind());
        CHECK(who.get_book_reader(file, reasons) == nullptr);
    }
    SECTION("a screen still requires reading glasses") {
        who.toggle_trait(trait_id("HYPEROPIC"));
        CHECK(who.get_book_reader(file, reasons) == nullptr);
    }
}

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
    CHECK(file.volume() == 1_ml);
    file.set_var("volume", 0);
    CHECK(file.volume() == 1_ml);
    CHECK(file.charges_per_volume(1_ml) == 1);
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
    CHECK(imported.volume() == 1_ml);
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
