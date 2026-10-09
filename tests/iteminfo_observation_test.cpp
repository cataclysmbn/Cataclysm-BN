#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting_inventory_request.h"
#include "faction.h"
#include "game.h"
#include "item.h"
#include "item_preview.h"
#include "iteminfo_request.h"
#include "itype.h"
#include "iuse_actor.h"
#include "json.h"
#include "map/map.h"
#include "map/submap.h"
#include "npc.h"
#include "nutrient_range.h"
#include "output.h"
#include "player_helpers.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "rng.h"
#include "skill.h"
#include "state_helpers.h"
#include "text_snippets.h"
#include "type_id.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"
#include "water_source.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(CATA_MCP)
#    include "advanced_inv.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "cursesdef.h"
#    include "inventory_ui.h"
#    include "pickup.h"
#    include "ui.h"
#    include "uistate.h"
#endif

namespace {

constexpr auto info_test_pos = tripoint_bub_ms{60, 60, 0};

auto recipe_availability_fields() -> std::vector<available_status*> {
    using namespace std::views;
    namespace ranges = std::ranges;
    auto result = std::vector<available_status*>{};
    const auto field_address = [](const auto& component) { return &component.available; };
    const auto append = [&result, &field_address](const auto& groups) {
        ranges::transform(groups | join, std::back_inserter(result), field_address);
    };
    const auto append_requirements = [&append](const requirement_data& requirements) {
        append(requirements.get_components());
        append(requirements.get_tools());
        append(requirements.get_qualities());
    };
    for (const auto& entry : recipe_dict) {
        append_requirements(entry.second.simple_requirements());
        ranges::for_each(entry.second.deduped_requirements().alternatives(), append_requirements);
    }
    return result;
}

auto recipe_availability() -> std::vector<int> {
    using namespace std::views;
    namespace ranges = std::ranges;
    return recipe_availability_fields()
         | transform([](const auto* field) { return static_cast<int>(*field); })
         | ranges::to<std::vector>();
}

struct availability_snapshot {
    available_status* field;
    available_status original;
};

auto snapshot_availability(available_status* field) -> availability_snapshot {
    return {.field = field, .original = *field};
}

struct recipe_availability_guard {
    std::vector<availability_snapshot> values;

    recipe_availability_guard() {
        namespace ranges = std::ranges;
        ranges::transform(
            recipe_availability_fields(), std::back_inserter(values), snapshot_availability);
    }
    ~recipe_availability_guard() {
        // Fixture cleanup only, after all purity/parity assertions and explicit native queries.
        for (const auto& value : values) { *value.field = value.original; }
    }
};

struct observation_fixture {
    // Move aside the singleton only to install a fresh test fixture, never to roll back a query.
    avatar original_avatar = std::move(get_avatar());
    restore_on_out_of_scope<cata_default_random_engine> restore_rng{rng_get_engine()};
    restore_on_out_of_scope<time_point> restore_turn{calendar::turn};
    recipe_availability_guard restore_availability;

    observation_fixture() {
        get_avatar() = avatar{};
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().setpos(map_local_to_abs(get_map(), info_test_pos));
        calendar::turn = calendar::turn_zero + 12_hours;
        get_avatar().invalidate_crafting_inventory();
        REQUIRE_FALSE(get_avatar().has_trait(trait_id("DEBUG_HS")));
        REQUIRE_FALSE(get_avatar().has_trait(trait_id("DEBUG_STORAGE")));
    }

    ~observation_fixture() {
        clear_all_state();
        get_avatar() = std::move(original_avatar);
    }
};

auto saved(const auto& value) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    value.serialize(json);
    return stream.str();
}

auto saved_submap() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    json.member("factions", *g->faction_manager_ptr);
    json.member("submaps");
    json.start_array();
    for (const auto x : {-1, 0, 1}) {
        for (const auto y : {-1, 0, 1}) {
            json.start_object();
            get_map().get_submap_at(info_test_pos + tripoint{x * SEEX, y * SEEY, 0})->store(json);
            json.end_object();
        }
    }
    json.end_array();
    json.end_object();
    return stream.str();
}

/// Snapshot faction authority including runtime fields omitted from native save serialization.
auto ownership_factions() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_array();
    for (const auto& [id, value] : g->faction_manager_ptr->all()) {
        json.start_object();
        json.member("id", id);
        json.member("saved", value);
        json.member("validated", value.validated);
        json.member("description", value.desc);
        json.member("currency", value.currency);
        json.member("lone_wolf_faction", value.lone_wolf_faction);
        json.member("monster_faction", value.mon_faction);
        json.member("members");
        json.start_array();
        for (const auto& [member_id, member] : value.members) {
            json.start_object();
            json.member("id", member_id);
            json.member("name", member.first);
            json.member("known", member.second);
            json.end_object();
        }
        json.end_array();
        json.member("epilogues");
        json.start_array();
        for (const auto& [minimum, maximum, snippet] : value.epilogue_data) {
            json.start_object();
            json.member("minimum", minimum);
            json.member("maximum", maximum);
            json.member("snippet", snippet);
            json.end_object();
        }
        json.end_array();
        json.end_object();
    }
    json.end_array();
    return stream.str();
}

struct observation_baseline {
    std::string avatar = saved(get_avatar());
    std::string candidate;
    std::string world = saved_submap();
    cata_default_random_engine rng = rng_get_engine();
    std::vector<int> requirements = recipe_availability();

    explicit observation_baseline(const item& value): candidate(saved(value)) {}

    auto check(const item& value) const -> void {
        CHECK(saved(get_avatar()) == avatar);
        CHECK(saved(value) == candidate);
        CHECK(saved_submap() == world);
        CHECK(rng_get_engine() == rng);
        CHECK(recipe_availability() == requirements);
    }
};

auto observe(const item& value) -> std::string {
    return value.info_string({.mode = iteminfo_mode::observation});
}

// Real polymorphic actor installed only on dedicated TEST_DATA types. Constructors/copies are
// production item paths; the probe records their virtual on_spawned contract, not mocked ranges.
class range_spawn_actor final: public iuse_transform {
    int& calls;

public:
    explicit range_spawn_actor(int& calls): calls(calls) {}
    auto clone() const -> std::unique_ptr<iuse_actor> override {
        return std::make_unique<range_spawn_actor>(*this);
    }
    auto info(const item& /*it*/, std::vector<iteminfo>& /*dump*/) const -> void override {}
    auto info_for_display(const item& /*it*/, std::vector<iteminfo>& /*dump*/) const
        -> void override {}
    auto on_spawned(item& it) const -> void override {
        ++calls;
        ++get_avatar().moves;
        it.set_var("range_spawn_probe", rng(0, 1000));
    }
};

struct range_actor_fixture {
    itype& type;
    restore_on_out_of_scope<decltype(itype::use_methods)> restore;

