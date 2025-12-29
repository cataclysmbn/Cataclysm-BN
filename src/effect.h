#pragma once

#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bodypart.h"
#include "calendar.h"
#include "catalua_type_operators.h"
#include "flat_set.h"
#include "hash_utils.h"
#include "translations.h"
#include "type_id.h"

class Character;

enum game_message_type : int;
class JsonIn;
class JsonObject;
class JsonOut;
class effect;

/** Handles the large variety of weed messages. */
void weed_msg( Character &who );

enum effect_rating {
    e_good,     // The effect is good for the one who has it.
    e_neutral,  // There is no effect or the effect is very nominal. This is the default.
    e_bad,      // The effect is bad for the one who has it.
    e_mixed     // The effect has good and bad parts to the one who has it.
};

struct caused_effect {
    public:
        efftype_id type;
        /** Minimum parent effect intensity to apply the new effect. */
        int intensity_requirement = 0;
        /** If false, prevents application if the trigger was parent decaying to 0 duration. */
        bool allow_on_decay = true;
        /** If false, prevents application if the trigger was parent being removed with at least 1 turn of duration left. */
        bool allow_on_remove = false;

        /**
         * Duration of the new effect.
         * If 0, type's max duration will be used instead.
         * Should be left at 0 for permanent effects.
         */
        time_duration duration = 0_turns;
        /**
         * If true, duration field is ignored and parent intensity is copied.
         * If true and parent duration was <1, the new effect will not be applied.
         */
        bool inherit_duration = false;
        /** Intensity of the new effect. */
        int intensity = 0;
        /** If true, intensity field is ignored and parent effect intensity is copied. */
        bool inherit_intensity = false;

        bodypart_str_id bp = bodypart_str_id::NULL_ID();
        bool inherit_body_part = true;

        void load_decay( const JsonObject &jo );

        auto tie() const {
            return std::tie( type, intensity_requirement, allow_on_decay, allow_on_remove,
                             duration, inherit_duration, intensity, inherit_intensity );
        }

        bool operator==( const caused_effect &rhs ) const {
            return tie() == rhs.tie();
        }
    private:
        void load( const JsonObject &jo );
};


class effect_type
{
        friend void load_effect_type( const JsonObject &jo );
        friend class effect;
    public:
        effect_type() = default;

        efftype_id id;

        /** Returns if an effect is good or bad for message display. */
        effect_rating get_rating() const;

        /** Returns looks_like. */
        std::string get_looks_like() const;
        /** Returns true if there is a listed name in the JSON entry for each intensity from
         *  1 to max_intensity. */
        bool use_name_ints() const;
        /** Returns true if there is a listed description in the JSON entry for each intensity
         *  from 1 to max_intensity with the matching reduced value. */
        bool use_desc_ints( bool reduced ) const;

        /** Returns the appropriate game_message_type when a new effect is obtained. This is equal to
         *  an effect's "rating" value. */
        game_message_type gain_game_message_type() const;
        /** Returns the appropriate game_message_type when an effect is lost. This is opposite to
         *  an effect's "rating" value. */
        game_message_type lose_game_message_type() const;

        /** Returns the message displayed when a new effect is obtained. */
        std::string get_apply_message() const;
        /** Returns the memorial log added when a new effect is obtained. */
        std::string get_apply_memorial_log() const;
        /** Returns the message displayed when an effect is removed. */
        std::string get_remove_message() const;
        /** Returns the memorial log added when an effect is removed. */
        std::string get_remove_memorial_log() const;
        /** Returns the effect's description displayed when character conducts blood analysis. */
        std::string get_blood_analysis_description() const;

        /** Returns true if an effect will only target main body parts (i.e., those with HP). */
        bool get_main_parts() const;
        /** Returns the maximum duration of an effect. */
        time_duration get_max_duration() const;
        /** Returns the number of turns it takes for the intensity to fall by 1 or 0 if intensity isn't based on duration. */
        time_duration get_int_dur_factor() const;

        /** Returns the id of morale type this effect produces. */
        morale_type get_morale_type() const;

        const std::vector<caused_effect> &get_effects_on_remove() const {
            return effects_on_remove;
        }

        bool is_show_in_info() const;

        /** Returns true if an effect is permanent, i.e. it's duration does not decrease over time. */
        bool is_permanent() const;

        /** Loading helper functions */
        bool load_mod_data( const JsonObject &jo, const std::string &member );
        bool load_miss_msgs( const JsonObject &jo, const std::string &member );
        bool load_decay_msgs( const JsonObject &jo, const std::string &member );

        /** Registers the effect in the global map */
        static void register_ma_buff_effect( const effect_type &eff );

        /** Check if the effect type has the specified flag */
        bool has_flag( const flag_id &flag ) const;

