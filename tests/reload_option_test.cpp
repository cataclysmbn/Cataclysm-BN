#include "avatar.h"
#include "catch/catch.hpp"
#include "item.h"
#include "reload/reload.h"

#include <climits>

TEST_CASE("revolver_reload_option", "[reload],[reload_option],[gun]") {
    const time_point bday = calendar::start_of_cataclysm;
    avatar dummy;

    detached_ptr<item> det = item::spawn("sw_619", bday, 0);
    item& gun = *det;
    dummy.i_add(std::move(det));
    det = item::spawn("38_special", bday, gun.ammo_capacity());
    item& ammo = *det;
    dummy.i_add(std::move(det));
    REQUIRE(gun.has_flag(flag_id("RELOAD_ONE")));
    REQUIRE(gun.ammo_remaining() == 0);

    auto gun_option = item_reload_option(&dummy, &gun, &gun, ammo);
    REQUIRE(gun_option.qty() == 1);
    gun_option.qty(0);
    CHECK(gun_option.qty() == 1);
    gun_option.qty(INT_MAX);
    CHECK(gun_option.qty() == 1);

    det = item::spawn("38_speedloader", bday, 0);
    item& speedloader = *det;
    dummy.i_add(std::move(det));
    REQUIRE(speedloader.ammo_remaining() == 0);

    auto speedloader_option = item_reload_option(&dummy, &speedloader, &speedloader, ammo);
    CHECK(speedloader_option.qty() == speedloader.ammo_capacity());
    speedloader_option.qty(0);
    CHECK(speedloader_option.qty() == 1);
    speedloader_option.qty(INT_MAX);
    CHECK(speedloader_option.qty() == speedloader.ammo_capacity());

    speedloader.put_in(item::spawn(ammo));
    auto gun_speedloader_option = item_reload_option(&dummy, &gun, &gun, speedloader);
    CHECK(gun_speedloader_option.qty() == speedloader.ammo_capacity());
    gun_speedloader_option.qty(0);
    CHECK(gun_speedloader_option.qty() == 1);
    gun_speedloader_option.qty(INT_MAX);
    CHECK(gun_speedloader_option.qty() == speedloader.ammo_capacity());
}

TEST_CASE("magazine_reload_option", "[reload],[reload_option],[gun]") {
    const time_point bday = calendar::start_of_cataclysm;
    avatar dummy;

    detached_ptr<item> det = item::spawn("glockmag", bday, 0);
    item& magazine = *det;
    dummy.i_add(std::move(det));
    det = item::spawn("9mm", bday, magazine.ammo_capacity());
    item& ammo = *det;
    dummy.i_add(std::move(det));

    auto magazine_option = item_reload_option(&dummy, &magazine, &magazine, ammo);
    CHECK(magazine_option.qty() == magazine.ammo_capacity());
    magazine_option.qty(0);
    CHECK(magazine_option.qty() == 1);
    magazine_option.qty(INT_MAX);
    CHECK(magazine_option.qty() == magazine.ammo_capacity());

    magazine.put_in(item::spawn(ammo));
    det = item::spawn("glock_19", bday, 0);
    item& gun = *det;
    dummy.i_add(std::move(det));
    const item_reload_option gun_option(&dummy, &gun, &gun, magazine);
    CHECK(gun_option.qty() == 1);
}

TEST_CASE("belt_reload_option", "[reload],[reload_option],[gun]") {
    const time_point bday = calendar::start_of_cataclysm;
    avatar dummy;
    dummy.set_body();

    detached_ptr<item> det = item::spawn("belt308", bday, 0);
    item& belt = *det;
    dummy.i_add(std::move(det));
    det = item::spawn("308", bday, belt.ammo_capacity());
    item& ammo = *det;
    dummy.i_add(std::move(det));
    dummy.i_add(item::spawn("ammolink308", bday, belt.ammo_capacity()));
    // Belt is populated with "charges" rounds by the item constructor.
    belt.ammo_unset();

    REQUIRE(belt.ammo_remaining() == 0);
    auto belt_option = item_reload_option(&dummy, &belt, &belt, ammo);
    CHECK(belt_option.qty() == belt.ammo_capacity());
    belt_option.qty(0);
    CHECK(belt_option.qty() == 1);
    belt_option.qty(INT_MAX);
    CHECK(belt_option.qty() == belt.ammo_capacity());

    belt.put_in(item::spawn(ammo));
    det = item::spawn("m134", bday, 0);
    item& gun = *det;
    dummy.i_add(std::move(det));

    const item_reload_option gun_option(&dummy, &gun, &gun, belt);

    CHECK(gun_option.qty() == 1);
}

