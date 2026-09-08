#include "action_time_scale.h"
#include "activity_actor.h"
#include "activity_actor_definitions.h"
#include "activity_handlers.h"
#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "clzones.h"
#include "debug.h"
#include "flag.h"
#include "item.h"
#include "json.h"
#include "map_helpers.h"
#include "mapbuffer.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "salvage.h"
#include "skill.h"

#include <array>
#include <memory>
#include <ranges>
#include <sstream>

namespace {

struct lifecycle_state {
    int starts = 0;
    int turns = 0;
    int finishes = 0;
    int cancellations = 0;
};

class lifecycle_actor final: public activity_actor {
private:
    std::shared_ptr<lifecycle_state> state;
    bool seed_progress;
    bool stop_without_progress;

public:
    explicit lifecycle_actor(
        std::shared_ptr<lifecycle_state> state_in, bool seed_progress_in = true,
        bool stop_without_progress_in = true)
        : state(std::move(state_in)),
          seed_progress(seed_progress_in),
          stop_without_progress(stop_without_progress_in) {}

    activity_id get_type() const override { return activity_id("ACT_CLEAR_RUBBLE"); }

    void start(player_activity&, Character&) override {
        state->starts++;
        if (seed_progress) { progress.emplace("activity actor regression", 100); }
    }

    void do_turn(player_activity& act, Character&) override {
        state->turns++;
        if (!seed_progress && stop_without_progress) { act.set_to_null(); }
    }

    void finish(player_activity& act, Character&) override {
        state->finishes++;
        act.set_to_null();
    }

    void canceled(player_activity&, Character&) override { state->cancellations++; }

    void serialize(JsonOut& jsout) const override {
        jsout.start_object();
        jsout.member("progress", progress);
        jsout.end_object();
    }
};

const auto migrated_actor_ids = std::array{
    activity_id("ACT_BLEED"),
    activity_id("ACT_BUTCHER"),
    activity_id("ACT_BUTCHER_FULL"),
    activity_id("ACT_CLEAR_RUBBLE"),
    activity_id("ACT_CRAFT"),
    activity_id("ACT_DISMEMBER"),
    activity_id("ACT_DISSECT"),
    activity_id("ACT_DROP"),
    activity_id("ACT_FIELD_DRESS"),
    activity_id("ACT_FETCH_REQUIRED"),
    activity_id("ACT_FILL_LIQUID"),
    activity_id("ACT_FIRSTAID"),
    activity_id("ACT_GUNMOD_ADD"),
    activity_id("ACT_HAND_CRANK"),
    activity_id("ACT_HOTWIRE_CAR"),
    activity_id("ACT_LONGSALVAGE"),
    activity_id("ACT_MAKE_ZLAVE"),
    activity_id("ACT_MILK"),
    activity_id("ACT_MOVE_LOOT"),
    activity_id("ACT_MOVE_ITEMS"),
    activity_id("ACT_OPERATION"),
    activity_id("ACT_PLAY_WITH_PET"),
    activity_id("ACT_PULP"),
    activity_id("ACT_QUARTER"),
    activity_id("ACT_READ"),
    activity_id("ACT_REPAIR_ITEM"),
    activity_id("ACT_SHEAR"),
    activity_id("ACT_SKIN"),
    activity_id("ACT_SOCIALIZE"),
    activity_id("ACT_START_ENGINES"),
    activity_id("ACT_START_FIRE"),
    activity_id("ACT_STASH"),
    activity_id("ACT_STUDY_SPELL"),
    activity_id("ACT_TRAIN_SKILL"),
    activity_id("ACT_TRAIN"),
    activity_id("ACT_TRAIN_PET"),
    activity_id("ACT_TREE_COMMUNION"),
    activity_id("ACT_VEHICLE"),
    activity_id("ACT_WAIT_NPC"),
    activity_id("ACT_WAIT_STAMINA"),
    activity_id("ACT_WEAR")};

auto deserialize_legacy_actor(const activity_id& id, const std::string& json)
    -> std::unique_ptr<activity_actor> {
    const auto deserializer = activity_actors::legacy_deserialize_functions.find(id);
    REQUIRE(deserializer != activity_actors::legacy_deserialize_functions.end());
    std::istringstream input(json);
    JsonIn json_in(input);
    JsonObject data = json_in.get_object();
    return deserializer->second(data);
}

} // namespace