        static void check_consistency();

        LUA_TYPE_OPS( effect_type, id );

    private:
        bool permanent = false;

    protected:
        int max_intensity = 0;
        int max_effective_intensity = 0;
        time_duration max_duration = 0_turns;
        std::string looks_like;

        int dur_add_perc = 0;
        int int_add_val = 0;

        int int_decay_step = 0;
        int int_decay_tick = 0 ;
        time_duration int_dur_factor = 0_turns;

        std::set<flag_id> flags;

        bool main_parts_only = false;

        // Determines if effect should be shown in description.
        bool show_in_info = false;

        std::vector<trait_id> resist_traits;
        std::vector<efftype_id> resist_effects;
        std::vector<efftype_id> removes_effects;
        std::vector<efftype_id> blocks_effects;

        std::vector<std::pair<std::string, int>> miss_msgs;

        bool pain_sizing = false;
        bool hurt_sizing = false;
        bool harmful_cough = false;
        // TODO: Once addictions are JSON-ized it should be trivial to convert this to a
        // "generic" addiction reduces value
        bool pkill_addict_reduces = false;
        // This flag is hard-coded for specific IDs now
        // It needs to be set for monster::move_effects
        bool impairs_movement = false;

        std::vector<translation> name;
        std::string speed_mod_name;
        std::vector<std::string> desc;
        std::vector<std::string> reduced_desc;
        bool part_descs = false;

        std::vector<std::pair<std::string, game_message_type>> decay_msgs;

        effect_rating rating = effect_rating::e_neutral;

        std::string apply_message;
        std::string apply_memorial_log;
        std::string remove_message;
        std::string remove_memorial_log;

        std::string blood_analysis_description;

        morale_type morale;

        std::vector<caused_effect> effects_on_remove;

        /** Key tuple order is:("base_mods"/"scaling_mods", reduced: bool, type of mod: "STR", desired argument: "tick") */
        std::unordered_map <
        std::tuple<std::string, bool, std::string, std::string>, double, cata::tuple_hash > mod_data;
};

class effect
{
    public:
        effect() : eff_type( nullptr ), duration( 0_turns ), bp(),
            intensity( 1 ), start_time( calendar::turn_zero ),
            removed( true ) {
        }
        effect( const effect_type *peff_type, const time_duration &dur,
                const bodypart_str_id &part, int nintensity, const time_point &nstart_time ) :
            eff_type( peff_type ), duration( dur ), bp( part ),
            intensity( nintensity ), start_time( nstart_time ),
            removed( false ) {
        }
        effect( const effect & ) = default;
        effect &operator=( const effect & ) = default;

        /** Returns true if the effect is the result of `effect()`, ie. an effect that doesn't exist. */
        bool is_null() const;
        operator bool() const {
            return !is_null() && !is_removed();
        }

        /** Dummy used for "reference to effect()" */
        static effect null_effect;

        /** Returns the name displayed in the player status window. */
        std::string disp_name() const;
        /** Returns the description displayed in the player status window. */
        std::string disp_desc( bool reduced = false ) const;
        /** Returns the short description as set in json. */
        std::string disp_short_desc( bool reduced = false ) const;
        /** Returns true if a description will be formatted as "Your" + body_part + description. */
        bool use_part_descs() const;

        /** Returns the effect's matching effect_type. */
        const effect_type *get_effect_type() const;

        /** Decays effect durations, returning true if duration <= 0.
         *  This is called in the middle of a loop through all effects, which is
         *  why we aren't allowed to remove the effects here. */
        bool decay( const time_point &time, bool player );

        /** Returns the remaining duration of an effect. */
        time_duration get_duration() const;
        /** Returns the maximum duration of an effect. */
        time_duration get_max_duration() const;
        /** Sets the duration, capping at max_duration if it exists. */
        void set_duration( const time_duration &dur, bool alert = false );
        /** Mods the duration, capping at max_duration if it exists. */
        void mod_duration( const time_duration &dur, bool alert = false );
        /** Multiplies the duration, capping at max_duration if it exists. */
        void mult_duration( double dur, bool alert = false );

        /** Returns the turn the effect was applied. */
        time_point get_start_time() const;

        /** Returns the targeted body_part of the effect. This is NULL_ID for untargeted effects. */
        const bodypart_str_id &get_bp() const;

        /** Returns true if an effect is permanent, i.e. it's duration does not decrease over time. */
        bool is_permanent() const;

        /** Returns the intensity of an effect. */
        int get_intensity() const;
        /** Returns the maximum intensity of an effect. */
        int get_max_intensity() const;

        /**
         * Sets intensity of effect capped by range [1..max_intensity]
         * @param val Value to set intensity to
         * @param alert whether decay messages should be displayed
         * @return new intensity of the effect after val subjected to above cap
         */
        int set_intensity( int val, bool alert = false );

