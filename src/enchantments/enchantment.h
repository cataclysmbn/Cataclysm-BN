#pragma once

#include "calendar.h"
#include "magic.h"
#include "type_id.h"

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

// an "enchantment" is what passive artifact effects used to be:
// under certain conditions, the effect persists upon the appropriate Character
class enchantment {
public:
    // if a Character "has" an enchantment, it is viable to check for the condition
    enum has { WIELD, WORN, HELD, NUM_HAS };
    // the condition at which the enchantment is giving passive effects
    enum condition {
        ALWAYS,
        UNDERGROUND,
        ABOVEGROUND,
        UNDERWATER,
        NIGHT,
        DAY,
        DUSK,
        DAWN,
        ACTIVE, // the item, mutation, etc. is active
        INACTIVE,
        NUM_CONDITION
    };

    static void load_enchantment(const JsonObject& jo, const std::string& src);
    void load(const JsonObject& jo, const std::string& src = "");
    static void reset();

    // attempts to add two like enchantments together.
    // if their conditions don't match, return false. else true.
    bool add(const enchantment& rhs);

    // adds two enchantments together and ignores their conditions
    void force_add(const enchantment& rhs);

    int get_value_add(enchantment_value_id value) const;
    double get_value_multiply(enchantment_value_id value) const;
    int get_value_max(enchantment_value_id value) const;

    /**
     * Calculate bonus provided by this enchantment for given base value.
     */
    double calc_bonus(enchantment_value_id value, double base, bool round = false) const;

    // this enchantment has a valid condition and is in the right location
    bool is_active(const Character& guy, const item& parent) const;

    // @active means the container for the enchantment is active, for comparison to active flag.
    bool is_active(const Character& guy, bool active) const;

    /**
     * Whether this enchantment will be active if parent item is wielded.
     * Assumes condition is satisfied.
     */
    bool is_active_when_wielded() const {
        return has::WIELD == active_conditions.first || has::HELD == active_conditions.first;
    }

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

    bool operator==(const enchantment& rhs) const;

    static void check_consistency();
    void check() const;

    static void finalize_all();
    void finalize();

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

    std::pair<has, condition> active_conditions;

    void add_activation(const time_duration& freq, const fake_spell& fake);

    // checks if the enchantments have the same active_conditions
    bool stacks_with(const enchantment& rhs) const;

    int mult_bonus(enchantment_value_id value_type, int base_value) const;

    // performs cooldown and distance checks before casting enchantment spells
    void cast_enchantment_spell(
        Character& caster, const Creature* target, const fake_spell& sp) const;
};
