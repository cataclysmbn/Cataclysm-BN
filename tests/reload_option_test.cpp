#include "avatar.h"
#include "catch/catch.hpp"
#include "item.h"

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

    const item_reload_option gun_option(&dummy, &gun, &gun, ammo);
    REQUIRE(gun_option.qty() == 1);

    det = item::spawn("38_speedloader", bday, 0);
    item& speedloader = *det;
    dummy.i_add(std::move(det));
    REQUIRE(speedloader.ammo_remaining() == 0);

    const item_reload_option speedloader_option(&dummy, &speedloader, &speedloader, ammo);
    CHECK(speedloader_option.qty() == speedloader.ammo_capacity());

    speedloader.put_in(item::spawn(ammo));
    const item_reload_option gun_speedloader_option(&dummy, &gun, &gun, speedloader);
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

    const item_reload_option magazine_option(&dummy, &magazine, &magazine, ammo);
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
    const item_reload_option belt_option(&dummy, &belt, &belt, ammo);
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

    const item_reload_option bottle_option(&dummy, &bottle, &bottle, water);
    CHECK(bottle_option.qty() == bottle.get_remaining_capacity_for_liquid(water, true));

    // Add water to bottle?
    bottle.fill_with(item::spawn(water), 2);
    det = item::spawn("2lcanteen");
    item& canteen = *det;
    dummy.i_add(std::move(det));

    const item_reload_option canteen_option(&dummy, &canteen, &canteen, bottle);

    CHECK(canteen_option.qty() == 2);
}
