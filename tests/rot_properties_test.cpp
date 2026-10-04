#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "monster.h"
#include "rot.h"
#include "state_helpers.h"
#include "units_temperature.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "weather/weather.h"

#include <algorithm>
#include <array>
#include <ranges>

namespace {

auto fresh_food() -> detached_ptr<item> {
    auto food = item::spawn("test_rot_food", calendar::turn);
    food->set_relative_rot(0.0);
    return food;
}

auto fixed_weather() -> weather_manager {
    auto weather = weather_manager();
    weather.temperature = 18_c;
    weather.clear_temp_cache();
    return weather;
}

} // namespace

TEST_CASE(
    "Tile rot temperature honours storage precedence for every flag combination",
    "[rot][property]") {
    using enum temperature_flag;
    const auto mask = GENERATE(Catch::Generators::range(0, 8));
    const auto flags = rot::temp::tile_flags{
        .root_cellar = (mask & 1) != 0,
        .fridge = (mask & 2) != 0,
        .freezer = (mask & 4) != 0,
    };
    // Independent truth table: root cellar > freezer > fridge > ambient.
    constexpr auto expected = std::array{
        TEMP_NORMAL,  TEMP_ROOT_CELLAR, TEMP_FRIDGE,  TEMP_ROOT_CELLAR,
        TEMP_FREEZER, TEMP_ROOT_CELLAR, TEMP_FREEZER, TEMP_ROOT_CELLAR,
    };
    CAPTURE(mask);
    CHECK(rot::temp::for_tile(flags) == expected[mask]);
}

TEST_CASE(
    "Hourly rot rates are monotone and bounded across temperature boundaries", "[rot][property]") {
    auto previous = 0;
    for (const auto celsius : std::views::iota(-40, 81)) {
        const auto temperature = units::from_celsius(celsius);
        const auto rate = get_hourly_rotpoints_at_temp(temperature);
        CAPTURE(celsius);
        CHECK(rate >= previous);
        CHECK(rate <= 21240);
        if (celsius <= 0) {
            CHECK(rate == 0);
        } else if (celsius > 40) {
            CHECK(rate == 21240);
        }
        previous = rate;
    }
    CHECK(get_hourly_rotpoints_at_temp(18_c) == to_turns<int>(1_hours));
}

TEST_CASE(
    "Rot processing respects the strict ten minute interval and is idempotent", "[rot][property]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto elapsed =
        GENERATE(0_turns, 1_turns, 10_minutes, 10_minutes + 1_turns, 20_minutes, 1_hours);
    auto weather = fixed_weather();
    auto food = fresh_food();
    calendar::turn += elapsed;
    const auto context =
        item::rot_context{.position = tripoint_abs_ms(0, 0, 0), .weather = &weather};
    food->update_rot(context);
    const auto expected = elapsed > 10_minutes ? elapsed : 0_turns;
    CAPTURE(to_turns<int>(elapsed));
    CHECK(food->get_rot() == expected);
    food->update_rot(context);
    CHECK(food->get_rot() == expected);
}

TEST_CASE("Moving time backwards does not reverse accumulated rot", "[rot][time]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto weather = fixed_weather();
    auto food = fresh_food();
    food->set_rot(1_hours);
    const auto context =
        item::rot_context{.position = tripoint_abs_ms(0, 0, 0), .weather = &weather};
    calendar::turn -= 1_days;
    food->update_rot(context);
    CHECK(food->get_rot() == 1_hours);
    calendar::turn += 20_minutes;
    food->update_rot(context);
    CHECK(food->get_rot() == 80_minutes);
}