TEST_CASE("migrated activity ids have actor deserializers", "[activity][activity_actor]") {
    for (const activity_id& id : migrated_actor_ids) {
        INFO("activity id: " << id.str());
        CHECK(activity_actors::deserialize_functions.contains(id));
    }
}

TEST_CASE(
    "shared actor ids retain their dispatch registrations",
    "[activity][activity_actor][dispatch]") {
    const auto shared_actor_ids = std::array{
        activity_id("ACT_BLEED"),     activity_id("ACT_BUTCHER"), activity_id("ACT_BUTCHER_FULL"),
        activity_id("ACT_DISMEMBER"), activity_id("ACT_DISSECT"), activity_id("ACT_FIELD_DRESS"),
        activity_id("ACT_QUARTER"),   activity_id("ACT_SKIN"),    activity_id("ACT_FILL_LIQUID"),
        activity_id("ACT_VEHICLE")};

    for (const activity_id& id : shared_actor_ids) {
        INFO("shared actor id: " << id.str());
        CHECK(activity_actors::deserialize_functions.contains(id));
        CHECK(activity_actors::legacy_deserialize_functions.contains(id));
    }
}

TEST_CASE("activity actor owns lifecycle and progress", "[activity][activity_actor]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    auto state = std::make_shared<lifecycle_state>();

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<lifecycle_actor>(state)));
    REQUIRE(dummy.activity);
    REQUIRE(dummy.activity->has_actor());
    CHECK(state->starts == 1);
    CHECK(dummy.activity->get_actor()->progress.get_moves_total() == 100);

    process_activity(dummy);

    CHECK(state->turns > 0);
    CHECK(state->finishes == 1);
    CHECK(!dummy.activity);

    state = std::make_shared<lifecycle_state>();
    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<lifecycle_actor>(state)));
    REQUIRE(dummy.activity);
    dummy.cancel_activity();
    CHECK(state->starts == 1);
    CHECK(state->cancellations == 1);
    CHECK(!dummy.activity);
}

TEST_CASE("actor without generic progress can end safely", "[activity][activity_actor]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    auto state = std::make_shared<lifecycle_state>();

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<lifecycle_actor>(state, false)));
    REQUIRE(dummy.activity);
    dummy.moves = dummy.get_speed();

    CHECK_NOTHROW(dummy.activity->do_turn(dummy));
    CHECK(state->turns == 1);
    CHECK(!dummy.activity);
}

TEST_CASE(
    "actor without progress retains legacy duration during migration",
    "[activity][activity_actor]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    auto state = std::make_shared<lifecycle_state>();

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<lifecycle_actor>(state, false, false)));
    REQUIRE(dummy.activity);
    dummy.activity->moves_total = 1000000;
    dummy.activity->moves_left = 1000000;
    dummy.moves = dummy.get_speed();

    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    CHECK(state->turns == 1);
    CHECK(dummy.activity);
    CHECK(dummy.activity->get_moves_left() < 1000000);
    dummy.cancel_activity();
}

TEST_CASE(
    "activity actor serialization preserves dispatch and progress",
    "[activity][activity_actor][save_load]") {
    player_activity original(std::make_unique<wait_stamina_actor>(42));
    original.get_actor()->progress.emplace("stamina", 75, 31);

    std::ostringstream output;
    JsonOut json_out(output);
    original.serialize(json_out);

    player_activity loaded;
    std::istringstream input(output.str());
    JsonIn json_in(input);
    loaded.deserialize(json_in);

    REQUIRE(loaded.has_actor());
    CHECK(loaded.id() == activity_id("ACT_WAIT_STAMINA"));
    CHECK(loaded.get_actor()->progress.get_moves_total() == 75);
    CHECK(loaded.get_actor()->progress.get_moves_left() == 31);
}

