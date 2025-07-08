#include "bodypart.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "anatomy.h"
#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "pldata.h"
#include "type_id.h"
#include "locations.h"

const bodypart_str_id body_part_head( "head" );
const bodypart_str_id body_part_eyes( "eyes" );
const bodypart_str_id body_part_mouth( "mouth" );
const bodypart_str_id body_part_torso( "torso" );
const bodypart_str_id body_part_arm_l( "arm_l" );
const bodypart_str_id body_part_arm_r( "arm_r" );
const bodypart_str_id body_part_hand_l( "hand_l" );
const bodypart_str_id body_part_hand_r( "hand_r" );
const bodypart_str_id body_part_leg_l( "leg_l" );
const bodypart_str_id body_part_foot_l( "foot_l" );
const bodypart_str_id body_part_leg_r( "leg_r" );
const bodypart_str_id body_part_foot_r( "foot_r" );

side opposite_side( side s )
{
    switch( s ) {
        case side::BOTH:
            return side::BOTH;
        case side::LEFT:
            return side::RIGHT;
        case side::RIGHT:
            return side::LEFT;
        case side::num_sides:
            debugmsg( "invalid side %d", static_cast<int>( s ) );
            break;
    }

    return s;
}

namespace io
{

template<>
std::string enum_to_string<side>( side data )
{
    switch( data ) {
        // *INDENT-OFF*
        case side::LEFT: return "left";
        case side::RIGHT: return "right";
        case side::BOTH: return "both";
        // *INDENT-ON*
        case side::num_sides:
            break;
    }
    debugmsg( "Invalid side" );
    abort();
}

template<>
std::string enum_to_string<body_part>( body_part data )
{
    switch( data ) {
        case body_part::bp_torso:
            return "TORSO";
        case body_part::bp_head:
            return "HEAD";
        case body_part::bp_eyes:
            return "EYES";
        case body_part::bp_mouth:
            return "MOUTH";
        case body_part::bp_arm_l:
            return "ARM_L";
        case body_part::bp_arm_r:
            return "ARM_R";
        case body_part::bp_hand_l:
            return "HAND_L";
        case body_part::bp_hand_r:
            return "HAND_R";
        case body_part::bp_leg_l:
            return "LEG_L";
        case body_part::bp_leg_r:
            return "LEG_R";
        case body_part::bp_foot_l:
            return "FOOT_L";
        case body_part::bp_foot_r:
            return "FOOT_R";
        case body_part::num_bp:
            break;
    }
    debugmsg( "Invalid body_part" );
    abort();
}

} // namespace io

namespace
{

generic_factory<body_part_type> body_part_factory( "body part" );

} // namespace

bool is_legacy_bodypart_id( const std::string &id )
{
    static const std::vector<std::string> legacy_body_parts = {
        "TORSO",
        "HEAD",
        "EYES",
        "MOUTH",
        "ARM_L",
        "ARM_R",
        "HAND_L",
        "HAND_R",
        "LEG_L",
        "LEG_R",
        "FOOT_L",
        "FOOT_R",
        "NUM_BP",
    };

    return std::ranges::find( legacy_body_parts,
                              id ) != legacy_body_parts.end();
}

static body_part legacy_id_to_enum( const std::string &legacy_id )
{
    static const std::unordered_map<std::string, body_part> body_parts = {
        { "TORSO", bp_torso },
        { "HEAD", bp_head },
        { "EYES", bp_eyes },
        { "MOUTH", bp_mouth },
        { "ARM_L", bp_arm_l },
        { "ARM_R", bp_arm_r },
        { "HAND_L", bp_hand_l },
        { "HAND_R", bp_hand_r },
        { "LEG_L", bp_leg_l },
        { "LEG_R", bp_leg_r },
        { "FOOT_L", bp_foot_l },
        { "FOOT_R", bp_foot_r },
        { "NUM_BP", num_bp },
        // Also try the new ones, just in case
        { "torso", bp_torso },
        { "head", bp_head },
        { "eyes", bp_eyes },
        { "mouth", bp_mouth },
        { "arm_l", bp_arm_l },
        { "arm_r", bp_arm_r },
        { "hand_l", bp_hand_l },
        { "hand_r", bp_hand_r },
        { "leg_l", bp_leg_l },
        { "leg_r", bp_leg_r },
        { "foot_l", bp_foot_l },
        { "foot_r", bp_foot_r },
        { "num_bp", num_bp },
    };
    const auto &iter = body_parts.find( legacy_id );
    if( iter == body_parts.end() ) {
        debugmsg( "Invalid body part legacy id %s", legacy_id.c_str() );
        return num_bp;
    }

    return iter->second;
}

