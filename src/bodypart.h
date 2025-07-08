#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <initializer_list>
#include <string>

#include "flat_set.h"
#include "int_id.h"
#include "catalua_type_operators.h"
#include "string_id.h"
#include "translations.h"
#include "location_ptr.h"

class JsonObject;
class JsonIn;
class JsonOut;
class item;
struct body_part_type;

template <typename T> class location;
template <typename E> struct enum_traits;

using bodypart_str_id = string_id<body_part_type>;
using bodypart_id = int_id<body_part_type>;

extern const bodypart_str_id body_part_head;
extern const bodypart_str_id body_part_eyes;
extern const bodypart_str_id body_part_mouth;
extern const bodypart_str_id body_part_torso;
extern const bodypart_str_id body_part_arm_l;
extern const bodypart_str_id body_part_arm_r;
extern const bodypart_str_id body_part_hand_l;
extern const bodypart_str_id body_part_hand_r;
extern const bodypart_str_id body_part_leg_l;
extern const bodypart_str_id body_part_foot_l;
extern const bodypart_str_id body_part_leg_r;
extern const bodypart_str_id body_part_foot_r;

// The order is important ; pldata.h has to be in the same order
enum body_part : int {
    bp_torso = 0,
    bp_head,
    bp_eyes,
    bp_mouth,
    bp_arm_l,
    bp_arm_r,
    bp_hand_l,
    bp_hand_r,
    bp_leg_l,
    bp_leg_r,
    bp_foot_l,
    bp_foot_r,
    num_bp
};

template <typename T>
inline bool operator<( body_part a, T b )
{
    return static_cast<int>( a ) < static_cast<int>( b );
}

template<>
struct enum_traits<body_part> {
    static constexpr auto last = body_part::num_bp;
};

enum class side : int {
    BOTH,
    LEFT,
    RIGHT,
    num_sides
};

template<>
struct enum_traits<side> {
    static constexpr auto last = side::num_sides;
};

/**
 * Contains all valid @ref body_part values in the order they are
 * defined in. Use this to iterate over them.
 */
constexpr std::array<body_part, 12> all_body_parts = {{
        bp_torso, bp_head, bp_eyes, bp_mouth,
        bp_arm_l, bp_arm_r, bp_hand_l, bp_hand_r,
        bp_leg_l, bp_leg_r, bp_foot_l, bp_foot_r
    }
};

struct body_part_type {
    public:
        bodypart_str_id id;
        bool was_loaded = false;

        int sort_order = 0;

        // Those are stored untranslated
        translation name;
        translation name_multiple;
        translation accusative;
        translation accusative_multiple;
        translation name_as_heading;
        translation name_as_heading_multiple;
        std::string hp_bar_ui_text;
        std::string encumb_text;
        // Legacy "string id"
        std::string legacy_id = "num_bp";
        // Legacy enum "int id"
        body_part token = num_bp;
        /** Size of the body part when doing an unweighted selection. */
        float hit_size = 0.0f;
        /** Hit sizes for attackers who are smaller, equal in size, and bigger. */
        std::array<float, 3> hit_size_relative = {{ 0.0f, 0.0f, 0.0f }};
        /**
         * How hard is it to hit a given body part, assuming "owner" is hit.
         * Higher number means good hits will veer towards this part,
         * lower means this part is unlikely to be hit by inaccurate attacks.
         * Formula is `chance *= pow(hit_roll, hit_difficulty)`
         */
        float hit_difficulty = 0.0f;
        // Is this part important, as in, can you live without it?
        bool essential = false;
        // "Parent" of this part - main parts are their own "parents"
        // TODO: Connect head and limbs to torso
        bodypart_str_id main_part;
        // A part that has no opposite is its own opposite (that's pretty Zen)
        bodypart_str_id opposite_part;
        // Parts with no opposites have BOTH here
        side part_side = side::BOTH;

        int drench_capacity = 0;