    range_actor_fixture(const itype_id& id, int& calls)
        : type(const_cast<itype&>(id.obj())),
          restore(type.use_methods) {
        REQUIRE(id.str().starts_with("test_info_range_"));
        type.use_methods
            .emplace("range_spawn_probe", use_function{std::make_unique<range_spawn_actor>(calls)});
    }
};

// Character is abstract. This concrete real NPC delegates only its faction identity to the
// production Character defaults, while retaining the production actor/inventory implementation.
class base_faction_character final: public npc {
public:
    auto get_faction() const -> faction* override { return Character::get_faction(); } // *NOPAD*
    auto get_faction_id_for_display() const -> std::optional<faction_id> override {
        return Character::get_faction_id_for_display();
    }
};

} // namespace

TEST_CASE(
    "native book inspection retains missing skill insertion",
    "[iteminfo][observation][native_book_skill]") {
    auto fixture = observation_fixture{};
    auto book = item::spawn("test_info_book", calendar::turn);
    auto& you = get_avatar();
    const auto mechanics = skill_id("mechanics");

    REQUIRE_FALSE(you.get_all_skills().contains(mechanics));
    const auto passive = book->info_string({.mode = iteminfo_mode::observation});
    REQUIRE_FALSE(you.get_all_skills().contains(mechanics));
    CHECK(book->info_string() == passive);
    REQUIRE(you.get_all_skills().contains(mechanics));
    CHECK(std::as_const(you).get_skill_level_object(mechanics).level() == 0);
}

TEST_CASE("first item-information observation is passive", "[iteminfo][observation][rng]") {
    const auto fixture = observation_fixture{};
    auto candidate = item::spawn("test_info_jersey", calendar::turn);

    SECTION("unseen identified vanilla jersey snippet") {
        candidate = item::spawn("jersey", calendar::turn);
        candidate->set_snippet(snippet_id("decatur"));
        REQUIRE_FALSE(get_avatar().has_seen_snippet(snippet_id("decatur")));
    }
    SECTION("missing book skill entry") {
        candidate = item::spawn("test_info_book", calendar::turn);
        REQUIRE_FALSE(get_avatar().get_all_skills().contains(skill_id("mechanics")));
    }
    SECTION("newly eligible autolearn and cold natural resources") {
        get_avatar().set_skill_level(skill_id("mechanics"), 2);
        get_map().ter_set(info_test_pos + tripoint_east, ter_id("t_sewage"));
        get_map().ter_set(info_test_pos + tripoint_west, ter_id("t_water_sh"));
        get_avatar().i_add(item::spawn("test_info_jersey", calendar::turn));
        get_avatar().invalidate_crafting_inventory();
    }
    SECTION("nested real snippet and book") {
        auto container = item::spawn("test_backpack", calendar::turn);
        container->put_in(std::move(candidate));
        container->put_in(item::spawn("test_info_book", calendar::turn));
        candidate = std::move(container);
    }
    SECTION("transform preview with random-snippet target") {
        candidate = item::spawn("test_info_transform", calendar::turn);
    }
    SECTION("recipe-exemplar food and cold ingredient availability") {
        candidate = item::spawn("test_info_food", calendar::turn, 1);
        candidate->set_var("recipe_exemplar", "test_info_food");
        get_avatar().set_skill_level(skill_id("mechanics"), 2);
        get_avatar().i_add(item::spawn("test_info_food_component", calendar::turn, 5));
        get_map().ter_set(info_test_pos + tripoint_east, ter_id("t_sewage"));
    }

    const auto before = observation_baseline{*candidate};
    const auto first = observe(*candidate);
    REQUIRE_FALSE(first.empty());
    before.check(*candidate);
    CHECK(observe(*candidate) == first);
    before.check(*candidate);

    if (candidate->is_container()) {
        for (const auto* child : candidate->contents.all_items_top()) {
            REQUIRE_FALSE(observe(*child).empty());
            before.check(*candidate);
        }
    }
    if (candidate->is_book()) {
        CHECK(first.find("Your current") != std::string::npos);
        CHECK(first.find("unread chapters") != std::string::npos);
        CHECK(first.find("TEST observation water assembly") != std::string::npos);
        CHECK_FALSE(get_avatar().get_all_skills().contains(skill_id("mechanics")));
    }
    // Observation left the authority prestate intact, so inspection must return identical details.
    CHECK(candidate->info_string() == first);
}

TEST_CASE(
    "passive throw descriptions isolate native nested copy effects",
    "[iteminfo][observation][rng][throwing]") {
    const auto fixture = observation_fixture{};
    auto calls = 0;
    const auto parent_actor = range_actor_fixture{itype_id("test_info_range_ammo"), calls};
    const auto child_actor = range_actor_fixture{itype_id("test_info_range_child"), calls};
    auto candidate = item::spawn("test_info_range_ammo", calendar::turn, 5);
    candidate->put_in(item::spawn("test_info_range_child", calendar::turn));
    candidate->get_components().push_back(item::spawn("test_info_range_child", calendar::turn));
    get_avatar().str_max = 12;
    get_avatar().str_cur = 12;
    get_avatar().set_skill_level(skill_id("throw"), 4);
    const auto before = observation_baseline{*candidate};
    const auto initial_calls = calls;
    const auto initial_moves = get_avatar().moves;
    const auto range = get_avatar().throw_range_for_display(*candidate);
    REQUIRE(range > 0);
    before.check(*candidate);
    CHECK(get_avatar().throw_range_for_display(*candidate) == range);
    const auto first = observe(*candidate);
    CHECK(first.find("Throw") != std::string::npos);
    CHECK(observe(*candidate) == first);
    before.check(*candidate);
    CHECK(calls == initial_calls);

    CHECK(get_avatar().throw_range(*candidate) == range);
    CHECK(calls == initial_calls + 3);
    CHECK(get_avatar().moves == initial_moves + 3);
    CHECK(rng_get_engine() != before.rng);
    CHECK(saved(*candidate) == before.candidate);
    CHECK(saved_submap() == before.world);
    const auto native_calls = calls;
    const auto native_rng = rng_get_engine();
    CHECK(candidate->info_string() == first);
    CHECK(calls == native_calls + 3);
    CHECK(rng_get_engine() != native_rng);
}

