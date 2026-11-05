#include "activity_handlers.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catalua.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "digital_media.h"
#include "digital_recipes.h"
#include "flag.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "item_group.h"
#include "iteminfo_query.h"
#include "itype.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "json.h"
#include "map/mapdata.h"
#include "map_helpers.h"
#include "morale_types.h"
#include "photo_morale.h"
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
#include <string_view>

namespace {
auto reload_item(const item& original) -> detached_ptr<item> {
    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    original.serialize(writer);
    auto input = std::istringstream(output.str());
    auto parser = JsonIn(input);
    return item::spawn(parser);
}
} // namespace

TEST_CASE("digital_media_settings_are_editable_from_lua", "[digital_media][lua]") {
    auto storage = item::spawn("usb_drive");
    auto music = item::spawn("test_digital_music_collection");
    music->charges = 9;
    auto reference = item::spawn("test_digital_reference");
    auto& file = *reference;
    auto lua = make_lua_state();
    lua["test_music"] = &*music;
    lua["test_reference"] = &file;
    const auto script = lua.safe_script(R"(
        assert(test_music:get_var_num("digital_music_morale_cap", 0) == 9)
        assert(test_reference:get_var_str("digital_recipes", "") ~= "")
        test_music:set_var_num("digital_music_morale_cap", 4)
        test_reference:set_var_str("digital_recipes", "")
        test_reference:set_var_str("digital_random_recipes", '{"count":1,"candidates":["test_soldering_iron_digital_media"],"min_difficulty":3,"max_difficulty":3}')
    )");
    REQUIRE(script.valid());
    storage->put_in(std::move(music));
    storage->put_in(std::move(reference));
    CHECK(digital_music_on(*storage).morale_cap == 4);
    const auto recipes = digital_file_recipes(file);
    REQUIRE(recipes.size() == 1);
    CHECK(recipes.front().recipe == recipe_id("test_soldering_iron_digital_media"));
    CHECK(recipes.front().skill_level == 3);

    const auto loaded = reload_item(*storage);
    CHECK(digital_music_on(*loaded).morale_cap == 4);
    REQUIRE(loaded->contents.num_item_stacks() == 2);
    CHECK(loaded->contents.back().get_var("digital_recipes").empty());
    CHECK(loaded->contents.back().get_var("digital_random_recipes")
          == file.get_var("digital_random_recipes"));
    const auto loaded_recipes = digital_file_recipes(loaded->contents.back());
    REQUIRE(loaded_recipes.size() == 1);
    CHECK(loaded_recipes.front().skill_level == 3);
}

TEST_CASE(
    "loading_digital_settings_restores_defaults_and_preserves_overrides",
    "[digital_media][savegame]") {
    const auto customized = GENERATE(false, true);
    auto music = item::spawn("test_digital_music_collection");
    if (customized) {
        music->set_var("digital_music_morale_cap", 4);
        music->set_var("digital_music_cooldown_seconds", 0);
    } else {
        music->erase_var("digital_music_morale_cap");
        music->erase_var("digital_music_cooldown_seconds");
    }
    music->set_var("DIGITAL_FILE_ID", "saved-file");
    const auto loaded = reload_item(*music);
    CHECK(loaded->get_var("digital_music_morale_cap", 0) == (customized ? 4 : 9));
    CHECK(loaded->get_var("digital_music_cooldown_seconds", -1) == (customized ? 0 : 60));
    CHECK(loaded->get_var("DIGITAL_FILE_ID") == "saved-file");
}

TEST_CASE(
    "loading_legacy_recipe_files_preserves_the_resolved_selection",
    "[digital_media][recipes][savegame]") {
    auto reference = item::spawn("test_digital_reference");
    initialize_digital_recipe_file(*reference);
    const auto selection = reference->get_var("DIGITAL_RECIPES");
    REQUIRE_FALSE(selection.empty());
    const auto definitions = reference->get_var("digital_recipes");
    reference->erase_var("digital_recipes");
    const auto loaded = reload_item(*reference);
    CHECK(loaded->get_var("digital_recipes") == definitions);
    CHECK(loaded->get_var("DIGITAL_RECIPES") == selection);
}

TEST_CASE(
    "inline_recipe_variables_validate_fixed_and_random_definitions", "[digital_media][recipes]") {
    auto file = item::spawn("test_digital_reference");
    REQUIRE(validate_digital_recipe_data(file->item_vars()).has_value());
    SECTION("unknown recipes are rejected") {
        file->set_var("digital_recipes", R"([{"id":"test_missing_digital_recipe"}])");
        const auto result = validate_digital_recipe_data(file->item_vars());
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().find("test_missing_digital_recipe") != std::string::npos);
    }
    SECTION("invalid selection ranges are rejected") {
        file->set_var("digital_random_recipes", R"({"count":-1})");
        CHECK_FALSE(validate_digital_recipe_data(file->item_vars()).has_value());
    }
    SECTION("malformed recipe JSON is rejected") {
        file->set_var("digital_recipes", "not JSON");
        CHECK_FALSE(validate_digital_recipe_data(file->item_vars()).has_value());
    }
    SECTION("random recipes do not duplicate fixed entries") {
        file->set_var("digital_random_recipes",
                      R"({"count":1,"candidates":["test_soldering_iron_digital_media"]})");
        initialize_digital_recipe_file(*file);
        const auto recipes = digital_file_recipes(*file);
        REQUIRE(recipes.size() == 1);
        CHECK(recipes.front().skill_level == 1);
    }
    SECTION("empty resolved selections do not reroll after settings change") {
        file->set_var("digital_recipes", "");
        file->set_var("digital_random_recipes", R"({"count":0})");
        initialize_digital_recipe_file(*file);
        REQUIRE(file->has_var("DIGITAL_RECIPES"));
        REQUIRE(digital_file_recipes(*file).empty());
        file->set_var("digital_random_recipes",
                      R"({"count":1,"candidates":["test_soldering_iron_digital_media"]})");
        initialize_digital_recipe_file(*file);
        CHECK(digital_file_recipes(*file).empty());
        file->erase_var("DIGITAL_RECIPES");
        initialize_digital_recipe_file(*file);
        CHECK(digital_file_recipes(*file).size() == 1);
    }
}

