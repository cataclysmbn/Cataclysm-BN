#include "flag_trait.h"

#include "debug.h"
#include "generic_factory.h"
#include "json.h"
#include "type_id.h"
#include "type_id_implement.h"

#include <unordered_map>

namespace {
generic_factory<json_trait_flag> json_trait_flags_all("json_trait_flags");
} // namespace

IMPLEMENT_STRING_AND_INT_IDS(json_trait_flag, json_trait_flags_all);

json_trait_flag::operator bool() const { return id.is_valid(); }

auto json_trait_flag::get(const std::string& id) -> const json_trait_flag& {
    static const json_trait_flag null_value = json_trait_flag();
    const trait_flag_str_id f_id(id);
    return f_id.is_valid() ? *f_id : null_value;
}

void json_trait_flag::load(const JsonObject&, const std::string&) {}

void json_trait_flag::check_consistency() { json_trait_flags_all.check(); }

void json_trait_flag::reset() { json_trait_flags_all.reset(); }

void json_trait_flag::load_all(const JsonObject& jo, const std::string& src) {
    json_trait_flags_all.load(jo, src);
}

void json_trait_flag::check() const {
    for (const auto& conflicting : conflicts_) {
        if (!trait_flag_str_id(conflicting).is_valid()) {
            debugmsg("trait flag definition %s specifies unknown conflicting field %s", id.str(),
                     conflicting);
        }
    }
}

void json_trait_flag::finalize_all() { json_trait_flags_all.finalize(); }

auto json_trait_flag::is_ready() -> bool { return !json_trait_flags_all.empty(); }

auto json_trait_flag::get_all() -> const std::vector<json_trait_flag>& {
    return json_trait_flags_all.get_all();
}
