#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "player.h"
#include "state_helpers.h"
#include "type_id.h"
#include "visitable.h"

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

TEST_CASE("reload_magazine", "[magazine] [visitable] [item] [item_location]") {
    clear_all_state();
    const itype_id gun_id("m4a1");
    const ammotype gun_ammo("223");
    const itype_id ammo_id("556");      // any type of compatible ammo
    const itype_id alt_ammo("223");     // any alternative type of compatible ammo
    const itype_id bad_ammo("9mm");     // any type of incompatible ammo
    const itype_id mag_id("stanag30");  // must be set to default magazine
    const itype_id bad_mag("glockmag"); // any incompatible magazine
    const int mag_cap = 30;

    CHECK(ammo_id != alt_ammo);
    CHECK(ammo_id != bad_ammo);
    CHECK(alt_ammo != bad_ammo);
    CHECK(mag_id != bad_mag);
    CHECK(mag_cap > 0);

    player& p = g->u;
    p.worn.clear();
    p.inv_clear();
    p.remove_primary_weapon();
    p.wear_item(item::spawn("backpack")); // so we don't drop anything
    detached_ptr<item> det = item::spawn(mag_id);
    item& mag = *det;
    p.i_add(std::move(det));
    CHECK(mag.is_magazine() == true);
    CHECK(mag.is_reloadable() == true);
    CHECK(mag.is_reloadable_with(ammo_id) == true);
    CHECK(mag.is_reloadable_with(alt_ammo) == true);
    CHECK(mag.is_reloadable_with(bad_ammo) == false);
    CHECK(p.can_reload(mag) == true);
    CHECK(p.can_reload(mag, ammo_id) == true);
    CHECK(p.can_reload(mag, alt_ammo) == true);
    CHECK(p.can_reload(mag, bad_ammo) == false);
    CHECK(mag.ammo_types().count(gun_ammo));
    CHECK(mag.ammo_capacity() == mag_cap);
    CHECK(mag.ammo_current().is_null());
    CHECK(mag.ammo_data() == nullptr);

    GIVEN("An empty magazine") {
        CHECK(mag.ammo_remaining() == 0);

        WHEN("the magazine is reloaded with incompatible ammo") {
            det = item::spawn(bad_ammo);
            item& ammo = *det;
            p.i_add(std::move(det));
            bool ok = mag.reload(g->u, ammo, mag.ammo_capacity());
            THEN("reloading should fail") {
                REQUIRE_FALSE(ok);
                REQUIRE(mag.ammo_remaining() == 0);
            }
        }

        WHEN("the magazine is loaded with an excess of ammo") {
            det = item::spawn(ammo_id, calendar::turn, mag_cap + 5);
            item& ammo = *det;
            p.i_add(std::move(det));
            REQUIRE(ammo.charges == mag_cap + 5);

            bool ok = mag.reload(g->u, ammo, mag.ammo_capacity());
            THEN("reloading is successful") {
                REQUIRE(ok);

                AND_THEN("the current ammo is updated") {
                    REQUIRE(mag.ammo_current() == ammo_id);
                    REQUIRE(mag.ammo_data());
                }
                AND_THEN("the magazine is filled to capacity") {
                    REQUIRE(mag.ammo_remaining() == mag.ammo_capacity());
                }
                AND_THEN("a single correctly sized ammo stack remains in the inventory") {
                    std::vector<const item*> found;
                    p.visit_items([&ammo_id, &found](const item* e) {
                        if (e->typeId() == ammo_id) { found.push_back(e); }
                        // ignore ammo contained within guns or magazines
                        return (e->is_gun() || e->is_magazine())
                                 ? VisitResponse::SKIP
                                 : VisitResponse::NEXT;
                    });
                    REQUIRE(found.size() == 1);
                    REQUIRE(found[0]->charges == 5);
                }
            }
        }

        WHEN("the magazine is partially reloaded with compatible ammo") {
            det = item::spawn(ammo_id, calendar::turn, mag_cap - 2);
            item& ammo = *det;
            p.i_add(std::move(det));
            REQUIRE(ammo.charges == mag_cap - 2);

            bool ok = mag.reload(g->u, ammo, mag.ammo_capacity());
            THEN("reloading is successful") {
                REQUIRE(ok == true);

                AND_THEN("the current ammo is updated") {
                    REQUIRE(mag.ammo_current() == ammo_id);
                    REQUIRE(mag.ammo_data());
                }
                AND_THEN("the magazine is filled with the correct quantity") {
                    REQUIRE(mag.ammo_remaining() == mag_cap - 2);
                }
                AND_THEN("the ammo stack was completely used") {
                    std::vector<const item*> found;
                    p.visit_items([&ammo_id, &found](const item* e) {
                        if (e->typeId() == ammo_id) { found.push_back(e); }
                        // ignore ammo contained within guns or magazines
                        return (e->is_gun() || e->is_magazine())
                                 ? VisitResponse::SKIP
                                 : VisitResponse::NEXT;
                    });
                    REQUIRE(found.empty());
                }
            }

            AND_WHEN("the magazine is further reloaded with matching ammo") {
                det = item::spawn(ammo_id, calendar::turn, 10);
                item& ammo = *det;
                p.i_add(std::move(det));
                REQUIRE(ammo.charges == 10);
                REQUIRE(mag.ammo_remaining() == mag_cap - 2);

                bool ok = mag.reload(g->u, ammo, mag.ammo_capacity());
                THEN("further reloading is successful") {
                    REQUIRE(ok);

                    AND_THEN("the magazine is filled to capacity") {
                        REQUIRE(mag.ammo_remaining() == mag.ammo_capacity());
                    }
                    AND_THEN("a single correctly sized ammo stack remains in the inventory") {
                        std::vector<const item*> found;
                        p.visit_items([&ammo_id, &found](const item* e) {
                            if (e->typeId() == ammo_id) { found.push_back(e); }
                            // ignore ammo contained within guns or magazines
                            return (e->is_gun() || e->is_magazine())
                                     ? VisitResponse::SKIP
                                     : VisitResponse::NEXT;
                        });
                        REQUIRE(found.size() == 1);
                        REQUIRE(found[0]->charges == 8);
                    }
                }
            }

            AND_WHEN("the magazine is further reloaded with compatible but different ammo") {
                det = item::spawn(alt_ammo);
                item& ammo = *det;
                p.i_add(std::move(det));
                bool ok = mag.reload(g->u, ammo, mag.ammo_capacity());
                THEN("further reloading should fail") {
                    REQUIRE_FALSE(ok);
                    REQUIRE(mag.ammo_remaining() == mag_cap - 2);
                }
            }

            AND_WHEN("the magazine is further reloaded with incompatible ammo") {
                det = item::spawn(bad_ammo);
                item& ammo = *det;
                p.i_add(std::move(det));
                bool ok = mag.reload(g->u, ammo, mag.ammo_capacity());
                THEN("further reloading should fail") {
                    REQUIRE_FALSE(ok);
                    REQUIRE(mag.ammo_remaining() == mag_cap - 2);
                }
            }
        }
    }

    GIVEN("an empty gun without an integral magazine") {
        det = item::spawn(gun_id);
        item& gun = *det;
        p.i_add(std::move(det));
        CHECK(gun.is_gun() == true);
        CHECK(gun.is_reloadable() == true);
        CHECK(p.can_reload(gun) == true);
        CHECK(p.can_reload(gun, mag_id) == true);
        CHECK(p.can_reload(gun, ammo_id) == false);
        CHECK(gun.magazine_integral() == false);
        CHECK(gun.magazine_default() == mag_id);
        CHECK(gun.magazine_compatible().count(mag_id) == 1);
        CHECK(gun.magazine_current() == nullptr);
        CHECK(gun.ammo_types().count(gun_ammo));
        CHECK(gun.ammo_capacity() == 0);
        CHECK(gun.ammo_remaining() == 0);
        CHECK(gun.ammo_current().is_null());
        CHECK(gun.ammo_data() == nullptr);

        WHEN("the gun is reloaded with an incompatible magazine") {
            det = item::spawn(bad_mag);
            item& mag = *det;
            p.i_add(std::move(det));
            bool ok = gun.reload(g->u, mag, 1);
            THEN("reloading should fail") {
                REQUIRE_FALSE(ok);
                REQUIRE_FALSE(gun.magazine_current());
            }
        }

        WHEN("the gun is reloaded with an empty compatible magazine") {
            CHECK(mag.ammo_remaining() == 0);

            bool ok = gun.reload(g->u, mag, 1);
            THEN("reloading is successful") {
                REQUIRE(ok);
                REQUIRE(gun.magazine_current());

                AND_THEN("the gun contains the correct magazine") {
                    REQUIRE(gun.magazine_current()->typeId() == mag_id);
                }
                AND_THEN("the ammo type for the gun remains unchanged") {
                    REQUIRE(gun.ammo_types().count(gun_ammo));
                }
                AND_THEN("the ammo capacity is correctly set") {
                    REQUIRE(gun.ammo_capacity() == mag_cap);
                }
                AND_THEN("the gun contains no ammo") {
                    REQUIRE(gun.ammo_current().is_null());
                    REQUIRE(gun.ammo_remaining() == 0);
                    REQUIRE(gun.ammo_data() == nullptr);
                }
            }
        }

        WHEN("the gun is reloaded with a partially filled compatible magazine") {
            mag.ammo_set(ammo_id, mag_cap - 2);
            CHECK(mag.ammo_current() == ammo_id);
            CHECK(mag.ammo_remaining() == mag_cap - 2);

            bool ok = gun.reload(g->u, mag, 1);
            THEN("reloading is successful") {
                REQUIRE(ok);
                REQUIRE(gun.magazine_current());

                AND_THEN("the gun contains the correct magazine") {
                    REQUIRE(gun.magazine_current()->typeId() == mag_id);
                }
                AND_THEN("the ammo type for the gun remains unchanged") {
                    REQUIRE(gun.ammo_types().count(gun_ammo));
                }
                AND_THEN("the ammo capacity is correctly set") {
                    REQUIRE(gun.ammo_capacity() == mag_cap);
                }
                AND_THEN("the gun contains the correct amount and type of ammo") {
                    REQUIRE(gun.ammo_remaining() == mag_cap - 2);
                    REQUIRE(gun.ammo_current() == ammo_id);
                    REQUIRE(gun.ammo_data());
                }

                AND_WHEN(
                    "the guns magazine is further reloaded with compatible but different "
                    "ammo") {
                    det = item::spawn(alt_ammo, calendar::turn, 10);
                    item& ammo = *det;
                    p.i_add(std::move(det));
                    bool ok = gun.magazine_current()->reload(g->u, ammo, 10);
                    THEN("further reloading should fail") {
                        REQUIRE_FALSE(ok);
                        REQUIRE(gun.ammo_remaining() == mag_cap - 2);
                    }
                }

                AND_WHEN("the guns magazine is further reloaded with incompatible ammo") {
                    det = item::spawn(bad_ammo, calendar::turn, 10);
                    item& ammo = *det;
                    p.i_add(std::move(det));
                    bool ok = gun.magazine_current()->reload(g->u, ammo, 10);
                    THEN("further reloading should fail") {
                        REQUIRE_FALSE(ok);
                        REQUIRE(gun.ammo_remaining() == mag_cap - 2);
                    }
                }

                AND_WHEN("the guns magazine is further reloaded with matching ammo") {
                    det = item::spawn(ammo_id, calendar::turn, 10);
                    item& ammo = *det;
                    p.i_add(std::move(det));
                    REQUIRE(ammo.charges == 10);

                    bool ok = gun.magazine_current()->reload(g->u, ammo, 10);
                    THEN("further reloading is successful") {
                        REQUIRE(ok);

                        AND_THEN("the guns contained magazine is filled to capacity") {
                            REQUIRE(gun.ammo_remaining() == mag_cap);
                        }
                        AND_THEN("a single correctly sized ammo stack remains in the inventory") {
                            std::vector<const item*> found;
                            p.visit_items([&ammo_id, &found](const item* e) {
                                if (e->typeId() == ammo_id) { found.push_back(e); }
                                // ignore ammo contained within guns or magazines
                                return (e->is_gun() || e->is_magazine())
                                         ? VisitResponse::SKIP
                                         : VisitResponse::NEXT;
                            });
                            REQUIRE(found.size() == 1);
                            REQUIRE(found[0]->charges == 8);
                        }
                    }
                }
            }
        }
    }
}
