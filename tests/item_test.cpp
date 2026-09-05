#include "ammo_effect.h"
#include "cached_item_options.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "enums.h"
#include "field_type.h"
#include "flag.h"
#include "item.h"
#include "itype.h"
#include "math_defines.h"
#include "ret_val.h"
#include "type_id.h"
#include "units.h"
#include "value_ptr.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

static_assert(std::is_same_v<units::volume::value_type, std::int64_t>);

TEST_CASE("item_volume", "[item]") {
    // Need to pick some item here which is count_by_charges and for which each
    // charge is at least 1_ml.  Battery works for now.
    item& i = *item::spawn_temporary(
        "battery", calendar::start_of_cataclysm, item::default_charges_tag());
    REQUIRE(i.count_by_charges());
    // Would be better with Catch2 generators
    const units::volume big_volume = units::from_milliliter(std::numeric_limits<int>::max() / 2);
    for (units::volume v : {0_ml, 1_ml, i.volume(), big_volume}) {
        INFO("checking batteries that fit in " << v);
        const int charges_that_should_fit = i.charges_per_volume(v);
        i.charges = charges_that_should_fit;
        CHECK(i.volume() <= v); // this many charges should fit
        i.charges++;
        CHECK(i.volume() > v); // one more charge should not fit
    }
}

TEST_CASE("solitary_item_has_no_charges", "[item]") {
    const item& grenade =
        *item::spawn_temporary("grenade", calendar::start_of_cataclysm, item::solitary_tag());

    REQUIRE_FALSE(grenade.count_by_charges());
    CHECK(grenade.charges == 0);
}

TEST_CASE("large_item_storage_volumes", "[item][volume]") {
    constexpr auto legacy_int_limit_ml = static_cast<std::int64_t>(std::numeric_limits<int>::max());
    const auto volume_above_legacy_int_limit = units::from_milliliter(legacy_int_limit_ml + 1);
    CHECK(units::to_milliliter(volume_above_legacy_int_limit) == legacy_int_limit_ml + 1);

    const item& large_container = *item::spawn_temporary("test_large_container");
    CHECK(large_container.get_container_capacity() == 3000000_liter);
}

TEST_CASE("charge_volume_calculation_uses_wide_intermediate", "[item][volume]") {
    item& battery = *item::spawn_temporary(
        "battery", calendar::start_of_cataclysm, item::default_charges_tag());
    REQUIRE(battery.count_by_charges());

    const auto volume_with_int_overflowing_product = units::from_milliliter(
        static_cast<std::int64_t>(std::numeric_limits<int>::max()) / 2 + 1);
    CHECK(battery.charges_per_volume(volume_with_int_overflowing_product)
          == units::to_milliliter(volume_with_int_overflowing_product));
    CHECK(battery.charges_per_volume(units::volume_max) == item::INFINITE_CHARGES);
    CHECK(battery.type->charges_per_volume(units::volume_max) == item::INFINITE_CHARGES);
}

TEST_CASE("simple_item_layers", "[item]") {
    CHECK(item::spawn_temporary("arm_warmers")->get_layer() == UNDERWEAR_LAYER);
    CHECK(item::spawn_temporary("10gal_hat")->get_layer() == REGULAR_LAYER);
    CHECK(item::spawn_temporary("baldric")->get_layer() == WAIST_LAYER);
    CHECK(item::spawn_temporary("aep_suit")->get_layer() == OUTER_LAYER);
    CHECK(item::spawn_temporary("2byarm_guard")->get_layer() == BELTED_LAYER);
}

TEST_CASE("gun_layer", "[item]") {
    item& gun = *item::spawn_temporary("win70");
    detached_ptr<item> mod = item::spawn("shoulder_strap");
    CHECK(gun.is_gunmod_compatible(*mod).success());
    gun.put_in(std::move(mod));
    CHECK(gun.get_layer() == BELTED_LAYER);
}

TEST_CASE("ethereal_item_with_malformed_counter_expires_without_throwing", "[item]") {
    auto ethereal = item::spawn("rock");
    ethereal->set_flag(flag_ETHEREAL_ITEM);
    ethereal->set_var("ethereal", "not-a-number");

    CHECK_NOTHROW(
        ethereal = item::process(std::move(ethereal), nullptr, tripoint_bub_ms::zero(), false, 1));
    CHECK_FALSE(ethereal);
}

TEST_CASE("gun_cycle_mode_wraps_from_last_to_first", "[item]") {
    item& reach_bow = *item::spawn_temporary("reach_bow");
    const auto modes = reach_bow.gun_all_modes();

    REQUIRE(modes.size() >= 2);

    const auto first_mode = modes.begin()->first;
    const auto second_mode = std::next(modes.begin())->first;
    const auto last_mode = std::prev(modes.end())->first;

    REQUIRE(reach_bow.gun_set_mode(first_mode));
    reach_bow.gun_cycle_mode();
    CHECK(reach_bow.gun_get_mode_id() == second_mode);

    REQUIRE(reach_bow.gun_set_mode(last_mode));
    reach_bow.gun_cycle_mode();
    CHECK(reach_bow.gun_get_mode_id() == first_mode);
}