/**@relates string_id*/
template<>
bool string_id<body_part_type>::is_valid() const
{
    return body_part_factory.is_valid( *this );
}

/** @relates int_id */
template<>
bool int_id<body_part_type>::is_valid() const
{
    return body_part_factory.is_valid( *this );
}

/**@relates string_id*/
template<>
const body_part_type &string_id<body_part_type>::obj() const
{
    return body_part_factory.obj( *this );
}

/** @relates int_id */
template<>
const body_part_type &int_id<body_part_type>::obj() const
{
    return body_part_factory.obj( *this );
}

/** @relates int_id */
template<>
const bodypart_str_id &int_id<body_part_type>::id() const
{
    return body_part_factory.convert( *this );
}

/**@relates string_id*/
template<>
bodypart_id string_id<body_part_type>::id() const
{
    return body_part_factory.convert( *this, bodypart_id( 0 ) );
}

/** @relates int_id */
template<>
int_id<body_part_type>::int_id( const string_id<body_part_type> &id ) : _id( id.id() ) {}

body_part get_body_part_token( const std::string &id )
{
    return legacy_id_to_enum( id );
}

const bodypart_str_id &convert_bp( body_part bp )
{
    static const std::vector<bodypart_str_id> body_parts = {
        bodypart_str_id( "torso" ),
        bodypart_str_id( "head" ),
        bodypart_str_id( "eyes" ),
        bodypart_str_id( "mouth" ),
        bodypart_str_id( "arm_l" ),
        bodypart_str_id( "arm_r" ),
        bodypart_str_id( "hand_l" ),
        bodypart_str_id( "hand_r" ),
        bodypart_str_id( "leg_l" ),
        bodypart_str_id( "leg_r" ),
        bodypart_str_id( "foot_l" ),
        bodypart_str_id( "foot_r" ),
        bodypart_str_id::NULL_ID()
    };
    if( bp > num_bp || bp < bp_torso ) {
        debugmsg( "Invalid body part token %d", bp );
        return body_parts[ num_bp ];
    }

    return body_parts[static_cast<size_t>( bp )];
}

static const body_part_type &get_bp( body_part bp )
{
    return convert_bp( bp ).obj();
}

void body_part_type::load_bp( const JsonObject &jo, const std::string &src )
{
    body_part_factory.load( jo, src );
}

void body_part_type::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "id", id );

    mandatory( jo, was_loaded, "name", name );
    // This is NOT the plural of `name`; it's a name refering to the pair of
    // bodyparts which this bodypart belongs to, and thus should not be implemented
    // using "vgettext" or "translation::make_plural". Otherwise, in languages
    // without plural forms, translation of this string would indicate it
    // to be a left or right part, while it is not.
    optional( jo, was_loaded, "name_multiple", name_multiple );

    mandatory( jo, was_loaded, "accusative", accusative );
    // same as the above comment
    optional( jo, was_loaded, "accusative_multiple", accusative_multiple );

    mandatory( jo, was_loaded, "heading", name_as_heading );
    // Same as the above comment
    optional( jo, was_loaded, "heading_multiple", name_as_heading_multiple, name_as_heading );
    optional( jo, was_loaded, "hp_bar_ui_text", hp_bar_ui_text );
    optional( jo, was_loaded, "encumbrance_text", encumb_text );

    mandatory( jo, was_loaded, "sort_order", sort_order );

    assign( jo, "hit_size", hit_size, true );
    assign( jo, "hit_difficulty", hit_difficulty, true );
    assign( jo, "hit_size_relative", hit_size_relative, true );

    assign( jo, "base_hp", base_hp, true );

    assign( jo, "legacy_id", legacy_id );
    token = legacy_id_to_enum( legacy_id );

    optional( jo, was_loaded, "main_part", main_part, id );
    optional( jo, was_loaded, "opposite_part", opposite_part, id );

    optional( jo, was_loaded, "essential", essential, false );

    assign( jo, "drench_capacity", drench_capacity, true );

    optional( jo, was_loaded, "hot_morale_mod", hot_morale_mod, 0.0 );
    optional( jo, was_loaded, "cold_morale_mod", cold_morale_mod, 0.0 );

    optional( jo, was_loaded, "stylish_bonus", stylish_bonus, 0 );

    optional( jo, was_loaded, "bionic_slots", bionic_slots_, 0 );

    const auto side_reader = enum_flags_reader<side> { "side" };
    optional( jo, was_loaded, "side", part_side, side_reader, side::BOTH );
}

