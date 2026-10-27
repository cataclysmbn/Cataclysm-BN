#include "activity_handlers.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "digital_media.h"
#include "digital_recipes.h"
#include "flag.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "item_group.h"
#include "iuse.h"
#include "json.h"
#include "map_helpers.h"
#include "morale_types.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "ret_val.h"
#include "skill.h"
#include "state_helpers.h"
#include "type_id.h"
#include "value_ptr.h"

#include <algorithm>
#include <sstream>

TEST_CASE("digital_storage_accepts_files_only", "[digital_media][item]") {
    const auto media_id = GENERATE(
        "usb_drive", "mobile_memory_card", "mobile_memory_card_used", "mobile_memory_card_data",
        "mobile_memory_card_encrypted", "eink_tablet_pc", "eink_tablet_pc_music", "laptop",
        "laptop_screen_lit", "laptop_music", "control_laptop", "control_laptop_screen_lit",
        "control_laptop_music", "wrist_comp_budget", "wrist_comp_budget_flashlight",
        "wrist_comp_budget_music", "wrist_comp_control", "wrist_comp_control_flashlight",
        "wrist_comp_control_music");
    const auto media = item::spawn(media_id);
    const auto file = item::spawn("test_digital_instruction");
    const auto rock = item::spawn("test_rock");
    const auto bottle = item::spawn("bottle_plastic");

    CHECK(media->can_contain(*file));
    CHECK_FALSE(media->can_contain(*rock));
    CHECK_FALSE(bottle->can_contain(*file));

    media->put_in(item::spawn(*file));
    CHECK(media->spill_contents(tripoint_bub_ms::zero()));
    CHECK_FALSE(media->contents.empty());
}

TEST_CASE("digital_media_content_is_rolled_once", "[digital_media][item_group]") {
    auto media = item::spawn("test_digital_media");
    SECTION("new media") {
        initialize_digital_media(*media);
        REQUIRE(media->contents.num_item_stacks() == 1);
        CHECK(media->contents.front().typeId() == itype_id("test_digital_instruction"));
        media->contents.clear_items();
        initialize_digital_media(*media);
        CHECK(media->contents.empty());
    }
    SECTION("legacy cards retain their original data") {
        media->set_flag(flag_MC_HAS_DATA);
        media->set_var("MC_RECIPE", "SIMPLE");
        initialize_digital_media(*media);
        REQUIRE(media->contents.num_item_stacks() == 1);
        CHECK(media->contents.front().typeId() == itype_id("digital_cooking_recipe_archive"));
        CHECK(media->contents.front().has_var("DIGITAL_RECIPES"));
        CHECK_FALSE(media->has_var("MC_RECIPE"));
        initialize_digital_media(*media);
        CHECK(media->contents.num_item_stacks() == 1);
    }
}

TEST_CASE("digital_files_are_copied_and_encryption_is_respected", "[digital_media]") {
    auto dummy = avatar{};
    auto media = item::spawn("test_digital_media");
    auto reader = item::spawn("eink_tablet_pc");
    reader->ammo_set(itype_id("battery"), 10);
    initialize_digital_media(*media);
    REQUIRE(media->contents.num_item_stacks() == 1);
    media->contents.front().set_var("test_content", "preserved");

    media->set_flag(flag_MC_ENCRYPTED);
    CHECK(download_digital_files(*reader, *media, dummy) == 0);
    CHECK(reader->contents.empty());
    media->unset_flag(flag_MC_ENCRYPTED);

    CHECK(download_digital_files(*reader, *media, dummy) == 1);
    REQUIRE(reader->contents.num_item_stacks() == 1);
    CHECK(reader->contents.front().get_var("test_content") == "preserved");
    CHECK(media->contents.num_item_stacks() == 1);
    CHECK(download_digital_files(*reader, *media, dummy) == 0);
    CHECK(download_digital_files(*reader, *reader, dummy) == 0);
    reader->ammo_unset();
    CHECK(download_digital_files(*reader, *media, dummy) == 0);
}

