#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "json.h"
#include "map/map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "state_helpers.h"
#include "units_temperature.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "weather/weather.h"

#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct lifecycle_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<units::temperature> temperature{get_weather().temperature};

    lifecycle_state() {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + 91_days;
        get_map().set_temperature(tripoint_bub_ms::zero(), 0);
        get_map().set_temperature(tripoint_bub_ms(60, 60, 0), 0);
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~lifecycle_state() {
        clear_all_state();
        get_weather().clear_temp_cache();
    }
};

auto serialized(const item& value) -> std::string {
    auto stream = std::ostringstream();
    auto json = JsonOut(stream);
    value.serialize(json);
    return stream.str();
}

auto reload(const item& value) -> detached_ptr<item> {
    auto stream = std::istringstream(serialized(value));
    auto json = JsonIn(stream);
    return item::spawn(json);
}

auto ingredient(bool contained) -> detached_ptr<item> {
    auto food = item::spawn("test_rot_food", calendar::turn);
    food->set_relative_rot(0.25);
    if (!contained) { return food; }
    auto container = item::spawn("test_rot_open");
    container->put_in(std::move(food));
    return container;
}

auto edible_part(item& value, bool contained) -> item& // *NOPAD*
{
    return contained ? value.contents.front() : value;
}

struct cooking_rig {
    vehicle* veh = nullptr;
    int freezer_part = -1;
    tripoint_bub_ms station{60, 60, 0};
    tripoint_bub_ms freezer{59, 60, 0};
};

auto make_cooking_rig(bool vehicle_kitchen) -> cooking_rig {
    auto rig = cooking_rig{};
    auto& here = get_map();
    get_avatar().setpos(rig.station + tripoint_south);
    here.set_temperature(rig.station, 0);
    here.set_temperature(rig.freezer, 0);
    if (vehicle_kitchen) {
        rig.veh = here.add_vehicle(vproto_id("none"), rig.station, 0_degrees, 0, 0);
        REQUIRE(rig.veh != nullptr);
        for (const auto x : std::array{-1, 0, 1}) {
            REQUIRE(
                rig.veh->install_part(tripoint_mnt_veh(x, 0, 0), vpart_id("frame_vertical"), true)
                >= 0);
        }
        REQUIRE(
            rig.veh->install_part(tripoint_mnt_veh::zero(), vpart_id("kitchen_unit"), true) >= 0);
        REQUIRE(rig.veh->install_part(tripoint_mnt_veh(1, 0, 0), vpart_id("storage_battery"), true)
                >= 0);
        rig.freezer_part =
            rig.veh->install_part(tripoint_mnt_veh(-1, 0, 0), vpart_id("minifreezer"), true);
        REQUIRE(rig.freezer_part >= 0);
        rig.veh->part(rig.freezer_part).enabled = true;
        rig.veh->charge_battery(5000);
        here.add_vehicle_to_cache(rig.veh);
    } else {
        here.furn_set(rig.freezer, furn_str_id("f_minifreezer_on"));
    }
    here.build_map_cache(rig.station.z(), true);
    return rig;
}

auto cook_at(const recipe_id& id, const tripoint_bub_ms& station) -> item& // *NOPAD*
{
    auto& you = get_avatar();
    const auto& rec = id.obj();
    // The minimum skill roll exceeds the maximum difficulty roll (24-sided dice).
    // DEBUG_HS cannot be used: it bypasses real material and tool consumption.
    you.set_skill_level(rec.skill_used, rec.difficulty * 24 + 1);
    for (const auto& [skill, difficulty] : rec.required_skills) {
        you.set_skill_level(skill, difficulty * 24 + 1);
    }
    you.learn_recipe(&rec);
    you.invalidate_crafting_inventory();
    REQUIRE(rec.deduped_requirements()
                .can_make_with_inventory(you.crafting_inventory(), rec.get_component_filter()));
    REQUIRE(lighting_crafting_speed_multiplier(you, rec) > 0.0);
    you.make_craft(id, 1, station);
    REQUIRE(you.activity);
    auto turns = 0;
    while (you.activity && you.activity->id() == activity_id("ACT_CRAFT") && turns < 100000) {
        ++turns;
        you.moves = 100;
        you.activity->do_turn(you);
        calendar::turn += 1_turns;
    }
    REQUIRE(turns < 100000);
    const auto results = you.items_with([&](const auto& it) {
        return it.typeId() == rec.result();
    });
    REQUIRE(results.size() == 1);
    return *results.front();
}

} // namespace

