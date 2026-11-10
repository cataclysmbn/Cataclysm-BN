#include "construction_group.h"

#include "generic_factory.h"
#include "json.h"
#include "type_id_implement.h"

namespace {

generic_factory<construction_group> all_construction_groups("construction groups");

} // namespace

IMPLEMENT_STRING_AND_INT_IDS(construction_group, all_construction_groups)

void construction_group::load(const JsonObject& jo, const std::string&) {
    mandatory(jo, was_loaded, "name", _name);
}

std::string construction_group::name() const { return _name.translated(); }

size_t construction_group::count() { return all_construction_groups.size(); }

void construction_groups::load(const JsonObject& jo, const std::string& src) {
    all_construction_groups.load(jo, src);
}

void construction_groups::reset() { all_construction_groups.reset(); }

const std::vector<construction_group>& construction_groups::get_all() {
    return all_construction_groups.get_all();
}
