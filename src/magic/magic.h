#pragma once

#include "bodypart.h"
#include "catalua_type_operators.h"
#include "coordinates.h"
#include "damage.h"
#include "enum_bitset.h"
#include "event_bus.h"
#include "magic/spell_selector.h"
#include "sounds.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

class Creature;
class JsonIn;
class JsonObject;
class JsonOut;
class nc_color;
class Character;
class spell;
class time_duration;
namespace cata {
class event;
} // namespace cata
template <typename E> struct enum_traits;

enum spell_flag {
    PERMANENT,      // items or creatures spawned with this spell do not disappear and die as normal
    IGNORE_WALLS,   // spell's aoe goes through walls
    SWAP_POS,       // a projectile spell swaps the positions of the caster and target
    HOSTILE_SUMMON, // summon spell always spawns a hostile monster
    HOSTILE_50,     // summoned monster spawns friendly 50% of the time
    SILENT,         // spell makes no noise at target
    NO_EXPLOSION_VFX, // spell has no visual explosion
    LOUD,             // spell makes extra noise at target
    VERBAL,           // spell makes noise at caster location, mouth encumbrance affects fail %
    SOMATIC,          // arm encumbrance affects fail % and casting time (slightly)
    NO_HANDS,         // hands do not affect spell energy cost
    UNSAFE_TELEPORT,  // teleport spell risks killing the caster or others
    NO_LEGS,          // legs do not affect casting time
    CONCENTRATE,      // focus affects spell fail %
    RANDOM_AOE,       // picks random number between min+increment*level and max instead of normal
                      // behavior
    RANDOM_DAMAGE,    // picks random number between min+increment*level and max instead of normal
                      // behavior
    DIVIDE_DAMAGE,    // divides damage equally among all the targets of the spell
    RANDOM_DURATION,  // picks random number between min+increment*level and max instead of normal
                      // behavior
    RANDOM_TARGET,    // picks a random valid target within your range instead of normal behavior.
    MUTATE_THRESH,    // allows mutate spell_effect to try and cross thresholds for the category
                      // provided, accuracy optionally defines highest tier of threshold to test for
                      // (default of 1).
    MUTATE_TRAIT,     // overrides the mutate spell_effect to use a specific trait_id instead of a
                      // category
    WONDER, // instead of casting each of the extra_spells, it picks N of them and casts them (where
            // N is std::min( damage(), number_of_spells ))
    PAIN_NORESIST, // pain altering spells can't be resisted (like with the deadened trait)
    NO_FAIL,       // this spell cannot fail when you cast it
    BRAWL,         // this spell can be used by brawlers
    DUPE_SOUND,    // this spell will play 'duplicate' sounds, if relevant to the spell effect
    ADD_MELEE_DAM, // Add melee damage to the spell's damage. Legacy method, "melee_dam" vector is
                   // preferred instead
    PHYSICAL, // IMPLIES BRAWL. This spell is actually a Physical Technique / Weapon Arte / similar,
              // and is sort-of a replacement of martial arts.
    MOD_MELEE_MOVES, // Use melee attack cost as a base and add spell cost on top
    MOD_MELEE_STAM,  // Use melee stamina cost as a base and add spell cost on top
    LAST
};

enum energy_type {
    hp_energy,
    mana_energy,
    stamina_energy,
    bionic_energy,
    fatigue_energy,
    none_energy
};

enum valid_target {
    target_ally,
    target_hostile,
    target_self,
    target_ground,
    target_none,
    target_item,
    target_fd_fire,
    target_fd_blood,
    _LAST
};

template <> struct enum_traits<valid_target> {
    static constexpr auto last = valid_target::_LAST;
};

template <> struct enum_traits<spell_flag> {
    static constexpr auto last = spell_flag::LAST;
};

struct fake_spell {
    spell_id id;
    // max level this spell can be
    // if null pointer, spell can be up to its own max level
    std::optional<int> max_level;
    // level for things that need it
    int level = 0;
    // target tripoint is source (true) or target (false)
    bool self = false;
    // a chance to trigger the enchantment spells
    int trigger_once_in = 1;
    // a message when the enchantment is triggered
    translation trigger_message;
    // a message when the enchantment is triggered and is on npc
    translation npc_trigger_message;

