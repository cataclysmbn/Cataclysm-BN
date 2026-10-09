#include "cata_libintl.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "client_input.h"
#include "client_interaction_prepared.h"
#include "input.h"
#include "json.h"
#include "language.h"
#include "mod_manager.h"
#include "options.h"
#include "path_info.h"
#include "translation_snapshot.h"
#include "translations.h"
#include "worldfactory.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#if defined(__GLIBC__)
#    include <malloc.h>
#endif
#include <ranges>
#include <sstream>
#include <utility>

namespace {
struct catalogue_options {
    std::string plain = "first";
    std::string long_text = "long translation";
    std::optional<std::string> mod_text;
};

/// Real MO bytes exercised by the native catalogue parser, including contextual plurals.
auto catalogue(const catalogue_options& options = {}) -> std::string {
    struct entry {
        std::string original;
        std::string translated;
    };
    auto entries = std::vector<entry>{
        {.original = "",
         .translated =
             "Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit\nPlural-Forms: nplurals=2; plural=n!=1;\n"},
        {.original = std::string("ctx\004apple\0apples", 16),
         .translated = std::string("pomme\0pommes", 12)},
        {.original = "long", .translated = options.long_text},
        {.original = "plain", .translated = options.plain},
    };
    if (options.mod_text) {
        entries.push_back({.original = "mod-only", .translated = *options.mod_text});
    }
    std::ranges::sort(entries, {}, &entry::original);
    auto result = std::string(28 + entries.size() * 16, '\0');
    const auto put = [&](const std::size_t offset, const std::uint32_t value) {
        for (const auto byte : std::views::iota(std::size_t{0}, std::size_t{4})) {
            result[offset + byte] = static_cast<char>(value >> (byte * 8));
        }
    };
    put(0, 0x950412de);
    put(8, entries.size());
    put(12, 28);
    put(16, 28 + entries.size() * 8);
    for (const auto ordinal : std::views::iota(std::size_t{0}, entries.size())) {
        const auto& input = entries[ordinal];
        put(28 + ordinal * 8, input.original.size());
        put(32 + ordinal * 8, result.size());
        result += input.original;
        result += '\0';
        put(28 + entries.size() * 8 + ordinal * 8, input.translated.size());
        put(32 + entries.size() * 8 + ordinal * 8, result.size());
        result += input.translated;
        result += '\0';
    }
    return result;
}

auto locale(catalogue_options options = {}) -> localization::locale_snapshot {
    return localization::locale_snapshot::from_catalogues({catalogue(options)});
}
} // namespace

TEST_CASE(
    "translation producer and acquisition work is observable",
    "[translation_snapshot_measurement]") {
    // Controlled producer batches only. Resettable counters are not whole-startup measurements.
    const auto batch_size = std::size_t{10000};
    const auto batch_raw = std::string(200, 'r');
    const auto batch_context = std::string(32, 'c');
    const auto batch_plural = std::string(204, 'p');
    auto batch = std::vector<translation>{};
    batch.reserve(batch_size);
    localization::reset_snapshot_work();
#if defined(__GLIBC__)
    const auto before_heap = mallinfo2();
#endif
    const auto batch_start = std::chrono::steady_clock::now();
    for (const auto ordinal : std::views::iota(std::size_t{0}, batch_size)) {
        batch.push_back(pl_translation(batch_context, batch_raw, batch_plural));
        static_cast<void>(ordinal);
    }
    const auto batch_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - batch_start)
            .count();
#if defined(__GLIBC__)
    const auto after_heap = mallinfo2();
    std::cout << "translation-bulk glibc_held_heap_delta="
              << (static_cast<std::int64_t>(after_heap.uordblks + after_heap.hblkhd)
                  - static_cast<std::int64_t>(before_heap.uordblks + before_heap.hblkhd))
              << '\n';
