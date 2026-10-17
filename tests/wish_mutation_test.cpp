#include "catacharset.h"
#include "catch/catch.hpp"
#include "debug_menu.h"
#include "mutation.h"
#include "translations.h"
#include "type_id.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string cat_str(const mutation_category_id& category) { return category.str(); }

// The mutation type and the id prefixes that the menu groups into the NPC page.
const std::string npc_type = "BACKGROUND_SURVIVAL_STORY";
const std::array<std::string_view, 3> npc_prefixes = {"NPC_", "BGSS_", "Exp_"};

bool is_npc_mutation(const trait_id& mutation) {
    for (const std::string_view prefix : npc_prefixes) {
        if (mutation.str().starts_with(prefix)) { return true; }
    }
    return mutation->types.contains(npc_type);
}

// The mutations the menu groups into the debug page: the ones the data marks as
// debugging tools and the ones named DEBUG_*.
const std::string debug_prefix = "DEBUG_";

bool is_debug_mutation(const mutation_branch& mutation) {
    return mutation.debug || mutation.id.str().starts_with(debug_prefix);
}

// The categories a mutation is expected to be listed under: the ones it names plus
// the ones it is the threshold mutation of.
std::set<std::string> expected_categories(const mutation_branch& branch) {
    std::set<std::string> ret;
    for (const mutation_category_id& category : branch.category) { ret.insert(category.str()); }
    for (const auto& [category_id, category] : mutation_category_trait::get_all()) {
        if (std::ranges::find(category.threshold_muts, branch.id)
            != category.threshold_muts.end()) {
            ret.insert(category_id.str());
        }
    }
    return ret;
}

// The mutation type pages a mutation is expected to be listed on.  The NPC
// background stories have the NPC page instead of a page of their own.
std::set<std::string> expected_types(const mutation_branch& branch) {
    std::set<std::string> ret = branch.types;
    ret.erase(npc_type);
    return ret;
}

// The index of a mutation in the order of the loaded mutations.
size_t mutation_index(const std::vector<mutation_branch>& mutations, const trait_id& id) {
    for (size_t i = 0; i < mutations.size(); i++) {
        if (mutations[i].id == id) { return i; }
    }
    return mutations.size();
}

size_t page_of(const std::vector<std::string>& ids, const std::string& id, size_t first) {
    const auto found = std::ranges::find(ids, id);
    REQUIRE(found != ids.end());
    return first + static_cast<size_t>(found - ids.begin());
}

size_t page_of(
    const std::vector<mutation_category_id>& ids, const mutation_category_id& id, size_t first) {
    const auto found = std::ranges::find(ids, id);
    REQUIRE(found != ids.end());
    return first + static_cast<size_t>(found - ids.begin());
}

bool listed_on(const std::vector<size_t>& pages, size_t page) {
    return std::ranges::find(pages, page) != pages.end();
}

} // namespace

TEST_CASE("wish mutation pages of the loaded mutation data", "[wish][ui][mutate]") {
    const std::vector<mutation_branch>& mutations = mutation_branch::get_all();
    const debug_menu::wish_mutation_group_list groups = debug_menu::wish_mutation_groups();
    REQUIRE(groups.size() == mutations.size());

    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);
    REQUIRE(pages.size() == groups.size());

    // The pages are the unfiltered one, the generic one, the mutation types, the NPC
    // page, the categories and the debug page last.
    const size_t npc_page = 2 + types.size();
    const size_t first_category_page = npc_page + 1;
    const size_t debug_page = first_category_page + categories.size();
    size_t generic = 0;
    size_t filtered = 0;

    for (size_t i = 0; i < mutations.size(); i++) {
        const mutation_branch& branch = mutations[i];
        const debug_menu::wish_mutation_group& group = groups[i];
        CAPTURE(branch.id.str());

        std::set<std::string> got_categories;
        for (const mutation_category_id& category : group.categories) {
            got_categories.insert(category.str());
        }
        CHECK(got_categories == expected_categories(branch));

        const std::set<std::string> got_types(group.types.begin(), group.types.end());
        CHECK(got_types == expected_types(branch));
        CHECK(group.npc == is_npc_mutation(branch.id));
        CHECK(group.debug == is_debug_mutation(branch));

        // Every mutation is on the unfiltered page, and the mutations that have no
        // page of their own are on the generic page as well.
        REQUIRE(!pages[i].empty());
        CHECK(pages[i][0] == 0);
        const bool no_page_of_its_own =
            group.types.empty() && !group.npc && !group.debug && group.categories.empty();
        CHECK(listed_on(pages[i], 1) == no_page_of_its_own);
        if (no_page_of_its_own) {
            generic++;
        } else {
            filtered++;
        }

        for (const std::string& type : group.types) {
            CHECK(listed_on(pages[i], page_of(types, type, 2)));
        }
        if (group.npc) { CHECK(listed_on(pages[i], npc_page)); }
        for (const mutation_category_id& category : group.categories) {
            CHECK(listed_on(pages[i], page_of(categories, category, first_category_page)));
        }
        if (group.debug) { CHECK(listed_on(pages[i], debug_page)); }
        CHECK(std::ranges::is_sorted(pages[i]));
    }

    // Both the generic page and the pages with a filter have to be worth showing.
    CHECK(generic > 0);
    CHECK(filtered > 0);
    INFO("mutations with no page of their own: " << generic);
}