        //Morale parameters
        float hot_morale_mod = 0;
        float cold_morale_mod = 0;
        float stylish_bonus = 0;

        int base_hp = 60;

        LUA_TYPE_OPS( body_part_type, id );

        void load( const JsonObject &jo, const std::string &src );
        void finalize();
        void check() const;

        static void load_bp( const JsonObject &jo, const std::string &src );

        // Clears all bps
        static void reset();
        // Post-load finalization
        static void finalize_all();
        // Verifies that body parts make sense
        static void check_consistency();

        int bionic_slots() const {
            return bionic_slots_;
        }
    private:
        int bionic_slots_ = 0;
};

class wield_status
{
    public:
        wield_status( const wield_status & ) = delete;
        wield_status &operator=( const wield_status & ) = delete;
        wield_status( wield_status && ) noexcept;
        wield_status &operator=( wield_status && ) noexcept;
        wield_status( location<item> *loc ) : wielded( loc ) {};
        location_ptr<item, false> wielded;
};

enum class water_tolerance : int {
    WT_IGNORED = 0,
    WT_NEUTRAL,
    WT_GOOD,
    NUM_WATER_TOLERANCE
};

class bodypart
{
    private:
        bodypart_str_id id;

        int hp_cur;
        int hp_max;

        int healed_total = 0;
        int damage_bandaged = 0;
        int damage_disinfected = 0;

        // @todo BODYTEMP_NORM
        int temp_cur = 5000;
        int temp_conv = 5000;
        int frostbite_timer = 0;

        int wetness = 0;
        // Just a cache, don't save/load
        std::array<int, static_cast<size_t>( water_tolerance::NUM_WATER_TOLERANCE )> mut_drench;

    public:
        // TODO: private
        wield_status wielding;

    public:
        bodypart( const bodypart & ) = delete;
        bodypart( bodypart && ) = default;
        bodypart();
        bodypart( location<item> *loc ): id( bodypart_str_id( "num_bp" ) ), hp_cur( 0 ), hp_max( 0 ),
            wielding( loc ) {}
        bodypart( bodypart_str_id id, location<item> *loc ): id( id ), hp_cur( id->base_hp ),
            hp_max( id->base_hp ), wielding( loc )  {}

        bodypart &operator=( bodypart && ) = default;

        bodypart_id get_id() const;
        bodypart_str_id get_str_id() const;

        void set_hp_to_max();
        bool is_at_max_hp() const;

        int get_hp_cur() const;
        int get_hp_max() const;
        int get_healed_total() const;
        int get_damage_bandaged() const;
        int get_damage_disinfected() const;

        void set_hp_cur( int set );
        void set_hp_max( int set );
        void set_healed_total( int set );
        void set_damage_bandaged( int set );
        void set_damage_disinfected( int set );

        void mod_hp_cur( int mod );
        void mod_hp_max( int mod );
        void mod_healed_total( int mod );
        void mod_damage_bandaged( int mod );
        void mod_damage_disinfected( int mod );

        int get_temp_cur() const {
            return temp_cur;
        }
        void set_temp_cur( int set ) {
            temp_cur = set;
        }

        int get_temp_conv() const {
            return temp_conv;
        }
        void set_temp_conv( int set ) {
            temp_conv = set;
        }

        int get_frostbite_timer() const {
            return frostbite_timer;
        }
        void set_frostbite_timer( int set ) {
            frostbite_timer = set;
        }

        int get_wetness() const {
            return wetness;
        }

        void set_wetness( int set ) {
            wetness = set;
        }

        int get_drench_capacity() const {
            return id->drench_capacity;
        }


        std::array<int, static_cast<size_t>( water_tolerance::NUM_WATER_TOLERANCE )> get_mut_drench()
        const {
            return mut_drench;
        }

        void set_mut_drench( std::array<int, static_cast<size_t>( water_tolerance::NUM_WATER_TOLERANCE )>
                             set ) {
            mut_drench = set;
        }

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

