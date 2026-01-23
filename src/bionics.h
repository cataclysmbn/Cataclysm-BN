#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bodypart.h"
#include "calendar.h"
#include "catalua_type_operators.h"
#include "flat_set.h"
#include "pldata.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "enums.h"
#include "color.h"

class JsonIn;
class JsonObject;
class JsonOut;
class Character;
class player;

enum class character_stat : char;

/**
 * Bonus modifiers that can be applied by bionics.
 * Mirrors many fields from mutation_branch for consistency.
 * Used for both passive_bonuses and active_bonuses in bionic_data.
 */
struct bionic_bonuses {
    // Stat modifiers (replicates mutation pattern)
    float str_modifier = 0.0f;
    float dex_modifier = 0.0f;
    float int_modifier = 0.0f;
    float per_modifier = 0.0f;

    // Health & Healing
    float pain_recovery = 0.0f;
    float healing_awake = 0.0f;
    float healing_resting = 0.0f;
    float mending_modifier = 0.0f;
    float hp_modifier = 0.0f;
    float hp_modifier_secondary = 0.0f;
    float hp_adjustment = 0.0f;
    float healthy_rate = 1.0f;
    float bleed_resist = 0.0f;

    // Combat bonuses
    int cut_dmg_bonus = 0;
    float pierce_dmg_bonus = 0.0f;
    int bash_dmg_bonus = 0;

    // Movement & Speed
    float dodge_modifier = 0.0f;
    float speed_modifier = 1.0f;
    float movecost_modifier = 1.0f;
    float movecost_flatground_modifier = 1.0f;
    float movecost_obstacle_modifier = 1.0f;
    float movecost_swim_modifier = 1.0f;
    float attackcost_modifier = 1.0f;

    // Physical capabilities
    float falling_damage_multiplier = 1.0f;
    float max_stamina_modifier = 1.0f;
    float stamina_regen_modifier = 0.0f;
    float weight_capacity_modifier = 1.0f;

    // Perception & Stealth
    float hearing_modifier = 1.0f;
    float noise_modifier = 1.0f;
    float stealth_modifier = 0.0f;
    float night_vision_range = 0.0f;

    // Environment
    int bodytemp_min = 0;
    int bodytemp_max = 0;
    int bodytemp_sleep = 0;
    float temperature_speed_modifier = 0.0f;
    float scent_modifier = 1.0f;

    // Resource consumption
    float metabolism_modifier = 0.0f;
    float thirst_modifier = 0.0f;
    float fatigue_modifier = 0.0f;
    float fatigue_regen_modifier = 0.0f;

    // Skills & Crafting
    float reading_speed_multiplier = 1.0f;
    float skill_rust_multiplier = 1.0f;
    float crafting_speed_modifier = 1.0f;
    float construction_speed_modifier = 1.0f;
    float packmule_modifier = 1.0f;
    std::map<skill_id, int> craft_skill_bonus;

    // Overmap
    float overmap_sight = 0.0f;
    float overmap_multiplier = 1.0f;

    // Social (infrastructure for future bio_face_mask, bio_voice, bio_deformity conversion)
    social_modifiers social_mods;

    // Mana
    float mana_modifier = 0.0f;
    float mana_multiplier = 1.0f;
    float mana_regen_multiplier = 1.0f;

    /** Returns true if any field differs from its default value */
    bool has_any() const;

    void load( const JsonObject &jo, bool strict );
};

struct bionic_data {
    bionic_data();
    ~bionic_data();

    bionic_id id;

    translation name;
    translation description;
    /** Power cost on activation */
    units::energy power_activate = 0_kJ;
    /** Power cost on deactivation */
    units::energy power_deactivate = 0_kJ;
    /** Power cost over time, does nothing without a non-zero charge_time */
    units::energy power_over_time = 0_kJ;
    /** Power cost when the bionic's special effect is triggered */
    units::energy power_trigger = 0_kJ;
    /** Kcal cost when the bionic's special effect is triggered */
    int kcal_trigger = 0;
    /** How often a bionic draws or produces power while active in turns */
    int charge_time = 0;
    /** Power bank size **/
    units::energy capacity = 0_kJ;


