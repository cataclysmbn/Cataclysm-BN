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
        void add_to_relic( relic &relic, int &points_left );
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
        // Enchantments;
        std::vector<enchantment> enchantments;
    private:
        void add_enchantment( enchantment &ench );
};

class relicgen_set
{
    public:
        int min_count;
        int max_count;
        void add_to_relic( relic &relic, int &points_left );
        void load( const JsonObject &jo );
        void finalize();
        void check() const;
        weighted_int_list<relicgen_part> effects;
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
        std::vector<relicgen_set> effects;
        weighted_int_list<itype_id> itypes;
        weighted_int_list<translation> name_prefix;
        weighted_int_list<translation> name_postfix;

        detached_ptr<item> generate_relic() const;
        relic generate_relicdata() const;
};

