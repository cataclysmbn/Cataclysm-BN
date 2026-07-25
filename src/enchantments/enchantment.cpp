#include "enchantment.h"

#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "creature.h"
#include "debug.h"
#include "effect.h"
#include "enchantment_condition.h"
#include "enchantment_flag.h"
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
#include "type_id_implement.h"
#include "units.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <vector>

namespace {
generic_factory<enchantment> enchant_factory("enchantment");
} // namespace

IMPLEMENT_STRING_AND_INT_IDS(enchantment, enchant_factory);

std::vector<std::string> enchantment::get_effect_string(bool is_item) const {
    std::string cond_string;
    if (conditions.empty()) { cond_string = _("At all times"); }
    for (const enchantment_condition_id cond_id : conditions) {
        if (!cond_string.empty()) { cond_string += _(" and "); }
        cond_string += cond_id->condition_info.translated();
    }

    std::map<enchantment_value_id, int> value_effects;
    for (const auto [ench_id, effect] : values_add) {
        if (effect > 0) {
            value_effects[ench_id] += (ench_id->increase_good) ? 1 : -1;
        } else if (effect < 0) {
            value_effects[ench_id] += (ench_id->increase_good) ? -1 : 1;
        }
    }
    for (const auto [ench_id, effect] : values_multiply) {
        if (effect > 1) {
            value_effects[ench_id] += (ench_id->increase_good) ? 1 : -1;
        } else if (effect < 1) {
            value_effects[ench_id] += (ench_id->increase_good) ? -1 : 1;
        }
    }
    for (const auto [ench_id, effect] : values_max) {
        // Value max fundamentally cant be bad
        value_effects[ench_id] += 1;
    }
    std::vector<std::string> result;
    bool describe = false;
    result.push_back(cond_string);
    for (const auto [ench_id, goodbad] : value_effects) {
        const std::string color = goodbad > 0 ? "green" : goodbad < 0 ? "red" : "magenta";
        result.push_back(string_format("  <color_%s>%s</color>", color, ench_id->desc));
        describe = true;
    }
    for (const auto [eff_id, intense] : ench_effects) {
        if (!eff_id->is_show_in_info()) { continue; }
        effect_rating rating = eff_id->get_rating();
        const std::string color =
            rating == effect_rating::e_good ? "green"
            : rating == effect_rating::e_bad
                ? "red"
                : "magenta";
        const std::string name =
            effect(&*eff_id, 1_turns, bodypart_str_id::NULL_ID(), intense, calendar::turn)
                .disp_name();
        if (name != "") {
            result.push_back(string_format(_("  <color_%s>Gives effect %s</color>"), color, name));
            describe = true;
        }
    }
    for (const auto eff_id : immune_effects) {
        if (!eff_id->is_show_in_info()) { continue; }
        effect_rating rating = eff_id->get_rating();
        const std::string color =
            rating == effect_rating::e_bad ? "green"
            : rating == effect_rating::e_good
                ? "red"
                : "magenta";
        const std::string name =
            effect(&*eff_id, 1_turns, bodypart_str_id::NULL_ID(), 1, calendar::turn).disp_name();
        if (name != "") {
            result.push_back(
                string_format(_("  <color_%s>Provides Immunity To %s</color>"), color, name));
            describe = true;
        }
    }
    for (const auto trait_id : mutations) {
        const std::string color =
            trait_id->points > 0 ? "green"
            : trait_id->points < 0
                ? "red"
                : "magenta";
        result.push_back(
            string_format(_("  <color_%s>Gives mutation %s</color>"), color, trait_id->name()));
        describe = true;
    }
    for (const auto [flag, _] : flags) { result.push_back(string_format("  %s", flag->info)); }
    if (describe) {
        return result;
    } else {
        return std::vector<std::string>();
    }
}

void enchantment::load_enchantment(const JsonObject& jo, const std::string& src) {
    enchant_factory.load(jo, src);
}

void enchantment::reset() { enchant_factory.reset(); }