TEST_CASE(
    "legacy activity actors reconstruct their own duration",
    "[activity][activity_actor][save_load][legacy]") {
    const auto wait_npc = deserialize_legacy_actor(
        activity_id("ACT_WAIT_NPC"),
        R"({"str_values":["test NPC"],"moves_total":100,"moves_left":40})");
    REQUIRE(wait_npc);
    CHECK(wait_npc->progress.get_moves_total() == 100);
    CHECK(wait_npc->progress.get_moves_left() == 40);

    const auto train = deserialize_legacy_actor(
        activity_id("ACT_TRAIN"),
        R"({"name":"melee","index":-1,"moves_total":200,"moves_left":75})");
    REQUIRE(train);
    CHECK(train->progress.get_moves_total() == 200);
    CHECK(train->progress.get_moves_left() == 75);

    const auto train_pet = deserialize_legacy_actor(
        activity_id("ACT_TRAIN_PET"),
        R"({"str_values":["dog"],"moves_total":300,"moves_left":125})");
    REQUIRE(train_pet);
    CHECK(train_pet->progress.get_moves_total() == 300);
    CHECK(train_pet->progress.get_moves_left() == 125);

    const auto wear = deserialize_legacy_actor(
        activity_id("ACT_WEAR"),
        R"({"targets":[],"values":[],"moves_total":400,"moves_left":225})");
    REQUIRE(wear);
    CHECK(wear->progress.get_moves_total() == 400);
    CHECK(wear->progress.get_moves_left() == 225);
}

TEST_CASE(
    "simple actor activities complete through actor-owned progress",
    "[activity][activity_actor][completion]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<wait_npc_actor>("test NPC")));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("waiting", 1);
    process_activity(dummy);
    CHECK(!dummy.activity);

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<train_actor>("melee")));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("training", 1);
    process_activity(dummy);
    CHECK(!dummy.activity);
}

TEST_CASE(
    "skill training uses its own actor and continues past one iteration",
    "[activity][activity_actor][train_skill]") {
    clear_map();
    clear_avatar();
    set_time(calendar::turn_zero + 1_minutes);
    avatar& dummy = get_avatar();
    item& training_tool = dummy.i_add(item::spawn("fake_training_exercise_machine"));
    const skill_id training_skill("swimming");
    dummy.set_skill_level(training_skill, 0);
    const int old_exercise = dummy.get_skill_level_object(training_skill).exercise(true);

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<train_skill_activity_actor>(train_skill_activity_actor_options{
            .training_skill = training_skill.str(),
            .training_skill_xp = 1,
            .training_skill_xp_chance = 101,
            .training_skill_max_level = 5,
            .training_skill_fatigue = 1,
            .training_skill_interval = 1,
            .moves_total = 1000,
            .tool = safe_reference<item>(&training_tool),
        })));
    REQUIRE(dummy.activity);
    REQUIRE(dummy.activity->has_actor());
    CHECK(dynamic_cast<repair_actor*>(dummy.activity->get_actor()) == nullptr);
    CHECK(dynamic_cast<train_skill_activity_actor*>(dummy.activity->get_actor()) != nullptr);

    {
        action_time_scale::scoped_calendar_turns_this_tick one_minute(10);
        dummy.moves = dummy.get_speed();
        dummy.activity->do_turn(dummy);
    }

    CHECK(dummy.activity);
    CHECK(dummy.get_skill_level_object(training_skill).exercise(true) > old_exercise);
}

TEST_CASE(
    "pet training cancels safely when its target is lost",
    "[activity][activity_actor][train_pet][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<train_pet_actor>(weak_ptr_fast<monster>(), "lost pet")));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("training pet", 1);

    REQUIRE_NOTHROW([&dummy]() { dummy.activity->do_turn(dummy); }());
    CHECK(!dummy.activity);
}

TEST_CASE(
    "wear actor cancels safely when every target is lost",
    "[activity][activity_actor][wear][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<wear_actor>(std::vector<wear_actor::wear_target>{
            {.item_ref = safe_reference<item>(), .quantity = 1}})));
    REQUIRE(dummy.activity);
    dummy.moves = dummy.get_speed();

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("Lost target item of ACT_WEAR"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "wear actor completes a valid worn-item target",
    "[activity][activity_actor][wear][completion]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    item& shirt = dummy.i_add(item::spawn("tshirt"));

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<wear_actor>(std::vector<wear_actor::wear_target>{
            {.item_ref = safe_reference<item>(&shirt), .quantity = 0}})));
    REQUIRE(dummy.activity);
    dummy.moves = dummy.get_speed();

    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    CHECK(!dummy.activity);
    CHECK(dummy.is_worn(shirt));
}