#endif
    const auto producer = localization::snapshot_work();
    std::cout << "translation-bulk rows=" << batch_size
              << " raw_bytes=200 context_bytes=32 plural_bytes=204 producer_us=" << batch_us
              << " publication_allocations=" << producer.input_publications
              << " instrumented_copy_bytes=" << producer.input_copied_bytes << " hashed_text_bytes="
              << producer.input_hashed_bytes << " value_size=" << sizeof(translation)
              << " payload_size=" << sizeof(localization::translation_input)
              << " vector_storage_excluded=true\n";
    CHECK(producer.input_publications == batch_size);
    CHECK(producer.input_copied_bytes == batch_size * (200 + 32 + 204));
    CHECK(producer.input_hashed_bytes == producer.input_copied_bytes);
    const auto raw = std::string(2 * 1024 * 1024, 'x');
    localization::reset_snapshot_work();
    const auto producer_start = std::chrono::steady_clock::now();
    auto source = to_translation(raw);
    const auto producer_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - producer_start)
            .count();
    source.translated();
    localization::reset_snapshot_work();
    const auto acquire_start = std::chrono::steady_clock::now();
    auto same_storage = true;
    for (const auto ordinal : std::views::iota(0, 100000)) {
        const auto snapshot = source.snapshot();
        same_storage = same_storage && snapshot.raw_view().data() == source.debug_get_raw().data();
        static_cast<void>(ordinal);
    }
    REQUIRE(same_storage);
    const auto acquire_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - acquire_start)
            .count();
    std::cout << "translation-large producer_us=" << producer_us
              << " acquisitions=100000 acquire_us=" << acquire_us
              << " acquisitions_with_pointer_check_no_Catch_loop=true\n";
    CHECK(localization::snapshot_work().input_publications == 0);
    CHECK(localization::snapshot_work().input_hashed_bytes == 0);
    CHECK(localization::snapshot_work().input_copied_bytes == 0);
}

TEST_CASE(
    "translation input acquisition never clones cold or warmed long text",
    "[translation_snapshot][translations]") {
    const auto bytes = std::size_t{2 * 1024 * 1024};
    const auto raw = std::string(bytes, 'x');
    localization::reset_snapshot_work();
    auto source = to_translation(raw);
    CHECK(localization::snapshot_work().input_publications == 1);
    CHECK(localization::snapshot_work().input_copied_bytes == bytes);
    CHECK(localization::snapshot_work().input_hashed_bytes == bytes);
    const auto cold = source.snapshot();
    CHECK(cold.raw_view().data() == source.debug_get_raw().data());
    CHECK(source.translated() == raw); // warm the actual eager cache
    localization::reset_snapshot_work();
    for (const auto index : std::views::iota(0, 100)) {
        const auto captured = source.snapshot();
        CHECK(captured.raw_view().data() == cold.raw_view().data());
        CHECK(captured.fingerprint() == cold.fingerprint());
        static_cast<void>(index);
    }
    CHECK(localization::snapshot_work().input_acquisitions == 100);
    CHECK(localization::snapshot_work().input_publications == 0);
    CHECK(localization::snapshot_work().input_copied_bytes == 0);
    CHECK(localization::snapshot_work().input_hashed_bytes == 0);
    auto copied = source;
    auto moved = std::move(copied);
    CHECK(moved == source);
    moved.add_context("different");
    CHECK(moved != source);
    CHECK(cold.raw_view() == raw);
    source = translation{};
    CHECK(source.empty());
    CHECK(cold.translated({}) == raw);
    CHECK(cold.raw_view().data() != nullptr);
    const auto translated_locale = locale({.long_text = raw});
    const auto translated = to_translation("long").snapshot();
    CHECK(translated.translated(translated_locale) == raw);
    const auto survivor = [&]() { return to_translation(raw).snapshot(); }();
    CHECK(survivor.translated({}) == raw);
}

