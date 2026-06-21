#include "enchantment.h"

#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "creature.h"
#include "debug.h"
#include "enchantment_value.h"
#include "enum_conversions.h"
#include "enums.h"
#include "generic_factory.h"
#include "json.h"
#include "map.h"
#include "mutation.h"
#include "point.h"
#include "rng.h"
#include "string_id.h"
#include "type_id.h"
#include "units.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>

template <typename E> struct enum_traits;

template <> struct enum_traits<enchantment::has> {
    static constexpr enchantment::has last = enchantment::has::NUM_HAS;
};

template <> struct enum_traits<enchantment::condition> {
    static constexpr enchantment::condition last = enchantment::condition::NUM_CONDITION;
};

namespace io {
// TODO: Migrate these enums to ids too
// *INDENT-OFF*
template <> std::string enum_to_string<enchantment::has>(enchantment::has data) {
    switch (data) {
        case enchantment::has::HELD:
            return "HELD";
        case enchantment::has::WIELD:
            return "WIELD";
        case enchantment::has::WORN:
            return "WORN";
        case enchantment::has::NUM_HAS:
            break;
    }
    debugmsg("Invalid enchantment::has");
    abort();
}

template <> std::string enum_to_string<enchantment::condition>(enchantment::condition data) {
    switch (data) {
        case enchantment::condition::ALWAYS:
            return "ALWAYS";
        case enchantment::condition::UNDERGROUND:
            return "UNDERGROUND";
        case enchantment::condition::ABOVEGROUND:
            return "ABOVEGROUND";
        case enchantment::condition::UNDERWATER:
            return "UNDERWATER";
        case enchantment::condition::DAY:
            return "DAY";
        case enchantment::condition::NIGHT:
            return "NIGHT";
        case enchantment::condition::DUSK:
            return "DUSK";
        case enchantment::condition::DAWN:
            return "DAWN";
        case enchantment::condition::ACTIVE:
            return "ACTIVE";
        case enchantment::condition::INACTIVE:
            return "INACTIVE";
        case enchantment::condition::NUM_CONDITION:
            break;
    }
    debugmsg("Invalid enchantment::condition");
    abort();
}

// *INDENT-ON*
} // namespace io

namespace {
generic_factory<enchantment> enchant_factory("enchantment");
} // namespace

template <> const enchantment& string_id<enchantment>::obj() const {
    return enchant_factory.obj(*this);
}

template <> bool string_id<enchantment>::is_valid() const {
    return enchant_factory.is_valid(*this);
}

void enchantment::load_enchantment(const JsonObject& jo, const std::string& src) {
    enchant_factory.load(jo, src);
}

void enchantment::reset() { enchant_factory.reset(); }

bool enchantment::is_active(const Character& guy, const item& parent) const {
    if (!guy.has_item(parent)) { return false; }

    if (active_conditions.first == has::WIELD && !guy.is_wielding(parent)) { return false; }

    if (active_conditions.first == has::WORN && !guy.is_worn(parent)) { return false; }

    return is_active(guy, parent.is_active());
}

bool enchantment::is_active(const Character& guy, const bool active) const {
    if (active_conditions.second == condition::ACTIVE) { return active; }

    if (active_conditions.second == condition::INACTIVE) { return !active; }

    if (active_conditions.second == condition::ALWAYS) { return true; }

    if (active_conditions.second == condition::NIGHT) { return is_night(calendar::turn); }

    if (active_conditions.second == condition::DAY) { return is_day(calendar::turn); }

    if (active_conditions.second == condition::DUSK) { return is_dusk(calendar::turn); }

    if (active_conditions.second == condition::DAWN) { return is_dawn(calendar::turn); }

    if (active_conditions.second == condition::UNDERGROUND) { return guy.bub_pos().z() < 0; }

    if (active_conditions.second == condition::ABOVEGROUND) { return guy.bub_pos().z() > -1; }

    if (active_conditions.second == condition::UNDERWATER) {
        return get_map().is_divable(guy.bub_pos());
    }
    return false;
}

void enchantment::add_activation(const time_duration& freq, const fake_spell& fake) {
    intermittent_activation[freq].emplace_back(fake);
}

