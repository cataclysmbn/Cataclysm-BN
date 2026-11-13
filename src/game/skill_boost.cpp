#include "skill_boost.h"

#include "generic_factory.h"
#include "json.h"
#include "type_id_implement.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace {
generic_factory<skill_boost> all_skill_boosts("skill boost", "stat");
} // namespace

IMPLEMENT_STRING_AND_INT_IDS(skill_boost, all_skill_boosts);

auto skill_boost::get_all() -> const std::vector<skill_boost>& {
    return all_skill_boosts.get_all();
}

auto skill_boost::get(const std::string& stat_str) -> std::optional<skill_boost> {
    for (const skill_boost& boost : get_all()) {
        if (boost.stat() == stat_str) { return std::optional<skill_boost>(boost); }
    }
    return std::nullopt;
}

void skill_boost::load_boost(const JsonObject& jo, const std::string& src) {
    all_skill_boosts.load(jo, src);
}

void skill_boost::load(const JsonObject& jo, const std::string&) {
    mandatory(jo, was_loaded, "skills", _skills);
    mandatory(jo, was_loaded, "skill_offset", _offset);
    mandatory(jo, was_loaded, "scaling_power", _power);
}

void skill_boost::reset() { all_skill_boosts.reset(); }

auto skill_boost::stat() const -> std::string { return id.str(); }

auto skill_boost::skills() const -> const std::vector<std::string>& { return _skills; }

auto skill_boost::calc_bonus(int skill_total) const -> float {
    if (skill_total + _offset <= 0) { return 0.0; }
    return std::max(0.0, std::floor(std::pow(skill_total + _offset, _power)));
}
