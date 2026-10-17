#include "catacharset.h"
#include "catch/catch.hpp"
#include "debug_menu.h"
#include "item_category.h"
#include "item_factory.h"
#include "itype.h"
#include "type_id.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

// The spawn item menu puts every spawnable item type on one page per item category.
// The pages come from debug_menu::wish_item_categories, which has to list each category
// that some item type actually uses, exactly once, or items end up without a page of
// their own.
namespace {

std::string cat_str(const item_category_id& category) { return category.str(); }

} // namespace

TEST_CASE("wish item categories are unique and valid", "[wish][ui]") {
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(
        item_controller->all());

    REQUIRE(!categories.empty());

    std::set<std::string> seen;
    for (const item_category_id& category : categories) {
        CAPTURE(cat_str(category));
        CHECK(category.is_valid());
        CHECK(seen.insert(cat_str(category)).second);
    }
}

TEST_CASE("wish item categories are ordered like the item category list", "[wish][ui]") {
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(
        item_controller->all());

    REQUIRE(categories.size() > 1);

    for (size_t i = 1; i < categories.size(); i++) {
        CAPTURE(cat_str(categories[i - 1]), cat_str(categories[i]));
        CHECK_FALSE(*categories[i] < *categories[i - 1]);
    }
}

TEST_CASE("wish item categories cover every item type", "[wish][ui]") {
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(
        item_controller->all());

    std::set<std::string> pages;
    for (const item_category_id& category : categories) { pages.insert(cat_str(category)); }

    for (const itype* type : item_controller->all()) {
        if (type->category_force.is_empty() || !type->category_force.is_valid()) { continue; }
        CAPTURE(type->get_id().str(), cat_str(type->category_force));
        CHECK(pages.count(cat_str(type->category_force)) == 1);
    }

    // Categories the item factory assigns on its own, see calc_category().
    for (const std::string& id :
         {"guns", "magazines", "ammo", "mods", "tools", "weapons", "food"}) {
        CAPTURE(id);
        CHECK(pages.count(id) == 1);
    }
}

TEST_CASE("wish item pages match the category of every item type", "[wish][ui]") {
    const std::vector<const itype*> items = item_controller->all();
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(items);
    const std::vector<std::size_t> pages = debug_menu::wish_item_pages(items, categories);

    REQUIRE(categories.size() > 1);
    REQUIRE(pages.size() == items.size());

    size_t uncategorized = 0;
    for (size_t i = 0; i < items.size(); i++) {
        const item_category_id& category = items[i]->category_force;
        CAPTURE(items[i]->get_id().str(), cat_str(category), pages[i]);
        // Each page is either the unfiltered first one or one of the categories.
        CHECK(pages[i] <= categories.size());
        if (category.is_empty() || !category.is_valid()) {
            // Item types without a category only show up in the first page.
            CHECK(pages[i] == 0);
            uncategorized++;
        } else {
            // Items of a category show up in the page of that category.
            REQUIRE(pages[i] > 0);
            CHECK(categories[pages[i] - 1] == category);
        }
    }
    INFO("item types without a category: " << uncategorized);
}

TEST_CASE("wish item tab bar keeps the current page visible", "[wish][ui]") {
    const std::vector<const itype*> items = item_controller->all();
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(items);
    std::vector<std::string> tabs;
    tabs.reserve(categories.size() + 1);
    tabs.emplace_back("ALL");
    for (const item_category_id& category : categories) { tabs.emplace_back(category->name()); }

    REQUIRE(tabs.size() > 1);

    // The tab bar shares the menu with the item info panel, so it is only about
    // half of the screen wide, which is not enough room for all of the labels.
    for (const int width : {20, 47}) {
        for (size_t current = 0; current < tabs.size(); current++) {
            const auto range = debug_menu::wish_item_tab_range(tabs, current, width);
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