TEST_CASE("digital_storage_accepts_files_only", "[digital_media][item]") {
    const auto media_id = GENERATE(
        "usb_drive", "genome_drive", "sd_memory_card", "eink_tablet_pc", "eink_tablet_pc_music",
        "laptop", "laptop_screen_lit", "laptop_music", "control_laptop",
        "control_laptop_screen_lit", "control_laptop_music", "wrist_comp_budget",
        "wrist_comp_budget_flashlight", "wrist_comp_budget_music", "wrist_comp_control",
        "wrist_comp_control_flashlight", "wrist_comp_control_music", "smart_phone",
        "smart_phone_music", "smart_phone_flashlight", "afs_atomic_smartphone",
        "afs_atomic_smartphone_music", "afs_atomic_smartphone_flashlight");
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

TEST_CASE("saved_text_files_survive_copying_and_loading", "[digital_media][savegame]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto dummy = avatar{};
    auto media = item::spawn("usb_drive");
    const auto file_id =
        GENERATE("digital_saved_news", "digital_saved_flier", "digital_character_sheet",
                 "digital_radio_archive", "digital_saved_email");
    auto text = item::spawn(file_id);
    text->set_snippet(snippet_id(text->type->snippet_category + "_1"));
    const auto description = iteminfo_query(std::vector{iteminfo_parts::DESCRIPTION});
    const auto article = text->info_string(description);
    REQUIRE_FALSE(article.empty());
    CHECK(article.find(text->type->description.translated()) == std::string::npos);
    media->put_in(std::move(text));
    auto reader = item::spawn("wrist_comp_budget");
    reader->ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(*reader, *media, dummy) == 1);
    CHECK(reader->contents.front().info_string(description) == article);
    const auto loaded = reload_item(*reader);
    REQUIRE(loaded->contents.num_item_stacks() == 1);
    CHECK(loaded->contents.front().info_string(description) == article);
}

TEST_CASE(
    "digital_loot_keeps_software_without_its_default_usb_wrapper", "[digital_media][item_group]") {
    auto media = item::spawn("test_digital_software_media");
    initialize_digital_media(*media);
    REQUIRE(media->contents.num_item_stacks() == 1);
    const auto& file = media->contents.front();
    CHECK(file.typeId() == itype_id("software_useless"));
    CHECK(is_digital_file(file));
    CHECK(file.contents.empty());
}

TEST_CASE("mp3_players_spawn_with_preloaded_music", "[digital_media][item]") {
    auto player = item::spawn("mp3");
    REQUIRE(player->contents.num_item_stacks() == 1);
    const auto& music = player->contents.front();
    CHECK(music.typeId() == itype_id("digital_music_collection"));
    CHECK(music.charges == 20);
    CHECK(is_digital_file(music));
}

TEST_CASE("viewing_camera_photos_grants_shared_photo_morale", "[digital_media][morale]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::turn_zero + 12_hours;
    auto viewer = avatar{};
    apply_photo_viewing_morale(viewer);
    const auto first_reward = viewer.get_morale(MORALE_PHOTOS);
    CHECK(first_reward >= 15);
    CHECK(first_reward <= 30);

    apply_photo_viewing_morale(viewer);
    CHECK(viewer.get_morale(MORALE_PHOTOS) == first_reward);
    calendar::turn += 10_minutes;
    viewer.rem_morale(MORALE_PHOTOS);
    apply_photo_viewing_morale(viewer);
    CHECK(viewer.get_morale(MORALE_PHOTOS) >= 15);

    auto psychopath = avatar{};
    psychopath.toggle_trait(trait_id("PSYCHOPATH"));
    apply_photo_viewing_morale(psychopath);
    CHECK(psychopath.get_morale(MORALE_PHOTOS) == 0);
}

TEST_CASE(
    "terminal_disassembly_recovers_hard_drives_with_persistent_files",
    "[digital_media][hard_drive][item_group]") {
    const auto terminal =
        GENERATE("t_console", "t_console_broken", "f_console_table", "f_console_broken_table");
    const auto drop_group =
        std::string_view(terminal).starts_with("t_")
            ? ter_str_id(terminal).obj().deconstruct.drop_group
            : furn_str_id(terminal).obj().deconstruct.drop_group;
    const auto drops = item_group::items_from(drop_group, calendar::turn_zero);
    namespace ranges = std::ranges;
    using namespace std::views;
    const auto found = ranges::find_if(drops, [](const auto& drop) {
        return drop->typeId() == itype_id("hard_drive");
    });
    REQUIRE(found != drops.end());
    auto& drive = **found;
    initialize_digital_media(drive);
    REQUIRE_FALSE(drive.contents.empty());
    const auto possible = item_group::every_possible_item_from(
        item_group_id(drive.get_var("digital_content")));
    for (const auto* file : drive.contents.all_items_top()) {
        CHECK(is_digital_file(*file));
        CHECK(possible.contains(file->type));
    }
    const auto identities =
        drive.contents.all_items_top()
        | transform([](const auto* file) { return file->get_var("DIGITAL_FILE_ID"); })
        | ranges::to<std::vector<std::string>>();
    initialize_digital_media(drive);
    CHECK(ranges::equal(
        identities,
        drive.contents.all_items_top() | transform([](const auto* file) {
            return file->get_var("DIGITAL_FILE_ID");
        })));
}

TEST_CASE(
    "hard_drive_files_can_be_downloaded_copied_and_saved",
    "[digital_media][hard_drive][savegame]") {
    auto dummy = avatar{};
    auto drive = item::spawn("test_digital_hard_drive");
    auto& storage = *drive;
    initialize_digital_media(storage);
    REQUIRE(storage.contents.num_item_stacks() == 1);
    const auto identity = storage.contents.front().get_var("DIGITAL_FILE_ID");
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(reader, storage, dummy) == 1);
    dummy.i_add(std::move(device));
    storage.contents.clear_items();
    REQUIRE(upload_digital_files(dummy, {.source = reader, .destination = storage}) == 1);
    CHECK(storage.contents.front().get_var("DIGITAL_FILE_ID") == identity);
    CHECK(is_writable_digital_media(storage));
    dummy.i_add(std::move(drive));
    CHECK(std::ranges::contains(digital_genome_storage(dummy), &storage));

    const auto loaded = reload_item(storage);
    initialize_digital_media(*loaded);
    REQUIRE(loaded->contents.num_item_stacks() == 1);
    CHECK(loaded->contents.front().typeId() == itype_id("test_digital_instruction"));
    CHECK(loaded->contents.front().get_var("DIGITAL_FILE_ID") == identity);
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
    auto reader = item::spawn("wrist_comp_budget");
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