TEST_CASE(
    "stamina wait owns an indefinite actor progress task", "[activity][activity_actor][progress]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<wait_stamina_actor>(dummy.get_stamina_max())));
    REQUIRE(dummy.activity);
    REQUIRE(dummy.activity->has_actor());

    CHECK(!dummy.activity->get_actor()->progress.invalid());
    CHECK(!dummy.activity->complete());
    dummy.cancel_activity();
}

TEST_CASE("salvage actor cancels without targets", "[activity][activity_actor][salvage]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<salvage_activity_actor>()));

    CHECK(!dummy.activity);
}

TEST_CASE("salvage actor finishes after its final target", "[activity][activity_actor][salvage]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    auto activity = player_activity(std::make_unique<salvage_activity_actor>());
    REQUIRE(activity.has_actor());

    const auto debug_message = capture_debugmsg_during([&] {
        activity.get_actor()->finish(activity, dummy);
    });

    CHECK(debug_message.empty());
    CHECK(activity.is_null());
}

TEST_CASE(
    "salvage actor completes every multi-salvage target", "[activity][activity_actor][salvage]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    dummy.setpos(test_origin);
    dummy.i_add(item::spawn("knife_butcher"));

    auto& here = dummy.get_mapbuffer();
    auto first_sheet = item::spawn("sheet");
    auto second_sheet = item::spawn("sheet");
    REQUIRE_FALSE(here.add_item_or_charges(test_origin, std::move(first_sheet)));
    REQUIRE_FALSE(here.add_item_or_charges(test_origin, std::move(second_sheet)));
    REQUIRE(here.get_items(test_origin) != nullptr);
    REQUIRE(here.get_items(test_origin)->size() == 2);

    iuse_locations targets;
    for (item* const target : *here.get_items(test_origin)) { targets.emplace_back(*target, 0); }
    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<salvage_activity_actor>(std::move(targets), dummy.abs_pos(), true)));
    REQUIRE(dummy.activity);

    const auto count_sheets = [&]() {
        auto result = 0;
        for (const item* const target : *here.get_items(test_origin)) {
            if (target->typeId() == itype_id("sheet")) { ++result; }
        }
        return result;
    };

    const auto debug_message = capture_debugmsg_during([&] {
        for (const auto attempt : std::views::iota(0, 1000)) {
            (void)attempt;
            if (!dummy.activity || count_sheets() < 2) { break; }
            dummy.moves = 100000;
            dummy.activity->do_turn(dummy);
        }
        CHECK(count_sheets() == 1);
        CHECK(dummy.activity);

        for (const auto attempt : std::views::iota(0, 1000)) {
            (void)attempt;
            if (!dummy.activity) { break; }
            dummy.moves = 100000;
            dummy.activity->do_turn(dummy);
        }
    });

    CHECK(debug_message.empty());
    CHECK(count_sheets() == 0);
    CHECK_FALSE(dummy.activity);
}

TEST_CASE(
    "ownerless active thrown items use the supplied absolute position",
    "[activity][item_location][throw]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    detached_ptr<item> lit = item::spawn("molotov_lit");
    lit->activate();

    auto processed = dummy.get_mapbuffer().process_item_at(dummy.abs_pos(), std::move(lit), true);
    CHECK(!processed);
}

TEST_CASE(
    "read activity progress message tolerates a lost book",
    "[activity][activity_actor][safe_reference]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<read_activity_actor>()));
    REQUIRE(dummy.activity);

    CHECK_NOTHROW(dummy.activity->get_progress_message(dummy));
}

TEST_CASE(
    "read activity preserves continuous reading and skill progress",
    "[activity][activity_actor][reading]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    dummy.setID(character_id(1), true);
    detached_ptr<item> det = item::spawn("manual_mechanics");
    auto& book = dummy.i_add(std::move(det));
    dummy.i_add(item::spawn("atomic_lamp"));
    dummy.do_read(&book);

    auto actor = std::make_unique<read_activity_actor>(
        safe_reference<item>(&book), std::vector<read_activity_actor::npc_learner>(), false, 1);
    actor->continuous_reader_id = dummy.getID().get_value();
    dummy.assign_activity(std::make_unique<player_activity>(std::move(actor)));
    REQUIRE(dummy.activity);

    const auto progress = dummy.activity->get_progress_message(dummy);
    REQUIRE(progress);
    CHECK(progress->find("%") != std::string::npos);

    dummy.moves = 100000;
    dummy.activity->do_turn(dummy);

    REQUIRE(dummy.activity);
    CHECK(dummy.activity->has_actor());
    CHECK(dummy.activity->get_actor()->get_type() == activity_id("ACT_READ"));
}