TEST_CASE(
    "passive gun descriptions isolate ammo and nested native construction",
    "[iteminfo][observation][rng][gun][ranged]") {
    const auto fixture = observation_fixture{};
    auto calls = 0;
    const auto gun_actor = range_actor_fixture{itype_id("test_info_range_gun"), calls};
    const auto mod_actor = range_actor_fixture{itype_id("test_info_range_mod"), calls};
    const auto ammo_actor = range_actor_fixture{itype_id("test_info_range_ammo"), calls};
    auto candidate = item::spawn("test_info_range_gun", calendar::turn);
    get_avatar().str_max = 12;
    get_avatar().str_cur = 12;
    get_avatar().set_skill_level(skill_id("throw"), 4);
    SECTION("loaded throwing-skill gun") {
        candidate->ammo_set(itype_id("test_info_range_ammo"), 1);
    }
    SECTION("unloaded description assumes default ammo without spawning it natively") {
        candidate->ammo_unset();
    }
    const auto before = observation_baseline{*candidate};
    const auto initial_calls = calls;
    const auto first = observe(*candidate);
    CHECK(first.find("Maximum range") != std::string::npos);
    CHECK(first.find("Ranged damage") != std::string::npos);
    CHECK(observe(*candidate) == first);
    before.check(*candidate);
    CHECK(calls == initial_calls);

    if (candidate->ammo_remaining()) {
        const auto base_range = candidate->gun_range_for_display(true);
        const auto player_range = candidate->gun_range_for_display(&get_avatar());
        const auto price = candidate->price_for_display(false);
        CHECK(price > 0);
        CHECK(candidate->price_for_display(false) == price);
        CHECK(candidate->gun_range_for_display(true) == base_range);
        CHECK(candidate->gun_range_for_display(&get_avatar()) == player_range);
        before.check(*candidate);
        CHECK(calls == initial_calls);
        CHECK(candidate->gun_range(true) == base_range);
        CHECK(calls == initial_calls + 3);
        CHECK(rng_get_engine() != before.rng);
        const auto native_rng = rng_get_engine();
        CHECK(candidate->gun_range(&get_avatar()) == player_range);
        CHECK(calls == initial_calls + 6);
        CHECK(rng_get_engine() != native_rng);
        const auto price_rng = rng_get_engine();
        CHECK(candidate->price(false) == price);
        CHECK(calls == initial_calls + 7);
        CHECK(rng_get_engine() != price_rng);
    }
    const auto native_calls = calls;
    const auto native_rng = rng_get_engine();
    CHECK(candidate->info_string() == first);
    // Inspection retains gun/ammo preview construction and the native range call chain.
    CHECK(calls > native_calls);
    CHECK(rng_get_engine() != native_rng);
    CHECK(saved(*candidate) == before.candidate);
    CHECK(saved_submap() == before.world);
}

TEST_CASE(
    "passive transform descriptions preserve native delegated spawn effects",
    "[iteminfo][observation][rng]") {
    const auto fixture = observation_fixture{};
    auto calls = 0;
    const auto target_actor = range_actor_fixture{itype_id("test_info_range_child"), calls};
    auto candidate = item::spawn("test_info_range_countdown", calendar::turn);
    const auto before = observation_baseline{*candidate};
    const auto initial_calls = calls;
    const auto first = observe(*candidate);
    CHECK(first.find("Turns into") != std::string::npos);
    CHECK(first.find("Countdown") != std::string::npos);
    CHECK(observe(*candidate) == first);
    before.check(*candidate);
    CHECK(calls == initial_calls);
    CHECK(candidate->info_string() == first);
    CHECK(calls == initial_calls + 1);
    CHECK(rng_get_engine() != before.rng);
    CHECK(saved(*candidate) == before.candidate);
    CHECK(saved_submap() == before.world);
}

TEST_CASE(
    "native inspection retains snippet reading and full information", "[iteminfo][observation]") {
    const auto fixture = observation_fixture{};
    auto jersey = item::spawn("test_info_jersey", calendar::turn);
    const auto id = snippet_id("test_info_unseen");
    REQUIRE_FALSE(get_avatar().has_seen_snippet(id));
    const auto before = observation_baseline{*jersey};
    const auto passive = observe(*jersey);
    before.check(*jersey);
    CHECK(passive.find("An unseen observation jersey inscription.") != std::string::npos);
    const auto inspected = jersey->info_string();
    CHECK(inspected == passive);
    CHECK(get_avatar().has_seen_snippet(id));
    CHECK(get_avatar().get_value("has_seen_snippet_test_info_unseen") == "true");
    CHECK(saved(get_avatar()) != before.avatar);
    CHECK(saved(*jersey) == before.candidate);
    CHECK(rng_get_engine() == before.rng);
}

TEST_CASE(
    "observation includes fresh autolearn recipes without storing them",
    "[iteminfo][observation][npc]") {
    const auto fixture = observation_fixture{};
    auto& you = get_avatar();
    auto helper = npc{};
    helper.set_skill_level(skill_id("mechanics"), 2);
    you.set_skill_level(skill_id("mechanics"), 2);
    const auto& recipe = recipe_id("test_info_result").obj();
    auto jersey = item::spawn("test_info_jersey", calendar::turn);
    you.i_add(item::spawn("test_info_jersey", calendar::turn));
    get_map().ter_set(info_test_pos + tripoint_east, ter_id("t_sewage"));
    get_map().ter_set(info_test_pos + tripoint_west, ter_id("t_water_sh"));
    const auto helper_before = saved(helper);
    const auto before = observation_baseline{*jersey};
    REQUIRE(before.avatar.find("test_info_result") == std::string::npos);
    REQUIRE(helper_before.find("test_info_result") == std::string::npos);
    const auto recipes = you.get_learned_recipes_for_display();
    CHECK(recipes.contains(recipe));
    before.check(*jersey);
    const auto inventory = you.crafting_inventory_for_display();
    CHECK(inventory.has_amount(itype_id("water"), 1));
    CHECK(inventory.has_amount(itype_id("water_sewage"), 1));
    before.check(*jersey);
    const auto helpers = std::vector<npc*>{&helper};
    CHECK(you.get_available_recipes(
                 {.crafting_inv = inventory, .helpers = &helpers, .observation = true})
              .contains(recipe));
    CHECK(saved(helper) == helper_before);
    before.check(*jersey);
    CHECK(observe(*jersey).find("TEST observation water assembly") != std::string::npos);
    before.check(*jersey);

    // Native knowledge lookup still commits eligible recipes. No rollback masks observation.
    CHECK(helper.get_learned_recipes().contains(recipe));
    CHECK(saved(helper) != helper_before);
    CHECK(you.get_learned_recipes().contains(recipe));
    CHECK(saved(you) != before.avatar);
}

TEST_CASE(
    "resource observation does not sample or prime the native crafting cache",
    "[iteminfo][observation][rng]") {
    const auto fixture = observation_fixture{};
    auto& here = get_map();
    auto& you = get_avatar();
    const auto sewage = info_test_pos + tripoint_east;
    const auto natural = info_test_pos + tripoint_west;
    here.ter_set(sewage, ter_id("t_sewage"));
    here.ter_set(natural, ter_id("t_water_sh"));
    auto marker = item::spawn("test_info_result", calendar::turn);
    const auto before = observation_baseline{*marker};
    CHECK(here.water_source_at(sewage)->type == itype_id("water_sewage"));
    CHECK(here.water_source_at(natural)->type == itype_id("water"));
    const auto inventory = you.crafting_inventory_for_display();
    CHECK(inventory.has_amount(itype_id("water"), 1));
    CHECK(inventory.has_amount(itype_id("water_sewage"), 1));
    before.check(*marker);

    const auto rng_before_native = rng_get_engine();
    you.crafting_inventory();
    CHECK(rng_get_engine() != rng_before_native);
    const auto rng_after_native = rng_get_engine();
    you.crafting_inventory();
    CHECK(rng_get_engine() == rng_after_native);
    const auto taken = here.water_from(sewage);
    CHECK(taken->typeId() == itype_id("water_sewage"));
    CHECK(taken->poison >= 1);
    CHECK(taken->poison <= 7);
}