TEST_CASE("digital_urbex_maps_preserve_map_behavior_and_location", "[digital_media][map]") {
    auto dummy = avatar{};
    auto map = item::spawn("digital_urbex_map");
    const auto paper = item::spawn("urbexmap");
    REQUIRE(is_digital_file(*map));
    CHECK(map->weight() == 0_gram);
    CHECK(map->volume() == 1_ml);
    CHECK(map->charges_per_volume(1_ml) == 1);
    const auto* digital_use = map->get_use("reveal_map");
    const auto* paper_use = paper->get_use("reveal_map");
    REQUIRE(digital_use != nullptr);
    REQUIRE(paper_use != nullptr);
    const auto* digital_action = dynamic_cast<const reveal_map_actor*>(
        digital_use->get_actor_ptr());
    const auto* paper_action = dynamic_cast<const reveal_map_actor*>(paper_use->get_actor_ptr());
    REQUIRE(digital_action != nullptr);
    REQUIRE(paper_action != nullptr);
    CHECK(digital_action->radius == paper_action->radius);
    CHECK(digital_action->omt_types == paper_action->omt_types);
    CHECK(digital_action->omt_types_view == paper_action->omt_types_view);
    CHECK(digital_action->omt_types_view_exclude == paper_action->omt_types_view_exclude);

    const auto center = tripoint_abs_omt(12, 34, 0);
    map->set_var("reveal_map_center_omt", center);
    auto media = item::spawn("usb_drive");
    media->put_in(std::move(map));
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(reader, *media, dummy) == 1);
    auto& file = reader.contents.front();
    CHECK(file.get_var("reveal_map_center_omt", tripoint_abs_omt()) == center);
    dummy.i_add(std::move(device));
    REQUIRE(digital_reader_for(dummy, file));
    reader.ammo_unset();
    CHECK_FALSE(digital_reader_for(dummy, file));

    const auto loaded = reload_item(file);
    CHECK(loaded->typeId() == itype_id("digital_urbex_map"));
    CHECK(loaded->get_var("reveal_map_center_omt", tripoint_abs_omt()) == center);
    CHECK(loaded->get_use("reveal_map") != nullptr);
}

TEST_CASE("reader_files_can_be_copied_onto_an_sd_card", "[digital_media]") {
    auto dummy = avatar{};
    auto source = item::spawn("test_digital_multimedia_media");
    initialize_digital_media(*source);
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(reader, *source, dummy) == 2);
    dummy.i_add(std::move(device));
    auto card = item::spawn("sd_memory_card");
    REQUIRE(is_writable_digital_media(*card));

    CHECK(upload_digital_files(dummy, {.source = reader, .destination = *card}) == 2);
    CHECK(card->typeId() == itype_id("sd_memory_card"));
    CHECK(is_writable_digital_media(*card));
    CHECK(digital_music_on(*card).songs == 9);
    CHECK(digital_music_on(reader).songs == 9);
    CHECK(upload_digital_files(dummy, {.source = reader, .destination = *card}) == 0);
    CHECK(card->contents.num_item_stacks() == 2);

    reader.put_in(item::spawn("test_digital_instruction"));
    CHECK(upload_digital_files(dummy, {.source = reader, .destination = *card}) == 1);
    CHECK(card->contents.num_item_stacks() == 3);
    CHECK(card->contents.back().typeId() == itype_id("test_digital_instruction"));
    CHECK(digital_music_on(*card).songs == 9);

    auto empty_card = item::spawn("sd_memory_card");
    reader.ammo_unset();
    CHECK(upload_digital_files(dummy, {.source = reader, .destination = *empty_card}) == 0);
    CHECK(is_writable_digital_media(*empty_card));
}

TEST_CASE("exporting_a_selected_file_preserves_identity_and_metadata", "[digital_media]") {
    auto dummy = avatar{};
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    auto original = item::spawn("test_digital_instruction");
    original->set_var("test_content", "preserved");
    auto& file = *original;
    reader.put_in(std::move(original));
    dummy.i_add(std::move(device));
    auto card = item::spawn("sd_memory_card");
    REQUIRE(
        upload_digital_files(dummy, {.source = reader, .destination = *card, .file = &file}) == 1);
    REQUIRE(card->contents.num_item_stacks() == 1);
    CHECK(reader.contents.num_item_stacks() == 1);
    const auto& exported = card->contents.front();
    CHECK(exported.typeId() == file.typeId());
    CHECK(exported.get_var("DIGITAL_FILE_ID") == file.get_var("DIGITAL_FILE_ID"));
    CHECK(exported.get_var("test_content") == "preserved");

    auto other_reader = item::spawn("wrist_comp_budget");
    other_reader->ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(*other_reader, *card, dummy) == 1);
    CHECK(other_reader->contents.front().get_var("DIGITAL_FILE_ID")
          == file.get_var("DIGITAL_FILE_ID"));
    CHECK(card->contents.num_item_stacks() == 1);
}