TEST_CASE("digital_recipe_references_require_a_powered_reader", "[digital_media][recipes]") {
    auto dummy = avatar{};
    dummy.set_skill_level(skill_id("electronics"), 1);
    const auto& recipe = recipe_id("test_soldering_iron_digital_media").obj();
    REQUIRE_FALSE(dummy.knows_recipe(&recipe));
    auto media = item::spawn("test_digital_media");
    initialize_digital_media(*media);
    auto reader = item::spawn("eink_tablet_pc");
    reader->ammo_set(itype_id("battery"), 10);

    SECTION("a file on passive media supplies no recipes") {
        auto inv = inventory{};
        inv.add_item(*media, false);
        CHECK_FALSE(dummy.get_recipes_from_books(inv).contains(recipe));
        CHECK(media->contents.front().get_available_recipes(dummy).empty());
    }
    SECTION("a loose file supplies no recipes") {
        auto inv = inventory{};
        inv.add_item(media->contents.front(), false);
        CHECK_FALSE(dummy.get_recipes_from_books(inv).contains(recipe));
    }
    SECTION("installed files supply references without memorizing recipes") {
        REQUIRE(download_digital_files(*reader, *media, dummy) == 1);
        auto inv = inventory{};
        inv.add_item(*reader, false);
        CHECK(dummy.get_recipes_from_books(inv).contains(recipe));
        CHECK_FALSE(dummy.knows_recipe(&recipe));
        CHECK_FALSE(
            dummy.get_recipes_from_books(inv, [](const auto& /*recipe*/) { return false; })
                .contains(recipe));
        dummy.set_skill_level(skill_id("electronics"), 0);
        CHECK_FALSE(dummy.get_recipes_from_books(inv).contains(recipe));
        dummy.set_skill_level(skill_id("electronics"), 1);
        reader->ammo_unset();
        CHECK_FALSE(dummy.get_recipes_from_books(inv).contains(recipe));
    }
}

TEST_CASE(
    "digital_reference_files_supply_recipes_without_being_books", "[digital_media][recipes]") {
    auto dummy = avatar{};
    dummy.set_skill_level(skill_id("electronics"), 1);
    const auto& recipe = recipe_id("test_soldering_iron_digital_media").obj();
    auto reference = item::spawn("test_digital_reference");
    REQUIRE_FALSE(reference->is_book());
    initialize_digital_recipe_file(*reference);
    CHECK(reference->get_available_recipes(dummy).empty());
    CHECK_FALSE(dummy.knows_recipe(&recipe));

    auto device = item::spawn("eink_tablet_pc");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    reader.put_in(std::move(reference));
    dummy.i_add(std::move(device));
    auto inv = inventory{};
    inv.add_item(reader, false);
    const auto references = dummy.get_recipes_from_books(inv);
    CHECK(references.contains(recipe));
    CHECK(references.get_custom_difficulty(&recipe) == 1);
    CHECK_FALSE(dummy.knows_recipe(&recipe));
    dummy.set_skill_level(skill_id("electronics"), 0);
    CHECK_FALSE(dummy.get_recipes_from_books(inv).contains(recipe));
    dummy.set_skill_level(skill_id("electronics"), 1);
    reader.ammo_unset();
    CHECK_FALSE(dummy.get_recipes_from_books(inv).contains(recipe));
}

TEST_CASE("both_legacy_recipe_tokens_migrate_to_reference_files", "[digital_media][recipes]") {
    const auto token = GENERATE(std::string("SIMPLE"), std::string("SCIENCE"));
    auto media = item::spawn("mobile_memory_card");
    media->set_flag(flag_MC_HAS_DATA);
    media->set_var("MC_RECIPE", token);
    initialize_digital_media(*media);
    REQUIRE(media->contents.num_item_stacks() == 1);
    CHECK(
        media->contents.front().typeId()
        == itype_id(
            token == "SCIENCE"
                ? "digital_science_recipe_archive"
                : "digital_cooking_recipe_archive"));
    CHECK_FALSE(media->has_var("MC_RECIPE"));
    CHECK(media->contents.front().has_var("DIGITAL_RECIPES"));
    initialize_digital_media(*media);
    CHECK(media->contents.num_item_stacks() == 1);
}

TEST_CASE("digital_item_actions_require_installation", "[digital_media][iuse]") {
    auto dummy = avatar{};
    auto media = item::spawn("usb_drive");
    media->put_in(item::spawn("test_digital_action"));
    auto& source_file = media->contents.front();
    auto* source = &*media;
    dummy.i_add(std::move(media));

    CHECK_FALSE(dummy.invoke_item(&source_file, "transform"));
    CHECK(source_file.typeId() == itype_id("test_digital_action"));

    auto device = item::spawn("eink_tablet_pc");
    device->ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(*device, *source, dummy) == 1);
    auto& installed_file = device->contents.front();
    dummy.i_add(std::move(device));
    dummy.invoke_item(&installed_file, "transform");
    CHECK(installed_file.typeId() == itype_id("test_digital_instruction"));
}