TEST_CASE("observation does not repair stale item ownership", "[iteminfo][observation]") {
    const auto fixture = observation_fixture{};
    auto value = item::spawn("test_info_result", calendar::turn);
    value->set_owner(faction_id("test_info_missing_owner"));
    value->set_old_owner(faction_id("test_info_missing_previous_owner"));
    auto native = item::spawn(*value);
    const auto before = observation_baseline{*value};
    const auto first = observe(*value);
    before.check(*value);
    CHECK(observe(*value) == first);
    before.check(*value);
    CHECK(native->info_string() == first);
    CHECK(saved(*native).find("test_info_missing_owner") == std::string::npos);
    CHECK(saved(*native).find("test_info_missing_previous_owner") == std::string::npos);
}

TEST_CASE(
    "full passive descriptions preserve nearby component ownership",
    "[iteminfo][observation][nearby_ownership][faction][rng]") {
    const auto fixture = observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    // Preserve retained NPC faction pointers while installing isolated authority for this fixture.
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    const auto external_id = faction_id("test_info_faction");
    const auto actor_id = faction_id("your_followers");
    auto* external = manager.get(external_id);
    auto* actor_faction = manager.get(actor_id);
    REQUIRE(external != nullptr);
    REQUIRE(actor_faction != nullptr);
    auto owner = external_id;
    auto previous_owner = external_id;
    auto available = false;
    auto scenario = std::string{};

    SECTION("missing nearby owner and previous owner are effectively unowned") {
        scenario = "missing nearby owner and previous owner";
        owner = faction_id("test_info_missing_owner");
        previous_owner = faction_id("test_info_missing_previous_owner");
        available = true;
    }
    SECTION("missing nearby owner with an absent templated previous owner") {
        scenario = "missing nearby owner, absent templated previous owner";
        owner = faction_id("test_info_missing_owner");
        manager.remove_faction(external_id);
        available = true;
    }
    SECTION("friendly nearby owner with a stale previous owner") {
        scenario = "friendly nearby owner, stale previous owner";
        previous_owner = faction_id("test_info_missing_previous_owner");
    }
    SECTION("unvalidated friendly owner at the exclusion boundary") {
        scenario = "unvalidated friendly owner at -10";
        external->validated = false;
        external->name = "stale nearby faction name";
        external->desc = "stale nearby faction description";
        external->relations.clear();
        external->likes_u = -10;
    }
    SECTION("hostile runtime attitude just below the exclusion boundary") {
        scenario = "hostile runtime attitude at -11";
        external->likes_u = -11;
        available = true;
    }
    SECTION("absent nearby owner template is effectively friendly") {
        scenario = "absent friendly nearby owner template";
        manager.remove_faction(external_id);
    }
    SECTION("same faction ownership remains available") {
        scenario = "same faction ownership";
        owner = previous_owner = actor_id;
        available = true;
    }
    SECTION("unvalidated actor faction must not be repaired by ownership comparison") {
        scenario = "unvalidated actor faction";
        actor_faction->validated = false;
        actor_faction->name = "stale actor faction name";
        actor_faction->relations.clear();
    }
    SECTION("absent actor faction must not be created by ownership comparison") {
        scenario = "absent actor faction";
        manager.remove_faction(actor_id);
    }
    INFO(scenario);

    auto candidate = item::spawn("test_info_jersey", calendar::turn);
    auto component = item::spawn("test_info_jersey", calendar::turn);
    component->set_owner(owner);
    component->set_old_owner(previous_owner);
    component->invlet = 'z';
    const auto component_pos = info_test_pos + tripoint_north;
    get_map().add_item(component_pos, std::move(component));
    const auto& nearby = get_map().i_at(component_pos).only_item();
    get_avatar().set_skill_level(skill_id("mechanics"), 2);
    get_map().ter_set(info_test_pos + tripoint_east, ter_id("t_sewage"));
    get_map().ter_set(info_test_pos + tripoint_west, ter_id("t_water_sh"));
    get_avatar().invalidate_crafting_inventory();
    REQUIRE_FALSE(get_avatar().has_seen_snippet(snippet_id("test_info_unseen")));
    REQUIRE_FALSE(get_avatar().has_amount(itype_id("test_info_jersey"), 1));

    // No ownership getter or inventory/description query precedes this complete authority baseline.
    const auto before = observation_baseline{*candidate};
    const auto nearby_before = saved(nearby);
    const auto factions_before = ownership_factions();
    const auto check_authority = [&] {
        before.check(*candidate);
        CHECK(saved(nearby) == nearby_before);
        CHECK(ownership_factions() == factions_before);
    };
    const auto& rec = recipe_id("test_info_result").obj();
    const auto recipe_name = rec.result_name(true);
    const auto unavailable_name = colorize(recipe_name, c_dark_gray);
    const auto first = observe(*candidate);
    CHECK(first.find("An unseen observation jersey inscription.") != std::string::npos);
    CHECK(first.find(recipe_name) != std::string::npos);
    CHECK((first.find(unavailable_name) == std::string::npos) == available);
    {
        INFO("after first FULL passive description");
        check_authority();
    }
    CHECK(observe(*candidate) == first);
    {
        INFO("after repeated FULL passive description, compared to the original prestate");
        check_authority();
    }

    const auto effective_owner = manager.get_for_display(owner);
    if (owner == faction_id("test_info_missing_owner")) {
        CHECK_FALSE(effective_owner);
    } else {
        REQUIRE(effective_owner);
        CHECK((effective_owner->likes_u < -10 || owner == actor_id) == available);
    }
    for (const auto query : {"first", "repeated"}) {
        INFO(query << " local resource observation, compared to the original prestate");
        const auto inventory = get_avatar().crafting_inventory_for_display();
        CHECK(inventory.has_amount(itype_id("test_info_jersey"), 1) == available);
        CHECK(inventory.has_amount(itype_id("water"), 1));
        CHECK(inventory.has_amount(itype_id("water_sewage"), 1));
        CHECK(rec.can_make_with_inventory_for_display(inventory) == available);
        check_authority();
    }
    // After all passive checks, the first native inventory must still sample its cold resources.
    const auto native_rng = rng_get_engine();
    get_avatar().crafting_inventory();
    CHECK(rng_get_engine() != native_rng);
    const auto cached_rng = rng_get_engine();
    get_avatar().crafting_inventory();
    CHECK(rng_get_engine() == cached_rng);
}