        /**
         * Modify intensity of effect capped by range [1..max_intensity]
         * @param mod Amount to increase current intensity by
         * @param alert whether decay messages should be displayed
         * @return new intensity of the effect after modification and capping
         */
        int mod_intensity( int mod, bool alert = false );

        /**
         * Returns if the effect is disabled and set up for removal.
         */
        bool is_removed() const {
            return removed;
        }
        void set_removed() {
            removed = true;
        }

        /** Returns the string id of the resist trait to be used in has_trait("id"). */
        const std::vector<trait_id> &get_resist_traits() const;
        /** Returns the string id of the resist effect to be used in has_effect("id"). */
        const std::vector<efftype_id> &get_resist_effects() const;
        /** Returns the string ids of the effects removed by this effect to be used in remove_effect("id"). */
        const std::vector<efftype_id> &get_removes_effects() const;
        /** Returns the string ids of the effects blocked by this effect to be used in add_effect("id"). */
        std::vector<efftype_id> get_blocks_effects() const;

        /** Returns the matching modifier type from an effect, used for getting actual effect effects. */
        int get_mod( const std::string &arg, bool reduced = false ) const;
        /** Returns the average return of get_mod for a modifier type. Used in effect description displays. */
        int get_avg_mod( const std::string &arg, bool reduced = false ) const;
        /** Returns the amount of a modifier type applied when a new effect is first added. */
        int get_amount( const std::string &arg, bool reduced = false ) const;
        /** Returns the minimum value of a modifier type that get_mod() and get_amount() will push the player to. */
        int get_min_val( const std::string &arg, bool reduced = false ) const;
        /** Returns the maximum value of a modifier type that get_mod() and get_amount() will push the player to. */
        int get_max_val( const std::string &arg, bool reduced = false ) const;
        /** Returns true if the given modifier type's trigger chance is affected by size mutations. */
        bool get_sizing( const std::string &arg ) const;
        /** Returns the approximate percentage chance of a modifier type activating on any given tick, used for descriptions. */
        double get_percentage( const std::string &arg, int val, bool reduced = false ) const;
        /** Checks to see if a given modifier type can activate, and performs any rolls required to do so. mod is a direct
         *  multiplier on the overall chance of a modifier type activating. */
        bool activated( const time_point &when, const std::string &arg, int val,
                        bool reduced = false, double mod = 1 ) const;

        /** Check if the effect has the specified flag */
        bool has_flag( const flag_id &flag ) const;

        /** Returns the modifier caused by addictions. Currently only handles painkiller addictions. */
        double get_addict_mod( const std::string &arg, int addict_level ) const;
        /** Returns true if the coughs caused by an effect can harm the player directly. */
        bool get_harmful_cough() const;
        /** Returns the percentage value by further applications of existing effects' duration is multiplied by. */
        int get_dur_add_perc() const;
        /** Returns the number of turns it takes for the intensity to fall by 1 or 0 if intensity isn't based on duration. */
        time_duration get_int_dur_factor() const;
        /** Returns the amount an already existing effect intensity is modified by further applications of the same effect. */
        int get_int_add_val() const;

        /** Returns a vector of the miss message messages and chances for use in add_miss_reason() while the effect is in effect. */
        std::vector<std::pair<std::string, int>> get_miss_msgs() const;

        /** Returns the value used for display on the speed modifier window in the player status menu. */
        std::string get_speed_name() const;

        /** Returns if the effect is supposed to be handled in Creature::movement */
        bool impairs_movement() const;

        /** Create a set of effects that should replace this one when it decays to 0 duration. */
        std::vector<effect> create_decay_effects() const;
        /** Create a set of effects that should replace this one when it is removed prematurely. */
        std::vector<effect> create_removal_effects() const;

        /** Returns the effect's matching effect_type id. */
        const efftype_id &get_id() const {
            return eff_type->id;
        }

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

    private:
        std::vector<effect> create_child_effects( bool decay ) const;

    protected:
        const effect_type *eff_type;
        time_duration duration;
        bodypart_str_id bp;
        int intensity;
        time_point start_time;
        bool removed;

        // TODO: REMOVE!
        bool permanent = false;
    public:
        /**
         * Legacy compatibility TODO: Remove
         * No set un-permanent, because no use case
         */
        void set_permanent();

};

void load_effect_type( const JsonObject &jo );
void reset_effect_types();

std::vector<efftype_id> find_all_effect_types();

std::string texitify_base_healing_power( int power );
std::string texitify_healing_power( int power );

// Inheritance here allows forward declaration of the map in class Creature.
class effects_map : public
    std::unordered_map<efftype_id, std::unordered_map<bodypart_str_id, effect>>
{
};