TEST_CASE("digital_study_uses_the_reader", "[digital_media][reading]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    set_time(calendar::turn_zero + 12_hours);
    auto& dummy = get_avatar();
    clear_character(dummy);
    auto device = item::spawn("eink_tablet_pc");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    reader.put_in(item::spawn("test_digital_instruction"));
    auto& file = reader.contents.front();
    dummy.i_add(std::move(device));
    REQUIRE(digital_reader_for(dummy, file));

    REQUIRE(dummy.read(&file, true));
    REQUIRE_FALSE(dummy.activity->is_null());
    SECTION("losing power stops study") {
        reader.ammo_unset();
        activity_handlers::read_do_turn(&*dummy.activity, &dummy);
        CHECK(dummy.activity->is_null());
        CHECK_FALSE(digital_reader_for(dummy, file));
        CHECK_FALSE(dummy.knows_recipe(&recipe_id("test_soldering_iron_digital_media").obj()));
    }
    SECTION("losing the device stops study") {
        const auto removed = reader.detach();
        activity_handlers::read_do_turn(&*dummy.activity, &dummy);
        CHECK(dummy.activity->is_null());
        CHECK_FALSE(digital_reader_for(dummy, file));
    }
    SECTION("UPS power can supply an empty reader") {
        reader.ammo_unset();
        reader.set_flag(flag_USE_UPS);
        auto supply = item::spawn("UPS_off", calendar::turn, 10);
        auto& ups = *supply;
        dummy.i_add(std::move(supply));
        REQUIRE(digital_reader_for(dummy, file));
        calendar::turn += 1_minutes;
        activity_handlers::read_do_turn(&*dummy.activity, &dummy);
        CHECK_FALSE(dummy.activity->is_null());
        CHECK(ups.ammo_remaining() == 9);
        CHECK(reader.ammo_remaining() == 0);
    }
    SECTION("a complete lesson trains skills and consumes battery power") {
        for (auto turns = 0; turns < 300 && !dummy.activity->is_null(); ++turns) {
            calendar::turn += 1_turns;
            dummy.moves = 100;
            dummy.activity->do_turn(dummy);
        }
        CHECK(dummy.activity->is_null());
        CHECK(dummy.get_skill_level_object(skill_id("electronics")).exercise() > 0);
        CHECK(reader.ammo_remaining() < 10);
        CHECK(reader.ammo_remaining() > 0);
    }
}

TEST_CASE("digital_media_spawns_with_json_defined_contents", "[digital_media][item_group]") {
    const auto media =
        item_group::items_from(item_group_id("test_preloaded_digital_media"), calendar::turn_zero);
    REQUIRE(media.size() == 1);
    CHECK(media.front()->typeId() == itype_id("mobile_memory_card_data"));
    REQUIRE(media.front()->contents.num_item_stacks() == 1);
    CHECK(media.front()->contents.front().typeId() == itype_id("test_digital_instruction"));
}

TEST_CASE("photo_and_music_files_use_json_quantities_and_effects", "[digital_media]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::turn_zero + 12_hours;
    auto dummy = avatar{};
    clear_character(dummy);
    auto media = item::spawn("test_digital_multimedia_media");
    initialize_digital_media(*media);
    REQUIRE(media->contents.num_item_stacks() == 2);
    CHECK(digital_music_on(*media).songs == 9);
    CHECK(digital_photos_on(*media) == 7);
    auto device = item::spawn("eink_tablet_pc");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(reader, *media, dummy) == 2);
    dummy.i_add(std::move(device));
    CHECK(digital_music_on(reader).morale_cap == 9);
    namespace ranges = std::ranges;
    const auto& files = reader.contents.all_items_top();
    const auto album_it = ranges::find_if(files, [](const auto* file) {
        return file->typeId() == itype_id("test_digital_photo_album");
    });
    REQUIRE(album_it != files.end());
    auto* album = *album_it;
    REQUIRE(view_digital_photos(dummy, *album));
    CHECK(digital_photos_on(reader) == 7);
    CHECK(dummy.get_morale(MORALE_PHOTOS) == 5);
    const auto moves_after_viewing = dummy.moves;
    CHECK_FALSE(view_digital_photos(dummy, *album));
    CHECK(dummy.moves == moves_after_viewing);
    CHECK(download_digital_files(reader, *media, dummy) == 0);
    CHECK(digital_photos_on(reader) == 7);
    CHECK(digital_photos_on(*media) == 7);
    calendar::turn += 59_seconds;
    CHECK_FALSE(view_digital_photos(dummy, *album));
    calendar::turn += 1_seconds;
    REQUIRE(view_digital_photos(dummy, *album));
    CHECK(digital_photos_on(reader) == 7);
}

