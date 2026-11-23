#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "calendar.h"
#include "catalua_type_operators.h"
#include "coordinates.h"
#include "io_tags.h"
#include "monster.h"
#include "point.h"
#include "type_id.h"

class JsonIn;
class JsonObject;
class JsonOut;
// from overmap.h
class overmap;
struct MonsterGroupEntry;

using FreqDef = std::vector<MonsterGroupEntry>;
using FreqDef_iter = FreqDef::iterator;

struct MonsterGroupEntry {
    mtype_id name;
    mongroup_id monster_group;
    int frequency;
    int cost_multiplier;
    int pack_minimum = 1;
    int pack_maximum = 1;
    std::vector<std::string> conditions;
    time_duration starts;
    time_duration ends;
    bool lasts_forever() const {
        return ends <= 0_turns;
    }

    MonsterGroupEntry() = default;
    MonsterGroupEntry( const mtype_id &id, const mongroup_id &mgroup_id, int new_freq, int new_cost,
                       int new_pack_min, int new_pack_max, const time_duration &new_starts, const time_duration &new_ends )
        : name( id )
        , monster_group( mgroup_id )
        , frequency( new_freq )
        , cost_multiplier( new_cost )
        , pack_minimum( new_pack_min )
        , pack_maximum( new_pack_max )
        , starts( new_starts )
        , ends( new_ends ) {
    }

    LUA_TYPE_OPS( MonsterGroupEntry, name );
};

struct MonsterGroupResult {
    mtype_id name;
    int pack_size;

    MonsterGroupResult() : name( mtype_id::NULL_ID() ), pack_size( 0 ) {
    }

    MonsterGroupResult( const mtype_id &id, int new_pack_size )
        : name( id ), pack_size( new_pack_size ) {
    }
};

class MonsterGroup
{
    public:
        // Generic Factory
        mongroup_id id;
        bool was_loaded = false;

        // Data
        mtype_id defaultMonster;
        FreqDef  monsters;
        int evolve_chance = 0;
        int evolve_repeat = 0;
        bool IsMonsterInGroup( const mtype_id &id ) const;
        bool is_animal = false;
        // replaces this group after a period of
        // time when exploring an unexplored portion of the map
        bool replace_monster_group = false;
        mongroup_id new_monster_group = mongroup_id::NULL_ID();
        time_duration monster_group_time = 0_turns;
        bool is_safe = false; /// Used for @ref mongroup::is_safe()
        int freq_total = 0; // Default 1000 unless specified - max number to roll for spawns

        LUA_TYPE_OPS( MonsterGroup, id );

        void load( const JsonObject &jo, const std::string &src );
        void finalize();
        void check() const;
        static void load_monster_groups( const JsonObject &jo, const std::string &src );
        static void finalize_all();
        static void check_consistency();
        static void reset();

        static std::vector<MonsterGroup> get_all();
};

struct mongroup {
    mongroup_id type;
    tripoint_abs_sm abs_pos; // position of the mongroup in absolute submap coordinates
    unsigned int radius = 1;
    unsigned int population = 1;
    tripoint_abs_sm target; // location the horde is interested in.
    // TODO(cross-dim-alias): tripoint_abs_sm has no dimension field.  A nemesis
    // horde chasing a player who dimension-travels loses the dimension context of
    // the target.  Full fix requires attaching a dimension_id to the target or
    // storing it separately on mongroup.  Acceptable limitation for this PR.
    tripoint_abs_sm nemesis_target; // abs target for nemesis hordes
    int interest = 0; //interest to target in percents
    bool dying = false;
    bool horde = false;
    /** This property will be ignored if the vector is empty.
     *  Otherwise it will keep track of the individual monsters that
     *  are contained in this horde, and the population property will
     *  be ignored instead.
     */
    std::vector<monster> monsters;

    /** There are two types of hordes: "city", who try to stick around cities
     *  and return to them whenever possible.
     *  And "roam", who roam around the map randomly, not taking care to return
     *  anywhere.
     */
    std::string horde_behaviour;
    bool diffuse = false;   // group size ind. of dist. from center and radius invariant
    mongroup( const mongroup_id &ptype, const tripoint_abs_sm &ppos,
              unsigned int prad, unsigned int ppop )
        : type( ptype )
        , abs_pos( ppos )
        , radius( prad )
        , population( ppop ) {
    }
    mongroup( const std::string &ptype, const tripoint_abs_sm &ppos, unsigned int prad,
              unsigned int ppop,
              tripoint_abs_sm ptarget, int pint, bool pdie, bool phorde, bool pdiff ) :
        type( ptype ), abs_pos( ppos ), radius( prad ), population( ppop ), target( ptarget ),
        interest( pint ), dying( pdie ), horde( phorde ), diffuse( pdiff ) { }
    mongroup() = default;
    LUA_TYPE_OPS( mongroup, type );
    bool is_safe() const;
    bool empty() const;
    void clear();
    auto set_target( const tripoint_abs_sm &p ) -> void {
        target = p;
    }
    auto set_nemesis_target( const tripoint_abs_sm &p ) -> void {
        nemesis_target = p;
    }
    void wander( const overmap & );
    void inc_interest( int inc ) {
        interest += inc;
        if( interest > 100 ) {
            interest = 100;
        }
    }
    void dec_interest( int dec ) {
        interest -= dec;
        if( interest < 15 ) {
            interest = 15;
        }
    }
    void set_interest( int set ) {
        if( set < 15 ) {
            set = 15;
        }
        if( set > 100 ) {
            set = 100;
        }
        interest = set;
    }
    float avg_speed() const;

    template<typename Archive>
    void io( Archive & );
    using archive_type_tag = io::object_archive_tag;

    void deserialize( JsonIn &data );
    void deserialize_legacy( JsonIn &json );
    void serialize( JsonOut &json ) const;
};

class MonsterGroupManager
{
    public:
        static void LoadMonsterBlacklist( const JsonObject &jo );
        static void LoadMonsterWhitelist( const JsonObject &jo );
        static std::vector<MonsterGroupResult> GetResultFromGroup( const mongroup_id &group,
                int *quantity = nullptr,
                bool use_pack_size = false, bool from_subgroup = false );
        static bool IsMonsterInGroup( const mongroup_id &group, const mtype_id &monster );
        static bool isValidMonsterGroup( const mongroup_id &group );
        static const mongroup_id &Monster2Group( const mtype_id &monster );
        static std::vector<mtype_id> GetMonstersFromGroup( const mongroup_id &group );
        static const MonsterGroup &GetMonsterGroup( const mongroup_id &group );
        static const MonsterGroup &GetUpgradedMonsterGroup( const mongroup_id &group );
        static std::vector<mongroup_id> get_all_group_ids();
        /**
         * Gets a random monster, weighted by frequency.
         * Ignores cost multiplier.
         */
        static const mtype_id &GetRandomMonsterFromGroup( const mongroup_id &group );

        static bool monster_is_blacklisted( const mtype_id &m );

        static bool is_animal( const mongroup_id &group );
};