bool enchantment::is_active(const Character& guy, const item& parent) const {

    bool is_active = parent.is_active();
    bool active = true;
    for (const enchantment_condition_id cond_id : conditions) {
        if (!active) { break; }
        switch (cond_id->cond_type) {
            case enchantment_condition_type::ITEM:
                active &= cond_id->item_condition(parent);
                break;
            case enchantment_condition_type::ITEM_CHARACTER:
                active &= cond_id->item_character_condition(guy, parent);
                break;
            case enchantment_condition_type::CHARACTER:
                active &= cond_id->character_condition(guy, is_active);
                break;
            case enchantment_condition_type::GLOBAL:
                active &= cond_id->generic_condition(is_active);
                break;
            default:
                debugmsg(
                    "Enchantment %s has.... AN INVALID ENCHANTMENT CONDITION TYPE, it will never trigger.",
                    id.str());
                active = false;
                break;
        }
    }
    return active;
}

bool enchantment::is_active(const item& parent) const {

    bool is_active = parent.is_active();
    bool active = true;
    for (const enchantment_condition_id cond_id : conditions) {
        if (!active) { break; }
        switch (cond_id->cond_type) {
            case enchantment_condition_type::ITEM:
                active &= cond_id->item_condition(parent);
                break;
            case enchantment_condition_type::ITEM_CHARACTER:
                debugmsg(
                    "Enchantment %s has item and character condition %s on a non-supporting enchantment value, it will never trigger.",
                    id.str(), cond_id.str());
                active = false;
                break;
            case enchantment_condition_type::CHARACTER:
                debugmsg(
                    "Enchantment %s has character condition %s on a non-supporting enchantment value, it will never trigger.",
                    id.str(), cond_id.str());
                active = false;
                break;
            case enchantment_condition_type::GLOBAL:
                active &= cond_id->generic_condition(is_active);
                break;
            default:
                debugmsg(
                    "Enchantment %s has.... AN INVALID ENCHANTMENT CONDITION TYPE, it will never trigger.",
                    id.str());
                active = false;
                break;
        }
    }
    return active;
}

