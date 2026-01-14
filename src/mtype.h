#pragma once

#include <map>
#include <optional>
#include <set>
#include <array>
#include <string>
#include <variant>
#include <vector>

#include "behavior.h"
#include "calendar.h"
#include "color.h"
#include "cursesdef.h"
#include "damage.h"
#include "enum_bitset.h"
#include "enums.h"
#include "mattack_common.h"
#include "legacy_pathfinding.h"
#include "pathfinding.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "catalua_type_operators.h"

class Creature;
class monster;
struct dealt_projectile_attack;
struct species_type;
template <typename E> struct enum_traits;

enum body_part : int;
enum creature_size : int;

using mon_action_death  = void ( * )( monster & );
using mon_action_attack = bool ( * )( monster * );
using mon_action_defend = void ( * )( monster &, Creature *, dealt_projectile_attack const * );
using bodytype_id = std::string;
using TraitSet = std::set<trait_id>;
class JsonArray;
class JsonObject;

// These are triggers which may affect the monster's anger or morale.
// They are handled in monster::check_triggers(), in monster.cpp
enum class mon_trigger : int {
    STALK,              // Increases when following the player
    MEAT,               // Meat or a corpse nearby
    HOSTILE_WEAK,       // Hurt hostile player/npc/monster seen
    HOSTILE_CLOSE,      // Hostile creature within a few tiles
    HURT,               // We are hurt
    FIRE,               // Fire nearby
    FRIEND_DIED,        // A monster of the same type died
    FRIEND_ATTACKED,    // A monster of the same type attacked
    SOUND,              // Heard a sound
    PLAYER_NEAR_BABY,   // Player/npc is near a baby monster of this type
    MATING_SEASON,      // It's the monster's mating season (defined by baby_flags)
    NETHER_ATTENTION,   // Player/npc currently has effect_attention

    _LAST               // This item must always remain last.
};

template<>
struct enum_traits<mon_trigger> {
    static constexpr auto last = mon_trigger::_LAST;
};