TEST_CASE("digital_music_survives_stop_and_replay_after_cooldown", "[digital_media][iuse]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    calendar::turn = calendar::turn_zero + 12_hours;
    auto& dummy = get_avatar();
    clear_character(dummy);
    auto device = item::spawn("test_digital_music_reader");
    auto& reader = *device;
    initialize_digital_media(reader);
    dummy.i_add(std::move(device));
    REQUIRE(digital_music_on(reader).songs == 9);

    dummy.invoke_item(&reader, "music_player");
    REQUIRE(reader.typeId() == itype_id("test_digital_music_reader_music"));
    CHECK(reader.is_active());
    CHECK(reader.get_use("EINKTABLETPC") != nullptr);
    CHECK(digital_music_on(reader).songs == 9);

    dummy.invoke_item(&reader, "MP3_ON");
    REQUIRE(reader.typeId() == itype_id("test_digital_music_reader"));
    CHECK_FALSE(reader.is_active());
    CHECK(digital_music_on(reader).songs == 9);
    dummy.invoke_item(&reader, "music_player");
    CHECK_FALSE(reader.is_active());

    calendar::turn += 60_seconds;
    dummy.invoke_item(&reader, "music_player");
    CHECK(reader.is_active());
    CHECK(digital_music_on(reader).songs == 9);
}

TEST_CASE("different_music_files_have_independent_cooldowns", "[digital_media][iuse]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    calendar::turn = calendar::turn_zero + 12_hours;
    auto& dummy = get_avatar();
    clear_character(dummy);
    auto device = item::spawn("test_digital_music_reader");
    auto& reader = *device;
    initialize_digital_media(reader);
    REQUIRE(reader.contents.num_item_stacks() == 1);
    auto& first = reader.contents.front();
    auto extra = item::spawn("test_digital_music_collection");
    extra->charges = 4;
    auto& second = *extra;
    reader.put_in(std::move(extra));
    dummy.i_add(std::move(device));

    REQUIRE(listen_to_digital_music(dummy, reader, first));
    CHECK(digital_music_playing(reader).morale_cap == 9);
    dummy.invoke_item(&reader, "MP3_ON");
    CHECK(digital_music_cooldown_remaining(dummy, first) == 60_seconds);
    CHECK(digital_music_cooldown_remaining(dummy, second) == 0_seconds);

    REQUIRE(listen_to_digital_music(dummy, reader, second));
    CHECK(digital_music_playing(reader).songs == 4);
    CHECK(digital_music_playing(reader).morale_cap == 4);
    dummy.add_morale(MORALE_MUSIC, 9, 9);
    iuse::mp3_on(&dummy, &reader, true, dummy.bub_pos());
    CHECK(dummy.get_morale(MORALE_MUSIC) >= 9);
    CHECK_FALSE(listen_to_digital_music(dummy, reader, first));
    CHECK(digital_music_playing(reader).songs == 4);

    auto duplicate = item::spawn(first);
    auto& copy = *duplicate;
    reader.put_in(std::move(duplicate));
    CHECK_FALSE(listen_to_digital_music(dummy, reader, copy));
    dummy.invoke_item(&reader, "MP3_ON");
    calendar::turn += 60_seconds;
    REQUIRE(listen_to_digital_music(dummy, reader, first));
    CHECK(digital_music_playing(reader).songs == 9);
}

TEST_CASE("different_photo_albums_have_independent_cooldowns", "[digital_media]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::turn_zero + 12_hours;
    auto dummy = avatar{};
    clear_character(dummy);
    auto device = item::spawn("test_digital_music_reader");
    auto& reader = *device;
    auto first_album = item::spawn("test_digital_photo_album");
    first_album->charges = 7;
    auto& first = *first_album;
    auto second_album = item::spawn("test_digital_photo_album");
    second_album->charges = 7;
    auto& second = *second_album;
    reader.put_in(std::move(first_album));
    reader.put_in(std::move(second_album));
    dummy.i_add(std::move(device));

    REQUIRE(view_digital_photos(dummy, first));
    const auto morale_after_first = dummy.get_morale(MORALE_PHOTOS);
    REQUIRE(view_digital_photos(dummy, second));
    CHECK(dummy.get_morale(MORALE_PHOTOS) > morale_after_first);
    CHECK_FALSE(view_digital_photos(dummy, first));
    CHECK(first.charges == 7);
    CHECK(second.charges == 7);
    auto duplicate = item::spawn(first);
    auto& copy = *duplicate;
    reader.put_in(std::move(duplicate));
    CHECK_FALSE(view_digital_photos(dummy, copy));
}