    fake_spell() = default;
    fake_spell(
        const spell_id& sp_id, bool hit_self = false,
        const std::optional<int>& max_level = std::nullopt)
        : id(sp_id),
          max_level(max_level),
          self(hit_self) {}

    // gets the spell with an additional override for minimum level (default 0)
    auto get_spell(int min_level_override = 0) const -> spell;

    auto operator==(const fake_spell& rhs) const -> bool;

    // Borrowed from LUA_TYPE_OPS, catalua_type_operators.h
    inline auto operator<(const fake_spell& rhs) const -> bool { return (id) < rhs.id; }

    void load(const JsonObject& jo);
    void serialize(JsonOut& json) const;
    void deserialize(JsonIn& jsin);
};

class spell_events: public event_subscriber {
public:
    void notify(const cata::event&) override;
};

class spell_type {
public:
    spell_type() = default;

    bool was_loaded = false;

    spell_id id;
    // spell sprite
    std::string sprite;
    // spell name
    translation name;
    // spell description
    translation description;
    // spell message when cast
    translation message;
    // spell sound effect
    translation sound_description;
    skill_id skill;

    // scale based on stats
    bool scale_str;
    bool scale_dex;
    bool scale_per;
    bool scale_int;

    // Mutations that block the spell from being cast
    std::set<trait_id> blocker_mutations;

    requirement_id spell_components;

    sounds::sound_t sound_type = sounds::sound_t::_LAST;
    bool sound_ambient = false;
    std::string sound_id;
    std::string sound_variant;
    // spell effect string. used to look up spell function
    std::string effect_name;
    std::function<void(const spell&, Creature&, const tripoint_bub_ms&)> effect;
    // extra information about spell effect. allows for combinations for effects
    std::string effect_str;
    // list of additional "spell effects"
    std::vector<fake_spell> additional_spells;

    // if the spell has a field name defined, this is where it is
    std::optional<field_type_id> field;
    // the chance one_in( field_chance ) that the field spawns at a tripoint in the area of the
    // spell
    int field_chance = 0;
    // field intensity at spell level 0
    int min_field_intensity = 0;
    // increment of field intensity per level
    float field_intensity_increment = 0.0f;
    // maximum field intensity allowed
    int max_field_intensity = 0;
    // field intensity added to the map is +- ( 1 + field_intensity_variance ) * field_intensity
    float field_intensity_variance = 0.0f;

    // minimum damage this spell can cause
    int min_damage = 0;
    // amount of damage increase per spell level
    float damage_increment = 0.0f;
    // maximum damage this spell can cause
    int max_damage = 0;

    // minimum range of a spell
    int min_range = 0;
    // amount of range increase per spell level
    float range_increment = 0.0f;
    // max range this spell can achieve
    int max_range = 0;

    // minimum "accuracy" of a spell
    int min_accuracy = 0;
    // amount of "accuracy" change per level
    float accuracy_increment = 0.0f;
    // maximum "accuracy"
    int max_accuracy = 0;

    // minimum area of effect of a spell (radius)
    // 0 means the spell only affects the target
    int min_aoe = 0;
    // amount of area of effect increase per spell level (radius)
    float aoe_increment = 0.0f;
    // max area of effect of a spell (radius)
    int max_aoe = 0;

    // damage over time deals damage per turn

    // minimum damage over time
    int min_dot = 0;
    // increment per spell level
    float dot_increment = 0.0f;
    // max damage over time
    int max_dot = 0;

    // amount of time effect lasts

    // minimum time for effect in moves
    int min_duration = 0;
    // increment per spell level in moves
    // DoT is per turn, but increments can be smaller
    int duration_increment = 0;
    // max time for effect in moves
    int max_duration = 0;

    // amount of damage that is piercing damage
    // not added to damage stat

    // minimum pierce damage
    int min_pierce = 0;
    // increment of pierce damage per spell level
    float pierce_increment = 0;
    // max pierce damage
    int max_pierce = 0;

    // base energy cost of spell
    int base_energy_cost = 0;
    // increment of energy cost per spell level
    float energy_increment = 0.0f;
    // max or min energy cost, based on sign of energy_increment
    int final_energy_cost = 0.0f;