    /** Is true if a bionic is an active instead of a passive bionic */
    bool activated = false;
    /**
    * If true, this bionic is included with another.
    */
    bool included = false;
    /**Factor modifiying weight capacity*/
    float weight_capacity_modifier = 1.0f;
    /**Bonus to weight capacity*/
    units::mass weight_capacity_bonus = 0_gram;
    /**Map of stats and their corresponding bonuses passively granted by a bionic*/
    std::map<character_stat, int> stat_bonus;
    /**Bonuses applied when bionic is passive, or when activatable bionic is NOT powered*/
    bionic_bonuses passive_bonuses;
    /**Bonuses applied when activatable bionic IS powered (replaces passive_bonuses)*/
    bionic_bonuses active_bonuses;
    /**This bionic draws power through a cable*/
    bool is_remote_fueled = false;
    /**This bionic draws power through a cable*/
    units::energy remote_fuel_draw = 0_J;
    /**Fuel types that can be used by this bionic*/
    std::vector<itype_id> fuel_opts;
    /**How much fuel this bionic can hold*/
    int fuel_capacity = 0;
    /**Fraction of fuel energy converted to bionic power*/
    float fuel_efficiency = 0.0f;
    /**Multiplies the amount of fuel when loading into the bionic storage*/
    int fuel_multiplier = 1;
    /**Fraction of fuel energy passively converted to bionic power*/
    float passive_fuel_efficiency = 0.0f;
    /**Fraction of coverage diminishing fuel_efficiency*/
    std::optional<float> coverage_power_gen_penalty;
    /**If true this bionic emits heat when producing power*/
    bool exothermic_power_gen = false;
    /**Type of field emitted by this bionic when it produces energy*/
    emit_id power_gen_emission = emit_id::NULL_ID();

    /**Amount of environemental protection offered by this bionic*/
    std::map<bodypart_str_id, int> env_protec;
    /**Amount of bash protection offered by this bionic*/
    std::map<bodypart_str_id, int> bash_protec;
    /**Amount of cut protection offered by this bionic*/
    std::map<bodypart_str_id, int> cut_protec;
    /**Amount of bullet protection offered by this bionic*/
    std::map<bodypart_str_id, int> bullet_protec;

    /**
     * Body part slots used to install this bionic, mapped to the amount of space required.
     */
    std::map<bodypart_str_id, int> occupied_bodyparts;
    /**
     * Body part encumbered by this bionic, mapped to the amount of encumbrance caused.
     */
    std::map<bodypart_str_id, int> encumbrance;

    /**
     * Fake item created for crafting with this bionic available.
     * Also the item used for gun bionics.
     */
    itype_id fake_item;
    /**
     * Mutations/trait that are removed upon installing this CBM.
     * E.g. enhanced optic bionic may cancel HYPEROPIC trait.
     */
    std::vector<trait_id> canceled_mutations;

    /** bionic enchantments */
    std::vector<enchantment_id> enchantments;

    /**
     * The spells you learn when you install this bionic, and what level you learn them at.
     */
    std::map<spell_id, int> learned_spells;

    /**
     * Additional bionics that are installed automatically when this
     * bionic is installed. This can be used to install several bionics
     * from one CBM item, which is useful as each of those can be
     * activated independently.
     */
    std::vector<bionic_id> included_bionics;

    /**
     * Id of another bionic which this bionic can upgrade.
     */
    bionic_id upgraded_bionic = bionic_id::NULL_ID();
    /**
     * Upgrades available for this bionic (opposite to @ref upgraded_bionic).
     */
    std::set<bionic_id> available_upgrades;
    /**
     * Id of other bionics which this bionic needs to have installed to be installed.
     * Also prevents those bionics from being removed while this bionic is installed.
     */
    std::vector<bionic_id> required_bionics;
    /**
    * Id of other bionics which this bionic prevents from being installed.
    * Also prevents those bionics from being installed while this bionic is installed.
    */
    std::vector<bionic_id> conflicting_bionics;

