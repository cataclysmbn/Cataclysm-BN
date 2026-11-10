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

auto construction_group::name() const -> std::string { return _name.translated(); }

auto construction_group::count() -> size_t { return all_construction_groups.size(); }

void construction_groups::load(const JsonObject& jo, const std::string& src) {
    all_construction_groups.load(jo, src);
}

void construction_groups::reset() { all_construction_groups.reset(); }

auto construction_groups::get_all() -> const std::vector<construction_group>& {
    return all_construction_groups.get_all();
}