TEST_CASE(
    "nearby ownership resource observation preserves a real NPC fallback faction",
    "[iteminfo][observation][nearby_ownership][faction][npc][rng]") {
    const auto fixture = observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    REQUIRE(manager.get(faction_id("your_followers")) != nullptr);
    REQUIRE(manager.get(faction_id("test_info_faction")) != nullptr);
    auto actor = npc{};
    // Initial placement, not migration of a fixture NPC that is unregistered in the overmap.
    const auto actor_pos = project_remain<coords::sm>(map_local_to_abs(get_map(), info_test_pos));
    actor.spawn_at_precise(actor_pos.quotient, actor_pos.remainder_tripoint);
    auto candidate = item::spawn("test_info_jersey", calendar::turn);
    auto component = item::spawn("test_info_jersey", calendar::turn);
    component->set_owner(faction_id("test_info_faction"));
    component->set_old_owner(faction_id("test_info_faction"));
    get_map().add_item(info_test_pos, std::move(component));
    REQUIRE_FALSE(manager.all().contains(faction_id("no_faction")));
    const auto before = observation_baseline{*candidate};
    const auto nearby_before = saved(get_map().i_at(info_test_pos).only_item());
    const auto actor_before = saved(actor);
    const auto factions_before = ownership_factions();
    const auto check_authority = [&] {
        before.check(*candidate);
        CHECK(saved(get_map().i_at(info_test_pos).only_item()) == nearby_before);
        CHECK(saved(actor) == actor_before);
        CHECK(ownership_factions() == factions_before);
    };
    const auto first = observe(*candidate);
    REQUIRE_FALSE(first.empty());
    check_authority();
    CHECK(observe(*candidate) == first);
    check_authority();
    for (const auto query : {"first", "repeated"}) {
        INFO(query << " real NPC resource observation without a cached faction pointer");
        const auto inventory = actor.crafting_inventory_for_display();
        CHECK_FALSE(inventory.has_amount(itype_id("test_info_jersey"), 1));
        check_authority();
    }
}

TEST_CASE(
    "polymorphic resource observation preserves cached and base Character faction identity",
    "[iteminfo][observation][nearby_ownership][faction][npc][rng]") {
    const auto fixture = observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    REQUIRE(manager.get(faction_id("your_followers")) != nullptr);
    const auto external_id = faction_id("test_info_faction");
    const auto fallback_id = faction_id("no_faction");
    auto* external = manager.get(external_id);
    REQUIRE(external != nullptr);
    auto cached = npc{};
    auto base = base_faction_character{};
    cached.setID(character_id(777));
    base.setID(character_id(778));
    const auto actor_pos = project_remain<coords::sm>(map_local_to_abs(get_map(), info_test_pos));
    cached.spawn_at_precise(actor_pos.quotient, actor_pos.remainder_tripoint);
    base.spawn_at_precise(actor_pos.quotient, actor_pos.remainder_tripoint);
    auto* actor = static_cast<Character*>(&cached);
    auto expected_id = std::optional{fallback_id};
    auto owner = external_id;
    auto available = false;
    auto native_faction_effect = true;
    auto scenario = std::string{};

    SECTION("fallback ignores a pending link ID without a cached faction pointer") {
        scenario = "fallback with pending link ID";
        cached.set_fac_id(external_id.str());
        REQUIRE_FALSE(manager.all().contains(fallback_id));
    }
    SECTION("fallback does not validate its stored faction") {
        scenario = "unvalidated fallback faction";
        auto* fallback = manager.get(fallback_id);
        REQUIRE(fallback != nullptr);
        fallback->validated = false;
        fallback->name = "stale fallback name";
    }
    SECTION("cached faction is used without validating its stored metadata") {
        scenario = "unvalidated cached same faction";
        native_faction_effect = false;
        cached.set_fac(external_id);
        external->validated = false;
        external->name = "stale cached faction name";
        external->desc = "stale cached faction description";
        expected_id = external_id;
        available = true;
    }
    SECTION("cached identity wins over a disagreeing pending link ID") {
        scenario = "cached identity differs from pending link ID";
        native_faction_effect = false;
        cached.set_fac(external_id);
        cached.set_fac_id("test_info_missing_actor_link");
        expected_id = external_id;
        available = true;
    }
    SECTION("cached runtime identity is not the owning manager key") {
        scenario = "cached runtime identity differs from owner key";
        native_faction_effect = false;
        cached.set_fac(external_id);
        external->id = faction_id("test_info_runtime_actor_identity");
        expected_id = external->id;
    }
    SECTION("base Character has no faction rather than the NPC fallback") {
        scenario = "base Character without cached faction";
        native_faction_effect = false;
        actor = &base;
        expected_id = std::nullopt;
        owner = fallback_id;
        REQUIRE(manager.get(fallback_id) != nullptr);
    }
    SECTION("base Character ignores a cached faction and pending link ID") {
        scenario = "base Character with cached faction";
        native_faction_effect = false;
        base.set_fac(external_id);
        base.set_fac_id("test_info_missing_actor_link");
        actor = &base;
        expected_id = std::nullopt;
    }
    SECTION("base Character still allows hostile foreign components") {
        scenario = "base Character with hostile foreign owner";
        native_faction_effect = false;
        actor = &base;
        expected_id = std::nullopt;
        external->likes_u = -11;
        available = true;
    }
    SECTION("base Character still allows effectively unowned components") {
        scenario = "base Character with missing owner";
        native_faction_effect = false;
        actor = &base;
        expected_id = std::nullopt;
        owner = faction_id("test_info_missing_owner");
        available = true;
    }
    INFO(scenario);

    auto candidate = item::spawn("test_info_jersey", calendar::turn);
    auto component = item::spawn("test_info_jersey", calendar::turn);
    component->set_owner(owner);
    component->set_old_owner(faction_id("test_info_missing_previous_owner"));
    get_map().add_item(info_test_pos, std::move(component));
    get_map().ter_set(info_test_pos + tripoint_east, ter_id("t_sewage"));
    const auto before = observation_baseline{*candidate};
    const auto nearby_before = saved(get_map().i_at(info_test_pos).only_item());
    const auto cached_before = saved(cached);
    const auto base_before = saved(base);
    const auto factions_before = ownership_factions();
    // This cache invalidation epoch is monotonic: compare across queries, never roll it back.
    const auto friends_before = g_npc_friends_dirty_version.load();
    const auto check_authority = [&] {
        before.check(*candidate);
        CHECK(saved(get_map().i_at(info_test_pos).only_item()) == nearby_before);
        CHECK(saved(cached) == cached_before);
        CHECK(saved(base) == base_before);
        CHECK(ownership_factions() == factions_before);
        CHECK(g_npc_friends_dirty_version.load() == friends_before);
    };
    const auto first = observe(*candidate);
    REQUIRE_FALSE(first.empty());
    check_authority();
    CHECK(observe(*candidate) == first);
    check_authority();
    for (const auto query : {"first", "repeated"}) {
        INFO(query << " polymorphic actor identity/resource observation");
        CHECK(actor->get_faction_id_for_display() == expected_id);
        const auto inventory = actor->crafting_inventory_for_display();
        CHECK(inventory.has_amount(itype_id("test_info_jersey"), 1) == available);
        CHECK(inventory.has_amount(itype_id("water_sewage"), 1));
        check_authority();
    }
    // Only after all passive checks: native fallback lookup still creates/validates authority,
    // while a cached NPC pointer and the base Character default retain their original semantics.
    const auto native_rng = rng_get_engine();
    const auto* native_faction = actor->get_faction();
    if (expected_id) {
        REQUIRE(native_faction != nullptr);
        CHECK(native_faction->id == *expected_id);
    } else {
        CHECK(native_faction == nullptr);
    }
    CHECK((ownership_factions() != factions_before) == native_faction_effect);
    CHECK(saved(get_map().i_at(info_test_pos).only_item()) == nearby_before);
    CHECK(rng_get_engine() == native_rng);
}

