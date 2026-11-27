#include "activity_type.h"
#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "clzones.h"
#include "flag.h"
#include "item.h"
#include "item_category.h"
#include "item_search.h"
#include "itype.h"
#include "json.h"
#include "language.h"
#include "map/map.h"
#include "map_helpers.h"
#include "options_helpers.h"
#include "output.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "relic.h"
#include "ui.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"
#include "zone_groups.h"
#include "zone_sorting.h"
#include "zone_ui.h"

#include <sstream>
#include <string>

TEST_CASE("zone_row_keeps_room_for_long_name_and_location", "[zone][zone_ui]") {
    const auto nearby = make_zone_row_layout(43, "0 C");
    CHECK(nearby.name_width == 35);
    CHECK(nearby.location_x == 40);
    CHECK(nearby.vehicle_x == 39);
    const auto distant = make_zone_row_layout(43, "12345 NW");
    CHECK(distant.name_width == 30);
    CHECK(distant.location_x == 35);
    CHECK(distant.location_x + utf8_width("12345 NW") == 43);
    const auto unicode_direction = std::string("12345 \u5317\u897f");
    const auto unicode = make_zone_row_layout(43, unicode_direction);
    CHECK(unicode.location_x + utf8_width(unicode_direction) == 43);
    const auto wide = make_zone_row_layout(63, "0 C");
    CHECK(wide.name_width > nearby.name_width);
}

TEST_CASE("custom_zone_details_show_only_filter_text", "[zone][zone_ui][json]") {
    auto stream = std::istringstream{R"({"mark":"c:food,m:iron,q:hammering"})"};
    auto input = JsonIn{stream};
    auto options = loot_options{};
    options.deserialize(input.get_object());
    CHECK(options.get_zone_name_suggestion().empty());
    const auto descriptions = options.get_descriptions();
    REQUIRE(descriptions.size() == 1);
    CHECK(descriptions.front().first.empty());
    CHECK(descriptions.front().second == "c:food,m:iron,q:hammering");
}

TEST_CASE("category_storage_categories_and_round_trip", "[zone_ui][category_storage]") {
    for (const auto& group : loot_zone_groups()) {
        REQUIRE(!group.categories.empty());
        for (const auto& category : group.categories) { CHECK(category.is_valid()); }
    }
    const auto tools = std::ranges::find(loot_zone_groups(), "tools", [](const auto& group) {
        return group.id.str();
    });
    REQUIRE(tools != loot_zone_groups().end());
    auto options = loot_options();
    options.set_categories(tools->categories);
    const auto hammer = item::spawn_temporary("test_zone_hammer");
    const auto crowbar = item::spawn_temporary("test_zone_crowbar");
    const auto rock = item::spawn_temporary("test_rock");
    REQUIRE(options.matches(*hammer));
    REQUIRE(options.matches(*crowbar));
    CHECK_FALSE(options.matches(*rock));
    auto stream = std::istringstream(R"({"categories":["LOOT_WORKSHOP_TOOLS"]})");
    auto input = JsonIn(stream);
    options.deserialize(input.get_object());
    CHECK(options.matches(*hammer));
    CHECK_FALSE(options.matches(*crowbar));
    auto out = std::ostringstream();
    auto json = JsonOut(out);
    json.start_object();
    options.serialize(json);
    json.end_object();
    auto restored = loot_options();
    auto saved = std::istringstream(out.str());
    auto saved_input = JsonIn(saved);
    restored.deserialize(saved_input.get_object());
    CHECK(restored.uses_categories());
    CHECK(restored.matches(*hammer));
    CHECK_FALSE(restored.matches(*crowbar));
    {
        const auto language = override_option("USE_LANG", "en_US");
        set_language();
        CHECK(restored.matches(*hammer));
        CHECK_FALSE(restored.matches(*crowbar));
    }
    set_language();
}

TEST_CASE(
    "category_storage_is_fallback_for_unavailable_specialized_zone",
    "[zone_ui][category_storage]") {
    clear_avatar();
    clear_map();
    auto& mgr = zone_manager::get_manager();
    auto out = std::ostringstream();
    auto json = JsonOut(out);
    mgr.serialize(json);
    const auto restore = on_out_of_scope([&]() {
        auto stream = std::istringstream(out.str());
        auto input = JsonIn(stream);
        mgr.deserialize(input);
        mgr.cache_data();
    });
    auto empty = std::istringstream("[]");
    auto input = JsonIn(empty);
    mgr.deserialize(input);
    mgr.cache_data();
    auto& here = get_map();
    const auto origin = get_avatar().abs_pos();
    const auto broad = origin + tripoint_rel_ms(1, 0, 0);
    const auto specific = origin + tripoint_rel_ms(2, 0, 0);
    auto options = make_shared_fast<loot_options>();
    const auto tools = std::ranges::find(loot_zone_groups(), "tools", [](const auto& group) {
        return group.id.str();
    });
    REQUIRE(tools != loot_zone_groups().end());
    options->set_categories(tools->categories);
    mgr.add("Tools", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, broad, broad, options);
    const auto hammer = item::spawn_temporary("test_zone_hammer");
    REQUIRE(hammer->get_category().zone() == zone_type_id("LOOT_WORKSHOP_TOOLS"));
    CHECK(mgr.get_near_zone_type_for_item(*hammer, origin, 10) == zone_type_id("LOOT_CUSTOM"));
    mgr.add("Workshop", zone_type_id("LOOT_WORKSHOP_TOOLS"), your_fac, false, true, specific,
            specific);
    CHECK(mgr.get_near_zone_type_for_item(*hammer, origin, 10)
          == zone_type_id("LOOT_WORKSHOP_TOOLS"));
    auto rocks = item::spawn("test_rock");
    rocks->charges = 1000000;
    here.add_item(abs_to_bub(specific), std::move(rocks));
    REQUIRE(here.free_volume(abs_to_bub(specific)) < hammer->volume());
    CHECK(mgr.get_near_zone_type_for_item(*hammer, origin, 10) == zone_type_id("LOOT_CUSTOM"));
    here.i_clear(abs_to_bub(specific));
    here.ter_set(abs_to_bub(specific), ter_id("t_wall"));
    CHECK(mgr.get_near_zone_type_for_item(*hammer, origin, 10) == zone_type_id("LOOT_CUSTOM"));
}

TEST_CASE("category_food_choices_are_independent", "[zone_ui][category_storage]") {
    auto options = loot_options();
    auto stream = std::istringstream(R"({"categories":["LOOT_DRINK"]})");
    auto input = JsonIn(stream);
    options.deserialize(input.get_object());
    const auto water = item::spawn_temporary("test_zone_drink");
    const auto apple = item::spawn_temporary("test_zone_food");
    CHECK(options.matches(*water));
    CHECK_FALSE(options.matches(*apple));
}

TEST_CASE("zone_details_panel_grows_and_shrinks_with_selected_content", "[zone_ui]") {
    const auto short_details = make_zone_panel_layout(40, 3);
    const auto long_details = make_zone_panel_layout(40, 14);
    CHECK(long_details.details_height == 14);
    CHECK(long_details.footer_height == long_details.details_height + 7);
    CHECK(long_details.list_height < short_details.list_height);
    CHECK(long_details.footer_height + long_details.list_height == 40);
    CHECK(make_zone_panel_layout(40, 3).details_height == short_details.details_height);
    const auto overflow = make_zone_panel_layout(24, 100);
    CHECK(overflow.list_height >= 3);
    CHECK(overflow.footer_height + overflow.list_height == 24);
    CHECK(overflow.details_height < 100);
}

