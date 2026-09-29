#include "catch/catch.hpp"
#include "reload/reload_ui.h"

TEST_CASE("reload_hotkey_preparation", "[reload][reload_ui]") {
    const auto last_ammo = itype_id("38_special");

    SECTION("matching ammunition without an inherited key uses the opening key") {
        auto state = reload_ui::prepare_hotkeys(last_ammo, 'r');
        const auto hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = last_ammo, .inherited_hotkey = char(-1), .index = 2});

        CHECK(hotkey == 'r');
        CHECK(state.opening_key_bound);
        CHECK(state.fallback_index == -1);
    }

    SECTION("an inherited key preserves the first matching fallback") {
        auto state = reload_ui::prepare_hotkeys(last_ammo, 'r');
        const auto first_hotkey = reload_ui::
            assign_hotkey(state, {.ammo_type = last_ammo, .inherited_hotkey = 'a', .index = 3});
        const auto repeated_hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = last_ammo, .inherited_hotkey = char(-1), .index = 4});

        CHECK(first_hotkey == 'a');
        CHECK(repeated_hotkey == char(-1));
        CHECK(state.opening_key_bound);
        CHECK(state.fallback_index == 3);
    }

    SECTION("an inherited opening-key collision disables the fallback") {
        auto state = reload_ui::prepare_hotkeys(last_ammo, 'r');
        const auto hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = itype_id("9mm"), .inherited_hotkey = 'r', .index = 1});

        CHECK(hotkey == 'r');
        CHECK(state.opening_key_bound);
        CHECK(state.fallback_index == -1);
    }

    SECTION("Enter never overrides a row hotkey") {
        auto state = reload_ui::prepare_hotkeys(last_ammo, '\n');
        const auto hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = last_ammo, .inherited_hotkey = char(-1), .index = 0});

        CHECK(hotkey == char(-1));
        CHECK(state.opening_key_bound);
        CHECK(state.fallback_index == -1);
    }

    SECTION("a later opening-key collision cancels an earlier matching fallback") {
        auto state = reload_ui::prepare_hotkeys(last_ammo, 'r');
        reload_ui::
            assign_hotkey(state, {.ammo_type = last_ammo, .inherited_hotkey = 'a', .index = 2});
        REQUIRE(state.fallback_index == 2);

        const auto hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = itype_id("9mm"), .inherited_hotkey = 'r', .index = 3});

        CHECK(hotkey == 'r');
        CHECK(state.fallback_index == -1);
    }

    SECTION("an extended opening key keeps the original narrowing and comparison") {
        const auto opening_key = 256 + 'r';
        auto state = reload_ui::prepare_hotkeys(last_ammo, opening_key);
        const auto hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = last_ammo, .inherited_hotkey = char(-1), .index = 2});

        CHECK(hotkey == static_cast<char>(opening_key));
        CHECK(state.opening_key_bound);
        CHECK(state.fallback_index == 0);
    }

    SECTION("without a remembered-type match the opening-key fallback stays on the first row") {
        auto state = reload_ui::prepare_hotkeys(last_ammo, 'r');
        const auto hotkey = reload_ui::assign_hotkey(
            state, {.ammo_type = itype_id("9mm"), .inherited_hotkey = 'a', .index = 2});

        CHECK(hotkey == 'a');
        CHECK_FALSE(state.opening_key_bound);
        CHECK(state.fallback_index == 0);
    }
}

TEST_CASE("reload_picker_wielded_key", "[reload][reload_ui]") {
    SECTION("an unlettered row shows the reload key and answers every binding") {
        const auto key = reload_ui::resolve_wielded_key(
            {.reload_keys = {'r', 'R'},
             .own_invlet = 0,
             .taken_invlets = {'a'},
             .action_keys = {'q'}});

        CHECK(key.keys == std::vector<char>{'r', 'R'});
        CHECK(key.shown == 'r');
        CHECK_FALSE(key.shadowed);
    }

    SECTION("a carried item's letter keeps the reload key, and the row still shows it shadowed") {
        const auto key = reload_ui::resolve_wielded_key(
            {.reload_keys = {'r', 'R'}, .own_invlet = 0, .taken_invlets = {'r'}, .action_keys = {}});

        CHECK(key.keys == std::vector<char>{'R'});
        CHECK(key.shown == 'r');
        CHECK(key.shadowed);
    }

    // A remapped reload key can land on a key the picker already uses, such as `e` for examine.
    SECTION("a picker action keeps the reload key") {
        const auto key = reload_ui::resolve_wielded_key(
            {.reload_keys = {'e'}, .own_invlet = 0, .taken_invlets = {}, .action_keys = {'e', 'w'}});

        CHECK(key.keys.empty());
        CHECK(key.shown == 'e');
        CHECK(key.shadowed);
    }

    SECTION("a lettered row keeps its letter and still answers the reload key") {
        const auto key = reload_ui::resolve_wielded_key(
            {.reload_keys = {'r'}, .own_invlet = 'a', .taken_invlets = {}, .action_keys = {}});

        CHECK(key.keys == std::vector<char>{'r'});
        CHECK_FALSE(key.shown.has_value());
    }

    SECTION("an unbound reload action offers no key") {
        const auto key = reload_ui::resolve_wielded_key(
            {.reload_keys = {}, .own_invlet = 0, .taken_invlets = {}, .action_keys = {}});

        CHECK(key.keys.empty());
        CHECK_FALSE(key.shown.has_value());
    }
}
