#include "activity_handlers.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "memory_fast.h"
#include "monster.h"
#include "mtype.h"
#include "player_activity.h"
#include "point.h"
#include "skill.h"
#include "state_helpers.h"
#include "type_id.h"

#include <cmath>
#include <string>

static const activity_id ACT_TRAIN_PET("ACT_TRAIN_PET");

static const efftype_id effect_ai_waiting("ai_waiting");
static const efftype_id effect_pet("pet");
static const efftype_id effect_well_fed("well_fed");

static const mtype_id mon_dog("mon_dog");
static const mtype_id mon_dog_gshepherd("mon_dog_gshepherd");

static const skill_id skill_survival("survival");

// A position well away from the player's test location so
// the monster's line-of-sight checks do not interfere with tests.
static const tripoint_bub_ms mon_pos(10, 10, 0);

// -----------------------------------------------------------------------
// make_pet / is_pet
// -----------------------------------------------------------------------

TEST_CASE("make_pet sets friendly to -1", "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    REQUIRE(mon.friendly == 0);
    mon.make_pet();
    CHECK(mon.friendly == -1);
}

TEST_CASE("make_pet adds effect_pet", "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    REQUIRE_FALSE(mon.has_effect(effect_pet));
    mon.make_pet();
    CHECK(mon.has_effect(effect_pet));
}

TEST_CASE(
    "is_pet returns true only when both friendly==-1 and effect_pet present",
    "[pet]["
    "monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    SECTION("unfriendly monster is not a pet") {
        mon.friendly = 0;
        CHECK_FALSE(mon.is_pet());
    }

    SECTION("make_friendly monster (friendly > 0) is not a pet") {
        mon.make_friendly();
        REQUIRE(mon.friendly > 0);
        CHECK_FALSE(mon.is_pet());
    }

    SECTION("friendly=-1 without effect_pet is not a pet") {
        mon.friendly = -1;
        REQUIRE_FALSE(mon.has_effect(effect_pet));
        CHECK_FALSE(mon.is_pet());
    }

    SECTION("make_pet produces a true pet") {
        mon.make_pet();
        CHECK(mon.is_pet());
    }
}


TEST_CASE("make_pet does not reset training_level", "[pet][monster][training]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    mon.make_pet();
    mon.training_level = 2;

    // Calling make_pet again (e.g. re-feeding) must preserve training_level
    mon.make_pet();
    CHECK(mon.training_level == 2);
}

TEST_CASE("repeated make_pet calls preserve is_pet status", "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    mon.make_pet();
    REQUIRE(mon.is_pet());

    mon.make_pet();
    CHECK(mon.is_pet());
}

// -----------------------------------------------------------------------
// effect_well_fed
// -----------------------------------------------------------------------

TEST_CASE("adding effect_well_fed to a pet works", "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();

    REQUIRE_FALSE(mon.has_effect(effect_well_fed));
    mon.add_effect(effect_well_fed, 24_hours);
    CHECK(mon.has_effect(effect_well_fed));
}

TEST_CASE("effect_well_fed duration is refreshed on re-add", "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();

    mon.add_effect(effect_well_fed, 1_hours);
    const time_duration first = mon.get_effect_dur(effect_well_fed);

    mon.add_effect(effect_well_fed, 24_hours);
    const time_duration second = mon.get_effect_dur(effect_well_fed);

    CHECK(second > first);
}

// -----------------------------------------------------------------------
// Training stat scaling
// -----------------------------------------------------------------------

TEST_CASE("pet training stat scaling with training_level", "[pet][monster][training]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    REQUIRE(mon.type->pet_training.has_value());

    const int base_hp = mon.type->hp;
    const float base_melee = mon.type->melee_skill;
    const float base_dodge = mon.type->sk_dodge;
    const float hp_mult = mon.type->pet_training->hp;
    const float melee_mult = mon.type->pet_training->melee;
    const float dodge_mult = mon.type->pet_training->dodge;

    SECTION("level 0: stats equal base") {
        mon.training_level = 0;
        CHECK(mon.get_hp_max() == base_hp);
        CHECK(mon.get_melee() == Approx(base_melee));
        CHECK(mon.get_dodge_base() == Approx(base_dodge));
    }

    SECTION("level 1: stats scaled by multiplier^1") {
        mon.training_level = 1;
        CHECK(mon.get_hp_max() == static_cast<int>(base_hp * std::pow(hp_mult, 1)));
        CHECK(mon.get_melee() == Approx(base_melee * std::pow(melee_mult, 1)));
        CHECK(mon.get_dodge_base() == Approx(base_dodge * std::pow(dodge_mult, 1)));
    }

    SECTION("level 2: stats scaled by multiplier^2") {
        mon.training_level = 2;
        CHECK(mon.get_hp_max() == static_cast<int>(base_hp * std::pow(hp_mult, 2)));
        CHECK(mon.get_melee() == Approx(base_melee * std::pow(melee_mult, 2)));
        CHECK(mon.get_dodge_base() == Approx(base_dodge * std::pow(dodge_mult, 2)));
    }

    SECTION("higher level gives better stats than lower level") {
        mon.training_level = 1;
        const int hp1 = mon.get_hp_max();
        mon.training_level = 2;
        const int hp2 = mon.get_hp_max();
        CHECK(hp2 > hp1);
    }
}