// Feel free to add to m_flags.  Order shouldn't matter, just keep it tidy!
// And comment them well. ;)
// TODO: And rename them to 'mon_flags'
// TODO: And turn them into an enum class (like mon_trigger).
enum m_flag : int {
    MF_SEES,                // It can see you (and will run/follow)
    MF_HEARS,               // It can hear you
    MF_GOODHEARING,         // Pursues sounds more than most monsters
    MF_SMELLS,              // It can smell you
    MF_KEENNOSE,            //Keen sense of smell
    MF_STUMBLES,            // Stumbles in its movement
    MF_WARM,                // Warm blooded
    MF_NEMESIS,             // Is a nemesis monster
    MF_NOHEAD,              // Headshots not allowed!
    MF_HARDTOSHOOT,         // It's one size smaller for ranged attacks, no less then creature_size::tiny
    MF_GRABS,               // Its attacks may grab us!
    MF_BASHES,              // Bashes down doors
    MF_DESTROYS,            // Bashes down walls and more
    MF_BORES,               // Tunnels through just about anything
    MF_POISON,              // Poisonous to eat
    MF_VENOM,               // Attack may poison the player
    MF_BADVENOM,            // Attack may SEVERELY poison the player
    MF_PARALYZE,            // Attack may paralyze the player with venom
    MF_BLEED,               // Causes player to bleed
    MF_WEBWALK,             // Doesn't destroy webs
    MF_DIGS,                // Digs through the ground
    MF_CAN_DIG,             // Can dig and walk
    MF_FLIES,               // Can fly (over water, etc)
    MF_AQUATIC,             // Confined to water
    MF_SWIMS,               // Treats water as 50 movement point terrain
    MF_ATTACKMON,           // Attacks other monsters
    MF_ANIMAL,              // Is an "animal" for purposes of the Animal Empath trait
    MF_PLASTIC,             // Absorbs physical damage to a great degree
    MF_SUNDEATH,            // Dies in full sunlight
    MF_ELECTRIC,            // Shocks unarmed attackers
    MF_ACIDPROOF,           // Immune to acid
    MF_ACIDTRAIL,           // Leaves a trail of acid
    MF_SHORTACIDTRAIL,      // Leaves an intermittent trail of acid
    MF_FIREPROOF,           // Immune to fire
    MF_SLUDGEPROOF,         // Ignores the effect of sludge trails
    MF_SLUDGETRAIL,         // Causes monster to leave a sludge trap trail when moving
    MF_COLDPROOF,           // Immune to cold damage
    MF_BIOPROOF,            // Immune to biological damage
    MF_DARKPROOF,           // Immune to dark damage
    MF_LIGHTPROOF,          // Immune to light damage
    MF_PSIPROOF,            // Immune to psionic damage
    MF_FIREY,               // Burns stuff and is immune to fire
    MF_QUEEN,               // When it dies, local populations start to die off too
    MF_ELECTRONIC,          // e.g. a robot; affected by EMP blasts, and other stuff
    MF_FUR,                 // May produce fur when butchered
    MF_LEATHER,             // May produce leather when butchered
    MF_WOOL,                // May produce wool when butchered
    MF_FEATHER,             // May produce feather when butchered
    MF_BONES,               // May produce bones and sinews when butchered; if combined with POISON flag, tainted bones, if combined with HUMAN, human bones
    MF_FAT,                 // May produce fat when butchered; if combined with POISON flag, tainted fat
    MF_CONSOLE_DESPAWN,     // Despawns when a nearby console is properly hacked
    MF_IMMOBILE,            // Doesn't move (e.g. turrets)
    MF_ID_CARD_DESPAWN,     // Despawns when a science ID card is used on a nearby console
    MF_RIDEABLE_MECH,       // A rideable mech that is immobile until ridden.
    MF_CARD_OVERRIDE,        // Not a mech, but can be converted to friendly using an ID card in the same way that mechs can.
    MF_MILITARY_MECH,       // Makes rideable mechs and card-scanner bots require a military ID instead of industrial to convert to friendly.
    MF_MECH_RECON_VISION,   // This mech gives you IR night-vision.
    MF_MECH_DEFENSIVE,      // This mech gives you thorough protection.
    MF_HIT_AND_RUN,         // Flee for several turns after a melee attack
    MF_GUILT,               // You feel guilty for killing it
    MF_PAY_BOT,             // You can pay this bot to be your friend for a time
    MF_HUMAN,               // It's a live human, as long as it's alive
    MF_NO_BREATHE,          // Creature can't drown and is unharmed by gas, smoke, or poison
    MF_FLAMMABLE,           // Monster catches fire, burns, and spreads fire to nearby objects
    MF_REVIVES,             // Monster corpse will revive after a short period of time
    MF_CHITIN,              // May produce chitin when butchered
    MF_VERMIN,              // Obsolete flag labeling "nuisance" or "scenery" monsters, now used to prevent loading the same.
    MF_NOGIB,               // Creature won't leave gibs / meat chunks when killed with huge damage.
    MF_LARVA,               // Creature is a larva. Currently used for gib and blood handling.
    MF_ARTHROPOD_BLOOD,     // Forces monster to bleed hemolymph.
    MF_ACID_BLOOD,          // Makes monster bleed acid. Fun stuff! Does not automatically dissolve in a pool of acid on death.
    MF_BILE_BLOOD,          // Makes monster bleed bile.
    MF_ABSORBS,             // Consumes objects it moves over which gives bonus hp.
    MF_ABSORBS_SPLITS,      // Consumes objects it moves over which gives bonus hp. If it gets enough bonus HP, it spawns a copy of itself.
    MF_CBM_CIV,             // May produce a common CBM a power CBM when butchered.
    MF_CBM_POWER,           // May produce a power CBM when butchered, independent of MF_CBM_wev.
    MF_CBM_SCI,             // May produce a bionic from bionics_sci when butchered.
    MF_CBM_OP,              // May produce a bionic from bionics_op when butchered, and the power storage is mk 2.
    MF_CBM_TECH,            // May produce a bionic from bionics_tech when butchered.
    MF_CBM_SUBS,            // May produce a bionic from bionics_subs when butchered.
    MF_UNUSED_76,           // !! Unused Flag position, kept for compatability purposes
    MF_FISHABLE,            // It is fishable.
    MF_GROUP_BASH,          // Monsters that can pile up against obstacles and add their strength together to break them.
    MF_SWARMS,              // Monsters that like to group together and form loose packs
    MF_GROUP_MORALE,        // Monsters that are more courageous when near friends
    MF_INTERIOR_AMMO,       // Monster contain's its ammo inside itself, no need to load on launch. Prevents ammo from being dropped on disable.
    MF_CLIMBS,              // Monsters that can climb certain terrain and furniture
    MF_PACIFIST,            // Monsters that will never use melee attack, useful for having them use grab without attacking the player
    MF_PUSH_MON,            // Monsters that can push creatures out of their way
    MF_PUSH_VEH,            // Monsters that can push vehicles out of their way
    MF_NIGHT_INVISIBILITY,  // Monsters that are invisible in poor light conditions
    MF_REVIVES_HEALTHY,     // When revived, this monster has full hitpoints and speed
    MF_NO_NECRO,            // This monster can't be revived by necros. It will still rise on its own.
    MF_AVOID_DANGER_1,      // This monster will path around some dangers instead of through them.
    MF_AVOID_DANGER_2,      // This monster will path around most dangers instead of through them.
    MF_AVOID_FIRE,          // This monster will path around heat-related dangers instead of through them.
    MF_AVOID_FALL,          // This monster will path around cliffs instead of off of them.
    MF_PRIORITIZE_TARGETS,  // This monster will prioritize targets depending on their danger levels
    MF_NOT_HALLU,           // Monsters that will NOT appear when player's producing hallucinations
    MF_CANPLAY,             // This monster can be played with if it's a pet.
    MF_PET_MOUNTABLE,       // This monster can be mounted and ridden when tamed.
    MF_PET_HARNESSABLE,     // This monster can be harnessed when tamed.
    MF_DOGFOOD,             // This monster will respond to the `dog whistle` item.
    MF_MILKABLE,            // This monster is milkable.
    MF_SHEARABLE,           // This monster is shearable.
    MF_NO_BREED,            // This monster doesn't breed, even though it has breed data
    MF_NO_FUNG_DMG,         // This monster can't be fungalized or damaged by fungal spores.
    MF_PET_WONT_FOLLOW,     // This monster won't follow the player automatically when tamed.
    MF_DRIPS_NAPALM,        // This monster ocassionally drips napalm on move
    MF_DRIPS_GASOLINE,      // This monster occasionally drips gasoline on move
    MF_ELECTRIC_FIELD,      // This monster is surrounded by an electrical field that ignites flammable liquids near it
    MF_LOUDMOVES,           // This monster makes move noises as if ~2 sizes louder, even if flying.
    MF_CAN_OPEN_DOORS,      // This monster can open doors.
    MF_STUN_IMMUNE,         // This monster is immune to the stun effect
    MF_DROPS_AMMO,          // This monster drops ammo. Check to make sure starting_ammo paramter is present for this monster type!
    MF_CAN_BE_ORDERED,      // If friendly, allow setting this monster to ignore hostiles and prioritize following the player.
    MF_SMALL_HEAD,          // This monster has a smaller head in proportion to the rest of its body than usual, making it 20% harder to shoot the head. 0.1 -> 0.08
    MF_TINY_HEAD,           // This monster has a tiny head in proportion to the rest of its body, making it 50% hard to shoot the head. 0.1 -> 0.05
    MF_NO_HEAD_BONUS_CRIT,  // This monster can still be hit in the head, but cannot be dealt extra damage past the 1.5 multiplier.
    MF_HEAD_BONUS_MAX_CRIT_1,       // This monster has a vulnerable head, increasing the maximum potential critical multiplier by 0.5 (50%)
    MF_HEAD_BONUS_MAX_CRIT_2,       // This monster has a extremely vulnerable head, increasing the maximum potential critical multiplier by 1.0 (100%)
    MF_TORSO_BONUS_MAX_CRIT_1,      // This monster has a vulnerable torso, increasing the maximum potential critical multiplier by 0.25 (25%)
    MF_TORSO_BONUS_MAX_CRIT_2,      // This monster has a extremely vulnerable torso, increasing the maximum potential critical multiplier by 0.5 (50%)
    MF_PROJECTILE_RESISTANT_1,      // This monster has a torso and limbs that are notably resistant to projectiles, with a default x1 damage mult cap.
    MF_PROJECTILE_RESISTANT_2,      // This monster has a torso and limbs that are very resistant to projectiles, with a default x0.8 damage mult cap.
    MF_PROJECTILE_RESISTANT_3,      // This monster has a torso and limbs that are extremely resistant to projectiles, with a default x0.5 damage mult cap.
    MF_PROJECTILE_RESISTANT_4,      // This monster has a torso and limbs that are almost immune to projectiles, with a default x0.2 damage mult cap.
    MF_VOLATILE,            // This monster tends to explode if hit by fire or bullets, fire weapons will always catch them on fire.
    MF_CANT_CLONE,            // This monster can't be recreated using cloning vats, genome samples will fail if used on it.
    MF_MOUNTABLE_STAIRS,     // When ridden, this monster can go up or down stairs and climb.
    MF_MOUNTABLE_OBSTACLES,     // When ridden, this monster can pass obstacles like fences or doorways when mounted.
    MF_FACTION_MEMORY,      // This monster tracks anger separately per faction