TEST_CASE(
    "zone_creation_checklist_mixes_categories_without_rules_becoming_storage",
    "[zone_ui][category_storage]") {
    const auto available = zone_storage_categories();
    const auto rules = zone_creation_rules();
    CHECK(std::ranges::contains(available, zone_type_id("LOOT_BOOKS")));
    CHECK(std::ranges::contains(available, zone_type_id("LOOT_WORKSHOP_TOOLS")));
    CHECK_FALSE(std::ranges::contains(available, zone_type_id("LOOT_CUSTOM")));
    for (const auto& rule : rules) {
        CHECK(rule.is_valid());
        CHECK_FALSE(std::ranges::contains(available, rule));
    }
    auto selected = std::vector<zone_type_id>{zone_type_id("LOOT_BOOKS")};
    const auto tools = std::ranges::find(loot_zone_groups(), "tools", [](const auto& group) {
        return group.id.str();
    });
    REQUIRE(tools != loot_zone_groups().end());
    toggle_zone_categories(selected, {tools->categories.front()});
    toggle_zone_categories(selected, tools->categories);
    CHECK(selected.size() == tools->categories.size() + 1);
    toggle_zone_categories(selected, tools->categories);
    CHECK(selected == std::vector<zone_type_id>{zone_type_id("LOOT_BOOKS")});
    toggle_zone_categories(selected, tools->categories);
    auto options = loot_options();
    options.set_categories(selected);
    CHECK(options.matches(*item::spawn_temporary("test_zone_hammer")));
    CHECK(options.matches(*item::spawn_temporary("test_zone_book")));
    CHECK_FALSE(options.matches(*item::spawn_temporary("test_rock")));
    auto out = std::ostringstream();
    auto json = JsonOut(out);
    json.start_object();
    options.serialize(json);
    json.end_object();
    auto restored = loot_options();
    auto stream = std::istringstream(out.str());
    auto input = JsonIn(stream);
    restored.deserialize(input.get_object());
    CHECK(restored.matches(*item::spawn_temporary("test_zone_hammer")));
    CHECK(restored.matches(*item::spawn_temporary("test_zone_book")));
    CHECK_FALSE(restored.matches(*item::spawn_temporary("test_rock")));
}

TEST_CASE("zone_filter_wrap_keeps_conditions_intact", "[zone_ui]") {
    const auto lines = wrap_zone_filter("c:food,c:cooking tools,m:iron", 20);
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "c:food,");
    CHECK(lines[1] == "c:cooking tools,");
    CHECK(lines[2] == "m:iron");
    CHECK(wrap_zone_filter("c:food,m:iron", 40) == std::vector<std::string>{"c:food,m:iron"});
    for (const auto& line : wrap_zone_filter("c:agricultural tools", 12)) {
        CHECK(utf8_width(line) <= 12);
    }
}

TEST_CASE("zone_groups_follow_document_order_without_duplicate_types", "[zone_ui]") {
    const auto& groups = loot_zone_groups();
    REQUIRE(groups.size() == 17);
    CHECK(groups.front().id.str() == "food");
    CHECK(groups[4].id.str() == "technology");
    CHECK(groups[13].id.str() == "npc");
    CHECK(groups[14].id.str() == "fieldwork");
    CHECK(groups[15].id.str() == "construction");
    CHECK(groups.back().id.str() == "rules");
    for (const auto& id : groups.back().categories) {
        CHECK(std::ranges::contains(zone_creation_rules(), id));
    }
    auto seen = std::vector<zone_type_id>();
    for (const auto& group : groups) {
        for (const auto& id : group.categories) {
            CHECK(id.is_valid());
            CHECK_FALSE(std::ranges::contains(seen, id));
            seen.push_back(id);
        }
    }
    const auto categories = zone_storage_categories();
    CHECK(std::ranges::contains(categories, zone_type_id("LOOT_SCRAP_ELECTRONICS")));
    CHECK_FALSE(std::ranges::contains(categories, zone_type_id("FARM_PLOT")));
}

TEST_CASE("zone_category_continuations_start_at_left_edge", "[zone_ui]") {
    const auto lines = wrap_zone_filter("Workshop tools, Entry tools, Books", 25);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "Workshop tools,");
    CHECK(lines[1] == "Entry tools, Books");
}

TEST_CASE("zone_group_names_and_suggestions_follow_language", "[zone_ui][i18n]") {
    auto storage = loot_options();
    auto english_order = std::vector<zone_type_id>();
    auto english_groups = std::vector<std::string>();
    storage.set_categories({zone_type_id("LOOT_WORKSHOP_TOOLS")});
    {
        const auto language = override_option("USE_LANG", "en_US");
        set_language();
        english_order = zone_storage_categories();
        for (const auto& group : loot_zone_groups()) { english_groups.push_back(group.id.str()); }
        CHECK(storage.get_zone_name_suggestion() == "Workshop Tools");
        CHECK(zone_selection_name({zone_type_id("LOOT_FOOD")}) == "Food");
        CHECK(zone_selection_name({zone_type_id("LOOT_BOOKS"), zone_type_id("LOOT_TOOLS")})
              == "Other Tools and Books");
    }
    {
        const auto language = override_option("USE_LANG", "ru_RU");
        set_language();
        CHECK(zone_storage_categories() == english_order);
        auto russian_groups = std::vector<std::string>();
        for (const auto& group : loot_zone_groups()) { russian_groups.push_back(group.id.str()); }
        CHECK(russian_groups == english_groups);
        CHECK(storage.get_zone_name_suggestion() == zone_type_id("LOOT_WORKSHOP_TOOLS")->name());
        CHECK(
            zone_selection_name({zone_type_id("LOOT_FOOD")}) == zone_type_id("LOOT_FOOD")->name());
        CHECK(
            zone_selection_name({zone_type_id("LOOT_BOOKS"), zone_type_id("LOOT_TOOLS")})
            == zone_type_id("LOOT_TOOLS")->name() + _(" and ") + zone_type_id("LOOT_BOOKS")->name());
    }
    set_language();
}