    // base encumbrance value for the spell to be hindered by the caster's
    // arms. anything over this value will affect the spell.
    int arm_encumbrance_threshold = 20;

    // base encumerance value for the spell to be hindered by the caster's
    // legs. anything over this value will affect the spell.
    int leg_encumbrance_threshold = 20;

    // spell is restricted to being cast by only this class
    // if spell_class is empty, spell is unrestricted
    trait_id spell_class;

    // can the spell be chosen on chargen
    bool starting_spell;
    // How many points to choose the spell
    int starting_points;
    // How many points for each additional level
    int increase_points;
    // Max level of starting spell
    int max_starting_level;
    // the difficulty of casting a spell
    int difficulty = 0;

    // max level this spell can achieve
    int max_level = 0;

    // base amount of time to cast the spell in moves
    int base_casting_time = 0;
    // If spell is to summon a vehicle, the vproto_id of the vehicle
    std::string vehicle_id;
    // increment of casting time per level
    float casting_time_increment = 0.0f;
    // max or min casting time
    int final_casting_time = 0;

    // Manually defined flat sound value for people who are interested. Directly used as loudness
    // instead of calculating.
    short volume;

    // Does leveling this spell lead to learning another spell?
    std::map<std::string, int> learn_spells;

    // what energy do you use to cast this spell
    energy_type energy_source = energy_type::none_energy;

    damage_type dmg_type = damage_type::DT_NULL;

    // Melee damage types that the 'spell' uses
    std::vector<damage_type> melee_dam;

    // list of valid targets to be affected by the area of effect.
    enum_bitset<valid_target> effect_targets;

    // list of valid targets enum
    enum_bitset<valid_target> valid_targets;

    std::set<mtype_id> targeted_monster_ids;

    // lits of bodyparts this spell applies its effect to
    std::set<bodypart_str_id> affected_bps;

    enum_bitset<spell_flag> spell_tags;

    static void load_spell(const JsonObject& jo, const std::string& src);
    void load(const JsonObject& jo, const std::string&);
    /**
     * All spells in the game.
     */
    static auto get_all() -> const std::vector<spell_type>&;
    static void check_consistency();
    static void reset_all();
    auto is_valid() const -> bool;

    LUA_TYPE_OPS(spell_type, id);
};

class spell {
public:
    // Here for Lua reasons.
    spell_id type;

private:
    friend class spell_events;
    // basic spell data

    // once you accumulate enough exp you level the spell
    int experience = 0;
    // returns damage type for the spell
    auto dmg_type() const -> damage_type;

    // alternative cast message
    translation alt_message;

    // minimum damage including levels
    auto min_leveled_damage() const -> int;
    // minimum aoe including levels
    auto min_leveled_aoe() const -> int;
    // minimum duration including levels (moves)
    auto min_leveled_duration() const -> int;
    // get the sum of the deltas of relevant stats away from 8
    auto get_stats_deltas(const Character& guy) const -> int;
    // get the multiplier to spell stats from character stats
    auto get_stat_mult(bool decrease, const Character& guy) const -> double;

public:
    spell() = default;
    spell(spell_id sp, int xp = 0);

    // sets the message to be different than the spell_type specifies
    void set_message(const translation& msg);

    // how much exp you need for the spell to gain a level
    auto exp_to_next_level() const -> int;
    // progress to the next level, expressed as a percent
    auto exp_progress() const -> std::string;
    // how much xp you have total
    auto xp() const -> int;
    // gain some exp
    void gain_exp(int nxp);
    void set_exp(int nxp);
    // how much xp you get if you successfully cast the spell
    auto casting_exp(const Character& guy) const -> int;
    // modifier for gaining exp
    auto exp_modifier(const Character& guy) const -> float;
    // level up!
    void gain_level();
    // gains a number of levels, or until max. 0 or less just returns early.
    void gain_levels(int gains);
    void set_level(int nlevel);
    // is the spell at max level?
    auto is_max_level() const -> bool;
    // what is the max level of the spell
    auto get_max_level() const -> int;
    // what are the blocker mutations
    auto get_blocker_muts() const -> std::set<trait_id>;