TEST_CASE(
    "Storage temperatures constrain rot without depending on ambient weather", "[rot][property]") {
    using enum temperature_flag;
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto ambient = GENERATE(-20_c, 0_c, 18_c, 50_c);
    const auto storage =
        GENERATE(TEMP_NORMAL, TEMP_FREEZER, TEMP_FRIDGE, TEMP_ROOT_CELLAR, TEMP_HEATER);
    auto weather = fixed_weather();
    weather.temperature = ambient;
    const auto base = fresh_food();
    auto normal = item::spawn(*base);
    auto stored = item::spawn(*base);
    calendar::turn += 1_hours;
    normal->update_rot({.position = tripoint_abs_ms(0, 0, 0), .weather = &weather});
    stored->update_rot(
        {.position = tripoint_abs_ms(0, 0, 0), .temperature = storage, .weather = &weather});
    CAPTURE(static_cast<int>(storage), units::to_celsius(ambient));
    if (storage == TEMP_FREEZER) {
        CHECK(stored->get_rot() == 0_turns);
    } else if (storage == TEMP_FRIDGE) {
        CHECK(stored->get_rot() <= normal->get_rot());
        if (ambient > temperatures::fridge) { CHECK(stored->get_rot() < normal->get_rot()); }
    } else if (storage == TEMP_HEATER) {
        CHECK(stored->get_rot() >= normal->get_rot());
        CHECK(stored->get_rot() > 0_turns);
        if (ambient < temperatures::normal) { CHECK(stored->get_rot() > normal->get_rot()); }
    } else if (storage == TEMP_ROOT_CELLAR) {
        CHECK(to_turns<int>(stored->get_rot())
              == get_hourly_rotpoints_at_temp(temperatures::root_cellar));
    } else {
        CHECK(stored->get_rot() == normal->get_rot());
    }
}

TEST_CASE(
    "Historical rot catch-up preserves frozen items and never runs twice", "[rot][property]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto elapsed = GENERATE(1_hours + 1_turns, 2_hours, 24_hours);
    const auto z = GENERATE(-1, 0);
    const auto starting = GENERATE(false, true);
    const auto restore_starting = restore_on_out_of_scope(g->new_game);
    g->new_game = starting;
    const auto temperature =
        GENERATE(temperature_flag::TEMP_FREEZER, temperature_flag::TEMP_ROOT_CELLAR);
    auto weather = fixed_weather();
    auto food = fresh_food();
    calendar::turn += elapsed;
    const auto context = item::rot_context{
        .position = tripoint_abs_ms(0, 0, z), .temperature = temperature, .weather = &weather};
    food->update_rot(context);
    const auto accumulated = food->get_rot();
    CAPTURE(z, to_turns<int>(elapsed), static_cast<int>(temperature));
    if (temperature == temperature_flag::TEMP_FREEZER) {
        CHECK(accumulated == 0_turns);
    } else {
        CHECK(accumulated > 0_turns);
    }
    food->update_rot(context);
    CHECK(food->get_rot() == accumulated);
}

TEST_CASE("Initial rot variation remains within its documented bounds", "[rot][boundary]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm;
    auto food = fresh_food();
    auto weather = fixed_weather();
    calendar::turn += 20_minutes;
    food->update_rot({.position = tripoint_abs_ms::zero(), .weather = &weather});
    const auto variation = food->get_shelf_life() / 5;
    CHECK(food->get_rot() >= 20_minutes - variation);
    CHECK(food->get_rot() <= 20_minutes + variation);
    const auto accumulated = food->get_rot();
    food->update_rot({.position = tripoint_abs_ms::zero(), .weather = &weather});
    CHECK(food->get_rot() == accumulated);
}

TEST_CASE("Invalid null item types are diagnosed without being destroyed", "[rot][boundary]") {
    auto it = item::spawn();
    const auto* identity = it.get();
    const auto message = capture_debugmsg_during([&]() {
        it = item::actualize_rot(std::move(it), {.position = tripoint_abs_ms::zero()});
    });
    CHECK(message.find("skipping item") != std::string::npos);
    CHECK(it.get() == identity);
}

TEST_CASE(
    "Food removal requires exceeding shelf life and neither sealing nor a carrier",
    "[rot][property]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto relative_rot = GENERATE(0.0, 1.0, 2.0, 2.01, 3.0);
    const auto sealed = GENERATE(false, true);
    const auto carried = GENERATE(false, true);
    auto weather = fixed_weather();
    auto food = fresh_food();
    food->set_relative_rot(relative_rot);
    const auto* identity = food.get();
    food = item::process_rot(
        std::move(food), sealed, tripoint_bub_ms::zero(), carried ? &get_avatar() : nullptr,
        temperature_flag::TEMP_NORMAL, weather);
    CAPTURE(relative_rot, sealed, carried);
    CHECK(static_cast<bool>(food) == (relative_rot <= 2.0 || sealed || carried));
    if (food) { CHECK(food.get() == identity); }
}