TEST_CASE("zone_group_json_extensions_keep_unassigned_categories_available", "[zone_ui][json]") {
    const auto saved = loot_zone_groups();
    const auto restore = on_out_of_scope([&]() { reset_loot_zone_groups(saved); });
    const auto load = [](const std::string& text) {
        auto stream = std::istringstream(text);
        auto input = JsonIn(stream);
        load_loot_zone_group(input.get_object(), "test_data");
    };
    load(R"({"id":"tools","copy-from":"tools","delete":{"categories":["LOOT_TOOLS"]}})");
    const auto available = zone_storage_categories();
    CHECK(std::ranges::contains(available, zone_type_id("LOOT_TOOLS")));
    CHECK(std::ranges::count(available, zone_type_id("LOOT_TOOLS")) == 1);
    load(R"({"id":"tools","copy-from":"tools","extend":{"categories":["LOOT_TOOLS"]}})");
    const auto tools = std::ranges::find(loot_zone_groups(), "tools", [](const auto& group) {
        return group.id.str();
    });
    REQUIRE(tools != loot_zone_groups().end());
    CHECK(std::ranges::count(tools->categories, zone_type_id("LOOT_TOOLS")) == 1);
    load(
        R"({"id":"test_mod_group","name":"Mod tools","sort_order":-10,"categories":["LOOT_TOOLS","not_installed"]})");
    CHECK(loot_zone_groups().front().id.str() == "test_mod_group");
    const auto reordered = zone_storage_categories();
    CHECK(std::ranges::count(reordered, zone_type_id("LOOT_TOOLS")) == 1);
    CHECK_FALSE(std::ranges::contains(reordered, zone_type_id("not_installed")));
    load(R"({"id":"rules","copy-from":"rules","sort_order":-1000})");
    const auto rule_group = std::ranges::find(loot_zone_groups(), "rules", [](const auto& group) {
        return group.id.str();
    });
    const auto activity_group =
        std::ranges::find(loot_zone_groups(), "activity", &loot_zone_group::kind);
    CHECK(rule_group > activity_group);
    load(R"({"id":"rules","copy-from":"rules","categories":["LOOT_IGNORE"]})");
    const auto single_rule_rows = make_zone_checklist_rows(false);
    CHECK(
        std::ranges::count_if(
            single_rule_rows,
            [](const auto& row) {
                return row.rule
                    && row.members == std::vector<zone_type_id>{zone_type_id("LOOT_IGNORE")};
            })
        == 1);
    load(
        R"({"id":"test_inherited_group","copy-from":"tools","name":{"str":"Test tools","ctxt":"zone group test"},"delete":{"categories":["LOOT_TOOLS"]}})");
    const auto inherited =
        std::ranges::find(loot_zone_groups(), "test_inherited_group", [](const auto& group) {
            return group.id.str();
        });
    REQUIRE(inherited != loot_zone_groups().end());
    CHECK(inherited->title == to_translation("zone group test", "Test tools"));
    CHECK_FALSE(std::ranges::contains(inherited->categories, zone_type_id("LOOT_TOOLS")));
    CHECK(std::ranges::contains(inherited->categories, zone_type_id("LOOT_WORKSHOP_TOOLS")));
    reset_loot_zone_groups();
    CHECK(zone_creation_rules().empty());
    auto english_order = std::vector<zone_type_id>();
    {
        const auto language = override_option("USE_LANG", "en_US");
        set_language();
        english_order = zone_storage_categories();
    }
    {
        const auto language = override_option("USE_LANG", "ru_RU");
        set_language();
        CHECK(zone_storage_categories() == english_order);
    }
    set_language();
}

TEST_CASE("zone_filter_prefix_highlights_follow_filter_syntax", "[zone_ui]") {
    const auto text = std::string("c:food,-m:iron,b:q:cut;k:survival,http:c:literal");
    auto marked = std::vector<std::string>();
    for (const auto byte : zone_filter_prefixes(text)) { marked.push_back(text.substr(byte, 2)); }
    CHECK(marked == std::vector<std::string>{"c:", "m:", "b:", "q:", "k:"});
    CHECK(zone_filter_prefixes("ordinary text without a prefix").empty());
    CHECK(zone_filter_prefixes("<color_red>text</color>,m:wood") == std::vector<size_t>{24});
}

TEST_CASE("simple_zone_functions_share_storage_area", "[zone_ui]") {
    CHECK(zone_can_share_area(zone_type_id("AUTO_EAT")));
    CHECK(zone_can_share_area(zone_type_id("AUTO_DRINK")));
    CHECK(zone_can_share_area(zone_type_id("SOURCE_FIREWOOD")));
    CHECK(zone_can_share_area(zone_type_id("NPC_RETREAT")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("FARM_PLOT")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("CONSTRUCTION_BLUEPRINT")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("CONSTRUCTION_IGNORE")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("CHOP_TREES")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("MINING")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("FISHING_SPOT")));
    CHECK_FALSE(zone_can_share_area(zone_type_id("LOOT_CUSTOM")));
    CHECK(std::ranges::contains(zone_creation_rules(), zone_type_id("AUTO_EAT")));
    clear_avatar();
    clear_map();
    auto& mgr = zone_manager::get_manager();
    auto saved = std::ostringstream();
    auto saved_out = JsonOut(saved);
    mgr.serialize(saved_out);
    const auto restore = on_out_of_scope([&]() {
        auto stream = std::istringstream(saved.str());
        auto input = JsonIn(stream);
        mgr.deserialize(input);
        mgr.cache_data();
    });
    auto empty = std::istringstream("[]");
    auto input = JsonIn(empty);
    mgr.deserialize(input);
    mgr.cache_data();
    const auto origin = get_avatar().abs_pos();
    const auto spot = origin + tripoint_rel_ms(1, 0, 0);
    auto storage = make_shared_fast<loot_options>();
    storage->set_categories({zone_type_id("LOOT_PFOOD")});
    mgr.add("Perishable food", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, spot, spot,
            storage);
    mgr.add("Auto eat", zone_type_id("AUTO_EAT"), your_fac, false, true, spot, spot);
    const auto apple = item::spawn_temporary("test_zone_food");
    REQUIRE(storage->matches(*apple));
    CHECK(mgr.get_near_zone_type_for_item(*apple, origin, 10) == zone_type_id("LOOT_CUSTOM"));
    CHECK(mgr.get_near(zone_type_id("AUTO_EAT"), origin, 10).contains(spot));
    auto out = std::ostringstream();
    auto json = JsonOut(out);
    mgr.serialize(json);
    auto round_trip = std::istringstream(out.str());
    auto restored = JsonIn(round_trip);
    mgr.deserialize(restored);
    mgr.cache_data();
    CHECK(mgr.get_near_zone_type_for_item(*apple, origin, 10) == zone_type_id("LOOT_CUSTOM"));
    CHECK(mgr.get_near(zone_type_id("AUTO_EAT"), origin, 10).contains(spot));
}

namespace {
struct sorting_fixture {
    std::string saved;
    std::optional<override_option> language;

    sorting_fixture() {
        language.emplace("USE_LANG", "en_US");
        set_language();
        auto stream = std::ostringstream();
        auto output = JsonOut(stream);
        zone_manager::get_manager().serialize(output);
        saved = stream.str();
        clear_avatar();
        clear_map();
        zone_manager::reset_manager();
    }
    ~sorting_fixture() {
        clear_map();
        zone_manager::reset_manager();
        auto stream = std::istringstream(saved);
        auto input = JsonIn(stream);
        zone_manager::get_manager().deserialize(input);
        zone_manager::get_manager().cache_data();
        language.reset();
        set_language();
    }

    auto add(const tripoint_abs_ms& point, const zone_type_id& type) -> void {
        zone_manager::get_manager().add(type.str(), type, your_fac, false, true, point, point);
    }
    auto custom(const tripoint_abs_ms& point, const std::string& json) -> void {
        auto options = make_shared_fast<loot_options>();
        auto stream = std::istringstream(json);
        auto input = JsonIn(stream);
        options->deserialize(input.get_object());
        zone_manager::get_manager().add(
            "Custom", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, point, point, options);
    }
    auto sort() -> void {
        auto& actor = get_avatar();
        actor.assign_activity(activity_id("ACT_MOVE_LOOT"));
        process_activity(actor);
    }
};
} // namespace

