#include "active_item_cache.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

#include <utility>

TEST_CASE("active_item_cache_ignores_expired_references", "[item]") {
    auto cache = active_item_cache();
    {
        auto active = item::
            spawn("firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
        active->activate();
        cache.add(*active);
        REQUIRE_FALSE(cache.empty());
    }
    CHECK(cache.empty());
}

TEST_CASE("active_item_cache_tracks_bionic_scannable_corpses", "[item]") {
    auto cache = active_item_cache();
    auto corpse = item::make_corpse(mtype_id("mon_zombie_soldier"), calendar::turn, "");
    REQUIRE(corpse->is_corpse());
    REQUIRE(corpse->needs_processing());

    cache.add(*corpse);
    auto scannable_corpses = cache.get_special(special_item_type::bionic_scannable_corpse);
    REQUIRE(scannable_corpses.size() == 1);
    CHECK(scannable_corpses.front() == corpse.get());

    cache.remove(corpse.get());
    CHECK(cache.get_special(special_item_type::bionic_scannable_corpse).empty());
}

TEST_CASE("nonperishable_food_does_not_enter_active_item_cache", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();
    here.clear_items(test_origin);
    const auto baseline_active_submaps = here.get_submaps_with_active_items();

    auto sugar = item::spawn("sugar");
    REQUIRE(sugar->is_food());
    REQUIRE_FALSE(sugar->goes_bad());
    REQUIRE_FALSE(sugar->needs_processing());
    here.add_item(test_origin, std::move(sugar));
    CHECK(here.get_submaps_with_active_items() == baseline_active_submaps);

    auto sashimi = item::spawn("sashimi");
    REQUIRE(sashimi->is_food());
    REQUIRE(sashimi->goes_bad());
    REQUIRE(sashimi->needs_processing());
    here.add_item(test_origin, std::move(sashimi));
    CHECK(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));
}

TEST_CASE("pointer_item_removal_updates_active_item_cache", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();
    here.clear_items(test_origin);

    auto active =
        item::spawn("firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
    active->activate();
    REQUIRE(active->needs_processing());
    auto* const active_ptr = &*active;

    here.add_item(test_origin, std::move(active));
    CHECK(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));

    auto removed = here.remove_item(test_origin, active_ptr);
    REQUIRE(removed != nullptr);
    CHECK_FALSE(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));
    CHECK(get_map().check_submap_active_item_consistency().empty());
}

TEST_CASE("stack_iterator_item_removal_updates_active_item_cache", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();
    here.clear_items(test_origin);

    auto active =
        item::spawn("firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
    active->activate();
    REQUIRE(active->needs_processing());
    auto* const active_ptr = &*active;

    here.add_item(test_origin, std::move(active));
    REQUIRE(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));

    auto removed = here.remove_item(test_origin, active_ptr);
    REQUIRE(removed != nullptr);
    CHECK_FALSE(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));
    CHECK(get_map().check_submap_active_item_consistency().empty());
}

TEST_CASE("stack_clear_updates_active_item_cache", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();
    here.clear_items(test_origin);

    auto active =
        item::spawn("firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
    active->activate();
    REQUIRE(active->needs_processing());

    here.add_item(test_origin, std::move(active));
    REQUIRE(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));

    auto removed = here.clear_items(test_origin);
    REQUIRE(removed.size() == 1);
    CHECK_FALSE(here.get_submaps_with_active_items().contains(project_to<coords::sm>(test_origin)));
    CHECK(get_map().check_submap_active_item_consistency().empty());
}

TEST_CASE("nested_processing_flag_changes_invalidate_container_cache", "[item]") {
    clear_all_state();

    auto backpack = item::spawn("backpack");
    backpack->put_in(item::spawn("rock"));
    item& stored = backpack->contents.front();

    REQUIRE_FALSE(backpack->needs_processing());

    stored.set_flag(flag_RADIO_ACTIVATION);
    CHECK(backpack->needs_processing());

    stored.unset_flag(flag_RADIO_ACTIVATION);
    CHECK_FALSE(backpack->needs_processing());

    stored.set_flag(flag_ETHEREAL_ITEM);
    CHECK(backpack->needs_processing());

    stored.unset_flags();
    CHECK_FALSE(backpack->needs_processing());
}

TEST_CASE("nested_processing_food_flag_changes_invalidate_container_cache", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();

    auto backpack = item::spawn("backpack");
    backpack->put_in(item::spawn("bread"));
    item& stored = backpack->contents.front();
    stored.deactivate();

    REQUIRE_FALSE(stored.is_active());
    REQUIRE(backpack->needs_processing());

    stored.set_flag(flag_PROCESSING);
    CHECK_FALSE(backpack->needs_processing());

    stored.unset_flag(flag_PROCESSING);
    CHECK(backpack->needs_processing());

    stored.set_flag(flag_PROCESSING);
    CHECK_FALSE(backpack->needs_processing());

    stored.unset_flags();
    CHECK(backpack->needs_processing());
}