    bool can_uninstall = true;
    std::string no_uninstall_reason;

    bool starting_bionic = false;
    int points = 0;

    std::set<flag_id> flags;
    bool has_flag( const flag_id &flag ) const;

    // Helper methods for bionic type checking
    bool is_power_source() const;
    bool is_toggled() const;
    bool is_gun() const;
    bool is_weapon() const;

    itype_id itype() const;

    bool is_included( const bionic_id &id ) const;

    static void load_bionic( const JsonObject &jo, const std::string &src );
    static void check_consistency();
    static void finalize_all();
    static std::vector<bionic_data> get_all();
    static void reset();

    bool was_loaded = false;
    void load( const JsonObject &obj, const std::string & );
    void check() const;
    void finalize() const;

    LUA_TYPE_OPS( bionic_data, id );
};

struct bionic {
        bionic_id id;
        int         charge_timer  = 0;
        char        invlet  = 'a';
        bool        powered = false;
        bool        show_sprite = true;
        /* Ammunition actually loaded in this bionic gun in deactivated state */
        itype_id    ammo_loaded = itype_id::NULL_ID();
        /* Ammount of ammo actually held inside by this bionic gun in deactivated state */
        unsigned int         ammo_count = 0;
        /* An amount of time during which this bionic has been rendered inoperative. */
        time_duration        incapacitated_time;
        /* The amount of energy the Bionic has stored for it's function. [Currently only used for ADS]*/
        units::energy        energy_stored = 0_kJ;
        bionic()
            : id( "bio_batteries" ), incapacitated_time( 0_turns ) {
        }
        bionic( bionic_id pid, char pinvlet )
            : id( pid ), invlet( pinvlet ), incapacitated_time( 0_turns ) { }

        const bionic_data &info() const {
            return *id;
        }

        void set_flag( const std::string &flag );
        void remove_flag( const std::string &flag );
        bool has_flag( const std::string &flag ) const;

        int get_quality( const quality_id &quality ) const;

        bool is_this_fuel_powered( const itype_id &this_fuel ) const;
        void toggle_safe_fuel_mod();
        void toggle_auto_start_mod();

        void set_auto_start_thresh( float val );
        float get_auto_start_thresh() const;
        bool is_auto_start_on() const;
        bool is_auto_start_keep_full() const;

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
    private:
        // generic bionic specific flags
        cata::flat_set<std::string> bionic_tags;
        float auto_start_threshold = -1.0;
};

nc_color get_bionic_text_color( const bionic &bio, const bool isHighlightedBionic );
struct bionic_sort_less {
    bionic_ui_sort_mode mode = bionic_ui_sort_mode::NONE;
    bool operator()( const bionic &lhs, const bionic &rhs ) const;
    bool operator()( const bionic *lhs, const bionic *rhs ) const {
        return lhs && rhs && operator()( *lhs, *rhs );
    }
};

// A simpler wrapper to allow forward declarations of it. std::vector can not
// be forward declared without a *definition* of bionic, but this wrapper can.
class bionic_collection : public std::vector<bionic>
{
};

/**List of bodyparts occupied by a bionic*/
std::vector<bodypart_id> get_occupied_bodyparts( const bionic_id &bid );

char get_free_invlet( bionic_collection &bionics );
std::string list_occupied_bps( const bionic_id &bio_id, const std::string &intro,
                               bool each_bp_on_new_line = true );

/// Checks if Character needs anesthesia at all
bool cbm_needs_anesthesia( const Character &who );

/// Has enough anesthetic for surgery
bool has_enough_anesthesia( const itype *cbm, Character &doc,
                            const Character &patient );

int bionic_manip_cos( float adjusted_skill, int bionic_difficulty );

std::vector<bionic_id> bionics_cancelling_trait( const std::vector<bionic_id> &bios,
        const trait_id &tid );


