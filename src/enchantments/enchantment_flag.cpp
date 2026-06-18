#include "enchantment_flag.h"

#include "assign.h"
#include "debug.h"
#include "generic_factory.h"
#include "type_id_implement.h"

#include <optional>

namespace {
generic_factory<enchantment_flag> all_enchantment_flags("Enchantment flags");
}

IMPLEMENT_STRING_AND_INT_IDS(enchantment_flag, all_enchantment_flags);

void enchantment_flag::load_enchantment_flags(const JsonObject& jo, const std::string& src) {
    all_enchantment_flags.load(jo, src);
}

void enchantment_flag::load(const JsonObject& jo, const std::string& src) {
    optional(jo, was_loaded, "conflicts", conflicts);
}

void enchantment_flag::check() const {
    for (const auto& ench_flag : conflicts) {
        if (!ench_flag.is_valid()) {
            debugmsg("Enchantment flag %s has invalid enchantment flag conflict %s.", id.str(),
                     ench_flag.str());
        }
    }
}

void enchantment_flag::check_consistency() { all_enchantment_flags.check(); }


std::vector<enchantment_flag> enchantment_flag::get_all() {
    return all_enchantment_flags.get_all();
}

void enchantment_flag::reset() { all_enchantment_flags.reset(); }