TEST_CASE("training_level is capped at max_level", "[pet][monster][training]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    REQUIRE(mon.type->pet_training.has_value());

    const int max_level = mon.type->pet_training->max_level;
    mon.training_level = max_level;

    const int hp_at_max = mon.get_hp_max();
    const int base_hp = mon.type->hp;
    CHECK(hp_at_max > base_hp);

    // Going above max_level in the raw field should not happen, but if it
    // did the formula would scale — verify it's not capped by the getter.
    // The capping is enforced in train_pet_finish, not in get_hp_max.
    mon.training_level = max_level + 1;
    CHECK(mon.get_hp_max() > hp_at_max);
}

// -----------------------------------------------------------------------
// train_pet_finish
// -----------------------------------------------------------------------

TEST_CASE(
    "train_pet_finish with null monster reference does not crash",
    "[pet][monster][training]["
    "activity]") {
    clear_all_state();
    avatar& p = get_avatar();

    player_activity act(ACT_TRAIN_PET);
    // Push an expired (default-constructed, never-assigned) weak_ptr so
    // lock() will return nullptr, exercising the null-guard we added.
    act.monsters.push_back(weak_ptr_fast<monster>{});
    act.str_values.push_back("test_pet");

    // Must not crash and must set the activity to null.
    activity_handlers::train_pet_finish(&act, &p);
    CHECK(act.is_null());
}

TEST_CASE("train_pet_finish removes effect_well_fed", "[pet][monster][training][activity]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.add_effect(effect_well_fed, 24_hours);
    REQUIRE(mon.has_effect(effect_well_fed));

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    // Ensure skill meets the minimum so we don't early-return before consuming well_fed.
    REQUIRE(mon.type->pet_training.has_value());
    p.set_skill_level(skill_survival, mon.type->pet_training->min_skill);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    CHECK_FALSE(mon.has_effect(effect_well_fed));
}

TEST_CASE(
    "train_pet_finish increments training_level on success",
    "[pet][monster][training]["
    "activity]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.add_effect(effect_well_fed, 24_hours);
    mon.training_level = 0;

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    // Skill 25 guarantees success: 4*25=100 >= rng(0,100) always.
    p.set_skill_level(skill_survival, 25);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    CHECK(mon.training_level == 1);
}

TEST_CASE("train_pet_finish does not exceed max_level", "[pet][monster][training][activity]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    REQUIRE(mon.type->pet_training.has_value());

    const int max_level = mon.type->pet_training->max_level;
    mon.training_level = max_level;
    mon.add_effect(effect_well_fed, 24_hours);

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    p.set_skill_level(skill_survival, 25);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    CHECK(mon.training_level == max_level);
}

TEST_CASE(
    "train_pet_finish with insufficient skill does not increment training_level",
    "[pet]["
    "monster]["
    "training]["
    "activity"
    "]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.add_effect(effect_well_fed, 24_hours);
    mon.training_level = 0;

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    REQUIRE(mon.type->pet_training.has_value());
    // Set skill below the minimum required for this pet.
    p.set_skill_level(skill_survival, mon.type->pet_training->min_skill - 1);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    // Should return early without touching training_level or well_fed.
    CHECK(mon.training_level == 0);
    CHECK(mon.has_effect(effect_well_fed));
}

TEST_CASE(
    "training_level is preserved after re-feeding (make_pet called again)",
    "[pet][monster]["
    "training]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.training_level = 2;

    // Simulate re-feeding: make_pet is called again.
    mon.make_pet();

    CHECK(mon.training_level == 2);
    CHECK(mon.is_pet());
}