TEST_CASE("common_liquids_define_spill_fields", "[item][liquid][field]") {
    CHECK(item::spawn_temporary("water")->type->spill_field == field_type_id("fd_water"));
    CHECK(item::spawn_temporary("salt_water")->type->spill_field == field_type_id("fd_salt_water"));
    CHECK(
        item::spawn_temporary("soapy_water")->type->spill_field == field_type_id("fd_soapy_water"));
    CHECK(item::spawn_temporary("water_sewage")->type->spill_field == field_type_id("fd_sewage"));
    CHECK(item::spawn_temporary("gasoline")->type->spill_field == field_type_id("fd_fuel"));
    CHECK(item::spawn_temporary("motor_oil")->type->spill_field == field_type_id("fd_oil"));
    CHECK(item::spawn_temporary("plut_slurry")->type->spill_field
          == field_type_id("fd_plutonium_slurry"));
    CHECK(field_type_id("fd_water").obj().get_tint() == c_blue);
    CHECK(field_type_id("fd_water").obj().get_tint_rgb() == RGBColor::try_parse("blue"));
    CHECK(field_type_id("fd_sewage").obj().get_tint() == c_cyan);
    CHECK(field_type_id("test_fd_rgb_tint").obj().get_tint() == c_unset);
    CHECK(field_type_id("test_fd_rgb_tint").obj().get_tint_rgb() == RGBColor::try_parse("#123456"));
    CHECK(item::spawn_temporary("soapy_water")->ammo_type() == ammotype("water"));
    CHECK(field_type_id("fd_soapy_water").obj().get_intensity_level().field_effects.size() == 1);
    CHECK(field_type_id("fd_soapy_water").obj().get_intensity_level().field_effects.front().id
          == efftype_id("downed"));
    CHECK(field_type_id("fd_plutonium_slurry").obj().get_extra_radiation_max(0) == 1);
    CHECK(field_type_id("fd_plutonium_slurry").obj().get_extra_radiation_max(1) == 2);
    CHECK(field_type_id("fd_plutonium_slurry").obj().get_extra_radiation_max(2) == 3);
}

TEST_CASE("super_soaker_uses_water_without_mount_restrictions", "[item][gun]") {
    item& squirt_gun = *item::spawn_temporary("super_soaker");

    CHECK(squirt_gun.ammo_types().count(ammotype("water")) == 1);
    CHECK(squirt_gun.ammo_default() == itype_id("water"));
    CHECK_FALSE(squirt_gun.has_flag(flag_MOUNTED_GUN));
}

TEST_CASE("water_cannons_inherit_liquid_trail_effects_from_ammo", "[item][gun][field]") {
    item& squirt_gun = *item::spawn_temporary("super_soaker");

    squirt_gun.ammo_set(itype_id("water"), 1);
    CHECK(squirt_gun.ammo_effects().contains(ammo_effect_str_id("STREAM_WATER")));

    squirt_gun.ammo_set(itype_id("soapy_water"), 1);
    CHECK(squirt_gun.ammo_effects().contains(ammo_effect_str_id("STREAM_SOAPY_WATER")));
}

TEST_CASE("stacking_cash_cards", "[item]") {
    // Differently-charged cash cards should stack if neither is zero.
    item& cash0 = *item::spawn_temporary("cash_card", calendar::turn_zero, 0);
    item& cash1 = *item::spawn_temporary("cash_card", calendar::turn_zero, 1);
    item& cash2 = *item::spawn_temporary("cash_card", calendar::turn_zero, 2);
    CHECK(!cash0.stacks_with(cash1));
    CHECK(cash1.stacks_with(cash2));
}

// second minute hour day week season year