TEST_CASE("butcher actor owns progress after corpse setup", "[activity][activity_actor][butcher]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos();

    auto corpse = item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "");
    CHECK_FALSE(buffer.add_item_or_charges(target, std::move(corpse)));
    auto* const items = buffer.get_items(target);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    item* const corpse_ptr = items->front();

    dummy.i_add(item::spawn("knife_butcher"));
    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<butcher_actor>(
        activity_id("ACT_BUTCHER"), safe_reference<item>(corpse_ptr))));
    REQUIRE(dummy.activity);

    dummy.moves = dummy.get_speed();
    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    REQUIRE(dummy.activity);
    REQUIRE(dummy.activity->has_actor());
    REQUIRE(!dummy.activity->get_actor()->progress.invalid());

    const auto moves_left = dummy.activity->get_moves_left();
    CHECK(moves_left > 0);

    dummy.moves = dummy.get_speed();
    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    CHECK(dummy.activity->get_moves_left() < moves_left);
}

TEST_CASE(
    "butcher actor cancels when no suitable tool exists", "[activity][activity_actor][butcher]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos();

    auto corpse = item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "");
    CHECK_FALSE(buffer.add_item_or_charges(target, std::move(corpse)));
    auto* const items = buffer.get_items(target);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    item* const corpse_ptr = items->front();

    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<butcher_actor>(
        activity_id("ACT_BUTCHER"), safe_reference<item>(corpse_ptr))));
    REQUIRE(dummy.activity);
    CHECK(dummy.activity->speed.tools == Approx(1.0f));

    dummy.moves = dummy.get_speed();
    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    CHECK(!dummy.activity);
    const auto* remaining = buffer.get_items(target);
    REQUIRE(remaining != nullptr);
    REQUIRE(remaining->size() == 1);
    CHECK(remaining->front()->is_corpse());
}

TEST_CASE(
    "butcher actor completes and removes its corpse",
    "[activity][activity_actor][butcher][completion]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos();

    CHECK_FALSE(buffer.add_item_or_charges(
        target, item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "")));
    auto* const items = buffer.get_items(target);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    item* const corpse_ptr = items->front();

    dummy.i_add(item::spawn("knife_butcher"));
    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<butcher_actor>(
        activity_id("ACT_BUTCHER"), safe_reference<item>(corpse_ptr))));
    REQUIRE(dummy.activity);

    process_activity(dummy);

    CHECK(!dummy.activity);
    const auto* remaining = buffer.get_items(target);
    REQUIRE(remaining != nullptr);
    CHECK(std::ranges::none_of(*remaining, [](const item* obj) {
        return obj != nullptr && obj->is_corpse();
    }));
}

TEST_CASE(
    "butcher actor cancels after its corpse is removed",
    "[activity][activity_actor][butcher][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos();

    CHECK_FALSE(buffer.add_item_or_charges(
        target, item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "")));
    auto* const items = buffer.get_items(target);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    item* const corpse_ptr = items->front();

    dummy.i_add(item::spawn("knife_butcher"));
    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<butcher_actor>(
        activity_id("ACT_BUTCHER"), safe_reference<item>(corpse_ptr))));
    REQUIRE(dummy.activity);
    dummy.moves = dummy.get_speed();
    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    REQUIRE(dummy.activity);

    CHECK(buffer.remove_item(target, corpse_ptr) != nullptr);
    dummy.moves = dummy.get_speed();
    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "shared butcher actors cancel when their corpse is removed",
    "[activity][activity_actor][butcher][target_loss][shared_id]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    auto target = dummy.abs_pos() + tripoint_east;

    const std::array<activity_id, 8> butcher_ids =
        {activity_id("ACT_BLEED"),     activity_id("ACT_BUTCHER"), activity_id("ACT_BUTCHER_FULL"),
         activity_id("ACT_DISMEMBER"), activity_id("ACT_DISSECT"), activity_id("ACT_FIELD_DRESS"),
         activity_id("ACT_QUARTER"),   activity_id("ACT_SKIN")};

    for (const activity_id& id : butcher_ids) {
        INFO("activity id: " << id.str());
        REQUIRE_FALSE(buffer.add_item_or_charges(
            target, item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "")));
        auto* const items = buffer.get_items(target);
        REQUIRE(items != nullptr);
        REQUIRE(items->size() == 1);
        item* const corpse_ptr = items->front();

        dummy.assign_activity(std::make_unique<player_activity>(
            std::make_unique<butcher_actor>(id, safe_reference<item>(corpse_ptr))));
        REQUIRE(dummy.activity);
        CHECK(dummy.activity->id() == id);
        CHECK(buffer.remove_item(target, corpse_ptr) != nullptr);

        dummy.moves = dummy.get_speed();
        REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));
        CHECK(!dummy.activity);
        target += tripoint_east;
    }
}