TEST_CASE(
    "train_pet_finish removes effect_ai_waiting so pet is not stuck after training",
    "[pet]["
    "monster"
    "]["
    "trainin"
    "g]["
    "activit"
    "y]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.add_effect(effect_well_fed, 24_hours);
    // Simulate what monexamine::train_pet does when the activity starts.
    mon.add_effect(effect_ai_waiting, 60_minutes);
    REQUIRE(mon.has_effect(effect_ai_waiting));

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    REQUIRE(mon.type->pet_training.has_value());
    p.set_skill_level(skill_survival, mon.type->pet_training->min_skill);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    CHECK_FALSE(mon.has_effect(effect_ai_waiting));
}

// -----------------------------------------------------------------------
// Pet armor volume filter
// -----------------------------------------------------------------------

// Replicates the filter lambda from monexamine::pet_armor_loc so we can
// verify which armor items are compatible with a given monster without
// opening the UI menu.
static auto armor_matches(const monster& z, const item& it) -> bool {
    return z.type->bodytype == it.get_pet_armor_bodytype()
        && z.get_volume() >= it.get_pet_armor_min_vol()
        && z.get_volume() < it.get_pet_armor_max_vol();
}

TEST_CASE(
    "pet armor volume filter matches correct size for German shepherd", "[pet][armor][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog_gshepherd.str(), mon_pos);

    // German shepherd: base volume 35 L, bodytype "quadruped".
    // Small quadruped armor: 1 L – 54999 ml. Medium: 55 L – 1000 L.
    INFO("monster volume: " << mon.get_volume());
    CHECK(mon.type->bodytype == "quadruped");

    auto small_armor = item::spawn("leather_armor_small_quadruped");
    auto medium_armor = item::spawn("leather_armor_medium_quadruped");

    CHECK(small_armor->is_pet_armor());
    CHECK(medium_armor->is_pet_armor());

    SECTION("small armor matches a 35 L quadruped") { CHECK(armor_matches(mon, *small_armor)); }

    SECTION("medium armor does not match a 35 L quadruped") {
        CHECK_FALSE(armor_matches(mon, *medium_armor));
    }
}

// -----------------------------------------------------------------------
// Extended description: taming requirements
// -----------------------------------------------------------------------

static const mtype_id mon_bear_cub("mon_bear_cub");
static const mtype_id mon_zombie("mon_zombie");

TEST_CASE(
    "extended description shows taming food and pet armor for a tamable monster",
    "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    const std::string desc = mon.extended_description();
    INFO(desc);
    CHECK(desc.find("Taming it requires:") != std::string::npos);
    CHECK(desc.find("Food: dog food") != std::string::npos);
    // The training minimum belongs to the training section, not the taming one,
    // and the two blocks are separated by a blank line.
    CHECK(desc.find("Training it requires:") != std::string::npos);
    CHECK(desc.find("Survival: 3") > desc.find("Training it requires:"));
    CHECK(desc.find("\n\nTraining it requires:") != std::string::npos);
    CHECK(desc.find("Survival: 3\n\n") != std::string::npos);
    CHECK(desc.find("It can wear pet armor such as ") != std::string::npos);
    CHECK(desc.find("It cannot be tamed.") == std::string::npos);
}

TEST_CASE(
    "extended description shows required traits for a monster with tamer_traits",
    "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_bear_cub.str(), mon_pos);

    const std::string desc = mon.extended_description();
    INFO(desc);
    CHECK(desc.find("Taming it requires:") != std::string::npos);
    CHECK(desc.find("Food: dog food") != std::string::npos);
    CHECK(desc.find("Traits: ") != std::string::npos);
    CHECK(desc.find("Animal Empathy") != std::string::npos);
    CHECK(desc.find("Bear") != std::string::npos);
    CHECK(desc.find("Survival: 6") != std::string::npos);
}

TEST_CASE(
    "extended description says the monster cannot be tamed when it has no pet food",
    "[pet][monster]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_zombie.str(), mon_pos);

    const std::string desc = mon.extended_description();
    INFO(desc);
    CHECK(desc.find("It cannot be tamed.") != std::string::npos);
    CHECK(desc.find("It can be tamed with") == std::string::npos);
}

// -----------------------------------------------------------------------
// Configurable taming / training skills
// -----------------------------------------------------------------------