TEST_CASE(
    "native nearby ownership recovery and resource extraction remain effectful",
    "[iteminfo][observation][nearby_ownership][faction][rng]") {
    const auto fixture = observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    auto candidate = item::spawn("test_info_result", calendar::turn);
    auto component = item::spawn("test_info_jersey", calendar::turn);
    auto available = true;
    SECTION("missing owner recovery still validates and creates a templated previous owner") {
        component->set_owner(faction_id("test_info_missing_owner"));
        component->set_old_owner(faction_id("test_info_faction"));
        REQUIRE_FALSE(manager.all().contains(faction_id("test_info_faction")));
    }
    SECTION("ownership gating still validates the actor and repairs the previous owner") {
        component->set_owner(faction_id("test_info_faction"));
        component->set_old_owner(faction_id("test_info_missing_previous_owner"));
        REQUIRE(manager.get(faction_id("test_info_faction")) != nullptr);
        auto* actor_faction = manager.get(faction_id("your_followers"));
        REQUIRE(actor_faction != nullptr);
        actor_faction->validated = false;
        available = false;
    }
    get_map().add_item(info_test_pos, std::move(component));
    const auto& nearby = get_map().i_at(info_test_pos).only_item();
    const auto sewage = info_test_pos + tripoint_east;
    get_map().ter_set(sewage, ter_id("t_sewage"));
    const auto before = observation_baseline{*candidate};
    const auto nearby_before = saved(nearby);
    const auto factions_before = ownership_factions();
    const auto& inventory = get_avatar().crafting_inventory();
    CHECK(inventory.has_amount(itype_id("test_info_jersey"), 1) == available);
    CHECK(saved(nearby) != nearby_before);
    CHECK(saved(nearby).find("test_info_missing_owner") == std::string::npos);
    CHECK(saved(nearby).find("test_info_missing_previous_owner") == std::string::npos);
    CHECK(ownership_factions() != factions_before);
    CHECK(manager.all().at(faction_id("test_info_faction")).validated);
    if (!available) { CHECK(manager.all().at(faction_id("your_followers")).validated); }
    CHECK(saved_submap() != before.world);
    CHECK(saved(get_avatar()) == before.avatar);
    CHECK(saved(*candidate) == before.candidate);
    CHECK(rng_get_engine() != before.rng);

    const auto extraction_rng = rng_get_engine();
    const auto taken = get_map().water_from(sewage);
    REQUIRE(taken);
    CHECK(taken->typeId() == itype_id("water_sewage"));
    CHECK(taken->poison >= 1);
    CHECK(taken->poison <= 7);
    CHECK(rng_get_engine() != extraction_rng);
}

TEST_CASE(
    "local resource inventory leaves dropped-item invlets unchanged", "[iteminfo][observation]") {
    const auto fixture = observation_fixture{};
    auto dropped = item::spawn("test_info_result", calendar::turn);
    dropped->invlet = 'z';
    get_map().add_item(info_test_pos, std::move(dropped));
    auto marker = item::spawn("test_info_result", calendar::turn);
    const auto before = observation_baseline{*marker};
    const auto inventory = get_avatar().crafting_inventory_for_display();
    CHECK(inventory.has_amount(itype_id("test_info_result"), 1));
    before.check(*marker);
    CHECK(get_map().i_at(info_test_pos).only_item().invlet == 'z');
}

TEST_CASE(
    "transform descriptions preview without changing native use", "[iteminfo][observation][rng]") {
    const auto fixture = observation_fixture{};
    auto source = item::spawn("test_info_transform", calendar::turn);
    const auto before = observation_baseline{*source};
    const auto description = observe(*source);
    CHECK(description.find("TEST observation transform target") != std::string::npos);
    before.check(*source);
    CHECK(observe(*source) == description);
    before.check(*source);
    // Native inspection is deliberately effectful: retain its full-output assertion without
    // applying a passive-state assertion after its original random-snippet construction.
    CHECK(source->info_string() == description);
    CHECK(rng_get_engine() != before.rng);
    CHECK(saved(*source) == before.candidate);
    const auto* actor = dynamic_cast<const iuse_transform*>(
        source->get_use("transform")->get_actor_ptr());
    REQUIRE(actor != nullptr);
    actor->use(get_avatar(), *source, false, info_test_pos);
    CHECK(source->typeId() == itype_id("test_info_target"));
}

TEST_CASE(
    "recipe previews preserve native results and component rules",
    "[iteminfo][observation][recipe][rng]") {
    const auto fixture = observation_fixture{};
    auto id = recipe_id("test_info_target");
    SECTION("variable-size fitting result") {}
    SECTION("contained food") { id = recipe_id("test_info_food_child"); }
    SECTION("hot food and byproduct") { id = recipe_id("test_info_food_recursive"); }
    SECTION("dehydrated food") { id = recipe_id("test_info_food_dry"); }
    SECTION("shelf-stable food and full magazines") {
        id = recipe_id("test_info_food_shelf_stable");
    }
    const auto& rec = *id;
    auto fresh = item::spawn("test_info_food_component", calendar::turn, 1);
    auto rotten = item::spawn(*fresh);
    rotten->set_relative_rot(2.0);
    auto empty = item::spawn("test_info_battery", calendar::turn, 0);
    auto full = item::spawn(*empty);
    full->ammo_set(full->ammo_default(), full->ammo_capacity());
    REQUIRE(empty->is_magazine());
    REQUIRE(empty->ammo_remaining() == 0);
    REQUIRE(full->ammo_remaining() == full->ammo_capacity());
    const auto before = observation_baseline{*fresh};
    const auto preview = rec.create_result_for_display();
    before.check(*fresh);
    const auto normal_filter = rec.get_component_filter_for_display();
    const auto no_rotten = rec.get_component_filter_for_display(recipe_filter_flags::no_rotten);
    const auto hot = rec.hot_result_for_display();
    const auto dry = rec.dehydrate_result_for_display();
    before.check(*fresh);
    CHECK(saved(*rec.create_result_for_display()) == saved(*preview));
    before.check(*fresh);
    CHECK(hot == (id == recipe_id("test_info_food_recursive")));
    CHECK(dry == (id == recipe_id("test_info_food_dry")));
    if (id == recipe_id("test_info_food_dry")) { CHECK(preview->charges == 2); }
    if (id == recipe_id("test_info_food_child")) {
        REQUIRE(preview->contents.num_item_stacks() == 1);
        CHECK(preview->contents.front().typeId() == itype_id("test_info_food_child"));
        CHECK(preview->contents.front().charges == 1);
    }
    if (id == recipe_id("test_info_target")) { CHECK(preview->has_flag(flag_id("FIT"))); }
    if (id == recipe_id("test_info_food_shelf_stable")) {
        CHECK_FALSE(normal_filter(*rotten));
        CHECK_FALSE(normal_filter(*empty));
        CHECK(normal_filter(*full));
    }
    if (id == recipe_id("test_info_food_recursive")) {
        CHECK(normal_filter(*rotten));
        CHECK_FALSE(no_rotten(*rotten));
    }
    const auto native = rec.create_result();
    CHECK(native->typeId() == preview->typeId());
    CHECK(native->charges == preview->charges);
    CHECK(native->item_tags == preview->item_tags);
    CHECK(native->contents.num_item_stacks() == preview->contents.num_item_stacks());
    if (preview->contents.num_item_stacks() == 1) {
        CHECK(native->contents.front().typeId() == preview->contents.front().typeId());
        CHECK(native->contents.front().charges == preview->contents.front().charges);
    }
    const auto native_filter = rec.get_component_filter();
    const auto native_no_rotten = rec.get_component_filter(recipe_filter_flags::no_rotten);
    for (const auto* component : {fresh.get(), rotten.get(), empty.get(), full.get()}) {
        CHECK(normal_filter(*component) == native_filter(*component));
        CHECK(no_rotten(*component) == native_no_rotten(*component));
    }
    CHECK(rec.hot_result() == hot);
    CHECK(rec.dehydrate_result() == dry);
    // Real randomized-result construction still uses the gameplay engine.
    CHECK(rng_get_engine() != before.rng);
}