TEST_CASE(
    "removable_storage_encryption_preserves_files_and_blocks_transfers",
    "[digital_media][savegame]") {
    const auto media_id = GENERATE("usb_drive", "sd_memory_card");
    auto dummy = avatar{};
    auto device = item::spawn("wrist_comp_budget");
    auto& source = *device;
    source.ammo_set(itype_id("battery"), 10);
    source.put_in(item::spawn("test_digital_instruction"));
    source.contents.front().set_var("test_content", "preserved");
    dummy.i_add(std::move(device));
    auto media = item::spawn(media_id);
    REQUIRE(upload_digital_files(dummy, {.source = source, .destination = *media}) == 1);
    const auto file_id = media->contents.front().get_var("DIGITAL_FILE_ID");
    const auto unlocked_name = media->tname();

    REQUIRE(set_digital_media_encryption(*media, true));
    CHECK(media->typeId() == itype_id(media_id));
    CHECK(media->tname() != unlocked_name);
    CHECK_FALSE(set_digital_media_encryption(source, true));
    CHECK_FALSE(is_writable_digital_media(*media));
    CHECK(upload_digital_files(dummy, {.source = source, .destination = *media}) == 0);

    auto loaded = reload_item(*media);
    REQUIRE(loaded->has_flag(flag_MC_ENCRYPTED));
    CHECK(loaded->typeId() == itype_id(media_id));
    REQUIRE(loaded->contents.num_item_stacks() == 1);
    CHECK(loaded->contents.front().get_var("test_content") == "preserved");
    CHECK(loaded->contents.front().get_var("DIGITAL_FILE_ID") == file_id);

    auto destination = item::spawn("wrist_comp_budget");
    destination->ammo_set(itype_id("battery"), 10);
    CHECK(download_digital_files(*destination, *loaded, dummy) == 0);
    CHECK(destination->contents.empty());
    REQUIRE(set_digital_media_encryption(*loaded, false));
    CHECK(is_writable_digital_media(*loaded));
    CHECK(loaded->tname() == unlocked_name);
    REQUIRE(download_digital_files(*destination, *loaded, dummy) == 1);
    CHECK(destination->contents.front().get_var("DIGITAL_FILE_ID") == file_id);
    CHECK(loaded->contents.num_item_stacks() == 1);
}

TEST_CASE("sd_card_exports_preserve_camera_data_and_reject_uninstalled_files", "[digital_media]") {
    auto dummy = avatar{};
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    reader.put_in(item::spawn("test_digital_instruction"));
    dummy.i_add(std::move(device));
    auto card = item::spawn("sd_memory_card");

    SECTION("existing camera data is preserved") {
        card->set_var("MC_EXTENDED_PHOTOS", "existing camera data");
        CHECK(is_writable_digital_media(*card));
        CHECK(upload_digital_files(dummy, {.source = reader, .destination = *card}) == 1);
        CHECK(card->get_var("MC_EXTENDED_PHOTOS") == "existing camera data");
    }
    SECTION("files must be installed on the source reader") {
        const auto elsewhere = item::spawn("test_digital_instruction");
        CHECK(upload_digital_files(
                  dummy, {.source = reader, .destination = *card, .file = &*elsewhere})
              == 0);
        CHECK(is_writable_digital_media(*card));
    }
}

TEST_CASE("digital_recipe_references_require_a_powered_reader", "[digital_media][recipes]") {
    auto dummy = avatar{};
    dummy.set_skill_level(skill_id("electronics"), 1);
    const auto& recipe = recipe_id("test_soldering_iron_digital_media").obj();
    REQUIRE_FALSE(dummy.knows_recipe(&recipe));
    const auto file_id = GENERATE("test_digital_instruction", "test_digital_reference");
    auto media = item::spawn("usb_drive");
    media->put_in(item::spawn(file_id));
    auto reader = item::spawn("wrist_comp_budget");
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
        const auto references = dummy.get_recipes_from_books(inv);
        CHECK(references.contains(recipe));
        CHECK(references.get_custom_difficulty(&recipe) == 1);
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
    "random_recipe_files_keep_their_selection_when_copied_and_saved",
    "[digital_media][recipes][savegame]") {
    auto dummy = avatar{};
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    auto reference = item::spawn("test_digital_random_reference");
    auto& file = *reference;
    reader.put_in(std::move(reference));
    const auto recipes = digital_file_recipes(file);
    REQUIRE(recipes.size() == 1);
    CHECK(recipes.front().recipe == recipe_id("test_soldering_iron_digital_media"));
    CHECK(recipes.front().skill_level == 3);
    const auto selection = file.get_var("DIGITAL_RECIPES");
    initialize_digital_recipe_file(file);
    CHECK(file.get_var("DIGITAL_RECIPES") == selection);
    dummy.i_add(std::move(device));
    auto card = item::spawn("sd_memory_card");
    REQUIRE(
        upload_digital_files(dummy, {.source = reader, .destination = *card, .file = &file}) == 1);
    CHECK(card->contents.front().get_var("DIGITAL_RECIPES") == selection);
    const auto loaded = reload_item(*card);
    REQUIRE(loaded->contents.num_item_stacks() == 1);
    CHECK(loaded->contents.front().get_var("DIGITAL_RECIPES") == selection);
}

TEST_CASE("both_legacy_recipe_tokens_migrate_to_reference_files", "[digital_media][recipes]") {
    const auto token = GENERATE(std::string("SIMPLE"), std::string("SCIENCE"));
    auto media = item::spawn("sd_memory_card");
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

    auto device = item::spawn("wrist_comp_budget");
    device->ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(*device, *source, dummy) == 1);
    auto& installed_file = device->contents.front();
    dummy.i_add(std::move(device));
    dummy.invoke_item(&installed_file, "transform");
    CHECK(installed_file.typeId() == itype_id("test_digital_instruction"));
}

TEST_CASE("hello_world_runs_repeatedly_using_a_powered_reader", "[digital_media][iuse]") {
    auto dummy = avatar{};
    const auto reader_id =
        GENERATE("eink_tablet_pc", "laptop", "control_laptop", "smart_phone", "wrist_comp_budget",
                 "wrist_comp_control");
    auto device = item::spawn(reader_id);
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    auto executable = item::spawn("digital_hello_world");
    auto& file = *executable;
    REQUIRE(file.charges == 0);
    reader.put_in(std::move(executable));
    dummy.i_add(std::move(device));
    const auto* action = file.get_use("manualnoise");
    REQUIRE(action != nullptr);
    CHECK(action->can_call(dummy, file, false, dummy.bub_pos()).success());

    const auto original_moves = dummy.moves;
    dummy.invoke_item(&file, "manualnoise");
    CHECK(dummy.moves == original_moves - 100);
    dummy.invoke_item(&file, "manualnoise");
    CHECK(dummy.moves == original_moves - 200);
    CHECK(file.charges == 0);
    CHECK(file.typeId() == itype_id("digital_hello_world"));
    CHECK(reader.contents.num_item_stacks() == 1);

    reader.ammo_unset();
    dummy.invoke_item(&file, "manualnoise");
    CHECK(dummy.moves == original_moves - 200);
}

