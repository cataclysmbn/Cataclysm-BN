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

TEST_CASE("wish item categories are unique and valid", "[wish][ui]") {
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(
        item_controller->all());

    REQUIRE(!categories.empty());

    std::set<std::string> seen;
    for (const item_category_id& category : categories) {
        CAPTURE(category.str());
        CHECK(category.is_valid());
        CHECK(seen.insert(category.str()).second);
    }
}

TEST_CASE("wish item categories are ordered like the item category list", "[wish][ui]") {
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(
        item_controller->all());

    REQUIRE(categories.size() > 1);

    for (size_t i = 1; i < categories.size(); i++) {
        CAPTURE(categories[i - 1].str(), categories[i].str());
        CHECK_FALSE(*categories[i] < *categories[i - 1]);
    }
}

TEST_CASE("wish item categories cover every item type", "[wish][ui]") {
    const std::vector<item_category_id> categories = debug_menu::wish_item_categories(
        item_controller->all());

    std::set<std::string> pages;
    for (const item_category_id& category : categories) { pages.insert(category.str()); }

    for (const itype* type : item_controller->all()) {
        if (type->category_force.is_empty() || !type->category_force.is_valid()) { continue; }
        CAPTURE(type->get_id().str(), type->category_force.str());
        CHECK(pages.count(type->category_force.str()) == 1);
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
        CAPTURE(items[i]->get_id().str(), category.str(), pages[i]);
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