TEST_CASE(
    "Cooking on day 22 does not spoil ingredients when using an oil cooker or RV kitchen",
    "[rot][crafting][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    // There is no public forget-all API; restore the mutable recipe store after crafting.
    const auto recipes = restore_on_out_of_scope(
        const_cast<recipe_subset&>(get_avatar().get_learned_recipes()));
    const auto vehicle_kitchen = GENERATE(false, true);
    const auto recipe_name = GENERATE("cornbread", "pemmican", "sausagegravy");
    const auto handmade = GENERATE(false, true);
    CAPTURE(vehicle_kitchen, recipe_name, handmade);
    set_time(calendar::start_of_cataclysm + 12_hours);
    auto rig = make_cooking_rig(vehicle_kitchen);
    auto& you = get_avatar();
    you.i_add(item::spawn("pot"));
    you.i_add(item::spawn("butchering_kit"));
    you.i_add(item::spawn("rock_quern"));
    auto* oil_cooker = vehicle_kitchen ? nullptr : &you.i_add(item::spawn("oil_cooker"));
    const auto fuel_before =
        vehicle_kitchen
            ? rig.veh->fuel_left(itype_id("battery"), true)
            : oil_cooker->ammo_remaining();
    REQUIRE(fuel_before > 0);

    auto source_food = std::vector<item*>();
    const auto add = [&](const char* type, int charges) {
        auto value = item::spawn(itype_id(type), calendar::turn, charges);
        value->set_relative_rot(0.0);
        source_food.push_back(value.get());
        if (value->made_of(LIQUID)) {
            value = item::in_container(itype_id("bottle_plastic"), std::move(value));
        }
        if (vehicle_kitchen) {
            REQUIRE_FALSE(rig.veh->add_item(rig.freezer_part, std::move(value)));
        } else {
            get_map().add_item(rig.freezer, std::move(value));
        }
    };
    const auto name = std::string(recipe_name);
    if (name == "cornbread") {
        add(handmade ? "corn" : "cornmeal", handmade ? 1 : 3);
        add("water_clean", 2);
    } else {
        add(handmade ? "fat" : "lard", handmade ? 2 : 1);
        add("dry_meat", name == "pemmican" ? 2 : 1);
        if (name == "pemmican") {
            add("dry_mushroom", 1);
        } else {
            add(handmade ? "corn" : "cornmeal", 1);
            add("mushroom", 2);
        }
    }
    set_time(calendar::turn + 21_days);
    for (const auto* food : source_food) { REQUIRE_FALSE(food->rotten()); }
    if (handmade) {
        if (name != "pemmican") {
            CHECK_FALSE(cook_at(recipe_id("cornmeal"), rig.station).goes_bad());
        }
        if (name != "cornbread") {
            CHECK_FALSE(cook_at(recipe_id("lard"), rig.station).goes_bad());
        }
    }
    auto& food = cook_at(recipe_id(recipe_name), rig.station);
    CHECK_FALSE(food.rotten());
    REQUIRE_FALSE(food.get_components().empty());
    for (const auto* component : food.get_components()) {
        CAPTURE(component->typeId().str());
        CHECK(component->charges > 0);
        CHECK(std::isfinite(component->get_relative_rot()));
        CHECK_FALSE(component->rotten());
    }
    CHECK(food.components_to_string().find("(rotten)") == std::string::npos);
    const auto fuel_after =
        vehicle_kitchen
            ? rig.veh->fuel_left(itype_id("battery"), true)
            : oil_cooker->ammo_remaining();
    CHECK(fuel_after < fuel_before);
}