    // what is the intensity of the field the spell generates ( 0 if no field )
    auto field_intensity() const -> int;
    // how much damage does the spell do
    auto damage() const -> int;
    auto get_dealt_damage_instance() const -> dealt_damage_instance;
    auto get_damage_instance() const -> damage_instance;
    // damage with character stats taken into account
    auto damage_as_character(const Character& guy) const -> int;
    auto get_dealt_damage_instance(const Character& guy) const -> dealt_damage_instance;
    auto get_damage_instance(const Character& guy) const -> damage_instance;
    // how big is the spell's radius
    auto aoe() const -> int;
    // "accuracy" of spells (used for determining body part hit)
    auto accuracy() const -> int;
    // distance spell can be cast
    auto range() const -> int;
    // how much energy does the spell cost
    auto energy_cost(const Character& guy) const -> int;
    // how long does this spell's effect last
    auto duration() const -> int;
    auto duration_turns() const -> time_duration;
    // How loud is the spell?
    auto volume() const -> short;
    // how often does the spell fail
    // based on difficulty, level of spell, spellcraft skill, intelligence
    auto spell_fail(const Character& guy) const -> float;
    auto colorized_fail_percent(const Character& guy) const -> std::string;
    // how long does it take to cast the spell
    auto casting_time(const Character& guy) const -> int;
    // the requirement data for spell components. includes tools, items, and qualities.
    auto components() const -> const requirement_data&;
    auto has_components() const -> bool;
    // can the Character cast this spell?
    auto can_cast(Character& guy) const -> bool;
    // can the Character learn this spell?
    auto can_learn(const Character& guy) const -> bool;
    // is this spell valid
    auto is_valid() const -> bool;
    // is the bodypart affected by the effect
    auto bp_is_affected(body_part bp) const -> bool;
    // check if the spell has a particular flag
    auto has_flag(const spell_flag& flag) const -> bool;
    // check if the spell's class is the same as input
    auto is_spell_class(const trait_id& mid) const -> bool;

    auto in_aoe(const tripoint_bub_ms& source, const tripoint_bub_ms& target) const -> bool;

    // get spell id (from type)
    auto id() const -> spell_id;
    // get spell class (from type)
    auto spell_class() const -> trait_id;
    // get skill id
    auto skill() const -> skill_id;
    // get spell effect string (from type)
    auto effect() const -> std::string;
    // get spell effect_str data
    auto effect_data() const -> std::string;
    // get spell summon vehicle id
    auto summon_vehicle_id() const -> vproto_id;
    // name of spell (translated)
    auto name() const -> std::string;
    // description of spell (translated)
    auto description() const -> std::string;
    // spell message when cast (translated)
    auto message() const -> std::string;
    // energy source as a string (translated)
    auto energy_string() const -> std::string;
    // energy cost returned as a string
    auto energy_cost_string(const Character& guy) const -> std::string;
    // current energy the Character has available as a string
    auto energy_cur_string(const Character& guy) const -> std::string;
    // prints out a list of valid targets separated by commas
    auto enumerate_targets() const -> std::string;
    // returns the name string of all list of all targeted monster id
    // if targeted_monster_ids is empty, it returns an empty string
    auto list_targeted_monster_names() const -> std::string;

    auto damage_string(const Character& guy) const -> std::string;
    auto aoe_string() const -> std::string;
    auto duration_string() const -> std::string;

    // energy source enum
    auto energy_source() const -> energy_type;
    // the color that's representative of the damage type
    auto damage_type_color() const -> nc_color;
    auto damage_type_string() const -> std::string;
    // your level in this spell
    auto get_level() const -> int;
    // difficulty of the level
    auto get_difficulty() const -> int;

    // tries to create a field at the location specified
    void create_field(const tripoint_bub_ms& at) const;

    // makes a spell sound at the location
    void make_sound(const tripoint_bub_ms& target, Creature& caster) const;
    void make_sound(const tripoint_bub_ms& target, Creature& caster, int loudness) const;
    // heals the critter at the location, returns amount healed (Character heals each body part)
    auto heal(const tripoint_bub_ms& target) const -> int;