TEST_CASE(
    "frozen translations preserve context plural literal fallback and publication identity",
    "[translation_snapshot][translations]") {
    const auto first = locale();
    const auto same = locale();
    const auto changed = locale({.plain = "second"});
    CHECK(first.fingerprint() == same.fingerprint());
    CHECK(first.fingerprint() != changed.fingerprint());
    const auto plain = to_translation("plain").snapshot();
    CHECK(plain.translated(first) == "first");
    CHECK(plain.translated(changed) == "second");
    CHECK(plain.translated(first) == "first");
    CHECK(no_translation("plain").snapshot().translated(first) == "plain");
    CHECK(to_translation("missing").snapshot().translated(first) == "missing");
    const auto contextual = pl_translation("ctx", "apple", "apples");
    CHECK(contextual.snapshot().translated(first, 1) == "pomme");
    CHECK(contextual.snapshot().translated(first, 2) == "pommes");
    CHECK(contextual.snapshot().translated({}, 1) == "apple");
    CHECK(contextual.snapshot().translated({}, 2) == "apples");
    CHECK(translation{}.snapshot().fingerprint() == no_translation("").snapshot().fingerprint());
    CHECK(to_translation("").snapshot().translated(first).empty());
    CHECK(to_translation("plain").legacy_hash().first);
    CHECK_FALSE(contextual.legacy_hash().first);
    CHECK_THROWS_AS(
        localization::locale_snapshot::from_catalogues({"not a catalogue"}), std::runtime_error);
    auto source = to_translation("plain");
    const auto before = source.snapshot();
    SECTION("context mutation") { source.add_context("ctx"); }
    SECTION("plural mutation") { source.make_plural(); }
    SECTION("raw deserialization") {
        auto stream = std::istringstream{"\"changed raw\""};
        auto json = JsonIn{stream};
        source.deserialize(json);
    }
    SECTION("contextual plural deserialization") {
        source.make_plural();
        auto stream = std::istringstream{
            R"json({"ctxt":"ctx","str":"apple","str_pl":"apples","//NOLINT(cata-text-style)":true})json"};
        auto json = JsonIn{stream};
        source.deserialize(json);
        CHECK(source == contextual);
    }
    SECTION("same singular plural deserialization") {
        source.make_plural();
        auto stream = std::istringstream{
            R"json({"str_sp":"plain","//NOLINT(cata-text-style)":true})json"};
        auto json = JsonIn{stream};
        source.deserialize(json);
        CHECK(source.snapshot().translated({}, 2) == "plain");
    }
    CHECK(source.snapshot().fingerprint() != before.fingerprint());
    CHECK(before.translated(first) == "first");
}

TEST_CASE(
    "pinning the current catalogue preserves eager callers and does not invalidate language",
    "[translation_snapshot][translations]") {
    const auto pinned = l10n_data::pin_library();
    const auto version = detail::get_current_language_version();
    const auto* old_library = &l10n_data::get_library();
    const auto source = to_translation("plain");
    const auto value = source.snapshot().translated(pinned);
    const auto eager = source.translated();
    CHECK(value == eager);
    CHECK(&pinned.library() == old_library);
    localization::reset_snapshot_work();
    const auto another = l10n_data::pin_library();
    CHECK(another.fingerprint() == pinned.fingerprint());
    CHECK(detail::get_current_language_version() == version);
    CHECK(localization::snapshot_work().catalogue_publications == 0);
    CHECK(localization::snapshot_work().catalogue_hashed_bytes == 0);
    // Replaces the actual singleton publication. Restores catalogue selection through its producer.
    const auto restore = on_out_of_scope([]() { l10n_data::reload_catalogues(); });
    l10n_data::reload_catalogues();
    CHECK(&l10n_data::get_library() != old_library);
    CHECK(&pinned.library() == old_library);
    CHECK(source.snapshot().translated(pinned) == value);
    CHECK(source.translated() == eager);
}

TEST_CASE(
    "already plural long inputs invalidate only the eager cache",
    "[translation_snapshot][translations]") {
    const auto raw = std::string(2 * 1024 * 1024, 'x');
    auto source = pl_translation("ctx", raw, raw + "s");
    const auto frozen = source.snapshot();
    CHECK(source.translated(2) == raw + "s");
    localization::reset_snapshot_work();
    source.make_plural();
    const auto after = source.snapshot();
    CHECK(after.raw_view().data() == frozen.raw_view().data());
    CHECK(after.fingerprint() == frozen.fingerprint());
    CHECK(localization::snapshot_work().input_publications == 0);
    CHECK(localization::snapshot_work().input_copied_bytes == 0);
    CHECK(localization::snapshot_work().input_hashed_bytes == 0);
    CHECK(source.translated(2) == raw + "s");
    CHECK(localization::snapshot_work().eager_cache_refreshes == 1);
    CHECK(source.translated(2) == raw + "s");
    CHECK(localization::snapshot_work().eager_cache_refreshes == 1);
    CHECK(localization::snapshot_work().input_publications == 0);
}

TEST_CASE(
    "translation assignments comparisons and eager cache keep value behavior",
    "[translation_snapshot][translations]") {
    const auto singular = std::string{"__snapshot_one__"};
    const auto plural = std::string{"__snapshot_many__"};
    auto source = pl_translation(singular, plural);
    localization::reset_snapshot_work();
    CHECK(source.translated(1) == singular);
    CHECK(source.translated(1) == singular);
    CHECK(localization::snapshot_work().eager_cache_refreshes == 1);
    CHECK(source.translated(2) == plural);
    CHECK(localization::snapshot_work().eager_cache_refreshes == 2);
    const auto frozen = source.snapshot();
    auto assigned = to_translation("different");
    assigned.translated();
    assigned = source;
    auto moved = no_translation("another");
    moved = std::move(assigned);
    localization::reset_snapshot_work();
    CHECK(moved == source);
    CHECK(moved.translated(2) == plural);
    CHECK(localization::snapshot_work().eager_cache_refreshes == 0);
    moved.add_context("context");
    CHECK(moved != source);
    CHECK(moved.translated(2) == plural);
    CHECK(source.translated(2) == plural);
    CHECK(localization::snapshot_work().eager_cache_refreshes == 1);
    CHECK(frozen.translated({}, 2) == plural);
    source = translation{};
    moved = translation{};
    CHECK(frozen.translated({}, 1) == singular);
    CHECK(frozen.translated({}, 2) == plural);
    CHECK(to_translation("aaa").translated_lt(to_translation("zzz")));
    CHECK(no_translation("aaa").translated_eq(to_translation("aaa")));
    CHECK(no_translation("aaa").translated_ne(to_translation("zzz")));
    CHECK(no_translation("aaa") != to_translation("aaa"));
    CHECK_FALSE(no_translation("aaa").legacy_hash().first);
    CHECK_FALSE(pl_translation("aaa", "aaas").legacy_hash().first);
    CHECK((to_translation("aaa") + "bbb") == "aaabbb");
    CHECK((std::string{"aaa"} + to_translation("bbb")) == "aaabbb");
    CHECK((to_translation("aaa") + to_translation("bbb")) == "aaabbb");
    const auto surviving = []() { return pl_translation("ctx", "apple", "apples").snapshot(); }();
    const auto owned_locale = locale();
    CHECK(surviving.translated(owned_locale, 1) == "pomme");
    CHECK(surviving.translated(owned_locale, 2) == "pommes");
}

TEST_CASE(
    "malformed deserialization preserves historical partial updates and frozen inputs",
    "[translation_snapshot][translations]") {
    auto source = pl_translation("old context", "original", "originals");
    source.translated(2);
    const auto frozen = source.snapshot();
    auto expected = source;
    auto malformed = std::string{};
    SECTION("new context remains after bad raw value") {
        malformed = R"json({"ctxt":"new context","str":42})json";
        expected = pl_translation("new context", "original", "originals");
    }
    SECTION("context is cleared before bad raw value") {
        malformed = R"json({"str":42})json";
        expected = pl_translation("original", "originals");
    }
    SECTION("bad root leaves inputs unchanged") { malformed = "[]"; }
    SECTION("string parser clears context before throwing") {
        malformed = "\"unterminated";
        expected = pl_translation("original", "originals");
    }
    auto stream = std::istringstream{malformed};
    auto json = JsonIn{stream};
    localization::reset_snapshot_work();
    CHECK_THROWS_AS(source.deserialize(json), JsonError);
    CHECK(source == expected);
    CHECK(source.snapshot().fingerprint() == expected.snapshot().fingerprint());
    CHECK(frozen.translated({}, 2) == "originals");
    CHECK(frozen.raw_view() == "original");
    CHECK(source.translated(2) == "originals");
    CHECK(localization::snapshot_work().eager_cache_refreshes == 1);
}

TEST_CASE(
    "actual language and mod publishers retain pinned descriptions across replacements",
    "[translation_snapshot][translations]") {
    REQUIRE(world_generator);
    REQUIRE(world_generator->active_world);
    const auto base_path = PATH_INFO::base_path();
    const auto language = get_option<std::string>("USE_LANG");
    const auto old_loaded = l10n_data::mod_catalogues_are_loaded();
    const auto old_locale = l10n_data::pin_library();
    auto& mods = world_generator->active_world->info->active_mod_order;
    const auto old_mods = mods;
    auto& mod = const_cast<MOD_INFORMATION&>(mod_id{"test_data"}.obj());
    const auto old_mod_path = mod.path;
    const auto directory =
        std::filesystem::path{PATH_INFO::user_dir()} / "translation-publication-fixture";
    REQUIRE(std::filesystem::create_directory(directory));
    const auto restore = [&]() {
        mod.path = old_mod_path;
        mods = old_mods;
        PATH_INFO::init_base_path(base_path);
        get_options().get_option("USE_LANG").setValue(language);
        set_language();
        l10n_data::unload_catalogues();
        l10n_data::load_mod_catalogues();
        if (!old_loaded) { l10n_data::unload_mod_catalogues(); }
        std::filesystem::remove_all(directory);
    };
    auto cleanup = on_out_of_scope{restore};
    const auto write = [&](const std::filesystem::path& path, const catalogue_options& options) {
        std::filesystem::create_directories(path.parent_path());
        auto output = std::ofstream{path, std::ios::binary};
        const auto bytes = catalogue(options);
        output.write(bytes.data(), bytes.size());
        REQUIRE(output.good());
    };
    const auto english = directory / "lang/mo/en_US/LC_MESSAGES/cataclysm-bn.mo";
    const auto french = directory / "lang/mo/fr_FR/LC_MESSAGES/cataclysm-bn.mo";
    write(english, {.plain = "English first", .long_text = std::string(2 * 1024 * 1024, 'e')});
    write(french, {.plain = "French second"});
    mod.path = (directory / "mod").string();
    const auto mod_catalogue = directory / "mod/lang/fr_FR.mo";
    write(mod_catalogue, {.mod_text = "mod first"});
    mods.clear();
    PATH_INFO::init_base_path(directory.string());
    get_options().get_option("USE_LANG").setValue("en_US");
    set_language();
    const auto english_locale = l10n_data::pin_library();
    auto source = to_translation("plain");
    auto long_source = to_translation("long");
    CHECK(source.translated() == "English first");
    CHECK(long_source.translated() == std::string(2 * 1024 * 1024, 'e'));
    localization::reset_snapshot_work();
    const auto long_frozen = long_source.snapshot();
    const auto another_english = l10n_data::pin_library();
    CHECK(localization::snapshot_work().input_copied_bytes == 0);
    CHECK(localization::snapshot_work().input_hashed_bytes == 0);
    CHECK(localization::snapshot_work().catalogue_hashed_bytes == 0);
    CHECK(another_english.fingerprint() == english_locale.fingerprint());
    const auto frozen = source.snapshot();
    get_options().get_option("USE_LANG").setValue("fr_FR");
    set_language();
    const auto french_locale = l10n_data::pin_library();
    CHECK(french_locale.fingerprint() != english_locale.fingerprint());
    CHECK(source.translated() == "French second");
    CHECK(frozen.translated(english_locale) == "English first");
    CHECK(long_frozen.translated(english_locale) == std::string(2 * 1024 * 1024, 'e'));
    l10n_data::unload_catalogues();
    mods = {mod_id{"test_data"}};
    l10n_data::load_mod_catalogues();
    const auto first_mod = l10n_data::pin_library();
    const auto mod_input = to_translation("mod-only").snapshot();
    CHECK(mod_input.translated(first_mod) == "mod first");
    CHECK(source.translated() == "French second"); // base catalogue still wins duplicates
    auto context = input_context{"FROZEN_LOCALE"};
    const auto input = game_client::input_context_scope{context, "FROZEN_LOCALE"};
    const auto prepare = [&](const localization::locale_snapshot& pinned) {
        auto model = game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices = {{.id = "description", .label = "Description"}}};
        return game_client::prepare_interaction(
            context, std::move(model),
            {
                .dependency_keys =
                    {std::to_string(pinned.fingerprint()) + ":"
                     + std::to_string(mod_input.fingerprint())},
                .render = [owned = pinned,
                           mod_input](const auto /*index*/) { return mod_input.translated(owned); },
            });
    };
    auto old_schema = std::string{};
    {
        const auto old_boundary =
            game_client::prepared_interaction_scope{context, prepare(first_mod)};
        old_schema = game_client::current_interaction({.limit = 0}).schema_id;
        write(mod_catalogue, {.mod_text = "mod second"});
        l10n_data::reload_catalogues();
        CHECK(game_client::current_interaction({.limit = 1}).choices.front().description
              == "mod first");
        CHECK(game_client::current_interaction({.limit = 0}).schema_id == old_schema);
    }
    const auto second_mod = l10n_data::pin_library();
    CHECK(second_mod.fingerprint() != first_mod.fingerprint());
    {
        const auto new_boundary =
            game_client::prepared_interaction_scope{context, prepare(second_mod)};
        CHECK(game_client::current_interaction({.limit = 1}).choices.front().description
              == "mod second");
        CHECK(game_client::current_interaction({.limit = 0}).schema_id != old_schema);
    }
    l10n_data::unload_mod_catalogues();
    CHECK(to_translation("mod-only").translated() == "mod-only");
    CHECK(mod_input.translated(first_mod) == "mod first");
    CHECK(mod_input.translated(second_mod) == "mod second");
    CHECK(source.translated() == "French second");
    l10n_data::unload_catalogues();
    CHECK(source.translated() == "plain");
    CHECK(frozen.translated(english_locale) == "English first");
    restore();
    cleanup.cancel();
    CHECK(l10n_data::pin_library().fingerprint() == old_locale.fingerprint());
    CHECK(l10n_data::mod_catalogues_are_loaded() == old_loaded);
}