TEST_CASE("stacking_over_time", "[item]") {
    item& A = *item::spawn_temporary("bologna");
    item& B = *item::spawn_temporary("bologna");

    GIVEN("Two items with the same birthday (stack mode: legacy)") {
        merge_comestible_mode = merge_comestible_t::merge_legacy;

        REQUIRE(A.stacks_with(B));
        WHEN("the items are aged different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 1_turns);
            B.mod_rot(B.type->comestible->spoils - 3_turns);
            THEN("they don't stack") { CHECK(!A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the minute but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 5_minutes);
            B.mod_rot(B.type->comestible->spoils - 5_minutes);
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different minutes") {
            A.mod_rot(A.type->comestible->spoils - 5_minutes);
            B.mod_rot(B.type->comestible->spoils - 5_minutes);
            B.mod_rot(5_turns);
            THEN("they don't stack") { CHECK(!A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the hour but different numbers of minutes") {
            A.mod_rot(A.type->comestible->spoils - 5_hours);
            B.mod_rot(B.type->comestible->spoils - 5_hours);
            B.mod_rot(-5_minutes);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different hours") {
            A.mod_rot(A.type->comestible->spoils - 5_hours);
            B.mod_rot(B.type->comestible->spoils - 5_hours);
            B.mod_rot(5_turns);
            THEN("they don't stack") { CHECK(!A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the day but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 3_days);
            B.mod_rot(B.type->comestible->spoils - 3_days);
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different days") {
            A.mod_rot(A.type->comestible->spoils - 3_days);
            B.mod_rot(B.type->comestible->spoils - 3_days);
            B.mod_rot(5_turns);
            THEN("they don't stack") { CHECK(!A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the week but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 7_days);
            B.mod_rot(B.type->comestible->spoils - 7_days);
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different weeks") {
            A.mod_rot(A.type->comestible->spoils - 7_days);
            B.mod_rot(B.type->comestible->spoils - 7_days);
            B.mod_rot(5_turns);
            THEN("they don't stack") { CHECK(!A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the season but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - calendar::season_length());
            B.mod_rot(B.type->comestible->spoils - calendar::season_length());
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different seasons") {
            A.mod_rot(A.type->comestible->spoils - calendar::season_length());
            B.mod_rot(B.type->comestible->spoils - calendar::season_length());
            B.mod_rot(5_turns);
            THEN("they don't stack") { CHECK(!A.stacks_with(B)); }
        }
    }

    GIVEN("Two items with the same birthday (stack mode: all)") {
        merge_comestible_mode = merge_comestible_t::merge_all;
        similarity_threshold = 1.0f;

        REQUIRE(A.stacks_with(B));
        WHEN("the items are aged different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 1_turns);
            B.mod_rot(B.type->comestible->spoils - 3_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the minute but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 5_minutes);
            B.mod_rot(B.type->comestible->spoils - 5_minutes);
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different minutes") {
            A.mod_rot(A.type->comestible->spoils - 5_minutes);
            B.mod_rot(B.type->comestible->spoils - 5_minutes);
            B.mod_rot(5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the hour but different numbers of minutes") {
            A.mod_rot(A.type->comestible->spoils - 5_hours);
            B.mod_rot(B.type->comestible->spoils - 5_hours);
            B.mod_rot(-5_minutes);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different hours") {
            A.mod_rot(A.type->comestible->spoils - 5_hours);
            B.mod_rot(B.type->comestible->spoils - 5_hours);
            B.mod_rot(5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the day but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 3_days);
            B.mod_rot(B.type->comestible->spoils - 3_days);
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different days") {
            A.mod_rot(A.type->comestible->spoils - 3_days);
            B.mod_rot(B.type->comestible->spoils - 3_days);
            B.mod_rot(5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the week but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - 7_days);
            B.mod_rot(B.type->comestible->spoils - 7_days);
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different weeks") {
            A.mod_rot(A.type->comestible->spoils - 7_days);
            B.mod_rot(B.type->comestible->spoils - 7_days);
            B.mod_rot(5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged the same to the season but different numbers of seconds") {
            A.mod_rot(A.type->comestible->spoils - calendar::season_length());
            B.mod_rot(B.type->comestible->spoils - calendar::season_length());
            B.mod_rot(-5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
        WHEN("the items are aged a few seconds different but different seasons") {
            A.mod_rot(A.type->comestible->spoils - calendar::season_length());
            B.mod_rot(B.type->comestible->spoils - calendar::season_length());
            B.mod_rot(5_turns);
            THEN("they stack") { CHECK(A.stacks_with(B)); }
        }
    }
}


TEST_CASE("magazine_copyfrom_extends", "[item]") {
    item gun("glock_19");
    CHECK(gun.magazine_compatible().count(itype_id("glockmag_test")) > 0);
    CHECK(gun.magazine_compatible().count(itype_id("glockmag")) > 0);
}

TEST_CASE("armor_override", "[item]") {
    item armor("test_override_armor");
    CHECK(armor.bullet_resist() > 100);
    CHECK(armor.cut_resist() == 0);
}

TEST_CASE("armor_override_damaged", "[item]") {
    item armor_undamaged("test_override_armor_damageable");
    item armor("test_override_armor_damageable");
    armor.inc_damage();
    CHECK(armor_undamaged.bullet_resist() > armor.bullet_resist());
}

TEST_CASE("items_have_default_attack_statblocks", "[item]") {
    item sword("test_balanced_sword");
    REQUIRE(sword.type != nullptr);
    const itype& sword_type = *sword.type;

    REQUIRE(sword_type.attacks.size() == 1);
    const attack_statblock& attack = sword_type.attacks.begin()->second;
    CHECK(attack.to_hit == sword_type.m_to_hit);
    for (const damage_unit& du : attack.damage.damage_units) {
        CHECK(du.amount == sword_type.melee[du.type]);
        CHECK(du.damage_multiplier == 1.0f);
        CHECK(du.res_mult == 1.0f);
        CHECK(du.res_pen == 0.0f);
    }
}

TEST_CASE("stacking_corpses", "[item]") {
    item& human_corpse1 = *item::make_corpse();
    item& human_corpse2 = *item::make_corpse();
    item& non_human_corpse1 = *item::make_corpse(mtype_id("mon_dog"));
    item& non_human_corpse2 = *item::make_corpse(mtype_id("mon_rabbit"));
    item not_a_corpse("test_rock");

    WHEN("not all corpses") {
        THEN("corpses only stacks with corpses") {
            CHECK(human_corpse1.stacks_with(human_corpse2));
            CHECK(!human_corpse1.stacks_with(not_a_corpse));
        }
    }

    WHEN("corpse type different") {
        THEN("they don't stack") { CHECK(!human_corpse1.stacks_with(non_human_corpse1)); }
    }

    WHEN("corpses differently damaged") {
        human_corpse2.inc_damage();
        THEN("should still stack") { CHECK(human_corpse1.stacks_with(human_corpse2)); }
    }

    WHEN("default merge_comestible_mode doesn't stack corpses like rottable food") {
        merge_comestible_mode = merge_comestible_t::merge_all;

        THEN("different corpses don't stack") {
            CHECK(!non_human_corpse1.stacks_with(non_human_corpse2));
        }
    }
}

TEST_CASE("gunmod_weight_volume_test", "[item][gunmod]") {
    // All test items are defined in data/mods/TEST_DATA/items.json
    // glock_19: weight=600g, volume=500ml
    // test_mod_neg_int: integral_weight=-100g, integral_volume=-100ml
    // test_mod_mult: weight_multiplier=0.90, volume_multiplier=0.90
    // test_mod_clamp: integral_weight=-999g, integral_volume=-999ml

    SECTION("negative integral modifier reduces gun weight and volume") {
        detached_ptr<item> gun = item::spawn("glock_19");
        REQUIRE(gun);

        const units::mass w0 = gun->weight();
        const units::volume v0 = gun->volume();
        INFO("base weight: " << w0 << "  base volume: " << v0);

        detached_ptr<item> mod = item::spawn("test_mod_neg_int");
        REQUIRE(gun->is_gunmod_compatible(*mod).success());
        gun->put_in(std::move(mod));

        CHECK(gun->weight() == w0 - 100_gram);
        CHECK(gun->volume() == v0 - 100_ml);
    }

    SECTION("multiplier modifier reduces gun weight and volume") {
        detached_ptr<item> gun = item::spawn("glock_19");
        REQUIRE(gun);

        const units::mass w0 = gun->weight();
        const units::volume v0 = gun->volume();

        detached_ptr<item> mod = item::spawn("test_mod_mult");
        REQUIRE(gun->is_gunmod_compatible(*mod).success());
        gun->put_in(std::move(mod));

        CHECK(gun->weight() == w0 * 9 / 10);
        CHECK(gun->volume() == v0 * 9 / 10);
    }

    SECTION("both mods together apply multiplicative then additive") {
        detached_ptr<item> gun = item::spawn("glock_19");
        REQUIRE(gun);

        const units::mass w0 = gun->weight();
        const units::volume v0 = gun->volume();

        detached_ptr<item> mod_mult = item::spawn("test_mod_mult");
        REQUIRE(gun->is_gunmod_compatible(*mod_mult).success());
        gun->put_in(std::move(mod_mult));

        detached_ptr<item> mod_neg = item::spawn("test_mod_neg_int");
        REQUIRE(gun->is_gunmod_compatible(*mod_neg).success());
        gun->put_in(std::move(mod_neg));

        CHECK(gun->weight() == w0 * 9 / 10 - 100_gram);
        CHECK(gun->volume() == v0 * 9 / 10 - 100_ml);
    }

    SECTION("clamping at 1 percent prevents near-zero values") {
        detached_ptr<item> gun = item::spawn("glock_19");
        REQUIRE(gun);

        const units::mass w0 = gun->weight();
        const units::volume v0 = gun->volume();

        detached_ptr<item> mod = item::spawn("test_mod_clamp");
        REQUIRE(gun->is_gunmod_compatible(*mod).success());
        gun->put_in(std::move(mod));

        CHECK(gun->weight() == std::max(w0 - 999_gram, w0 / 100));
        CHECK(gun->volume() == std::max(v0 - 999_ml, v0 / 100));
    }
}
