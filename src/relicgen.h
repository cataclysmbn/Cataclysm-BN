#pragma once

#include <string>
#include <vector>

#include "coordinates.h"
#include "json_source_location.h"
#include "magic/magic.h"
#include "enchantments/enchantment.h"
#include "translations.h"
#include "relic.h"
#include "weighted_list.h"

class Creature;
class JsonIn;
class JsonObject;
class JsonOut;

class relicgen_part
{
    public:
        void load( const JsonObject &jo );
        void finalize();
        void check() const;
        // Integer cost; Can be negative, how much this gives or takes from the relic's cost
        // When cost > remaining power when randomly chosen, relicgen stops
        int cost;
        // Relic recharges to add;
        std::vector<relic_recharge> recharges;
        // Activate spells to add;
        std::vector<fake_spell> actives;
        // Adds most things relics can add with the following excemptions:
        // Mutations: Anything mutations can do enchantments should be given the abiity to
        // Emitter: Current limitation with enchantments is there can only be one emitter; Need to hide that
        // See enchantments for what these do, we are purely setting an enchantment from this.
        std::map<efftype_id, int> ench_effects;
        std::set<itype_id> fake_items;
        std::map<enchantment_value_id, int> values_add;
        std::map<enchantment_value_id, double> values_multiply;
        std::map<enchantment_value_id, int> values_max;
        std::vector<fake_spell> hit_me_effect;
        std::vector<fake_spell> hit_you_effect;
        std::map<time_duration, std::vector<fake_spell>> intermittent_activation;
        std::set<enchantment_condition_id> conditions;
        std::set<efftype_id> immune_effects;
        std::set<field_type_id> immune_fields;
        std::map<enchantment_flag_id, int> flags;
        std::vector<enchantment_vision_id> special_visions;
};
class relicgen
{
    public:
        // Universal loading stuff
        static void load_relicgens( const JsonObject &jo, const std::string &src );

        void load( const JsonObject &jo, const std::string &src );

        static void check_consistency();

        void check() const;

        static void finalize_all();
        void finalize();

        static auto get_all() -> std::vector<relicgen>;

        static void reset();

        // Generic Factory Stuff
        relicgen_id id;
        bool was_loaded = false;

        // Minimum and maximum power level of the artifact, used for above cost
        int min_power;
        int max_power;

        // Relicgen part choices
        weighted_int_list<relicgen_part> effects;
        weighted_int_list<itype_id> itypes;
        weighted_int_list<translation> name_prefix;
        weighted_int_list<translation> name_postfix;

        detached_ptr<item> generate_relic() const;
        relic generate_relicdata() const;
};