TEST_CASE("debug mutations are listed on the debug page", "[wish][ui][mutate]") {
    const std::vector<mutation_branch>& mutations = mutation_branch::get_all();
    const debug_menu::wish_mutation_group_list groups = debug_menu::wish_mutation_groups();
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);

    // The debug page is the last page of the tab bar, after the categories.
    const size_t debug_page = 3 + types.size() + categories.size();
    size_t debug = 0;
    for (size_t i = 0; i < mutations.size(); i++) {
        const mutation_branch& branch = mutations[i];
        if (!is_debug_mutation(branch)) {
            CHECK(!listed_on(pages[i], debug_page));
            continue;
        }
        debug++;
        CAPTURE(branch.id.str());
        CHECK(listed_on(pages[i], debug_page));
        // A debugging mutation has a page of its own, so it is not on the generic page.
        CHECK(!listed_on(pages[i], 1));
        CHECK(pages[i].back() == debug_page);
    }
    // The core data has its DEBUG_* mutations, and mods may add more.
    CHECK(debug > 0);
    INFO("mutations on the debug page: " << debug);
}

TEST_CASE("wish mutation page ids of the loaded mutation data", "[wish][ui][mutate]") {
    const std::vector<mutation_branch>& mutations = mutation_branch::get_all();
    const debug_menu::wish_mutation_group_list groups = debug_menu::wish_mutation_groups();
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);

    std::vector<std::string> category_ids;
    category_ids.reserve(categories.size());
    for (const mutation_category_id& category : categories) {
        category_ids.push_back(category.str());
    }
    CHECK(std::ranges::is_sorted(category_ids));
    CHECK(std::adjacent_find(category_ids.begin(), category_ids.end()) == category_ids.end());
    CHECK(std::ranges::is_sorted(types));
    CHECK(std::adjacent_find(types.begin(), types.end()) == types.end());

    // The data groups mutations under MARLOSS without defining that category, so it
    // still has to have a page.
    const size_t marloss = mutation_index(mutations, trait_id("MARLOSS"));
    REQUIRE(marloss < mutations.size());
    CHECK(std::ranges::find(categories, mutation_category_id("MARLOSS")) != categories.end());
    CHECK(!groups[marloss].categories.empty());

    // The mutation types are the pages between the generic page and the NPC page, and
    // the NPC background stories are not one of them.
    CHECK(std::ranges::find(types, "hair_color") != types.end());
    CHECK(std::ranges::find(types, "SKIN") != types.end());
    CHECK(std::ranges::find(types, npc_type) == types.end());
}

TEST_CASE("a threshold mutation is listed under its category", "[wish][ui][mutate]") {
    const std::vector<mutation_branch>& mutations = mutation_branch::get_all();
    const debug_menu::wish_mutation_group_list groups = debug_menu::wish_mutation_groups();
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);

    const trait_id threshold("THRESH_PLANT");
    const size_t index = mutation_index(mutations, threshold);
    REQUIRE(index < mutations.size());
    // The threshold mutation names no category of its own.
    REQUIRE(mutations[index].category.empty());

    const size_t plant_page = page_of(categories, mutation_category_id("PLANT"), 3 + types.size());
    CHECK(listed_on(pages[index], plant_page));
    // It is listed under its category, so it is not repeated on the generic page.
    CHECK(!listed_on(pages[index], 1));
}

TEST_CASE("NPC mutations and mutation types are not on the generic page", "[wish][ui][mutate]") {
    const std::vector<mutation_branch>& mutations = mutation_branch::get_all();
    const debug_menu::wish_mutation_group_list groups = debug_menu::wish_mutation_groups();
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);
    const size_t npc_page = 2 + types.size();

    size_t npc = 0;
    size_t typed = 0;
    for (size_t i = 0; i < mutations.size(); i++) {
        if (groups[i].npc) {
            npc++;
            CAPTURE(mutations[i].id.str());
            CHECK(listed_on(pages[i], npc_page));
            CHECK(!listed_on(pages[i], 1));
        }
        if (!groups[i].types.empty()) {
            typed++;
            CHECK(!listed_on(pages[i], 1));
        }
    }
    // The NPC traits, the NPC background stories and the NPC experience traits are
    // all on the NPC page, and the mutation types have pages of their own.
    CHECK(npc > 100);
    CHECK(typed > 0);
}

TEST_CASE("wish mutation pages of a mutation with several categories", "[wish][ui][mutate]") {
    debug_menu::wish_mutation_group_list groups(2);
    groups[0].categories = {mutation_category_id("PLANT"), mutation_category_id("INSECT")};
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    REQUIRE(categories.size() == 2);
    CHECK(cat_str(categories[0]) == "INSECT");
    CHECK(cat_str(categories[1]) == "PLANT");
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    CHECK(types.empty());

    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);
    REQUIRE(pages.size() == 2);
    // The pages are ALL, GENERIC, NPC and then the categories, so the mutation with
    // two categories is on the unfiltered page and on both of them and the mutation
    // without any is on the unfiltered and on the generic page.
    CHECK(pages[0] == std::vector<size_t>{0, 3, 4});
    CHECK(pages[1] == std::vector<size_t>{0, 1});
}

