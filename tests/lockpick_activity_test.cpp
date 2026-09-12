#include "activity_actor_definitions.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "item.h"
#include "json.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

#include <sstream>
#include <string>

static const itype_id itype_picklocks("picklocks");
static const itype_id itype_pseudo_bio_picklock("pseudo_bio_picklock");

static auto serialize_activity(const player_activity& act) -> std::string {
    auto os = std::ostringstream();
    auto jsout = JsonOut(os);
    act.serialize(jsout);
    return os.str();
}

TEST_CASE(
    "lockpick activity autosave does not resolve empty fake lockpick",
    "[activity][lockpick][save]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    clear_avatar();

    avatar& dummy = get_avatar();
    item& lockpick = dummy.i_add(item::spawn(itype_picklocks));
    REQUIRE_FALSE(lockpick.is_null());

    auto act = player_activity(lockpick_activity_actor::use_item(100, lockpick, dummy.abs_pos()));
    REQUIRE(act.id() == activity_id("ACT_LOCKPICK"));

    std::string saved;
    const auto debug = capture_debugmsg_during([&]() { saved = serialize_activity(act); });

    CHECK(debug.find("Attempted to resolve invalid location_ptr") == std::string::npos);
    REQUIRE_FALSE(saved.empty());

    auto iss = std::istringstream(saved);
    auto jsin = JsonIn(iss);
    auto loaded = player_activity();
    const auto load_debug = capture_debugmsg_during([&]() { loaded.deserialize(jsin); });
    CHECK(load_debug.find("Attempted to resolve invalid location_ptr") == std::string::npos);
    CHECK(loaded.id() == activity_id("ACT_LOCKPICK"));
}

TEST_CASE(
    "bionic lockpick activity serializes fake lockpick without debugmsg",
    "[activity][lockpick][save]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    clear_avatar();

    avatar& dummy = get_avatar();
    auto act = player_activity(lockpick_activity_actor::use_bionic(
        item::spawn(itype_pseudo_bio_picklock), dummy.abs_pos()));
    REQUIRE(act.id() == activity_id("ACT_LOCKPICK"));

    std::string saved;
    const auto debug = capture_debugmsg_during([&]() { saved = serialize_activity(act); });

    CHECK(debug.find("Attempted to resolve invalid location_ptr") == std::string::npos);
    REQUIRE_FALSE(saved.empty());
    CHECK(saved.find("pseudo_bio_picklock") != std::string::npos);
}