    // casts the spell effect. returns true if successful
    void cast_spell_effect(Creature& source, const tripoint_bub_ms& target) const;
    // goes through the spell effect and all of its internal spells
    void cast_all_effects(Creature& source, const tripoint_bub_ms& target) const;
    // uses up the components in @you's inventory
    void use_components(Character& who) const;
    // checks if a target point is in spell range
    auto is_target_in_range(const Creature& caster, const tripoint_bub_ms& p) const -> bool;

    // is the target valid for this spell?
    auto is_valid_target(const Creature& caster, const tripoint_bub_ms& p) const -> bool;
    auto is_valid_target(valid_target t) const -> bool;
    auto is_valid_effect_target(valid_target t) const -> bool;
    auto target_by_monster_id(const tripoint_bub_ms& p) const -> bool;

    // picks a random valid tripoint from @area
    auto random_valid_target(const Creature& caster, const tripoint_bub_ms& caster_pos) const
        -> std::optional<tripoint_bub_ms>;

    LUA_TYPE_OPS(spell, type);
};

class known_magic {
private:
    // list of spells known
    std::map<spell_id, spell> spellbook;
    // invlets assigned to spell_id
    std::map<spell_id, int, spell_id::LexCmp> invlets;
    // the last known spell selected for casting
    std::optional<spell_id> last_cast_spell_id;
    // per-Character spell selector preferences
    std::set<spell_id, spell_id::LexCmp> favorite_spells;
    spell_selector_category_id last_spell_selector_category;
    // the base mana a Character would start with
    int mana_base = 0;
    // current mana
    int mana = 0;

public:
    // ignores all distractions when casting a spell when true
    bool casting_ignore = false;

    known_magic();

    void learn_spell(const std::string& sp, Character& guy, bool force = false);
    void learn_spell(const spell_id& sp, Character& guy, bool force = false);
    void learn_spell(const spell_type* sp, Character& guy, bool force = false);
    void forget_spell(const std::string& sp);
    void forget_spell(const spell_id& sp);
    // time in moves for the Character to memorize the spell
    auto time_to_learn_spell(const Character& guy, const spell_id& sp) const -> int;
    auto time_to_learn_spell(const Character& guy, const std::string& str) const -> int;
    auto can_learn_spell(const Character& guy, const spell_id& sp) const -> bool;
    auto knows_spell(const std::string& sp) const -> bool;
    auto knows_spell(const spell_id& sp) const -> bool;
    // does the Character know a spell?
    auto knows_spell() const -> bool;
    // spells known by Character
    auto spells() const -> std::vector<spell_id>;
    // gets the spell associated with the spell_id to be edited
    auto get_spell(const spell_id& sp) -> spell&;
    auto last_cast_spell() const -> std::optional<spell_id>;
    auto set_last_cast_spell(const spell_id& sp) -> void;
    auto toggle_favorite_spell(const spell_id& sp) -> bool;
    // opens up a ui that the Character can choose a spell from
    // returns the index of the spell in the vector of spells
    auto select_spell(Character& guy) -> int;
    // get all known spells
    auto get_spells() -> std::vector<spell*>;
    // how much mana is available to use to cast spells
    auto available_mana() const -> int;
    // max mana vailable
    auto max_mana(const Character& guy) const -> int;
    void mod_mana(const Character& guy, int add_mana);
    void set_mana(int new_mana);
    /** Mana regeneration rate (units per turn). */
    auto mana_regen_rate(const Character& guy) const -> double;
    void update_mana(const Character& guy, double turns);
    // does the Character have enough energy to cast this spell?
    // not specific to mana
    auto has_enough_energy(const Character& guy, spell& sp) const -> bool;

    void on_mutation_gain(const trait_id& mid, Character& guy);
    void on_mutation_loss(const trait_id& mid);

    void serialize(JsonOut& json) const;
    void deserialize(JsonIn& jsin);