void body_part_type::reset()
{
    body_part_factory.reset();
}

void body_part_type::finalize_all()
{
    body_part_factory.finalize();
}

void body_part_type::finalize()
{
}

void body_part_type::check_consistency()
{
    for( const body_part bp : all_body_parts ) {
        const auto &legacy_bp = convert_bp( bp );
        if( !legacy_bp.is_valid() ) {
            debugmsg( "Mandatory body part %s was not loaded", legacy_bp.c_str() );
        }
    }

    body_part_factory.check();
}

void body_part_type::check() const
{
    const auto &under_token = get_bp( token );
    if( this != &under_token ) {
        debugmsg( "Body part %s has duplicate token %d, mapped to %s", id.c_str(), token,
                  under_token.id.c_str() );
    }

    if( !id.is_null() && main_part.is_null() ) {
        debugmsg( "Body part %s has unset main part", id.c_str() );
    }

    if( !id.is_null() && opposite_part.is_null() ) {
        debugmsg( "Body part %s has unset opposite part", id.c_str() );
    }

    if( !main_part.is_valid() ) {
        debugmsg( "Body part %s has invalid main part %s.", id.c_str(), main_part.c_str() );
    }

    if( !opposite_part.is_valid() ) {
        debugmsg( "Body part %s has invalid opposite part %s.", id.c_str(), opposite_part.c_str() );
    }
}

std::string body_part_name( body_part bp, int number )
{
    return body_part_name( convert_bp( bp ), number );
}

std::string body_part_name( const bodypart_id &bp, int number )
{
    // See comments in `body_part_type::load` about why these two strings are
    // not a single translation object with plural enabled.
    return number > 1 ? bp->name_multiple.translated() : bp->name.translated();
}

std::string body_part_name_accusative( body_part bp, int number )
{
    return body_part_name_accusative( convert_bp( bp ), number );
}

std::string body_part_name_accusative( const bodypart_id &bp, int number )
{
    // See comments in `body_part_type::load` about why these two strings are
    // not a single translation object with plural enabled.
    return number > 1 ? bp->accusative_multiple.translated() : bp->accusative.translated();
}

std::string body_part_name_as_heading( body_part bp, int number )
{
    return body_part_name_as_heading( convert_bp( bp ), number );
}

std::string body_part_name_as_heading( const bodypart_id &bp, int number )
{
    // See comments in `body_part_type::load` about why these two strings are
    // not a single translation object with plural enabled.
    return number > 1 ? bp->name_as_heading_multiple.translated() : bp->name_as_heading.translated();
}

std::string body_part_hp_bar_ui_text( const bodypart_id &bp )
{
    return _( bp->hp_bar_ui_text );
}

std::string encumb_text( body_part bp )
{
    const std::string &txt = get_bp( bp ).encumb_text;
    return !txt.empty() ? _( txt ) : txt;
}

bodypart_str_id random_body_part( bool main_parts_only )
{
    const auto &part = human_anatomy->random_body_part();
    return main_parts_only ? part->main_part : part.id();
}

body_part mutate_to_main_part( body_part bp )
{
    return get_bp( bp ).main_part->token;
}

body_part opposite_body_part( body_part bp )
{
    return get_bp( bp ).opposite_part->token;
}

bodypart::bodypart() : bodypart( new fake_item_location() ) {};