TEST_CASE(
    "liquid transfer cancels after its vehicle source is lost",
    "[activity][activity_actor][liquid_transfer][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    const auto source = dummy.abs_pos() + tripoint_east;

    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<liquid_transfer_actor>(
        LST_VEHICLE, source, 0, LTT_MAP, dummy.abs_pos(), safe_reference<item>())));
    REQUIRE(dummy.activity);

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("vehicle source"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "vehicle actors cancel after their vehicle is lost",
    "[activity][activity_actor][vehicle][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    const auto target = dummy.abs_pos() + tripoint_east;

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<hotwire_car_actor>(target, 1, 1)));
    REQUIRE(dummy.activity);
    dummy.moves = dummy.get_speed();
    const std::string hotwire_debug = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(hotwire_debug, Catch::Matchers::Contains("vehicle not found"));
    CHECK(!dummy.activity);

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<vehicle_work_actor>(vehicle_work_actor_options{
            .command = 'r',
            .part_pos = target,
            .part_type = vpart_id("frame"),
            .part_index = 0,
            .moves_total = 1,
        })));
    REQUIRE(dummy.activity);
    dummy.moves = dummy.get_speed();
    const std::string vehicle_work_debug = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(vehicle_work_debug, Catch::Matchers::Contains("vehicle not found"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "gunmod actor completes and cancels after a target is lost",
    "[activity][activity_actor][gunmod][completion][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    item& gun = dummy.i_add(item::spawn("glock_19"));
    item& mod = dummy.i_add(item::spawn("pistol_stock"));
    REQUIRE(gun.is_gunmod_compatible(mod).success());

    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<gunmod_add_actor>(
        100, 0, 0, "", safe_reference<item>(&gun), safe_reference<item>(&mod))));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("installing gunmod", 1);
    dummy.moves = dummy.get_speed();
    REQUIRE_NOTHROW(dummy.activity->do_turn(dummy));

    CHECK(!dummy.activity);
    CHECK(gun.gunmods().size() == 1);

    item& second_gun = dummy.i_add(item::spawn("glock_19"));
    item& second_mod = dummy.i_add(item::spawn("pistol_stock"));
    REQUIRE(second_gun.is_gunmod_compatible(second_mod).success());
    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<gunmod_add_actor>(
        100, 0, 0, "", safe_reference<item>(&second_gun), safe_reference<item>(&second_mod))));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("installing gunmod", 1);
    auto removed_mod = dummy.inv_remove_item(&second_mod);
    REQUIRE(removed_mod);
    dummy.moves = dummy.get_speed();

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("Lost gun or gunmod"));
    CHECK(!dummy.activity);
    CHECK(second_gun.gunmods().empty());
}