TEST_CASE("memory_card_label_matches_retained_contents", "[digital_media]") {
    auto media = item::spawn("mobile_memory_card");
    media->put_in(item::spawn("test_digital_instruction"));
    media->set_var("MC_EXTENDED_PHOTOS", "preserved camera data");
    finish_memory_card_download(*media);
    CHECK(media->typeId() == itype_id("mobile_memory_card_data"));
    CHECK(media->has_flag(flag_MC_HAS_DATA));
    CHECK(media->contents.num_item_stacks() == 1);
    CHECK(media->get_var("MC_EXTENDED_PHOTOS") == "preserved camera data");
    media->contents.clear_items();
    media->erase_var("MC_EXTENDED_PHOTOS");
    media->on_contents_changed();
    CHECK(media->typeId() == itype_id("mobile_memory_card_used"));
    CHECK_FALSE(media->has_flag(flag_MC_HAS_DATA));
}

TEST_CASE("populating_a_clean_card_updates_its_state", "[digital_media]") {
    auto media = item::spawn("mobile_memory_card_used");
    media->put_in(item::spawn("test_digital_instruction"));
    CHECK(media->typeId() == itype_id("mobile_memory_card_data"));
}

TEST_CASE("loading_a_populated_legacy_clean_card_repairs_its_state", "[digital_media][savegame]") {
    auto media = item::spawn("mobile_memory_card_used");
    // Reproduce the old save state, bypassing the corrected insertion path.
    media->contents.insert_item(item::spawn("test_digital_instruction"));
    REQUIRE(media->typeId() == itype_id("mobile_memory_card_used"));
    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    media->serialize(writer);
    auto input = std::istringstream(output.str());
    auto reader = JsonIn(input);
    const auto loaded = item::spawn(reader);
    CHECK(loaded->typeId() == itype_id("mobile_memory_card_data"));
    REQUIRE(loaded->contents.num_item_stacks() == 1);
    CHECK(loaded->contents.front().typeId() == itype_id("test_digital_instruction"));
}

TEST_CASE("saving_and_loading_preserve_playable_media_counts", "[digital_media][savegame][iuse]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto& dummy = get_avatar();
    clear_character(dummy);
    auto device = item::spawn("test_digital_music_reader");
    initialize_digital_media(*device);
    auto album = item::spawn("test_digital_photo_album");
    album->charges = 7;
    device->put_in(std::move(album));
    REQUIRE(digital_music_on(*device).songs == 9);
    REQUIRE(digital_photos_on(*device) == 7);

    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    device->serialize(writer);
    CHECK(digital_music_on(*device).songs == 9);
    CHECK(digital_photos_on(*device) == 7);

    auto input = std::istringstream(output.str());
    auto parser = JsonIn(input);
    auto loaded = item::spawn(parser);
    CHECK(digital_music_on(*loaded).songs == 9);
    CHECK(digital_photos_on(*loaded) == 7);
    auto& reader = *loaded;
    dummy.i_add(std::move(loaded));
    dummy.invoke_item(&reader, "music_player");
    CHECK(reader.is_active());
    CHECK(digital_music_playing(reader).songs == 9);
}

TEST_CASE("legacy_photo_and_music_counters_migrate_without_losing_metadata", "[digital_media]") {
    auto media = item::spawn("mobile_memory_card");
    media->set_flag(flag_MC_HAS_DATA);
    media->set_var("MC_PHOTOS", 7);
    media->set_var("MC_MUSIC", 9);
    media->set_var("MC_EXTENDED_PHOTOS", "preserved photo metadata");
    initialize_digital_media(*media);
    CHECK(digital_music_on(*media).songs == 9);
    CHECK(digital_photos_on(*media) == 7);
    CHECK_FALSE(media->has_var("MC_PHOTOS"));
    CHECK_FALSE(media->has_var("MC_MUSIC"));
    CHECK(media->get_var("MC_EXTENDED_PHOTOS") == "preserved photo metadata");
    initialize_digital_media(*media);
    CHECK(media->contents.num_item_stacks() == 2);

    auto reader = item::spawn("eink_tablet_pc");
    reader->set_var("EIPC_PHOTOS", 11);
    reader->set_var("EIPC_MUSIC", 13);
    migrate_legacy_digital_media(*reader);
    initialize_digital_media(*reader);
    CHECK(digital_photos_on(*reader) == 11);
    CHECK(digital_music_on(*reader).songs == 13);
    CHECK(reader->contents.num_item_stacks() == 2);
}