    MF_MAX                  // Sets the length of the flags - obviously must be LAST
};

template<>
struct enum_traits<m_flag> {
    static constexpr m_flag last = m_flag::MF_MAX;
};

/** Used to store monster effects placed on attack */
struct mon_effect_data {
    efftype_id id;
    int duration;
    bool affect_hit_bp;
    body_part bp;
    int chance;
    // TODO: Remove
    bool permanent;

    mon_effect_data( const efftype_id &nid, int dur, bool ahbp, body_part nbp,
                     int nchance, bool perm ) :
        id( nid ), duration( dur ), affect_hit_bp( ahbp ), bp( nbp ),
        chance( nchance ), permanent( perm ) {}
};


/** Pet food data */
struct pet_food_data {
    std::set<std::string> food;
    std::set<TraitSet> tamer_traits;
    std::string pet;
    std::string feed;

    bool was_loaded = false;
    void load( const JsonObject &jo );
    void deserialize( JsonIn &jsin );
};

struct regen_modifier {
    float base_modifier;
    float scale_modifier;
};

struct mtype {
    private:
        friend class MonsterGenerator;
        translation name;
        translation description;

        std::set< const species_type * > species_ptrs;

        enum_bitset<m_flag> flags;

        enum_bitset<mon_trigger> anger;
        enum_bitset<mon_trigger> fear;
        enum_bitset<mon_trigger> placate;