TEST_CASE(
    "hand crank actor cancels after its tool is lost",
    "[activity][activity_actor][hand_crank][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    item& charger = dummy.i_add(item::spawn("hand_crank_charger"));

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<hand_crank_charge_actor>(1, 1, 0, itype_id("battery"))));
    REQUIRE(dummy.activity);
    dummy.activity->add_tool(&charger);
    dummy.activity->get_actor()->progress.emplace("charging battery", 1);
    {
        auto removed_charger = dummy.inv_remove_item(&charger);
        REQUIRE(removed_charger);
    }
    dummy.moves = dummy.get_speed();

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("Hand-crank activity lost its tool"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "start fire actor cancels after its tool is lost",
    "[activity][activity_actor][start_fire][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    item& matches = dummy.i_add(item::spawn("matches"));

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<start_fire_actor>(0, dummy.abs_pos(), 0)));
    REQUIRE(dummy.activity);
    dummy.activity->add_tool(&matches);
    dummy.activity->get_actor()->progress.emplace("starting fire", 1);
    {
        auto removed_matches = dummy.inv_remove_item(&matches);
        REQUIRE(removed_matches);
    }
    dummy.moves = dummy.get_speed();

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("Starting fire activity lost its tool"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "make zlave actor keeps its absolute corpse target",
    "[activity][activity_actor][make_zlave][target]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos() + tripoint_east;

    REQUIRE_FALSE(buffer.add_item_or_charges(
        target, item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "")));
    auto* const items = buffer.get_items(target);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    item* const corpse = items->front();

    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<make_zlave_actor>(
        100, corpse->display_name(), safe_reference<item>(corpse))));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("enslaving corpse", 1);

    process_activity(dummy);

    CHECK(!dummy.activity);
    CHECK(corpse->has_var("zlave"));
}

TEST_CASE(
    "pulp actor reads and mutates its absolute corpse target",
    "[activity][activity_actor][pulp][target]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos() + tripoint_east;

    REQUIRE_FALSE(buffer.add_item_or_charges(
        target, item::make_corpse(mtype_id("mon_zombie"), calendar::turn, "")));
    auto* const items = buffer.get_items(target);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    item* const corpse = items->front();
    corpse->set_damage(corpse->max_damage());
    dummy.str_cur = 100;

    dummy.assign_activity(std::make_unique<player_activity>(std::make_unique<pulp_actor>(target)));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("pulping corpse", 1);

    process_activity(dummy);

    CHECK(!dummy.activity);
    CHECK(corpse->has_flag(flag_PULPED));
}

TEST_CASE(
    "play with pet actor cancels after its pet is lost",
    "[activity][activity_actor][play_with_pet][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<play_with_pet_actor>(weak_ptr_fast<monster>(), "pet")));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("playing with pet", 1);

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("Lost pet target"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "milk actor cancels after its animal is lost",
    "[activity][activity_actor][milk][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(std::make_unique<player_activity>(
        std::make_unique<milk_actor>(dummy.abs_pos() + tripoint_east)));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("milking", 1);

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("source creature for liquid transfer"));
    CHECK(!dummy.activity);
}

TEST_CASE("crowbar honors an explicit absolute target", "[activity][item_action][prying][target]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos() + tripoint_east;

    REQUIRE(buffer.set_furn(target, furn_id("f_crate_c")));
    dummy.str_cur = 100;
    auto crowbar = item::spawn("crowbar");
    item* const crowbar_ptr = crowbar.get();
    dummy.i_add(std::move(crowbar));

    dummy.invoke_item(crowbar_ptr, "CROWBAR", target);
    CHECK(buffer.furn(target) == furn_id("f_crate_o"));
}

TEST_CASE(
    "first aid actor cancels after its target is lost",
    "[activity][activity_actor][firstaid][target_loss]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();

    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<firstaid_actor>("torso")));
    REQUIRE(dummy.activity);
    REQUIRE(dummy.activity->has_actor());
    dummy.activity->get_actor()->progress.emplace("first aid", 1);
    dummy.moves = dummy.get_speed();

    const std::string debug_message = capture_debugmsg_during([&dummy]() {
        dummy.activity->do_turn(dummy);
    });
    CHECK_THAT(debug_message, Catch::Matchers::Contains("Lost target of ACT_FIRSTAID"));
    CHECK(!dummy.activity);
}