TEST_CASE(
    "zone_sorting_moves_items_with_capacity_and_overlap_constraints", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& actor = get_avatar();
    const auto source = actor.abs_pos();
    const auto fallback = source + tripoint_rel_ms(0, 2, 0);
    const auto specialized = source + tripoint_rel_ms(4, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.custom(fallback, R"({"categories":["LOOT_WORKSHOP_TOOLS"]})");
    fixture.add(specialized, zone_type_id("LOOT_WORKSHOP_TOOLS"));
    auto expected = specialized;
    auto scenario = std::string();
    SECTION("specialized_storage_wins") { scenario = "specialized_storage_wins"; }
    SECTION("full_specialized_storage_falls_back") {
        scenario = "full_specialized_storage_falls_back";
        auto rocks = item::spawn("test_rock");
        rocks->charges = 1000000;
        here.add_item(abs_to_bub(specialized), std::move(rocks));
        expected = fallback;
    }

    SECTION("overlapping_nonmatching_custom_zone_at_source_does_not_trap_items") {
        scenario = "overlapping_nonmatching_custom_zone_at_source_does_not_trap_items";
        fixture.custom(source, R"({"mark":"rock"})");
        here.ter_set(abs_to_bub(specialized), ter_id("t_wall"));
        expected = fallback;
    }

    SECTION("overlapping_filters_accept_union_of_matches") {
        scenario = "overlapping_filters_accept_union_of_matches";
        fixture.custom(specialized, R"({"mark":"rock"})");
        fixture.custom(specialized, R"({"mark":"hammer"})");
    }
    SECTION("accessible_manual_filter_takes_precedence") {
        scenario = "accessible_manual_filter_takes_precedence";
        fixture.custom(fallback, R"({"mark":"hammer"})");
        expected = fallback;
    }


    SECTION("already_sorted_item_stays_in_full_specialized_source") {
        scenario = "already_sorted_item_stays_in_full_specialized_source";
        fixture.add(source, zone_type_id("LOOT_WORKSHOP_TOOLS"));
        auto rocks = item::spawn("test_rock");
        rocks->charges = 1000000;
        here.add_item(abs_to_bub(source), std::move(rocks));
        expected = source;
    }
    SECTION("no_available_storage_leaves_item_at_source") {
        scenario = "no_available_storage_leaves_item_at_source";
        here.ter_set(abs_to_bub(specialized), ter_id("t_wall"));
        here.ter_set(abs_to_bub(fallback), ter_id("t_wall"));
        expected = source;
    }
    here.add_item(abs_to_bub(source), item::spawn("test_zone_hammer"));
    CAPTURE(scenario);
    fixture.sort();
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(expected)), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_hammer");
    }));
    if (expected != source) { CHECK(here.i_at(abs_to_bub(source)).empty()); }
}

TEST_CASE(
    "legacy_zones_keep_names_filters_coordinates_and_behavior", "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    auto stream = std::istringstream(R"([
        {"name":"Old tools","type":"LOOT_WORKSHOP_TOOLS","invert":false,"enabled":true,
         "start_x":61,"start_y":60,"start_z":0,"end_x":61,"end_y":60,"end_z":0},
        {"name":"My old filter","type":"LOOT_CUSTOM","invert":false,"enabled":true,
         "start":[62,60,0],"end":[62,60,0],"mark":"hammer"}
    ])");
    auto input = JsonIn(stream);
    manager.deserialize(input);
    manager.cache_data();
    auto output = std::ostringstream();
    auto json = JsonOut(output);
    manager.serialize(json);
    auto saved = std::istringstream(output.str());
    auto parsed = JsonIn(saved);
    const auto zones = parsed.get_array();
    REQUIRE(zones.size() == 2);
    const auto old_tools = zones.get_object(0);
    const auto old_filter = zones.get_object(1);
    old_tools.allow_omitted_members();
    old_filter.allow_omitted_members();
    CHECK(old_tools.get_string("name") == "Old tools");
    CHECK(old_filter.get_string("mark") == "hammer");
    CHECK_FALSE(old_filter.has_member("categories"));
    auto reloaded = std::istringstream(output.str());
    auto reload_input = JsonIn(reloaded);
    manager.deserialize(reload_input);
    const auto custom = manager.get_zones(zone_type_id("LOOT_CUSTOM"), tripoint_abs_ms(62, 60, 0));
    REQUIRE(custom.size() == 1);
    const auto& options = dynamic_cast<const loot_options&>(custom.front().get_options());
    CHECK(options.matches(*item::spawn_temporary("test_zone_hammer")));
    CHECK_FALSE(options.matches(*item::spawn_temporary("test_rock")));
}

TEST_CASE(
    "vehicle_category_storage_moves_items_and_falls_back_when_full", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos();
    const auto cargo_point = source + tripoint_rel_ms(3, 0, 0);
    const auto fallback = source + tripoint_rel_ms(0, 2, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.custom(fallback, R"({"categories":["LOOT_WORKSHOP_TOOLS"]})");
    auto* cart =
        here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(cargo_point), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(abs_to_bub(cargo_point)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    const auto part = cargo->part_index();
    cart->get_items(part).clear();
    auto zone = zone_data(
        "Workshop cart", zone_type_id("LOOT_WORKSHOP_TOOLS"), your_fac, false, true, cargo_point,
        cargo_point);
    zone.set_dimension(here.get_bound_dimension());
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    auto full = false;
    SECTION("available_vehicle_storage") {}
    SECTION("cargo_over_terrain_that_rejects_items") {
        here.furn_set(abs_to_bub(cargo_point), furn_id("f_safe_c"));
        REQUIRE_FALSE(here.can_put_items_ter_furn(abs_to_bub(cargo_point)));
        const auto candidate = item::spawn_temporary("test_zone_hammer");
        CHECK(zone_sorting_destination(*candidate, source, 10) == cargo_point);
    }
    SECTION("full_vehicle_storage_uses_ground_fallback") {
        const auto rock_volume = item::spawn_temporary("test_rock")->volume();
        while (cart->free_volume(part) >= rock_volume) {
            REQUIRE_FALSE(cart->add_item(part, item::spawn("test_rock")));
        }
        REQUIRE(cart->free_volume(part) < item::spawn_temporary("test_zone_hammer")->volume());
        full = true;
    }
    SECTION("full_ground_beneath_vehicle_does_not_block_cargo") {
        for (auto i = 0; i < MAX_ITEM_IN_SQUARE; ++i) {
            here.add_item(abs_to_bub(cargo_point), item::spawn("test_rock"));
        }
        REQUIRE(here.i_at(abs_to_bub(cargo_point)).size() == MAX_ITEM_IN_SQUARE);
    }
    SECTION("moving_vehicle_updates_destination") {
        here.displace_vehicle(*cart, tripoint_rel_ms(0, 1, 0));
        manager.cache_vzones();
        CHECK_FALSE(manager.has(zone_type_id("LOOT_WORKSHOP_TOOLS"), cargo_point));
    }
    here.add_item(abs_to_bub(source), item::spawn("test_zone_hammer"));
    fixture.sort();
    CHECK(here.i_at(abs_to_bub(source)).empty());
    const auto cargo_has_hammer = std::ranges::any_of(cart->get_items(part), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_hammer");
    });
    CHECK(cargo_has_hammer == !full);
    CHECK(
        std::ranges::any_of(
            here.i_at(abs_to_bub(fallback)),
            [](const auto& it) { return it->typeId() == itype_id("test_zone_hammer"); })
        == full);
}

TEST_CASE(
    "mod_storage_without_group_or_loot_prefix_remains_selectable",
    "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();

    const auto id = zone_type_id("test_mod_parts_storage");
    CHECK(std::ranges::contains(zone_storage_categories(), id));
    auto type = itype(*item::spawn_temporary("test_zone_hammer")->type);
    type.category_force = item_category_id("test_mod_parts");
    auto candidate = item::spawn(&type);
    auto options = loot_options();
    options.set_categories({id});
    CHECK(options.matches(*candidate));
    const auto destination = get_avatar().abs_pos() + tripoint_rel_ms(2, 0, 0);
    fixture.add(destination, id);
    CHECK(zone_sorting_destination(*candidate, get_avatar().abs_pos(), 10) == destination);
}

TEST_CASE(
    "sorting_takes_items_from_vehicle_source_into_category_storage", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos() + tripoint_rel_ms(1, 0, 0);
    const auto destination = get_avatar().abs_pos() + tripoint_rel_ms(0, 2, 0);
    fixture.custom(destination, R"({"categories":["LOOT_WORKSHOP_TOOLS"]})");
    auto* cart = here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(source), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(abs_to_bub(source)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    cart->get_items(cargo->part_index()).clear();
    REQUIRE_FALSE(cart->add_item(cargo->part_index(), item::spawn("test_zone_hammer")));
    auto zone = zone_data(
        "Unsorted cart", zone_type_id("LOOT_UNSORTED"), your_fac, false, true, source, source);
    zone.set_dimension(here.get_bound_dimension());
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    fixture.sort();
    CHECK(cart->get_items(cargo->part_index()).empty());
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(destination)), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_hammer");
    }));
}