bool enchantment::is_active(const Character& guy, const bool is_active) const {
    bool active = true;
    for (const enchantment_condition_id cond_id : conditions) {
        if (!active) { break; }
        switch (cond_id->cond_type) {
            case enchantment_condition_type::ITEM:
                debugmsg(
                    "Enchantment %s has item condition %s on a non-item, it will never trigger.",
                    id.str(), cond_id.str());
                active = false;
                break;
            case enchantment_condition_type::ITEM_CHARACTER:
                debugmsg(
                    "Enchantment %s has item and character condition %s on a non-item, it will never trigger.",
                    id.str(), cond_id.str());
                active = false;
                break;
            case enchantment_condition_type::CHARACTER:
                active &= cond_id->character_condition(guy, is_active);
                break;
            case enchantment_condition_type::GLOBAL:
                active &= cond_id->generic_condition(is_active);
                break;
            default:
                debugmsg(
                    "Enchantment %s has.... AN INVALID ENCHANTMENT CONDITION TYPE, it will never trigger.",
                    id.str());
                active = false;
                break;
        }
    }
    return active;
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

    optional(jo, was_loaded, "conditions", conditions);
    if (jo.has_string("has")) { conditions.insert(enchantment_condition_id(jo.get_string("has"))); }
    if (jo.has_string("condition")) {
        conditions.insert(enchantment_condition_id(jo.get_string("condition")));
    }

    for (JsonObject jsobj : jo.get_array("ench_effects")) {
        ench_effects.emplace(efftype_id(jsobj.get_string("effect")), jsobj.get_int("intensity"));
    }

    optional(jo, was_loaded, "mutations", mutations);
    optional(jo, was_loaded, "immune_effects", immune_effects);
    optional(jo, was_loaded, "immune_fields", immune_fields);

    if (jo.has_array("values")) {
        for (const JsonObject value_obj : jo.get_array("values")) {
            enchantment_value_id value = enchantment_value_id(value_obj.get_string("value"));
            const int add = value_obj.get_int("add", 0);
            const double mult = value_obj.get_float("multiply", 0.0);
            const double max = value_obj.get_int("max", 0);
            if (add != 0) { values_add.emplace(value, add); }
            if (mult != 0.0) {
                // Limit precision to minimize inconsistencies between platforms / compilers
                const double mul = static_cast<int>(std::round(mult * 100'000)) / 100'000.0;
                values_multiply.emplace(value, mul);
            }
            if (max != 0) { values_max.emplace(value, max); }
        }
    }
    if (jo.has_array("flags")) {
        for (const auto flag : jo.get_string_array("flags")) {
            flags[enchantment_flag_id(flag)] = 1;
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

    jsout.member("conditions", conditions);
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

    jsout.member("flags");
    jsout.start_array();
    for (const auto& [ench_flag_id, cnt] : flags) { jsout.write(ench_flag_id.str()); }
    jsout.end_array();

    jsout.member("immune_effects", immune_effects);
    jsout.member("immune_fields", immune_fields);
    jsout.end_object();
}

bool enchantment::stacks_with(const enchantment& rhs) const { return conditions == rhs.conditions; }

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

    for (const auto& pair_values : rhs.values_max) {
        if (values_max[pair_values.first] < pair_values.second) {
            values_max[pair_values.first] = pair_values.second;
        }
    }

    hit_me_effect.insert(hit_me_effect.end(), rhs.hit_me_effect.begin(), rhs.hit_me_effect.end());

    hit_you_effect
        .insert(hit_you_effect.end(), rhs.hit_you_effect.begin(), rhs.hit_you_effect.end());

    ench_effects.insert(rhs.ench_effects.begin(), rhs.ench_effects.end());

    immune_effects.insert(rhs.immune_effects.begin(), rhs.immune_effects.end());
    immune_fields.insert(rhs.immune_fields.begin(), rhs.immune_fields.end());

    if (rhs.emitter) { emitter = rhs.emitter; }

    for (const trait_id& branch : rhs.mutations) { mutations.emplace(branch); }

    for (const std::pair<const time_duration, std::vector<fake_spell>>& act_pair :
         rhs.intermittent_activation) {
        for (const fake_spell& fake : act_pair.second) {
            intermittent_activation[act_pair.first].emplace_back(fake);
        }
    }
    for (const auto& [ench_flag_id, count] : rhs.flags) {
        int remains = count;
        for (const enchantment_flag_id& conf_flag : ench_flag_id->conflicts) {
            if (flags.contains(conf_flag)) {
                flags[conf_flag] -= count;
                if (flags[conf_flag] <= 0) {
                    remains = std::min(-flags[conf_flag], remains);
                    flags.erase(conf_flag);
                }
            }
        }
        if (remains > 0) { flags[ench_flag_id] += remains; }
    }
}

bool enchantment::has_flag(const enchantment_flag_id flag) const {
    if (!flag.is_valid()) { debugmsg("Tried to get invalid enchantment flag \"%s\".", flag); }
    if (flags.contains(flag)) {
        if (flags.at(flag) <= 0) {
            debugmsg("Flag \"%s\" was canceled but remains in the list", flag);
        } else {
            return true;
        }
    }
    return false;
}

bool enchantment::has_value(const enchantment_value_id value) const {
    if (!value.is_valid()) { debugmsg("Tried to get invalid enchantment value \"%s\".", value); }
    return values_add.contains(value) || values_multiply.contains(value)
        || values_max.contains(value);
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
    if (value->has_parent()) { result += get_value_multiply(value->get_parent()); }

    return result;
}

int enchantment::get_value_max(const enchantment_value_id value) const {
    if (!value.is_valid()) { debugmsg("Tried to get invalid enchantment value \"%s\".", value); }
    int result = 0;
    if (values_max.contains(value)) { result = values_max.at(value); }
    if (value->has_parent()) { result = std::max(result, get_value_max(value->get_parent())); }

    return result;
}

double enchantment::calc_bonus(enchantment_value_id value, double base, bool round) const {
    double add = value->can_add ? get_value_add(value) : 0.0;
    double mul = value->can_mult ? get_value_multiply(value) : 0.0;
    double max = value->can_max ? get_value_max(value) : 0.0;
    double ret = add + base * mul;
    // This is seperated because apparently adding 0.0 is very scrungly to the computer
    // Caused a bunch of tests to splode
    if (max != 0) { ret += max; }
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
        && values_add == rhs.values_add && values_max == rhs.values_max
        && hit_me_effect == rhs.hit_me_effect && hit_you_effect == rhs.hit_you_effect
        && intermittent_activation == intermittent_activation && conditions == rhs.conditions;
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
    auto val_max_copy = values_max;
    for (const auto& [ench_val_id, val] : val_max_copy) {
        if (ench_val_id->id != ench_val_id) {
            problems.push_back(string_format(
                "\nenchantment value %s is using legacy enchantment name automatically migrated to "
                "%s",
                ench_val_id.str(), ench_val_id->id.str()));
            values_max.erase(ench_val_id);
            if (values_max.contains(ench_val_id->id)) {
                values_max[ench_val_id->id] += val;
            } else {
                values_max[ench_val_id->id] = val;
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
    auto flags_copy = flags;
    for (const auto& [ench_flag_id, _] : flags) {
        auto parents = ench_flag_id->get_parents();
        for (const auto parent : parents) { flags[parent] = 1; }
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

void enchantment::check(std::set<enchantment_condition_type> incompatible_cond_types) const {
    // TODO: Where was it declared? CONTEXT!
    const char* ench_desc = id.is_empty() ? "An inline enchantment" : "Enchantment";
    std::vector<std::string> problems;
    std::set<enchantment_condition_type> cond_types;
    for (const auto& ench_cond_id : conditions) {
        if (!ench_cond_id.is_valid()) {
            problems.push_back(
                string_format("\nenchantment condition %s is invalid", ench_cond_id.str()));
        } else {
            if (incompatible_cond_types.contains(ench_cond_id->cond_type)) {
                problems.push_back(string_format(
                    "\nenchantment condition %s has type %s unsupported by the enchantment's usage",
                    ench_cond_id.str(), io::enum_to_string(ench_cond_id->cond_type)));
            }
            cond_types.insert(ench_cond_id->cond_type);
        }
    }
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
    for (const auto& [ench_flag, _] : flags) {
        if (!ench_flag.is_valid()) {
            problems.push_back(string_format("\nenchantment flag %s is invalid", ench_flag.str()));
        }
    }
    auto val_add_copy = values_add;
    for (const auto& [ench_val_id, val] : val_add_copy) {
        if (!ench_val_id.is_valid()) {
            problems.push_back(
                string_format("\nenchantment value %s is invalid", ench_val_id.str()));
        } else {
            if (!ench_val_id->can_add) {
                problems.push_back(
                    string_format("\nenchantment value %s cannot be added to", ench_val_id.str()));
            }
            for (const auto& cond_type : ench_val_id->unsupported_conditions) {
                if (cond_types.contains(cond_type)) {
                    problems.push_back(string_format(
                        "\nenchantment value %s does not support condition type %s",
                        ench_val_id.str(), io::enum_to_string(cond_type))

                    );
                }
            }
        }
    }
    auto val_max_copy = values_max;
    for (const auto& [ench_val_id, val] : val_max_copy) {
        if (!ench_val_id.is_valid()) {
            problems.push_back(
                string_format("\nenchantment value %s is invalid", ench_val_id.str()));
        } else {
            if (!ench_val_id->can_max) {
                problems.push_back(
                    string_format("\nenchantment value %s cannot use max", ench_val_id.str()));
            }
            for (const auto& cond_type : ench_val_id->unsupported_conditions) {
                if (cond_types.contains(cond_type)) {
                    problems.push_back(string_format(
                        "\nenchantment value %s does not support condition type %s",
                        ench_val_id.str(), io::enum_to_string(cond_type))

                    );
                }
            }
        }
    }
    auto val_mult_copy = values_multiply;
    for (const auto& [ench_val_id, val] : val_mult_copy) {
        if (!ench_val_id.is_valid()) {
            problems.push_back(
                string_format("\nenchantment value %s is invalid", ench_val_id.str()));
        } else {
            if (!ench_val_id->can_mult) {
                problems.push_back(
                    string_format("\nenchantment value %s cannot be added to", ench_val_id.str()));
            }

            for (const auto& cond_type : ench_val_id->unsupported_conditions) {
                if (cond_types.contains(cond_type)) {
                    problems.push_back(string_format(
                        "\nenchantment value %s does not support condition type %s",
                        ench_val_id.str(), io::enum_to_string(cond_type))

                    );
                }
            }
        }
    }
    if (!problems.empty()) {
        debugmsg("%s %s has: %s", ench_desc, id.c_str(),
                 enumerate_as_string(problems, enumeration_conjunction::none));
    }
}

void enchantment::check_consistency() { enchant_factory.check(); }