TEST_CASE(
    "Ingredients listed in finished food stop aging",
    "[rot][ingredients][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    const auto contained = GENERATE(false, true);
    const auto legacy_without_flag = GENERATE(false, true);
    const auto save_and_load = GENERATE(false, true);
    auto product = item::spawn("test_rot_food");
    if (legacy_without_flag) {
        product->get_components().push_back(ingredient(contained));
    } else {
        product->add_component(ingredient(contained));
    }
    if (save_and_load) { product = reload(*product); }
    auto& food = edible_part(*product->get_components().front(), contained);
    const auto before = serialized(food);
    calendar::turn += 20_minutes;

    CHECK(food.get_rot() == 6_hours);
    CHECK(food.get_relative_rot() == Approx(0.25));
    CHECK_FALSE(food.rotten());
    food.update_rot({.position = tripoint_abs_ms::zero()});
    CHECK(food.get_rot() == 6_hours);
    CHECK(serialized(food) == before);
}

TEST_CASE(
    "Ingredients added to an unfinished recipe can still spoil",
    "[rot][ingredients][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    const auto& recipe = recipe_id("meat_cooked").obj();
    auto craft =
        item::spawn(&recipe, 1, std::vector<detached_ptr<item>>{}, std::vector<item_comp>{});
    craft->add_component(ingredient(false));
    auto& food = *craft->get_components().front();
    REQUIRE(food.has_flag(flag_id("COMPONENT")));
    calendar::turn += 20_minutes;
    CHECK(food.get_relative_rot() == Approx((6_hours + 20_minutes) / 1_days));
    CHECK(food.get_rot() == 6_hours + 20_minutes);
    calendar::turn += 20_minutes;
    food.attempt_detach([](auto&& material) {
        CHECK(material->get_relative_rot() == Approx((6_hours + 40_minutes) / 1_days));
        return std::move(material);
    });
    CHECK(food.get_rot() == 6_hours + 40_minutes);
}

TEST_CASE(
    "Recovered ingredients keep their original freshness",
    "[rot][ingredients][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    const auto contained = GENERATE(false, true);
    const auto corpse = GENERATE(false, true);
    const auto clear_directly = GENERATE(false, true);
    const auto processing_record = GENERATE(false, true);
    auto owner =
        corpse ? item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn)
               : item::spawn("test_rot_food");
    auto component = ingredient(contained);
    if (processing_record) { edible_part(*component, contained).set_flag(flag_id("PROCESSING")); }
    owner->add_component(std::move(component));
    calendar::turn += 21_days;
    auto recovered = clear_directly ? owner->get_components().clear() : owner->remove_components();
    REQUIRE(recovered.size() == 1);
    REQUIRE(owner->get_components().empty());
    auto& food = edible_part(*recovered.front(), contained);
    CHECK_FALSE(food.has_flag(flag_id("PROCESSING")));
    CHECK(food.get_rot() == 6_hours);
    calendar::turn += 20_minutes;
    food.update_rot({.position = tripoint_abs_ms::zero()});
    CHECK(food.get_rot() == 6_hours + 20_minutes);
    CHECK(food.get_relative_rot() == Approx((6_hours + 20_minutes) / 1_days));
}

TEST_CASE(
    "Rotten ingredients stay in a finished food's ingredient list",
    "[rot][ingredients][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    const auto placed = GENERATE(false, true);
    auto owner = item::spawn("test_rot_food");
    auto* const record_owner = owner.get();
    auto food = ingredient(false);
    food->set_relative_rot(3.0);
    owner->add_component(std::move(food));
    if (placed) { get_map().add_item(tripoint_bub_ms(65, 65, 0), std::move(owner)); }
    calendar::turn += 20_minutes;
    record_owner->get_components().front()->attempt_detach([](auto&& record) {
        return item::process_rot(std::move(record), tripoint_bub_ms::zero());
    });
    REQUIRE(record_owner->get_components().size() == 1);
    CHECK(record_owner->get_components().front()->get_rot() == 3_days);
}