void enchantment::load(const JsonObject& jo, const std::string&) {
    optional(jo, was_loaded, "id", id, enchantment_id(""));

    jo.read("hit_you_effect", hit_you_effect);
    jo.read("hit_me_effect", hit_me_effect);
    jo.read("emitter", emitter);

    if (jo.has_object("intermittent_activation")) {
        JsonObject jobj = jo.get_object("intermittent_activation");
        for (const JsonObject effect_obj : jobj.get_array("effects")) {
            time_duration freq = read_from_json_string<
                time_duration>(*effect_obj.get_raw("frequency"), time_duration::units);
            if (effect_obj.has_array("spell_effects")) {
                for (const JsonObject fake_spell_obj : effect_obj.get_array("spell_effects")) {
                    fake_spell fake;
                    fake.load(fake_spell_obj);
                    add_activation(freq, fake);
                }
            } else if (effect_obj.has_object("spell_effects")) {
                fake_spell fake;
                JsonObject fake_spell_obj = effect_obj.get_object("spell_effects");
                fake.load(fake_spell_obj);
                add_activation(freq, fake);
            }
        }
    }

    active_conditions.first = io::string_to_enum<has>(jo.get_string("has", "HELD"));
    active_conditions.second = io::string_to_enum<condition>(jo.get_string("condition", "ALWAYS"));

    for (JsonObject jsobj : jo.get_array("ench_effects")) {
        ench_effects.emplace(efftype_id(jsobj.get_string("effect")), jsobj.get_int("intensity"));
    }

    optional(jo, was_loaded, "mutations", mutations);

    if (jo.has_array("values")) {
        for (const JsonObject value_obj : jo.get_array("values")) {
            enchantment_value_id value = enchantment_value_id(value_obj.get_string("value"));
            const int add = value_obj.get_int("add", 0);
            const double mult = value_obj.get_float("multiply", 0.0);
            if (add != 0) { values_add.emplace(value, add); }
            if (mult != 0.0) {
                // Limit precision to minimize inconsistencies between platforms / compilers
                const double mul = static_cast<int>(std::round(mult * 100'000)) / 100'000.0;
                values_multiply.emplace(value, mul);
            }
        }
    }
}

void enchantment::serialize(JsonOut& jsout) const {
    jsout.start_object();

    if (!id.is_empty()) {
        jsout.member("id", id);
        jsout.end_object();
        // if the enchantment has an id then it is defined elsewhere and does not need to be
        // serialized.
        return;
    }

    jsout.member("has", io::enum_to_string<has>(active_conditions.first));
    jsout.member("condition", io::enum_to_string<condition>(active_conditions.second));
    if (emitter) { jsout.member("emitter", emitter); }

    if (!hit_you_effect.empty()) { jsout.member("hit_you_effect", hit_you_effect); }

    if (!hit_me_effect.empty()) { jsout.member("hit_me_effect", hit_me_effect); }

    if (!intermittent_activation.empty()) {
        jsout.member("intermittent_activation");
        jsout.start_object();
        jsout.member("effects");
        jsout.start_array();
        for (const std::pair<time_duration, std::vector<fake_spell>> pair :
             intermittent_activation) {
            jsout.start_object();
            jsout.member("frequency");
            dump_to_json_string<time_duration>(pair.first, jsout, time_duration::units);
            jsout.member("spell_effects");
            jsout.start_array();
            for (const fake_spell& sp : pair.second) { sp.serialize(jsout); }
            jsout.end_array();
            jsout.end_object();
        }
        jsout.end_array();
        jsout.end_object();
    }

    if (!ench_effects.empty()) {
        jsout.member("ench_effects");
        jsout.start_array();
        for (const std::pair<const efftype_id, int>& eff : ench_effects) {
            jsout.start_object();
            jsout.member("effect", eff.first);
            jsout.member("intensity", eff.second);
            jsout.end_object();
        }
        jsout.end_array();
    }

    jsout.member("mutations", mutations);

    jsout.member("values");
    jsout.start_array();
    for (enchantment_value ench_val : enchantment_value::get_all()) {
        enchantment_value_id ench_val_id = ench_val.id;
        auto val_add = get_value_add(ench_val_id);
        auto val_mult = get_value_multiply(ench_val_id);
        if (val_add == 0 && val_mult == 0.0) { continue; }
        jsout.start_object();
        jsout.member("value", ench_val_id.str());
        if (val_add != 0) { jsout.member("add", val_add); }
        if (val_mult != 0) { jsout.member("multiply", val_mult); }
        jsout.end_object();
    }
    jsout.end_array();

    jsout.end_object();
}

bool enchantment::stacks_with(const enchantment& rhs) const {
    return active_conditions == rhs.active_conditions;
}

bool enchantment::add(const enchantment& rhs) {
    if (!stacks_with(rhs)) { return false; }
    force_add(rhs);
    return true;
}

void enchantment::force_add(const enchantment& rhs) {
    for (const auto& pair_values : rhs.values_add) {
        values_add[pair_values.first] += pair_values.second;
    }
    for (const auto& pair_values : rhs.values_multiply) {
        // values do not multiply against each other, they add.
        // so +10% and -10% will add to 0%
        values_multiply[pair_values.first] += pair_values.second;
    }

    hit_me_effect.insert(hit_me_effect.end(), rhs.hit_me_effect.begin(), rhs.hit_me_effect.end());

    hit_you_effect
        .insert(hit_you_effect.end(), rhs.hit_you_effect.begin(), rhs.hit_you_effect.end());

    ench_effects.insert(rhs.ench_effects.begin(), rhs.ench_effects.end());

    if (rhs.emitter) { emitter = rhs.emitter; }

    for (const trait_id& branch : rhs.mutations) { mutations.emplace(branch); }

    for (const std::pair<const time_duration, std::vector<fake_spell>>& act_pair :
         rhs.intermittent_activation) {
        for (const fake_spell& fake : act_pair.second) {
            intermittent_activation[act_pair.first].emplace_back(fake);
        }
    }
}

int enchantment::get_value_add(const enchantment_value_id value) const {
    if (!value.is_valid()) { debugmsg("Tried to get invalid enchantment value \"%s\".", value); }
    int result = 0;
    if (values_add.contains(value)) { result += values_add.at(value); }
    if (value->has_parent()) { result += get_value_add(value->get_parent()); }

    return result;
}

double enchantment::get_value_multiply(const enchantment_value_id value) const {
    if (!value.is_valid()) { debugmsg("Tried to get invalid enchantment value \"%s\".", value); }
    double result = 0;
    if (values_multiply.contains(value)) { result += values_multiply.at(value); }
    if (value->has_parent()) { result += get_value_add(value->get_parent()); }

    return result;
}

double enchantment::calc_bonus(enchantment_value_id value, double base, bool round) const {
    double add = value->can_add ? get_value_add(value) : 0.0;
    double mul = value->can_mult ? get_value_multiply(value) : 1.0;
    double ret = add + base * mul;
    if (round) { ret = trunc(ret); }
    return ret;
}

int enchantment::mult_bonus(enchantment_value_id value_type, int base_value) const {
    return get_value_multiply(value_type) * base_value;
}

void enchantment::activate_effects(Character& guy) const {
    for (const std::pair<efftype_id, int> eff : ench_effects) {
        guy.add_effect(eff.first, 1_seconds, bodypart_str_id::NULL_ID(), eff.second);
    }
}

void enchantment::deactivate_removed_effects(Character& guy, const enchantment& other) const {
    for (const std::pair<efftype_id, int> eff : other.ench_effects) {
        if (!ench_effects.contains(eff.first)) {
            guy.remove_effect(eff.first, bodypart_str_id::NULL_ID());
        }
    }
}

void enchantment::activate_passive(Character& guy) const {
    if (emitter) { get_map().emit_field(guy.bub_pos(), *emitter); }
    activate_effects(guy);
    for (const std::pair<const time_duration, std::vector<fake_spell>>& activation :
         intermittent_activation) {
        // a random approximation!
        if (one_in(to_seconds<int>(activation.first))) {
            for (const fake_spell& fake : activation.second) {
                fake.get_spell(0).cast_all_effects(guy, guy.bub_pos());
            }
        }
    }
}

void enchantment::cast_hit_you(Character& caster, const Creature& target) const {
    for (const fake_spell& sp : hit_you_effect) { cast_enchantment_spell(caster, &target, sp); }
}

void enchantment::cast_hit_me(Character& caster, const Creature* target) const {
    for (const fake_spell& sp : hit_me_effect) { cast_enchantment_spell(caster, target, sp); }
}

void enchantment::cast_enchantment_spell(
    Character& caster, const Creature* target, const fake_spell& sp) const {
    // check the chances
    if (!one_in(sp.trigger_once_in)) { return; }

    if (sp.self) {
        caster
            .add_msg_player_or_npc(m_good, sp.trigger_message, sp.npc_trigger_message, caster.name);
        sp.get_spell(sp.level).cast_all_effects(caster, caster.bub_pos());
    } else if (target != nullptr) {
        const Creature& trg_crtr = *target;
        const spell& spell_lvl = sp.get_spell(sp.level);
        if (!spell_lvl.is_valid_target(caster, trg_crtr.bub_pos())
            || !spell_lvl.is_target_in_range(caster, trg_crtr.bub_pos())) {
            return;
        }

        caster.add_msg_player_or_npc(
            m_good, sp.trigger_message, sp.npc_trigger_message, caster.name, trg_crtr.disp_name());

        spell_lvl.cast_all_effects(caster, trg_crtr.bub_pos());
    }
}

bool enchantment::operator==(const enchantment& rhs) const {
    return id == rhs.id && mutations == rhs.mutations && emitter == rhs.emitter
        && ench_effects == rhs.ench_effects && values_multiply == rhs.values_multiply
        && values_add == rhs.values_add && hit_me_effect == rhs.hit_me_effect
        && hit_you_effect == rhs.hit_you_effect
        && intermittent_activation == intermittent_activation
        && active_conditions == rhs.active_conditions;
}

namespace {

template <float mutation_branch::* First> bool is_set_value(const trait_id& mut, float val) {
    return (*mut).*First == val;
}

template <float mutation_branch::* First, float mutation_branch::*... Rest,
          std::enable_if_t<(sizeof...(Rest) > 0), bool> NonEmpty = false>
bool is_set_value(const trait_id& mut, float val) {
    return (*mut).*First == val && is_set_value<Rest...>(mut, val);
}

} // namespace

void enchantment::finalize() {
    const char* ench_desc = id.is_empty() ? "An inline enchantment" : "Enchantment";
    std::vector<std::string> problems;
    // These checks are done here because mutation cannot occur in the last stage
    auto val_add_copy = values_add;
    for (const auto& [ench_val_id, val] : val_add_copy) {
        if (ench_val_id->id != ench_val_id) {
            problems.push_back(string_format(
                "\nenchantment value %s is using legacy enchantment name automatically migrated to "
                "%s",
                ench_val_id.str(), ench_val_id->id.str()));
            values_add.erase(ench_val_id);
            if (values_add.contains(ench_val_id->id)) {
                values_add[ench_val_id->id] += val;
            } else {
                values_add[ench_val_id->id] = val;
            }
        }
    }
    auto val_mult_copy = values_multiply;
    for (const auto& [ench_val_id, val] : val_mult_copy) {
        if (ench_val_id->id != ench_val_id) {
            problems.push_back(string_format(
                "\nenchantment value %s is using legacy enchantment name automatically migrated to "
                "%s",
                ench_val_id.str(), ench_val_id->id.str()));
            values_multiply.erase(ench_val_id);
            if (values_multiply.contains(ench_val_id->id)) {
                values_multiply[ench_val_id->id] += val;
            } else {
                values_multiply[ench_val_id->id] = val;
            }
        }
    }
    if (!problems.empty()) {
        debugmsg("%s %s has: %s", ench_desc, id.c_str(),
                 enumerate_as_string(problems, enumeration_conjunction::none));
    }
}

void enchantment::finalize_all() {
    enchant_factory.finalize();
    for (const enchantment& ench : enchant_factory.get_all()) {
        const_cast<enchantment&>(ench).finalize();
    }
}

bool nested_enchant_check(
    const enchantment& ench, const enchantment_id& to_match, std::set<trait_id> mut_to_match) {
    // Populate mutations given first
    for (const trait_id& mut_id : ench.get_mutations()) {
        if (mut_to_match.contains(mut_id)) { return false; }
        mut_to_match.insert(mut_id);
    }
    for (const trait_id& mut_id : ench.get_mutations()) {
        for (const enchantment_id& nested_ench : mut_id->enchantments) {
            if (nested_ench == to_match) { return false; }
            if (!nested_enchant_check(*nested_ench, to_match, mut_to_match)) { return false; }
        }
        for (const enchantment& nested_ench : mut_id->mut_enchantments) {
            if (!nested_enchant_check(nested_ench, to_match, mut_to_match)) { return false; }
        }
    }
    return true;
}

void enchantment::check() const {
    // TODO: Where was it declared? CONTEXT!
    const char* ench_desc = id.is_empty() ? "An inline enchantment" : "Enchantment";
    std::vector<std::string> problems;
    for (const trait_id& mut : mutations) {
        if (!mut.is_valid()) {
            debugmsg("%s %s has invalid mutation %s", ench_desc, id.c_str(), mut.c_str());
        }

        if (!nested_enchant_check(*this, id, std::set<trait_id>())) {
            problems.push_back(string_format(
                "\ninfinite loop of mutations giving enchantments or dual application of a "
                "mutation caused by this enchantment",
                mut.str()));
        }
    }
    auto val_add_copy = values_add;
    for (const auto& [ench_val_id, val] : val_add_copy) {
        if (!ench_val_id.is_valid()) {
            problems.push_back(
                string_format("\nenchantment value %s is invalid", ench_val_id.str()));
        } else if (!ench_val_id->can_add) {
            problems.push_back(
                string_format("\nenchantment value %s cannot be added to", ench_val_id.str()));
        }
    }
    auto val_mult_copy = values_multiply;
    for (const auto& [ench_val_id, val] : val_mult_copy) {
        if (!ench_val_id.is_valid()) {
            problems.push_back(
                string_format("\nenchantment value %s is invalid", ench_val_id.str()));
        } else if (!ench_val_id->can_mult) {
            problems.push_back(
                string_format("\nenchantment value %s cannot be added to", ench_val_id.str()));
        }
    }
    if (!problems.empty()) {
        debugmsg("%s %s has: %s", ench_desc, id.c_str(),
                 enumerate_as_string(problems, enumeration_conjunction::none));
    }
}

void enchantment::check_consistency() { enchant_factory.check(); }