static const skill_id skill_melee("melee");
static const skill_id skill_speech("speech");

TEST_CASE(
    "pet tame and training skills default to survival with no tame requirement",
    "[pet][monster][skill]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    REQUIRE(mon.type->pet_training.has_value());
    CHECK(mon.type->petfood.tame_skill == skill_survival);
    CHECK(mon.type->petfood.tame_min_skill == 0);
    CHECK(mon.type->pet_training->skill == skill_survival);
    // No minimum level means any character meets the taming skill requirement.
    avatar& p = get_avatar();
    p.set_skill_level(skill_survival, 0);
    CHECK(mon.type->petfood.tame_skill_requirement_met(p));
}

TEST_CASE(
    "tame_skill_requirement_met gates on the configured tame skill and level",
    "[pet][monster][skill]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    static mtype custom_type = *mon.type;
    custom_type.petfood.tame_skill = skill_speech;
    custom_type.petfood.tame_min_skill = 2;

    avatar& p = get_avatar();
    p.set_skill_level(skill_speech, 0);
    p.set_skill_level(skill_survival, 10);
    CHECK_FALSE(custom_type.petfood.tame_skill_requirement_met(p));
    p.set_skill_level(skill_speech, 2);
    CHECK(custom_type.petfood.tame_skill_requirement_met(p));
}

TEST_CASE(
    "extended description reflects custom tame and training skills", "[pet][monster][skill]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    // Local copy of the type so the shared monster data is not mutated.
    // static so the pointer outlives the creature tracker cleanup.
    static mtype custom_type = *mon.type;
    custom_type.petfood.tame_skill = skill_speech;
    custom_type.petfood.tame_min_skill = 2;
    custom_type.pet_training->skill = skill_melee;
    mon.type = &custom_type;

    const std::string desc = mon.extended_description();
    INFO(desc);
    // Tame skill requirement sits in the taming section, training skill in its own.
    CHECK(desc.find("Speaking: 2") != std::string::npos);
    CHECK(desc.find("Speaking: 2") < desc.find("Training it requires:"));
    CHECK(desc.find("Training it requires:") != std::string::npos);
    CHECK(desc.find("Melee: 3") > desc.find("Training it requires:"));
    CHECK(desc.find("Survival:") == std::string::npos);
}

TEST_CASE(
    "extended description omits the tame skill line when no minimum is required",
    "[pet][monster][skill]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);

    const std::string desc = mon.extended_description();
    INFO(desc);
    // Dog has no tame_min_skill, so only the training requirement is listed.
    CHECK(desc.find("Survival: 3") != std::string::npos);
    CHECK(desc.find("Skill:") == std::string::npos);
}

TEST_CASE(
    "train_pet_finish gates on the monster's training skill, not survival",
    "[pet][monster][training][activity][skill]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.add_effect(effect_well_fed, 24_hours);
    mon.training_level = 0;
    REQUIRE(mon.type->pet_training.has_value());

    static mtype custom_type = *mon.type;
    custom_type.pet_training->skill = skill_melee;
    mon.type = &custom_type;

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    // High survival but melee below the minimum: training must fail.
    p.set_skill_level(skill_survival, 25);
    p.set_skill_level(skill_melee, mon.type->pet_training->min_skill - 1);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    CHECK(mon.training_level == 0);
    CHECK(mon.has_effect(effect_well_fed));
}

TEST_CASE(
    "train_pet_finish succeeds with the training skill even at zero survival",
    "[pet][monster][training][activity][skill]") {
    clear_all_state();
    monster& mon = spawn_test_monster(mon_dog.str(), mon_pos);
    mon.make_pet();
    mon.add_effect(effect_well_fed, 24_hours);
    mon.training_level = 0;
    REQUIRE(mon.type->pet_training.has_value());

    static mtype custom_type = *mon.type;
    custom_type.pet_training->skill = skill_melee;
    mon.type = &custom_type;

    avatar& p = get_avatar();
    move_player_out_of_the_way();
    // Survival 0, but melee 25 guarantees success: 10*25=250 >= rng(0,100).
    p.set_skill_level(skill_survival, 0);
    p.set_skill_level(skill_melee, 25);

    player_activity act(ACT_TRAIN_PET);
    act.monsters.push_back(g->shared_from(mon));
    act.str_values.push_back(mon.get_name());

    activity_handlers::train_pet_finish(&act, &p);
    CHECK(mon.training_level == 1);
}