TEST_CASE(
    "wish mutation pages of mutations with a type and of an NPC mutation", "[wish][ui][mutate]") {
    debug_menu::wish_mutation_group_list groups(3);
    groups[0].types = {"hair_color"};
    groups[1].types = {"SKIN", "hair_color"};
    groups[2].npc = true;
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    CHECK(categories.empty());
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    REQUIRE(types.size() == 2);
    CHECK(types[0] == "SKIN");
    CHECK(types[1] == "hair_color");

    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);
    REQUIRE(pages.size() == 3);
    // The pages are ALL, GENERIC, SKIN, hair_color and NPC; the mutation in both
    // types is on both of their pages, and the generic page holds none of them.
    CHECK(pages[0] == std::vector<size_t>{0, 3});
    CHECK(pages[1] == std::vector<size_t>{0, 2, 3});
    CHECK(pages[2] == std::vector<size_t>{0, 4});
}

TEST_CASE("the debug page comes after the categories", "[wish][ui][mutate]") {
    debug_menu::wish_mutation_group_list groups(2);
    groups[0].categories = {mutation_category_id("PLANT")};
    groups[1].debug = true;
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    REQUIRE(categories.size() == 1);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    CHECK(types.empty());

    const debug_menu::wish_mutation_page_list pages =
        debug_menu::wish_mutation_pages(groups, categories, types);
    REQUIRE(pages.size() == 2);
    // The pages are ALL, GENERIC, NPC, PLANT and the debug page, which is the last
    // one and holds no other mutation.
    CHECK(pages[0] == std::vector<size_t>{0, 3});
    CHECK(pages[1] == std::vector<size_t>{0, 4});
}

TEST_CASE("wish mutation page labels of data without a definition", "[wish][ui][mutate]") {
    const mutation_category_id plant("PLANT");
    REQUIRE(mutation_category_is_valid(plant));
    CHECK(debug_menu::wish_mutation_category_label(plant)
          == mutation_category_trait::get_category(plant).name());

    // The data can group mutations under a category that it never defines, such as
    // MARLOSS.  get_category() trusts the id it is given, so such a page is labelled
    // with the id, which is what the menu listed as a filter before the tab bar was
    // added, and which the translation can give a name to.
    const mutation_category_id undefined("DSH_UNDEFINED_CATEGORY");
    REQUIRE(!mutation_category_is_valid(undefined));
    CHECK(debug_menu::wish_mutation_category_label(undefined)
          == to_translation("DSH_UNDEFINED_CATEGORY").translated());

    // Mutation types have no name of their own: the game shows the id for the types
    // it has no display name for, and that id is translated when there is an entry
    // for it.
    CHECK(debug_menu::wish_mutation_type_label("hair_color")
          == to_translation("hair color").translated());
    CHECK(debug_menu::wish_mutation_type_label("DSH_UNDEFINED_TYPE")
          == to_translation("DSH_UNDEFINED_TYPE").translated());
}

TEST_CASE("wish mutation tab bar keeps the current page visible", "[wish][ui][mutate]") {
    const debug_menu::wish_mutation_group_list groups = debug_menu::wish_mutation_groups();
    const std::vector<mutation_category_id> categories = debug_menu::wish_mutation_categories(
        groups);
    const std::vector<std::string> types = debug_menu::wish_mutation_types(groups);
    // The labels of the tab bar, in the order the menu puts them in.
    std::vector<std::string> tabs = {"ALL", "GENERIC"};
    for (const std::string& type : types) {
        tabs.push_back(debug_menu::wish_mutation_type_label(type));
    }
    tabs.emplace_back("NPC");
    for (const mutation_category_id& category : categories) {
        tabs.push_back(debug_menu::wish_mutation_category_label(category));
    }
    tabs.emplace_back("DEBUG");

    REQUIRE(tabs.size() > 2);

    // The tab bar shares the menu with the mutation info panel, so it is only about
    // half of the screen wide, which is not enough room for all of the labels.
    for (const int width : {20, 47}) {
        for (size_t current = 0; current < tabs.size(); current++) {
            const auto range = debug_menu::wish_mutation_tab_range(tabs, current, width);
            CAPTURE(width, current, range.first, range.last);
            // The current page is the one that must never be scrolled out of the bar.
            CHECK(range.first <= current);
            CHECK(range.last >= current);
            CHECK(range.last < tabs.size());
            if (current == 0) { CHECK(range.first == 0); }
            // Everything in the range fits into the bar, where the 2 columns are the
            // offset draw_tabs() gives the first label.  A single label wider than
            // the whole bar cannot be shown at all; the menu's own "<< category >>"
            // line below the bar still tells which page is the current one.
            auto used = 2;
            for (size_t i = range.first; i <= range.last; i++) { used += utf8_width(tabs[i]) + 3; }
            if (range.first != range.last) { CHECK(used <= width); }
        }
    }
}
