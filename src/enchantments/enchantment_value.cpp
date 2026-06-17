#include "enchantment_value.h"

#include "assign.h"
#include "debug.h"
#include "generic_factory.h"

namespace {
generic_factory<enchantment_value> all_enchantment_values("Enchantment Values");
}

/** @relates string_id */
template <> const enchantment_value& string_id<enchantment_value>::obj() const {
    return all_enchantment_values.obj(*this);
}

/** @relates int_id */
template <> const enchantment_value& int_id<enchantment_value>::obj() const {
    return all_enchantment_values.obj(*this);
}

/** @relates string_id */
template <> bool string_id<enchantment_value>::is_valid() const {
    return all_enchantment_values.is_valid(*this);
}

/** @relates int_id */
template <> bool int_id<enchantment_value>::is_valid() const {
    return all_enchantment_values.is_valid(*this);
}

/** @relates string_id */
template <> int_id<enchantment_value> string_id<enchantment_value>::id() const {
    return all_enchantment_values.convert(*this, int_id<enchantment_value>(INVALID_CID));
}

/** @relates int_id */
template <> const string_id<enchantment_value>& int_id<enchantment_value>::id() const {
    return all_enchantment_values.convert(*this);
}

/** @relates int_id */
template <> int_id<enchantment_value>::int_id(const enchantment_value_id& id) {
    *this = all_enchantment_values.convert(id, int_id<enchantment_value>(INVALID_CID));
}

void enchantment_value::load_enchantment_values(const JsonObject& jo, const std::string& src) {
    all_enchantment_values.load(jo, src);
}

void enchantment_value::load(const JsonObject& jo, const std::string& src) {
    mandatory(jo, was_loaded, "can_add", can_add);
    mandatory(jo, was_loaded, "can_mult", can_mult);
    if (jo.has_array("suffixes")) {
        for (std::string& suffix : jo.get_string_array("suffixes")) {
            enchantment_value suffixed = enchantment_value(*this);
            suffixed.id = enchantment_value_id(suffixed.id.str() + "_" + suffix);
            suffixed.parent_id = id;
            all_enchantment_values.insert(suffixed);
        }
    }
}

void enchantment_value::check() const {
    // Currently a placeholder, needed for generic factories
    // This will be important if anything more complex then two bools and non-c++ definitions
    // Gets added to enchantment values
    return;
}

void enchantment_value::check_consistency() { all_enchantment_values.check(); }

bool enchantment_value::has_parent() const { return parent_id != enchantment_value_id::NULL_ID(); }

enchantment_value_id enchantment_value::get_parent() const { return parent_id; }

std::vector<enchantment_value> enchantment_value::get_all() {
    return all_enchantment_values.get_all();
}

void enchantment_value::reset() { all_enchantment_values.reset(); }