TEST_CASE("digital_tool_actions_keep_explicit_charge_requirements", "[digital_media][iuse]") {
    auto dummy = avatar{};
    const auto tool = item::spawn("test_digital_charged_program");
    const auto* action = tool->get_use("manualnoise");
    REQUIRE(action != nullptr);
    CHECK(action->can_call(dummy, *tool, false, dummy.bub_pos()).success());
    tool->ammo_unset();
    CHECK_FALSE(action->can_call(dummy, *tool, false, dummy.bub_pos()).success());
}

TEST_CASE("digital_study_uses_the_reader", "[digital_media][reading]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    set_time(calendar::turn_zero + 12_hours);
    auto& dummy = get_avatar();
    clear_character(dummy);
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    reader.put_in(item::spawn("test_digital_instruction"));
    auto& file = reader.contents.front();
    dummy.i_add(std::move(device));
    REQUIRE(digital_reader_for(dummy, file));

    const auto already_known = dummy.knows_recipe(
        &recipe_id("test_soldering_iron_digital_media").obj());
    REQUIRE(dummy.read(&file, true));
    REQUIRE_FALSE(dummy.activity->is_null());
    SECTION("losing power stops study") {
        reader.ammo_unset();
        activity_handlers::read_do_turn(&*dummy.activity, &dummy);
        CHECK(dummy.activity->is_null());
        CHECK_FALSE(digital_reader_for(dummy, file));
        CHECK(dummy.knows_recipe(&recipe_id("test_soldering_iron_digital_media").obj())
              == already_known);
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

TEST_CASE("crafted_satellite_maps_can_be_installed_on_readers", "[digital_media][crafting]") {
    auto dummy = avatar{};
    const auto media = recipe_id("satellitemap")->create_result();
    REQUIRE(media->typeId() == itype_id("usb_drive"));
    REQUIRE(media->contents.num_item_stacks() == 1);
    CHECK(media->contents.front().typeId() == itype_id("satellitemap"));
    auto reader = item::spawn("wrist_comp_budget");
    reader->ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(*reader, *media, dummy) == 1);
    CHECK(reader->contents.front().get_use("reveal_map") != nullptr);
}

TEST_CASE("camera_photo_and_music_files_use_json_quantities", "[digital_media]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::turn_zero + 12_hours;
    auto dummy = avatar{};
    auto media = item::spawn("test_digital_multimedia_media");
    initialize_digital_media(*media);
    REQUIRE(media->contents.num_item_stacks() == 2);
    CHECK(digital_music_on(*media).songs == 9);
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(reader, *media, dummy) == 2);
    dummy.i_add(std::move(device));
    CHECK(digital_music_on(reader).morale_cap == 9);
    namespace ranges = std::ranges;
    const auto& files = reader.contents.all_items_top();
    const auto album_it = ranges::find_if(files, [](const auto* file) {
        return file->typeId() == itype_id("digital_camera_photo_collection");
    });
    REQUIRE(album_it != files.end());
    auto* album = *album_it;
    CHECK(is_digital_file(*album));
    CHECK(download_digital_files(reader, *media, dummy) == 0);
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
    auto dummy = avatar{};
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

TEST_CASE("loading_an_obsolete_used_sd_card_preserves_its_files", "[digital_media][savegame]") {
    auto media = item::spawn("sd_memory_card");
    media->put_in(item::spawn("test_digital_instruction"));
    media->contents.front().set_var("test_content", "preserved");
    media->contents.front().set_var("DIGITAL_FILE_ID", "legacy-file");
    auto photos = item::spawn("digital_camera_photo_collection");
    photos->set_var("CAMERA_EXTENDED_PHOTOS",
                    R"([{"quality":5,"name":"family","description":"A family photo."}])");
    media->put_in(std::move(photos));
    media->set_var("DIGITAL_INITIALIZED", 1);
    auto output = std::ostringstream{};
    auto writer = JsonOut(output);
    media->serialize(writer);
    auto saved = output.str();
    const auto current_id = std::string("sd_memory_card");
    const auto position = saved.find(current_id);
    REQUIRE(position != std::string::npos);
    const auto legacy_id =
        GENERATE("mobile_memory_card_used", "mobile_memory_card_data", "mobile_memory_card",
                 "mobile_memory_card_encrypted", "mobile_memory_card_science");
    saved.replace(position, current_id.size(), legacy_id);
    auto input = std::istringstream(saved);
    auto reader = JsonIn(input);
    const auto loaded = item::spawn(reader);
    CHECK(loaded->typeId() == itype_id("sd_memory_card"));
    REQUIRE(loaded->contents.num_item_stacks() == 2);
    CHECK(loaded->contents.front().typeId() == itype_id("test_digital_instruction"));
    CHECK(loaded->contents.front().get_var("test_content") == "preserved");
    CHECK(loaded->contents.front().get_var("DIGITAL_FILE_ID") == "legacy-file");
    CHECK(loaded->contents.back().get_var("CAMERA_EXTENDED_PHOTOS")
          == R"([{"quality":5,"name":"family","description":"A family photo."}])");
    const auto encrypted =
        std::string_view(legacy_id) == "mobile_memory_card_encrypted"
        || std::string_view(legacy_id) == "mobile_memory_card_science";
    CHECK(loaded->has_flag(flag_MC_ENCRYPTED) == encrypted);
    CHECK(loaded->has_flag(flag_MC_SCIENCE_STUFF)
          == (std::string_view(legacy_id) == "mobile_memory_card_science"));
    CHECK(is_writable_digital_media(*loaded) == !encrypted);
    initialize_digital_media(*loaded);
    CHECK(loaded->contents.num_item_stacks() == 2);
}

TEST_CASE(
    "saving_and_loading_preserve_music_and_camera_photos", "[digital_media][savegame][iuse]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto dummy = avatar{};
    clear_character(dummy);
    auto device = item::spawn("test_digital_music_reader");
    initialize_digital_media(*device);
    auto photos = item::spawn("digital_camera_photo_collection");
    photos->set_var("CAMERA_EXTENDED_PHOTOS",
                    R"([{"quality":5,"name":"family","description":"A family photo."}])");
    device->put_in(std::move(photos));
    REQUIRE(digital_music_on(*device).songs == 9);

    auto loaded = reload_item(*device);
    CHECK(digital_music_on(*loaded).songs == 9);
    CHECK(loaded->contents.back().get_var("CAMERA_EXTENDED_PHOTOS")
          == R"([{"quality":5,"name":"family","description":"A family photo."}])");
    auto& reader = *loaded;
    dummy.i_add(std::move(loaded));
    dummy.invoke_item(&reader, "music_player");
    CHECK(reader.is_active());
    CHECK(digital_music_playing(reader).songs == 9);
}

TEST_CASE("legacy_photo_counters_are_removed_and_music_counts_migrate", "[digital_media]") {
    auto media = item::spawn("sd_memory_card");
    media->set_flag(flag_MC_HAS_DATA);
    media->set_var("MC_PHOTOS", 7);
    media->set_var("MC_MUSIC", 9);
    media->set_var("MC_EXTENDED_PHOTOS", "preserved photo metadata");
    initialize_digital_media(*media);
    CHECK(digital_music_on(*media).songs == 9);
    CHECK_FALSE(media->has_var("MC_PHOTOS"));
    CHECK_FALSE(media->has_var("MC_MUSIC"));
    CHECK(media->get_var("MC_EXTENDED_PHOTOS") == "preserved photo metadata");
    initialize_digital_media(*media);
    CHECK(media->contents.num_item_stacks() == 1);

    auto reader = item::spawn("wrist_comp_budget");
    reader->set_var("EIPC_PHOTOS", 11);
    reader->set_var("EIPC_MUSIC", 13);
    migrate_legacy_digital_media(*reader);
    initialize_digital_media(*reader);
    CHECK(digital_music_on(*reader).songs == 13);
    CHECK_FALSE(reader->has_var("EIPC_PHOTOS"));
    CHECK(reader->contents.num_item_stacks() == 1);
}

TEST_CASE(
    "readers_generate_personal_files_once_and_keep_them_across_modes", "[digital_media][iuse]") {
    const auto base =
        GENERATE("laptop", "control_laptop", "wrist_comp_budget", "wrist_comp_control",
                 "smart_phone", "afs_atomic_smartphone", "eink_tablet_pc");
    auto dummy = avatar{};
    auto device = item::spawn(base);
    auto& reader = *device;
    if (!reader.ammo_types().empty()) { reader.ammo_set(itype_id("battery"), 100); }
    const auto group = item_group_id(reader.get_var("digital_content"));
    const auto possible = item_group::every_possible_item_from(group);
    const auto personal = item_group::every_possible_item_from(
        item_group_id("digital_content_personal"));
    for (const auto* type : personal) { CHECK(possible.contains(type)); }
    initialize_digital_media(reader);
    REQUIRE(reader.has_var("DIGITAL_INITIALIZED"));
    REQUIRE(digital_music_on(reader).songs >= 20);
    const auto count = reader.contents.num_item_stacks();
    const auto music = reader.contents.all_items_top();
    const auto found = std::ranges::find_if(music, [](const auto* file) {
        return file->has_flag(flag_id("DIGITAL_MUSIC"));
    });
    REQUIRE(found != music.end());
    auto& collection = **found;
    const auto identity = collection.get_var("DIGITAL_FILE_ID");
    dummy.i_add(std::move(device));

    if (reader.type->can_use("transform")) {
        dummy.invoke_item(&reader, "transform");
        REQUIRE(reader.is_active());
        reader.type->tick(dummy, reader, dummy.bub_pos());
        CHECK(reader.is_active());
        CHECK(reader.get_use("EINKTABLETPC") != nullptr);
        CHECK(digital_reader_ready(reader, dummy));
        CHECK(digital_reader_for(dummy, collection));
    }
    REQUIRE(listen_to_digital_music(dummy, reader, collection));
    CHECK(reader.type->can_use("MP3_ON"));
    CHECK(reader.get_use("EINKTABLETPC") != nullptr);
    dummy.invoke_item(&reader, "MP3_ON");
    CHECK(reader.typeId() == itype_id(base));
    CHECK_FALSE(reader.is_active());
    initialize_digital_media(reader);
    CHECK(reader.contents.num_item_stacks() == count);
    CHECK(collection.get_var("DIGITAL_FILE_ID") == identity);

    auto loaded = reload_item(reader);
    initialize_digital_media(*loaded);
    CHECK(loaded->contents.num_item_stacks() == count);
    CHECK(digital_music_on(*loaded).songs == digital_music_on(reader).songs);
}

TEST_CASE("downloading_initializes_unopened_media_once", "[digital_media]") {
    auto dummy = avatar{};
    auto reader = item::spawn("wrist_comp_budget");
    reader->ammo_set(itype_id("battery"), 100);
    auto media = item::spawn("test_digital_media");
    CHECK(download_digital_files(*reader, *media, dummy) == 1);
    CHECK(download_digital_files(*reader, *media, dummy) == 0);
    CHECK(media->contents.num_item_stacks() == 1);

    auto encrypted = item::spawn("test_digital_media_encrypting");
    CHECK(download_digital_files(*reader, *encrypted, dummy) == 0);
    CHECK(encrypted->has_flag(flag_MC_ENCRYPTED));
    CHECK(reader->contents.num_item_stacks() == 1);
}

TEST_CASE(
    "camera_photos_become_browsable_files_on_the_same_device",
    "[digital_media][camera][savegame]") {
    const auto device_id = GENERATE("camera", "camera_pro", "smart_phone", "wrist_comp_budget");
    auto device = item::spawn(device_id);
    CHECK_FALSE(find_camera_photo_collection(*device));
    const auto photo_key = GENERATE("CAMERA_EXTENDED_PHOTOS", "CAMERA_NPC_PHOTOS");
    const auto photos = std::string(
        R"([{"quality":5,"name":"zombie","description":"A zombie selfie."}])");
    device->set_var(photo_key, photos);
    device->set_var("CAMERA_MONSTER_PHOTOS", ",mon_zombie,5,");
    migrate_legacy_digital_media(*device);
    REQUIRE_FALSE(device->has_var(photo_key));
    REQUIRE_FALSE(device->has_var("CAMERA_MONSTER_PHOTOS"));
    auto album = find_camera_photo_collection(*device);
    REQUIRE(album);
    CHECK(is_digital_file(album->get()));
    CHECK(device->can_contain(album->get()));
    CHECK(album->get().charges == 1);
    CHECK(album->get().get_var(photo_key) == photos);
    CHECK(digital_monster_photos(*device).at(mtype_id("mon_zombie")) == 5);
    const auto identity = album->get().get_var("DIGITAL_FILE_ID");
    migrate_legacy_digital_media(*device);
    CHECK(device->contents.num_item_stacks() == 1);
    CHECK(find_camera_photo_collection(*device)->get().get_var("DIGITAL_FILE_ID") == identity);

    auto loaded = reload_item(*device);
    migrate_legacy_digital_media(*loaded);
    album = find_camera_photo_collection(*loaded);
    REQUIRE(album);
    CHECK(album->get().charges == 1);
    CHECK(album->get().get_var(photo_key) == photos);
    CHECK(digital_monster_photos(*loaded).at(mtype_id("mon_zombie")) == 5);
}

TEST_CASE(
    "captured_photo_collections_can_be_revisited_and_copied_after_new_captures",
    "[digital_media][camera]") {
    auto dummy = avatar{};
    auto device = item::spawn("camera_pro");
    auto& camera = *device;
    camera.ammo_set(itype_id("battery"), 100);
    dummy.i_add(std::move(device));
    auto& album = camera_photo_collection(camera);
    const auto first = std::string(
        R"([{"quality":3,"name":"zombie","description":"A distant zombie."}])");
    album.set_var("CAMERA_EXTENDED_PHOTOS", first);
    album.set_var("CAMERA_MONSTER_PHOTOS", ",mon_zombie,3,");
    update_camera_photo_collection(camera, 1);
    const auto original = album.get_var("DIGITAL_FILE_ID");
    auto card = item::spawn("sd_memory_card");
    REQUIRE(
        upload_digital_files(dummy, {.source = camera, .destination = *card, .file = &album}) == 1);
    auto reader = item::spawn("wrist_comp_budget");
    reader->ammo_set(itype_id("battery"), 100);
    REQUIRE(download_digital_files(*reader, *card, dummy) == 1);
    CHECK(reader->contents.front().get_var("CAMERA_EXTENDED_PHOTOS") == first);
    CHECK(digital_monster_photos(*reader).at(mtype_id("mon_zombie")) == 3);
    CHECK(album.charges == 1);
    CHECK(album.get_var("CAMERA_EXTENDED_PHOTOS") == first);
    CHECK(download_digital_files(*reader, *card, dummy) == 0);

    const auto second = std::string(
        R"([{"quality":3,"name":"zombie","description":"A distant zombie."},{"quality":5,"name":"zombie","description":"A zombie selfie."}])");
    album.set_var("CAMERA_EXTENDED_PHOTOS", second);
    album.set_var("CAMERA_MONSTER_PHOTOS", ",mon_zombie,5,");
    update_camera_photo_collection(camera, 2);
    CHECK(album.get_var("DIGITAL_FILE_ID") != original);
    CHECK(camera.contents.num_item_stacks() == 1);
    auto next_card = item::spawn("sd_memory_card");
    REQUIRE(
        upload_digital_files(dummy, {.source = camera, .destination = *next_card, .file = &album})
        == 1);
    REQUIRE(download_digital_files(*reader, *next_card, dummy) == 1);
    CHECK(digital_monster_photos(*reader).at(mtype_id("mon_zombie")) == 5);
    CHECK(reader->contents.back().get_var("CAMERA_EXTENDED_PHOTOS") == second);
    CHECK(album.charges == 2);
    CHECK(card->contents.front().get_var("CAMERA_EXTENDED_PHOTOS") == first);
}

TEST_CASE(
    "legacy_genome_drives_migrate_to_copyable_sample_files", "[digital_media][genome][savegame]") {
    const auto drive_id =
        GENERATE("genome_drive", "genome_drive_black_rat", "genome_drive_wolf_mutant_huge");
    auto drive = item::spawn(drive_id);
    if (!drive->has_var("specimen_sample")) {
        drive->set_var("specimen_sample", "mon_black_rat");
        drive->set_var("specimen_sample_progress", "1");
        drive->set_var("specimen_name", "partial rat sample");
    }
    const auto specimen = drive->get_var("specimen_sample");
    const auto progress = drive->get_var("specimen_sample_progress");
    const auto name = drive->get_var("specimen_name");
    drive->set_var("item_note", "Keep this drive");
    SECTION("opening old media migrates it") { initialize_digital_media(*drive); }
    SECTION("loading an old save migrates it") { drive = reload_item(*drive); }
    REQUIRE(drive->contents.num_item_stacks() == 1);
    auto& sample = drive->contents.front();
    CHECK(is_digital_file(sample));
    CHECK(drive->can_contain(sample));
    CHECK(sample.get_var("specimen_sample") == specimen);
    CHECK(sample.get_var("specimen_sample_progress") == progress);
    CHECK(sample.get_var("specimen_name") == name);
    CHECK(drive->get_var("item_note") == "Keep this drive");
    CHECK(drive->typeId() == itype_id("usb_drive"));
    CHECK_FALSE(drive->has_var("specimen_sample"));
    const auto identity = sample.get_var("DIGITAL_FILE_ID");
    REQUIRE_FALSE(identity.empty());
    initialize_digital_media(*drive);
    CHECK(drive->contents.num_item_stacks() == 1);
    CHECK(sample.get_var("DIGITAL_FILE_ID") == identity);

    auto dummy = avatar{};
    auto device = item::spawn("wrist_comp_budget");
    auto& reader = *device;
    reader.ammo_set(itype_id("battery"), 10);
    REQUIRE(download_digital_files(reader, *drive, dummy) == 1);
    CHECK(download_digital_files(reader, *drive, dummy) == 0);
    drive = reload_item(*drive);
    initialize_digital_media(*drive);
    CHECK(drive->typeId() == itype_id("usb_drive"));
    REQUIRE(drive->contents.num_item_stacks() == 1);
    CHECK(drive->contents.front().get_var("DIGITAL_FILE_ID") == identity);
    CHECK(download_digital_files(reader, *drive, dummy) == 0);

    auto& imported = reader.contents.front();
    CHECK(imported.get_var("specimen_sample") == specimen);
    CHECK(imported.get_var("specimen_sample_progress") == progress);
    CHECK(imported.get_var("specimen_name") == name);
    CHECK(imported.get_var("DIGITAL_FILE_ID") == identity);
    dummy.i_add(std::move(device));
    auto card = item::spawn("sd_memory_card");
    REQUIRE(upload_digital_files(dummy, {.source = reader, .destination = *card, .file = &imported})
            == 1);
    CHECK(card->contents.front().get_var("specimen_sample") == specimen);
    CHECK(card->contents.front().get_var("specimen_sample_progress") == progress);
    CHECK(card->contents.front().get_var("specimen_name") == name);
    CHECK(card->contents.front().get_var("DIGITAL_FILE_ID") == identity);
}

TEST_CASE("genome_workstation_finds_samples_inside_storage", "[digital_media][genome]") {
    auto dummy = avatar{};
    const auto storage_id =
        GENERATE("genome_drive", "usb_drive", "sd_memory_card", "eink_tablet_pc");
    auto storage = item::spawn(storage_id);
    auto file = item::spawn("digital_genome_sample");
    file->set_var("specimen_sample", "mon_black_rat");
    file->set_var("specimen_sample_progress", "1");
    auto* sample = file.get();
    storage->put_in(std::move(file));
    dummy.i_add(std::move(storage));
    auto old_drive = item::spawn("genome_drive_black_rat");
    dummy.i_add(std::move(old_drive));
    const auto samples = digital_genome_samples(dummy);
    REQUIRE(samples.size() == 2);
    CHECK(std::ranges::contains(samples, sample));
    CHECK(std::ranges::all_of(samples, [](const auto* entry) { return is_digital_file(*entry); }));
    CHECK(digital_genome_samples(dummy).size() == 2);
}

TEST_CASE(
    "genome_storage_accepts_usb_and_sd_cards_without_erasing_files", "[digital_media][genome]") {
    const auto media_id = GENERATE("usb_drive", "sd_memory_card");
    auto dummy = avatar{};
    auto media = item::spawn(media_id);
    auto& storage = *media;
    storage.put_in(item::spawn("test_digital_instruction"));
    dummy.i_add(std::move(media));
    auto encrypted_card = item::spawn("sd_memory_card");
    encrypted_card->set_flag(flag_MC_ENCRYPTED);
    dummy.i_add(std::move(encrypted_card));
    dummy.i_add(item::spawn("wrist_comp_budget"));
    const auto destinations = digital_genome_storage(dummy);
    REQUIRE(destinations.size() == 1);
    CHECK(destinations.front() == &storage);
    storage.put_in(item::spawn("digital_genome_sample"));
    CHECK(storage.contents.num_item_stacks() == 2);
    CHECK(storage.contents.front().typeId() == itype_id("test_digital_instruction"));
    CHECK(digital_genome_samples(dummy).size() == 1);
}

TEST_CASE("prefilled_genome_files_spawn_in_ordinary_usb_drives", "[digital_media][genome]") {
    const auto file_id =
        GENERATE("digital_genome_sample_black_rat", "digital_genome_sample_wolf_mutant_huge");
    const auto drive = item::in_its_container(item::spawn(file_id));
    CHECK(drive->typeId() == itype_id("usb_drive"));
    REQUIRE(drive->contents.num_item_stacks() == 1);
    CHECK(drive->contents.front().typeId() == itype_id(file_id));
    CHECK(is_digital_file(drive->contents.front()));
    CHECK_FALSE(drive->contents.front().get_var("specimen_sample").empty());
}

TEST_CASE("encrypted_cards_hide_genome_samples_from_the_workstation", "[digital_media][genome]") {
    auto dummy = avatar{};
    auto media = item::spawn("sd_memory_card");
    media->set_flag(flag_MC_ENCRYPTED);
    media->put_in(item::spawn("digital_genome_sample_black_rat"));
    dummy.i_add(std::move(media));
    CHECK(digital_genome_samples(dummy).empty());
}

TEST_CASE("reader_ports_control_storage_transfers", "[digital_media][ports]") {
    const auto reader_id =
        GENERATE("laptop", "laptop_screen_lit", "laptop_music", "smart_phone", "smart_phone_music",
                 "smart_phone_flashlight", "afs_atomic_smartphone", "wrist_comp_budget");
    const auto storage_id = GENERATE("usb_drive", "sd_memory_card", "hard_drive");
    const auto supports_all = std::string_view(reader_id) == "wrist_comp_budget";
    const auto is_laptop = std::string_view(reader_id).starts_with("laptop");
    const auto is_sd = std::string_view(storage_id) == "sd_memory_card";
    const auto compatible = supports_all || is_laptop != is_sd;
    auto dummy = avatar{};
    auto device = item::spawn(reader_id);
    auto& reader = *device;
    if (reader.ammo_required() > 0) { reader.ammo_set(itype_id("battery"), 10); }
    reader.set_var("DIGITAL_INITIALIZED", 1);
    dummy.i_add(std::move(device));
    auto storage = item::spawn(storage_id);
    storage->set_var("DIGITAL_INITIALIZED", 1);
    storage->put_in(item::spawn("test_digital_instruction"));
    CHECK(digital_reader_can_access_storage(reader, *storage) == compatible);
    CHECK(download_digital_files(reader, *storage, dummy) == (compatible ? 1 : 0));
    reader.put_in(item::spawn("test_digital_reference"));
    CHECK(upload_digital_files(dummy, {.source = reader, .destination = *storage})
          == (compatible ? 1 : 0));
    CHECK(storage->contents.num_item_stacks() == (compatible ? 2 : 1));
}

TEST_CASE("digital_storage_ports_match_complete_tokens", "[digital_media][ports]") {
    auto reader = item::spawn("wrist_comp_budget");
    auto storage = item::spawn("usb_drive");
    reader->set_var("digital_reader_ports", "USB_C,USB");
    CHECK(digital_reader_can_access_storage(*reader, *storage));
    reader->set_var("digital_reader_ports", "USB_C");
    CHECK_FALSE(digital_reader_can_access_storage(*reader, *storage));
    storage->set_var("digital_storage_ports", "SD,USB_C");
    CHECK(digital_reader_can_access_storage(*reader, *storage));
    storage->erase_var("digital_storage_ports");
    reader->erase_var("digital_reader_ports");
    CHECK(digital_reader_can_access_storage(*reader, *storage));
}