TEST_CASE(
    "vehicle_custom_category_filter_survives_save_and_accepts_matching_items",
    "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos();
    const auto destination = source + tripoint_rel_ms(3, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    auto* cart =
        here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(destination), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(abs_to_bub(destination)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    cart->get_items(cargo->part_index()).clear();
    auto options = make_shared_fast<loot_options>();
    options->set_categories({zone_type_id("LOOT_WORKSHOP_TOOLS")});
    auto zone = zone_data(
        "Category cart", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, destination,
        destination, options);
    zone.set_dimension(here.get_bound_dimension());
    auto saved = std::ostringstream();
    auto output = JsonOut(saved);
    zone.serialize(output);
    auto stream = std::istringstream(saved.str());
    auto input = JsonIn(stream);
    zone.deserialize(input);
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    CHECK(manager.custom_loot_has(destination, item::spawn_temporary("test_zone_hammer")));
    CHECK_FALSE(manager.custom_loot_has(destination, item::spawn_temporary("test_rock")));
    here.add_item(abs_to_bub(source), item::spawn("test_zone_hammer"));
    here.add_item(abs_to_bub(source), item::spawn("test_rock"));
    fixture.sort();
    CHECK(std::ranges::any_of(cart->get_items(cargo->part_index()), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_hammer");
    }));
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(source)), [](const auto& it) {
        return it->typeId() == itype_id("test_rock");
    }));
}

TEST_CASE(
    "missing_mod_category_is_preserved_without_becoming_catch_all",
    "[zone_ui][zone_sorting][json]") {
    auto stream = std::istringstream(R"({"categories":["removed_mod_storage"]})");
    auto input = JsonIn(stream);
    auto options = loot_options();
    options.deserialize(input.get_object());
    CHECK_FALSE(options.matches(*item::spawn_temporary("test_zone_hammer")));
    CHECK_FALSE(options.matches(*item::spawn_temporary("test_rock")));
    REQUIRE(options.get_descriptions().size() == 1);
    CHECK(options.get_descriptions().front().second == "removed_mod_storage");
    auto saved = std::ostringstream();
    auto output = JsonOut(saved);
    output.start_object();
    options.serialize(output);
    output.end_object();
    CHECK(saved.str().find("removed_mod_storage") != std::string::npos);
}

TEST_CASE("narrow_category_storage_precedes_general_storage", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& actor = get_avatar();
    const auto source = actor.abs_pos();
    auto broad = source + tripoint_rel_ms(2, 0, 0);
    auto narrow = source + tripoint_rel_ms(0, 2, 0);
    auto expected = narrow;
    SECTION("first_placement") {}
    SECTION("reverse_placement") {
        std::swap(broad, narrow);
        expected = narrow;
    }
    SECTION("full_narrow_storage_uses_general_storage") {
        auto rocks = item::spawn("test_rock");
        rocks->charges = 1000000;
        get_map().add_item(abs_to_bub(narrow), std::move(rocks));
        expected = broad;
    }
    SECTION("blocked_narrow_storage_uses_general_storage") {
        get_map().ter_set(abs_to_bub(narrow), ter_id("t_wall"));
        expected = broad;
    }
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.custom(narrow, R"({"categories":["LOOT_WORKSHOP_TOOLS"]})");
    fixture.custom(
        broad,
        R"({"categories":["LOOT_WORKSHOP_TOOLS","LOOT_ENTRY_TOOLS","LOOT_COOKING_TOOLS","LOOT_CHEMISTRY_TOOLS","LOOT_FARM_TOOLS","LOOT_TOOLS"]})");
    const auto hammer = item::spawn_temporary("test_zone_hammer");
    CHECK(zone_sorting_destination(*hammer, actor.abs_pos(), 10) == expected);
    get_map().add_item(abs_to_bub(source), item::spawn("test_zone_hammer"));
    fixture.sort();
    CHECK(std::ranges::any_of(get_map().i_at(abs_to_bub(expected)), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_hammer");
    }));
}

TEST_CASE("mod_priority_storage_is_discovered_and_matches", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();

    auto type = itype(*item::spawn_temporary("test_zone_hammer")->type);
    type.category_force = item_category_id("test_priority_category");
    auto candidate = item::spawn(&type);
    candidate->set_flag(flag_id("FIREWOOD"));
    REQUIRE(candidate->get_category().priority_zone(*candidate)
            == zone_type_id("test_priority_storage"));
    CHECK(std::ranges::contains(zone_storage_categories(), zone_type_id("test_priority_storage")));
    auto options = loot_options();
    options.set_categories({zone_type_id("test_priority_storage")});
    CHECK(options.matches(*candidate));
}


TEST_CASE("edited_category_storage_sorts_after_reload", "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    auto& here = get_map();
    const auto source = get_avatar().abs_pos();
    const auto destination = source + tripoint_rel_ms(3, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    auto options = make_shared_fast<loot_options>();
    options->set_categories({zone_type_id("LOOT_BOOKS")});
    manager.add("Storage", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, destination,
                destination, options);
    options->set_categories({zone_type_id("LOOT_WORKSHOP_TOOLS")});
    auto saved = std::ostringstream();
    auto json = JsonOut(saved);
    manager.serialize(json);
    auto stream = std::istringstream(saved.str());
    auto input = JsonIn(stream);
    manager.deserialize(input);
    manager.cache_data();
    here.add_item(abs_to_bub(source), item::spawn("test_zone_hammer"));
    here.add_item(abs_to_bub(source), item::spawn("test_zone_book"));
    fixture.sort();
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(destination)), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_hammer");
    }));
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(source)), [](const auto& it) {
        return it->typeId() == itype_id("test_zone_book");
    }));
}