TEST_CASE(
    "recursive exemplar nutrients retain native availability and arithmetic",
    "[iteminfo][observation][recipe][rng]") {
    const auto fixture = observation_fixture{};
    auto& you = get_avatar();
    auto include_child = true;
    auto include_high = true;
    auto tools = true;
    auto skill = 2;
    auto expected = nutrient_range{
        .minimum = {.kcal = 80, .vitamins = {}}, .maximum = {.kcal = 380, .vitamins = {}}};
    SECTION("all ingredient alternatives") {}
    SECTION("unavailable high alternative") {
        include_high = false;
        expected.maximum.kcal = 180;
    }
    SECTION("unavailable recursive alternative") {
        include_child = false;
        expected.minimum.kcal = 380;
    }
    SECTION("missing required tool") {
        tools = false;
        expected.minimum.kcal = expected.maximum.kcal = 500;
    }
    SECTION("unknown recipe") {
        skill = 0;
        expected.minimum.kcal = expected.maximum.kcal = 500;
    }
    you.set_skill_level(skill_id("mechanics"), skill);
    if (include_child) { you.i_add(item::spawn("test_info_food_child", calendar::turn, 1)); }
    if (include_high) { you.i_add(item::spawn("test_info_food_high", calendar::turn, 1)); }
    you.i_add(item::spawn("test_info_food_low", calendar::turn, 1));
    if (tools) { you.i_add(item::spawn("hotplate", calendar::turn, 0)); }
    get_map().ter_set(info_test_pos + tripoint_east, ter_id("t_sewage"));
    auto food = item::spawn("test_info_food_recursive", calendar::turn, 1);
    REQUIRE_FALSE(food->has_flag(flag_id("NUTRIENT_OVERRIDE")));
    food->set_var("recipe_exemplar", "test_info_food_recursive");
    const auto before = observation_baseline{*food};
    const auto range = compute_nutrient_range_for_display(
        you, {.food = *food, .recipe = recipe_id("test_info_food_recursive")});
    CHECK(range.minimum.kcal == expected.minimum.kcal);
    CHECK(range.maximum.kcal == expected.maximum.kcal);
    before.check(*food);
    const auto first = observe(*food);
    CHECK(first.find("vary with available ingredients") != std::string::npos);
    CHECK(first.find("Calories") != std::string::npos);
    CHECK(first.find("Vitamins") != std::string::npos);
    before.check(*food);
    CHECK(observe(*food) == first);
    const auto repeated = compute_nutrient_range_for_display(
        you, {.food = *food, .recipe = recipe_id("test_info_food_recursive")});
    CHECK(repeated.minimum == range.minimum);
    CHECK(repeated.maximum == range.maximum);
    before.check(*food);
    const auto native = you.compute_nutrient_range(*food, recipe_id("test_info_food_recursive"));
    CHECK(native.first == range.minimum);
    CHECK(native.second == range.maximum);
    CHECK(food->info_string() == first);
    CHECK(rng_get_engine() != before.rng);
}

TEST_CASE(
    "effective faction descriptions do not validate or create authority",
    "[iteminfo][observation][faction]") {
    const auto fixture = observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    // Move map nodes rather than copying/replacing them: retained NPCs hold faction pointers.
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    auto value = item::spawn("test_info_result", calendar::turn);
    const auto id = faction_id("test_info_faction");
    value->set_owner(id);
    value->set_old_owner(id);
    auto* stored = manager.get(id);
    REQUIRE(stored != nullptr);
    SECTION("unvalidated stored faction") {
        stored->validated = false;
        stored->name = "stale template name";
        stored->desc = "stale template description";
        stored->currency = itype_id("water");
        stored->food_supply = 1234;
        stored->members.emplace(character_id(42), std::make_pair("member", true));
        stored->relations.clear();
        stored->relations["test_info_custom_relation"].set(npc_factions::watch_your_back);
    }
    SECTION("absent faction template") {
        manager.clear();
        stored = nullptr;
    }
    SECTION("validated custom runtime name") { stored->name = "custom faction name"; }
    SECTION("invalid previous owner") {
        value->set_old_owner(faction_id("test_info_missing_previous_owner"));
    }
    const auto before = observation_baseline{*value};
    const auto was_validated = stored && stored->validated;
    const auto effective = manager.get_for_display(id);
    REQUIRE(effective);
    before.check(*value);
    if (stored && !stored->validated) {
        CHECK(effective->name == "TEST observation faction");
        CHECK(effective->desc == "Effective observation faction description.");
        CHECK(effective->currency == itype_id("test_info_result"));
        CHECK(effective->food_supply == 1234);
        CHECK(effective->members.contains(character_id(42)));
        CHECK(effective->relations.contains("test_info_custom_relation"));
        CHECK(effective->relations.contains("test_info_faction"));
        CHECK_FALSE(stored->validated);
    }
    const auto first = observe(*value);
    CHECK(first.find("Owner: " + effective->name) != std::string::npos);
    before.check(*value);
    if (stored) { CHECK(stored->validated == was_validated); }
    CHECK(observe(*value) == first);
    CHECK(saved(*manager.get_for_display(id)) == saved(*effective));
    before.check(*value);
    if (stored) { CHECK(stored->validated == was_validated); }
    CHECK(value->info_string() == first);
    CHECK(saved(*value).find("test_info_missing_previous_owner") == std::string::npos);
    const auto* native = manager.get(id);
    REQUIRE(native != nullptr);
    CHECK(native->validated);
    CHECK(saved(*native) == saved(*effective));
}