        behavior::node_t goals;

        void add_special_attacks( const JsonObject &jo, const std::string &member_name,
                                  const std::string &src );
        void remove_special_attacks( const JsonObject &jo, const std::string &member_name,
                                     const std::string &src );

        void add_special_attack( JsonArray inner, const std::string &src );
        void add_special_attack( const JsonObject &obj, const std::string &src );

        void add_regeneration_modifiers( const JsonObject &jo, const std::string &member_name,
                                         const std::string &src );
        void remove_regeneration_modifiers( const JsonObject &jo, const std::string &member_name,
                                            const std::string &src );

        void add_regeneration_modifier( const JsonObject &inner, const std::string &src );

    public:
        mtype_id id;

        std::map<itype_id, int> starting_ammo; // Amount of ammo the monster spawns with.
        // Name of item group that is used to create item dropped upon death, or empty.
        item_group_id death_drops;

        /** Stores effect data for effects placed on attack */
        std::vector<mon_effect_data> atk_effs;

        /** Mod origin */
        std::vector<std::pair<mtype_id, mod_id>> src;

        std::set<species_id> species;
        std::set<std::string> categories;
        std::vector<material_id> mat;
        /** UTF-8 encoded symbol, should be exactly one cell wide. */
        std::string sym;
        /** hint for tilesets that don't have a tile for this monster */
        std::string looks_like;
        mfaction_id default_faction;
        bodytype_id bodytype;
        nc_color color = c_white;
        creature_size size;
        units::volume volume;
        units::mass weight;
        phase_id phase;