TEST_CASE("manual_filter_edit_and_favorite_rule_are_independent", "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos();
    const auto other_source = source + tripoint_rel_ms(1, 0, 0);
    const auto destination = source + tripoint_rel_ms(4, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.add(other_source, zone_type_id("LOOT_UNSORTED"));
    fixture.add(source, zone_type_id("LOOT_IGNORE_FAVORITES"));
    auto options = make_shared_fast<loot_options>();
    auto initial = std::istringstream(R"({"mark":"rock"})");
    auto initial_input = JsonIn(initial);
    options->deserialize(initial_input.get_object());
    manager.add("Manual", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, destination,
                destination, options);
    auto edited = std::istringstream(R"({"mark":"hammer"})");
    auto edited_input = JsonIn(edited);
    options->deserialize(edited_input.get_object());
    auto saved = std::ostringstream();
    auto json = JsonOut(saved);
    manager.serialize(json);
    auto stream = std::istringstream(saved.str());
    auto input = JsonIn(stream);
    manager.deserialize(input);
    manager.cache_data();
    for (const auto point : {source, other_source}) {
        auto hammer = item::spawn("test_zone_hammer");
        hammer->is_favorite = true;
        here.add_item(abs_to_bub(point), std::move(hammer));
    }
    fixture.sort();
    CHECK(here.i_at(abs_to_bub(source)).size() == 1);
    CHECK(here.i_at(abs_to_bub(other_source)).empty());
    CHECK(here.i_at(abs_to_bub(destination)).size() == 1);
}

TEST_CASE("empty_category_storage_does_not_become_manual_filter", "[zone_ui][json]") {
    auto options = loot_options();
    options.set_categories({});
    auto output = std::ostringstream();
    auto json = JsonOut(output);
    json.start_object();
    options.serialize(json);
    json.end_object();
    auto stream = std::istringstream(output.str());
    auto input = JsonIn(stream);
    auto restored = loot_options();
    restored.deserialize(input.get_object());
    CHECK(restored.uses_categories());
    CHECK_FALSE(restored.matches(*item::spawn_temporary("test_zone_hammer")));
}

TEST_CASE("category_storage_honors_mod_food_mapping", "[zone_ui][zone_sorting]") {
    auto& category = const_cast<item_category&>(item_category_id("food").obj());
    const auto original = category;
    const auto restore = on_out_of_scope([&]() { category = original; });
    auto stream = std::istringstream(
        R"({"id":"food","name":"Food","sort_rank":0,"zone":"test_mod_parts_storage"})");
    auto input = JsonIn(stream);
    category.load(input.get_object(), "test_data");
    auto options = loot_options();
    options.set_categories({zone_type_id("test_mod_parts_storage")});
    const auto apple = item::spawn_temporary("test_zone_food");
    REQUIRE(apple->get_category().get_id() == item_category_id("food"));
    CHECK(options.matches(*apple));
}

TEST_CASE("manual_zones_keep_type_and_filter_edit_actions", "[zone_ui]") {
    for (const auto enabled : {false, true}) {
        const auto option = override_option("USE_ZONE_MANAGER", enabled ? "true" : "false");
        CAPTURE(enabled);
        auto options = make_shared_fast<loot_options>();
        auto stream = std::istringstream(R"({"mark":"hammer"})");
        auto input = JsonIn(stream);
        options->deserialize(input.get_object());
        const auto zone = zone_data(
            "Old manual storage", zone_type_id("LOOT_CUSTOM"), your_fac, false, true,
            tripoint_abs_ms::zero(), tripoint_abs_ms::zero(), options);
        const auto entries = zone_edit_entries(zone);
        const auto change_type = std::ranges::find(entries, 2, &uilist_entry::retval);
        REQUIRE(change_type != entries.end());
        CHECK(change_type->enabled);
        const auto edit_filter = std::ranges::find(entries, 3, &uilist_entry::retval);
        REQUIRE(edit_filter != entries.end());
        CHECK(edit_filter->enabled);
        CHECK(options->get_descriptions().front().second == "hammer");
        CHECK_FALSE(options->uses_categories());
    }
}

TEST_CASE("area_rule_headings_cannot_toggle_all_rules", "[zone_ui]") {
    const auto rows = make_zone_checklist_rows(false);
    for (const auto& row : rows) {
        if (row.rule && !row.single_type) {
            CAPTURE(row.label);
            CHECK(row.members.size() <= 1);
        }
    }
    for (const auto& id : zone_creation_rules()) {
        CAPTURE(id.str());
        CHECK(
            std::ranges::count_if(
                rows,
                [&](const auto& row) {
                    return row.rule && !row.single_type
                        && row.members == std::vector<zone_type_id>{id};
                })
            == 1);
    }
    const auto categories = make_zone_checklist_rows(true);
    CHECK(std::ranges::none_of(categories, &zone_checklist_row::rule));
}

TEST_CASE("zone_sidebar_shortcuts_use_green_keys", "[zone_ui]") {
    const auto labels = zone_shortcut_labels();
    CHECK(labels[4][0] == shortcut_text(c_light_green, _("<O> - Toggle Overlays")));
    CHECK(labels[4][1] == shortcut_text(c_light_green, _("<G> - Submap grid")));
}

TEST_CASE("category_storage_fails_closed_in_legacy_versions", "[zone_ui][json]") {
    for (const auto& selected :
         {std::vector<zone_type_id>{},
          std::vector<zone_type_id>{zone_type_id("LOOT_WORKSHOP_TOOLS")},
          std::vector<zone_type_id>{zone_type_id("removed_mod_storage")}}) {
        auto options = loot_options();
        options.set_categories(selected);
        auto output = std::ostringstream();
        auto json = JsonOut(output);
        json.start_object();
        options.serialize(json);
        json.end_object();
        auto stream = std::istringstream(output.str());
        auto input = JsonIn(stream);
        const auto saved = input.get_object();
        REQUIRE(saved.has_array("categories"));
        REQUIRE(saved.has_string("mark"));
        const auto fallback = saved.get_string("mark");
        REQUIRE_FALSE(fallback.empty());
        const auto legacy_filter = item_filter_from_string(fallback);
        for (const auto id :
             {"test_zone_hammer", "test_rock", "test_zone_drink", "test_zone_book"}) {
            CAPTURE(id, fallback);
            CHECK_FALSE(legacy_filter(*item::spawn_temporary(id)));
        }
        auto restored = loot_options();
        restored.deserialize(saved);
        CHECK(restored.uses_categories());
        CHECK(restored.matches(*item::spawn_temporary("test_zone_hammer"))
              == (!selected.empty() && selected.front() == zone_type_id("LOOT_WORKSHOP_TOOLS")));
        // An old version writes only mark back; upgrading again must remain safe.
        auto legacy_output = std::ostringstream();
        auto legacy_json = JsonOut(legacy_output);
        legacy_json.start_object();
        legacy_json.member("mark", fallback);
        legacy_json.end_object();
        auto legacy_stream = std::istringstream(legacy_output.str());
        auto legacy_input = JsonIn(legacy_stream);
        auto after_downgrade = loot_options();
        after_downgrade.deserialize(legacy_input.get_object());
        CHECK_FALSE(after_downgrade.uses_categories());
        CHECK_FALSE(after_downgrade.matches(*item::spawn_temporary("test_zone_hammer")));
        CHECK_FALSE(after_downgrade.matches(*item::spawn_temporary("test_rock")));
    }
}

TEST_CASE("confirming_native_storage_keeps_food_fallback", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    const auto origin = get_avatar().abs_pos();
    const auto destination = origin + tripoint_rel_ms(2, 0, 0);
    for (const auto& type : zone_storage_categories()) {
        CAPTURE(type.str());
        CHECK(zone_storage_type_after_edit(type, {type}) == type);
    }
    for (const auto& type : {zone_type_id("LOOT_FOOD"), zone_type_id("LOOT_DRINK")}) {
        const auto kept_type = zone_storage_type_after_edit(type, {type});
        const auto candidate = item::spawn_temporary(
            type == zone_type_id("LOOT_FOOD") ? "test_zone_food" : "test_zone_perishable_drink");
        fixture.add(destination, kept_type);
        CHECK(manager.get_near_zone_type_for_item(*candidate, origin, 10) == type);
    }
    CHECK(zone_storage_type_after_edit(zone_type_id("LOOT_FOOD"), {zone_type_id("LOOT_BOOKS")})
          == zone_type_id("LOOT_CUSTOM"));
    CHECK(zone_storage_type_after_edit(
              zone_type_id("LOOT_FOOD"), {zone_type_id("LOOT_FOOD"), zone_type_id("LOOT_BOOKS")})
          == zone_type_id("LOOT_CUSTOM"));
}

TEST_CASE(
    "activity_storage_keeps_native_types_and_mixed_storage_actions", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    const auto origin = get_avatar().abs_pos();
    const auto point = origin + tripoint_rel_ms(2, 0, 0);
    for (const auto& type :
         {zone_type_id("LOOT_CORPSE"), zone_type_id("LOOT_WOOD"), zone_type_id("LOOT_SEEDS")}) {
        CAPTURE(type.str());
        REQUIRE(zone_storage_type({type}) == type);
        CHECK(zone_storage_type_after_edit(zone_type_id("LOOT_FOOD"), {type}) == type);
        const auto selected = std::vector<zone_type_id>{type, zone_type_id("LOOT_BOOKS")};
        REQUIRE(zone_storage_type(selected) == zone_type_id("LOOT_CUSTOM"));
        auto options = make_shared_fast<loot_options>();
        options->set_categories(selected);
        manager.add("Mixed activity storage", zone_type_id("LOOT_CUSTOM"), your_fac, false, true,
                    point, point, options);
        REQUIRE(zone_activity_points(type, origin, 60).contains(point));
        CHECK(zone_activity_at(point, type) != nullptr);
        // Activity discovery must not promote a general storage zone to a specialized destination.
        CHECK(manager.get_near(type, origin, 60).empty());
        CHECK(zone_activity_points(type, origin, 1).empty());
        auto& zone = manager.get_zones().back().get();
        zone.set_enabled(false);
        manager.cache_data();
        CHECK(zone_activity_points(type, origin, 60).empty());
        CHECK(zone_activity_at(point, type) == nullptr);
    }
}

TEST_CASE("activity_storage_respects_faction_and_manual_filters", "[zone_ui]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    const auto point = get_avatar().abs_pos();
    fixture.custom(point, R"({"mark":"log"})");
    auto options = make_shared_fast<loot_options>();
    options->set_categories({zone_type_id("LOOT_WOOD")});
    manager.add("Other faction", zone_type_id("LOOT_CUSTOM"), faction_id("other_faction"), false,
                true, point, point, options);
    CHECK(zone_activity_points(zone_type_id("LOOT_WOOD"), point).empty());
    CHECK(zone_activity_at(point, zone_type_id("LOOT_WOOD")) == nullptr);
    CHECK(zone_activity_at(point, zone_type_id("LOOT_BOOKS")) == nullptr);
}

TEST_CASE("vehicle_category_storage_is_visible_to_activities", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto origin = get_avatar().abs_pos();
    const auto point = origin + tripoint_rel_ms(3, 0, 0);
    auto* cart = here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(point), 0_degrees, 0, 0);
    REQUIRE(cart);
    const auto cargo = here.veh_at(abs_to_bub(point)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    auto options = make_shared_fast<loot_options>();
    options->set_categories({zone_type_id("LOOT_CORPSE"), zone_type_id("LOOT_BOOKS")});
    auto zone = zone_data(
        "Mixed cart", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, point, point, options);
    zone.set_dimension(here.get_bound_dimension());
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    REQUIRE(zone_activity_points(zone_type_id("LOOT_CORPSE"), origin).contains(point));
    CHECK(zone_activity_at(point, zone_type_id("LOOT_CORPSE")) != nullptr);
    here.displace_vehicle(*cart, tripoint_rel_ms(0, 1, 0));
    manager.cache_vzones();
    const auto moved = cart->abs_part_location(cargo->part_index());
    REQUIRE(moved != point);
    CHECK_FALSE(zone_activity_points(zone_type_id("LOOT_CORPSE"), origin).contains(point));
    CHECK(zone_activity_points(zone_type_id("LOOT_CORPSE"), origin).contains(moved));
    CHECK(zone_activity_at(moved, zone_type_id("LOOT_CORPSE")) != nullptr);
}

TEST_CASE("plank_cutting_uses_mixed_category_storage", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    const auto saved_time = calendar::turn;
    const auto restore_time = on_out_of_scope([&]() { set_time(saved_time); });
    set_time(calendar::turn_zero + 12_hours);
    auto& actor = get_avatar();
    auto& here = get_map();
    const auto source = actor.abs_pos();
    fixture.custom(source, R"({"categories":["LOOT_WOOD","LOOT_BOOKS"]})");
    actor.i_add(item::spawn("hatchet"));
    here.add_item(abs_to_bub(source), item::spawn("log"));
    actor.assign_activity(activity_id("ACT_MULTIPLE_CHOP_PLANKS"));
    process_activity(actor);
    CHECK(std::ranges::none_of(here.i_at(abs_to_bub(source)), [](const auto& entry) {
        return entry->typeId() == itype_id("log");
    }));
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(source)), [](const auto& entry) {
        return entry->typeId() == itype_id("2x4") || entry->typeId() == itype_id("splinter");
    }));
}