TEST_CASE(
    "churn activity completes at its absolute placement", "[activity][activity_legacy][churn]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    auto target = dummy.abs_pos();
    target += tripoint(1, 0, 0);

    REQUIRE(buffer.set_ter(target, t_dirt));
    auto activity = std::make_unique<player_activity>(activity_id("ACT_CHURN"), 1);
    activity->placement = target;
    dummy.assign_activity(std::move(activity));

    REQUIRE(dummy.activity);
    process_activity(dummy);

    REQUIRE(!dummy.activity);
    const auto terrain = buffer.ter(target);
    REQUIRE(terrain.has_value());
    CHECK(*terrain == t_dirtmound);

    REQUIRE(buffer.set_ter(target, t_dirt));
    auto canceled = std::make_unique<player_activity>(activity_id("ACT_CHURN"), 100);
    canceled->placement = target;
    dummy.assign_activity(std::move(canceled));
    REQUIRE(dummy.activity);
    dummy.cancel_activity();

    const auto unchanged = buffer.ter(target);
    REQUIRE(unchanged.has_value());
    CHECK(*unchanged == t_dirt);
}

TEST_CASE(
    "clear rubble actor completes at its absolute placement",
    "[activity][activity_actor][clear_rubble][target]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto target = dummy.abs_pos() + tripoint_east;

    REQUIRE(buffer.set_furn(target, furn_id("f_rubble")));
    dummy.assign_activity(
        std::make_unique<player_activity>(std::make_unique<clear_rubble_actor>(target)));
    REQUIRE(dummy.activity);
    dummy.activity->get_actor()->progress.emplace("clearing rubble", 1);

    process_activity(dummy);

    CHECK(!dummy.activity);
    CHECK(buffer.furn(target) == furn_id("f_null"));
}

TEST_CASE(
    "spellcasting activity finishes its self-target cast",
    "[activity][activity_legacy][spellcasting]") {
    clear_map();
    clear_avatar();
    avatar& dummy = get_avatar();
    dummy.set_stamina(0);

    auto activity = std::make_unique<player_activity>(activity_id("ACT_SPELLCASTING"), 1);
    activity->name = "test_spell_montage";
    activity->values = {0, 1, 0};
    dummy.assign_activity(std::move(activity));
    REQUIRE(dummy.activity);

    REQUIRE_NOTHROW(activity_handlers::spellcasting_finish(dummy.activity.get(), &dummy));

    CHECK(!dummy.activity);
    CHECK(dummy.get_stamina() == 1000);
}

TEST_CASE(
    "multiple butcher activity continues through its corpse backlog",
    "[activity][activity_legacy][multiple_butcher]") {
    clear_map();
    clear_avatar();
    zone_manager::reset_manager();
    avatar& dummy = get_avatar();
    const auto previous_turn = calendar::turn;
    const auto cleanup = on_out_of_scope([&dummy, previous_turn]() {
        dummy.backlog.clear();
        clear_avatar();
        zone_manager::reset_manager();
        set_time(previous_turn);
    });
    set_time(calendar::turn_zero + 12_hours);
    mapbuffer& buffer = dummy.get_mapbuffer();
    const auto first_target = dummy.abs_pos();
    auto second_target = first_target;
    second_target += tripoint(1, 0, 0);

    zone_manager::get_manager()
        .add("test corpse zone", zone_type_id("LOOT_CORPSE"), faction_id("your_followers"), false,
             true, first_target, second_target);
    buffer.add_item_or_charges(
        first_target, item::make_corpse(mtype_id("mon_rabbit"), calendar::turn, ""));
    buffer.add_item_or_charges(
        second_target, item::make_corpse(mtype_id("mon_rabbit"), calendar::turn, ""));
    const auto first_setup_items = buffer.get_items(first_target);
    const auto second_setup_items = buffer.get_items(second_target);
    REQUIRE(first_setup_items);
    REQUIRE(second_setup_items);
    REQUIRE(first_setup_items->size() == 1);
    REQUIRE(second_setup_items->size() == 1);
    dummy.i_add(item::spawn("knife_butcher"));
    dummy.assign_activity(activity_id("ACT_MULTIPLE_BUTCHER"));

    REQUIRE(dummy.activity);
    process_activity(dummy);

    CHECK(!dummy.activity);
    const auto first_items = buffer.get_items(first_target);
    const auto second_items = buffer.get_items(second_target);
    REQUIRE(first_items);
    REQUIRE(second_items);
    CHECK(std::ranges::none_of(*first_items, [](const item* obj) {
        return obj != nullptr && obj->is_corpse();
    }));
    CHECK(std::ranges::none_of(*second_items, [](const item* obj) {
        return obj != nullptr && obj->is_corpse();
    }));
}