TEST_CASE("active_item_cache_moves_items_when_processing_speed_changes", "[item]") {
    clear_all_state();

    auto backpack = item::spawn("backpack");
    backpack->put_in(item::spawn("sashimi"));
    REQUIRE(backpack->needs_processing());
    REQUIRE(backpack->processing_speed() == to_turns<int>(10_minutes));

    auto cache = active_item_cache();
    cache.add(*backpack);
    CHECK(cache.count().total == 1);

    auto active =
        item::spawn("firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
    active->activate();
    backpack->put_in(std::move(active));
    REQUIRE(backpack->needs_processing());
    CHECK(backpack->processing_speed() == 1);

    cache.add(*backpack);
    CHECK(cache.count().total == 1);
}

TEST_CASE("content_removal_helpers_invalidate_processing_cache", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();
    here.clear_items(test_origin);

    SECTION("spill contents") {
        auto backpack = item::spawn("backpack");
        auto active = item::
            spawn("firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
        active->activate();
        backpack->put_in(std::move(active));

        auto& carried_backpack = get_avatar().i_add(std::move(backpack));
        REQUIRE(carried_backpack.needs_processing());
        carried_backpack.contents.spill_contents(test_origin);
        CHECK_FALSE(carried_backpack.needs_processing());
    }

    SECTION("handle casings") {
        auto backpack = item::spawn("backpack");
        auto casing = item::spawn("rock");
        casing->set_flag(flag_CASING);
        casing->set_flag(flag_RADIO_ACTIVATION);
        backpack->put_in(std::move(casing));

        REQUIRE(backpack->needs_processing());
        backpack->contents.casings_handle([](detached_ptr<item>&& /*it*/) {
            return detached_ptr<item>();
        });
        CHECK_FALSE(backpack->needs_processing());
    }

    SECTION("same-size top content mutation") {
        auto backpack = item::spawn("backpack");
        auto radio = item::spawn("rock");
        radio->set_flag(flag_RADIO_ACTIVATION);
        backpack->put_in(std::move(radio));

        REQUIRE(backpack->needs_processing());
        backpack->contents.remove_top_items_with([](detached_ptr<item>&& it) {
            it->item_tags.erase(flag_RADIO_ACTIVATION);
            return std::move(it);
        });
        CHECK_FALSE(backpack->needs_processing());
    }

    SECTION("same-size nested content mutation") {
        auto backpack = item::spawn("backpack");
        auto inner_bag = item::spawn("bag_plastic");
        auto radio = item::spawn("rock");
        radio->set_flag(flag_RADIO_ACTIVATION);
        inner_bag->put_in(std::move(radio));
        backpack->put_in(std::move(inner_bag));

        REQUIRE(backpack->needs_processing());
        backpack->contents.front().contents.remove_top_items_with([](detached_ptr<item>&& it) {
            it->item_tags.erase(flag_RADIO_ACTIVATION);
            return std::move(it);
        });
        CHECK_FALSE(backpack->needs_processing());
    }
}

TEST_CASE("active_item_cache_slow_items_accrue_elapsed_time", "[item]") {
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm;

    auto cache = active_item_cache();
    auto sashimi = item::spawn("sashimi");
    REQUIRE(sashimi->processing_speed() == to_turns<int>(10_minutes));
    auto& sashimi_ref = *sashimi;
    cache.add(sashimi_ref);

    auto items_to_process = cache.get_for_processing();
    CHECK(items_to_process.empty());

    calendar::turn += 20_minutes;
    items_to_process = cache.get_for_processing();
    REQUIRE(items_to_process.size() == 1);
    CHECK(items_to_process.front() == &sashimi_ref);

    items_to_process = cache.get_for_processing();
    CHECK(items_to_process.empty());
}

TEST_CASE("place_active_item_at_various_coordinates", "[item]") {
    clear_all_state();
    auto& here = g->m.get_mapbuffer();
    for (auto z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z) {
        for (auto x = 0; x < T_MAPSIZE_X; ++x) {
            for (auto y = 0; y < T_MAPSIZE_Y; ++y) {
                here.clear_items(bub_to_abs(tripoint_bub_ms{x, y, z}));
            }
        }
    }
    const auto baseline_active_submaps = here.get_submaps_with_active_items();
    // An arbitrary active item.
    auto& active = *item::spawn_temporary(
        "firecracker_act", calendar::start_of_cataclysm, item::default_charges_tag());
    active.activate();

    // For each space in a wide area place the item and check if the cache has been updated.
    const auto z = 0;
    for (auto x = 0; x < T_MAPSIZE_X; ++x) {
        for (auto y = 0; y < T_MAPSIZE_Y; ++y) {
            auto pos = bub_to_abs(tripoint_bub_ms{x, y, z});
            auto sm_pos = project_to<coords::sm>(pos);
            REQUIRE(here.get_items(pos)->empty());
            CAPTURE(x, y, z);
            CAPTURE(pos.x(), pos.y(), pos.z());
            REQUIRE(here.get_submaps_with_active_items() == baseline_active_submaps);
            REQUIRE(here.get_submaps_with_active_items().find(sm_pos)
                    == here.get_submaps_with_active_items().end());
            auto n = item::spawn(active);
            auto& item_ref = *n;
            here.add_item(pos, std::move(n));
            REQUIRE(item_ref.is_active());
            auto expected_active_submaps = baseline_active_submaps;
            expected_active_submaps.insert(sm_pos);
            REQUIRE(here.get_submaps_with_active_items() == expected_active_submaps);
            REQUIRE(here.get_submaps_with_active_items().find(sm_pos)
                    != here.get_submaps_with_active_items().end());
            REQUIRE_FALSE(here.get_items(pos)->empty());
            here.clear_items(pos);
            REQUIRE(here.get_submaps_with_active_items() == baseline_active_submaps);
        }
    }
}