        void set_location( location<item> *loc );
};

class body_part_set
{
    private:
        cata::flat_set<bodypart_str_id> parts;

        explicit body_part_set( const cata::flat_set<bodypart_str_id> &other ) : parts( other ) { }

    public:
        body_part_set() = default;
        body_part_set( std::initializer_list<bodypart_str_id> bps ) {
            for( const bodypart_str_id &bp : bps ) {
                set( bp );
            }
        }
        /** Adds all elements from provided set to this set. */
        body_part_set &unify_set( const body_part_set &rhs );
        /** Removes all elements that are absent from the provided set. */
        body_part_set &intersect_set( const body_part_set &rhs );
        /** Removes all elements that are present in the provided set. */
        body_part_set &substract_set( const body_part_set &rhs );
        /** Creates new set that is the intersection of this set and provided set. */
        body_part_set make_intersection( const body_part_set &rhs ) const;

        void fill( const std::vector<bodypart_id> &bps );


        bool test( const bodypart_str_id &bp ) const {
            return parts.count( bp ) > 0;
        }
        void set( const bodypart_str_id &bp ) {
            parts.insert( bp );
        }
        void reset( const bodypart_str_id &bp ) {
            parts.erase( bp );
        }
        void reset() {
            parts.clear();
        }
        bool any() const {
            return !parts.empty();
        }
        bool none() const {
            return parts.empty();
        }
        size_t count() const {
            return parts.size();
        }

        cata::flat_set<bodypart_str_id>::iterator begin() const {
            return parts.begin();
        }

        cata::flat_set<bodypart_str_id>::iterator end() const {
            return parts.end();
        }

        template<typename Stream>
        void serialize( Stream &s ) const {
            s.write( parts );
        }
        template<typename Stream>
        void deserialize( Stream &s ) {
            s.read( parts );
        }
};

// Returns if passed string is legacy bodypart (i.e "TORSO", not "torso")
bool is_legacy_bodypart_id( const std::string &id );

/** Returns the new id for old token */
const bodypart_str_id &convert_bp( body_part bp );

/** Returns the opposite side. */
side opposite_side( side s );

// identify the index of a body part's "other half", or itself if not
const std::array<size_t, 12> bp_aiOther = {{0, 1, 2, 3, 5, 4, 7, 6, 9, 8, 11, 10}};

/** Returns the matching name of the body_part token. */
std::string body_part_name( body_part bp, int number = 1 );
std::string body_part_name( const bodypart_id &bp, int number = 1 );

/** Returns the matching accusative name of the body_part token, i.e. "Shrapnel hits your X".
 *  These are identical to body_part_name above in English, but not in some other languages. */
std::string body_part_name_accusative( body_part bp, int number = 1 );
std::string body_part_name_accusative( const bodypart_id &bp, int number = 1 );

/** Returns the name of the body parts in a context where the name is used as
 * a heading or title e.g. "Left Arm". */
std::string body_part_name_as_heading( body_part bp, int number );
std::string body_part_name_as_heading( const bodypart_id &bp, int number );

/** Returns the body part text to be displayed in the HP bar */
std::string body_part_hp_bar_ui_text( const bodypart_id &bp );

/** Returns the matching encumbrance text for a given body_part token. */
std::string encumb_text( body_part bp );

/** Returns a random body_part token. main_parts_only will limit it to arms, legs, torso, and head. */
bodypart_str_id random_body_part( bool main_parts_only = false );

/** Returns the matching main body_part that corresponds to the input; i.e. returns bp_arm_l from bp_hand_l. */
body_part mutate_to_main_part( body_part bp );
/** Returns the opposite body part (limb on the other side) */
body_part opposite_body_part( body_part bp );

/** Returns the matching body_part token from the corresponding body_part string. */
body_part get_body_part_token( const std::string &id );