TEST_CASE("Corpse removal has a ten day boundary and excludes revivers", "[rot][property]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto age = GENERATE(10_days - 1_turns, 10_days, 10_days + 1_turns);
    const auto revives = GENERATE(false, true);
    const auto sealed = GENERATE(false, true);
    auto weather = fixed_weather();
    auto corpse = item::make_corpse(
        mtype_id(revives ? "mon_test_rot_reviver" : "mon_test_rot_corpse"), calendar::turn);
    corpse->set_relative_rot(0.0);
    corpse->set_rot(age);
    REQUIRE(corpse->can_revive() == revives);
    corpse = item::process_rot(
        std::move(corpse), sealed, tripoint_bub_ms::zero(), nullptr, temperature_flag::TEMP_NORMAL,
        weather);
    CAPTURE(to_turns<int>(age), revives, sealed);
    CHECK(static_cast<bool>(corpse) == (age <= 10_days || revives || sealed));
}

TEST_CASE(
    "Actualization propagates sealing and preservation through nested containers",
    "[rot][property]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto container_type = GENERATE("test_rot_open", "test_rot_sealed", "test_rot_preserving");
    const auto depth = GENERATE(0, 1, 3);
    const auto rotten = GENERATE(false, true);
    auto weather = fixed_weather();
    auto contents = fresh_food();
    contents->set_relative_rot(rotten ? 3.0 : 0.0);
    for (const auto ignored : std::views::iota(0, depth)) {
        static_cast<void>(ignored);
        auto inner = item::spawn("test_rot_open");
        inner->put_in(std::move(contents));
        contents = std::move(inner);
    }
    auto outer = item::spawn(container_type);
    outer->put_in(item::spawn("test_rot_remains"));
    outer->put_in(std::move(contents));
    const auto* identity = outer.get();
    calendar::turn += 1_hours;
    outer = item::actualize_rot(
        std::move(outer), {.position = bub_to_abs(tripoint_bub_ms::zero()), .weather = &weather});
    REQUIRE(outer.get() == identity);
    const auto all = outer->contents.all_items_ptr();
    const auto food = std::ranges::find_if(all, [](const auto* it) {
        return it->typeId() == itype_id("test_rot_food");
    });
    const auto should_remain = !rotten || std::string_view(container_type) != "test_rot_open";
    CAPTURE(container_type, depth, rotten);
    REQUIRE((food != all.end()) == should_remain);
    CHECK(std::ranges::count_if(
              all, [](const auto* it) { return it->typeId() == itype_id("test_rot_remains"); })
          == 1);
    if (food != all.end()) {
        const auto preserved = std::string_view(container_type) == "test_rot_preserving";
        CHECK((*food)->get_rot() == (rotten ? 3_days : preserved ? 0_turns : 1_hours));
    }
}

TEST_CASE("Field dressing reduces corpse rot without stopping it", "[rot][corpse]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto weather = fixed_weather();
    auto normal = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
    normal->set_relative_rot(0.0);
    auto dressed = item::spawn(*normal);
    dressed->set_flag(flag_id("FIELD_DRESS"));
    calendar::turn += 1_hours;
    normal->update_rot({.position = tripoint_abs_ms::zero(), .weather = &weather});
    dressed->update_rot({.position = tripoint_abs_ms::zero(), .weather = &weather});
    CHECK(normal->get_rot() == 1_hours);
    CHECK(dressed->get_rot() == 45_minutes);
}

TEST_CASE(
    "Freshness estimates honour preserving storage and finite shelf life", "[rot][freshness]") {
    using enum temperature_flag;
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto storage =
        GENERATE(TEMP_NORMAL, TEMP_FREEZER, TEMP_FRIDGE, TEMP_ROOT_CELLAR, TEMP_HEATER);
    auto food = fresh_food();
    const auto fresh_estimate = food->minimum_freshness_duration(storage);
    CAPTURE(static_cast<int>(storage));
    if (storage == TEMP_FREEZER) {
        CHECK(fresh_estimate == calendar::INDEFINITELY_LONG_DURATION);
    } else {
        CHECK(fresh_estimate > 0_turns);
        CHECK(fresh_estimate < calendar::INDEFINITELY_LONG_DURATION);
        food->set_relative_rot(0.5);
        CHECK(to_turns<int>(food->minimum_freshness_duration(storage))
              == Approx(to_turns<int>(fresh_estimate) / 2.0).margin(1));
    }
    auto preserving = item::spawn("test_rot_preserving");
    preserving->put_in(std::move(food));
    CHECK(preserving->contents.front().minimum_freshness_duration(storage)
          == calendar::INDEFINITELY_LONG_DURATION);
    CHECK(item::spawn("test_rot_remains")->minimum_freshness_duration(storage)
          == calendar::INDEFINITELY_LONG_DURATION);
}

