#include "construction_category.h"

#include "generic_factory.h"
#include "int_id.h"
#include "json.h"
#include "string_id.h"
#include "type_id_implement.h"

#include <set>
#include <string>

namespace {

generic_factory<construction_category> all_construction_categories("construction categories");

} // namespace

IMPLEMENT_STRING_AND_INT_IDS(construction_category, all_construction_categories)

void construction_category::load(const JsonObject& jo, const std::string&) {
    mandatory(jo, was_loaded, "name", _name);
}

size_t construction_category::count() { return all_construction_categories.size(); }

void construction_categories::load(const JsonObject& jo, const std::string& src) {
    all_construction_categories.load(jo, src);
}

void construction_categories::reset() { all_construction_categories.reset(); }

const std::vector<construction_category>& construction_categories::get_all() {
    return all_construction_categories.get_all();
}