TEST_CASE("canteen_reload_option", "[reload],[reload_option],[liquid]") {
    avatar dummy;

    detached_ptr<item> det = item::spawn("water_clean", calendar::start_of_cataclysm, 2);
    item& water = *det;
    dummy.i_add(std::move(det));
    det = item::spawn("bottle_plastic");
    item& bottle = *det;
    dummy.i_add(std::move(det));

    auto bottle_option = item_reload_option(&dummy, &bottle, &bottle, water);
    const auto bottle_capacity = bottle.get_remaining_capacity_for_liquid(water, true);
    CHECK(bottle_option.qty() == bottle_capacity);
    bottle_option.qty(0);
    CHECK(bottle_option.qty() == 1);
    bottle_option.qty(INT_MAX);
    CHECK(bottle_option.qty() == bottle_capacity);

    // Add water to bottle?
    bottle.fill_with(item::spawn(water), 2);
    det = item::spawn("2lcanteen");
    item& canteen = *det;
    dummy.i_add(std::move(det));

    auto canteen_option = item_reload_option(&dummy, &canteen, &canteen, bottle);
    CHECK(canteen_option.qty() == 2);
    canteen_option.qty(0);
    CHECK(canteen_option.qty() == 1);
    canteen_option.qty(INT_MAX);
    CHECK(canteen_option.qty() == 2);
}

TEST_CASE("reload_empty_option_value_initialization", "[reload][reload_option]") {
    const auto option = item_reload_option();

    CHECK(option.ammo == nullptr);
    CHECK_FALSE(option);
}

TEST_CASE("reload_quantity_respects_the_limiting_resource", "[reload][reload_option]") {
    avatar who;
    who.set_body();
    const auto birthday = calendar::start_of_cataclysm;

    SECTION("source charges can limit a magazine reload before capacity does") {
        auto& magazine = who.i_add(item::spawn("glockmag", birthday, 0));
        auto& ammo = who.i_add(item::spawn("9mm", birthday, 3));
        REQUIRE(magazine.ammo_remaining() == 0);
        REQUIRE(magazine.ammo_capacity() > ammo.charges);

        auto option = item_reload_option(&who, &magazine, &magazine, ammo);
        option.qty(INT_MAX);
        CHECK(option.qty() == 3);
    }

    SECTION("remaining capacity can limit a reload before source charges do") {
        auto& magazine = who.i_add(item::spawn("glockmag", birthday, 0));
        magazine.ammo_set(itype_id("9mm"), magazine.ammo_capacity() - 2);
        auto& ammo = who.i_add(item::spawn("9mm", birthday, 6));
        REQUIRE(magazine.ammo_capacity() - magazine.ammo_remaining() == 2);

        auto option = item_reload_option(&who, &magazine, &magazine, ammo);
        option.qty(INT_MAX);
        CHECK(option.qty() == 2);
    }

    SECTION("scarce belt linkages limit quantity without consuming them during selection") {
        auto& belt = who.i_add(item::spawn("belt308", birthday, 0));
        belt.ammo_unset();
        auto& ammo = who.i_add(item::spawn("308", birthday, 10));
        who.i_add(item::spawn("ammolink308", birthday, 3));
        REQUIRE(belt.ammo_capacity() > 3);
        REQUIRE(who.charges_of(itype_id("ammolink308")) == 3);

        auto option = item_reload_option(&who, &belt, &belt, ammo);
        option.qty(INT_MAX);
        CHECK(option.qty() == 3);
        CHECK(who.charges_of(itype_id("ammolink308")) == 3);
        CHECK(ammo.charges == 10);
        CHECK(belt.ammo_remaining() == 0);
    }
}