bodypart_id bodypart::get_id() const
{
    return id;
}

bodypart_str_id bodypart::get_str_id() const
{
    return id;
}

void bodypart::set_hp_to_max()
{
    hp_cur = hp_max;
}

bool bodypart::is_at_max_hp() const
{
    return hp_cur == hp_max;
}

int bodypart::get_hp_cur() const
{
    return hp_cur;
}

int bodypart::get_hp_max() const
{
    return hp_max;
}

int bodypart::get_healed_total() const
{
    return healed_total;
}

int bodypart::get_damage_bandaged() const
{
    return damage_bandaged;
}

int bodypart::get_damage_disinfected() const
{
    return damage_disinfected;
}

void bodypart::set_hp_cur( int set )
{
    hp_cur = set;
}

void bodypart::set_hp_max( int set )
{
    hp_max = set;
}

void bodypart::set_healed_total( int set )
{
    healed_total = set;
}

void bodypart::set_damage_bandaged( int set )
{
    damage_bandaged = set;
}

void bodypart::set_damage_disinfected( int set )
{
    damage_disinfected = set;
}

void bodypart::mod_hp_cur( int mod )
{
    hp_cur += mod;
}

void bodypart::mod_hp_max( int mod )
{
    hp_max += mod;
}

void bodypart::mod_healed_total( int mod )
{
    healed_total += mod;
}

void bodypart::mod_damage_bandaged( int mod )
{
    damage_bandaged += mod;
}

void bodypart::mod_damage_disinfected( int mod )
{
    damage_disinfected += mod;
}

body_part_set &body_part_set::unify_set( const body_part_set &rhs )
{
    for( auto i = rhs.parts.begin(); i != rhs.parts.end(); i++ ) {
        if( parts.count( *i ) == 0 ) {
            parts.insert( *i );
        }
    }
    return *this;
}

body_part_set &body_part_set::intersect_set( const body_part_set &rhs )
{
    for( auto it = parts.begin(); it < parts.end(); ) {
        if( rhs.parts.count( *it ) == 0 ) {
            it = parts.erase( it );
        } else {
            it++;
        }
    }
    return *this;
}

body_part_set &body_part_set::substract_set( const body_part_set &rhs )
{
    for( auto j = rhs.parts.begin(); j != rhs.parts.end(); j++ ) {
        if( parts.count( *j ) > 0 ) {
            parts.erase( *j );
        }
    }
    return *this;
}

body_part_set body_part_set::make_intersection( const body_part_set &rhs ) const
{
    body_part_set new_intersection;
    new_intersection.parts = parts;
    new_intersection.intersect_set( rhs );
    return new_intersection;
}

void body_part_set::fill( const std::vector<bodypart_id> &bps )
{
    for( const bodypart_id &bp : bps ) {
        parts.insert( bp.id() );
    }
}

void bodypart::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "id", id );
    json.member( "hp_cur", hp_cur );
    json.member( "hp_max", hp_max );
    json.member( "damage_bandaged", damage_bandaged );
    json.member( "damage_disinfected", damage_disinfected );
    json.member( "temp_cur", temp_cur );
    json.member( "temp_conv", temp_conv );
    json.member( "frostbite_timer", frostbite_timer );
    json.member( "wetness", wetness );
    json.end_object();
}

void bodypart::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();
    jo.read( "id", id, true );
    jo.read( "hp_cur", hp_cur, true );
    jo.read( "hp_max", hp_max, true );
    jo.read( "damage_bandaged", damage_bandaged, true );
    jo.read( "damage_disinfected", damage_disinfected, true );
    jo.read( "temp_cur", temp_cur, true );
    jo.read( "temp_conv", temp_conv, false );
    jo.read( "frostbite_timer", frostbite_timer, true );
    jo.read( "wetness", wetness, true );
}

void bodypart::set_location( location<item> *loc )
{
    wielding.wielded.set_loc_hack( loc );
}

wield_status::wield_status( wield_status &&source ) noexcept : wielded(
        source.wielded.get_loc_hack() )
{
    wielded = source.wielded.release();
}

wield_status &wield_status::operator=( wield_status &&source ) noexcept
{
    wielded = source.wielded.release();
    return *this;
}