TEST_CASE("activity_search_clips_storage_to_radius_and_level", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    const auto origin = get_avatar().abs_pos();
    auto options = make_shared_fast<loot_options>();
    options->set_categories({zone_type_id("LOOT_SEEDS"), zone_type_id("LOOT_BOOKS")});
    manager.add("Wide storage", zone_type_id("LOOT_CUSTOM"), your_fac, false, true,
                origin + tripoint_rel_ms(20, 20, 1), origin - tripoint_rel_ms(20, 20, 1), options);
    const auto type = zone_type_id("LOOT_SEEDS");
    const auto points = zone_activity_points(type, origin, 2);
    REQUIRE(points.size() == 25);
    for (const auto& point : points) {
        CHECK(point.z() == origin.z());
        CHECK(square_dist(point, origin) <= 2);
    }
    CHECK(zone_activity_points(type, origin, 0) == std::unordered_set<tripoint_abs_ms>{origin});
    CHECK(zone_activity_points(type, origin, -1).empty());
    CHECK(zone_activity_points(type, origin + tripoint_rel_ms(1000, 0, 0), 2).empty());
    CHECK(zone_activity_points(type, origin + tripoint_rel_ms(0, 0, 2), 2).empty());
}

TEST_CASE("zone_checklist_search_excludes_group_toggles", "[zone_ui]") {
    // Initialize real menu state without drawing a window or querying input.
    const auto make_menu = []() {
        const auto restore = restore_on_out_of_scope<bool>(test_mode);
        test_mode = false;
        return uilist();
    };
    const auto restore_language = on_out_of_scope([]() { set_language(); });
    const auto language = override_option("USE_LANG", "en_US");
    set_language();
    for (const auto categories_only : {false, true}) {
        CAPTURE(categories_only);
        const auto rows = make_zone_checklist_rows(categories_only);
        auto selection = zone_category_selection();
        auto menu = make_menu();
        for (auto i = 0; i < static_cast<int>(rows.size()); ++i) {
            menu.entries.push_back(make_zone_checklist_entry(rows[i], selection, i));
        }
        menu.set_filter("Books");
        const auto visible = menu.get_filtered();
        REQUIRE_FALSE(visible.empty());
        for (const auto index : visible) { CHECK(rows[index].members.size() == 1); }
        const auto& row = rows[visible.front()];
        CHECK(row.members == std::vector<zone_type_id>{zone_type_id("LOOT_BOOKS")});
        REQUIRE(menu.selected == visible.front());
        toggle_zone_categories(selection.categories, rows[menu.selected].members);
        CHECK(selection.categories == std::vector<zone_type_id>{zone_type_id("LOOT_BOOKS")});
        CHECK_FALSE(std::ranges::contains(selection.categories, zone_type_id("LOOT_MAP")));

        menu.set_filter("Books and maps");
        CHECK(menu.get_filtered().empty());
        CHECK(menu.selected == -1);
        menu.set_filter("Maps");
        REQUIRE(menu.get_filtered().size() == 1);
        CHECK(rows[menu.selected].members == std::vector<zone_type_id>{zone_type_id("LOOT_MAP")});
        menu.set_filter("Corpses");
        REQUIRE(menu.get_filtered().size() == 1);
        CHECK(
            rows[menu.selected].members == std::vector<zone_type_id>{zone_type_id("LOOT_CORPSE")});
        menu.clear_filter();
        CHECK(menu.get_filtered().size() == rows.size());
    }
    auto standard_menu = make_menu();
    standard_menu.addentry("Books and maps");
    standard_menu.set_filter("Books");
    CHECK(standard_menu.get_filtered().size() == 1);
}