        int difficulty = 0;     /** many uses; 30 min + (diff-3)*30 min = earliest appearance */
        // difficulty from special attacks instead of from melee attacks, defenses, HP, etc.
        int difficulty_base = 0;
        int hp = 0;
        int speed = 0;          /** e.g. human = 100 */
        int agro = 0;           /** chance will attack [-100,100] */
        int morale = 0;         /** initial morale level at spawn */

        // Number of hitpoints regenerated per turn.
        int regenerates = 0;
        // Effects that can modify regeneration
        std::map<efftype_id, regen_modifier> regeneration_modifiers;
        // Monster regenerates very quickly in poorly lit tiles.
        bool regenerates_in_dark = false;
        // Will stop fleeing if at max hp, and regen anger and morale.
        bool regen_morale = false;

        void faction_display( catacurses::window &w, const point &top_left, const int width ) const;

        // mountable ratio for rider weight vs. mount weight, default 0.3
        float mountable_weight_ratio = 0.3;

        int attack_cost = 100;  /** moves per regular attack */
        int melee_skill = 0;    /** melee hit skill, 20 is superhuman hitting abilities */
        int melee_dice = 0;     /** number of dice of bonus bashing damage on melee hit */
        int melee_sides = 0;    /** number of sides those dice have */

        int grab_strength = 1;    /**intensity of the effect_grabbed applied*/

        // Pet food category this monster is in
        pet_food_data petfood;

        std::set<scenttype_id> scents_tracked; /**Types of scent tracked by this mtype*/
        std::set<scenttype_id> scents_ignored; /**Types of scent ignored by this mtype*/

        int sk_dodge = 0;       /** dodge skill */

        /** If unset (-1) then values are calculated automatically from other properties */
        int armor_bash = -1;     /** innate armor vs. bash */
        int armor_cut = -1;     /** innate armor vs. cut */
        int armor_dark = -1;     /** innate armor vs. dark */
        int armor_light = -1;    /** innate armor vs. light */
        int armor_psi = -1;      /** innate armor vs. psi */
        int armor_stab = -1;     /** innate armor vs. stabbing */
        int armor_bullet = -1;   /** innate armor vs. bullet */
        int armor_acid = -1;     /** innate armor vs. acid */
        int armor_fire = -1;     /** innate armor vs. fire */
        int armor_cold = -1;     /** innate armor vs. cold */
        int armor_electric = -1; /** innate armor vs. electrical */

        // Vision range is linearly scaled depending on lighting conditions
        int vision_day = 40;    /** vision range in bright light */
        int vision_night = 1;   /** vision range in total darkness */

        damage_instance melee_damage; // Basic melee attack damage
        harvest_id harvest;
        float luminance;           // 0 is default, >0 gives luminance to lightmap

        unsigned int def_chance; // How likely a special "defensive" move is to trigger (0-100%, default 0)
        // special attack frequencies and function pointers
        std::map<std::string, mtype_special_attack> special_attacks;
        std::vector<std::string> special_attacks_names; // names of attacks, in json load order