    // returns false if the spell or invlet is unavailable
    auto set_invlet(const spell_id& sp, int invlet, const std::set<int>& used_invlets) -> bool;
    void rem_invlet(const spell_id& sp);

private:
    auto set_spell_selector_category(spell_selector_category_id category) -> void;
    // Drops malformed/conflicting active invlets and returns the keys reserved by active spells.
    auto sanitize_invlets(const std::set<int>& reserved_invlets) -> std::set<int>;
    // returns the assigned invlet or selects an unused active invlet
    auto get_invlet(const spell_id& sp, std::set<int>& used_invlets) -> int;
};

namespace spell_effect {

void teleport_random(const spell& sp, Creature& caster, const tripoint_bub_ms&);
void pain_split(const spell&, Creature&, const tripoint_bub_ms&);
void target_attack(const spell& sp, Creature& caster, const tripoint_bub_ms& epicenter);
void projectile_attack(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void cone_attack(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void line_attack(const spell& sp, Creature& caster, const tripoint_bub_ms& target);

void area_pull(const spell& sp, Creature& caster, const tripoint_bub_ms& center);
void area_push(const spell& sp, Creature& caster, const tripoint_bub_ms& center);
void directed_push(const spell& sp, Creature& caster, const tripoint_bub_ms& target);

auto spell_effect_blast(
    const spell&, const tripoint_bub_ms&, const tripoint_bub_ms& target, int aoe_radius,
    bool ignore_walls) -> std::set<tripoint_bub_ms>;
auto spell_effect_cone(
    const spell& sp, const tripoint_bub_ms& source, const tripoint_bub_ms& target, int aoe_radius,
    bool ignore_walls) -> std::set<tripoint_bub_ms>;
auto spell_effect_line(
    const spell&, const tripoint_bub_ms& source, const tripoint_bub_ms& target, int aoe_radius,
    bool ignore_walls) -> std::set<tripoint_bub_ms>;

void spawn_ethereal_item(const spell& sp, Creature&, const tripoint_bub_ms&);
void recover_energy(const spell& sp, Creature&, const tripoint_bub_ms& target);
void spawn_summoned_monster(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void spawn_summoned_vehicle(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void translocate(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
// adds a timed event to the caster only
void timed_event(const spell& sp, Creature& caster, const tripoint_bub_ms&);
void transform_blast(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void noise(const spell& sp, Creature&, const tripoint_bub_ms& target);
void vomit(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void explosion(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void flashbang(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void mod_moves(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void map_area(const spell& sp, Creature& caster, const tripoint_bub_ms&);
void morale(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void charm_monster(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void mutate(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void bash(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void dash(const spell& sp, Creature& caster, const tripoint_bub_ms& target);
void none(const spell& sp, Creature&, const tripoint_bub_ms& target);
} // namespace spell_effect

class spellbook_callback: public uilist_callback {
private:
    std::vector<spell_type> spells;

public:
    void add_spell(const spell_id& sp);
    void refresh(uilist* menu) override;
};

// Utility structure to run area queries over weight map. It uses shortest-path-expanding-tree,
// similar to the ones used in pathfinding. Some spell effects, like area_pull use the final
// tree to determine where to move affected objects.
struct area_expander {
    // A single node for a tree.
    struct node {
        // Expanded position
        tripoint_bub_ms position;
        // Previous position
        tripoint_bub_ms from;
        // Accumulated cost.
        float cost = 0;
    };

    int max_range = -1;
    int max_expand = -1;

    // The area we have visited already.
    std::vector<node> area;

    // Maps coordinate to expanded node.
    std::map<tripoint_bub_ms, int> area_search;

    struct area_node_comparator {
        area_node_comparator(std::vector<area_expander::node>& area): area(area) {}

        auto operator()(int a, int b) const -> bool { return area[a].cost < area[b].cost; }

        std::vector<area_expander::node>& area;
    };

    std::priority_queue<int, std::vector<int>, area_node_comparator> frontier;

    area_expander();
    // Check whether we have already visited this node.
    auto contains(const tripoint_bub_ms& pt) const -> int;

    // Adds node to a search tree. Returns true if new node is allocated.
    auto enqueue(const tripoint_bub_ms& from, const tripoint_bub_ms& to, float cost) -> bool;

    // Run wave propagation
    auto run(const tripoint_bub_ms& center) -> int;

    // Sort nodes by its cost.
    void sort_ascending();

    void sort_descending();
};
