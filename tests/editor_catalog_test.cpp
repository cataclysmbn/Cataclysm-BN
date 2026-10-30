#include "catch/catch.hpp"
#include "editor/catalog.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

auto ids(const editor::catalog_page& page) -> std::vector<std::string> {
    return page.entries | std::views::transform(&editor::catalog_entry::id)
         | std::ranges::to<std::vector>();
}

} // namespace

TEST_CASE("every kind of catalog lists what the game loaded", "[editor_catalog]") {
    using enum editor::catalog_kind;
    for (const auto kind :
         {terrain, furniture, trap, field, item, item_group, monster, monster_group, vehicle,
          npc_template, nested_mapgen, palette, overmap_terrain}) {
        CAPTURE(static_cast<int>(kind));
        const auto page = editor::search_catalog({.kind = kind});
        CHECK(page.total > 10);
        CHECK(page.entries.size() == std::min<std::size_t>(page.total, 50));
        CHECK(std::ranges::is_sorted(ids(page)));
    }
}

TEST_CASE("a catalog filter matches the id or the name, ignoring case", "[editor_catalog]") {
    const auto by_id = editor::search_catalog(
        {.kind = editor::catalog_kind::terrain, .filter = "T_WALL_LOG"});
    CHECK(std::ranges::contains(ids(by_id), std::string("t_wall_log")));
    CHECK(std::ranges::all_of(by_id.entries, [](const auto& entry) {
        return entry.id.find("t_wall_log") != std::string::npos;
    }));
    const auto by_name = editor::search_catalog(
        {.kind = editor::catalog_kind::monster, .filter = "zombie dog"});
    // No monster id holds a space, so these matched by name.
    CHECK(by_name.total > 0);
    CHECK(std::ranges::all_of(by_name.entries, [](const auto& entry) {
        return entry.id.find(' ') == std::string::npos;
    }));
}

TEST_CASE("catalog pages split the matches without gaps", "[editor_catalog]") {
    const auto query = editor::
        catalog_query{.kind = editor::catalog_kind::item, .filter = "knife", .page_size = 7};
    const auto first = editor::search_catalog(query);
    REQUIRE(first.total > 14);
    auto all = std::vector<std::string>{};
    for (auto page = std::size_t{0}; page * 7 < first.total; ++page) {
        auto next = query;
        next.page = page;
        std::ranges::copy(ids(editor::search_catalog(next)), std::back_inserter(all));
    }
    CHECK(all.size() == first.total);
    std::ranges::sort(all);
    CHECK(std::ranges::adjacent_find(all) == all.end());
    auto past = query;
    past.page = first.total;
    CHECK(editor::search_catalog(past).entries.empty());
}

TEST_CASE("a catalog keeps what one mod last defined", "[editor_catalog]") {
    const auto core = editor::search_catalog(
        {.kind = editor::catalog_kind::furniture, .mod = "bn"});
    CHECK(core.total > 10);
    CHECK(editor::search_catalog({.kind = editor::catalog_kind::furniture, .mod = "no_such_mod"})
              .total
          == 0);
}

TEST_CASE(
    "a catalog search ranks an exact name first, then names and words starting with it, names "
    "holding it, and ids holding it",
    "[editor_catalog]") {
    const auto found = editor::search_catalog(
        {.kind = editor::catalog_kind::furniture, .filter = "Bed", .page_size = 1000});
    REQUIRE(found.total > 5);
    const auto rank = [](const editor::catalog_entry& entry) {
        auto name = entry.name;
        std::ranges::transform(name, name.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (name == "bed") { return 0; }
        if (name.starts_with("bed")) { return 1; }
        if (name.contains(" bed")) { return 2; }
        if (name.contains("bed")) { return 3; }
        return 4;
    };
    CHECK(found.entries.front().id == "f_bed");
    CHECK(std::ranges::is_sorted(found.entries, {}, [&](const editor::catalog_entry& entry) {
        return rank(entry);
    }));
    // Something matched by its id alone comes last.
    CHECK(rank(found.entries.back()) == 4);
}

TEST_CASE(
    "among names that match a search alike, the game's own and shortest come first",
    "[editor_catalog]") {
    const auto first = [](const editor::catalog_kind kind, const std::string& filter) {
        const auto found = editor::search_catalog({.kind = kind, .filter = filter, .page_size = 1});
        REQUIRE_FALSE(found.entries.empty());
        return found.entries.front().id;
    };
    CHECK(first(editor::catalog_kind::terrain, "door") == "t_door_c");
    CHECK(first(editor::catalog_kind::furniture, "counter") == "f_counter");
}

TEST_CASE(
    "catalog entries that share a name are told apart by the part of their id that differs",
    "[editor_catalog]") {
    const auto found = editor::search_catalog(
        {.kind = editor::catalog_kind::terrain, .page_size = 100000});
    auto shared = 0;
    for (const auto& entry : found.entries) {
        const auto twins =
            found.entries | std::views::filter([&](const editor::catalog_entry& other) {
                return other.name == entry.name;
            })
            | std::views::transform(&editor::catalog_entry::id) | std::ranges::to<std::vector>();
        INFO(entry.id);
        if (twins.size() == 1) {
            CHECK(entry.distinct.empty());
            continue;
        }
        ++shared;
        // The suffix is what follows the start every twin's id shares, without separators,
        // so it never repeats the whole id shown beside it.
        CHECK(entry.distinct != entry.id);
        CHECK(entry.id.ends_with(entry.distinct));
        CHECK_FALSE(entry.distinct.starts_with('_'));
        const auto& [first, last] = std::ranges::minmax(twins);
        const auto common = std::ranges::mismatch(first, last).in1 - first.begin();
        CHECK(entry.id.size() - entry.distinct.size() >= static_cast<std::size_t>(common));
    }
    CHECK(shared > 0);
}

TEST_CASE("the strings of a JSON text are read past escaped quotes", "[editor_catalog]") {
    const auto text = std::string(R"({ "name": "a \"quoted\" word", "id": "t_floor", "x": "\\" })");
    CHECK(editor::quoted_strings(text)
          == std::vector<
              std::string_view>{"name", R"(a \"quoted\" word)", "id", "t_floor", "x", R"(\\)"});
}