TEST_CASE(
    "Storage temperature follows real item locations through nested containers",
    "[rot][location]") {
    using enum temperature_flag;
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(65, 65, 0);
    auto outer = item::spawn("test_rot_open");
    auto inner = item::spawn("test_rot_open");
    inner->put_in(fresh_food());
    outer->put_in(std::move(inner));
    const auto* food = &outer->contents.front().contents.front();
    CHECK(rot::temp::for_location(here, *food) == TEMP_NORMAL);
    SECTION("avatar inventory") {
        get_avatar().i_add(std::move(outer));
        CHECK(rot::temp::for_location(here, *food) == TEMP_NORMAL);
    }
    SECTION("monster inventory") {
        auto& mon = spawn_test_monster("debug_mon", pos);
        mon.add_item(std::move(outer));
        CHECK(rot::temp::for_location(here, *food) == TEMP_NORMAL);
    }
    SECTION("map furniture and terrain") {
        const auto type = GENERATE("f_null", "f_fridge_on", "f_minifreezer_on");
        const auto cellar = GENERATE(false, true);
        here.ter_set(pos, cellar ? t_rootcellar : t_floor);
        here.furn_set(pos, furn_str_id(type));
        here.add_item(pos, std::move(outer));
        const auto furniture_temp =
            std::string_view(type) == "f_fridge_on" ? TEMP_FRIDGE
            : std::string_view(type) == "f_minifreezer_on"
                ? TEMP_FREEZER
                : TEMP_NORMAL;
        CHECK(rot::temp::for_location(here, *food) == (cellar ? TEMP_ROOT_CELLAR : furniture_temp));
    }
    SECTION("vehicle cold storage and engine heater precedence") {
        const auto type = GENERATE("fridge", "freezer", "seat");
        const auto enabled = GENERATE(false, true);
        const auto heater = GENERATE(false, true);
        auto* veh = here.add_vehicle(vproto_id("none"), pos, 0_degrees, 0, 0);
        REQUIRE(veh != nullptr);
        REQUIRE(veh->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"), true) >= 0);
        const auto part = veh->install_part(tripoint_mnt_veh::zero(), vpart_id(type), true);
        REQUIRE(part >= 0);
        veh->part(part).enabled = enabled;
        here.add_vehicle_to_cache(veh);
        const auto cooling = enabled && std::string_view(type) != "seat";
        const auto expected =
            cooling ? (std::string_view(type) == "freezer" ? TEMP_FREEZER : TEMP_FRIDGE)
                    : TEMP_NORMAL;
        REQUIRE_FALSE(veh->add_item(part, std::move(outer)));
        CHECK(rot::temp::for_location(here, *food) == expected);
        CHECK(
            rot::temp::for_part(*veh, part, heater)
            == (cooling  ? expected
                : heater ? TEMP_HEATER
                         : TEMP_NORMAL));
    }
}

TEST_CASE(
    "Nonperishables and empty pointers survive rot actualization appropriately",
    "[rot][boundary]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto weather = fixed_weather();
    const auto context =
        item::rot_context{.position = tripoint_abs_ms::zero(), .weather = &weather};
    CHECK_FALSE(item::actualize_rot(detached_ptr<item>(), context));
    CHECK_FALSE(item::process_rot(detached_ptr<item>(), tripoint_bub_ms::zero()));
    const auto type = GENERATE("test_rot_remains", "test_rot_nonperishable", "test_rot_open");
    auto it = item::spawn(type);
    const auto* identity = it.get();
    calendar::turn += 365_days;
    it = item::actualize_rot(std::move(it), context);
    REQUIRE(it.get() == identity);
    CHECK(it->get_rot() == 0_turns);
}