        std::vector<mon_action_death>  dies;       // What happens when this monster dies
        std::vector<std::function<void( monster & )>> on_death;

        // This monster's special "defensive" move that may trigger when the monster is attacked.
        // Note that this can be anything, and is not necessarily beneficial to the monster
        mon_action_defend sp_defense;

        // Monster upgrade variables
        int half_life;
        int age_grow;
        mtype_id upgrade_into;
        mongroup_id upgrade_group;
        mtype_id burn_into;
        mtype_id zombify_into;
        mtype_id fungalize_into;

        // Monster reproduction variables
        std::optional<time_duration> baby_timer;
        int baby_count;
        mtype_id baby_monster;
        itype_id baby_egg;
        std::vector<std::string> baby_flags;

        // Monster's ability to destroy terrain and vehicles
        int bash_skill;

        // All the bools together for space efficiency

        // TODO: maybe make this private as well? It must be set to `true` only once,
        // and must never be set back to `false`.
        bool was_loaded = false;
        bool upgrades;
        bool reproduces;

        // Do we indiscriminately attack characters, or should we wait until one annoys us?
        bool aggro_character = true;

        mtype();
        /**
         * Check if this type is of the same species as the other one, because
         * species is a set and can contain several species, one entry that is
         * in both monster types fulfills that test.
         */
        bool same_species( const mtype &other ) const;
        /**
         * If this is not empty, the monster can be converted into an item
         * of this type (if it's friendly).
         */
        itype_id revert_to_itype;
        /**
         * If this monster is a rideable mech with built-in weapons, this is the weapons id
         */
        itype_id mech_weapon;
        /**
         * If this monster is a rideable mech it needs a power source battery type
         */
        itype_id mech_battery;
        /**
         * If this monster is a rideable mech with enhanced strength, this is the strength it gives to the player
         */
        int mech_str_bonus = 0;

        /** Emission sources that cycle each turn the monster remains alive */
        std::map<emit_id, time_duration> emit_fields;

        pathfinding_settings legacy_path_settings;
        pathfinding_settings legacy_path_settings_buffed;

        PathfindingSettings path_settings;
        RouteSettings route_settings;
        PathfindingSettings path_settings_buffed;
        RouteSettings route_settings_buffed;

        // We rely on external options to construct pathfinding options
        //   which are subject to change by rebalancing mods
        //   thus necessiating a late load
        std::unordered_map<std::string, std::variant<float, bool, int>> recorded_path_settings;
        void setup_pathfinding_deferred();

        // Used to fetch the properly pluralized monster type name
        std::string nname( unsigned int quantity = 1 ) const;
        bool has_special_attack( const std::string &attack_name ) const;
        bool has_flag( m_flag flag ) const;
        void set_flag( m_flag flag, bool state = true );
        bool made_of( const material_id &material ) const;
        bool made_of_any( const std::set<material_id> &materials ) const;
        bool has_anger_trigger( mon_trigger trigger ) const;
        bool has_fear_trigger( mon_trigger trigger ) const;
        bool has_placate_trigger( mon_trigger trigger ) const;
        bool in_category( const std::string &category ) const;
        bool in_species( const species_id &spec ) const;
        bool in_species( const species_type &spec ) const;
        std::vector<std::string> species_descriptions() const;
        //Used for corpses.
        field_type_id bloodType() const;
        field_type_id gibType() const;
        // The item id of the meat items that are produced by this monster (or "null")
        // if there is no matching item type. e.g. "veggy" for plant monsters.
        itype_id get_meat_itype() const;
        int get_meat_chunks_count() const;
        std::string get_description() const;
        std::string get_footsteps() const;
        void set_strategy();
        void add_goal( const std::string &goal_id );
        const behavior::node_t *get_goals() const;

        // Historically located in monstergenerator.cpp
        void load( const JsonObject &jo, const std::string &src );
        LUA_TYPE_OPS( mtype, id );
};

mon_effect_data load_mon_effect_data( const JsonObject &e );