TEST_CASE("zone_category_display_limit_does_not_change_storage", "[zone_ui][json]") {
    const auto limit = override_option("ZONE_CATEGORY_DISPLAY_LIMIT", "10");
    auto selected = zone_storage_categories();
    std::erase(selected, zone_type_id("LOOT_BOOKS"));
    REQUIRE(selected.size() >= 10);
    selected.resize(10);
    auto first_ten = std::string();
    for (const auto& id : selected) { first_ten += (first_ten.empty() ? "" : ", ") + id->name(); }
    auto options = loot_options();
    options.set_categories(selected);
    CHECK(options.get_descriptions().front().second == first_ten);
    selected.push_back(zone_type_id("LOOT_BOOKS"));
    options.set_categories(selected);
    const auto book = item::spawn_temporary("test_zone_book");
    REQUIRE(options.matches(*book));
    auto before = std::ostringstream();
    auto before_json = JsonOut(before);
    before_json.start_object();
    options.serialize(before_json);
    before_json.end_object();
    CHECK(options.get_descriptions().front().second == first_ten + ", " + _("and others"));
    {
        const auto one = override_option("ZONE_CATEGORY_DISPLAY_LIMIT", "1");
        CHECK(options.get_descriptions().front().second
              == selected.front()->name() + ", " + _("and others"));
        CHECK(options.matches(*book));
        auto manual = loot_options();
        auto text = std::istringstream(R"({"mark":"c:books,c:food,m:wood"})");
        auto input = JsonIn(text);
        manual.deserialize(input.get_object());
        CHECK(manual.get_descriptions().front().second == "c:books,c:food,m:wood");
    }
    {
        const auto unlimited = override_option("ZONE_CATEGORY_DISPLAY_LIMIT", "0");
        CHECK(options.get_descriptions().front().second
              == first_ten + ", " + zone_type_id("LOOT_BOOKS")->name());
    }
    auto after = std::ostringstream();
    auto after_json = JsonOut(after);
    after_json.start_object();
    options.serialize(after_json);
    after_json.end_object();
    CHECK(after.str() == before.str());
    CHECK(options.matches(*book));
    options.set_categories({});
    CHECK(options.get_descriptions().front().second.empty());
}

TEST_CASE("zone_suggested_names_describe_small_selections", "[zone_ui]") {
    const auto restore_language = on_out_of_scope([]() { set_language(); });
    const auto language = override_option("USE_LANG", "en_US");
    set_language();
    CHECK(zone_selection_name({zone_type_id("LOOT_COOKING_TOOLS")}) == "Cooking Tools");
    CHECK(zone_selection_name({zone_type_id("LOOT_WEAPONS"), zone_type_id("LOOT_GUNS")})
          == "Guns and Weapons");
    CHECK(zone_selection_name(
              {zone_type_id("LOOT_AMMO"), zone_type_id("LOOT_WEAPONS"), zone_type_id("LOOT_GUNS")})
          == "Guns, Weapons, Ammo");
    CHECK(
        zone_selection_name(
            {zone_type_id("LOOT_AMMO"), zone_type_id("LOOT_WEAPONS"), zone_type_id("LOOT_GUNS"),
             zone_type_id("LOOT_MAGAZINES")})
        == "Weapons and ammunition");
    CHECK(
        zone_selection_name(
            {zone_type_id("LOOT_FOOD"), zone_type_id("LOOT_WORKSHOP_TOOLS"),
             zone_type_id("LOOT_ENTRY_TOOLS"), zone_type_id("LOOT_COOKING_TOOLS"),
             zone_type_id("LOOT_CHEMISTRY_TOOLS")})
        == "Food and Tools");
    CHECK(
        zone_selection_name(
            {zone_type_id("LOOT_FOOD"), zone_type_id("LOOT_PFOOD"),
             zone_type_id("LOOT_WORKSHOP_TOOLS"), zone_type_id("LOOT_COOKING_TOOLS")})
        == "Food, Perishable food, Workshop Tools, Cooking Tools");
    CHECK(zone_selection_name({zone_type_id("missing_mod_zone")}) == "missing_mod_zone");
    CHECK(zone_selection_name({}) == "Category storage");
}