#if defined(CATA_MCP)
namespace {

struct observation_ui_fixture {
    restore_on_out_of_scope<bool> restore_test_mode{test_mode};
    restore_on_out_of_scope<int> restore_termx{TERMX};
    restore_on_out_of_scope<int> restore_termy{TERMY};
    restore_on_out_of_scope<int> restore_full_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> restore_full_height{FULL_SCREEN_HEIGHT};
    restore_on_out_of_scope<catacurses::window> restore_stdscr{catacurses::stdscr};
    restore_on_out_of_scope<catacurses::window> restore_newscr{catacurses::newscr};
    restore_on_out_of_scope<uistatedata> restore_uistate{uistate};
    point old_screen = game_client::memory::screen_size();

    observation_ui_fixture() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~observation_ui_fixture() {
        game_client::memory::set_input_provider({});
        game_client::memory::resize(old_screen.x, old_screen.y);
    }
};

auto native_cancel() -> input_event {
    auto command = game_client::input_command{};
    command.action = "QUIT";
    const auto event =
        game_client::resolve_input_command(command, game_client::memory::screen_size());
    REQUIRE(event);
    return *event;
}

} // namespace

TEST_CASE(
    "passive native providers preserve first-query state off-page and on rejection",
    "[iteminfo][observation][client][interaction][mcp]") {
    const auto fixture = observation_fixture{};
    const auto ui = observation_ui_fixture{};
    auto& you = get_avatar();
    you.moves = 1000;
    auto marker = item::spawn("test_info_result", calendar::turn);
    auto selector = inventory_pick_selector{you};
    auto run = std::function<auto()->void>{};

    SECTION("inventory selector") {
        you.i_add(item::spawn("test_info_jersey", calendar::turn));
        you.i_add(item::spawn("test_info_book", calendar::turn));
        you.i_add(item::spawn("test_info_transform", calendar::turn));
        selector.add_character_items(you);
        run = [&]() { CHECK(selector.execute() == nullptr); };
    }
    SECTION("pickup details and semantic provider") {
        get_map().add_item(info_test_pos, item::spawn("test_info_jersey", calendar::turn));
        get_map().add_item(info_test_pos, item::spawn("test_info_book", calendar::turn));
        get_map().add_item(info_test_pos, item::spawn("test_info_transform", calendar::turn));
        run = [&]() { pickup::pick_up(info_test_pos, 0, pickup::from_ground); };
    }
    SECTION("advanced inventory provider") {
        you.i_add(item::spawn("test_info_jersey", calendar::turn));
        you.i_add(item::spawn("test_info_book", calendar::turn));
        get_map().add_item(info_test_pos, item::spawn("test_info_transform", calendar::turn));
        uistate.transfer_save = advanced_inv_save_state{};
        uistate.transfer_save.active_left = true;
        uistate.transfer_save.saved_area = AIM_INVENTORY;
        uistate.transfer_save.saved_area_right = AIM_CENTER;
        uistate.transfer_save.pane.area_idx = AIM_INVENTORY;
        uistate.transfer_save.pane_right.area_idx = AIM_CENTER;
        uistate.adv_inv_container_location = -1;
        run = []() { create_advanced_inv(); };
    }

    // Includes provider initialization and passive native redraw before the first descriptor.
    const auto before = observation_baseline{*marker};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        REQUIRE(reads == 1);
        before.check(*marker);
        const auto page = game_client::current_interaction({.offset = 0, .limit = 1});
        REQUIRE(page.structured);
        REQUIRE(page.choice_total >= 3);
        CHECK(page.choices.size() == 1);
        before.check(*marker);
        const auto repeated = game_client::current_interaction({.offset = 1, .limit = 1});
        CHECK(repeated.choice_total == page.choice_total);
        before.check(*marker);
        const auto invalid = game_client::resolve_interaction_command({
            .input_id = page.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = "nonexistent-item",
            .value = {},
            .submit = std::nullopt,
            .count = std::nullopt,
            .position = std::nullopt,
        });
        CHECK_FALSE(invalid);
        before.check(*marker);
        return native_cancel();
    });
    run();
    CHECK(reads == 1);
    before.check(*marker);
}
#endif

TEST_CASE(
    "nearby crafting ownership stays stale for observation and recovers natively",
    "[iteminfo][observation][nearby_ownership][rng]") {
    const auto fixture = observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    manager.create_if_needed();
    auto& you = get_avatar();
    auto component = item::spawn("test_info_jersey", calendar::turn);
    component->set_owner(faction_id("test_info_missing_owner"));
    component->set_old_owner(faction_id("test_info_missing_previous_owner"));
    get_map().add_item(info_test_pos, std::move(component));
    const auto before = saved(get_map().i_at(info_test_pos).only_item());
    const auto before_rng = rng_get_engine();
    you.invalidate_crafting_inventory();
    for (const auto pass : {0, 1}) {
        INFO("cold/repeated ownership observation " << pass);
        const auto observed = you.crafting_inventory_for_display();
        CHECK(observed.has_amount(itype_id("test_info_jersey"), 1));
        CHECK(saved(get_map().i_at(info_test_pos).only_item()) == before);
        CHECK(rng_get_engine() == before_rng);
    }
    const auto& native = you.crafting_inventory();
    CHECK(native.has_amount(itype_id("test_info_jersey"), 1));
    const auto after = saved(get_map().i_at(info_test_pos).only_item());
    CHECK(after.find("test_info_missing_owner") == std::string::npos);
    CHECK(after.find("test_info_missing_previous_owner") == std::string::npos);
}

TEST_CASE(
    "crafting observation keeps inventory letters of vehicle cargo",
    "[iteminfo][observation][vehicle]") {
    const auto fixture = observation_fixture{};
    auto& here = get_map();
    auto* const cart = here.add_vehicle(
        vproto_id("shopping_cart"), info_test_pos + tripoint_east, 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(info_test_pos + tripoint_east).part_with_feature("CARGO", true);
    REQUIRE(cargo);
    cargo->vehicle().get_items(cargo->part_index()).clear();
    auto lettered = item::spawn("test_info_jersey", calendar::turn);
    lettered->invlet = 'z';
    REQUIRE_FALSE(cargo->vehicle().add_item(cargo->part(), std::move(lettered)));
    const auto& stored = cargo->vehicle().get_items(cargo->part_index()).only_item();
    REQUIRE(stored.invlet == 'z');
    const auto before = observation_baseline{stored};

    const auto inventory = get_avatar().crafting_inventory_for_display();

    CHECK(inventory.has_amount(itype_id("test_info_jersey"), 1));
    CHECK(stored.invlet == 'z');
    before.check(stored);
}
