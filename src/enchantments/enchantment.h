#pragma once

#include "calendar.h"
#include "enchantment_condition.h"
#include "magic/magic.h"
#include "type_id.h"

#include <iomanip>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Character;
class Creature;
class item;
class JsonOut;
class JsonIn;
class JsonObject;
enum enchantment_condition_type : unsigned int;
// an "enchantment" is what passive artifact effects used to be:
// under certain conditions, the effect persists upon the appropriate Character
class enchantment {
public:
    static void load_enchantment(const JsonObject& jo, const std::string& src);
    void load(const JsonObject& jo, const std::string& src = "");
    static void reset();

    // attempts to add two like enchantments together.
    // if their conditions don't match, return false. else true.
    bool add(const enchantment& rhs);

    // adds two enchantments together and ignores their conditions
    void force_add(const enchantment& rhs);

    // Check if the enchantment has a certain value
    bool has_value(enchantment_value_id value) const;

    int get_value_add(enchantment_value_id value) const;
    double get_value_multiply(enchantment_value_id value) const;
    int get_value_max(enchantment_value_id value) const;

    // Gets weather the enchantment has a flag or not
    bool has_flag(enchantment_flag_id flag) const;

    /**
     * Calculate bonus provided by this enchantment for given base value.
     */
    double calc_bonus(enchantment_value_id value, double base, bool round = false) const;

    // this enchantment has a valid condition and is in the right location
    bool is_active(const Character& guy, const item& parent) const;

    // this enchantment has a valid condition and is in the right location
    bool is_active(const item& parent) const;

    // @active means the container for the enchantment is active, for comparison to active flag.
    bool is_active(const Character& guy, bool active) const;

    // modifies character stats, or does other passive effects
    void activate_passive(Character& guy) const;

    // activates effects
    void activate_effects(Character& guy) const;

    // deactivate effects
    void deactivate_removed_effects(Character& guy, const enchantment& other) const;

    enchantment_id id;

    bool was_loaded = false;

    void serialize(JsonOut& jsout) const;

    // casts all the hit_you_effects on the target
    void cast_hit_you(Character& caster, const Creature& target) const;
    // casts all the hit_me_effects on self or a target depending on the enchantment definition
    void cast_hit_me(Character& caster, const Creature* target) const;

    const std::set<trait_id>& get_mutations() const { return mutations; }

    bool is_immune_effect(const efftype_id& eff) const { return immune_effects.contains(eff); }

    bool is_immune_field(const field_type_id& fd) const { return immune_fields.contains(fd); }

    bool operator==(const enchantment& rhs) const;

    static void check_consistency();
    void check(
        std::set<enchantment_condition_type> cond_types = std::set<enchantment_condition_type>())
        const;

    static void finalize_all();
    void finalize();

    std::vector<std::string> get_effect_string(bool is_item) const;

private:
    std::set<trait_id> mutations;
    std::optional<emit_id> emitter;
    std::map<efftype_id, int> ench_effects;

    // values that add to the base value
    std::map<enchantment_value_id, int> values_add;
    // values that get multiplied to the base value
    // multipliers add to each other instead of multiply against themselves
    std::map<enchantment_value_id, double> values_multiply;
    // values from which the highest value is chosen
    std::map<enchantment_value_id, int> values_max;

    std::vector<fake_spell> hit_me_effect;
    std::vector<fake_spell> hit_you_effect;

    std::map<time_duration, std::vector<fake_spell>> intermittent_activation;

    std::set<enchantment_condition_id> conditions;

    std::set<efftype_id> immune_effects;

    std::set<field_type_id> immune_fields;

    std::map<enchantment_flag_id, int> flags;

    void add_activation(const time_duration& freq, const fake_spell& fake);

    // checks if the enchantments have the same active_conditions
    bool stacks_with(const enchantment& rhs) const;

    int mult_bonus(enchantment_value_id value_type, int base_value) const;

    // performs cooldown and distance checks before casting enchantment spells
    void cast_enchantment_spell(
        Character& caster, const Creature* target, const fake_spell& sp) const;
};