TEST_CASE("Reclaimed ingredients keep aging after they are moved", "[rot][lifecycle]") {
    const auto state = lifecycle_state();
    const auto place_during_callback = GENERATE(false, true);
    auto owner = item::spawn("test_rot_food");
    owner->add_component(ingredient(false));
    calendar::turn += 21_days;
    auto recovered = detached_ptr<item>();
    owner->get_components().front()->attempt_detach([&](auto&& record) {
        CHECK(record->get_rot() == 6_hours);
        if (place_during_callback) {
            auto& placed = get_avatar().i_add(std::move(record));
            calendar::turn += 20_minutes;
            CHECK(placed.get_rot() == 6_hours + 20_minutes);
            recovered = placed.detach();
        } else {
            recovered = std::move(record);
        }
        return detached_ptr<item>();
    });
    REQUIRE(recovered);
    CHECK(owner->get_components().empty());
    calendar::turn += 20_minutes;
    CHECK(recovered->get_rot() == 6_hours + (place_during_callback ? 40_minutes : 20_minutes));
}

TEST_CASE(
    "Smoking or milling stops food from aging without making it fresh again",
    "[rot][ingredients][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    auto food = ingredient(false);
    food->set_flag(flag_id("PROCESSING"));
    calendar::turn += 1_hours;
    food->update_rot({.position = tripoint_abs_ms::zero()});
    CHECK(food->get_shelf_life() == 1_days);
    CHECK(food->get_relative_rot() == Approx(0.25));
    CHECK(food->get_rot() == 6_hours);
    // Existing smoker/mill completion API accounts for the processing interval.
    food->mod_last_rot_check(1_hours);
    food->unset_flag(flag_id("PROCESSING"));
    calendar::turn += 20_minutes;
    CHECK(food->get_rot() == 6_hours + 20_minutes);
}

TEST_CASE(
    "A copy of an ingredient ages without changing the finished food's ingredient list",
    "[rot][ingredients][lifecycle][issue_10181]") {
    const auto state = lifecycle_state();
    const auto contained = GENERATE(false, true);
    const auto assign = GENERATE(false, true);
    const auto processing_record = GENERATE(false, true);
    auto owner = item::spawn("test_rot_food");
    auto component = ingredient(contained);
    if (processing_record) { edible_part(*component, contained).set_flag(flag_id("PROCESSING")); }
    owner->add_component(std::move(component));
    auto& record = *owner->get_components().front();
    const auto before = serialized(record);
    calendar::turn += 21_days;
    auto copy = assign ? item::spawn("test_rot_food") : item::spawn(record);
    if (assign) { *copy = record; }
    CHECK(serialized(record) == before);
    auto& food = edible_part(*copy, contained);
    CHECK_FALSE(food.has_flag(flag_id("PROCESSING")));
    CHECK(food.get_rot() == 6_hours);
    calendar::turn += 20_minutes;
    CHECK(food.get_rot() == 6_hours + 20_minutes);
}

TEST_CASE(
    "An old COMPONENT flag alone does not prevent food from spoiling",
    "[rot][ingredients][lifecycle]") {
    const auto state = lifecycle_state();
    auto food = ingredient(false);
    food->set_flag(flag_id("COMPONENT"));
    calendar::turn += 20_minutes;
    CHECK(food->get_relative_rot() == Approx(0.25));
    food->update_rot({.position = tripoint_abs_ms::zero()});
    CHECK(food->get_relative_rot() == Approx((6_hours + 20_minutes) / 1_days));
}
