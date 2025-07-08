#pragma once

#include <array>
#include <map>
#include <vector>
#include <string>

#include "enum_traits.h"
#include "type_id.h"

class item;
class monster;
class JsonObject;
class JsonArray;
class JsonIn;

enum body_part : int;

enum damage_type : int {
    DT_NULL = 0, // null damage, doesn't exist
    DT_TRUE, // typeless damage, should always go through
    DT_BIOLOGICAL, // internal damage, like from smoke or poison
    DT_BASH, // bash damage
    DT_CUT, // cut damage
    DT_ACID, // corrosive damage, e.g. acid
    DT_STAB, // stabbing/piercing damage
    DT_HEAT, // e.g. fire, plasma
    DT_COLD, // e.g. heatdrain, cryogrenades
    DT_DARK, // e.g. eldritch blast from D&D, death coil from Warcraft
    DT_LIGHT, // e.g. holy magic that damages undead, Holy light from Warcraft
    DT_PSI, // e.g. mental magic from Spellforce, Mind Spike from D&D
    DT_ELECTRIC, // e.g. electrical discharge
    DT_BULLET, // bullets and other fast moving projectiles
    NUM_DT
};

template<>
struct enum_traits<damage_type> {
    static constexpr damage_type last = NUM_DT;
};

struct damage_unit {
    damage_type type;
    float amount;
    float res_pen;
    float res_mult;
    float damage_multiplier;

    damage_unit( damage_type dt, float amt, float arpen = 0.0f,
                 float armor_mult = 1.0f, float dmg_mult = 1.0f ) :
        type( dt ), amount( amt ), res_pen( arpen ), res_mult( armor_mult ), damage_multiplier( dmg_mult ) { }

    bool operator==( const damage_unit &other ) const;

    /** Return damage_type as a human-readable string */
    const std::string get_name() const;
};

// a single atomic unit of damage from an attack. Can include multiple types
// of damage at different armor mitigation/penetration values
struct damage_instance {
    std::vector<damage_unit> damage_units;
    damage_instance();
    static damage_instance physical( float bash, float cut, float stab, float arpen = 0.0f );
    damage_instance( damage_type dt, float amt, float arpen = 0.0f,
                     float arpen_mult = 1.0f, float dmg_mult = 1.0f );
    void mult_damage( double multiplier, bool pre_armor = false );
    float type_damage( damage_type dt ) const;
    float total_damage() const;
    void clear();
    bool empty() const;

    std::vector<damage_unit>::iterator begin();
    std::vector<damage_unit>::const_iterator begin() const;
    std::vector<damage_unit>::iterator end();
    std::vector<damage_unit>::const_iterator end() const;

    bool operator==( const damage_instance &other ) const;

    /**
     * Adds damage to the instance.
     * If the damage type already exists in the instance, the old and new instance are normalized.
     * The normalization means that the effective damage can actually decrease (depending on target's armor).
     */
    /*@{*/
    void add_damage( damage_type dt, float amt, float arpen = 0.0f,
                     float arpen_mult = 1.0f, float dmg_mult = 1.0f );
    void add( const damage_instance &added_di );
    void add( const damage_unit &new_du );
    /*@}*/

    /**
     * Return the armour penetration value for a particular damage type
     * If the damage_instance has no such damage, return default value of 0
     */
    float get_armor_pen( damage_type dt ) const;
    /**
     * Return the armour multiplier value for a particular damage type
     * If the damage_instance has no such damage, return default value of 1
     */
    float get_armor_mult( damage_type dt ) const;
    /**
     * @return true if at least one damage_unit in this damage_instance has armour penetration or multiplier different from the default
     */
    bool has_armor_piercing() const;

    void deserialize( JsonIn & );
};

struct dealt_damage_instance {
    std::array<int, NUM_DT> dealt_dams;
    bodypart_str_id bp_hit;

    dealt_damage_instance();
    void set_damage( damage_type dt, int amount );
    int type_damage( damage_type dt ) const;
    int total_damage() const;
};

struct resistances {
    std::map<damage_type, float> flat;

    resistances();

    // If to_self is true, we want armor's own resistance, not one it provides to wearer
    resistances( const item &armor, bool to_self = false );
    void set_resist( damage_type dt, float amount );
    float type_resist( damage_type dt ) const;

    float get_effective_resist( const damage_unit &du ) const;

    resistances combined_with( const resistances &other ) const;
};

const std::map<std::string, damage_type> &get_dt_map();
damage_type dt_by_name( const std::string &name );
std::string name_by_dt( const damage_type &dt );

const skill_id &skill_by_dt( damage_type dt );

damage_instance load_damage_instance( const JsonObject &jo );
damage_instance load_damage_instance( const JsonArray &jarr );

damage_instance load_damage_instance_inherit( const JsonObject &jo, const damage_instance &parent );
damage_instance load_damage_instance_inherit( const JsonArray &jarr,
        const damage_instance &parent );

resistances load_resistances_instance( const JsonObject &jo );

// Returns damage or resistance data
// Handles some shorthands
std::map<damage_type, float> load_damage_map( const JsonObject &jo );


